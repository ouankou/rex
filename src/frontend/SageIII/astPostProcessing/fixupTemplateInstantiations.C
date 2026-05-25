// tps (01/14/2010) : Switching from rose.h to sage3.
#include "astPostProcessing/fixupTemplateInstantiations.h"

#include "markCompilerGenerated.h"

#include "sage3basic.h"

#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace {
class MemoryPoolTraversalFilterGuard {
public:
  explicit MemoryPoolTraversalFilterGuard(
      Rose::MemoryPoolTraversalFilter next_filter)
      : previous_filter_(Rose::getMemoryPoolTraversalFilter()) {
    Rose::setMemoryPoolTraversalFilter(next_filter);
  }

  ~MemoryPoolTraversalFilterGuard() {
    Rose::setMemoryPoolTraversalFilter(previous_filter_);
  }

private:
  Rose::MemoryPoolTraversalFilter previous_filter_;
};

SgProject *projectForMemoryPoolNodeImpl(SgNode *node,
                                        std::unordered_set<SgNode *> &visited) {
  if (node == nullptr) {
    return nullptr;
  }
  if (!visited.insert(node).second) {
    return nullptr;
  }

  for (SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (SgProject *project = isSgProject(current)) {
      return project;
    }
  }

  if (SgSourceFile *file = SageInterface::getEnclosingSourceFile(node)) {
    return isSgProject(file->get_parent());
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(node)) {
    if (SgScopeStatement *scope = decl->get_scope()) {
      if (SgProject *project = projectForMemoryPoolNodeImpl(scope, visited)) {
        return project;
      }
    }
    if (SgDeclarationStatement *first_nondef =
            decl->get_firstNondefiningDeclaration()) {
      if (SgProject *project =
              projectForMemoryPoolNodeImpl(first_nondef, visited)) {
        return project;
      }
    }
    if (SgDeclarationStatement *defining_decl =
            decl->get_definingDeclaration()) {
      if (SgProject *project =
              projectForMemoryPoolNodeImpl(defining_decl, visited)) {
        return project;
      }
    }
  }

  if (SgClassType *class_type = isSgClassType(node)) {
    return projectForMemoryPoolNodeImpl(class_type->get_declaration(), visited);
  }

  if (SgType *type = isSgType(node)) {
    if (SgClassType *class_type = isSgClassType(type)) {
      return projectForMemoryPoolNodeImpl(class_type->get_declaration(),
                                          visited);
    }
  }

  return nullptr;
}

SgProject *projectForMemoryPoolNode(SgNode *node) {
  std::unordered_set<SgNode *> visited;
  return projectForMemoryPoolNodeImpl(node, visited);
}

template <typename InstantiationDeclT, typename TemplateDeclT>
void recoverTemplateDeclarationLink(InstantiationDeclT *decl) {
  if (decl == nullptr || decl->get_templateDeclaration() != nullptr) {
    return;
  }

  if (InstantiationDeclT *first_nondef = dynamic_cast<InstantiationDeclT *>(
          decl->get_firstNondefiningDeclaration())) {
    if (first_nondef->get_templateDeclaration() != nullptr) {
      decl->set_templateDeclaration(first_nondef->get_templateDeclaration());
      return;
    }
  }

  if (InstantiationDeclT *defining_decl =
          dynamic_cast<InstantiationDeclT *>(decl->get_definingDeclaration())) {
    if (defining_decl->get_templateDeclaration() != nullptr) {
      decl->set_templateDeclaration(defining_decl->get_templateDeclaration());
      return;
    }
  }

  if (TemplateDeclT *specialized_template = dynamic_cast<TemplateDeclT *>(
          decl->get_specializedTemplateDeclaration())) {
    decl->set_templateDeclaration(specialized_template);
  }
}

SgScopeStatement *selectTemplateLookupScope(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  SgScopeStatement *lookup_scope = isSgScopeStatement(decl->get_parent());
  if (lookup_scope == nullptr) {
    lookup_scope = decl->get_scope();
  }
  if (lookup_scope == nullptr) {
    lookup_scope = SageBuilder::topScopeStack();
  }
  if (lookup_scope == nullptr) {
    lookup_scope = SageInterface::getGlobalScope(decl);
  }

  return lookup_scope;
}

SgScopeStatement *selectMemberInstantiationScope(
    SgTemplateInstantiationMemberFunctionDecl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (SgScopeStatement *class_scope = decl->get_class_scope()) {
    return class_scope;
  }

  return selectTemplateLookupScope(decl);
}

void suppressLocatedNodeOutput(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }

  if (Sg_File_Info *fi = node->get_startOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = node->get_endOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
}

void hideSynthesizedFunctionDeclaration(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return;
  }

  markAsCompilerGenerated(decl);
  suppressLocatedNodeOutput(decl);

  if (SgFunctionParameterList *params = decl->get_parameterList()) {
    suppressLocatedNodeOutput(params);
    for (SgInitializedName *param : params->get_args()) {
      suppressLocatedNodeOutput(param);
    }
  }

  if (SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(decl)) {
    suppressLocatedNodeOutput(member_decl->get_CtorInitializerList());
  }
}

bool isLambdaClosureClassDeclaration(SgDeclarationStatement *declaration) {
  SgClassDeclaration *class_decl = isSgClassDeclaration(declaration);
  if (class_decl == nullptr) {
    return false;
  }

  auto source_backed = [](SgLocatedNode *node) {
    if (node == nullptr || node->get_file_info() == nullptr) {
      return false;
    }

    Sg_File_Info *fi = node->get_file_info();
    return fi->get_line() > 0 && !fi->isCompilerGenerated() &&
           !fi->isFrontendSpecific() &&
           !fi->isSourcePositionUnavailableInFrontend();
  };

  auto is_lambda_closure = [](SgClassDeclaration *candidate) {
    if (candidate == nullptr) {
      return false;
    }

    SgLambdaExp *lambda = isSgLambdaExp(candidate->get_parent());
    return lambda != nullptr && lambda->get_lambda_closure_class() == candidate;
  };

  auto has_source_backed_call_operator = [&](SgClassDeclaration *candidate) {
    if (candidate == nullptr) {
      return false;
    }

    SgClassDeclaration *defining_decl =
        isSgClassDeclaration(candidate->get_definingDeclaration());
    if (defining_decl == nullptr) {
      defining_decl = candidate;
    }

    SgClassDefinition *class_def = defining_decl->get_definition();
    if (class_def == nullptr) {
      class_def = candidate->get_definition();
    }
    if (class_def == nullptr) {
      return false;
    }

    for (SgDeclarationStatement *member : class_def->get_members()) {
      SgMemberFunctionDeclaration *member_func =
          isSgMemberFunctionDeclaration(member);
      if (member_func == nullptr ||
          member_func->get_name().getString() != "operator()") {
        continue;
      }

      if (source_backed(member_func)) {
        return true;
      }
      if (SgFunctionDeclaration *def_decl =
              isSgFunctionDeclaration(member_func->get_definingDeclaration())) {
        if (source_backed(def_decl)) {
          return true;
        }
        if (SgFunctionDefinition *func_def = def_decl->get_definition()) {
          if (source_backed(func_def) || source_backed(func_def->get_body())) {
            return true;
          }
        }
      }
      if (SgFunctionDefinition *func_def = member_func->get_definition()) {
        if (source_backed(func_def) || source_backed(func_def->get_body())) {
          return true;
        }
      }
    }

    return false;
  };

  return is_lambda_closure(class_decl) ||
         is_lambda_closure(isSgClassDeclaration(
             class_decl->get_firstNondefiningDeclaration())) ||
         is_lambda_closure(
             isSgClassDeclaration(class_decl->get_definingDeclaration())) ||
         has_source_backed_call_operator(class_decl);
}

bool scopeContainsStatement(SgScopeStatement *scope, SgStatement *stmt) {
  if (scope == nullptr || stmt == nullptr) {
    return false;
  }

  SgStatementPtrList statements = scope->generateStatementList();
  return std::find(statements.begin(), statements.end(), stmt) !=
         statements.end();
}

SgDeclarationStatementPtrList *
scopeOwnedDeclarationList(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }

  if (SgGlobal *global = isSgGlobal(scope)) {
    return &global->get_declarations();
  }

  if (SgNamespaceDefinitionStatement *namespace_scope =
          isSgNamespaceDefinitionStatement(scope)) {
    return &namespace_scope->get_declarations();
  }

  if (SgClassDefinition *class_scope = isSgClassDefinition(scope)) {
    return &class_scope->get_members();
  }

  if (SgDeclarationScope *declaration_scope = isSgDeclarationScope(scope)) {
    return &declaration_scope->get_declarations();
  }

  return nullptr;
}

bool ensureDeclarationInScopeBeforeTarget(SgScopeStatement *scope,
                                          SgDeclarationStatement *stmt,
                                          SgDeclarationStatement *target) {
  if (scope == nullptr || stmt == nullptr) {
    return false;
  }

  SgDeclarationStatementPtrList *declarations =
      scopeOwnedDeclarationList(scope);
  if (declarations == nullptr) {
    return false;
  }

  if (stmt->get_parent() != nullptr && stmt->get_parent() != scope) {
    return false;
  }

  SgDeclarationStatementPtrList::iterator existing =
      std::find(declarations->begin(), declarations->end(), stmt);
  if (existing != declarations->end()) {
    if (stmt->get_parent() != scope) {
      stmt->set_parent(scope);
    }
    return true;
  }

  SgDeclarationStatementPtrList::iterator insert_position = declarations->end();
  if (target != nullptr) {
    SgDeclarationStatementPtrList::iterator target_position =
        std::find(declarations->begin(), declarations->end(), target);
    if (target_position != declarations->end()) {
      insert_position = target_position;
    }
  }

  stmt->set_parent(scope);
  declarations->insert(insert_position, stmt);
  return true;
}

void ensureStatementInScopeBeforeTarget(SgScopeStatement *scope,
                                        SgStatement *stmt,
                                        SgStatement *target) {
  if (scope == nullptr || stmt == nullptr) {
    return;
  }

  if (ensureDeclarationInScopeBeforeTarget(scope,
                                           isSgDeclarationStatement(stmt),
                                           isSgDeclarationStatement(target))) {
    return;
  }

  if (scopeContainsStatement(scope, stmt)) {
    if (stmt->get_parent() != scope) {
      stmt->set_parent(scope);
    }
    return;
  }

  if (target != nullptr && scopeContainsStatement(scope, target)) {
    scope->insert_statement(target, stmt, true);
    return;
  }

  scope->append_statement(stmt);
}

SgFunctionParameterList *cloneParameterList(SgFunctionDeclaration *decl) {
  if (decl == nullptr || decl->get_parameterList() == nullptr) {
    return SageBuilder::buildFunctionParameterList_nfi();
  }

  SgFunctionParameterList *copied = isSgFunctionParameterList(
      SageInterface::deepCopy(decl->get_parameterList()));
  return copied != nullptr ? copied
                           : SageBuilder::buildFunctionParameterList_nfi();
}

bool haveEquivalentTemplateArguments(const SgTemplateArgumentPtrList &lhs,
                                     const SgTemplateArgumentPtrList &rhs) {
  return SageInterface::templateArgumentListEquivalence(lhs, rhs);
}

SgName templateInstantiationBaseName(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return SgName();
  }

  if (SgTemplateInstantiationFunctionDecl *inst =
          isSgTemplateInstantiationFunctionDecl(decl)) {
    if (inst->get_templateName().is_null() == false) {
      return inst->get_templateName();
    }
  }

  if (SgTemplateInstantiationMemberFunctionDecl *inst =
          isSgTemplateInstantiationMemberFunctionDecl(decl)) {
    if (inst->get_templateName().is_null() == false) {
      return inst->get_templateName();
    }
  }

  auto strip_template_args = [](const SgName &name) -> SgName {
    std::string spelled = name.getString();
    std::string::size_type template_pos = spelled.find('<');
    if (template_pos == std::string::npos) {
      return name;
    }

    std::string base = spelled.substr(0, template_pos);
    while (!base.empty() &&
           std::isspace(static_cast<unsigned char>(base.back()))) {
      base.pop_back();
    }
    return SgName(base);
  };

  if (isSgTemplateInstantiationFunctionDecl(decl) != nullptr ||
      isSgTemplateInstantiationMemberFunctionDecl(decl) != nullptr) {
    return strip_template_args(decl->get_name());
  }

  return decl->get_name();
}

bool sameTemplateInstantiationMemberFunctionSignature(
    SgTemplateInstantiationMemberFunctionDecl *lhs,
    SgTemplateInstantiationMemberFunctionDecl *rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }

  if (templateInstantiationBaseName(lhs) !=
      templateInstantiationBaseName(rhs)) {
    return false;
  }

  if (!SageInterface::isEquivalentType(lhs->get_type(), rhs->get_type())) {
    return false;
  }

  return haveEquivalentTemplateArguments(lhs->get_templateArguments(),
                                         rhs->get_templateArguments());
}

bool sameTemplateInstantiationFunctionSignature(
    SgTemplateInstantiationFunctionDecl *lhs,
    SgTemplateInstantiationFunctionDecl *rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }

  if (templateInstantiationBaseName(lhs) !=
      templateInstantiationBaseName(rhs)) {
    return false;
  }

  if (!SageInterface::isEquivalentType(lhs->get_type(), rhs->get_type())) {
    return false;
  }

  return haveEquivalentTemplateArguments(lhs->get_templateArguments(),
                                         rhs->get_templateArguments());
}

SgTemplateInstantiationFunctionDecl *
canonicalFirstNondefiningFunctionInstantiation(
    SgTemplateInstantiationFunctionDecl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (SgTemplateInstantiationFunctionDecl *first_nondef =
          isSgTemplateInstantiationFunctionDecl(
              decl->get_firstNondefiningDeclaration())) {
    return first_nondef;
  }

  return decl;
}

using TemplateInstantiationFunctionBucket =
    std::vector<SgTemplateInstantiationFunctionDecl *>;
using TemplateInstantiationFunctionBucketMap =
    std::unordered_map<std::string, TemplateInstantiationFunctionBucket>;
using TemplateInstantiationFunctionProjectBuckets =
    std::unordered_map<SgProject *, TemplateInstantiationFunctionBucketMap>;

TemplateInstantiationFunctionProjectBuckets &
templateInstantiationFunctionRepairBuckets() {
  static TemplateInstantiationFunctionProjectBuckets buckets;
  return buckets;
}

int &templateInstantiationFunctionRepairCacheDepth() {
  static int depth = 0;
  return depth;
}

void clearTemplateInstantiationFunctionRepairBuckets() {
  templateInstantiationFunctionRepairBuckets().clear();
}

void cacheTemplateInstantiationFunctionForRepair(
    SgTemplateInstantiationFunctionDecl *decl) {
  if (decl == nullptr) {
    return;
  }

  SgProject *project = projectForMemoryPoolNode(decl);
  if (project == nullptr) {
    return;
  }

  TemplateInstantiationFunctionBucket &bucket =
      templateInstantiationFunctionRepairBuckets()
          [project][templateInstantiationBaseName(decl).getString()];
  if (std::find(bucket.begin(), bucket.end(), decl) == bucket.end()) {
    bucket.push_back(decl);
  }
}

void populateTemplateInstantiationFunctionRepairBuckets(SgNode *root) {
  if (root == nullptr) {
    return;
  }

  class Traversal : public AstSimpleProcessing {
  public:
    void visit(SgNode *node) override {
      cacheTemplateInstantiationFunctionForRepair(
          isSgTemplateInstantiationFunctionDecl(node));
    }
  } traversal;

  traversal.traverse(root, preorder);
}

class TemplateInstantiationFunctionRepairCacheGuard {
public:
  explicit TemplateInstantiationFunctionRepairCacheGuard(SgNode *root) {
    if (templateInstantiationFunctionRepairCacheDepth()++ == 0) {
      clearTemplateInstantiationFunctionRepairBuckets();
      populateTemplateInstantiationFunctionRepairBuckets(root);
    }
  }

  ~TemplateInstantiationFunctionRepairCacheGuard() {
    if (--templateInstantiationFunctionRepairCacheDepth() == 0) {
      clearTemplateInstantiationFunctionRepairBuckets();
    }
  }
};

SgTemplateInstantiationFunctionDecl *
findExistingFirstNondefiningFunctionInstantiation(
    SgTemplateInstantiationFunctionDecl *inst) {
  if (inst == nullptr) {
    return nullptr;
  }

  auto pick_candidate = [&](SgTemplateInstantiationFunctionDecl *candidate)
      -> SgTemplateInstantiationFunctionDecl * {
    if (candidate == nullptr || candidate == inst) {
      return nullptr;
    }
    if (!sameTemplateInstantiationFunctionSignature(candidate, inst)) {
      return nullptr;
    }
    if (candidate->get_definingDeclaration() == inst &&
        candidate->get_definition() == nullptr) {
      return candidate;
    }
    return canonicalFirstNondefiningFunctionInstantiation(candidate);
  };

  if (SgFunctionSymbol *symbol =
          isSgFunctionSymbol(inst->get_symbol_from_symbol_table())) {
    if (SgTemplateInstantiationFunctionDecl *candidate = pick_candidate(
            isSgTemplateInstantiationFunctionDecl(symbol->get_declaration()))) {
      return candidate;
    }
    if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
            symbol->get_declaration()->get_firstNondefiningDeclaration())) {
      if (SgTemplateInstantiationFunctionDecl *candidate = pick_candidate(
              isSgTemplateInstantiationFunctionDecl(first_nondef))) {
        return candidate;
      }
    }
  }

  if (SgScopeStatement *scope = selectTemplateLookupScope(inst)) {
    for (SgStatement *stmt : scope->generateStatementList()) {
      if (SgTemplateInstantiationFunctionDecl *candidate =
              pick_candidate(isSgTemplateInstantiationFunctionDecl(stmt))) {
        return candidate;
      }
    }
  }

  SgProject *target_project = projectForMemoryPoolNode(inst);
  TemplateInstantiationFunctionProjectBuckets::const_iterator project_it =
      templateInstantiationFunctionRepairBuckets().find(target_project);
  if (project_it == templateInstantiationFunctionRepairBuckets().end()) {
    return nullptr;
  }

  TemplateInstantiationFunctionBucketMap::const_iterator bucket_it =
      project_it->second.find(templateInstantiationBaseName(inst).getString());
  if (bucket_it == project_it->second.end()) {
    return nullptr;
  }

  SgTemplateInstantiationFunctionDecl *fallback = nullptr;
  for (SgTemplateInstantiationFunctionDecl *entry : bucket_it->second) {
    SgTemplateInstantiationFunctionDecl *candidate = pick_candidate(entry);
    if (candidate == nullptr) {
      continue;
    }

    if (candidate->get_definingDeclaration() == inst) {
      return candidate;
    }

    if (fallback == nullptr) {
      fallback = candidate;
    }
  }

  return fallback;
}

void copyTemplateInstantiationFunctionMetadata(
    SgTemplateInstantiationFunctionDecl *src,
    SgTemplateInstantiationFunctionDecl *dst) {
  if (src == nullptr || dst == nullptr) {
    return;
  }

  dst->set_templateDeclaration(src->get_templateDeclaration());
  dst->set_specializedTemplateDeclaration(
      src->get_specializedTemplateDeclaration());
  dst->set_template_argument_list_is_explicit(
      src->get_template_argument_list_is_explicit());
  dst->set_specialization(src->get_specialization());
  dst->set_nameResetFromMangledForm(src->get_nameResetFromMangledForm());
  dst->set_templateName(src->get_templateName());

  SgTemplateArgumentPtrList copied_template_args;
  for (SgTemplateArgument *arg : src->get_templateArguments()) {
    copied_template_args.push_back(
        isSgTemplateArgument(SageInterface::deepCopy(arg)));
  }
  SageBuilder::setTemplateArgumentsInDeclaration(dst, &copied_template_args);

  dst->get_deducedTemplateArguments().clear();
  for (SgTemplateArgument *arg : src->get_deducedTemplateArguments()) {
    dst->get_deducedTemplateArguments().push_back(
        isSgTemplateArgument(SageInterface::deepCopy(arg)));
  }

  SageBuilder::setTemplateArgumentParents(dst);
}

void ensureTemplateInstantiationFunctionDeclarationChain(
    SgTemplateInstantiationFunctionDecl *inst) {
  if (inst == nullptr || inst->get_definingDeclaration() != inst) {
    return;
  }

  SgDeclarationStatement *first_nondef_decl =
      inst->get_firstNondefiningDeclaration();
  if (first_nondef_decl != nullptr && first_nondef_decl != inst &&
      first_nondef_decl != inst->get_definingDeclaration()) {
    return;
  }

  SgTemplateInstantiationFunctionDecl *first_nondef =
      findExistingFirstNondefiningFunctionInstantiation(inst);

  if (first_nondef == nullptr) {
    SgScopeStatement *scope = selectTemplateLookupScope(inst);
    SgFunctionType *function_type = isSgFunctionType(inst->get_type());
    if (scope == nullptr || function_type == nullptr ||
        function_type->get_return_type() == nullptr) {
      return;
    }

    SgFunctionParameterList *param_list = cloneParameterList(inst);
    SgTemplateArgumentPtrList template_args;
    for (SgTemplateArgument *arg : inst->get_templateArguments()) {
      template_args.push_back(
          isSgTemplateArgument(SageInterface::deepCopy(arg)));
    }

    SgFunctionDeclaration *synthesized_decl =
        SageBuilder::buildNondefiningFunctionDeclaration(
            inst->get_name(), function_type->get_return_type(), param_list,
            scope, /*buildTemplateInstantiation=*/true, &template_args,
            SgStorageModifier::e_default, /*forceFreeFunctionScope=*/false);

    first_nondef = canonicalFirstNondefiningFunctionInstantiation(
        isSgTemplateInstantiationFunctionDecl(synthesized_decl));
    if (first_nondef == nullptr) {
      return;
    }

    copyTemplateInstantiationFunctionMetadata(inst, first_nondef);
    first_nondef->setForward();
    hideSynthesizedFunctionDeclaration(first_nondef);
    ensureStatementInScopeBeforeTarget(scope, first_nondef, inst);
    cacheTemplateInstantiationFunctionForRepair(first_nondef);
  }

  if (first_nondef == nullptr || first_nondef == inst) {
    return;
  }

  first_nondef->set_firstNondefiningDeclaration(first_nondef);
  first_nondef->set_definingDeclaration(inst);
  inst->set_definingDeclaration(inst);
  inst->set_firstNondefiningDeclaration(first_nondef);
}

SgTemplateInstantiationMemberFunctionDecl *
canonicalFirstNondefiningMemberInstantiation(
    SgTemplateInstantiationMemberFunctionDecl *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (SgTemplateInstantiationMemberFunctionDecl *first_nondef =
          isSgTemplateInstantiationMemberFunctionDecl(
              decl->get_firstNondefiningDeclaration())) {
    return first_nondef;
  }

  return decl;
}

SgTemplateInstantiationMemberFunctionDecl *
findExistingFirstNondefiningMemberInstantiation(
    SgTemplateInstantiationMemberFunctionDecl *inst) {
  if (inst == nullptr) {
    return nullptr;
  }

  auto pick_candidate =
      [&](SgTemplateInstantiationMemberFunctionDecl *candidate)
      -> SgTemplateInstantiationMemberFunctionDecl * {
    if (candidate == nullptr || candidate == inst) {
      return nullptr;
    }
    if (!sameTemplateInstantiationMemberFunctionSignature(candidate, inst)) {
      return nullptr;
    }
    return canonicalFirstNondefiningMemberInstantiation(candidate);
  };

  if (SgMemberFunctionSymbol *symbol =
          isSgMemberFunctionSymbol(inst->get_symbol_from_symbol_table())) {
    if (SgTemplateInstantiationMemberFunctionDecl *candidate =
            pick_candidate(isSgTemplateInstantiationMemberFunctionDecl(
                symbol->get_declaration()))) {
      return candidate;
    }
  }

  SgScopeStatement *scope = selectMemberInstantiationScope(inst);
  SgMemberFunctionType *member_type = isSgMemberFunctionType(inst->get_type());
  if (scope != nullptr && member_type != nullptr) {
    SgName lookup_name = inst->get_name();
    SgTemplateArgumentPtrList &template_args = inst->get_templateArguments();
    lookup_name =
        SageBuilder::appendTemplateArgumentsToName(lookup_name, template_args);

    if (SgMemberFunctionSymbol *symbol =
            isSgMemberFunctionSymbol(scope->find_symbol_by_type_of_function<
                                     SgTemplateInstantiationMemberFunctionDecl>(
                lookup_name, member_type, nullptr, &template_args))) {
      if (SgTemplateInstantiationMemberFunctionDecl *candidate =
              pick_candidate(isSgTemplateInstantiationMemberFunctionDecl(
                  symbol->get_declaration()))) {
        return candidate;
      }
    }

    for (SgStatement *stmt : scope->generateStatementList()) {
      if (SgTemplateInstantiationMemberFunctionDecl *candidate = pick_candidate(
              isSgTemplateInstantiationMemberFunctionDecl(stmt))) {
        return candidate;
      }
    }
  }

  return nullptr;
}

void copyTemplateInstantiationMemberFunctionMetadata(
    SgTemplateInstantiationMemberFunctionDecl *src,
    SgTemplateInstantiationMemberFunctionDecl *dst) {
  if (src == nullptr || dst == nullptr) {
    return;
  }

  dst->set_templateDeclaration(src->get_templateDeclaration());
  dst->set_specializedTemplateDeclaration(
      src->get_specializedTemplateDeclaration());
  dst->set_template_argument_list_is_explicit(
      src->get_template_argument_list_is_explicit());
  dst->set_specialization(src->get_specialization());
  dst->set_nameResetFromMangledForm(src->get_nameResetFromMangledForm());
  dst->set_templateName(src->get_templateName());

  SgTemplateArgumentPtrList copied_template_args;
  for (SgTemplateArgument *arg : src->get_templateArguments()) {
    copied_template_args.push_back(
        isSgTemplateArgument(SageInterface::deepCopy(arg)));
  }
  SageBuilder::setTemplateArgumentsInDeclaration(dst, &copied_template_args);

  dst->get_deducedTemplateArguments().clear();
  for (SgTemplateArgument *arg : src->get_deducedTemplateArguments()) {
    dst->get_deducedTemplateArguments().push_back(
        isSgTemplateArgument(SageInterface::deepCopy(arg)));
  }

  // The deduced-argument copies are created after
  // setTemplateArgumentsInDeclaration() has already repaired template-argument
  // ownership, so re-run the declaration-level parent fix to attach both lists
  // to the synthesized declaration chain.
  SageBuilder::setTemplateArgumentParents(dst);
}

void ensureTemplateInstantiationMemberFunctionDeclarationChain(
    SgTemplateInstantiationMemberFunctionDecl *inst) {
  if (inst == nullptr || inst->get_definition() == nullptr ||
      inst->get_definingDeclaration() != inst) {
    return;
  }

  SgDeclarationStatement *first_nondef_decl =
      inst->get_firstNondefiningDeclaration();
  if (first_nondef_decl != nullptr && first_nondef_decl != inst &&
      first_nondef_decl != inst->get_definingDeclaration()) {
    return;
  }

  SgTemplateInstantiationMemberFunctionDecl *first_nondef =
      findExistingFirstNondefiningMemberInstantiation(inst);

  if (first_nondef == nullptr) {
    SgScopeStatement *scope = selectMemberInstantiationScope(inst);
    SgMemberFunctionType *member_type =
        isSgMemberFunctionType(inst->get_type());
    if (scope == nullptr || member_type == nullptr ||
        member_type->get_return_type() == nullptr) {
      return;
    }

    SgFunctionParameterList *param_list = cloneParameterList(inst);
    SgTemplateArgumentPtrList template_args;
    for (SgTemplateArgument *arg : inst->get_templateArguments()) {
      template_args.push_back(
          isSgTemplateArgument(SageInterface::deepCopy(arg)));
    }

    SgMemberFunctionDeclaration *synthesized_decl =
        SageBuilder::buildNondefiningMemberFunctionDeclaration(
            inst->get_name(), member_type->get_return_type(), param_list, scope,
            member_type->get_mfunc_specifier(),
            /*buildTemplateInstantiation=*/true, &template_args);

    first_nondef = canonicalFirstNondefiningMemberInstantiation(
        isSgTemplateInstantiationMemberFunctionDecl(synthesized_decl));
    if (first_nondef == nullptr) {
      return;
    }

    copyTemplateInstantiationMemberFunctionMetadata(inst, first_nondef);
    first_nondef->setForward();
    hideSynthesizedFunctionDeclaration(first_nondef);
    ensureStatementInScopeBeforeTarget(scope, first_nondef, inst);
  }

  if (first_nondef == nullptr || first_nondef == inst) {
    return;
  }

  first_nondef->set_firstNondefiningDeclaration(first_nondef);
  first_nondef->set_definingDeclaration(inst);
  inst->set_definingDeclaration(inst);
  inst->set_firstNondefiningDeclaration(first_nondef);
}

void canonicalizeClassTypeToFirstNondefiningDeclaration(
    SgClassDeclaration *decl) {
  if (decl == nullptr) {
    return;
  }

  SgClassDeclaration *first_nondef =
      isSgClassDeclaration(decl->get_firstNondefiningDeclaration());
  if (first_nondef == nullptr) {
    first_nondef = decl;
  }

  SgClassDeclaration *defining_decl =
      isSgClassDeclaration(first_nondef->get_definingDeclaration());
  if (defining_decl == nullptr) {
    defining_decl = isSgClassDeclaration(decl->get_definingDeclaration());
  }

  auto belongs_to_current_chain = [&](SgClassDeclaration *candidate) {
    if (candidate == nullptr) {
      return false;
    }

    if (candidate == decl || candidate == first_nondef ||
        candidate == defining_decl) {
      return true;
    }

    SgClassDeclaration *candidate_first =
        isSgClassDeclaration(candidate->get_firstNondefiningDeclaration());
    SgClassDeclaration *candidate_def =
        isSgClassDeclaration(candidate->get_definingDeclaration());

    return candidate_first == first_nondef || candidate_first == decl ||
           candidate_def == defining_decl || candidate_def == decl;
  };

  auto type_belongs_to_current_chain = [&](SgClassType *candidate_type) {
    if (candidate_type == nullptr) {
      return false;
    }

    SgClassDeclaration *type_decl =
        isSgClassDeclaration(candidate_type->get_declaration());
    return type_decl == nullptr || belongs_to_current_chain(type_decl);
  };

  bool saw_foreign_class_type = false;
  auto select_canonical_type = [&](SgClassDeclaration *candidate_decl) {
    if (candidate_decl == nullptr) {
      return static_cast<SgClassType *>(nullptr);
    }

    SgClassType *candidate_type = isSgClassType(candidate_decl->get_type());
    if (type_belongs_to_current_chain(candidate_type)) {
      return candidate_type;
    }
    if (candidate_type != nullptr) {
      saw_foreign_class_type = true;
    }

    return static_cast<SgClassType *>(nullptr);
  };

  SgClassType *canonical_type = select_canonical_type(first_nondef);
  if (canonical_type == nullptr) {
    canonical_type = select_canonical_type(decl);
  }
  if (canonical_type == nullptr) {
    canonical_type = select_canonical_type(defining_decl);
  }
  if (canonical_type == nullptr && saw_foreign_class_type &&
      first_nondef->get_firstNondefiningDeclaration() != nullptr) {
    canonical_type = SgClassType::createType(first_nondef);
  }
  if (canonical_type == nullptr) {
    return;
  }

  auto attach_type = [&](SgClassDeclaration *candidate) {
    if (candidate != nullptr && candidate->get_type() != canonical_type) {
      candidate->set_type(canonical_type);
    }
  };

  attach_type(first_nondef);
  attach_type(defining_decl);
  attach_type(decl);

  if (type_belongs_to_current_chain(canonical_type) &&
      canonical_type->get_declaration() != first_nondef) {
    canonical_type->set_declaration(first_nondef);
  }
}

class CanonicalizeClassTypesOnMemoryPool : public ROSE_VisitTraversal {
public:
  void visit(SgNode *node) override {
    SgClassType *class_type = isSgClassType(node);
    if (class_type == nullptr) {
      return;
    }

    SgClassDeclaration *decl =
        isSgClassDeclaration(class_type->get_declaration());
    if (decl == nullptr) {
      return;
    }

    SgClassDeclaration *first_nondef =
        isSgClassDeclaration(decl->get_firstNondefiningDeclaration());
    if (first_nondef == nullptr) {
      first_nondef = decl;
    }

    canonicalizeClassTypeToFirstNondefiningDeclaration(decl);

    if (class_type->get_declaration() != first_nondef) {
      class_type->set_declaration(first_nondef);
    }
  }
};

void normalizeNamespaceTemplateDeclarationFlags(
    SgTemplateClassDeclaration *decl);

void ensureTemplateInstantiationDeclarationLink(
    SgTemplateInstantiationDecl *inst) {
  recoverTemplateDeclarationLink<SgTemplateInstantiationDecl,
                                 SgTemplateClassDeclaration>(inst);

  if (inst == nullptr || inst->get_templateDeclaration() != nullptr) {
    return;
  }

  SgName template_name = inst->get_templateName();
  if (template_name.getString().empty()) {
    template_name = inst->get_name();
  }
  if (template_name.getString().empty()) {
    return;
  }

  SgScopeStatement *lookup_scope = selectTemplateLookupScope(inst);
  if (lookup_scope == nullptr) {
    return;
  }

  SgTemplateClassSymbol *tmpl_sym = lookup_scope->lookup_template_class_symbol(
      template_name, nullptr, nullptr);
  if (tmpl_sym == nullptr) {
    tmpl_sym = SageInterface::lookupTemplateClassSymbolInParentScopes(
        template_name, nullptr, nullptr, lookup_scope);
  }
  if (tmpl_sym != nullptr) {
    inst->set_templateDeclaration(
        isSgTemplateClassDeclaration(tmpl_sym->get_declaration()));
    return;
  }

  SgTemplateParameterPtrList empty_params;
  SgTemplateArgumentPtrList empty_args;
  SgTemplateClassDeclaration *stub =
      SageBuilder::buildNondefiningTemplateClassDeclaration_nfi(
          template_name, inst->get_class_type(), lookup_scope, &empty_params,
          &empty_args);
  if (stub == nullptr) {
    return;
  }

  stub->setForward();
  stub->set_firstNondefiningDeclaration(stub);
  stub->set_definingDeclaration(nullptr);
  if (isSgGlobal(lookup_scope) != nullptr ||
      isSgNamespaceDefinitionStatement(lookup_scope) != nullptr) {
    normalizeNamespaceTemplateDeclarationFlags(stub);
  } else if (stub->get_file_info() != nullptr) {
    stub->get_file_info()->setCompilerGenerated();
    stub->get_file_info()->unsetOutputInCodeGeneration();
  }
  inst->set_templateDeclaration(stub);
}

void ensureTemplateInstantiationFunctionDeclarationLink(
    SgTemplateInstantiationFunctionDecl *inst) {
  recoverTemplateDeclarationLink<SgTemplateInstantiationFunctionDecl,
                                 SgTemplateFunctionDeclaration>(inst);

  if (inst == nullptr || inst->get_templateDeclaration() != nullptr) {
    return;
  }

  SgFunctionType *function_type = isSgFunctionType(inst->get_type());
  if (function_type == nullptr) {
    return;
  }

  SgName template_name = inst->get_templateName();
  if (template_name.getString().empty()) {
    template_name = inst->get_name();
  }
  if (template_name.getString().empty()) {
    return;
  }

  SgScopeStatement *lookup_scope = selectTemplateLookupScope(inst);
  if (lookup_scope == nullptr) {
    return;
  }

  SgTemplateParameterPtrList empty_params;
  SgTemplateFunctionSymbol *tmpl_sym =
      lookup_scope->lookup_template_function_symbol(
          template_name, function_type, &empty_params);
  if (tmpl_sym == nullptr) {
    tmpl_sym = isSgTemplateFunctionSymbol(
        SageInterface::lookupTemplateFunctionSymbolInParentScopes(
            template_name, function_type, &empty_params, lookup_scope));
  }
  if (tmpl_sym != nullptr) {
    inst->set_templateDeclaration(
        isSgTemplateFunctionDeclaration(tmpl_sym->get_declaration()));
  }
}

void ensureTemplateInstantiationMemberFunctionDeclarationLink(
    SgTemplateInstantiationMemberFunctionDecl *inst) {
  if (inst != nullptr &&
      inst->get_specialization() == SgDeclarationStatement::e_specialization &&
      !inst->get_template_argument_list_is_explicit() &&
      inst->get_templateArguments().empty() &&
      inst->get_deducedTemplateArguments().empty()) {
    inst->set_templateDeclaration(nullptr);
    inst->set_specializedTemplateDeclaration(nullptr);
    return;
  }

  recoverTemplateDeclarationLink<SgTemplateInstantiationMemberFunctionDecl,
                                 SgTemplateMemberFunctionDeclaration>(inst);

  if (inst == nullptr || inst->get_templateDeclaration() != nullptr) {
    return;
  }

  SgFunctionType *function_type = isSgFunctionType(inst->get_type());
  if (function_type == nullptr) {
    return;
  }

  SgName template_name = inst->get_templateName();
  if (template_name.getString().empty()) {
    template_name = inst->get_name();
  }
  if (template_name.getString().empty()) {
    return;
  }

  SgScopeStatement *lookup_scope = selectTemplateLookupScope(inst);
  if (lookup_scope == nullptr) {
    return;
  }

  SgTemplateParameterPtrList empty_params;
  SgTemplateMemberFunctionSymbol *tmpl_sym =
      lookup_scope->lookup_template_member_function_symbol(
          template_name, function_type, &empty_params);
  if (tmpl_sym == nullptr) {
    tmpl_sym = isSgTemplateMemberFunctionSymbol(
        SageInterface::lookupTemplateMemberFunctionSymbolInParentScopes(
            template_name, function_type, &empty_params, lookup_scope));
  }
  if (tmpl_sym != nullptr) {
    inst->set_templateDeclaration(
        isSgTemplateMemberFunctionDeclaration(tmpl_sym->get_declaration()));
  }
}

class RepairTemplateInstantiationLinksOnMemoryPool
    : public ROSE_VisitTraversal {
public:
  void visit(SgNode *node) override {
    ensureTemplateInstantiationFunctionDeclarationChain(
        isSgTemplateInstantiationFunctionDecl(node));
    ensureTemplateInstantiationDeclarationLink(
        isSgTemplateInstantiationDecl(node));
    ensureTemplateInstantiationFunctionDeclarationLink(
        isSgTemplateInstantiationFunctionDecl(node));
    ensureTemplateInstantiationMemberFunctionDeclarationLink(
        isSgTemplateInstantiationMemberFunctionDecl(node));
  }
};

void normalizeNamespaceTemplateDeclarationFlags(
    SgTemplateClassDeclaration *decl) {
  auto normalize_one = [](SgTemplateClassDeclaration *candidate) {
    if (candidate == nullptr) {
      return;
    }
    SgScopeStatement *scope = candidate->get_scope();
    SgNode *parent = candidate->get_parent();
    const bool namespace_or_global_scope =
        isSgGlobal(scope) != nullptr ||
        isSgNamespaceDefinitionStatement(scope) != nullptr ||
        isSgGlobal(parent) != nullptr ||
        isSgNamespaceDefinitionStatement(parent) != nullptr;
    if (!namespace_or_global_scope) {
      return;
    }
    if (Sg_File_Info *fi = candidate->get_file_info()) {
      fi->unsetCompilerGenerated();
      fi->unsetFrontendSpecific();
      fi->unsetOutputInCodeGeneration();
    }
    if (Sg_File_Info *fi = candidate->get_endOfConstruct()) {
      fi->unsetCompilerGenerated();
      fi->unsetFrontendSpecific();
      fi->unsetOutputInCodeGeneration();
    }
  };

  normalize_one(decl);
  if (SgTemplateClassDeclaration *first = isSgTemplateClassDeclaration(
          decl != nullptr ? decl->get_firstNondefiningDeclaration()
                          : nullptr)) {
    if (first != decl) {
      normalize_one(first);
    }
  }
  if (SgTemplateClassDeclaration *def = isSgTemplateClassDeclaration(
          decl != nullptr ? decl->get_definingDeclaration() : nullptr)) {
    if (def != decl) {
      normalize_one(def);
    }
  }
}
} // namespace

void repairTemplateInstantiationDeclLinksInMemoryPool();

void fixupTemplateInstantiations(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Fixup template specializations:");
  TemplateInstantiationFunctionRepairCacheGuard function_cache_guard(node);

  // This simplifies how the traversal is called!
  FixupTemplateInstantiations declarationFixupTraversal;

  // I think the default should be preorder so that the interfaces would be more
  // uniform
  declarationFixupTraversal.traverse(node, preorder);

  repairTemplateInstantiationDeclLinksInMemoryPool();
  canonicalizeClassTypesInMemoryPool(node);
}

void canonicalizeClassTypesInMemoryPool(SgNode *root) {
  // Canonicalize all class types in the process memory pool. AST consistency
  // checks traverse the whole pool, including detached helper types whose
  // project cannot be recovered through parent links, so the repair pass must
  // cover that same surface area.
  (void)root;
  MemoryPoolTraversalFilterGuard clear_filter(nullptr);
  CanonicalizeClassTypesOnMemoryPool memory_pool_fixup;
  SgClassType::traverseMemoryPoolNodes(memory_pool_fixup);
}

void repairTemplateInstantiationDeclLinksInMemoryPool() {
  MemoryPoolTraversalFilterGuard clear_filter(nullptr);
  RepairTemplateInstantiationLinksOnMemoryPool memory_pool_fixup;
  SgTemplateInstantiationDecl::traverseMemoryPoolNodes(memory_pool_fixup);
  SgTemplateInstantiationFunctionDecl::traverseMemoryPoolNodes(
      memory_pool_fixup);
  SgTemplateInstantiationMemberFunctionDecl::traverseMemoryPoolNodes(
      memory_pool_fixup);
}

void FixupTemplateInstantiations::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(node)) {
    canonicalizeClassTypeToFirstNondefiningDeclaration(class_decl);
  }
  if (SgClassType *class_type = isSgClassType(node)) {
    canonicalizeClassTypeToFirstNondefiningDeclaration(
        isSgClassDeclaration(class_type->get_declaration()));
  }

  // Ensure template instantiations reference a valid template declaration.
  if (SgTemplateInstantiationDecl *inst = isSgTemplateInstantiationDecl(node)) {
    ensureTemplateInstantiationDeclarationLink(inst);
    if (SgTemplateClassDeclaration *template_decl =
            isSgTemplateClassDeclaration(inst->get_templateDeclaration())) {
      normalizeNamespaceTemplateDeclarationFlags(template_decl);
    }
  }
  if (SgTemplateInstantiationFunctionDecl *inst =
          isSgTemplateInstantiationFunctionDecl(node)) {
    ensureTemplateInstantiationFunctionDeclarationChain(inst);
    ensureTemplateInstantiationFunctionDeclarationLink(inst);
  }
  if (SgTemplateInstantiationMemberFunctionDecl *inst =
          isSgTemplateInstantiationMemberFunctionDecl(node)) {
    ensureTemplateInstantiationMemberFunctionDeclarationChain(inst);
    ensureTemplateInstantiationMemberFunctionDeclarationLink(inst);
  }

  // Take care of marking the whole subtree of any declarations
  // that the legacy frontend/Sage connection marked as compiler generated.
  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);

  // DQ (1/18/2014): Testcode test2012_75.c demonstrates why we need to force
  // the function parameters to be marked as compiler generated.  Else there are
  // errors in how comments are woven back into the AST. DQ (1/18/2014): Skip
  // function parameter lists, since they are always marked as compiler
  // generated (because we don't have source position information for them).
  // Perhaps marking it as frontend specific would be more appropriate).
  // if (declaration != NULL)
  // if (declaration != NULL && isSgFunctionParameterList(declaration) == NULL)
  if (declaration != NULL) {
    if (declaration->get_file_info() == NULL) {
      printf("Error: (declaration->get_file_info() == NULL) declaration = %p = "
             "%s = %s \n",
             declaration, declaration->class_name().c_str(),
             SageInterface::get_name(declaration).c_str());
    }
    ROSE_ASSERT(declaration->get_file_info() != NULL);

    // DQ (6/17/2005): compiler generated does not imply that it will be
    // output by the unparser (anymore) Some declarations are marked as
    // compiler generated in the legacy frontend/Sage III translation, but
    // the whole subtree is never marked at that point.  This step marks
    // the whole subtree as compiler generated when just the declaration
    // is detected as having been marked in the legacy frontend/Sage III
    // translation.
    if (declaration->get_file_info()->isCompilerGenerated() == true &&
        !isLambdaClosureClassDeclaration(declaration)) {

      // DQ (8/10/2005): We should never mark a template declaration as compiler
      // generated (though perhaps partial specializations could be marked as
      // such later). ROSE_ASSERT(isSgTemplateDeclaration(node) == NULL);
      if (isSgTemplateDeclaration(node) == NULL) {
        // Mark the whole declaration as compiler generated since we
        // could not do so in the legacy frontend/Sage III translation
        markAsCompilerGenerated(declaration);
      }
    }
  }
}
