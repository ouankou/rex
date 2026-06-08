#include "nameQualificationSupport.h"

#include "sage3basic.h"

#include "nonrealQualificationSupport.h"
#include "sageGeneric.h"

#include <unordered_map>

using namespace std;

namespace si = SageInterface;

// This value must be greater than 3 to cause most output to be generated.
#define DEBUG_NAME_QUALIFICATION_LEVEL 0

#ifndef WARNING_FOR_NONREAL_DEVEL
#define WARNING_FOR_NONREAL_DEVEL 0
#endif

// DQ (9/2/2020): Moved to the top of the file from the SgInitializedName case
// in the evaluate inherited attribute function. DQ (4/27/2019): Set these to be
// the same for now.
#define DEBUG_INITIALIZED_NAME 0
// #define DEBUG_INITIALIZED_NAME DEBUG_NAME_QUALIFICATION_LEVEL

namespace {
size_t count_scope_qualifiers(const std::string &name) {
  size_t count = 0;
  size_t pos = name.find("::");
  while (pos != std::string::npos) {
    ++count;
    pos = name.find("::", pos + 2);
  }
  return count;
}

bool should_replace_type_name(const std::string &existing,
                              const std::string &candidate) {
  const size_t existing_depth = count_scope_qualifiers(existing);
  const size_t candidate_depth = count_scope_qualifiers(candidate);
  if (candidate_depth != existing_depth) {
    return candidate_depth > existing_depth;
  }
  return candidate.size() > existing.size();
}

bool should_preseed_referenced_name_for_untraversed_declaration(
    SgNode *reference_node, SgDeclarationStatement *declaration) {
  if (reference_node == NULL || declaration == NULL) {
    return true;
  }

  auto physical_filename = [](SgNode *node) -> std::string {
    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == NULL) {
      return "";
    }

    auto pick_filename = [](Sg_File_Info *fi) -> std::string {
      if (fi == NULL) {
        return "";
      }
      const std::string filename = fi->get_filenameString();
      if (filename.empty() || filename == "NULL_FILE") {
        return "";
      }
      return filename;
    };

    std::string filename = pick_filename(located->get_startOfConstruct());
    if (!filename.empty()) {
      return filename;
    }

    filename = pick_filename(located->get_file_info());
    if (!filename.empty()) {
      return filename;
    }

    return pick_filename(located->get_endOfConstruct());
  };

  const std::string reference_filename = physical_filename(reference_node);
  const std::string declaration_filename = physical_filename(declaration);
  if (!reference_filename.empty() && !declaration_filename.empty() &&
      reference_filename != declaration_filename) {
    return true;
  }

  SgSourceFile *reference_file =
      SageInterface::getEnclosingSourceFile(reference_node);
  SgSourceFile *declaration_file =
      SageInterface::getEnclosingSourceFile(declaration);
  if (reference_file == NULL || declaration_file == NULL) {
    return true;
  }

  return reference_file != declaration_file;
}

std::string build_explicit_qualifier_string(const SgStringList &tokens,
                                            bool explicit_global) {
  std::string qualifier;
  if (explicit_global) {
    qualifier = "::";
  }
  for (const std::string &token : tokens) {
    if (token.empty()) {
      continue;
    }
    if (!qualifier.empty() && qualifier.back() != ':') {
      qualifier += "::";
    }
    qualifier += token;
    qualifier += "::";
  }
  return qualifier;
}

bool getExplicitQualifierTokens(const SgNode *node, SgStringList &tokens,
                                bool &explicit_global) {
  tokens.clear();
  explicit_global = false;

  if (const SgTemplateMemberFunctionRefExp *tmpl_member =
          isSgTemplateMemberFunctionRefExp(node)) {
    tokens = tmpl_member->get_explicit_name_qualification_tokens();
    explicit_global = tmpl_member->get_explicit_global_qualification();
  } else if (const SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(node)) {
    tokens = member_ref->get_explicit_name_qualification_tokens();
    explicit_global = member_ref->get_explicit_global_qualification();
  } else if (const SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(node)) {
    tokens = tmpl_func->get_explicit_name_qualification_tokens();
    explicit_global = tmpl_func->get_explicit_global_qualification();
  } else if (const SgFunctionRefExp *func_ref = isSgFunctionRefExp(node)) {
    tokens = func_ref->get_explicit_name_qualification_tokens();
    explicit_global = func_ref->get_explicit_global_qualification();
  } else if (const SgVarRefExp *var_ref = isSgVarRefExp(node)) {
    tokens = var_ref->get_explicit_name_qualification_tokens();
    explicit_global = var_ref->get_explicit_global_qualification();
  } else if (const SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(node)) {
    tokens = nonreal_ref->get_explicit_name_qualification_tokens();
    explicit_global = nonreal_ref->get_explicit_global_qualification();
  } else if (const SgEnumVal *enum_val = isSgEnumVal(node)) {
    tokens = enum_val->get_explicit_name_qualification_tokens();
    explicit_global = enum_val->get_explicit_global_qualification();
  } else {
    return false;
  }

  return !tokens.empty() || explicit_global;
}

bool is_class_like_scope_for_name_qualification(SgScopeStatement *scope) {
  return isSgClassDefinition(scope) != NULL ||
         isSgTemplateClassDefinition(scope) != NULL ||
         isSgTemplateInstantiationDefn(scope) != NULL;
}

bool declaration_has_real_visible_source(SgDeclarationStatement *decl) {
  if (decl == NULL) {
    return false;
  }

  Sg_File_Info *fi = decl->get_file_info();
  if (fi == NULL) {
    return false;
  }

  if (fi->isCompilerGenerated() || fi->isTransformation() ||
      fi->isFrontendSpecific()) {
    return false;
  }

  return fi->isOutputInCodeGeneration();
}

bool is_hidden_friend_free_function_decl(SgFunctionDeclaration *decl) {
  if (decl == NULL) {
    return false;
  }

  bool has_lexical_friend_in_class_scope = false;
  bool has_visible_nonclass_source_decl = false;

  SgFunctionDeclaration *candidates[] = {
      decl, isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration()),
      isSgFunctionDeclaration(decl->get_definingDeclaration())};

  for (SgFunctionDeclaration *candidate : candidates) {
    if (candidate == NULL) {
      continue;
    }

    SgScopeStatement *parent_scope =
        isSgScopeStatement(candidate->get_parent());
    if (candidate->get_declarationModifier().isFriend() &&
        is_class_like_scope_for_name_qualification(parent_scope)) {
      has_lexical_friend_in_class_scope = true;
    }

    if (declaration_has_real_visible_source(candidate) &&
        !is_class_like_scope_for_name_qualification(parent_scope)) {
      has_visible_nonclass_source_decl = true;
    }
  }

  return has_lexical_friend_in_class_scope && !has_visible_nonclass_source_decl;
}

bool scopes_are_equivalent_for_name_qualification(SgScopeStatement *lhs,
                                                  SgScopeStatement *rhs) {
  return lhs == rhs || (lhs != NULL && rhs != NULL &&
                        SgScopeStatement::isEquivalentScope(lhs, rhs));
}

bool applyExplicitQualifier(const SgNode *node, std::string &qualifier,
                            int &output_length, bool &output_global) {
  SgStringList tokens;
  bool explicit_global = false;
  if (!getExplicitQualifierTokens(node, tokens, explicit_global)) {
    return false;
  }

  qualifier = build_explicit_qualifier_string(tokens, explicit_global);
  output_length = static_cast<int>(tokens.size());
  output_global = explicit_global;
  return true;
}

bool getExplicitQualifierLength(const SgNode *node, int &length) {
  int explicit_length = -1;
  bool explicit_global = false;
  SgStringList explicit_tokens;

  if (getExplicitQualifierTokens(node, explicit_tokens, explicit_global)) {
    explicit_length = static_cast<int>(explicit_tokens.size());
    int effective_length = explicit_length + (explicit_global ? 1 : 0);
    if (effective_length > 0) {
      length = effective_length;
      return true;
    }
  }

  if (const SgTemplateMemberFunctionRefExp *tmpl_member =
          isSgTemplateMemberFunctionRefExp(node)) {
    explicit_length = tmpl_member->get_explicit_name_qualification_length();
    explicit_global = tmpl_member->get_explicit_global_qualification();
  } else if (const SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(node)) {
    explicit_length = member_ref->get_explicit_name_qualification_length();
    explicit_global = member_ref->get_explicit_global_qualification();
  } else if (const SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(node)) {
    explicit_length = tmpl_func->get_explicit_name_qualification_length();
    explicit_global = tmpl_func->get_explicit_global_qualification();
  } else if (const SgFunctionRefExp *func_ref = isSgFunctionRefExp(node)) {
    explicit_length = func_ref->get_explicit_name_qualification_length();
    explicit_global = func_ref->get_explicit_global_qualification();
  } else if (const SgVarRefExp *var_ref = isSgVarRefExp(node)) {
    explicit_length = var_ref->get_explicit_name_qualification_length();
    explicit_global = var_ref->get_explicit_global_qualification();
  } else if (const SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(node)) {
    explicit_length = nonreal_ref->get_explicit_name_qualification_length();
    explicit_global = nonreal_ref->get_explicit_global_qualification();
  } else if (const SgEnumVal *enum_val = isSgEnumVal(node)) {
    explicit_length = enum_val->get_explicit_name_qualification_length();
    explicit_global = enum_val->get_explicit_global_qualification();
  } else {
    return false;
  }

  if (explicit_length < 0) {
    return false;
  }

  int effective_length = explicit_length + (explicit_global ? 1 : 0);
  length = effective_length;
  return true;
}

SgName
get_template_name_for_instantiation(SgDeclarationStatement *declaration) {
  if (SgTemplateInstantiationDecl *instantiation =
          isSgTemplateInstantiationDecl(declaration)) {
    if (SgTemplateClassDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    if (SgTemplateFunctionDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
          isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    if (SgTemplateMemberFunctionDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  if (SgTemplateInstantiationTypedefDeclaration *instantiation =
          isSgTemplateInstantiationTypedefDeclaration(declaration)) {
    if (SgTemplateTypedefDeclaration *templateDecl =
            instantiation->get_templateDeclaration()) {
      return templateDecl->get_name();
    }
  }
  return SgName();
}

SgDeclarationStatement *preferAssociatedDefinitionForTemplateInstantiation(
    SgDeclarationStatement *declaration) {
  auto *inst = isSgTemplateInstantiationDecl(declaration);
  if (inst == nullptr) {
    return declaration;
  }

  auto *def_inst =
      isSgTemplateInstantiationDecl(inst->get_definingDeclaration());
  if (def_inst == nullptr || def_inst == inst ||
      def_inst->get_definition() == nullptr) {
    return declaration;
  }

  const bool is_explicit_specialization =
      inst->get_specialization() == SgDeclarationStatement::e_specialization ||
      def_inst->get_specialization() ==
          SgDeclarationStatement::e_specialization;
  const bool defining_decl_will_unparse =
      def_inst->get_file_info() != nullptr &&
      def_inst->get_file_info()->isOutputInCodeGeneration();

  return (is_explicit_specialization || defining_decl_will_unparse)
             ? static_cast<SgDeclarationStatement *>(def_inst)
             : declaration;
}

struct ScopeUsingDirectiveOrderEntry {
  SgUsingDirectiveStatement *directive = NULL;
  size_t lexical_index = 0;
  unsigned int source_sequence = 0;
};

struct ScopeUsingDirectiveOrderCacheEntry {
  std::unordered_map<const SgStatement *, size_t> direct_child_index_cache;
  std::vector<ScopeUsingDirectiveOrderEntry> using_directives;
};

struct ScopeUsingDirectiveOrderCache {
  std::unordered_map<const SgScopeStatement *,
                     ScopeUsingDirectiveOrderCacheEntry>
      entries;
};

ScopeUsingDirectiveOrderCache &scope_using_directive_order_cache() {
  static ScopeUsingDirectiveOrderCache cache;
  return cache;
}

void clear_scope_using_directive_order_cache() {
  scope_using_directive_order_cache().entries.clear();
}

template <typename StatementList>
void record_scope_using_directive_entries(
    const StatementList &statements,
    ScopeUsingDirectiveOrderCacheEntry &entry) {
  entry.using_directives.reserve(statements.size());

  for (size_t i = 0; i < statements.size(); ++i) {
    SgStatement *statement = isSgStatement(statements[i]);
    if (statement == NULL) {
      continue;
    }

    SgUsingDirectiveStatement *using_directive =
        isSgUsingDirectiveStatement(statement);
    if (using_directive == NULL) {
      continue;
    }

    ScopeUsingDirectiveOrderEntry using_entry;
    using_entry.directive = using_directive;
    using_entry.lexical_index = i;
    if (Sg_File_Info *file_info = statement->get_file_info()) {
      using_entry.source_sequence = file_info->get_source_sequence_number();
    }
    entry.using_directives.push_back(using_entry);
  }
}

template <typename StatementList>
size_t find_statement_index_in_list(const StatementList &statements,
                                    const SgStatement *target) {
  for (size_t i = 0; i < statements.size(); ++i) {
    if (statements[i] == target) {
      return i;
    }
  }

  return static_cast<size_t>(-1);
}

size_t
lookup_direct_child_statement_index(SgScopeStatement *scope,
                                    SgStatement *statement,
                                    ScopeUsingDirectiveOrderCacheEntry &entry) {
  if (scope == NULL || statement == NULL || statement->get_parent() != scope) {
    return static_cast<size_t>(-1);
  }

  auto cached = entry.direct_child_index_cache.find(statement);
  if (cached != entry.direct_child_index_cache.end()) {
    return cached->second;
  }

  size_t index = static_cast<size_t>(-1);
  if (SgGlobal *global_scope = isSgGlobal(scope)) {
    index = find_statement_index_in_list(global_scope->get_declarations(),
                                         statement);
  } else if (SgNamespaceDefinitionStatement *namespace_scope =
                 isSgNamespaceDefinitionStatement(scope)) {
    index = find_statement_index_in_list(namespace_scope->get_declarations(),
                                         statement);
  } else if (SgDeclarationScope *declaration_scope =
                 isSgDeclarationScope(scope)) {
    index = find_statement_index_in_list(declaration_scope->get_declarations(),
                                         statement);
  } else if (SgClassDefinition *class_scope = isSgClassDefinition(scope)) {
    index = find_statement_index_in_list(class_scope->get_members(), statement);
  } else if (SgTemplateClassDefinition *template_class_scope =
                 isSgTemplateClassDefinition(scope)) {
    index = find_statement_index_in_list(template_class_scope->get_members(),
                                         statement);
  } else if (SgTemplateInstantiationDefn *instantiation_scope =
                 isSgTemplateInstantiationDefn(scope)) {
    index = find_statement_index_in_list(instantiation_scope->get_members(),
                                         statement);
  } else if (scope->containsOnlyDeclarations()) {
    index =
        find_statement_index_in_list(scope->getDeclarationList(), statement);
  } else if (scope->variantT() == V_SgBasicBlock) {
    index = find_statement_index_in_list(scope->getStatementList(), statement);
  }

  if (index != static_cast<size_t>(-1)) {
    entry.direct_child_index_cache.emplace(statement, index);
  }

  return index;
}

ScopeUsingDirectiveOrderCacheEntry &
get_scope_using_directive_order(SgScopeStatement *scope) {
  static ScopeUsingDirectiveOrderCacheEntry empty_entry;
  if (scope == NULL) {
    return empty_entry;
  }

  ScopeUsingDirectiveOrderCache &cache = scope_using_directive_order_cache();
  auto found = cache.entries.find(scope);
  if (found != cache.entries.end()) {
    return found->second;
  }

  ScopeUsingDirectiveOrderCacheEntry entry;
  if (SgGlobal *global_scope = isSgGlobal(scope)) {
    record_scope_using_directive_entries(global_scope->get_declarations(),
                                         entry);
  } else if (SgNamespaceDefinitionStatement *namespace_scope =
                 isSgNamespaceDefinitionStatement(scope)) {
    record_scope_using_directive_entries(namespace_scope->get_declarations(),
                                         entry);
  } else if (SgDeclarationScope *declaration_scope =
                 isSgDeclarationScope(scope)) {
    record_scope_using_directive_entries(declaration_scope->get_declarations(),
                                         entry);
  } else if (SgClassDefinition *class_scope = isSgClassDefinition(scope)) {
    record_scope_using_directive_entries(class_scope->get_members(), entry);
  } else if (SgTemplateClassDefinition *template_class_scope =
                 isSgTemplateClassDefinition(scope)) {
    record_scope_using_directive_entries(template_class_scope->get_members(),
                                         entry);
  } else if (SgTemplateInstantiationDefn *instantiation_scope =
                 isSgTemplateInstantiationDefn(scope)) {
    record_scope_using_directive_entries(instantiation_scope->get_members(),
                                         entry);
  } else if (scope->containsOnlyDeclarations()) {
    record_scope_using_directive_entries(scope->getDeclarationList(), entry);
  } else if (scope->variantT() == V_SgBasicBlock) {
    record_scope_using_directive_entries(scope->getStatementList(), entry);
  }

  return cache.entries.emplace(scope, std::move(entry)).first->second;
}
} // unnamed namespace

// ***********************************************************
// Main calling function to support name qualification support
// ***********************************************************

// void generateNameQualificationSupport( SgNode* node, std::set<SgNode*>&
// referencedNameSet )
void generateNameQualificationSupport(SgNode *node,
                                      SgUnorderedNodeSet &referencedNameSet) {
  // This function is the top level API for Name Qualification support.
  // This is the only function that need be seen by ROSE.  This function
  // is called in the function:
  //      Unparser::unparseFile(SgSourceFile* file, SgUnparse_Info& info )
  // in the unparser.C file.  Thus the name qualification is computed
  // as we start to process a file and the computed values saved into the
  // SgNode static data member maps. Two maps are used:
  //    one to support qualification of IR nodes that are named, and
  //    one to support name qualification of types.
  // These are passed by reference and references are stored to them in
  // the NameQualificationTraversal class.

  TimingPerformance timer("Name qualification support:");
  clear_scope_using_directive_order_cache();

  // DQ (5/28/2011): Initialize the local maps to the static maps in SgNode.
  // This is requires so the types used in template arguments can call the
  // unparser to support there generation of name qualified nested types.

  // DQ (9/7/2014): Modified to handle template header map (for template
  // declarations). NameQualificationTraversal
  // t(SgNode::get_globalQualifiedNameMapForNames(),SgNode::get_globalQualifiedNameMapForTypes(),SgNode::get_globalTypeNameMap(),referencedNameSet);
  // NameQualificationTraversal
  // t(SgNode::get_globalQualifiedNameMapForNames(),SgNode::get_globalQualifiedNameMapForTypes(),
  //                              SgNode::get_globalQualifiedNameMapForTemplateHeaders(),SgNode::get_globalTypeNameMap(),referencedNameSet);
  NameQualificationTraversal t(
      SgNode::get_globalQualifiedNameMapForNames(),
      SgNode::get_globalQualifiedNameMapForTypes(),
      SgNode::get_globalQualifiedNameMapForTemplateHeaders(),
      SgNode::get_globalTypeNameMap(),
      SgNode::get_globalQualifiedNameMapForMapsOfTypes(), referencedNameSet);

  NameQualificationInheritedAttribute ih;

  {
    // DQ (8/14/2025): Adding performance timer for call to
    // buildDeclarationSets().
    TimingPerformance timer("Name qualification support: buildDeclarationSets");

    // DQ (4/3/2014): Added assertion.
    t.declarationSet = SageInterface::buildDeclarationSets(node);
    ASSERT_not_null(t.declarationSet);
  }

  // DQ (8/14/2025): Adding a performance optimization to only do name
  // qualification on the part of the AST that will unparsed. This should be a
  // significant subset of the number of lines of code (O(1000) or so, since the
  // source files is typically such a small part of the whole translation unit.
  // Note that the default is still to process the whole translation unit, but
  // this optimization will support the better handling of large files ($1M line
  // translation units). Call the traversal. t.traverse(node,ih); Pei-Hung
  // (8/19/2025): revert to use the traversal over parents
  // SageInterface::getProject() would find multiple SgProject in copyAST_tests
  // and break the assertion

  // Get the project from a traversal over the parents back to the root of the
  // AST.
  SgProject *project = SageInterface::getProject(node);
  // Or we can use this function which does not require a traversal.
  //   SgProject* project = SageInterface::getProject();
  ROSE_ASSERT(project != NULL);

  if (project->get_suppressNameQualificationAcrossWholeTranslationUnit() ==
      true) {
    t.set_suppressNameQualificationAcrossWholeTranslationUnit(true);

    // DQ (8/14/2025): Adding performance timer for call to
    // traverseInputFiles().
    TimingPerformance timer("Name qualification support: traverseInputFiles:");

    // t.traverseInputFiles(node,ih);
    t.traverseInputFiles(project, ih);
  } else {
    // DQ (8/14/2025): Adding performance timer for call to traverse().
    TimingPerformance timer("Name qualification support: traverse:");

    t.traverse(node, ih);
  }

  delete t.declarationSet;
  t.declarationSet = nullptr;
}

void NameQualificationTraversal::generateNestedTraversalWithExplicitScope(
    SgNode *node, SgScopeStatement *input_currentScope,
    SgStatement *input_currentStatement, SgNode *input_referenceNode) {
  ASSERT_not_null(input_currentScope);

  // DQ (9/7/2014): Modified to handle template header map (for template
  // declarations). NameQualificationTraversal
  // t(this->qualifiedNameMapForNames,this->qualifiedNameMapForTypes,this->typeNameMap,this->referencedNameSet);
  // NameQualificationTraversal
  // t(this->qualifiedNameMapForNames,this->qualifiedNameMapForTypes,this->qualifiedNameMapForTemplateHeaders,this->typeNameMap,this->referencedNameSet);
  NameQualificationTraversal t(
      this->qualifiedNameMapForNames, this->qualifiedNameMapForTypes,
      this->qualifiedNameMapForTemplateHeaders, this->typeNameMap,
      this->qualifiedNameMapForMapsOfTypes, this->referencedNameSet);

  t.explictlySpecifiedCurrentScope = input_currentScope;

  // DQ (4/19/2019): This might not be required (passin it via the inherited
  // attribute might be all thisis required. DQ (4/19/2019): Added support to
  // include current statement (required for nested traversals of types to
  // support name qualification for SgPointerMemberType).
  t.explictlySpecifiedCurrentStatement = input_currentStatement;

  // DQ (4/7/2014): Set this explicitly using the one already built.
  ASSERT_not_null(declarationSet);
  t.declarationSet = declarationSet;
  ASSERT_not_null(t.declarationSet);

  NameQualificationInheritedAttribute ih;

  // DQ (4/3/2014): Added assertion.
  ASSERT_not_null(declarationSet);

  // This fails for test2001_02.C.
  // ROSE_ASSERT(declarationSet == NULL);

  // DQ (5/24/2013): Added scope to inherited attribute.
  ih.set_currentScope(input_currentScope);

  // DQ (4/19/2019): Added support to include current statement (required for
  // nested traversals of types to support name qualification for
  // SgPointerMemberType).
  ih.set_currentStatement(input_currentStatement);
  ih.set_referenceNode(input_referenceNode);

  // Call the traversal.
  t.traverse(node, ih);
}

// *******************
// Inherited Attribute
// *******************

NameQualificationInheritedAttribute::NameQualificationInheritedAttribute() {
  // Default constructor

  // DQ (5/24/2013): Allow the current scope to be tracked from the traversal of
  // the AST instead of being computed at each IR node which is a problem for
  // template arguments. See test2013_187.C for an example of this.
  currentScope = NULL;
  currentStatement = NULL;
  referenceNode = NULL;
}

NameQualificationInheritedAttribute::NameQualificationInheritedAttribute(
    const NameQualificationInheritedAttribute &X) {
  // Copy constructor.

  // DQ (5/24/2013): Allow the current scope to be tracked from the traversal of
  // the AST instead of being computed at each IR node which is a problem for
  // template arguments. See test2013_187.C for an example of this.
  currentScope = X.currentScope;
  currentStatement = X.currentStatement;
  referenceNode = X.referenceNode;
}

SgScopeStatement *NameQualificationInheritedAttribute::get_currentScope() {
  return currentScope;
}

void NameQualificationInheritedAttribute::set_currentScope(
    SgScopeStatement *scope) {
  currentScope = scope;
}

SgStatement *NameQualificationInheritedAttribute::get_currentStatement() {
  return currentStatement;
}

void NameQualificationInheritedAttribute::set_currentStatement(
    SgStatement *statement) {
  currentStatement = statement;
}

SgNode *NameQualificationInheritedAttribute::get_referenceNode() {
  return referenceNode;
}

void NameQualificationInheritedAttribute::set_referenceNode(SgNode *node) {
  referenceNode = node;
}

// *********************
// Synthesized Attribute
// *********************

NameQualificationSynthesizedAttribute::NameQualificationSynthesizedAttribute() {
  // Default constructor
  node = NULL;
}

NameQualificationSynthesizedAttribute::NameQualificationSynthesizedAttribute(
    SgNode *astNode) {
  // DQ (8/2/2020): Added support for debugging.
  node = astNode;
}

NameQualificationSynthesizedAttribute::NameQualificationSynthesizedAttribute(
    const NameQualificationSynthesizedAttribute &X) {
  // Copy constructor.

  // DQ (8/2/2020): Added support for debugging.
  node = X.node;
}

// *******************
// NameQualificationTraversal
// *******************

// NameQualificationTraversal::NameQualificationTraversal(
//      std::map<SgNode*,std::string> & input_qualifiedNameMapForNames,
//      std::map<SgNode*,std::string> & input_qualifiedNameMapForTypes,
//      std::map<SgNode*,std::string> &
//      input_qualifiedNameMapForTemplateHeaders, std::map<SgNode*,std::string>
//      & input_typeNameMap, std::map<SgNode*,std::map<SgNode*,std::string> > &
//      input_qualifiedNameMapForMapsOfTypes, std::set<SgNode*> &
//      input_referencedNameSet)
NameQualificationTraversal::NameQualificationTraversal(
    NameQualificationMapType &input_qualifiedNameMapForNames,
    NameQualificationMapType &input_qualifiedNameMapForTypes,
    NameQualificationMapType &input_qualifiedNameMapForTemplateHeaders,
    NameQualificationMapType &input_typeNameMap,
    NameQualificationMapOfMapsType &input_qualifiedNameMapForMapsOfTypes,
    NameQualificationSetType &input_referencedNameSet)
    : referencedNameSet(input_referencedNameSet),
      qualifiedNameMapForNames(input_qualifiedNameMapForNames),
      qualifiedNameMapForTypes(input_qualifiedNameMapForTypes),
      qualifiedNameMapForTemplateHeaders(
          input_qualifiedNameMapForTemplateHeaders),
      typeNameMap(input_typeNameMap),
      qualifiedNameMapForMapsOfTypes(input_qualifiedNameMapForMapsOfTypes) {
  // Nothing to do here.

  explictlySpecifiedCurrentScope = NULL;
  explictlySpecifiedCurrentStatement = NULL;

  // DQ (8/3/2019): Reset the static data member that holds the
  // aliasSymbolCausalNodeSet.
  SgSymbolTable::clear_aliasSymbolCausalNodeSet();
  ROSE_ASSERT(SgSymbolTable::get_aliasSymbolCausalNodeSet().empty() == true);

  // DQ (7/19/2025): This is how we are turning on and off a special name
  // qualification mode required in the symbol table support.  In general, is is
  // too expensive to be use everywhere, and has a dramatic imact on the support
  // for SgAliasSymbols within the AST_PostProcessing() (specifically the
  // support for using directives such as "using namespace std;", which can take
  // 80% of the compilation time).
  // ROSE_ASSERT(SgSymbolTable::get_name_qualification_mode() == false);

  SgSymbolTable::set_name_qualification_mode(true);

  ROSE_ASSERT(SgSymbolTable::get_name_qualification_mode() == true);

  declarationSet = NULL;

  // DQ (5/22/2024): Building a mechanism to turn off name qualification after a
  // specific template instantiation function has been processed.  This is debug
  // code to trace down a problem with name qualification growing too large and
  // consuming all memory.
  disableNameQualification = false;

  // DQ (8/14/2025): Adding optimization (default is false) to support name
  // qualification retricted to just the input source file (instead of the whole
  // translation unit).
  suppressNameQualificationAcrossWholeTranslationUnit = false;
}

// DQ (5/28/2011): Added support to set the static global qualified name map in
// SgNode. const std::map<SgNode*,std::string> &
const NameQualificationTraversal::NameQualificationMapType &
NameQualificationTraversal::get_qualifiedNameMapForNames() const {
  return qualifiedNameMapForNames;
}

// DQ (5/28/2011): Added support to set the static global qualified name map in
// SgNode. const std::map<SgNode*,std::string> &
const NameQualificationTraversal::NameQualificationMapType &
NameQualificationTraversal::get_qualifiedNameMapForTypes() const {
  return qualifiedNameMapForTypes;
}

// DQ (3/13/2019): Added support to set the static global qualified name map in
// SgNode. const std::map<SgNode*,std::map<SgNode*,std::string> > & const
// std::map<SgNode*,NameQualificationTraversal::NameQualificationMapType> &
const NameQualificationTraversal::NameQualificationMapOfMapsType &
NameQualificationTraversal::get_qualifiedNameMapForMapsOfTypes() const {
  return qualifiedNameMapForMapsOfTypes;
}

// DQ (9/7/2014): Added support to set the template headers in template
// declarations. const std::map<SgNode*,std::string> &
const NameQualificationTraversal::NameQualificationMapType &
NameQualificationTraversal::get_qualifiedNameMapForTemplateHeaders() const {
  return qualifiedNameMapForTemplateHeaders;
}

int numberOfSymbolsWithName(const SgName &name, SgScopeStatement *scope) {
  // DQ (6/20/2011): This function counts the number of symbols associated with
  // the same name. This function should be put into the SgScopeStatement for
  // more general use.

  // We might have to have separate functions specific to functions, variables,
  // etc. This function addresses a requirement associated with a bug
  // demonstrated by test2011_84.C.

  ASSERT_not_null(scope);
  SgSymbol *symbol = scope->lookup_function_symbol(name);

  int count = 0;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In numberOfSymbolsWithName(): symbol = %p scope = %p = %s \n",
              symbol, scope, scope->class_name().c_str());
#endif

  while (symbol != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "     In loop: symbol = %p = %s \n", symbol,
                symbol->class_name().c_str());
#endif
    count++;
    symbol = scope->next_any_symbol();
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In numberOfSymbolsWithName(): count = %d \n",
              count);
#endif

  return count;
}

SgDeclarationStatement *
NameQualificationTraversal::associatedDeclaration(SgScopeStatement *scope) {
  SgDeclarationStatement *return_declaration = NULL;
  switch (scope->variantT()) {
  case V_SgClassDefinition: {
    SgClassDefinition *definition = isSgClassDefinition(scope);
    ASSERT_not_null(definition);

    SgClassDeclaration *declaration = definition->get_declaration();
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // DQ (11/20/2011): Added support for template declarations (template class
    // declarations)
  case V_SgTemplateClassDefinition: {
    SgTemplateClassDefinition *definition = isSgTemplateClassDefinition(scope);
    ASSERT_not_null(definition);

    SgTemplateClassDeclaration *declaration = definition->get_declaration();
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgNamespaceDefinitionStatement: {
    SgNamespaceDefinitionStatement *definition =
        isSgNamespaceDefinitionStatement(scope);
    ASSERT_not_null(definition);

    // Let the first definition be used to get the associated first declaration
    // so that we are always refering to a consistant declaration for any chain
    // of namespaces.  If not the first then perhaps the last?
    while (definition->get_previousNamespaceDefinition() != NULL) {
      // MLOG_WARN_C(MLOG_UNPARSER, "Iterating through the namespace chain...
      // \n");
      definition = definition->get_previousNamespaceDefinition();
    }

    SgNamespaceDeclarationStatement *declaration =
        definition->get_namespaceDeclaration();
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgTemplateInstantiationDefn: {
    SgTemplateInstantiationDefn *definition =
        isSgTemplateInstantiationDefn(scope);
    ASSERT_not_null(definition);

    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(definition->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // DQ (7/11/2014): Added this case to support test2014_84.C.
  case V_SgFunctionDefinition: {
    SgFunctionDefinition *definition = isSgFunctionDefinition(scope);
    ASSERT_not_null(definition);

    SgFunctionDeclaration *declaration =
        isSgFunctionDeclaration(definition->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // Added support for template function definitions.
  case V_SgTemplateFunctionDefinition: {
    SgTemplateFunctionDefinition *definition =
        isSgTemplateFunctionDefinition(scope);
    ASSERT_not_null(definition);

    SgTemplateFunctionDeclaration *declaration =
        isSgTemplateFunctionDeclaration(definition->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

    // Declaration scopes are often synthetic and do not map directly to a
    // source declaration. Nonreal declaration scopes are rooted at their
    // SgNonrealDecl parent and carry the source-level dependent qualifier
    // chain (e.g., A<T>::). Preserve that association so name qualification
    // can recover unresolved using targets.
  case V_SgDeclarationScope: {
    if (SgNonrealDecl *nrdecl = isSgNonrealDecl(scope->get_parent())) {
      return_declaration = nrdecl;
    } else {
      return_declaration = NULL;
    }
    break;
  }

    // DQ (6/26/2019): Added rage-based for loop (see test2019_483.C).
  case V_SgRangeBasedForStatement:

    // Some scopes don't have an associated declaration (return NULL in these
    // cases). Also missing some of the Fortran specific scopes.
  case V_SgGlobal:
  case V_SgIfStmt:
  case V_SgWhileStmt:
  case V_SgDoWhileStmt:
  case V_SgForStatement:
  case V_SgForAllStatement:
  case V_SgBasicBlock:
  case V_SgSwitchStatement:
  case V_SgCatchOptionStmt: {
    return_declaration = NULL;
    break;
  }

    // Catch anything that migh have been missed (and exit so it can be
    // identified and fixed).
  default: {
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Default reached in "
        "NameQualificationTraversal::associatedDeclaration() scope = %s \n",
        scope->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return return_declaration;
}

SgDeclarationStatement *
NameQualificationTraversal::associatedDeclaration(SgType *type) {
  SgDeclarationStatement *return_declaration = NULL;

  // DQ (1/26/2013): Added assertion.
  ASSERT_not_null(type);

  // DQ (4/15/2019): This is a new default that appears to work well for all of
  // our ROSE regression tests. MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::associatedDeclaration(): Calling stripType()
  // with SgType::STRIP_POINTER_MEMBER_TYPE explicitly \n");

  // DQ (Don't skip over SgPointerMemberType.
  // DQ (4/15/2019): Adding SgType::STRIP_POINTER_MEMBER_TYPE to the stripType()
  // call. We want to strip away all by typedef types. SgType* strippedType =
  // type->stripType(SgType::STRIP_MODIFIER_TYPE|SgType::STRIP_REFERENCE_TYPE|SgType::STRIP_RVALUE_REFERENCE_TYPE|SgType::STRIP_POINTER_TYPE|SgType::STRIP_ARRAY_TYPE);
  // SgType* strippedType =
  // type->stripType(SgType::STRIP_MODIFIER_TYPE|SgType::STRIP_REFERENCE_TYPE|SgType::STRIP_RVALUE_REFERENCE_TYPE|SgType::STRIP_POINTER_TYPE|SgType::STRIP_ARRAY_TYPE|SgType::STRIP_POINTER_MEMBER_TYPE);
  SgType *strippedType = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  ASSERT_not_null(strippedType);

  // switch (type->variantT())
  switch (strippedType->variantT()) {
  case V_SgClassType: {
    SgClassType *classType = isSgClassType(strippedType);
    ASSERT_not_null(classType);

    SgClassDeclaration *declaration =
        isSgClassDeclaration(classType->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration =
        preferAssociatedDefinitionForTemplateInstantiation(declaration);
    break;
  }

  case V_SgTypedefType: {
    SgTypedefType *typedefType = isSgTypedefType(strippedType);
    ASSERT_not_null(typedefType);

    SgTypedefDeclaration *declaration =
        isSgTypedefDeclaration(typedefType->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgEnumType: {
    SgEnumType *enumType = isSgEnumType(strippedType);
    ASSERT_not_null(enumType);

    SgEnumDeclaration *declaration =
        isSgEnumDeclaration(enumType->get_declaration());
    ASSERT_not_null(declaration);

    return_declaration = declaration;
    break;
  }

  case V_SgNonrealType: {
    SgNonrealType *nrtype = isSgNonrealType(strippedType);
    ASSERT_not_null(nrtype);
    return_declaration = nrtype->get_declaration();
    ASSERT_not_null(return_declaration);
    break;
  }

    // DQ (4/18/2019): This case is required because we need to process chains
    // of SgPointerMemberType IR nodes (see test2019_373.C).
  case V_SgPointerMemberType: {
    SgPointerMemberType *pointerMemberType =
        isSgPointerMemberType(strippedType);
    return_declaration = pointerMemberType->get_class_declaration_of();
    ASSERT_not_null(return_declaration);
    break;
  }
    // Catch anything that might have been missed (and exit so it can be
    // identified and fixed).
  default: {
    // PL (10/15/2025): Replacing long list of cases with a simple check for
    // isSgNamedType. Recommended change by DQ.
    if (isSgNamedType(strippedType)) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Default reached in "
                  "NameQualificationTraversal::associatedDeclaration() type = "
                  "%s strippedType = %s \n",
                  type->class_name().c_str(),
                  strippedType->class_name().c_str());
      ROSE_ABORT();
    } else {
      // All types that are not an SgNamedType have a nullptr declaration.
      return_declaration = nullptr;
    }
  }
  }

  return return_declaration;
}

void NameQualificationTraversal::evaluateTemplateInstantiationDeclaration(
    SgDeclarationStatement *declaration, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  // DQ (9/23/2012): Added assertions.
  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  SgScopeStatement *qualificationScope = currentScope;
  if (SgScopeStatement *positionScope =
          SageInterface::getScope(positionStatement)) {
    qualificationScope = positionScope;
  }

  TemplateDeclarationEvaluationKey evaluationKey = {
      declaration, qualificationScope, positionStatement};
  if (completedTemplateDeclarationEvaluations.find(evaluationKey) !=
      completedTemplateDeclarationEvaluations.end()) {
    return;
  }
  if (!activeTemplateDeclarationEvaluations.insert(evaluationKey).second) {
    return;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "1111111111111111111111111111111111111111111111111"
                             "1111111111111111111 \n");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In evaluateTemplateInstantiationDeclaration(): declaration = %p "
              "= %s currentScope = %p = %s positionStatement = %p = %s \n",
              declaration, declaration->class_name().c_str(), currentScope,
              currentScope->class_name().c_str(), positionStatement,
              positionStatement->class_name().c_str());
#endif

  // DQ (10/31/2015): This code is designed to eliminate the infinite
  // recursion possible in some rare cases of template instantiation (see
  // test2015_105.C extracted from ROSE compiling ROSE header files and the
  // template-heavy usage present there).  Note that this could be restricted
  // to the handling of SgTemplateInstantiationDecl instead (I think).  But
  // it might be that I have just not yet seen a recursive case using
  // template functions instantiations, template member function
  // instantiations and template variable instantiations.
  SgTemplateInstantiationDecl *templateInstantiationDeclaration =
      isSgTemplateInstantiationDecl(declaration);
  SgClassDefinition *nonconst_def =
      templateInstantiationDeclaration != NULL
          ? isSgClassDefinition(
                templateInstantiationDeclaration->get_definition())
          : NULL;

  // DQ (5/22/2024): Count the number of function invocations so that we can
  // turn on forceSkip selectively.
  static size_t functionCallCounter = 0;
  (void)functionCallCounter;

  functionCallCounter++;

#if DEBUG_TEMPINSTDECL
  printf("In evaluateTemplateInstantiationDeclaration(): functionCallCounter = "
         "%zu \n",
         functionCallCounter);
#endif

  bool forceSkip = false;

  // DQ (5/22/2024): Building a mechanism to turn off name qualification after a
  // specific template instantiation function has been processed.  This is debug
  // code to trace down a problem with name qualification growing too large and
  // consuming all memory.
  if (disableNameQualification == true) {
#if DEBUG_NONTERMINATION || DEBUG_TEMPINSTDECL || 0
    printf("In evaluateTemplateInstantiationDeclaration(): Setting forceSkip = "
           "true \n");
#endif
    forceSkip = true;
  }

  // DQ (5/12/2024): Make sure that we don't have any NULL entries.
  ROSE_ASSERT(MangledNameSupport::visitedTemplateDefinitions.find(NULL) ==
              MangledNameSupport::visitedTemplateDefinitions.end());

  if (forceSkip == true ||
      MangledNameSupport::visitedTemplateDefinitions.find(nonconst_def) !=
          MangledNameSupport::visitedTemplateDefinitions.end()) {
    // Skip the call that would result in infinte recursion.
    activeTemplateDeclarationEvaluations.erase(evaluationKey);
    return;
  } else {
    // Only handle the case of a SgTemplateInstantiationDecl.
    SgClassDefinition *templateInstantiationDefinition =
        isSgTemplateInstantiationDefn(nonconst_def);
    if (templateInstantiationDefinition != NULL) {
      // Not clear why we need to use an iterator to simply insert a pointer
      // into the set. SgTemplateInstantiationDefn*
      // nonconst_templateInstantiationDefinition =
      // const_cast<SgTemplateInstantiationDefn*>(templateInstantiationDefinition);
      MangledNameSupport::setType::iterator it =
          MangledNameSupport::visitedTemplateDefinitions.begin();
      // MangledNameSupport::visitedTemplateDeclarations.insert(it,nonconst_templateInstantiationDefinition);
      MangledNameSupport::visitedTemplateDefinitions.insert(it, nonconst_def);
    }

    // DQ (11/1/2015): Indented this code (switch statement) to conform to new
    // block layout. DQ (6/1/2011): Added support for template arguments.
    switch (declaration->variantT()) {
    case V_SgTemplateInstantiationDecl: {
      SgTemplateInstantiationDecl *templateInstantiationDeclaration =
          isSgTemplateInstantiationDecl(declaration);
      ASSERT_not_null(templateInstantiationDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      // MLOG_WARN_C(MLOG_UNPARSER, "$$$$$$$$$ ---
      // templateInstantiationDeclaration = %p
      // \n",templateInstantiationDeclaration);
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "$$$$$$$$$ --- templateInstantiationDeclaration = %p "
          "templateInstantiationDeclaration->get_templateArguments().size() = "
          "%" PRIuPTR " \n",
          templateInstantiationDeclaration,
          templateInstantiationDeclaration->get_templateArguments().size());
#endif

      // Evaluate all template arguments.
      evaluateNameQualificationForTemplateArgumentList(
          templateInstantiationDeclaration->get_templateArguments(),
          qualificationScope, positionStatement);
      break;
    }

    case V_SgTemplateInstantiationFunctionDecl: {
      SgTemplateInstantiationFunctionDecl
          *templateInstantiationFunctionDeclaration =
              isSgTemplateInstantiationFunctionDecl(declaration);
      ASSERT_not_null(templateInstantiationFunctionDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "$$$$$$$$$ --- templateInstantiationFunctionDeclaration = %p \n",
          templateInstantiationFunctionDeclaration);
#endif
      // Evaluate all template arguments.
      evaluateNameQualificationForTemplateArgumentList(
          templateInstantiationFunctionDeclaration->get_templateArguments(),
          qualificationScope, positionStatement);
      break;
    }

    case V_SgTemplateInstantiationMemberFunctionDecl: {
      SgTemplateInstantiationMemberFunctionDecl
          *templateInstantiationMemberFunctionDeclaration =
              isSgTemplateInstantiationMemberFunctionDecl(declaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "$$$$$$$$$ --- "
                  "templateInstantiationMemberFunctionDeclaration = %p \n",
                  templateInstantiationMemberFunctionDeclaration);
#endif
      ASSERT_not_null(templateInstantiationMemberFunctionDeclaration);

      // Evaluate all template arguments.
      // evaluateNameQualificationForTemplateArgumentList
      // (templateInstantiationMemberFunctionDeclaration->get_templateArguments(),currentScope,positionStatement);
      SgTemplateArgumentPtrList &templateArgumentList =
          templateInstantiationMemberFunctionDeclaration
              ->get_templateArguments();
      evaluateNameQualificationForTemplateArgumentList(
          templateArgumentList, qualificationScope, positionStatement);
      break;
    }

      // DQ (3/31/2018): Added code to help debug strange case (see
      // Cxx11_tests/test2018_68.C).
    case V_SgTemplateFunctionDeclaration: {
      // Actually there is nothing to do here.
      break;
    }

      // DQ (4/14/2018): Added case for template typedef instantiations (see
      // test2018_83.C for an example where name qualification of the template
      // arguments is required).
    case V_SgTemplateInstantiationTypedefDeclaration: {
      SgTemplateInstantiationTypedefDeclaration
          *templateInstantiationTypedefDeclaration =
              isSgTemplateInstantiationTypedefDeclaration(declaration);
      ASSERT_not_null(templateInstantiationTypedefDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "$$$$$$$$$ --- templateInstantiationTypedefDeclaration = %p \n",
          templateInstantiationTypedefDeclaration);
#endif
      // Evaluate all template arguments.
      evaluateNameQualificationForTemplateArgumentList(
          templateInstantiationTypedefDeclaration->get_templateArguments(),
          qualificationScope, positionStatement);
      break;
    }

    case V_SgNonrealDecl: {
      SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration);
      ASSERT_not_null(nrdecl);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "$$$$$$$$$ --- nrdecl = %p \n", nrdecl);
#endif
      evaluateNameQualificationForTemplateArgumentList(
          nrdecl->get_tpl_args(), qualificationScope, positionStatement);

      // If this nonreal decl is part of a nested nonreal chain, make sure
      // template arguments on the parent links are also qualified relative
      // to the current use site.
      SgNode *parent = nrdecl->get_parent();
      while (parent != nullptr) {
        SgDeclarationScope *nrscope = isSgDeclarationScope(parent);
        if (nrscope == nullptr) {
          break;
        }
        SgNonrealDecl *parent_nrdecl = isSgNonrealDecl(nrscope->get_parent());
        if (parent_nrdecl == nullptr) {
          break;
        }
        evaluateNameQualificationForTemplateArgumentList(
            parent_nrdecl->get_tpl_args(), qualificationScope,
            positionStatement);
        parent = parent_nrdecl->get_parent();
      }

      break;
    }

    default: {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "This IR node does not contain template arguments to "
                  "process: declaration = %p = %s \n",
                  declaration, declaration->class_name().c_str());
#endif
    }
    }

    // DQ (10/31/2015): The rule here is that after processing as a mangled name
    // we remove the template instantiation from the list so that other
    // non-nested uses of the template instantiation will force the manged name
    // to be generated.
    if (templateInstantiationDefinition != NULL) {
      MangledNameSupport::visitedTemplateDefinitions.erase(nonconst_def);
    }
  }

  activeTemplateDeclarationEvaluations.erase(evaluationKey);
  completedTemplateDeclarationEvaluations.insert(evaluationKey);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "Leaving evaluateTemplateInstantiationDeclaration(): declaration "
              "= %p = %s currentScope = %p = %s positionStatement = %p = %s \n",
              declaration, declaration->class_name().c_str(), currentScope,
              currentScope->class_name().c_str(), positionStatement,
              positionStatement->class_name().c_str());
#endif
}

int NameQualificationTraversal::nameQualificationDepthOfParent(
    SgDeclarationStatement *declaration, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  // Now resolve how much name qualification is required.
  int qualificationDepth = 0;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "***** Inside of "
      "NameQualificationTraversal::nameQualificationDepthOfParent() ***** \n");
  MLOG_WARN_C(MLOG_UNPARSER, "   declaration  = %p = %s = %s \n", declaration,
              declaration->class_name().c_str(),
              SageInterface::get_name(declaration).c_str());
  MLOG_WARN_C(MLOG_UNPARSER, "   currentScope = %p = %s = %s \n", currentScope,
              currentScope->class_name().c_str(),
              SageInterface::get_name(currentScope).c_str());
#endif

  // qualificationDepth++;
  // SgDeclaration* classOrNamespaceDefinition =
  // classDefinition->get_declaration()->get_scope();
  SgScopeStatement *parentScope = declaration->get_scope();
  // SgName parentName = associatedName(parentScope);

  // DQ (6/24/2018): Added assertion.
  ASSERT_not_null(parentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "***** Inside of "
      "NameQualificationTraversal::nameQualificationDepthOfParent() ***** \n");
  MLOG_WARN_C(MLOG_UNPARSER, "   parentScope = %p = %s = %s \n", parentScope,
              parentScope->class_name().c_str(),
              SageInterface::get_name(parentScope).c_str());
#endif

  // qualificationDepth =
  // nameQualificationDepth(parentName,parentScope,positionStatement) + 1;
  SgGlobal *globalScope = isSgGlobal(parentScope);
  if (globalScope != NULL) {
    // There is no declaration associated with global scope so we have to
    // process the case of a null pointer...
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 1)
    MLOG_WARN_C(MLOG_UNPARSER,
                "parentDeclaration == NULL: parentScope = %p = %s \n",
                parentScope, parentScope->class_name().c_str());
#endif
  } else {
    // Now ask the same question recursively using the parent declaration and
    // the same currentScope (is it visible from the same point in the code).
    SgDeclarationStatement *parentDeclaration =
        associatedDeclaration(parentScope);

    // In some cases the declaration can be in a scope with is not associated
    // with a declaration (e.g. SgBasicBlock or SgForStatement).
    if (parentDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "In NameQualificationTraversal::nameQualificationDepthOfParent(): "
          "calling nameQualificationDepth(): parentDeclaration = %p = %s = %s "
          "\n",
          parentDeclaration, parentDeclaration->class_name().c_str(),
          SageInterface::get_name(parentDeclaration).c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- parentScope = %p = %s \n", parentScope,
                  parentScope->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                  positionStatement, positionStatement->class_name().c_str());
#endif
      // qualificationDepth =
      // nameQualificationDepth(parentDeclaration,parentScope,positionStatement);
      qualificationDepth = nameQualificationDepth(
          parentDeclaration, currentScope, positionStatement);
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // #if 0
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "Leaving nameQualificationDepthOfParent(): declaration = %p = %s \n",
      declaration, declaration->class_name().c_str());
#endif

  return qualificationDepth;
}

bool NameQualificationTraversal::requiresTypeElaboration(SgSymbol *symbol) {
  // DQ (5/14/2011): type elaboration only works between non-types and types.
  // Different types must be distinquished using name qualification. If this is
  // a type then since all types are given equal weight we need more name
  // qualification to distinquish them. However, if this is a non-type then type
  // elaboration is sufficent to distinquish the type (e.g. from a variable
  // name).
  bool typeElaborationRequired = false;

  ASSERT_not_null(symbol);
  switch (symbol->variantT()) {
    // DQ (7/23/2011): Class elaboration can be required....
  case V_SgClassSymbol:

    // DQ (2/25/2012): Added support for SgTemplateClassSymbol.
  case V_SgTemplateClassSymbol:

    // DQ (2/12/2012): Added support for SgTemplateMemberFunctionSymbol.
  case V_SgTemplateMemberFunctionSymbol:

    // DQ (2/12/2013): Added support for SgTemplateFunctionSymbol.
  case V_SgTemplateFunctionSymbol:

    // TV (05/24/2018): Added support for SgTemplateTypedefSymbol
  case V_SgTemplateTypedefSymbol:

    // DQ (6/21/2011): Added case for SgFunctionSymbol (triggers type
    // elaboration).
  case V_SgFunctionSymbol:
  case V_SgMemberFunctionSymbol:
  case V_SgTemplateVariableSymbol:
  case V_SgVariableSymbol: {
    typeElaborationRequired = true;
    break;
  }

    // DQ (3/31/2013): We need an example of this before I allow it (I don't
    // think type elaboration is required here).
  case V_SgEnumFieldSymbol:

    // DQ (6/22/2011): Added case for SgEnumSymbol (see test2011_95.C)
  case V_SgEnumSymbol:
  case V_SgNamespaceSymbol: // Note sure about this!!!
  case V_SgTemplateSymbol:  // Note sure about this!!!
  case V_SgNonrealSymbol:   // Note sure about this!!!
  case V_SgTypedefSymbol: {
    typeElaborationRequired = false;
    break;
  }

    // DQ (9/21/2011): Added support for alias symbol (recursive call).
  case V_SgAliasSymbol: {
    SgAliasSymbol *alias = isSgAliasSymbol(symbol);
    ASSERT_not_null(alias);

    // DQ (7/12/2014): The newer design of the symbol table handling means that
    // we will never see a SgAliasSymbol at this level.
    ROSE_ABORT();
  }

  default: {
    MLOG_WARN_C(MLOG_UNPARSER,
                "Default reached in "
                "NameQualificationTraversal::requiresTypeElaboration(): symbol "
                "= %p = %s \n",
                symbol, symbol->class_name().c_str());
    ROSE_ABORT();
  }
  }

  return typeElaborationRequired;
}

void NameQualificationTraversal::processNameQualificationArrayType(
    SgArrayType *arrayType, SgScopeStatement *currentScope) {
  // Note that we may have to traverse base types in case they include other
  // SgArrayType IR nodes where their index requires name qualification. SgType*
  // strippedArrayType =
  // arrayType->stripType(SgType::STRIP_MODIFIER_TYPE|SgType::STRIP_REFERENCE_TYPE|SgType::STRIP_POINTER_TYPE);
  // ASSERT_not_null(strippedArrayType);

  SgExpression *index = arrayType->get_index();
  if (index != NULL) {
    // DQ (7/23/2011): This will not work since the current scope is not know
    // and can't be determined from the type (which is shared).
    ASSERT_not_null(currentScope);
    generateNestedTraversalWithExplicitScope(index, currentScope);
    // DQ (8/21/2014): It appears that test2014_137.C is not being caught by
    // this trap.
    SgVarRefExp *varRefExp = isSgVarRefExp(index);
    if (varRefExp != NULL) {
    }
  }
}

void NameQualificationTraversal::processNameQualificationForPossibleArrayType(
    SgType *possibleArrayType, SgScopeStatement *currentScope) {
  // DQ (7/23/2011): Refactored support for name qualification of the index
  // expressions used in array types.

  SgType *strippedPossibleArrayType = possibleArrayType->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE);
  ASSERT_not_null(strippedPossibleArrayType);
  SgArrayType *arrayType = isSgArrayType(strippedPossibleArrayType);
  if (arrayType != NULL) {
    processNameQualificationArrayType(arrayType, currentScope);

    // Now process the base type, since it might be part of a multi-dimentional
    // array type (in C/C++ these are a chain of array types).
    processNameQualificationForPossibleArrayType(arrayType->get_base_type(),
                                                 currentScope);
  }
}

void NameQualificationTraversal::functionReport(
    SgFunctionDeclaration *functionDeclaration) {
  // Report on the details of functions as part of debugging.  This function
  // supports an analysis of the use of the getline function in iostream header
  // file.  It is used as a template, it is overloaded, and it is
  // instantiationed in several ways.  As a result it is a subject for a case
  // study in the name qualification to eveluate the influence of different
  // instances (kinds) of the same function function and how the logic for name
  // qualification and detection of functions hiding other functions works to
  // drive the name qualification.

  // SgFunctionDeclaration* functionDeclaration =
  // isSgFunctionDeclaration(astNode);
  if (functionDeclaration != NULL) {
    string name = functionDeclaration->get_name();
    std::size_t pos = name.find("getline");
    if (pos != string::npos) {
      MLOG_WARN_C(MLOG_UNPARSER, "found getline function: pos = %zu \n", pos);
      MLOG_WARN_C(MLOG_UNPARSER, "   --- function name = %s \n", name.c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- functionDeclaration = %p = %s \n",
                  functionDeclaration,
                  functionDeclaration->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- function mangled name = %s \n",
                  functionDeclaration->get_mangled_name().str());
      SgTemplateInstantiationFunctionDecl *templateInstantiationFunctionDecl =
          isSgTemplateInstantiationFunctionDecl(functionDeclaration);
      SgTemplateInstantiationMemberFunctionDecl
          *templateInstantiationMemberFunctionDecl =
              isSgTemplateInstantiationMemberFunctionDecl(functionDeclaration);

      if (templateInstantiationFunctionDecl != NULL) {
        SgTemplateFunctionDeclaration *templateFunctionDeclaration =
            isSgTemplateFunctionDeclaration(
                templateInstantiationFunctionDecl->get_templateDeclaration());
        ASSERT_not_null(templateFunctionDeclaration);
        MLOG_WARN_C(MLOG_UNPARSER,
                    "   --- templateFunctionDeclaration = %p = %s \n",
                    templateFunctionDeclaration,
                    templateFunctionDeclaration->class_name().c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- "
            "templateFunctionDeclaration->get_file_info()->get_filename() = %s "
            "\n",
            templateFunctionDeclaration->get_file_info()->get_filename());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- templateFunctionDeclaration->get_file_info()->get_line()   "
            "  = %d \n",
            templateFunctionDeclaration->get_file_info()->get_line());
      } else {
        if (templateInstantiationMemberFunctionDecl != NULL) {
          SgTemplateMemberFunctionDeclaration
              *templateMemberFunctionDeclaration =
                  isSgTemplateMemberFunctionDeclaration(
                      templateInstantiationMemberFunctionDecl
                          ->get_templateDeclaration());
          ASSERT_not_null(templateMemberFunctionDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "   --- templateMemberFunctionDeclaration = %p = %s \n",
                      templateMemberFunctionDeclaration,
                      templateMemberFunctionDeclaration->class_name().c_str());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "   --- "
                      "templateMemberFunctionDeclaration->get_file_info()->get_"
                      "filename() = %s \n",
                      templateMemberFunctionDeclaration->get_file_info()
                          ->get_filename());
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "   --- "
              "templateMemberFunctionDeclaration->get_file_info()->get_line()  "
              "   = %d \n",
              templateMemberFunctionDeclaration->get_file_info()->get_line());
        } else {
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "   --- functionDeclaration->get_file_info()->get_filename() = "
              "%s \n",
              functionDeclaration->get_file_info()->get_filename());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "   --- functionDeclaration->get_file_info()->get_line() "
                      "    = %d \n",
                      functionDeclaration->get_file_info()->get_line());
        }
      }

      MLOG_WARN_C(MLOG_UNPARSER,
                  "   --- functionDeclaration->isCompilerGenerated() = %s \n",
                  functionDeclaration->isCompilerGenerated() ? "true"
                                                             : "false");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "   --- functionDeclaration->isTransformation()    = %s \n",
                  functionDeclaration->isTransformation() ? "true" : "false");
      // MLOG_WARN_C(MLOG_UNPARSER, "   --- function: %s
      // \n",functionDeclaration->unparseToString().c_str());
      if (functionDeclaration->isCompilerGenerated() == true) {
        ROSE_ASSERT(functionDeclaration->isTransformation() == false);

        functionDeclaration->setTransformation();
        functionDeclaration->setOutputInCodeGeneration();

        MLOG_WARN_C(MLOG_UNPARSER,
                    "   --- (before unparse) "
                    "functionDeclaration->isTransformation()    = %s \n",
                    functionDeclaration->isTransformation() ? "true" : "false");

        MLOG_WARN_C(MLOG_UNPARSER, "   --- function: %s \n",
                    functionDeclaration->unparseToString().c_str());

        functionDeclaration->unsetOutputInCodeGeneration();
        functionDeclaration->unsetTransformation();
      } else {
        // MLOG_WARN_C(MLOG_UNPARSER, "   --- (non-compiler-generated) function:
        // %s \n",functionDeclaration->unparseToString().c_str());
        MLOG_WARN_C(MLOG_UNPARSER, "   --- function: %s \n",
                    functionDeclaration->unparseToString().c_str());
      }
    }
  }
}

// int NameQualificationTraversal::nameQualificationDepth ( SgScopeStatement*
// classOrNamespaceDefinition )
int NameQualificationTraversal::nameQualificationDepth(
    SgDeclarationStatement *declaration, SgScopeStatement *currentScope,
    SgStatement *positionStatement, bool forceMoreNameQualification) {
  // Note that the input must be a declaration because it can include enums
  // (SgDeclarationStatement IR nodes) that don't have a corresponding
  // definition (SgScopeStatement IR nodes).

  // This function computes the number of qualified names required to uniquely
  // qualify and input reference. It evaluates how much name qualification is
  // required (typically 0 (no qualification), but sometimes the depth of the
  // nesting of scopes plus 1 (full qualification with global scoping
  // operator)).

  // The positionStatement is the position of the associated reference to the
  // declaration. It is used when "using declarations" are not at the top of the
  // scope.  Initially we will assume that the such "using declarations" are at
  // the top of the scope.

  // How this works:
  // The function inputs are:
  //    1) the declaration whose reference we are evaluating for name
  //    qualification. 2) the current scope of the reference to the declaration
  //    (the computed name qualification will be
  //       prepended to the name of the declaration which is a reference to the
  //       declaration passed to this function).
  //    3) The positonStatement is required to allow name qualification
  //    decisions to be based on the reference
  //       declarations position in scope relative to the input declaration.
  //       Some function declarations require name qualification depending on if
  //       they appear before or after a prototype declaration for the function
  //       that would define it's scope (separate from its visability.

  // Note: we are evaluating the name qualification for references to
  // declarations (e.g. the defining member function outside of the class which
  // contains the non-defining (prototype) member function declaration).

  // At this point, the symbol for the input function has been looked up in the
  // parent scope of the declaration we are evaluating for name qualification.
  // The lookup is for any symbol matching the name, not the name plus the kind
  // of declaration (which will come in a next step). If it is not found, then
  // it is not visible, and thus at least one level of name qualification will
  // be required.  If it is found then we need an additional step to decide if
  // the existence of some declaration with that name will force name
  // elaboration (type elaboration) or name qualification.

  // A test is done on the kind of symbol to determine if its associated
  // declaration will force qualification or type elaboration.  If the symbol
  // kind (of the kind of declaration associated with the symbol) matches the
  // input declaration then name qualification will be required, if it is a
  // different kind of declaration the type elaboration might be all that is
  // required.  For example, a variable name may be the same as a class name,
  // but where this happens, only type elaboration is required to distinquish
  // the two.  However, if a declaration name is classing with another
  // declaration name of the same kind, then only name qualification will
  // distinguish the two.

  // If name qualification is required, then we repeat the lookup in the symbol
  // table, using a more refined search to only get symbols of the type that
  // would force name qualification.

  // Note that this function is recursive, the current scope will remain fixed,
  // but the declaration associated with the target decaration will be walked up
  // in the AST toward the global scope, each time computing the associated
  // declaration for each new scope where we evaluated.  The count of the number
  // of scopes (scope depth) required for name qualification (until the scope
  // containing the input declaration is found) is all that this function
  // returns.

  // Note: this function is overloaded to take other kinds of IR nodes that
  // require name qualification, but are not SgDeclarationStatements (e.g.
  // SgInitializedName).

  ASSERT_not_null(declaration);
  ASSERT_not_null(currentScope);

  // DQ (6/22/2011): Assert this as a preliminary step to its removal.
  ROSE_ASSERT(forceMoreNameQualification == false);

  // DQ (4/4/2014): Added assertion.
  ASSERT_not_null(positionStatement);

  if (SgUsingDeclarationStatement *usingDeclaration =
          isSgUsingDeclarationStatement(declaration)) {
    if (SgDeclarationStatement *associatedDeclaration =
            usingDeclaration->get_declaration()) {
      if (associatedDeclaration != declaration) {
        return nameQualificationDepth(associatedDeclaration, currentScope,
                                      positionStatement,
                                      forceMoreNameQualification);
      }
    }

    if (SgInitializedName *associatedInitializedName =
            usingDeclaration->get_initializedName()) {
      return nameQualificationDepth(associatedInitializedName, currentScope,
                                    positionStatement);
    }
  }

  int qualificationDepth = 0;

  bool typeElaborationIsRequired = false;
  // bool globalQualifierIsRequired = false;

#define DEBUG_FUNCTION_RESOLUTION 0

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "\n\n#############################################"
                             "############################# \n");
  MLOG_WARN_C(MLOG_UNPARSER, "#################################################"
                             "######################### \n");
  MLOG_WARN_C(MLOG_UNPARSER, "#################################################"
                             "######################### \n");
  MLOG_WARN_C(MLOG_UNPARSER, "#################################################"
                             "######################### \n");
  MLOG_WARN_C(MLOG_UNPARSER,
              "##### Inside of "
              "NameQualificationTraversal::nameQualificationDepth() ##### \n");
  MLOG_WARN_C(MLOG_UNPARSER, "#################################################"
                             "######################### \n");
  MLOG_WARN_C(MLOG_UNPARSER, "#################################################"
                             "######################### \n");
  MLOG_WARN_C(MLOG_UNPARSER, "#################################################"
                             "######################### \n");

  // The use of SageInterface::generateUniqueName() can cause the unparser to be
  // called and triggers the name qualification recursively but only for
  // template declaration (SgTemplateInstantiationDecl, I think).
  // MLOG_WARN_C(MLOG_UNPARSER, "declaration  = %p = %s = %s = %s
  // \n",declaration,declaration->class_name().c_str(),SageInterface::get_name(declaration).c_str(),SageInterface::generateUniqueName(declaration,true).c_str());
  MLOG_WARN_C(MLOG_UNPARSER, "   --- declaration  = %p = %s = %s \n",
              declaration, declaration->class_name().c_str(),
              SageInterface::get_name(declaration).c_str());
  declaration->get_startOfConstruct()->display("declaration");
  MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope = %p = %s = %s \n",
              currentScope, currentScope->class_name().c_str(),
              SageInterface::get_name(currentScope).c_str());
  currentScope->get_startOfConstruct()->display("currentScope");
  MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
              positionStatement, positionStatement->class_name().c_str());
  positionStatement->get_startOfConstruct()->display("positionStatement");
#endif

  SgNonrealDecl *nonrealDecl = isSgNonrealDecl(declaration);
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(declaration);
  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(declaration);
  SgTypedefDeclaration *typedefDeclaration =
      isSgTypedefDeclaration(declaration);
  SgTemplateDeclaration *templateDeclaration =
      isSgTemplateDeclaration(declaration);
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
  SgNamespaceDeclarationStatement *namespaceDeclaration =
      isSgNamespaceDeclarationStatement(declaration);

  // DQ (4/9/2018): Added support for namespace alias declarations.
  SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
      isSgNamespaceAliasDeclarationStatement(declaration);

  // Make sure that the definitions and declarations are consistant.
  // ROSE_ASSERT(classDefinition != NULL || namespaceDefinition != NULL);
  // ROSE_ASSERT((classDefinition != NULL && classDeclaration != NULL) ||
  // (namespaceDefinition != NULL && namespaceDeclaration != NULL));
  // ROSE_ASSERT(classDeclaration != NULL || namespaceDeclaration != NULL);
  // ROSE_ASSERT(classDeclaration != NULL || namespaceDeclaration != NULL ||
  // variableDeclaration != NULL || functionDeclaration != NULL ||
  // typedefDeclaration != NULL || templateDeclaration != NULL ||
  // enumDeclaration != NULL );
  ROSE_ASSERT(classDeclaration != NULL || namespaceDeclaration != NULL ||
              namespaceAliasDeclaration != NULL ||
              variableDeclaration != NULL || functionDeclaration != NULL ||
              typedefDeclaration != NULL || templateDeclaration != NULL ||
              enumDeclaration != NULL || nonrealDecl != NULL);

  // ROSE_ASSERT((classDeclaration != NULL && classDefinition != NULL) ||
  // (namespaceDeclaration != NULL && namespaceDefinition != NULL) ||
  // variableDeclaration != NULL);

  // SgName name = (classDeclaration != NULL) ? classDeclaration->get_name() :
  // ((namespaceDeclaration != NULL) ? namespaceDeclaration->get_name() :
  // "unknown");
  SgName name =
      (nonrealDecl != NULL)            ? nonrealDecl->get_name()
      : (classDeclaration != NULL)     ? classDeclaration->get_name()
      : (namespaceDeclaration != NULL) ? namespaceDeclaration->get_name()
      : (namespaceAliasDeclaration != NULL)
          ? namespaceAliasDeclaration->get_name()
      : (variableDeclaration != NULL)
          ? SageInterface::getFirstInitializedName(variableDeclaration)
                ->get_name()
      : (functionDeclaration != NULL) ? functionDeclaration->get_name()
      : (typedefDeclaration != NULL)  ? typedefDeclaration->get_name()
      : (templateDeclaration != NULL) ? templateDeclaration->get_name()
      : (enumDeclaration != NULL)     ? enumDeclaration->get_name()
                                      : "unknown_name";

  SgName templateName = get_template_name_for_instantiation(declaration);
  if (templateName.is_null() == false) {
    name = templateName;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In nameQualificationDepth(SgDeclarationStatement*,...): "
              "declaration = %p = %s name = %s \n",
              declaration, declaration->class_name().c_str(), name.str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In nameQualificationDepth(SgDeclarationStatement*,...): "
              "Skipping special handling of un-named constructs (not "
              "required): declaration = %p = %s \n",
              declaration, declaration->class_name().c_str());
#endif

  // DQ (8/16/2013): Build the template parameters and template arguments as
  // appropriate (will be NULL pointers for some types of declarations).
  SgTemplateParameterPtrList *templateParameterList =
      SageBuilder::getTemplateParameterList(declaration);
  SgTemplateArgumentPtrList *templateArgumentList =
      SageBuilder::getTemplateArgumentList(declaration);
  SgTemplateArgumentPtrList *lookupTemplateArgumentList = templateArgumentList;
  if (templateName.is_null() == false) {
    // Use template names for scope lookup without binding to specific
    // arguments.
    lookupTemplateArgumentList = NULL;
  }

  {
    // Note that there can be more than one symbol if the name is hidden in a
    // base class scope (and thus there are SgAliasSymbols using the same name).
    ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Initial lookup: name = %s currentScope = %p = %s \n",
                name.str(), currentScope, currentScope->class_name().c_str());
#endif

    // DQ (8/16/2013): Added support for more precise symbol lookup (which
    // requires the template parameters and template arguments). DQ 8/21/2012):
    // this is looking in the parent scopes of the currentScope and thus not
    // including the currentScope. This is a bug for test2011_31.C where there
    // is a variable who's name hides the name in the parent scopes (and it not
    // detected). SgSymbol* symbol =
    // SageInterface::lookupSymbolInParentScopes(name,currentScope);
    SgSymbol *symbol = SageInterface::lookupSymbolInParentScopes(
        name, currentScope, templateParameterList, lookupTemplateArgumentList);

    auto canonical_first_declaration =
        [](SgDeclarationStatement *decl) -> SgDeclarationStatement * {
      if (decl == NULL) {
        return NULL;
      }
      if (SgDeclarationStatement *first =
              decl->get_firstNondefiningDeclaration()) {
        return first;
      }
      return decl;
    };

    auto declaration_for_symbol =
        [](SgSymbol *candidate) -> SgDeclarationStatement * {
      if (candidate == NULL) {
        return NULL;
      }
      if (SgAliasSymbol *alias = isSgAliasSymbol(candidate)) {
        candidate = alias->get_alias();
      }
      if (SgClassSymbol *class_symbol = isSgClassSymbol(candidate)) {
        return class_symbol->get_declaration();
      }
      if (SgTypedefSymbol *typedef_symbol = isSgTypedefSymbol(candidate)) {
        return typedef_symbol->get_declaration();
      }
      if (SgEnumSymbol *enum_symbol = isSgEnumSymbol(candidate)) {
        return enum_symbol->get_declaration();
      }
      return NULL;
    };

    auto imported_by_prior_using_directive =
        [&](SgDeclarationStatement *target_declaration) -> bool {
      if (target_declaration == NULL ||
          positionStatement->get_file_info() == NULL) {
        return false;
      }

      SgDeclarationStatement *target_first_declaration =
          canonical_first_declaration(target_declaration);
      unsigned int position_sequence =
          positionStatement->get_file_info()->get_source_sequence_number();

      for (SgScopeStatement *scope = currentScope; scope != NULL;) {
        ScopeUsingDirectiveOrderCacheEntry &scope_entry =
            get_scope_using_directive_order(scope);
        size_t position_lexical_index = lookup_direct_child_statement_index(
            scope, positionStatement, scope_entry);

        for (const ScopeUsingDirectiveOrderEntry &using_entry :
             scope_entry.using_directives) {
          if (using_entry.directive == NULL) {
            continue;
          }
          if (position_lexical_index != static_cast<size_t>(-1) &&
              using_entry.lexical_index >= position_lexical_index) {
            break;
          }
          if (position_sequence != 0 && using_entry.source_sequence != 0 &&
              using_entry.source_sequence >= position_sequence) {
            continue;
          }

          SgNamespaceDeclarationStatement *namespace_declaration =
              using_entry.directive->get_namespaceDeclaration();
          if (namespace_declaration == NULL) {
            continue;
          }

          SgNamespaceDefinitionStatement *namespace_definition =
              namespace_declaration->get_definition();
          if (namespace_definition == NULL) {
            continue;
          }

          SgSymbol *imported_symbol = namespace_definition->lookup_symbol(
              name, templateParameterList, lookupTemplateArgumentList);
          SgDeclarationStatement *imported_declaration =
              declaration_for_symbol(imported_symbol);
          if (imported_declaration == NULL) {
            continue;
          }

          if (canonical_first_declaration(imported_declaration) !=
              target_first_declaration) {
            return true;
          }
        }

        SgScopeStatement *next_scope = scope->get_scope();
        if (next_scope == scope) {
          break;
        }
        scope = next_scope;
      }

      return false;
    };

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
    MLOG_WARN_C(MLOG_UNPARSER, "Initial lookup: symbol = %p = %s \n", symbol,
                (symbol != NULL) ? symbol->class_name().c_str() : "NULL");
#endif

    // This is used to count the number of symbols of the same type in a single
    // scope. size_t numberOfSymbols = 0; bool forceMoreNameQualification =
    // false;

    // DQ (4/12/2014): we need to record that there was another function
    // identified in the parent scopes that we will want to have force name
    // qualification.
    bool foundAnOverloadedFunctionWithSameName = false;
    bool foundAnOverloadedFunctionInSameScope = false;

    // DQ (2/14/2019): Save a copy of the symbol looked up by name so that we
    // can resolve if a variable hides a type (which is where name qualification
    // is not appropriate).
    SgSymbol *original_symbol_lookedup_by_name = symbol;

    if (symbol != NULL) {
      // MLOG_WARN_C(MLOG_UNPARSER, "Lookup symbol based on name only: symbol =
      // %p = %s \n",symbol,symbol->class_name().c_str());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Lookup symbol based on name only (via parents starting at "
                  "currentScope = %p = %s: name = %s symbol = %p = %s) \n",
                  currentScope, currentScope->class_name().c_str(), name.str(),
                  symbol, symbol->class_name().c_str());
      if (isSgFunctionSymbol(symbol) != NULL) {
        SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
        SgFunctionDeclaration *functionDeclaration =
            functionSymbol->get_declaration();
        ASSERT_not_null(functionDeclaration);
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "functionSymbol = %p functionDeclaration = %p = %s name = %s \n",
            functionSymbol, functionDeclaration,
            functionDeclaration->class_name().c_str(),
            functionDeclaration->get_name().str());
      }
#endif
      SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);

      // DQ (7/12/2014): The newer design of the symbol table handling means
      // that we will never see a SgAliasSymbol at this level.
      ROSE_ASSERT(aliasSymbol == NULL);

      if (aliasSymbol != NULL) {
        symbol = aliasSymbol->get_alias();
        ASSERT_not_null(symbol);

        // DQ (7/12/2014): The newer design of the symbol table handling means
        // that we will never see a SgAliasSymbol at this level.
        ROSE_ABORT();
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "In NameQualificationTraversal::nameQualificationDepth(): Detected "
            "a SgAliasSymbol: alias = %p baseSymbol = %p = %s \n",
            aliasSymbol, symbol, symbol->class_name().c_str());
        // DQ (8/21/2012): Commented out the assertion, but warn about how
        // nesting appears to be present. DQ (8/20/2012): The symbol in side the
        // SgAliasSymbol should not be another alias. This fails for
        // test2004_48.C).
        if (isSgAliasSymbol(symbol) != NULL) {
          // DQ (8/24/2012): Allow this to be output a little less often.
          static int counter = 0;
          if (counter++ % 100 == 0) {
            MLOG_WARN_C(MLOG_UNPARSER,
                        "WARNING: can't assert isSgAliasSymbol(symbol) == NULL "
                        "after processing SgAliasSymbol (might require loop to "
                        "strip away nested SgAliasSymbol symbols. \n");
          }
        }
        // ROSE_ASSERT(isSgAliasSymbol(symbol) == NULL);
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
      // We have to check the kind of declaration against the kind of symbol
      // found. A local variable (for example) could hide the same name used for
      // the declaration.  This if we find symbol inconsistant with the
      // declaration then we need some form of qualification (sometimes just
      // type elaboration).
      MLOG_WARN_C(MLOG_UNPARSER, "### Targeting a declaration = %p = %s \n",
                  declaration, declaration->class_name().c_str());
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "   --- declaration->get_firstNondefiningDeclaration() = %p \n",
          declaration->get_firstNondefiningDeclaration());
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "   --- declaration->get_definingDeclaration()         = %p \n",
          declaration->get_definingDeclaration());
#endif
      switch (declaration->variantT()) {
        // DQ (12/26/2011): Added support for template class declarations (part
        // of new design for template declarations).
      case V_SgTemplateClassDeclaration:
      case V_SgClassDeclaration: {
        SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration);
        ASSERT_not_null(classDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "classDeclaration name = %s \n",
                    classDeclaration->get_name().str());
#endif
        SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
        // ASSERT_not_null(classSymbol);
        if (classSymbol == NULL) {
          bool typedefAliasesClass = false;
          SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
          if (typedefSymbol != NULL) {
            SgTypedefDeclaration *typedefDeclaration =
                typedefSymbol->get_declaration();
            if (typedefDeclaration != NULL) {
              SgType *baseType = typedefDeclaration->get_base_type();
              if (baseType != NULL) {
                const int strip_bits =
                    SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
                    SgType::STRIP_RVALUE_REFERENCE_TYPE |
                    SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE |
                    SgType::STRIP_TYPEDEF_TYPE;
                baseType = baseType->stripType(strip_bits);
              }
              SgClassType *classType = isSgClassType(baseType);
              if (classType != NULL) {
                SgDeclarationStatement *typedefDeclarationStatement =
                    classType->get_declaration();
                SgClassDeclaration *typedefClassDeclaration =
                    isSgClassDeclaration(typedefDeclarationStatement);
                if (typedefClassDeclaration != NULL) {
                  SgDeclarationStatement *typedefFirstDeclaration =
                      typedefClassDeclaration
                          ->get_firstNondefiningDeclaration();
                  if (typedefFirstDeclaration == NULL) {
                    typedefFirstDeclaration = typedefClassDeclaration;
                  }
                  SgDeclarationStatement *classFirstDeclaration =
                      classDeclaration->get_firstNondefiningDeclaration();
                  if (classFirstDeclaration == NULL) {
                    classFirstDeclaration = classDeclaration;
                  }
                  typedefAliasesClass =
                      (typedefFirstDeclaration == classFirstDeclaration);
                }
              }
            }
          }

          // This is only type elaboration if it is a variable that is the
          // conflict, if it is a typedef then more qualification is required.
          // (see test2011_37.C). MLOG_WARN_C(MLOG_UNPARSER, "Type elaboration
          // is required: declaration = %s symbol = %s
          // \n",declaration->class_name().c_str(),symbol->class_name().c_str());
          // typeElaborationIsRequired = true;
          if (typedefAliasesClass == false &&
              requiresTypeElaboration(symbol) == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Type elaboration is required: declaration = %s symbol = %s \n",
                declaration->class_name().c_str(),
                symbol->class_name().c_str());
#endif
            typeElaborationIsRequired = true;

            // DQ (2/13/2019): Adding more name qualification (debugging
            // test2011_33.C). forceMoreNameQualification = true;
          } else if (typedefAliasesClass == false) {
            // I think we have to force an extra level of name
            // qualification.
            // DQ (2/13/2019): I think we need to check if a qualified
            // nondefining declaration has been made for this class, else no
            // qualification should be output.
            SgDeclarationStatement *declarationToSearchForInReferencedNameSet =
                declaration->get_firstNondefiningDeclaration() != NULL
                    ? declaration->get_firstNondefiningDeclaration()
                    : declaration;
            ASSERT_not_null(declarationToSearchForInReferencedNameSet);
            bool skipNameQualification = false;
            if (referencedNameSet.find(
                    declarationToSearchForInReferencedNameSet) ==
                referencedNameSet.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "   --- $$$$$$$$$$ NOT Found: declaration %p = %s in "
                  "referencedNameSet referencedNameSet.size() = %" PRIuPTR
                  " \n",
                  declaration, declaration->class_name().c_str(),
                  referencedNameSet.size());
#endif
              skipNameQualification = true;
            } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- $$$$$$$$$$ FOUND: declaration %p = %s in "
                          "referencedNameSet \n",
                          declaration, declaration->class_name().c_str());
#endif
            }

            // Check if a nondefining declaration has been seen already, if so
            // then this may be a non-defining or defining declaration in
            // another scope and they name qualification would be required.
            // forceMoreNameQualification = true;
            if (skipNameQualification == false) {
              forceMoreNameQualification = true;
            }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Forcing an extra level of name qualification "
                        "forceMoreNameQualification = %s \n",
                        forceMoreNameQualification ? "true" : "false");
#endif
          }

          // DQ (8/16/2013): Modified API for symbol lookup.
          // Reset the symbol to one that will match the declaration.
          // symbol =
          // SageInterface::lookupClassSymbolInParentScopes(name,currentScope);
          symbol = SageInterface::lookupClassSymbolInParentScopes(
              name, currentScope, NULL);
          // ASSERT_not_null(symbol);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          if (symbol != NULL) {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
          } else {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "classSymbol == NULL \n");
          }
#endif
        } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Symbol matches the class declaration "
                      "(classDeclaration->get_firstNondefiningDeclaration()) "
                      "classSymbol->get_declaration() = %p \n",
                      classSymbol->get_declaration());
#endif
          // DQ (6/9/2011): I would prefer to have this be true and it might
          // work if it is not, but I would like to have this be a warning for
          // now!
          // ROSE_ASSERT(classDeclaration->get_firstNondefiningDeclaration() ==
          // classSymbol->get_declaration());
          if (classDeclaration->get_firstNondefiningDeclaration() !=
              classSymbol->get_declaration()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "WARNING: classDeclaration->get_firstNondefiningDeclaration() "
                "!= classSymbol->get_declaration() \n");
#endif
          }

          // DQ (1/24/2019): Find any associated (outer) class definition scope
          // and check if we need global qualification.
          SgScopeStatement *temp_scope = currentScope;
          while (isSgGlobal(temp_scope) == NULL &&
                 isSgClassDefinition(temp_scope) == NULL) {
            temp_scope = temp_scope->get_scope();
          }
          SgClassDefinition *classDefinition = isSgClassDefinition(temp_scope);
          if (classDefinition != NULL) {
            SgClassDeclaration *definingClassDeclaration =
                classDefinition->get_declaration();
            ASSERT_not_null(definingClassDeclaration);
            SgClassDeclaration *nondefiningClassDeclaration =
                isSgClassDeclaration(definingClassDeclaration
                                         ->get_firstNondefiningDeclaration());
            ASSERT_not_null(nondefiningClassDeclaration);

            if (inaccessibleClassSets.find(nondefiningClassDeclaration) !=
                inaccessibleClassSets.end()) {
              // If any of the class declarations in the list of inaccessible
              // class declaration match, then we have to add global
              // qualification.
              std::set<SgClassDeclaration *> &inaccessible_classes =
                  inaccessibleClassSets[nondefiningClassDeclaration];
              SgClassDeclaration *nondefiningClassDeclaration =
                  isSgClassDeclaration(
                      classDeclaration->get_firstNondefiningDeclaration());
              // ROSE_ASSERT(nondefiningClassDeclaration ==
              // classDeclaration->get_firstNondefiningDeclaration());

              if (inaccessible_classes.find(nondefiningClassDeclaration) !=
                  inaccessible_classes.end()) {
                qualificationDepth++;
              }
            }
          }
        }

        break;
      }

      case V_SgNamespaceDeclarationStatement: {
        // There is no type elaboration for a reference to a namespace, so I am
        // not sure what to do here.
        SgNamespaceDeclarationStatement *namespaceDeclaration =
            isSgNamespaceDeclarationStatement(declaration);
        ASSERT_not_null(namespaceDeclaration);

        SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);

        // DQ (6/5/2011): Added support for case where namespaceSymbol == NULL.
        // ASSERT_not_null(namespaceSymbol);
        if (namespaceSymbol == NULL) {
          // This is the case of test2011_72.C (where there is a function with a
          // name matching the name of the namespace). There is no such think a
          // namespace elaboration, but if there was it might be required at
          // this point.

          // Reset the symbol to one that will match the declaration.
          symbol = SageInterface::lookupNamespaceSymbolInParentScopes(
              name, currentScope);

          // ASSERT_not_null(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          if (symbol != NULL) {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
          } else {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "functionSymbol == NULL \n");
          }
#endif
        } else {
          // This is the typical case.
        }

        break;
      }

      case V_SgNamespaceAliasDeclarationStatement: {
        // There is no type elaboration for a reference to a namespace, so I am
        // not sure what to do here.
        SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
            isSgNamespaceAliasDeclarationStatement(declaration);
        ASSERT_not_null(namespaceAliasDeclaration);

        SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);

        // DQ (8/1/2020): Record the associated
        // NamespaceAliasDeclarationStatement so it can be used instead in
        // namequalification. SgNamespaceDeclarationStatement*
        // namespaceDeclaration =
        // namespaceAliasDeclaration->get_namespaceDeclaration();
        // namespaceAliasDeclarationMap.insert(pair<SgNamespaceDeclarationStatement,SgNamespaceAliasDeclarationStatement>(namespaceDeclaration,namespaceAliasDeclaration));
        // printf ("case V_SgNamespaceAliasDeclarationStatement:
        // namespaceAliasDeclarationMap.size() = %zu
        // \n",namespaceAliasDeclarationMap.size());

        // DQ (6/5/2011): Added support for case where namespaceSymbol == NULL.
        // ASSERT_not_null(namespaceSymbol);
        if (namespaceSymbol == NULL) {
          // This is the case of test2011_72.C (where there is a function with a
          // name matching the name of the namespace). There is no such think a
          // namespace elaboration, but if there was it might be required at
          // this point.

          // Reset the symbol to one that will match the declaration.
          symbol = SageInterface::lookupNamespaceSymbolInParentScopes(
              name, currentScope);

          // ASSERT_not_null(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          if (symbol != NULL) {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
          } else {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "functionSymbol == NULL \n");
          }
#endif
        } else {
          // This is the typical case.
        }
        break;
      }

        // DQ (12/28/2011): Added support for template functions and template
        // member functions.
      case V_SgTemplateFunctionDeclaration:
      case V_SgTemplateMemberFunctionDeclaration:

        // DQ (6/1/2011): Added case for SgTemplateInstantiationFunctionDecl.
        // case V_SgTemplateInstantiationFunctionDecl:
      case V_SgTemplateInstantiationMemberFunctionDecl:
      case V_SgMemberFunctionDeclaration:
      case V_SgFunctionDeclaration: {
        SgFunctionDeclaration *functionDeclaration =
            isSgFunctionDeclaration(declaration);
        ASSERT_not_null(functionDeclaration);
#if DEBUG_FUNCTION_RESOLUTION
        printf("functionDeclaration = %p = %s name = %s \n",
               functionDeclaration, functionDeclaration->class_name().c_str(),
               functionDeclaration->get_name().str());
#endif
        SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case function declaration: functionSymbol = %p \n",
                    functionSymbol);
#endif
        // DQ (7/22/2017): Added test for
        // SgTemplateInstantiationDirectiveStatement, so that we can process the
        // template arguments correctly (using the scope of the
        // SgTemplateInstantiationDirectiveStatement instead of the scope of the
        // SgTemplateInstantiationMemberFunctionDecl (which can be different)).
        // DQ (6/3/2017): Add test to check if this is part of a template
        // instantiation directive. However, I think that out use of name
        // qualification is independent of this result.
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    functionDeclaration->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "******** Found a member function template instantiation that is "
              "a part of a SgTemplateInstantiationDirectiveStatement \n");
#endif
        }

        SgFunctionType *functionType = functionDeclaration->get_type();
        ASSERT_not_null(functionType);
#if DEBUG_FUNCTION_RESOLUTION
        printf("functionSymbol = %p \n", functionSymbol);
#endif
        // ASSERT_not_null(classSymbol);
        if (functionSymbol == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Type elaboration is required: declaration = %s symbol = %s \n",
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          // DQ (7/25/2018): Type elaboration does not make sense for functions.
          // This is a case where name qualification is required because the
          // function is hidden by some non-function. The symbol was non-null
          // and it was not a function.  Question: could it be a function that
          // hides the function name from another function (I think so)?
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(MLOG_UNPARSER,
                      "########### NOTE: NEED TO FORCE NAME QUALIFICATION "
                      "SINCE TYPE ELABLORATION IS NOT A SOLUTION FOR "
                      "declarations hiding a function \n");
          MLOG_WARN_C(MLOG_UNPARSER,
                      "functionSymbol == NULL: symbol = %p = %s \n", symbol,
                      symbol->class_name().c_str());
#endif
          // DQ (9/2/2020): Name qualification of functions for
          // SgCtorInitializionList should not force name qualification. I think
          // we have to force an extra level of name qualification (see
          // Cxx11_tests/test2018_121.C).
          forceMoreNameQualification = true;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(MLOG_UNPARSER,
                      "WARNING: Present implementation of symbol table will "
                      "not find alias symbols of SgFunctionSymbol \n");
#endif
          // Reset the symbol to one that will match the declaration.
          // DQ (4/12/2014): I think we need to use the version of the function
          // that matches the function type. See test2014_42.C for an example of
          // this. symbol =
          // SageInterface::lookupFunctionSymbolInParentScopes(name,currentScope);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(MLOG_UNPARSER,
                      "NOTE: we are now using the function type in the initial "
                      "function symbol lookup? \n");
#endif

          // DQ (4/6/2018): Note that since we use the function type, we are
          // getting the subset of matching function that would force additional
          // name qualification.
          symbol = SageInterface::lookupFunctionSymbolInParentScopes(
              name, functionType, currentScope);

          // ASSERT_not_null(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          if (symbol != NULL) {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
          } else {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "functionSymbol == NULL \n");
          }
#endif
        } else {
          // DQ (4/12/2014): But is this the correct symbol for a function of
          // the same type. See test2014_42.C for an example where this is an
          // overloaded function declaration and the WRONG one.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(MLOG_UNPARSER,
                      "case function declaration: functionSymbol = %p: but is "
                      "it associated with the correct type \n",
                      functionSymbol);
#endif
          SgFunctionType *functionTypeAssociatedWithSymbol =
              isSgFunctionType(functionSymbol->get_type());
          ASSERT_not_null(functionTypeAssociatedWithSymbol);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
          MLOG_WARN_C(MLOG_UNPARSER,
                      "case function declaration: functionType = %p \n",
                      functionType);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "case function declaration: "
                      "functionTypeAssociatedWithSymbol = %p \n",
                      functionTypeAssociatedWithSymbol);
#endif
          if (functionType != functionTypeAssociatedWithSymbol) {
#if ((DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0) || DEBUG_FUNCTION_RESOLUTION
            printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                   "@@@@@@@@@@@@@@@@@@@@ \n");
            MLOG_WARN_C(MLOG_UNPARSER,
                        "NOTE: we are now using the function type in the "
                        "initial function symbol lookup? \n");
            printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                   "@@@@@@@@@@@@@@@@@@@@ \n");
#endif
            // DQ (4/12/2014): we need to record that there was another function
            // identified in the parent scopes that we will want to have force
            // name qualification.
            foundAnOverloadedFunctionWithSameName = true;

            // DQ (4/12/2014): Check if the scopes are the same.  If the same
            // then we don't need name qualification.
            SgScopeStatement *scopeAssociatedWithSymbol =
                functionSymbol->get_declaration()->get_scope();
            ASSERT_not_null(scopeAssociatedWithSymbol);
            SgScopeStatement *scopeOfDeclaration = declaration->get_scope();
            ASSERT_not_null(scopeOfDeclaration);

            if (scopeAssociatedWithSymbol == scopeOfDeclaration) {
              foundAnOverloadedFunctionInSameScope = true;
            }

            // DQ (4/6/2018): Note that since we use the function type, we are
            // getting the subset of matching function that would force
            // additional name qualification.
            symbol = SageInterface::lookupFunctionSymbolInParentScopes(
                name, functionType, currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
            MLOG_WARN_C(MLOG_UNPARSER,
                        "After using the function type: symbol = %p \n",
                        symbol);
#endif
          } else {

            // DQ (9/21/2020): Cxx11_tests/test2020_95.C and test2020_100.C
            // demonstrate that this is not enough. DQ (8/30/2020): Adding
            // support for a more sophisticated level of function ambiguity
            // resolution. Here we add the lookup of same named functions in the
            // scopes defined by the types associated with function parameters.

#define DEBUG_FUNCTION_AMBIGUITY (0 || DEBUG_FUNCTION_RESOLUTION)

#if DEBUG_FUNCTION_AMBIGUITY
            printf("\n\nWe found the correct function, but now we need to "
                   "check for any other possible matches that would drive more "
                   "name qualification \n");

            printf("Before loop over function parameter types: "
                   "foundAnOverloadedFunctionWithSameName = %s \n",
                   foundAnOverloadedFunctionWithSameName ? "true" : "false");
            printf("Before loop over function parameter types: "
                   "foundAnOverloadedFunctionInSameScope = %s \n",
                   foundAnOverloadedFunctionInSameScope ? "true" : "false");
            printf(" --- currentScope   = %p = %s name = %s \n", currentScope,
                   currentScope->class_name().c_str(),
                   SageInterface::get_name(currentScope).c_str());
            printf(" --- functionSymbol = %p = %s name = %s \n", functionSymbol,
                   functionSymbol->class_name().c_str(),
                   SageInterface::get_name(functionSymbol).c_str());
#endif
            SgDeclarationStatement *declaration =
                functionSymbol->get_declaration();
            ROSE_ASSERT(declaration != NULL);
            bool isFriendFunction =
                (declaration->get_declarationModifier().isFriend() == true);

            // Compute a scope outside of the scope where the function is
            // recognized.
            SgScopeStatement *alternate_scope = declaration->get_scope();
            if (isSgGlobal(alternate_scope) == NULL) {
              alternate_scope = alternate_scope->get_scope();
            }

#if ((DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0) || DEBUG_FUNCTION_RESOLUTION
            printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&"
                   "&&&&&&&&&&&&&&&&&&&& \n");
            printf("In NameQualificationTraversal::nameQualificationDepth(): "
                   "check if we need function parameter resolution: "
                   "forceMoreNameQualification = %s \n",
                   forceMoreNameQualification ? "true" : "false");
            printf(
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "check if we need function parameter resolution: name = %s \n",
                name.str());
            printf("In NameQualificationTraversal::nameQualificationDepth(): "
                   "check if we need function parameter resolution: "
                   "declaration = %p = %s name = %s \n",
                   declaration, declaration->class_name().c_str(),
                   SageInterface::get_name(declaration).c_str());
            printf("In NameQualificationTraversal::nameQualificationDepth(): "
                   "check if we need function parameter resolution: "
                   "currentScope = %p = %s name = %s \n",
                   currentScope, currentScope->class_name().c_str(),
                   SageInterface::get_name(currentScope).c_str());
            printf("In NameQualificationTraversal::nameQualificationDepth(): "
                   "check if we need function parameter resolution: "
                   "alternate_scope = %p = %s name = %s \n",
                   alternate_scope, alternate_scope->class_name().c_str(),
                   SageInterface::get_name(alternate_scope).c_str());
            printf("In NameQualificationTraversal::nameQualificationDepth(): "
                   "check if we need function parameter resolution: "
                   "positionStatement = %p = %s name = %s \n",
                   positionStatement, positionStatement->class_name().c_str(),
                   SageInterface::get_name(positionStatement).c_str());
            printf("In NameQualificationTraversal::nameQualificationDepth(): "
                   "check if we need function parameter resolution: "
                   "functionType = %p = %s name = %s \n",
                   functionType, functionType->class_name().c_str(),
                   SageInterface::get_name(functionType).c_str());
#endif
            SgSymbol *alternate_symbol =
                SageInterface::lookupFunctionSymbolInParentScopes(
                    name, functionType, alternate_scope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_FUNCTION_RESOLUTION
            printf("alternate_symbol = %p \n", alternate_symbol);
            if (alternate_symbol != NULL) {
              printf("alternate_symbol = %p = %s name = %s \n",
                     alternate_symbol, alternate_symbol->class_name().c_str(),
                     alternate_symbol->get_name().str());
            }
#endif

#if DEBUG_FUNCTION_AMBIGUITY
            printf(" --- declaration = %p = %s name = %s \n", declaration,
                   declaration->class_name().c_str(),
                   SageInterface::get_name(declaration).c_str());
            printf(" --- isFriendFunction = %s \n",
                   isFriendFunction ? "true" : "false");
#endif
            bool symbols_match = ((alternate_symbol != NULL) &&
                                  (functionSymbol == alternate_symbol));
#if DEBUG_FUNCTION_AMBIGUITY
            printf(" --- symbols_match = %s \n",
                   symbols_match ? "true" : "false");
#endif
            // DQ (9/22/2020): Cxx11_tests/test2020_95.C demonstrated that we
            // needed more than just this code below to handle
            // Cxx11_tests/test2020_101.C. DQ (8/31/2020): friend functions are
            // not processed using this parameter based lookup. Specifically,
            // less name qualification is allowed for GNU versions after 7.x and
            // in particular version 10.2. Also an error for clang version 10.x.
            // if (isFriendFunction == false)
            if (isFriendFunction == false && symbols_match == false) {
              // Use the scopes of the function parameters to look for where
              // there could be an ambiguity.
              SgFunctionParameterTypeList *functionParameterTypeList =
                  functionType->get_argument_list();
              ROSE_ASSERT(functionParameterTypeList != NULL);

              SgTypePtrList &typeList =
                  functionParameterTypeList->get_arguments();
              for (SgTypePtrList::iterator i = typeList.begin();
                   i != typeList.end(); i++) {
                // for each type in the parameter type list.
                SgType *parameter_type = *i;
#if DEBUG_FUNCTION_AMBIGUITY
                printf("parameter_type = %p = %s \n", parameter_type,
                       parameter_type->class_name().c_str());
#endif
                unsigned char strip_bit_array =
                    SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
                    SgType::STRIP_RVALUE_REFERENCE_TYPE |
                    SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE |
                    SgType::STRIP_TYPEDEF_TYPE |
                    SgType::STRIP_POINTER_MEMBER_TYPE;

                SgType *stripped_parameter_type =
                    parameter_type->stripType(strip_bit_array);
#if DEBUG_FUNCTION_AMBIGUITY
                printf("stripped_parameter_type = %p = %s \n",
                       stripped_parameter_type,
                       stripped_parameter_type->class_name().c_str());
#endif
                SgNamedType *parameter_namedType =
                    isSgNamedType(stripped_parameter_type);
                if (parameter_namedType != NULL) {
                  SgDeclarationStatement *parameter_declaration =
                      parameter_namedType->get_declaration();
                  ROSE_ASSERT(parameter_declaration != NULL);
#if DEBUG_FUNCTION_AMBIGUITY
                  printf("parameter_declaration = %p = %s \n",
                         parameter_declaration,
                         parameter_declaration->class_name().c_str());
#endif
                  SgScopeStatement *parameter_scope = declaration->get_scope();
                  ROSE_ASSERT(parameter_scope != NULL);
#if DEBUG_FUNCTION_AMBIGUITY
                  // printf ("parameter_scope = %p = %s
                  // \n",parameter_scope,parameter_scope->class_name().c_str());
                  printf("parameter_scope = %p = %s name = %s \n",
                         parameter_scope, parameter_scope->class_name().c_str(),
                         SageInterface::get_name(parameter_scope).c_str());
#endif

                  // Check if this is in the parent scopes.
                  bool detectedInParentScope = false;
                  SgScopeStatement *tmp_scope = currentScope;
                  // while (tmp_scope != NULL && tmp_scope != parameter_scope)
                  while (tmp_scope != NULL && isSgGlobal(tmp_scope) == NULL &&
                         tmp_scope != parameter_scope) {
#if DEBUG_FUNCTION_AMBIGUITY
                    printf("tmp_scope = %p = %s name = %s \n", tmp_scope,
                           tmp_scope->class_name().c_str(),
                           SageInterface::get_name(tmp_scope).c_str());
#endif
                    tmp_scope = tmp_scope->get_scope();
                    if (tmp_scope != NULL && tmp_scope == parameter_scope) {
#if DEBUG_FUNCTION_AMBIGUITY
                      printf("Found parameter_scope in parent scopes \n");
#endif
                      detectedInParentScope = true;
                    }
                  }

                  ROSE_ASSERT(tmp_scope != NULL);
#if DEBUG_FUNCTION_AMBIGUITY
                  printf("detectedInParentScope = %s \n",
                         detectedInParentScope ? "true" : "false");
                  printf("After loop: tmp_scope = %p = %s name = %s \n",
                         tmp_scope, tmp_scope->class_name().c_str(),
                         SageInterface::get_name(tmp_scope).c_str());
#endif
                  SgGlobal *globalScope = isSgGlobal(tmp_scope);
                  // if (detectedInParentScope == true)
                  if (detectedInParentScope == true && globalScope == NULL) {
                    SgSymbol *parameter_symbol =
                        SageInterface::lookupFunctionSymbolInParentScopes(
                            name, functionType, parameter_scope);
                    if (parameter_symbol != NULL) {
#if DEBUG_FUNCTION_AMBIGUITY
                      printf(
                          "Found an ambiguity: parameter_symbol = %p = %s \n",
                          parameter_symbol,
                          parameter_symbol->class_name().c_str());
#endif
                      foundAnOverloadedFunctionWithSameName = true;
                      foundAnOverloadedFunctionInSameScope = false;
                    }
                  }
                }
              }
            }
#if DEBUG_FUNCTION_AMBIGUITY
            printf("After loop over function parameter types: "
                   "foundAnOverloadedFunctionWithSameName = %s \n",
                   foundAnOverloadedFunctionWithSameName ? "true" : "false");
            printf("After loop over function parameter types: "
                   "foundAnOverloadedFunctionInSameScope = %s \n",
                   foundAnOverloadedFunctionInSameScope ? "true" : "false");
            printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&"
                   "&&&&&&&&&&&&&&&&&&&& \n");
#endif
          }
        }
        // numberOfSymbols = currentScope->count_symbol(name);
        break;
      }

        // DQ (11/10/2014): Added support for templated typedefs (and their
        // instantiations).
      case V_SgTemplateTypedefDeclaration:
      case V_SgTemplateInstantiationTypedefDeclaration:

      case V_SgTypedefDeclaration: {
        SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(declaration);
        ASSERT_not_null(typedefDeclaration);

        // DQ (7/22/2017): Added test for
        // SgTemplateInstantiationDirectiveStatement, so that we can process the
        // template arguments correctly (using the scope of the
        // SgTemplateInstantiationDirectiveStatement instead of the scope of the
        // SgTemplateInstantiationTypedefDeclaration (which can be different)).
        // DQ (6/3/2017): Add test to check if this is part of a template
        // instantiation directive. However, I think that out use of name
        // qualification is independent of this result.
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    typedefDeclaration->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "******** Found a typedef template instantiation that is a part "
              "of a SgTemplateInstantiationDirectiveStatement \n");
#endif
          MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
          ROSE_ABORT();
        }

        SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
        if (typedefSymbol == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Type elaboration is required: declaration = %s symbol = %s \n",
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          typeElaborationIsRequired = true;

          // MLOG_WARN_C(MLOG_UNPARSER, "WARNING: Present implementation of
          // symbol table will not find alias symbols of SgTypedefSymbol \n");

          // Reset the symbol to one that will match the declaration.
          symbol = SageInterface::lookupTypedefSymbolInParentScopes(
              name, currentScope);
          // DQ (5/15/2011): Added this to support where symbol after moving
          // name qualification support to the astPostProcessing phase instead
          // of calling it in the unparser.
          if (symbol != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
#endif
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "typedefSymbol == NULL \n");
#endif
            // DQ (7/24/2011): I don't understand this code...this appears to be
            // a cut/paste error. Look for a template symbol symbol =
            // SageInterface::lookupTemplateSymbolInParentScopes(name,currentScope);
            // ASSERT_not_null(symbol);
          }
        }
        break;
      }

        // DQ (8/13/2013): I think that this case should not
        // appear, since SgTemplateDeclaration is a part of
        // older work.
      case V_SgTemplateDeclaration: {
        SgTemplateDeclaration *templateDeclaration =
            isSgTemplateDeclaration(declaration);
        ASSERT_not_null(templateDeclaration);
        // DQ (7/24/2018): This is output spew for
        // Cxx11_tests/test2016_90.C and
        // Cxx_tests/test2013_63.C (and others). It is not new,
        // but it is also not clear that it is too much of an
        // issue that we have some used of
        // SgTemplateDeclaration in place since within
        // templates we can at times not have enough
        // information to build anything more specific. All of
        // these issues appear to be related to input codes
        // using template-heavy headers.
        symbol = NULL;

        break;
      }

        // DQ (11/16/2013): I think we do need this case and test2013_273.C
        // demonstrates this. DQ (6/1/2011): Not clear if we need a special case
        // for the case of SgTemplateInstantiationMemberFunctionDecl. I think we
        // need to call: evaluateNameQualificationForTemplateArgumentList() to
        // evaluate template arguments for both
        // SgTemplateInstantiationFunctionDecl and
        // SgTemplateInstantiationMemberFunctionDecl.
      case V_SgTemplateInstantiationFunctionDecl: {
        // DQ (6/4/2017): Added notes on the additional complexity of name
        // qualification for template instantiations. Note that there are
        // several things that can cause name qualification for a template
        // instantiation:
        //    1) A different template instantiation that is visible from the
        //    same scope (current scope),
        //       this would be the typical case most similar to other
        //       constructs.
        //    2) A template declaration that is visible from the same scope
        //    (current scope) using the same
        //       name as the template declaration associated with the template
        //       instantiation.
        //    3) A non-template function declaration (member of non-member) that
        //    is visible from the same
        //       scope (current scope) using the same name. See test2017_40.C.
        // Note also that when the current scope is a namespace, that lookups
        // have to be normalized in terms of the global namespace (unique
        // namespace definition used to union all declaration across all
        // namespaces that are equivalent (becuase namespaces are reentrent).

        SgTemplateInstantiationFunctionDecl *templateInstantiationFunction =
            isSgTemplateInstantiationFunctionDecl(declaration);
        ASSERT_not_null(templateInstantiationFunction);

        // DQ (6/3/2017): Add test to check if this is part of a template
        // instantiation directive. However, I think that out use of name
        // qualification is independent of this result.
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    templateInstantiationFunction->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
          // DQ (11/18/2017): Commented out the trap to force an exit here!
        }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "In NameQualificationTraversal::nameQualificationDepth(): "
                    "case V_SgTemplateInstantiationFunctionDecl: "
                    "templateInstantiationFunction = %p = %s \n",
                    templateInstantiationFunction,
                    templateInstantiationFunction->class_name().c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- templateInstantiationFunction->get_name()         = %s \n",
            templateInstantiationFunction->get_name().str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- templateInstantiationFunction->get_templateName() = %s \n",
            templateInstantiationFunction->get_templateName().str());
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        // If this is a SgNamespaceDefinition, then we need to reset it to the
        // uniquly represented namespace definition.
        MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s \n", currentScope,
                    currentScope->class_name().c_str());
#endif
        // DQ (6/3/2017): Adding code to normalize the scope where it is a
        // namespace definition, since that can be more than one that are the
        // same namespace and we have a mechanism to resolve equivalents for
        // this special case. BTW, I worry that this should be done more
        // generally and uniformally within the name qualification support.
        SgNamespaceDefinitionStatement *namespaceDefinitionStatement =
            isSgNamespaceDefinitionStatement(currentScope);
        if (namespaceDefinitionStatement != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Reset the currentScope to the namespace's global "
                      "definition (namespace normalization) \n");
#endif
          currentScope = namespaceDefinitionStatement->get_global_definition();
          ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "calling lookupFunctionSymbolInParentScopes(): name = %s "
                      "currentScope = %p = %s \n",
                      name.str(), currentScope,
                      currentScope->class_name().c_str());
#endif
          // Reset the symbol to be consistant with the unique scope (in case
          // the currentScope was reset above.
          symbol = SageInterface::lookupFunctionSymbolInParentScopes(
              name, currentScope);

          // DQ (7/21/2024): Added debugging output for testing.
          if (symbol == NULL) {
            printf("SageInterface::lookupFunctionSymbolInParentScopes() "
                   "returned NULL: name = %s currentScope = %p = %s = %s \n",
                   name.str(), currentScope, currentScope->class_name().c_str(),
                   SageInterface::get_name(currentScope).c_str());
          }

          // DQ (7/21/2024): This can be NULL for the processing of
          // nlohmann/json.hpp with ROSE ASSERT_not_null(symbol);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "@@@@@@@@ name = %s declaration = %p = %s symbol = %s \n",
                      name.str(), declaration,
                      declaration->class_name().c_str(),
                      symbol->class_name().c_str());
#endif
        }

        SgTemplateSymbol *templateSymbol = isSgTemplateSymbol(symbol);

        // DQ (5/21/2017): I think this is never a SgTemplateSymbol (checking).
        // DQ (6/4/2017): Maybe if it was a template specialization that was
        // forcing the name qualification (then the names might match, we would
        // need to have a test code example of this).
        ROSE_ASSERT(templateSymbol == NULL);

        if (templateSymbol == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Type elaboration is required: declaration = %s symbol = %s \n",
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          // The existance of any symbol identified to hide the current
          // statement is cause for at least type elaboration.
          typeElaborationIsRequired = true;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "calling lookupFunctionSymbolInParentScopes(): name = %s "
                      "currentScope = %p = %s \n",
                      name.str(), currentScope,
                      currentScope->class_name().c_str());
#endif
          // Reset the symbol to one that will match the declaration.
          // DQ (4/12/2014): I think we need to use the function type here!
#if (DEBUG_NAME_QUALIFICATION_LEVEL >= 1) || 0
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Should we be using the function type in the initial "
                      "SgTemplateInstantiationFunctionDecl symbol lookup? \n");
#endif
          // Reset the symbol to be consistatn with the unique scope (in case
          // the currentScope was reset above. symbol =
          // SageInterface::lookupFunctionSymbolInParentScopes(name,currentScope);

          // DQ (6/23/2013): Fixing test2013_223.C (function hiding template
          // function instantiation).
          SgName templateInstantiationFunctionNameWithoutTemplateArguments =
              templateInstantiationFunction->get_templateName();

          // DQ (5/21/2017): Get the name with template arguments (I think we
          // need it instead).
          SgName templateInstantiationFunctionName =
              templateInstantiationFunction->get_name();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "   --- "
              "templateInstantiationFunctionNameWithoutTemplateArguments = %s "
              "\n",
              templateInstantiationFunctionNameWithoutTemplateArguments.str());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "   --- templateInstantiationFunctionName                "
                      "         = %s \n",
                      templateInstantiationFunctionName.str());
#endif

          // DQ (4/6/2018): I think we should be using the function type in the
          // lookupFunctionSymbolInParentScopes(), else we could be confusing
          // overloaded function which would not require name qualification.
          // MLOG_WARN_C(MLOG_UNPARSER, "Shouldn't we be using the function type
          // to refine the symbol table lookup? \n");

          // DQ (5/22/2017): Modified to use the name with the template
          // arguments. DQ (4/4/2014): Modified this to use the SgFunctionSymbol
          // type. SgSymbol* symbolHiddingTemplateSymbol =
          // SageInterface::lookupFunctionSymbolInParentScopes(templateFunctionNameWithoutTemplateArguments,currentScope);
          // SgFunctionSymbol* symbolHiddingTemplateInstantiationSymbol =
          // SageInterface::lookupFunctionSymbolInParentScopes(templateInstantiationFunctionNameWithoutTemplateArguments,currentScope);

          // DQ (4/7/2018): Adding the function type to make this more precise
          // (and avoid different overloaded functions). SgFunctionSymbol*
          // symbolHiddingTemplateInstantiationSymbol =
          // SageInterface::lookupFunctionSymbolInParentScopes(templateInstantiationFunctionName,currentScope);
          SgFunctionType *functionType =
              templateInstantiationFunction->get_type();
          ASSERT_not_null(functionType);
          SgFunctionSymbol *symbolHiddingTemplateInstantiationSymbol =
              SageInterface::lookupFunctionSymbolInParentScopes(
                  templateInstantiationFunctionName, functionType,
                  currentScope);

          if (isSgFunctionSymbol(symbol) == NULL) {
            // A non-function symbol in the current scope can hide the
            // instantiation name. Continue the qualification analysis using
            // the matching function symbol instead of the hiding declaration.
            if (symbol != NULL) {
              forceMoreNameQualification = true;
            }
            symbol = symbolHiddingTemplateInstantiationSymbol;
          }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "symbolHiddingTemplateInstantiationSymbol = %p \n",
                      symbolHiddingTemplateInstantiationSymbol);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "symbol                                   = %p \n",
                      symbol);
#endif
          // Handle the case of a template instantiation hidding the template
          // instantiation for which we want to determine name qualification.
          // See test2017_39.C
          if (symbolHiddingTemplateInstantiationSymbol != NULL &&
              symbolHiddingTemplateInstantiationSymbol != symbol) {
            // There is reason to think this template instantiation should have
            // some name qualification.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "WARNING: There is reason to think this template instantiation "
                "should have some name qualification because it may be hidden "
                "by another template instantiation \n");
            MLOG_WARN_C(MLOG_UNPARSER,
                        "   --- templateInstantiationFunctionName = %s \n",
                        templateInstantiationFunctionName.str());
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "   --- templateInstantiationFunction mangled name = %s \n",
                templateInstantiationFunction->get_mangled_name().str());
            SgFunctionDeclaration *functionHiddingInputFunction =
                symbolHiddingTemplateInstantiationSymbol->get_declaration();
            ASSERT_not_null(functionHiddingInputFunction);
            MLOG_WARN_C(MLOG_UNPARSER,
                        "   --- functionHiddingInputFunction = %s \n",
                        functionHiddingInputFunction->get_name().str());
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "   --- functionHiddingInputFunction mangled name = %s \n",
                functionHiddingInputFunction->get_mangled_name().str());
#endif
          } else {
            // There is no template instantiation hidding the template
            // instantiation for which we are computing the name qualification.
          }

          // Now we need to check for a non-template instantiation function
          // hidding the template instantiation.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Look for symbols from name without template arguments: name = "
              "%s template declaration = %s symbol = %s \n",
              templateInstantiationFunctionNameWithoutTemplateArguments.str(),
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          // DQ (4/7/2018): I think we should MAYBE be using the function type
          // in the lookupFunctionSymbolInParentScopes(), else we could be
          // confusing overloaded function which would not require name
          // qualification. MLOG_WARN_C(MLOG_UNPARSER, "Shouldn't we be using
          // the function type to refine the symbol table lookup? \n");

          // Note name change to variable (for clarification).
          // DQ (5/23/2017): Note that for template instatiations the template
          // must be visible from the template instatiation (or name qualified
          // to to be visible).
          SgFunctionSymbol *symbolHiddingTemplateSymbol =
              SageInterface::lookupFunctionSymbolInParentScopes(
                  templateInstantiationFunctionNameWithoutTemplateArguments,
                  currentScope);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "symbolHiddingTemplateSymbol              = %p \n",
                      symbolHiddingTemplateSymbol);
#endif
          // Handle the case of a non-template instnatiation hidding the
          // template declaration for the template instantiation for which we
          // want to determine name qualification. See test2017_40.C
          if (symbolHiddingTemplateSymbol != NULL &&
              symbolHiddingTemplateSymbol != symbol) {
            // This looks up the scope via the symbol table's parent (not sure
            // that is a great approach). SgScopeStatement* hiddingSymbolScope =
            // isSgScopeStatement(symbolHiddingTemplateInstantiationSymbol->get_parent()->get_parent());

            // Get the scope where this symbol is in the symbol table.
            // SgScopeStatement* hiddingSymbolScope =
            // isSgScopeStatement(symbolHiddingTemplateInstantiationSymbol->get_parent()->get_parent());
            SgScopeStatement *hiddingSymbolScope = isSgScopeStatement(
                symbolHiddingTemplateSymbol->get_parent()->get_parent());
            ASSERT_not_null(hiddingSymbolScope);

            // Get the scope of the template instantiation.
            SgScopeStatement *functionScope =
                templateInstantiationFunction->get_scope();
            ASSERT_not_null(functionScope);
            // DQ (4/4/2014): Look at the declarations associated with these
            // symbols. SgDeclarationStatement*
            // declarationHidingCurrentDeclaration =
            // symbolHiddingTemplateInstantiationSymbol->get_declaration();
            SgDeclarationStatement *declarationHidingCurrentDeclaration =
                symbolHiddingTemplateSymbol->get_declaration();
            ASSERT_not_null(declarationHidingCurrentDeclaration);

            SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
            ASSERT_not_null(functionSymbol);

            SgDeclarationStatement *declarationFromSymbol =
                functionSymbol->get_declaration();
            ASSERT_not_null(declarationFromSymbol);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "hiddingSymbolScope                  = %p = %s \n",
                        hiddingSymbolScope,
                        hiddingSymbolScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER,
                        "declaration from symbol             = %p = %s \n",
                        declarationFromSymbol,
                        declarationFromSymbol->class_name().c_str());
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "declaration hidding template symbol = %p = %s \n",
                declarationHidingCurrentDeclaration,
                declarationHidingCurrentDeclaration->class_name().c_str());
#endif
            bool currentScopeIsNestedWithinScopeOfHiddingDeclaration = false;

            // SgScopeStatement* temp_symbolScope = symbolScope;
            SgScopeStatement *temp_scope = currentScope;
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "case SgTemplateInstantiationFunctionDecl (template "
                        "declaration hidden): temp_scope = %p = %s \n",
                        temp_scope, temp_scope->class_name().c_str());
#endif
            while (isSgGlobal(temp_scope) == NULL &&
                   temp_scope != hiddingSymbolScope) {
              temp_scope = temp_scope->get_scope();
            }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            if (temp_scope != NULL) {
              MLOG_WARN_C(MLOG_UNPARSER,
                          "final value of temp_scope = %p = %s \n", temp_scope,
                          temp_scope->class_name().c_str());
            } else {
              MLOG_WARN_C(MLOG_UNPARSER, "final value of temp_scope = NULL \n");
            }

            MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
#endif
            // if (temp_scope == hiddingSymbolScope)
            if (hiddingSymbolScope != currentScope &&
                temp_scope == hiddingSymbolScope) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "Note: hiddingSymbolScope != currentScope && "
                          "temp_scope == hiddingSymbolScope \n");
#endif
              currentScopeIsNestedWithinScopeOfHiddingDeclaration = true;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "   --- Resetting the recorded symbol to the hidding symbol  "
                  "(triggering the name qualification evaluation) \n");
#endif
              symbol = symbolHiddingTemplateSymbol;
            } else {
              // if (hiddingSymbolScope == currentScope)
              if (hiddingSymbolScope == currentScope &&
                  declarationFromSymbol !=
                      declarationHidingCurrentDeclaration) {
                // DQ (6/3/2017): In this case we need to detect when the
                // template instantiation is hidden by a non-template
                // instantiation with the same name (sans template arguments).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
                MLOG_WARN_C(MLOG_UNPARSER,
                            "Note: hiddingSymbolScope == currentScope \n");
#endif
                currentScopeIsNestedWithinScopeOfHiddingDeclaration = true;

                // DQ (6/3/2017): Test resetting the symbol to NULL (triggering
                // the name qualification evaluation).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
                MLOG_WARN_C(MLOG_UNPARSER,
                            "   --- Resetting the recorded symbol to the "
                            "hidding symbol  (triggering the name "
                            "qualification evaluation) \n");
#endif
                // symbol = NULL;

                symbol = symbolHiddingTemplateSymbol;
              }
            }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "currentScopeIsNestedWithinScopeOfHiddingDeclaration = %s \n",
                currentScopeIsNestedWithinScopeOfHiddingDeclaration ? "true"
                                                                    : "false");
            MLOG_WARN_C(MLOG_UNPARSER, "currentScope       = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "hiddingSymbolScope = %p = %s \n",
                        hiddingSymbolScope,
                        hiddingSymbolScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "temp_scope         = %p = %s \n",
                        temp_scope, temp_scope->class_name().c_str());
#endif
            // DQ (4/4/2014): We don't want to treat the template instantiation
            // as be hidden by the template declaration because that does not
            // make sense. So check the declarations.  Also, if the template
            // instatiation is not output in the generated code, then it can't
            // be name qualified.
            bool willBeOutput =
                (declaration->get_file_info()->isCompilerGenerated() == false ||
                 (declaration->get_file_info()->isCompilerGenerated() &&
                  declaration->get_file_info()->isOutputInCodeGeneration()));
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER, "$$$$$ --- willBeOutput = %s \n",
                        willBeOutput ? "true" : "false");
#endif
            // DQ (4/4/2014): I think that they don't have to be the same kid of
            // function. bool isSameKindOfFunction =
            // (declarationHidingCurrentDeclaration->variant() ==
            // declarationFromSymbol->variant()); DQ (6/3/2017): Comment out the
            // resetting of the symbol to NULL (as a test).

            // If we visit the scope with the function hidding our template
            // function then we will need some name qualification. if
            // (temp_scope == symbolScope) if (willBeOutput &&
            // isSameKindOfFunction && temp_scope == symbolScope) if
            // (willBeOutput == true || (willBeOutput == false && temp_scope ==
            // hiddingSymbolScope))
            if (willBeOutput == true ||
                (willBeOutput == false &&
                 currentScopeIsNestedWithinScopeOfHiddingDeclaration == true)) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "WARNING: Some qualification is required to get past "
                          "the non-template function hidding the template "
                          "function = %p = %s \n",
                          templateInstantiationFunction,
                          templateInstantiationFunction->get_name().str());
#endif
              // MLOG_WARN_C(MLOG_UNPARSER, "   --- Resetting the recorded
              // symbol to NULL \n"); symbol = NULL;
            }
          } else {
            // DQ (5/23/2017): This case is not a problem because the template
            // is visible from the template instantiation is does not require
            // name qualification.
          }

          // DQ (7/24/2011): The symbol is NULL for test2011_121.C
          // ASSERT_not_null(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          if (symbol != NULL) {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
          } else {
            // DQ (6/22/2011): This is demonstrated by test2011_121.C
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Detected no template function instantiation symbol in "
                        "a parent scope (ignoring this case for now) \n");
          }
#endif
        }
        break;
      }

      case V_SgEnumDeclaration: {
        SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
        ASSERT_not_null(enumDeclaration);

        SgEnumSymbol *enumSymbol = isSgEnumSymbol(symbol);
        if (enumSymbol == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Type elaboration is required: declaration = %s symbol = %s \n",
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          typeElaborationIsRequired = true;

          // Reset the symbol to one that will match the declaration.
          symbol =
              SageInterface::lookupEnumSymbolInParentScopes(name, currentScope);

          // ASSERT_not_null(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          if (symbol != NULL) {
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
          } else {
            // DQ (6/22/2011): This is demonstrated by test2011_95.C
            MLOG_WARN_C(MLOG_UNPARSER, "Detected no enum symbol in a parent "
                                       "scope (ignoring this case for now) \n");
          }
#endif

        } else {
          // DQ (5/30/2019): If this is a SgEnumSymbol then are they others such
          // that we require name qualification to select the correct enum
          // declaration (type).

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          SgSymbol *symbol = SageInterface::lookupSymbolInParentScopes(
              name, currentScope, templateParameterList, templateArgumentList);
          MLOG_WARN_C(MLOG_UNPARSER, "Previous lookup: symbol = %p = %s \n",
                      symbol,
                      (symbol != NULL) ? symbol->class_name().c_str() : "NULL");
#endif
          size_t symbol_count = currentScope->count_symbol(name);
          bool isUnNamed = enumDeclaration->get_isUnNamed();
          if (symbol_count > 1 && isUnNamed == false) {
            forceMoreNameQualification = true;
          }
        }

        break;
      }

      case V_SgTemplateInstantiationDecl: {
        SgTemplateInstantiationDecl *templateInstantiationDeclaration =
            isSgTemplateInstantiationDecl(declaration);
        ASSERT_not_null(templateInstantiationDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Found a case of declaration == SgTemplateInstantiationDecl \n");
#endif
        // DQ (7/22/2017): Added test for
        // SgTemplateInstantiationDirectiveStatement, so that we can process the
        // template arguments correctly (using the scope of the
        // SgTemplateInstantiationDirectiveStatement instead of the scope of the
        // SgTemplateInstantiationDecl (which can be different)). DQ (6/3/2017):
        // Add test to check if this is part of a template instantiation
        // directive. However, I think that out use of name qualification is
        // independent of this result.
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    templateInstantiationDeclaration->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
        }

        SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
        if (classSymbol == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Type elaboration is required: declaration = %s symbol = %s \n",
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          typeElaborationIsRequired = true;

          // MLOG_WARN_C(MLOG_UNPARSER, "We might need the template arguments to
          // look up this template class instantiation. \n");

          // DQ (8/15/2013): This needs to be used in
          // lookupClassSymbolInParentScopes(), but the function does not accept
          // a SgTemplateArgumentPtrList pointer yet.
          SgTemplateArgumentPtrList *templateArgumentsList =
              &(templateInstantiationDeclaration->get_templateArguments());

          // DQ (8/16/2013): Modified API for symbol lookup.
          // Reset the symbol to one that will match the declaration.
          // symbol =
          // SageInterface::lookupClassSymbolInParentScopes(name,currentScope);
          symbol = SageInterface::lookupClassSymbolInParentScopes(
              name, currentScope, templateArgumentsList);

          // DQ (5/15/2011): Added this to support where symbol after moving
          // name qualification support to the astPostProcessing phase instead
          // of calling it in the unparser.
          if (symbol != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                symbol, symbol->class_name().c_str());
#endif
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "classSymbol == NULL \n");
#endif
            SgTemplateClassDeclaration *templateClassDeclaration =
                templateInstantiationDeclaration->get_templateDeclaration();
            ASSERT_not_null(templateClassDeclaration);

            SgTemplateParameterPtrList &templateParameterList =
                templateClassDeclaration->get_templateParameters();
            SgTemplateArgumentPtrList &templateArgumentList =
                templateClassDeclaration->get_templateSpecializationArguments();

            // DQ (8/13/2013): This needs to be looked up as a
            // SgTemplateClassSymbol.
            symbol = SageInterface::lookupTemplateClassSymbolInParentScopes(
                name, &templateParameterList, &templateArgumentList,
                currentScope);
            // DQ (5/15/2011): This fails for test2004_77.C)...
            // ASSERT_not_null(symbol);
          }
        } else {
          SgDeclarationStatement *nestedDeclaration =
              classSymbol->get_declaration();
          ASSERT_not_null(nestedDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER, "Need to dig deeper into this symbol! \n");
          MLOG_WARN_C(MLOG_UNPARSER, "nestedDeclaration = %p = %s \n",
                      nestedDeclaration,
                      nestedDeclaration->class_name().c_str());
#endif
          SgTemplateInstantiationDecl *nestedTemplateDeclaration =
              isSgTemplateInstantiationDecl(nestedDeclaration);
          if (nestedTemplateDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER, "nestedTemplateDeclaration = %p = %s \n",
                        nestedTemplateDeclaration,
                        nestedTemplateDeclaration->get_name().str());
#endif
          }
        }

        break;
      }

        // DQ (3/13/2012): Added support for SgTemplateVariableDeclaration (I
        // think it can just leverage the SgVariableDeclaration case).
      case V_SgTemplateVariableDeclaration:

        // DQ (7/22/2017): We will be added this support shortly.
        // case V_SgTemplateInstantiationVariableDeclaration:

      case V_SgVariableDeclaration: {
        SgVariableDeclaration *variableDeclaration =
            isSgVariableDeclaration(declaration);
        ASSERT_not_null(variableDeclaration);

        SgVariableSymbol *variableSymbol = isSgVariableSymbol(symbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "variableSymbol = %p = %s \n",
                    variableSymbol, symbol->class_name().c_str());
#endif
        // DQ (7/22/2017): Added test for
        // SgTemplateInstantiationDirectiveStatement, so that we can process the
        // template arguments correctly (using the scope of the
        // SgTemplateInstantiationDirectiveStatement instead of the scope of the
        // SgTemplateInstantiationVariableDeclaration (which can be different)).
        // DQ (6/3/2017): Add test to check if this is part of a template
        // instantiation directive. However, I think that out use of name
        // qualification is independent of this result.
        SgTemplateInstantiationDirectiveStatement
            *templateInstantiationDirectiveStatement =
                isSgTemplateInstantiationDirectiveStatement(
                    variableDeclaration->get_parent());
        if (templateInstantiationDirectiveStatement != NULL) {
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "******** Found a variable template instantiation that is a part "
              "of a SgTemplateInstantiationDirectiveStatement \n");
          MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
          ROSE_ABORT();
        }

        if (variableSymbol == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Type elaboration is required: declaration = %s symbol = %s \n",
              declaration->class_name().c_str(), symbol->class_name().c_str());
#endif
          typeElaborationIsRequired = true;

          // Reset the symbol to one that will match the declaration.
          // symbol =
          // SageInterface::lookupVariableSymbolInParentScopes(name,currentScope);
          variableSymbol = SageInterface::lookupVariableSymbolInParentScopes(
              name, currentScope);
          // if (symbol != NULL)
          if (variableSymbol != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            // MLOG_WARN_C(MLOG_UNPARSER, "Lookup symbol based symbol type:
            // reset symbol = %p = %s \n",symbol,symbol->class_name().c_str());
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Lookup symbol based symbol type: reset symbol = %p = %s \n",
                variableSymbol, variableSymbol->class_name().c_str());
#endif
            symbol = variableSymbol;
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "variableSymbol == NULL \n");
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "WARNING: In "
                        "NameQualificationTraversal::nameQualificationDepth(): "
                        "variableSymbol == NULL: searching for the associated "
                        "symbol using the declaration \n");
#endif
            // DQ (8/14/2013): Use an alternative mechanism to get the correct
            // symbol more directly (might be more expensive, or perhaps this
            // mechanism should be used more generally in this name
            // qualification support).
            SgInitializedName *currentVariableDeclarationInitializedName =
                SageInterface::getFirstInitializedName(variableDeclaration);
            ASSERT_not_null(currentVariableDeclarationInitializedName);

            // DQ (4/7/2014): Reset the symbol to NULL (snce we didn't find an
            // associated SgVariableSymbol). In the switch statement (below) we
            // will use the declaration to obtain the correct symbol and then
            // compare if we have found the correct one using the name lookup
            // through parent scopes.  Then we will beable to know if name
            // qualification is required. If we use the declaration to find the
            // symbol here, then we will detect that no name qualification is
            // required (where it might be).  Testcode test2014_39.C
            // demonstrates this issue.
            symbol = NULL;
            // DQ (6/5/2011): This assert fails for test2005_107.C (this is OK,
            // the referenced symbol is not visible from the current scope).
            // ASSERT_not_null(symbol);
          }
          // ASSERT_not_null(symbol);
          // MLOG_WARN_C(MLOG_UNPARSER, "Lookup symbol based symbol type: reset
          // symbol = %p = %s \n",symbol,symbol->class_name().c_str());
        }

        break;
      }

      case V_SgNonrealDecl: {
        ASSERT_not_null(symbol);
        if (!isSgNonrealSymbol(symbol)) {
          symbol = SageInterface::lookupNonrealSymbolInParentScopes(
              name, currentScope, templateParameterList, templateArgumentList);
        }

        break;
      }

      case V_SgProcedureHeaderStatement: {
        return 0;
        break;
      }

      default: {
        // Handle cases are we work through specific example codes.
        // MLOG_WARN_C(MLOG_UNPARSER, "default reached symbol = %s
        // \n",symbol->class_name().c_str());
        MLOG_WARN_C(MLOG_UNPARSER, "default reached declaration = %p = %s \n",
                    declaration, declaration->class_name().c_str());
        ROSE_ABORT();
      }
      }
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "SageInterface::lookupSymbolInParentScopes("
                                 "name,currentScope) returned NULL \n");
#endif
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "22222222222222222222222222222222222222222222222"
                               "22222222222222222222 \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Calling evaluateTemplateInstantiationDeclaration() from "
                "nameQualificationDepth() declaration = %p = %s currentScope = "
                "%p = %s \n",
                declaration, declaration->class_name().c_str(), currentScope,
                currentScope->class_name().c_str());
#endif

    // Refactored this code to another member function so that it could also
    // support evaluation of declarations found in types (more generally).
    evaluateTemplateInstantiationDeclaration(declaration, currentScope,
                                             positionStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "DONE: Calling evaluateTemplateInstantiationDeclaration() from "
                "nameQualificationDepth() declaration = %p = %s = %s "
                "currentScope = %p = %s \n",
                declaration, declaration->class_name().c_str(),
                SageInterface::get_name(declaration).c_str(), currentScope,
                currentScope->class_name().c_str());
    MLOG_WARN_C(MLOG_UNPARSER, "===== declaration->unparseToString() = %s \n",
                declaration->unparseToString().c_str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::nameQualificationDepth(): "
                "symbol = %p \n",
                symbol);
    if (symbol != NULL) {
      MLOG_WARN_C(MLOG_UNPARSER, "   --- symbol = %s \n",
                  symbol->class_name().c_str());
    }
#endif

    // At this point if there was any ambiguity in the first matching symbol
    // that was found, then we have resolved this to the correct type of symbol
    // (SgClassSymbol, SgFunctionSymbol, etc.). Now we want to resolve it to the
    // exact symbol that matches the declaration.
    if (symbol != NULL) {
      // DQ (5/6/2011): Now we have fixed derived class symbol tables to inject
      // there base classes symbols into the derived class.
      SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);

      // DQ (7/12/2014): The newer design of the symbol table handling means
      // that we will never see a SgAliasSymbol at this level.
      ROSE_ASSERT(aliasSymbol == NULL);
      // DQ (7/12/2014): debugging use of SgAliasSymbol.
      if (aliasSymbol != NULL) {
        // DQ (7/12/2014): The newer design of the symbol table handling means
        // that we will never see a SgAliasSymbol at this level.
        ROSE_ABORT();
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "forceMoreNameQualification = %s \n",
                  forceMoreNameQualification ? "true" : "false");
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0
      printf("In NameQualificationTraversal::nameQualificationDepth(): "
             "forceMoreNameQualification = %s \n",
             forceMoreNameQualification ? "true" : "false");
      printf("In NameQualificationTraversal::nameQualificationDepth(): "
             "declaration = %p = %s name = %s \n",
             declaration, declaration->class_name().c_str(),
             SageInterface::get_name(declaration).c_str());
      printf("In NameQualificationTraversal::nameQualificationDepth(): "
             "currentScope = %p = %s name = %s \n",
             currentScope, currentScope->class_name().c_str(),
             SageInterface::get_name(currentScope).c_str());
      printf("In NameQualificationTraversal::nameQualificationDepth(): "
             "positionStatement = %p = %s name = %s \n",
             positionStatement, positionStatement->class_name().c_str(),
             SageInterface::get_name(positionStatement).c_str());
#endif

      if (forceMoreNameQualification == true) {
        // If there is more than one symbol with the same name then name
        // qualification is required to distinguish between them. The exception
        // to this is overloaded member functions.  But might also be where type
        // evaluation is required.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Found a case of ambiguity (forceMoreNameQualification == "
                    "true) of declaration = %s in the currentScope = %p = %s = "
                    "%s (trigger additional name qualifier). \n",
                    declaration->class_name().c_str(), currentScope,
                    currentScope->class_name().c_str(),
                    SageInterface::get_name(currentScope).c_str());
#endif
        switch (declaration->variantT()) {
        case V_SgFunctionDeclaration: {
          // DQ (7/25/2018): If in the original matching (for name collission)
          // there was a match, then we need to force at least one more level of
          // name qualification for functions since type elaboration can not be
          // used to resolve an ambiguity on function (only makes sense for
          // types).
          qualificationDepth =
              nameQualificationDepthOfParent(declaration, currentScope,
                                             positionStatement) +
              1;
          break;
        }

        case V_SgMemberFunctionDeclaration: {
          // Don't qualify member function defined in their associated class.
          SgMemberFunctionDeclaration *memberFunctionDeclaration =
              isSgMemberFunctionDeclaration(declaration);
          SgScopeStatement *structurallyAssociatedScope =
              isSgScopeStatement(memberFunctionDeclaration->get_parent());
          ASSERT_not_null(structurallyAssociatedScope);

          // Note that structurallyAssociatedDeclaration could be NULL if the
          // function declaration is in global scope.
          SgDeclarationStatement *structurallyAssociatedDeclaration =
              associatedDeclaration(structurallyAssociatedScope);
          SgDeclarationStatement *semanticallyAssociatedDeclaration =
              memberFunctionDeclaration->get_associatedClassDeclaration();

          ASSERT_not_null(semanticallyAssociatedDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "structurallyAssociatedDeclaration = %p \n",
                      structurallyAssociatedDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "semanticallyAssociatedDeclaration = %p \n",
                      semanticallyAssociatedDeclaration);
#endif
          if (structurallyAssociatedDeclaration !=
              semanticallyAssociatedDeclaration) {
            // The associated class for the member function does not match its
            // structural location so we require name qualification.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "The associated class for the member function does not match "
                "its structural location so we require name qualification \n");
#endif
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "structurallyAssociatedDeclaration == "
                "semanticallyAssociatedDeclaration: qualificationDepth = %d \n",
                qualificationDepth);
#endif
            // DQ (4/27/2019): We need to force a level of qualification (I
            // think). DQ (4/28/2019): This does fix the ctor preinitialization
            // list name qualification bug (represented by test2019_415.C).
            qualificationDepth += 1;

            // DQ (4/28/2019): We can see that this is called twice, once from
            // the SgInitialization's initializer, and once from the
            // SgConstructorInitializer.
          }

          break;
        }

        case V_SgEnumDeclaration: {
          // An Enum can have a tag and it will be the it scope and trigger
          // unwanted name qualification. SgEnumDeclaration* enumDeclaration =
          // isSgEnumDeclaration(declaration);

          // I think what we want to do is recognize when there enum declaration
          // is declared directly in the typedef. We now make sure that name
          // qualification is not called in this case, so we should not reach
          // this point!
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Error: Skipping forced name qualification for enum "
                      "types (sorry, not implemented) \n");
#endif
          // DQ (5/30/2019): If we are forcing name qualification then I think
          // we need to increment this variable. See test2019_448.C for an
          // example of where this is needed.
          qualificationDepth += 1;
          // We do reach this point in test2004_105.C
          // ROSE_ABORT();

          break;
        }

        default: {
          // But we have to keep moving up the chain of scopes to see if the
          // parent might also require qualification.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "We are forcing the name qualification so continue to "
                      "resolve the name qualification depth... \n");
#endif
          qualificationDepth =
              nameQualificationDepthOfParent(declaration, currentScope,
                                             positionStatement) +
              1;
        }
        }
      } else {
        // The numberOfSymbols can be zero or one, because the symbol might not
        // be the the current scope. If it is zero then it just means that the
        // name is visible from the current scope by is not located in the
        // current scope.  If it is one, then there is a symbol matching the
        // name and we need to check if it is associated with the same
        // declaration or not.

        // However, since symbol != NULL, the numberOfSymbols should be
        // non-zero. ROSE_ASSERT(numberOfSymbols > 0);

        // DQ (7/12/2014): The newer design of the symbol table handling means
        // that we will never see a SgAliasSymbol at this level.
        ROSE_ASSERT(aliasSymbol == NULL);

        // Not clear if we want to resolve this to another scope since the alias
        // symbols scope is want might have to be qualified (not the scope of
        // the aliased declaration).
        if (aliasSymbol != NULL) {
          // DQ (7/12/2014): The newer design of the symbol table handling means
          // that we will never see a SgAliasSymbol at this level.
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "ERROR: The newer design of the symbol table handling means that "
              "we will never see a SgAliasSymbol at this level \n");
          ROSE_ABORT();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Resetting the symbol to that stored in the SgAliasSymbol \n");
#endif
          symbol = aliasSymbol->get_alias();

          // DQ (7/23/2011): If we can't assert this, then we need to loop
          // through the chain of alias symbols to get to the non-alias
          // (original) symbol.
          ROSE_ASSERT(isSgAliasSymbol(symbol) == NULL);
        }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(MLOG_UNPARSER, "AT SWITCH: symbol = %p = %s \n", symbol,
                    symbol->class_name().c_str());
#endif
        switch (symbol->variantT()) {
          // DQ (12/27/2011): Added support for template class symbols.
        case V_SgTemplateClassSymbol:
        case V_SgClassSymbol: {
          SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
          ASSERT_not_null(classSymbol);

          // This is a class symbol, check if the declaration is the same.
          // SgClassDeclaration* associatedClassDeclaration =
          // baseClass->get_base_class();
          SgClassDeclaration *associatedClassDeclaration =
              classSymbol->get_declaration();

          // DQ (7/21/2024): This can be a namespaceDeclaration (for the case of
          // processing the nlohmann/json header file).
          // ASSERT_not_null(classDeclaration);
          ASSERT_not_null(associatedClassDeclaration);

          bool same_declaration = false;
          if (classDeclaration != NULL) {
            same_declaration =
                (associatedClassDeclaration
                     ->get_firstNondefiningDeclaration() ==
                 classDeclaration->get_firstNondefiningDeclaration());
          }
          if (same_declaration == false) {
            SgTemplateInstantiationDecl *templateInstantiationDecl =
                isSgTemplateInstantiationDecl(classDeclaration);
            if (templateInstantiationDecl != NULL) {
              SgTemplateClassDeclaration *templateDecl =
                  templateInstantiationDecl->get_templateDeclaration();
              if (templateDecl != NULL) {
                same_declaration =
                    (associatedClassDeclaration
                         ->get_firstNondefiningDeclaration() ==
                     templateDecl->get_firstNondefiningDeclaration());
              }
            }
          }

          if (same_declaration == true) {
            // #if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            // DQ (1/4/2020): This is the better implementation and it should be
            // isolated into a separate function so that we can call it from the
            // case V_SgTypedefSymbol and case V_SgEnumSymbol (and maybe some
            // other locations as well (generating more test codes to drive this
            // would be helpful).

            // DQ (1/4/2020): Need to check if there is an opportunity for an
            // ambigous reference. size_t numberOfAliasSymbols =
            // currentScope->count_alias_symbol(name); symbol =
            // SageInterface::lookupTemplateSymbolInParentScopes(name,currentScope);
            // SgScopeStatement* scopeOfAssociatedTypedefDeclaration =
            // associatedTypedefDeclaration->get_scope();
            // ASSERT_not_null(scopeOfAssociatedTypedefDeclaration);
            // size_t numberOfAliasSymbols =
            // scopeOfAssociatedTypedefDeclaration->count_alias_symbol(name);
            bool includeCurrentScope = true;
            SgScopeStatement *ambiguityScope = currentScope;
            if (SgClassDefinition *current_classDefinition =
                    SageInterface::getEnclosingNode<SgClassDefinition>(
                        currentScope, includeCurrentScope)) {
              ambiguityScope = current_classDefinition;
            }
            ASSERT_not_null(ambiguityScope);

            bool has_scope_ambiguity =
                ambiguityScope->hasAmbiguity(name, symbol);
            bool imported_using_directive_conflict =
                imported_by_prior_using_directive(declaration);
            if (has_scope_ambiguity == true ||
                imported_using_directive_conflict == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "   --- qualificationDepth = %d \n",
                          qualificationDepth);
#endif
            }

          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            // The name does not match, so the associatedClassDeclaration is
            // hidding the base class declaration.
            MLOG_WARN_C(MLOG_UNPARSER,
                        "This class is NOT visible from where it is referenced "
                        "(declaration with same name does not match) \n");
            MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope      = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                        positionStatement,
                        positionStatement->class_name().c_str());
#endif
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgNamespaceSymbol: {
          SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);
          ASSERT_not_null(namespaceSymbol);
          SgNamespaceDeclarationStatement *associatedNamespaceDeclaration =
              namespaceSymbol->get_declaration();
          SgNamespaceAliasDeclarationStatement
              *associatedNamespaceAliasDeclaration =
                  namespaceSymbol->get_aliasDeclaration();

          // DQ (4/9/2018): Adding support for namespace alias.
          // ASSERT_not_null(namespaceDeclaration);
          ROSE_ASSERT(namespaceDeclaration != NULL ||
                      namespaceAliasDeclaration != NULL);

          // DQ (4/9/2018): Adding support for namespace alias.
          // ASSERT_not_null(associatedNamespaceDeclaration);
          ROSE_ASSERT(associatedNamespaceDeclaration != NULL ||
                      associatedNamespaceAliasDeclaration != NULL);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
          MLOG_WARN_C(MLOG_UNPARSER,
                      "namespaceDeclaration                = %p \n",
                      namespaceDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "namespaceAliasDeclaration           = %p \n",
                      namespaceAliasDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "associatedNamespaceDeclaration      = %p \n",
                      associatedNamespaceDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "associatedNamespaceAliasDeclaration = %p \n",
                      associatedNamespaceAliasDeclaration);
#endif
          // if
          // (associatedNamespaceDeclaration->get_firstNondefiningDeclaration()
          // == namespaceDeclaration->get_firstNondefiningDeclaration())
          if (associatedNamespaceDeclaration != NULL &&
              namespaceDeclaration != NULL &&
              associatedNamespaceDeclaration
                      ->get_firstNondefiningDeclaration() ==
                  namespaceDeclaration->get_firstNondefiningDeclaration()) {
            // This class is visible from where it is referenced.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This namespace IS visible from where it is referenced \n");
#endif
          } else {
            // DQ (4/9/2018): Added support for namespace alias.
            if (associatedNamespaceAliasDeclaration != NULL &&
                namespaceAliasDeclaration != NULL &&
                associatedNamespaceAliasDeclaration
                        ->get_firstNondefiningDeclaration() ==
                    namespaceAliasDeclaration
                        ->get_firstNondefiningDeclaration()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "This namespace alias IS visible from "
                                         "where it is referenced \n");
#endif
            } else {
              // The name does not match, so the associatedClassDeclaration is
              // hidding the base class declaration.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "This namespace is NOT visible from where it is referenced "
                  "(declaration with same name does not match) \n");
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- currentScope      = %p = %s \n", currentScope,
                          currentScope->class_name().c_str());
              MLOG_WARN_C(
                  MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                  positionStatement, positionStatement->class_name().c_str());
#endif
              // Now resolve how much name qualification is required; what ever
              // is required for the parent plus 1.
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;
            }
          }

          break;
        }

        case V_SgTemplateVariableSymbol:
        case V_SgVariableSymbol: {
          SgVariableSymbol *variableSymbol = isSgVariableSymbol(symbol);
          ASSERT_not_null(variableSymbol);

          // This is a variable symbol, check if the declaration is the same.
          // SgVariableDeclaration* associatedVariableDeclaration =
          // variableSymbol->get_declaration();
          SgInitializedName *associatedInitializedName =
              variableSymbol->get_declaration();

          ASSERT_not_null(variableDeclaration);
          ASSERT_not_null(associatedInitializedName);

          // if (associatedInitializedName->get_firstNondefiningDeclaration() ==
          // variableDeclaration->get_firstNondefiningDeclaration())
          if (associatedInitializedName ==
              SageInterface::getFirstInitializedName(variableDeclaration)) {
            // This variable is visible from where it is referenced.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This variable IS visible from where it is referenced \n");
#endif
            // DQ (12/23/2015): Need to check if there is an opportunity for an
            // ambigous reference.
            size_t numberOfAliasSymbols =
                currentScope->count_alias_symbol(name);
            if (numberOfAliasSymbols >= 2) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "   --- qualificationDepth = %d \n",
                          qualificationDepth);
#endif
            }
          } else {
            // The name does not match, so the associatedClassDeclaration is
            // hidding the base class declaration.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This variable is NOT visible from where it is referenced "
                "(declaration with same name does not match) \n");
            MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope      = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                        positionStatement,
                        positionStatement->class_name().c_str());
#endif
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

          // DQ (12/28/2011): Added support for new template handling in the
          // AST.
        case V_SgTemplateMemberFunctionSymbol:
        case V_SgTemplateFunctionSymbol:

        case V_SgMemberFunctionSymbol:
        case V_SgFunctionSymbol: {
          SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
          ASSERT_not_null(functionSymbol);

          ASSERT_not_null(functionDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Case of SgFunctionSymbol or SgMemberFunctionSymbol: "
                      "functionSymbol = %p = %s \n",
                      functionSymbol, functionSymbol->class_name().c_str());
          MLOG_WARN_C(
              MLOG_UNPARSER, "   --- functionDeclaration = %p = %s = %s \n",
              functionDeclaration, functionDeclaration->class_name().c_str(),
              functionDeclaration->get_name().str());
#endif
          // This is a function symbol, check if the declaration is the same.
          // SgFunctionDeclaration* associatedFunctionDeclaration =
          // functionSymbol->get_declaration();
          SgFunctionDeclaration *associatedFunctionDeclarationFromSymbol =
              functionSymbol->get_declaration();
          ASSERT_not_null(associatedFunctionDeclarationFromSymbol);

          auto canonical_function_declaration =
              [](SgFunctionDeclaration *decl) -> SgFunctionDeclaration * {
            if (decl == NULL) {
              return NULL;
            }

            SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                decl->get_firstNondefiningDeclaration());
            if (first_nondef != NULL) {
              return first_nondef;
            }

            SgFunctionDeclaration *defining_decl =
                isSgFunctionDeclaration(decl->get_definingDeclaration());
            if (defining_decl != NULL) {
              return defining_decl;
            }

            return decl;
          };
          SgFunctionDeclaration *canonicalAssociatedFunctionDeclaration =
              canonical_function_declaration(
                  associatedFunctionDeclarationFromSymbol);
          ASSERT_not_null(canonicalAssociatedFunctionDeclaration);

          ASSERT_not_null(functionDeclaration);

          // DQ (11/19/2013): This is added to support testing cases where we
          // would clearly fail the AST consistancy tests (e.g.
          // LoopProcessing.C). I hate this work around, but I am hoping it will
          // help identify a root cause of the problem.
          // ASSERT_not_null(functionDeclaration->get_firstNondefiningDeclaration());
          if (functionDeclaration->get_firstNondefiningDeclaration() == NULL) {
            MLOG_WARN_C(MLOG_UNPARSER,
                        "***** ERROR: In "
                        "NameQualificationTraversal::nameQualificationDepth(): "
                        "we are supporting this case though it is a violation "
                        "of the AST consistancy tests! ***** \n");
            return 0;
          }

          // DQ (11/18/2013): This is an assertion inside of
          // get_declaration_associated_with_symbol() which we are now failing.
          ROSE_ASSERT(functionDeclaration->get_firstNondefiningDeclaration() ==
                      functionDeclaration->get_firstNondefiningDeclaration()
                          ->get_firstNondefiningDeclaration());

          SgDeclarationStatement *declarationFromSymbol =
              functionDeclaration->get_declaration_associated_with_symbol();
          // DQ (11/18/2013): Try to reset this...
          if (declarationFromSymbol == NULL) {
            declarationFromSymbol =
                functionDeclaration->get_firstNondefiningDeclaration()
                    ->get_declaration_associated_with_symbol();
          }
          auto is_suppressed_instantiation_placeholder =
              [&](SgFunctionDeclaration *decl) -> bool {
            if (decl == NULL) {
              return false;
            }
            if (isSgTemplateInstantiationFunctionDecl(decl) == NULL &&
                isSgTemplateInstantiationMemberFunctionDecl(decl) == NULL) {
              return false;
            }

            auto output_enabled = [](SgLocatedNode *node) -> bool {
              return node != NULL && node->get_file_info() != NULL &&
                     node->get_file_info()->isOutputInCodeGeneration();
            };

            if (!output_enabled(decl)) {
              return true;
            }

            SgTemplateInstantiationDirectiveStatement *directive =
                isSgTemplateInstantiationDirectiveStatement(decl->get_parent());
            return directive != NULL && !output_enabled(directive);
          };
          if (declarationFromSymbol == NULL &&
              is_suppressed_instantiation_placeholder(functionDeclaration)) {
            return 0;
          }
          SgFunctionDeclaration *functionDeclarationFromSymbol =
              canonical_function_declaration(
                  isSgFunctionDeclaration(declarationFromSymbol));
          if (functionDeclarationFromSymbol == NULL) {
            functionDeclarationFromSymbol =
                canonicalAssociatedFunctionDeclaration;
          }
          ASSERT_not_null(functionDeclarationFromSymbol);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "associatedFunctionDeclarationFromSymbol                 = %p = "
              "%s \n",
              associatedFunctionDeclarationFromSymbol,
              associatedFunctionDeclarationFromSymbol->class_name().c_str());
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "--- associatedFunctionDeclarationFromSymbol->get_name() = %s \n",
              associatedFunctionDeclarationFromSymbol->get_name().str());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "functionDeclarationFromSymbol                           "
                      "= %p = %s \n",
                      functionDeclarationFromSymbol,
                      functionDeclarationFromSymbol->class_name().c_str());
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "--- functionDeclarationFromSymbol->get_name()           = %s \n",
              functionDeclarationFromSymbol->get_name().str());

          MLOG_WARN_C(MLOG_UNPARSER,
                      "associatedFunctionDeclarationFromSymbol->get_"
                      "firstNondefiningDeclaration() = %p \n",
                      associatedFunctionDeclarationFromSymbol
                          ->get_firstNondefiningDeclaration());
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "functionDeclarationFromSymbol->get_firstNondefiningDeclaration()"
              "           = %p \n",
              functionDeclarationFromSymbol->get_firstNondefiningDeclaration());
          if (associatedFunctionDeclarationFromSymbol
                  ->get_firstNondefiningDeclaration() == NULL) {
            // DQ (6/22/2011): This is the case when a function has only a
            // defining declaration and in this case no nondefining declaration
            // is built for a function. This is true for both
            // SgFunctionDeclaration and SgMemberFunctionDeclaration handling
            // (but may change to be more uniform with other declarations in the
            // future).
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Found a valid function with get_firstNondefiningDeclaration() "
                "== NULL (not a problem, just a special case)\n");
          }
#endif

          // if
          // (associatedFunctionDeclaration->get_firstNondefiningDeclaration()
          // == functionDeclaration->get_firstNondefiningDeclaration())
          if (canonicalAssociatedFunctionDeclaration ==
              functionDeclarationFromSymbol) {
            // DQ (4/12/2014): Now we know that it can be found, but we still
            // need to check if there would be another function that could be
            // used and for which we need name qualification to avoid.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0
            printf("Using foundAnOverloadedFunctionWithSameName = %s \n",
                   foundAnOverloadedFunctionWithSameName ? "true" : "false");
            printf("Using foundAnOverloadedFunctionInSameScope  = %s \n",
                   foundAnOverloadedFunctionInSameScope ? "true" : "false");
#endif
            // DQ (4/12/2014): We need to use the recorded value
            // foundAnOverloadedFunctionWithSameName because we may want to have
            // force name qualification.
            if (foundAnOverloadedFunctionWithSameName == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "There was another function identified in the process of "
                  "resolving that this function could be found. thus we will "
                  "require some name qualification \n");
#endif
              if (foundAnOverloadedFunctionInSameScope == false) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL >= 1)
                MLOG_WARN_C(
                    MLOG_UNPARSER,
                    "In name qualification support: case V_SgFunctionSymbol: "
                    "We need to compute the CORRECT name qualification depth: "
                    "using 1 for now! \n");
#endif
                qualificationDepth = 1;
              }
            }
            // DQ (6/20/2011): But we don't check for if there was another
            // declaration that might be a problem (overloaded functions don't
            // count!)... This function is visible from where it is referenced.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "This function or member function IS visible from "
                        "where it is referenced (but there could still be "
                        "ambiguity if this was just the first of several "
                        "symbols found in the current scope) \n");
            MLOG_WARN_C(MLOG_UNPARSER,
                        "   --- currentScope        = %p = %s \n", currentScope,
                        currentScope->class_name().c_str());
            MLOG_WARN_C(
                MLOG_UNPARSER, "   --- functionDeclaration = %p = %s \n",
                functionDeclaration, functionDeclaration->class_name().c_str());

            // But we need to check if there is another such symbol in the same
            // scope that would trigger qualification. SgScopeStatement*
            // associatedScope = associatedFunctionDeclaration->get_scope();
            // ASSERT_not_null(associatedScope);
            // MLOG_WARN_C(MLOG_UNPARSER, "Searching associatedScope = %p = %s
            // \n",associatedScope,associatedScope->class_name().c_str());
            SgClassDefinition *classDefinition =
                isSgClassDefinition(functionDeclaration->get_parent());
            if (classDefinition != NULL) {
              MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s \n",
                          currentScope, currentScope->class_name().c_str());
              MLOG_WARN_C(MLOG_UNPARSER, "Searching classDefinition = %p \n",
                          classDefinition);

              // int numberOfSymbolsWithMatchingName =
              // numberOfSymbolsWithName(name,associatedScope);
              int numberOfSymbolsWithMatchingName =
                  numberOfSymbolsWithName(name, classDefinition);
              MLOG_WARN_C(MLOG_UNPARSER,
                          "numberOfSymbolsWithMatchingName = %d \n",
                          numberOfSymbolsWithMatchingName);

              // ROSE_ASSERT(numberOfSymbolsWithMatchingName == 1);
            }
#endif
          } else {
            // The name does not match, so the associatedFunctionDeclaration is
            // hidding the function declaration.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This function or member function is NOT visible from where it "
                "is referenced (declaration with same name does not match) \n");
            MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope      = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                        positionStatement,
                        positionStatement->class_name().c_str());
#endif
            SgName functionDeclarationFromSymbol_mangled_name =
                functionDeclarationFromSymbol->get_mangled_name();
            SgName associatedFunctionDeclarationFromSymbol_mangled_name =
                canonicalAssociatedFunctionDeclaration->get_mangled_name();

            // DQ (4/2/2018): Check the names to see if they could be the same
            // (possible error checking).
            if (canonicalAssociatedFunctionDeclaration->get_name() ==
                functionDeclarationFromSymbol->get_name()) {
              // DQ (4/7/2018): I think we can assert this (this fails for
              // Cxx_tests/test2017_29.C).
              // ROSE_ASSERT(functionDeclarationFromSymbol_mangled_name !=
              // associatedFunctionDeclarationFromSymbol_mangled_name);
            }

            // SgName functionDeclarationFromSymbol_mangled_name           =
            // functionDeclarationFromSymbol->get_mangled_name(); SgName
            // associatedFunctionDeclarationFromSymbol_mangled_name =
            // associatedFunctionDeclarationFromSymbol->get_mangled_name(); if
            // (associatedFunctionDeclarationFromSymbol->get_name() ==
            // functionDeclarationFromSymbol->get_name())
            if (associatedFunctionDeclarationFromSymbol_mangled_name ==
                functionDeclarationFromSymbol_mangled_name) {
            }

            // DQ (4/7/2018): I think we can also assert this (unless the
            // function parameters would make the difference)!
            // ROSE_ASSERT(associatedFunctionDeclarationFromSymbol->get_name()
            // != functionDeclarationFromSymbol->get_name()); DQ (4/7/2018):
            // Only increment the name qualification depth if these are the same
            // function. int increment = 1; DQ (4/7/2018): This must be set to
            // 1, and Cxx_tests/test2011_39.C demonstrates this.
            int increment = 1;
            // DQ (4/8/2014): If the type match then use an increment of 1, else
            // don't increment the qualificationDepth. Now resolve how much name
            // qualification is required; what ever is required for the parent
            // plus 1. qualificationDepth =
            // nameQualificationDepthOfParent(declaration,currentScope,positionStatement)
            // + 1;
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                increment;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
            MLOG_WARN_C(MLOG_UNPARSER, "   --- qualificationDepth = %d \n",
                        qualificationDepth);
#endif
          }
          break;
        }

          // DQ (11/10/2014): Adding support for templated typedef declarations.
        case V_SgTemplateTypedefSymbol:

        case V_SgTypedefSymbol: {
          SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
          ASSERT_not_null(typedefSymbol);

          // This is a typdef symbol, check if the declaration is the same.
          SgTypedefDeclaration *associatedTypedefDeclaration =
              typedefSymbol->get_declaration();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "associatedTypedefDeclaration = %p = %s \n",
                      associatedTypedefDeclaration,
                      associatedTypedefDeclaration->get_name().str());
#endif
          ASSERT_not_null(typedefDeclaration);
          ASSERT_not_null(associatedTypedefDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "associatedTypedefDeclaration->get_firstNondefiningDeclaration() "
              "= %p \n",
              associatedTypedefDeclaration->get_firstNondefiningDeclaration());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "typedefDeclaration->get_firstNondefiningDeclaration()   "
                      "        = %p \n",
                      typedefDeclaration->get_firstNondefiningDeclaration());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "currentScope                                            "
                      "        = %p = %s \n",
                      currentScope, currentScope->class_name().c_str());
#endif
          if (associatedTypedefDeclaration->get_firstNondefiningDeclaration() ==
              typedefDeclaration->get_firstNondefiningDeclaration()) {
            // This typedef is visible from where it is referenced.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This typedef IS visible from where it is referenced \n");
#endif
            // DQ (2/8/2019): If type elaboration was required, and the symbol
            // is from a base class. Then we need name qualification because the
            // type elaboration will not protect the type reference from being
            // hidden.
            if (typeElaborationIsRequired == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- Since type elaboration was required because "
                          "it was hidden, add name qualification to support it "
                          "being unambiguous \n");
#endif
              qualificationDepth = 1;
            }
            // DQ (1/4/2020): Need to check if there is an opportunity for an
            // ambigous reference. size_t numberOfAliasSymbols =
            // currentScope->count_alias_symbol(name); symbol =
            // SageInterface::lookupTemplateSymbolInParentScopes(name,currentScope);
            // SgScopeStatement* scopeOfAssociatedTypedefDeclaration =
            // associatedTypedefDeclaration->get_scope();
            // ASSERT_not_null(scopeOfAssociatedTypedefDeclaration);
            // size_t numberOfAliasSymbols =
            // scopeOfAssociatedTypedefDeclaration->count_alias_symbol(name);
            bool includeCurrentScope = true;
            SgScopeStatement *ambiguityScope = currentScope;
            if (SgClassDefinition *current_classDefinition =
                    SageInterface::getEnclosingNode<SgClassDefinition>(
                        currentScope, includeCurrentScope)) {
              ambiguityScope = current_classDefinition;
            }
            ASSERT_not_null(ambiguityScope);

            if (ambiguityScope->hasAmbiguity(name, symbol) == true ||
                imported_by_prior_using_directive(declaration) == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "   --- qualificationDepth = %d \n",
                          qualificationDepth);
#endif
            }
          } else {
            // The name does not match, so the associatedFunctionDeclaration is
            // hidding the base class declaration.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This typedef is NOT visible from where it is referenced "
                "(declaration with same name does not match) \n");
            MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope      = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                        positionStatement,
                        positionStatement->class_name().c_str());
#endif
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgTemplateSymbol: {
          SgTemplateSymbol *templateSymbol = isSgTemplateSymbol(symbol);
          ASSERT_not_null(templateSymbol);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "WARNING: Support for name qualification for "
              "SgTemplateInstantiationFunctionDecl is not implemented yet \n");
#endif
          break;
        }

        case V_SgEnumSymbol: {
          SgEnumSymbol *enumSymbol = isSgEnumSymbol(symbol);
          ASSERT_not_null(enumSymbol);

          // This is a typdef symbol, check if the declaration is the same.
          SgEnumDeclaration *associatedEnumDeclaration =
              enumSymbol->get_declaration();

          ASSERT_not_null(enumDeclaration);
          ASSERT_not_null(associatedEnumDeclaration);

          if (associatedEnumDeclaration->get_firstNondefiningDeclaration() ==
              enumDeclaration->get_firstNondefiningDeclaration()) {
            // This class is visible from where it is referenced.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "This enum IS visible from where it is referenced \n");
#endif
            // DQ (2/8/2019): If type elaboration was required, and the symbol
            // is from a base class. Then we need name qualification because the
            // type elaboration will not protect the type reference from being
            // hidden.
            if (typeElaborationIsRequired == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- Since type elaboration was required because "
                          "it was hidden, add name qualification to support it "
                          "being unambiguous \n");
#endif
              // DQ (2/14/2019): If this is a typedef that is hidden by a
              // variable then we don't require extra name qualification.
              if (isSgVariableSymbol(original_symbol_lookedup_by_name) !=
                  NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
                MLOG_WARN_C(
                    MLOG_UNPARSER,
                    "This enum IS visible and variables can't hide types, so "
                    "no extra name qualification is required \n");
#endif
              } else {
                qualificationDepth = 1;
              }
            }
            // DQ (1/4/2020): Need to check if there is an opportunity for an
            // ambigous reference. size_t numberOfAliasSymbols =
            // currentScope->count_alias_symbol(name); symbol =
            // SageInterface::lookupTemplateSymbolInParentScopes(name,currentScope);
            // SgScopeStatement* scopeOfAssociatedTypedefDeclaration =
            // associatedTypedefDeclaration->get_scope();
            // ASSERT_not_null(scopeOfAssociatedTypedefDeclaration);
            // size_t numberOfAliasSymbols =
            // scopeOfAssociatedTypedefDeclaration->count_alias_symbol(name);
            bool includeCurrentScope = true;
            SgScopeStatement *ambiguityScope = currentScope;
            if (SgClassDefinition *current_classDefinition =
                    SageInterface::getEnclosingNode<SgClassDefinition>(
                        currentScope, includeCurrentScope)) {
              ambiguityScope = current_classDefinition;
            }
            ASSERT_not_null(ambiguityScope);

            if (ambiguityScope->hasAmbiguity(name, symbol) == true ||
                imported_by_prior_using_directive(declaration) == true) {
              qualificationDepth =
                  nameQualificationDepthOfParent(declaration, currentScope,
                                                 positionStatement) +
                  1;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "   --- qualificationDepth = %d \n",
                          qualificationDepth);
#endif
            }
          } else {
            // The name does not match, so the associatedFunctionDeclaration is
            // hidding the base class declaration.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "This enum is NOT visible from where it is referenced "
                        "(declaration with same name does not match) \n");
            MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope      = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                        positionStatement,
                        positionStatement->class_name().c_str());
#endif
            // Now resolve how much name qualification is required; what ever is
            // required for the parent plus 1.
            qualificationDepth =
                nameQualificationDepthOfParent(declaration, currentScope,
                                               positionStatement) +
                1;
          }

          break;
        }

        case V_SgNonrealSymbol: {
#if WARNING_FOR_NONREAL_DEVEL
          MLOG_WARN_C(MLOG_UNPARSER,
                      "WARNING: Support for name qualification depth for "
                      "SgNonrealSymbol is not implemented yet \n");
#endif
          break;
        }

        default: {
          // Handle cases are we work through specific example codes.
          MLOG_WARN_C(MLOG_UNPARSER, "default reached symbol = %s \n",
                      symbol->class_name().c_str());
          ROSE_ABORT();
        }
        }
      }

    } else {
      // DQ (4/2/2018): This is the predicate for this false branch.
      ROSE_ASSERT(symbol == NULL);

      // This class is visible from where it is referenced.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "\n\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                                 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      MLOG_WARN_C(MLOG_UNPARSER, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                                 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      MLOG_WARN_C(MLOG_UNPARSER, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                                 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      MLOG_WARN_C(MLOG_UNPARSER, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
                                 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "This declaration = %p = %s is NOT visible from where it is "
                  "referenced (no declaration with same name, calling "
                  "nameQualificationDepthOfParent()) \n",
                  declaration, declaration->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "   --- positionStatement = %p = %s \n",
                  positionStatement, positionStatement->class_name().c_str());

      MLOG_WARN_C(MLOG_UNPARSER, "Calling nameQualificationDepthOfParent() \n");
#endif
      qualificationDepth = nameQualificationDepthOfParent(
                               declaration, currentScope, positionStatement) +
                           1;
    }
  }

  // DQ (12/10/2016): Eliminating a warning that we want to be an error:
  // -Werror=unused-but-set-variable. DQ (12/10/2016): Debugging information
  // that makes sure that typeElaborationIsRequired is used and so will not
  // generate a warning. This is a variable that is essential for internal
  // debugging so we certainly don't want to eliminate it.
  if (typeElaborationIsRequired == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Note that typeElaborationIsRequired == true \n");
#endif
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::nameQualificationDepth(): "
              "qualificationDepth = %d Report type elaboration: "
              "typeElaborationIsRequired = %s \n",
              qualificationDepth,
              (typeElaborationIsRequired == true) ? "true" : "false");
#endif

  return qualificationDepth;
}

SgDeclarationStatement *
NameQualificationTraversal::getDeclarationAssociatedWithType(SgType *type) {
  // Note that this function could be eliminated since it only wraps another
  // function.

  ASSERT_not_null(type);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In getDeclarationAssociatedWithType(): type = %s \n",
              type->class_name().c_str());
#endif

  // DQ (4/15/2019): Strip away any wrapped types (e.g. pointers and
  // references). Note: SgPointerMemberType is processed explicitly. SgType*
  // strippedType =
  // type->stripType(SgType::STRIP_MODIFIER_TYPE|SgType::STRIP_REFERENCE_TYPE|SgType::STRIP_RVALUE_REFERENCE_TYPE|SgType::STRIP_POINTER_TYPE|SgType::STRIP_ARRAY_TYPE);
  SgType *strippedType = type->stripType(
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE);
  ASSERT_not_null(strippedType);

  // Use the stripped type to evaluate the associated declaration.
  type = strippedType;

  // DQ (4/28/2019): Note that this function calls stripType(), so it's use
  // above is redundant.
  SgDeclarationStatement *declaration = associatedDeclaration(type);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In getDeclarationAssociatedWithType(): declaration = %p \n",
              declaration);
#endif

  // Primative types will not have an asociated declaration...
  // ASSERT_not_null(declaration);
  if (declaration == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In getDeclarationAssociatedWithType(): declaration == NULL "
                "type = %s \n",
                type->class_name().c_str());
#endif

  } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In getDeclarationAssociatedWithType(): declaration            "
                "                        = %p = %s \n",
                declaration, SageInterface::get_name(declaration).c_str());
    MLOG_WARN_C(MLOG_UNPARSER,
                "In getDeclarationAssociatedWithType(): "
                "declaration->get_firstNondefiningDeclaration() = %p \n",
                declaration->get_firstNondefiningDeclaration());
    MLOG_WARN_C(MLOG_UNPARSER,
                "In getDeclarationAssociatedWithType(): "
                "declaration->get_definingDeclaration()         = %p \n",
                declaration->get_definingDeclaration());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    if (declaration != declaration->get_firstNondefiningDeclaration()) {
      // Output some debug information to learn more about this error
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "In getDeclarationAssociatedWithType(): declaration = %p = %s \n",
          declaration, declaration->class_name().c_str());
      ASSERT_not_null(declaration->get_file_info());
      declaration->get_file_info()->display("declaration");

      ASSERT_not_null(declaration->get_firstNondefiningDeclaration());
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "In getDeclarationAssociatedWithType(): "
          "declaration->get_firstNondefiningDeclaration() = %p = %s \n",
          declaration->get_firstNondefiningDeclaration(),
          declaration->get_firstNondefiningDeclaration()->class_name().c_str());
      ASSERT_not_null(
          declaration->get_firstNondefiningDeclaration()->get_file_info());
      declaration->get_firstNondefiningDeclaration()->get_file_info()->display(
          "declaration->get_firstNondefiningDeclaration()");
    }
#endif

    // Explicit class specializations can require the defining declaration so
    // hidden declaration synthesis emits the real specialization body instead
    // of reintroducing a detached forward declaration.
    ROSE_ASSERT(declaration == declaration->get_firstNondefiningDeclaration() ||
                declaration == declaration->get_definingDeclaration() ||
                isSgEnumDeclaration(declaration) != NULL);
  }

  return declaration;
}

#define DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS 0

// void evaluateNameQualificationForTemplateArgumentList
// (SgTemplateArgumentPtrList & templateArgumentList, SgScopeStatement*
// currentScope, SgStatement* positionStatement);
void NameQualificationTraversal::
    evaluateNameQualificationForTemplateArgumentList(
        SgTemplateArgumentPtrList &templateArgumentList,
        SgScopeStatement *currentScope, SgStatement *positionStatement) {
  // DQ (6/4/2011): Note that test2005_73.C demonstrate where the Template
  // arguments are shared between template instantiations.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // DQ (9/24/2012): Track the recursive depth in computing name qualification
  // for template arguments of template instantiations used as template
  // arguments.
  static int recursiveDepth = 0;

  // Used for debugging...
  int counter = 0;

  MLOG_WARN_C(MLOG_UNPARSER,
              "\n\n************************************************************"
              "*********************************************************\n");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In "
              "NameQualificationTraversal::"
              "evaluateNameQualificationForTemplateArgumentList(): "
              "templateArgumentList.size() = %" PRIuPTR
              " recursiveDepth = %d \n",
              templateArgumentList.size(), recursiveDepth);
  MLOG_WARN_C(MLOG_UNPARSER,
              "****************************************************************"
              "*****************************************************\n");

  MLOG_WARN_C(MLOG_UNPARSER,
              "In "
              "NameQualificationTraversal::"
              "evaluateNameQualificationForTemplateArgumentList(): "
              "currentScope = %p = %s positionStatement = %p = %s \n",
              currentScope, currentScope->class_name().c_str(),
              positionStatement, positionStatement->class_name().c_str());

  ASSERT_not_null(positionStatement);
  positionStatement->get_file_info()->display(
      "In "
      "NameQualificationTraversal::"
      "evaluateNameQualificationForTemplateArgumentList()");
#endif

  SgScopeStatement *effectiveScope = currentScope;
  bool preserve_unnamed_namespace_scope = false;
  if (positionStatement != nullptr) {
    if (SgScopeStatement *positionScope =
            SageInterface::getScope(positionStatement)) {
      effectiveScope = positionScope;
      if (SgNamespaceDefinitionStatement *position_ns =
              isSgNamespaceDefinitionStatement(positionScope)) {
        if (SgNamespaceDeclarationStatement *position_ns_decl =
                position_ns->get_namespaceDeclaration()) {
          preserve_unnamed_namespace_scope =
              position_ns_decl->get_isUnnamedNamespace();
        }
      }
    }
  }
  if (SgClassDefinition *class_def = isSgClassDefinition(effectiveScope)) {
    if (Sg_File_Info *fi = class_def->get_file_info()) {
      if (fi->isCompilerGenerated()) {
        if (SgClassDeclaration *class_decl = class_def->get_declaration()) {
          if (SgScopeStatement *decl_scope = class_decl->get_scope()) {
            effectiveScope = decl_scope;
          }
        }
      }
    }
  }

  TemplateArgumentListEvaluationKey evaluationKey = {
      &templateArgumentList, effectiveScope, positionStatement};
  if (completedTemplateArgumentListEvaluations.find(evaluationKey) !=
      completedTemplateArgumentListEvaluations.end()) {
    return;
  }
  if (!activeTemplateArgumentListEvaluations.insert(evaluationKey).second) {
    return;
  }

  SgTemplateArgumentPtrList::iterator i = templateArgumentList.begin();
  while (i != templateArgumentList.end()) {
    SgTemplateArgument *templateArgument = *i;
    ASSERT_not_null(templateArgument);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
    MLOG_WARN_C(MLOG_UNPARSER,
                "*** Processing template argument #%d templateArgument = %p \n",
                counter, templateArgument);
    // SgName testNameInMap = templateArgument->get_qualified_name_prefix();
#endif

    // DQ (5/29/2019): Newer version of code (still refactoring this section).
    switch (templateArgument->get_argumentType()) {
    case SgTemplateArgument::type_argument: {
      ASSERT_not_null(templateArgument->get_type());
      SgType *type = templateArgument->get_type();

      ASSERT_not_null(type);

      break;
    }

    case SgTemplateArgument::nontype_argument: {
      // DQ (8/12/2013): This can be either an SgExpression or
      // SgInitializedName. ASSERT_not_null(templateArgument->get_expression());
      ROSE_ASSERT(templateArgument->get_expression() != NULL ||
                  templateArgument->get_initializedName() != NULL);
      ROSE_ASSERT(templateArgument->get_expression() == NULL ||
                  templateArgument->get_initializedName() == NULL);
      if (templateArgument->get_expression() != NULL) {
        SgExpression *expression = templateArgument->get_expression();

        ASSERT_not_null(expression);
      } else {
        SgType *type = templateArgument->get_initializedName()->get_type();
        ASSERT_not_null(type);
        SgInitializedName *iname = templateArgument->get_initializedName();

        ASSERT_not_null(iname);
      }

      break;
    }

    case SgTemplateArgument::template_template_argument: {
      // SgDeclarationStatement * tpldecl =
      // templateArgument->get_templateDeclaration();
      SgDeclarationStatement *decl =
          templateArgument->get_templateDeclaration();
      ASSERT_not_null(decl);

      SgTemplateDeclaration *tpl_decl = isSgTemplateDeclaration(decl);
      ROSE_ASSERT(tpl_decl == NULL);

      break;
    }

    case SgTemplateArgument::start_of_pack_expansion_argument: {
      break;
    }

    case SgTemplateArgument::argument_undefined: {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error argument_undefined in "
                  "evaluateNameQualificationForTemplateArgumentList \n");
      ROSE_ABORT();
      break;
    }

    default: {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error default reached in "
                  "evaluateNameQualificationForTemplateArgumentList \n");
      ROSE_ABORT();
    }
    }

    // DQ (5/29/2019): Older version of code.
    SgType *type = templateArgument->get_type();
    SgExpression *expression = templateArgument->get_expression();
    SgDeclarationStatement *tpldecl =
        templateArgument->get_templateDeclaration();
    SgInitializedName *iname = templateArgument->get_initializedName();
    if (type != NULL) {
      // Reduce the type to the base type stripping off wrappers that would hide
      // the fundamental type inside.
      SgType *strippedType = type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
          SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
          SgType::STRIP_ARRAY_TYPE);
      ASSERT_not_null(strippedType);

      // SgNamedType* namedType = isSgNamedType(type);
      SgNamedType *namedType = isSgNamedType(strippedType);
      if (namedType != NULL) {
        // This could be a type that requires name qualification (reference to a
        // declaration).

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
        MLOG_WARN_C(MLOG_UNPARSER,
                    "templateArgument = %p contains type which is namedType = "
                    "%p = %s \n",
                    templateArgument, namedType,
                    namedType->class_name().c_str());
#endif
        SgDeclarationStatement *templateArgumentTypeDeclaration =
            getDeclarationAssociatedWithType(type);
        if (templateArgumentTypeDeclaration != NULL) {
          // Check the visability and unambiguity of this declaration.
          // Note that since the recursion happens before we set the names, all
          // qualified name are set first at the nested types and then used in
          // the setting of qualified names at the higher level types (less
          // nested types).

          // DQ (5/15/2011): Added recursive handling of template arguments
          // which can require name qualification.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "xxxxxx --- Making a RECURSIVE call to nameQualificationDepth() "
              "on the template argument recursiveDepth = %d \n",
              recursiveDepth);
          // DQ (9/24/2012): I think this is the way to make the recursive call
          // to handle name qualification of template arguments in nexted
          // template instantiations.
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Need to call evaluateNameQualificationForTemplateArgumentList() "
              "on any possible template argument list for type in "
              "templateArgument = %p (namely namedType = %p = %s) \n",
              templateArgument, namedType, namedType->class_name().c_str());
#endif

          SgClassType *classType = isSgClassType(namedType);
          SgNonrealType *nrType = isSgNonrealType(namedType);
          if (classType != NULL) {
            // If this is a class then it should be relative to it's
            // declaration.
            SgClassDeclaration *classDeclaration =
                isSgClassDeclaration(classType->get_declaration());
            ASSERT_not_null(classDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "namedType is a SgClassType: classDeclaration = %p = %s \n",
                classDeclaration, classDeclaration->class_name().c_str());
#endif
            SgTemplateInstantiationDecl *templateClassInstantiationDeclaration =
                isSgTemplateInstantiationDecl(classDeclaration);
            if (templateClassInstantiationDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              recursiveDepth++;
#endif
              evaluateNameQualificationForTemplateArgumentList(
                  templateClassInstantiationDeclaration
                      ->get_templateArguments(),
                  effectiveScope, positionStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              recursiveDepth--;
#endif
            }
          } else if (nrType == NULL) {
            // If not a class then (e.g. typedef) then it is relative to the
            // typedef declaration, but we don't have to recursively evaluate
            // the type.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "This is not a SgClassType nor a SgNonrealType, so we don't "
                "have to recursively evaluate for template arguments. \n");
#endif
          }

          if (nrType != NULL) {
            SgNonrealDecl *nrdecl = isSgNonrealDecl(nrType->get_declaration());
            ASSERT_not_null(nrdecl);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
            MLOG_WARN_C(MLOG_UNPARSER,
                        "namedType is a SgNonrealType: nrdecl = %p = %s \n",
                        nrdecl, nrdecl->class_name().c_str());
#endif
            do {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              recursiveDepth++;
#endif
              evaluateNameQualificationForTemplateArgumentList(
                  nrdecl->get_tpl_args(), effectiveScope, positionStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              recursiveDepth--;
#endif

              if (nrdecl->get_templateDeclaration() != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
                MLOG_WARN_C(MLOG_UNPARSER,
                            " - nrdecl->get_templateDeclaration() = %p = %s \n",
                            nrdecl->get_templateDeclaration(),
                            nrdecl->get_templateDeclaration()
                                ? nrdecl->get_templateDeclaration()
                                      ->class_name()
                                      .c_str()
                                : "");
#endif
                int amountOfNameQualificationRequired =
                    nameQualificationDepth(nrdecl->get_templateDeclaration(),
                                           effectiveScope, positionStatement);
                setNameQualification(templateArgument,
                                     nrdecl->get_templateDeclaration(),
                                     amountOfNameQualificationRequired);
              }

              SgNode *nrdecl_parent = nrdecl->get_parent();
              ASSERT_not_null(nrdecl_parent);
              nrdecl_parent = nrdecl_parent->get_parent();
              ASSERT_not_null(nrdecl_parent);

              ROSE_ASSERT(nrdecl_parent != nrdecl); // That would be a loop...

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
              MLOG_WARN_C(MLOG_UNPARSER, " - nrdecl_parent = %p = %s \n",
                          nrdecl_parent, nrdecl_parent->class_name().c_str());
#endif

              nrdecl = isSgNonrealDecl(nrdecl_parent);
            } while (nrdecl != NULL);

          } else {
            int amountOfNameQualificationRequiredForTemplateArgument =
                nameQualificationDepth(namedType, effectiveScope,
                                       positionStatement);
            auto templateDeclForInstantiation =
                [](SgDeclarationStatement *decl) -> SgDeclarationStatement * {
              if (auto *inst = isSgTemplateInstantiationDecl(decl)) {
                return inst->get_templateDeclaration();
              }
              if (auto *inst =
                      isSgTemplateInstantiationTypedefDeclaration(decl)) {
                return inst->get_templateDeclaration();
              }
              if (auto *inst = isSgTemplateInstantiationFunctionDecl(decl)) {
                return inst->get_templateDeclaration();
              }
              if (auto *inst =
                      isSgTemplateInstantiationMemberFunctionDecl(decl)) {
                return inst->get_templateDeclaration();
              }
              return nullptr;
            };

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
            MLOG_WARN_C(MLOG_UNPARSER,
                        "xxxxxx --- "
                        "amountOfNameQualificationRequiredForTemplateArgument "
                        "= %d (for type = %p (%s) = %s) (counter = %d "
                        "recursiveDepth = %d) \n",
                        amountOfNameQualificationRequiredForTemplateArgument,
                        namedType, namedType->class_name().c_str(),
                        namedType->get_name().str(), counter, recursiveDepth);
            MLOG_WARN_C(MLOG_UNPARSER,
                        "xxxxxx --- Must call a function to set the name "
                        "qualification data in the SgTemplateArgument = %p \n",
                        templateArgument);
#endif

            SgDeclarationStatement *qualificationDeclaration =
                templateArgumentTypeDeclaration;
            if (templateArgumentTypeDeclaration != nullptr) {
              if (SgDeclarationStatement *templateDecl =
                      templateDeclForInstantiation(
                          templateArgumentTypeDeclaration)) {
                if (templateDecl->get_scope() != nullptr) {
                  qualificationDeclaration = templateDecl;
                  if (amountOfNameQualificationRequiredForTemplateArgument ==
                      0) {
                    int templateDeclAmount = nameQualificationDepth(
                        templateDecl, effectiveScope, positionStatement);
                    if (templateDeclAmount >
                        amountOfNameQualificationRequiredForTemplateArgument) {
                      amountOfNameQualificationRequiredForTemplateArgument =
                          templateDeclAmount;
                    }
                  }
                }
              }
            }

            if (amountOfNameQualificationRequiredForTemplateArgument == 0 &&
                qualificationDeclaration != nullptr) {
              SgScopeStatement *declScope =
                  qualificationDeclaration->get_scope();
              bool currentInDeclScope = false;
              for (SgScopeStatement *scope = effectiveScope; scope != nullptr;
                   scope = scope->get_scope()) {
                if (scope == declScope) {
                  currentInDeclScope = true;
                  break;
                }
                if (scope == scope->get_scope()) {
                  break;
                }
              }
              if (!currentInDeclScope && declScope != nullptr) {
                int forcedAmount = 0;
                for (SgScopeStatement *scope = declScope;
                     scope != nullptr && isSgGlobal(scope) == nullptr;
                     scope = scope->get_scope()) {
                  if (isSgNamespaceDefinitionStatement(scope) != nullptr ||
                      isSgClassDefinition(scope) != nullptr) {
                    ++forcedAmount;
                  }
                  if (scope == scope->get_scope()) {
                    break;
                  }
                }
                if (forcedAmount > 0) {
                  amountOfNameQualificationRequiredForTemplateArgument =
                      forcedAmount;
                }
              }
            }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
            MLOG_WARN_C(MLOG_UNPARSER,
                        "templateArgumentTypeDeclaration = %p = %s \n",
                        templateArgumentTypeDeclaration,
                        templateArgumentTypeDeclaration->class_name().c_str());
#endif
            setNameQualification(
                templateArgument, qualificationDeclaration,
                amountOfNameQualificationRequiredForTemplateArgument);
          }
        }
      }

      // If this was not a SgNamedType (and even if it is, see test2011_117.C),
      // it still might be a type where name qualification is required (might be
      // a SgArrayType with an index requiring qualification).
      processNameQualificationForPossibleArrayType(type, effectiveScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::"
                  "evaluateNameQualificationForTemplateArgumentList(): DONE: "
                  "processing type = %p = %s \n",
                  type, type->class_name().c_str());
#endif
    } else if (expression != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER, "Template argument was an expression = %p \n",
                  expression);
#endif
      // Check if this is a variable in which case it might require name
      // qualification.  If we we have to traverse this expression recursively.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      // We need to traverse this expression and evaluate if any name
      // qualification is required on its pieces (e.g. referenced variables)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Call to generateNestedTraversalWithExplicitScope() with "
                  "expression = %p = %s \n",
                  expression, expression->class_name().c_str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::"
                  "evaluateNameQualificationForTemplateArgumentList(): Calling "
                  "generateNestedTraversalWithExplicitScope(): with expression "
                  "= %p = %s and currentScope = %p = %s \n",
                  expression, expression->class_name().c_str(), currentScope,
                  currentScope->class_name().c_str());
#endif
      // DQ (3/15/2019): Added Comment: This is required because the expression
      // can be a subtree that would have to be separately traversed.
      generateNestedTraversalWithExplicitScope(expression, effectiveScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::"
                  "evaluateNameQualificationForTemplateArgumentList(): DONE: "
                  "Call to generateNestedTraversalWithExplicitScope() with "
                  "expression = %p = %s \n",
                  expression, expression->class_name().c_str());
#endif

    } else if (iname != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::"
                  "evaluateNameQualificationForTemplateArgumentList(): Empty "
                  "case: template argument is an initialized name = %p \n",
                  iname);
#endif
    } else if (tpldecl != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::"
                  "evaluateNameQualificationForTemplateArgumentList(): "
                  "template argument is a template = %p (%s)\n",
                  tpldecl, tpldecl->class_name().c_str());
#endif
      int amountOfNameQualificationRequiredForTemplateArgument =
          nameQualificationDepth(tpldecl, effectiveScope, positionStatement);
      setNameQualification(
          templateArgument, tpldecl,
          amountOfNameQualificationRequiredForTemplateArgument);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::"
                  "evaluateNameQualificationForTemplateArgumentList(): DONE: "
                  "template argument is a template = %p (%s)\n",
                  tpldecl, tpldecl->class_name().c_str());
#endif
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "===== After finishing with evaluation of templateArgument name "
        "qualification: templateArgument = %p testNameInMap = %s \n",
        templateArgument, templateArgument->unparseToString().c_str());
    MLOG_WARN_C(MLOG_UNPARSER,
                "===== templateArgument->unparseToString() = %s \n",
                templateArgument->unparseToString().c_str());
#endif

    i++;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    // Used for debugging...
    counter++;
#endif
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_TEMPLATE_ARGUMENTS
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "************************************************************************"
      "*****************************************************\n");
  MLOG_WARN_C(MLOG_UNPARSER,
              "Leaving "
              "NameQualificationTraversal::"
              "evaluateNameQualificationForTemplateArgumentList(): "
              "templateArgumentList.size() = %" PRIuPTR
              " recursiveDepth = %d \n",
              templateArgumentList.size(), recursiveDepth);
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "************************************************************************"
      "*****************************************************\n\n");
#endif

  activeTemplateArgumentListEvaluations.erase(evaluationKey);
  completedTemplateArgumentListEvaluations.insert(evaluationKey);
}

#define DEBUG_NAME_QUALIFICATION_LEVEL_FOR_NAME_QUALIFICATION_DEPTH 0

int NameQualificationTraversal::nameQualificationDepth(
    SgType *type, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  int amountOfNameQualificationRequired = 0;

  ASSERT_not_null(type);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_NAME_QUALIFICATION_DEPTH || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "In nameQualificationDepth(SgType*): type = %p = %s \n", type,
              type->class_name().c_str());
#endif

  // DQ (7/23/2011): If this is an array type, then we need special processing
  // for any name qualification of its index expressions.
  processNameQualificationForPossibleArrayType(type, currentScope);

  SgDeclarationStatement *declaration = getDeclarationAssociatedWithType(type);
  if (declaration != NULL) {

    // Check the visability and unambiguity of this declaration.
    amountOfNameQualificationRequired =
        nameQualificationDepth(declaration, currentScope, positionStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_NAME_QUALIFICATION_DEPTH
    MLOG_WARN_C(MLOG_UNPARSER, "amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
  } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_NAME_QUALIFICATION_DEPTH
    MLOG_WARN_C(MLOG_UNPARSER,
                "ERROR: In nameQualificationDepth(SgType*): declaration NOT "
                "found for type = %p = %s\n",
                type, type->class_name().c_str());
#endif
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_NAME_QUALIFICATION_LEVEL_FOR_NAME_QUALIFICATION_DEPTH
  MLOG_WARN_C(MLOG_UNPARSER,
              "Leaving nameQualificationDepth(SgType*): type = %p = %s "
              "amountOfNameQualificationRequired = %d \n",
              type, type->class_name().c_str(),
              amountOfNameQualificationRequired);
#endif

  return amountOfNameQualificationRequired;
}

int NameQualificationTraversal::nameQualificationDepthForType(
    SgInitializedName *initializedName, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  ASSERT_not_null(initializedName);
  ASSERT_not_null(positionStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In nameQualificationDepthForType(): initializedName = %s type = "
              "%p = %s currentScope = %p = %s \n",
              initializedName->get_name().str(), initializedName->get_type(),
              initializedName->get_type()->class_name().c_str(), currentScope,
              currentScope->class_name().c_str());
#endif

  SgType *initializedNameType = initializedName->get_type();

  SgPointerMemberType *pointerMemberType =
      isSgPointerMemberType(initializedNameType);
  if (pointerMemberType != nullptr) {
    SgType *baseType = pointerMemberType->get_base_type();
    ASSERT_not_null(baseType);

    // Handle member functions as a special case.
    SgMemberFunctionType *memberFunctionType = isSgMemberFunctionType(baseType);
    if (memberFunctionType != NULL) {
      SgType *returnType = memberFunctionType->get_return_type();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "nameQualificationDepthForType(): case SgPointerMemberType: Reset "
          "associated initializedNameType: returnType = %p = %s \n",
          returnType, returnType->class_name().c_str());
#endif
      initializedNameType = returnType;
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "nameQualificationDepthForType(): case SgPointerMemberType: "
                  "Reset associated initializedNameType: baseType = %p = %s \n",
                  baseType, baseType->class_name().c_str());
#endif
      initializedNameType = baseType;
    }
  }

  return nameQualificationDepth(initializedNameType, currentScope,
                                positionStatement);
}

int NameQualificationTraversal::nameQualificationDepth(
    SgInitializedName *initializedName, SgScopeStatement *currentScope,
    SgStatement *positionStatement) {
  int amountOfNameQualificationRequired = 0;

  ASSERT_not_null(initializedName);
  // SgScopeStatement* currentScope = initializedName->get_scope();
  ASSERT_not_null(currentScope);

  SgName name = initializedName->get_name();

  // DQ (6/5/2011): Test if this has a valid name (if not then it need not be
  // qualified). Examples of tests codes: test2005_114.C and test2011_73.C.
  if (name.is_null() == true) {
    // An empty name implies that no name qualification would make sense.
    return 0;
  }

  // DQ (12/28/2011): Added test...
  ASSERT_not_null(initializedName->get_scope());

  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::nameQualificationDepth():
  // initializedName->get_scope() = %p = %s
  // \n",initializedName->get_scope(),initializedName->get_scope()->class_name().c_str());

  SgDeclarationStatement *declaration =
      associatedDeclaration(initializedName->get_scope());
  // ASSERT_not_null(declaration);

  // MLOG_WARN_C(MLOG_UNPARSER, "************** In nameQualificationDepth():
  // declaration = %p \n",declaration);

  SgVariableSymbol *variableSymbol = NULL;

  // DQ (8/16/2013): Modified to support new API.
  // SgSymbol* symbol =
  // SageInterface::lookupSymbolInParentScopes(name,currentScope);
  SgSymbol *symbol =
      SageInterface::lookupSymbolInParentScopes(name, currentScope, NULL, NULL);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In "
              "NameQualificationTraversal::nameQualificationDepth("
              "SgInitializedName* = %p): symbol = %p \n",
              initializedName, symbol);
#endif

  if (symbol != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Lookup symbol based on name only (via parents starting at "
                "currentScope = %p = %s: name = %s symbol = %p = %s) \n",
                currentScope, currentScope->class_name().c_str(), name.str(),
                symbol, symbol->class_name().c_str());
#endif

    // Loop over possible chain of alias symbols to find the original sysmbol.
    SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);

    // DQ (7/12/2014): The newer design of the symbol table handling means that
    // we will never see a SgAliasSymbol at this level.
    ROSE_ASSERT(aliasSymbol == NULL);

    while (aliasSymbol != NULL) {
      // DQ (7/12/2014): The newer design of the symbol table handling means
      // that we will never see a SgAliasSymbol at this level.
      ROSE_ABORT();
      // DQ (7/12/2014): debugging use of SgAliasSymbol.
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "In NameQualificationTraversal::nameQualificationDepth(): resolving "
          "alias symbol in loop: alias = %p baseSymbol = %p = %s \n",
          aliasSymbol, aliasSymbol->get_alias(),
          aliasSymbol->get_alias()->class_name().c_str());
      symbol = aliasSymbol->get_alias();
      aliasSymbol = isSgAliasSymbol(symbol);
    }
    ROSE_ASSERT(isSgAliasSymbol(symbol) == NULL);
    variableSymbol = isSgVariableSymbol(symbol);

    if (variableSymbol == NULL) {
      variableSymbol =
          SageInterface::lookupVariableSymbolInParentScopes(name, currentScope);

      // ASSERT_not_null(variableSymbol);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      if (variableSymbol != NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Lookup symbol based symbol type: variableSymbol = %p = %s \n",
            variableSymbol, variableSymbol->class_name().c_str());
      } else {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "In "
                    "NameQualificationTraversal::nameQualificationDepth("
                    "SgInitializedName*,SgScopeStatement*,SgStatement*): "
                    "variableSymbol == NULL \n");
      }
#endif

      // amountOfNameQualificationRequired =
      // nameQualificationDepth(associatedDeclaration(initializedName->get_scope()),currentScope,positionStatement)
      // + 1; SgDeclarationStatement* declaration =
      // associatedDeclaration(initializedName->get_scope());

      // DQ (6/21/2011): This assertion fails for test2007_55.C.
      // DQ (6/5/2011): This assertion fails for test2005_114.C.
      // ASSERT_not_null(declaration);
      // amountOfNameQualificationRequired =
      // nameQualificationDepth(declaration,currentScope,positionStatement) + 1;
      if (declaration != NULL) {
        amountOfNameQualificationRequired =
            nameQualificationDepth(declaration, currentScope,
                                   positionStatement) +
            1;
      } else {
        // DQ (6/21/2011): This is the case for test2007_55.C.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "WARNING: there is no associated declaration! \n");
#endif
      }
    } else {

#define DEBUG_SKIP_VARIABLE_SYMBOL 0

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
      MLOG_WARN_C(MLOG_UNPARSER,
                  "initializedName->get_prev_decl_item() = %p \n",
                  initializedName->get_prev_decl_item());
      MLOG_WARN_C(MLOG_UNPARSER, "initializedName->get_parent() = %p = %s \n",
                  initializedName->get_parent(),
                  initializedName->get_parent()->class_name().c_str());
#endif
      // DQ (6/1/2019): If this is associated with an extern declaration then
      // don't use this symbol (see test2019_470.C). This should not apply to
      // enum values which we would want to detect the correct symbol for to
      // support the name qualification. bool skipThisSymbol =
      // (initializedName->get_prev_decl_item() != NULL); bool skipThisSymbol =
      // (initializedName->get_prev_decl_item() != NULL &&
      // isSgEnumDeclaration(initializedName->get_parent()) == NULL); bool
      // skipThisSymbol = (initializedName->get_prev_decl_item() != NULL ||
      // isSgEnumDeclaration(initializedName->get_parent()) != NULL); bool
      // skipThisSymbol = (initializedName->get_prev_decl_item() != NULL ||
      // isSgEnumDeclaration(initializedName->get_parent()) == NULL); bool
      // skipThisSymbol = (initializedName->get_prev_decl_item() != NULL ||
      // isSgEnumDeclaration(initializedName->get_parent()) == NULL); bool
      // skipThisSymbol = false;
      bool skipThisSymbol = true;

      // Check if the initializedName is appearing in different scopes, which
      // would trigger name qualification. SgVariableDeclaration*
      // variableDeclaration =
      // isSgVariableDeclaration(initializedName->get_parent()); if
      // (initializedName->get_prev_decl_item() != NULL && variableDeclaration
      // != NULL)
      if (initializedName->get_prev_decl_item() != NULL) {
        SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(
            initializedName->get_prev_decl_item()->get_parent());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
        MLOG_WARN_C(MLOG_UNPARSER, "variableDeclaration = %p \n",
                    variableDeclaration);
#endif
        // if (variableDeclaration != NULL &&
        // variableDeclaration->get_declarationModifier().get_storageModifier().isExtern()
        // && variableDeclaration->get_linkage().empty() == true)
        if (variableDeclaration != NULL) {
          SgVariableDeclaration *possible_extern_variableDeclaration =
              isSgVariableDeclaration(
                  initializedName->get_prev_decl_item()->get_parent());
          SgVariableDeclaration *original_variableDeclaration =
              isSgVariableDeclaration(initializedName->get_parent());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
          MLOG_WARN_C(MLOG_UNPARSER,
                      "possible_extern_variableDeclaration = %p \n",
                      possible_extern_variableDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "original_variableDeclaration        = %p \n",
                      original_variableDeclaration);
#endif
          if (possible_extern_variableDeclaration != NULL &&
              original_variableDeclaration != NULL) {
            // Need to check if either of these declarations was marked as
            // extern.
            bool possible_extern_foundExternModifier =
                (possible_extern_variableDeclaration->get_declarationModifier()
                     .get_storageModifier()
                     .isExtern() &&
                 possible_extern_variableDeclaration->get_linkage().empty() ==
                     true);
            bool original_foundExternModifier =
                (original_variableDeclaration->get_declarationModifier()
                     .get_storageModifier()
                     .isExtern() &&
                 original_variableDeclaration->get_linkage().empty() == true);
            bool foundExternModifier = (possible_extern_foundExternModifier ||
                                        original_foundExternModifier);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
            MLOG_WARN_C(MLOG_UNPARSER, "foundExternModifier = %s \n",
                        foundExternModifier ? "true" : "false");
#endif
            if (foundExternModifier == true) {
              SgScopeStatement *possible_extern_variable_scope =
                  possible_extern_variableDeclaration->get_scope();
              SgScopeStatement *original_variable_scope =
                  original_variableDeclaration->get_scope();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
              MLOG_WARN_C(MLOG_UNPARSER,
                          "possible_extern_variable_scope = %p = %s \n",
                          possible_extern_variable_scope,
                          possible_extern_variable_scope->class_name().c_str());
              MLOG_WARN_C(MLOG_UNPARSER,
                          "original_variable_scope        = %p = %s \n",
                          original_variable_scope,
                          original_variable_scope->class_name().c_str());
#endif
              SgNamespaceDefinitionStatement
                  *possible_extern_variable_namespace_definition =
                      isSgNamespaceDefinitionStatement(
                          possible_extern_variable_scope);
              SgNamespaceDefinitionStatement
                  *original_variable_namespace_definition =
                      isSgNamespaceDefinitionStatement(original_variable_scope);
              if (possible_extern_variable_namespace_definition != NULL &&
                  original_variable_namespace_definition != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
                MLOG_WARN_C(
                    MLOG_UNPARSER,
                    "possible_extern_variable_namespace_definition = %p \n",
                    possible_extern_variable_namespace_definition);
                MLOG_WARN_C(
                    MLOG_UNPARSER,
                    "original_variable_namespace_definition        = %p \n",
                    original_variable_namespace_definition);
                MLOG_WARN_C(MLOG_UNPARSER,
                            "possible_extern_variable_namespace_definition->"
                            "get_global_definition() = %p \n",
                            possible_extern_variable_namespace_definition
                                ->get_global_definition());
                MLOG_WARN_C(MLOG_UNPARSER,
                            "original_variable_namespace_definition->get_"
                            "global_definition()        = %p \n",
                            original_variable_namespace_definition
                                ->get_global_definition());
#endif
                if (possible_extern_variable_namespace_definition
                        ->get_global_definition() ==
                    original_variable_namespace_definition
                        ->get_global_definition()) {
                  // skipThisSymbol = true;
                  skipThisSymbol = false;
                }
              } else {
                ROSE_ASSERT(possible_extern_variable_scope != NULL &&
                            original_variable_scope != NULL);
                if (possible_extern_variable_scope == original_variable_scope) {
                  // skipThisSymbol = true;
                  skipThisSymbol = false;
                }
              }
            } else {
              // If neither is marked extern, then they should not be name
              // qualified.
              skipThisSymbol = false;
            }
          }
        }
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_SKIP_VARIABLE_SYMBOL
      MLOG_WARN_C(MLOG_UNPARSER, "skipThisSymbol = %s \n",
                  skipThisSymbol ? "true" : "false");
#endif

      // DQ (6/4/2011): Get the associated symbol so that we can avoid matching
      // on name only; and not the actual SgVariableSymbol symbols.
      SgVariableSymbol *targetInitializedNameSymbol = isSgVariableSymbol(
          initializedName->search_for_symbol_from_symbol_table());
      ASSERT_not_null(targetInitializedNameSymbol);

      // DQ (6/1/2019): If this is associated with an extern declaration then
      // don't use this symbol (see test2019_470.C). DQ (6/4/2011): Make sure we
      // have the correct symbol, else we have detected a collision which will
      // require name qualification to resolve. if (variableSymbol ==
      // targetInitializedNameSymbol) if (variableSymbol ==
      // targetInitializedNameSymbol && skipThisSymbol == false)
      if (variableSymbol == targetInitializedNameSymbol &&
          skipThisSymbol == false) {
        // Found the correct symbol.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "Found the correct SgVariableSymbol \n");
#endif
      } else {
        // This is not the correct symbol, even though the unqualified names
        // match.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "These symbols only match based on name and is not the "
                    "targetInitializedNameSymbol. \n");
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        // DQ (12/15/2014): Liao's move tool will cause this message to be
        // output, but it is not a problem (I think).
        if (declaration == NULL) {
          MLOG_WARN_C(MLOG_UNPARSER, "variableSymbol = %p \n", variableSymbol);
          MLOG_WARN_C(MLOG_UNPARSER, "targetInitializedNameSymbol = %p = %s \n",
                      targetInitializedNameSymbol,
                      targetInitializedNameSymbol->get_name().str());
          MLOG_WARN_C(MLOG_UNPARSER, "initializedName = %p = %s \n",
                      initializedName, initializedName->get_name().str());
          initializedName->get_file_info()->display(
              "NameQualificationTraversal::nameQualificationDepth(): "
              "initializedName");
        }
#endif

        // DQ (12/28/2011): I think it may be OK to have this be NULL, in which
        // case there is not name qualification (scope has no associated
        // declaration, so it is NULL as is should be).
        // ASSERT_not_null(declaration);
        if (declaration != NULL) {
          amountOfNameQualificationRequired =
              nameQualificationDepth(declaration, currentScope,
                                     positionStatement) +
              1;
        } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          // DQ (12/15/2014): Liao's move tool will cause this message to be
          // output, but it is not a problem (I think).
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Warning: In "
                      "NameQualificationTraversal::nameQualificationDepth() "
                      "--- It might be that this is an incorrect fix for where "
                      "declaration == NULL in test2004_97.C \n");
#endif
        }
      }
    }
  } else {
    // Symbol for the SgInitializedName is not in the current scope or those of
    // parent scopes.  So some name qualification is required.
    // amountOfNameQualificationRequired =
    // nameQualificationDepth(associatedDeclaration(initializedName->get_scope()),currentScope,positionStatement)
    // + 1; SgDeclarationStatement* declaration =
    // associatedDeclaration(initializedName->get_scope());
    // amountOfNameQualificationRequired =
    // nameQualificationDepth(declaration,currentScope,positionStatement) + 1;
    // ASSERT_not_null(declaration);

    // See test2004_34.C for an example of where declaration == NULL
    if (declaration != NULL) {
      amountOfNameQualificationRequired =
          nameQualificationDepth(declaration, currentScope, positionStatement) +
          1;
    } else {
      // This can be the case of ??? "catch (Overflow)" (see test2004_43.C)
      // instead of "catch (Overflow xxx)" (see test2011_71.C).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::nameQualificationDepth("
                  "SgInitializedName*): declaration == NULL, why is this? "
                  "initializedName->get_scope() = %p = %s \n",
                  initializedName->get_scope(),
                  initializedName->get_scope()->class_name().c_str());
#endif
      // ROSE_ABORT();
    }
  }

  // amountOfNameQualificationRequired =
  // nameQualificationDepth(declaration,currentScope,positionStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "amountOfNameQualificationRequired = %d \n",
              amountOfNameQualificationRequired);
#endif

  return amountOfNameQualificationRequired;
}

// DQ (3/14/2019): Adding debugging support to output the map of names.
// void NameQualificationTraversal::outputNameQualificationMap( const
// std::map<SgNode*,std::string> & qualifiedNameMap )
void NameQualificationTraversal::outputNameQualificationMap(
    const NameQualificationMapType &qualifiedNameMap) {
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::outputNameQualificationMap(): "
              "qualifiedNameMap.size() = %zu \n",
              qualifiedNameMap.size());

  int counter = 0;
  // std::map<SgNode*,std::string>::const_iterator i = qualifiedNameMap.begin();
  NameQualificationMapType::const_iterator i = qualifiedNameMap.begin();
  while (i != qualifiedNameMap.end()) {
    ASSERT_not_null(i->first);

    MLOG_WARN_C(MLOG_UNPARSER,
                " --- counter = %d *i = i->first = %p = %s i->second = %s \n",
                counter, i->first, i->first->class_name().c_str(),
                i->second.c_str());

    counter++;
    i++;
  }
}

// DQ (8/14/2025): Adding optimization (default is false) to support name
// qualification retricted to just the input source file (instead of the whole
// translation unit).
void NameQualificationTraversal::
    set_suppressNameQualificationAcrossWholeTranslationUnit(bool value) {
  suppressNameQualificationAcrossWholeTranslationUnit = value;
}

// DQ (8/14/2025): Adding optimization (default is false) to support name
// qualification retricted to just the input source file (instead of the whole
// translation unit).
bool NameQualificationTraversal::
    get_suppressNameQualificationAcrossWholeTranslationUnit() {
  return suppressNameQualificationAcrossWholeTranslationUnit;
}

void NameQualificationTraversal::addToNameMap(SgNode *reference_node,
                                              string qualified_name) {
  addToNameMap(reference_node, static_cast<SgNode *>(NULL), qualified_name);
}

void NameQualificationTraversal::addToNameMap(SgNode *reference_node,
                                              SgNode *type_node,
                                              string qualified_name) {
  addToNameMap(reference_node, type_node, static_cast<SgNode *>(NULL),
               qualified_name);
}

void NameQualificationTraversal::addToNameMap(SgNode *reference_node,
                                              SgNode *type_node,
                                              SgNode *context_node,
                                              string qualified_name) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In addToNameMap(): nodeReference = %p = %s typeNameString = %s \n",
      reference_node, reference_node->class_name().c_str(),
      qualified_name.c_str());
#endif

  ASSERT_not_null(reference_node);

  // DQ (6/21/2011): This is refactored code used in traverseType() and
  // traverseTemplatedFunction(). bool isTemplateName =
  // (typeNameString.find('<') != string::npos) && (typeNameString.find("::") !=
  // string::npos);
  bool isTemplateName =
      (qualified_name.find('<') !=
       string::npos); // && (qualified_name.find("::") != string::npos);

  bool isPointerMemberType = (isSgPointerMemberType(reference_node) != NULL);

  // DQ (4/21/2019): Unclear if we should store the intermediately generated
  // strings for each part of a type. I think that test2019_385.C makes it clear
  // that we need to allow the SgPointerMemberType intermediate strings to be
  // reset. isPointerMemberType = false;

  bool isInitializedName = (isSgInitializedName(reference_node) != NULL);

  // DQ (4/28/2019): Adding support for the base type if a SgTypedefDeclaration.
  bool isTypedefDeclaration = (isSgTypedefDeclaration(reference_node) != NULL);

  // DQ (4/28/2019): We only want to save the type as a string if it has a
  // SgPointerMemberType (or a template instantiation). I think the same thing
  // is also true for the type associated with a SgInitializedName as well.
  SgTypedefDeclaration *typedefDeclaration =
      isSgTypedefDeclaration(reference_node);
  if (typedefDeclaration != NULL) {
    SgType *baseType = typedefDeclaration->get_base_type();

    // We realy need to strip off any modifiers before we check if it is a
    // SgPointerMembertype.
    unsigned char bit_array =
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE;
    baseType = baseType->stripType(bit_array);
    ASSERT_not_null(baseType);

    // Check if this is something that could be name qualified.
    SgPointerMemberType *pointerMemberType = isSgPointerMemberType(baseType);
    SgNamedType *namedType = isSgNamedType(baseType);
    // if (pointerMemberType == NULL)
    if (pointerMemberType == NULL && namedType == NULL) {
      isTypedefDeclaration = false;
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    // Find out what type this is, we need to detect template instantiations as
    // well, so that they CAN be used to generate strings.
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In addToNameMap(): case SgTypedefDeclaration: baseType = %p = %s \n",
        baseType, baseType->class_name().c_str());
#endif
  }

  // DQ (4/28/2019): Handle the SgInitialzedName the same as the typedef.
  SgInitializedName *initializedName = isSgInitializedName(reference_node);
  if (initializedName != NULL) {
    SgType *type = initializedName->get_type();

    // We realy need to strip off any modifiers before we check if it is a
    // SgPointerMembertype.
    unsigned char bit_array =
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE;
    type = type->stripType(bit_array);
    ASSERT_not_null(type);

    // Check if this is something that could be name qualified.
    SgPointerMemberType *pointerMemberType = isSgPointerMemberType(type);
    SgNamedType *namedType = isSgNamedType(type);
    // if (pointerMemberType == NULL)
    if (pointerMemberType == NULL && namedType == NULL) {
      isInitializedName = false;
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    // Find out what type this is, we need to detect template instantiations as
    // well, so that they CAN be used to generate strings.
    MLOG_WARN_C(MLOG_UNPARSER,
                "In addToNameMap(): case SgInitializedName: type = %p = %s \n",
                type, type->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In addToNameMap(): case SgInitializedName: isInitializedName = %s \n",
        isInitializedName ? "true" : "false");
#endif
  }

  // DQ (4/21/2019): The list of nodes which we will have to allow to store
  // types as generated strings will have to include expressions (SgNewExp,
  // SgSizeOf, SgCastExp, SgTypeIdOp) and functions (SgFunctionDeclaration) and
  // maybe that is about it.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "In addToNameMap(): isTemplateName = %s isPointerMemberType = %s "
              "nodeReference = %p = %s typeNameString = %s \n",
              isTemplateName ? "true" : "false",
              isPointerMemberType ? "true" : "false", reference_node,
              reference_node->class_name().c_str(), qualified_name.c_str());
#endif

  // DQ (4/21/2019): Adding case to support SgInitializedName.
  // DQ (4/18/2019): We need to support SgPointerMemberType as well.
  // if (isTemplateName == true)
  // if (isTemplateName == true || isPointerMemberType == true)
  // if (isTemplateName == true || isPointerMemberType == true ||
  // isInitializedName == true)
  bool isConstructorInitializer =
      (isSgConstructorInitializer(reference_node) != NULL);
  if (isTemplateName == true || isPointerMemberType == true ||
      isInitializedName == true || isTypedefDeclaration == true ||
      isConstructorInitializer == true) {
    SgNode *contextKey = (context_node != NULL) ? context_node : reference_node;
    if (type_node != NULL) {
      NameQualificationMapType &scopedTypeNameMap =
          qualifiedNameMapForMapsOfTypes[reference_node];
      NameQualificationMapType::iterator scopedIter =
          scopedTypeNameMap.find(contextKey);
      if (scopedIter == scopedTypeNameMap.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(MLOG_UNPARSER,
                    "============== Inserting qualifier for name = %s into "
                    "scoped type name map at IR node = %p = %s \n",
                    typeNameString.c_str(), contextKey,
                    contextKey->class_name().c_str());
#endif
        scopedTypeNameMap.insert(
            std::pair<SgNode *, std::string>(contextKey, qualified_name));
      } else {
        if (scopedIter->second != qualified_name &&
            should_replace_type_name(scopedIter->second, qualified_name)) {
          scopedIter->second = qualified_name;
        }
      }

      NameQualificationMapType::iterator legacyIter =
          typeNameMap.find(reference_node);
      if (legacyIter == typeNameMap.end()) {
        typeNameMap.insert(
            std::pair<SgNode *, std::string>(reference_node, qualified_name));
      } else if (legacyIter->second != qualified_name &&
                 should_replace_type_name(legacyIter->second, qualified_name)) {
        legacyIter->second = qualified_name;
      }
    } else {
      if (typeNameMap.find(reference_node) == typeNameMap.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(MLOG_UNPARSER,
                    "============== Inserting qualifier for name = %s into "
                    "typeNameMap list at IR node = %p = %s \n",
                    qualified_name.c_str(), reference_node,
                    reference_node->class_name().c_str());
#endif
        typeNameMap.insert(
            std::pair<SgNode *, std::string>(reference_node, qualified_name));
      } else {
        NameQualificationMapType::iterator i = typeNameMap.find(reference_node);
        ROSE_ASSERT(i != typeNameMap.end());
        if (i->second != qualified_name &&
            should_replace_type_name(i->second, qualified_name)) {
          i->second = qualified_name;
        }
      }
    }
  } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    // DQ (8/19/2013): comment added for debugging.
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::addToNameMap(): isTemplateName "
                "== false, typeNameString = %s NOT added to typeNameMap for "
                "key = %p = %s \n",
                qualified_name.c_str(), reference_node,
                reference_node->class_name().c_str());
#endif
  }
}

void NameQualificationTraversal::traverseType(SgType *type,
                                              SgNode *nodeReferenceToType,
                                              SgScopeStatement *currentScope,
                                              SgStatement *positionStatement) {
  // The type can contain subtypes (e.g. template arguments) and when the
  // subtypes need to be name qualified the name of the encompassing type has a
  // name that depends upon its location in the source code (and could vary
  // depending on the positon in a single basic block, I think).

#define DEBUG_TRAVERSE_TYPE 0

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Starting traversal of type: type = %p = %s \n", type,
              type->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In traverseType(): nodeReferenceToType = %p = %s \n",
              nodeReferenceToType, nodeReferenceToType->class_name().c_str());
#endif

  // DQ (4/27/2019): If this is an un-named type then it can't be name
  // qualified, so we can exit directly. We don't want to generate strings to
  // represent the types from unNamed declarations that could not be name
  // qualified anyway.
  bool isUnNamed = false;
  // SgNamedType* namedType = isSgNamedType(type);
  unsigned char bit_array =
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE;
  SgType *strippedType = type->stripType(bit_array);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE
  MLOG_WARN_C(MLOG_UNPARSER, "In traverseType(): strippedType = %p = %s \n",
              strippedType, strippedType->class_name().c_str());
#endif

  SgNamedType *namedType = isSgNamedType(strippedType);
  if (namedType != NULL) {
    SgEnumType *enumType = isSgEnumType(namedType);
    SgClassType *classType = isSgClassType(namedType);

    if (enumType != NULL) {
      SgEnumDeclaration *enumDeclaration =
          isSgEnumDeclaration(enumType->get_declaration());
      ASSERT_not_null(enumDeclaration);
#if DEBUG_TRAVERSE_TYPE
      MLOG_WARN_C(MLOG_UNPARSER,
                  "enumDeclaration->get_isUnNamed()        = %s \n",
                  enumDeclaration->get_isUnNamed() ? "true" : "false");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "namedType->get_autonomous_declaration() = %s \n",
                  namedType->get_autonomous_declaration() ? "true" : "false");
#endif
      isUnNamed = enumDeclaration->get_isUnNamed();
    } else {
      if (classType != NULL) {
        SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(classType->get_declaration());
        ASSERT_not_null(classDeclaration);
#if DEBUG_TRAVERSE_TYPE
        MLOG_WARN_C(MLOG_UNPARSER,
                    "classDeclaration->get_isUnNamed()       = %s \n",
                    classDeclaration->get_isUnNamed() ? "true" : "false");
        MLOG_WARN_C(MLOG_UNPARSER,
                    "namedType->get_autonomous_declaration() = %s \n",
                    namedType->get_autonomous_declaration() ? "true" : "false");
#endif
        isUnNamed = classDeclaration->get_isUnNamed();
      } else {
        // This is most commonly a SgTypedefType, but they can't be un-named.
#if DEBUG_TRAVERSE_TYPE
        MLOG_WARN_C(MLOG_UNPARSER,
                    "In traverseType(): Alternative SgNamedType: namedType = "
                    "%p = %s \n",
                    namedType, namedType->class_name().c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "namedType->get_autonomous_declaration()               = %s \n",
            namedType->get_autonomous_declaration() ? "true" : "false");
#endif
      }
    }

    // Return from function if this is associated wqith an unNamed declaration
    // (since it could not be name qualified).
    if (isUnNamed == true) {
#if DEBUG_TRAVERSE_TYPE
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In traverseType(): isUnNamed == true: returning \n");
#endif
      return;
    }
  }

  // DQ (4/22/2019): Need to detect when this is part of a type from a paremter
  // in a function parameter list.
  SgInitializedName *initializedName = isSgInitializedName(nodeReferenceToType);
  bool inArgList = false;
  if (initializedName != NULL) {
#if DEBUG_TRAVERSE_TYPE
    MLOG_WARN_C(MLOG_UNPARSER,
                "Found a SgInitializedName: initializedName = %p name = %s \n",
                initializedName, initializedName->get_name().str());
#endif
    // If this is a part of a function parameter list, then we need to set
    // info.inArgList() in the Unparse_Info object that we pass to the unparse
    // the type.
    SgFunctionParameterList *functionParameterList =
        isSgFunctionParameterList(initializedName->get_parent());
    inArgList = (functionParameterList != NULL);
  }

  // DQ (4/28/2019): Need to detect when this is part of a base type from a
  // typedef declaration.
  SgTypedefDeclaration *typedefDeclaration =
      isSgTypedefDeclaration(nodeReferenceToType);
  bool inTypedefDecl = false;
  if (typedefDeclaration != NULL) {
#if DEBUG_TRAVERSE_TYPE
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Found a SgTypedefDeclaration: typedefDeclaration = %p name = %s \n",
        typedefDeclaration, typedefDeclaration->get_name().str());
#endif
    inTypedefDecl = true;
  }

  // DQ (4/27/2019): Could this be how we trigger name qualification for
  // constructor preinitialization lists?
  SgConstructorInitializer *constructorInitializer =
      isSgConstructorInitializer(nodeReferenceToType);
  if (constructorInitializer != NULL) {
#if DEBUG_TRAVERSE_TYPE
    MLOG_WARN_C(MLOG_UNPARSER, "Found a SgConstructorInitializer: %p \n",
                constructorInitializer);
#endif
  }

  ASSERT_not_null(nodeReferenceToType);

  // DQ (3/29/2019): I think we are ready to address this now.
  // Some type IR nodes are difficult to save as a string and reuse. So for now
  // we will skip supporting some type IR nodes with generated name
  // qualification specific to where they are used.
  bool skipThisType = false;
  if (isSgPointerMemberType(type) != NULL) {
    // DQ (3/29/2019): We would like to no longer skip this type.
    // skipThisType = true;
  }

  // DQ (4/20/2019): We should have a SgType member function for this (which
  // would be more complete). bool isPrimativeType = (isSgTypeInt(type) != NULL)
  // || (isSgTypeLong(type) != NULL) || (isSgTypeVoid(type) != NULL);
  bool isPrimativeType = type->isPrimativeType();
  bool isFunctionType = (isSgFunctionType(type) != NULL);

  // DQ (4/27/2019): Added support for member functions to handle test2019_102.C
  // test code. bool isMemberFunctionType = (isSgMemberFunctionType(type) !=
  // NULL);

  // bool isEnumType = (isSgEnumType(type) != NULL);
  // bool isEnumType = false;

  // if (isPrimativeType == true)
  // if (isPrimativeType == true || isFunctionType == true)
  // if (isPrimativeType == true || isFunctionType == true || isEnumType ==
  // true) if (isPrimativeType == true || isFunctionType == true ||
  // isMemberFunctionType == true)
  if (isPrimativeType == true || isFunctionType == true) {
    // DQ (3/29/2019): We should skip this type because they are never name
    // qualified.
    skipThisType = true;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In NameQualificationTraversal::traverseType:\n");
  MLOG_WARN_C(MLOG_UNPARSER, " -- type = %p (%s) : %s\n", type,
              type->class_name().c_str(), type->unparseToString().c_str());
#endif

  // DQ (5/17/2019): Adding support to only handle class types that are from
  // template instantiations (since they can contains types that require name
  // qualificaiton). bool isTemplateInstantiationType = false;
  SgClassType *classType = isSgClassType(type);
  if (classType != NULL) {
    SgDeclarationStatement *classDeclaration = classType->get_declaration();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::traverseType: classDeclaration "
                "= %p = %s \n",
                classDeclaration, classDeclaration->class_name().c_str());
#endif
    SgTemplateInstantiationDecl *templateInstantiationDecl =
        isSgTemplateInstantiationDecl(classDeclaration);
    if (templateInstantiationDecl == NULL) {
      // isTemplateInstantiationType = true;
      skipThisType = true;
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::traverseType: skipThisType = %s \n",
      skipThisType ? "true" : "false");
#endif

  if (skipThisType == false) {
    SgDeclarationStatement *declaration = associatedDeclaration(type);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, " -- declaration = %p (%s)\n", declaration,
                declaration ? declaration->class_name().c_str() : "");
#endif
    if (declaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "In NameQualificationTraversal::traverseType(): Calling "
          "evaluateTemplateInstantiationDeclaration(): declaration = %p = %s "
          "currentScope = %p = %s positionStatement = %p = %s \n",
          declaration, declaration->class_name().c_str(), currentScope,
          currentScope->class_name().c_str(), positionStatement,
          positionStatement->class_name().c_str());
#endif
      evaluateTemplateInstantiationDeclaration(declaration, currentScope,
                                               positionStatement);
    }

    SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
    ASSERT_not_null(unparseInfoPointer);
    unparseInfoPointer->set_outputCompilerGeneratedStatements();

    // Avoid unpasing the class definition when unparseing the type.
    unparseInfoPointer->set_SkipClassDefinition();

    // DQ (5/8/2013): Added specification to skip enum definitions also (see
    // test2012_202.C).
    unparseInfoPointer->set_SkipEnumDefinition();

    // Associate the unparsing of this type with the statement or scope where it
    // occures. This is the key to use in the lookup of the qualified name. But
    // this is the correct key....
    // unparseInfoPointer->set_reference_node_for_qualification(positionStatement);
    // unparseInfoPointer->set_reference_node_for_qualification(currentScope);
    unparseInfoPointer->set_reference_node_for_qualification(
        nodeReferenceToType);
    unparseInfoPointer->set_current_scope(currentScope);

    SgSourceFile *sourceFile =
        SageInterface::getEnclosingSourceFile(nodeReferenceToType);
    if (sourceFile == NULL) {
      sourceFile = SageInterface::getEnclosingSourceFile(positionStatement);
    }
    if (sourceFile == NULL) {
      sourceFile = SageInterface::getEnclosingSourceFile(currentScope);
    }
    if (sourceFile != NULL) {
      unparseInfoPointer->set_current_source_file(sourceFile);
    }

    // DQ (5/7/2013): A problem with this is that it combines the first and
    // second parts of the type into a single string (e.g. the array type will
    // include two parts "base_type" <array name> "[index]". When this is
    // combined for types that have two parts (most types don't) the result is
    // an error when the type is unparsed.  It is not clear, but a solution
    // might be for this to be built here as just the 1st part, and let the
    // second part be generated when the array type is unparsed. BTW, the reason
    // why it is computed here is that there may be many nested types that
    // require name qualifications and so it is required that we save the whole
    // string.  However, name qualification might only apply to the first part
    // of types.  So we need to investigate this. This is a problem demonstrated
    // in test2013_156.C and test2013_158.C.

    // DQ (5/8/2013): Set the SgUnparse_Info so that only the first part will be
    // unparsed.
    unparseInfoPointer->set_isTypeFirstPart();
    // DQ (4/27/2019): Comment out out the setting of SkipClassSpecifier.  If
    // this is commented out then we pass a lot more (about 19 more) of the test
    // codes in the Cxx_test directory, else we only fail 7 test codes. However,
    // we fail a number of the tests in the Cxx11_tests directory. Not clear how
    // to proceed here.

    // DQ (5/14/2019): We can sometimes need this class specifier to be output
    // (e.g. test2019_430.C) (testing). DQ (8/19/2013): Added specification to
    // skip class specifier (fixes problem with test2013_306.C).
    // unparseInfoPointer->set_SkipClassSpecifier();
    // DQ (4/22/2019): Make this as being from a parameter list (types are
    // unparse with extra parenthesis).
    if (inArgList == true) {
      unparseInfoPointer->set_inArgList();

      // DQ (5/19/2019): when in the function argument list, don't output the
      // class specifier. unparseInfoPointer->set_SkipClassSpecifier();
    }

    // DQ (4/28/2019): Need to detect when this is part of a base type from a
    // typedef declaration.
    if (inTypedefDecl == true) {
      unparseInfoPointer->set_inTypedefDecl();

      // Carry declaration-level elaboration policy into generated type strings
      // so map-based type names match the AST semantics.
      if (typedefDeclaration->skipElaborateType()) {
        unparseInfoPointer->set_SkipClassSpecifier();
      }
    }

    // DQ (5/18/2019): Makr this as being in a SgAggregateInitializer.
    bool exit_in_processing_aggregateInitializer = false;
    SgAggregateInitializer *aggregateInitializer =
        isSgAggregateInitializer(nodeReferenceToType);
    if (aggregateInitializer != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE
      MLOG_WARN_C(MLOG_UNPARSER,
                  "++++++++++++++++ explicitly setting "
                  "unparseInfoPointer->set_inAggregateInitializer() \n");
#endif
      exit_in_processing_aggregateInitializer = true;
      unparseInfoPointer->set_inAggregateInitializer();

      // DQ (5/18/2019): when in the SgAggregateInitializer, don't output the
      // class specifier.
      unparseInfoPointer->set_SkipClassSpecifier();
    }

    SgConstructorInitializer *constructorInitializer =
        isSgConstructorInitializer(nodeReferenceToType);
    if (constructorInitializer != NULL) {
      // DQ (5/19/2019): when in the function argument list, don't output the
      // class specifier.
      unparseInfoPointer->set_SkipClassSpecifier();
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "++++++++++++++++ Calling globalUnparseToString(): type = %p = %s \n",
        type, type->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "++++++++++++++++ unparseInfoPointer->inAggregateInitializer() = %s \n",
        unparseInfoPointer->inAggregateInitializer() ? "true" : "false");
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE
    // DQ (5/18/2019): Adding debugging info.
    // unparseInfoPointer->display("In
    // NameQualificationTraversal::traverseType(): unparseInfoPointer \n");
#endif

    bool isContainedInTemplateInstantiationDefn = false;
    SgTemplateInstantiationDefn *templateInstantiationDefn = NULL;
    SgScopeStatement *parentScope = NULL;
    SgStatement *statement = isSgStatement(nodeReferenceToType);
    if (statement != NULL) {
      parentScope = statement->get_scope();
    } else {
      if (initializedName != NULL) {
        parentScope = initializedName->get_scope();
      } else {
        parentScope = NULL;
      }
    }

    // SgGlobal* globalScope = isSgGlobal(parentScope);
    while (isSgGlobal(parentScope) == NULL && parentScope != NULL) {
      templateInstantiationDefn = isSgTemplateInstantiationDefn(parentScope);
      if (templateInstantiationDefn != NULL) {
        isContainedInTemplateInstantiationDefn = true;
      }

      parentScope = parentScope->get_scope();
    }

    // DQ (7/12/2022): If this is inside of a SgTemplateInstantiationDefn then
    // see if we can suppress the generation since this is where the type names
    // that are too long come from. string typeNameString =
    // globalUnparseToString(type,unparseInfoPointer);
    string typeNameString;
    SgType *typeForGeneratedName = type;
    if (unparseInfoPointer->isTypeFirstPart()) {
      if (SgArrayType *arrayType = isSgArrayType(type)) {
        // The cached type string is only reused for the first declarator
        // fragment. For arrays that fragment is the element type.
        typeForGeneratedName = arrayType->get_base_type();
      }
    }
    // DQ (7/13/2022): Modified code to avoid name qualification in template
    // class instantiations.
    if (isContainedInTemplateInstantiationDefn == false) {
      typeNameString =
          globalUnparseToString(typeForGeneratedName, unparseInfoPointer);
    }

    // Constructor initializers can carry the leading qualification on the
    // initializer itself. Only force the type string to a fully qualified
    // spelling when no constructor-name qualifier has been recorded yet.
    bool constructor_has_name_qualifier = false;
    if (constructorInitializer != NULL) {
      NameQualificationMapType::const_iterator qualifier_it =
          qualifiedNameMapForNames.find(constructorInitializer);
      constructor_has_name_qualifier =
          qualifier_it != qualifiedNameMapForNames.end() &&
          !qualifier_it->second.empty();
    }
    if (constructorInitializer != NULL && !constructor_has_name_qualifier &&
        typeNameString.find("::") == std::string::npos &&
        typeNameString.find('<') == std::string::npos) {
      if (SgNamedType *named = isSgNamedType(strippedType)) {
        std::string qualified = named->get_qualified_name().getString();
        if (!qualified.empty() && qualified != typeNameString) {
          typeNameString = qualified;
        }
      }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE || 0
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "++++++++++++++++ typeNameString (globalUnparseToString()) = %s \n",
        typeNameString.c_str());
#endif

    // DQ (5/18/2019): Testing...
    // ROSE_ASSERT(exit_in_processing_aggregateInitializer == false);
    if (exit_in_processing_aggregateInitializer != false) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_TYPE
      // DQ (5/18/2019): Adding debugging info.
      unparseInfoPointer->display(
          "In NameQualificationTraversal::traverseType(): "
          "exit_in_processing_aggregateInitializer == true: unparseInfoPointer "
          "\n");
#endif
    }

    // DQ (7/13/2011): Some standard library types can be very long
    // (see test2004_35.C). This is symptematic of an error which
    // causes the whole class to be included with the class definition.
    // This was fixed by calling
    // unparseInfoPointer->set_SkipClassDefinition() above. if
    // (typeNameString.length() > 6000) if (typeNameString.length() >
    // 600)
    if (typeNameString.length() > 6000) {
      if (SgProject::get_verbose() > 0) {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Warning: type names should not be this "
                    "long...(unless this is from template-heavy "
                    "headers) typeNameString.length() = %" PRIuPTR " \n",
                    typeNameString.length());
      }

      // DQ (10/11/2015): Increased max size of typename handled in
      // ROSE (a 42K charater long typename was generated by
      // test2015_87.C), so we will allow this. DQ (1/30/2013):
      // Increased already too long limit for string lengths for
      // typenames.  This test fails for ROSE compiling ROSE with a
      // type name that is 17807 characters long.  I have increased
      // the max allowable typename lengtt to over twice that for
      // good measure. DQ (7/22/2011): The
      // a992-thrifty-mips-compiler Hudson test fails because it
      // generates a typename that is even longer 5149, so we need
      // an even larger upper bound.  This should be looked into
      // later to see why some of these different platforms are
      // generating such large typenames. if
      // (typeNameString.length() > 10000) if
      // (typeNameString.length() > 40000) if
      // (typeNameString.length() > 400000) if
      // (typeNameString.length() > 400)
      if (typeNameString.length() > 400000) {
        // If your ever curious, you can output the type name.
        // MLOG_WARN_C(MLOG_UNPARSER, "Error: typeNameString = %s
        // \n",typeNameString.c_str());
        // DQ (7/11/2022): Output the type info:
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Type name from unparseToString is too long: type = %p = %s \n",
            type, type->class_name().c_str());

        printf("Output debugging info: calling "
               "recursivePrintCurrentAndParent() \n");
        SageInterface::recursivePrintCurrentAndParent(nodeReferenceToType);

        // DQ (2/7/2017): Output offending type name string to a
        // file for inspection.
        ASSERT_not_null(positionStatement);
        positionStatement->get_file_info()->display(
            "Output offending type name string to a file for inspection: "
            "debug");

        SgFile *problemFile =
            SageInterface::getEnclosingFileNode(positionStatement);
        string filename = problemFile->getFileName();
        filename += ".typename";

        MLOG_WARN_C(MLOG_UNPARSER,
                    "Generating a file (%s) to hold the typename \n",
                    filename.c_str());

        MLOG_WARN_C(MLOG_UNPARSER,
                    "Error: type names should not be this long... "
                    "(even for template-heavy headers) "
                    "typeNameString.length() = %" PRIuPTR " \n",
                    typeNameString.length());
        MLOG_WARN_C(MLOG_UNPARSER, "nodeReferenceToType = %p = %s \n",
                    nodeReferenceToType,
                    nodeReferenceToType->class_name().c_str());
        if (nodeReferenceToType->get_file_info()) {
          nodeReferenceToType->get_file_info()->display(
              "Error: type names should not be this long...: debug");
        }

        ROSE_ABORT();
      }
    }

    // DQ (6/21/2011): Refactored this code for use in
    // traverseTemplatedFunction() PL (10/24/2025): Added guard to protect a
    // type name string from being added to the name map if we're in a template
    // instantiation definition
    if (isContainedInTemplateInstantiationDefn == false) {
      addToNameMap(nodeReferenceToType, type, positionStatement,
                   typeNameString);
    }

    // DQ (2/18/2013): Fixing generation of too many SgUnparse_Info object.
    delete unparseInfoPointer;

  } else {
    // Output a message when we cheat on this IR node (even if this is a clue
    // for George).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 0) || 0
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Skipping precompuation of string for name qualified type = %p = %s \n",
        type, type->class_name().c_str());
#endif
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Ending traversal of type: type = %p = %s \n", type,
              type->class_name().c_str());
#endif
}

void NameQualificationTraversal::traverseTemplatedFunction(
    SgFunctionRefExp *functionRefExp, SgNode *nodeReference,
    SgScopeStatement *currentScope, SgStatement *positionStatement) {
  // Called using
  // traverseTemplatedFunction(functionRefExp,templateInstantiationFunctionDeclaration,currentScope,currentStatement)

  ASSERT_not_null(functionRefExp);
  ASSERT_not_null(nodeReference);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  // MLOG_WARN_C(MLOG_UNPARSER, "Inside of traverseTemplatedFunction
  // functionRefExp = %p currentScope = %p = %s
  // \n",functionRefExp,currentScope,currentScope->class_name().c_str());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Starting traversal of traverseTemplatedFunction "
              "functionRefExp = %p currentScope = %p = %s \n",
              functionRefExp, currentScope, currentScope->class_name().c_str());
#endif

  bool skipThisFunction = false;
  if (skipThisFunction == false) {
    SgTemplateInstantiationFunctionDecl
        *templateInstantiationFunctionDeclaration =
            isSgTemplateInstantiationFunctionDecl(
                functionRefExp->getAssociatedFunctionDeclaration());
    if (templateInstantiationFunctionDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Found a SgTemplateInstantiationFunctionDecl that will have template "
          "arguments that might require qualification. name = %s \n",
          templateInstantiationFunctionDeclaration->get_name().str());
#endif
      evaluateTemplateInstantiationDeclaration(
          templateInstantiationFunctionDeclaration, currentScope,
          positionStatement);
    }

    SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
    ASSERT_not_null(unparseInfoPointer);
    unparseInfoPointer->set_outputCompilerGeneratedStatements();

    // Avoid unpasing the class definition when unparseing the type.
    unparseInfoPointer->set_SkipClassDefinition();

    // DQ (1/13/2014): Set the output of the enum defintion to match that of the
    // class definition (consistancy is now inforced).
    unparseInfoPointer->set_SkipEnumDefinition();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "nodeReference = %p = %s \n", nodeReference,
                nodeReference->class_name().c_str());
#endif
    // Associate the unparsing of this type with the statement or scope where it
    // occures. This is the key to use in the lookup of the qualified name. But
    // this is the correct key....
    // unparseInfoPointer->set_reference_node_for_qualification(positionStatement);
    // unparseInfoPointer->set_reference_node_for_qualification(currentScope);
    unparseInfoPointer->set_reference_node_for_qualification(nodeReference);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "Calling globalUnparseToString() \n");
#endif
    string functionNameString =
        globalUnparseToString(functionRefExp, unparseInfoPointer);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "++++++++++++++++ functionNameString (globalUnparseToString()) = %s \n",
        functionNameString.c_str());
#endif

    // DQ (10/31/2015): Increased the maximum allowable size of function names
    // (because test2015_98.C demonstrates a longer name (length == 5062)). DQ
    // (6/24/2013): Increased upper bound to support ROSE compiling ROSE. This
    // is symptematic of an error which causes the whole class to be included
    // with the class definition.  This was fixed by calling
    // unparseInfoPointer->set_SkipClassDefinition() above. if
    // (functionNameString.length() > 2000) if (functionNameString.length() >
    // 5000)
    if (functionNameString.length() > 10000) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: function names should not be this long... "
                  "functionNameString.length() = %" PRIuPTR " \n",
                  functionNameString.length());
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: function names should not be this long... "
                  "functionNameString          = \n%s \n",
                  functionNameString.c_str());
    }

    // DQ (6/21/2011): Refactored this code for use in
    // traverseTemplatedFunction()
    addToNameMap(nodeReference, functionNameString);

    // DQ (2/18/2013): Fixing generation of too many SgUnparse_Info object.
    delete unparseInfoPointer;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Ending traversal of traverseTemplatedFunction "
              "functionRefExp = %p currentScope = %p = %s \n",
              functionRefExp, currentScope, currentScope->class_name().c_str());
#endif
}

void NameQualificationTraversal::traverseTemplatedMemberFunction(
    SgMemberFunctionRefExp *memberFunctionRefExp, SgNode *nodeReference,
    SgScopeStatement *currentScope, SgStatement *positionStatement) {
  // Called using
  // traverseTemplatedFunction(functionRefExp,templateInstantiationFunctionDeclaration,currentScope,currentStatement)

  ASSERT_not_null(memberFunctionRefExp);
  ASSERT_not_null(nodeReference);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

  // MLOG_WARN_C(MLOG_UNPARSER, "Inside of traverseTemplatedFunction
  // functionRefExp = %p currentScope = %p = %s
  // \n",functionRefExp,currentScope,currentScope->class_name().c_str());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Starting traversal of traverseTemplatedFunction "
              "memberFunctionRefExp = %p currentScope = %p = %s \n",
              memberFunctionRefExp, currentScope,
              currentScope->class_name().c_str());
#endif

  bool skipThisFunction = false;
  if (skipThisFunction == false) {
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDeclaration =
            isSgTemplateInstantiationMemberFunctionDecl(
                memberFunctionRefExp->getAssociatedMemberFunctionDeclaration());
    if (templateInstantiationMemberFunctionDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Found a SgTemplateInstantiationMemberFunctionDecl that will have "
          "template arguments that might require qualification. name = %s \n",
          templateInstantiationMemberFunctionDeclaration->get_name().str());
#endif
      evaluateTemplateInstantiationDeclaration(
          templateInstantiationMemberFunctionDeclaration, currentScope,
          positionStatement);
    }

    SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
    ASSERT_not_null(unparseInfoPointer);
    unparseInfoPointer->set_outputCompilerGeneratedStatements();

    // Avoid unpasing the class definition when unparsing the type.
    unparseInfoPointer->set_SkipClassDefinition();

    // DQ (1/13/2014): Set the output of the enum defintion to match that of the
    // class definition (consistancy is now inforced).
    unparseInfoPointer->set_SkipEnumDefinition();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "nodeReference = %p = %s \n", nodeReference,
                nodeReference->class_name().c_str());
#endif
    // Associate the unparsing of this type with the statement or scope where it
    // occures. This is the key to use in the lookup of the qualified name. But
    // this is the correct key....
    // unparseInfoPointer->set_reference_node_for_qualification(positionStatement);
    // unparseInfoPointer->set_reference_node_for_qualification(currentScope);
    unparseInfoPointer->set_reference_node_for_qualification(nodeReference);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "Calling globalUnparseToString() \n");
#endif
    string memberFunctionNameString =
        globalUnparseToString(memberFunctionRefExp, unparseInfoPointer);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "++++++++++++++++ memberFunctionNameString "
                "(globalUnparseToString()) = %s \n",
                memberFunctionNameString.c_str());
#endif
    // DQ (3/30/2018): Incremented this for ROSE compiling ROSE with
    // template-heavy headers (after bugfix for private types). DQ
    // (12/3/2014): Incremented this for ARES application files. DQ
    // (6/9/2013): I have incremented this value to support mangled names
    // in the protobuf-2.5.0 application. This is symptematic of an error
    // which causes the whole class to be included with the class
    // definition.  This was fixed by calling
    // unparseInfoPointer->set_SkipClassDefinition() above. [Robb Matzke
    // 2018-06-19]: Incremented from 8000 to 9000 because <rose.h> has a
    // name that's 8960 characters, namely "__gnu_cxx::new_allocator<
    // _Rb_tree_node< map< ... >
    //    ::value_type > > ::deallocate".
    // if (memberFunctionNameString.length() > 4000)
    // if (memberFunctionNameString.length() > 8000)
    // if (memberFunctionNameString.length() > 8000)
    if (memberFunctionNameString.length() > 9000) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: function names should not be this long... "
                  "memberFunctionNameString.length() = %" PRIuPTR " \n",
                  memberFunctionNameString.length());
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: function names should not be this long... "
                  "memberFunctionNameString = \n%s \n",
                  memberFunctionNameString.c_str());
      ROSE_ABORT();
    }

    // DQ (6/21/2011): Refactored this code for use in
    // traverseTemplatedFunction()
    addToNameMap(nodeReference, memberFunctionNameString);

    // DQ (2/18/2013): Fixing generation of too many SgUnparse_Info object.
    delete unparseInfoPointer;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Ending traversal of traverseTemplatedMemberFunction "
              "memberFunctionRefExp = %p currentScope = %p = %s \n",
              memberFunctionRefExp, currentScope,
              currentScope->class_name().c_str());
#endif
}

void NameQualificationTraversal::traverseTemplatedClass(
    SgBaseClass *baseClass, SgNode *nodeReference,
    SgScopeStatement *currentScope, SgStatement *positionStatement) {
  // DQ (4/12/2019): Notes on Cxx11_tests/test342.C:
  // It may be that we need a function like "traverseTemplatedFunction" for
  // classes to support the reference to a template class instantiation in a
  // SgBaseClass. The issue is that the template arguments can have arbitrary
  // complexity of name qualification requirements such that we need to save the
  // associtate string generated once after all of the required name
  // qualification (of the template arguments) has been figured out.

  // The point is to save the generated name of the class declaration, correctly
  // name qualified (with all template parameters name qualified) so that it can
  // be used directly (as a string) in the unparsing. The call to
  // "addToNameMap()" is the key part that saves the name of the template
  // instantiation.

  // Currently the base class references the shared template class instantiation
  // (so this might be the root of the problem as well). If sharing is the
  // issue, then generateding a strring to hold the name of the class with
  // ccntext depenent template argument name qualification would be the solution
  // (just as it is for shared types).

  // Called similar to
  // traverseTemplatedFunction(functionRefExp,templateInstantiationFunctionDeclaration,currentScope,currentStatement)

  ASSERT_not_null(baseClass);
  ASSERT_not_null(nodeReference);
  ASSERT_not_null(currentScope);
  ASSERT_not_null(positionStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Starting traversal of traverseTemplatedFunction baseClass "
              "= %p currentScope = %p = %s \n",
              baseClass, currentScope, currentScope->class_name().c_str());
#endif

  bool skipThisClass = false;
  if (skipThisClass == false) {
    SgTemplateInstantiationDecl *templateInstantiationClassDeclaration =
        isSgTemplateInstantiationDecl(baseClass->get_base_class());
    if (templateInstantiationClassDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Found a SgTemplateInstantiationDecl that will have template "
                  "arguments that might require qualification. name = %s \n",
                  templateInstantiationClassDeclaration->get_name().str());
#endif
      evaluateTemplateInstantiationDeclaration(
          templateInstantiationClassDeclaration, currentScope,
          positionStatement);
    }

    SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
    ASSERT_not_null(unparseInfoPointer);
    unparseInfoPointer->set_outputCompilerGeneratedStatements();

    // Avoid unpasing the class definition when unparseing the type.
    unparseInfoPointer->set_SkipClassDefinition();

    // DQ (1/13/2014): Set the output of the enum defintion to match that of the
    // class definition (consistancy is now inforced).
    unparseInfoPointer->set_SkipEnumDefinition();

    // DQ (4/12/2019): This is how we skip the generation of the name with the
    // "template<> struct" specifiers and trailing ";".
    unparseInfoPointer->set_inEmbeddedDecl();
    unparseInfoPointer->set_SkipSemiColon();
    unparseInfoPointer->set_SkipClassSpecifier();

    unparseInfoPointer->set_SkipNameQualification();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "In traverseTemplatedClass(): nodeReference = %p = %s \n",
                nodeReference, nodeReference->class_name().c_str());
#endif

    // DQ (3/13/2019): Not setting the reference_node_for_qualification, may be
    // the best way to make sure we don't get a name qualification prefix.
    // Associate the unparsing of this type with the statement or scope where it
    // occures. This is the key to use in the lookup of the qualified name. But
    // this is the correct key....
    // unparseInfoPointer->set_reference_node_for_qualification(positionStatement);
    // unparseInfoPointer->set_reference_node_for_qualification(currentScope);
    unparseInfoPointer->set_reference_node_for_qualification(nodeReference);
    unparseInfoPointer->set_current_scope(currentScope);

    SgSourceFile *sourceFile = SageInterface::getEnclosingSourceFile(
        templateInstantiationClassDeclaration);
    if (sourceFile == NULL) {
      sourceFile = SageInterface::getEnclosingSourceFile(positionStatement);
    }
    if (sourceFile == NULL) {
      sourceFile = SageInterface::getEnclosingSourceFile(currentScope);
    }
    if (sourceFile != NULL) {
      unparseInfoPointer->set_current_source_file(sourceFile);
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER, "Calling globalUnparseToString() \n");
#endif
    string classNameString = globalUnparseToString(
        templateInstantiationClassDeclaration, unparseInfoPointer);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "++++++++++++++++ classNameString (globalUnparseToString()) = %s \n",
        classNameString.c_str());
#endif

    // DQ (10/31/2015): Increased the maximum allowable size of function names
    // (because test2015_98.C demonstrates a longer name (length == 5062)). DQ
    // (6/24/2013): Increased upper bound to support ROSE compiling ROSE. This
    // is symptematic of an error which causes the whole class to be included
    // with the class definition.  This was fixed by calling
    // unparseInfoPointer->set_SkipClassDefinition() above. if
    // (functionNameString.length() > 2000) if (functionNameString.length() >
    // 5000)
    if (classNameString.length() > 10000) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: class names should not be this long... "
                  "classNameString.length() = %" PRIuPTR " \n",
                  classNameString.length());
    }

    // DQ (6/21/2011): Refactored this code for use in
    // traverseTemplatedFunction()
    addToNameMap(nodeReference, classNameString);

    // DQ (2/18/2013): Fixing generation of too many SgUnparse_Info object.
    delete unparseInfoPointer;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "<<<<< Ending traversal of traverseTemplatedClass baseClass = %p "
              "currentScope = %p = %s \n",
              baseClass, currentScope, currentScope->class_name().c_str());
#endif
}

bool NameQualificationTraversal::
    skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(
        SgDeclarationStatement *declaration) {
  // DQ (6/9/2011): Support for test2011_78.C (we only qualify function call
  // references where the function has been declared in a scope where it could
  // be expected to be defined (e.g. not using a forward declaration in a
  // SgBasicBlock, since the function definition could not live in the
  // SgBasicBlock.

  // DQ (4/27/2019): Added assertion.
  ASSERT_not_null(declaration);

  bool skipNameQualification = false;
  SgDeclarationStatement *declarationToSearchForInReferencedNameSet =
      declaration->get_firstNondefiningDeclaration() != NULL
          ? declaration->get_firstNondefiningDeclaration()
          : declaration;
  ASSERT_not_null(declarationToSearchForInReferencedNameSet);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In "
              "skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefi"
              "nable(): declaration->get_firstNondefiningDeclaration() = %p \n",
              declaration->get_firstNondefiningDeclaration());
  MLOG_WARN_C(MLOG_UNPARSER,
              "   --- declarationToSearchForInReferencedNameSet->get_parent() "
              "= %p = %s \n",
              declarationToSearchForInReferencedNameSet->get_parent(),
              declarationToSearchForInReferencedNameSet->get_parent()
                  ->class_name()
                  .c_str());

  MLOG_WARN_C(MLOG_UNPARSER,
              "   --- declaration                                             "
              "= %p = %s \n",
              declaration, declaration->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER,
              "   --- declaration->get_parent()                               "
              "= %p = %s \n",
              declaration->get_parent(),
              declaration->get_parent()->class_name().c_str());
  if (declaration->get_firstNondefiningDeclaration() != NULL) {
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "   --- declaration ->get_firstNondefiningDeclaration()              = "
        "%p = %s \n",
        declaration->get_firstNondefiningDeclaration(),
        declaration->get_firstNondefiningDeclaration()->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "   --- declaration->get_firstNondefiningDeclaration()->get_parent() = "
        "%p = %s \n",
        declaration->get_firstNondefiningDeclaration()->get_parent(),
        declaration->get_firstNondefiningDeclaration()
            ->get_parent()
            ->class_name()
            .c_str());
  }
  if (declaration->get_definingDeclaration() != NULL) {
    MLOG_WARN_C(MLOG_UNPARSER,
                "   --- declaration ->get_definingDeclaration()                "
                "      = %p = %s \n",
                declaration->get_definingDeclaration(),
                declaration->get_definingDeclaration()->class_name().c_str());
    MLOG_WARN_C(MLOG_UNPARSER,
                "   --- declaration->get_definingDeclaration()->get_parent()   "
                "      = %p = %s \n",
                declaration->get_definingDeclaration()->get_parent(),
                declaration->get_definingDeclaration()
                    ->get_parent()
                    ->class_name()
                    .c_str());
  }
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0
  SgFunctionDeclaration *inputFunctionDeclaration =
      isSgFunctionDeclaration(declaration);
  if (inputFunctionDeclaration != NULL) {
    printf("inputFunctionDeclaration name = %s \n",
           inputFunctionDeclaration->get_name().str());
    SgScopeStatement *scope = inputFunctionDeclaration->get_scope();
    printf("scope = %p = %s \n", scope, scope->class_name().c_str());

    SgName mangledName = inputFunctionDeclaration->get_mangled_name();
    printf("mangledName = %s \n", mangledName.str());

    SgSourceFile *sourceFile =
        SageInterface::getEnclosingNode<SgSourceFile>(inputFunctionDeclaration);
    ROSE_ASSERT(sourceFile != NULL);
    printf("sourceFile = %p name = %s \n", sourceFile,
           sourceFile->getFileName().c_str());
    inputFunctionDeclaration->get_file_info()->display(
        "inputFunctionDeclaration: debug");
  }

  SgFunctionDeclaration
      *inputFunctionDeclarationToSearchForInReferencedNameSet =
          isSgFunctionDeclaration(declarationToSearchForInReferencedNameSet);
  if (inputFunctionDeclarationToSearchForInReferencedNameSet != NULL) {
    printf(
        "inputFunctionDeclarationToSearchForInReferencedNameSet name = %s \n",
        inputFunctionDeclarationToSearchForInReferencedNameSet->get_name()
            .str());
    SgScopeStatement *scope =
        inputFunctionDeclarationToSearchForInReferencedNameSet->get_scope();
    printf("scope = %p = %s \n", scope, scope->class_name().c_str());

    SgName mangledName = inputFunctionDeclarationToSearchForInReferencedNameSet
                             ->get_mangled_name();
    printf("mangledName = %s \n", mangledName.str());

    SgSourceFile *sourceFile = SageInterface::getEnclosingNode<SgSourceFile>(
        inputFunctionDeclarationToSearchForInReferencedNameSet);
    ROSE_ASSERT(sourceFile != NULL);
    printf("sourceFile = %p name = %s \n", sourceFile,
           sourceFile->getFileName().c_str());

    inputFunctionDeclarationToSearchForInReferencedNameSet->get_file_info()
        ->display(
            "inputFunctionDeclarationToSearchForInReferencedNameSet: debug");
  }

  printf("Output referencedNameSet: \n");
  for (NameQualificationSetType::iterator i = referencedNameSet.begin();
       i != referencedNameSet.end(); i++) {
    // printf (" --- referencedNameSet: element: \n");

    // DQ (10/17/2020): There is a NULL entry in the referencedNameSet, this
    // should not exist. ROSE_ASSERT(*i != NULL);

    if (*i != NULL) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "   --- *** referencedNameSet member *i = %p = %s \n", *i,
                  (*i)->class_name().c_str());
      SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(*i);
      if (functionDeclaration != NULL) {
        // DQ (10/17/2020): This is a declaration from likely a different file,
        // but with the same name as what we are searching for. So although I
        // would not want to compare function names, that could be one solution.
        // Either that or identify if these are from a different file.
        printf("   --- *** functionDeclaration name = %s \n",
               functionDeclaration->get_name().str());
        SgScopeStatement *scope = functionDeclaration->get_scope();
        printf("   --- *** scope = %p = %s \n", scope,
               scope->class_name().c_str());

        SgName mangledName = functionDeclaration->get_mangled_name();
        printf("   --- *** mangledName = %s \n", mangledName.str());

        SgSourceFile *sourceFile =
            SageInterface::getEnclosingNode<SgSourceFile>(functionDeclaration);
        ROSE_ASSERT(sourceFile != NULL);
        printf("   --- *** sourceFile = %p name = %s \n", sourceFile,
               sourceFile->getFileName().c_str());

        functionDeclaration->get_file_info()->display(
            "functionDeclaration: debug");
      }
    } else {
      // printf (" --- element = %p \n",*i);
      MLOG_WARN_C(MLOG_UNPARSER,
                  "   --- *** referencedNameSet member *i = %p \n", *i);
    }
  }
#endif

  // DQ (8/18/2012): If this is a template instantiation, then we need to look
  // at where the template declaration is and if IT is defined. See
  // test2009_30.C for an example of this.
  SgTemplateInstantiationFunctionDecl *templateInstantiationFunctionDecl =
      isSgTemplateInstantiationFunctionDecl(declaration);
  if (templateInstantiationFunctionDecl != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In "
        "skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable("
        "): templateInstantiationFunctionDecl->get_name() = %p = %s \n",
        templateInstantiationFunctionDecl,
        templateInstantiationFunctionDecl->get_name().str());
#endif

    // DQ (8/18/2012): Note that test2012_57.C and test2012_59.C have template
    // specalizations that don't appear to have there associated template
    // declaration set properly, issue a warning for now.
    // declarationToSearchForInReferencedNameSet =
    // templateInstantiationFunctionDecl->get_templateDeclaration();
    if (templateInstantiationFunctionDecl->get_templateDeclaration() == NULL) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: "
                  "templateInstantiationFunctionDecl->get_templateDeclaration()"
                  " == NULL for templateInstantiationFunctionDecl = %p = %s \n",
                  templateInstantiationFunctionDecl,
                  templateInstantiationFunctionDecl->get_name().str());
    } else {
      declarationToSearchForInReferencedNameSet =
          templateInstantiationFunctionDecl->get_templateDeclaration();
    }
    ASSERT_not_null(declarationToSearchForInReferencedNameSet);
  } else {
    // Also test for member function.
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDecl =
            isSgTemplateInstantiationMemberFunctionDecl(declaration);
    if (templateInstantiationMemberFunctionDecl != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "In "
          "skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinabl"
          "e(): templateInstantiationMemberFunctionDecl->get_name() = %p = %s "
          "\n",
          templateInstantiationMemberFunctionDecl,
          templateInstantiationMemberFunctionDecl->get_name().str());
#endif
      declarationToSearchForInReferencedNameSet =
          templateInstantiationMemberFunctionDecl->get_templateDeclaration();
      ASSERT_not_null(declarationToSearchForInReferencedNameSet);
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "In "
                  "skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIs"
                  "Definable(): This is not a template function instantation "
                  "(member nor non-member function) \n");
#endif
    }
  }

  // DQ (6/22/2011): This fixes test2011_97.C which only has a defining
  // declaration so that the declaration->get_firstNondefiningDeclaration() was
  // NULL. if
  // (referencedNameSet.find(declaration->get_firstNondefiningDeclaration()) ==
  // referencedNameSet.end())
  if (referencedNameSet.find(declarationToSearchForInReferencedNameSet) ==
      referencedNameSet.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "   --- $$$$$$$$$$ NOT Found: declaration %p = %s in "
                "referencedNameSet referencedNameSet.size() = %" PRIuPTR " \n",
                declaration, declaration->class_name().c_str(),
                referencedNameSet.size());
#endif
    skipNameQualification = true;
  } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "   --- $$$$$$$$$$ FOUND: declaration %p = %s in referencedNameSet \n",
        declaration, declaration->class_name().c_str());
#endif
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "Leaving "
              "skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefi"
              "nable(): skipNameQualification = %s \n",
              skipNameQualification ? "true" : "false");
#endif

  return skipNameQualification;
}

// void NameQualificationTraversal::nameQualificationTypeSupport  ( SgType*
// type, SgScopeStatement* currentScope, SgInitializedName* initializedName,
// SgStatement* currentStatement, SgStatement* positionStatement )
void NameQualificationTraversal::nameQualificationTypeSupport(
    SgType *type, SgScopeStatement *currentScope,
    SgInitializedName *initializedName) {
  // DQ (8/8/2020): this is code refactored from the
  // evaluateInheritedAttribute() function within the SgInitializeName handling.
  // This code support the name qualification of the type associated with a
  // SgInitializedName (it might be useful else where as well).

  // else for type handling.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "Case SgInitializedName: initializedName->get_type(): before "
              "stripType(): type = %p = %s \n",
              type, type->class_name().c_str());
#endif
  // DQ (4/15/2019): Reset the type so that we don't miss the
  // SgPointerMemberType. unsigned char bit_array = SgType::STRIP_MODIFIER_TYPE
  // | SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE |
  //                           SgType::STRIP_POINTER_TYPE  |
  //                           SgType::STRIP_POINTER_TYPE   |
  //                           SgType::STRIP_ARRAY_TYPE;
  unsigned char bit_array =
      SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
      SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
      SgType::STRIP_ARRAY_TYPE;
  type = type->stripType(bit_array);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
  MLOG_WARN_C(MLOG_UNPARSER,
              "Case SgInitializedName: initializedName->get_type(): after "
              "stripType(): type = %p = %s \n",
              type, type->class_name().c_str());
#endif

  SgStatement *currentStatement =
      SageInterface::getEnclosingStatement(initializedName);
  ASSERT_not_null(currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "case of SgInitializedName: currentStatement = %p = %s \n",
              currentStatement, currentStatement->class_name().c_str());
#endif

#if DEBUG_INITIALIZED_NAME
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "################################################################## \n");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "Case SgInitializedName: Processing the SgInitializedName IR's type \n");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "################################################################## \n");
#endif

  // DQ (4/19/2019): It might be that we should call this after the traveral
  // over each type instead of before we traverse the type.
  // traverseType(initializedName->get_type(),initializedName,currentScope,currentStatement);

  // DQ (4/19/2019): Added current statement to paremter list for recursive
  // call. DQ (4/18/2019): I think we need to traverse the type doing a proper
  // type travesal, since it can consist of long chains of types that each must
  // be name qualified. The example of a chain of SgPointerToMemberTypes is the
  // best example of this.
  ASSERT_not_null(currentScope);

  // DQ (4/27/2019): Refactored this code to be outside of the flase block
  // below, so it can be used both there and outside the false branch afterward.
  SgDeclarationStatement *declaration =
      getDeclarationAssociatedWithType(initializedName->get_type());

  // DQ (4/22/2019): If we have resolved the type (after stripType() function)
  // to a SgPointerMemberType, then we need to traverse the type using a type
  // traversal.  Else we can handle it normally.
  // generateNestedTraversalWithExplicitScope(type,currentScope,currentStatement,initializedName);
  // traverseType(initializedName->get_type(),initializedName,currentScope,currentStatement);
  SgPointerMemberType *pointerMemberType = isSgPointerMemberType(type);
  if (pointerMemberType != NULL) {
#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "########################### \n");
    MLOG_WARN_C(MLOG_UNPARSER, "Case SgInitializedName: Calling "
                               "generateNestedTraversalWithExplicitScope() \n");
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "########################### \n");
#endif
    generateNestedTraversalWithExplicitScope(type, currentScope,
                                             currentStatement, initializedName);

    // DQ (4/19/2019): It might be that we should call this after the traveral
    // over each type instead of before we traverse the type. This way we save
    // the correctly computed string for each type after the different parts of
    // name qualificaiton are in place.
    traverseType(initializedName->get_type(), initializedName, currentScope,
                 currentStatement);

#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER,
                "##############################################################"
                "###################################### \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgInitializedName: DONE: Processing the recursive "
                "evaluation of the SgInitializedName IR's type \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "##############################################################"
                "###################################### \n");
#endif
  } else {
#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "Normal processing of type (no recursive call "
                               "to evaluate the type) \n");
#endif
    // The code for the normal processing of the type is below.

#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "########################## \n");
    MLOG_WARN_C(MLOG_UNPARSER, "Case SgInitializedName: Normal Processing the "
                               "SgInitializedName IR's type \n");
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "########################## \n");
#endif

    // DQ (4/27/2019): refactoring: As a result of the block processing of the
    // type being extended, and this variable being used outside of that block,
    // we need to move this variable declaration to be outside of this block at
    // the top of this block. We want to handle types from every where a
    // SgInitializedName might be used. SgDeclarationStatement* declaration =
    // getDeclarationAssociatedWithType(initializedName->get_type());

    // DQ (4/22/2019): If there is a SgPointerMemberType, then don't processs as
    // a normal type.
    if (pointerMemberType != NULL) {
      if (declaration != NULL) {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "None null declaration where detected valid "
                    "SgPointerMemberType: declaration = %p = %s \n",
                    declaration, declaration->class_name().c_str());
      }
      ROSE_ASSERT(declaration == NULL);
    }

    // DQ (4/14/2019): Add support for declType.

    // DQ (4/14/2019): An alternative might be to support this in the
    // getDeclarationAssociatedWithType() function.

    SgDeclType *declType = isSgDeclType(type);
    if (declType != NULL) {
      // Not clear if we need to worry about when the base type of the
      // SgPointerMemberType is a SgDeclType.

      // We need to handle any possible name qualification of a type or
      // SgVarRefExp used as decltype argument.
      SgExpression *baseExpression = declType->get_base_expression();
      SgType *baseType = declType->get_base_type();
      if (baseExpression != NULL) {
        // Need name qualification for expression used in decltype().
        // DQ (8/8/2020): Removed reference to "n".
        // DQ (6/30/2013): Added to support using
        // generateNestedTraversalWithExplicitScope() instead of
        // generateNameQualificationSupport(). SgStatement* currentStatement =
        // SageInterface::getEnclosingStatement(n);
        SgStatement *currentStatement =
            SageInterface::getEnclosingStatement(initializedName);
        // DQ (9/14/2015): Added debugging code.
        // DQ (9/14/2015): This can be an expression in a type, in which case we
        // don't have an associated scope.
        if (currentStatement == NULL) {
          // This can be an expression in a type, in which case we don't have an
          // associated scope.
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "Note: This can be an expression in a type, in which case we "
              "don't have an associated scope: baseExpression = %p = %s \n",
              baseExpression, baseExpression->class_name().c_str());
        } else {
          ASSERT_not_null(currentStatement);
          SgScopeStatement *currentScope = currentStatement->get_scope();
          ASSERT_not_null(currentScope);
          // DQ (6/30/2013): For the recursive call use
          // generateNestedTraversalWithExplicitScope() instead of
          // generateNameQualificationSupport().
          // generateNameQualificationSupport(originalExpressionTree,referencedNameSet);
          generateNestedTraversalWithExplicitScope(baseExpression,
                                                   currentScope);
        }
      } else {
        // Need name qualification for type used in decltype().  Not clear what
        // I good example is of this!
        ASSERT_not_null(baseType);
        declaration = getDeclarationAssociatedWithType(baseType);
      }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Case of SgInitializedName: getDeclarationAssociatedWithType(): type = "
        "%p = %s declaration = %p \n",
        initializedName->get_type(),
        initializedName->get_type()->class_name().c_str(), declaration);
#endif

    // DQ (4/22/2019): The detect of a SgPointerMemberType will force a type
    // traversal, which means we don't process the type as a normal type.  Even
    // if it has a valid declaration. Note: "Normal Type" in the name below
    // means not having pointer to member types and being a type derived from a
    // declaration.
    bool processAsNormalTypeThatMightRequireNameQualification =
        ((pointerMemberType == NULL) && (declaration != NULL));

#if DEBUG_INITIALIZED_NAME || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "processAsNormalTypeThatMightRequireNameQualification = %s \n",
                processAsNormalTypeThatMightRequireNameQualification ? "true"
                                                                     : "false");
#endif
    // if (declaration != NULL)
    if (processAsNormalTypeThatMightRequireNameQualification == true) {
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(initializedName);
      ASSERT_not_null(currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgInitializedName: currentStatement = %p = %s \n",
                  currentStatement, currentStatement->class_name().c_str());
#endif
      SgScopeStatement *currentScope = currentStatement->get_scope();
      ASSERT_not_null(currentScope);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgInitializedName: currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
#endif
      // DQ (2/21/2019): The constructor initializers need to start their name
      // qualification from the class declaration. bool debugging = false;
      SgCtorInitializerList *ctorInitializerList =
          isSgCtorInitializerList(initializedName->get_parent());
      if (ctorInitializerList != NULL) {
        SgClassDefinition *classDefinition =
            isSgClassDefinition(initializedName->get_scope());
        if (classDefinition != NULL) {
          SgClassDeclaration *classDeclaration =
              classDefinition->get_declaration();
          ASSERT_not_null(classDeclaration);

          currentScope = classDeclaration->get_scope();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Found case of SgInitializedName in constructor "
                      "preinitialization list: currentScope = %p = %s \n",
                      currentScope, currentScope->class_name().c_str());
#endif
        }
        // debugging = true;
      }

      // int amountOfNameQualificationRequiredForType =
      // nameQualificationDepthForType(initializedName,currentScope,currentStatement);
      // int amountOfNameQualificationRequiredForType =
      // nameQualificationDepthForType(initializedName,currentStatement);
      int amountOfNameQualificationRequiredForType =
          nameQualificationDepthForType(initializedName, currentScope,
                                        currentStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgInitializedName's (%s) type: "
                  "amountOfNameQualificationRequiredForType = %d \n",
                  initializedName->get_name().str(),
                  amountOfNameQualificationRequiredForType);
#endif
      // Name-qualification for referenced types is also gated by
      // skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(),
      // which consults referencedNameSet. Types declared in scopes we don't
      // traverse during unparsing (common for system headers) won't be present
      // there, so record the representative declaration when we see the type
      // reference.
      SgDeclarationStatement *declarationForReferencedNameSet =
          declaration->get_firstNondefiningDeclaration();
      if (declarationForReferencedNameSet == NULL) {
        declarationForReferencedNameSet =
            declaration->get_definingDeclaration();
        if (declarationForReferencedNameSet == NULL) {
          declarationForReferencedNameSet = declaration;
          ASSERT_not_null(declarationForReferencedNameSet);
        }
        ASSERT_not_null(declarationForReferencedNameSet);
      }
      ASSERT_not_null(declarationForReferencedNameSet);

      SgScopeStatement *scopeOfDeclaration =
          isSgScopeStatement(declarationForReferencedNameSet->get_parent());
      bool acceptableDeclarationScope =
          (scopeOfDeclaration != NULL &&
           scopeOfDeclaration->variantT() != V_SgBasicBlock);

      if (acceptableDeclarationScope == true &&
          should_preseed_referenced_name_for_untraversed_declaration(
              currentStatement, declarationForReferencedNameSet) &&
          referencedNameSet.find(declarationForReferencedNameSet) ==
              referencedNameSet.end()) {
        referencedNameSet.insert(declarationForReferencedNameSet);
      }

      // DQ (8/4/2012): This is redundant code with where the SgInitializedName
      // appears in the SgVariableDeclaration.
      // **************************************************
      // DQ (8/4/2012): The type being used might not have to be qualified if it
      // is associated with a SgClassDeclaration that has not been defined yet.
      // This fixes test2012_165.C.
      // **************************************************
      bool skipGlobalNameQualification =
          skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(
              declaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgInitializedName: currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
#endif
      // DQ (8/4/2012): However, this quasi-pathological case does not apply to
      // template instantiations (only non-template classes or maybe named types
      // more generally?).  Handle template declarations similarly. OR enum
      // declarations (since they can have a forward declaration (except that
      // this is a common languae extension...).
      if (isSgClassDeclaration(declaration) != NULL ||
          isSgTemplateInstantiationDecl(declaration) != NULL ||
          isSgEnumDeclaration(declaration) != NULL ||
          isSgNonrealDecl(declaration) != NULL) {
        // Do the regularly schedule name qualification for these cases.
        skipGlobalNameQualification = false;
      } else {
        // Look back through the scopes and see if we are in a template
        // instantiation or template scope, if so then do the regularly
        // scheduled name qualification.

        SgScopeStatement *scope = declaration->get_scope();
        // MLOG_WARN_C(MLOG_UNPARSER, "case of SgInitializedName: scope = %p =
        // %s \n",scope,scope->class_name().c_str());
        int distanceBackThroughScopes =
            amountOfNameQualificationRequiredForType;
        // MLOG_WARN_C(MLOG_UNPARSER, "case of SgInitializedName:
        // distanceBackThroughScopes = %d \n",distanceBackThroughScopes);
        while (distanceBackThroughScopes > 0 && scope != NULL) {
          // Traverse backwards through the scopes checking for a
          // SgTemplateClassDefinition scope If we traverse off the end of
          // SgGlobal then the amountOfNameQualificationRequiredForType value
          // was trying to trigger global qualification, so this is not a
          // problem. We at least need isSgTemplateInstantiationDefn, not clear
          // about isSgTemplateClassDefinition.

          if (isSgTemplateInstantiationDefn(scope) != NULL ||
              isSgTemplateClassDefinition(scope) != NULL) {
            skipGlobalNameQualification = false;
          }

          ASSERT_not_null(scope);
          scope = scope->get_scope();

          distanceBackThroughScopes--;
        }
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Test of Type used in SgInitializedName: declaration = %p = "
                  "%s skipGlobalNameQualification = %s \n",
                  declaration, declaration->class_name().c_str(),
                  skipGlobalNameQualification ? "true" : "false");
#endif
      // DQ (4/26/2019): Need to call setNameQualificationForType so that we can
      // save the name qualification string using the SgInitializedName as the
      // key. DQ (8/4/2012): Added support to permit global qualification be be
      // skipped explicitly (see test2012_164.C and test2012_165.C for examples
      // where this is important).
      // setNameQualification(initializedName,declaration,amountOfNameQualificationRequiredForType,skipGlobalNameQualification);
      setNameQualificationOnType(initializedName, declaration,
                                 amountOfNameQualificationRequiredForType,
                                 skipGlobalNameQualification);

#if DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "@@@@@@@@@ Calling traverseType() to save type as string if it is "
          "contained types that would be shared AND name qualified \n");
#endif
      // DQ (4/27/2019): I think we need to call this function to handle the
      // name qualification on template arguments where the type is a template
      // instantiation.
      SgStatement *associatedStatement = currentScope;

      SgNode *initializedNameParent = initializedName->get_parent();
      ASSERT_not_null(initializedNameParent);

#if DEBUG_INITIALIZED_NAME || 0
      MLOG_WARN_C(MLOG_UNPARSER, "initializedNameParent = %p = %s \n",
                  initializedNameParent,
                  initializedNameParent->class_name().c_str());
#endif
      bool skipTraverseType = false;
      SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(initializedNameParent);
      if (variableDeclaration != NULL) {
#if DEBUG_INITIALIZED_NAME
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "variableDeclaration->get_"
            "variableDeclarationContainsBaseTypeDefiningDeclaration() = %s \n",
            variableDeclaration
                    ->get_variableDeclarationContainsBaseTypeDefiningDeclaration()
                ? "true"
                : "false");
#endif
        // DQ (5/26/2019): I think this should be the reversed.
        // skipTraverseType =
        // variableDeclaration->get_variableDeclarationContainsBaseTypeDefiningDeclaration();
        if (variableDeclaration
                ->get_variableDeclarationContainsBaseTypeDefiningDeclaration() ==
            true) {
          skipTraverseType = true;
        } else {
          // The name of the type will have to be output!
        }
      } else {
#if DEBUG_INITIALIZED_NAME
        MLOG_WARN_C(MLOG_UNPARSER, "initializedNameParent = %p = %s \n",
                    initializedNameParent,
                    initializedNameParent->class_name().c_str());
#endif
      }

#if DEBUG_INITIALIZED_NAME || 0
      MLOG_WARN_C(MLOG_UNPARSER, "skipTraverseType = %s \n",
                  skipTraverseType ? "true" : "false");
      MLOG_WARN_C(MLOG_UNPARSER, "initializedName->get_type() = %p = %s \n",
                  initializedName->get_type(),
                  initializedName->get_type()->class_name().c_str());
#endif
      // MLOG_WARN_C(MLOG_UNPARSER, "Calling traverseType ALWAYS! \n");
      if (skipTraverseType == false) {
        traverseType(initializedName->get_type(), initializedName, currentScope,
                     associatedStatement);
      }
    } else {
      // DQ (8/23/2014): This case is demonstrated by test2014_145.C. where a
      // SgInitializedName is used in a SgArrayType. However, it would provide
      // greater symetry to handle the SgInitializedName objects in the
      // processing of the SgFunctionParameterList similar to how they are
      // handling in the SgVariableDeclaration.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Case of SgInitializedName: getDeclarationAssociatedWithType() == "
          "NULL (this not associated with a type)  \n");
#endif

      // DQ (4/10/2019): I think we need to call this function to handle the
      // name qualification on template arguments where the type is a template
      // instantiation. DQ (8/23/2014): Adding this to support SgInitializedName
      // in SgArrayType in function parameter lists. SgDeclarationStatement*
      // associatedDeclaration = NULL;

      // DQ (8/8/2020): This is passed in as a function parameter.
      // SgScopeStatement* currentScope = inheritedAttribute.get_currentScope();
      ASSERT_not_null(currentScope);

      SgStatement *associatedStatement = currentScope;

      // int amountOfNameQualificationRequiredForType =
      // nameQualificationDepthForType(initializedName,currentScope,associatedDeclaration);
      int amountOfNameQualificationRequiredForType =
          nameQualificationDepthForType(initializedName, currentScope,
                                        associatedStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgInitializedName's type: "
                  "amountOfNameQualificationRequiredForType = %d \n",
                  amountOfNameQualificationRequiredForType);
#endif
      // bool skipGlobalNameQualification = false;
      // setNameQualificationOnType(initializedName,declaration,amountOfNameQualificationRequiredForType,skipGlobalNameQualification);
      // setNameQualificationOnType(initializedName,associatedStatement,amountOfNameQualificationRequiredForType,skipGlobalNameQualification);
      // DQ (4/27/2019): This will avoid over use of the generated string
      // mechanism to represent types, but we need to allow template
      // instantiations to be processed. DQ (4/10/2019): I think we need to call
      // this function to handle the name qualification on template arguments
      // where the type is a template instantiation.
      // traverseType(initializedName->get_type(),initializedName,currentScope,associatedStatement);
      if (amountOfNameQualificationRequiredForType > 0) {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Calling traverseType() on initializedName->get_type(): "
                    "amountOfNameQualificationRequiredForType > 0: "
                    "amountOfNameQualificationRequiredForType = %d Calling "
                    "traverseType() \n",
                    amountOfNameQualificationRequiredForType);
        traverseType(initializedName->get_type(), initializedName, currentScope,
                     associatedStatement);
      }
    }

#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "######################### \n");
    MLOG_WARN_C(MLOG_UNPARSER, "Case SgInitializedName: DONE: Processing the "
                               "SgInitializedName IR's type \n");
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "######################### \n");
#endif

    // DQ (4/27/2019): This is the new location of the end of the block to
    // process the SgInitializedName's type.
  }

  // endif for type handling.
}

NameQualificationInheritedAttribute
NameQualificationTraversal::evaluateInheritedAttribute(
    SgNode *n, NameQualificationInheritedAttribute inheritedAttribute) {
  ASSERT_not_null(n);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "\n\n****************************************************** \n");
  MLOG_WARN_C(MLOG_UNPARSER,
              "****************************************************** \n");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "Inside of NameQualificationTraversal::evaluateInheritedAttribute(): "
      "node = %p = %s = %s \n",
      n, n->class_name().c_str(), SageInterface::get_name(n).c_str());
  MLOG_WARN_C(MLOG_UNPARSER,
              "****************************************************** \n");
#endif

  // DQ (8/14/2025): This is an optimization to skip the traversal of the AST
  // outside of what is in the source tree.
  if (suppressNameQualificationAcrossWholeTranslationUnit == true) {
    // SgStatement* statement = isSgStatement(n);
    SgLocatedNode *locatedNode = isSgLocatedNode(n);
    // if (statement != NULL)
    if (locatedNode != NULL) {
      // DQ (8/14/2025): Adding support to count the number of statements
      // traversed in the name qualification when using traverseInputFile(). It
      // should be only the statements in the source file, but it appears to
      // include statements marked as compilerGenerated.
      AstPerformance::
          numberOfStatementsProcessedInNameQualificationUsingTraverseInputFile++;

      const bool locatedIsGenerated = locatedNode->isCompilerGenerated();
      const bool locatedIsOutput =
          locatedNode->get_file_info() != nullptr &&
          locatedNode->get_file_info()->isOutputInCodeGeneration();

      // We could just check is the nearest parent statement is compiler
      // generated. Or we could see if this is from a header file...(let's not
      // do that).
      SgStatement *statement =
          SageInterface::getEnclosingStatement(locatedNode);
      if (statement != nullptr) {
        const bool statementIsGenerated = statement->isCompilerGenerated();
        const bool statementIsOutput =
            statement->get_file_info() != nullptr &&
            statement->get_file_info()->isOutputInCodeGeneration();
        if (locatedIsGenerated && !locatedIsOutput && statementIsGenerated &&
            !statementIsOutput) {
          return NameQualificationInheritedAttribute(inheritedAttribute);
        }
        if (!locatedIsGenerated && statementIsGenerated && !statementIsOutput) {
          return NameQualificationInheritedAttribute(inheritedAttribute);
        }
      } else if (locatedIsGenerated && !locatedIsOutput) {
        return NameQualificationInheritedAttribute(inheritedAttribute);
      }
    }
  }

#if DEBUG_NONTERMINATION
  // DQ (5/3/2024): Debugging non-terminating name qualification case in unit
  // testing.
  printf("In evaluateInheritedAttribute(): n = %p = %s \n", n,
         n->class_name().c_str());
  Sg_File_Info *tmp_fileInfo = n->get_file_info();
  if (tmp_fileInfo != NULL) {
    printf("NameQualificationTraversal: --- n = %p = %s line %d col = %d file "
           "= %s \n",
           n, n->class_name().c_str(), tmp_fileInfo->get_line(),
           tmp_fileInfo->get_col(), tmp_fileInfo->get_filenameString().c_str());
  }
#endif

  SgSourceFile *sourceFile = isSgSourceFile(n);
  if (sourceFile != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    printf("In evaluateInheritedAttribute(): sourceFile = %p = %s \n",
           sourceFile, sourceFile->getFileName().c_str());
#endif
  }

  // DQ (5/24/2013): Allow the current scope to be tracked from the traversal of
  // the AST instead of being computed at each IR node which is a problem for
  // template arguments. See test2013_187.C for an example of this.
  SgScopeStatement *evaluateInheritedAttribute_currentScope =
      isSgScopeStatement(n);
  if (evaluateInheritedAttribute_currentScope != NULL) {
    inheritedAttribute.set_currentScope(
        evaluateInheritedAttribute_currentScope);
  }

  // DQ (5/24/2013): We can't set the current scope until we at first get past
  // the SgProject and SgSourceFile IR nodes in the AST traversal.
  if (isSgSourceFile(n) == NULL && isSgProject(n) == NULL) {
    // DQ (5/25/2013): This only appears to fail for test2013_63.C.
    if (inheritedAttribute.get_currentScope() == NULL) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: In "
                  "NameQualificationTraversal::evaluateInheritedAttribute(): "
                  "inheritedAttribute.get_currentScope() == NULL: node = %p = "
                  "%s = %s \n",
                  n, n->class_name().c_str(),
                  SageInterface::get_name(n).c_str());
    }
    // ASSERT_not_null(inheritedAttribute.get_currentScope());
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // Extra information about the location of the current node.
  Sg_File_Info *fileInfo = n->get_file_info();
  if (fileInfo != NULL) {
    MLOG_WARN_C(MLOG_UNPARSER,
                "NameQualificationTraversal: --- n = %p = %s line %d col = %d "
                "file = %s \n",
                n, n->class_name().c_str(), fileInfo->get_line(),
                fileInfo->get_col(), fileInfo->get_filenameString().c_str());
  }
#endif

  // Locations where name qualified references can exist:
  //   1) Base class names
  //   2) Variable names in declarations (see test2011_30.C)
  //   3) Types referenced by variables
  //   4) Types referenced in function parameter lists
  //   5) Return types referenced by functions (including covariant types for
  //   member functions) 6) References to functions thrown by functions 7)
  //   Namespaces referenced by SgUsingDirectiveStatement IR nodes 8) Variables
  //   and declarations reference from SgUsingDeclarationStatement IR nodes 9)
  //   Functions reference by SgFunctionRefExp IR nodes
  //  10) Functions reference by SgMemberFunctionRefExp IR nodes
  //  11) Variable reference by SgVarRefExp IR nodes
  //  12) Template arguments (and default template parameter specifications)
  //  13) Template parameters?
  //  14) Function declarations
  //  15) Member function declarations
  //  16) Typedef declarations
  //  17) Throw exception lists
  //  18) A number of expressions (listed below)
  //         SgVarRefExp
  //         SgFunctionRefExp
  //         SgMemberFunctionRefExp
  //         SgConstructorInitializer
  //         SgNewExp
  //         SgCastExp
  //         SgSizeOfOp
  //         SgTypeIdOp
  //  19) SgVarRefExp's hidden in array types (SgArrayType) (requires explicitly
  //  specified current scope). 20)

  // The use of name qualification in types is a complicated because types are
  // shared and the same type can have it's template arguments qualified
  // differently depending on where it is referenced.  This is an issue for all
  // references to types containing template arguments and not just where
  // SgInitializedName are used. Since name qualification of the same type can
  // only vary at most from statement to statement in some cases likely only
  // from scope to scope) we need only associate names to statements (see note
  // 1). I would like to for now use scopes as the finest level of resolution.
  // The solution:
  //    1) Support a test for which types are effected.  a member function of
  //    SgType will evaluate if
  //       a type uses template arguments or subtypes using template arguments
  //       and if these could require name qualification.
  //    2) A map will be created in each scope (or maybe statement) for types
  //    used in that scope (or statement)
  //       which will store the computed name of the type (evaluated as part of
  //       the name qualification support; called immediately at the start of
  //       the unparsing of each SgFile).  The SgType pointer will be used as
  //       the key into the map of SgType to names (stored as strings).
  //    3) The unparser will check for entries in the associated map and use the
  //    stringified type names if they
  //       are available.  This can be done at the top level of the
  //       unparseType() function.

  // Note 1: A forward declaraion of a function (and maybe a class) can happen
  // in a scope that does not allow a defining declaration and when this happens
  // the name qualification of that function is undefined.  However after a
  // forward declaration in a scope permitting a defining declaration, the
  // function name must be qualified as per usual name qualification rules.

  // DQ (8/4/2012): It is too complex to add this declaration support here (use
  // the previous code and just handle the specific cases where we need to add a
  // declaration to the reference set separately. DQ (8/4/2012): Let any
  // procesing define a declaration to be used for the reference set. Ititially
  // it is NULL, but specific cases can set this so that the associated
  // declaration we be recorded as referenced.  This is important for
  // test2012_164.C, where a variable declaration generates a reference to a
  // type that at first must not use a qualified name (since there is no
  // explicit forward declaration for the type (a class/struct).  This is a
  // facinating case since the scope of the declaration is the outer namespace
  // from where it is first implicitly referenced via a variable declaration.
  // This is part of debugging test2005_133.C (of which test2012_16[3-5].C are
  // simpler cases. SgDeclarationStatement* declarationForReferencedNameSet =
  // NULL;

  // DQ (6/11/2011): This is a new IR nodes, but the use of it causes a few
  // problems (test2004_109.C) because the source position is not computed
  // correctly (I think).
  SgTemplateClassDefinition *templateClassDefinition =
      isSgTemplateClassDefinition(n);
  if (templateClassDefinition != NULL) {

    // DQ (11/20/2011): Commented out this assertion.
    // ROSE_ABORT();
  }

  SgClassDefinition *classDefinition = isSgClassDefinition(n);
  // if (classDefinition != NULL && templateClassDefinition == NULL)
  if (classDefinition != NULL) {
    // Add all of the named types from this class into the set that have already
    // been seen. Note that this should not include nested classes (I think).

    SgBaseClassPtrList &baseClassList = classDefinition->get_inheritances();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "!!!!! Evaluate the derived classes: are they visible --- "
                "baseClassList.size() = %" PRIuPTR " \n",
                baseClassList.size());
#endif

    SgBaseClassPtrList::iterator i = baseClassList.begin();
    while (i != baseClassList.end()) {

#define DEBUG_BASE_CLASS_SUPPORT 0

      // Check each base class.
      SgBaseClass *baseClass = *i;
      ASSERT_not_null(baseClass);

      if (isSgNonrealBaseClass(baseClass)) {
        // FIXME nothing to do?
      } else if (isSgExpBaseClass(baseClass)) {
        ROSE_ABORT(); // TODO traverse the expression ???
      } else {
        SgClassDeclaration *classDeclaration = baseClass->get_base_class();
        ASSERT_not_null(classDeclaration);
        SgScopeStatement *currentScope = classDefinition->get_scope();
        ASSERT_not_null(currentScope);

        // Name these better to be more clear.
        SgClassDeclaration *derivedClassDeclaration =
            classDefinition->get_declaration();
        if (SgClassDeclaration *firstNondefining = isSgClassDeclaration(
                derivedClassDeclaration->get_firstNondefiningDeclaration())) {
          derivedClassDeclaration = firstNondefining;
#if DEBUG_BASE_CLASS_SUPPORT
          MLOG_WARN_C(MLOG_UNPARSER,
                      "RESET derivedClassDeclaration to "
                      "firstNondefiningDeclaration: %p \n",
                      derivedClassDeclaration);
#endif
        }
        // SgClassDeclaration* baseClassDeclaration    = classDeclaration;
        SgClassDeclaration *baseClassDeclaration = baseClass->get_base_class();
        if (SgClassDeclaration *firstNondefining = isSgClassDeclaration(
                baseClassDeclaration->get_firstNondefiningDeclaration())) {
          baseClassDeclaration = firstNondefining;
#if DEBUG_BASE_CLASS_SUPPORT
          MLOG_WARN_C(MLOG_UNPARSER,
                      "RESET baseClassDeclaration to "
                      "firstNondefiningDeclaration: %p \n",
                      baseClassDeclaration);
#endif
        }

        ASSERT_not_null(derivedClassDeclaration);
        ASSERT_not_null(baseClassDeclaration);

        // DQ (1/24/2019): Build a list of private base classes and accumulate
        // them from any base classes. This is important to support additional
        // name qualification required when derived classes reference a nested
        // base class that may be private.
        SgBaseClassModifier *baseClassDeclarationBaseClassModifier =
            baseClass->get_baseClassModifier();
        ASSERT_not_null(baseClassDeclarationBaseClassModifier);
        SgAccessModifier &baseClassDeclarationAccessModifier =
            baseClassDeclarationBaseClassModifier->get_accessModifier();
#if DEBUG_BASE_CLASS_SUPPORT
        MLOG_WARN_C(MLOG_UNPARSER,
                    "  --- derivedClassDeclaration = %p name = %s \n",
                    derivedClassDeclaration,
                    SageInterface::get_name(derivedClassDeclaration).c_str());
        MLOG_WARN_C(MLOG_UNPARSER,
                    "  --- baseClassDeclaration    = %p name = %s \n",
                    baseClassDeclaration,
                    SageInterface::get_name(baseClassDeclaration).c_str());
        MLOG_WARN_C(MLOG_UNPARSER,
                    "  --- classDeclarationAccessModifier = %s \n",
                    baseClassDeclarationAccessModifier.displayString().c_str());

        MLOG_WARN_C(MLOG_UNPARSER,
                    "  --- privateBaseClassSets.size()  = %zu \n",
                    privateBaseClassSets.size());
        MLOG_WARN_C(MLOG_UNPARSER,
                    "  --- inaccessibleClassSets.size() = %zu \n",
                    inaccessibleClassSets.size());
#endif
        if (baseClassDeclarationAccessModifier.isPrivate() == true) {
#if DEBUG_BASE_CLASS_SUPPORT
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Found private derivation of baseClassDeclaration = %s "
                      "from derivedClassDeclaration = %s \n",
                      baseClassDeclaration->get_name().str(),
                      derivedClassDeclaration->get_name().str());
#endif
          if (privateBaseClassSets.find(derivedClassDeclaration) !=
              privateBaseClassSets.end()) {
#if DEBUG_BASE_CLASS_SUPPORT
            MLOG_WARN_C(MLOG_UNPARSER,
                        "privateBaseClassSets has an entry for this class: "
                        "derivedClassDeclaration = %p = %s name = %s \n",
                        derivedClassDeclaration,
                        derivedClassDeclaration->class_name().c_str(),
                        derivedClassDeclaration->get_name().str());
#endif
          } else {
            std::set<SgClassDeclaration *> privateClasses;
            privateClasses.insert(baseClassDeclaration);
            privateBaseClassSets.insert(
                std::pair<SgClassDeclaration *, std::set<SgClassDeclaration *>>(
                    derivedClassDeclaration, privateClasses));
          }
        }

        // Build up the inaccessibleClassSets map.
        if (privateBaseClassSets.find(baseClassDeclaration) !=
            privateBaseClassSets.end()) {
          std::set<SgClassDeclaration *> &privateBaseClasses =
              privateBaseClassSets[baseClassDeclaration];
#if DEBUG_BASE_CLASS_SUPPORT
          MLOG_WARN_C(MLOG_UNPARSER,
                      "In base class: find the set of private base classes: "
                      "privateBaseClasses.size() = %zu \n",
                      privateBaseClasses.size());
#endif
          std::set<SgClassDeclaration *>::iterator j =
              privateBaseClasses.begin();
          while (j != privateBaseClasses.end()) {
            SgClassDeclaration *privateBaseClassDeclaration = *j;
            ASSERT_not_null(privateBaseClassDeclaration);
#if DEBUG_BASE_CLASS_SUPPORT
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "  --- privateBaseClassDeclaration = %p = %s name = %s \n",
                privateBaseClassDeclaration,
                privateBaseClassDeclaration->class_name().c_str(),
                privateBaseClassDeclaration->get_name().str());
#endif
            std::set<SgClassDeclaration *> privateClasses;
            privateClasses.insert(privateBaseClassDeclaration);
            inaccessibleClassSets.insert(
                std::pair<SgClassDeclaration *, std::set<SgClassDeclaration *>>(
                    derivedClassDeclaration, privateClasses));

            j++;
          }
        } else {
#if DEBUG_BASE_CLASS_SUPPORT
          MLOG_WARN_C(MLOG_UNPARSER,
                      "baseClassDeclaration = %p = %s name = %s : has no "
                      "recorded private base classes \n",
                      baseClassDeclaration,
                      baseClassDeclaration->class_name().c_str(),
                      baseClassDeclaration->get_name().str());
#endif
        }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Calling nameQualificationDepth() for base class "
                    "baseClassDeclaration name = %s \n",
                    baseClassDeclaration->get_name().str());
#endif
        int amountOfNameQualificationRequired = nameQualificationDepth(
            baseClassDeclaration, currentScope, classDefinition);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "amountOfNameQualificationRequired (base class) = %d \n",
                    amountOfNameQualificationRequired);
#endif

        // DQ (4/12/2019): We need this name qualification, but we need to avoid
        // redundant name qualification.
        setNameQualification(baseClass, baseClassDeclaration,
                             amountOfNameQualificationRequired);
      }

      // DQ (12/23/2015): Also need to add this to the aliasSymbolCausalNodeSet.
      SgSymbolTable::insert_aliasSymbolCausalNodeSet(baseClass);

      // DQ (4/12/2019): New code to uniformally support template instantiations
      // that are referenced (shared), where they are shared.

      // DQ (4/12/2019): If this is a templated class then we have to save the
      // name because its templated name might have template arguments that
      // require name qualification.
      ASSERT_not_null(baseClass);
      // SgTemplateInstantiationDecl* templateInstantiationClassDeclaration =
      // isSgTemplateInstantiationDecl(*i);
      SgTemplateInstantiationDecl *templateInstantiationClassDeclaration =
          isSgTemplateInstantiationDecl(baseClass->get_base_class());
      if (templateInstantiationClassDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Found a SgTemplateInstantiationDecl that will have template "
            "arguments that might require qualification. name = %s \n",
            templateInstantiationClassDeclaration->get_name().str());
#endif
        // DQ (4/12/2019): When this is a function call in an array type index
        // expression we can't identify an associated statement.
        SgStatement *currentStatement =
            SageInterface::getEnclosingStatement(baseClass);
        // ASSERT_not_null(currentStatement);
        if (currentStatement != NULL) {
          SgScopeStatement *currentScope = currentStatement->get_scope();
          ASSERT_not_null(currentScope);

          // traverseTemplatedFunction(functionRefExp,templateInstantiationFunctionDeclaration,currentScope,currentStatement);
          // traverseTemplatedFunction(functionRefExp,functionRefExp,currentScope,currentStatement);
          traverseTemplatedClass(baseClass, baseClass, currentScope,
                                 currentStatement);
        } else {
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Note: Name qualification: parent statement could not be "
                      "identified for baseClass = %p = %s \n",
                      baseClass, baseClass->class_name().c_str());
        }
      }

      i++;
    }
  }

  // DQ (6/16/2017): It might be that this case should exclue the case of a
  // SgTemplateInstantiationDecl so that it can be processed below. Handle
  // references to SgMemberFunctionDeclaration...
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(n);
  if (classDeclaration != NULL) {
    // Could it be that we only want to do this for the defining declaration?
    // No, since prototypes must also use name qualification!

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "In name qualification:\n");
    MLOG_WARN_C(MLOG_UNPARSER, " - classDeclaration   = %p = %s \n",
                classDeclaration, classDeclaration->class_name().c_str());
    MLOG_WARN_C(MLOG_UNPARSER, " -     ->get_parent() = %p = %s \n",
                classDeclaration->get_parent(),
                classDeclaration->get_parent()
                    ? classDeclaration->get_parent()->class_name().c_str()
                    : "");
#endif

    // We need the structural location in scope (not the semantic one).
    SgScopeStatement *currentScope =
        isSgScopeStatement(classDeclaration->get_parent());

    // DQ (6/16/2017): Handle the case of a template instantiation directive
    // containing a template instantiation (which is also a SgClassDeclaration).
    if (currentScope == NULL) {
      // Check if this is a template class instantiation that is part of a
      // template instantiation directive
      SgTemplateInstantiationDirectiveStatement
          *templateInstantiationDirectiveStatement =
              isSgTemplateInstantiationDirectiveStatement(
                  classDeclaration->get_parent());
      if (templateInstantiationDirectiveStatement != NULL) {
        // currentScope is that of the parent of the
        // templateInstantiationDirectiveStatement
        currentScope = isSgScopeStatement(
            templateInstantiationDirectiveStatement->get_parent());
        // I think this has to be true.
        ASSERT_not_null(currentScope);
      } else {
        // DQ (2/18/2019): Adding support for when the SgClassDeclaration is
        // defined in another declaration (e.g. SgTypedefDeclaration).
        SgNode *parent = classDeclaration->get_parent();
        SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(parent);
        // currentScope = isSgScopeStatement(typedefDeclaration->get_parent());
        if (typedefDeclaration != NULL) {
          currentScope = isSgScopeStatement(typedefDeclaration->get_parent());

          // DQ (2/18/2019): We should have a valid currentScope at this point.
          if (currentScope == NULL) {
            MLOG_WARN_C(MLOG_UNPARSER,
                        "NOTE: Could not identify scope for class declaration: "
                        "parent = %p = %s \n",
                        parent, parent->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
            ROSE_ABORT();
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Found SgClassDeclaration in SgTypedefDeclaration: "
                        "currentScope = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
#endif
          }
        } else {
          // DQ (2/19/2019): This is frequently a SgLambdaExp or a
          // SgVariableDeclaration Computing the current scope does not always
          // seem possible.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "In name qualification: Cannot compute a valid scope for "
                      "the classDeclaration = %p = %s \n",
                      classDeclaration, classDeclaration->class_name().c_str());
          MLOG_WARN_C(MLOG_UNPARSER, " --- parent = %p = %s \n", parent,
                      parent->class_name().c_str());
#endif
        }
      }
    }

    // ASSERT_not_null(currentScope);
    if (currentScope != NULL) {
      // Only use name qualification where the scopes of the declaration's use
      // (currentScope) is not the same as the scope of the class declaration.
      // However, the analysis should work and determin that the required name
      // qualification length is zero.

      // DQ (7/22/2017): Refactored this code.
      SgScopeStatement *class_scope = classDeclaration->get_scope();

      // DQ (7/22/2017): I think we can assert this.
      ASSERT_not_null(class_scope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "currentScope                  = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "classDeclaration->get_scope() = %p = %s \n",
                  class_scope, class_scope->class_name().c_str());
#endif
      if (currentScope != class_scope &&
          !SgScopeStatement::isEquivalentScope(currentScope, class_scope)) {
        // DQ (6/11/2013): Added test to make sure that name qualification is
        // ignored for friend function where the class has not yet been seen. if
        // (classDeclaration->get_declarationModifier().isFriend() == false)
        SgDeclarationStatement *declarationForReferencedNameSet =
            classDeclaration->get_firstNondefiningDeclaration();
        ASSERT_not_null(declarationForReferencedNameSet);
        if (referencedNameSet.find(declarationForReferencedNameSet) !=
            referencedNameSet.end()) {
          int amountOfNameQualificationRequired = nameQualificationDepth(
              classDeclaration, currentScope, classDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "SgClassDeclaration: amountOfNameQualificationRequired = %d \n",
              amountOfNameQualificationRequired);
#endif
          setNameQualification(classDeclaration,
                               amountOfNameQualificationRequired);
        } else {
          // DQ (2/12/2019): This branch is taken within
          // Cxx11_tests/test2019_107.C where the associated
          // SgTemplateInstantiationDecl is a specialization and does require
          // name qualification.
          const bool preserve_semantic_outer_qualification =
              classDeclaration->get_parent() != class_scope &&
              classDeclaration->get_scope() == class_scope;
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "This classDeclaration has not been seen before so skip "
                      "the name qualification \n");
#endif
          // DQ (2/12/2019): If this is a SgTemplateInstantiationDecl, it might
          // require name qualification. Non-template named out-of-line
          // definitions such as `struct N::color` can also require it when the
          // declaration is emitted from a different lexical scope.
          SgTemplateInstantiationDecl *templateInstantiationDecl =
              isSgTemplateInstantiationDecl(classDeclaration);
          if (templateInstantiationDecl != NULL ||
              preserve_semantic_outer_qualification) {
            int amountOfNameQualificationRequired = nameQualificationDepth(
                classDeclaration, currentScope, classDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "SgClassDeclaration: amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
            setNameQualification(classDeclaration,
                                 amountOfNameQualificationRequired);
          }
        }
      } else {
        // DQ (7/22/2017): I think the template arguments name qualification can
        // be required. This fixes test2017_56.C.
        int amountOfNameQualificationRequired = nameQualificationDepth(
            classDeclaration, currentScope, classDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    " - amountOfNameQualificationRequired = %d \n",
                    amountOfNameQualificationRequired);
#endif
        setNameQualification(classDeclaration,
                             amountOfNameQualificationRequired);
      }
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: SgClassDeclaration -- currentScope "
                                 "is not available, not clear why! \n");
#endif
      // ROSE_ABORT();
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    // DQ (9/23/2012): We need to handle the template arguments associate with
    // this template instantiation.
    SgTemplateInstantiationDecl *templateClassInstantiationDeclaration =
        isSgTemplateInstantiationDecl(classDeclaration);
    if (templateClassInstantiationDeclaration != NULL) {
      // There are template parameters that may require name qualification.
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "WARNING: There are template parameters that may require name "
          "qualification. templateClassInstantiationDeclaration = %p \n",
          templateClassInstantiationDeclaration);

      MLOG_WARN_C(MLOG_UNPARSER,
                  "  --- "
                  "templateClassInstantiationDeclaration->get_"
                  "firstNondefiningDeclaration() = %p \n",
                  templateClassInstantiationDeclaration
                      ->get_firstNondefiningDeclaration());
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "  --- "
          "templateClassInstantiationDeclaration->get_definingDeclaration()    "
          "     = %p \n",
          templateClassInstantiationDeclaration->get_definingDeclaration());

      SgTemplateArgumentPtrList &l =
          templateClassInstantiationDeclaration->get_templateArguments();
      for (SgTemplateArgumentPtrList::iterator i = l.begin(); i != l.end();
           i++) {
        MLOG_WARN_C(MLOG_UNPARSER, "  --- template argument = %p = %s \n", *i,
                    (*i)->class_name().c_str());
      }
    }
#endif
  }

  // DQ (4/26/2019): Adding support to detect SgTemplateVariableDeclaration (see
  // test2019_399.C).
  SgTemplateVariableDeclaration *templateVariableDeclaration =
      isSgTemplateVariableDeclaration(n);
  if (templateVariableDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgTemplateVariableDeclaration: name qualificaiton will "
                "be processed by the SgInitializedName support \n");
#endif
  }

  // Handle the types used in variable declarations...
  // A problem with this implementation is that it relies on there being one
  // SgInitializedName per SgVariableDeclaration. This is currently the case for
  // C++, but we would like to fix this.  It is not clear if the
  // SgInitializedName should carry its own qualification or not (this violates
  // the idea that the IR node that has the reference stored the name
  // qualification data).
  SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(n);
  if (variableDeclaration != NULL) {
    // DQ (4/10/2019): I would like to have all processing happen through the
    // support for SgInitializedName, but maybe that is a problem.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Skipping the previous handling of the SgVariableDeclaration "
                "(need to check bit field name qualification) \n");
#endif

    SgInitializedName *initializedName =
        SageInterface::getFirstInitializedName(variableDeclaration);
    ASSERT_not_null(initializedName);

    // DQ (7/24/2011): if there is a bit-field width specifier then it could
    // contain variable references that require name qualification.
    SgVariableDefinition *variableDefinition =
        isSgVariableDefinition(initializedName->get_declptr());
    if (variableDefinition != NULL) {
      // This is not always the correct current scope (see test2011_70.C for an
      // example).
      SgScopeStatement *currentScope =
          SageInterface::getScope(variableDeclaration);
      ASSERT_not_null(currentScope);

      SgExpression *bitFieldWidthSpecifier = variableDefinition->get_bitfield();
      if (bitFieldWidthSpecifier != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "Traverse the bitFieldWidthSpecifier and "
                                   "add any required name qualification.\n");
#endif
        // DQ (4/28/2019): We might need to add additional arguments to this
        // function, such as in:
        // generateNestedTraversalWithExplicitScope(type,currentScope,currentStatement,initializedName);
        generateNestedTraversalWithExplicitScope(bitFieldWidthSpecifier,
                                                 currentScope);
      }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "++++++++++++++++ DONE: Calling nameQualificationDepth to "
                "evaluate the name \n\n");
#endif
  }

  // DQ (8/23/2014): Adding more uniform support for SgInitializedName objects
  // by supporting the SgFunctionParameterList (similar to the
  // SgVariableDeclaration).
  SgFunctionParameterList *functionParameterList = isSgFunctionParameterList(n);
  if (functionParameterList != NULL) {
  }

  // DQ (4/18/2019): Added support for traversals over the type, so that we can
  // support SgPointerMemberType which can exist within nested type. Because
  // this can happen in nexted types we need the type traversal to discover
  // these.  I am hoping this will not be a performance issue for long types
  // stemming from template instatiations, if so we might want to detect these
  // separately and avoid name qualification for them.
  SgPointerMemberType *pointerMemberType = isSgPointerMemberType(n);
  if (pointerMemberType != NULL) {
    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    ASSERT_not_null(currentScope);

    // DQ (4/19/2019): This is not a good idea, I have modified the recursive
    // step to allow us to pass the currentStatement as well (optionally). DQ
    // (4/18/2019): See if we can make the currentStatement just the
    // currentScope. SgStatement* currentStatement = currentScope;
    SgStatement *currentStatement = inheritedAttribute.get_currentStatement();
    ASSERT_not_null(currentStatement);

    // We need to have saved the referenceNode to use since this is associated
    // with a shared type.
    SgNode *referenceNode = inheritedAttribute.get_referenceNode();
    ASSERT_not_null(referenceNode);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgPointerMemberType: referenceNode = %p = %s \n",
                referenceNode, referenceNode->class_name().c_str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "###################################################### \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgPointerMemberType: Compute name qualification() \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "###################################################### \n");
#endif

    SgDeclarationStatement *classDeclaration =
        pointerMemberType->get_class_declaration_of();
    ASSERT_not_null(classDeclaration);

    // SgDeclarationStatement* declarationForInitializedName = classDeclaration;
    // ASSERT_not_null(declarationForInitializedName);
    // SgDeclarationStatement* positionStatement =
    // isSgDeclarationStatement(initializedName->get_parent());

    // DQ (4/21/2019): This a SgExprStatement when we are processing a
    // SgSizeOfOp IR node (see test2019_379.C). SgDeclarationStatement*
    // positionStatement = isSgDeclarationStatement(currentStatement);
    SgStatement *positionStatement = isSgStatement(currentStatement);
    if (positionStatement == NULL) {
      ASSERT_not_null(currentStatement);
      MLOG_WARN_C(MLOG_UNPARSER, "Error: currentStatement = %p = %s \n",
                  currentStatement, currentStatement->class_name().c_str());
    }
    ASSERT_not_null(positionStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    // MLOG_WARN_C(MLOG_UNPARSER, "Correcting associated declaration:
    // declarationForInitializedName = %p = %s
    // \n",declarationForInitializedName,declarationForInitializedName->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Correcting associated declaration: classDeclaration  = %p = %s \n",
        classDeclaration, classDeclaration->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Correcting associated declaration: positionStatement = %p = %s \n",
        positionStatement, positionStatement->class_name().c_str());
#endif
    // int amountOfNameQualificationRequiredForName =
    // nameQualificationDepth(initializedName,currentScope,variableDeclaration);
    // int amountOfNameQualificationRequiredForName =
    // nameQualificationDepth(initializedName,currentScope,declarationForInitializedName);
    // int amountOfNameQualificationRequiredForName =
    // nameQualificationDepth(declarationForInitializedName,currentScope,positionStatement);
    int amountOfNameQualificationRequired = nameQualificationDepth(
        classDeclaration, currentScope, positionStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "SgPointerMemberType: amountOfNameQualificationRequired = %d \n",
        amountOfNameQualificationRequired);
#endif

    // DQ (4/20/2019): Need to pass the reference node which is the node for
    // which this is a type (intial testing this should be a SgInitializedName).
    // bool skipGlobalNameQualification = true;
    // bool skipGlobalNameQualification = false;
    // setNameQualificationOnName(initializedName,declaration,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
    // setNameQualificationOnName(initializedName,positionStatement,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
    // setNameQualificationOnName(initializedName,declarationForInitializedName,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
    // setNameQualification(pointerMemberType,classDeclaration,amountOfNameQualificationRequired,skipGlobalNameQualification);
    // setNameQualification(pointerMemberType,classDeclaration,amountOfNameQualificationRequired);
    // setNameQualification(pointerMemberType,referenceNode,amountOfNameQualificationRequired);
    // setNameQualification(referenceNode,classDeclaration,amountOfNameQualificationRequired);
    // setNameQualification(pointerMemberType,classDeclaration,amountOfNameQualificationRequired);
    setNameQualificationOnClassOf(pointerMemberType, classDeclaration,
                                  amountOfNameQualificationRequired);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "################################################ \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgPointerMemberType: Evaluate the base type \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "################################################ \n");
#endif

    // DQ (4/20/2019): Now look at the base type, if it is not a
    // SgPointerMemberType, then compute the associated name qualification. We
    // will of course traverse this type, but at the time of the traversal, we
    // would not compute the name qualification since it would be done from the
    // context of the IR node that references the type.
    SgType *baseType = pointerMemberType->get_base_type();
    ASSERT_not_null(baseType);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgPointerMemberType: baseType = %p = %s \n", baseType,
                baseType->class_name().c_str());
#endif

    // DQ (4/21/2019): Reset the type so that we don't miss the
    // SgPointerMemberType, but ignore other modifiers.
    unsigned char bit_array =
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE;
    baseType = baseType->stripType(bit_array);
    ASSERT_not_null(baseType);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Case SgPointerMemberType: after stripType(): baseType = %p = %s \n",
        baseType, baseType->class_name().c_str());
#endif

    SgPointerMemberType *pointerMemberBaseType =
        isSgPointerMemberType(baseType);
    if (pointerMemberBaseType == NULL) {
      // Need to handle name qualification of this type (if there is an
      // associated declaration).

      SgDeclarationStatement *declaration =
          getDeclarationAssociatedWithType(baseType);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER, "Case SgPointerMemberType: declaration = %p = %s \n",
          declaration,
          (declaration != NULL) ? baseType->class_name().c_str() : "null");
#endif
      if (declaration != NULL) {
        // DQ (4/21/2019): Handle the base type of the SgPointerMemberType, if
        // it is not another nested SgPointerMemberType IR node.
        ASSERT_not_null(currentScope);
        ASSERT_not_null(positionStatement);
        // int amountOfNameQualificationRequiredForName =
        // nameQualificationDepth(declarationForInitializedName,currentScope,positionStatement);
        int amountOfNameQualificationRequiredForType = nameQualificationDepth(
            declaration, currentScope, positionStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "SgPointerMemberType: base type: "
                    "amountOfNameQualificationRequiredForType = %d \n",
                    amountOfNameQualificationRequiredForType);
#endif
        // bool skipGlobalNameQualification = true;
        // bool skipGlobalNameQualification = false;
        // setNameQualificationOnName(initializedName,declaration,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
        // setNameQualificationOnName(initializedName,positionStatement,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
        // setNameQualificationOnName(initializedName,declarationForInitializedName,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
        // setNameQualificationOnType(pointerMemberType,declaration,amountOfNameQualificationRequiredForType,skipGlobalNameQualification);
        setNameQualificationOnBaseType(
            pointerMemberType, declaration,
            amountOfNameQualificationRequiredForType);
      } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "The base type of the SgPointerMemberType has no "
                    "associated declaration (so cannot be name qualified) \n");
#endif
      }
    } else {
      // This is a nested SgPointerMemberType in a SgPointerMemberType, it will
      // be processed as part of the type traversal.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Detected a nested SgPointerMemberType, it will be processed "
                  "as part of the recursion of the type traversal \n");
#endif
    }
  }

  // Handle SgType name qualification where SgInitializedName's appear outside
  // of SgVariableDeclaration's (e.g. in function parameter declarations).
  SgInitializedName *initializedName = isSgInitializedName(n);
  if (initializedName != NULL) {

    // bool debugging = false;
#if DEBUG_INITIALIZED_NAME || 0
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Case SgInitializedName: initializedName = %p = %s name = %s \n",
        initializedName, initializedName->class_name().c_str(),
        initializedName->get_name().str());
    MLOG_WARN_C(MLOG_UNPARSER,
                " --- initializedName->get_parent() = %p = %s \n",
                initializedName->get_parent(),
                initializedName->get_parent()->class_name().c_str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case of SgInitializedName: type = %p = %s name = %s \n",
                initializedName->get_type(),
                initializedName->get_type()->class_name().c_str(),
                initializedName->get_name().str());
#endif
    // DQ (3/31/2019): Adding debugging support to debug pointer-to-membr name
    // qualification. bool debugging = (initializedName->get_name() ==
    // "callback_func_ptr"); bool debugging = (initializedName->get_name() ==
    // "pointer_to_data"); bool debugging = (initializedName->get_name() ==
    // "p2"); bool debugging = (initializedName->get_name() == "xyz");

    // DQ (3/31/2019): Adding name qualification for the SgInitialized name
    // directly (then we need to remove it from there it is introduced in the
    // SgVariableDeclaration and the SgFunctionParameterList). The point of
    // adding it here is the it is required for pointer-to-member declarations
    // that are not associated with the base type of the pointer-to-member type
    // and must be name qualified differently from the base type (as
    // demonstrated in C++11_tests/test2019_333.C).

    // DQ (4/26/2019): We need the currentScope where we are evaluating the name
    // qualification, not the scope of the initialized name. SgScopeStatement*
    // currentScope = SageInterface::getScope(variableDeclaration);
    // SgScopeStatement* currentScope =
    // isSgScopeStatement(variableDeclaration->get_parent()); SgScopeStatement*
    // currentScope = initializedName->get_scope();
    SgNode *initializedNameParent = initializedName->get_parent();
    SgDeclarationStatement *declarationStatement =
        isSgDeclarationStatement(initializedNameParent);
    SgScopeStatement *currentScope = NULL;
    if (declarationStatement != NULL) {
      currentScope = declarationStatement->get_scope();
    } else {
      // Condition variables (e.g. if/switch) and range-based for
      // variables can host initialized names without an explicit
      // declaration statement parent.
      currentScope = initializedName->get_scope();
      if (currentScope == NULL) {
        SgStatement *enclosingStatement =
            SageInterface::getEnclosingStatement(initializedName);
        if (enclosingStatement != NULL) {
          currentScope = SageInterface::getScope(enclosingStatement);
        }
      }
    }

    // DQ (9/2/2020): Name qualification for the SgCtorInitializerList should
    // use the scope of the associated class declaration.
    SgCtorInitializerList *ctorInitializerList =
        isSgCtorInitializerList(initializedNameParent);
    if (ctorInitializerList != NULL) {
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          isSgMemberFunctionDeclaration(ctorInitializerList->get_parent());
      ROSE_ASSERT(memberFunctionDeclaration != NULL);
      // currentStatement =
      // memberFunctionDeclaration->get_firstNondefiningDeclaration();
      currentScope = memberFunctionDeclaration->get_scope();
#if DEBUG_INITIALIZED_NAME
      printf("Case of SgInitializedName: from SgCtorInitializerList: "
             "currentScope = %p = %s name = %s \n",
             currentScope, currentScope->class_name().c_str(),
             SageInterface::get_name(currentScope).c_str());
#endif
    }

#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgInitializedName: currentScope = %p = %s \n",
                currentScope, currentScope->class_name().c_str());
#endif

    // DQ (4/12/2019): Now that we have unified the SgInitializedName support,
    // we need to handle this case here instead of in the
    // SgfunctionParameterList support. DQ (8/29/2014): This is a result of a
    // transformation in the tutorial (codeCoverage.C) that does not appear to
    // be implemeting a transformation correctly.
    if (currentScope == NULL) {
      // DQ (4/12/2019): Need to setup a local copy of the functionParameterList
      // (since this code has been moved).
      SgFunctionParameterList *functionParameterList =
          isSgFunctionParameterList(initializedName->get_parent());
      ASSERT_not_null(functionParameterList);

      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: currentScope == NULL: functionParameterList = %p \n",
                  functionParameterList);
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(functionParameterList->get_parent());
      ASSERT_not_null(functionDeclaration);
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: currentScope == NULL: functionDeclaration = %p = %s "
                  "name = %s \n",
                  functionDeclaration,
                  functionDeclaration->class_name().c_str(),
                  functionDeclaration->get_name().str());
      ASSERT_not_null(functionDeclaration->get_file_info());
      functionDeclaration->get_file_info()->display(
          "Error: currentScope == NULL: functionParameterList->get_parent(): "
          "debug");
      SgScopeStatement *temp_scope =
          SageInterface::getScope(functionDeclaration);
      ASSERT_not_null(temp_scope);

      // DQ (8/29/2014): It appears that we can't ask the
      // SgFunctionParameterList for it's scope, but we can find the
      // SgFunctionDeclaration from the parent and ask it; so this should be
      // fixed.
      currentScope = temp_scope;

      MLOG_WARN_C(MLOG_UNPARSER,
                  "It appears that in the case of a transforamtion, we can't "
                  "always ask the SgFunctionParameterList for it's scope, but "
                  "we can find the SgFunctionDeclaration from the parent and "
                  "ask it; so this should be fixed. \n");
    }

    ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s \n", currentScope,
                currentScope->class_name().c_str());
#endif

    // DQ (8/8/2020): Moved from the refactored code below.
    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(initializedName);
    ASSERT_not_null(currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "case of SgInitializedName: currentStatement = %p = %s \n",
                currentStatement, currentStatement->class_name().c_str());
#endif

    // DQ (8/8/2020): Moved from the refactored code below.
    // DQ (4/27/2019): Refactored this code to be outside of the flase block
    // below, so it can be used both there and outside the false branch
    // afterward.

    // DQ (10/18/2020): Commented out to see where this is first used.
    // SgDeclarationStatement* declaration =
    // getDeclarationAssociatedWithType(initializedName->get_type());

    SgType *type = initializedName->get_type();
    ASSERT_not_null(type);

    // if for type handling.
    // Refactored this code.
    nameQualificationTypeSupport(type, currentScope, initializedName);

    // DQ (9/2/2020): This is the start of the non-commented out code!
#if DEBUG_INITIALIZED_NAME && 0
    printf("2222222222222222222222222222222222222222222222222222222222222222222"
           "22222222 \n");
    printf("2222222222222222222222222222222222222222222222222222222222222222222"
           "22222222 \n");
    printf("2222222222222222222222222222222222222222222222222222222222222222222"
           "22222222 \n");
    printf("2222222222222222222222222222222222222222222222222222222222222222222"
           "22222222 \n");
    printf("2222222222222222222222222222222222222222222222222222222222222222222"
           "22222222 \n");
    printf("2222222222222222222222222222222222222222222222222222222222222222222"
           "22222222 \n");
#endif

#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "############################ \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgInitializedName: Processing the SgInitializedName IR's "
                "name = %s \n",
                initializedName->get_name().str());
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "############################ \n");
#endif
    // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "initializedName->get_prev_decl_item() = %p \n",
                initializedName->get_prev_decl_item());
#endif

    bool initializedNameCouldRequireNameQualification =
        (initializedName->get_prev_decl_item() != NULL);
#if DEBUG_INITIALIZED_NAME
    printf("Case SgInitializedName: "
           "initializedNameCouldRequireNameQualification = %s \n",
           initializedNameCouldRequireNameQualification ? "true" : "false");
#endif
    if (initializedNameCouldRequireNameQualification == true) {
      SgInitializedName *originallyDeclaredInitializedName =
          initializedName->get_prev_decl_item();

#if DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "originallyDeclaredInitializedName = %p = %s name = %s \n",
                  originallyDeclaredInitializedName,
                  originallyDeclaredInitializedName->class_name().c_str(),
                  originallyDeclaredInitializedName->get_name().str());
#endif
      // SgInitializedName* initializedName =
      // SageInterface::getFirstInitializedName(variableDeclaration);
      ASSERT_not_null(initializedName);
      ASSERT_not_null(initializedName->get_parent());

      ASSERT_not_null(originallyDeclaredInitializedName->get_parent());

      // SgDeclarationStatement* associatedDeclaration =
      // isSgDeclarationStatement(initializedName->get_parent());
      // SgDeclarationStatement* associatedDeclaration =
      // isSgDeclarationStatement(initializedName->get_parent());
      SgDeclarationStatement *associatedDeclaration = isSgDeclarationStatement(
          originallyDeclaredInitializedName->get_parent());
      if (associatedDeclaration == NULL) {
#if DEBUG_INITIALIZED_NAME
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Note: unexpected IR node: "
            "originallyDeclaredInitializedName->get_parent() = %p = %s \n",
            originallyDeclaredInitializedName->get_parent(),
            originallyDeclaredInitializedName->get_parent()
                ->class_name()
                .c_str());
#endif
        // DQ (4/27/2019): Address at least this specific case of a
        // SgClassDefinition (should include case of SgTemplateClassDefinition).
        // SgTemplateClassDefinition
        SgClassDefinition *classDefinition = isSgClassDefinition(
            originallyDeclaredInitializedName->get_parent());
        if (classDefinition != NULL) {
          associatedDeclaration = classDefinition->get_declaration();
          ASSERT_not_null(associatedDeclaration);
        }
      }

      // DQ (6/27/2019): Added more debugging support.
      if (associatedDeclaration == NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Note: unexpected IR node: "
            "originallyDeclaredInitializedName->get_parent() = %p = %s \n",
            originallyDeclaredInitializedName->get_parent(),
            originallyDeclaredInitializedName->get_parent()
                ->class_name()
                .c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            " --- originallyDeclaredInitializedName->get_name() = %s \n",
            originallyDeclaredInitializedName->get_name().str());
        originallyDeclaredInitializedName->get_file_info()->display(
            "unexpected IR node");
      }
      ASSERT_not_null(associatedDeclaration);

      // Reuse the previously computed currentScope.
      // This is not always the correct current scope (see test2011_70.C for an
      // example). SgScopeStatement* currentScope =
      // SageInterface::getScope(variableDeclaration); SgScopeStatement*
      // currentScope = isSgScopeStatement(variableDeclaration->get_parent());
      // SgScopeStatement* currentScope =
      // SageInterface::getScope(associatedDeclaration);
      ASSERT_not_null(currentScope);
#if DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgInitializedName: name: currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "================ Calling nameQualificationDepthForType to "
                  "evaluate the type \n");
#endif
      // Compute the depth of name qualification from the current statement:
      // variableDeclaration. int amountOfNameQualificationRequiredForType =
      // nameQualificationDepthForType(initializedName,variableDeclaration); int
      // amountOfNameQualificationRequiredForType =
      // nameQualificationDepthForType(initializedName,currentScope,variableDeclaration);
      int amountOfNameQualificationRequiredForName = nameQualificationDepth(
          initializedName, currentScope, associatedDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgInitializedName: name: "
                  "amountOfNameQualificationRequiredForName = %d \n",
                  amountOfNameQualificationRequiredForName);
#endif

      // DQ (4/26/2019): Call this directly for the name qualificaiton of the
      // SgInitializedName. bool skipGlobalNameQualification =
      // skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(declaration);
      // bool skipGlobalNameQualification = true;
      bool skipGlobalNameQualification = false;
      // setNameQualificationOnName(initializedName,declaration,amountOfNameQualificationRequiredForName,skipGlobalNameQualification);
      setNameQualificationOnName(initializedName, associatedDeclaration,
                                 amountOfNameQualificationRequiredForName,
                                 skipGlobalNameQualification);
    }

#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "######################### \n");
    MLOG_WARN_C(MLOG_UNPARSER, "Case SgInitializedName: DONE: Processing the "
                               "SgInitializedName IR's name \n");
    MLOG_WARN_C(MLOG_UNPARSER, "###############################################"
                               "######################### \n");
#endif
    // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#if DEBUG_INITIALIZED_NAME && 0
    printf("3333333333333333333333333333333333333333333333333333333333333333333"
           "33333333 \n");
    printf("3333333333333333333333333333333333333333333333333333333333333333333"
           "33333333 \n");
    printf("3333333333333333333333333333333333333333333333333333333333333333333"
           "33333333 \n");
    printf("3333333333333333333333333333333333333333333333333333333333333333333"
           "33333333 \n");
    printf("3333333333333333333333333333333333333333333333333333333333333333333"
           "33333333 \n");
#endif

    // DQ (4/28/2019): Trying to support the initializer here, so that we can
    // support constructor preinitialization lists, rather than through the
    // SgConstructor initializer.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "initializedName->get_preinitialization() = %d \n",
                initializedName->get_preinitialization());
#endif
    // DQ (12/8/2019): If this is a simple data member then we don't need anme
    // qualification on its type (which does not appear in the source code).
    bool is_simple_data_member = false;
    if (initializedName->get_preinitialization() ==
        SgInitializedName::e_data_member) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Found a data member used in pre-initialization list \n");
#endif
      is_simple_data_member = true;
    }

    // DQ (4/26/2019): The initializer should be processed as an expression to
    // be name qualified separately.
#if DEBUG_INITIALIZED_NAME
    MLOG_WARN_C(MLOG_UNPARSER,
                "############################################# \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case SgInitializedName: Check for initializer \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "############################################# \n");
#endif
    // DQ (12/17/2013): Added support for name qualification of
    // preinitialization list elements (see test codes: test2013_285-288.C). if
    // (initializedName->get_initptr() != NULL)
    if (initializedName->get_initptr() != NULL &&
        is_simple_data_member == false) {
      // DQ (2/7/2019): I think this can't be a SgPointerMemberType, so the code
      // specific to this case does not go here.
      // ROSE_ASSERT(isSgPointerMemberType(initializedName->get_type()) ==
      // NULL);
#if DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER, "#############################################"
                                 "################################# \n");
      MLOG_WARN_C(MLOG_UNPARSER, "Case SgInitializedName: Processing the "
                                 "SgInitializedName IR node's initializer \n");
      MLOG_WARN_C(MLOG_UNPARSER, "#############################################"
                                 "################################# \n");
#endif
      // DQ (4/28/2019): Added this variable declaration to support compiling
      // this section that was previously commented out.
      SgStatement *associatedStatement = currentScope;
#if DEBUG_INITIALIZED_NAME
      MLOG_WARN_C(MLOG_UNPARSER, "initializedName = %p name = %s \n",
                  initializedName, initializedName->get_name().str());
      MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s \n", currentScope,
                  currentScope->class_name().c_str());
#endif
      SgConstructorInitializer *constructorInitializer =
          isSgConstructorInitializer(initializedName->get_initptr());
      // ASSERT_not_null(constructorInitializer);
      if (constructorInitializer != NULL) {
#if DEBUG_INITIALIZED_NAME
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "######################################################## \n");
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Case SgInitializedName: (constructorInitializer != NULL) \n");
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "######################################################## \n");
#endif
        // DQ (12/8/2019): Note that "type" is a variable declared above and we
        // don't what to hide that variable. SgType* type =
        // initializedName->get_type(); SgType* type =
        // constructorInitializer->get_type();
        SgType *constructorInitializer_type =
            constructorInitializer->get_type();
        ASSERT_not_null(constructorInitializer_type);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Test for special case of SgInitializedName used in "
                    "SgCtorInitializerList: type = %p = %s \n",
                    type, type->class_name().c_str());
#endif
        SgCtorInitializerList *ctor =
            isSgCtorInitializerList(initializedName->get_parent());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Test for special case of SgInitializedName used in "
                    "SgCtorInitializerList: ctor = %p "
                    "constructorInitializer_type = %p = %s \n",
                    ctor, constructorInitializer_type,
                    constructorInitializer_type->class_name().c_str());
#endif
        if (ctor != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Calling setNameQualificationOnName() (operating "
                      "DIRECTLY on the SgInitializedName) \n");
#endif
#if DEBUG_INITIALIZED_NAME
          MLOG_WARN_C(MLOG_UNPARSER,
                      "########################################################"
                      "###################################### \n");
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Case SgInitializedName: (ctor != NULL && (functionType "
                      "!= NULL || memberFunctionType != NULL)) \n");
          MLOG_WARN_C(MLOG_UNPARSER,
                      "########################################################"
                      "###################################### \n");
#endif

          // DQ (2/2/2019): NOTE: constructorInitializer->get_declaration() ==
          // NULL when there is not associated constructor for the class (e.g.
          // the case where the default constructor (compiler generated) is
          // used). DQ (1/13/2014): This only get's qualification when the name
          // being used matches the class name, else this is a data member and
          // should not be qualified.  See test2014_01.C. SgName functionName =
          // (functionType != NULL) ? functionType->get_name() :
          // memberFunctionType->get_name();
          SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(
              constructorInitializer->get_declaration());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Test for special case of SgInitializedName used in "
                      "SgCtorInitializerList: functionDeclaration = %p \n",
                      functionDeclaration);
#endif
          // DQ (2/2/2019): This is non-null for all but
          // legacy frontend 5.0, so this is debugging
          // support.
          SgDeclarationStatement *associatedDeclaration = NULL;
          SgName constructorTargetName;

          if (functionDeclaration != NULL) {
            constructorTargetName = functionDeclaration->get_name();

            SgClassDefinition *classDefinition =
                isSgClassDefinition(functionDeclaration->get_scope());
            ROSE_ASSERT(classDefinition != NULL);
            associatedDeclaration =
                isSgClassDeclaration(classDefinition->get_declaration());
            ROSE_ASSERT(associatedDeclaration != NULL);
          } else if (SgClassDeclaration *classDeclaration =
                         constructorInitializer->get_class_decl()) {
            constructorTargetName = classDeclaration->get_name();
            associatedDeclaration = isSgClassDeclaration(
                classDeclaration->get_firstNondefiningDeclaration());
            if (associatedDeclaration == NULL) {
              associatedDeclaration = classDeclaration;
            }
            ROSE_ASSERT(associatedDeclaration != NULL);
          }

          if (associatedDeclaration == NULL) {
#if DEBUG_INITIALIZED_NAME
            MLOG_WARN_C(MLOG_UNPARSER,
                        "######################################################"
                        "######################################################"
                        "################### \n");
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Case SgInitializedName: SKIPPING CALL TO "
                "setNameQualificationOnName(): functionDeclaration == NULL \n");
            MLOG_WARN_C(MLOG_UNPARSER,
                        "######################################################"
                        "######################################################"
                        "################### \n");
#endif
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Test for special case of SgInitializedName used in "
                        "SgCtorInitializerList: functionName = %s \n",
                        constructorTargetName.str());
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "Test for special case of SgInitializedName used in "
                "SgCtorInitializerList: initializedName->get_name() = %s \n",
                initializedName->get_name().str());
#endif
#if DEBUG_INITIALIZED_NAME
            MLOG_WARN_C(MLOG_UNPARSER, "@@@@@ currentScope = %p = %s \n",
                        currentScope, currentScope->class_name().c_str());
#endif

#if DEBUG_INITIALIZED_NAME
            printf("44444444444444444444444444444444444444444444444444444444444"
                   "4444444444444444 \n");
            printf("44444444444444444444444444444444444444444444444444444444444"
                   "4444444444444444 \n");
            printf("44444444444444444444444444444444444444444444444444444444444"
                   "4444444444444444 \n");
            printf("44444444444444444444444444444444444444444444444444444444444"
                   "4444444444444444 \n");
            printf("44444444444444444444444444444444444444444444444444444444444"
                   "4444444444444444 \n");
#endif
#if DEBUG_INITIALIZED_NAME
            printf("Case of SgInitializedName: functionDeclaration = %p = %s "
                   "name = %s \n",
                   functionDeclaration,
                   functionDeclaration->class_name().c_str(),
                   SageInterface::get_name(functionDeclaration).c_str());
            printf("Case of SgInitializedName: currentScope = %p = %s name = "
                   "%s \n",
                   currentScope, currentScope->class_name().c_str(),
                   SageInterface::get_name(currentScope).c_str());
            printf("Case of SgInitializedName: associatedStatement = %p = %s "
                   "name = %s \n",
                   associatedStatement,
                   associatedStatement->class_name().c_str(),
                   SageInterface::get_name(associatedStatement).c_str());
#endif
#if DEBUG_INITIALIZED_NAME
            printf("66666666666666666666666666666666666666666666666666666666666"
                   "66666666666666 \n");
            printf("66666666666666666666666666666666666666666666666666666666666"
                   "66666666666666 \n");
            printf("Handling specific case of SgInitializedName from "
                   "SgCtorInitializationList \n");
            printf("66666666666666666666666666666666666666666666666666666666666"
                   "66666666666666 \n");
            printf("66666666666666666666666666666666666666666666666666666666666"
                   "66666666666666 \n");
#endif
            // DQ (9/3/2020): This is I think the only meaningful change to
            // address Cxx11_tests/test2020_89.C. DQ (9/2/2020): If this is a
            // SgInitializedName from a SgCtorInitializationList, then we want
            // to search for the class associated with the member function in
            // the parent of the scope of the class definition. However, this
            // code might be too specific to this narrow case (something to look
            // at in the morning).
            // Constructor preinitialization names are resolved from the
            // constructor's lexical class scope. Dropping to the parent scope
            // hides shadowing by the current class name and can misclassify a
            // base initializer as a delegating constructor.
            // DQ (9/2/2020): Use alternative declarations for initialized names
            // from ctor initialization list support. int
            // amountOfNameQualificationRequiredForType =
            // nameQualificationDepthForType(initializedName,currentScope,associatedStatement);
            // int amountOfNameQualificationRequiredForType =
            // nameQualificationDepth(functionDeclaration,currentScope,associatedStatement);
            int amountOfNameQualificationRequiredForType =
                nameQualificationDepth(associatedDeclaration, currentScope,
                                       associatedStatement);
#if DEBUG_INITIALIZED_NAME
            MLOG_WARN_C(MLOG_UNPARSER,
                        "amountOfNameQualificationRequiredForType = %d \n",
                        amountOfNameQualificationRequiredForType);
#endif
            if (initializedName->get_name() == constructorTargetName) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_INITIALIZED_NAME
              MLOG_WARN_C(MLOG_UNPARSER,
                          "amountOfNameQualificationRequiredForType = %d \n",
                          amountOfNameQualificationRequiredForType);
#endif
#if DEBUG_INITIALIZED_NAME
              MLOG_WARN_C(MLOG_UNPARSER,
                          "####################################################"
                          "####################################################"
                          "####################### \n");
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "Case SgInitializedName: Processing the SgInitializedName IR "
                  "node's type ((initializedName->get_name() == functionName) "
                  "== true) \n");
              MLOG_WARN_C(MLOG_UNPARSER,
                          "####################################################"
                          "####################################################"
                          "####################### \n");
#endif
              // DQ (10/18/2020): Moved declaration to where it is being used.
              SgDeclarationStatement *declaration =
                  getDeclarationAssociatedWithType(initializedName->get_type());
              if (declaration == NULL) {
                declaration = associatedDeclaration;
              }
              ROSE_ASSERT(declaration != NULL);

              // DQ (4/28/2019): Added variable to allow this section to be
              // compiled.
              bool skipGlobalNameQualification = false;
              setNameQualificationOnName(
                  initializedName, declaration,
                  amountOfNameQualificationRequiredForType,
                  skipGlobalNameQualification);

              // DQ (3/31/2019): Uncomment this to trigger review because I now
              // think we should be calling setNameQualification() instead of
              // setNameQualificationOnName().  Because
              // setNameQualificationOnName() should use the new name
              // qualification fields for the SgInitializedName instead of the
              // fields for the SgInitializedName's type.
            } else {
#if DEBUG_INITIALIZED_NAME
              MLOG_WARN_C(MLOG_UNPARSER,
                          "####################################################"
                          "####################################################"
                          "####################### \n");
              MLOG_WARN_C(MLOG_UNPARSER,
                          "Case SgInitializedName: SKIPPING CALL TO "
                          "setNameQualificationOnName() \n");
              MLOG_WARN_C(MLOG_UNPARSER,
                          "####################################################"
                          "####################################################"
                          "####################### \n");
#endif
            }
#if DEBUG_INITIALIZED_NAME
            printf("55555555555555555555555555555555555555555555555555555555555"
                   "5555555555555555 \n");
            printf("55555555555555555555555555555555555555555555555555555555555"
                   "5555555555555555 \n");
            printf("55555555555555555555555555555555555555555555555555555555555"
                   "5555555555555555 \n");
            printf("55555555555555555555555555555555555555555555555555555555555"
                   "5555555555555555 \n");
            printf("55555555555555555555555555555555555555555555555555555555555"
                   "5555555555555555 \n");
#endif
          }
        } else {
        }
      } else {
      }
    }

    // DQ (4/26/2019): The initializer should be processed as an expression to
    // be name qualified separately.

    // DQ (10/18/2020): Moved declaration to where it is being used.
    SgDeclarationStatement *declaration =
        getDeclarationAssociatedWithType(initializedName->get_type());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    if (declaration == NULL) {
      printf("initializedName->get_type() = %p = %s \n",
             initializedName->get_type(),
             initializedName->get_type()->class_name().c_str());
      SgType *strippedType = initializedName->get_type()->stripType();
      printf("strippedType = %p = %s \n", strippedType,
             strippedType->class_name().c_str());
    }
    // ROSE_ASSERT(declaration != NULL);
#endif

    // DQ (10/18/2020): Only insert into the referencedNameSet if this is a
    // declaration that we have seen, must be non-null. DQ (8/4/2012): Isolate
    // that handling of the referencedNameSet from the use of
    // skipGlobalNameQualification so that we can debug (test2012_96.C). if
    // (skipGlobalNameQualification == true &&
    // referencedNameSet.find(declaration) == referencedNameSet.end()) if
    // (referencedNameSet.find(declaration) == referencedNameSet.end())
    if ((declaration != NULL) &&
        (referencedNameSet.find(declaration) == referencedNameSet.end())) {
      // No qualification is required but we do want to count this as a
      // reference to the class.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "No qualification should be used for this type (class = %p = "
                  "%s) AND insert it into the referencedNameSet \n",
                  declaration,
                  declaration ? declaration->class_name().c_str() : "");
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      printf("INSERTING INTO referencedNameSet: declaration = %p = %s name = "
             "%s \n",
             declaration, declaration->class_name().c_str(),
             SageInterface::get_name(declaration).c_str());
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
#endif
      referencedNameSet.insert(declaration);
    }
  }

  // Handle references to SgFunctionDeclaration...
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(n);

  // DQ (6/4/2011): Avoid processing as both member and non-member function...
  // if (functionDeclaration != NULL)
  if (functionDeclaration != NULL && isSgMemberFunctionDeclaration(n) == NULL) {
    // Could it be that we only want to do this for the defining declaration?
    // No, since prototypes must also use name qualification!

    // We need the structural location in scope (not the semantic one).
    SgScopeStatement *currentScope =
        isSgScopeStatement(functionDeclaration->get_parent());
    // SgScopeStatement* currentScope = functionDeclaration->get_scope();

    // SgStatement* currentStatement =
    // SageInterface::getEnclosingStatement(functionDeclaration->get_parent());
    // ASSERT_not_null(currentStatement);

    // Make sure these are the same. test2005_57.C presents what might be a
    // relevant test code. ROSE_ASSERT(currentScope ==
    // currentStatement->get_scope());

    // DQ (11/18/2017): When the parent is not a scope, it could be a
    // SgTemplateInstantiationDirectiveStatement, in which case we want the
    // parent of that. See test2017_66.C (and previously test2006_08.C) for an
    // example of this case.
    if (currentScope == NULL) {
      SgTemplateInstantiationDirectiveStatement
          *templateInstantiationDirectiveStatement =
              isSgTemplateInstantiationDirectiveStatement(
                  functionDeclaration->get_parent());
      if (templateInstantiationDirectiveStatement != NULL) {
        currentScope = isSgScopeStatement(
            templateInstantiationDirectiveStatement->get_parent());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Case of (functionDeclaration != NULL && "
            "isSgMemberFunctionDeclaration(n) == NULL): reset using "
            "SgTemplateInstantiationDirectiveStatement: currentScope = %p \n",
            currentScope);
#endif
        // Now we should have a valid currentScope.
        ASSERT_not_null(currentScope);
      }
    }

    // Reopened namespaces are represented by distinct ROSE namespace
    // definitions that share a logical scope. For declarations attached to one
    // fragment while their semantic scope points at an equivalent fragment, use
    // the declaration scope for name-qualification analysis so we don't emit a
    // redundant namespace qualifier inside the same logical namespace.
    if (currentScope != NULL && functionDeclaration->get_scope() != NULL) {
      SgNamespaceDefinitionStatement *currentNamespace =
          isSgNamespaceDefinitionStatement(currentScope);
      SgNamespaceDefinitionStatement *declarationNamespace =
          isSgNamespaceDefinitionStatement(functionDeclaration->get_scope());
      if (currentNamespace != NULL && declarationNamespace != NULL &&
          currentNamespace != declarationNamespace &&
          SgScopeStatement::isEquivalentScope(currentNamespace,
                                              declarationNamespace)) {
        currentScope = declarationNamespace;
      }
    }

    // ASSERT_not_null(currentScope);
    if (currentScope != NULL) {
      // Handle the function return type...
      ASSERT_not_null(functionDeclaration->get_orig_return_type());
      ASSERT_not_null(functionDeclaration->get_type());
      ASSERT_not_null(functionDeclaration->get_type()->get_return_type());
      SgType *returnType = functionDeclaration->get_orig_return_type();
      ASSERT_not_null(returnType);

      if (si::typeCarriesWrittenNonrealQualification(returnType)) {
        const bool preserve_written_type_elaboration =
            functionDeclaration
                ->get_type_elaboration_required_for_return_type();
        functionDeclaration->set_global_qualification_required_for_return_type(
            false);
        functionDeclaration->set_name_qualification_length_for_return_type(0);
        functionDeclaration->set_type_elaboration_required_for_return_type(
            preserve_written_type_elaboration);
        qualifiedNameMapForTypes[functionDeclaration] = "";
      } else {
        SgDeclarationStatement *declaration =
            getDeclarationAssociatedWithType(returnType);
        if (declaration != NULL) {
          int amountOfNameQualificationRequiredForReturnType =
              nameQualificationDepth(declaration, currentScope,
                                     functionDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgFunctionDeclaration's return type: "
                      "amountOfNameQualificationRequiredForType = %d \n",
                      amountOfNameQualificationRequiredForReturnType);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Putting the name qualification for the type into the "
                      "return type of SgFunctionDeclaration = %p = %s \n",
                      functionDeclaration,
                      functionDeclaration->get_name().str());
#endif
          // setNameQualificationReturnType(functionDeclaration,amountOfNameQualificationRequiredForReturnType);
          setNameQualificationReturnType(
              functionDeclaration, declaration,
              amountOfNameQualificationRequiredForReturnType);
        } else {
          // This case is common for builtin functions such as: __builtin_powi
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "declaration == NULL: could not put name qualification for the "
              "type into the return type of SgFunctionDeclaration = %p = %s \n",
              functionDeclaration, functionDeclaration->get_name().str());
#endif
        }
      }

      // DQ (6/3/2011): Traverse the type to set any possible template arguments
      // (or other subtypes?) that require name qualification.
      traverseType(returnType, functionDeclaration, currentScope,
                   functionDeclaration);

      // Handle the function name...
      // DQ (6/20/2011): Friend function can be qualified...sometimes...
      // if (functionDeclaration->get_declarationModifier().isFriend() == true
      // || functionDeclaration->get_specialFunctionModifier().isOperator() ==
      // true)
      if (functionDeclaration->get_specialFunctionModifier().isOperator() ==
          true) {
        // DQ (6/19/2011): We sometimes have to qualify friends if it is to
        // avoid ambiguity (see test2006_159.C) (but we never qualify an
        // operator, I think). Maybe a friend declaration should add an
        // SgAliasSymbol to the class definition scope's symbol table. Then
        // simpler rules (no special case) would cause the name qualification to
        // be generated properly.

        // Old comment
        // Never use name qualification for friend functions or operators. I am
        // more sure of the case of friend functions than operators. Friend
        // functions will have a global scope (though this might change in the
        // future; google "friend global scope injection").
        // MLOG_WARN_C(MLOG_UNPARSER, "Detected a friend or operator function,
        // these are not provided with name qualification. \n");
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "Detected a operator function, these are "
                                   "not provided with name qualification. \n");
#endif
      } else {
        // Only use name qualification where the scopes of the declaration's use
        // (currentScope) is not the same as the scope of the function
        // declaration.  However, the analysis should work and determin that the
        // required name qualification length is zero.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "I would like to not have to have this "
                    "SgFunctionDeclaration logic, we should get the name "
                    "qualification correct more directly. \n");
#endif
        // DQ (6/20/2011): Friend function can be qualified and a fix to add a
        // SgAliasSymbol to the class definition scope's symbol table should
        // allow it to be handled with greater precission. DQ (6/25/2011):
        // Friend functions can require global qualification as well (see
        // test2011_106.C). Not clear how to handle this case.
        if (functionDeclaration->get_declarationModifier().isFriend() == true) {
          // This is the case of a friend function declaration which requires
          // more name qualification than expected. Note that this might be
          // compiler dependent but at least GNU g++ required more qualification
          // than expected.
          SgScopeStatement *scope = functionDeclaration->get_scope();
          SgGlobal *globalScope = isSgGlobal(scope);
          if (globalScope != NULL) {
            // We want to specify global qualification when the friend function
            // is in global scope (see test2011_106.C). DQ (6/25/2011): This
            // will output the name qualification correctly AND cause the output
            // of the outlined function to be supressed.
            int amountOfNameQualificationRequired = 0;

            // Check if this function declaration has been seen already...
            // if (functionDeclaration ==
            // functionDeclaration->get_firstNondefiningDeclaration())

            // This is the same code as below so it could be refactored (special
            // handling for function declarations that are defining declaration
            // and don't have an associated nondefining declaration).

            // DQ (8/4/2012): This is a case using the referencedNameSet that
            // should be refactored out of this location.
            SgDeclarationStatement *declarationForReferencedNameSet =
                functionDeclaration->get_firstNondefiningDeclaration();

            if (declarationForReferencedNameSet == NULL) {
              // Note that a function with only a defining declaration will not
              // have a nondefining declaration automatically constructed in the
              // AST (unlike classes and some onther sorts of declarations).
              declarationForReferencedNameSet =
                  functionDeclaration->get_definingDeclaration();

              // DQ (6/22/2011): I think this is true.  This assertion fails for
              // test2006_78.C (a template example code).
              // ROSE_ASSERT(declarationForReferencedNameSet == declaration);

              // DQ (6/23/2011): This assertion fails for the LoopProcessor on
              // tests/nonsmoke/functional/roseTests/loopProcessingTests/mm.C
              // ASSERT_not_null(declarationForReferencedNameSet);
              if (declarationForReferencedNameSet == NULL) {
                declarationForReferencedNameSet = functionDeclaration;
                ASSERT_not_null(declarationForReferencedNameSet);
              }
              ASSERT_not_null(declarationForReferencedNameSet);
            }
            ASSERT_not_null(declarationForReferencedNameSet);

            // DQ (8/4/2012): We would like to refactor this
            // code (I think).
            bool requires_global_qualification =
                (referencedNameSet.find(declarationForReferencedNameSet) !=
                 referencedNameSet.end());
            if (isSgGlobal(currentScope) == NULL) {
              requires_global_qualification = true;
            }

            if (requires_global_qualification) {
              amountOfNameQualificationRequired = 1;
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "Force global qualification for friend function: "
                          "amountOfNameQualificationRequired = %d \n",
                          amountOfNameQualificationRequired);
#endif
            } else {
              // No global qualification is required.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "No qualification should be used for "
                                         "this friend function. \n");
#endif
            }
            setNameQualification(functionDeclaration,
                                 amountOfNameQualificationRequired);
          } else {
            // Not clear what to do with this case, I guess we just want
            // standard qualification rules.
            int amountOfNameQualificationRequired = nameQualificationDepth(
                functionDeclaration, currentScope, functionDeclaration);
            setNameQualification(functionDeclaration,
                                 amountOfNameQualificationRequired);
          }
        } else {
          // DQ (3/31/2018): Added assertion.
          ASSERT_not_null(functionDeclaration->get_scope());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "currentScope = %p functionDeclaration->get_scope() = %p \n",
              currentScope, functionDeclaration->get_scope());
          MLOG_WARN_C(MLOG_UNPARSER, "functionDeclaration->get_scope() = %s \n",
                      functionDeclaration->get_scope()->class_name().c_str());
#endif
          // Case of non-member functions (more logical name qualification
          // rules).
          if (currentScope != functionDeclaration->get_scope()) {
            // DQ (1/21/2013): Added support for testing the more general
            // equivalence of scopes (where the pointers are not equal, applies
            // only to namespaces, I think).
            bool isSameNamespace = SgScopeStatement::isEquivalentScope(
                currentScope, functionDeclaration->get_scope());
            if (isSameNamespace == false) {
              int amountOfNameQualificationRequired = nameQualificationDepth(
                  functionDeclaration, currentScope, functionDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
              MLOG_WARN_C(MLOG_UNPARSER,
                          "isSameNamespace == false: SgFunctionDeclaration: "
                          "amountOfNameQualificationRequired = %d \n",
                          amountOfNameQualificationRequired);
#endif
              // DQ (21/2011): test2011_89.C demonstrates a case where name
              // qualification of a functionRef expression is required. DQ
              // (6/9/2011): Support for test2011_78.C (we only qualify function
              // call references where the function has been declared in a scope
              // where it could be expected to be defined (e.g. not using a
              // forward declaration in a SgBasicBlock, since the function
              // definition could not live in the SgBasicBlock.
              bool skipNameQualification =
                  skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(
                      functionDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "isSameNamespace == false: Test of "
                          "functionDeclaration: skipNameQualification = %s \n",
                          skipNameQualification ? "true" : "false");
#endif
              if (skipNameQualification == false) {
                setNameQualification(functionDeclaration,
                                     amountOfNameQualificationRequired);
              }
            } else {
              // DQ (3/31/2018): Note that we still might require name
              // qualification on any template arguments in the template
              // function instantiation. Ignore the case fo a
              // SgTemplateFunctionDeclaration.
              setNameQualification(functionDeclaration, 0);
              if (isSgTemplateInstantiationFunctionDecl(functionDeclaration) !=
                  NULL) {
                // This point of calling this function is to just have the
                // template arguments evaluated for name qualification (see
                // Cxx11_tests/test2018_68.C).
                int amountOfNameQualificationRequired = nameQualificationDepth(
                    functionDeclaration, currentScope, functionDeclaration);

                // Add this to make sure that amountOfNameQualificationRequired
                // is referenced to avoid a compiler warning.
                ROSE_ASSERT(amountOfNameQualificationRequired >= 0);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
                MLOG_WARN_C(MLOG_UNPARSER,
                            "SgFunctionDeclaration: "
                            "amountOfNameQualificationRequired = %d \n",
                            amountOfNameQualificationRequired);
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
                // DQ (4/2/2018): Report anything that is unusual, i.e. non-zero
                // name qualification length.
                if (amountOfNameQualificationRequired > 0) {
                  MLOG_WARN_C(MLOG_UNPARSER,
                              "Warning: name qualification length should be "
                              "zero for a function declared in the same scope: "
                              "amountOfNameQualificationRequired = %d \n",
                              amountOfNameQualificationRequired);

                  MLOG_WARN_C(MLOG_UNPARSER,
                              "functionDeclaration = %p = %s = %s \n",
                              functionDeclaration,
                              functionDeclaration->class_name().c_str(),
                              functionDeclaration->get_mangled_name().str());

                  MLOG_WARN_C(MLOG_UNPARSER,
                              "currentScope                     = %p = %s \n",
                              currentScope, currentScope->class_name().c_str());
                  SgNamespaceDefinitionStatement *currentNamespaceDefinition =
                      isSgNamespaceDefinitionStatement(currentScope);
                  if (currentNamespaceDefinition != NULL) {
                    SgNamespaceDeclarationStatement
                        *currentNamespaceDeclaration =
                            currentNamespaceDefinition
                                ->get_namespaceDeclaration();
                    MLOG_WARN_C(
                        MLOG_UNPARSER,
                        "currentNamespaceDeclaration->get_name() = %s \n",
                        currentNamespaceDeclaration->get_name().str());
                  }

                  MLOG_WARN_C(
                      MLOG_UNPARSER,
                      "functionDeclaration->get_scope() = %p = %s \n",
                      functionDeclaration->get_scope(),
                      functionDeclaration->get_scope()->class_name().c_str());
                  SgNamespaceDefinitionStatement *functionNamespaceDefinition =
                      isSgNamespaceDefinitionStatement(
                          functionDeclaration->get_scope());
                  if (functionNamespaceDefinition != NULL) {
                    SgNamespaceDeclarationStatement
                        *functionNamespaceDeclaration =
                            functionNamespaceDefinition
                                ->get_namespaceDeclaration();
                    MLOG_WARN_C(
                        MLOG_UNPARSER,
                        "functionNamespaceDeclaration->get_name() = %s \n",
                        functionNamespaceDeclaration->get_name().str());
                  }
                }
#endif
              } else {
                // DQ (11/24/2020): This seems like an important case to handle,
                // but perhaps it is handled below.
              }
            }
          } else {
            // DQ (10/31/2013): Added to support name qualification on template
            // parameters (see test2013_273.C). However, this just leads to over
            // qualification (use of global qualification which is not
            // required).
            setNameQualification(functionDeclaration, 0);

            SgTemplateInstantiationFunctionDecl *templateFunction =
                isSgTemplateInstantiationFunctionDecl(functionDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "@@@@@@@@@@@@@ Calling nameQualificationDepth(): "
                        "functionDeclaration = %p = %s \n",
                        functionDeclaration,
                        functionDeclaration->class_name().c_str());
            if (templateFunction != NULL) {
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::evaluateInheritedAttribute(): "
                  "for case of SgTemplateInstantiationFunctionDecl: "
                  "templateFunction = %p = %s \n",
                  templateFunction, templateFunction->class_name().c_str());
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- templateFunction->get_name()         = %s \n",
                          templateFunction->get_name().str());
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- templateFunction->get_templateName() = %s \n",
                          templateFunction->get_templateName().str());
            }
#endif

            // DQ (11/16/2013): The point is that we need to handle the name
            // qualification on any associated template arguments not on the
            // function itself. So we just want to call
            // nameQualificationDepth(), to get the anem qualification on the
            // template arguments, but we can safely ignore the return result
            // since it need not be used to drive name qualification of the
            // function.  Either that or we handle the template arguments
            // explicitly. int amountOfNameQualificationRequired =
            // nameQualificationDepth(functionDeclaration,currentScope,functionDeclaration);
            // DQ (11/18/2013): Restrict this to template instantiations, else
            // failing some astInterface tests (deepcopy.C).
            // nameQualificationDepth(functionDeclaration,currentScope,functionDeclaration);
            if (templateFunction != NULL) {
              nameQualificationDepth(functionDeclaration, currentScope,
                                     functionDeclaration);
            }

            // setNameQualification(functionDeclaration,amountOfNameQualificationRequired);
          }
        }
      }

      // DQ (4/14/2018): Add the name qualification computation to the
      // parameterList_syntax (since it will be used by preference in the
      // unparser). generateNestedTraversalWithExplicitScope( SgNode* node,
      // SgScopeStatement* input_currentScope )
      if (functionDeclaration->get_type_syntax_is_available() == true) {

        // DQ (4/20/2018): This is an error reported by Charles, but in a
        // reproducer testcode that does not generate the error for me.  I
        // expect that it might be an issue of not recompiling the build tree
        // after the header files have been changed between versions that fixed
        // a previous bug (unrelated) and was checked in recently. I prefer the
        // assertion, but I will remove it and support a conditional check for
        // now (before I leave on vacation).
        // ASSERT_not_null(functionDeclaration->get_parameterList_syntax());
        // generateNestedTraversalWithExplicitScope(functionDeclaration->get_parameterList_syntax(),currentScope);
        if (functionDeclaration->get_parameterList_syntax() != NULL) {
          generateNestedTraversalWithExplicitScope(
              functionDeclaration->get_parameterList_syntax(), currentScope);
        } else {
          // We might want to output a message here, but I will avoid doing so
          // now.
        }
      }
    } else {
      // Note that test2005_57.C presents an example that triggers this case and
      // so might be a relevant test code.  Example: "template<typename T> void
      // foobar (T x){ }".
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: SgFunctionDeclaration -- currentScope is not "
                  "available, not clear why! \n");
#endif
      // ROSE_ABORT();
    }
  }

  // Handle references to SgMemberFunctionDeclaration...
  SgMemberFunctionDeclaration *memberFunctionDeclaration =
      isSgMemberFunctionDeclaration(n);
  if (memberFunctionDeclaration != NULL) {
    // Could it be that we only want to do this for the defining declaration?
    // No, since prototypes must also use name qualification!

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    // DQ (9/7/2014): Added debugging to verify that this case is supporting
    // name qualification of SgTemplateMemberFunctionDeclaration IR node.
    if (isSgTemplateMemberFunctionDeclaration(memberFunctionDeclaration) !=
        NULL) {
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Note: This case supports SgTemplateMemberFunctionDeclaration as "
          "well: memberFunctionDeclaration = %p = %s \n",
          memberFunctionDeclaration,
          memberFunctionDeclaration->get_name().str());
      // ROSE_ABORT();
    }
#endif

    // We need the structural location in scope (not the semantic one).
    SgScopeStatement *currentScope =
        isSgScopeStatement(memberFunctionDeclaration->get_parent());

    // DQ (4/20/2018): Added new code to support where member functions are used
    // in SgTemplateInstantiationDirectiveStatement. DQ (4/20/2018): When the
    // parent is not a scope, it could be a
    // SgTemplateInstantiationDirectiveStatement, in which case we want the
    // parent of that. See test2017_66.C (and previously test2006_08.C) for an
    // example of this case.
    if (currentScope == NULL) {
      SgTemplateInstantiationDirectiveStatement
          *templateInstantiationDirectiveStatement =
              isSgTemplateInstantiationDirectiveStatement(
                  memberFunctionDeclaration->get_parent());
      if (templateInstantiationDirectiveStatement != NULL) {
        currentScope = isSgScopeStatement(
            templateInstantiationDirectiveStatement->get_parent());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Case of (memberFunctionDeclaration != NULL): reset using "
            "SgTemplateInstantiationDirectiveStatement: currentScope = %p \n",
            currentScope);
#endif
        // Now we should have a valid currentScope.
        ASSERT_not_null(currentScope);
      }
    }

    // ASSERT_not_null(currentScope);
    if (currentScope != NULL) {
      // Handle the function return type...
      ASSERT_not_null(memberFunctionDeclaration->get_orig_return_type());
      ASSERT_not_null(memberFunctionDeclaration->get_type());
      ASSERT_not_null(memberFunctionDeclaration->get_type()->get_return_type());
      SgType *returnType = memberFunctionDeclaration->get_orig_return_type();
      ASSERT_not_null(returnType);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "case SgMemberFunctionDeclaration: returnType = %p = %s = %s \n",
          returnType, returnType->class_name().c_str(),
          returnType->unparseToString().c_str());
      SgType *return_syntax_type = NULL;
      // DQ (2/25/2019): Use the type syntax when it is available.
      if (memberFunctionDeclaration->get_type_syntax_is_available() == true) {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgMemberFunctionDeclaration: Using the type_syntax "
                    "since it is available: "
                    "memberFunctionDeclaration->get_type_syntax() = %p \n",
                    memberFunctionDeclaration->get_type_syntax());
        SgFunctionType *functionType =
            isSgFunctionType(memberFunctionDeclaration->get_type_syntax());
        ASSERT_not_null(functionType);
        // return_syntax_type = memberFunctionDeclaration->get_type_syntax();
        if (functionType->get_orig_return_type() != NULL) {
          return_syntax_type = functionType->get_orig_return_type();
        } else {
          return_syntax_type = functionType->get_return_type();
        }
        ASSERT_not_null(return_syntax_type);
      }

      if (return_syntax_type != NULL) {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgMemberFunctionDeclaration: return_syntax_type = %p "
                    "= %s = %s \n",
                    return_syntax_type,
                    return_syntax_type->class_name().c_str(),
                    return_syntax_type->unparseToString().c_str());
      }
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case SgMemberFunctionDeclaration: returnType = %p = %s \n",
                  returnType, returnType->class_name().c_str());
      SgTemplateType *template_returnType = isSgTemplateType(returnType);
      if (template_returnType != NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "template_returnType                                    = %p \n",
            template_returnType);
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "template_returnType->get_name()                        = %s \n",
            template_returnType->get_name().str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "template_returnType->get_template_parameter_position() = %d \n",
            template_returnType->get_template_parameter_position());
      }
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "   --- memberFunctionDeclaration->get_firstNondefiningDeclaration() "
          "= %p = %s \n",
          memberFunctionDeclaration->get_firstNondefiningDeclaration(),
          memberFunctionDeclaration->get_firstNondefiningDeclaration()
              ->class_name()
              .c_str());
      if (memberFunctionDeclaration->get_definingDeclaration() != NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- memberFunctionDeclaration->get_definingDeclaration() = %p "
            "= %s \n",
            memberFunctionDeclaration->get_definingDeclaration(),
            memberFunctionDeclaration->get_definingDeclaration()
                ->class_name()
                .c_str());
      }
      MLOG_WARN_C(MLOG_UNPARSER,
                  "memberFunctionDeclaration->get_type() = %p = %s \n",
                  memberFunctionDeclaration->get_type(),
                  memberFunctionDeclaration->get_type()->class_name().c_str());
#endif

      if (si::typeCarriesWrittenNonrealQualification(returnType)) {
        const bool preserve_written_type_elaboration =
            memberFunctionDeclaration
                ->get_type_elaboration_required_for_return_type();
        memberFunctionDeclaration
            ->set_global_qualification_required_for_return_type(false);
        memberFunctionDeclaration
            ->set_name_qualification_length_for_return_type(0);
        memberFunctionDeclaration
            ->set_type_elaboration_required_for_return_type(
                preserve_written_type_elaboration);
        qualifiedNameMapForTypes[memberFunctionDeclaration] = "";
      } else {
        SgDeclarationStatement *declaration =
            getDeclarationAssociatedWithType(returnType);
        if (declaration != NULL) {

          int amountOfNameQualificationRequiredForReturnType =
              nameQualificationDepth(declaration, currentScope,
                                     memberFunctionDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgMemberFunctionDeclaration's return type: "
                      "amountOfNameQualificationRequiredForType = %d \n",
                      amountOfNameQualificationRequiredForReturnType);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Putting the name qualification for the type into the "
                      "return type of SgMemberFunctionDeclaration = %p = %s \n",
                      memberFunctionDeclaration,
                      memberFunctionDeclaration->get_name().str());
#endif
          setNameQualificationReturnType(
              memberFunctionDeclaration, declaration,
              amountOfNameQualificationRequiredForReturnType);
        } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "declaration == NULL: could not put name qualification for "
              "the type into the return type of "
              "SgMemberFunctionDeclaration = %p = %s \n",
              memberFunctionDeclaration,
              memberFunctionDeclaration->get_name().str());
#endif
        }
      }

      // DQ (6/3/2011): Traverse the type to set any possible template arguments
      // (or other subtypes?) that require name qualification.
      traverseType(returnType, memberFunctionDeclaration, currentScope,
                   memberFunctionDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "Don't forget possible covariant return types "
                                 "for SgMemberFunctionDeclaration IR nodes \n");

      // Only use name qualification where the scopes of the declaration's use
      // (currentScope) is not the same as the scope of the function
      // declaration.  However, the analysis should work and determin that the
      // required name qualification length is zero.
      MLOG_WARN_C(MLOG_UNPARSER,
                  "I would like to not have to have this "
                  "SgMemberFunctionDeclaration logic, we should get the name "
                  "qualification correct more directly. \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "   --- memberFunctionDeclaration->get_scope() = %p = %s \n",
                  memberFunctionDeclaration->get_scope(),
                  memberFunctionDeclaration->get_scope()->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER,
                  "   --- currentScope                           = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
#endif
      if (currentScope != memberFunctionDeclaration->get_scope()) {
        // DQ (1/21/2013): Note that the concept of equivalent scope is fine
        // here if it only tests the equivalence of the pointers. We can't have
        // member functions in namespaces so we don't require that more general
        // test for scope equivalence.

        int amountOfNameQualificationRequired = nameQualificationDepth(
            memberFunctionDeclaration, currentScope, memberFunctionDeclaration);
        if (SgClassDeclaration *associatedClassDeclaration =
                isSgClassDeclaration(memberFunctionDeclaration
                                         ->get_associatedClassDeclaration())) {
          if (SgClassDeclaration *firstNondefining = isSgClassDeclaration(
                  associatedClassDeclaration
                      ->get_firstNondefiningDeclaration())) {
            associatedClassDeclaration = firstNondefining;
          }

          // Out-of-class member syntax must always qualify through the
          // associated class itself, even when that class lives in the global
          // scope. Reuse the class's own required depth to avoid overcounting
          // outer namespaces made visible through using directives, but force
          // at least one qualification level so the class name is preserved.
          auto scope_to_class_decl =
              [](SgScopeStatement *scope) -> SgClassDeclaration * {
            if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
              return isSgClassDeclaration(class_def->get_declaration());
            }
            if (SgTemplateClassDefinition *template_def =
                    isSgTemplateClassDefinition(scope)) {
              return isSgTemplateClassDeclaration(
                  template_def->get_declaration());
            }
            if (SgTemplateInstantiationDefn *inst_def =
                    isSgTemplateInstantiationDefn(scope)) {
              return isSgTemplateInstantiationDecl(inst_def->get_declaration());
            }
            return nullptr;
          };

          auto next_enclosing_scope =
              [&](SgScopeStatement *scope) -> SgScopeStatement * {
            if (scope == NULL) {
              return NULL;
            }

            if (SgClassDeclaration *class_decl = scope_to_class_decl(scope)) {
              return class_decl->get_scope();
            }

            if (SgNamespaceDefinitionStatement *namespace_def =
                    isSgNamespaceDefinitionStatement(scope)) {
              if (SgNamespaceDeclarationStatement *namespace_decl =
                      namespace_def->get_namespaceDeclaration()) {
                return namespace_decl->get_scope();
              }
            }

            return isSgScopeStatement(scope->get_parent());
          };

          auto associated_class_qualification_depth =
              [&](SgClassDeclaration *class_decl) -> int {
            if (class_decl == NULL) {
              return 0;
            }

            int depth = 1;
            for (SgScopeStatement *scope = class_decl->get_scope();
                 scope != NULL &&
                 !SgScopeStatement::isEquivalentScope(scope, currentScope);
                 scope = next_enclosing_scope(scope)) {
              if (scope_to_class_decl(scope) != NULL) {
                ++depth;
              } else if (SgNamespaceDefinitionStatement *namespace_def =
                             isSgNamespaceDefinitionStatement(scope)) {
                SgNamespaceDeclarationStatement *namespace_decl =
                    namespace_def->get_namespaceDeclaration();
                if (namespace_decl == NULL ||
                    !namespace_decl->get_isUnnamedNamespace()) {
                  ++depth;
                }
              }
            }

            return depth;
          };

          amountOfNameQualificationRequired =
              std::max(1, nameQualificationDepth(associatedClassDeclaration,
                                                 currentScope,
                                                 memberFunctionDeclaration));
          amountOfNameQualificationRequired = std::max(
              amountOfNameQualificationRequired,
              associated_class_qualification_depth(associatedClassDeclaration));
        }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "SgMemberFunctionDeclaration: "
                    "amountOfNameQualificationRequired = %d \n",
                    amountOfNameQualificationRequired);
#endif
        setNameQualification(memberFunctionDeclaration,
                             amountOfNameQualificationRequired);

      } else {
        // DQ (9/7/2014): This branch is taken by the non-defining template
        // member functions defined outside of their associated template class
        // declarations. There are also other cases where this branch is taken.

        // Don't know what test code exercises this case (see test2005_73.C).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "WARNING: SgMemberFunctionDeclaration -- currentScope is not "
            "available through predicate (currentScope != "
            "memberFunctionDeclaration->get_scope()), not clear why! \n");
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- memberFunctionDeclaration->get_scope() = %p = %s \n",
            memberFunctionDeclaration->get_scope(),
            memberFunctionDeclaration->get_scope()->class_name().c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "   --- currentScope                           = %p = %s \n",
            currentScope, currentScope->class_name().c_str());
#endif
        // ROSE_ABORT();
      }

      // DQ (4/14/2018): Add the name qualification computation to the
      // parameterList_syntax (since it will be used by preference in the
      // unparser). generateNestedTraversalWithExplicitScope( SgNode* node,
      // SgScopeStatement* input_currentScope )
      if (memberFunctionDeclaration->get_type_syntax_is_available() == true) {
        ASSERT_not_null(memberFunctionDeclaration->get_parameterList_syntax());
        generateNestedTraversalWithExplicitScope(
            memberFunctionDeclaration->get_parameterList_syntax(),
            currentScope);
      }
    } else {
      // Note that test2005_63.C presents an example that triggers this case and
      // so might be a relevant. This is also the reason why test2005_73.C is
      // failing!!!  Fix it tomorrow!!!
      // (SgTemplateInstantiationDirectiveStatement)
      SgDeclarationStatement *currentStatement =
          isSgDeclarationStatement(memberFunctionDeclaration->get_parent());

      // DQ (9/4/2014): Lambda functions (in SgLambdaExp) are an example where
      // this fails. ASSERT_not_null(currentStatement);
      if (currentStatement != NULL) {
        SgScopeStatement *currentScope =
            isSgScopeStatement(currentStatement->get_parent());
        if (currentScope != NULL) {
          int amountOfNameQualificationRequired =
              nameQualificationDepth(memberFunctionDeclaration, currentScope,
                                     memberFunctionDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgMemberFunctionDeclaration: "
                      "amountOfNameQualificationRequired = %d \n",
                      amountOfNameQualificationRequired);
#endif
          setNameQualification(memberFunctionDeclaration,
                               amountOfNameQualificationRequired);

          // DQ (4/14/2018): Add the name qualification computation to the
          // parameterList_syntax (since it will be used by preference in the
          // unparser). generateNestedTraversalWithExplicitScope( SgNode* node,
          // SgScopeStatement* input_currentScope )
          if (memberFunctionDeclaration->get_type_syntax_is_available() ==
              true) {
            // DQ (4/14/2018): I can't detect that we have any test codes that
            // reach here! This might be a subject to ingestigate later.
            ASSERT_not_null(
                memberFunctionDeclaration->get_parameterList_syntax());
            generateNestedTraversalWithExplicitScope(
                memberFunctionDeclaration->get_parameterList_syntax(),
                currentScope);
          }
        } else {
          MLOG_WARN_C(MLOG_UNPARSER,
                      "WARNING: SgMemberFunctionDeclaration -- currentScope is "
                      "not available through parent SgDeclarationStatement, "
                      "not clear why! \n");
          ROSE_ABORT();
        }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "WARNING: SgMemberFunctionDeclaration -- currentScope is "
                    "not available, not clear why! \n");
#endif
        // ROSE_ABORT();
      } else {
        // This should only be a lambda function defined in a SgLambdaExp.
        ASSERT_not_null(isSgLambdaExp(memberFunctionDeclaration->get_parent()));
      }
    }
  }

  // DQ (6/3/2017): Since the underling template instantiation is not shared,
  // and becasue it is traversed explicitly, We can (I think) reserve the name
  // qualification of the template instantiation to the instantiated template
  // function directly and need not support an additional (redundant) evaluation
  // of name qualification here.

  // DQ (4/3/2014): Added new case to address no longer traversing this IR
  // node's member. See test2005_73.C.
  SgTemplateInstantiationDirectiveStatement
      *templateInstantiationDirectiveStatement =
          isSgTemplateInstantiationDirectiveStatement(n);
  if (templateInstantiationDirectiveStatement != NULL) {
    // DQ (4/3/2014): We no longer traverse the declaration referenced in this
    // IR node so we have to handle the name qualification requiredments for the
    // associated declaration directly.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "################ Processing "
                "SgTemplateInstantiationDirectiveStatement (name qualification "
                "is handled within the nested template instantiation) \n");
#endif

    SgDeclarationStatement *declarationStatement = isSgDeclarationStatement(
        templateInstantiationDirectiveStatement->get_declaration());
    if (declarationStatement != NULL) {
      SgScopeStatement *currentScope = isSgScopeStatement(
          templateInstantiationDirectiveStatement->get_parent());
      if (currentScope != NULL) {
        int amountOfNameQualificationRequired =
            nameQualificationDepth(declarationStatement, currentScope,
                                   templateInstantiationDirectiveStatement);
        if (SgClassDeclaration *classDeclaration =
                isSgClassDeclaration(declarationStatement)) {
          setNameQualification(classDeclaration,
                               amountOfNameQualificationRequired);
        } else if (SgFunctionDeclaration *functionDeclaration =
                       isSgFunctionDeclaration(declarationStatement)) {
          setNameQualification(functionDeclaration,
                               amountOfNameQualificationRequired);
        }
      }
    }
  }

  // DQ (5/14/2011): Added support for the name qualification of the base type
  // used in typedefs. Handle references to SgTypedefDeclaration...
  SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(n);
  if (typedefDeclaration != NULL) {
    // Could it be that we only want to do this for the defining declaration?
    // No, since prototypes must also use name qualification!

#define DEBUG_TYPEDEF (DEBUG_NAME_QUALIFICATION_LEVEL > 3)

    // We need the structural location in scope (not the semantic one).
    // SgScopeStatement* currentScope =
    // isSgScopeStatement(typedefDeclaration->get_parent());
    SgScopeStatement *currentScope = typedefDeclaration->get_scope();
    ASSERT_not_null(currentScope);

    SgType *baseType = typedefDeclaration->get_base_type();
    ASSERT_not_null(baseType);
    SgDeclarationStatement *baseTypeDeclaration =
        associatedDeclaration(baseType);

    // DQ (4/10/2019): Handle the case when this is a typedef of a
    // SgPointrMemberType.
    SgDeclarationStatement *pointerMemberClassDeclaration = NULL;

    SgPointerMemberType *pointerMemberType = isSgPointerMemberType(baseType);
    if (pointerMemberType != NULL) {
#if DEBUG_TYPEDEF
      MLOG_WARN_C(MLOG_UNPARSER, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
                                 "$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
      MLOG_WARN_C(MLOG_UNPARSER, "Found a SgPointerMemberType in the base type "
                                 "of a SgTypedefDeclaration \n");
      MLOG_WARN_C(MLOG_UNPARSER, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
                                 "$$$$$$$$$$$$$$$$$$$$$$$$$ \n");

      MLOG_WARN_C(
          MLOG_UNPARSER,
          "We need to make a recursive type traversal for this case! \n");
#endif

#if DEBUG_TYPEDEF
      MLOG_WARN_C(MLOG_UNPARSER, "#############################################"
                                 "################################ \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Case SgTypedefDeclaration: Calling "
                  "generateNestedTraversalWithExplicitScope() \n");
      MLOG_WARN_C(MLOG_UNPARSER, "#############################################"
                                 "################################ \n");
#endif
      generateNestedTraversalWithExplicitScope(
          baseType, currentScope, typedefDeclaration, typedefDeclaration);

      // DQ (4/19/2019): It might be that we should call this after the traveral
      // over each type instead of before we traverse the type. This way we save
      // the correctly computed string for each type after the different parts
      // of name qualificaiton are in place.
      traverseType(baseType, typedefDeclaration, currentScope,
                   typedefDeclaration);

#if DEBUG_TYPEDEF
      MLOG_WARN_C(MLOG_UNPARSER,
                  "############################################################"
                  "########################################### \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Case SgTypedefDeclaration: DONE: Processing the recursive "
                  "evaluation of the SgInitializedName IR's type \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "############################################################"
                  "########################################### \n");
#endif

    } else {
      // DQ (4/28/2019): (refactored) Put the rest of the
      // non-SgPointerMemberType support into the false block.

      // If the base type is defined in the typedef directly then it should need
      // no name qualification by definition.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration()"
          " = %s \n",
          typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration()
              ? "true"
              : "false");
#endif

      // DQ (4/10/2019): Handle the case when this is a typedef of a
      // SgPointrMemberType.
      if (pointerMemberClassDeclaration != NULL) {
        int amountOfNameQualificationRequiredOnPointerMemberClass =
            nameQualificationDepth(pointerMemberClassDeclaration, currentScope,
                                   typedefDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "SgTypedefDeclaration: "
            "amountOfNameQualificationRequiredOnPointerMemberClass = %d \n",
            amountOfNameQualificationRequiredOnPointerMemberClass);
#endif

        ASSERT_not_null(pointerMemberClassDeclaration);
        setNameQualificationOnPointerMemberClass(
            typedefDeclaration, pointerMemberClassDeclaration,
            amountOfNameQualificationRequiredOnPointerMemberClass);
      }

      // This is NULL if the base type is not associated with a declaration
      // (e.g. not a SgNamedType). ASSERT_not_null(baseTypeDeclaration); if
      // (baseTypeDeclaration != NULL)
      if ((baseTypeDeclaration != NULL) &&
          (typedefDeclaration
               ->get_typedefBaseTypeContainsDefiningDeclaration() == false)) {
        int amountOfNameQualificationRequiredForBaseType = 0;
        if (isSgTemplateInstantiationDecl(baseTypeDeclaration) != NULL) {
          amountOfNameQualificationRequiredForBaseType = nameQualificationDepth(
              baseTypeDeclaration, currentScope, typedefDeclaration);
        } else {
          amountOfNameQualificationRequiredForBaseType = nameQualificationDepth(
              baseType, currentScope, typedefDeclaration);
        }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "SgTypedefDeclaration: "
                    "amountOfNameQualificationRequiredForBaseType = %d \n",
                    amountOfNameQualificationRequiredForBaseType);
#endif
        ASSERT_not_null(baseTypeDeclaration);
        // setNameQualification(typedefDeclaration,baseTypeDeclaration,amountOfNameQualificationRequiredForBaseType);
        setNameQualificationOnBaseType(
            typedefDeclaration, baseTypeDeclaration,
            amountOfNameQualificationRequiredForBaseType);
      }

#if DEBUG_TYPEDEF || 0
      // DQ (2/15/2017): Note that this output will be the way to identify the
      // start of a failing infinite loop for
      // tests/nonsmoke/functional/CompileTests/RoseExample_tests/testRoseHeaders_11.C.
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Calling traverseType on SgTypedefDeclaration = %p name = %s \n",
          typedefDeclaration, typedefDeclaration->get_name().str());
#endif

#if DEBUG_TYPEDEF || 0
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration()"
          " = %s \n",
          typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration()
              ? "true"
              : "false");
#endif
      // DQ (5/2/2019): check the defining declaration for the associated
      // declaration (see test2019_427.C). For example, a typedef with multiple
      // declarations ("typedef struct {} A,*Aptr;"). bool skipTraverseType =
      // typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration();
      SgTypedefDeclaration *definingTypedefDeclaration = typedefDeclaration;
      SgDeclarationStatement *assocaitedDeclaration =
          typedefDeclaration->get_declaration();
      SgDeclarationStatement *definingDeclaration = assocaitedDeclaration;
      if (assocaitedDeclaration != NULL) {
        definingDeclaration = assocaitedDeclaration->get_definingDeclaration();
        // ASSERT_not_null(definingDeclaration);
        if (definingDeclaration != NULL) {
          definingTypedefDeclaration =
              isSgTypedefDeclaration(definingDeclaration->get_parent());

          // DQ (5/2/2019): If this is NULL, then use the original version.
          if (definingTypedefDeclaration == NULL) {
            definingTypedefDeclaration = typedefDeclaration;
          }
        }
        ASSERT_not_null(definingTypedefDeclaration);
      }

      bool skipTraverseType =
          definingTypedefDeclaration
              ->get_typedefBaseTypeContainsDefiningDeclaration();
      // DQ (6/3/2011): Traverse the type to set any possible template arguments
      // (or other subtypes?) that require name qualification.
      // traverseType(baseType,typedefDeclaration,currentScope,typedefDeclaration);
      if (skipTraverseType == false) {
        traverseType(baseType, typedefDeclaration, currentScope,
                     typedefDeclaration);

#if DEBUG_TYPEDEF || 0
        MLOG_WARN_C(MLOG_UNPARSER,
                    "DONE: Calling traverseType on SgTypedefDeclaration = %p "
                    "name = %s \n",
                    typedefDeclaration, typedefDeclaration->get_name().str());
#endif
      } else {
#if DEBUG_TYPEDEF || 0
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Skipped call to traverseType for case "
                    "SgTypedefDeclaration = %p name = %s \n",
                    typedefDeclaration, typedefDeclaration->get_name().str());
#endif
      }

      // DQ (4/14/2018): Adding support for name qualification of template
      // arguments (though it should not be requirted for the tyepdef directly).
      SgTemplateInstantiationTypedefDeclaration
          *templateInstantiationTypedefDeclaration =
              isSgTemplateInstantiationTypedefDeclaration(typedefDeclaration);
      if (templateInstantiationTypedefDeclaration != NULL) {
        // This point of calling this function is to just have the template
        // arguments evaluated for name qualification (see
        // Cxx11_tests/test2018_68.C).

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        int amountOfNameQualificationRequired = nameQualificationDepth(
            templateInstantiationTypedefDeclaration, currentScope,
            templateInstantiationTypedefDeclaration);
        MLOG_WARN_C(MLOG_UNPARSER,
                    "SgTemplateInstantiationTypedefDeclaration: "
                    "amountOfNameQualificationRequired = %d \n",
                    amountOfNameQualificationRequired);

        // DQ (4/14/2018): Report anything that is unusual, i.e. non-zero name
        // qualification length.
        if (amountOfNameQualificationRequired > 0) {
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Warning: name qualification length should be zero for a "
                      "templateInstantiationTypedefDeclaration declared in the "
                      "same scope: amountOfNameQualificationRequired = %d \n",
                      amountOfNameQualificationRequired);

          MLOG_WARN_C(
              MLOG_UNPARSER,
              "templateInstantiationTypedefDeclaration = %p = %s = %s \n",
              templateInstantiationTypedefDeclaration,
              templateInstantiationTypedefDeclaration->class_name().c_str(),
              functionDeclaration->get_mangled_name().str());
        }
#endif
      }

      // DQ (4/28/2019): (refactored) Put the rest of the
      // non-SgPointerMemberType support into the false block.
    }
  }

  // Handle references in SgUsingDirectiveStatement...
  SgUsingDirectiveStatement *usingDirective = isSgUsingDirectiveStatement(n);
  if (usingDirective != NULL) {
    SgNamespaceDeclarationStatement *namespaceDeclaration =
        usingDirective->get_namespaceDeclaration();
    ASSERT_not_null(namespaceDeclaration);
    SgScopeStatement *currentScope = usingDirective->get_scope();
    ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s = %s \n", currentScope,
                currentScope->class_name().c_str(),
                SageInterface::get_name(currentScope).c_str());
#endif

    int amountOfNameQualificationRequired = nameQualificationDepth(
        namespaceDeclaration, currentScope, usingDirective);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "SgUsingDirectiveStatement's SgNamespaceDeclarationStatement: "
                "amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
    setNameQualification(usingDirective, namespaceDeclaration,
                         amountOfNameQualificationRequired);
  }

  SgUsingDeclarationStatement *usingDeclaration =
      isSgUsingDeclarationStatement(n);
  if (usingDeclaration != NULL) {
    SgDeclarationStatement *associatedDeclaration =
        usingDeclaration->get_declaration();
    SgInitializedName *associatedInitializedName =
        usingDeclaration->get_initializedName();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In case for SgUsingDeclarationStatement: "
                "associatedDeclaration = %p associatedInitializedName = %p \n",
                associatedDeclaration, associatedInitializedName);
    if (associatedDeclaration != NULL)
      MLOG_WARN_C(MLOG_UNPARSER, "associatedDeclaration = %p = %s = %s = %s \n",
                  associatedDeclaration,
                  associatedDeclaration->class_name().c_str(),
                  SageInterface::get_name(associatedDeclaration).c_str(),
                  SageInterface::generateUniqueName(associatedDeclaration, true)
                      .c_str());
    if (associatedInitializedName != NULL)
      MLOG_WARN_C(
          MLOG_UNPARSER, "associatedInitializedName = %p = %s = %s = %s \n",
          associatedInitializedName,
          associatedInitializedName->class_name().c_str(),
          SageInterface::get_name(associatedInitializedName).c_str(),
          SageInterface::generateUniqueName(associatedInitializedName, true)
              .c_str());
#endif
    SgScopeStatement *currentScope = usingDeclaration->get_scope();
    ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s = %s \n", currentScope,
                currentScope->class_name().c_str(),
                SageInterface::get_name(currentScope).c_str());
#endif

    int amountOfNameQualificationRequired = 0;
    if (associatedDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "associatedDeclaration != NULL: associatedDeclaration = %p = "
                  "%s = %s \n",
                  associatedDeclaration,
                  associatedDeclaration->class_name().c_str(),
                  SageInterface::get_name(associatedDeclaration).c_str());
      MLOG_WARN_C(MLOG_UNPARSER,
                  "associatedDeclaration != NULL: currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
      if (currentScope->get_scope() != NULL)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "associatedDeclaration != NULL: currentScope->get_scope() "
                    "= %p = %s \n",
                    currentScope->get_scope(),
                    currentScope->get_scope()->class_name().c_str());
#endif
      // DQ (3/31/2014): Compute the required depth for global qualification
      // (required for using declarations).
      amountOfNameQualificationRequired =
          depthOfGlobalNameQualification(associatedDeclaration);

      // DQ (1/11/2019): Don't outout added global qualification for the case of
      // a inheriting constructor. DQ (6/22/2011): If the
      // amountOfNameQualificationRequired is zero then add one to force at
      // least global qualification. See test2004_80.C for an example. if
      // (isSgGlobal(currentScope->get_scope()) != NULL &&
      // amountOfNameQualificationRequired == 0)
      bool is_inheriting_constructor =
          usingDeclaration->get_is_inheriting_constructor();
      if (is_inheriting_constructor == false &&
          isSgGlobal(currentScope->get_scope()) != NULL &&
          amountOfNameQualificationRequired == 0) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Handling special case to force at least global qualification. \n");
#endif
        amountOfNameQualificationRequired += 1;
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgUsingDeclarationStatement's associatedDeclaration: "
                  "amountOfNameQualificationRequired = %d \n",
                  amountOfNameQualificationRequired);
#endif

      // DQ (1/10/2019): If this is a constructor, member function, then we must
      // qualifiy it to distinquish it from the base class name.
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          isSgMemberFunctionDeclaration(associatedDeclaration);
      if (memberFunctionDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "Found a member function = %p name = %s \n",
                    memberFunctionDeclaration,
                    memberFunctionDeclaration->get_name().str());
#endif
        if (memberFunctionDeclaration->get_specialFunctionModifier()
                .isConstructor() == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER, "Found a constructor = %p name = %s \n",
                      memberFunctionDeclaration,
                      memberFunctionDeclaration->get_name().str());
#endif
        }
      }

      setNameQualification(usingDeclaration, associatedDeclaration,
                           amountOfNameQualificationRequired);
    } else {
      ASSERT_not_null(associatedInitializedName);
      amountOfNameQualificationRequired = nameQualificationDepth(
          associatedInitializedName, currentScope, usingDeclaration);

      setNameQualification(usingDeclaration, associatedInitializedName,
                           amountOfNameQualificationRequired);
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "SgUsingDeclarationStatement's SgVarRefExp: "
                "amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
  }

  // DQ (7/8/2014): Adding support for name qualification of the
  // SgNamespaceDeclarationStatement referenced by a
  // SgNamespaceAliasDeclarationStatement. Handle references in
  // SgNamespaceAliasDeclarationStatement...
  SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
      isSgNamespaceAliasDeclarationStatement(n);
  if (namespaceAliasDeclaration != NULL) {
    string namespaceDeclarationName;
    // SgNamespaceDeclarationStatement* namespaceDeclaration =
    // namespaceAliasDeclaration->get_namespaceDeclaration();
    SgDeclarationStatement *namespaceDeclaration = NULL;
    if (namespaceAliasDeclaration->get_is_alias_for_another_namespace_alias() ==
        true) {
      namespaceDeclaration =
          namespaceAliasDeclaration->get_namespaceAliasDeclaration();

      // DQ (8/1/2020): Set the name.
      namespaceDeclarationName =
          namespaceAliasDeclaration->get_namespaceAliasDeclaration()
              ->get_name();
    } else {
      namespaceDeclaration =
          namespaceAliasDeclaration->get_namespaceDeclaration();

      // DQ (8/1/2020): Set the name.
      namespaceDeclarationName =
          namespaceAliasDeclaration->get_namespaceDeclaration()->get_name();
    }
    ASSERT_not_null(namespaceDeclaration);

    // DQ (8/1/2020): Record the associated NamespaceAliasDeclarationStatement
    // so it can be used instead in namequalification.
    // inheritedAttribute.get_namespaceAliasDeclarationMap().insert(pair<SgDeclarationStatement*,SgNamespaceAliasDeclarationStatement*>(namespaceDeclaration,namespaceAliasDeclaration));

    namespaceAliasDeclarationMap.insert(
        pair<SgDeclarationStatement *, SgNamespaceAliasDeclarationStatement *>(
            namespaceDeclaration, namespaceAliasDeclaration));

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
    printf("In evaluateInheritedAttribute(): Saved namespace alias: %s to "
           "namespace declaration = %s \n",
           namespaceAliasDeclaration->get_name().str(),
           namespaceDeclarationName.c_str());
    printf(" --- namespaceAliasDeclarationMap.size() = %zu \n",
           namespaceAliasDeclarationMap.size());
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
#endif
    SgScopeStatement *currentScope = namespaceAliasDeclaration->get_scope();
    ASSERT_not_null(currentScope);

    int amountOfNameQualificationRequired = nameQualificationDepth(
        namespaceDeclaration, currentScope, namespaceAliasDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "SgNamespaceAliasDeclarationStatement's "
                "SgNamespaceDeclarationStatement: "
                "amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
    setNameQualification(namespaceAliasDeclaration, namespaceDeclaration,
                         amountOfNameQualificationRequired);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "DONE: SgNamespaceAliasDeclarationStatement's "
                "SgNamespaceDeclarationStatement: "
                "amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
  }

  SgNonrealRefExp *nrRefExp = isSgNonrealRefExp(n);
  if (nrRefExp != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "case SgNonrealRefExp: nrRefExp = %p\n",
                nrRefExp);
#endif
    SgNonrealSymbol *nrsym = nrRefExp->get_symbol();
    ASSERT_not_null(nrsym);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, " --- nrsym = %p : %s\n", nrsym,
                nrsym->get_name().str());
#endif

    SgNonrealDecl *nrdecl = nrsym->get_declaration();
    ASSERT_not_null(nrdecl);

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(nrRefExp);
    if (currentStatement != NULL) {
      SgScopeStatement *currentScope = currentStatement->get_scope();

      SgTemplateArgumentPtrList &expr_args = nrRefExp->get_templateArguments();
      SgTemplateArgumentPtrList &decl_args = nrdecl->get_tpl_args();
      SgTemplateArgumentPtrList &tpl_args =
          !expr_args.empty() ? expr_args : decl_args;
      evaluateNameQualificationForTemplateArgumentList(tpl_args, currentScope,
                                                       currentStatement);

      SgDeclarationStatement *declstmt = nrdecl;
      if (nrdecl->get_templateDeclaration() != NULL) {
        declstmt = nrdecl->get_templateDeclaration();
      }

      if (SgNode::get_globalQualifiedNameMapForNames().find(nrRefExp) ==
          SgNode::get_globalQualifiedNameMapForNames().end()) {
        int amountOfNameQualificationRequired =
            nameQualificationDepth(declstmt, currentScope, currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    " --- amountOfNameQualificationRequired = %d\n",
                    amountOfNameQualificationRequired);
#endif
        setNameQualification(nrRefExp, declstmt,
                             amountOfNameQualificationRequired);
      }
    }
  } else {
#if WARNING_FOR_NONREAL_DEVEL
    MLOG_WARN_C(MLOG_UNPARSER,
                "Skipping name-qualification of SgNonrealRefExp as no "
                "enclosing statement could be found ROSE-1701 (not an issue "
                "while template are unparsed from string).\n");
#endif
  }

  // DQ (5/12/2011): We want to located name qualification information about
  // referenced functions at the SgFunctionRefExp and SgMemberFunctionRefExp IR
  // node instead of the SgFunctionCallExp IR node.
  SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(n);
  if (functionRefExp != NULL) {

    SgFunctionDeclaration *functionDeclaration =
        functionRefExp->getAssociatedFunctionDeclaration();
    // ASSERT_not_null(functionDeclaration);
    if (functionDeclaration != NULL) {
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(functionRefExp);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "!!!!!!!!!!!!!!! case SgFunctionRefExp: currentStatement = "
                  "%p = %s \n",
                  currentStatement, currentStatement->class_name().c_str());
#endif
      // DQ (9/17/2011); Added escape for where the currentStatement == NULL
      // (fails for STL code when the original expression trees are used to
      // eliminate the constant folded values).
      // ASSERT_not_null(currentStatement);
      if (currentStatement != NULL) {
        // DQ (3/15/2019): If this is part of an recursive call then the
        // inheritedAttribute.get_currentScope() is set and we should use it as
        // the currentScope. DQ (9/17/2011): this is the original case we want
        // to restore later... SgScopeStatement* currentScope =
        // currentStatement->get_scope();
        SgScopeStatement *currentScope = NULL;
        if (inheritedAttribute.get_currentScope() != NULL) {
          currentScope = inheritedAttribute.get_currentScope();
        } else {
          currentScope = currentStatement->get_scope();
        }
        // ASSERT_not_null(currentScope);

        // DQ (1/31/2019): If this is a member function or template member
        // function instantiation, AND it is definted outside of the class scope
        // THEN we need to use the structural scope instead of the logical
        // scope.
        if (currentScope != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "!!!!!!!!!!!!!!! currentScope = %p = %s \n", currentScope,
                      currentScope->class_name().c_str());
#endif
          SgStatement *parentStatement =
              isSgStatement(currentStatement->get_parent());
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "!!!!!!!!!!!!!!! parentStatement = %p = %s \n",
                      parentStatement, parentStatement->class_name().c_str());
#endif
          if (parentStatement != currentScope) {
            currentScope = parentStatement->get_scope();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "!!!!!!!!!!!!!!! RESETTING VIA PARENT: currentScope = "
                        "%p = %s \n",
                        currentScope, currentScope->class_name().c_str());
#endif
          }
        }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgFunctionRefExp: currentScope = %p = %s \n",
                    currentScope, currentScope->class_name().c_str());
#endif
        int amountOfNameQualificationRequired = nameQualificationDepth(
            functionDeclaration, currentScope, currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "SgFunctionCallExp's function name: "
                    "amountOfNameQualificationRequired = %d \n",
                    amountOfNameQualificationRequired);
#endif
        // Name-qualification for function references is gated by
        // skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(),
        // which consults referencedNameSet. Declarations we don't
        // traverse during unparsing (common for system headers)
        // won't be recorded there, so populate referencedNameSet
        // here when we see the reference (while still excluding
        // non-definable scopes such as SgBasicBlock).

        SgDeclarationStatement *declarationForReferencedNameSet =
            functionDeclaration->get_firstNondefiningDeclaration();
        if (declarationForReferencedNameSet == NULL) {
          declarationForReferencedNameSet =
              functionDeclaration->get_definingDeclaration();
          if (declarationForReferencedNameSet == NULL) {
            declarationForReferencedNameSet = functionDeclaration;
            ASSERT_not_null(declarationForReferencedNameSet);
          }
          ASSERT_not_null(declarationForReferencedNameSet);
        }
        ASSERT_not_null(declarationForReferencedNameSet);

        // Match
        // skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable():
        // for template instantiations, use the template declaration
        // as the representative in referencedNameSet.
        SgTemplateInstantiationFunctionDecl *templateInstantiationFunctionDecl =
            isSgTemplateInstantiationFunctionDecl(functionDeclaration);
        if (templateInstantiationFunctionDecl != NULL &&
            templateInstantiationFunctionDecl->get_templateDeclaration() !=
                NULL) {
          declarationForReferencedNameSet =
              templateInstantiationFunctionDecl->get_templateDeclaration();
          ASSERT_not_null(declarationForReferencedNameSet);
        } else {
          SgTemplateInstantiationMemberFunctionDecl
              *templateInstantiationMemberFunctionDecl =
                  isSgTemplateInstantiationMemberFunctionDecl(
                      functionDeclaration);
          if (templateInstantiationMemberFunctionDecl != NULL &&
              templateInstantiationMemberFunctionDecl
                      ->get_templateDeclaration() != NULL) {
            declarationForReferencedNameSet =
                templateInstantiationMemberFunctionDecl
                    ->get_templateDeclaration();
            ASSERT_not_null(declarationForReferencedNameSet);
          }
        }

        SgScopeStatement *scopeOfDeclaration =
            isSgScopeStatement(declarationForReferencedNameSet->get_parent());
        bool acceptableDeclarationScope =
            (scopeOfDeclaration != NULL &&
             scopeOfDeclaration->variantT() != V_SgBasicBlock);

        if (acceptableDeclarationScope == true &&
            should_preseed_referenced_name_for_untraversed_declaration(
                currentStatement, declarationForReferencedNameSet) &&
            referencedNameSet.find(declarationForReferencedNameSet) ==
                referencedNameSet.end()) {
          referencedNameSet.insert(declarationForReferencedNameSet);
        }

        bool skipNameQualification =
            skipNameQualificationIfNotProperlyDeclaredWhereDeclarationIsDefinable(
                functionDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Test of functionRefExp: skipNameQualification = %s \n",
                    skipNameQualification ? "true" : "false");
#endif
        if (skipNameQualification == false) {
          setNameQualification(functionRefExp, functionDeclaration,
                               amountOfNameQualificationRequired);
        }
      } else {
      }
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: functionDeclaration == NULL in SgFunctionCallExp "
                  "for name qualification support! \n");
#endif
    }

    // If this is a templated function then we have to save the name because its
    // templated name might have template arguments that require name
    // qualification.
    SgTemplateInstantiationFunctionDecl
        *templateInstantiationFunctionDeclaration =
            isSgTemplateInstantiationFunctionDecl(
                functionRefExp->getAssociatedFunctionDeclaration());
    if (templateInstantiationFunctionDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Found a SgTemplateInstantiationFunctionDecl that will have template "
          "arguments that might require qualification. name = %s \n",
          templateInstantiationFunctionDeclaration->get_name().str());
#endif
      // DQ (12/18/2016): When this is a function call in an array type index
      // expression we can't identify an associated statement.
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(functionRefExp);
      // ASSERT_not_null(currentStatement);
      if (currentStatement != NULL) {
        SgScopeStatement *currentScope = currentStatement->get_scope();
        ASSERT_not_null(currentScope);

        // traverseTemplatedFunction(functionRefExp,templateInstantiationFunctionDeclaration,currentScope,currentStatement);
        traverseTemplatedFunction(functionRefExp, functionRefExp, currentScope,
                                  currentStatement);
      } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Note: Name qualification: parent statement could not be "
                    "identified (may be hidden in array type index) for "
                    "functionRefExp = %p = %s \n",
                    functionRefExp, functionRefExp->class_name().c_str());
#endif
      }
    }
  }

#define PSEUDO_DESTRUCTOR_REF_SUPPORT 1

#if PSEUDO_DESTRUCTOR_REF_SUPPORT
  // DQ (1/18/2020): Adding support for SgPseudoDestructorRefExp (see
  // C++11_tests/test2020_56.C).
  SgPseudoDestructorRefExp *pseudoDestructorRefExp =
      isSgPseudoDestructorRefExp(n);
  if (pseudoDestructorRefExp != NULL) {

#define DEBUG_PSEUDO_DESTRUCTOR_REF (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0

#if DEBUG_PSEUDO_DESTRUCTOR_REF
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Detected SgPseudoDestructorRefExp: pseudoDestructorRefExp = %p \n",
        pseudoDestructorRefExp);
#endif
    SgType *type = pseudoDestructorRefExp->get_object_type();
    ASSERT_not_null(type);

    SgNamedType *namedType = isSgNamedType(type);
    // REX FIX: SgPseudoDestructorRefExp may have non-named types (e.g., for
    // primitive types or template type parameters in patterns like ptr->~T()
    // where T is not a class). Skip name qualification for such cases as they
    // don't require it.
    if (namedType == NULL) {
#if DEBUG_PSEUDO_DESTRUCTOR_REF
      MLOG_WARN_C(MLOG_UNPARSER, "Skipping name qualification for non-named "
                                 "type in SgPseudoDestructorRefExp\n");
#endif
      return inheritedAttribute;
    }

    SgDeclarationStatement *declarationStatement = namedType->get_declaration();
    ASSERT_not_null(declarationStatement);

    // if (memberFunctionDeclaration != NULL)
    if (declarationStatement != NULL) {
      // DQ (2/17/2019): Adding support for pointers to member functions.
      // if (isMemberFunctionMemberReference == false)
      // if (isMemberFunctionMemberReference == false || isAddressTaken == true)

      // DQ (2/23/2019): Except that this code works in all cases that I can see
      // at the moment, I think that the current scope should be taken from the
      // type of the pointer being dereferenced instead of from the location of
      // the statement containing the memberFunctionRefExp.  But I can't build a
      // counter example that fails.

      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(pseudoDestructorRefExp);

#if DEBUG_PSEUDO_DESTRUCTOR_REF
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Compute the currentStatement: currentStatement = %p \n",
                  currentStatement);
#endif
      if (currentStatement == NULL) {
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Error: Location of where we can NOT associate the "
                    "expression to a statement \n");
        pseudoDestructorRefExp->get_file_info()->display(
            "Error: currentStatement == NULL: memberFunctionRefExp: debug");
        declarationStatement->get_file_info()->display(
            "Error: currentStatement == NULL: memberFunctionDeclaration: "
            "debug");
      }
      ASSERT_not_null(currentStatement);

#if DEBUG_PSEUDO_DESTRUCTOR_REF
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "case of SgPseudoDestructorRefExp: currentStatement = %p = %s \n",
          currentStatement, currentStatement->class_name().c_str());
#endif
      SgScopeStatement *currentScope = currentStatement->get_scope();
      ASSERT_not_null(currentScope);

#if DEBUG_PSEUDO_DESTRUCTOR_REF
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgPseudoDestructorRefExp: currentScope = %p = %s \n",
                  currentScope, currentScope->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER, "***** case of SgPseudoDestructorRefExp: "
                                 "Calling nameQualificationDepth() ***** \n");
#endif
      // int amountOfNameQualificationRequired =
      // nameQualificationDepth(memberFunctionDeclaration,currentScope,currentStatement);
      int amountOfNameQualificationRequired = nameQualificationDepth(
          declarationStatement, currentScope, currentStatement);

#if DEBUG_PSEUDO_DESTRUCTOR_REF
      MLOG_WARN_C(MLOG_UNPARSER,
                  "***** case of SgPseudoDestructorRefExp: DONE: Calling "
                  "nameQualificationDepth() ***** \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgPseudoDestructorRefExp's name: "
                  "amountOfNameQualificationRequired = %d \n",
                  amountOfNameQualificationRequired);
#endif
      // setNameQualification(memberFunctionRefExp,memberFunctionDeclaration,amountOfNameQualificationRequired);
      setNameQualification(pseudoDestructorRefExp, declarationStatement,
                           amountOfNameQualificationRequired);
      // DQ (2/17/2019): Case of xxx !(isDataMemberReference == true &&
      // isAddressTaken == true)
    } else {
#if DEBUG_PSEUDO_DESTRUCTOR_REF || 0
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "WARNING: declarationStatement == NULL in SgPseudoDestructorRefExp "
          "for name qualification support! \n");
#endif
    }
  }

// #endif for #if PSEUDO_DESTRUCTOR_REF_SUPPORT
#endif

  SgMemberFunctionRefExp *memberFunctionRefExp = isSgMemberFunctionRefExp(n);
  if (memberFunctionRefExp != NULL) {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        memberFunctionRefExp->getAssociatedMemberFunctionDeclaration();
    // ASSERT_not_null(functionDeclaration);

#define DEBUG_MEMBER_FUNCTION_REF (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0

#if DEBUG_MEMBER_FUNCTION_REF
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "case of SgMemberFunctionRefExp: memberFunctionDeclaration = %p \n",
        memberFunctionDeclaration);
#endif

    // DQ (2/7/2019): Adding support for name qualification induced from
    // SgPointerMemberType function paramters.
    bool nameQualificationInducedFromPointerMemberType = false;

    bool isMemberFunctionMemberReference =
        SageInterface::isMemberFunctionMemberReference(memberFunctionRefExp);
    bool isAddressTaken = SageInterface::isAddressTaken(memberFunctionRefExp);

#if DEBUG_MEMBER_FUNCTION_REF
    MLOG_WARN_C(MLOG_UNPARSER, "isMemberFunctionMemberReference = %s \n",
                isMemberFunctionMemberReference ? "true" : "false");
#endif
    // DQ (2/23/2019): I think that the test code test2019_191.C is not setting
    // this correctly. The logic for member function pointers (references) is
    // not yet worked out as well as for data membr references.
#if DEBUG_MEMBER_FUNCTION_REF
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Explicitly setting isMemberFunctionMemberReference == false \n");
#endif
    // isMemberFunctionMemberReference = false;

#if DEBUG_MEMBER_FUNCTION_REF || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case of SgMemberFunctionRefExp: "
                "isMemberFunctionMemberReference = %s isAddressTaken = %s \n",
                isMemberFunctionMemberReference ? "true" : "false",
                isAddressTaken ? "true" : "false");
#endif
    if (isMemberFunctionMemberReference == true && isAddressTaken == true) {
#if DEBUG_MEMBER_FUNCTION_REF || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Detected case of name qualification required due to pointer "
                  "to member function reference \n");
#endif
      nameQualificationInducedFromPointerMemberType = true;
    } else {
      // bool isMemberFunctionMemberReference =
      // SageInterface::isMemberFunctionMemberReference(memberFunctionRefExp);
      if (isMemberFunctionMemberReference == true) {
#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(MLOG_UNPARSER,
                    "This is a member function member reference requiring name "
                    "qualification to the class where the data member "
                    "reference is referenced \n");
#endif
        ROSE_ASSERT(isAddressTaken == false);

        // DQ (2/17/2019): Debugging pointer to membr function (similar to
        // pointer to member data). bool isAddressTaken =
        // SageInterface::isAddressTaken(memberFunctionRefExp);
        // MLOG_WARN_C(MLOG_UNPARSER, "isAddressTaken = %s \n",isAddressTaken ?
        // "true" : "false");

        // std::list<SgClassType*> classChain =
        // SageInterface::getClassTypeChainForDataMemberReference(memberFunctionRefExp);
        std::list<SgClassType *> classChain =
            SageInterface::getClassTypeChainForMemberReference(
                memberFunctionRefExp);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgMemberFunctionRefExp: classChain.size() = %zu \n",
                    classChain.size());
        std::list<SgClassType *>::iterator classChain_iterator =
            classChain.begin();
        while (classChain_iterator != classChain.end()) {
          MLOG_WARN_C(MLOG_UNPARSER,
                      " --- *classChain_iterator = %p = %s name = %s \n",
                      *classChain_iterator,
                      (*classChain_iterator)->class_name().c_str(),
                      (*classChain_iterator)->get_name().str());

          classChain_iterator++;
        }
#endif
        // DQ (2/16/2019): We need to call something like this, but specialized
        // to just use the single class in the classChain.
        // setNameQualification(varRefExp,variableDeclaration,amountOfNameQualificationRequired);

        if (classChain.empty() == false) {
          std::list<SgClassType *>::iterator classChain_first =
              classChain.begin();
          std::string qualifier =
              std::string((*classChain_first)->get_name().str()) + "::";

#if DEBUG_MEMBER_FUNCTION_REF
          MLOG_WARN_C(MLOG_UNPARSER, "data member qualifier = %s \n",
                      qualifier.c_str());
#endif
          // DQ (2/16/2019): Mark this as at least non-zero, but it is computed
          // based on where the ambiguity is instead of as a length of the chain
          // of scope from the variable referenced's variable declaration scope.
          memberFunctionRefExp->set_name_qualification_length(1);

          memberFunctionRefExp->set_global_qualification_required(false);
          memberFunctionRefExp->set_type_elaboration_required(false);

          if (qualifiedNameMapForNames.find(memberFunctionRefExp) ==
              qualifiedNameMapForNames.end()) {
#if DEBUG_MEMBER_FUNCTION_REF
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Inserting qualifier for name = %s into list at IR "
                        "node = %p = %s \n",
                        qualifier.c_str(), memberFunctionRefExp,
                        memberFunctionRefExp->class_name().c_str());
#endif
            qualifiedNameMapForNames.insert(std::pair<SgNode *, std::string>(
                memberFunctionRefExp, qualifier));
          } else {
            // DQ (6/20/2011): We see this case in test2011_87.C.
            // If it already existes then overwrite the existing information.
            NameQualificationMapType::iterator i =
                qualifiedNameMapForNames.find(memberFunctionRefExp);
            ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if DEBUG_MEMBER_FUNCTION_REF
            string previousQualifier = i->second.c_str();
            MLOG_WARN_C(MLOG_UNPARSER,
                        "WARNING: test 0: replacing previousQualifier = %s "
                        "with new qualifier = %s \n",
                        previousQualifier.c_str(), qualifier.c_str());
#endif
            if (i->second != qualifier) {
              // DQ (7/23/2011): Multiple uses of the SgVarRefExp expression in
              // SgArrayType will cause the name qualification to be reset each
              // time.  This is OK since it is used to build the type name that
              // will be saved.
              i->second = qualifier;
            }
          }
        } else {
#if DEBUG_MEMBER_FUNCTION_REF
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "This has an empty class chain: classChain.size() = %zu \n",
              classChain.size());
#endif

          // DQ (6/1/2019): When the function called is from a base class and
          // conflicts with a member function in the derived class then we need
          // additional name qualification.

#if DEBUG_MEMBER_FUNCTION_REF
          MLOG_WARN_C(MLOG_UNPARSER,
                      " (isMemberFunctionMemberReference == true && "
                      "isAddressTaken == false) == true \n");
#endif
          // ROSE_ASSERT (isMemberFunctionMemberReference == true &&
          // isAddressTaken == false);

          SgStatement *currentStatement =
              SageInterface::getEnclosingStatement(memberFunctionRefExp);
          // ASSERT_not_null(currentStatement);
          if (currentStatement != NULL) {
            SgScopeStatement *currentScope = currentStatement->get_scope();
            ASSERT_not_null(currentScope);

            ASSERT_not_null(memberFunctionDeclaration);

            int amountOfNameQualificationRequired = nameQualificationDepth(
                memberFunctionDeclaration, currentScope, currentStatement);

#if DEBUG_MEMBER_FUNCTION_REF
            MLOG_WARN_C(MLOG_UNPARSER,
                        "***** case of SgMemberFunctionRefExp: DONE: Calling "
                        "nameQualificationDepth() ***** \n");
            MLOG_WARN_C(MLOG_UNPARSER,
                        "SgMemberFunctionCallExp's member function name: "
                        "amountOfNameQualificationRequired = %d \n",
                        amountOfNameQualificationRequired);
#endif
            ASSERT_not_null(memberFunctionRefExp);

            setNameQualification(memberFunctionRefExp,
                                 memberFunctionDeclaration,
                                 amountOfNameQualificationRequired);
          } else {
#if DEBUG_MEMBER_FUNCTION_REF || 0
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "case of SgMemberFunctionRefExp: currentStatement == NULL "
                "(could be a function call hidden in a decltype()) \n");
#endif
          }
        }
      } else {
        ROSE_ASSERT(isMemberFunctionMemberReference == false);
        if (isAddressTaken == true) {
        } else {
          ROSE_ASSERT(isAddressTaken == false);
        }
      }
    }

#if DEBUG_MEMBER_FUNCTION_REF
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Case of SgMemberFunctionRefExp: memberFunctionDeclaration = %p \n",
        memberFunctionDeclaration);
#endif

    if (memberFunctionDeclaration != NULL) {
      // DQ (2/17/2019): Adding support for pointers to member functions.
      // if (isMemberFunctionMemberReference == false)
      if (isMemberFunctionMemberReference == false || isAddressTaken == true) {

        // DQ (2/23/2019): Except that this code works in all cases that I can
        // see at the moment, I think that the current scope should be taken
        // from the type of the pointer being dereferenced instead of from the
        // location of the statement containing the memberFunctionRefExp.  But I
        // can't build a counter example that fails.

        SgStatement *currentStatement =
            SageInterface::getEnclosingStatement(memberFunctionRefExp);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Compute the currentStatement: currentStatement = %p \n",
                    currentStatement);
#endif
        if (currentStatement == NULL) {
          // DQ (8/19/2014): Because we know where this can happen we don't need
          // to always output debugging info. A better test might be to find the
          // type that embeds the expression and make sure it is a SgArrayType.
          // DQ (7/11/2014): test2014_83.C demonstrates how this can happen
          // because the SgMemberFunctionRefExp appears in an index expression
          // of an array type in a variable declaration.
          SgType *associatedType =
              SageInterface::getAssociatedType(memberFunctionRefExp);
          if (associatedType != NULL) {
            SgArrayType *arrayType = isSgArrayType(associatedType);
            if (arrayType == NULL) {
#if DEBUG_MEMBER_FUNCTION_REF
              MLOG_WARN_C(MLOG_UNPARSER,
                          "Warning: Location of where we can NOT associate the "
                          "expression to a SgArrayType \n");
              memberFunctionRefExp->get_file_info()->display(
                  "Error: currentStatement == NULL: memberFunctionRefExp: "
                  "debug");
              memberFunctionDeclaration->get_file_info()->display(
                  "Error: currentStatement == NULL: memberFunctionDeclaration: "
                  "debug");
#endif
            } else {
#if DEBUG_MEMBER_FUNCTION_REF
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "Note: Location of where we CAN associate the expression to "
                  "a statement: confirmed unassociated expression is buried in "
                  "a type: associatedType = %p = %s \n",
                  associatedType, associatedType->class_name().c_str());
#endif
            }
          } else {
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Error: Location of where we can NOT associate the "
                        "expression to a statement \n");
            memberFunctionRefExp->get_file_info()->display(
                "Error: currentStatement == NULL: memberFunctionRefExp: debug");
            memberFunctionDeclaration->get_file_info()->display(
                "Error: currentStatement == NULL: memberFunctionDeclaration: "
                "debug");
          }

          // DQ (7/11/2014): Added support for when this is a nested call and
          // the scope where the call is made from is essential.
          if (explictlySpecifiedCurrentScope != NULL) {
#if DEBUG_MEMBER_FUNCTION_REF
            MLOG_WARN_C(MLOG_UNPARSER,
                        "explictlySpecifiedCurrentScope = %p = %s \n",
                        explictlySpecifiedCurrentScope,
                        explictlySpecifiedCurrentScope->class_name().c_str());
#endif

            // DQ (4/19/2019): Now that we (optionally) also pass in the
            // explictlySpecifiedCurrentStatement, we might want to use it
            // directly.
            currentStatement = explictlySpecifiedCurrentScope;
          } else {
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Error: explictlySpecifiedCurrentScope == NULL \n");

            MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
            ROSE_ABORT();
          }
        }
        ASSERT_not_null(currentStatement);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "case of SgMemberFunctionRefExp: currentStatement = %p = %s \n",
            currentStatement, currentStatement->class_name().c_str());
#endif
        SgScopeStatement *currentScope = currentStatement->get_scope();
        ASSERT_not_null(currentScope);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case of SgMemberFunctionRefExp: currentScope = %p = %s \n",
                    currentScope, currentScope->class_name().c_str());
        MLOG_WARN_C(MLOG_UNPARSER, "***** case of SgMemberFunctionRefExp: "
                                   "Calling nameQualificationDepth() ***** \n");
#endif
        int amountOfNameQualificationRequired = nameQualificationDepth(
            memberFunctionDeclaration, currentScope, currentStatement);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(MLOG_UNPARSER,
                    "***** case of SgMemberFunctionRefExp: DONE: Calling "
                    "nameQualificationDepth() ***** \n");
        MLOG_WARN_C(MLOG_UNPARSER,
                    "SgMemberFunctionCallExp's member function name: "
                    "amountOfNameQualificationRequired = %d \n",
                    amountOfNameQualificationRequired);
#endif
        // DQ (2/7/2019): Add an extra level of name qualification if this is
        // pointer-to-member type induced.
        if (nameQualificationInducedFromPointerMemberType == true) {
          // DQ (2/8/2019): Only add name qualification if not present (else we
          // can get over qualification that can show up as pointer names in the
          // name qualification, see Cxx11_tests/test2019_86.C).
          if (amountOfNameQualificationRequired == 0) {
            amountOfNameQualificationRequired++;
          }
#if DEBUG_MEMBER_FUNCTION_REF
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Found case of name qualification required because the "
                      "variable is associated with SgPointerMemberType: "
                      "amountOfNameQualificationRequired = %d \n",
                      amountOfNameQualificationRequired);
#endif
        }

        setNameQualification(memberFunctionRefExp, memberFunctionDeclaration,
                             amountOfNameQualificationRequired);
        // DQ (2/17/2019): Case of xxx !(isDataMemberReference == true &&
        // isAddressTaken == true)
      } else {
      }
    } else {
#if DEBUG_MEMBER_FUNCTION_REF || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: memberFunctionDeclaration == NULL in "
                  "SgMemberFunctionCallExp for name qualification support! \n");
#endif
    }

    // If this is a templated function then we have to save the name because its
    // templated name might have template arguments that require name
    // qualification.
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDeclaration =
            isSgTemplateInstantiationMemberFunctionDecl(
                memberFunctionRefExp->getAssociatedMemberFunctionDeclaration());
    if (templateInstantiationMemberFunctionDeclaration != NULL) {
#if DEBUG_MEMBER_FUNCTION_REF
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Found a SgTemplateInstantiationMemberFunctionDecl that will have "
          "template arguments that might require qualification. name = %s \n",
          templateInstantiationMemberFunctionDeclaration->get_name().str());
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Must handle templated SgMemberFunctionRefExp! \n");
#endif

      // DQ (5/24/2013): Added support for member function template argument
      // lists to have similar handling, such as to
      // SgTemplateInstantiationFunctionDecl IR nodes.  This is required to
      // support test codes such as test2013_188.C.
      SgStatement *currentStatement =
          SageInterface::getEnclosingStatement(memberFunctionRefExp);

      // DQ (4/15/2019): This fails for legacy frontend 5.0 only, on
      // Cxx_tests/test2004_149.C (as a result of recent work Sunday
      // afternoon). ASSERT_not_null(currentStatement);
      if (currentStatement != NULL) {
        SgScopeStatement *currentScope = currentStatement->get_scope();
        ASSERT_not_null(currentScope);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "case of SgMemberFunctionRefExp: currentStatement = %p = %s \n",
            currentStatement, currentStatement->class_name().c_str());
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case of SgMemberFunctionRefExp: currentScope = %p = %s \n",
                    currentScope, currentScope->class_name().c_str());
        MLOG_WARN_C(MLOG_UNPARSER,
                    "***** calling traverseTemplatedMemberFunction() \n");
#endif

        // traverseTemplatedFunction(functionRefExp,templateInstantiationFunctionDeclaration,currentScope,currentStatement);
        // traverseTemplatedFunction(functionRefExp,functionRefExp,currentScope,currentStatement);
        traverseTemplatedMemberFunction(memberFunctionRefExp,
                                        memberFunctionRefExp, currentScope,
                                        currentStatement);

#if DEBUG_MEMBER_FUNCTION_REF
        MLOG_WARN_C(MLOG_UNPARSER,
                    "***** DONE: calling traverseTemplatedMemberFunction() \n");
#endif
      }
    }
  }

  // DQ (5/31/2011): This is a derived class from SgExpression and
  // SgInitializer...
  SgConstructorInitializer *constructorInitializer =
      isSgConstructorInitializer(n);
  if (constructorInitializer != NULL) {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        constructorInitializer->get_declaration();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Case of SgConstructorInitializer: memberFunctionDeclaration = %p \n",
        memberFunctionDeclaration);
#endif

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(constructorInitializer);

    if (currentStatement == NULL) {
      // DQ (1/28/2019): This can happen when the expression is used in an array
      // type declaration (e.g. within a variable declaration for an array).
      // NOTE: this will be possibly incorrect if there is a using declaration
      // in the scope that would be important to the name qualification. We
      // would then need to know if the declarration declaring the array type
      // was before or after the using declaration. Not clear what would be the
      // best way to solve that problem (though it would not be in the set of
      // directived already processed, so it might be fine).
      ASSERT_not_null(constructorInitializer->get_parent());

      SgScopeStatement *tmp_currentScope =
          inheritedAttribute.get_currentScope();
      ASSERT_not_null(tmp_currentScope);
      // If we don't have a statement derived from the expression to reference,
      // then use the first statement in the current scope.
      currentStatement = tmp_currentScope->firstStatement();
      ASSERT_not_null(currentStatement);
    }
    ASSERT_not_null(currentStatement);

    // If this could occur in a SgForStatement then this should be fixed up as
    // it is elsewhere...
    SgScopeStatement *currentScope = currentStatement->get_scope();
    ASSERT_not_null(currentScope);

    if (memberFunctionDeclaration != NULL) {
      int amountOfNameQualificationRequired = nameQualificationDepth(
          memberFunctionDeclaration, currentScope, currentStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgConstructorInitializer's constructor member function "
                  "name: amountOfNameQualificationRequired = %d \n",
                  amountOfNameQualificationRequired);
#endif
      setNameQualification(constructorInitializer, memberFunctionDeclaration,
                           amountOfNameQualificationRequired);
    } else {
      // DQ (6/1/2011): This happens when there is no explicit constructor that
      // can be used to build a class, in this case the class name must be used
      // to define a default constructor. This is a problem for test2004_130.C
      // (at line 165 col = 14 file under the build-tree compiler header
      // staging area, e.g., g++_HEADERS/hdrs3/bits/stl_iterator_base_types.h).
      // Need to investigate this later (it is strange that it is not an issue
      // in test2011_63.C, but it is a struct instead of a class and that might
      // be why).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "WARNING: memberFunctionDeclaration == NULL in "
          "SgConstructorInitializer for name qualification support! \n");
#endif
      // ROSE_ABORT();

      // DQ (6/4/2011): Added support for this case.
      SgClassDeclaration *classDeclaration =
          constructorInitializer->get_class_decl();
      // ASSERT_not_null(classDeclaration);
      if (classDeclaration != NULL) {
        SgDeclarationStatement *typeDeclaration =
            getDeclarationAssociatedWithType(
                constructorInitializer->get_type());
        SgDeclarationStatement *canonicalTypeDeclaration =
            typeDeclaration != NULL &&
                    typeDeclaration->get_firstNondefiningDeclaration() != NULL
                ? typeDeclaration->get_firstNondefiningDeclaration()
                : typeDeclaration;
        SgDeclarationStatement *canonicalClassDeclaration =
            classDeclaration->get_firstNondefiningDeclaration() != NULL
                ? classDeclaration->get_firstNondefiningDeclaration()
                : classDeclaration;
        bool type_already_spells_constructor_name =
            isSgNonrealType(constructorInitializer->get_type()) != NULL;

        // For constructor expressions that name the class type directly,
        // traverseType() already records the class qualification on the type
        // itself when the type node preserves the written qualified spelling.
        // Adding a separate constructor-name qualifier in that case duplicates
        // the prefix (e.g. `xxx::xxx::struct1`).
        if (canonicalTypeDeclaration == canonicalClassDeclaration &&
            type_already_spells_constructor_name) {
          constructorInitializer->set_global_qualification_required(false);
          constructorInitializer->set_name_qualification_length(0);
          constructorInitializer->set_type_elaboration_required(false);
          NameQualificationMapType::iterator qualifier_it =
              qualifiedNameMapForNames.find(constructorInitializer);
          if (qualifier_it == qualifiedNameMapForNames.end()) {
            qualifiedNameMapForNames.insert(
                std::pair<SgNode *, std::string>(constructorInitializer, ""));
          } else {
            qualifier_it->second.clear();
          }
          SgNode::get_globalQualifiedNameMapForNames()[constructorInitializer] =
              "";
        } else {
          // An example of the problem is test2005_42.C, where the class name is
          // used to generate the constructor initializer name.
          int amountOfNameQualificationRequired = nameQualificationDepth(
              classDeclaration, currentScope, currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "SgConstructorInitializer's constructor (class default "
              "constructor) name: amountOfNameQualificationRequired = %d \n",
              amountOfNameQualificationRequired);
#endif
          // This will attach the new type string to the classDeclaration.
          setNameQualification(constructorInitializer, classDeclaration,
                               amountOfNameQualificationRequired);
        }
      } else {
        // This is a strange error: see test2004_77.C
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "WARNING: In SgConstructorInitializer name qualification "
                    "support: neither memberFunctionDeclaration or "
                    "classDeclaration are valid pointers. \n");
#endif
      }
    }

    // After processing the name qualification for the class declaration, we
    // need to also process the reference to the type for any name qualification
    // on possible template arguments.
    ASSERT_not_null(constructorInitializer->get_type());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Calling traverseType() on constructorInitializer = %p class "
                "type = %p = %s \n",
                constructorInitializer, constructorInitializer->get_type(),
                constructorInitializer->get_type()->class_name().c_str());
#endif
    // DQ (8/19/2013): Added the call to associate the name qualified class name
    // with the constructorInitializer. DQ (8/19/2013): Traverse the type to set
    // any possible template arguments (or other subtypes?) that require name
    // qualification.
    traverseType(constructorInitializer->get_type(), constructorInitializer,
                 currentScope, currentStatement);

    if (SgClassDeclaration *classDeclaration =
            constructorInitializer->get_class_decl()) {
      SgDeclarationStatement *typeDeclaration =
          getDeclarationAssociatedWithType(constructorInitializer->get_type());
      SgDeclarationStatement *canonicalTypeDeclaration =
          typeDeclaration != NULL &&
                  typeDeclaration->get_firstNondefiningDeclaration() != NULL
              ? typeDeclaration->get_firstNondefiningDeclaration()
              : typeDeclaration;
      SgDeclarationStatement *canonicalClassDeclaration =
          classDeclaration->get_firstNondefiningDeclaration() != NULL
              ? classDeclaration->get_firstNondefiningDeclaration()
              : classDeclaration;
      bool type_already_spells_constructor_name =
          isSgNonrealType(constructorInitializer->get_type()) != NULL;
      if (canonicalTypeDeclaration == canonicalClassDeclaration &&
          type_already_spells_constructor_name) {
        constructorInitializer->set_global_qualification_required(false);
        constructorInitializer->set_name_qualification_length(0);
        constructorInitializer->set_type_elaboration_required(false);
        NameQualificationMapType::iterator qualifier_it =
            qualifiedNameMapForNames.find(constructorInitializer);
        if (qualifier_it == qualifiedNameMapForNames.end()) {
          qualifiedNameMapForNames.insert(
              std::pair<SgNode *, std::string>(constructorInitializer, ""));
        } else {
          qualifier_it->second.clear();
        }
        SgNode::get_globalQualifiedNameMapForNames()[constructorInitializer] =
            "";
      }
    }
  }

  // DQ (3/21/2018): This is a derived class from SgConstructorInitializer...
  SgAggregateInitializer *aggregateInitializer = isSgAggregateInitializer(n);
  if (aggregateInitializer != NULL) {
    // DQ (3/21/2018): Ignore the member function for the case of a
    // SgAggregateInitializer. SgMemberFunctionDeclaration*
    // memberFunctionDeclaration = aggregateInitializer->get_declaration();
    // SgMemberFunctionDeclaration* memberFunctionDeclaration = NULL;
    SgType *aggregateInitializerType = aggregateInitializer->get_type();
    ASSERT_not_null(aggregateInitializerType);
    SgClassType *aggregateInitializerClassType =
        isSgClassType(aggregateInitializerType);
    SgClassDeclaration *aggregateInitializerClassDeclaration = NULL;
    if (aggregateInitializerClassType != NULL) {
      aggregateInitializerClassDeclaration = isSgClassDeclaration(
          aggregateInitializerClassType->get_declaration());
      ASSERT_not_null(aggregateInitializerClassDeclaration);
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case of SgAggregateInitializer: "
                "aggregateInitializerClassDeclaration = %p \n",
                aggregateInitializerClassDeclaration);
#endif

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(aggregateInitializer);
    if (currentStatement == NULL) {
      // DQ (1/28/2019): This can happen when the expression is used in an array
      // type declaration (e.g. within a variable declaration for an array).
      // NOTE: this will be possibly incorrect if there is a using declaration
      // in the scope that would be important to the name qualification. We
      // would then need to know if the declarration declaring the array type
      // was before or after the using declaration. Not clear what would be the
      // best wayy to solve that problem (though it would not be in the set of
      // directived already processed, so it might be fine).
      ASSERT_not_null(aggregateInitializer->get_parent());

      SgScopeStatement *tmp_currentScope =
          inheritedAttribute.get_currentScope();
      ASSERT_not_null(tmp_currentScope);
      // If we don't have a statement derived from the expression to reference,
      // then use the first statement in the current scope.
      currentStatement = tmp_currentScope->firstStatement();
      ASSERT_not_null(currentStatement);
    }
    ASSERT_not_null(currentStatement);

    // If this could occur in a SgForStatement then this should be fixed up as
    // it is elsewhere...
    SgScopeStatement *currentScope = currentStatement->get_scope();
    ASSERT_not_null(currentScope);

    if (aggregateInitializerClassDeclaration != NULL) {
      int amountOfNameQualificationRequired = nameQualificationDepth(
          aggregateInitializerClassDeclaration, currentScope, currentStatement);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgAggregateInitializer's class declaration name: "
                  "amountOfNameQualificationRequired = %d \n",
                  amountOfNameQualificationRequired);
#endif
      setNameQualification(aggregateInitializer,
                           aggregateInitializerClassDeclaration,
                           amountOfNameQualificationRequired);

      // After processing the name qualification for the class declaration, we
      // need to also process the reference to the type for any name
      // qualification on possible template arguments.
      ASSERT_not_null(aggregateInitializer->get_type());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Calling traverseType() on aggregateInitializer = %p class "
                  "type = %p = %s \n",
                  aggregateInitializer, aggregateInitializer->get_type(),
                  aggregateInitializer->get_type()->class_name().c_str());
#endif
      // DQ (8/19/2013): Added the call to associate the name qualified class
      // name with the constructorInitializer. DQ (8/19/2013): Traverse the type
      // to set any possible template arguments (or other subtypes?) that
      // require name qualification.
      traverseType(aggregateInitializer->get_type(), aggregateInitializer,
                   currentScope, currentStatement);
    } else {
      // This is likely the case of a SgAggregateInitializer used for an array
      // (not associated with a class declaration).
    }
  }

  SgVarRefExp *varRefExp = isSgVarRefExp(n);
  if (varRefExp != NULL) {
    // We need to store the information about the required name qualification in
    // the SgVarRefExp IR node.

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(varRefExp);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Case of SgVarRefExp: varRefExp = %p currentStatement = %p = %s \n",
        varRefExp, currentStatement,
        currentStatement != NULL ? currentStatement->class_name().c_str()
                                 : "null");
#endif

    // DQ (2/7/2019): Adding support for name qualification induced from
    // SgPointerMemberType function paramters.
    bool nameQualificationInducedFromPointerMemberType = false;

    // DQ (2/8/2019): And then I woke up in the morning and had a better idea.
    // DQ (2/8/2019): An alternative to supporting pointer-to-member name
    // qualification would be to detect member data accessed via a pointer. so
    // we need to look at the parent of a SgVarRefExp and see if it is a
    // SgAddressOfOp when it is a reference to a data member.
    bool isDataMemberReference =
        SageInterface::isDataMemberReference(varRefExp);
    bool isAddressTaken = SageInterface::isAddressTaken(varRefExp);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case of SgVarRefExp: isDataMemberReference = %s "
                "isAddressTaken = %s \n",
                isDataMemberReference ? "true" : "false",
                isAddressTaken ? "true" : "false");
#endif
    bool isAddressOfCurrentObjectDataMemberReference =
        SageInterface::isAddressOfCurrentObjectDataMemberReference(varRefExp);
    bool nameQualificationInducedByPointerToMember =
        isDataMemberReference == true && isAddressTaken == true &&
        isAddressOfCurrentObjectDataMemberReference == false;

    if (nameQualificationInducedByPointerToMember == true) {
      nameQualificationInducedFromPointerMemberType = true;
    } else {
      // DQ (2/15/2019): Debugging Cxx11_tests/test2019_129.C.  Data member
      // references should have a current statement that starts in the class XXX
      // (instead of where XXX.yyy is located. If this is a data member
      // reference, then we need to change the perspective to a currentStatement
      // of that of the class where it is a data member reference.  So XXX.yyy
      // would have yyy be a data member of XXX and so the current statement
      // would be the scope represent by XXX.

      // DQ (2/15/2019): Unfortunately data member name qualification happens
      // top down, instaed of bottom up like all other name qualification.  So
      // this is going to complicate things.

      // Need to change the name of this function to be more specific.
      if (isDataMemberReference == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Change the starting location for name qualification to the class "
            "where the data member reference is referenced \n");
#endif
        ROSE_ASSERT(isAddressTaken == false ||
                    isAddressOfCurrentObjectDataMemberReference == true);
        // reset the current statement.

        // Insead of returning the SgClassType at the end of the chain, we
        // actaully need to generate the chain of SgClassType the reflects the
        // path taken (represented by the chain of SgCastExp expressions).

        // Then for each element of the chain, we need to lookup the symbol (a
        // in "X x; x.a;") in the scope representing each class, and see if the
        // number of causal nodes is more than one. The name qualification
        // length is the longest chain between scopes where two of the
        // associated SgAliasSymbols have 2 or more causal nodes.

        // DQ (2/16/2019): We need to look for both variable and base class
        // ambiguity.  I don't think we need base class ambiguity, since that is
        // not allowed in the langauge.

        // Second generation of this function.
        // std::list<SgClassType*> classChain =
        // SageInterface::getClassTypeChainForDataMemberReference(varRefExp);
        std::list<SgClassType *> classChain =
            SageInterface::getClassTypeChainForMemberReference(varRefExp);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgVarRefExp: classChain.size() = %zu \n",
                    classChain.size());
        std::list<SgClassType *>::iterator classChain_iterator =
            classChain.begin();
        while (classChain_iterator != classChain.end()) {
          MLOG_WARN_C(MLOG_UNPARSER,
                      " --- *classChain_iterator = %p = %s name = %s \n",
                      *classChain_iterator,
                      (*classChain_iterator)->class_name().c_str(),
                      (*classChain_iterator)->get_name().str());

          classChain_iterator++;
        }
#endif
        // DQ (11/10/2020): This assertion failed (classChain.size() <= 2) where
        // the value was 3 for some of the target code for codeSegregation. DQ
        // (11/8/2020): I now think that Cxx11_tests/test_2019_120.C could be
        // used to make this arbitrarily long. DQ (12/11/2019): Modified to
        // provide a larger upper bound for classChain.size(). DQ (2/16/2019): I
        // think this is always true, since base class abiguity is not allowed
        // in the C++ language. ROSE_ASSERT(classChain.size() == 1);
        // ROSE_ASSERT(classChain.empty() == true || classChain.size() == 1);
        // ROSE_ASSERT(classChain.empty() == true || classChain.size() <= 2);
        if (classChain.size() > 3) {
          printf("In name qualification: Case of SgVarRefExp: "
                 "classChain.size() > 3: classChain.size() = %zu \n",
                 classChain.size());
        }

        // DQ (2/16/2019): We need to call something like this, but specialized
        // to just use the single class in the classChain.
        // setNameQualification(varRefExp,variableDeclaration,amountOfNameQualificationRequired);

        if (classChain.empty() == false) {
          // DQ (1/19/2020): Might need to recursively call the name
          // qualification on the classChain_first since it can be a class that
          // required name qualificaiton to resolve an ambiguity.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          std::list<SgClassType *>::iterator classChain_first =
              classChain.begin();
          MLOG_WARN_C(MLOG_UNPARSER,
                      "(*classChain_first)->get_name().str() = %s \n",
                      (*classChain_first)->get_name().str());
#endif
          // This is much more complex code, but it satisfies all of the test
          // codes including the ones for codeSegragation when the the symbol
          // table for the global scope across file is cleared. DQ (11/8/2020):
          // Potential bug fix for name qualification error that only happens in
          // transformations (e.g. codeSegregation and outlining). Note that it
          // appears as an issue to fix only when the buildSourceFile() function
          // is used and the symbol table associated with the global scope
          // across files is cleared (recently implementd in
          // SageInterface::buildFile() (called by
          // SageInterface::getSourceFile()).
          std::list<SgClassType *>::iterator classChain_last;
          std::list<SgClassType *>::iterator classChain_target =
              classChain.begin();
          std::list<SgClassType *>::iterator i = classChain.begin();

          bool useNextClass = false;
          // Note that the start of the chain may not be the most apropriate
          // class to use. This is demonstrated by Cxx_tests/test2019_130.C and
          // the codeSegragation tool test_93.cpp.
          while (i != classChain.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
            MLOG_WARN_C(MLOG_UNPARSER, " --- (*i)->get_name().str() = %s \n",
                        (*i)->get_name().str());
#endif
            // Review the length of the causal nodes for the alias symbol.  If
            // is is one then no name qualification is needed, if it is more
            // than one then name qualification is required to disambiguate the
            // member access.
            SgClassType *classType = *i;
            ROSE_ASSERT(classType != NULL);

            SgClassDeclaration *classDeclaration =
                isSgClassDeclaration(classType->get_declaration());
            ROSE_ASSERT(classDeclaration != NULL);
            SgClassDeclaration *definingClassDeclaration = isSgClassDeclaration(
                classDeclaration->get_definingDeclaration());
            if (definingClassDeclaration != NULL) {
              SgSymbol *varRefExp_symbol = varRefExp->get_symbol();
              ROSE_ASSERT(varRefExp_symbol != NULL);

              SgName varRefExp_name = varRefExp_symbol->get_name();
              SgClassDefinition *classDefinition =
                  definingClassDeclaration->get_definition();
              ROSE_ASSERT(classDefinition != NULL);

              size_t number_of_alias_symbols =
                  classDefinition->count_alias_symbol(varRefExp_name);
              if (number_of_alias_symbols > 0) {
                SgAliasSymbol *aliasSymbol =
                    classDefinition->lookup_alias_symbol(varRefExp_name,
                                                         varRefExp_symbol);
                if (aliasSymbol != NULL) {
                  if (aliasSymbol->get_causal_nodes().size() == 1) {
                    // Reset to where there will be no ambiguity.
                    classChain_target = i;
                  } else {
                    if (aliasSymbol->get_causal_nodes().size() > 1) {
                      // Use the next element in the chain.
                      useNextClass = true;
                    }
                  }
                }
              } else {
                // Reset to where there will be no ambiguity.
                if (useNextClass == true) {
                  classChain_target = i;
                  useNextClass = false;
                }
              }
            }

            // qualifier += std::string((*i)->get_name().str()) + "::";
            classChain_last = i;
            i++;
          }
          // std::string qualifier =
          // std::string((*classChain_last)->get_name().str()) + "::";
          std::string qualifier =
              std::string((*classChain_target)->get_name().str()) + "::";

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER, "data member qualifier = %s \n",
                      qualifier.c_str());
#endif
          // DQ (2/16/2019): Mark this as at least non-zero, but it is computed
          // based on where the ambiguity is instead of as a length of the chain
          // of scope from the variable referenced's variable declaration scope.
          varRefExp->set_name_qualification_length(1);

          varRefExp->set_global_qualification_required(false);
          varRefExp->set_type_elaboration_required(false);

          if (qualifiedNameMapForNames.find(varRefExp) ==
              qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Inserting qualifier for name = %s into list at IR "
                        "node = %p = %s \n",
                        qualifier.c_str(), varRefExp,
                        varRefExp->class_name().c_str());
#endif
            qualifiedNameMapForNames.insert(
                std::pair<SgNode *, std::string>(varRefExp, qualifier));
          } else {
            // DQ (6/20/2011): We see this case in test2011_87.C.
            // If it already existes then overwrite the existing information.
            NameQualificationMapType::iterator i =
                qualifiedNameMapForNames.find(varRefExp);
            ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            string previousQualifier = i->second.c_str();
            MLOG_WARN_C(MLOG_UNPARSER,
                        "WARNING: test 1: replacing previousQualifier = %s "
                        "with new qualifier = %s \n",
                        previousQualifier.c_str(), qualifier.c_str());
#endif
            if (i->second != qualifier) {
              // DQ (7/23/2011): Multiple uses of the SgVarRefExp expression in
              // SgArrayType will cause the name qualification to be reset each
              // time.  This is OK since it is used to build the type name that
              // will be saved.
              i->second = qualifier;
            }
          }
        }
      }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Case of SgVarRefExp: "
                "nameQualificationInducedFromPointerMemberType = %s \n",
                nameQualificationInducedFromPointerMemberType ? "true"
                                                              : "false");
    MLOG_WARN_C(MLOG_UNPARSER,
                " --- isDataMemberReference = %s isAddressTaken = %s "
                "isAddressOfCurrentObjectDataMemberReference = %s \n",
                isDataMemberReference ? "true" : "false",
                isAddressTaken ? "true" : "false",
                isAddressOfCurrentObjectDataMemberReference ? "true" : "false");
    MLOG_WARN_C(MLOG_UNPARSER, " --- currentStatement = %p \n",
                currentStatement);
#endif

    if (isDataMemberReference == false ||
        nameQualificationInducedByPointerToMember == true) {
      // DQ (7/24/2020): Is this declared above this point?
      // variableDeclaration = NULL;

      // DQ (6/23/2011): This test fails for the new name qualification after a
      // transformation in
      // tests/nonsmoke/functional/roseTests/programTransformationTests/test1.C
      // ASSERT_not_null(currentStatement);
      if (currentStatement != NULL) {
        // DQ (5/30/2011): Handle the case of test2011_58.C (index declaration
        // in for loop construct). SgScopeStatement* currentScope =
        // currentStatement->get_scope();
        SgScopeStatement *currentScope = isSgScopeStatement(currentStatement);
        if (currentScope == NULL) {
          currentScope = currentStatement->get_scope();
        }
        ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Case SgVarRefExp: (could this be in an array type?) "
                    "currentScope = %p = %s \n",
                    currentScope, currentScope->class_name().c_str());
#endif
        SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
        ASSERT_not_null(variableSymbol);
        SgInitializedName *initializedName = variableSymbol->get_declaration();
        ASSERT_not_null(initializedName);

        // DQ (7/18/2012): Added test as part of debugging test2011_75.C.
        ASSERT_not_null(initializedName->get_parent());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Case of SgVarRefExp: varRefExp = %p : initializedName "
                    "name = %s parent = %p = %s \n",
                    varRefExp, initializedName->get_name().str(),
                    initializedName->get_parent(),
                    initializedName->get_parent()->class_name().c_str());
#endif
        // DQ (7/24/3030): This variable declaration hides an outer declaration
        // using the same variable name.
        SgVariableDeclaration *variableDeclaration =
            isSgVariableDeclaration(initializedName->get_parent());
        // ASSERT_not_null(variableDeclaration);

        // DQ (7/24/2020): Debugging Cxx20_tests/test2020_122.C and
        // Cxx_tests/test2020_14.C. bool inDesignatedInitializer = false;
        if ((variableDeclaration != NULL) &&
            (variableDeclaration !=
             variableDeclaration->get_definingDeclaration())) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 0
          printf("SgVarRefExp is not in a defining SgVariableDeclaration: "
                 "might be in a SgDesignatedInitializer \n");
#endif
          SgExprListExp *exprListExp = isSgExprListExp(varRefExp->get_parent());
          if (exprListExp != NULL) {
            SgDesignatedInitializer *designatedInitializer =
                isSgDesignatedInitializer(exprListExp->get_parent());
            if (designatedInitializer != NULL) {
              // inDesignatedInitializer = true;
              // This is not a SgInitializedName being declared by a variable
              // declaration. Setting this to NULL will force the TRUE case
              // below to be taken, which after the check for some builtin
              // predefined names, will cause not name qualification to be
              // added.
              variableDeclaration = NULL;
            }
          }
        }
        if (variableDeclaration == NULL) {
          // This is the special case for the compiler generated variable
          // "__PRETTY_FUNCTION__".
          if (initializedName->get_name() == "__PRETTY_FUNCTION__" ||
              initializedName->get_name() == "__func__") {
            // Skip these cases ... no name qualification is required.
          } else {
            // If this is a SgInitializedName from a function parameter list
            // then it does not need qualification.
            SgFunctionParameterList *functionParameterList =
                isSgFunctionParameterList(initializedName->get_parent());
            if (functionParameterList != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "Names from function parameter list can not be name "
                          "qualified (because they are the initial "
                          "declarations): name = %s \n",
                          initializedName->get_name().str());
#endif
            } else {

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "varRefExp's initialized name = %s is not associated "
                          "with a SgVariableDeclaration \n",
                          initializedName->get_name().str());
              initializedName->get_file_info()->display(
                  "This SgInitializedName is not associated with a "
                  "SgVariableDeclaration");

              SgStatement *currentStatement =
                  SageInterface::getEnclosingStatement(
                      initializedName->get_parent());
              ASSERT_not_null(currentStatement);

              SgScopeStatement *targetScope = initializedName->get_scope();
              MLOG_WARN_C(MLOG_UNPARSER, "targetScope = %p = %s \n",
                          targetScope, targetScope->class_name().c_str());

              MLOG_WARN_C(MLOG_UNPARSER,
                          "SgVarRefExp case (no associated "
                          "variableDeclaration): currentStatement = %p = %s \n",
                          currentStatement,
                          currentStatement->class_name().c_str());

              MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
#endif
              // DQ (7/18/2012): Uncommented to debug test2011_75.C, not fixed,
              // but test2005_103.C fails and so this should be commented again
              // (I think). ROSE_ABORT();
            }
          }
        } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "In case SgVarRefExp: (variableDeclaration != NULL) "
                      "Calling nameQualificationDepth() \n");
#endif
          // DQ (12/21/2015): When this is a data member of a class/struct then
          // we are consistatnly overqualifying the SgVarRefExp because we are
          // not considering the case of a variable of type class that is being
          // used with the SgArrowExp or SgDotExp which would not require the
          // name qualification.  The only case where we would still need the
          // name qualification is the relatively rare case of multiple
          // inheritance (which must be detected separately).

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER, "variableDeclaration = %p \n",
                      variableDeclaration);
          MLOG_WARN_C(MLOG_UNPARSER, "currentScope        = %p = %s \n",
                      currentScope, currentScope->class_name().c_str());
          MLOG_WARN_C(MLOG_UNPARSER, "currentStatement    = %p = %s \n",
                      currentStatement, currentStatement->class_name().c_str());
          variableDeclaration->get_file_info()->display(
              "Before nameQualificationDepth()");
#endif
          int amountOfNameQualificationRequired = nameQualificationDepth(
              variableDeclaration, currentScope, currentStatement);

          // DQ (2/7/2019): Add an extra level of name qualification if this is
          // pointer-to-member type induced.
          if (nameQualificationInducedFromPointerMemberType == true) {
            // DQ (2/8/2019): Only add name qualification if not present (else
            // we can get over qualification that can show up as pointer names
            // in the name qualification, see Cxx11_tests/test2019_86.C).
            if (amountOfNameQualificationRequired == 0) {
              // DQ (3/30/2019): Experiment with commenting this out!
              amountOfNameQualificationRequired++;
            }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Found case of name qualification required because the "
                        "variable is associated with SgPointerMemberType: "
                        "amountOfNameQualificationRequired = %d \n",
                        amountOfNameQualificationRequired);
#endif
          }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgVarRefExp's SgDeclarationStatement: "
                      "amountOfNameQualificationRequired = %d \n",
                      amountOfNameQualificationRequired);
#endif
          setNameQualification(varRefExp, variableDeclaration,
                               amountOfNameQualificationRequired);

          // DQ (12/23/2015): If there are multiple symbols with the same name
          // then we require the name qualification. See test2015_140.C for an
          // example.
          SgName name = initializedName->get_name().str();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgVarRefExp's SgDeclarationStatement: "
                      "initializedName->get_name() = %s \n",
                      name.str());
#endif

          // size_t numberOfAliasSymbols =
          // currentScope->count_alias_symbol(name);
          int numberOfAliasSymbols = currentScope->count_alias_symbol(name);
          // if (numberOfAliasSymbols > 1)

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgVarRefExp's SgDeclarationStatement: "
                      "numberOfAliasSymbols              = %d \n",
                      numberOfAliasSymbols);
          MLOG_WARN_C(MLOG_UNPARSER,
                      "SgVarRefExp's SgDeclarationStatement: "
                      "amountOfNameQualificationRequired = %d \n",
                      amountOfNameQualificationRequired);
#endif

          if (numberOfAliasSymbols > 1 &&
              amountOfNameQualificationRequired == 0) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "WARNING: name qualification can be required when "
                        "there are multiple base classes with the same "
                        "referenced variable via SgAliasSymbol \n");
#endif
          } else {
            // DQ (12/23/2015): Note that this is not a count of the
            // SgVariableSymbol IR nodes. size_t numberOfSymbolsWithSameName =
            // currentScope->count_symbol(name);
            int numberOfSymbolsWithSameName =
                (int)currentScope->count_symbol(name);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "SgVarRefExp's SgDeclarationStatement: "
                        "numberOfSymbolsWithSameName       = %d \n",
                        numberOfSymbolsWithSameName);
#endif

            // if (numberOfSymbolsWithSameName > 1)
            // if (numberOfSymbolsWithSameName > 1 &&
            // amountOfNameQualificationRequired == 0)
            if ((numberOfSymbolsWithSameName - numberOfAliasSymbols) > 1 &&
                amountOfNameQualificationRequired == 0) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "WARNING: name qualification can be required when "
                          "there are multiple base classes with the same "
                          "referenced variable via SgVariableSymbol \n");
#endif
            }
            // ROSE_ASSERT(numberOfSymbolsWithSameName < 2);
            // if (numberOfSymbolsWithSameName >= 2 &&
            // amountOfNameQualificationRequired == 0)
            if ((numberOfSymbolsWithSameName - numberOfAliasSymbols) > 1 &&
                amountOfNameQualificationRequired == 0) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- numberOfSymbolsWithSameName       = %d \n",
                          numberOfSymbolsWithSameName);
              MLOG_WARN_C(MLOG_UNPARSER,
                          "   --- amountOfNameQualificationRequired = %d \n",
                          amountOfNameQualificationRequired);
#endif
            }
            // ROSE_ASSERT(numberOfSymbolsWithSameName < 2 ||
            // amountOfNameQualificationRequired > 0);

            // DQ (12/23/2015): This fails for the LULESH-OMP tests, I think I
            // need to relax this (it is a new assection). It might be that we
            // need to also check that these are all SgVariableSymbol (since
            // there could be different kinds of SgSymbol (which would have to
            // be allowed)). ROSE_ASSERT((numberOfSymbolsWithSameName -
            // numberOfAliasSymbols) <= 1 || amountOfNameQualificationRequired >
            // 0);
          }
          // ROSE_ASSERT(numberOfAliasSymbols < 2);
          ROSE_ASSERT(numberOfAliasSymbols <= 1 ||
                      amountOfNameQualificationRequired > 0);
        }

        // End of new test...
      } else {
        // DQ (7/23/2011): This case happens when the SgVarRefExp can not be
        // associated with a statement. I think this only happens when a
        // constant variable is used in an array index of an array type. DQ
        // (7/24/2011): This fails for the
        // tests/nonsmoke/functional/CompileTests/OpenMP_tests/objectLastprivate.cpp
        // test code. ASSERT_not_null(explictlySpecifiedCurrentScope);
        if (explictlySpecifiedCurrentScope != NULL) {
          // DQ (4/19/2019): Now that we (optionally) also pass in the
          // explictlySpecifiedCurrentStatement, we might want to use it
          // directly.
          currentStatement = explictlySpecifiedCurrentScope;

          SgVariableSymbol *variableSymbol = varRefExp->get_symbol();
          ASSERT_not_null(variableSymbol);

          SgInitializedName *initializedName =
              variableSymbol->get_declaration();
          ASSERT_not_null(initializedName);

          SgNode *parent = initializedName->get_parent();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "In case SgVarRefExp: (currentStatement == NULL) Calling "
                      "nameQualificationDepth() variableDeclaration = %p "
                      "initializedName->get_parent() = %p = %s \n",
                      variableDeclaration, parent,
                      parent ? parent->class_name().c_str() : "");
#endif

          SgVariableDeclaration *variableDeclaration =
              isSgVariableDeclaration(parent);
          SgDeclarationScope *decl_scope = isSgDeclarationScope(parent);
          if (variableDeclaration != NULL) {
            int amountOfNameQualificationRequired = nameQualificationDepth(
                variableDeclaration, explictlySpecifiedCurrentScope,
                currentStatement);
            setNameQualification(varRefExp, variableDeclaration,
                                 amountOfNameQualificationRequired);
          } else if (decl_scope != NULL) {
            // NOP that is a nontype template parameter
          } else {
            SgFunctionParameterList *functionParameterList =
                isSgFunctionParameterList(parent);
            SgTemplateClassDefinition *tpldef =
                isSgTemplateClassDefinition(parent);
            SgTemplateParameter *tplParam = isSgTemplateParameter(parent);
            SgTemplateInstantiationDefn *templateInstantiationDefn =
                isSgTemplateInstantiationDefn(parent);
            SgEnumDeclaration *enumDecl = isSgEnumDeclaration(parent);

            int amountOfNameQualificationRequired = nameQualificationDepth(
                initializedName, explictlySpecifiedCurrentScope,
                currentStatement);
            if (functionParameterList != NULL) {
              setNameQualification(varRefExp, functionParameterList,
                                   amountOfNameQualificationRequired);
            } else if (tpldef != NULL) {
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "WARNING: In "
                  "NameQualificationTraversal::evaluateInheritedAttribute: "
                  "Found SgInitializedName whose parent is a template class "
                  "definition. It does not sound right!!!\n");
              ASSERT_not_null(tpldef->get_parent());
              SgDeclarationStatement *tpldecl =
                  isSgDeclarationStatement(tpldef->get_parent());
              ASSERT_not_null(tpldecl);
              ROSE_ASSERT(isSgTemplateClassDeclaration(tpldecl));
              setNameQualification(varRefExp, tpldecl,
                                   amountOfNameQualificationRequired);
            } else if (templateInstantiationDefn != NULL) {
              setNameQualification(varRefExp,
                                   templateInstantiationDefn->get_declaration(),
                                   amountOfNameQualificationRequired);
            } else if (tplParam != NULL) {
              ASSERT_not_null(tplParam->get_parent());
              SgDeclarationStatement *tpldecl =
                  isSgDeclarationStatement(tplParam->get_parent());
              ASSERT_not_null(tpldecl);
              ROSE_ASSERT(isSgTemplateFunctionDeclaration(tpldecl) ||
                          isSgTemplateMemberFunctionDeclaration(tpldecl) ||
                          isSgTemplateClassDeclaration(tpldecl) ||
                          isSgTemplateTypedefDeclaration(tpldecl) ||
                          isSgTemplateVariableDeclaration(tpldecl) ||
                          isSgNonrealDecl(tpldecl));
              setNameQualification(varRefExp, tpldecl,
                                   amountOfNameQualificationRequired);
            } else if (enumDecl != NULL) {
              setNameQualification(varRefExp, enumDecl,
                                   amountOfNameQualificationRequired);
              //                             setNameQualification(varRefExp,
              //                             isSgScopeStatement(enumDecl->get_parent()),
              //                             amountOfNameQualificationRequired);
            } else {
              // MLOG_WARN_C(MLOG_UNPARSER, "ERROR: Unexpected parent for
              // SgInitializedName: parent = %p (%s)\n", parent, parent ?
              // parent->class_name().c_str() : "");
              printf("ERROR: Unexpected parent for SgInitializedName: parent = "
                     "%p (%s)\n",
                     parent, parent ? parent->class_name().c_str() : "");
              ROSE_ABORT();
            }
          }
        } else if (variableDeclaration != NULL) {
          int amountOfNameQualificationRequired = nameQualificationDepth(
              variableDeclaration, explictlySpecifiedCurrentScope,
              currentStatement);
          setNameQualification(varRefExp, variableDeclaration,
                               amountOfNameQualificationRequired);
        } else {
          // TV (09/13/2018): in ROSE/tutorial/: ./loopOptimization  -w -bk1
          // -fs0 -c
          // /data1/roseenv/src/tmp-merge/tutorial/inputCode_LoopOptimization_blocking.C
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "WARNING: Unexpected conditions in "
              "NameQualificationTraversal::evaluateInheritedAttribute.\n");
          //                       ROSE_ABORT();
        }
      }

      // DQ (2/16/2019): End of false branch for: if (isDataMemberReference ==
      // false)
    } else {
      // DQ (1/14/2020): To support Cxx11_tests/test2020_50.C, a variable in an
      // un-named union we could just argue that if the union is un-named then
      // the variable declaration should not be considered to be a member (then
      // the name qualification could proceed using the branch above).
    }
  }

  // DQ (6/9/2011): Added support for test2011_79.C (enum values can require
  // name qualification).
  SgEnumVal *enumVal = isSgEnumVal(n);
  if (enumVal != NULL) {
    SgScopeStatement *currentScope = NULL;

    SgEnumDeclaration *enumDeclaration = enumVal->get_declaration();
    ASSERT_not_null(enumDeclaration);

    // DQ (7/9/2019): Original code which addresses requirements for name
    // qualification based on visability, but not ambiguity.

    SgStatement *currentStatement =
        SageInterface::getEnclosingStatement(enumVal);
    // ASSERT_not_null(currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "case of SgEnumVal: currentStatement = %p \n",
                currentStatement);
#endif

    if (currentStatement != NULL) {
      currentScope = isSgScopeStatement(currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgEnumVal: currentStatement = %p = %s currentScope "
                  "= %p = %s \n",
                  currentStatement, currentStatement->class_name().c_str(),
                  currentScope,
                  currentScope != NULL ? currentScope->class_name().c_str()
                                       : "NULL");
#endif
      // If the current statement was not a scope, then what scope contains the
      // current statement.
      if (currentScope == NULL) {
        // DQ (5/24/2013): This is a better way to set the scope (see
        // test2013_187.C). currentScope = currentStatement->get_scope();
        ASSERT_not_null(inheritedAttribute.get_currentScope());
        currentScope = inheritedAttribute.get_currentScope();
      }
      ASSERT_not_null(currentScope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgEnumVal (after setting currentScope): "
                  "currentStatement = %p = %s currentScope = %p = %s \n",
                  currentStatement, currentStatement->class_name().c_str(),
                  currentScope,
                  currentScope != NULL ? currentScope->class_name().c_str()
                                       : "NULL");
#endif
      ASSERT_not_null(inheritedAttribute.get_currentScope());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgEnumVal : inheritedAttribute.get_currentScope() = "
                  "%p = %s \n",
                  inheritedAttribute.get_currentScope(),
                  inheritedAttribute.get_currentScope()->class_name().c_str());
#endif
    } else {
      // If the enum value is contained in an index expression then
      // currentStatement will be NULL. But then the current scope should be
      // known explicitly.
      currentScope = explictlySpecifiedCurrentScope;

      // DQ (9/17/2011); Added escape for where the currentScope == NULL (fails
      // for STL code when the original expression trees are used to eliminate
      // the constant folded values). ASSERT_not_null(currentScope);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      // DQ (4/19/2019): Now that we (optionally) also pass in the
      // explictlySpecifiedCurrentStatement, we might want to use it directly.
      MLOG_WARN_C(MLOG_UNPARSER,
                  "case of SgEnumVal: Using explictlySpecifiedCurrentScope for "
                  "the value of currentStatement: need to check this! \n");
#endif
      // Use the currentScope as the currentStatement
      currentStatement = currentScope;
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "currentStatement = %p \n", currentStatement);
    if (currentStatement != NULL) {
      MLOG_WARN_C(MLOG_UNPARSER, "currentStatement = %p = %s = %s \n",
                  currentStatement, currentStatement->class_name().c_str(),
                  SageInterface::get_name(currentStatement).c_str());
    }

    MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p \n", currentScope);
    if (currentScope != NULL) {
      MLOG_WARN_C(MLOG_UNPARSER, "currentScope = %p = %s = %s \n", currentScope,
                  currentScope->class_name().c_str(),
                  SageInterface::get_name(currentScope).c_str());
    }
#endif

    // DQ (9/17/2011); Added escape for where the currentScope == NULL (fails
    // for STL code when the original expression trees are used to eliminate the
    // constant folded values). ASSERT_not_null(currentScope);
    if (currentScope != NULL) {
      // DQ (9/17/2011): this is the original case we waant to restore later...
      ASSERT_not_null(currentScope);

      // We need to look up the qualification for the enum name and not the enum
      // declaration (which may have a different name (or no name).

      // DQ (7/8/2019): Ideally this would form an iteration over the scopes
      // from the current scope through the scopes connect via the base class.

      // DQ (7/8/2019): Added varialbe to store the contribution to name
      // qualification from ambiguity, as uposed to visability.
      int additionalNameQualificationToResolveAmbiguity = 0;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "######################################################## \n");
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Testing the EnumVal name instead of the Enum declaration \n");
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "######################################################## \n");
#endif
      SgName enumVal_name = enumVal->get_name();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "enumVal_name = %s \n", enumVal_name.str());
#endif

      {
        // If there was no symbol, then there was no ambiguity to force the name
        // qualification.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "If there was no symbol, then there was no ambiguity to force the "
            "name qualification in the current scope directly \n");
#endif
        // DQ (8/16/2013): Build the template parameters and template arguments
        // as appropriate (will be NULL pointers for some types of
        // declarations).
        SgTemplateParameterPtrList *templateParameterList =
            NULL; // SageBuilder::getTemplateParameterList(declaration);
        SgTemplateArgumentPtrList *templateArgumentList =
            NULL; // SageBuilder::getTemplateArgumentList(declaration);

        SgEnumDeclaration *enumDeclaration = enumVal->get_declaration();
        ASSERT_not_null(enumDeclaration);
        SgScopeStatement *enumDeclarationScope = enumDeclaration->get_scope();
        ASSERT_not_null(enumDeclarationScope);

        SgSymbol *symbolFromEnumDeclarationScope =
            enumDeclarationScope->lookup_enum_field_symbol(enumVal_name);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        if (symbolFromEnumDeclarationScope == NULL) {
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "name qualification: case of SgEnumVal: Found case of "
              "enumVal_name = %s not in scope of enumDeclaration = %p = %s \n",
              enumVal_name.str(), enumDeclaration,
              enumDeclaration->class_name().c_str());
          MLOG_WARN_C(MLOG_UNPARSER,
                      " --- symbolFromEnumDeclarationScope == NULL \n");
        }
#endif
        // ASSERT_not_null(symbolFromEnumDeclarationScope);

        SgSymbol *symbolFromParents = SageInterface::lookupSymbolInParentScopes(
            enumVal_name, currentScope, templateParameterList,
            templateArgumentList);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "symbolFromParents = %p \n",
                    symbolFromParents);
#endif
        if (symbolFromParents != NULL &&
            symbolFromEnumDeclarationScope != NULL &&
            symbolFromParents != symbolFromEnumDeclarationScope) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Found a reason for adding name qualification \n");
#endif
          additionalNameQualificationToResolveAmbiguity++;
        }
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "############################################################## \n");
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "DONE: Testing the EnumVal name instead of the Enum declaration \n");
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "############################################################## \n");
#endif

      int amountOfNameQualificationRequired = nameQualificationDepth(
          enumDeclaration, currentScope, currentStatement);

      if (amountOfNameQualificationRequired == 0) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Specify name qualification to resolve ambiguity: "
                    "additionalNameQualificationToResolveAmbiguity = %d \n",
                    additionalNameQualificationToResolveAmbiguity);
#endif
        amountOfNameQualificationRequired =
            additionalNameQualificationToResolveAmbiguity;
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "SgEnumVal: amountOfNameQualificationRequired = %d \n",
                  amountOfNameQualificationRequired);
#endif
      setNameQualification(enumVal, enumDeclaration,
                           amountOfNameQualificationRequired);
    } else {
      // DQ (9/17/2011): Added this case, print a warning and fix thiat after
      // debugging the constant folding value elimination..
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: SgEnumVal name qualification not handled for the "
                  "case of currentScope == NULL \n");
    }
  }

  // DQ (6/2/2011): Handle the range of expressions that can reference types
  // that might require name qualification...
  SgNewExp *newExp = isSgNewExp(n);
  SgSizeOfOp *sizeOfOp = isSgSizeOfOp(n);
  SgCastExp *castExp = isSgCastExp(n);
  SgTypeIdOp *typeIdOp = isSgTypeIdOp(n);
  if (newExp != NULL || sizeOfOp != NULL || castExp != NULL ||
      typeIdOp != NULL) {
    SgExpression *referenceToType = isSgExpression(n);

    bool skipQualification = false;

    SgType *qualifiedType = NULL;
    switch (n->variantT()) {
    case V_SgNewExp: {
      qualifiedType = newExp->get_specified_type();
      break;
    }

    case V_SgSizeOfOp: {
      qualifiedType = sizeOfOp->get_operand_type();
      if (qualifiedType == NULL) {
        // This is the case of a value, which need not be qualified. Except that
        // it could be a variable, but then it should be a SgVarRefExp
        ASSERT_not_null(sizeOfOp->get_operand_expr());
        skipQualification = true;
      }
      break;
    }

    case V_SgCastExp: {
      qualifiedType = castExp->get_type();
      break;
    }

      // DQ (1/26/2013): typeId operator can take either an expression or a
      // type, get_type() returns the type independent of which is specified.
      // case V_SgTypeIdOp: qualifiedType = typeIdOp->get_type(); break;
    case V_SgTypeIdOp: {
      qualifiedType = typeIdOp->get_operand_type();
      if (qualifiedType == NULL) {
        ASSERT_not_null(typeIdOp->get_operand_expr());
        skipQualification = true;
      }
      break;
    }

    default: {
      // Anything else should not make it this far...
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: default reached in switch... n = %p = %s \n", n,
                  n->class_name().c_str());
      ROSE_ABORT();
    }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    if (qualifiedType == NULL) {
      // We see this case for test2006_139.C  (code is: "sizeof("string")" or
      // "sizeof(<SgVarRefExp>)" ).
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Note: qualifiedType == NULL for n = %p = %s \n", n,
                  n->class_name().c_str());
    }
#endif
    // ASSERT_not_null(qualifiedType);

    if (skipQualification == false) {
      // DQ (1/26/2013): added assertion.
      ASSERT_not_null(qualifiedType);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "before stripType(): qualifiedType = %p = %s \n",
                  qualifiedType, qualifiedType->class_name().c_str());
#endif
      // DQ (5/19/2019): Comment this out since it causes the cast to loose the
      // information about casts of pointer types. See test2019_433.C.
      // qualifiedType = qualifiedType->stripType(SgType::STRIP_POINTER_TYPE);
      SgType *strippedQualifiedType =
          qualifiedType->stripType(SgType::STRIP_POINTER_TYPE);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "after stripType():  qualifiedType         = %p = %s \n",
                  qualifiedType, qualifiedType->class_name().c_str());
      MLOG_WARN_C(MLOG_UNPARSER,
                  "after stripType():  strippedQualifiedType = %p = %s \n",
                  strippedQualifiedType,
                  strippedQualifiedType->class_name().c_str());
#endif

      // DQ (4/26/2019): This variable is not used.
      // DQ (4/16/2019): If the qualifiedType is a SgPointerMemberType, then we
      // need to have this return the declaration associated with the base type.
      // SgDeclarationStatement* associatedTypeDeclaration =
      // associatedDeclaration(qualifiedType);
      SgDeclarationStatement *associatedTypeDeclaration =
          associatedDeclaration(strippedQualifiedType);

      // DQ (4/15/2019): Adding SgPointerMemberType support for the few
      // expressions that contain explicit references to types
      // SgPointerMemberType* pointerMemberType =
      // isSgPointerMemberType(qualifiedType);
      SgPointerMemberType *pointerMemberType =
          isSgPointerMemberType(strippedQualifiedType);
      if (pointerMemberType != NULL) {

        // DQ (4/27/2019): This is the newer code for what we have detected a
        // SgPointerMemberType.

        // DQ (4/21/2019): Then save call traverseType to save the type as a
        // string to be accessed when unparsing the type from this expression
        // (e.g. new operator).

        SgStatement *currentStatement =
            SageInterface::getEnclosingStatement(referenceToType);
        SgScopeStatement *currentScope = currentStatement->get_scope();
        ASSERT_not_null(currentScope);
        // generateNestedTraversalWithExplicitScope(type,currentScope,currentStatement,initializedName);
        generateNestedTraversalWithExplicitScope(
            pointerMemberType, currentScope, currentStatement, referenceToType);
        // DQ (4/19/2019): It might be that we should call this after the
        // traveral over each type instead of before we traverse the type. This
        // way we save the correctly computed string for each type after the
        // different parts of name qualificaiton are in place.
        // traverseType(initializedName->get_type(),initializedName,currentScope,currentStatement);
        traverseType(pointerMemberType, referenceToType, currentScope,
                     currentStatement);

      } else {

        // DQ (4/27/2019): Turn on this code that was previously disabled (does
        // not address SgPointerMemberType support requirements.
        // SgDeclarationStatement* associatedTypeDeclaration =
        // associatedDeclaration(qualifiedType);
        if (associatedTypeDeclaration != NULL) {
          SgStatement *currentStatement =
              SageInterface::getEnclosingStatement(n);

          // ASSERT_not_null(currentStatement);
          if (currentStatement != NULL) {
            SgScopeStatement *currentScope = currentStatement->get_scope();
            if (currentScope != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER, "INFO: currentStatement = %p (%s)\n",
                          currentStatement,
                          currentStatement->class_name().c_str());
              MLOG_WARN_C(MLOG_UNPARSER, "INFO: currentScope     = %p (%s)\n",
                          currentScope, currentScope->class_name().c_str());
#endif

              int amountOfNameQualificationRequiredForType =
                  nameQualificationDepth(associatedTypeDeclaration,
                                         currentScope, currentStatement);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "SgExpression (name = %s) type: "
                          "amountOfNameQualificationRequiredForType = %d \n",
                          referenceToType->class_name().c_str(),
                          amountOfNameQualificationRequiredForType);
#endif
              setNameQualification(referenceToType, associatedTypeDeclaration,
                                   amountOfNameQualificationRequiredForType);
              // DQ (6/3/2011): Traverse the type to set any possible template
              // arguments (or other subtypes?) that require name qualification.
              traverseType(qualifiedType, referenceToType, currentScope,
                           currentStatement);
            } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
              MLOG_WARN_C(MLOG_UNPARSER,
                          "WARNING: currentStatement->get_scope() == NULL for "
                          "case of referenceToType = %p = %s \n",
                          referenceToType,
                          referenceToType->class_name().c_str());
#endif
            }
          } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "WARNING: currentStatement == NULL for case of "
                        "referenceToType = %p = %s \n",
                        referenceToType, referenceToType->class_name().c_str());
#endif
          }
        } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Note: associatedTypeDeclaration == NULL in SgExpression "
                      "for name qualification support! referenceToType = %s \n",
                      referenceToType->class_name().c_str());
#endif
        }
        // DQ (4/27/2019): This has been moved from above since it should be
        // extended to include this code which is supporting the
        // non-SgPointerMemberType.
      }
    } else {
      // DQ (4/21/2019): We can skip this because it was not a type (likely an
      // expression, which will be traversed next).
    }
  }

  // DQ (6/21/2011): Added support for name qualification of expressions
  // contained in originalExpressionTree's where they are stored.
  SgExpression *expression = isSgExpression(n);
  if (expression != NULL) {
    SgExpression *originalExpressionTree =
        expression->get_originalExpressionTree();
    if (originalExpressionTree != NULL) {
      // Note that we have to pass the local copy of the referencedNameSet so
      // that the same set will be used for all recursive calls (see
      // test2011_89.C).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "@@@@@@@@@@@@@ Recursive call to the originalExpressionTree "
                  "= %p = %s \n",
                  originalExpressionTree,
                  originalExpressionTree->class_name().c_str());
#endif
      SgStatement *currentStatement = SageInterface::getEnclosingStatement(n);
      // DQ (9/14/2015): Added debugging code.
      // DQ (9/14/2015): This can be an expression in a type, in which case we
      // don't have an associated scope.
      if (currentStatement == NULL) {
        // This can be an expression in a type, in which case we don't have an
        // associated scope.
      } else {
        ASSERT_not_null(currentStatement);
        SgScopeStatement *currentScope = currentStatement->get_scope();
        ASSERT_not_null(currentScope);

        generateNestedTraversalWithExplicitScope(originalExpressionTree,
                                                 currentScope);
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "@@@@@@@@@@@@@ DONE: Recursive call to the "
                  "originalExpressionTree = %p = %s \n",
                  originalExpressionTree,
                  originalExpressionTree->class_name().c_str());
#endif
    }
  }

  // DQ (6/25/2011): Added support for use from unparseToString().
  // I don't think that we need this case since the unparser handles the case of
  // using the fully qualified name directly when called from the
  // unparseToString() function.
  SgType *type = isSgType(n);
  if (type != NULL) {
    // void NameQualificationTraversal::traverseType ( SgType* type, SgNode*
    // nodeReferenceToType, SgScopeStatement* currentScope, SgStatement*
    // positionStatement ) SgNode* nodeReferenceToType    = NULL;
    // SgScopeStatement* currentScope = NULL;
    // SgStatement* positionStatement = NULL;
    // traverseType(type,initializedName,currentScope,currentStatement);
  }

  // ******************************************************************************
  // Now that this declaration is processed, mark it as being seen (place into
  // set).
  // ******************************************************************************

  // DQ (2/13/2019): I think that this kind of declaration was not previously
  // processed for name qualification. Likely missed because enums previously
  // could not have a prototype declaration (but can under C++11).
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(n);
  if (enumDeclaration != NULL) {
    // We need the structural location in scope (not the semantic one).
    SgScopeStatement *currentScope =
        isSgScopeStatement(enumDeclaration->get_parent());

    if (currentScope == NULL) {
      // DQ (2/18/2019): Adding support for when the SgEnumDeclaration is
      // defined in another declaration (e.g. SgTypedefDeclaration).
      SgNode *parent = enumDeclaration->get_parent();
      SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(parent);
      if (typedefDeclaration != NULL) {
        currentScope = isSgScopeStatement(typedefDeclaration->get_parent());

        // DQ (2/18/2019): We should have a valid currentScope at this point.
        if (currentScope == NULL) {
          MLOG_WARN_C(MLOG_UNPARSER,
                      "NOTE: Could not identify scope for enum declaration: "
                      "parent = %p = %s \n",
                      parent, parent->class_name().c_str());
          MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
          ROSE_ABORT();
        } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Found SgEnumDeclaration in SgTypedefDeclaration: "
                      "currentScope = %p = %s \n",
                      currentScope, currentScope->class_name().c_str());
#endif
        }
      } else {
        // DQ (2/19/2019): This is frequently a SgLambdaExp or a
        // SgVariableDeclaration Computing the current scope does not always
        // seem possible.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "In name qualification: Cannot compute a valid scope for "
                    "the enumDeclaration = %p = %s \n",
                    enumDeclaration, enumDeclaration->class_name().c_str());
        MLOG_WARN_C(MLOG_UNPARSER, " --- parent = %p = %s \n", parent,
                    parent->class_name().c_str());
#endif
      }
    }

    // ASSERT_not_null(currentScope);
    if (currentScope != NULL) {
      // Only use name qualification where the scopes of the declaration's use
      // (currentScope) is not the same as the scope of the class declaration.
      // However, the analysis should work and determin that the required name
      // qualification length is zero.

      // DQ (7/22/2017): Refactored this code.
      SgScopeStatement *enum_scope = enumDeclaration->get_scope();

      // DQ (7/22/2017): I think we can assert this.
      ASSERT_not_null(enum_scope);
      // if (currentScope != classDeclaration->get_scope())
      if (currentScope != enum_scope) {
        // DQ (1/21/2013): We should be able to assert this.
        ASSERT_not_null(enumDeclaration->get_scope());

        // DQ (1/21/2013): Added new static function to support testing for
        // equivalent when the scopes are namespaces.
        bool isSameNamespace = SgScopeStatement::isEquivalentScope(
            currentScope, enumDeclaration->get_scope());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "isSameNamespace = %s \n",
                    isSameNamespace ? "true" : "false");
#endif
        // DQ (1/21/2013): Added code to support when equivalent namespaces are
        // detected.
        if (isSameNamespace == false) {
          // DQ (6/11/2013): Added test to make sure that name qualification is
          // ignored for friend function where the class has not yet been seen.
          // if (classDeclaration->get_declarationModifier().isFriend() ==
          // false)
          SgDeclarationStatement *declarationForReferencedNameSet =
              enumDeclaration->get_firstNondefiningDeclaration();
          ASSERT_not_null(declarationForReferencedNameSet);
          if (referencedNameSet.find(declarationForReferencedNameSet) !=
              referencedNameSet.end()) {
            int amountOfNameQualificationRequired = nameQualificationDepth(
                enumDeclaration, currentScope, enumDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "SgEnumDeclaration: amountOfNameQualificationRequired = %d \n",
                amountOfNameQualificationRequired);
#endif
            setNameQualification(enumDeclaration,
                                 amountOfNameQualificationRequired);
          } else {
            // DQ (2/12/2019): This branch is taken within
            // Cxx11_tests/test2019_120.C where the associated
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "This enumDeclaration has not been seen before so skip "
                        "the name qualification \n");
#endif
          }
        }
      } else {
        // Don't know what test code exercises this case (see test2011_62.C).
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "WARNING: SgEnumDeclaration -- currentScope is not "
                    "available through predicate (currentScope != "
                    "enumDeclaration->get_scope()), not clear why! \n");
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Commenting out: enumDeclaration->get_parent() == "
                    "enumDeclaration->get_scope() in name qualitication \n");
        // ROSE_ASSERT(classDeclaration->get_parent() ==
        // classDeclaration->get_scope());
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "name qualification for enumDeclaration->get_scope()  = %p = %s \n",
            enumDeclaration->get_scope(),
            enumDeclaration->get_scope()->class_name().c_str());
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "enumDeclaration->get_parent()                        = %p = %s \n",
            enumDeclaration->get_parent(),
            enumDeclaration->get_parent()->class_name().c_str());
#endif
        // ROSE_ABORT();

        // DQ (7/22/2017): I think the template arguments name qualification can
        // be required, but is ignored. DQ (7/22/2017): I think the template
        // arguments name qualification can be required. This fixes
        // test2017_56.C.
        int amountOfNameQualificationRequired = nameQualificationDepth(
            enumDeclaration, currentScope, enumDeclaration);
        // We only really wanted to make sure that any template arguments were
        // properly name qualified.
        amountOfNameQualificationRequired = 0;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "NEW CASE: currentScope != enumDeclaration->get_scope(): "
            "SgEnumDeclaration: amountOfNameQualificationRequired = %d \n",
            amountOfNameQualificationRequired);
#endif
        setNameQualification(enumDeclaration,
                             amountOfNameQualificationRequired);
      }
    } else {
      // DQ (2/13/2019): I think this can happen if the enum declaration is in a
      // typedef declaration or parameter list, etc (less common places to find
      // enum declarations).

      // NOTE: Cxx_tests/test2019_125.C demonstrates where this kind of enum
      // requires name qualification.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: SgEnumDeclaration -- currentScope "
                                 "is not available, not clear why! \n");
#endif
      // enumDeclaration->get_file_info()->display("Cannot determine current
      // scope for SgEnumDeclaration");

      SgDeclarationStatement *outerDeclaration =
          isSgDeclarationStatement(enumDeclaration->get_parent());
      ASSERT_not_null(outerDeclaration);
      currentScope = isSgScopeStatement(outerDeclaration->get_parent());
      ASSERT_not_null(currentScope);

      int amountOfNameQualificationRequired = nameQualificationDepth(
          enumDeclaration, currentScope, enumDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "NEW CASE: currentScope != enumDeclaration->get_scope(): "
          "SgEnumDeclaration: amountOfNameQualificationRequired = %d \n",
          amountOfNameQualificationRequired);
#endif
      setNameQualification(enumDeclaration, amountOfNameQualificationRequired);

      // DQ (2/13/2019): Make this an error for now!
      // ROSE_ABORT();
    }
  }

  SgDeclarationStatement *declaration = isSgDeclarationStatement(n);
  if (declaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
           "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "Found a SgDeclarationStatement in the evaluation of name "
                "qualification declaration = %p = %s \n",
                declaration, declaration->class_name().c_str());
    printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
           "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
#endif
    // If this is a declaration of something that has a name then we need to
    // mark it as having been seen.

    // In some cases of C++ name qualification depending on if the defining
    // declaration (or a forward declaration is in a scope that would define the
    // declaration to a scope where the declaration could be present).  This
    // detail is handled by reporting if such a declaration has been seen yet.
    // Since the preorder traversal is the same as the traversal used in the
    // unparsing it is sufficient to record the order of the processing here and
    // not complicate the unparser directly.  Note that the use of function
    // declarations follow these rules and so are a problem when the prototype
    // is defined in a function (where it does not communicate the defining
    // declarations location) instead of in a global scope or namespace scope
    // (where it does appear to communicate its position.

    // SgDeclarationStatement* firstNondefiningDeclaration   =
    // declaration->get_firstNondefiningDeclaration();
    SgDeclarationStatement *declarationForReferencedNameSet =
        declaration->get_firstNondefiningDeclaration();
    // ROSE_ASSERT(declarationForReferencedNameSet == NULL);
    // declarationForReferencedNameSet =
    // declaration->get_firstNondefiningDeclaration();

    if (declarationForReferencedNameSet == NULL) {
      // Note that a function with only a defining declaration will not have a
      // nondefining declaration automatically constructed in the AST (unlike
      // classes and some other sorts of declarations).
      declarationForReferencedNameSet = declaration->get_definingDeclaration();

      // DQ (6/22/2011): I think this is true.  This assertion fails for
      // test2006_78.C (a template example code).
      // ROSE_ASSERT(declarationForReferencedNameSet == declaration);

      // DQ (6/23/2011): This assertion fails for the LoopProcessor on
      // tests/nonsmoke/functional/roseTests/loopProcessingTests/mm.C
      // ASSERT_not_null(declarationForReferencedNameSet);
      if (declarationForReferencedNameSet == NULL) {
        declarationForReferencedNameSet = declaration;
        ASSERT_not_null(declarationForReferencedNameSet);
      }
      ASSERT_not_null(declarationForReferencedNameSet);
    }
    ASSERT_not_null(declarationForReferencedNameSet);
    // Look at each declaration, but as soon as we find an acceptable one put
    // the declarationForReferencedNameSet into the set so that we can search on
    // a uniform representation of the declaration. Note that we want the scope
    // of where it is located and not it's scope if it were name qualified...
    // SgScopeStatement* scopeOfNondefiningDeclaration =
    // isSgScopeStatement(firstNondefiningDeclaration->get_parent());
    ASSERT_not_null(declaration->get_parent());
    SgScopeStatement *scopeOfDeclaration =
        isSgScopeStatement(declaration->get_parent());

    // DQ (5/19/2017): added support for test2017_39.C
    // (SgTemplateInstantiationDirectiveStatement support). In this case the
    // SgTemplateInstantiation is a declaration hidden inside of the
    // SgTemplateInstantiationDirectiveStatement.
    SgTemplateInstantiationDirectiveStatement *templateInstantiationDirective =
        isSgTemplateInstantiationDirectiveStatement(declaration->get_parent());
    if (templateInstantiationDirective != NULL) {
      scopeOfDeclaration =
          isSgScopeStatement(templateInstantiationDirective->get_parent());
      ASSERT_not_null(scopeOfDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "************* Found SgTemplateInstantiationDirectiveStatement: "
          "reset scope to that of the template instantiation directive: scope "
          "= %p = %s  \n",
          scopeOfDeclaration, scopeOfDeclaration->class_name().c_str());
#endif
    }

    bool acceptableDeclarationScope = false;

    // I think that some declarations might not appear in a scope properly (e.g
    // pointer to function, etc.)
    // ASSERT_not_null(scopeOfNondefiningDeclaration);
    if (scopeOfDeclaration != NULL) {
      switch (scopeOfDeclaration->variantT()) {
        // At least this case is not allowed.
      case V_SgBasicBlock:
        acceptableDeclarationScope = false;
        break;

        // Everything else is OK!
        // DQ (6/22/2011): Note that a declaration in a typedef is an acceptable
        // scope under some cases (not clear on the limits of this case).
      default: {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "scopeOfNondefiningDeclaration = %p = %s \n",
                    scopeOfDeclaration,
                    scopeOfDeclaration->class_name().c_str());
#endif
        acceptableDeclarationScope = true;
      }
      }
    } else {
      // This appears to fail for something in
      // rose_required_macros_and_functions.h.

      // DQ (2/18/2019): This case happens when an enum declaration is
      // contained as the base type in a typedef declaration. In which
      // case the scope is just the scope of the enclosing typedef
      // declaration.
      SgNode *parent = declaration->get_parent();

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "scopeOfDeclaration == NULL: declaration               = %p = %s \n",
          declaration, declaration->class_name().c_str());
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "scopeOfDeclaration == NULL: declaration->get_parent() = %p = %s \n",
          parent, parent->class_name().c_str());
#endif

      // DQ (2/18/2019): Chasing down all the things that can be the parent when
      // the scope of a declaration computed via the parent is not clear.
      SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(parent);
      if (typedefDeclaration != NULL) {
        scopeOfDeclaration =
            isSgScopeStatement(typedefDeclaration->get_parent());
      } else {
        SgFunctionDeclaration *functionDeclaration =
            isSgFunctionDeclaration(parent);
        if (functionDeclaration != NULL) {
          ASSERT_not_null(functionDeclaration);
          scopeOfDeclaration =
              isSgScopeStatement(functionDeclaration->get_parent());
          if (scopeOfDeclaration == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "test 1: SgFunctionDeclaration: scopeOfDeclaration == NULL: "
                "cannot support name qualification: "
                "functionDeclaration->get_parent() = %p = %s \n",
                functionDeclaration->get_parent(),
                functionDeclaration->get_parent()->class_name().c_str());
#endif
          }
        } else {
          SgFunctionParameterList *functionParameterList =
              isSgFunctionParameterList(parent);
          if (functionParameterList != NULL) {
            SgFunctionDeclaration *functionDeclaration =
                isSgFunctionDeclaration(functionParameterList->get_parent());
            ASSERT_not_null(functionDeclaration);
            scopeOfDeclaration =
                isSgScopeStatement(functionDeclaration->get_parent());
            if (scopeOfDeclaration == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "test 2: SgFunctionParameterList: SgFunctionDeclaration: "
                  "scopeOfDeclaration == NULL: cannot support name "
                  "qualification: functionDeclaration->get_parent() = %p = %s "
                  "\n",
                  functionDeclaration->get_parent(),
                  functionDeclaration->get_parent()->class_name().c_str());
#endif
            }
          }
        }
      }

      if (scopeOfDeclaration == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "scopeOfDeclaration == NULL: Could not identify scope of "
            "declaration to support name qualification: parent = %p = %s \n",
            parent, parent->class_name().c_str());
#endif
      } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER, "lost scope: scopeOfDeclaration = %p = %s \n",
            scopeOfDeclaration, scopeOfDeclaration->class_name().c_str());
#endif
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "I hope that we can make this an error (scopeOfDeclaration == NULL) "
          "declaration = %p = %s declaration->get_parent() = %p = %s \n",
          declaration, declaration->class_name().c_str(),
          declaration->get_parent(),
          declaration->get_parent()->class_name().c_str());
#endif
      // ROSE_ABORT();
    }
    ASSERT_not_null(declarationForReferencedNameSet);
    // if (referencedNameSet.find(firstNondefiningDeclaration) ==
    // referencedNameSet.end()) if (acceptableDeclarationScope == true &&
    // firstNondefiningDeclaration != NULL &&
    // referencedNameSet.find(firstNondefiningDeclaration) ==
    // referencedNameSet.end())
    if (acceptableDeclarationScope == true &&
        referencedNameSet.find(declarationForReferencedNameSet) ==
            referencedNameSet.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Adding declarationForReferencedNameSet = %p = %s to set of "
                  "visited declarations \n",
                  declarationForReferencedNameSet,
                  declarationForReferencedNameSet->class_name().c_str());
#endif
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      printf("INSERTING INTO referencedNameSet: "
             "declarationForReferencedNameSet = %p = %s name = %s \n",
             declarationForReferencedNameSet,
             declarationForReferencedNameSet->class_name().c_str(),
             SageInterface::get_name(declarationForReferencedNameSet).c_str());
      printf(" --- Could have used: declaration = %p = %s name = %s \n",
             declaration, declaration->class_name().c_str(),
             SageInterface::get_name(declaration).c_str());
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
             "@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
#endif
      referencedNameSet.insert(declarationForReferencedNameSet);
    } else {
      // MLOG_WARN_C(MLOG_UNPARSER, "firstNondefiningDeclaration = %p NOT added
      // to referencedNameSet \n",firstNondefiningDeclaration);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
             "$$$$$$$$$$$$$$ \n");
      MLOG_WARN_C(MLOG_UNPARSER,
                  "declarationForReferencedNameSet = %p NOT added to "
                  "referencedNameSet \n",
                  declarationForReferencedNameSet);
      printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
             "$$$$$$$$$$$$$$ \n");
#endif
    }
  }

  // DQ (7/12/2014): Add any possible nodes that can generate SgAliasSymbols to
  // the SgSymbolTable::p_aliasSymbolCausalNodeSet . This is used by the symbol
  // table to know when to use or ignore SgAliasSymbols in symbol table lookups.
  if (isSgUsingDirectiveStatement(n) != NULL ||
      isSgUsingDeclarationStatement(n) != NULL || isSgBaseClass(n) != NULL) {
    SgSymbolTable::insert_aliasSymbolCausalNodeSet(n);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::evaluateInheritedAttribute(): "
                "Added SgAliasSymbols causal node = %p = %s to "
                "SgSymbolTable::p_aliasSymbolCausalNodeSet size = %" PRIuPTR
                " \n",
                n, n->class_name().c_str(),
                SgSymbolTable::get_aliasSymbolCausalNodeSet().size());
#endif

    // DQ (12/23/2015): This does not appear to have any effect (the SgBaseClass
    // is not traversed in the AST).
    if (isSgBaseClass(n) != NULL) {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "NameQualificationTraversal::evaluateInheritedAttribute(): "
                  "Identified SgBaseClass in traversal \n");
      ROSE_ABORT();
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "****************************************************** \n");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "Leaving NameQualificationTraversal::evaluateInheritedAttribute(): node "
      "= %p = %s \n",
      n, n->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER,
              "******************************************************\n\n\n");
#endif

  return NameQualificationInheritedAttribute(inheritedAttribute);
}

NameQualificationSynthesizedAttribute
NameQualificationTraversal::evaluateSynthesizedAttribute(
    SgNode *n, NameQualificationInheritedAttribute inheritedAttribute,
    SynthesizedAttributesList synthesizedAttributeList) {
  // This is not used now but will likely be used later.
  // NameQualificationSynthesizedAttribute returnAttribute;
  NameQualificationSynthesizedAttribute returnAttribute(n);

  // DQ (8/2/2020): Added assertion.
  ROSE_ASSERT(n != NULL);

  // #if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)

  // DQ (8/14/2025): This is an optimization to skip the traversal of the AST
  // outside of what is in the source tree.
  if (suppressNameQualificationAcrossWholeTranslationUnit == true) {
    // SgStatement* statement = isSgStatement(n);
    SgLocatedNode *locatedNode = isSgLocatedNode(n);
    // if (statement != NULL)
    if (locatedNode != NULL) {
      // DQ (8/14/2025): Adding support to count the number of statements
      // traversed in the name qualification when using traverseInputFile(). It
      // should be only the statements in the source file, but it appears to
      // include statements marked as compilerGenerated.
      // AstPerformance::numberOfStatementsProcessedInNameQualificationUsingTraverseInputFile++;

      // if (statement->get_file_info()->get_filenameString() !=
      // "compilerGenerated") if (statement->isCompilerGenerated() == true)
      if (locatedNode->isCompilerGenerated() == false) {
        // We could just check is the nearest parent statement is compiler
        // generated. Or we could see if this is from a header file...(let's not
        // do that).
        SgStatement *statement =
            SageInterface::getEnclosingStatement(locatedNode);
        if (statement->isCompilerGenerated() == false) {
        } else {
          return returnAttribute;
        }
      } else {
        return returnAttribute;
      }
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // DQ (6/23/2013): Output the generated name with required name qualification
  // for debugging.
  switch (n->variantT()) {
    // DQ (8/19/2013): Added case to support debugging.
  case V_SgConstructorInitializer:

  case V_SgMemberFunctionRefExp: {
    MLOG_WARN_C(MLOG_UNPARSER, "***********************************************"
                               "*************************************** \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "In evaluateSynthesizedAttribute(): node = %p = %s "
                "node->unparseToString() = %s \n",
                n, n->class_name().c_str(), n->unparseToString().c_str());
    MLOG_WARN_C(MLOG_UNPARSER, "***********************************************"
                               "*************************************** \n");
    break;
  }

  default: {
    // do nothing
  }
  }
#endif

  // DQ (4/21/2019): We need to compute the final generated string for the type
  // from the parts that have been constructed from the type traversal in the
  // evaluation of the inherited attribute. This we should not miss type
  // modifieres in between the referenceNode and the SgPointerMemberType IR node
  // (need to look into this).

  // DQ (4/21/2019): I now think that we don't need this, but how is the
  // referenceNode a SgPointerMemberType. NOTE: We might also just call this
  // once when n is the same as a saved referenceNode.
  SgNode *referenceNode = inheritedAttribute.get_referenceNode();
  if (n == referenceNode) {
    // DQ (4/21/2019): I think this must be called on in the
    // evaluateSythesisedAttribute traversal, since it requires that the string
    // results already be computed for nested types (visited first in the
    // evaluation of the inherited attributes)

    ASSERT_not_null(referenceNode);
    MLOG_WARN_C(MLOG_UNPARSER,
                "##############################################################"
                "####################################################### \n");
    MLOG_WARN_C(MLOG_UNPARSER,
                "In evaluateSynthesizedAttribute(): Case n == referenceNode: "
                "referenceNode = %p = %s: Calling traverseType() \n",
                referenceNode, referenceNode->class_name().c_str());
    MLOG_WARN_C(MLOG_UNPARSER,
                "##############################################################"
                "####################################################### \n");

    SgScopeStatement *currentScope = inheritedAttribute.get_currentScope();
    ASSERT_not_null(currentScope);

    SgStatement *currentStatement = inheritedAttribute.get_currentStatement();
    ASSERT_not_null(currentStatement);

    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In evaluateSynthesizedAttribute(): referenceNode    = %p = %s \n",
        referenceNode, referenceNode->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In evaluateSynthesizedAttribute(): currentScope     = %p = %s \n",
        currentScope, currentScope->class_name().c_str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In evaluateSynthesizedAttribute(): currentStatement = %p = %s \n",
        currentStatement, currentStatement->class_name().c_str());

    SgInitializedName *initializedName = isSgInitializedName(referenceNode);
    if (initializedName != NULL) {
      SgType *generateStringForType = initializedName->get_type();
      ASSERT_not_null(generateStringForType);

      traverseType(generateStringForType, referenceNode, currentScope,
                   currentStatement);
    } else {
      // DQ (4/21/2019): We will have to handle every kind of IR node that will
      // require a computed string for it's type (a few, but not too many).
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Error: We need to find the type and the kind of IR node from which "
          "it is to be generated as a string: referenceNode = %p = %s \n",
          referenceNode, referenceNode->class_name().c_str());
      ROSE_ABORT();
    }

    MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
    ROSE_ABORT();
  }

  // Iterate over the synthesizedAttributeList.
  SynthesizedAttributesList::iterator i = synthesizedAttributeList.begin();
  while (i != synthesizedAttributeList.end()) {
    NameQualificationSynthesizedAttribute synthesizedAttribute = *i;
    // ROSE_ASSERT(synthesizedAttribute != NULL);
    SgNode *synthesizedAttributeNode = synthesizedAttribute.node;

    // DQ (8/2/2020): Not clear why this can be NULL.
    // ROSE_ASSERT(synthesizedAttributeNode != NULL);

    if (synthesizedAttributeNode != NULL) {
      SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
          isSgNamespaceAliasDeclarationStatement(synthesizedAttributeNode);
      if (namespaceAliasDeclaration != NULL) {
        SgDeclarationStatement *declaration =
            namespaceAliasDeclaration
                    ->get_is_alias_for_another_namespace_alias()
                ? isSgDeclarationStatement(
                      namespaceAliasDeclaration
                          ->get_namespaceAliasDeclaration())
                : isSgDeclarationStatement(
                      namespaceAliasDeclaration->get_namespaceDeclaration());
        ROSE_ASSERT(declaration != NULL);
        // ROSE_ASSERT(namespaceAliasDeclarationMap.find(declaration) !=
        // namespaceAliasDeclarationMap.end());
        if (namespaceAliasDeclarationMap.find(declaration) !=
            namespaceAliasDeclarationMap.end()) {
          namespaceAliasMapType::iterator i =
              namespaceAliasDeclarationMap.find(declaration);
          ROSE_ASSERT(i != namespaceAliasDeclarationMap.end());
          namespaceAliasDeclarationMap.erase(i);
        }
      }
    }

    i++;
  }

  // DQ (8/2/2020): Added assertion.
  ROSE_ASSERT(returnAttribute.node != NULL);

  return returnAttribute;
}

#define DEBUG_TRAVERSE_NONREAL_FOR_SCOPE 0

// Dealing with nonreal's scope: the scope is where the nonreal is instantiated
// this code either:
//  - Uses the scope of the associated template, if it exists
//  - Traverse nonreal parent using while loop, else
SgScopeStatement *
traverseNonrealDeclForCorrectScope(SgDeclarationStatement *declaration) {
  SgScopeStatement *scope = declaration->get_scope();
  ASSERT_not_null(scope);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_NONREAL_FOR_SCOPE
  MLOG_WARN_C(MLOG_UNPARSER, "In traverseNonrealDeclForCorrectScope():\n");
  MLOG_WARN_C(MLOG_UNPARSER, " --- declaration = %p (%s)\n", declaration,
              declaration->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER, " --- scope = %p (%s)\n", scope,
              scope->class_name().c_str());
#endif

  SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration);
  while (nrdecl != NULL) {
    if (nrdecl->get_is_template_param()) {
      // Template parameters are looked up in template-parameter scope;
      // qualifying them as namespace/class members is invalid.
      SgScopeStatement *param_scope = nrdecl->get_scope();
      if (SgDeclarationScope *decl_scope = isSgDeclarationScope(param_scope)) {
        scope = decl_scope;
      } else {
        scope = param_scope;
      }
      ASSERT_not_null(scope);
      break;
    }

    if (nrdecl->get_templateDeclaration() == NULL) {
      SgDeclarationScope *decl_scope =
          isSgDeclarationScope(nrdecl->get_scope());
      if (decl_scope == NULL) {
        // Nonreal declarations can live directly in regular scopes (e.g.
        // concept declarations). In that case, use the current scope.
        scope = nrdecl->get_scope();
        ASSERT_not_null(scope);
        break;
      }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_NONREAL_FOR_SCOPE
      MLOG_WARN_C(MLOG_UNPARSER, " --- decl_scope = %p (%s)\n", decl_scope,
                  decl_scope->class_name().c_str());
#endif

      SgNode *decl_scope_parent = decl_scope->get_parent();
      ASSERT_not_null(decl_scope_parent);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_NONREAL_FOR_SCOPE
      MLOG_WARN_C(MLOG_UNPARSER, " --- decl_scope_parent = %p (%s)\n",
                  decl_scope_parent, decl_scope_parent->class_name().c_str());
#endif

      SgNonrealDecl *nr_parent = isSgNonrealDecl(decl_scope_parent);
      if (nr_parent != NULL) {
        ROSE_ASSERT(nr_parent !=
                    nrdecl); // LOOP in the nonreal declaration: forbidden
        nrdecl = nr_parent;
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_NONREAL_FOR_SCOPE
        MLOG_WARN_C(MLOG_UNPARSER, " --- nrdecl = %p (%s)\n", nrdecl,
                    nrdecl->class_name().c_str());
#endif
      } else {
        SgScopeStatement *parent_scope = isSgScopeStatement(decl_scope_parent);
        if (parent_scope == NULL) {
          parent_scope = SageInterface::getEnclosingScope(decl_scope_parent);
        }
        ASSERT_not_null(parent_scope);
        scope = parent_scope;
        break;
      }
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_NONREAL_FOR_SCOPE
      MLOG_WARN_C(MLOG_UNPARSER,
                  " --- nrdecl->get_templateDeclaration() = %p (%s)\n",
                  nrdecl->get_templateDeclaration(),
                  nrdecl->get_templateDeclaration()->class_name().c_str());
#endif

      scope = nrdecl->get_templateDeclaration()->get_scope();
      ASSERT_not_null(scope);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || DEBUG_TRAVERSE_NONREAL_FOR_SCOPE
      MLOG_WARN_C(MLOG_UNPARSER, " --- scope = %p (%s)\n", scope,
                  scope->class_name().c_str());
#endif

      break;
    }
  }

  return scope;
}

static SgScopeStatement *
scopeForPointerMemberTypeQualification(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);

  SgScopeStatement *scope = NULL;
  if (SgNonrealDecl *nrdecl = isSgNonrealDecl(declaration)) {
    if (!nrdecl->get_is_template_param()) {
      scope = nrdecl->get_scope();
    }
  } else {
    scope = declaration->get_scope();
  }

  if (scope != NULL && isSgDeclarationScope(scope) == NULL) {
    return scope;
  }

  if (SgScopeStatement *enclosing_scope =
          SageInterface::getEnclosingScope(declaration)) {
    return enclosing_scope;
  }

  return traverseNonrealDeclForCorrectScope(declaration);
}

// ************************************************************************************
//    These overloaded functions, setNameQualification(), support references to
//    IR
// nodes that require name qualification.  Each function inserts a qualified
// name (string) into a map stored as a static data member in SgNode. For each
// IR node that is qualified, the reference to the IR node carries the name
// qualification (is used as a key in the map of qualified names).  There are
// two maps, one for the qualification of names and one for qualification of
// types.  Note that, since types are shared, it is more clear that the type
// can't carry the qualified name because it could be different at each location
// where the type is referenced; thus the reference to the type carries the
// qualified name (via the map).  The case of why named IR constructs have to
// have there qualified name in the IR node referencing the named construct is
// similar.
//
// They are only a few IR nodes that reference IR nodes that can be qualified:
//    SgExpression IR nodes:
//       SgVarRefExp
//       SgFunctionRefExp
//       SgMemberFunctionRefExp
//       SgConstructorInitializer
//       SgNewExp
//       SgCastExp
//       SgSizeOfOp
//       SgTypeIdOp
//
//    SgDeclarationStatement IR nodes:
//       SgFunctionDeclaration (for the function name)
//       SgFunctionDeclaration (for the return type)
//       SgUsingDeclarationStatement (for references to a declaration (e.g.
//       namespace or class)) SgUsingDeclarationStatement (for references to a
//       SgInitializedName) SgUsingDirectiveStatement SgVariableDeclaration
//       SgTypedefDeclaration
//       SgClassDeclaration
//       SgEnumDeclaration
//
//    SgStatement IR nodes:
//       SgForInitStatement is not a problems since it is a list of
//       SgInitializedName
//
//    SgLocatedNode nodes:
//       SgInitializedName
//
//    SgSupport nodes:
//       SgBaseClass
//       SgTemplateArgument
//
// Other (not yet supported) IR nodes recognized to reference types that could
// require name qualification support:
//    SgExpression IR nodes:
//
//    And maybe also:
//       SgPseudoDestructorRefExp
//       SgTemplateParameter
//
// Note also that name qualifiction can be required on expressions that are a
// part of the originalExpressionTree that represent the expanded representation
// from constant folding.  Thus we have to make a recursive call on all valid
// originalExpressionTree pointers where they are present:
//     SgBinaryOp
//     SgValueExp
//     SgFunctionRefExp
//     SgValueExp
//     SgCastExp
// ************************************************************************************

// DQ (4/20/2019): For where the input is a type (SgPointerMemberType) we need
// to pass in a referenceNode for what will become the key in the map for the
// name qualification prefix. void
// NameQualificationTraversal::setNameQualification ( SgPointerMemberType*
// pointerMemberType, SgNode* referenceNode, int
// amountOfNameQualificationRequired ) void
// NameQualificationTraversal::setNameQualification ( SgNode* referenceNode,
// SgDeclarationStatement* declaration, int amountOfNameQualificationRequired )
void NameQualificationTraversal::setNameQualificationOnClassOf(
    SgPointerMemberType *pointerMemberType, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // DQ (4/19/2019): Adding support for chains of SpPointerMemberType types
  // (requires type traversal).

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgPointerMemberType*) \n");
#endif

  SgScopeStatement *scope = scopeForPointerMemberTypeQualification(declaration);
  // SgScopeStatement * scope =
  // traverseNonrealDeclForCorrectScope(referenceNode);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  auto fileInfoWithinDeclRange = [](Sg_File_Info *target,
                                    SgDeclarationStatement *owner) -> bool {
    if (target == nullptr || owner == nullptr) {
      return false;
    }
    if (target->isCompilerGenerated() || target->isTransformation()) {
      return false;
    }

    Sg_File_Info *begin = owner->get_startOfConstruct();
    if (begin == nullptr || begin->isCompilerGenerated() ||
        begin->isTransformation()) {
      begin = owner->get_file_info();
    }

    Sg_File_Info *end = owner->get_endOfConstruct();
    if (end == nullptr || end->isCompilerGenerated() ||
        end->isTransformation()) {
      end = begin;
    }

    if (begin == nullptr || end == nullptr || begin->isCompilerGenerated() ||
        begin->isTransformation() || end->isCompilerGenerated() ||
        end->isTransformation()) {
      return false;
    }

    if (target->get_filenameString() != begin->get_filenameString() ||
        target->get_filenameString() != end->get_filenameString()) {
      return false;
    }

    auto lessOrEqual = [](Sg_File_Info *lhs, Sg_File_Info *rhs) {
      return lhs->get_line() < rhs->get_line() ||
             (lhs->get_line() == rhs->get_line() &&
              lhs->get_col() <= rhs->get_col());
    };

    if (!lessOrEqual(begin, end)) {
      std::swap(begin, end);
    }

    return lessOrEqual(begin, target) && lessOrEqual(target, end);
  };

  // DQ (4/19/2019): I would like to not have to add these data members to the
  // SgPointerMemberType IR node (see if we can do this).
  // pointerMemberType->set_global_qualification_required(outputGlobalQualification);
  // pointerMemberType->set_name_qualification_length(outputNameQualificationLength);

  // There should be no type evaluation required for a member pointer type, as
  // far as we know.
  ROSE_ASSERT(outputTypeEvaluation == false);
  // pointerMemberType->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification():
  // pointerMemberType->get_name_qualification_length()     = %d
  // \n",pointerMemberType->get_name_qualification_length());
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification():
  // pointerMemberType->get_type_elaboration_required()     = %s
  // \n",pointerMemberType->get_type_elaboration_required() ? "true" : "false");
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification():
  // pointerMemberType->get_global_qualification_required() = %s
  // \n",pointerMemberType->get_global_qualification_required() ? "true" :
  // "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "pointerMemberType: outputNameQualificationLength  = %d \n",
              outputNameQualificationLength);
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "pointerMemberType: outputTypeEvaluation           = %s \n",
              outputTypeEvaluation ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "pointerMemberType: outputGlobalQualification      = %s \n",
              outputGlobalQualification ? "true" : "false");

  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "qualifier = %s \n",
              qualifier.c_str());
#endif

  // if (qualifiedNameMapForTypes.find(pointerMemberType) ==
  // qualifiedNameMapForTypes.end())
  if (qualifiedNameMapForNames.find(pointerMemberType) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for (SgPointerMemberType) name = %s into "
                "list at IR node = %p = %s \n",
                qualifier.c_str(), pointerMemberType,
                pointerMemberType->class_name().c_str());
#endif
    // qualifiedNameMapForNames.insert(std::pair<SgNode*,std::string>(pointerMemberType,qualifier));
    // qualifiedNameMapForTypes.insert(std::pair<SgNode*,std::string>(pointerMemberType,qualifier));
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(pointerMemberType, qualifier));

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    // MLOG_WARN_C(MLOG_UNPARSER, "Testing name in map: for SgPointerMemberType
    // = %p qualified name = %s
    // \n",pointerMemberType,pointerMemberType->get_qualified_name_prefix().str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Testing name in map: for SgPointerMemberType = %p qualified name = %s "
        "\n",
        pointerMemberType,
        pointerMemberType->get_qualified_name_prefix_for_class_of().str());
    // MLOG_WARN_C(MLOG_UNPARSER,
    // "SgNode::get_globalQualifiedNameMapForNames().size() = %" PRIuPTR "
    // \n",SgNode::get_globalQualifiedNameMapForNames().size());
    // MLOG_WARN_C(MLOG_UNPARSER,
    // "SgNode::get_globalQualifiedNameMapForTypes().size() = %" PRIuPTR "
    // \n",SgNode::get_globalQualifiedNameMapForTypes().size());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "SgNode::get_globalQualifiedNameMapForNames().size() = %" PRIuPTR " \n",
        SgNode::get_globalQualifiedNameMapForNames().size());
#endif
  } else {
    // If it already existes then overwrite the existing information.
    // std::map<SgNode*,std::string>::iterator i =
    // qualifiedNameMapForNames.find(pointerMemberType); ROSE_ASSERT (i !=
    // qualifiedNameMapForNames.end()); std::map<SgNode*,std::string>::iterator
    // i = qualifiedNameMapForTypes.find(pointerMemberType);
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(pointerMemberType);
    // ROSE_ASSERT (i != qualifiedNameMapForTypes.end());
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 3: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    if (i->second != qualifier) {
      // DQ (3/15/2019): We need to disable the assertion below because it can
      // happen (see Cxx11_tests/test2019_214.C).
      i->second = qualifier;
    }
  }
}

void NameQualificationTraversal::setNameQualificationOnBaseType(
    SgPointerMemberType *pointerMemberType, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // DQ (4/19/2019): Adding support for chains of SpPointerMemberType types
  // (requires type traversal).

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgPointerMemberType*) \n");
#endif

  SgScopeStatement *scope = scopeForPointerMemberTypeQualification(declaration);
  // SgScopeStatement * scope =
  // traverseNonrealDeclForCorrectScope(referenceNode);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  // DQ (4/19/2019): I would like to not have to add these data members to the
  // SgPointerMemberType IR node (see if we can do this).
  // pointerMemberType->set_global_qualification_required(outputGlobalQualification);
  // pointerMemberType->set_name_qualification_length(outputNameQualificationLength);

  // There should be no type evaluation required for a pointer member type, as
  // far as we know.
  ROSE_ASSERT(outputTypeEvaluation == false);
  // pointerMemberType->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification():
  // pointerMemberType->get_name_qualification_length()     = %d
  // \n",pointerMemberType->get_name_qualification_length());
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification():
  // pointerMemberType->get_type_elaboration_required()     = %s
  // \n",pointerMemberType->get_type_elaboration_required() ? "true" : "false");
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification():
  // pointerMemberType->get_global_qualification_required() = %s
  // \n",pointerMemberType->get_global_qualification_required() ? "true" :
  // "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "pointerMemberType: outputNameQualificationLength  = %d \n",
              outputNameQualificationLength);
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "pointerMemberType: outputTypeEvaluation           = %s \n",
              outputTypeEvaluation ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "pointerMemberType: outputGlobalQualification      = %s \n",
              outputGlobalQualification ? "true" : "false");

  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "qualifier = %s \n",
              qualifier.c_str());
#endif

  if (qualifiedNameMapForTypes.find(pointerMemberType) ==
      qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for (SgPointerMemberType) name = %s into "
                "list at IR node = %p = %s \n",
                qualifier.c_str(), pointerMemberType,
                pointerMemberType->class_name().c_str());
#endif
    // qualifiedNameMapForNames.insert(std::pair<SgNode*,std::string>(pointerMemberType,qualifier));
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(pointerMemberType, qualifier));

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    // MLOG_WARN_C(MLOG_UNPARSER, "Testing name in map: for SgPointerMemberType
    // = %p qualified name = %s
    // \n",pointerMemberType,pointerMemberType->get_qualified_name_prefix().str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Testing name in map: for SgPointerMemberType = %p qualified name = %s "
        "\n",
        pointerMemberType,
        pointerMemberType->get_qualified_name_prefix_for_base_type().str());
    // MLOG_WARN_C(MLOG_UNPARSER,
    // "SgNode::get_globalQualifiedNameMapForNames().size() = %" PRIuPTR "
    // \n",SgNode::get_globalQualifiedNameMapForNames().size());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "SgNode::get_globalQualifiedNameMapForTypes().size() = %" PRIuPTR " \n",
        SgNode::get_globalQualifiedNameMapForTypes().size());
#endif
  } else {
    // If it already existes then overwrite the existing information.
    // std::map<SgNode*,std::string>::iterator i =
    // qualifiedNameMapForNames.find(pointerMemberType); ROSE_ASSERT (i !=
    // qualifiedNameMapForNames.end());
    NameQualificationMapType::iterator i =
        qualifiedNameMapForTypes.find(pointerMemberType);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 3: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    if (i->second != qualifier) {
      // DQ (3/15/2019): We need to disable the assertion below because it can
      // happen (see Cxx11_tests/test2019_214.C).
      i->second = qualifier;
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgVarRefExp *varRefExp, SgVariableDeclaration *variableDeclaration,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (7/31/2012): check if this is a SgVarRefExp that is associated with a
  // class that is un-named, if so then supress the name qualification (which
  // would use the internally generated name).  Note that all constructs that
  // are un-named have names generated internally for them so that we can
  // support the AST merge process and generally reference multiple un-named
  // constructs that may exist in a single compilation unit. SgClassDeclaration*
  // classDeclaration = isSgClassDeclaration(varRefExp->parent());

  ASSERT_not_null(varRefExp);
  SgBinaryOp *dotExp = isSgDotExp(varRefExp->get_parent());
  SgBinaryOp *arrowExp = isSgArrowExp(varRefExp->get_parent());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgVarRefExp*) \n");
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "dotExp = %p arrowExp = %p \n", dotExp, arrowExp);
#endif

  SgVarRefExp *possibleClassVarRefExp = NULL;
  if (dotExp != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "Note that this code is overly sensitive to the "
                               "local structure of the AST expressions \n");
#endif
    possibleClassVarRefExp = isSgVarRefExp(dotExp->get_lhs_operand());

    if (possibleClassVarRefExp == NULL) {
      SgPntrArrRefExp *possiblePntrArrRefExp =
          isSgPntrArrRefExp(dotExp->get_lhs_operand());
      if (possiblePntrArrRefExp != NULL) {
        possibleClassVarRefExp =
            isSgVarRefExp(possiblePntrArrRefExp->get_lhs_operand());
      } else {
      }
    }
  }

  if (arrowExp != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "Note that this code is overly sensitive to the "
                               "local structure of the AST expressions \n");
#endif
    possibleClassVarRefExp = isSgVarRefExp(arrowExp->get_lhs_operand());

    if (possibleClassVarRefExp == NULL) {
      SgPntrArrRefExp *possiblePntrArrRefExp =
          isSgPntrArrRefExp(arrowExp->get_lhs_operand());
      if (possiblePntrArrRefExp != NULL) {
        possibleClassVarRefExp =
            isSgVarRefExp(possiblePntrArrRefExp->get_lhs_operand());
      }
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "possibleClassVarRefExp = %p \n",
              possibleClassVarRefExp);
#endif

  SgClassType *classType = NULL;
  if (possibleClassVarRefExp != NULL) {
    SgType *varRefExpType = possibleClassVarRefExp->get_type();
    ASSERT_not_null(varRefExpType);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExpType = %p = %s \n",
                varRefExpType, varRefExpType->class_name().c_str());
#endif

    // DQ (8/2/2012): test2007_06.C and test2012_156.C show that we need to
    // strip past the typedefs. Note that we don't want to strip typedefs, since
    // that could take us past public types and into private types. SgType*
    // possibleClassType =
    // varRefExpType->stripType(SgType::STRIP_MODIFIER_TYPE|SgType::STRIP_REFERENCE_TYPE|SgType::STRIP_POINTER_TYPE|SgType::STRIP_ARRAY_TYPE);
    // // Excluding SgType::STRIP_TYPEDEF_TYPE
    SgType *possibleClassType = varRefExpType->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE | SgType::STRIP_TYPEDEF_TYPE);
    classType = isSgClassType(possibleClassType);
  }

  bool isAnUnamedConstructs = false;
  if (classType != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "classType = %p = %s \n",
                classType, classType->class_name().c_str());
#endif
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(classType->get_declaration());
    ASSERT_not_null(classDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "classDeclaration = %p = %s \n",
                classDeclaration, classDeclaration->class_name().c_str());
#endif

    // DQ (9/4/2012): I don't think that the defining declaration should have to
    // exist. However this was a previously passing test for all of the
    // regression tests.
    // ASSERT_not_null(classDeclaration->get_definingDeclaration());
    if (classDeclaration->get_definingDeclaration() != NULL) {
      SgClassDeclaration *definingClassDeclaration =
          isSgClassDeclaration(classDeclaration->get_definingDeclaration());
      if (definingClassDeclaration == NULL) {
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "ERROR: definingClassDeclaration == NULL: "
            "classDeclaration->get_definingDeclaration() = %p = %s \n",
            classDeclaration->get_definingDeclaration(),
            classDeclaration->get_definingDeclaration()->class_name().c_str());
      }
      ASSERT_not_null(definingClassDeclaration);

      // This should be true so assert this here.
      ROSE_ASSERT(classDeclaration->get_isUnNamed() ==
                  definingClassDeclaration->get_isUnNamed());
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "WARNING: classDeclaration->get_definingDeclaration() == NULL: This "
          "was a previously passing test, but not now that we have force "
          "SgTemplateTypes to be handled in the local type table. \n");
#endif
    }

    if (classDeclaration->get_isUnNamed() == true) {
      isAnUnamedConstructs = true;
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "isAnUnamedConstructs = %s \n",
              isAnUnamedConstructs ? "true" : "false");
#endif

  // DQ (7/31/2012): If this is an un-named construct then no qualifiaction can
  // be used since there is no associated name.
  if (isAnUnamedConstructs == false) {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(variableDeclaration);
    int effectiveNameQualification = amountOfNameQualificationRequired;
    getExplicitQualifierLength(varRefExp, effectiveNameQualification);
    string qualifier = setNameQualificationSupport(
        scope, effectiveNameQualification, outputNameQualificationLength,
        outputGlobalQualification, outputTypeEvaluation);
    applyExplicitQualifier(varRefExp, qualifier, outputNameQualificationLength,
                           outputGlobalQualification);

    varRefExp->set_global_qualification_required(outputGlobalQualification);
    varRefExp->set_name_qualification_length(outputNameQualificationLength);

    // There should be no type evaluation required for a variable reference, as
    // I recall.
    ROSE_ASSERT(outputTypeEvaluation == false);
    varRefExp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExp->get_name_qualification_length()     = %d \n",
                varRefExp->get_name_qualification_length());
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExp->get_type_elaboration_required()     = %s \n",
                varRefExp->get_type_elaboration_required() ? "true" : "false");
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExp->get_global_qualification_required() = %s \n",
                varRefExp->get_global_qualification_required() ? "true"
                                                               : "false");
#endif

    if (qualifiedNameMapForNames.find(varRefExp) ==
        qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
          qualifier.c_str(), varRefExp, varRefExp->class_name().c_str());
#endif
      qualifiedNameMapForNames.insert(
          std::pair<SgNode *, std::string>(varRefExp, qualifier));
    } else {
      // DQ (6/20/2011): We see this case in test2011_87.C.
      // If it already existes then overwrite the existing information.
      NameQualificationMapType::iterator i =
          qualifiedNameMapForNames.find(varRefExp);
      ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      string previousQualifier = i->second.c_str();
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: test 2: replacing previousQualifier = %s with new "
                  "qualifier = %s \n",
                  previousQualifier.c_str(), qualifier.c_str());
#endif
      if (i->second != qualifier) {
        // DQ (7/23/2011): Multiple uses of the SgVarRefExp expression in
        // SgArrayType will cause the name qualification to be reset each time.
        // This is OK since it is used to build the type name that will be
        // saved.
        i->second = qualifier;
      }
    }
  } else {
    SgScopeStatement *scope =
        traverseNonrealDeclForCorrectScope(variableDeclaration);
    string qualifier = setNameQualificationSupport(
        scope, amountOfNameQualificationRequired, outputNameQualificationLength,
        outputGlobalQualification, outputTypeEvaluation);
    applyExplicitQualifier(varRefExp, qualifier, outputNameQualificationLength,
                           outputGlobalQualification);

    varRefExp->set_global_qualification_required(outputGlobalQualification);
    varRefExp->set_name_qualification_length(outputNameQualificationLength);

    // There should be no type evaluation required for a variable reference, as
    // I recall.
    ROSE_ASSERT(outputTypeEvaluation == false);
    varRefExp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExp->get_name_qualification_length()     = %d \n",
                varRefExp->get_name_qualification_length());
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExp->get_type_elaboration_required()     = %s \n",
                varRefExp->get_type_elaboration_required() ? "true" : "false");
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "varRefExp->get_global_qualification_required() = %s \n",
                varRefExp->get_global_qualification_required() ? "true"
                                                               : "false");
#endif

    if (qualifiedNameMapForNames.find(varRefExp) ==
        qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
          qualifier.c_str(), varRefExp, varRefExp->class_name().c_str());
#endif
      qualifiedNameMapForNames.insert(
          std::pair<SgNode *, std::string>(varRefExp, qualifier));
    } else {
      // DQ (6/20/2011): We see this case in test2011_87.C.
      // If it already existes then overwrite the existing information.
      NameQualificationMapType::iterator i =
          qualifiedNameMapForNames.find(varRefExp);
      ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      string previousQualifier = i->second.c_str();
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: test 2: replacing previousQualifier = %s with new "
                  "qualifier = %s \n",
                  previousQualifier.c_str(), qualifier.c_str());
#endif
      if (i->second != qualifier) {
        // DQ (7/23/2011): Multiple uses of the SgVarRefExp expression in
        // SgArrayType will cause the name qualification to be reset each time.
        // This is OK since it is used to build the type name that will be
        // saved.
        i->second = qualifier;
      }
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgFunctionRefExp *functionRefExp,
    SgFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgFunctionRefExp*) \n");
#endif

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(functionDeclaration);
  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(functionRefExp, effectiveNameQualification);
  string qualifier = setNameQualificationSupport(
      scope, effectiveNameQualification, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  const bool has_explicit_ref_qualifier = applyExplicitQualifier(
      functionRefExp, qualifier, outputNameQualificationLength,
      outputGlobalQualification);

  functionRefExp->set_global_qualification_required(outputGlobalQualification);
  functionRefExp->set_name_qualification_length(outputNameQualificationLength);

  // There should be no type evaluation required for a function reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);
  functionRefExp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "functionRefExp->get_name_qualification_length()     = %d \n",
              functionRefExp->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "functionRefExp->get_type_elaboration_required()     = %s \n",
              functionRefExp->get_type_elaboration_required() ? "true"
                                                              : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "functionRefExp->get_global_qualification_required() = %s \n",
              functionRefExp->get_global_qualification_required() ? "true"
                                                                  : "false");
#endif

  // DQ (5/2/2012): I don't think that global qualification is allowed for
  // friend functions (so test for this). test2012_59.C is an example of this
  // issue.
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_declarationModifier().isFriend() = %s \n",
      functionDeclaration->get_declarationModifier().isFriend() ? "true"
                                                                : "false");
  if (functionDeclaration->get_firstNondefiningDeclaration() != NULL)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "functionDeclaration->get_firstNondefiningDeclaration()->get_"
                "declarationModifier().isFriend() = %s \n",
                functionDeclaration->get_firstNondefiningDeclaration()
                        ->get_declarationModifier()
                        .isFriend()
                    ? "true"
                    : "false");
  if (functionDeclaration->get_definingDeclaration() != NULL)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualification(): "
                "functionDeclaration->get_definingDeclaration()->get_"
                "declarationModifier().isFriend()         = %s \n",
                functionDeclaration->get_definingDeclaration()
                        ->get_declarationModifier()
                        .isFriend()
                    ? "true"
                    : "false");
#endif

  // Look for friend declaration on both declaration (defining and
  // non-defining).
  bool isFriend = false;
  if (functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
    isFriend =
        isFriend || functionDeclaration->get_firstNondefiningDeclaration()
                        ->get_declarationModifier()
                        .isFriend();
  }
  if (functionDeclaration->get_definingDeclaration() != NULL) {
    isFriend = isFriend || functionDeclaration->get_definingDeclaration()
                               ->get_declarationModifier()
                               .isFriend();
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "isFriend                                       = %s \n",
              isFriend ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "functionDeclaration->get_definingDeclaration() = %p \n",
              functionDeclaration->get_definingDeclaration());
#endif

  // DQ (4/2/2014): After discusion with Markus, this is a problem that is a
  // significant issue and requires a general solution that would be useful more
  // generally than just to this specific problem.  We need to build a data
  // stucture and hide it behind a class with an appropriate API.  The data
  // structure should have a container of non-defining declarations for each
  // first-non-defining declaration.  Thus we would have a way to find all of
  // the non-defining declarations associated with any function and thus query
  // if one of them was declard in a scope that defined its scope definatively.
  // This class should be a part of an AST Information sort of object that we
  // would use to collect similar analysis information that would be separate
  // from the AST, but might be used with the AST for certain purposes (e.g.
  // removning all functions including all associated non-defining
  // declarations).

  // DQ (4/6/2014): Adding support for new analysis results. Fails for
  // test2013_242.C.
  ASSERT_not_null(declarationSet);
  // ROSE_ASSERT(declarationSet->getDeclarationMap().size() != 0);

  SgClassDefinition *friend_class_def = NULL;
  if (isFriend) {
    friend_class_def = isSgClassDefinition(functionDeclaration->get_parent());
    if (friend_class_def == NULL &&
        functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_firstNondefiningDeclaration()->get_parent());
    }
    if (friend_class_def == NULL &&
        functionDeclaration->get_definingDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_definingDeclaration()->get_parent());
    }
  }

  auto has_explicit_global_friend_qualification =
      [](SgFunctionDeclaration *decl) {
        return decl != NULL && decl->get_global_qualification_required() &&
               decl->get_name_qualification_length() > 0;
      };

  bool explicit_global_friend_decl =
      has_explicit_global_friend_qualification(functionDeclaration) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_firstNondefiningDeclaration())) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_definingDeclaration()));

  if (isFriend && friend_class_def != NULL) {
    if (!explicit_global_friend_decl && !has_explicit_ref_qualifier &&
        is_hidden_friend_free_function_decl(functionDeclaration)) {
      // Hidden friends are only reachable through ADL unless a real
      // namespace/global redeclaration exists. Adding a namespace qualifier to
      // such calls turns valid source into invalid qualified lookup.
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    } else if (!explicit_global_friend_decl) {
      if (amountOfNameQualificationRequired == 0) {
        outputNameQualificationLength = 0;
        outputGlobalQualification = false;
        qualifier = "";
      }
    } else {
      SgScopeStatement *friend_enclosing_scope = friend_class_def->get_scope();
      SgScopeStatement *friend_decl_scope = functionDeclaration->get_scope();
      if (friend_enclosing_scope != NULL && friend_decl_scope != NULL &&
          !SgScopeStatement::isEquivalentScope(friend_enclosing_scope,
                                               friend_decl_scope)) {
        if (outputNameQualificationLength < 1) {
          outputNameQualificationLength = 1;
        }
        outputGlobalQualification = (isSgGlobal(friend_decl_scope) != NULL);
        if (outputGlobalQualification) {
          qualifier = "::";
        }
      } else {
        outputNameQualificationLength = 0;
        outputGlobalQualification = false;
        qualifier = "";
      }
    }

    functionRefExp->set_global_qualification_required(
        outputGlobalQualification);
    functionRefExp->set_name_qualification_length(
        outputNameQualificationLength);
  }

  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualification(): qualifier = %s
  // \n",qualifier.c_str());

  if (qualifiedNameMapForNames.find(functionRefExp) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for (SgFunctionRefExp) name = %s into "
                "list at IR node = %p = %s \n",
                qualifier.c_str(), functionRefExp,
                functionRefExp->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(functionRefExp, qualifier));

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Testing name in map: for SgFunctionRefExp = %p qualified name = %s \n",
        functionRefExp, functionRefExp->get_qualified_name_prefix().str());
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "SgNode::get_globalQualifiedNameMapForNames().size() = %" PRIuPTR " \n",
        SgNode::get_globalQualifiedNameMapForNames().size());
#endif
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(functionRefExp);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 3: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    if (i->second != qualifier) {
      // DQ (3/15/2019): We need to disable the assertion below because it can
      // happen (see Cxx11_tests/test2019_214.C).
      i->second = qualifier;
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgMemberFunctionRefExp *functionRefExp,
    SgMemberFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgMemberFunctionRefExp*) \n");
#endif

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(functionDeclaration);
  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(functionRefExp, effectiveNameQualification);
  string qualifier = setNameQualificationSupport(
      scope, effectiveNameQualification, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  applyExplicitQualifier(functionRefExp, qualifier,
                         outputNameQualificationLength,
                         outputGlobalQualification);
  functionRefExp->set_global_qualification_required(outputGlobalQualification);
  functionRefExp->set_name_qualification_length(outputNameQualificationLength);

  // There should be no type evaluation required for a function reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);
  functionRefExp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "memberFunctionRefExp->get_name_qualification_length()     = %d \n",
      functionRefExp->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "memberFunctionRefExp->get_type_elaboration_required()     = %s \n",
      functionRefExp->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "memberFunctionRefExp->get_global_qualification_required() = %s \n",
      functionRefExp->get_global_qualification_required() ? "true" : "false");
#endif
  if (qualifiedNameMapForNames.find(functionRefExp) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting (memberFunction) qualifier for name = %s into list "
                "at IR node = %p = %s \n",
                qualifier.c_str(), functionRefExp,
                functionRefExp->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(functionRefExp, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(functionRefExp);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 4: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    if (i->second != qualifier) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 1
      MLOG_WARN_C(MLOG_UNPARSER,
                  "NOTE: test 4: replacing previousQualifier = %s with new "
                  "qualifier = %s \n",
                  i->second.c_str(), qualifier.c_str());
#endif
      i->second = qualifier;
      MLOG_WARN_C(MLOG_UNPARSER,
                  " --- Name qualificaiton was previously and error: we may "
                  "need to set it to something different: qualifier = %s \n",
                  qualifier.c_str());
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgPseudoDestructorRefExp *pseudoDestructorRefExp,
    SgDeclarationStatement *declarationStatement,
    int amountOfNameQualificationRequired) {
  // This is where we hide the details of translating the intepretation of the
  // amountOfNameQualificationRequired which can be greater than the number of
  // nested scopes to a representation that is bounded by the number of nested
  // scopes and sets the global qualification to be true. If I decide I don't
  // like this here, then we might find a way to handling this point more
  // directly later. This at least gets it set properly in the AST.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgPseudoDestructorRefExp*) \n");
#endif

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(declarationStatement);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  pseudoDestructorRefExp->set_global_qualification_required(
      outputGlobalQualification);
  pseudoDestructorRefExp->set_name_qualification_length(
      outputNameQualificationLength);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);
  pseudoDestructorRefExp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "pseudoDestructorRefExp->get_name_qualification_length()     = %d \n",
      pseudoDestructorRefExp->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "pseudoDestructorRefExp->get_type_elaboration_required()     = %s \n",
      pseudoDestructorRefExp->get_type_elaboration_required() ? "true"
                                                              : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "pseudoDestructorRefExp->get_global_qualification_required() = %s \n",
      pseudoDestructorRefExp->get_global_qualification_required() ? "true"
                                                                  : "false");
#endif
  if (qualifiedNameMapForNames.find(pseudoDestructorRefExp) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting (pseudoDestructorRefExp) qualifier for name = %s "
                "into list at IR node = %p = %s \n",
                qualifier.c_str(), pseudoDestructorRefExp,
                pseudoDestructorRefExp->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(pseudoDestructorRefExp, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(pseudoDestructorRefExp);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 4.5: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    if (i->second != qualifier) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "NOTE: test 4.5: replacing previousQualifier = %s with new "
                  "qualifier = %s \n",
                  i->second.c_str(), qualifier.c_str());
#endif
      i->second = qualifier;
      MLOG_WARN_C(MLOG_UNPARSER,
                  " --- Name qualificaiton was previously and error: we may "
                  "need to set it to something different: qualifier = %s \n",
                  qualifier.c_str());
    }
  }
}

// DQ (6/4/2011): This function handles a specific case that is demonstrated by
// test2005_42.C. DQ (6/1/2011): Added support for qualification of the
// SgConstructorInitializer. void
// NameQualificationTraversal::setNameQualification(SgConstructorInitializer*
// constructorInitializer, SgMemberFunctionDeclaration* functionDeclaration, int
// amountOfNameQualificationRequired)
void NameQualificationTraversal::setNameQualification(
    SgConstructorInitializer *constructorInitializer,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // DQ (6/4/2011): This handles the case of both the declaration being a
  // SgMemberFunctionDeclaration and a SgClassDeclaration.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgConstructorInitializer*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  constructorInitializer->set_global_qualification_required(
      outputGlobalQualification);
  constructorInitializer->set_name_qualification_length(
      outputNameQualificationLength);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);
  constructorInitializer->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "constructorInitializer->get_name_qualification_length()     = %d \n",
      constructorInitializer->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "constructorInitializer->get_type_elaboration_required()     = %s \n",
      constructorInitializer->get_type_elaboration_required() ? "true"
                                                              : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "constructorInitializer->get_global_qualification_required() = %s \n",
      constructorInitializer->get_global_qualification_required() ? "true"
                                                                  : "false");
#endif

  if (qualifiedNameMapForNames.find(constructorInitializer) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), constructorInitializer,
        constructorInitializer->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(constructorInitializer, qualifier));
  } else {
    // DQ (2/12/2012): Fixing support where the name qualification must be
    // rewritten where it is used in a different context. this appears to
    // be a common requirement.  This case appears to not have been a
    // problem before but is now with the new legacy frontend 4.3 support.
    // This has been added because of the requirements of that support.

    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(constructorInitializer);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 5: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second.empty() || qualifier.size() > i->second.size()) {
      MLOG_WARN_C(MLOG_UNPARSER, "Error: name in qualifiedNameMapForNames "
                                 "already exists and is different...\n");
      MLOG_WARN_C(MLOG_UNPARSER, ">>>> %s\n", i->second.c_str());
      MLOG_WARN_C(MLOG_UNPARSER, ">>>> %s\n", qualifier.c_str());
      i->second = qualifier;
    }
  }

  // DQ (6/4/2011): Added test...
  ROSE_ASSERT(SgNode::get_globalQualifiedNameMapForNames().find(
                  constructorInitializer) !=
              SgNode::get_globalQualifiedNameMapForNames().end());
}

void NameQualificationTraversal::setNameQualification(
    SgEnumVal *enumVal, SgEnumDeclaration *enumDeclaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgEnumVal*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(enumDeclaration);
  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(enumVal, effectiveNameQualification);
  string qualifier = setNameQualificationSupport(
      scope, effectiveNameQualification, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  applyExplicitQualifier(enumVal, qualifier, outputNameQualificationLength,
                         outputGlobalQualification);

  enumVal->set_global_qualification_required(outputGlobalQualification);
  enumVal->set_name_qualification_length(outputNameQualificationLength);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);
  enumVal->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "enumVal->get_name_qualification_length()     = %d \n",
              enumVal->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "enumVal->get_type_elaboration_required()     = %s \n",
              enumVal->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "enumVal->get_global_qualification_required() = %s \n",
              enumVal->get_global_qualification_required() ? "true" : "false");
#endif
  if (qualifiedNameMapForNames.find(enumVal) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), enumVal, enumVal->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(enumVal, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(enumVal);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test6: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      // DQ (5/3/2013): Note that this happens for test2013_144.C where the
      // enumValue is used as the size in the array type (and the SgArrayType is
      // shared). See comments in the test code for how this might be improved
      // (forcing name qualification).
      i->second = qualifier;
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgBaseClass *baseClass, SgClassDeclaration *classDeclaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgBaseClass*) \n");
#endif

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(classDeclaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  baseClass->set_global_qualification_required(outputGlobalQualification);
  baseClass->set_name_qualification_length(outputNameQualificationLength);
  baseClass->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "baseClass->get_name_qualification_length()     = %d \n",
              baseClass->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "baseClass->get_type_elaboration_required()     = %s \n",
              baseClass->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "baseClass->get_global_qualification_required() = %s \n",
              baseClass->get_global_qualification_required() ? "true"
                                                             : "false");
#endif

  if (qualifiedNameMapForNames.find(baseClass) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), baseClass, baseClass->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(baseClass, qualifier));
  } else {
    // DQ (6/17/2013): I think it is reasonable that this might have been
    // previously set and we have to overwrite the last value as we handle it
    // again in a different context.

    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(baseClass);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 7: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;
      // DQ (6/17/2013): Commented out this assertion.
      MLOG_WARN_C(MLOG_UNPARSER, "Error: name in qualifiedNameMapForNames "
                                 "already exists and is different... \n");
      ROSE_ABORT();
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgFunctionDeclaration *functionDeclaration,
    int amountOfNameQualificationRequired) {
  // This takes only a SgFunctionDeclaration since it is where we locate the
  // name qualification information AND is the correct scope from which to
  // iterate backwards through scopes to evaluate what name qualification is
  // required.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // MLOG_WARN_C(MLOG_UNPARSER,
  // "\n************************************************ \n");

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgFunctionDeclaration*): "
              "functionDeclaration = %p \n",
              functionDeclaration);
  MLOG_WARN_C(MLOG_UNPARSER, " --- functionDeclaration->get_name()   = %s \n",
              functionDeclaration->get_name().str());
  MLOG_WARN_C(MLOG_UNPARSER, " --- amountOfNameQualificationRequired = %d \n",
              amountOfNameQualificationRequired);
  MLOG_WARN_C(MLOG_UNPARSER,
              " --- functionDeclaration->get_definingDeclaration() = %p \n",
              functionDeclaration->get_definingDeclaration());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      " --- functionDeclaration->get_firstNondefiningDeclaration() = %p \n",
      functionDeclaration->get_firstNondefiningDeclaration());
#endif

  const int existingNameQualificationLength =
      functionDeclaration->get_name_qualification_length();
  const bool existingGlobalQualification =
      functionDeclaration->get_global_qualification_required();
  SgUnorderedMapNodeToString::iterator existingQualifier =
      qualifiedNameMapForNames.find(functionDeclaration);
  const bool preserveExplicitSourceQualifier =
      existingQualifier != qualifiedNameMapForNames.end() &&
      !existingQualifier->second.empty() &&
      (existingNameQualificationLength > 0 || existingGlobalQualification) &&
      declaration_has_real_visible_source(functionDeclaration);
  const std::string explicitSourceQualifier =
      preserveExplicitSourceQualifier ? existingQualifier->second : "";

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(functionDeclaration);
  ROSE_ASSERT(scope != NULL);

  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  SgScopeStatement *parent_scope =
      isSgScopeStatement(functionDeclaration->get_parent());
  auto same_logical_namespace_scope = [](SgScopeStatement *lhs,
                                         SgScopeStatement *rhs) -> bool {
    if (lhs == NULL || rhs == NULL) {
      return false;
    }
    if (lhs == rhs || SgScopeStatement::isEquivalentScope(lhs, rhs)) {
      return true;
    }
    if (isSgGlobal(lhs) != NULL && isSgGlobal(rhs) != NULL) {
      return true;
    }

    SgNamespaceDefinitionStatement *lhs_ns =
        isSgNamespaceDefinitionStatement(lhs);
    SgNamespaceDefinitionStatement *rhs_ns =
        isSgNamespaceDefinitionStatement(rhs);
    if (lhs_ns == NULL || rhs_ns == NULL) {
      return false;
    }

    SgNamespaceDeclarationStatement *lhs_decl =
        lhs_ns->get_namespaceDeclaration();
    SgNamespaceDeclarationStatement *rhs_decl =
        rhs_ns->get_namespaceDeclaration();
    if (lhs_decl == NULL || rhs_decl == NULL) {
      return false;
    }

    SgDeclarationStatement *lhs_first =
        lhs_decl->get_firstNondefiningDeclaration();
    if (lhs_first == NULL) {
      lhs_first = lhs_decl;
    }
    SgDeclarationStatement *rhs_first =
        rhs_decl->get_firstNondefiningDeclaration();
    if (rhs_first == NULL) {
      rhs_first = rhs_decl;
    }
    return lhs_first == rhs_first;
  };
  const bool lexically_in_same_file_scope =
      parent_scope != NULL &&
      (isSgNamespaceDefinitionStatement(parent_scope) != NULL ||
       isSgGlobal(parent_scope) != NULL) &&
      same_logical_namespace_scope(parent_scope, scope);
  if (lexically_in_same_file_scope) {
    qualifier = "";
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgFunctionDeclaration*): scope = %p "
              "qualifier = %s \n",
              scope, qualifier.c_str());
  MLOG_WARN_C(MLOG_UNPARSER, " --- outputGlobalQualification         = %s \n",
              outputGlobalQualification ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER, " --- outputNameQualificationLength     = %d \n",
              outputNameQualificationLength);
  MLOG_WARN_C(MLOG_UNPARSER, " --- amountOfNameQualificationRequired = %d \n",
              amountOfNameQualificationRequired);
#endif

  // DQ (9/7/2014): Added suppor for where this is a template member or
  // non-member function declaration and we need to genrate the name with the
  // associated template header.
  string template_header;
  SgTemplateFunctionDeclaration *templateFunctionDeclaration =
      isSgTemplateFunctionDeclaration(functionDeclaration);
  SgTemplateMemberFunctionDeclaration *templateMemberFunctionDeclaration =
      isSgTemplateMemberFunctionDeclaration(functionDeclaration);
  bool buildTemplateHeaderString = (templateFunctionDeclaration != NULL ||
                                    templateMemberFunctionDeclaration != NULL);
  if (buildTemplateHeaderString == true) {
    // DQ (9/7/2014): First idea, but not likely to work...and too complex.
    // Note that another aspect of this implementation might be that we save a
    // set of template class declarations so that we can match types in the
    // function's parameter list against the template class declaration set so
    // that we know when to build function parameter types as template types vs.
    // template instantiation types. This would require that we save a more
    // complex data structure than a simple string.  It is also not clear if all
    // references to a template class instantiation could be assumed to be
    // references to it's template declaration? Or maybe the problem is that
    // there is some other function parameter lis that we need to consult.

    // DQ (9/7/2014): Better:
    // A better solution would be to make sure that we generate type in the
    // legacy frontend/ROSE translation using the template function's
    // paramter list associated with the first non-defining declaration
    // (instead of the one being generated as part of building the defining
    // declaration (which is using the same a_routine_ptr as that used to
    // build the template instantiation.  As a result we a mixing the types
    // in the defining template declaration with that of the defining
    // template instantiation (which is always wrong).  So the simple
    // solution is to just use the types from the non-defining template
    // member or non-member function declaration.  The same should apply to
    // the function return type.  This is the simplest solution to date.

    // DQ (9/8/2014): The best solution was to translate the defining
    // non-template function declarations when we saw them as defining
    // declarations, but only put the non-defining declaration into the class
    // template (to match the normalization done by legacy frontend) and then
    // attach the defining template declaration ahead of the first associated
    // template instantiation. This appears to work well and will soon be
    // evaluated for further tests.
    template_header = setTemplateHeaderNameQualificationSupport(
        functionDeclaration->get_scope(), amountOfNameQualificationRequired);
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_declarationModifier().isFriend() = %s \n",
      functionDeclaration->get_declarationModifier().isFriend() ? "true"
                                                                : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "outputNameQualificationLength                             = %d \n",
      outputNameQualificationLength);
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "outputGlobalQualification                                 = %s \n",
      outputGlobalQualification ? "true" : "false");
#endif

  // DQ (2/18/2024): Note that there may not be a nondefining declaration,
  // and then we still don't want to output the global name qualification.
  // Global/namespace-scope function declarations should never emit a leading
  // "::".
  if (outputGlobalQualification == true) {
    SgScopeStatement *decl_parent =
        isSgScopeStatement(functionDeclaration->get_parent());
    if (decl_parent != NULL &&
        (isSgGlobal(decl_parent) != NULL ||
         isSgNamespaceDefinitionStatement(decl_parent) != NULL)) {
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    }
  }
  // DQ (2/16/2013): Note that test2013_67.C is a case where name qualification
  // of the friend function is required. I think it is because it is a non
  // defining declaration instead of a defining declaration. DQ (3/31/2012): I
  // don't think that global qualification is allowed for friend functions (so
  // test for this). test2012_57.C is an example of this issue. if
  // (outputGlobalQualification == true &&
  // functionDeclaration->get_declarationModifier().isFriend() == true) if (
  // (outputGlobalQualification == true) &&
  // (functionDeclaration->get_declarationModifier().isFriend() == true) &&
  // (functionDeclaration == functionDeclaration->get_definingDeclaration()))
  if ((outputGlobalQualification == true) &&
      (functionDeclaration->get_declarationModifier().isFriend() == true) &&
      ((functionDeclaration->get_definingDeclaration() == NULL) ||
       (functionDeclaration ==
        functionDeclaration->get_definingDeclaration()))) {
    SgScopeStatement *decl_parent =
        isSgScopeStatement(functionDeclaration->get_parent());
    bool allow_friend_global = true;
    if (decl_parent != NULL &&
        (isSgGlobal(decl_parent) != NULL ||
         isSgNamespaceDefinitionStatement(decl_parent) != NULL)) {
      allow_friend_global = false;
    }
    if (allow_friend_global == false) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: We can't specify global qualification of friend "
                  "function (qualifier reset to be empty string) \n");
#endif
      // Note that I think this might only be an issue where
      // outputNameQualificationLength == 0.
      ROSE_ASSERT(outputNameQualificationLength == 0);

      // Reset the values (and the qualifier string).
      // outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    }
  }

  bool isFriendDecl = functionDeclaration->get_declarationModifier().isFriend();
  if (functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
    isFriendDecl =
        isFriendDecl || functionDeclaration->get_firstNondefiningDeclaration()
                            ->get_declarationModifier()
                            .isFriend();
  }
  if (functionDeclaration->get_definingDeclaration() != NULL) {
    isFriendDecl =
        isFriendDecl || functionDeclaration->get_definingDeclaration()
                            ->get_declarationModifier()
                            .isFriend();
  }

  SgClassDefinition *friend_class_def = NULL;
  if (isFriendDecl) {
    friend_class_def = isSgClassDefinition(functionDeclaration->get_parent());
    if (friend_class_def == NULL &&
        functionDeclaration->get_firstNondefiningDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_firstNondefiningDeclaration()->get_parent());
    }
    if (friend_class_def == NULL &&
        functionDeclaration->get_definingDeclaration() != NULL) {
      friend_class_def = isSgClassDefinition(
          functionDeclaration->get_definingDeclaration()->get_parent());
    }
  }

  auto has_explicit_global_friend_qualification =
      [](SgFunctionDeclaration *decl) {
        return decl != NULL && decl->get_global_qualification_required() &&
               decl->get_name_qualification_length() > 0;
      };

  bool explicit_global_friend_decl =
      has_explicit_global_friend_qualification(functionDeclaration) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_firstNondefiningDeclaration())) ||
      has_explicit_global_friend_qualification(isSgFunctionDeclaration(
          functionDeclaration->get_definingDeclaration()));

  if (isFriendDecl && friend_class_def != NULL) {
    SgScopeStatement *friend_enclosing_scope = friend_class_def->get_scope();
    SgScopeStatement *friend_decl_scope = functionDeclaration->get_scope();

    SgFunctionDeclaration *friend_first_nondef = isSgFunctionDeclaration(
        functionDeclaration->get_firstNondefiningDeclaration());
    SgScopeStatement *friend_first_scope =
        friend_first_nondef != NULL ? friend_first_nondef->get_scope() : NULL;

    bool friend_targets_global_scope =
        (friend_decl_scope != NULL && isSgGlobal(friend_decl_scope) != NULL) ||
        (friend_first_scope != NULL && isSgGlobal(friend_first_scope) != NULL);

    bool requires_global_friend_qualification =
        friend_enclosing_scope != NULL &&
        isSgGlobal(friend_enclosing_scope) == NULL &&
        friend_targets_global_scope;

    auto is_class_like_scope = [](SgScopeStatement *scope) {
      return isSgClassDefinition(scope) != NULL ||
             isSgTemplateClassDefinition(scope) != NULL;
    };

    bool preserve_member_friend_qualification =
        isSgMemberFunctionDeclaration(functionDeclaration) != NULL &&
        friend_decl_scope != NULL && is_class_like_scope(friend_decl_scope) &&
        !SgScopeStatement::isEquivalentScope(friend_class_def,
                                             friend_decl_scope);

    if (requires_global_friend_qualification) {
      if (outputNameQualificationLength < 1) {
        outputNameQualificationLength = 1;
      }
      outputGlobalQualification = true;
      qualifier = "::";
    } else if (!explicit_global_friend_decl &&
               !preserve_member_friend_qualification) {
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier = "";
    } else {
      if (friend_enclosing_scope != NULL && friend_decl_scope != NULL &&
          SgScopeStatement::isEquivalentScope(friend_enclosing_scope,
                                              friend_decl_scope)) {
        outputNameQualificationLength = 0;
        outputGlobalQualification = false;
        qualifier = "";
      }
    }
  }

  if (preserveExplicitSourceQualifier) {
    outputNameQualificationLength = existingNameQualificationLength;
    outputGlobalQualification = existingGlobalQualification;
    outputTypeEvaluation = false;
    qualifier = explicitSourceQualifier;
  }

  functionDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  functionDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  functionDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_name_qualification_length()     = %d \n",
      functionDeclaration->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_type_elaboration_required()     = %s \n",
      functionDeclaration->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_global_qualification_required() = %s \n",
      functionDeclaration->get_global_qualification_required() ? "true"
                                                               : "false");

  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "functionDeclaration = %p firstNondefiningDeclaration() = %p \n",
              functionDeclaration,
              functionDeclaration->get_firstNondefiningDeclaration());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "functionDeclaration = %p definingDeclaration()         = %p \n",
              functionDeclaration,
              functionDeclaration->get_definingDeclaration());
#endif

  if (qualifiedNameMapForNames.find(functionDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), functionDeclaration,
        functionDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(functionDeclaration, qualifier));
  } else {
    // If it already exists then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(functionDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "WARNING: In NameQualificationTraversal::setNameQualification(): test "
        "8: replacing previousQualifier = %s with new qualifier = %s \n",
        previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      // Later traversals can refine the declaration context, especially for
      // declarations appearing in reopened namespaces or after declaration
      // chain normalization. Keep the cache in sync with the final qualifier.
      i->second = qualifier;
    }
  }

  if (buildTemplateHeaderString == true) {
    // Add the template header string to a new map.

    if (qualifiedNameMapForTemplateHeaders.find(functionDeclaration) ==
        qualifiedNameMapForTemplateHeaders.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Inserting qualifier for template header = %s into list at "
                  "IR node = %p = %s \n",
                  template_header.c_str(), functionDeclaration,
                  functionDeclaration->class_name().c_str());
#endif
      qualifiedNameMapForTemplateHeaders.insert(
          std::pair<SgNode *, std::string>(functionDeclaration,
                                           template_header));
    } else {
      // If it already exists then overwrite the existing information.
      NameQualificationMapType::iterator i =
          qualifiedNameMapForTemplateHeaders.find(functionDeclaration);
      ROSE_ASSERT(i != qualifiedNameMapForTemplateHeaders.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      string previous_template_header = i->second.c_str();
      MLOG_WARN_C(MLOG_UNPARSER,
                  "WARNING: test 9: replacing previousQualifier = %s with new "
                  "qualifier = %s \n",
                  previous_template_header.c_str(), template_header.c_str());
#endif
      if (i->second != template_header) {
        i->second = template_header;
        // DQ (9/7/2014): Make this an error.
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Error: name in qualifiedNameMapForTemplateHeaders already "
                    "exists and is different... \n");
        ROSE_ABORT();
      }
    }
  }

  // MLOG_WARN_C(MLOG_UNPARSER, "****************** DONE ********************
  // \n\n");
}

// void NameQualificationTraversal::setNameQualificationReturnType (
// SgFunctionDeclaration* functionDeclaration, int
// amountOfNameQualificationRequired )
void NameQualificationTraversal::setNameQualificationReturnType(
    SgFunctionDeclaration *functionDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // This takes only a SgFunctionDeclaration since it is where we locate the
  // name qualification information AND is the correct scope from which to
  // iterate backwards through scopes to evaluate what name qualification is
  // required.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualificationReturnType(SgFunctionDeclaration*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  const bool preserve_written_type_elaboration =
      functionDeclaration->get_type_elaboration_required_for_return_type();
  functionDeclaration->set_global_qualification_required_for_return_type(
      outputGlobalQualification);
  functionDeclaration->set_name_qualification_length_for_return_type(
      outputNameQualificationLength);
  functionDeclaration->set_type_elaboration_required_for_return_type(
      outputTypeEvaluation || preserve_written_type_elaboration);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_name_qualification_length_for_return_type()    "
      " = %d \n",
      functionDeclaration->get_name_qualification_length_for_return_type());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_type_elaboration_required_for_return_type()    "
      " = %s \n",
      functionDeclaration->get_type_elaboration_required_for_return_type()
          ? "true"
          : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "functionDeclaration->get_global_qualification_required_for_return_type()"
      " = %s \n",
      functionDeclaration->get_global_qualification_required_for_return_type()
          ? "true"
          : "false");
#endif

  if (qualifiedNameMapForTypes.find(functionDeclaration) ==
      qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for type = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), functionDeclaration,
        functionDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(functionDeclaration, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForTypes.find(functionDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 10: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      // DQ (8/3/2019): Output a message about how we are debugging this.
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Commented out reset of name qualification: replacing "
                  "previousQualifier = %s with new qualifier = %s \n",
                  i->second.c_str(), qualifier.c_str());
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgUsingDeclarationStatement *usingDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In "
                             "setNameQualification(SgUsingDeclarationStatement*"
                             ",SgDeclarationStatement*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  usingDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  usingDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  usingDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDeclaration->get_name_qualification_length()     = %d \n",
              usingDeclaration->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDeclaration->get_type_elaboration_required()     = %s \n",
              usingDeclaration->get_type_elaboration_required() ? "true"
                                                                : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDeclaration->get_global_qualification_required() = %s \n",
              usingDeclaration->get_global_qualification_required() ? "true"
                                                                    : "false");
#endif

  if (qualifiedNameMapForNames.find(usingDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), usingDeclaration,
        usingDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(usingDeclaration, qualifier));
  } else {
    // If it already exists then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(usingDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 11: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;
      MLOG_WARN_C(MLOG_UNPARSER, "Error: name in qualifiedNameMapForNames "
                                 "already exists and is different... \n");
      ROSE_ABORT();
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgUsingDeclarationStatement *usingDeclaration,
    SgInitializedName *associatedInitializedName,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In "
                             "setNameQualification(SgUsingDeclarationStatement*"
                             ",SgInitializedName*) \n");
#endif

  string qualifier = setNameQualificationSupport(
      associatedInitializedName->get_scope(), amountOfNameQualificationRequired,
      outputNameQualificationLength, outputGlobalQualification,
      outputTypeEvaluation);

  usingDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  usingDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  usingDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDeclaration->get_name_qualification_length()     = %d \n",
              usingDeclaration->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDeclaration->get_type_elaboration_required()     = %s \n",
              usingDeclaration->get_type_elaboration_required() ? "true"
                                                                : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDeclaration->get_global_qualification_required() = %s \n",
              usingDeclaration->get_global_qualification_required() ? "true"
                                                                    : "false");
#endif

  if (qualifiedNameMapForNames.find(usingDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), usingDeclaration,
        usingDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(usingDeclaration, qualifier));
  } else {
    MLOG_WARN_C(MLOG_UNPARSER,
                "Error: name in qualifiedNameMapForNames already exists... \n");
    ROSE_ABORT();
  }
}

void NameQualificationTraversal::setNameQualification(
    SgUsingDirectiveStatement *usingDirective,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgUsingDirectiveStatement*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  usingDirective->set_global_qualification_required(outputGlobalQualification);
  usingDirective->set_name_qualification_length(outputNameQualificationLength);
  usingDirective->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDirective->get_name_qualification_length()     = %d \n",
              usingDirective->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDirective->get_type_elaboration_required()     = %s \n",
              usingDirective->get_type_elaboration_required() ? "true"
                                                              : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "usingDirective->get_global_qualification_required() = %s \n",
              usingDirective->get_global_qualification_required() ? "true"
                                                                  : "false");
#endif

  if (qualifiedNameMapForNames.find(usingDirective) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), usingDirective,
        usingDirective->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(usingDirective, qualifier));
  } else {
    ROSE_ASSERT(qualifiedNameMapForNames[usingDirective] == qualifier);
  }
}

// DQ (7/8/2014): Adding support for name qualification of
// SgNamespaceDeclarations within a SgNamespaceAliasDeclarationStatement.
void NameQualificationTraversal::setNameQualification(
    SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In setNameQualification(SgNamespaceAliasDeclarationStatement*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  namespaceAliasDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  namespaceAliasDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  namespaceAliasDeclaration->set_type_elaboration_required(
      outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "namespaceAliasDeclaration->get_name_qualification_length()     = %d \n",
      namespaceAliasDeclaration->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "namespaceAliasDeclaration->get_type_elaboration_required()     = %s \n",
      namespaceAliasDeclaration->get_type_elaboration_required() ? "true"
                                                                 : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "namespaceAliasDeclaration->get_global_qualification_required() = %s \n",
      namespaceAliasDeclaration->get_global_qualification_required() ? "true"
                                                                     : "false");
#endif

  if (qualifiedNameMapForNames.find(namespaceAliasDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), namespaceAliasDeclaration,
        namespaceAliasDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(namespaceAliasDeclaration, qualifier));
  } else {
    MLOG_WARN_C(MLOG_UNPARSER,
                "Error: name in qualifiedNameMapForNames already exists... \n");
    ROSE_ABORT();
  }
}

// DQ (3/31/2019): Renamed this function to make it more clear now that we have
// two versions, one to name qualify the SgInitializedName and one to name
// qualify the type used in the SgInitializedName. DQ (8/4/2012): Added support
// to permit global qualification to be skipped explicitly (see test2012_164.C
// and test2012_165.C for examples where this is important). void
// NameQualificationTraversal::setNameQualification(SgInitializedName*
// initializedName,SgFunctionDeclaration* functionDeclaration, int
// amountOfNameQualificationRequired) void
// NameQualificationTraversal::setNameQualification(SgInitializedName*
// initializedName,SgDeclarationStatement* declaration, int
// amountOfNameQualificationRequired) void
// NameQualificationTraversal::setNameQualification(SgInitializedName*
// initializedName,SgDeclarationStatement* declaration, int
// amountOfNameQualificationRequired, bool skipGlobalQualification)
void NameQualificationTraversal::setNameQualificationOnType(
    SgInitializedName *initializedName, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired, bool skipGlobalQualification) {
  // This is used to set the name qualification on the type referenced by the
  // SgInitializedName, and not on the SgInitializedName IR node itself.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (4/28/2019): Added assertion.
  ASSERT_not_null(initializedName);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualificationOnType(SgInitializedName*): "
              "initializedName = %p = %s \n",
              initializedName, initializedName->get_name().str());
#endif

  // DQ (4/28/2019): Added assertion.
  ASSERT_not_null(declaration);

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  const int preservedNameQualificationLength =
      initializedName->get_name_qualification_length_for_type();
  const bool preservedGlobalQualification =
      initializedName->get_global_qualification_required_for_type();
  const bool preservedTypeElaboration =
      initializedName->get_type_elaboration_required_for_type();
  const bool preserveWrittenTypeSpelling =
      preservedNameQualificationLength > 0 || preservedGlobalQualification ||
      preservedTypeElaboration;
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  auto canonical_class_decl =
      [](SgClassDeclaration *decl) -> SgClassDeclaration * {
    if (decl == NULL) {
      return NULL;
    }
    if (SgClassDeclaration *first =
            isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first;
    }
    if (SgClassDeclaration *def =
            isSgClassDeclaration(decl->get_definingDeclaration())) {
      return def;
    }
    return decl;
  };
  bool type_decl_is_in_same_enclosing_class = false;
  SgClassDefinition *reference_class_definition =
      SageInterface::getEnclosingClassDefinition(initializedName);
  SgClassDefinition *declaration_class_definition =
      SageInterface::getEnclosingClassDefinition(declaration);
  if (reference_class_definition != NULL &&
      declaration_class_definition != NULL) {
    SgClassDeclaration *reference_class_decl =
        canonical_class_decl(reference_class_definition->get_declaration());
    SgClassDeclaration *declaration_class_decl =
        canonical_class_decl(declaration_class_definition->get_declaration());
    type_decl_is_in_same_enclosing_class =
        reference_class_decl != NULL &&
        reference_class_decl == declaration_class_decl;
  }

  auto tag_decl_owned_by_declarator_visible_without_qualification =
      [&]() -> bool {
    if (initializedName == NULL || declaration == NULL) {
      return false;
    }

    if (isSgClassDeclaration(declaration) == NULL &&
        isSgEnumDeclaration(declaration) == NULL) {
      return false;
    }

    SgDeclarationStatement *decl_owner =
        isSgDeclarationStatement(declaration->get_parent());
    if (decl_owner == NULL) {
      return false;
    }

    if (isSgVariableDeclaration(decl_owner) == NULL &&
        isSgTypedefDeclaration(decl_owner) == NULL) {
      return false;
    }

    SgScopeStatement *reference_scope = initializedName->get_scope();
    SgScopeStatement *declaration_scope =
        traverseNonrealDeclForCorrectScope(declaration);
    if (reference_scope == NULL || declaration_scope == NULL) {
      return false;
    }

    return reference_scope == declaration_scope;
  };

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In setNameQualificationOnType(SgInitializedName*): qualifier = %s \n",
      qualifier.c_str());
#endif

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "declaration = %p = %s \n", declaration,
              declaration->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER,
              "declaration->get_firstNondefiningDeclaration() = %p \n",
              declaration->get_firstNondefiningDeclaration());
  MLOG_WARN_C(MLOG_UNPARSER,
              "declaration->get_definingDeclaration()         = %p \n",
              declaration->get_definingDeclaration());
#endif

  // unsigned int sourceSequenceForInitializedName =
  // initializedName->get_file_info()->get_source_sequence_number(); unsigned
  // int sourceSequenceForTypeDeclaration =
  // declaration->get_file_info()->get_source_sequence_number();

  // DQ (5/15/2018): Test code test2018_65.C demonstrates that we need to
  // suppress the name qualification of the type if the defining declaration has
  // not been seen yet.
  unsigned int sourceSequenceForInitializedName = 0;
  unsigned int sourceSequenceForTypeDeclaration = 0;
  SgDeclarationStatement *definingDeclaration =
      declaration->get_definingDeclaration();
  if (definingDeclaration != NULL) {
    // If we have a defining declaration, then query the source sequence
    // numbers.
    ASSERT_not_null(initializedName->get_file_info());
    ASSERT_not_null(declaration->get_file_info());
    sourceSequenceForTypeDeclaration =
        definingDeclaration->get_file_info()->get_source_sequence_number();
    sourceSequenceForInitializedName =
        initializedName->get_file_info()->get_source_sequence_number();
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "sourceSequenceForInitializedName = %u \n",
              sourceSequenceForInitializedName);
  MLOG_WARN_C(MLOG_UNPARSER, "sourceSequenceForTypeDeclaration = %u \n",
              sourceSequenceForTypeDeclaration);
#endif

  const bool initializedNameHasStableSourceOrdering =
      initializedName->get_file_info() != NULL &&
      initializedName->get_file_info()->isTransformation() == false &&
      initializedName->get_file_info()->isCompilerGenerated() == false;
  bool outputNameQualification = initializedNameHasStableSourceOrdering
                                     ? (sourceSequenceForTypeDeclaration <
                                        sourceSequenceForInitializedName)
                                     : (amountOfNameQualificationRequired > 0);

  // DQ (5/15/2018): If this is a SgTemplateInstantiationTypedefDeclaration then
  // output the name qualification.
  if (isSgTemplateInstantiationTypedefDeclaration(declaration) != NULL) {
    outputNameQualification = true;
  }

  if (type_decl_is_in_same_enclosing_class == true) {
    qualifier = "";
    outputNameQualification = false;
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualificationOnType(SgInitializedName*): "
              "outputNameQualification = %s \n",
              outputNameQualification ? "true" : "false");
#endif

  // DQ (5/15/2018): Explicitly check for qualifier == "::" (see
  // Cxxx11_tests/test2018_97.C). DQ (8/4/2012): In rare cases we have to
  // eliminate qualification only if it is going to be global qualification. if
  // (skipGlobalQualification == true && qualifier == "::") if
  // (skipGlobalQualification == true) if (skipGlobalQualification == true &&
  // qualifier == "::")
  if (skipGlobalQualification == true && outputNameQualification == false) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In "
                "NameQualificationTraversal::setNameQualification("
                "SgInitializedName* initializedName): skipGlobalQualification "
                "has caused global qualification to be ignored \n");
#endif
    qualifier = "";

    outputNameQualificationLength = 0;
    outputGlobalQualification = false;

    // Note clear if this is what we want.
    outputTypeEvaluation = false;
  }

  if (preserveWrittenTypeSpelling) {
    // Preserve frontend-written type spelling on parameters and other
    // declarations. The name-qualification pass computes the minimal
    // semantic qualifier for successful lookup, but source-to-source output
    // must not discard explicit nested-name/global qualifiers or elaborated
    // keywords that were present in the original declaration.
    qualifier = initializedName->get_qualified_name_prefix_for_type().str();
    if (qualifier.empty()) {
      int ignoredNameQualificationLength = 0;
      bool ignoredGlobalQualification = false;
      bool ignoredTypeEvaluation = false;
      if (preservedNameQualificationLength > 0) {
        qualifier = setNameQualificationSupport(
            scope, preservedNameQualificationLength,
            ignoredNameQualificationLength, ignoredGlobalQualification,
            ignoredTypeEvaluation);
      }
      if (preservedGlobalQualification && qualifier.rfind("::", 0) != 0) {
        qualifier = "::" + qualifier;
      }
    }

    outputNameQualificationLength = preservedNameQualificationLength;
    outputGlobalQualification = preservedGlobalQualification;
    outputTypeEvaluation = preservedTypeElaboration;
  }

  if (preserveWrittenTypeSpelling == false &&
      tag_decl_owned_by_declarator_visible_without_qualification()) {
    // Declarator-owned tags such as `class A *a;`, `class A a;`, and
    // later shared-type references to those same-scope surfaces remain
    // reachable through ordinary lexical lookup. Adding a generated global
    // qualifier ties the output to a declarator surface instead of the tag's
    // actual visibility and can produce invalid `::A`.
    qualifier = "";
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
  }

  initializedName->set_global_qualification_required_for_type(
      outputGlobalQualification);
  initializedName->set_name_qualification_length_for_type(
      outputNameQualificationLength);
  initializedName->set_type_elaboration_required_for_type(outputTypeEvaluation);

  // The name-qualification pass normally computes no additional elaborated
  // type requirement on its own, but it may need to preserve frontend-written
  // elaboration that was already attached to the declaration.
  ROSE_ASSERT(outputTypeEvaluation == false || preserveWrittenTypeSpelling);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualificationOnType(): "
      "initializedName->get_name_qualification_length_for_type()     = %d \n",
      initializedName->get_name_qualification_length_for_type());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualificationOnType(): "
      "initializedName->get_type_elaboration_required_for_type()     = %s \n",
      initializedName->get_type_elaboration_required_for_type() ? "true"
                                                                : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualificationOnType(): "
      "initializedName->get_global_qualification_required_for_type() = %s \n",
      initializedName->get_global_qualification_required_for_type() ? "true"
                                                                    : "false");
#endif

  if (qualifiedNameMapForTypes.find(initializedName) ==
      qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for type = %s into list at "
                "SgInitializedName IR node = %p = %s \n",
                qualifier.c_str(), initializedName,
                initializedName->get_name().str());
#endif
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(initializedName, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForTypes.find(initializedName);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 12: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      // ROSE_ABORT();
    }
  }
}

// DQ (12/17/2013): Added support for the name qualification of the
// SgInitializedName object when used in the context of the preinitialization
// list.
void NameQualificationTraversal::setNameQualificationOnName(
    SgInitializedName *initializedName, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired, bool skipGlobalQualification) {
  // This is used to set the name qualification on the SgInitializedName
  // directly, and not on the type referenced by the SgInitializedName IR node.

  if (initializedName == nullptr) {
    return;
  }

  if (initializedName->get_name().getString().empty()) {
    initializedName->set_global_qualification_required(false);
    initializedName->set_name_qualification_length(0);
    initializedName->set_type_elaboration_required(false);
    qualifiedNameMapForNames.erase(initializedName);
    return;
  }

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualificationOnName(SgInitializedName*): "
              "amountOfNameQualificationRequired = %d \n",
              amountOfNameQualificationRequired);
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  // DQ (8/4/2012): In rare cases we have to eliminate qualification only if it
  // is going to be global qualification. if (skipGlobalQualification == true &&
  // qualifier == "::")
  if (skipGlobalQualification == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "In "
                "NameQualificationTraversal::setNameQualificationOnName("
                "SgInitializedName* initializedName): skipGlobalQualification "
                "has caused global qualification to be ignored \n");
#endif
    qualifier = "";

    outputNameQualificationLength = 0;
    outputGlobalQualification = false;

    // Note clear if this is what we want.
    outputTypeEvaluation = false;
  }

  // DQ (3/31/2019): New version of code
  initializedName->set_global_qualification_required(outputGlobalQualification);
  initializedName->set_name_qualification_length(outputNameQualificationLength);
  initializedName->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualificationOnName():
  // initializedName->get_name_qualification_length_for_type()     = %d
  // \n",initializedName->get_name_qualification_length_for_type());
  // MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualificationOnName():
  // initializedName->get_type_elaboration_required_for_type()     = %s
  // \n",initializedName->get_type_elaboration_required_for_type() ? "true" :
  // "false"); MLOG_WARN_C(MLOG_UNPARSER, "In
  // NameQualificationTraversal::setNameQualificationOnName():
  // initializedName->get_global_qualification_required_for_type() = %s
  // \n",initializedName->get_global_qualification_required_for_type() ? "true"
  // : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualificationOnName(): "
              "initializedName->get_name_qualification_length()     = %d \n",
              initializedName->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualificationOnName(): "
              "initializedName->get_type_elaboration_required()     = %s \n",
              initializedName->get_type_elaboration_required() ? "true"
                                                               : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualificationOnName(): "
              "initializedName->get_global_qualification_required() = %s \n",
              initializedName->get_global_qualification_required() ? "true"
                                                                   : "false");
#endif

  if (qualifiedNameMapForNames.find(initializedName) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for SgInitializedName = %s into list at "
                "SgInitializedName IR node = %p = %s \n",
                qualifier.c_str(), initializedName,
                initializedName->get_name().str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(initializedName, qualifier));
  } else {
    // If it already exists then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(initializedName);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 13: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      // ROSE_ABORT();
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgVariableDeclaration *variableDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // This is used to set the name qualification on the associated
  // SgInitializedName (there is only one per SgVariableDeclaration at present,
  // but this may be changed (fixed) in the future.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualification(SgVariableDeclaration*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  variableDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  variableDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  variableDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "variableDeclaration->get_name_qualification_length()     = %d \n",
      variableDeclaration->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "variableDeclaration->get_type_elaboration_required()     = %s \n",
      variableDeclaration->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualification(): "
      "variableDeclaration->get_global_qualification_required() = %s \n",
      variableDeclaration->get_global_qualification_required() ? "true"
                                                               : "false");
#endif

  NameQualificationMapType::iterator it_qualifiedNameMapForNames =
      qualifiedNameMapForNames.find(variableDeclaration);
  if (it_qualifiedNameMapForNames == qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for name = %s into list at "
                "SgVariableDeclaration IR node = %p = %s \n",
                qualifier.c_str(), variableDeclaration,
                variableDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(variableDeclaration, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(variableDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 14: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      // ROSE_ABORT();
    }
  }
}

void NameQualificationTraversal::setNameQualificationOnBaseType(
    SgTypedefDeclaration *typedefDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualificationOnBaseType(SgTypedefDeclaration*) \n");
  MLOG_WARN_C(MLOG_UNPARSER, " --- typedefDeclaration = %p (%s)\n",
              typedefDeclaration, typedefDeclaration->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER, " --- declaration = %p (%s)\n", declaration,
              declaration->class_name().c_str());
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  // A typedef can own the written nondefining tag declaration for a C-style
  // spelling such as `typedef struct A A;`. In that case, and when a sibling
  // typedef in the same scope reuses the same written tag (`typedef struct A
  // B;`), the base type spelling is still a declaration in the current scope,
  // not a qualified reference to an outer declaration. Treating these as
  // ordinary references injects invalid output like `typedef struct ::A A;`.
  if ((isSgClassDeclaration(declaration) != nullptr ||
       isSgEnumDeclaration(declaration) != nullptr) &&
      isSgTypedefDeclaration(declaration->get_parent()) != nullptr) {
    SgTypedefDeclaration *owner_typedef =
        isSgTypedefDeclaration(declaration->get_parent());
    if (owner_typedef->get_scope() == typedefDeclaration->get_scope() &&
        scopes_are_equivalent_for_name_qualification(
            declaration->get_scope(), typedefDeclaration->get_scope())) {
      outputNameQualificationLength = 0;
      outputGlobalQualification = false;
      qualifier.clear();
    }
  }

  if ((isSgClassDeclaration(declaration) != nullptr ||
       isSgEnumDeclaration(declaration) != nullptr) &&
      scopes_are_equivalent_for_name_qualification(
          declaration->get_scope(), typedefDeclaration->get_scope())) {
    // The typedef base type names a tag that already lives in the current
    // scope. It is never a qualified lookup to an outer declaration, even when
    // the tag was built structurally and has no meaningful source range.
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    qualifier.clear();
  }

  typedefDeclaration->set_global_qualification_required_for_base_type(
      outputGlobalQualification);
  typedefDeclaration->set_name_qualification_length_for_base_type(
      outputNameQualificationLength);
  typedefDeclaration->set_type_elaboration_required_for_base_type(
      outputTypeEvaluation ||
      typedefDeclaration->get_type_elaboration_required_for_base_type());

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualificationOnBaseType(): "
      "typedefDeclaration->get_name_qualification_length_for_base_type()     = "
      "%d \n",
      typedefDeclaration->get_name_qualification_length_for_base_type());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualificationOnBaseType(): "
      "typedefDeclaration->get_type_elaboration_required_for_base_type()     = "
      "%s \n",
      typedefDeclaration->get_type_elaboration_required_for_base_type()
          ? "true"
          : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In NameQualificationTraversal::setNameQualificationOnBaseType(): "
      "typedefDeclaration->get_global_qualification_required_for_base_type() = "
      "%s \n",
      typedefDeclaration->get_global_qualification_required_for_base_type()
          ? "true"
          : "false");
#endif

  if (qualifiedNameMapForTypes.find(typedefDeclaration) ==
      qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for type = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), typedefDeclaration,
        typedefDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(typedefDeclaration, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForTypes.find(typedefDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 15: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: name in qualifiedNameMapForTypes "
                                 "already exists and is different... \n");
      // ROSE_ABORT();

      SgName testNameInMap = typedefDeclaration->get_qualified_name_prefix();
      MLOG_WARN_C(MLOG_UNPARSER, "testNameInMap = %s \n", testNameInMap.str());
#endif
    }
  }
}

void NameQualificationTraversal::setNameQualificationOnPointerMemberClass(
    SgTypedefDeclaration *typedefDeclaration,
    SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In setNameQualificationOnPointerMemberClass(SgTypedefDeclaration*) \n");
  MLOG_WARN_C(MLOG_UNPARSER, " --- typedefDeclaration = %p (%s)\n",
              typedefDeclaration, typedefDeclaration->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER, " --- declaration = %p (%s)\n", declaration,
              declaration->class_name().c_str());
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  typedefDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  typedefDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  typedefDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualificationOnPointerMemberClass(): "
      "typedefDeclaration->get_name_qualification_length()     = %d \n",
      typedefDeclaration->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualificationOnPointerMemberClass(): "
      "typedefDeclaration->get_type_elaboration_required()     = %s \n",
      typedefDeclaration->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualificationOnPointerMemberClass(): "
      "typedefDeclaration->get_global_qualification_required() = %s \n",
      typedefDeclaration->get_global_qualification_required() ? "true"
                                                              : "false");
#endif

  if (qualifiedNameMapForNames.find(typedefDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for type = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), typedefDeclaration,
        typedefDeclaration->class_name().c_str());
#endif
    // qualifiedNameMapForTypes.insert(std::pair<SgNode*,std::string>(typedefDeclaration,qualifier));
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(typedefDeclaration, qualifier));
  } else {
    // If it already existes then overwrite the existing information.
    // std::map<SgNode*,std::string>::iterator i =
    // qualifiedNameMapForTypes.find(typedefDeclaration); ROSE_ASSERT (i !=
    // qualifiedNameMapForTypes.end());
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(typedefDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 16: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: name in qualifiedNameMap already "
                                 "exists and is different... \n");
      // ROSE_ABORT();

      SgName testNameInMap = typedefDeclaration->get_qualified_name_prefix();
      MLOG_WARN_C(MLOG_UNPARSER, "testNameInMap = %s \n", testNameInMap.str());
#endif
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgTemplateArgument *templateArgument, SgDeclarationStatement *declaration,
    int amountOfNameQualificationRequired) {
  // This function will generate the qualified name prefix (without the name of
  // the declaration) and add it to the map of name qualification strings
  // referenced via the IR node that references the SgTemplateArgument.

  // DQ (6/1/2011): Note that the name qualification could be more complex than
  // this function presently supports. The use of derivation can permit there to
  // be multiple legal qualified names for a single construct.  There could also
  // be some qualified names using using type names that are private or
  // protected and thus can only be used in restricted contexts.  This sumbject
  // of multiple qualified names or selecting amongst them for where each may be
  // used is not handled presently.

  // DQ (9/23/2012): Note that the template arguments of the defining
  // declaration don't appear to be set (only for the nondefining declaration).
  // This was a problem for test2012_220.C.  The fix was to make sure that the
  // unparsing of the SgClassType consistantly uses the nondefining declaration,
  // and it's template arguments.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#define DEBUG_TEMPLATE_ARGUMENT_NAME_QUALIFICATION 0

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_TEMPLATE_ARGUMENT_NAME_QUALIFICATION
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgTemplateArgument*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(declaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_TEMPLATE_ARGUMENT_NAME_QUALIFICATION
  MLOG_WARN_C(MLOG_UNPARSER, " - qualifier = %s\n", qualifier.c_str());
#endif

#if DEBUG_NONTERMINATION || 0
  printf("In setNameQualification(SgTemplateArgument*): qualifier.length() = "
         "%zu \n",
         qualifier.length());
#endif

  // DQ (5/23/2024): When the names start getting too long, use this as a way to
  // short-circuit the runaway process.
  const size_t qualifier_length_limit = 6000;
  if (qualifier.length() > qualifier_length_limit) {
    static bool messageOutput = false;
    if (messageOutput == false) {
      // MLOG_WARN_C(MLOG_UNPARSER, "In
      // setNameQualification(SgTemplateArgument*): qualifier.length() length
      // exceeded (qualifier_length_limit = %zu) Setting
      // disableNameQualification == true \n",qualifier_length_limit);
      printf("In setNameQualification(SgTemplateArgument*): qualifier.length() "
             "length exceeded (qualifier_length_limit = %zu) Setting "
             "disableNameQualification == true \n",
             qualifier_length_limit);
      messageOutput = true;

      disableNameQualification = true;
    }

    // Zero out the qualifier.
    qualifier = "";
  }

  // These may not be important under the newest version of name qualification
  // that uses the qualified name string map to IR nodes that reference the
  // construct using the name qualification.
  const int existingQualificationLength =
      templateArgument->get_name_qualification_length();
  const bool existingGlobalQualification =
      templateArgument->get_global_qualification_required();
  const bool existingTypeElaboration =
      templateArgument->get_type_elaboration_required();
  bool replaceQualification = false;
  if (outputNameQualificationLength > existingQualificationLength) {
    replaceQualification = true;
  } else if (outputNameQualificationLength == existingQualificationLength &&
             outputGlobalQualification && !existingGlobalQualification) {
    replaceQualification = true;
  } else if (outputTypeEvaluation && !existingTypeElaboration) {
    replaceQualification = true;
  }
  if (!replaceQualification) {
    outputNameQualificationLength = existingQualificationLength;
    outputGlobalQualification = existingGlobalQualification;
    outputTypeEvaluation = existingTypeElaboration;
  }
  outputTypeEvaluation = outputTypeEvaluation || existingTypeElaboration;

  // Keep the generated qualifier string consistent with the final metadata we
  // store on the template argument. Frontend-written qualifications can be
  // preserved by the fields above even when the local name-qualification pass
  // computes a shorter qualifier, and the unparser reads the string map rather
  // than the integer depth directly.
  {
    int finalizedQualificationLength = 0;
    bool finalizedGlobalQualification = false;
    bool finalizedTypeEvaluation = false;
    qualifier = setNameQualificationSupport(
        scope, outputNameQualificationLength, finalizedQualificationLength,
        finalizedGlobalQualification, finalizedTypeEvaluation);
    if (outputGlobalQualification && qualifier.rfind("::", 0) != 0) {
      qualifier = "::" + qualifier;
    }
  }

  templateArgument->set_global_qualification_required(
      outputGlobalQualification);
  templateArgument->set_name_qualification_length(
      outputNameQualificationLength);
  templateArgument->set_type_elaboration_required(outputTypeEvaluation);

  // Preserve an explicitly written elaborated spelling from the frontend.
  // Name qualification can determine that no extra elaboration is required for
  // lookup, but it should not erase source-level `struct` / `class` / `enum`
  // syntax attached to a template type argument.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) ||                                    \
    DEBUG_TEMPLATE_ARGUMENT_NAME_QUALIFICATION
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "templateArgument                                      = %p \n",
              templateArgument);
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "templateArgument->get_name_qualification_length()     = %d \n",
              templateArgument->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "templateArgument->get_type_elaboration_required()     = %s \n",
              templateArgument->get_type_elaboration_required() ? "true"
                                                                : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "templateArgument->get_global_qualification_required() = %s \n",
              templateArgument->get_global_qualification_required() ? "true"
                                                                    : "false");
#endif

  // DQ (9/25/2012): The code below is more complex than I would like because it
  // has to set the name qualification on both the template arguments of the
  // defining and nondefining declarations.

  // DQ (9/22/2012): This is the bug to fix tomorrow morning...
  // XXX:  Either we should be setting the name_qualification_length on the
  // SgTemplateArgument for the defining declaration (as well as the (first?)
  // nondefining declaration)
  //       or we should be sharing the SgTemplateArgument across both the
  //       non-defining and defining declarations. I think I would like to share
  //       the SgTemplateArgument (this this problem would take care of itself).

  // TV (04/04/2018): Look for matching template argument in the peer
  // declaration (defining vs. nondefining).
  // For non-real template instantiation, i.e. member of a template parameter:
  // "T0::template T1<A>", the template argument
  // ("A") parent is the global scope (FIXME or is it the scope of the template
  // parameter ("T0")??? FIXME)
  SgTemplateArgument *defining_templateArgument = NULL;
  SgNode *tpl_arg_parent = templateArgument->get_parent();

#if DEBUG_TEMPLATE_ARGUMENT_NAME_QUALIFICATION
  MLOG_WARN_C(MLOG_UNPARSER, "tpl_arg_parent = %p = %s \n", tpl_arg_parent,
              tpl_arg_parent ? tpl_arg_parent->class_name().c_str() : "");
#endif

  ASSERT_not_null(tpl_arg_parent);

  SgDeclarationStatement *associatedDeclaration =
      isSgDeclarationStatement(tpl_arg_parent);
  if (associatedDeclaration != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "associatedDeclaration = %p = %s \n",
                associatedDeclaration,
                associatedDeclaration->class_name().c_str());
#endif
    SgDeclarationStatement *firstNondefining_associatedDeclaration =
        associatedDeclaration->get_firstNondefiningDeclaration();
    SgDeclarationStatement *defining_associatedDeclaration =
        associatedDeclaration->get_definingDeclaration();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "firstNondefining_associatedDeclaration = %p \n",
                firstNondefining_associatedDeclaration);
#endif
    SgTemplateInstantiationDecl *nondefining_instantiation =
        isSgTemplateInstantiationDecl(firstNondefining_associatedDeclaration);
    SgTemplateInstantiationDecl *defining_instantiation =
        isSgTemplateInstantiationDecl(defining_associatedDeclaration);

    auto find_template_arg_index = [](const SgTemplateArgumentPtrList &list,
                                      SgTemplateArgument *arg) -> int {
      int index = 0;
      for (SgTemplateArgumentPtrList::const_iterator i = list.begin();
           i != list.end(); ++i, ++index) {
        if (*i == arg) {
          return index;
        }
      }
      return -1;
    };

    if (nondefining_instantiation != NULL && defining_instantiation != NULL) {
      SgTemplateArgumentPtrList &nondef_args =
          nondefining_instantiation->get_templateArguments();
      SgTemplateArgumentPtrList &def_args =
          defining_instantiation->get_templateArguments();

      int index = find_template_arg_index(nondef_args, templateArgument);
      if (index >= 0 && static_cast<size_t>(index) < def_args.size()) {
        defining_templateArgument = def_args[index];
      }
      if (defining_templateArgument == NULL) {
        index = find_template_arg_index(def_args, templateArgument);
        if (index >= 0 && static_cast<size_t>(index) < nondef_args.size()) {
          defining_templateArgument = nondef_args[index];
        }
      }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "defining_templateArgument = %p \n",
                  defining_templateArgument);
#endif

      // This is false when the template arguments are shared (which appears to
      // happen sometimes, see test2004_38.C).
      // ASSERT_not_null(defining_templateArgument);
      if (defining_templateArgument != NULL &&
          defining_templateArgument != templateArgument) {
        // Mark the associated template argument in the peer declaration so
        // that it can be output with qualification (see test2012_220.C).
        defining_templateArgument->set_global_qualification_required(
            outputGlobalQualification);
        defining_templateArgument->set_name_qualification_length(
            outputNameQualificationLength);
        defining_templateArgument->set_type_elaboration_required(
            outputTypeEvaluation);

        // DQ (9/24/2012): Make sure these are different.
        ROSE_ASSERT(defining_templateArgument != templateArgument);
      }
    }
  } else {
  }

  // Look for the template argument in the IR node map and either reset it or
  // add it to the map. The support for the template argument from the defining
  // declaration makes this a bit more complex, but both are set to always be
  // the same (since we will prefer to use that from the defining declaration in
  // the unparsing).  Note that the preference for the defining declaration use
  // in the unparsing comes from supporting the corner case of type declarations
  // nested in other declarations; e.g. "struct X { int a; } Y;" where the
  // declaration of the type "X" is nested in the declaration of the variable
  // "Y" (there are several different forms of this).
  if (qualifiedNameMapForTypes.find(templateArgument) ==
      qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for name or type = %s into list at IR "
                "node = %p = %s \n",
                qualifier.c_str(), templateArgument,
                templateArgument->class_name().c_str());
#endif
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(templateArgument, qualifier));

    // Handle the defining declaration's template argument.
    // if (defining_templateArgument != NULL)
    if (defining_templateArgument != NULL &&
        defining_templateArgument != templateArgument) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Insert qualified name = %s for defining_templateArgument = %p \n",
          qualifier.c_str(), defining_templateArgument);
#endif
      NameQualificationMapType::iterator def_iter =
          qualifiedNameMapForTypes.find(defining_templateArgument);
      if (def_iter == qualifiedNameMapForTypes.end()) {
        qualifiedNameMapForTypes.insert(std::pair<SgNode *, std::string>(
            defining_templateArgument, qualifier));
      } else if (def_iter->second.empty() ||
                 qualifier.size() > def_iter->second.size()) {
        def_iter->second = qualifier;
      }
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "NOTE: defining_templateArgument != NULL && "
                  "defining_templateArgument != templateArgument (qualified "
                  "not inserted into qualifiedNameMapForTypes using "
                  "defining_templateArgument = %p \n",
                  defining_templateArgument);
#endif
    }
  } else {
    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForTypes.find(templateArgument);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 17: for templateArgument = %p replacing "
                "previousQualifier = %s with new qualifier = %s \n",
                templateArgument, previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second.empty() || qualifier.size() > i->second.size()) {
      i->second = qualifier;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: name in qualifiedNameMapForTypes "
                                 "already exists and is different... \n");
      // ROSE_ABORT();
#endif

      SgName testNameInMap = templateArgument->get_qualified_name_prefix();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER, "testNameInMap = %s \n", testNameInMap.str());
#endif

      // DQ (9/24/2012): Check that the defining declaration's template argument
      // is uniformally set. if (defining_templateArgument != NULL)
      if (defining_templateArgument != NULL &&
          defining_templateArgument != templateArgument) {
        ROSE_ASSERT(qualifiedNameMapForTypes.find(defining_templateArgument) !=
                    qualifiedNameMapForTypes.end());
        NameQualificationMapType::iterator j =
            qualifiedNameMapForTypes.find(defining_templateArgument);
        ROSE_ASSERT(j != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER,
                    "For defining_templateArgument = %p j->second = %s "
                    "qualifier = %s \n",
                    defining_templateArgument, j->second.c_str(),
                    qualifier.c_str());
#endif
        // ROSE_ASSERT(j->second != qualifier);

        if (j->second.empty() || qualifier.size() > j->second.size()) {
          j->second = qualifier;
        }

        SgName defining_testNameInMap =
            defining_templateArgument->get_qualified_name_prefix();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(MLOG_UNPARSER, "defining_testNameInMap = %s \n",
                    defining_testNameInMap.str());
#endif
        ROSE_ASSERT(defining_testNameInMap == testNameInMap);
      }
    } else {
      // DQ (5/30/2019): Need to handle the case where the name qualification
      // stored in the qualifiedNameMapForTypes are the same for
      // templateArgument, but different for defining_templateArgument.  This is
      // a bugfix for test2019_444.C reported by Charles as part of reproducers
      // for bugs in ROE from ASC codes.
      if (defining_templateArgument != NULL &&
          defining_templateArgument != templateArgument) {
        ROSE_ASSERT(qualifiedNameMapForTypes.find(defining_templateArgument) !=
                    qualifiedNameMapForTypes.end());
        NameQualificationMapType::iterator j =
            qualifiedNameMapForTypes.find(defining_templateArgument);
        ROSE_ASSERT(j != qualifiedNameMapForTypes.end());
        // ROSE_ASSERT(j->second == qualifier);
        if (j->second.empty() || qualifier.size() > j->second.size()) {
          j->second = qualifier;
        }
      }
    }
  }
}

// void NameQualificationTraversal::setNameQualification(SgCastExp* castExp,
// SgDeclarationStatement* typeDeclaration, int
// amountOfNameQualificationRequired)
void NameQualificationTraversal::setNameQualification(
    SgExpression *exp, SgDeclarationStatement *typeDeclaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (6/4/2011): This should not be a SgConstructorInitializer since that
  // uses the qualifiedNameMapForNames instead of the qualifiedNameMapForTypes.
  ROSE_ASSERT(isSgConstructorInitializer(exp) == NULL);

  // DQ (11/22/2016): Added assertion.
  ASSERT_not_null(typeDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgExpression*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(typeDeclaration);
  int effectiveNameQualification = amountOfNameQualificationRequired;
  getExplicitQualifierLength(exp, effectiveNameQualification);
  string qualifier = setNameQualificationSupport(
      scope, effectiveNameQualification, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  applyExplicitQualifier(exp, qualifier, outputNameQualificationLength,
                         outputGlobalQualification);

  // Preserve an explicitly written elaborated spelling from the frontend. The
  // name-qualification analysis can decide that no additional elaboration is
  // required for lookup, but it should not erase a source-level `struct` /
  // `class` / `enum` that was already attached to this type reference.
  outputTypeEvaluation =
      outputTypeEvaluation || exp->get_type_elaboration_required();

  exp->set_global_qualification_required(outputGlobalQualification);
  exp->set_name_qualification_length(outputNameQualificationLength);

  // DQ (6/2/2011): I think that type elaboration could be required for casts,
  // but I am not certain.
  exp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "exp->get_name_qualification_length()     = %d \n",
              exp->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "exp->get_type_elaboration_required()     = %s \n",
              exp->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "exp->get_global_qualification_required() = %s \n",
              exp->get_global_qualification_required() ? "true" : "false");
#endif
  if (qualifiedNameMapForTypes.find(exp) == qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), exp, exp->class_name().c_str());
#endif
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(exp, qualifier));
  } else {
    // DQ (6/21/2011): Now we are catching this case...

    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i = qualifiedNameMapForTypes.find(exp);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 18: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;
    }
  }
}

void NameQualificationTraversal::setNameQualificationForPointerToMember(
    SgExpression *exp, SgDeclarationStatement *typeDeclaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (6/4/2011): This should not be a SgConstructorInitializer since that
  // uses the qualifiedNameMapForNames instead of the qualifiedNameMapForTypes.
  ROSE_ASSERT(isSgConstructorInitializer(exp) == NULL);

  // DQ (11/22/2016): Added assertion.
  ASSERT_not_null(typeDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In setNameQualificationForPointerToMember(SgExpression*): exp = "
              "%p = %s \n",
              exp, exp->class_name().c_str());
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(typeDeclaration);

  // Nonreal references preserve unresolved/dependent source spellings. Only
  // an explicit source qualifier should survive to the unparser; deriving one
  // from declaration scope changes lookup semantics.
  int effectiveNameQualification = 0;
  getExplicitQualifierLength(exp, effectiveNameQualification);
  string qualifier = setNameQualificationSupport(
      scope, effectiveNameQualification, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  applyExplicitQualifier(exp, qualifier, outputNameQualificationLength,
                         outputGlobalQualification);

  // DQ (4/16/2019): These are virtual functions defined for a subset of IR
  // nodes to be valid, but defined for SgExpression to be an explicit error.
  exp->set_global_qualification_for_pointer_to_member_class_required(
      outputGlobalQualification);
  exp->set_name_qualification_for_pointer_to_member_class_length(
      outputNameQualificationLength);

  // DQ (6/2/2011): I think that type elaboration could be required for casts,
  // but I am not certain.
  exp->set_type_elaboration_for_pointer_to_member_class_required(
      outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualificationForPointerToMember(): "
      "exp->get_name_qualification_length()     = %d \n",
      exp->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualificationForPointerToMember(): "
      "exp->get_type_elaboration_required()     = %s \n",
      exp->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualificationForPointerToMember(): "
      "exp->get_global_qualification_required() = %s \n",
      exp->get_global_qualification_required() ? "true" : "false");
#endif

  // if (qualifiedNameMapForTypes.find(exp) == qualifiedNameMapForTypes.end())
  if (qualifiedNameMapForNames.find(exp) == qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), exp, exp->class_name().c_str());
#endif
    // qualifiedNameMapForTypes.insert(std::pair<SgNode*,std::string>(exp,qualifier));
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(exp, qualifier));
  } else {
    // DQ (6/21/2011): Now we are catching this case...

    // If it already existes then overwrite the existing information.
    // std::map<SgNode*,std::string>::iterator i =
    // qualifiedNameMapForTypes.find(exp);
    NameQualificationMapType::iterator i = qualifiedNameMapForNames.find(exp);
    // ROSE_ASSERT (i != qualifiedNameMapForTypes.end());
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 18: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      // DQ (4/16/2019): Since expressions are not shared, we should be able to
      // inforce this.
      MLOG_WARN_C(MLOG_UNPARSER, "Exiting as a test! \n");
      ROSE_ABORT();
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgNonrealRefExp *exp, SgDeclarationStatement *typeDeclaration,
    int amountOfNameQualificationRequired) {
  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (11/22/2016): Added assertion.
  ASSERT_not_null(typeDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgNonrealRefExp*) \n");
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(typeDeclaration);

  // Nonreal references preserve unresolved/dependent source spellings. Only
  // an explicit source qualifier should survive to the unparser; deriving one
  // from declaration scope changes lookup semantics.
  int effectiveNameQualification = 0;
  getExplicitQualifierLength(exp, effectiveNameQualification);
  string qualifier = setNameQualificationSupport(
      scope, effectiveNameQualification, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);
  applyExplicitQualifier(exp, qualifier, outputNameQualificationLength,
                         outputGlobalQualification);

  exp->set_global_qualification_required(outputGlobalQualification);
  exp->set_name_qualification_length(outputNameQualificationLength);

  // DQ (6/2/2011): I think that type elaboration could be required for casts,
  // but I am not certain.
  exp->set_type_elaboration_required(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "exp->get_name_qualification_length()     = %d \n",
              exp->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "exp->get_type_elaboration_required()     = %s \n",
              exp->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "exp->get_global_qualification_required() = %s \n",
              exp->get_global_qualification_required() ? "true" : "false");
#endif
  if (qualifiedNameMapForNames.find(exp) == qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), exp, exp->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(exp, qualifier));
  } else {
    // DQ (6/21/2011): Now we are catching this case...

    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i = qualifiedNameMapForNames.find(exp);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 19: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: name in qualifiedNameMapForNames "
                                 "already exists and is different... \n");
      ROSE_ABORT();

      SgName testNameInMap = exp->get_qualified_name_prefix();
      MLOG_WARN_C(MLOG_UNPARSER, "testNameInMap = %s \n", testNameInMap.str());
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgAggregateInitializer *exp, SgDeclarationStatement *typeDeclaration,
    int amountOfNameQualificationRequired) {
  // DQ (3/22/2018): This is a special version required for the
  // SgAggregateInitializer, becuase it uses the
  // set_global_qualification_required_for_type() named functions instead of the
  // set_global_qualification_required() named functions.  The alternative to to
  // reusing the version of setNameQualification() that takes a SgExpression
  // would force special unparser support for the outputType function.  And
  // since the name qualification for the case of a class type is handled as
  // name qualification on the output of a type, fixing up the name
  // qualification is the better solution (so it seems to be after trying the
  // alternative).

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

  // DQ (6/4/2011): This should not be a SgConstructorInitializer since that
  // uses the qualifiedNameMapForNames instead of the qualifiedNameMapForTypes.
  ROSE_ASSERT(isSgConstructorInitializer(exp) == NULL);

  // DQ (11/22/2016): Added assertion.
  ASSERT_not_null(typeDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualification(SgAggregateInitializer*"
      "): TOP of function: amountOfNameQualificationRequired = %d \n",
      amountOfNameQualificationRequired);
#endif

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(typeDeclaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  exp->set_global_qualification_required_for_type(outputGlobalQualification);
  exp->set_name_qualification_length_for_type(outputNameQualificationLength);

  // DQ (6/2/2011): I think that type elaboration could be required for casts,
  // but I am not certain.
  exp->set_type_elaboration_required_for_type(outputTypeEvaluation);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualification(SgAggregateInitializer*"
      "): exp->get_name_qualification_length()     = %d \n",
      exp->get_name_qualification_length_for_type());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualification(SgAggregateInitializer*"
      "): exp->get_type_elaboration_required()     = %s \n",
      exp->get_type_elaboration_required_for_type() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setNameQualification(SgAggregateInitializer*"
      "): exp->get_global_qualification_required() = %s \n",
      exp->get_global_qualification_required_for_type() ? "true" : "false");
#endif

  if (qualifiedNameMapForTypes.find(exp) == qualifiedNameMapForTypes.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "Inserting qualifier for name = %s into list at IR node = %p = %s \n",
        qualifier.c_str(), exp, exp->class_name().c_str());
#endif
    qualifiedNameMapForTypes.insert(
        std::pair<SgNode *, std::string>(exp, qualifier));
  } else {
    // DQ (6/21/2011): Now we are catching this case...

    // If it already existes then overwrite the existing information.
    NameQualificationMapType::iterator i = qualifiedNameMapForTypes.find(exp);
    ROSE_ASSERT(i != qualifiedNameMapForTypes.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 20: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      MLOG_WARN_C(MLOG_UNPARSER, "WARNING: name in qualifiedNameMapForTypes "
                                 "already exists and is different... \n");
      // ROSE_ABORT();

      SgName testNameInMap = exp->get_qualified_name_prefix();
      MLOG_WARN_C(MLOG_UNPARSER, "testNameInMap = %s \n", testNameInMap.str());
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgClassDeclaration *classDeclaration,
    int amountOfNameQualificationRequired) {
  // This is used to set the name qualification on the associated
  // SgClassDeclaration.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgClassDeclaration*) \n");
  MLOG_WARN_C(MLOG_UNPARSER, " - classDeclaration = %p (%s)\n",
              classDeclaration, classDeclaration->class_name().c_str());
  MLOG_WARN_C(MLOG_UNPARSER, " - amountOfNameQualificationRequired = %d\n",
              amountOfNameQualificationRequired);
#endif

  SgScopeStatement *scope =
      traverseNonrealDeclForCorrectScope(classDeclaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  SgScopeStatement *parent_scope =
      isSgScopeStatement(classDeclaration->get_parent());
  auto same_logical_namespace_scope = [](SgScopeStatement *lhs,
                                         SgScopeStatement *rhs) -> bool {
    if (lhs == NULL || rhs == NULL) {
      return false;
    }
    if (lhs == rhs || SgScopeStatement::isEquivalentScope(lhs, rhs)) {
      return true;
    }
    if (isSgGlobal(lhs) != NULL && isSgGlobal(rhs) != NULL) {
      return true;
    }

    SgNamespaceDefinitionStatement *lhs_ns =
        isSgNamespaceDefinitionStatement(lhs);
    SgNamespaceDefinitionStatement *rhs_ns =
        isSgNamespaceDefinitionStatement(rhs);
    if (lhs_ns == NULL || rhs_ns == NULL) {
      return false;
    }

    SgNamespaceDeclarationStatement *lhs_decl =
        lhs_ns->get_namespaceDeclaration();
    SgNamespaceDeclarationStatement *rhs_decl =
        rhs_ns->get_namespaceDeclaration();
    if (lhs_decl == NULL || rhs_decl == NULL) {
      return false;
    }

    SgDeclarationStatement *lhs_first =
        lhs_decl->get_firstNondefiningDeclaration();
    if (lhs_first == NULL) {
      lhs_first = lhs_decl;
    }
    SgDeclarationStatement *rhs_first =
        rhs_decl->get_firstNondefiningDeclaration();
    if (rhs_first == NULL) {
      rhs_first = rhs_decl;
    }
    return lhs_first == rhs_first;
  };
  auto canonical_class_declaration =
      [](SgClassDeclaration *decl) -> SgClassDeclaration * {
    if (decl == NULL) {
      return NULL;
    }
    if (SgClassDeclaration *first =
            isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
      return first;
    }
    if (SgClassDeclaration *def =
            isSgClassDeclaration(decl->get_definingDeclaration())) {
      return def;
    }
    return decl;
  };
  auto class_declaration_from_scope =
      [&](SgScopeStatement *candidate_scope) -> SgClassDeclaration * {
    if (candidate_scope == NULL) {
      return NULL;
    }
    if (SgClassDefinition *class_def = isSgClassDefinition(candidate_scope)) {
      return canonical_class_declaration(class_def->get_declaration());
    }
    if (SgTemplateClassDefinition *template_def =
            isSgTemplateClassDefinition(candidate_scope)) {
      return canonical_class_declaration(template_def->get_declaration());
    }
    if (SgTemplateInstantiationDefn *inst_def =
            isSgTemplateInstantiationDefn(candidate_scope)) {
      return canonical_class_declaration(
          isSgClassDeclaration(inst_def->get_declaration()));
    }
    if (SgDeclarationScope *decl_scope =
            isSgDeclarationScope(candidate_scope)) {
      return canonical_class_declaration(
          isSgClassDeclaration(decl_scope->get_parent()));
    }
    return NULL;
  };
  auto class_decl_has_same_source_typedef_owner =
      [&](SgClassDeclaration *decl) -> bool {
    if (decl == NULL) {
      return false;
    }
    if (SgTypedefDeclaration *owner_typedef =
            isSgTypedefDeclaration(decl->get_parent())) {
      return owner_typedef->get_scope() == decl->get_scope();
    }

    SgScopeStatement *decl_scope = isSgScopeStatement(decl->get_parent());
    if (decl_scope == NULL) {
      decl_scope = decl->get_scope();
    }
    if (decl_scope == NULL) {
      return false;
    }

    Sg_File_Info *decl_fi = decl->get_file_info();
    if (decl_fi == NULL) {
      return false;
    }

    SgClassDeclaration *canonical_decl = canonical_class_declaration(decl);
    auto visit_scope_declarations = [&](SgScopeStatement *scope,
                                        const auto &visit_declaration) -> bool {
      if (scope == NULL) {
        return false;
      }

      if (scope->containsOnlyDeclarations()) {
        for (SgDeclarationStatement *candidate : scope->getDeclarationList()) {
          if (candidate != NULL && visit_declaration(candidate)) {
            return true;
          }
        }
        return false;
      }

      if (SgBasicBlock *basic_block = isSgBasicBlock(scope)) {
        for (SgStatement *statement : basic_block->get_statements()) {
          if (SgDeclarationStatement *candidate =
                  isSgDeclarationStatement(statement)) {
            if (visit_declaration(candidate)) {
              return true;
            }
          }
        }
      }

      return false;
    };

    return visit_scope_declarations(
        decl_scope, [&](SgDeclarationStatement *candidate) -> bool {
          SgTypedefDeclaration *typedef_decl =
              isSgTypedefDeclaration(candidate);
          if (typedef_decl == NULL || typedef_decl->get_file_info() == NULL) {
            return false;
          }
          if (typedef_decl->get_file_info()->get_filenameString() !=
                  decl_fi->get_filenameString() ||
              typedef_decl->get_file_info()->get_line() !=
                  decl_fi->get_line() ||
              typedef_decl->get_file_info()->get_col() != decl_fi->get_col()) {
            return false;
          }

          SgType *base_type = typedef_decl->get_base_type();
          if (base_type == NULL) {
            base_type = typedef_decl->get_type();
          }
          SgClassType *class_type = isSgClassType(
              base_type != NULL ? base_type->findBaseType() : NULL);
          SgClassDeclaration *type_decl =
              class_type != NULL
                  ? canonical_class_declaration(
                        isSgClassDeclaration(class_type->get_declaration()))
                  : NULL;
          return type_decl == canonical_decl;
        });
  };
  const bool lexically_in_same_file_scope =
      parent_scope != NULL && scope != NULL &&
      (isSgNamespaceDefinitionStatement(parent_scope) != NULL ||
       isSgGlobal(parent_scope) != NULL) &&
      same_logical_namespace_scope(parent_scope, scope);
  const bool lexically_in_same_enclosing_class =
      parent_scope != NULL && scope != NULL &&
      class_declaration_from_scope(parent_scope) != NULL &&
      class_declaration_from_scope(parent_scope) ==
          class_declaration_from_scope(scope);
  const bool lexically_in_same_source_typedef =
      class_decl_has_same_source_typedef_owner(classDeclaration);
  if (lexically_in_same_file_scope || lexically_in_same_enclosing_class ||
      lexically_in_same_source_typedef) {
    qualifier = "";
    outputNameQualificationLength = 0;
    outputGlobalQualification = false;
    outputTypeEvaluation = false;
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, " - qualifier = %s\n", qualifier.c_str());
#endif

  classDeclaration->set_global_qualification_required(
      outputGlobalQualification);
  classDeclaration->set_name_qualification_length(
      outputNameQualificationLength);
  classDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      " - classDeclaration->get_name_qualification_length()     = %d \n",
      classDeclaration->get_name_qualification_length());
  MLOG_WARN_C(
      MLOG_UNPARSER,
      " - classDeclaration->get_type_elaboration_required()     = %s \n",
      classDeclaration->get_type_elaboration_required() ? "true" : "false");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      " - classDeclaration->get_global_qualification_required() = %s \n",
      classDeclaration->get_global_qualification_required() ? "true" : "false");
#endif

  if (qualifiedNameMapForNames.find(classDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for name = %s into list at "
                "SgClassDeclaration IR node = %p = %s \n",
                qualifier.c_str(), classDeclaration,
                classDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(classDeclaration, qualifier));
  } else {
    // If it already exists then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(classDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 21: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      MLOG_WARN_C(MLOG_UNPARSER, "Error: name in qualifiedNameMapForNames "
                                 "already exists and is different... \n");
      ROSE_ABORT();
    }
  }
}

void NameQualificationTraversal::setNameQualification(
    SgEnumDeclaration *enumDeclaration, int amountOfNameQualificationRequired) {
  // This is used to set the name qualification on the associated
  // SgEnumDeclaration.

  // Setup call to refactored code.
  int outputNameQualificationLength = 0;
  bool outputGlobalQualification = false;
  bool outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER, "In setNameQualification(SgEnumDeclaration*) \n");
#endif

  // DQ (2/22/2019): Adding assertion to debug GNU 4.9.3 issue.
  ASSERT_not_null(enumDeclaration);

  const int existingNameQualificationLength =
      enumDeclaration->get_name_qualification_length();
  const bool existingGlobalQualification =
      enumDeclaration->get_global_qualification_required();
  SgUnorderedMapNodeToString::iterator existingQualifier =
      qualifiedNameMapForNames.find(enumDeclaration);
  const bool preserveExplicitSourceQualifier =
      existingQualifier != qualifiedNameMapForNames.end() &&
      !existingQualifier->second.empty() &&
      (existingNameQualificationLength > 0 || existingGlobalQualification);

  SgScopeStatement *scope = traverseNonrealDeclForCorrectScope(enumDeclaration);
  string qualifier = setNameQualificationSupport(
      scope, amountOfNameQualificationRequired, outputNameQualificationLength,
      outputGlobalQualification, outputTypeEvaluation);

  if (preserveExplicitSourceQualifier && outputNameQualificationLength == 0 &&
      !outputGlobalQualification) {
    enumDeclaration->set_global_qualification_required(
        existingGlobalQualification);
    enumDeclaration->set_name_qualification_length(
        existingNameQualificationLength);
    enumDeclaration->set_type_elaboration_required(outputTypeEvaluation);
    return;
  }

  enumDeclaration->set_global_qualification_required(outputGlobalQualification);
  enumDeclaration->set_name_qualification_length(outputNameQualificationLength);
  enumDeclaration->set_type_elaboration_required(outputTypeEvaluation);

  // There should be no type evaluation required for a variable reference, as I
  // recall.
  ROSE_ASSERT(outputTypeEvaluation == false);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "enumDeclaration->get_name_qualification_length()     = %d \n",
              enumDeclaration->get_name_qualification_length());
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "enumDeclaration->get_type_elaboration_required()     = %s \n",
              enumDeclaration->get_type_elaboration_required() ? "true"
                                                               : "false");
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualification(): "
              "enumDeclaration->get_global_qualification_required() = %s \n",
              enumDeclaration->get_global_qualification_required() ? "true"
                                                                   : "false");
#endif

  if (qualifiedNameMapForNames.find(enumDeclaration) ==
      qualifiedNameMapForNames.end()) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER,
                "Inserting qualifier for name = %s into list at "
                "SgEnumDeclaration IR node = %p = %s \n",
                qualifier.c_str(), enumDeclaration,
                enumDeclaration->class_name().c_str());
#endif
    qualifiedNameMapForNames.insert(
        std::pair<SgNode *, std::string>(enumDeclaration, qualifier));
  } else {
    // If it already exists then overwrite the existing information.
    NameQualificationMapType::iterator i =
        qualifiedNameMapForNames.find(enumDeclaration);
    ROSE_ASSERT(i != qualifiedNameMapForNames.end());

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    string previousQualifier = i->second.c_str();
    MLOG_WARN_C(MLOG_UNPARSER,
                "WARNING: test 22: replacing previousQualifier = %s with new "
                "qualifier = %s \n",
                previousQualifier.c_str(), qualifier.c_str());
#endif
    // I think I can do this!
    // *i = std::pair<SgNode*,std::string>(templateArgument,qualifier);
    if (i->second != qualifier) {
      i->second = qualifier;

      MLOG_WARN_C(MLOG_UNPARSER, "Error: name in qualifiedNameMapForNames "
                                 "already exists and is different... \n");
      ROSE_ABORT();
    }
  }
}

string NameQualificationTraversal::setNameQualificationSupport(
    SgScopeStatement *scope, const int inputNameQualificationLength,
    int &output_amountOfNameQualificationRequired,
    bool &outputGlobalQualification, bool &outputTypeEvaluation) {
  // This is lower level support for the different overloaded
  // setNameQualification() functions. This function builds up the qualified
  // name as a string and then returns it to be used in either the map to names
  // or the map to types (two different hash maps).
  string qualifierString;

  output_amountOfNameQualificationRequired = inputNameQualificationLength;
  outputGlobalQualification = false;
  outputTypeEvaluation = false;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(MLOG_UNPARSER,
              "In NameQualificationTraversal::setNameQualificationSupport(): "
              "scope = %p = %s = %s inputNameQualificationLength = %d \n",
              scope, scope->class_name().c_str(),
              SageInterface::get_name(scope).c_str(),
              inputNameQualificationLength);
  MLOG_WARN_C(MLOG_UNPARSER, " --- outputGlobalQualification = %s \n",
              outputGlobalQualification ? "true" : "false");
#endif

  // DQ (8/1/2020): This should always be true.
  // ROSE_ASSERT(namespaceAliasDeclarationMapFromInheritedAttribute != NULL);

  for (int i = 0; i < inputNameQualificationLength; i++) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER, "   --- In loop: i = %d scope = %p = %s = %s \n",
                i, scope, scope->class_name().c_str(),
                SageInterface::get_name(scope).c_str());
#endif
    string scope_name;

    // DQ (8/9/2020): When we process a namespace alias, we can break out of the
    // loop over the length of the required namespace name qualification depth
    // (I think).  See Cxx_tests/test2020_24.C for an example.
    bool breakOutOfLoop = false;

    // DQ (8/19/2014): This is used to control the generation of qualified names
    // for un-named namespaces (and maybe also other un-named language
    // constructs).
    bool skip_over_scope = false;
    // This requirement to visit the template arguments occurs for templaed
    // functions and templated member functions as well.
    SgTemplateInstantiationDefn *templateClassDefinition =
        isSgTemplateInstantiationDefn(scope);
    if (templateClassDefinition != NULL) {
      // Need to investigate how to generate a better quality name.
      SgTemplateInstantiationDecl *templateClassDeclaration =
          isSgTemplateInstantiationDecl(
              templateClassDefinition->get_declaration());
      ASSERT_not_null(templateClassDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      // This is the normalized name (without name qualification for internal
      // template arguments)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "templateClassDeclaration->get_name()          = %s \n",
                  templateClassDeclaration->get_name().str());

      // This is the name of the template (without and internal template
      // arguments)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "templateClassDeclaration->get_templateName() = %s \n",
                  templateClassDeclaration->get_templateName().str());
#endif

      SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
      ASSERT_not_null(unparseInfoPointer);
      unparseInfoPointer->set_outputCompilerGeneratedStatements();
      // Ensure template arguments in scope names are fully qualified.
      unparseInfoPointer->set_language(SgFile::e_Cxx_language);
      unparseInfoPointer->set_SkipClassDefinition();
      unparseInfoPointer->set_SkipEnumDefinition();
      unparseInfoPointer->set_use_generated_name_for_template_arguments(true);
      unparseInfoPointer->set_requiresGlobalNameQualification();

      // templateClassDeclaration->get_file_info()->display("SgTemplateInstantiationDecl
      // trying to generate the qualified name: debug");

      string template_name = templateClassDeclaration->get_templateName();

      // DQ (2/22/2019): Note: the same moderately more complex handling for
      // template arguments in the unparser, might need to be used here for the
      // support of the name qualification.
      SgTemplateArgumentPtrList &templateArgumentList =
          templateClassDeclaration->get_templateArguments();
      bool isEmptyTemplateArgumentList = templateArgumentList.empty();

      // template_name += "< ";
      if (isEmptyTemplateArgumentList == false) {
        template_name += "< ";
      }

      // MLOG_WARN_C(MLOG_UNPARSER, "START: template_name = %s
      // \n",template_name.c_str());
      SgTemplateArgumentPtrList::iterator i = templateArgumentList.begin();

      bool previousTemplateArgumentOutput = false;
      while (i != templateArgumentList.end()) {
        bool skipTemplateArgument = false;
        bool stopTemplateArgument = false;
        (*i)->outputTemplateArgument(skipTemplateArgument,
                                     stopTemplateArgument);

        if (stopTemplateArgument) {
          break;
        }

        // if ((*i)->get_argumentType() !=
        // SgTemplateArgument::start_of_pack_expansion_argument)
        if (skipTemplateArgument == false) {
          // if (i != templateArgumentList.begin())
          if (previousTemplateArgumentOutput == true) {
            // DQ (2/11/2019): Adding debugging information for test2019_93.C.
            // template_name += " /* output comma: part 3 */ ";
            template_name += ",";
          }
          string template_argument_name =
              globalUnparseToString(*i, unparseInfoPointer);
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ In name "
              "qualification: templateArgument = %p template_argument_name "
              "(globalUnparseToString()) = %s \n",
              *i, template_argument_name.c_str());
          MLOG_WARN_C(MLOG_UNPARSER,
                      "   --- template_argument_name.length() = %zu \n",
                      template_argument_name.length());
#endif
          template_name += template_argument_name;
          previousTemplateArgumentOutput = true;
        }
        i++;
      }

      // template_name += "> ";
      if (isEmptyTemplateArgumentList == false) {
        template_name += ">";
      }

      scope_name = template_name;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "NAME OF SCOPE: scope name -- template_name = %s \n",
                  template_name.c_str());
#endif

      // DQ (2/18/2013): Fixing generation of too many SgUnparse_Info object.
      delete unparseInfoPointer;
    } else {
      // scope_name = scope->class_name().c_str();
      scope_name = SageInterface::get_name(scope).c_str();
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "Before test for __anonymous_ un-named scopes: scope_name = %s \n",
          scope_name.c_str());
#endif
      // DQ (4/6/2013): Test this scope name for that of n un-named
      // scope so that we can avoid name qualification using an
      // internally generated scope name. Note that the pointer is from
      // an legacy frontend object (e.g. a_type_ptr), so we can't
      // reproduce it in ROSE. This might be something to fix if we
      // want to be able to reproduce it.
      if (scope_name.substr(0, 14) == "__anonymous_0x") {
        // DQ (4/6/2013): Added test (this would be better to added to the AST
        // consistancy tests).
        SgClassDefinition *classDefinition = isSgClassDefinition(scope);
        if (classDefinition != NULL) {
          if (classDefinition->get_declaration()->get_isUnNamed() == false) {
            // DQ (4/11/2017): Klockworks reports that classDeclaration may be
            // NULL, so make sure that adding an assertion will fix the issue.
            SgClassDeclaration *classDeclaration =
                classDefinition->get_declaration();
            ASSERT_not_null(classDeclaration);
            MLOG_WARN_C(MLOG_UNPARSER,
                        "Error: class should be marked as unnamed: "
                        "classDeclaration = %p = %s \n",
                        classDeclaration,
                        classDeclaration->class_name().c_str());
            MLOG_WARN_C(MLOG_UNPARSER, "   --- classDeclaration name = %s \n",
                        classDeclaration->get_name().str());
          }
          ROSE_ASSERT(classDefinition->get_declaration()->get_isUnNamed() ==
                      true);
        }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "In NameQualificationTraversal::setNameQualificationSupport(): "
            "Detected scope_name of un-named scope: scope_name = %s (reset to "
            "empty string for name qualification) \n",
            scope_name.c_str());
#endif
        scope_name = "";

        // DQ (5/3/2013): If this case was detected then we can't use the
        // qualified name. The test2013_145.C demonstrates this case where one
        // part of the name qualification is empty string (unnamed scope,
        // specifically a union in the test code).
        qualifierString = "";

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
        // MLOG_WARN_C(MLOG_UNPARSER, "In
        // NameQualificationTraversal::setNameQualificationSupport(): Exiting
        // loop prematurely... \n");
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "In NameQualificationTraversal::setNameQualificationSupport(): "
            "skip over this un-named declaration in the generation of the more "
            "complete qualified name... \n");
#endif
        // DQ (4/14/2019): If there is an un-named declaration we just want to
        // not use that name, but we want to continue to iterate to collect the
        // associated scopes to build the qualified name. Note that
        // Cxx11_tests/test2019_373.C is an example where this is required.
        // break;
      } else {
        // DQ (4/6/2013): Added test (this would be better to add to the AST
        // consistancy tests). ROSE_ASSERT(scope->get_isUnNamed() == false);
        SgNamespaceDefinitionStatement *namespaceDefinition =
            isSgNamespaceDefinitionStatement(scope);
        if (namespaceDefinition != NULL) {

          SgNamespaceDeclarationStatement *namespaceDeclaration =
              isSgNamespaceDeclarationStatement(
                  namespaceDefinition->get_namespaceDeclaration());
          ASSERT_not_null(namespaceDeclaration);
          if (namespaceDeclaration->get_isUnnamedNamespace() == true) {
            skip_over_scope = true;
          } else {
            // DQ (8/1/2020): Adding support for references to the
            // NamespaceAlias (required for new failing test code). DQ
            // (8/1/2020): This should always be true.
            // ROSE_ASSERT(namespaceAliasDeclarationMapFromInheritedAttribute !=
            // NULL); if
            // (namespaceAliasDeclarationMapFromInheritedAttribute->find(namespaceDeclaration)
            // != namespaceAliasDeclarationMapFromInheritedAttribute->end())
            if (namespaceAliasDeclarationMap.find(namespaceDeclaration) !=
                namespaceAliasDeclarationMap.end()) {
              // SgDeclarationStatement* declaration =
              // namespaceAliasDeclarationMapFromInheritedAttribute->operator[](namespaceDeclaration);
              // SgDeclarationStatement* declaration =
              // namespaceAliasDeclarationMap[namespaceDeclaration];
              SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
                  namespaceAliasDeclarationMap[namespaceDeclaration];
              ROSE_ASSERT(namespaceAliasDeclaration != NULL);
              // qualifierString = " /* use the namspace alias */ ";

              // DQ (8/2/2020): Reset the name of the scope.
              scope_name = namespaceAliasDeclaration->get_name();

              breakOutOfLoop = true;
            }
          }
        } else {
          // DQ (9/7/2014): Added case for template class definitions (which we
          // were not using and thus it was not a problem that we didn't compute
          // them quite right).  These were being computed as
          // "class-name::class-name", but we need then to be computed to be:
          // "class-name<template-parameter>::class-name<template-parameter>"
          // instead. Other logic will have to add the template header where
          // these are used (not clear how to do that if we don't do it here).
          SgTemplateClassDefinition *templateClassDefinition =
              isSgTemplateClassDefinition(scope);
          if (templateClassDefinition != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualificationSupport(): "
                "Found SgTemplateClassDefinition: templateClassDefinition = %p "
                "= %s \n",
                templateClassDefinition,
                templateClassDefinition->class_name().c_str());
#endif
            SgTemplateClassDeclaration *templateClassDeclaration =
                templateClassDefinition->get_declaration();
            ASSERT_not_null(templateClassDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            // This is the normalized name (without name qualification for
            // internal template arguments)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualificationSupport(): "
                "templateClassDeclaration->get_name()          = %s \n",
                templateClassDeclaration->get_name().str());

            // This is the name of the template (without and internal template
            // arguments)
            MLOG_WARN_C(
                MLOG_UNPARSER,
                "In NameQualificationTraversal::setNameQualificationSupport(): "
                "templateClassDeclaration->get_templateName() = %s \n",
                templateClassDeclaration->get_templateName().str());
#endif

            SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
            ASSERT_not_null(unparseInfoPointer);
            unparseInfoPointer->set_outputCompilerGeneratedStatements();

            // templateClassDeclaration->get_file_info()->display("SgTemplateInstantiationDecl
            // trying to generate the qualified name: debug");

            string template_name = templateClassDeclaration->get_templateName();

            // DQ (9/12/2014): If we have template specialization arguments then
            // we wnat to use these instead of the template parameters (I
            // think). See test2014_222.C for an example.
            SgTemplateArgumentPtrList &templateSpecializationArgumentList =
                templateClassDeclaration->get_templateSpecializationArguments();
            bool use_template_specialization_arguments =
                templateSpecializationArgumentList.empty() == false &&
                templateClassDeclaration->get_specialization() !=
                    SgDeclarationStatement::e_no_specialization;
            if (use_template_specialization_arguments) {
              // DQ (9/13/2014): I have build overloaded versions of
              // globalUnparseToString() to handle that case of
              // SgTemplateArgumentPtrList.
              string template_specialization_argument_list_string =
                  globalUnparseToString(&templateSpecializationArgumentList,
                                        unparseInfoPointer);
              template_name += template_specialization_argument_list_string;
            } else {
              // DQ (9/9/2014): Modified to support empty name template
              // parameter lists as what appear if none are present (and this is
              // a non-template function in a template class which we consider
              // to be a template function because it can be instantiated).
              SgTemplateParameterPtrList &templateParameterList =
                  templateClassDeclaration->get_templateParameters();
              if (templateParameterList.empty() == false) {
                auto build_template_parameter_argument_list =
                    [&templateParameterList]() -> std::string {
                  std::string args = "<";
                  bool need_separator = false;
                  for (SgTemplateParameter *param : templateParameterList) {
                    if (param == NULL) {
                      continue;
                    }

                    std::string argument_name;
                    bool is_pack = param->get_is_parameter_pack();
                    if (SgInitializedName *init_name =
                            param->get_initializedName()) {
                      argument_name = init_name->get_name().str();
                      is_pack = is_pack || init_name->get_is_parameter_pack() ||
                                init_name->get_is_pack_element();
                    }

                    if (argument_name.empty()) {
                      if (SgTemplateType *template_type =
                              isSgTemplateType(param->get_type())) {
                        argument_name = template_type->get_name().str();
                        is_pack = is_pack || template_type->get_packed();
                      }
                    }

                    if (argument_name.empty()) {
                      if (SgTemplateDeclaration *tpl_decl =
                              isSgTemplateDeclaration(
                                  param->get_templateDeclaration())) {
                        argument_name = tpl_decl->get_name().str();
                      }
                    }

                    if (argument_name.empty()) {
                      continue;
                    }

                    if (need_separator) {
                      args += ",";
                    }
                    bool has_pack_suffix =
                        argument_name.size() >= 3 &&
                        argument_name.compare(argument_name.size() - 3, 3,
                                              "...") == 0;
                    args += argument_name;
                    if (is_pack && !has_pack_suffix) {
                      args += "...";
                    }
                    need_separator = true;
                  }

                  if (!need_separator) {
                    return "";
                  }

                  args += ">";
                  return args;
                };

                std::string template_parameter_list_string =
                    build_template_parameter_argument_list();
                if (template_parameter_list_string.empty()) {
                  // DQ (9/13/2014): I have build overloaded versions of
                  // globalUnparseToString() to handle that case of
                  // SgTemplateParameterPtrList.
                  template_parameter_list_string = globalUnparseToString(
                      &templateParameterList, unparseInfoPointer);
                }
                template_name += template_parameter_list_string;
              }
            }

            scope_name = template_name;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
            MLOG_WARN_C(MLOG_UNPARSER,
                        "setNameQualificationSupport(): case of "
                        "SgTemplateClassDefinition: scope_name = %s \n",
                        scope_name.c_str());
#endif
          } else {
            // DQ (4/27/2019): Need to add case for SgClassDefinition, to
            // support test2019_102.C.
            SgClassDefinition *classDefinition = isSgClassDefinition(scope);
            if (classDefinition != NULL) {
              if (SgTemplateInstantiationDecl *templateInstantiationDecl =
                      isSgTemplateInstantiationDecl(
                          classDefinition->get_declaration())) {
                SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
                ASSERT_not_null(unparseInfoPointer);
                unparseInfoPointer->set_outputCompilerGeneratedStatements();
                unparseInfoPointer->set_language(SgFile::e_Cxx_language);
                unparseInfoPointer->set_SkipClassDefinition();
                unparseInfoPointer->set_SkipEnumDefinition();
                unparseInfoPointer
                    ->set_use_generated_name_for_template_arguments(true);
                unparseInfoPointer->set_requiresGlobalNameQualification();

                string template_name =
                    templateInstantiationDecl->get_templateName();
                SgTemplateArgumentPtrList &templateArgumentList =
                    templateInstantiationDecl->get_templateArguments();
                bool isEmptyTemplateArgumentList = templateArgumentList.empty();
                if (isEmptyTemplateArgumentList == false) {
                  template_name += "< ";
                }

                bool previousTemplateArgumentOutput = false;
                for (SgTemplateArgument *arg : templateArgumentList) {
                  if (arg == NULL) {
                    continue;
                  }

                  bool skipTemplateArgument = false;
                  bool stopTemplateArgument = false;
                  arg->outputTemplateArgument(skipTemplateArgument,
                                              stopTemplateArgument);
                  if (stopTemplateArgument) {
                    break;
                  }
                  if (skipTemplateArgument) {
                    continue;
                  }

                  if (previousTemplateArgumentOutput) {
                    template_name += ",";
                  }
                  template_name +=
                      globalUnparseToString(arg, unparseInfoPointer);
                  previousTemplateArgumentOutput = true;
                }

                if (isEmptyTemplateArgumentList == false) {
                  template_name += ">";
                }

                scope_name = template_name;
                delete unparseInfoPointer;
              }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
              MLOG_WARN_C(
                  MLOG_UNPARSER,
                  "In "
                  "NameQualificationTraversal::setNameQualificationSupport(): "
                  "Found SgClassDefinition: classDefinition = %p = %s \n",
                  classDefinition, classDefinition->class_name().c_str());
#endif
            }
          }
        }
        // DQ (2/13/2019): Detect error in use of un-named scope (e.g.
        // SgBasicBlock). if (scope_name.substr(0,2) == "0x")
        if (scope_name.substr(0, 2) == "0x" && isSgGlobal(scope) == NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
          MLOG_WARN_C(MLOG_UNPARSER,
                      "WARNING: Detected scope name generated from pointer: i "
                      "= %d scope = %p = %s skip_over_scope = %s \n",
                      i, scope, scope->class_name().c_str(),
                      skip_over_scope ? "true" : "false");
          MLOG_WARN_C(MLOG_UNPARSER, " --- qualifierString = %s \n",
                      qualifierString.c_str());
#endif
          skip_over_scope = true;
        }
      }
    }

    SgScopeStatement *next_scope = scope->get_scope();
    SgDeclarationScope *decl_scope = isSgDeclarationScope(scope);
    if (decl_scope != NULL) {
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(scope->get_parent())) {
        SgName nonreal_name = nrdecl->get_name();
        if (!nrdecl->get_tpl_args().empty()) {
          nonreal_name = SageBuilder::appendTemplateArgumentsToName(
              nonreal_name, nrdecl->get_tpl_args());
        }
        std::string nonreal_scope_name = nonreal_name.getString();
        if (!nonreal_scope_name.empty()) {
          scope_name = nonreal_scope_name;
          skip_over_scope = false;
        }
        if (SgScopeStatement *nr_scope = nrdecl->get_scope()) {
          next_scope = nr_scope;
        }
      } else if (next_scope == NULL) {
        next_scope = SageInterface::getEnclosingScope(scope);
      }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(
          MLOG_UNPARSER,
          "setNameQualificationSupport(): case of SgDeclarationScope:
");
      MLOG_WARN_C(MLOG_UNPARSER, " --- scope_name         = %s 
",
                  scope_name.c_str());
      MLOG_WARN_C(MLOG_UNPARSER, " --- scope->get_scope() = %p (%s) 
",
                  scope->get_scope(),
                  scope->get_scope() != NULL
                      ? scope->get_scope()->class_name().c_str()
                      : "");
      MLOG_WARN_C(MLOG_UNPARSER, " --- scope->get_parent() = %p (%s) 
",
                  scope->get_parent(),
                  scope->get_parent() != NULL
                      ? scope->get_parent()->class_name().c_str()
                      : "");
//          ROSE_ABORT();
#endif
    }

    SgGlobal *globalScope = isSgGlobal(scope);
    if (globalScope != NULL) {
      // If we have iterated beyond the number of nested scopes, then set the
      // global qualification and reduce the name_qualification_length
      // correspondingly by one.

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER,
                  "!!!!! We have iterated beyond the number of nested scopes: "
                  "setting outputGlobalQualification == true \n");
#endif
      outputGlobalQualification = true;
      output_amountOfNameQualificationRequired =
          inputNameQualificationLength - 1;

      scope_name = "::";
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER, " --- scope_name = %s skip_over_scope = %s \n",
                scope_name.c_str(), skip_over_scope ? "true" : "false");
#endif

    if (skip_over_scope == false) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER, " --- outputGlobalQualification = %s \n",
                  outputGlobalQualification ? "true" : "false");
#endif
      // qualifierString = scope_name + "::" + qualifierString;
      if (outputGlobalQualification == true) {
        // Avoid out put of "::::" as substrings.
        if (qualifierString.rfind("::", 0) != 0) {
          qualifierString = "::" + qualifierString;
        }
      } else {
        // qualifierString = scope_name + "::" + qualifierString;
        if (scope_name.rfind("::", 0) == 0) {
          scope_name.erase(0, 2);
        }
        if (scope_name.length() == 0) {
          // Nothing to do for this case of an empty string for a scope name
          // (see test2006_121.C, using an un-named namespace).
        } else {
          qualifierString = scope_name + "::" + qualifierString;
        }
      }
    } else {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
      MLOG_WARN_C(MLOG_UNPARSER, " --- Case of skip_over_scope == true!\n");
#endif
    }
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER, " --- qualifierString = %s \n",
                qualifierString.c_str());
#endif
    if (globalScope != NULL)
      break;

    // We have to loop over scopes that are not named scopes!
    scope = next_scope;

    if (breakOutOfLoop == true) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 1
      printf("breakOutOfLoop == true: short curcuit this loop over the name "
             "qualification depth (becasue we used a namespace alias) \n");
#endif
      break;
    }
  }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) && 1
  printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
  printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "Leaving NameQualificationTraversal::setNameQualificationSupport(): "
      "outputGlobalQualification = %s output_amountOfNameQualificationRequired "
      "= %d qualifierString = %s \n",
      outputGlobalQualification ? "true" : "false",
      output_amountOfNameQualificationRequired, qualifierString.c_str());
  printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
  printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ \n");
#endif

  // DQ (6/12/2011): Make sure we have not generated a qualified name with
  // "::::" because of an scope translated to an empty name.
  size_t duplicate_colons = qualifierString.find("::::");
  while (duplicate_colons != string::npos) {
    qualifierString.replace(duplicate_colons, 4, "::");
    duplicate_colons = qualifierString.find("::::");
  }
  ROSE_ASSERT(qualifierString.find("::::") == string::npos);

  // DQ (6/23/2011): Never generate a qualified name from a pointer value.
  // This is a bug in the inlining support where the symbol tables are not setup
  // just right.
  if (qualifierString.substr(0, 2) == "0x") {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "WARNING: Detected qualified name generated from pointer value 0x..., "
        "reset to empty string (inlining does not fixup symbol tables) \n");
#endif
    qualifierString = "";
  }
  ROSE_ASSERT(qualifierString.substr(0, 2) != "0x");

  return qualifierString;
}

string NameQualificationTraversal::setTemplateHeaderNameQualificationSupport(
    SgScopeStatement *scope, const int inputNameQualificationLength) {
  // DQ (9/7/2014): This function generates a string that is used with name
  // qualification of template declarations. For example:
  //      template < typename T >
  //      template < typename S >
  //      void X<T>::A<S>::foobar (int x) { int a_value; }
  // Requires the template header string: "template < typename T > template <
  // typename S >" in addition to the usual name qualification (which here is in
  // terms of template parameters instead of template arguments (as in a
  // template instantiation), namely "X<T>::A<S>::"). This new support for
  // template headers also requires a new map of names to template declarations.

  // This is lower level support for the different overloaded
  // setNameQualification() functions. This function builds up the qualified
  // name as a string and then returns it to be used in either the map to names
  // or the map to types (two different hash maps).
  string accumulated_template_header_name;

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setTemplateHeaderNameQualificationSupport():"
      " scope = %p = %s = %s inputNameQualificationLength = %d \n",
      scope, scope->class_name().c_str(),
      SageInterface::get_name(scope).c_str(), inputNameQualificationLength);
#endif

  for (int i = 0; i < inputNameQualificationLength; i++) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(MLOG_UNPARSER, "   --- In loop: i = %d scope = %p = %s = %s \n",
                i, scope, scope->class_name().c_str(),
                SageInterface::get_name(scope).c_str());
#endif
    string template_header_name;

    // DQ (9/7/2014): Added case for template class definitions (which we were
    // not using and thus it was not a problem that we didn't compute them quite
    // right).  These were being computed as "class-name::class-name", but we
    // need then to be computed to be:
    // "class-name<template-parameter>::class-name<template-parameter>" instead.
    // Other logic will have to add the template header where these are used
    // (not clear how to do that if we don't do it here).
    SgTemplateClassDefinition *templateClassDefinition =
        isSgTemplateClassDefinition(scope);
    if (templateClassDefinition != NULL) {
#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Found SgTemplateClassDefinition: templateClassDefinition = "
                  "%p = %s \n",
                  templateClassDefinition,
                  templateClassDefinition->class_name().c_str());
#endif
      SgTemplateClassDeclaration *templateClassDeclaration =
          templateClassDefinition->get_declaration();
      ASSERT_not_null(templateClassDeclaration);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
      // This is the normalized name (without name qualification for internal
      // template arguments)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "templateClassDeclaration->get_name()          = %s \n",
                  templateClassDeclaration->get_name().str());

      // This is the name of the template (without and internal template
      // arguments)
      MLOG_WARN_C(MLOG_UNPARSER,
                  "templateClassDeclaration->get_templateName() = %s \n",
                  templateClassDeclaration->get_templateName().str());
#endif
      // templateClassDeclaration->get_file_info()->display("SgTemplateInstantiationDecl
      // trying to generate the qualified name: debug");

      SgTemplateParameterPtrList &templateParameterList =
          templateClassDeclaration->get_templateParameters();
      if (templateParameterList.empty() == false) {
        string template_name = buildTemplateHeaderString(templateParameterList);
        template_header_name = template_name;
      }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3)
    MLOG_WARN_C(
        MLOG_UNPARSER,
        "In "
        "NameQualificationTraversal::setTemplateHeaderNameQualificationSupport("
        "): template_header_name = %s accumulated_template_header_name = %s \n",
        template_header_name.c_str(), accumulated_template_header_name.c_str());
#endif
    accumulated_template_header_name =
        template_header_name + accumulated_template_header_name;

    // We have to loop over scopes that are not named scopes!
    scope = scope->get_scope();
  }

  ROSE_ASSERT(accumulated_template_header_name.substr(0, 2) != "0x");

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "In "
      "NameQualificationTraversal::setTemplateHeaderNameQualificationSupport():"
      " accumulated_template_header_name = %s \n",
      accumulated_template_header_name.c_str());
#endif

  return accumulated_template_header_name;
}

string NameQualificationTraversal::buildTemplateHeaderString(
    SgTemplateParameterPtrList &templateParameterList) {
  SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
  ASSERT_not_null(unparseInfoPointer);
  unparseInfoPointer->set_outputCompilerGeneratedStatements();

  string template_name = "template < ";
  // MLOG_WARN_C(MLOG_UNPARSER, "START: template_name = %s
  // \n",template_name.c_str());
  SgTemplateParameterPtrList::iterator i = templateParameterList.begin();
  while (i != templateParameterList.end()) {
    SgTemplateParameter *templateParameter = *i;
    ASSERT_not_null(templateParameter);

    // Maybe the unparser support should optionally insert the "typename" or
    // other parameter kind support.
    string template_parameter_name =
        globalUnparseToString(templateParameter, unparseInfoPointer);

    bool template_parameter_name_has_been_output = false;

    // DQ (9/10/2014): We only want to output the "typename" when it is required
    // (and exactly when it is required is not clear). Note that in C++ using
    // "class" or "typename" is equivalent. template_name += "typename ";
    switch (templateParameter->get_parameterType()) {
      // Only type parameters should require "typename" (but not if the type was
      // explicit).
    case SgTemplateParameter::type_parameter: {
      // DQ (9/10/2014): Added support for case
      // SgTemplateParameter::type_parameter.
      SgType *type = templateParameter->get_type();
      ASSERT_not_null(type);
      // If the type was explicit then don't output a redundant "typename".
      SgTemplateType *templateType = isSgTemplateType(type);
      if (templateType == NULL) {
        // This might tell us when to use "class: instead of "typename" but
        // since they are equivalent we can prefer to output "typename".
        SgClassType *classType = isSgClassType(type);
        if (classType != NULL) {
          // DQ (9/13/2014): See test2014_224.C for where this is required.
          template_name += "typename ";
          // string name = classType->get_name();
          // curprint(name);
        } else {
          // SgUnparse_Info ninfo(info);
          // unp->u_type->unparseType(type,ninfo);
        }
      } else {
        template_name += "typename ";
      }

      break;
    }

      // Non-type parameters should not require "typename".
    case SgTemplateParameter::nontype_parameter: {
      if (templateParameter->get_expression() != NULL) {
        // unp->u_exprStmt->unparseExpression(templateParameter->get_expression(),info);
      } else {
        if (templateParameter->get_initializedName() == NULL) {
          // Not clear what this is?
        }
        ASSERT_not_null(templateParameter->get_initializedName());

        SgType *type = templateParameter->get_initializedName()->get_type();
        ASSERT_not_null(type);
        // unp->u_type->outputType<SgInitializedName>(templateParameter->get_initializedName(),type,info);
        SgUnparse_Info *unparseInfoPointer = new SgUnparse_Info();
        ASSERT_not_null(unparseInfoPointer);
        unparseInfoPointer->set_outputCompilerGeneratedStatements();

        unparseInfoPointer->set_isTypeFirstPart();

        string template_parameter_name_1stpart =
            globalUnparseToString(type, unparseInfoPointer);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgTemplateParameter::nontype_parameter: "
                    "templateParameter = %p template_parameter_name_1stpart "
                    "(globalUnparseToString()) = %s \n",
                    templateParameter, template_parameter_name_1stpart.c_str());
#endif
        unparseInfoPointer->unset_isTypeFirstPart();

        // DQ (9/11/2014): Need to add a space.
        template_parameter_name_1stpart += " ";

        template_name += template_parameter_name_1stpart;

        // Put out the template parameter name.
        template_name += template_parameter_name;

        template_parameter_name_has_been_output = true;

        unparseInfoPointer->set_isTypeSecondPart();

        // Output the second part of the type.
        string template_parameter_name_2ndpart =
            globalUnparseToString(type, unparseInfoPointer);

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
        MLOG_WARN_C(MLOG_UNPARSER,
                    "case SgTemplateParameter::nontype_parameter: "
                    "templateParameter = %p template_parameter_name_2ndpart "
                    "(globalUnparseToString()) = %s \n",
                    templateParameter, template_parameter_name_2ndpart.c_str());
#endif
        template_name += template_parameter_name_2ndpart;

        // This is not really required since the SgUnparse_Info object will not
        // be used further.
        unparseInfoPointer->unset_isTypeSecondPart();
      }
      break;
    }

    case SgTemplateParameter::template_parameter: {
      ASSERT_not_null(templateParameter->get_templateDeclaration());
      SgNonrealDecl *nrdecl =
          isSgNonrealDecl(templateParameter->get_templateDeclaration());
      ASSERT_not_null(nrdecl);

      SgTemplateParameterPtrList &templateParameterList =
          nrdecl->get_tpl_params();

      template_name += buildTemplateHeaderString(templateParameterList);

      // Not clear if this should always be marked as "class".
      // TV (04/06/2018): not always (rarely?) necessary...
      template_name += "class ";
      break;
    }

    default: {
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Error: buildTemplateHeaderString(): default reached \n");
      ROSE_ABORT();
    }
    }

#if (DEBUG_NAME_QUALIFICATION_LEVEL > 3) || 0
    MLOG_WARN_C(MLOG_UNPARSER,
                "templateParameter = %p template_parameter_name "
                "(globalUnparseToString()) = %s \n",
                templateParameter, template_parameter_name.c_str());
#endif
    // template_name += template_parameter_name;
    if (template_parameter_name_has_been_output == false) {
      template_name += template_parameter_name;
    }
    i++;

    if (i != templateParameterList.end())
      template_name += ",";
  }

  template_name += " > ";

  return template_name;
}

// DQ (3/31/2014): Adding support for global qualifiction.
size_t NameQualificationTraversal::depthOfGlobalNameQualification(
    SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  size_t depthOfNameQualification = 0;

  SgScopeStatement *scope = declaration->get_scope();
  while (isSgGlobal(scope) == NULL) {
    if (scope->isNamedScope() == true) {
      depthOfNameQualification++;
    }

    scope = scope->get_scope();
  }

  return depthOfNameQualification;
}

// DQ (1/24/2019): display accumulated private base class map.
void NameQualificationTraversal::displayBaseClassMap(const string &label,
                                                     BaseClassSetMap &x) {
  // std::map<SgClassDeclaration*,std::set<SgClassDeclaration*> >
  // privateBaseClassSets );

  MLOG_WARN_C(MLOG_UNPARSER, "In displayBaseClassMap(): label = %s \n",
              label.c_str());

  // std::map<SgClassDeclaration*,std::set<SgClassDeclaration*> >::iterator i =
  // x.begin();
  BaseClassSetMap::iterator i = x.begin();
  while (i != x.end()) {
    SgClassDeclaration *derivedClassDeclaration = i->first;
    std::set<SgClassDeclaration *> &privateBaseClasses = i->second;

    MLOG_WARN_C(
        MLOG_UNPARSER, "  --- derivedClassDeclaration = %p = %s name = %s \n",
        derivedClassDeclaration, derivedClassDeclaration->class_name().c_str(),
        derivedClassDeclaration->get_name().str());
    MLOG_WARN_C(MLOG_UNPARSER, "  --- privateBaseClasses.size() = %zu \n",
                privateBaseClasses.size());

    std::set<SgClassDeclaration *>::const_iterator j =
        privateBaseClasses.begin();
    while (j != privateBaseClasses.end()) {
      SgClassDeclaration *privateBaseClassDeclaration = *j;
      ASSERT_not_null(privateBaseClassDeclaration);

      MLOG_WARN_C(
          MLOG_UNPARSER,
          "  --- --- privateBaseClassDeclaration = %p = %s name = %s \n",
          privateBaseClassDeclaration,
          privateBaseClassDeclaration->class_name().c_str(),
          privateBaseClassDeclaration->get_name().str());

      j++;
    }
    i++;
  }

  MLOG_WARN_C(MLOG_UNPARSER, "Leaving displayBaseClassMap(): label = %s \n",
              label.c_str());
}

// DQ (2/17/2019): Moved this to the name qualification file so we can work on
// it more easily.
bool SgScopeStatement::hasAmbiguity(SgName &name, SgSymbol *symbol) {
  // DQ (2/16/2019): Added to support detection of ambiguity that drives the
  // generation of name qualification.

  // NOTE: in the case where the declaration associated with the symbol is
  // declared in the current scope, we can't have any ambiguity.

#define DEBUG_HAS_AMBIGUITY 0

#if DEBUG_HAS_AMBIGUITY
  MLOG_WARN_C(
      MLOG_UNPARSER,
      "\nIn SgScopeStatement::hasAmbiguity(): name = %s symbol = %p = %s \n",
      name.str(), symbol, symbol->class_name().c_str());
#endif

#if DEBUG_HAS_AMBIGUITY
  MLOG_WARN_C(MLOG_UNPARSER,
              "Print the symbol table for the current scope = %p = %s \n", this,
              this->class_name().c_str());
  this->get_symbol_table()->print();
#endif

  // DQ (2/23/2019): This might be a possible alternative way (maybe a better
  // way) to detect possible ambiguity.
  size_t numberOfSymbols = this->count_symbol(name);

#if DEBUG_HAS_AMBIGUITY
  MLOG_WARN_C(MLOG_UNPARSER, "numberOfSymbols = %zu \n", numberOfSymbols);
#endif

  size_t numberOfAliasSymbols = this->count_alias_symbol(name);

#if DEBUG_HAS_AMBIGUITY
  MLOG_WARN_C(MLOG_UNPARSER, "numberOfAliasSymbols = %zu \n",
              numberOfAliasSymbols);
#endif

  bool ambiguityDetected = false;
  if (numberOfAliasSymbols > 1) {
    // Detected ambiguity that will require some name qualification.

    // If there are multiple SgAliasSymbols then we need to know if they are
    // associated with the same base class or different base classes. If all
    // from the same base class then there is no ambiguity. else if they are
    // from multiple base classes then it is the derivation that is providing
    // the possible ambiguity, which should be resolved via additional name
    // qualification.

    std::vector<SgNode *> causalNodeList;

    rose_hash_multimap *internal_table = this->get_symbol_table()->get_table();
    ASSERT_not_null(internal_table);

#if DEBUG_HAS_AMBIGUITY
    MLOG_WARN_C(MLOG_UNPARSER, "Before loop over symbols \n");
#endif
    std::pair<rose_hash_multimap::iterator, rose_hash_multimap::iterator>
        range = internal_table->equal_range(name);
    for (rose_hash_multimap::iterator i = range.first; i != range.second; ++i) {
      SgSymbol *orig_current_symbol = i->second;
      ASSERT_not_null(orig_current_symbol);

#if DEBUG_HAS_AMBIGUITY
      MLOG_WARN_C(MLOG_UNPARSER, "Top of loop over symbols \n");
      MLOG_WARN_C(MLOG_UNPARSER, "@@@@@@ orig_current_symbol = %p = %s \n",
                  orig_current_symbol,
                  orig_current_symbol->class_name().c_str());
#endif
      SgAliasSymbol *aliasSymbol = isSgAliasSymbol(orig_current_symbol);
      if (aliasSymbol != NULL) {
        size_t causalNodeCount = aliasSymbol->get_causal_nodes().size();
#if DEBUG_HAS_AMBIGUITY
        MLOG_WARN_C(MLOG_UNPARSER, " --- causalNodeCount = %zu \n",
                    causalNodeCount);
#endif
        if (causalNodeCount == 1) {
          // We need to know if each of the alias symbols has a different causal
          // node.
          SgNode *causalNode = aliasSymbol->get_causal_nodes()[0];
#if DEBUG_HAS_AMBIGUITY
          MLOG_WARN_C(MLOG_UNPARSER, " --- causalNode = %p \n", causalNode);
          MLOG_WARN_C(MLOG_UNPARSER,
                      " --- "
                      "find(causalNodeList.begin(),causalNodeList.end(),"
                      "causalNode) == causalNodeList.end() = %s \n",
                      find(causalNodeList.begin(), causalNodeList.end(),
                           causalNode) == causalNodeList.end()
                          ? "true"
                          : "false");
#endif
          if (find(causalNodeList.begin(), causalNodeList.end(), causalNode) ==
              causalNodeList.end()) {
            causalNodeList.push_back(causalNode);
          }
        } else if (causalNodeCount > 1) {
          // We have identified an ambiguity.
#if DEBUG_HAS_AMBIGUITY
          MLOG_WARN_C(MLOG_UNPARSER,
                      " --- We have identified an ambiguity (causalNodeCount > "
                      "1): causalNodeCount = %zu \n",
                      causalNodeCount);
#endif
          ambiguityDetected = true;
        } else {
          // Some alias symbols do not retain causal-node provenance.
          // Treat these as non-proven disambiguation evidence and fall back to
          // the surrounding symbol-count checks below instead of asserting.
        }
      } else {
        // DQ (2/17/2019): This case should be addressed.
        // I think this means that there is no ambiguity, since it would be
        // through a single class (else the base class is mixing alias symbols
        // with the non-alias symbols and it is less clear if there is an
        // ambiguity (but there still could be and we would not detect it). I
        // need a test code to demonstrate this before it can be properly
        // addressed.
#if DEBUG_HAS_AMBIGUITY
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Note: In SgScopeStatement::hasAmbiguity(): Found a non "
                    "SgAliasSymbol: orig_current_symbol = %p = %s \n",
                    orig_current_symbol,
                    orig_current_symbol->class_name().c_str());
#endif
      }
#if DEBUG_HAS_AMBIGUITY
      MLOG_WARN_C(MLOG_UNPARSER,
                  "Bottom of loop over symbols: causalNodeList.size() = %zu \n",
                  causalNodeList.size());
#endif
    }

#if DEBUG_HAS_AMBIGUITY
    MLOG_WARN_C(MLOG_UNPARSER,
                "After loop over symbols: causalNodeList.size() = %zu \n",
                causalNodeList.size());
#endif

    if (causalNodeList.size() > 1) {
      ambiguityDetected = true;
    }

#if DEBUG_HAS_AMBIGUITY
    MLOG_WARN_C(MLOG_UNPARSER, "Using count_alias_symbol(): Detected ambiguity "
                               "that will require some name qualification \n");
#endif
    // ambiguityDetected = true;
  } else {
    // No ambiguity that will require any name qualification.

#if DEBUG_HAS_AMBIGUITY
    MLOG_WARN_C(MLOG_UNPARSER, "Using count_alias_symbol(): No ambiguity that "
                               "will require any name qualification \n");
#endif
    // Lookup the SgAliasSymbol in the class scope.
    SgAliasSymbol *aliasSymbol = this->lookup_alias_symbol(name, symbol);
    // ASSERT_not_null(aliasSymbol);
    if (aliasSymbol != NULL) {
      if (aliasSymbol->get_causal_nodes().empty() == true) {
        if (numberOfSymbols > 1) {
          ambiguityDetected = true;
        }
      } else if (aliasSymbol->get_causal_nodes().size() > 1) {
        // Detected ambiguity that will require some name qualification.

#if DEBUG_HAS_AMBIGUITY
        MLOG_WARN_C(
            MLOG_UNPARSER,
            "Detected ambiguity that will require some name qualification \n");
#endif
        ambiguityDetected = true;
      } else {
        if (numberOfSymbols > 1) {
#if DEBUG_HAS_AMBIGUITY
          MLOG_WARN_C(MLOG_UNPARSER,
                      "Detected multible SgSymbol are available: consider this "
                      "an ambiguity: numberOfSymbols = %zu \n",
                      numberOfSymbols);
#endif
          ambiguityDetected = true;
        } else {
          // No ambiguity that will require any name qualification.

#if DEBUG_HAS_AMBIGUITY
          MLOG_WARN_C(
              MLOG_UNPARSER,
              "No ambiguity that will require any name qualification \n");
#endif
        }
      }
    } else {
#if DEBUG_HAS_AMBIGUITY
      MLOG_WARN_C(MLOG_UNPARSER, "No SgAliasSymbol is available \n");
#endif
      if (numberOfSymbols > 1) {
#if DEBUG_HAS_AMBIGUITY
        MLOG_WARN_C(MLOG_UNPARSER,
                    "Detected multible SgSymbol are available: consider this "
                    "an ambiguity: numberOfSymbols = %zu \n",
                    numberOfSymbols);
#endif
        ambiguityDetected = true;
      } else {
#if DEBUG_HAS_AMBIGUITY
        MLOG_WARN_C(MLOG_UNPARSER,
                    "No SgSymbol or only one SgSymbol is available \n");
#endif
      }
    }
  }

#if DEBUG_HAS_AMBIGUITY
  MLOG_WARN_C(MLOG_UNPARSER, "ambiguityDetected = %s \n",
              ambiguityDetected ? "true" : "false");
#endif

  return ambiguityDetected;
}
