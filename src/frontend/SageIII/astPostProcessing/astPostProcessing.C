// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "AstFixup.h"

#include "astPostProcessing.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

// DQ (8/20/2005): Make this local so that it can't be called externally!
void postProcessingSupport(SgNode *node);

SgSymbolTable &get_orphan_symbol_table() {
  static std::unique_ptr<SgSymbolTable> orphan_table;
  if (!orphan_table) {
    orphan_table.reset(new SgSymbolTable());
  }
  return *orphan_table;
}

void move_symbol_to_orphan_table(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return;
  }
  if (SgSymbolTable *parent_table = isSgSymbolTable(symbol->get_parent())) {
    if (parent_table->exists(symbol)) {
      return;
    }
  }
  // Keep detached symbols alive without reintroducing them into a scope.
  SgSymbolTable &orphan_table = get_orphan_symbol_table();
  if (orphan_table.exists(symbol)) {
    return;
  }
  orphan_table.insert(symbol->get_name(), symbol);
}

namespace {
// Global hook to let memory-pool traversals ignore unwanted nodes (e.g., Clang
// system-header template instantiations that are not part of the user AST).
Rose::MemoryPoolTraversalFilter s_memoryPoolTraversalFilter = NULL;
const std::unordered_set<SgNode *> *s_reachableNodes = NULL;

bool isUnreachableFromProject(SgNode *n) {
  if (s_reachableNodes == NULL)
    return false;

  return s_reachableNodes->find(n) == s_reachableNodes->end();
}

struct MemoryPoolFilterGuard {
  Rose::MemoryPoolTraversalFilter previous;
  std::unordered_set<SgNode *> reachable;

  explicit MemoryPoolFilterGuard(Rose::MemoryPoolTraversalFilter next,
                                 SgNode *root)
      : previous(Rose::getMemoryPoolTraversalFilter()) {
    if (next != NULL && root != NULL) {
      // Collect all nodes reachable from the current AST root so memory-pool
      // traversals can ignore stray nodes left in pools by the frontend.
      struct Collector : public AstSimpleProcessing {
        std::unordered_set<SgNode *> &set;
        explicit Collector(std::unordered_set<SgNode *> &s) : set(s) {}
        void visit(SgNode *n) override { set.insert(n); }
      } collector(reachable);
      collector.traverse(root, preorder);

      s_reachableNodes = &reachable;
      Rose::setMemoryPoolTraversalFilter(next);
    } else {
      Rose::setMemoryPoolTraversalFilter(next);
    }
  }

  ~MemoryPoolFilterGuard() {
    s_reachableNodes = NULL;
    Rose::setMemoryPoolTraversalFilter(previous);
  }
};

bool usesClangFrontend(SgNode *node) {
  if (SgProject *project = isSgProject(node)) {
    for (int i = 0; i < project->numberOfFiles(); ++i) {
      if (SgSourceFile *sourceFile = isSgSourceFile(&(project->get_file(i)))) {
        if (sourceFile->get_C_only() || sourceFile->get_Cxx_only() ||
            sourceFile->get_Cuda_only() || sourceFile->get_OpenCL_only())
          return true;
      }
    }
  } else if (SgSourceFile *sourceFile = isSgSourceFile(node)) {
    if (sourceFile->get_C_only() || sourceFile->get_Cxx_only() ||
        sourceFile->get_Cuda_only() || sourceFile->get_OpenCL_only())
      return true;
  }

  return false;
}

bool constraintFailureOnNode(SgNode *node) {
  auto failed = [](auto *target) -> bool {
    if (target == nullptr) {
      return false;
    }
    if (target->get_constraintSatisfactionEvaluated()) {
      if (!target->get_constraintSatisfactionSatisfied() ||
          target->get_constraintSatisfactionContainsErrors() ||
          target->get_constraintSatisfactionSubstitutionFailure()) {
        return true;
      }
    }
    if (target->get_sfinaeEvaluated() &&
        target->get_sfinaeSubstitutionFailure()) {
      return true;
    }
    return false;
  };

  if (auto *inst = isSgTemplateInstantiationMemberFunctionDecl(node)) {
    return failed(inst);
  }
  if (auto *inst = isSgTemplateInstantiationFunctionDecl(node)) {
    return failed(inst);
  }
  if (auto *inst = isSgTemplateInstantiationTypedefDeclaration(node)) {
    return failed(inst);
  }
  if (auto *inst = isSgTemplateVariableDeclaration(node)) {
    return failed(inst);
  }
  if (auto *inst = isSgTemplateInstantiationDecl(node)) {
    return failed(inst);
  }
  return false;
}

void pruneSymbolsForConstraintFailures(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  struct Pruner : public AstSimpleProcessing {
    void visit(SgNode *n) override {
      SgDeclarationStatement *decl = isSgDeclarationStatement(n);
      if (decl == nullptr) {
        return;
      }
      if (!constraintFailureOnNode(decl)) {
        return;
      }

      auto remove_symbol = [&](SgSymbol *symbol) {
        if (symbol == nullptr) {
          return;
        }
        if (SgSymbolTable *table = isSgSymbolTable(symbol->get_parent())) {
          if (table->exists(symbol)) {
            table->remove(symbol);
          }
        } else if (SgScopeStatement *scope =
                       isSgScopeStatement(symbol->get_parent())) {
          if (scope->symbol_exists(symbol)) {
            scope->remove_symbol(symbol);
          }
        }
        move_symbol_to_orphan_table(symbol);
      };

      if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
        for (SgInitializedName *init_name : var_decl->get_variables()) {
          if (init_name == nullptr) {
            continue;
          }
          remove_symbol(init_name->get_symbol_from_symbol_table());
        }
      }

      if (SgSymbol *symbol = decl->get_symbol_from_symbol_table()) {
        remove_symbol(symbol);
      }

      if (SgDeclarationStatement *first_nondef =
              decl->get_firstNondefiningDeclaration()) {
        if (SgSymbol *symbol = first_nondef->get_symbol_from_symbol_table()) {
          remove_symbol(symbol);
        }
      }
    }
  } pruner;

  pruner.traverse(node, preorder);
}

void parkDetachedSymbolsInMemoryPool() {
  struct Parker : public ROSE_VisitTraversal {
    void visit(SgNode *n) override {
      if (SgSymbol *symbol = isSgSymbol(n)) {
        if (symbol->get_parent() == nullptr) {
          move_symbol_to_orphan_table(symbol);
        }
      }
    }
  } parker;

  parker.traverseMemoryPool();
}

bool isTrackedStdTemplateDebugName(const std::string &name) {
  return name.find("ctype") != std::string::npos ||
         name.find("numpunct") != std::string::npos;
}

bool hasRealSourceFileInfo(Sg_File_Info *fi) {
  return fi != nullptr && fi->get_line() > 0 &&
         !fi->get_filenameString().empty() &&
         fi->get_filenameString() != "NULL_FILE" &&
         !fi->isCompilerGenerated() && !fi->isFrontendSpecific() &&
         !fi->isSourcePositionUnavailableInFrontend();
}

bool symbolBasisLooksDeleted(SgNode *basis) {
  return basis == nullptr || !SgNode::isLiveNode(basis) ||
         basis->class_name() == "SgNode";
}

SgNode *getSymbolBasisForRepair(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return nullptr;
  }

  if (SgVariableSymbol *var = isSgVariableSymbol(symbol)) {
    return var->get_declaration();
  }
  if (SgFunctionSymbol *func = isSgFunctionSymbol(symbol)) {
    return func->get_declaration();
  }
  if (SgMemberFunctionSymbol *memFunc = isSgMemberFunctionSymbol(symbol)) {
    return memFunc->get_declaration();
  }
  if (SgClassSymbol *cls = isSgClassSymbol(symbol)) {
    return cls->get_declaration();
  }
  if (SgTemplateSymbol *tmpl = isSgTemplateSymbol(symbol)) {
    return tmpl->get_declaration();
  }
  if (SgTypedefSymbol *typedefSym = isSgTypedefSymbol(symbol)) {
    return typedefSym->get_declaration();
  }
  if (SgEnumSymbol *enumSym = isSgEnumSymbol(symbol)) {
    return enumSym->get_declaration();
  }
  if (SgLabelSymbol *label = isSgLabelSymbol(symbol)) {
    return label->get_declaration();
  }
  if (SgNamespaceSymbol *ns = isSgNamespaceSymbol(symbol)) {
    return ns->get_declaration();
  }
  if (SgEnumFieldSymbol *field = isSgEnumFieldSymbol(symbol)) {
    return field->get_declaration();
  }

  return symbol->get_symbol_basis();
}

SgScopeStatement *recoverSymbolScopeForRepair(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return nullptr;
  }

  if (SgSymbolTable *parentTable = isSgSymbolTable(symbol->get_parent())) {
    if (SgScopeStatement *parentScope =
            isSgScopeStatement(parentTable->get_parent())) {
      return parentScope;
    }
  }

  SgNode *basis = getSymbolBasisForRepair(symbol);
  if (symbolBasisLooksDeleted(basis)) {
    return nullptr;
  }

  if (SgInitializedName *initName = isSgInitializedName(basis)) {
    return initName->get_scope();
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(basis)) {
    return decl->get_scope();
  }
  if (SgLabelStatement *label = isSgLabelStatement(basis)) {
    return label->get_scope();
  }

  return nullptr;
}

SgSymbol *lookupEquivalentLiveSymbolForRepair(SgSymbol *symbol,
                                              SgScopeStatement *scope) {
  if (symbol == nullptr || scope == nullptr) {
    return nullptr;
  }

  const SgName &name = symbol->get_name();

  if (isSgVariableSymbol(symbol) != nullptr) {
    return scope->lookup_variable_symbol(name);
  }

  if (isSgClassSymbol(symbol) != nullptr) {
    return scope->lookup_class_symbol(name,
                                      (SgTemplateArgumentPtrList *)nullptr);
  }

  if (SgFunctionSymbol *function_symbol = isSgFunctionSymbol(symbol)) {
    if (SgFunctionDeclaration *decl =
            isSgFunctionDeclaration(function_symbol->get_declaration())) {
      if (const SgType *type = decl->get_type()) {
        if (SgFunctionSymbol *existing = scope->lookup_function_symbol(
                name, type, (SgTemplateArgumentPtrList *)nullptr)) {
          return existing;
        }
      }
    }

    return scope->lookup_function_symbol(name);
  }

  if (SgMemberFunctionSymbol *member_symbol =
          isSgMemberFunctionSymbol(symbol)) {
    if (SgMemberFunctionDeclaration *decl =
            isSgMemberFunctionDeclaration(member_symbol->get_declaration())) {
      if (const SgType *type = decl->get_type()) {
        return scope->lookup_nontemplate_member_function_symbol(
            name, type, (SgTemplateArgumentPtrList *)nullptr);
      }
    }

    return nullptr;
  }

  if (SgTemplateFunctionSymbol *template_symbol =
          isSgTemplateFunctionSymbol(symbol)) {
    if (SgTemplateFunctionDeclaration *decl = isSgTemplateFunctionDeclaration(
            template_symbol->get_declaration())) {
      if (const SgType *type = decl->get_type()) {
        return scope->lookup_template_function_symbol(
            name, type, &decl->get_templateParameters());
      }
    }

    return nullptr;
  }

  if (SgTemplateMemberFunctionSymbol *template_symbol =
          isSgTemplateMemberFunctionSymbol(symbol)) {
    if (SgTemplateMemberFunctionDeclaration *decl =
            isSgTemplateMemberFunctionDeclaration(
                template_symbol->get_declaration())) {
      if (const SgType *type = decl->get_type()) {
        return scope->lookup_template_member_function_symbol(
            name, type, &decl->get_templateParameters());
      }
    }

    return nullptr;
  }

  if (isSgTypedefSymbol(symbol) != nullptr) {
    return scope->lookup_typedef_symbol(name);
  }

  if (isSgTemplateTypedefSymbol(symbol) != nullptr) {
    return scope->lookup_template_typedef_symbol(name);
  }

  if (isSgEnumSymbol(symbol) != nullptr) {
    return scope->lookup_enum_symbol(name);
  }

  if (isSgEnumFieldSymbol(symbol) != nullptr) {
    return scope->lookup_enum_field_symbol(name);
  }

  if (isSgLabelSymbol(symbol) != nullptr) {
    return scope->lookup_label_symbol(name);
  }

  if (isSgNamespaceSymbol(symbol) != nullptr) {
    return scope->lookup_namespace_symbol(name);
  }

  if (SgTemplateClassSymbol *template_symbol =
          isSgTemplateClassSymbol(symbol)) {
    if (SgTemplateClassDeclaration *decl =
            isSgTemplateClassDeclaration(template_symbol->get_declaration())) {
      return scope->lookup_template_class_symbol(
          name, &decl->get_templateParameters(),
          (SgTemplateArgumentPtrList *)nullptr);
    }

    return nullptr;
  }

  return nullptr;
}

void removeSymbolFromParentForRepair(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return;
  }

  if (SgSymbolTable *table = isSgSymbolTable(symbol->get_parent())) {
    if (table->exists(symbol)) {
      table->remove(symbol);
    }
  } else if (SgScopeStatement *scope =
                 isSgScopeStatement(symbol->get_parent())) {
    if (scope->symbol_exists(symbol)) {
      scope->remove_symbol(symbol);
    }
  }

  if (SgNode::isLiveNode(symbol)) {
    symbol->set_parent(nullptr);
  }
}

void rewriteSymbolEdgesInMemoryPool(
    const std::unordered_map<SgNode *, SgNode *> &replacements) {
  if (replacements.empty()) {
    return;
  }

  struct EdgeRewriter : public SimpleReferenceToPointerHandler {
    const std::unordered_map<SgNode *, SgNode *> &replacements;

    explicit EdgeRewriter(
        const std::unordered_map<SgNode *, SgNode *> &replacementMap)
        : replacements(replacementMap) {}

    void operator()(SgNode *&key, const SgName &, bool) override {
      if (key == nullptr) {
        return;
      }

      std::unordered_map<SgNode *, SgNode *>::const_iterator it =
          replacements.find(key);
      if (it != replacements.end() && it->second != key) {
        key = it->second;
      }
    }
  };

  struct Traversal : public ROSE_VisitTraversal {
    EdgeRewriter &rewriter;

    explicit Traversal(EdgeRewriter &edgeRewriter) : rewriter(edgeRewriter) {}

    void visit(SgNode *node) override {
      if (!SgNode::isLiveNode(node)) {
        return;
      }

      node->processDataMemberReferenceToPointers(&rewriter);
    }
  };

  EdgeRewriter rewriter(replacements);
  Traversal traversal(rewriter);
  traversal.traverseMemoryPool();
}

bool functionDeclIsNondefining(SgFunctionDeclaration *decl);
bool functionDeclOutputsCode(SgFunctionDeclaration *decl);
bool functionDeclOwnsAssociatedSymbol(SgFunctionDeclaration *decl);

std::string stableFunctionDeclIdentity(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return std::string();
  }

  const std::string mangled_name = decl->get_mangled_name().getString();
  if (!mangled_name.empty()) {
    return mangled_name;
  }

  const std::string qualified_name = decl->get_qualified_name().getString();
  if (!qualified_name.empty()) {
    return qualified_name;
  }

  return decl->get_name().getString();
}

size_t functionDeclParameterCount(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return 0;
  }

  SgFunctionParameterList *params = decl->get_parameterList();
  return params != nullptr ? params->get_args().size() : 0;
}

bool functionDeclStableTieLess(SgFunctionDeclaration *lhs,
                               SgFunctionDeclaration *rhs) {
  const std::string lhs_identity = stableFunctionDeclIdentity(lhs);
  const std::string rhs_identity = stableFunctionDeclIdentity(rhs);
  if (lhs_identity != rhs_identity) {
    return lhs_identity < rhs_identity;
  }

  const size_t lhs_param_count = functionDeclParameterCount(lhs);
  const size_t rhs_param_count = functionDeclParameterCount(rhs);
  if (lhs_param_count != rhs_param_count) {
    return lhs_param_count < rhs_param_count;
  }

  const bool lhs_nondef = functionDeclIsNondefining(lhs);
  const bool rhs_nondef = functionDeclIsNondefining(rhs);
  if (lhs_nondef != rhs_nondef) {
    return lhs_nondef;
  }

  const bool lhs_symbol_owner = functionDeclOwnsAssociatedSymbol(lhs);
  const bool rhs_symbol_owner = functionDeclOwnsAssociatedSymbol(rhs);
  if (lhs_symbol_owner != rhs_symbol_owner) {
    return lhs_symbol_owner;
  }

  const bool lhs_outputs = functionDeclOutputsCode(lhs);
  const bool rhs_outputs = functionDeclOutputsCode(rhs);
  if (lhs_outputs != rhs_outputs) {
    return lhs_outputs;
  }

  const int lhs_source_sequence = lhs->get_source_sequence_value();
  const int rhs_source_sequence = rhs->get_source_sequence_value();
  if ((lhs_source_sequence >= 0) != (rhs_source_sequence >= 0)) {
    return lhs_source_sequence >= 0;
  }
  if (lhs_source_sequence >= 0 && rhs_source_sequence >= 0 &&
      lhs_source_sequence != rhs_source_sequence) {
    return lhs_source_sequence < rhs_source_sequence;
  }

  return false;
}

void repairMalformedSymbolsInMemoryPool() {
  struct RepairPlan {
    std::unordered_map<SgNode *, SgNode *> replacements;
    std::vector<SgSymbol *> symbolsToDetach;
    std::vector<std::pair<SgSymbol *, SgScopeStatement *>> symbolsToInsert;
    std::vector<SgSymbol *> symbolsToOrphan;
  } plan;

  struct Collector : public ROSE_VisitTraversal {
    RepairPlan &plan;

    explicit Collector(RepairPlan &repairPlan) : plan(repairPlan) {}

    void visit(SgNode *node) override {
      SgSymbol *symbol = isSgSymbol(node);
      if (symbol == nullptr || !SgNode::isLiveNode(symbol)) {
        return;
      }

      if (SgSymbolTable *parentTable = isSgSymbolTable(symbol->get_parent())) {
        if (!parentTable->exists(symbol)) {
          symbol->set_parent(nullptr);
        }
      }

      SgScopeStatement *scope = recoverSymbolScopeForRepair(symbol);
      SgSymbol *equivalent = lookupEquivalentLiveSymbolForRepair(symbol, scope);
      if (equivalent == symbol) {
        equivalent = nullptr;
      }

      SgNode *basis = getSymbolBasisForRepair(symbol);
      if (symbolBasisLooksDeleted(basis)) {
        if (equivalent != nullptr) {
          plan.replacements.emplace(symbol, equivalent);
        }
        if (basis != nullptr) {
          plan.replacements.emplace(basis, nullptr);
        }
        plan.symbolsToDetach.push_back(symbol);
        return;
      }

      if (symbol->get_parent() != nullptr) {
        return;
      }

      if (equivalent != nullptr) {
        plan.replacements.emplace(symbol, equivalent);
        plan.symbolsToDetach.push_back(symbol);
        return;
      }

      if (scope != nullptr && scope->get_symbol_table() != nullptr) {
        plan.symbolsToInsert.push_back(std::make_pair(symbol, scope));
      } else {
        plan.symbolsToOrphan.push_back(symbol);
      }
    }
  } collector(plan);

  collector.traverseMemoryPool();
  rewriteSymbolEdgesInMemoryPool(plan.replacements);

  for (SgSymbol *symbol : plan.symbolsToDetach) {
    if (symbol == nullptr || !SgNode::isLiveNode(symbol)) {
      continue;
    }

    removeSymbolFromParentForRepair(symbol);
    // Keep detached symbols alive in the orphan table after repairing AST
    // references so external raw symbol pointers do not become dangling.
    move_symbol_to_orphan_table(symbol);
  }

  for (const std::pair<SgSymbol *, SgScopeStatement *> &entry :
       plan.symbolsToInsert) {
    SgSymbol *symbol = entry.first;
    SgScopeStatement *scope = entry.second;
    if (symbol == nullptr || scope == nullptr || !SgNode::isLiveNode(symbol) ||
        !SgNode::isLiveNode(scope) || symbol->get_parent() != nullptr) {
      continue;
    }

    SgSymbolTable *table = scope->get_symbol_table();
    if (table == nullptr) {
      continue;
    }

    if (!scope->symbol_exists(symbol)) {
      scope->insert_symbol(symbol->get_name(), symbol);
    } else if (symbol->get_parent() != table) {
      symbol->set_parent(table);
    }
  }

  for (SgSymbol *symbol : plan.symbolsToOrphan) {
    if (symbol == nullptr || !SgNode::isLiveNode(symbol) ||
        symbol->get_parent() != nullptr) {
      continue;
    }

    move_symbol_to_orphan_table(symbol);
  }
}

bool functionDeclHasRealSource(SgFunctionDeclaration *decl) {
  return decl != nullptr &&
         (hasRealSourceFileInfo(decl->get_startOfConstruct()) ||
          hasRealSourceFileInfo(decl->get_file_info()) ||
          hasRealSourceFileInfo(decl->get_endOfConstruct()));
}

Sg_File_Info *getDeclSortFileInfo(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (hasRealSourceFileInfo(decl->get_startOfConstruct())) {
    return decl->get_startOfConstruct();
  }
  if (hasRealSourceFileInfo(decl->get_file_info())) {
    return decl->get_file_info();
  }
  if (hasRealSourceFileInfo(decl->get_endOfConstruct())) {
    return decl->get_endOfConstruct();
  }

  return decl->get_file_info();
}

bool namespaceLikeScopeEquals(SgScopeStatement *lhs, SgScopeStatement *rhs) {
  if (lhs == rhs) {
    return true;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  if (isSgGlobal(lhs) != nullptr || isSgGlobal(rhs) != nullptr) {
    if (isSgGlobal(lhs) == nullptr || isSgGlobal(rhs) == nullptr) {
      return false;
    }

    Sg_File_Info *lhs_fi = lhs->get_file_info();
    Sg_File_Info *rhs_fi = rhs->get_file_info();
    if (lhs_fi == nullptr || rhs_fi == nullptr) {
      return false;
    }

    const int lhs_physical_id = lhs_fi->get_physical_file_id();
    const int rhs_physical_id = rhs_fi->get_physical_file_id();
    if (lhs_physical_id >= 0 && rhs_physical_id >= 0) {
      return lhs_physical_id == rhs_physical_id;
    }

    return lhs_fi->get_physical_filename() == rhs_fi->get_physical_filename();
  }

  SgNamespaceDefinitionStatement *lhs_ns =
      isSgNamespaceDefinitionStatement(lhs);
  SgNamespaceDefinitionStatement *rhs_ns =
      isSgNamespaceDefinitionStatement(rhs);
  if (lhs_ns == nullptr || rhs_ns == nullptr) {
    return false;
  }

  SgNamespaceDeclarationStatement *lhs_decl =
      lhs_ns->get_namespaceDeclaration();
  SgNamespaceDeclarationStatement *rhs_decl =
      rhs_ns->get_namespaceDeclaration();
  if (lhs_decl == nullptr || rhs_decl == nullptr) {
    return false;
  }

  SgDeclarationStatement *lhs_first =
      lhs_decl->get_firstNondefiningDeclaration();
  if (lhs_first == nullptr) {
    lhs_first = lhs_decl;
  }
  SgDeclarationStatement *rhs_first =
      rhs_decl->get_firstNondefiningDeclaration();
  if (rhs_first == nullptr) {
    rhs_first = rhs_decl;
  }

  return lhs_first == rhs_first;
}

bool declAttachedToScope(SgScopeStatement *scope,
                         SgDeclarationStatement *decl) {
  if (scope == nullptr || decl == nullptr) {
    return false;
  }

  SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
  return std::find(decls.begin(), decls.end(), decl) != decls.end();
}

void restoreFunctionDeclOutput(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return;
  }

  decl->setOutputInCodeGeneration();
  if (SgFunctionParameterList *params = decl->get_parameterList()) {
    params->setOutputInCodeGeneration();
    for (SgInitializedName *param : params->get_args()) {
      if (param != nullptr) {
        param->setOutputInCodeGeneration();
      }
    }
  }
}

void insertDeclBySourcePosition(SgScopeStatement *scope,
                                SgDeclarationStatement *decl) {
  if (scope == nullptr || decl == nullptr) {
    return;
  }

  SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
  if (std::find(decls.begin(), decls.end(), decl) != decls.end()) {
    return;
  }

  Sg_File_Info *decl_fi = getDeclSortFileInfo(decl);
  if (!hasRealSourceFileInfo(decl_fi)) {
    decls.push_back(decl);
    return;
  }

  auto insert_it = decls.end();
  for (auto it = decls.begin(); it != decls.end(); ++it) {
    SgDeclarationStatement *existing = *it;
    if (existing == nullptr) {
      continue;
    }

    Sg_File_Info *existing_fi = getDeclSortFileInfo(existing);
    if (!hasRealSourceFileInfo(existing_fi)) {
      continue;
    }
    if (existing_fi->get_filenameString() != decl_fi->get_filenameString()) {
      continue;
    }

    if (existing_fi->get_line() > decl_fi->get_line() ||
        (existing_fi->get_line() == decl_fi->get_line() &&
         existing_fi->get_col() > decl_fi->get_col())) {
      insert_it = it;
      break;
    }
  }

  decls.insert(insert_it, decl);
}

SgSourceFile *
resolveTranslationUnitSourceFileForPostprocessing(SgSourceFile *source_file) {
  if (source_file == nullptr || !source_file->get_isHeaderFile()) {
    return source_file;
  }

  SgIncludeFile *include_file = source_file->get_associated_include_file();
  if (include_file == nullptr) {
    return source_file;
  }

  SgSourceFile *translation_unit =
      include_file->get_source_file_of_translation_unit();
  return translation_unit != nullptr ? translation_unit : source_file;
}

void markNormalizedTemplateDeclarationSurfaces(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  struct Traversal : public AstSimpleProcessing {
    void visit(SgNode *n) override {
      SgDeclarationStatement *decl = isSgDeclarationStatement(n);
      if (decl == nullptr) {
        return;
      }

      if (isSgTemplateDeclaration(decl) == nullptr &&
          isSgTemplateClassDeclaration(decl) == nullptr &&
          isSgTemplateFunctionDeclaration(decl) == nullptr &&
          isSgTemplateMemberFunctionDeclaration(decl) == nullptr &&
          isSgTemplateVariableDeclaration(decl) == nullptr &&
          isSgTemplateTypedefDeclaration(decl) == nullptr) {
        return;
      }

      Sg_File_Info *fi = decl->get_file_info();
      if (fi == nullptr || !fi->isOutputInCodeGeneration()) {
        return;
      }

      SgSourceFile *source_file =
          resolveTranslationUnitSourceFileForPostprocessing(
              SageInterface::getEnclosingSourceFile(decl, true));
      if (source_file == nullptr || !source_file->get_unparse_tokens()) {
        return;
      }

      const bool normalized_declaration_formatting_requested =
          source_file->get_suppress_variable_declaration_normalization();
      const bool declaration_contains_transformation =
          decl->get_containsTransformation() ||
          decl->get_containsTransformationToSurroundingWhitespace();
      if (!normalized_declaration_formatting_requested &&
          !declaration_contains_transformation) {
        return;
      }

      decl->setTransformation();
      decl->setOutputInCodeGeneration();
    }
  } traversal;

  traversal.traverse(node, preorder);
}

void repairHiddenFunctionDeclarationSurfaces(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  struct Traversal : public AstSimpleProcessing {
    void visit(SgNode *n) override {
      SgScopeStatement *scope = isSgScopeStatement(n);
      if (scope == nullptr) {
        return;
      }
      if (isSgGlobal(scope) == nullptr &&
          isSgNamespaceDefinitionStatement(scope) == nullptr) {
        return;
      }

      SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
      std::vector<SgFunctionDeclaration *> missing_visible_firsts;
      std::unordered_set<SgFunctionDeclaration *> queued;

      for (SgDeclarationStatement *decl_stmt : decls) {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl_stmt);
        if (func_decl == nullptr ||
            isSgMemberFunctionDeclaration(func_decl) != nullptr ||
            isSgTemplateMemberFunctionDeclaration(func_decl) != nullptr) {
          continue;
        }

        SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
            func_decl->get_firstNondefiningDeclaration());
        if (first_nondef == nullptr || first_nondef == func_decl ||
            !functionDeclHasRealSource(first_nondef)) {
          continue;
        }

        if (isSgClassDefinition(first_nondef->get_parent()) != nullptr ||
            isSgTemplateClassDefinition(first_nondef->get_parent()) !=
                nullptr ||
            isSgTemplateInstantiationDefn(first_nondef->get_parent()) !=
                nullptr) {
          continue;
        }

        Sg_File_Info *func_fi = func_decl->get_file_info();
        const bool hidden_surface_decl =
            func_fi == nullptr || func_fi->isCompilerGenerated() ||
            func_fi->isFrontendSpecific() || func_fi->get_line() <= 0 ||
            func_fi->get_filenameString().empty() ||
            func_fi->get_filenameString() == "NULL_FILE" ||
            !func_fi->isOutputInCodeGeneration();
        if (!hidden_surface_decl) {
          continue;
        }

        SgScopeStatement *first_scope =
            isSgScopeStatement(first_nondef->get_parent());
        if (first_scope == nullptr) {
          first_scope = first_nondef->get_scope();
        }
        if (first_scope != nullptr &&
            !namespaceLikeScopeEquals(first_scope, scope)) {
          continue;
        }

        if (declAttachedToScope(scope, first_nondef)) {
          continue;
        }

        if (queued.insert(first_nondef).second) {
          missing_visible_firsts.push_back(first_nondef);
        }
      }

      for (SgFunctionDeclaration *first_nondef : missing_visible_firsts) {
        SgScopeStatement *old_parent =
            isSgScopeStatement(first_nondef->get_parent());
        if (old_parent != nullptr && old_parent != scope &&
            declAttachedToScope(old_parent, first_nondef)) {
          SgDeclarationStatementPtrList &old_decls =
              old_parent->getDeclarationList();
          old_decls.erase(
              std::remove(old_decls.begin(), old_decls.end(), first_nondef),
              old_decls.end());
        }

        restoreFunctionDeclOutput(first_nondef);
        if (first_nondef->get_scope() == nullptr ||
            !namespaceLikeScopeEquals(first_nondef->get_scope(), scope)) {
          first_nondef->set_scope(scope);
        }
        if (first_nondef->get_parent() != scope) {
          first_nondef->set_parent(scope);
        }

        insertDeclBySourcePosition(scope, first_nondef);
      }
    }
  } traversal;

  traversal.traverse(node, preorder);
}

void repairTypedefOwnedTagDeclarationSurfaces(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  auto fileInfoWithinDeclRange = [](Sg_File_Info *target,
                                    SgDeclarationStatement *owner) -> bool {
    if (!hasRealSourceFileInfo(target) || owner == nullptr) {
      return false;
    }

    Sg_File_Info *begin = owner->get_startOfConstruct();
    if (!hasRealSourceFileInfo(begin)) {
      begin = getDeclSortFileInfo(owner);
    }
    Sg_File_Info *end = owner->get_endOfConstruct();
    if (!hasRealSourceFileInfo(end)) {
      end = begin;
    }
    if (!hasRealSourceFileInfo(begin) || !hasRealSourceFileInfo(end)) {
      return false;
    }

    if (target->get_filenameString() != begin->get_filenameString() ||
        target->get_filenameString() != end->get_filenameString()) {
      return false;
    }

    auto less_or_equal = [](Sg_File_Info *lhs, Sg_File_Info *rhs) {
      return lhs->get_line() < rhs->get_line() ||
             (lhs->get_line() == rhs->get_line() &&
              lhs->get_col() <= rhs->get_col());
    };

    if (!less_or_equal(begin, end)) {
      std::swap(begin, end);
    }

    return less_or_equal(begin, target) && less_or_equal(target, end);
  };

  auto suppress_decl_output = [](SgDeclarationStatement *decl) {
    if (decl == nullptr) {
      return;
    }
    if (Sg_File_Info *fi = decl->get_file_info()) {
      fi->unsetOutputInCodeGeneration();
    }
    if (Sg_File_Info *fi = decl->get_startOfConstruct()) {
      fi->unsetOutputInCodeGeneration();
    }
    if (Sg_File_Info *fi = decl->get_endOfConstruct()) {
      fi->unsetOutputInCodeGeneration();
    }
  };

  struct Traversal : public AstSimpleProcessing {
    decltype(fileInfoWithinDeclRange) &fileInfoWithinDeclRange;
    decltype(suppress_decl_output) &suppress_decl_output;

    Traversal(decltype(fileInfoWithinDeclRange) &range_pred,
              decltype(suppress_decl_output) &suppress_output)
        : fileInfoWithinDeclRange(range_pred),
          suppress_decl_output(suppress_output) {}

    void visit(SgNode *n) override {
      SgScopeStatement *scope = isSgScopeStatement(n);
      if (scope == nullptr) {
        return;
      }
      if (isSgGlobal(scope) == nullptr &&
          isSgNamespaceDefinitionStatement(scope) == nullptr) {
        return;
      }

      SgDeclarationStatementPtrList decls_copy = scope->getDeclarationList();
      for (SgDeclarationStatement *decl_stmt : decls_copy) {
        if (decl_stmt == nullptr) {
          continue;
        }

        SgClassDeclaration *class_decl = isSgClassDeclaration(decl_stmt);
        SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl_stmt);
        if (class_decl == nullptr && enum_decl == nullptr) {
          continue;
        }

        if (SgLocatedNode *located = isSgLocatedNode(decl_stmt)) {
          Sg_File_Info *fi = located->get_file_info();
          if (!hasRealSourceFileInfo(fi) || !fi->isOutputInCodeGeneration()) {
            continue;
          }
        }

        bool autonomous = class_decl != nullptr
                              ? class_decl->get_isAutonomousDeclaration()
                              : enum_decl->get_isAutonomousDeclaration();
        if (!autonomous) {
          continue;
        }

        Sg_File_Info *decl_loc = getDeclSortFileInfo(decl_stmt);
        if (!hasRealSourceFileInfo(decl_loc)) {
          continue;
        }

        for (SgDeclarationStatement *owner_stmt : decls_copy) {
          SgTypedefDeclaration *typedef_decl =
              isSgTypedefDeclaration(owner_stmt);
          if (typedef_decl == nullptr || typedef_decl == decl_stmt) {
            continue;
          }
          if (!fileInfoWithinDeclRange(decl_loc, typedef_decl)) {
            continue;
          }

          auto rehome_candidate = [&](SgDeclarationStatement *candidate) {
            if (candidate == nullptr) {
              return;
            }
            Sg_File_Info *candidate_loc = getDeclSortFileInfo(candidate);
            if (!fileInfoWithinDeclRange(candidate_loc, typedef_decl)) {
              return;
            }
            if (declAttachedToScope(scope, candidate)) {
              SgDeclarationStatementPtrList &decls =
                  scope->getDeclarationList();
              decls.erase(std::remove(decls.begin(), decls.end(), candidate),
                          decls.end());
            }
            candidate->set_parent(typedef_decl);
            if (SgClassDeclaration *candidate_class =
                    isSgClassDeclaration(candidate)) {
              candidate_class->set_isAutonomousDeclaration(false);
            } else if (SgEnumDeclaration *candidate_enum =
                           isSgEnumDeclaration(candidate)) {
              candidate_enum->set_isAutonomousDeclaration(false);
            }
            suppress_decl_output(candidate);
          };

          if (class_decl != nullptr) {
            rehome_candidate(class_decl);
            rehome_candidate(isSgClassDeclaration(
                class_decl->get_firstNondefiningDeclaration()));
            rehome_candidate(
                isSgClassDeclaration(class_decl->get_definingDeclaration()));
          } else if (enum_decl != nullptr) {
            rehome_candidate(enum_decl);
            rehome_candidate(isSgEnumDeclaration(
                enum_decl->get_firstNondefiningDeclaration()));
            rehome_candidate(
                isSgEnumDeclaration(enum_decl->get_definingDeclaration()));
          }
          break;
        }
      }
    }
  } traversal(fileInfoWithinDeclRange, suppress_decl_output);

  traversal.traverse(node, preorder);
}

bool functionDeclIsNondefining(SgFunctionDeclaration *decl) {
  return decl != nullptr &&
         (decl->get_definition() == nullptr ||
          decl != isSgFunctionDeclaration(decl->get_definingDeclaration()));
}

bool functionDeclOutputsCode(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return false;
  }

  if (Sg_File_Info *fi = decl->get_file_info()) {
    return fi->isOutputInCodeGeneration();
  }

  return false;
}

SgFunctionDeclaration *
associatedFunctionSymbolDeclaration(SgFunctionDeclaration *decl) {
  return decl != nullptr ? isSgFunctionDeclaration(
                               decl->get_declaration_associated_with_symbol())
                         : nullptr;
}

bool functionDeclOwnsAssociatedSymbol(SgFunctionDeclaration *decl) {
  return decl != nullptr && associatedFunctionSymbolDeclaration(decl) == decl;
}

std::vector<SgFunctionDeclaration *>
relatedFunctionDeclarations(SgFunctionDeclaration *decl) {
  std::vector<SgFunctionDeclaration *> related;
  std::unordered_set<SgFunctionDeclaration *> seen;
  auto append = [&](SgFunctionDeclaration *candidate) {
    if (candidate == nullptr || candidate == decl ||
        !seen.insert(candidate).second) {
      return;
    }
    related.push_back(candidate);
  };

  append(associatedFunctionSymbolDeclaration(decl));
  append(isSgFunctionDeclaration(
      decl != nullptr ? decl->get_firstNondefiningDeclaration() : nullptr));
  append(isSgFunctionDeclaration(
      decl != nullptr ? decl->get_definingDeclaration() : nullptr));

  return related;
}

bool functionDeclSourceLess(SgFunctionDeclaration *lhs,
                            SgFunctionDeclaration *rhs) {
  if (lhs == rhs) {
    return false;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return lhs == nullptr;
  }

  Sg_File_Info *lhs_fi = getDeclSortFileInfo(lhs);
  Sg_File_Info *rhs_fi = getDeclSortFileInfo(rhs);
  if ((lhs_fi != nullptr) != (rhs_fi != nullptr)) {
    return lhs_fi != nullptr;
  }
  if (lhs_fi != nullptr && rhs_fi != nullptr &&
      lhs_fi->get_filenameString() != rhs_fi->get_filenameString()) {
    return lhs_fi->get_filenameString() < rhs_fi->get_filenameString();
  }
  if (lhs_fi != nullptr && rhs_fi != nullptr &&
      lhs_fi->get_line() != rhs_fi->get_line()) {
    return lhs_fi->get_line() < rhs_fi->get_line();
  }
  if (lhs_fi != nullptr && rhs_fi != nullptr &&
      lhs_fi->get_col() != rhs_fi->get_col()) {
    return lhs_fi->get_col() < rhs_fi->get_col();
  }

  return functionDeclStableTieLess(lhs, rhs);
}

void suppressFunctionDeclOutput(SgFunctionDeclaration *decl) {
  if (decl == nullptr) {
    return;
  }

  decl->unsetOutputInCodeGeneration();
  if (SgFunctionParameterList *params = decl->get_parameterList()) {
    params->unsetOutputInCodeGeneration();
    for (SgInitializedName *param : params->get_args()) {
      if (param != nullptr) {
        param->unsetOutputInCodeGeneration();
      }
    }
  }
}

std::string functionDeclSurfaceKey(SgFunctionDeclaration *decl) {
  if (decl == nullptr || !functionDeclIsNondefining(decl) ||
      !functionDeclOutputsCode(decl)) {
    return "";
  }

  Sg_File_Info *start_fi = getDeclSortFileInfo(decl);
  Sg_File_Info *end_fi = hasRealSourceFileInfo(decl->get_endOfConstruct())
                             ? decl->get_endOfConstruct()
                             : decl->get_file_info();
  if (start_fi == nullptr || end_fi == nullptr) {
    return "";
  }

  std::ostringstream key;
  key << decl->variantT() << '|' << decl->get_name().getString() << '|'
      << start_fi->get_filenameString() << ':' << start_fi->get_line() << ':'
      << start_fi->get_col() << '|' << end_fi->get_filenameString() << ':'
      << end_fi->get_line() << ':' << end_fi->get_col() << '|';

  if (SgFunctionParameterList *params = decl->get_parameterList()) {
    key << params->get_args().size();
  } else {
    key << -1;
  }

  return key.str();
}

void pruneDuplicateFunctionDeclarationSurfaces(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  struct Traversal : public AstSimpleProcessing {
    void visit(SgNode *n) override {
      SgScopeStatement *scope = isSgScopeStatement(n);
      if (scope == nullptr) {
        return;
      }
      if (isSgGlobal(scope) == nullptr &&
          isSgNamespaceDefinitionStatement(scope) == nullptr &&
          isSgClassDefinition(scope) == nullptr &&
          isSgTemplateClassDefinition(scope) == nullptr &&
          isSgTemplateInstantiationDefn(scope) == nullptr) {
        return;
      }

      SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
      std::unordered_map<std::string, SgFunctionDeclaration *> canonical_by_key;
      std::unordered_set<SgFunctionDeclaration *> duplicates;

      for (SgDeclarationStatement *decl_stmt : decls) {
        SgFunctionDeclaration *decl = isSgFunctionDeclaration(decl_stmt);
        if (decl == nullptr) {
          continue;
        }

        const std::string key = functionDeclSurfaceKey(decl);
        if (key.empty()) {
          continue;
        }

        auto existing = canonical_by_key.find(key);
        if (existing == canonical_by_key.end()) {
          canonical_by_key.emplace(key, decl);
          continue;
        }

        duplicates.insert(decl);
      }

      if (duplicates.empty()) {
        return;
      }

      for (SgFunctionDeclaration *duplicate : duplicates) {
        suppressFunctionDeclOutput(duplicate);
      }

      decls.erase(std::remove_if(decls.begin(), decls.end(),
                                 [&](SgDeclarationStatement *decl_stmt) {
                                   SgFunctionDeclaration *decl =
                                       isSgFunctionDeclaration(decl_stmt);
                                   return decl != nullptr &&
                                          duplicates.find(decl) !=
                                              duplicates.end();
                                 }),
                  decls.end());
    }
  } traversal;

  traversal.traverse(node, preorder);
}

void repairBrokenFunctionDeclarationChains(SgNode *node) {
  if (node == nullptr) {
    return;
  }

  Rose_STL_Container<SgNode *> function_nodes =
      NodeQuery::querySubTree(node, V_SgFunctionDeclaration);
  std::vector<SgFunctionDeclaration *> all_members;
  std::unordered_map<SgFunctionDeclaration *, size_t> discovery_order;
  std::unordered_set<SgFunctionDeclaration *> discovered;
  auto record_member = [&](SgFunctionDeclaration *decl) {
    if (decl == nullptr || !discovered.insert(decl).second) {
      return;
    }

    discovery_order.emplace(decl, discovery_order.size());
    all_members.push_back(decl);
  };
  for (SgNode *entry : function_nodes) {
    record_member(isSgFunctionDeclaration(entry));
  }

  // Follow the declaration-chain links transitively so a broken sequence such
  // as visible-decl -> visible-decl -> hidden-def still lands in one repair
  // component even when the immediate symbol owner differs across members.
  for (size_t i = 0; i < all_members.size(); ++i) {
    for (SgFunctionDeclaration *related :
         relatedFunctionDeclarations(all_members[i])) {
      record_member(related);
    }
  }

  if (all_members.empty()) {
    return;
  }

  std::unordered_map<SgFunctionDeclaration *, SgFunctionDeclaration *> parent;
  for (SgFunctionDeclaration *decl : all_members) {
    parent[decl] = decl;
  }

  auto find_root = [&](SgFunctionDeclaration *decl) {
    SgFunctionDeclaration *root = decl;
    while (parent[root] != root) {
      root = parent[root];
    }
    while (decl != root) {
      SgFunctionDeclaration *next = parent[decl];
      parent[decl] = root;
      decl = next;
    }
    return root;
  };

  auto unite = [&](SgFunctionDeclaration *lhs, SgFunctionDeclaration *rhs) {
    if (lhs == nullptr || rhs == nullptr) {
      return;
    }
    SgFunctionDeclaration *lhs_root = find_root(lhs);
    SgFunctionDeclaration *rhs_root = find_root(rhs);
    if (lhs_root != rhs_root) {
      parent[rhs_root] = lhs_root;
    }
  };

  for (SgFunctionDeclaration *decl : all_members) {
    for (SgFunctionDeclaration *related : relatedFunctionDeclarations(decl)) {
      unite(decl, related);
    }
  }

  std::unordered_map<SgFunctionDeclaration *,
                     std::vector<SgFunctionDeclaration *>>
      groups;
  for (SgFunctionDeclaration *decl : all_members) {
    groups[find_root(decl)].push_back(decl);
  }

  for (auto &group_entry : groups) {
    std::vector<SgFunctionDeclaration *> members = group_entry.second;
    members.erase(std::remove(members.begin(), members.end(), nullptr),
                  members.end());
    auto member_order = [&](SgFunctionDeclaration *decl) -> size_t {
      std::unordered_map<SgFunctionDeclaration *, size_t>::const_iterator it =
          discovery_order.find(decl);
      return it != discovery_order.end() ? it->second : discovery_order.size();
    };
    auto member_less = [&](SgFunctionDeclaration *lhs,
                           SgFunctionDeclaration *rhs) {
      if (functionDeclSourceLess(lhs, rhs)) {
        return true;
      }
      if (functionDeclSourceLess(rhs, lhs)) {
        return false;
      }

      return member_order(lhs) < member_order(rhs);
    };
    std::sort(members.begin(), members.end(), member_less);
    members.erase(std::unique(members.begin(), members.end()), members.end());
    if (members.empty()) {
      continue;
    }

    bool needs_repair = false;
    std::unordered_set<SgFunctionDeclaration *> first_nondef_roots;
    for (SgFunctionDeclaration *decl : members) {
      SgFunctionDeclaration *first_nondef =
          isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration());
      if (first_nondef == nullptr || !functionDeclIsNondefining(first_nondef) ||
          first_nondef->get_firstNondefiningDeclaration() != first_nondef) {
        needs_repair = true;
        break;
      }
      if (!functionDeclHasRealSource(first_nondef) &&
          functionDeclHasRealSource(decl)) {
        needs_repair = true;
        break;
      }
      first_nondef_roots.insert(first_nondef);
    }
    if (first_nondef_roots.size() > 1) {
      needs_repair = true;
    }
    if (!needs_repair) {
      continue;
    }

    auto better_first_nondef = [](SgFunctionDeclaration *candidate,
                                  SgFunctionDeclaration *current_best) {
      if (candidate == nullptr || !functionDeclIsNondefining(candidate)) {
        return false;
      }
      if (current_best == nullptr) {
        return true;
      }

      const bool candidate_real_source = functionDeclHasRealSource(candidate);
      const bool best_real_source = functionDeclHasRealSource(current_best);
      if (candidate_real_source != best_real_source) {
        return candidate_real_source;
      }

      const bool candidate_symbol_owner =
          functionDeclOwnsAssociatedSymbol(candidate);
      const bool best_symbol_owner =
          functionDeclOwnsAssociatedSymbol(current_best);
      if (candidate_symbol_owner != best_symbol_owner) {
        return candidate_symbol_owner;
      }

      const bool candidate_nondef = functionDeclIsNondefining(candidate);
      const bool best_nondef = functionDeclIsNondefining(current_best);
      if (candidate_nondef != best_nondef) {
        return candidate_nondef;
      }

      const bool candidate_outputs = functionDeclOutputsCode(candidate);
      const bool best_outputs = functionDeclOutputsCode(current_best);
      if (candidate_outputs != best_outputs) {
        return candidate_outputs;
      }

      return functionDeclSourceLess(candidate, current_best);
    };

    SgFunctionDeclaration *first_nondef = nullptr;
    SgFunctionDeclaration *defining_decl = nullptr;
    for (SgFunctionDeclaration *decl : members) {
      if (better_first_nondef(decl, first_nondef)) {
        first_nondef = decl;
      }

      if (decl != nullptr &&
          (decl->get_definition() != nullptr ||
           decl == isSgFunctionDeclaration(decl->get_definingDeclaration()))) {
        if (defining_decl == nullptr ||
            functionDeclSourceLess(decl, defining_decl)) {
          defining_decl = decl;
        }
      }
    }

    if (first_nondef == nullptr) {
      first_nondef = defining_decl;
    }
    if (first_nondef == nullptr) {
      first_nondef = members.front();
    }
    if (defining_decl == nullptr && first_nondef != nullptr &&
        first_nondef->get_definition() != nullptr) {
      defining_decl = first_nondef;
    }

    if (first_nondef != nullptr) {
      first_nondef->set_firstNondefiningDeclaration(first_nondef);
    }
    if (defining_decl != nullptr) {
      defining_decl->set_definingDeclaration(defining_decl);
      defining_decl->set_firstNondefiningDeclaration(first_nondef);
    }

    for (SgFunctionDeclaration *decl : members) {
      decl->set_firstNondefiningDeclaration(first_nondef);
      decl->set_definingDeclaration(defining_decl);
    }
  }
}

bool isTrackedNamespaceFragmentDebugName(const std::string &name) {
  return name == "std" || name == "__gnu_cxx";
}

bool isTrackedNamespaceDeclDebugName(SgDeclarationStatement *decl,
                                     std::string &name) {
  if (auto *tmpl_class = isSgTemplateClassDeclaration(decl)) {
    name = tmpl_class->get_name().getString();
    return name == "__normal_iterator";
  }
  if (auto *klass = isSgClassDeclaration(decl)) {
    name = klass->get_name().getString();
    return name == "__normal_iterator";
  }
  return false;
}

void debugDumpTrackedStdTemplateState(const char *phase, SgNode *node) {
  (void)phase;
  (void)node;
  return;

  if (node == nullptr) {
    return;
  }

  struct Traversal : AstSimpleProcessing {
    const char *phase;

    explicit Traversal(const char *phase_name) : phase(phase_name) {}

    void visit(SgNode *n) override {
      auto *inst = isSgTemplateInstantiationDecl(n);
      if (inst == nullptr) {
        return;
      }

      const std::string name = inst->get_name().getString();
      if (!isTrackedStdTemplateDebugName(name)) {
        return;
      }

      auto *parent_scope = isSgScopeStatement(inst->get_parent());
      auto *scope = inst->get_scope();
      auto *first = isSgTemplateInstantiationDecl(
          inst->get_firstNondefiningDeclaration());
      auto *def =
          isSgTemplateInstantiationDecl(inst->get_definingDeclaration());
      auto *parent_ns = isSgNamespaceDefinitionStatement(parent_scope);
      auto *scope_ns = isSgNamespaceDefinitionStatement(scope);

      auto dump_ns = [](SgNamespaceDefinitionStatement *ns) {
        if (ns == nullptr) {
          return std::string();
        }
        SgNamespaceDeclarationStatement *decl = ns->get_namespaceDeclaration();
        return decl != nullptr ? decl->get_name().getString() : std::string();
      };

      std::cerr
          << "DEBUG ast post " << phase << " template-inst " << name
          << " node=" << inst << " parent=" << inst->get_parent() << "/"
          << (inst->get_parent() != nullptr ? inst->get_parent()->class_name()
                                            : "")
          << " scope=" << scope << "/"
          << (scope != nullptr ? scope->class_name() : "")
          << " parent-ns=" << dump_ns(parent_ns)
          << " scope-ns=" << dump_ns(scope_ns) << " attached-parent="
          << (parent_scope != nullptr &&
                      std::find(parent_scope->getDeclarationList().begin(),
                                parent_scope->getDeclarationList().end(),
                                inst) !=
                          parent_scope->getDeclarationList().end()
                  ? "true"
                  : "false")
          << " forward=" << (inst->isForward() ? "true" : "false")
          << " def=" << (inst->get_definition() != nullptr ? "true" : "false")
          << " output="
          << (inst->get_file_info() != nullptr &&
                      inst->get_file_info()->isOutputInCodeGeneration()
                  ? "true"
                  : "false")
          << " specialization=" << inst->get_specialization()
          << " first=" << first << " defdecl=" << def << "\n";
    }
  } traversal(phase);

  traversal.traverse(node, preorder);

  Rose_STL_Container<SgNode *> namespaces =
      NodeQuery::querySubTree(node, V_SgNamespaceDefinitionStatement);
  for (SgNode *entry : namespaces) {
    auto *ns = isSgNamespaceDefinitionStatement(entry);
    if (ns == nullptr || ns->get_namespaceDeclaration() == nullptr) {
      continue;
    }

    const std::string ns_name =
        ns->get_namespaceDeclaration()->get_name().getString();
    if (!isTrackedNamespaceFragmentDebugName(ns_name)) {
      continue;
    }

    auto *ns_fi = ns->get_namespaceDeclaration()->get_startOfConstruct();
    std::cerr << "DEBUG ast post " << phase << " " << ns_name << "-fragment "
              << ns << " decls=" << ns->get_declarations().size()
              << " line=" << (ns_fi != nullptr ? ns_fi->get_line() : 0) << "\n";
    for (SgDeclarationStatement *decl : ns->get_declarations()) {
      std::string tracked_name;
      if (isTrackedNamespaceDeclDebugName(decl, tracked_name)) {
        auto *parent_scope = isSgScopeStatement(decl->get_parent());
        auto *decl_scope = decl->get_scope();
        auto *parent_ns = isSgNamespaceDefinitionStatement(parent_scope);
        auto *scope_ns = isSgNamespaceDefinitionStatement(decl_scope);
        auto *decl_fi = decl->get_startOfConstruct();
        auto *parent_ns_fi =
            parent_ns != nullptr &&
                    parent_ns->get_namespaceDeclaration() != nullptr
                ? parent_ns->get_namespaceDeclaration()->get_startOfConstruct()
                : nullptr;
        auto *scope_ns_fi =
            scope_ns != nullptr &&
                    scope_ns->get_namespaceDeclaration() != nullptr
                ? scope_ns->get_namespaceDeclaration()->get_startOfConstruct()
                : nullptr;
        std::cerr << "DEBUG ast post " << phase << " " << ns_name << "-entry "
                  << tracked_name << " node=" << decl
                  << " class=" << decl->class_name()
                  << " line=" << (decl_fi != nullptr ? decl_fi->get_line() : 0)
                  << " parent=" << decl->get_parent() << "/"
                  << (decl->get_parent() != nullptr
                          ? decl->get_parent()->class_name()
                          : "")
                  << " scope=" << decl_scope << "/"
                  << (decl_scope != nullptr ? decl_scope->class_name() : "")
                  << " parent-ns="
                  << (parent_ns != nullptr &&
                              parent_ns->get_namespaceDeclaration() != nullptr
                          ? parent_ns->get_namespaceDeclaration()
                                ->get_name()
                                .getString()
                          : "")
                  << " parent-line="
                  << (parent_ns_fi != nullptr ? parent_ns_fi->get_line() : 0)
                  << " scope-ns="
                  << (scope_ns != nullptr &&
                              scope_ns->get_namespaceDeclaration() != nullptr
                          ? scope_ns->get_namespaceDeclaration()
                                ->get_name()
                                .getString()
                          : "")
                  << " scope-line="
                  << (scope_ns_fi != nullptr ? scope_ns_fi->get_line() : 0)
                  << "\n";
      }

      if (ns_name != "std") {
        continue;
      }

      auto *inst = isSgTemplateInstantiationDecl(decl);
      if (inst == nullptr) {
        continue;
      }
      const std::string name = inst->get_name().getString();
      if (!isTrackedStdTemplateDebugName(name)) {
        continue;
      }
      std::cerr << "DEBUG ast post " << phase << " std-entry " << name
                << " node=" << inst
                << " forward=" << (inst->isForward() ? "true" : "false")
                << " def="
                << (inst->get_definition() != nullptr ? "true" : "false")
                << " output="
                << (inst->get_file_info() != nullptr &&
                            inst->get_file_info()->isOutputInCodeGeneration()
                        ? "true"
                        : "false")
                << " specialization=" << inst->get_specialization() << "\n";
    }
  }
}
} // namespace

namespace Rose {
ROSE_DLL_API void
setMemoryPoolTraversalFilter(MemoryPoolTraversalFilter filter) {
  s_memoryPoolTraversalFilter = filter;
}

ROSE_DLL_API MemoryPoolTraversalFilter getMemoryPoolTraversalFilter() {
  return s_memoryPoolTraversalFilter;
}
} // namespace Rose

// DQ (5/22/2005): Added function with better name, since none of the fixes are
// really temporary any more.
void AstPostProcessing(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("AST post-processing:");

  ROSE_ASSERT(node != NULL);
  bool ranPostProcessing = false;

  // DQ (1/31/2014): We want to enforce this, but for now issue a warning if it
  // is not followed. Later I want to change the function's API to onoy take a
  // SgProject.  Note that this is related to a performance bug that was fixed
  // by Gergo a few years ago.  The fix could be improved by enforcing that this
  // could not be called at the SgFile level of the hierarchy.
  if (isSgProject(node) == NULL) {
    // DQ (5/17/17): Note that this function is called, and this message is
    // output, from the outliner, which is OK but not ideal.
    if (SgProject::get_verbose() >= 1)
      printf("Warning: AstPostProcessing should ideally be called on SgProject "
             "(due to repeated memory pool traversals and quadratic \n");
    //          printf ("         behavior (over files) when multiple files are
    //          specified on the command line): node = %s
    //          \n",node->class_name().c_str());
  }
  // DQ (1/31/2014): This is a problem to enforce this for at least (this test
  // program): ROSE_ASSERT(isSgProject(node) != NULL);

  // DQ (3/17/2007): This should be empty
  if (SgNode::get_globalMangledNameMap().size() != 0) {
    if (SgProject::get_verbose() > 0) {
      printf("AstPostProcessing(): found a node with globalMangledNameMap size "
             "not equal to 0: SgNode = %s =%s ",
             node->class_name().c_str(), SageInterface::get_name(node).c_str());
      printf("SgNode::get_globalMangledNameMap().size() != 0 size = %" PRIuPTR
             " (clearing mangled name cache) \n",
             SgNode::get_globalMangledNameMap().size());
    }

    SgNode::clearGlobalMangledNameMap();
  }
  ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

  switch (node->variantT()) {
  case V_SgProject: {
    SgProject *project = isSgProject(node);
    ROSE_ASSERT(project != NULL);

    // GB (8/19/2009): Added this call to perform post-processing on
    // the entire project at once. Conversely, commented out the
    // loop iterating over all files because repeated calls to
    // AstPostProcessing are slow due to repeated memory pool
    // traversals of the same nodes over and over again.
    // Only postprocess the AST if it was generated, and not were we just did
    // the parsing. postProcessingSupport(node);

    // printf ("In AstPostProcessing(): project->get_exit_after_parser() = %s
    // \n",project->get_exit_after_parser() ? "true" : "false");
    if (project->get_exit_after_parser() == false) {
      postProcessingSupport(node);
      ranPostProcessing = true;
    }

    // printf ("SgProject support not implemented in AstPostProcessing \n");
    // ROSE_ABORT();
    break;
  }

  case V_SgDirectory: {
    ROSE_ASSERT(isSgDirectory(node));

    printf("SgDirectory support not implemented in AstPostProcessing \n");
    ROSE_ABORT();
  }

  case V_SgFile:
  case V_SgSourceFile: {
    SgFile *file = isSgFile(node);
    ROSE_ASSERT(file != NULL);

    // Only postprocess the AST if it was generated, and not were we just did
    // the parsing.
    if (file->get_exit_after_parser() == false) {
      postProcessingSupport(node);
      ranPostProcessing = true;
    }

    break;
  }

  default: {
    // list general post-processing fixup here ...
    postProcessingSupport(node);
    ranPostProcessing = true;
  }
  }

  if (ranPostProcessing && usesClangFrontend(node) &&
      (isSgProject(node) != nullptr || isSgSourceFile(node) != nullptr)) {
    repairMalformedSymbolsInMemoryPool();
  }

  // DQ (3/17/2007): Clear the static globalMangledNameMap, likely this is not
  // enough and the mangled name map should not be used while the names of
  // scopes are being reset (done in the AST post-processing).
  SgNode::clearGlobalMangledNameMap();
}

// DQ (3/4/2007): part of tempoary support for debugging where a defining and
// nondefining declaration are the same SgDeclarationStatement*
// saved_declaration;

void postProcessingSupport(SgNode *node) {
  // DQ (5/24/2006): Added this test to figue out where Symbol parent pointers
  // are being reset to NULL TestParentPointersOfSymbols::test();

  // DQ (7/25/2005): It is presently an error to call this function with a
  // SgProject or SgDirectory, since there is no way to compute the SgFile from
  // such IR nodes (could be multiply defined). ROSE_ASSERT(isSgProject(node) ==
  // NULL && isSgDirectory(node) == NULL); GB (8/19/2009): Removed the assertion
  // against calling this function on SgProject and SgDirectory nodes. Nothing
  // below needs to compute a SgFile, as far as I can tell; also, calling the
  // AstPostProcessing just once on an entire project is more efficient than
  // calling it once per file.

  // Only do AST post-processing for C/C++.
  bool noPostprocessing = (SageInterface::is_Fortran_language() == true);

  // If this is C or C++ then we are using the new legacy frontend
  // translation and using fewer fixups should be required, some are still
  // required.
  if (noPostprocessing == false) {
    // Skip memory-pool traversal of nodes that are not reachable from this AST
    // root. This avoids touching stray template instantiations left in the
    // pools by the frontend (e.g., Clang system-header artifacts) without
    // relying on path checks.
    MemoryPoolFilterGuard memoryPoolFilter(&isUnreachableFromProject, node);

// DQ (10/27/2015): Added test to detect cycles in typedef types.
#define DEBUG_TYPEDEF_CYCLES 0

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif

    // DQ (5/1/2012): After legacy frontend/ROSE translation, there should
    // be no IR nodes marked as transformations. Liao 11/21/2012.
    // AstPostProcessing() is called within both Frontend and Midend so we
    // have to detect the mode first before asserting no transformation
    // generated file info objects
    if (SageBuilder::SourcePositionClassificationMode !=
        SageBuilder::e_sourcePositionTransformation) {
      detectTransformations(node);
    }

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupTypeReferences() \n");
    }

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif

    if (SgProject::get_verbose() > 1) {
      printf("Calling topLevelResetParentPointer() \n");
    }

    // Reset and test and parent pointers so that it matches our definition
    // of the AST (as defined by the AST traversal mechanism).
    topLevelResetParentPointer(node);
    debugDumpTrackedStdTemplateState("after-topLevelResetParentPointer", node);

    if (SgProject::get_verbose() > 1) {
      printf("DONE: Calling topLevelResetParentPointer() \n");
    }

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif

    if (SgProject::get_verbose() > 1) {
      printf("Calling resetParentPointersInMemoryPool() \n");
    }

    // DQ (8/23/2012): Modified to take a SgNode so that we could compute the
    // global scope for use in setting parents of template instantiations that
    // have not be placed into the AST but exist in the memory pool. Another 2nd
    // step to make sure that parents of even IR nodes not traversed can be set
    // properly. resetParentPointersInMemoryPool();
    resetParentPointersInMemoryPool(node);
    debugDumpTrackedStdTemplateState("after-resetParentPointersInMemoryPool",
                                     node);

    if (SgProject::get_verbose() > 1) {
      printf("DONE: Calling resetParentPointersInMemoryPool() \n");
    }

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupAstDefiningAndNondefiningDeclarations() \n");
    }

    // DQ (6/27/2005): fixup the defining and non-defining declarations
    // referenced at each SgDeclarationStatement This is a more sophisticated
    // fixup than that done by fixupDeclarations. See test2009_09.C for an
    // example of a non-defining declaration appearing before a defining
    // declaration and requiring a fixup of the non-defining declaration
    // reference to the defining declaration.
    fixupAstDefiningAndNondefiningDeclarations(node);
    debugDumpTrackedStdTemplateState(
        "after-fixupAstDefiningAndNondefiningDeclarations", node);

#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
#endif
#if DEBUG_TYPEDEF_CYCLES
    printf("Calling TestAstForCyclesInTypedefs() \n");
    TestAstForCyclesInTypedefs::test();
    printf("DONE: Calling TestAstForCyclesInTypedefs() \n");
#endif
    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupAstSymbolTables() \n");
    }

    // Fixup the symbol tables (in each scope) and the global function
    // type symbol table. This is less important for C, but required
    // for C++. But since the new legacy frontend interface has to
    // handle C and C++ we don't setup the global function type table
    // there to be uniform.
    fixupAstSymbolTables(node);
    debugDumpTrackedStdTemplateState("after-fixupAstSymbolTables", node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupAstSymbolTablesToSupportAliasedSymbols() \n");
    }

    // DQ (4/14/2010): Added support for symbol aliases for C++
    // This is the support for C++ "using declarations" which uses symbol
    // aliases in the symbol table to provide correct visability of symbols
    // included from alternative scopes (e.g. namespaces).
    fixupAstSymbolTablesToSupportAliasedSymbols(node);
    debugDumpTrackedStdTemplateState(
        "after-fixupAstSymbolTablesToSupportAliasedSymbols", node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling resetTemplateNames() \n");
    }

    // DQ (2/12/2012): Added support for this, since AST_consistancy expects
    // get_nameResetFromMangledForm() == true.
    resetTemplateNames(node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupTemplateInstantiations() \n");
    }

    // **********************************************************************
    // DQ (4/29/2012): Added some of the template fixup support for
    // legacy frontend 4.3 work. DQ (6/21/2005): This function now only
    // marks the subtrees of all appropriate declarations as compiler
    // generated. DQ (5/27/2005): mark all template instantiations
    // (which we generate as template specializations) as compiler
    // generated. This is required to make them pass the unparser and
    // the phase where comments are attached.  Some fixup of filenames
    // and line numbers might also be required.
    fixupTemplateInstantiations(node);
    debugDumpTrackedStdTemplateState("after-fixupTemplateInstantiations", node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling markTemplateSpecializationsForOutput() \n");
    }

    // DQ (8/19/2005): Mark any template specialization (C++ specializations are
    // template instantiations that are explicit in the source code).  Such
    // template specializations are marked for output only if they are present
    // in the source file.  This detail could effect handling of header files
    // later on. Have this phase preceed the
    // markTemplateInstantiationsForOutput() since all specializations should be
    // searched for uses of (references to) instantiated template functions and
    // member functions.
    markTemplateSpecializationsForOutput(node);
    debugDumpTrackedStdTemplateState(
        "after-markTemplateSpecializationsForOutput", node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling markTemplateInstantiationsForOutput() \n");
    }

    // DQ (6/21/2005): This function marks template declarations for
    // output by the unparser (it is part of a fixed point iteration
    // over the AST to force find all templates that are required
    // (legacy frontend at the moment outputs only though template
    // functions that are required, but this function solves the more
    // general problem of instantiation of both function and member
    // function templates (and static data, later)).
    markTemplateInstantiationsForOutput(node);
    debugDumpTrackedStdTemplateState(
        "after-markTemplateInstantiationsForOutput", node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupFriendTemplateDeclarations() \n");
    }

    // DQ (10/21/2007): Friend template functions were previously not properly
    // marked which caused their generated template symbols to be added to the
    // wrong symbol tables.  This is a cause of numerous symbol table problems.
    fixupFriendTemplateDeclarations();
    // DQ (4/29/2012): End of new template fixup support for legacy
    // frontend 4.3 work.
    // **********************************************************************

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupSourcePositionConstructs() \n");
    }

    // DQ (5/14/2012): Fixup source code position information for the end of
    // functions to match the largest values in their subtree. DQ (10/27/2007):
    // Setup any endOfConstruct Sg_File_Info objects (report on where they
    // occur)
    fixupSourcePositionConstructs();
    debugDumpTrackedStdTemplateState("after-fixupSourcePositionConstructs",
                                     node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupTemplateArguments() \n");
    }

    // DQ (2/11/2017): Changed API to use SgSimpleProcessing based traversal.
    // DQ (11/27/2016): Fixup template arguments to additionally reference a
    // type that can be unparsed. fixupTemplateArguments();
    fixupTemplateArguments(node);
    debugDumpTrackedStdTemplateState("after-fixupTemplateArguments", node);

    // DQ (2/12/2012): This is a problem for test2004_35.C (debugging this
    // issue). printf ("Exiting after calling resetTemplateNames() \n");
    // ROSE_ABORT();

    if (SgProject::get_verbose() > 1) {
      printf("Calling resetConstantFoldedValues() \n");
    }

    // DQ (10/4/2012): Added this pass to support command line option to control
    // use of constant folding (fixes bug pointed out by Liao). DQ (9/14/2011):
    // Process the AST to remove constant folded values held in the expression
    // trees. This step defines a consistent AST more suitable for analysis
    // since only the constant folded values will be visited.  However, the
    // default should be to save the original expression trees and remove the
    // constant folded values since this represents the original code.

    SgProject *project = isSgProject(node);
    if (project != NULL &&
        project->get_suppressConstantFoldingPostProcessing() == false) {
      resetConstantFoldedValues(node);
    } else if (project != NULL && SgProject::get_verbose() >= 1) {
      MLOG_DEBUG_C(
          "astPostProcessing",
          "In postProcessingSupport: skipping call to "
          "resetConstantFoldedValues(): "
          "project->get_suppressConstantFoldingPostProcessing() = %s \n",
          project->get_suppressConstantFoldingPostProcessing() ? "true"
                                                               : "false");
    } else if (project == NULL) {
      MLOG_DEBUG_C("astPostProcessing", "postProcessingSupport should not be "
                                        "called for non SgProject IR nodes \n");
    }

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupSelfReferentialMacrosInAST() \n");
    }

    // DQ (10/5/2012): Fixup known macros that might expand into a recursive
    // mess in the unparsed code.
    fixupSelfReferentialMacrosInAST(node);

    // Make sure that frontend-specific and compiler-generated AST nodes are
    // marked as such. These two must run in this order since
    // checkIsCompilerGenerated depends on correct values of compiler-generated
    // flags.
    checkIsFrontendSpecificFlag(node);
    checkIsCompilerGeneratedFlag(node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupFileInfoInconsistanties() \n");
    }

    // DQ (11/14/2015): Fixup inconsistancies across the multiple Sg_File_Info
    // obejcts in SgLocatedNode and SgExpression IR nodes.
    fixupFileInfoInconsistanties(node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling markSharedDeclarationsForOutputInCodeGeneration() \n");
    }

    // DQ (2/25/2019): Adding support to mark shared defining declarations
    // across multiple files.
    markSharedDeclarationsForOutputInCodeGeneration(node);
    debugDumpTrackedStdTemplateState(
        "after-markSharedDeclarationsForOutputInCodeGeneration", node);

    // Prune symbols for constraint-unsatisfied instantiations after all
    // symbol-table fixups have completed.
    pruneSymbolsForConstraintFailures(node);

    // Late declaration fixups can relink class declaration chains after the
    // earlier template-instantiation pass. Re-canonicalize all reachable class
    // types in the memory pool so shared type nodes point back to the first
    // nondefining declaration.
    canonicalizeClassTypesInMemoryPool(node);

    debugDumpTrackedStdTemplateState("final", node);
    if (SgProject::get_verbose() > 1) {
      printf("Calling checkIsModifiedFlag() \n");
    }

    // This resets the isModified flag on each IR node so that we can record
    // where transformations are done in the AST.  If any transformations on
    // the AST are done, even just building it, this step should be the final
    // step.

    // DQ (4/16/2015): This is replaced with a better implementation.
    // checkIsModifiedFlag(node);
    unsetNodesMarkedAsModified(node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling detectTransformations() \n");
    }

    // DQ (5/2/2012): After legacy frontend/ROSE translation, there
    // should be no IR nodes marked as transformations. Liao
    // 11/21/2012. AstPostProcessing() is called within both Frontend
    // and Midend so we have to detect the mode first before asserting
    // no transformation generated file info objects
    if (SageBuilder::SourcePositionClassificationMode !=
        SageBuilder::e_sourcePositionTransformation) {
      detectTransformations(node);
    }

    if (SgProject::get_verbose() > 1) {
      printf("Calling fixupFunctionDefaultArguments() \n");
    }

    // DQ (4/24/2013): Detect the correct function declaration to declare the
    // use of default arguments. This can only be a single function and it can't
    // be any function (this is a moderately complex issue).
    fixupFunctionDefaultArguments(node);

    if (SgProject::get_verbose() > 1) {
      printf("Calling addPrototypesForTemplateInstantiations() \n");
    }

    // DQ (5/18/2017): Adding missing prototypes.
    addPrototypesForTemplateInstantiations(node);

    // DQ (12/20/2012): We now store the logical and physical source position
    // information. Although they are frequently the same, the use of #line
    // directives causes them to be different. This is part of debugging the
    // physical source position information which is used in the weaving of the
    // comments and CPP directives into the AST.  For this the consistancy check
    // is more helpful if done befor it is used (here), instead of after the
    // comment and CPP directive insertion in the AST Consistancy tests.
    if (SgProject::get_verbose() > 1) {
      printf("Calling checkPhysicalSourcePosition() \n");
    }

    checkPhysicalSourcePosition(node);

    // Final declaration and prototype fixups above can still allocate or relink
    // class types. Re-canonicalize the memory pool at the end of the pipeline
    // so every reachable SgClassType points back to the first nondefining
    // declaration in its class declaration chain.
    canonicalizeClassTypesInMemoryPool(node);

    // Late C++ declaration fixups can still leave hidden helper redeclarations
    // or broken first-nondefining chains rooted on non-visible function
    // declarations. Normalize the visible declaration surface before unparsing
    // consults symbol ownership for name qualification.
    repairHiddenFunctionDeclarationSurfaces(node);
    repairTypedefOwnedTagDeclarationSurfaces(node);
    repairBrokenFunctionDeclarationChains(node);
    pruneDuplicateFunctionDeclarationSurfaces(node);
    markNormalizedTemplateDeclarationSurfaces(node);

    parkDetachedSymbolsInMemoryPool();

    return;
  } else {

    ROSE_ASSERT(node != NULL);

    // DQ (7/19/2005): Moved to after parent pointer fixup!
    // subTemporaryAstFixes(node);

    // DQ (3/11/2006): Fixup NULL pointers left by users when building the AST
    // (note that the AST translation fixes these directly).  This step is
    // provided as a way to make the AST build by users consistant with what
    // is built elsewhere within ROSE.
    fixupNullPointersInAST(node);

    // DQ (8/9/2005): Some function definitions in third-party headers are
    // built without a body (example in test2005_102.C, but it appears to
    // work fine).
    fixupFunctionDefinitions(node);

    // DQ (8/10/2005): correct any template declarations mistakenly marked as
    // compiler-generated
    fixupTemplateDeclarations(node);

    // Output progress comments for these relatively expensive operations on the
    // AST
    MLOG_KEY_CXX("astPostProcessing")
        << "/* AST Postprocessing reset parent pointers */" << endl;

    topLevelResetParentPointer(node);

    // DQ (6/10/2007): This is called later, but call it now to reset the
    // parents in SgTemplateInstantiationDecl This is required (I think) so that
    // resetTemplateNames() can compute template argument name qualification
    // correctly. See test2005_28.C for where this is required.
    // resetParentPointersInMemoryPool();
    resetParentPointersInMemoryPool(node);

    // Output progress comments for these relatively expensive operations on the
    // AST
    MLOG_KEY_CXX("astPostProcessing")
        << "/* AST Postprocessing reset parent pointers (done) */" << endl;

    // DQ (7/19/2005): Moved to after parent pointer fixup!
    // subTemporaryAstFixes(node);
    removeInitializedNamePtr(node);

    // DQ (3/17/2007): This should be empty
    ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (12/1/2004): This should be done before the reset of template names
    // (since that operation requires valid scopes!) DQ (11/29/2004): Added to
    // support new explicit scope information on IR nodes
    // initializeExplicitScopeData(node);
    initializeExplicitScopes(node);

    // DQ (5/28/2006): Fixup names in declarations that are inconsistent (e.g.
    // where more than one non-defining declaration exists)
    resetNamesInAST();

    // DQ (3/17/2007): This should be empty
    ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // Output progress comments for these relatively expensive operations on the
    // AST
    MLOG_KEY_CXX("astPostProcessing")
        << "/* AST Postprocessing reset template names */" << endl;

    // DQ (5/15/2011): This causes template names to be computed as strings and
    // and without name qualification if we don't call the name qualification
    // before here. Or we reset the template names after we do the analysis to
    // support the name qualification. reset the names of template class
    // declarations
    resetTemplateNames(node);

    // DQ (2/12/2012): This is a problem for test2004_35.C (debugging this
    // issue). printf ("Exiting after calling resetTemplateNames() \n");
    // ROSE_ABORT();

    // DQ (3/17/2007): This should be empty
    ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // Output progress comments for these relatively expensive operations on the
    // AST
    MLOG_KEY_CXX("astPostProcessing")
        << "/* AST Postprocessing reset template names (done) */" << endl;

    // DQ (6/26/2007): Enum values used before they are defined in a class can
    // have NULL declaration pointers.  This fixup handles this case and
    // traverses just the SgEnumVal objects.
    fixupEnumValues();

    // DQ (4/7/2010): This was commented out to modify Fortran code, but I think
    // it should NOT modify Fortran code. DQ (5/21/2008): This only make since
    // for C and C++ (Error, this DOES apply to Fortran where the "parameter"
    // attribute is used!)
    if (SageInterface::is_Fortran_language() == false) {
      // DQ (3/20/2005): Fixup AST so that GNU g++ compile-able code will be
      // generated
      fixupInClassDataInitialization(node);
    }

    // DQ (3/24/2005): Fixup AST to generate code that works around GNU g++ bugs
    fixupforGnuBackendCompiler(node);

    // DQ (4/19/2005): fixup all definingDeclaration and NondefiningDeclaration
    // pointers in SgDeclarationStatement IR nodes fixupDeclarations(node);

    // DQ (5/20/2005): make the non-defining (forward) declarations added
    // by legacy frontend for static template specializations added under
    // the "--instantiation local" option match the defining declarations.
    fixupStorageAccessOfForwardTemplateDeclarations(node);

    // DQ (6/21/2005): This function now only marks the subtrees of all
    // appropriate declarations as compiler generated. DQ (5/27/2005): mark all
    // template instantiations (which we generate as template specializations)
    // as compiler generated. This is required to make them pass the unparser
    // and the phase where comments are attached.  Some fixup of filenames and
    // line numbers might also be required.
    fixupTemplateInstantiations(node);

    // fixupTemplateInstantiations() can synthesize or relink template
    // declarations for class instantiations. Re-run the template-declaration
    // invariant fixup after that pass so those newly linked declarations do
    // not remain marked compiler-generated.
    fixupTemplateDeclarations(node);

    // DQ (8/19/2005): Mark any template specialization (C++ specializations are
    // template instantiations that are explicit in the source code).  Such
    // template specializations are marked for output only if they are present
    // in the source file.  This detail could effect handling of header files
    // later on. Have this phase preceed the
    // markTemplateInstantiationsForOutput() since all specializations should be
    // searched for uses of (references to) instantiated template functions and
    // member functions.
    markTemplateSpecializationsForOutput(node);

    // DQ (6/21/2005): This function marks template declarations for
    // output by the unparser (it is part of a fixed point iteration over
    // the AST to force find all templates that are required (legacy
    // frontend at the moment outputs only though template functions that
    // are required, but this function solves the more general problem of
    // instantiation of both function and member function templates (and
    // static data, later)).
    markTemplateInstantiationsForOutput(node);

    // DQ (3/17/2007): This should be empty
    ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (3/16/2006): fixup any newly added declarations (see if we can
    // eliminate the first place where this is called, above) fixup all
    // definingDeclaration and NondefiningDeclaration pointers in
    // SgDeclarationStatement IR nodes driscoll6 (6/10/11): this traversal
    // sets p_firstNondefiningDeclaration for defining declarations.
    fixupDeclarations(node);

    // DQ (3/17/2007): This should be empty
    ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (2/12/2006): Moved to trail marking templates (as a test)
    // DQ (6/27/2005): fixup the defining and non-defining declarations
    // referenced at each SgDeclarationStatement This is a more sophisticated
    // fixup than that done by fixupDeclarations.
    fixupAstDefiningAndNondefiningDeclarations(node);

    // DQ (10/21/2007): Friend template functions were previously not properly
    // marked which caused their generated template symbols to be added to the
    // wrong symbol tables.  This is a cause of numerous symbol table problems.
    fixupFriendTemplateDeclarations();

    canonicalizeClassTypesInMemoryPool(node);

    // DQ (3/17/2007): This should be the last point at which the
    // globalMangledNameMap is empty The fixupAstSymbolTables will generate
    // calls to function types that will be placed into the
    // globalMangledNameMap.
    ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (6/26/2005): The global function type symbol table should be rebuilt
    // (since the names of templates used in qualified names of types have been
    // reset (in post processing).  Other local symbol tables should be
    // initalized and constructed for any empty scopes (for consistancy, we want
    // all scopes to have a valid symbol table pointer).
    fixupAstSymbolTables(node);

    // DQ (3/17/2007): At this point the globalMangledNameMap has been used in
    // the symbol table construction. OK.
    // ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (8/20/2005): Handle backend vendor specific template handling options
    // (e.g. g++ options: -fno-implicit-templates and
    // -fno-implicit-inline-templates)
    processTemplateHandlingOptions(node);

    // DQ (5/22/2005): relocate compiler generated forward template
    // instantiation declarations to appear after the template declarations and
    // before first use.
    // relocateCompilerGeneratedTemplateInstantiationDeclarationsInAST(node);

    // DQ (8/27/2005): This disables output of some template instantiations that
    // would result in "ambiguous template specialization" in g++
    // (version 3.3.x, 3.4.x, and 4.x).  See test2005_150.C for more detail.
    markOverloadedTemplateInstantiations(node);

    // DQ (9/5/2005): Need to mark all nodes in any subtree marked as a
    // transformation
    markTransformationsForOutput(node);

    // DQ (3/5/2006): Mark functions that are provided for backend compatability
    // as compiler generated by ROSE
    markBackendSpecificFunctionsAsCompilerGenerated(node);

    // DQ (5/24/2006): Added this test to figure out where Symbol parent
    // pointers are being reset to NULL TestParentPointersOfSymbols::test();

    // DQ (5/24/2006): reset the remaining parents in IR nodes missed by the AST
    // based traversals resetParentPointersInMemoryPool();
    resetParentPointersInMemoryPool(node);

    // Some friend function normalizations depend on finalized lexical parents.
    // Re-run the friend declaration fixup after parent reset so hidden-friend
    // template specializations attached to class bodies are visible
    // structurally.
    fixupFriendDeclarations();

    // DQ (3/17/2007): This should be empty
    // ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (5/29/2006): Fixup types in declarations that are not shared (e.g.
    // where more than one non-defining declaration exists)
    resetTypesInAST();

    // DQ (3/17/2007): This should be empty
    // ROSE_ASSERT(SgNode::get_globalMangledNameMap().size() == 0);

    // DQ (3/10/2007): fixup name of any template classes that have been copied
    // incorrectly into SgInitializedName list in base class constructor
    // preinitialization lists (see test2004_156.C for an example).
    resetContructorInitilizerLists();

    // DQ (10/27/2007): Setup any endOfConstruct Sg_File_Info objects (report on
    // where they occur)
    fixupSourcePositionConstructs();

    // DQ (1/19/2008): This can be called at nearly any point in the ast fixup.
    markLhsValues(node);

    // REX: legacy frontend-specific __PRETTY_FUNCTION__ fixup not needed
    // for Clang DQ (2/21/2010): legacy frontend had a trick where it
    // replaced "__PRETTY_FUNCTION__" variable references with variables
    // named after the containing function. This fixup normalized the
    // names back to "__PRETTY_FUNCTION__" for compatibility with GNU
    // preprocessor output. Clang handles __PRETTY_FUNCTION__ natively, so
    // this fixup is not needed.

    // DQ (11/24/2007): Support for Fortran resolution of array vs.
    // function references.
    if (SageInterface::is_Fortran_language() == true) {
      // I think this is not used since I can always figure out if something is
      // an array reference or a function call.
      fixupFortranReferences(node);

      // DQ (10/3/2008): This bug in OFP is now fixed so no fixup is required.
      // This is the most reliable way to introduce the Fortran "contains"
      // statement. insertFortranContainsStatement(node);
    }

    // DQ (9/26/2008): fixup the handling of use declarations (SgUseStatement).
    // This also will fixup C++ using declarations.
    fixupFortranUseDeclarations(node);

    // DQ (4/14/2010): Added support for symbol aliases for C++
    // This is the support for C++ "using declarations" which uses symbol
    // aliases in the symbol table to provide correct visability of symbols
    // included from alternative scopes (e.g. namespaces).
    fixupAstSymbolTablesToSupportAliasedSymbols(node);

    // DQ (6/24/2010): To support merge, we want to normalize the typedef lists
    // for each type so that the names of the types will evaluate to be the same
    // (and merge appropriately).
    normalizeTypedefSequenceLists();

    // Make sure that compiler-generated AST nodes are marked for
    // Sg_File_Info::isCompilerGenerated().
    checkIsCompilerGeneratedFlag(node);

    // DQ (4/16/2015): This is replaced with a better implementation.
    // DQ (5/22/2005): Nearly all AST fixup should be done before this closing
    // step QY: check the isModified flag CheckIsModifiedFlagSupport(node);
    // checkIsModifiedFlag(node);
    unsetNodesMarkedAsModified(node);

    // Late postprocessing can leave hidden compiler-generated redeclarations in
    // namespace/global declaration lists while dropping the real source-backed
    // first declaration they reference. Reinsert the visible declaration by
    // source order so unparsing sees the public surface, not the hidden helper.
    repairHiddenFunctionDeclarationSurfaces(node);
    repairTypedefOwnedTagDeclarationSurfaces(node);

    // Friend/template cleanup can leave function declaration chains rooted at
    // a hidden or defining declaration. Normalize the first-nondefining links
    // before name-qualification traversals consult symbol ownership.
    repairBrokenFunctionDeclarationChains(node);
    pruneDuplicateFunctionDeclarationSurfaces(node);

    // Late declaration/symbol-table normalization can re-self-link template
    // instantiation function declarations after the earlier template fixup.
    // Re-run the template-instantiation repair once the frontend pipeline has
    // finished mutating declaration chains so the final AST retains a valid
    // first-nondefining/defining relationship.
    fixupTemplateInstantiations(node);

    // Normalized declaration formatting emits template declarations from the
    // AST. Mark them as transformation surfaces so token-frontier construction
    // does not replay the original surrounding template text in parallel.
    markNormalizedTemplateDeclarationSurfaces(node);

    // This is used for both of the fillowing tests.
    SgSourceFile *sourceFile = isSgSourceFile(node);

    // DQ (9/11/2009): Added support for numbering of statements required to
    // support name qualification.
    if (sourceFile != NULL) {
      // DQ (9/11/2009): Added support for numbering of statements required to
      // support name qualification. sourceFile->buildStatementNumbering();
      SgGlobal *globalScope = sourceFile->get_globalScope();
      ROSE_ASSERT(globalScope != NULL);
      globalScope->buildStatementNumbering();
    } else {
      if (SgProject *project = isSgProject(node)) {
        SgFilePtrList &files = project->get_fileList();
        for (SgFilePtrList::iterator fileI = files.begin();
             fileI != files.end(); ++fileI) {
          if ((sourceFile = isSgSourceFile(*fileI))) {
            SgGlobal *globalScope = sourceFile->get_globalScope();
            ROSE_ASSERT(globalScope != NULL);
            globalScope->buildStatementNumbering();
          }
        }
      }
    }

    // DQ (4/4/2010): check that the global scope has statements.
    // This was an error for Fortran and it appeared that everything
    // was working when it was not.  It appeared because of a strange
    // error between versions of the OFP support files.  So as a
    // way to avoid this in the future, we issue a warning for Fortran
    // code that has no statements in the global scope.  It can still
    // be a valid Fortran code (containing only comments).  but this
    // should help avoid our test codes appearing to work when they
    // don't (in the future). For C/C++ files there should always be
    // something in the global scope (because or ROSE defined functions),
    // so this test should not be a problem.
    if (sourceFile != NULL) {
      SgGlobal *globalScope = sourceFile->get_globalScope();
      ROSE_ASSERT(globalScope != NULL);
      if (globalScope->get_declarations().empty() == true) {
        // DQ (3/17/2017): Added support to use message streams.
        MLOG_DEBUG_C("astPostProcessing",
                     "WARNING: no statements in global scope for file = %s \n",
                     sourceFile->getFileName().c_str());
      }
    } else {
      if (SgProject *project = isSgProject(node)) {
        SgFilePtrList &files = project->get_fileList();
        for (SgFilePtrList::iterator fileI = files.begin();
             fileI != files.end(); ++fileI) {
          if ((sourceFile = isSgSourceFile(*fileI))) {
            SgGlobal *globalScope = sourceFile->get_globalScope();
            ROSE_ASSERT(globalScope != NULL);
            if (globalScope->get_declarations().empty() == true) {
              // DQ (3/17/2017): Added support to use message streams.
              MLOG_DEBUG_C(
                  "astPostProcessing",
                  "WARNING: no statements in global scope for file = %s \n",
                  (*fileI)->getFileName().c_str());
            }
          }
        }
      }
    }

    // Keep class-type declarations canonical after the complete post-processing
    // pipeline, including late type reset and symbol-table fixups.
    canonicalizeClassTypesInMemoryPool(node);
    parkDetachedSymbolsInMemoryPool();
  }
}
