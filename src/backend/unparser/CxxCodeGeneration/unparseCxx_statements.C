/* unparse_stmt.C
 * Contains functions that unparse statements
 *
 * FORMATTING WILL BE DONE IN TWO WAYS:
 * 1. using the file_info object to get information from line and column number
 *    (for original source code)
 * 2. following a specified format that I have specified with indentations of
 *    length TABINDENT (for transformations)
 *
 * REMEMBER: For types and symbols, we still call the original unparse function
 * defined in sage since they dont have file_info. For expressions,
 * Unparse_ExprStmt::unparse is called, and for statements,
 * Unparse_ExprStmt::unparseStatement is called.
 *
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "nameQualificationSupport.h"
#include "unparser.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_set>
#include <vector>

// DQ (8/31/2013):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
// DQ (12/6/2014): Adding support for unparsing from the token stream.
#include "tokenStreamMapping.h"

#define ROSE_TRACK_PROGRESS_OF_ROSE_COMPILING_ROSE 0

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

#define OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES 0
#define OUTPUT_DEBUGGING_FUNCTION_INTERNALS 0

#define OUTPUT_DEBUGGING_UNPARSE_INFO 0

// Output the class name and function names as we unparse (for debugging)
#define OUTPUT_DEBUGGING_CLASS_NAME 0
#define OUTPUT_DEBUGGING_FUNCTION_NAME 0
#define OUTPUT_HIDDEN_LIST_DATA 0

// DQ (2/5/2021): Adding debugging support for token-based unparsing.
#define DEBUG_USING_CURPRINT 0

#define DEBUG_TOKEN_STREAM_UNPARSING 0

#define ENABLE_unparsedPartiallyUsingTokenStream 1

namespace {
std::string cxxSourceFileName(const SgSourceFile *source_file) {
  return source_file != nullptr ? source_file->getFileName() : "<null>";
}

SgSourceFile *requireExactCxxSourceFile(Unparser *unparser,
                                        const SgUnparse_Info &info,
                                        const char *context) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(context);
  SgSourceFile *inherited_source = info.get_current_source_file();
  SgSourceFile *active_source = isSgSourceFile(unparser->currentFile);
  if (inherited_source == nullptr || active_source == nullptr ||
      inherited_source != active_source) {
    const std::string inherited_name = cxxSourceFileName(inherited_source);
    const std::string active_name = cxxSourceFileName(active_source);
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[current-source-file]: context=%s "
            "inherited=%s active=%s must identify the same source file\n",
            context, inherited_name.c_str(), active_name.c_str());
    ROSE_ABORT();
  }
  return inherited_source;
}

void requireExactCxxStatementChild(SgStatement *owner, SgNode *child,
                                   const char *owner_kind,
                                   const char *child_role) {
  ASSERT_not_null(owner);
  ASSERT_not_null(owner_kind);
  ASSERT_not_null(child_role);
  if (child == nullptr || child->get_parent() != owner) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cxx-statement-structure]: %s has no "
            "exactly owned %s\n",
            owner_kind, child_role);
    ROSE_ABORT();
  }
}

void requireExactOptionalCxxStatementChild(SgStatement *owner, SgNode *child,
                                           const char *owner_kind,
                                           const char *child_role) {
  if (child != nullptr) {
    requireExactCxxStatementChild(owner, child, owner_kind, child_role);
  }
}

SgForInitStatement *requireExactForInitializer(SgForStatement *for_statement) {
  ASSERT_not_null(for_statement);
  SgForInitStatement *initializer = for_statement->get_for_init_stmt();
  if (initializer == nullptr || initializer->get_parent() != for_statement) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[for-init-owner]: for statement has no "
            "exactly owned SgForInitStatement\n");
    ROSE_ABORT();
  }

  const SgStatementPtrList &statements = initializer->get_init_stmt();
  if (statements.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[for-init-owner]: for initializer has no "
            "typed statement; syntactic absence requires SgNullStatement\n");
    ROSE_ABORT();
  }
  for (size_t index = 0; index < statements.size(); ++index) {
    if (statements[index] == nullptr ||
        statements[index]->get_parent() != initializer) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[for-init-owner]: for initializer "
              "contains a null or foreign statement at index=%zu\n",
              index);
      ROSE_ABORT();
    }
  }
  return initializer;
}

SgStatement *exactQualificationUseSite(const SgNode *node,
                                       const SgUnparse_Info &info) {
  return exactQualificationUseSiteForEmission(
      node, info.get_template_argument_qualification_context());
}

NameQualificationResult
exactStatementNameQualification(Unparser *unparser, const SgNode *node,
                                const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  return unparser->u_name->lookup_name_qualification(
      node, exactQualificationUseSite(node, info));
}

NameQualificationResult
exactStatementTypeQualification(Unparser *unparser, const SgNode *node,
                                const SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  return unparser->u_name->lookup_type_qualification(
      node, exactQualificationUseSite(node, info));
}

void configureExactFunctionReturnTypeInfo(Unparser *unparser,
                                          SgFunctionDeclaration *function,
                                          SgUnparse_Info &return_type_info) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(function);
  return_type_info.set_reference_node_for_qualification(function);
  return_type_info.set_declstatement_ptr(function);
  if (function->get_requiresNameQualificationOnReturnType()) {
    return_type_info.set_requiresGlobalNameQualification();
  }
  const NameQualificationResult qualification =
      exactStatementTypeQualification(unparser, function, return_type_info);
  return_type_info.set_name_qualification_length(qualification.length);
  return_type_info.set_global_qualification_required(qualification.global);
  return_type_info.set_type_elaboration_required(qualification.typeElaboration);
}

const SgOmpClausePtrList &requiredCxxOmpClauses(SgStatement *owner,
                                                SgOmpClauseList *clause_list) {
  if (owner == nullptr || clause_list == nullptr ||
      clause_list->get_parent() != owner) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openmp-clause-list]: C/C++ statement=%p "
            "has no exact clause-list owner\n",
            static_cast<void *>(owner));
    ROSE_ABORT();
  }
  return clause_list->get_clauses();
}

SgNode *cxxLexicalDeclarationParent(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  SgNode *parent = declaration->get_parent();
  if (SgDeclarationGroupStatement *group =
          isSgDeclarationGroupStatement(parent)) {
    group->validate();
    if (group->get_parent() == nullptr ||
        std::count(group->get_declarations().begin(),
                   group->get_declarations().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: member=%p/%s "
              "has no exact typed lexical owner\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    parent = group->get_parent();
  }
  return parent;
}

bool cxxSourceGroupMemberIsSupported(SgDeclarationStatement *declaration) {
  return isSgVariableDeclaration(declaration) != nullptr ||
         isSgFunctionDeclaration(declaration) != nullptr ||
         isSgTypedefDeclaration(declaration) != nullptr;
}

struct CxxSourceDeclarationGroup {
  SgDeclarationGroupStatement *owner = nullptr;
  std::vector<SgDeclarationStatement *> members;
};

bool cxxSourceGroupTypeUsesDeclaratorSyntax(SgType *type) {
  std::set<SgType *> visited;
  while (true) {
    if (type == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declarator-type]: declaration has a "
              "null type in its modifier chain\n");
      ROSE_ABORT();
    }
    if (!visited.insert(type).second) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[declarator-type]: cyclic modifier type "
              "chain\n");
      ROSE_ABORT();
    }
    SgModifierType *modifier = isSgModifierType(type);
    if (modifier == nullptr) {
      break;
    }
    type = modifier->get_base_type();
  }
  return isSgPointerType(type) != nullptr ||
         isSgPointerMemberType(type) != nullptr ||
         isSgReferenceType(type) != nullptr ||
         isSgRvalueReferenceType(type) != nullptr ||
         isSgArrayType(type) != nullptr || isSgFunctionType(type) != nullptr ||
         isSgPartialFunctionType(type) != nullptr ||
         isSgMemberFunctionType(type) != nullptr;
}

SgType *cxxSourceGroupDeclaredType(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(declaration)) {
    if (variable->get_variables().size() != 1 ||
        variable->get_variables().front() == nullptr ||
        variable->get_variables().front()->get_type() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: variable "
              "member=%p does not own exactly one typed declarator\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    return variable->get_variables().front()->get_type();
  }
  if (SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration)) {
    SgType *returnType = function->get_orig_return_type();
    if (returnType == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: function "
              "member=%p has no exact return type\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    return returnType;
  }
  if (SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(declaration)) {
    if (typedefDeclaration->get_base_type() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: typedef "
              "member=%p has no base type\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    return typedefDeclaration->get_base_type();
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[source-declaration-group]: member=%p/%s "
          "has no supported declared type\n",
          static_cast<void *>(declaration), declaration->class_name().c_str());
  ROSE_ABORT();
}

SgType *
cxxSourceGroupDeclarationSpecifierType(SgDeclarationStatement *declaration) {
  SgType *type = cxxSourceGroupDeclaredType(declaration);
  std::set<SgType *> visited;
  while (type != nullptr && visited.insert(type).second) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      if (!cxxSourceGroupTypeUsesDeclaratorSyntax(modifier->get_base_type())) {
        return modifier;
      }
      type = modifier->get_base_type();
      continue;
    }
    if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
      continue;
    }
    if (SgPointerMemberType *pointer = isSgPointerMemberType(type)) {
      type = pointer->get_base_type();
      continue;
    }
    if (SgReferenceType *reference = isSgReferenceType(type)) {
      type = reference->get_base_type();
      continue;
    }
    if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type)) {
      type = reference->get_base_type();
      continue;
    }
    if (SgArrayType *array = isSgArrayType(type)) {
      type = array->get_base_type();
      continue;
    }
    if (SgFunctionType *function = isSgFunctionType(type)) {
      type = function->get_return_type();
      continue;
    }
    return type;
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[source-declaration-group]: member=%p/%s "
          "has a null or cyclic declarator type\n",
          static_cast<void *>(declaration), declaration->class_name().c_str());
  ROSE_ABORT();
}

bool cxxSourceGroupDeclarationModifiersMatch(const SgDeclarationModifier &lhs,
                                             const SgDeclarationModifier &rhs) {
  return lhs == rhs && lhs.get_modifierVector() == rhs.get_modifierVector() &&
         lhs.get_typeModifier().get_modifierVector() ==
             rhs.get_typeModifier().get_modifierVector() &&
         lhs.get_accessModifier().get_modifier() ==
             rhs.get_accessModifier().get_modifier() &&
         lhs.get_accessModifier().get_is_explicit() ==
             rhs.get_accessModifier().get_is_explicit() &&
         lhs.get_storageModifier().get_modifier() ==
             rhs.get_storageModifier().get_modifier() &&
         lhs.get_storageModifier().get_thread_local_storage() ==
             rhs.get_storageModifier().get_thread_local_storage() &&
         lhs.get_gnu_attribute_visibility() ==
             rhs.get_gnu_attribute_visibility() &&
         lhs.get_gnu_type_visibility() == rhs.get_gnu_type_visibility();
}

bool cxxSourceGroupDeclarationSpecifierTypesMatch(SgType *lhs, SgType *rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  SgAutoType *lhsAuto = isSgAutoType(lhs);
  SgAutoType *rhsAuto = isSgAutoType(rhs);
  if (lhsAuto != nullptr || rhsAuto != nullptr) {
    return lhsAuto != nullptr && rhsAuto != nullptr &&
           lhsAuto->get_is_constrained() == rhsAuto->get_is_constrained() &&
           lhsAuto->get_source_constraint_spelling() ==
               rhsAuto->get_source_constraint_spelling();
  }
  return SageInterface::isEquivalentType(lhs, rhs);
}

void validateCxxSourceDeclarationGroupContract(
    const CxxSourceDeclarationGroup &group) {
  ROSE_ASSERT(!group.members.empty());
  SgDeclarationStatement *first = group.members.front();
  ASSERT_not_null(first);
  SgType *firstBase = cxxSourceGroupDeclarationSpecifierType(first);
  ASSERT_not_null(firstBase);
  const SgDeclarationModifier &firstModifier = first->get_declarationModifier();
  if (!cxxSourceGroupDeclarationModifiersMatch(
          group.owner->get_declarationModifier(), firstModifier) ||
      group.owner->get_decl_attributes() != first->get_decl_attributes() ||
      group.owner->get_linkage() != first->get_linkage() ||
      group.owner->isExternBrace() != first->isExternBrace()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p does "
            "not publish its first member=%p/%s name=%s common declaration "
            "contract (modifiers=%d attributes=%u/%u linkage='%s'/'%s' "
            "extern-brace=%d/%d)\n",
            static_cast<void *>(group.owner), static_cast<void *>(first),
            first->class_name().c_str(), SageInterface::get_name(first).c_str(),
            cxxSourceGroupDeclarationModifiersMatch(
                group.owner->get_declarationModifier(), firstModifier)
                ? 1
                : 0,
            group.owner->get_decl_attributes(), first->get_decl_attributes(),
            group.owner->get_linkage().c_str(), first->get_linkage().c_str(),
            group.owner->isExternBrace() ? 1 : 0,
            first->isExternBrace() ? 1 : 0);
    ROSE_ABORT();
  }
  SgFunctionDeclaration *firstFunction = isSgFunctionDeclaration(first);
  const bool firstConstexpr =
      firstFunction != nullptr
          ? firstFunction->get_is_constexpr()
          : (isSgVariableDeclaration(first) != nullptr
                 ? isSgVariableDeclaration(first)->get_is_constexpr()
                 : false);

  SgFunctionDeclaration *functionContract = firstFunction;
  for (size_t index = 1; index < group.members.size(); ++index) {
    SgDeclarationStatement *member = group.members[index];
    ASSERT_not_null(member);
    SgType *memberBase = cxxSourceGroupDeclarationSpecifierType(member);
    SgFunctionDeclaration *memberFunction = isSgFunctionDeclaration(member);
    const bool memberConstexpr =
        memberFunction != nullptr
            ? memberFunction->get_is_constexpr()
            : (isSgVariableDeclaration(member) != nullptr
                   ? isSgVariableDeclaration(member)->get_is_constexpr()
                   : false);
    const bool commonContractMatches =
        memberBase != nullptr &&
        cxxSourceGroupDeclarationSpecifierTypesMatch(firstBase, memberBase) &&
        cxxSourceGroupDeclarationModifiersMatch(
            firstModifier, member->get_declarationModifier()) &&
        first->get_linkage() == member->get_linkage() &&
        first->isExternBrace() == member->isExternBrace() &&
        firstConstexpr == memberConstexpr;
    if (!commonContractMatches) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p "
              "member index=%zu has a mismatched common declaration "
              "specifier/base-type/modifier contract\n",
              static_cast<void *>(group.owner), index);
      fprintf(
          stderr,
          "REX_UNPARSE_DETAIL[source-declaration-group]: first-base=%p/%s "
          "member-base=%p/%s equivalent=%d modifiers=%d linkage=%d "
          "extern-brace=%d constexpr=%d\n",
          static_cast<void *>(firstBase), firstBase->class_name().c_str(),
          static_cast<void *>(memberBase),
          memberBase != nullptr ? memberBase->class_name().c_str() : "<null>",
          memberBase != nullptr && cxxSourceGroupDeclarationSpecifierTypesMatch(
                                       firstBase, memberBase)
              ? 1
              : 0,
          cxxSourceGroupDeclarationModifiersMatch(
              firstModifier, member->get_declarationModifier())
              ? 1
              : 0,
          first->get_linkage() == member->get_linkage() ? 1 : 0,
          first->isExternBrace() == member->isExternBrace() ? 1 : 0,
          firstConstexpr == memberConstexpr ? 1 : 0);
      ROSE_ABORT();
    }
    if (memberFunction != nullptr) {
      if (functionContract == nullptr) {
        functionContract = memberFunction;
      } else if (!(functionContract->get_functionModifier() ==
                   memberFunction->get_functionModifier()) ||
                 functionContract->get_using_C11_Noreturn_keyword() !=
                     memberFunction->get_using_C11_Noreturn_keyword()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p "
                "member index=%zu has a mismatched shared function-specifier "
                "contract\n",
                static_cast<void *>(group.owner), index);
        ROSE_ABORT();
      }
    }
  }
}

CxxSourceDeclarationGroup
requireCxxSourceDeclarationGroup(SgDeclarationGroupStatement *owner) {
  ASSERT_not_null(owner);
  owner->validate();

  Sg_File_Info *start = owner->get_startOfConstruct();
  Sg_File_Info *end = owner->get_endOfConstruct();
  if (owner->get_parent() == nullptr || owner->get_scope() == nullptr ||
      start == nullptr || end == nullptr || start->get_physical_line() <= 0 ||
      end->get_physical_line() <= 0 ||
      start->get_physical_file_id() != end->get_physical_file_id()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p has "
            "no exact structural owner, lexical scope, or source range\n",
            static_cast<void *>(owner));
    ROSE_ABORT();
  }

  CxxSourceDeclarationGroup result;
  result.owner = owner;
  for (SgDeclarationStatement *member : owner->get_declarations()) {
    if (!cxxSourceGroupMemberIsSupported(member)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p "
              "contains unsupported member=%p/%s\n",
              static_cast<void *>(owner), static_cast<void *>(member),
              member != nullptr ? member->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(member)) {
      if (!function->isForward() || function->get_definition() != nullptr ||
          function->get_specialFunctionModifier().isConstructor() ||
          function->get_specialFunctionModifier().isDestructor() ||
          function->get_specialFunctionModifier().isConversion() ||
          isSgTemplateFunctionDeclaration(function) != nullptr ||
          isSgTemplateMemberFunctionDeclaration(function) != nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p "
                "contains a function that is not a plain declarator\n",
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
    }
    if (SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(member)) {
      if (typedefDeclaration->get_typedef_type() ==
          SgTypedefDeclaration::e_using) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p "
                "contains a using-alias declarator\n",
                static_cast<void *>(owner));
        ROSE_ABORT();
      }
    }
    result.members.push_back(member);
  }

  const bool anyTypedef =
      std::any_of(result.members.begin(), result.members.end(),
                  [](SgDeclarationStatement *member) {
                    return isSgTypedefDeclaration(member) != nullptr;
                  });
  const bool allTypedefs =
      std::all_of(result.members.begin(), result.members.end(),
                  [](SgDeclarationStatement *member) {
                    return isSgTypedefDeclaration(member) != nullptr;
                  });
  if (anyTypedef != allTypedefs) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p mixes "
            "typedef and non-typedef declaration specifiers\n",
            static_cast<void *>(owner));
    ROSE_ABORT();
  }
  validateCxxSourceDeclarationGroupContract(result);
  return result;
}

TokenStreamSequenceToNodeMapping *lookup_token_subsequence_mapping_for_node(
    SgSourceFile *source_file, SgLocatedNode *node,
    SgLocatedNode **mapped_node_out = nullptr) {
  if (mapped_node_out != nullptr) {
    *mapped_node_out = node;
  }
  if (source_file == nullptr || node == nullptr) {
    return nullptr;
  }

  const auto &token_map = source_file->get_tokenSubsequenceMap();
  auto lookup =
      [&](SgLocatedNode *candidate) -> TokenStreamSequenceToNodeMapping * {
    if (candidate == nullptr) {
      return nullptr;
    }
    const auto found = token_map.find(candidate);
    if (found == token_map.end()) {
      return nullptr;
    }
    if (found->second == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-map]: file=%s node=%s address=%p "
              "contains a null mapping entry\n",
              source_file->getFileName().c_str(),
              candidate->class_name().c_str(), static_cast<void *>(candidate));
      ROSE_ABORT();
    }
    TokenStreamSequenceToNodeMapping *mapping = found->second;
    std::unordered_set<SgNode *> associated_nodes;
    for (SgNode *associated : mapping->nodeVector) {
      const auto associatedMapping = token_map.find(associated);
      if (associated == nullptr ||
          !associated_nodes.insert(associated).second ||
          associatedMapping == token_map.end() ||
          associatedMapping->second != mapping) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[token-surface-owner]: file=%s "
                "mapping=%p has a null, duplicate, or unpublished associated "
                "node\n",
                source_file->getFileName().c_str(),
                static_cast<void *>(mapping));
        ROSE_ABORT();
      }
    }
    if (mapping->node != nullptr && mapping->node != candidate &&
        associated_nodes.find(candidate) == associated_nodes.end()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-surface-owner]: file=%s "
              "statement-type=%s is an alias of token-owner-type=%s\n",
              source_file->getFileName().c_str(),
              candidate->class_name().c_str(),
              mapping->node->class_name().c_str());
      ROSE_ABORT();
    }
    if (mapping->node == nullptr ||
        associated_nodes.find(mapping->node) == associated_nodes.end() ||
        associated_nodes.find(candidate) == associated_nodes.end() ||
        mapping->shared != (associated_nodes.size() > 1)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-surface-owner]: file=%s "
              "statement-type=%s is not an exact member of mapping=%p "
              "owner-type=%s associated-count=%zu shared=%d\n",
              source_file->getFileName().c_str(),
              candidate->class_name().c_str(), static_cast<void *>(mapping),
              mapping->node != nullptr ? mapping->node->class_name().c_str()
                                       : "<null>",
              associated_nodes.size(), mapping->shared ? 1 : 0);
      ROSE_ABORT();
    }
    if (mapped_node_out != nullptr) {
      *mapped_node_out = candidate;
    }
    return mapping;
  };

  if (TokenStreamSequenceToNodeMapping *mapping = lookup(node)) {
    return mapping;
  }
  return nullptr;
}

void requireExactSwitchLabelTokenBoundary(SgSourceFile *source_file,
                                          SgStatement *label, SgStatement *body,
                                          const char *label_kind) {
  ASSERT_not_null(label);
  ASSERT_not_null(body);
  ASSERT_not_null(label_kind);
  if (source_file == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[switch-label-token-boundary]: %s "
            "statement has no current source file for partial replay\n",
            label_kind);
    ROSE_ABORT();
  }

  TokenStreamSequenceToNodeMapping *label_mapping =
      lookup_token_subsequence_mapping_for_node(source_file, label);
  TokenStreamSequenceToNodeMapping *body_mapping =
      lookup_token_subsequence_mapping_for_node(source_file, body);
  if (label_mapping == nullptr || body_mapping == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[switch-label-token-boundary]: %s "
            "statement and its exactly owned body require direct token "
            "mappings for partial replay (label=%s body=%s)\n",
            label_kind, label_mapping != nullptr ? "mapped" : "missing",
            body_mapping != nullptr ? "mapped" : "missing");
    ROSE_ABORT();
  }

  const TokenStreamHalfOpenInterval &label_core =
      label_mapping->halfOpenInterval(
          TokenStreamIntervalKind::token_subsequence);
  const TokenStreamHalfOpenInterval &body_core = body_mapping->halfOpenInterval(
      TokenStreamIntervalKind::token_subsequence);
  if (body_core.begin <= label_core.begin || body_core.begin > label_core.end) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[switch-label-token-boundary]: %s "
            "statement has invalid exact boundary label-core=[%d,%d) "
            "body-core=[%d,%d)\n",
            label_kind, label_core.begin, label_core.end, body_core.begin,
            body_core.end);
    ROSE_ABORT();
  }
}

SgType *stripFunctionReturnTypeForInlineDefinitionCheck(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }

  return type->stripType(SgType::STRIP_MODIFIER_TYPE |
                         SgType::STRIP_REFERENCE_TYPE |
                         SgType::STRIP_RVALUE_REFERENCE_TYPE |
                         SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE);
}

bool declarationNeedsInlineFunctionReturnTypeDefinition(
    SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return false;
  }

  auto is_function_signature_owned_decl =
      [](SgDeclarationStatement *candidate) -> bool {
    if (candidate == nullptr) {
      return false;
    }

    SgNode *parent = candidate->get_parent();
    return isSgFunctionDeclaration(parent) != nullptr ||
           isSgMemberFunctionDeclaration(parent) != nullptr ||
           isSgFunctionParameterScope(parent) != nullptr ||
           isSgDeclarationScope(parent) != nullptr;
  };

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
            class_decl->get_firstNondefiningDeclaration())) {
      class_decl = first_nondef;
    }
    if (SgClassDeclaration *def_decl =
            isSgClassDeclaration(class_decl->get_definingDeclaration())) {
      if (def_decl->get_definition() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[inline-return-type]: class defining "
                "declaration=%p has no definition\n",
                static_cast<void *>(def_decl));
        ROSE_ABORT();
      }
      if (is_function_signature_owned_decl(def_decl) &&
          (!def_decl->get_isAutonomousDeclaration() ||
           def_decl->get_isUnNamed())) {
        return true;
      }
    }
    return false;
  }

  if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl)) {
    if (SgEnumDeclaration *first_nondef =
            isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration())) {
      enum_decl = first_nondef;
    }
    if (SgEnumDeclaration *def_decl =
            isSgEnumDeclaration(enum_decl->get_definingDeclaration())) {
      if (is_function_signature_owned_decl(def_decl) &&
          (!def_decl->get_isAutonomousDeclaration() ||
           def_decl->get_isUnNamed())) {
        return true;
      }
    }
    return false;
  }

  return false;
}

bool nodeHasTransformation(SgNode *node) {
  SgLocatedNode *located = isSgLocatedNode(node);
  return located != nullptr &&
         (located->isTransformation() ||
          located->get_containsTransformation() ||
          located->get_containsTransformationToSurroundingWhitespace());
}

SgStatement *requiredOwnedCxxStatementListEntry(SgBasicBlock *owner,
                                                size_t index) {
  ASSERT_not_null(owner);
  const SgStatementPtrList &statements = owner->get_statements();
  ASSERT_require(index < statements.size());
  SgStatement *statement = statements[index];
  if (statement == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[owned-statement-list]: "
            "owner=SgBasicBlock has null entry at index=%zu\n",
            index);
    ROSE_ABORT();
  }
  return statement;
}

void requireExactOwnedCxxStatementList(SgBasicBlock *owner) {
  ASSERT_not_null(owner);
  const SgStatementPtrList &statements = owner->get_statements();
  std::map<SgStatement *, size_t> occurrences;
  for (size_t index = 0; index < statements.size(); ++index) {
    SgStatement *statement = statements[index];
    if (statement == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-statement-list]: "
              "owner=SgBasicBlock has null entry at index=%zu\n",
              index);
      ROSE_ABORT();
    }
    ++occurrences[statement];
  }

  for (size_t index = 0; index < statements.size(); ++index) {
    SgStatement *statement = statements[index];
    const size_t count = occurrences.at(statement);
    if (statement->get_parent() != owner || count != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[owned-statement-list]: "
              "owner=SgBasicBlock entry index=%zu type=%s has parent=%s "
              "and occurrence-count=%zu\n",
              index, statement->class_name().c_str(),
              statement->get_parent() != nullptr
                  ? statement->get_parent()->class_name().c_str()
                  : "<null>",
              count);
      ROSE_ABORT();
    }
  }
}

PreprocessingInfo *requiredAttachedPreprocessingInfoEntry(
    const AttachedPreprocessingInfoType &attached, size_t index) {
  ASSERT_require(index < attached.size());
  PreprocessingInfo *info = attached[index];
  if (info == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[attached-preprocessing-info-list]: null "
            "entry at index=%zu\n",
            index);
    ROSE_ABORT();
  }
  return info;
}

void validateAttachedPreprocessingInfoList(
    const AttachedPreprocessingInfoType *attached) {
  if (attached == nullptr) {
    return;
  }
  for (size_t index = 0; index < attached->size(); ++index) {
    (void)requiredAttachedPreprocessingInfoEntry(*attached, index);
  }
}

bool functionReturnTypeNeedsInlineDefinition(SgType *type) {
  SgType *stripped_type = stripFunctionReturnTypeForInlineDefinitionCheck(type);
  if (stripped_type == nullptr) {
    return false;
  }

  if (SgClassType *class_type = isSgClassType(stripped_type)) {
    return declarationNeedsInlineFunctionReturnTypeDefinition(
        class_type->get_declaration());
  }

  if (SgEnumType *enum_type = isSgEnumType(stripped_type)) {
    return declarationNeedsInlineFunctionReturnTypeDefinition(
        enum_type->get_declaration());
  }

  return false;
}

bool parameterTypeNeedsInlineDefinition(SgType *type) {
  return functionReturnTypeNeedsInlineDefinition(type);
}

bool hasConcreteFileLocation(const Sg_File_Info *file_info) {
  return file_info != nullptr && !file_info->isCompilerGenerated() &&
         file_info->get_file_id() >= 0 && file_info->get_line() > 0;
}

void requireDirectSourceSurfaceOwnership(SgDeclarationStatement *declaration,
                                         const char *unparse_context) {
  if (declaration == nullptr || unparse_context == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-surface-ownership]: context=%s "
            "declaration=%p\n",
            unparse_context != nullptr ? unparse_context : "<null>",
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }

  Sg_File_Info *file_info = declaration->get_file_info();
  if (file_info == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-surface-ownership]: context=%s "
            "declaration=%p declaration-type=%s has no file info\n",
            unparse_context, static_cast<void *>(declaration),
            declaration->class_name().c_str());
    ROSE_ABORT();
  }

  if (hasConcreteFileLocation(file_info) &&
      isSgScopeStatement(declaration->get_parent()) != nullptr &&
      !file_info->isOutputInCodeGeneration() &&
      isSgTemplateInstantiationDirectiveStatement(declaration->get_parent()) ==
          nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-surface-ownership]: context=%s "
            "declaration=%p declaration-type=%s file=%s line=%d col=%d "
            "directly owned source surface is hidden from code generation\n",
            unparse_context, static_cast<void *>(declaration),
            declaration->class_name().c_str(),
            file_info->get_filenameString().c_str(), file_info->get_line(),
            file_info->get_col());
    ROSE_ABORT();
  }
}

bool locatedNodesOriginallySeparatedByLineBreak(const SgLocatedNode *previous,
                                                const SgLocatedNode *current) {
  if (previous == nullptr || current == nullptr) {
    return false;
  }

  const Sg_File_Info *previous_start = previous->get_startOfConstruct();
  const Sg_File_Info *current_start = current->get_startOfConstruct();
  return hasConcreteFileLocation(previous_start) &&
         hasConcreteFileLocation(current_start) &&
         previous_start->isSameFile(*current_start) &&
         current_start->get_line() > previous_start->get_line();
}

bool locatedNodeHasAttachedPreprocessingInfo(const SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  AttachedPreprocessingInfoType *attached =
      const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo();
  validateAttachedPreprocessingInfoList(attached);
  return attached != nullptr && !attached->empty();
}

bool locatedNodeHasBeforePreprocessingInfo(const SgLocatedNode *node) {
  AttachedPreprocessingInfoType *attached =
      node != nullptr
          ? const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo()
          : nullptr;
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == PreprocessingInfo::before) {
      return true;
    }
  }

  return false;
}

bool locatedNodeHasInsidePreprocessingInfo(const SgLocatedNode *node) {
  AttachedPreprocessingInfoType *attached =
      node != nullptr
          ? const_cast<SgLocatedNode *>(node)->getAttachedPreprocessingInfo()
          : nullptr;
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == PreprocessingInfo::inside) {
      return true;
    }
  }

  return false;
}

bool isConditionalPreprocessingDirective(
    PreprocessingInfo::DirectiveType type) {
  switch (type) {
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
    return true;
  default:
    return false;
  }
}

SgLocatedNode *
sourceDeclarationGroupBoundaryOwner(SgDeclarationStatement *member) {
  ASSERT_not_null(member);
  if (SgVariableDeclaration *variable = isSgVariableDeclaration(member)) {
    if (variable->get_variables().size() != 1 ||
        variable->get_variables().front() == nullptr ||
        variable->get_variables().front()->get_parent() != variable) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: variable "
              "member=%p has no exact initialized-name boundary\n",
              static_cast<void *>(member));
      ROSE_ABORT();
    }
    return variable->get_variables().front();
  }
  return member;
}

bool tokenLexemeSpellsConditionalPreprocessingDirective(
    const std::string &lexeme) {
  std::string trimmed = Rose::StringUtility::trim(lexeme);
  if (trimmed.empty() || trimmed[0] != '#') {
    return false;
  }

  size_t cursor = 1;
  while (cursor < trimmed.size() &&
         std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0) {
    ++cursor;
  }

  const std::string directive = trimmed.substr(cursor);
  auto matches = [&](const std::string &name) {
    if (directive.rfind(name, 0) != 0) {
      return false;
    }
    return directive.size() == name.size() ||
           std::isspace(static_cast<unsigned char>(directive[name.size()])) !=
               0;
  };

  return matches("if") || matches("ifdef") || matches("ifndef") ||
         matches("elif") || matches("else") || matches("endif");
}

bool tokenIntervalContainsConditionalPreprocessingDirective(
    SgSourceFile *source_file,
    const TokenStreamSequenceToNodeMapping *mapping) {
  if (source_file == nullptr || mapping == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[token-interval-query]: conditional "
                    "preprocessing query has no source file or mapping\n");
    ROSE_ABORT();
  }

  SgTokenPtrList &tokens = source_file->get_token_list();
  const TokenStreamHalfOpenInterval &leading =
      mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace);
  const TokenStreamHalfOpenInterval &trailing =
      mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace);
  const int begin = leading.begin;
  const int end = trailing.end;
  if (begin < 0 || end < begin || end > static_cast<int>(tokens.size())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-interval-query]: file=%s exact "
            "interval [%d,%d) is outside [0,%zu)\n",
            source_file->getFileName().c_str(), begin, end, tokens.size());
    ROSE_ABORT();
  }

  for (int index = begin; index < end; ++index) {
    SgToken *token = tokens[index];
    if (token == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-interval-query]: file=%s exact "
              "interval [%d,%d) contains null token=%d\n",
              source_file->getFileName().c_str(), begin, end, index);
      ROSE_ABORT();
    }
    if (token->get_classification_code() !=
        ROSE_token_ids::C_CXX_PREPROCESSING_INFO) {
      continue;
    }
    if (tokenLexemeSpellsConditionalPreprocessingDirective(
            token->get_lexeme_string())) {
      return true;
    }
  }

  return false;
}

bool basicBlockStartsWithLeadingPreprocessingInfo(const SgBasicBlock *block) {
  if (block == nullptr || block->get_statements().empty()) {
    return false;
  }

  SgStatement *first_statement =
      requiredOwnedCxxStatementListEntry(const_cast<SgBasicBlock *>(block), 0);
  AttachedPreprocessingInfoType *attached =
      first_statement->getAttachedPreprocessingInfo();
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == PreprocessingInfo::before) {
      return true;
    }
  }

  return false;
}

bool functionBodyOriginallyUsedSameLineOpeningBrace(const SgBasicBlock *block) {
  SgFunctionDefinition *function_definition =
      block != nullptr ? isSgFunctionDefinition(block->get_parent()) : nullptr;
  SgFunctionDeclaration *function_decl =
      function_definition != nullptr
          ? isSgFunctionDeclaration(function_definition->get_declaration())
          : nullptr;
  SgFunctionParameterList *parameter_list =
      function_decl != nullptr ? function_decl->get_parameterList() : nullptr;
  Sg_File_Info *body_start =
      block != nullptr ? block->get_startOfConstruct() : nullptr;
  Sg_File_Info *header_end = parameter_list != nullptr
                                 ? parameter_list->get_endOfConstruct()
                                 : nullptr;
  Sg_File_Info *decl_start = function_decl != nullptr
                                 ? function_decl->get_startOfConstruct()
                                 : nullptr;
  if (!hasConcreteFileLocation(body_start)) {
    return false;
  }

  if (hasConcreteFileLocation(header_end) &&
      body_start->isSameFile(header_end) &&
      body_start->get_line() == header_end->get_line()) {
    return true;
  }

  return hasConcreteFileLocation(decl_start) &&
         body_start->isSameFile(decl_start) &&
         body_start->get_line() == decl_start->get_line();
}

bool statementUsesCompactFunctionBodyLayout(const SgStatement *statement);

bool functionBodyOriginallyUsedCompactSingleLineLayout(
    const SgBasicBlock *block) {
  SgFunctionDefinition *function_definition =
      block != nullptr ? isSgFunctionDefinition(block->get_parent()) : nullptr;
  if (block == nullptr || function_definition == nullptr) {
    return false;
  }

  const SgStatementPtrList &statements = block->get_statements();
  if (statements.size() != 1 ||
      !statementUsesCompactFunctionBodyLayout(statements.front())) {
    return false;
  }

  Sg_File_Info *body_start = block->get_startOfConstruct();
  Sg_File_Info *body_end = block->get_endOfConstruct();
  Sg_File_Info *stmt_start = statements.front()->get_startOfConstruct();
  Sg_File_Info *stmt_end = statements.front()->get_endOfConstruct();
  if (!hasConcreteFileLocation(body_start) ||
      !hasConcreteFileLocation(body_end) ||
      !hasConcreteFileLocation(stmt_start) ||
      !hasConcreteFileLocation(stmt_end) ||
      !body_start->isSameFile(stmt_start) ||
      !body_start->isSameFile(stmt_end) || !body_start->isSameFile(body_end)) {
    return false;
  }

  return body_start->get_line() == stmt_start->get_line() &&
         body_start->get_line() == stmt_end->get_line() &&
         body_start->get_line() == body_end->get_line();
}

bool functionBodyPrefersSimpleSameLineOpeningBrace(const SgBasicBlock *block) {
  if (block == nullptr || block->get_statements().empty() ||
      block->isTransformation() || block->get_containsTransformation() ||
      block->get_containsTransformationToSurroundingWhitespace() ||
      locatedNodeHasAttachedPreprocessingInfo(
          const_cast<SgBasicBlock *>(block)) ||
      basicBlockStartsWithLeadingPreprocessingInfo(block)) {
    return false;
  }

  int top_level_control_statements = 0;
  for (size_t index = 0; index < block->get_statements().size(); ++index) {
    SgStatement *statement = requiredOwnedCxxStatementListEntry(
        const_cast<SgBasicBlock *>(block), index);

    switch (statement->variantT()) {
    case V_SgIfStmt:
    case V_SgForStatement:
    case V_SgRangeBasedForStatement:
    case V_SgWhileStmt:
    case V_SgDoWhileStmt:
    case V_SgSwitchStatement:
      ++top_level_control_statements;
      break;

    default:
      break;
    }
  }

  if (top_level_control_statements != 1) {
    return false;
  }

  const SgNodePtrList &while_nodes =
      NodeQuery::querySubTree(const_cast<SgBasicBlock *>(block), V_SgWhileStmt);
  const SgNodePtrList &switch_nodes = NodeQuery::querySubTree(
      const_cast<SgBasicBlock *>(block), V_SgSwitchStatement);
  if (!while_nodes.empty() || !switch_nodes.empty()) {
    return false;
  }

  const size_t loop_count =
      NodeQuery::querySubTree(const_cast<SgBasicBlock *>(block),
                              V_SgForStatement)
          .size() +
      NodeQuery::querySubTree(const_cast<SgBasicBlock *>(block),
                              V_SgRangeBasedForStatement)
          .size() +
      NodeQuery::querySubTree(const_cast<SgBasicBlock *>(block),
                              V_SgDoWhileStmt)
          .size();
  return loop_count <= 1;
}

bool shouldEmitElseOnSameLine(SgIfStmt *if_stmt) {
  if (if_stmt == nullptr) {
    return false;
  }

  SgStatement *true_body = if_stmt->get_true_body();
  SgStatement *false_body = if_stmt->get_false_body();
  if (true_body == nullptr || false_body == nullptr) {
    return false;
  }

  if (isSgBasicBlock(true_body) != nullptr) {
    return true;
  }

  Sg_File_Info *true_end = true_body->get_endOfConstruct();
  Sg_File_Info *false_start = false_body->get_startOfConstruct();
  if (hasConcreteFileLocation(true_end) &&
      hasConcreteFileLocation(false_start) &&
      true_end->isSameFile(false_start)) {
    return true_end->get_line() == false_start->get_line();
  }

  return false;
}

bool isClassAccessLabelStatement(const SgStatement *stmt) {
  const SgAccessLabelStatement *label = isSgAccessLabelStatement(stmt);
  if (label == nullptr || isSgClassDefinition(label->get_parent()) == nullptr) {
    return false;
  }
  label->validate();
  return true;
}

bool classMemberHasPreviousSibling(const SgStatement *stmt) {
  if (stmt == nullptr) {
    return false;
  }

  const SgClassDefinition *class_definition =
      isSgClassDefinition(stmt->get_parent());
  if (class_definition == nullptr) {
    return false;
  }

  const SgDeclarationStatementPtrList &members =
      class_definition->get_members();
  SgDeclarationStatementPtrList::const_iterator position =
      std::find(members.begin(), members.end(),
                isSgDeclarationStatement(const_cast<SgStatement *>(stmt)));
  return position != members.end() && position != members.begin();
}

bool isNamespaceOrGlobalScope(const SgNode *node) {
  return isSgNamespaceDefinitionStatement(node) != nullptr ||
         isSgGlobal(node) != nullptr;
}

bool declarationNeedsLeadingBlankLine(const SgStatement *stmt) {
  const SgClassDeclaration *class_decl = isSgClassDeclaration(stmt);
  const SgClassDeclaration *defining_decl =
      class_decl != nullptr
          ? isSgClassDeclaration(class_decl->get_definingDeclaration())
          : nullptr;
  if (class_decl == nullptr || defining_decl == nullptr ||
      defining_decl != class_decl || class_decl->get_definition() == nullptr) {
    return false;
  }

  if (isSgTemplateClassDeclaration(class_decl) != nullptr ||
      isSgTemplateInstantiationDecl(class_decl) != nullptr) {
    return false;
  }

  return isNamespaceOrGlobalScope(class_decl->get_scope());
}

bool statementUsesCompactFunctionBodyLayout(const SgStatement *statement) {
  if (statement == nullptr || statement->isTransformation() ||
      statement->get_containsTransformation() ||
      statement->get_containsTransformationToSurroundingWhitespace() ||
      locatedNodeHasAttachedPreprocessingInfo(
          isSgLocatedNode(const_cast<SgStatement *>(statement)))) {
    return false;
  }

  switch (statement->variantT()) {
  case V_SgExprStatement:
  case V_SgReturnStmt:
  case V_SgBreakStmt:
  case V_SgContinueStmt:
  case V_SgNullStatement:
    return true;

  default:
    return false;
  }
}

bool functionBodyUsesInlineSameLineOpeningBrace(const SgBasicBlock *block) {
  if (block == nullptr || block->isTransformation() ||
      block->get_containsTransformation() ||
      block->get_containsTransformationToSurroundingWhitespace() ||
      locatedNodeHasAttachedPreprocessingInfo(
          const_cast<SgBasicBlock *>(block)) ||
      basicBlockStartsWithLeadingPreprocessingInfo(block)) {
    return false;
  }

  SgFunctionDefinition *function_definition =
      isSgFunctionDefinition(block->get_parent());
  if (function_definition == nullptr) {
    return false;
  }

  SgFunctionDeclaration *function_decl =
      isSgFunctionDeclaration(function_definition->get_declaration());
  if (function_decl == nullptr) {
    return false;
  }

  return isSgMemberFunctionDeclaration(function_decl) != nullptr ||
         isSgTemplateFunctionDeclaration(function_decl) != nullptr ||
         isSgTemplateMemberFunctionDeclaration(function_decl) != nullptr;
}

bool basicBlockUsesCompactFunctionBodyLayout(const SgBasicBlock *block,
                                             const SgUnparse_Info &info) {
  if (info.SkipFormatting()) {
    return false;
  }
  SgFunctionDefinition *function_definition =
      block != nullptr ? isSgFunctionDefinition(block->get_parent()) : nullptr;
  const bool preserve_original_compact_layout =
      functionBodyOriginallyUsedCompactSingleLineLayout(block);
  if (block == nullptr || function_definition == nullptr ||
      block->isTransformation() || block->get_containsTransformation() ||
      block->get_containsTransformationToSurroundingWhitespace() ||
      locatedNodeHasAttachedPreprocessingInfo(
          const_cast<SgBasicBlock *>(block)) ||
      basicBlockStartsWithLeadingPreprocessingInfo(block)) {
    return false;
  }

  const SgStatementPtrList &statements = block->get_statements();
  if (statements.empty()) {
    Sg_File_Info *body_start = block->get_startOfConstruct();
    Sg_File_Info *body_end = block->get_endOfConstruct();
    return hasConcreteFileLocation(body_start) &&
           hasConcreteFileLocation(body_end) &&
           body_start->isSameFile(body_end) &&
           body_start->get_line() == body_end->get_line();
  }
  if (statements.size() != 1) {
    return false;
  }

  return preserve_original_compact_layout &&
         statementUsesCompactFunctionBodyLayout(statements.front());
}

void unparseCompactFunctionBodyStatement(Unparse_ExprStmt *unparser,
                                         SgStatement *statement,
                                         SgUnparse_Info &info) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(statement);

  SgUnparse_Info statement_info(info);
  statement_info.set_template_argument_qualification_context(statement);

  switch (statement->variantT()) {
  case V_SgExprStatement:
    unparser->unparseExprStmt(statement, statement_info);
    break;

  case V_SgReturnStmt:
    unparser->unparseReturnStmt(statement, statement_info);
    break;

  case V_SgBreakStmt:
    unparser->unparseBreakStmt(statement, statement_info);
    break;

  case V_SgContinueStmt:
    unparser->unparseContinueStmt(statement, statement_info);
    break;

  case V_SgNullStatement:
    unparser->unparseNullStatement(statement, statement_info);
    break;

  default:
    ROSE_ABORT();
  }
}

bool functionCanKeepCompactPartialTokenHeader(
    const SgFunctionDeclaration *function_decl, const SgUnparse_Info &info) {
  if (function_decl == nullptr) {
    return false;
  }

  SgSourceFile *sourcefile = info.get_current_source_file();
  if (sourcefile == nullptr || !sourcefile->get_unparse_tokens()) {
    return false;
  }

  if (function_decl->isTransformation() ||
      function_decl->get_containsTransformationToSurroundingWhitespace()) {
    return false;
  }

  if (function_decl->get_parameterList() != nullptr &&
      nodeHasTransformation(function_decl->get_parameterList())) {
    return false;
  }

  const SgFunctionDefinition *definition = function_decl->get_definition();
  if (definition != nullptr) {
    SgFunctionDefinition *mutable_definition =
        const_cast<SgFunctionDefinition *>(definition);
    if (mutable_definition->isTransformation() ||
        mutable_definition
            ->get_containsTransformationToSurroundingWhitespace()) {
      return false;
    }

    SgBasicBlock *body = mutable_definition->get_body();
    if (body != nullptr &&
        (body->isTransformation() ||
         body->get_containsTransformationToSurroundingWhitespace())) {
      return false;
    }
  }

  return true;
}

bool useStatementFormattingForStandaloneBasicBlock(
    const SgBasicBlock *basic_block) {
  if (basic_block == nullptr) {
    return false;
  }

  SgNode *parent = basic_block->get_parent();
  const bool parent_owns_block_layout =
      isSgFunctionDefinition(parent) != nullptr ||
      isSgForStatement(parent) != nullptr ||
      isSgRangeBasedForStatement(parent) != nullptr ||
      isSgWhileStmt(parent) != nullptr || isSgDoWhileStmt(parent) != nullptr ||
      isSgIfStmt(parent) != nullptr || isSgSwitchStatement(parent) != nullptr ||
      isSgTryStmt(parent) != nullptr ||
      isSgCatchOptionStmt(parent) != nullptr ||
      isSgClassDefinition(parent) != nullptr ||
      isSgNamespaceDefinitionStatement(parent) != nullptr;
  if (parent_owns_block_layout || isSgStatement(parent) == nullptr) {
    return false;
  }

  // The parent statement list has already selected the indentation for this
  // exact structural child. Transformation provenance changes token ownership,
  // not the block's layout ownership; reformatting a transformed block again
  // here discards the indentation established by its parent.
  return true;
}

bool classDefinitionHasInsidePreprocessingInfo(
    const SgClassDefinition *class_defn) {
  AttachedPreprocessingInfoType *attached =
      class_defn != nullptr ? const_cast<SgClassDefinition *>(class_defn)
                                  ->getAttachedPreprocessingInfo()
                            : nullptr;
  if (attached == nullptr) {
    return false;
  }

  for (size_t index = 0; index < attached->size(); ++index) {
    PreprocessingInfo *info =
        requiredAttachedPreprocessingInfoEntry(*attached, index);
    if (info->getRelativePosition() == PreprocessingInfo::inside) {
      return true;
    }
  }

  return false;
}

bool shouldRenderClassDefinitionInline(const SgClassDefinition *class_defn,
                                       bool unparsed_using_tokens) {
  if (class_defn == nullptr || unparsed_using_tokens) {
    return false;
  }

  if (!class_defn->get_members().empty() ||
      classDefinitionHasInsidePreprocessingInfo(class_defn) ||
      class_defn->get_packingAlignment() != 0) {
    return false;
  }

  return true;
}

bool classDefinitionBodyNeedsLeadingSpace(const SgClassDefinition *class_defn) {
  const SgClassDeclaration *declaration =
      class_defn != nullptr ? isSgClassDeclaration(class_defn->get_parent())
                            : nullptr;
  if (class_defn == nullptr || declaration == nullptr ||
      declaration->get_definition() != class_defn ||
      class_defn->get_parent() != declaration) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[class-definition-owner]: class definition "
            "has no exact owning declaration\n");
    ROSE_ABORT();
  }
  return !declaration->get_isUnNamed();
}

const char *classAccessName(SgAccessModifier::access_modifier_enum access) {
  switch (access) {
  case SgAccessModifier::e_private:
    return "private";
  case SgAccessModifier::e_protected:
    return "protected";
  case SgAccessModifier::e_public:
    return "public";
  case SgAccessModifier::e_default:
    return "default";
  case SgAccessModifier::e_unknown:
    return "unknown";
  case SgAccessModifier::e_not_applicable:
    return "not-applicable";
  case SgAccessModifier::e_undefined:
    return "undefined";
  case SgAccessModifier::e_last_modifier:
    return "invalid-last-modifier";
  }
  return "invalid";
}

SgAccessModifier::access_modifier_enum
currentClassAccess(const SgUnparse_Info &info) {
  if (info.isPrivateAccess()) {
    return SgAccessModifier::e_private;
  }
  if (info.isProtectedAccess()) {
    return SgAccessModifier::e_protected;
  }
  if (info.isPublicAccess()) {
    return SgAccessModifier::e_public;
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[class-access-state]: class body has no exact "
          "current lexical access\n");
  ROSE_ABORT();
}

void initializeClassAccessState(const SgClassDefinition *class_defn,
                                SgUnparse_Info &info) {
  ASSERT_not_null(class_defn);
  ASSERT_not_null(class_defn->get_declaration());
  switch (class_defn->get_declaration()->get_class_type()) {
  case SgClassDeclaration::e_class:
    info.set_isPrivateAccess();
    break;
  case SgClassDeclaration::e_struct:
  case SgClassDeclaration::e_union:
    info.set_isPublicAccess();
    break;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[class-access-state]: class=%p has "
            "unsupported class-key=%d\n",
            static_cast<const void *>(class_defn),
            static_cast<int>(class_defn->get_declaration()->get_class_type()));
    ROSE_ABORT();
  }
}

void applyClassAccessLabelState(const SgAccessLabelStatement *label,
                                SgUnparse_Info &info) {
  ASSERT_not_null(label);
  label->validate();
  switch (label->get_label_kind()) {
  case SgAccessLabelStatement::e_access_label_private:
    info.set_isPrivateAccess();
    return;
  case SgAccessLabelStatement::e_access_label_protected:
    info.set_isProtectedAccess();
    return;
  case SgAccessLabelStatement::e_access_label_public:
    info.set_isPublicAccess();
    return;
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[class-access-state]: access label=%p has "
          "unsupported label kind=%d\n",
          static_cast<const void *>(label),
          static_cast<int>(label->get_label_kind()));
  ROSE_ABORT();
}

bool isCxxClassNonentityDirective(const SgDeclarationStatement *member) {
  return isSgOmpBeginDeclareVariantStatement(member) != nullptr ||
         isSgOmpEndDeclareVariantStatement(member) != nullptr ||
         isSgOmpDeclareTargetStatement(member) != nullptr ||
         isSgOmpEndDeclareTargetStatement(member) != nullptr ||
         isSgOmpBeginDeclareTargetStatement(member) != nullptr ||
         isSgOmpAssumesStatement(member) != nullptr ||
         isSgOmpBeginAssumesStatement(member) != nullptr ||
         isSgOmpEndAssumesStatement(member) != nullptr ||
         isSgOmpGroupprivateStatement(member) != nullptr ||
         isSgOmpRequiresStatement(member) != nullptr;
}

void validateClassMemberAccess(const SgClassDefinition *class_defn,
                               const SgDeclarationStatement *member,
                               const SgUnparse_Info &info) {
  ASSERT_not_null(class_defn);
  ASSERT_not_null(member);
  SgScopeStatement *semantic_scope = member->get_scope();
  SgMemberFunctionDeclaration *friend_member_function =
      isSgMemberFunctionDeclaration(
          const_cast<SgDeclarationStatement *>(member));
  const SgFriendTypeDeclaration *friend_type_surface =
      isSgFriendTypeDeclaration(member);
  const bool lexical_friend_declaration =
      member->get_declarationModifier().isFriend() &&
      (friend_type_surface != nullptr ||
       isSgClassDeclaration(member) != nullptr ||
       isSgTemplateFunctionDeclaration(member) != nullptr ||
       (isSgFunctionDeclaration(member) != nullptr &&
        (friend_member_function == nullptr || semantic_scope != class_defn)));
  SgClassDeclaration *friend_class =
      isSgClassDeclaration(const_cast<SgDeclarationStatement *>(member));
  SgClassDeclaration *friend_canonical =
      friend_class != nullptr
          ? isSgClassDeclaration(
                friend_class->get_firstNondefiningDeclaration())
          : nullptr;
  SgClassDeclaration *friend_defining =
      friend_canonical != nullptr
          ? isSgClassDeclaration(friend_canonical->get_definingDeclaration())
          : nullptr;
  SgClassType *friend_type = friend_canonical != nullptr
                                 ? isSgClassType(friend_canonical->get_type())
                                 : nullptr;
  SgClassDeclaration *friend_type_declaration =
      friend_type != nullptr
          ? isSgClassDeclaration(friend_type->get_declaration())
          : nullptr;
  const bool exact_shared_project_friend_type =
      friend_type_declaration != nullptr && friend_canonical != nullptr &&
      friend_type_declaration != friend_canonical &&
      !friend_type_declaration->get_isUnNamed() &&
      !friend_canonical->get_isUnNamed() &&
      friend_type_declaration->get_type() == friend_type &&
      friend_type_declaration->get_class_type() ==
          friend_canonical->get_class_type() &&
      friend_type_declaration->get_name() == friend_canonical->get_name() &&
      !friend_type_declaration->get_mangled_name().is_null() &&
      friend_type_declaration->get_mangled_name() ==
          friend_canonical->get_mangled_name() &&
      friend_type_declaration->get_qualified_name() ==
          friend_canonical->get_qualified_name() &&
      SageInterface::getEnclosingSourceFile(friend_type_declaration) !=
          SageInterface::getEnclosingSourceFile(friend_canonical);
  const bool exact_friend_type_identity =
      friend_type != nullptr && (friend_type_declaration == friend_canonical ||
                                 exact_shared_project_friend_type);
  SgScopeStatement *canonical_scope_owner =
      friend_canonical != nullptr
          ? isSgScopeStatement(friend_canonical->get_parent())
          : nullptr;
  SgAuxiliaryDeclarationList *canonical_auxiliary_owner =
      friend_canonical != nullptr
          ? isSgAuxiliaryDeclarationList(friend_canonical->get_parent())
          : nullptr;
  auto equivalent_scope = [](SgScopeStatement *lhs, SgScopeStatement *rhs) {
    return lhs == rhs || (lhs != nullptr && rhs != nullptr &&
                          SgScopeStatement::isEquivalentScope(lhs, rhs));
  };
  auto direct_declaration_count =
      [](SgScopeStatement *scope,
         SgDeclarationStatement *declaration) -> size_t {
    ASSERT_not_null(scope);
    ASSERT_not_null(declaration);
    if (SgGlobal *global = isSgGlobal(scope)) {
      return std::count(global->get_declarations().begin(),
                        global->get_declarations().end(), declaration);
    }
    if (SgNamespaceDefinitionStatement *namespace_definition =
            isSgNamespaceDefinitionStatement(scope)) {
      return std::count(namespace_definition->get_declarations().begin(),
                        namespace_definition->get_declarations().end(),
                        declaration);
    }
    if (SgDeclarationScope *declaration_scope = isSgDeclarationScope(scope)) {
      return std::count(declaration_scope->get_declarations().begin(),
                        declaration_scope->get_declarations().end(),
                        declaration);
    }
    if (SgClassDefinition *class_definition = isSgClassDefinition(scope)) {
      return std::count(class_definition->get_members().begin(),
                        class_definition->get_members().end(), declaration);
    }
    if (SgTemplateClassDefinition *class_definition =
            isSgTemplateClassDefinition(scope)) {
      return std::count(class_definition->get_members().begin(),
                        class_definition->get_members().end(), declaration);
    }
    if (SgTemplateInstantiationDefn *class_definition =
            isSgTemplateInstantiationDefn(scope)) {
      return std::count(class_definition->get_members().begin(),
                        class_definition->get_members().end(), declaration);
    }
    if (SgBasicBlock *block = isSgBasicBlock(scope)) {
      return std::count(block->get_statements().begin(),
                        block->get_statements().end(), declaration);
    }
    return static_cast<size_t>(0);
  };
  const bool exact_canonical_owner =
      (canonical_scope_owner != nullptr &&
       equivalent_scope(canonical_scope_owner, semantic_scope) &&
       direct_declaration_count(canonical_scope_owner, friend_canonical) ==
           1) ||
      (canonical_auxiliary_owner != nullptr &&
       canonical_auxiliary_owner->get_parent() == semantic_scope &&
       semantic_scope != nullptr &&
       semantic_scope->get_auxiliary_declarations() ==
           canonical_auxiliary_owner &&
       std::count(canonical_auxiliary_owner->get_declarations().begin(),
                  canonical_auxiliary_owner->get_declarations().end(),
                  friend_canonical) == 1);
  SgMemberFunctionDeclaration *friend_member_canonical =
      friend_member_function != nullptr
          ? isSgMemberFunctionDeclaration(
                friend_member_function->get_firstNondefiningDeclaration())
          : nullptr;
  SgScopeStatement *member_canonical_scope_owner =
      friend_member_canonical != nullptr
          ? isSgScopeStatement(friend_member_canonical->get_parent())
          : nullptr;
  SgAuxiliaryDeclarationList *member_canonical_auxiliary_owner =
      friend_member_canonical != nullptr
          ? isSgAuxiliaryDeclarationList(friend_member_canonical->get_parent())
          : nullptr;
  const bool exact_member_canonical_owner =
      (member_canonical_scope_owner != nullptr &&
       equivalent_scope(member_canonical_scope_owner, semantic_scope) &&
       direct_declaration_count(member_canonical_scope_owner,
                                friend_member_canonical) == 1) ||
      (member_canonical_auxiliary_owner != nullptr &&
       member_canonical_auxiliary_owner->get_parent() == semantic_scope &&
       semantic_scope != nullptr &&
       semantic_scope->get_auxiliary_declarations() ==
           member_canonical_auxiliary_owner &&
       std::count(member_canonical_auxiliary_owner->get_declarations().begin(),
                  member_canonical_auxiliary_owner->get_declarations().end(),
                  friend_member_canonical) == 1);
  const bool member_semantic_scope_is_class =
      isSgClassDefinition(semantic_scope) != nullptr ||
      isSgTemplateClassDefinition(semantic_scope) != nullptr ||
      isSgTemplateInstantiationDefn(semantic_scope) != nullptr;
  const bool exact_qualified_member_friend_family =
      lexical_friend_declaration && friend_member_function != nullptr &&
      semantic_scope != nullptr && semantic_scope != class_defn &&
      member_semantic_scope_is_class && friend_member_canonical != nullptr &&
      friend_member_canonical != friend_member_function &&
      friend_member_canonical->variantT() ==
          friend_member_function->variantT() &&
      friend_member_canonical->get_firstNondefiningDeclaration() ==
          friend_member_canonical &&
      friend_member_canonical->get_scope() == semantic_scope &&
      friend_member_function->get_firstNondefiningDeclaration() ==
          friend_member_canonical &&
      friend_member_function->get_definingDeclaration() ==
          friend_member_canonical->get_definingDeclaration() &&
      friend_member_function->get_class_scope() == semantic_scope &&
      friend_member_canonical->get_class_scope() == semantic_scope &&
      exact_member_canonical_owner;
  const bool exact_class_friend_family =
      lexical_friend_declaration && friend_class != nullptr &&
      semantic_scope != nullptr && friend_canonical != nullptr &&
      friend_canonical != friend_class &&
      friend_canonical->get_firstNondefiningDeclaration() == friend_canonical &&
      friend_canonical->get_scope() != nullptr &&
      equivalent_scope(friend_canonical->get_scope(), semantic_scope) &&
      exact_friend_type_identity && friend_class->get_type() == friend_type &&
      friend_class->get_definingDeclaration() == friend_defining &&
      (friend_defining == nullptr ||
       (friend_defining->get_firstNondefiningDeclaration() ==
            friend_canonical &&
        friend_defining->get_definingDeclaration() == friend_defining &&
        friend_defining->get_type() == friend_type)) &&
      exact_canonical_owner;
  const SgStringList &friend_source_qualifier =
      member->get_source_name_qualification_tokens();
  const bool exact_dependent_self_canonical_class_friend =
      lexical_friend_declaration && friend_class != nullptr &&
      semantic_scope != nullptr && friend_canonical == friend_class &&
      friend_defining == nullptr && exact_friend_type_identity &&
      friend_class->get_type() == friend_type &&
      member->get_source_name_qualification_present() &&
      (!friend_source_qualifier.empty() ||
       member->get_source_name_global_qualification()) &&
      friend_class->get_name_qualification_length() ==
          static_cast<int>(friend_source_qualifier.size()) &&
      friend_class->get_global_qualification_required() ==
          member->get_source_name_global_qualification();
  SgClassSymbol *friend_symbol =
      friend_canonical != nullptr
          ? isSgClassSymbol(friend_canonical->get_symbol_from_symbol_table())
          : nullptr;
  SgSymbolTable *friend_symbol_table =
      semantic_scope != nullptr ? semantic_scope->get_symbol_table() : nullptr;
  const bool exact_independent_self_canonical_class_friend =
      lexical_friend_declaration && friend_class != nullptr &&
      semantic_scope != nullptr && friend_canonical == friend_class &&
      friend_defining == nullptr && exact_friend_type_identity &&
      friend_class->get_type() == friend_type &&
      member->get_source_name_qualification_present() &&
      friend_source_qualifier.empty() &&
      !member->get_source_name_global_qualification() &&
      friend_symbol != nullptr &&
      friend_symbol->get_declaration() == friend_canonical &&
      friend_symbol->get_symbol_basis() == friend_canonical &&
      friend_symbol_table != nullptr &&
      friend_symbol_table->get_parent() == semantic_scope &&
      friend_symbol->get_parent() == friend_symbol_table &&
      friend_symbol_table->exists(friend_symbol) &&
      SageInterface::getEnclosingSourceFile(class_defn) ==
          SageInterface::getEnclosingSourceFile(semantic_scope);
  const bool nested_class_friend =
      exact_class_friend_family && semantic_scope == class_defn;
  const bool exact_type_only_friend =
      lexical_friend_declaration && friend_type_surface != nullptr &&
      semantic_scope == class_defn &&
      friend_type_surface->get_parent() == class_defn &&
      friend_type_surface->get_friend_type() != nullptr &&
      friend_type_surface->get_firstNondefiningDeclaration() ==
          friend_type_surface &&
      friend_type_surface->get_definingDeclaration() == nullptr;
  const bool lexical_free_function_friend =
      lexical_friend_declaration && friend_type_surface == nullptr &&
      friend_class == nullptr && friend_member_function == nullptr;
  const bool exact_semantic_scope =
      exact_type_only_friend || exact_class_friend_family ||
      exact_dependent_self_canonical_class_friend ||
      exact_independent_self_canonical_class_friend ||
      exact_qualified_member_friend_family ||
      (lexical_free_function_friend && semantic_scope != nullptr &&
       (isSgGlobal(semantic_scope) != nullptr ||
        isSgNamespaceDefinitionStatement(semantic_scope) != nullptr)) ||
      (!lexical_friend_declaration &&
       (semantic_scope == class_defn ||
        isExactCLexicalRecordTagSourceSurface(member, class_defn)));
  if (member->get_parent() != class_defn || !exact_semantic_scope) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[class-member-access]: class=%p member=%p "
            "type=%s parent=%p semantic-scope=%p friend=%d nested-friend=%d "
            "canonical=%p canonical-owner=%d family=%d has no exact lexical "
            "and semantic ownership\n",
            static_cast<const void *>(class_defn),
            static_cast<const void *>(member), member->class_name().c_str(),
            static_cast<void *>(member->get_parent()),
            static_cast<void *>(semantic_scope),
            static_cast<int>(lexical_friend_declaration),
            static_cast<int>(nested_class_friend),
            static_cast<void *>(friend_canonical),
            static_cast<int>(exact_canonical_owner),
            static_cast<int>(exact_class_friend_family));
    ROSE_ABORT();
  }

  const SgAccessModifier &access =
      member->get_declarationModifier().get_accessModifier();
  if (isClassAccessLabelStatement(member)) {
    return;
  }
  if (access.get_is_explicit()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[class-member-access]: class=%p member=%p "
            "type=%s stores an explicit access label on a semantic "
            "declaration instead of a lexical access marker\n",
            static_cast<const void *>(class_defn),
            static_cast<const void *>(member), member->class_name().c_str());
    ROSE_ABORT();
  }

  const SgAccessModifier::access_modifier_enum expected =
      currentClassAccess(info);
  const SgAccessModifier::access_modifier_enum actual = access.get_modifier();
  if (lexical_friend_declaration) {
    if (!access.isNotApplicable()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[class-member-access]: class=%s "
              "lexical friend=%s name=%s has semantic access=%s instead of "
              "not-applicable\n",
              class_defn->get_declaration()->get_name().str(),
              member->class_name().c_str(),
              SageInterface::get_name(member).c_str(), classAccessName(actual));
      ROSE_ABORT();
    }
    return;
  }
  if (isSgPragmaDeclaration(member) != nullptr ||
      isCxxClassNonentityDirective(member)) {
    if (!access.isNotApplicable()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[class-member-access]: class=%s "
              "non-entity=%s has semantic access=%s instead of "
              "not-applicable\n",
              class_defn->get_declaration()->get_name().str(),
              member->class_name().c_str(), classAccessName(actual));
      ROSE_ABORT();
    }
    return;
  }
  if (actual != expected) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[class-member-access]: class=%s member=%s "
            "name=%s effective=%s does not match lexical=%s\n",
            class_defn->get_declaration()->get_name().str(),
            member->class_name().c_str(),
            SageInterface::get_name(member).c_str(), classAccessName(actual),
            classAccessName(expected));
    ROSE_ABORT();
  }
}

void restoreInheritedPartialTokenState(SgUnparse_Info &info,
                                       bool inherited_partial_token_state) {
  if (inherited_partial_token_state) {
    info.set_unparsedPartiallyUsingTokenStream();
  } else {
    info.unset_unparsedPartiallyUsingTokenStream();
  }
}

void enableInlineDefinitionForFunctionReturnTypeIfNeeded(SgUnparse_Info &info,
                                                         SgType *type) {
  if (!functionReturnTypeNeedsInlineDefinition(type)) {
    return;
  }

  info.unset_SkipClassDefinition();
  info.unset_SkipEnumDefinition();
  info.unset_SkipClassSpecifier();
}

void requireExactCxxMacroDirectiveList(SgSourceFile *source_file) {
  ASSERT_not_null(source_file);
  ROSEAttributesListContainer *container =
      source_file->get_preprocessorDirectivesAndCommentsList();
  if (container == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cxx-macro-directive-list]: file=%s has "
            "no typed directive-list container\n",
            source_file->getFileName().c_str());
    ROSE_ABORT();
  }

  for (const auto &entry : container->getList()) {
    const std::string &filename = entry.first;
    ROSEAttributesList *directives = entry.second;
    if (directives == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[cxx-macro-directive-list]: file=%s has "
              "null owned directive list\n",
              filename.c_str());
      ROSE_ABORT();
    }
    const std::vector<PreprocessingInfo *> &records = directives->getList();
    for (size_t index = 0; index < records.size(); ++index) {
      if (records[index] == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[cxx-macro-directive-list]: file=%s "
                "has null directive at index=%zu\n",
                filename.c_str(), index);
        ROSE_ABORT();
      }
    }
  }
}

SgTryStmt *getFunctionTryStmt(SgFunctionDefinition *definition) {
  if (definition == NULL) {
    return NULL;
  }
  SgBasicBlock *body = definition->get_body();
  if (body == NULL) {
    return NULL;
  }
  const SgStatementPtrList &statements = body->get_statements();
  if (statements.size() != 1) {
    return NULL;
  }
  SgTryStmt *try_stmt = isSgTryStmt(statements.front());
  if (try_stmt == NULL) {
    return NULL;
  }
  if (try_stmt->get_is_function_try_block() == false) {
    return NULL;
  }
  return try_stmt;
}

void unparseRequiresClauseExpression(Unparse_ExprStmt *expr_unparser,
                                     SgExpression *requires_clause,
                                     SgUnparse_Info &info);

std::vector<SgTemplateClassDeclaration *>
collectTemplateClassChain(SgDeclarationStatement *associated_decl) {
  std::vector<SgTemplateClassDeclaration *> chain;

  auto append_template_decl = [&](SgDeclarationStatement *decl) {
    SgTemplateClassDeclaration *template_decl =
        isSgTemplateClassDeclaration(decl);
    if (template_decl != NULL &&
        std::find(chain.begin(), chain.end(), template_decl) == chain.end()) {
      chain.push_back(template_decl);
    }
  };

  append_template_decl(associated_decl);

  SgNode *cursor =
      associated_decl != NULL ? associated_decl->get_scope() : nullptr;
  while (cursor != NULL) {
    if (SgClassDefinition *class_def = isSgClassDefinition(cursor)) {
      append_template_decl(class_def->get_declaration());
    } else if (SgTemplateClassDefinition *tpl_class_def =
                   isSgTemplateClassDefinition(cursor)) {
      append_template_decl(tpl_class_def->get_declaration());
    } else if (SgTemplateInstantiationDefn *inst_def =
                   isSgTemplateInstantiationDefn(cursor)) {
      append_template_decl(inst_def->get_declaration());
    }
    cursor = cursor->get_parent();
  }

  return chain;
}

SgDeclarationStatement *
exactTemplateClassHeaderOwner(SgTemplateClassDeclaration *declaration,
                              SgDeclarationStatement *semantic_owner) {
  ASSERT_not_null(declaration);
  SgClassDeclaration *source_owner =
      declaration->get_sourceSpelledTemplateClassOwner();
  switch (declaration->get_templateClassOwnerScopeKind()) {
  case SgTemplateClassDeclaration::e_template_class_owner_scope_semantic:
    if (source_owner != nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[template-class-header-owner]: "
              "declaration=%p name=%s selects its semantic owner but also "
              "publishes source owner=%p\n",
              static_cast<void *>(declaration),
              declaration->get_name().getString().c_str(),
              static_cast<void *>(source_owner));
      ROSE_ABORT();
    }
    return semantic_owner;

  case SgTemplateClassDeclaration::
      e_template_class_owner_scope_source_spelled: {
    SgScopeStatement *owner_scope =
        source_owner != nullptr ? source_owner->get_scope() : nullptr;
    SgClassDefinition *owner_definition =
        source_owner != nullptr ? source_owner->get_definition() : nullptr;
    if (source_owner == nullptr || source_owner == declaration ||
        owner_scope == nullptr || source_owner->get_parent() != nullptr ||
        owner_definition == nullptr ||
        owner_definition->get_declaration() != source_owner ||
        owner_definition->get_parent() != source_owner ||
        source_owner->get_file_info() == nullptr ||
        !source_owner->get_file_info()->isCompilerGenerated() ||
        !source_owner->get_file_info()->isFrontendSpecific()) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[template-class-header-owner]: "
          "declaration=%p name=%s has malformed source-spelled owner=%p "
          "scope=%p parent=%p definition=%p\n",
          static_cast<void *>(declaration),
          declaration->get_name().getString().c_str(),
          static_cast<void *>(source_owner), static_cast<void *>(owner_scope),
          static_cast<void *>(
              source_owner != nullptr ? source_owner->get_parent() : nullptr),
          static_cast<void *>(owner_definition));
      ROSE_ABORT();
    }
    return source_owner;
  }

  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-class-header-owner]: "
            "declaration=%p name=%s has invalid owner kind=%d\n",
            static_cast<void *>(declaration),
            declaration->get_name().getString().c_str(),
            static_cast<int>(declaration->get_templateClassOwnerScopeKind()));
    ROSE_ABORT();
  }
}

void unparseTrailingRequiresClauseIfPresent(
    Unparse_ExprStmt *expr_unparser, SgFunctionDeclaration *funcdecl_stmt,
    SgUnparse_Info &info) {
  ASSERT_not_null(expr_unparser);
  ASSERT_not_null(funcdecl_stmt);

  if (SgExpression *requires_clause =
          funcdecl_stmt->get_trailingRequiresClause()) {
    SgUnparse_Info rinfo(info);
    rinfo.set_SkipClassDefinition();
    rinfo.set_SkipEnumDefinition();
    expr_unparser->curprint(" requires ");
    unparseRequiresClauseExpression(expr_unparser, requires_clause, rinfo);
  }
}

void unparseRequiresClauseExpression(Unparse_ExprStmt *expr_unparser,
                                     SgExpression *requires_clause,
                                     SgUnparse_Info &info) {
  ASSERT_not_null(expr_unparser);
  ASSERT_not_null(requires_clause);

  // Parenthesizing an exact typed constraint is always valid and avoids any
  // need to inspect or repair source text in the backend.
  expr_unparser->curprint("(");
  expr_unparser->unparseExpression(requires_clause, info);
  expr_unparser->curprint(")");
}

bool functionUsesTrailingReturnTypeSyntax(
    const SgFunctionDeclaration *function_declaration) {
  ASSERT_not_null(function_declaration);
  // Prefix and trailing return types are both valid source forms for many
  // semantic return types, including decltype.  The frontend records the
  // exact written form; choosing one here from the return-type class would be
  // a late syntax repair and can move parameter references out of scope.
  return function_declaration->get_using_new_function_return_type_syntax();
}

bool isBaseOrDelegatingCtorPreinitializer(const SgInitializedName *ctor_init) {
  if (ctor_init == NULL) {
    return false;
  }

  switch (ctor_init->get_preinitialization()) {
  case SgInitializedName::e_virtual_base_class:
  case SgInitializedName::e_nonvirtual_base_class:
  case SgInitializedName::e_delegation_constructor:
    return true;

  default:
    return false;
  }
}

void unparseCtorPreinitializerDesignator(Unparse_ExprStmt *expr_unparser,
                                         Unparser *backend_unparser,
                                         SgInitializedName *ctor_init,
                                         SgUnparse_Info &type_info) {
  ASSERT_not_null(expr_unparser);
  ASSERT_not_null(backend_unparser);
  ASSERT_not_null(ctor_init);

  if (isBaseOrDelegatingCtorPreinitializer(ctor_init)) {
    // The preinitializer designator is source syntax, not the resolved class
    // identity used by semantic consumers.  The frontend must publish both
    // contracts on the initialized name; deriving the source spelling here
    // from the semantic type would reintroduce the late repair this path is
    // intended to eliminate.
    SgType *semantic_target_type = ctor_init->get_type();
    SgType *target_type = ctor_init->get_cxx_source_type();
    if (semantic_target_type == NULL || target_type == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constructor-preinitializer-target]: "
              "initialized-name=%p preinitialization=%d has no exact "
              "semantic/source target pair semantic=%p source=%p\n",
              static_cast<void *>(ctor_init),
              static_cast<int>(ctor_init->get_preinitialization()),
              static_cast<void *>(semantic_target_type),
              static_cast<void *>(target_type));
      ROSE_ABORT();
    }

    SgUnparse_Info designator_info(type_info);
    designator_info.set_SkipClassDefinition();
    designator_info.set_SkipEnumDefinition();
    // The initialized name owns the exact written type-use qualifier (for
    // example, `base_12::` in a nested-base preinitializer).  Looking through
    // the constructor expression instead conflates overload resolution with
    // source spelling and was the reason name qualification had a late +1
    // workaround for all same-owner member functions.
    designator_info.set_reference_node_for_qualification(ctor_init);
    designator_info.set_SkipClassSpecifier();
    backend_unparser->u_type->unparseCtorPreinitializerDesignatorType(
        target_type, designator_info);
    return;
  }

  if (ctor_init->get_name().is_null() ||
      ctor_init->get_name().getString().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[constructor-preinitializer-member]: "
            "initialized-name=%p has no exact member identity\n",
            static_cast<void *>(ctor_init));
    ROSE_ABORT();
  }
  SgName nameQualifier(
      exactStatementNameQualification(backend_unparser, ctor_init, type_info)
          .qualifier);
  if (!nameQualifier.is_null()) {
    expr_unparser->curprint(nameQualifier.str());
  }
  expr_unparser->curprint(ctor_init->get_name().str());
}

} // namespace

namespace {
SgUnparse_Info nestedStatementInfo(const SgUnparse_Info &info) {
  SgUnparse_Info nested(info);
  nested.unset_SkipSemiColon();
  nested.unset_unparsedPartiallyUsingTokenStream();
  return nested;
}
} // namespace

Unparse_ExprStmt::Unparse_ExprStmt(Unparser *unp, std::string fname)
    : UnparseLanguageIndependentConstructs(unp, fname) {
  // Nothing to do here!
}

Unparse_ExprStmt::~Unparse_ExprStmt() {
  // Nothing to do here!
}

void Unparse_ExprStmt::unparseFunctionTryBlock(SgTryStmt *try_stmt,
                                               SgUnparse_Info &ninfo) {
  ASSERT_not_null(try_stmt);
  SgStatement *try_body = try_stmt->get_body();
  ASSERT_not_null(try_body);

  unp->cur.format(try_body, ninfo, FORMAT_BEFORE_NESTED_STATEMENT);
  unparseStatement(try_body, ninfo);
  unp->cur.format(try_body, ninfo, FORMAT_AFTER_NESTED_STATEMENT);

  for (SgStatement *catch_stmt : try_stmt->get_catch_statement_seq()) {
    unparseStatement(catch_stmt, ninfo);
  }
}

string UnparseLanguageIndependentConstructs::token_sequence_position_name(
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type e) {
  string s;
  switch (e) {
  case e_leading_whitespace_start:
    s = "e_leading_whitespace_start";
    break;
  case e_leading_whitespace_end:
    s = "e_leading_whitespace_end";
    break;
  case e_token_subsequence_start:
    s = "e_token_subsequence_start";
    break;
  case e_token_subsequence_end:
    s = "e_token_subsequence_end";
    break;
  case e_trailing_whitespace_start:
    s = "e_trailing_whitespace_start";
    break;
  case e_trailing_whitespace_end:
    s = "e_trailing_whitespace_end";
    break;

    // DQ (12/31/2014): Added to support the middle subsequence of tokens in the
    // SgIfStmt as a special case.
  case e_else_whitespace_start:
    s = "e_else_whitespace_start";
    break;
  case e_else_whitespace_end:
    s = "e_else_whitespace_end";
    break;

  default: {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-sequence-position]: invalid "
            "value=%d\n",
            static_cast<int>(e));
    ROSE_ABORT();
  }
  }

  return s;
}

// DQ (6/6/2021): Adding the support to provide offsets to modify the starting
// and ending token sequence to unparse. void
// UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream (
// SgStatement* stmt,
// UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
// e_token_sequence_position_start,
// UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
// e_token_sequence_position_end)
void UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream(
    SgStatement *stmt,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        e_token_sequence_position_start,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        e_token_sequence_position_end,
    SgUnparse_Info &info, int start_offset, int end_offset) {
#if DEBUG_TOKEN_STREAM_UNPARSING
  printf("unparseStatementFromTokenStream(stmt = %p = %s): \n", stmt,
         stmt->class_name().c_str());
  printf("   --- stmt: filename = %s \n", stmt->getFilenameString().c_str());
  printf("   --- e_token_sequence_position_start = %d = %s \n",
         e_token_sequence_position_start,
         token_sequence_position_name(e_token_sequence_position_start).c_str());
  printf("   --- e_token_sequence_position_end   = %d = %s \n",
         e_token_sequence_position_end,
         token_sequence_position_name(e_token_sequence_position_end).c_str());
#endif
#if DEBUG_USING_CURPRINT
  curprint(
      string("\n/* In unparseStatementFromTokenStream(stmt,start,end,info): "
             "stmt = ") +
      stmt->class_name() +
      " get_containsTransformationToSurroundingWhitespace = " +
      string(stmt->get_containsTransformationToSurroundingWhitespace()
                 ? "true"
                 : "false") +
      " */\n");
#endif

#if DEBUG_USING_CURPRINT
  if (SgProject::get_verbose() > 0) {
    string s = "\n/* Unparse a partial token sequence: 1 stmt: stmt = " +
               stmt->class_name() + " */\n";
    curprint(s);
  }
#endif

  // DQ (6/6/2021): Adding the support to provide offsets to modify the starting
  // and ending token sequence to unparse. DQ (5/30/2021): We do want to
  // uniformally call this function (no exception for SgGlobal. The exception
  // here is that if the last token of the file is C/C++ syntax (e.g. "}") then
  // we don't want to output anything.
  // unparseStatementFromTokenStream(stmt,stmt,e_token_sequence_position_start,e_token_sequence_position_end,info);
  unparseStatementFromTokenStream(stmt, stmt, e_token_sequence_position_start,
                                  e_token_sequence_position_end, info,
                                  start_offset, end_offset);

#if DEBUG_TOKEN_STREAM_UNPARSING
  printf("Leaving unparseStatementFromTokenStream(stmt,start,end,info) \n");
#endif
#if DEBUG_USING_CURPRINT
  // curprint("\n/* Leaving
  // unparseStatementFromTokenStream(stmt,start,end,info): */ \n");
  string s =
      string("\n/* Leaving "
             "unparseStatementFromTokenStream(stmt,start,end,info): stmt = ") +
      stmt->class_name() + " */ \n";
  curprint(s);
#endif
}

// DQ (6/6/2021): Adding the support to provide offsets to modify the starting
// and ending token sequence to unparse. void
// UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream (
// // SgStatement* stmt_1, SgStatement* stmt_2,
//    SgLocatedNode* stmt_1, SgLocatedNode* stmt_2,
//    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
//    e_token_sequence_position_start,
//    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
//    e_token_sequence_position_end, bool unparseOnlyWhitespace )
void UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream(
    SgLocatedNode *stmt_1, SgLocatedNode *stmt_2,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        start_position,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        end_position,
    SgUnparse_Info &info, bool unparse_only_whitespace, int start_offset,
    int end_offset) {
  if (stmt_1 == nullptr || stmt_2 == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-replay]: null token-range endpoint\n");
    ROSE_ABORT();
  }

  SgSourceFile *source_file = info.get_current_source_file();
  if (source_file == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-replay]: first=%p/%s second=%p/%s "
            "has no current source file\n",
            static_cast<void *>(stmt_1), stmt_1->class_name().c_str(),
            static_cast<void *>(stmt_2), stmt_2->class_name().c_str());
    ROSE_ABORT();
  }

  SgTokenPtrList &tokens = source_file->get_token_list();
  const int token_count = static_cast<int>(tokens.size());
  TokenStreamSequenceToNodeMapping *first_mapping =
      lookup_token_subsequence_mapping_for_node(source_file, stmt_1);
  TokenStreamSequenceToNodeMapping *second_mapping =
      lookup_token_subsequence_mapping_for_node(source_file, stmt_2);
  if (first_mapping == nullptr || second_mapping == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-replay]: file=%s first=%p/%s "
            "second=%p/%s has no direct token mapping\n",
            source_file->getFileName().c_str(), static_cast<void *>(stmt_1),
            stmt_1->class_name().c_str(), static_cast<void *>(stmt_2),
            stmt_2->class_name().c_str());
    ROSE_ABORT();
  }

  auto validate_mapping = [&](SgLocatedNode *endpoint,
                              TokenStreamSequenceToNodeMapping *mapping) {
    auto validate_interval = [&](TokenStreamIntervalKind kind,
                                 const char *name) {
      const TokenStreamHalfOpenInterval &interval =
          mapping->halfOpenInterval(kind);
      if (interval.begin < 0 || interval.end < interval.begin ||
          interval.end > token_count) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[token-replay]: file=%s "
                "node=%p/%s %s=[%d,%d) is outside [0,%d)\n",
                source_file->getFileName().c_str(),
                static_cast<void *>(endpoint), endpoint->class_name().c_str(),
                name, interval.begin, interval.end, token_count);
        ROSE_ABORT();
      }
    };
    validate_interval(TokenStreamIntervalKind::leading_whitespace,
                      "leading-whitespace");
    validate_interval(TokenStreamIntervalKind::token_subsequence,
                      "token-subsequence");
    validate_interval(TokenStreamIntervalKind::trailing_whitespace,
                      "trailing-whitespace");
    validate_interval(TokenStreamIntervalKind::else_whitespace,
                      "else-whitespace");

    const TokenStreamHalfOpenInterval &leading =
        mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace);
    const TokenStreamHalfOpenInterval &core =
        mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
    const TokenStreamHalfOpenInterval &trailing =
        mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace);
    const TokenStreamHalfOpenInterval &else_whitespace =
        mapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace);
    if ((token_count != 0 && core.empty()) || leading.end != core.begin ||
        trailing.begin != core.end ||
        (!else_whitespace.empty() && (else_whitespace.begin < core.begin ||
                                      else_whitespace.end > core.end)) ||
        (else_whitespace.empty() && else_whitespace.begin != core.end)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-replay]: file=%s node=%p/%s "
              "has inconsistent published intervals leading=[%d,%d) "
              "core=[%d,%d) trailing=[%d,%d) else=[%d,%d)\n",
              source_file->getFileName().c_str(), static_cast<void *>(endpoint),
              endpoint->class_name().c_str(), leading.begin, leading.end,
              core.begin, core.end, trailing.begin, trailing.end,
              else_whitespace.begin, else_whitespace.end);
      ROSE_ABORT();
    }
  };
  validate_mapping(stmt_1, first_mapping);
  if (second_mapping != first_mapping) {
    validate_mapping(stmt_2, second_mapping);
  }

  auto selected_interval =
      [](TokenStreamSequenceToNodeMapping *mapping,
         UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
             position) -> const TokenStreamHalfOpenInterval & {
    switch (position) {
    case e_leading_whitespace_start:
    case e_leading_whitespace_end:
      return mapping->halfOpenInterval(
          TokenStreamIntervalKind::leading_whitespace);
    case e_token_subsequence_start:
    case e_token_subsequence_end:
      return mapping->halfOpenInterval(
          TokenStreamIntervalKind::token_subsequence);
    case e_trailing_whitespace_start:
    case e_trailing_whitespace_end:
      return mapping->halfOpenInterval(
          TokenStreamIntervalKind::trailing_whitespace);
    case e_else_whitespace_start:
    case e_else_whitespace_end:
      return mapping->halfOpenInterval(
          TokenStreamIntervalKind::else_whitespace);
    }
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-replay]: unknown token position=%d\n",
            static_cast<int>(position));
    ROSE_ABORT();
  };

  auto selects_start =
      [](UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
             position) {
        switch (position) {
        case e_leading_whitespace_start:
        case e_token_subsequence_start:
        case e_trailing_whitespace_start:
        case e_else_whitespace_start:
          return true;
        case e_leading_whitespace_end:
        case e_token_subsequence_end:
        case e_trailing_whitespace_end:
        case e_else_whitespace_end:
          return false;
        }
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[token-replay]: unknown token position=%d\n",
            static_cast<int>(position));
        ROSE_ABORT();
      };

  const TokenStreamHalfOpenInterval &start_interval =
      selected_interval(first_mapping, start_position);
  const TokenStreamHalfOpenInterval &end_interval =
      selected_interval(second_mapping, end_position);
  const bool start_selects_first = selects_start(start_position);
  const bool end_selects_first = selects_start(end_position);

  // Position selectors retain their historical exact-token meaning while the
  // replay range itself is half-open.  A selector into an explicit empty
  // interval names its published boundary and therefore emits no invented
  // neighboring token.
  const int selected_begin =
      start_selects_first ? start_interval.begin
                          : (start_interval.empty() ? start_interval.end
                                                    : start_interval.end - 1);
  const int selected_end =
      end_selects_first
          ? (end_interval.empty() ? end_interval.begin : end_interval.begin + 1)
          : end_interval.end;

  const long long replay_begin =
      static_cast<long long>(selected_begin) + start_offset;
  const long long replay_end =
      static_cast<long long>(selected_end) + end_offset;
  if (replay_begin < 0 || replay_end < replay_begin ||
      replay_end > token_count) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[token-replay]: file=%s first=%p/%s "
            "second=%p/%s selectors=%s,%s offsets=%d,%d produce malformed "
            "half-open interval [%lld,%lld) for token-count=%d\n",
            source_file->getFileName().c_str(), static_cast<void *>(stmt_1),
            stmt_1->class_name().c_str(), static_cast<void *>(stmt_2),
            stmt_2->class_name().c_str(),
            token_sequence_position_name(start_position).c_str(),
            token_sequence_position_name(end_position).c_str(), start_offset,
            end_offset, replay_begin, replay_end, token_count);
    ROSE_ABORT();
  }

  for (int index = static_cast<int>(replay_begin);
       index < static_cast<int>(replay_end); ++index) {
    SgToken *token = tokens[index];
    if (token == nullptr) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[token-replay]: file=%s interval=[%lld,%lld) "
          "contains null token at index=%d\n",
          source_file->getFileName().c_str(), replay_begin, replay_end, index);
      ROSE_ABORT();
    }
    if (unparse_only_whitespace &&
        token->get_classification_code() != ROSE_token_ids::C_CXX_WHITESPACE) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[token-replay]: file=%s interval=[%lld,%lld) "
          "requested whitespace-only replay but token=%d has "
          "classification=%d\n",
          source_file->getFileName().c_str(), replay_begin, replay_end, index,
          token->get_classification_code());
      ROSE_ABORT();
    }

    const std::string &lexeme = token->get_lexeme_string();
    if (lexeme.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-replay]: file=%s token=%d has an "
              "empty exact lexeme\n",
              source_file->getFileName().c_str(), index);
      ROSE_ABORT();
    }
    unp->get_output_stream().emit_raw_text(lexeme);
  }
}

void Unparse_ExprStmt::unparseFunctionParameterDeclaration(
    SgFunctionDeclaration *funcdecl_stmt, SgInitializedName *initializedName,
    bool outputParameterDeclaration, SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);

  ASSERT_not_null(initializedName);
  SgName tmp_name = initializedName->get_name();
  SgInitializer *tmp_init = initializedName->get_initializer();
  SgType *tmp_type = initializedName->get_type();
  SgFunctionParameterList *parameter_list =
      isSgFunctionParameterList(initializedName->get_parent());
  if (parameter_list == nullptr ||
      (parameter_list != funcdecl_stmt->get_parameterList() &&
       parameter_list != funcdecl_stmt->get_parameterList_syntax())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-parameter-owner]: function=%p "
            "name=%s parameter=%p parent=%p parent-type=%s semantic-list=%p "
            "syntax-list=%p\n",
            static_cast<void *>(funcdecl_stmt), funcdecl_stmt->get_name().str(),
            static_cast<void *>(initializedName),
            static_cast<void *>(initializedName->get_parent()),
            initializedName->get_parent() != nullptr
                ? initializedName->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(funcdecl_stmt->get_parameterList()),
            static_cast<void *>(funcdecl_stmt->get_parameterList_syntax()));
    ROSE_ABORT();
  }

  // DQ (9/7/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  // DQ (8/9/2013): refactored to support additional refactoring to separate out
  // code to unparse SgInitializedName.
  bool oldStyleDefinition =
      funcdecl_stmt->get_oldStyleDefinition() && !funcdecl_stmt->isForward();

  // printf ("In unparseFunctionParameterDeclaration(): Argument name = %s \n",
  //      (tmp_name.str() != NULL) ? tmp_name.str() : "NULL NAME");

  // initializedName.get_storageModifier().display("New storage modifiers in
  // unparseFunctionParameterDeclaration()");

  SgStorageModifier &storage = initializedName->get_storageModifier();
  if (storage.isExtern()) {
    curprint("extern ");
  }

  // DQ (7/202/2006): The isStatic() function in the SgStorageModifier held by
  // the SgInitializedName object should always be false. This is because the
  // static-ness of a variable is held by the SgVariableDeclaration (and the
  // SgStorageModified help in the SgDeclarationModifier). printf ("In
  // initializedName = %p test the return value of storage.isStatic() = %d = %d
  // (should be boolean value)
  // \n",initializedName,storage.isStatic(),storage.get_modifier());
  ROSE_ASSERT(storage.isStatic() == false);

  // This was a bug mistakenly reported by Isaac
  ROSE_ASSERT(storage.get_modifier() >= 0);

  if (storage.isStatic()) {
    curprint("static ");
  }

  if (storage.isAuto()) {
    // DQ (4/30/2004): Auto is a default which is to be supressed
    // in C old-style parameters and not really ever needed anyway?
    // curprint( "auto ");
  }

  if (storage.isRegister()) {
    // curprint( "register ");
    if ((oldStyleDefinition == false) || (outputParameterDeclaration == true)) {
      curprint("register ");
    }
  }

  if (storage.isMutable()) {
    curprint("mutable ");
  }

  if (storage.isTypedef()) {
    curprint("typedef ");
  }

  if (storage.isAsm()) {
    // DQ (2/6/2014): Fix to support GNU gcc.
    // curprint("asm ");
    curprint("__asm__ ");
  }

  // TV (05/06/2010): CUDA storage modifiers
  if (storage.isCudaGlobal()) {
    curprint("__device__ ");
  }

  if (storage.isCudaConstant()) {
    curprint("__device__ __constant__ ");
  }

  if (storage.isCudaShared()) {
    curprint("__device__ __shared__ ");
  }

  if (storage.isCudaDynamicShared()) {
    curprint("extern __device__ __shared__ ");
  }

  // Error checking, if we are using old style C function parameters, then I
  // must not be C++ code.
  if (oldStyleDefinition == true) {
    ROSE_ASSERT(SageInterface::is_Cxx_language() == false);
  }

  if ((oldStyleDefinition == false) || (outputParameterDeclaration == true)) {
    // output the type name for each argument
    if (tmp_type != NULL) {
      SgUnparse_Info ninfo(info);
      ninfo.set_template_argument_qualification_context(parameter_list);
      // DQ (2/3/2019): In the case of function parameters, the member pointer
      // types need an extra parenthesis. This might just apply to arrays of
      // SgMemberPointerType.
      SgPointerMemberType *pointerToMemberType =
          isSgPointerMemberType(tmp_type);
      if (pointerToMemberType != NULL) {
        ninfo.set_inArgList();
      }
      // Decide inline parameter-type definitions from the final named-type
      // declaration rather than trusting SgInitializedName::needs_definitions
      // for class/enum types. That flag can be copied through declaration
      // cloning long after the type declaration has been canonicalized, which
      // leads to ordinary namespace-scope enums/classes being emitted inline in
      // parameter lists.
      SgType *inline_check_type =
          stripFunctionReturnTypeForInlineDefinitionCheck(tmp_type);
      bool needs_inline_type_definition = false;
      if (isSgClassType(inline_check_type) != nullptr ||
          isSgEnumType(inline_check_type) != nullptr) {
        needs_inline_type_definition =
            parameterTypeNeedsInlineDefinition(tmp_type);
      } else {
        needs_inline_type_definition = initializedName->get_needs_definitions();
      }
      if (needs_inline_type_definition) {
        if (ninfo.SkipClassDefinition()) {
          ninfo.unset_SkipClassDefinition();
        }
        if (ninfo.SkipEnumDefinition()) {
          ninfo.unset_SkipEnumDefinition();
        }
      }
      // DQ (5/5/2013): Refactored code used here and in the
      // unparseTemplateArgument().
      unp->u_type->outputType<SgInitializedName>(initializedName, tmp_type,
                                                 ninfo);

      // DQ (2/3/2019): In the case of function parameters, the member pointer
      // types need an extra parenthesis.
      if (pointerToMemberType != NULL) {
        ninfo.unset_inArgList();
      }
    } else {
      curprint(tmp_name.str()); // for ... case
    }
  } else {
    curprint(tmp_name.str()); // for ... case
  }

  SgUnparse_Info ninfo3(info);
  ninfo3.unset_inArgList();
  ninfo3.set_template_argument_qualification_context(parameter_list);

  // DQ (4/27/2013): We now have better support in ROSE to know when to output
  // the default arguments, so we don't want to use this mechanism above.  So
  // now we always output the default arguments for function parameters in a
  // function declaration if they are defined in the AST. It is up to the
  // specification in the AST to have them in the correct locations, consistant
  // with the source code.
  bool outputInitializer = true;

  // Add an initializer if it exists
  if (outputInitializer == true && tmp_init != NULL) {
    // Cong (6/28/2011): When unparsing an initializer for a function parameter,
    // we should add a space before '='. Or else, foo(const int& = 1) will be
    // unparsed to foo(const int&=1) which contains an operator '&=", which is
    // incorrect.
    curprint(" = ");
    unp->u_exprStmt->unparseExpression(tmp_init, ninfo3);
  }

  // DQ (1/7/2014): Adding support for GNU specific noreturn attribute for
  // function parameters (only applies to parameters that are of function
  // pointer type).
  if (initializedName->isGnuAttributeNoReturn() == true) {
    curprint(" __attribute__((noreturn))");
  }
}

void Unparse_ExprStmt::unparseFunctionArgs(SgFunctionDeclaration *funcdecl_stmt,
                                           SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);

  // DQ (9/7/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgFunctionParameterList *source_parameter_list =
      funcdecl_stmt->get_parameterList_syntax();
  if (source_parameter_list == nullptr) {
    source_parameter_list = funcdecl_stmt->get_parameterList();
  }
  ASSERT_not_null(source_parameter_list);
  unparseAttachedPreprocessingInfo(source_parameter_list, info,
                                   PreprocessingInfo::inside);

  // DQ (1/18/2014): This is a better implementation than setting the source
  // position info on the function parameters.  See test2014_35.c for an example
  // that requires this solution using a new data member.
  if (funcdecl_stmt->get_prototypeIsWithoutParameters() == true) {
    return;
  }

  bool outputFunctionParameters = true;

  SgInitializedNamePtrList::iterator p = funcdecl_stmt->get_args().begin();

  // DQ (4/13/2018): I want to initialize this iterator, but it is not clear
  // what to initialize it to...
  SgInitializedNamePtrList::iterator p_syntax =
      funcdecl_stmt->get_args().begin();
  bool use_param_syntax = false;
  // PP (9/19/25): * get_type_syntax_is_available() is not tied to
  // get_parameterList_syntax()
  //                 in some parse_secondary_declaration flows
  //                 get_type_syntax_is_available() only implies
  //                 get_type_syntax() and not get_parameterList_syntax().
  //               * For recording auto types in returns, only type_syntax is
  //               used,
  //                 but not parameterList_syntax.
  //               => check for get_parameterList_syntax() directly.
  if (SgFunctionParameterList *paramSyntax =
          funcdecl_stmt->get_parameterList_syntax()) {
    use_param_syntax = true;
    p_syntax = paramSyntax->get_args().begin();
  }

  bool separatorBelongsToCurrentParameter = false;

  while (p != funcdecl_stmt->get_args().end()) {
    // Liao 11/9/2010,
    // Skip duplicated unparsing of the attached information for C function
    // arguments declared in old style. They usually should be unparsed when
    // unparsing the arguments which are outside of the parameter list are
    // outside of the parameter list See example code:
    // tests/nonsmoke/functional/CompileTests/C_tests/test2010_10.c
    SgInitializedName *source_parameter = use_param_syntax ? *p_syntax : *p;
    ASSERT_not_null(source_parameter);
    if (funcdecl_stmt->get_oldStyleDefinition() == false) {
      unparseAttachedPreprocessingInfo(source_parameter, info,
                                       PreprocessingInfo::before);
    }
    if (separatorBelongsToCurrentParameter) {
      curprint(", ");
      separatorBelongsToCurrentParameter = false;
    }
    // DQ (1/17/2014): Adding support in C to output function prototypes without
    // function parameters. unparseFunctionParameterDeclaration
    // (funcdecl_stmt,*p,false,info); if (outputFunctionParameters == true)
    if ((outputFunctionParameters == true) ||
        (funcdecl_stmt->get_oldStyleDefinition() == true)) {
      // DQ (4/13/2018): If we have saved the original syntax then use it, else
      // use the default (which is matching the defining function declaration).
      // unparseFunctionParameterDeclaration (funcdecl_stmt,*p,false,info);
      if (use_param_syntax) {
        // DQ (4/13/2018): One question would be are we using the correct name
        // qualification for any type referenced.
        unparseFunctionParameterDeclaration(funcdecl_stmt, *p_syntax, false,
                                            info);

      } else {
        unparseFunctionParameterDeclaration(funcdecl_stmt, *p, false, info);
      }
    }

    // Move to the next argument
    p++;

    // DQ (4/13/2018): Increment the type syntax iterator in unison.
    if (use_param_syntax) {
      p_syntax++;
    }

    // A conditional region beginning immediately before the following
    // parameter owns that parameter's leading separator.  Emitting the comma
    // before the opening directive would leave a trailing comma when the
    // condition is false.  Closing directives do not defer the separator: a
    // conditionally present leading parameter instead owns its trailing comma.
    if (p != funcdecl_stmt->get_args().end()) {
      if (use_param_syntax &&
          p_syntax == source_parameter_list->get_args().end()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[function-parameter-syntax]: "
                "function=%p semantic parameters outnumber syntax "
                "parameters\n",
                static_cast<void *>(funcdecl_stmt));
        ROSE_ABORT();
      }
      SgInitializedName *next_source_parameter =
          use_param_syntax ? *p_syntax : *p;
      ASSERT_not_null(next_source_parameter);
      separatorBelongsToCurrentParameter =
          funcdecl_stmt->get_oldStyleDefinition() == false &&
          locatedNodeHasConditionalRegionOpening(next_source_parameter,
                                                 PreprocessingInfo::before);
    }
    if (p != funcdecl_stmt->get_args().end() &&
        !separatorBelongsToCurrentParameter) {
      curprint(", ");
    }
    if (funcdecl_stmt->get_oldStyleDefinition() == false) {
      unparseAttachedPreprocessingInfo(source_parameter, info,
                                       PreprocessingInfo::after);
      unparseAttachedPreprocessingInfo(source_parameter, info,
                                       PreprocessingInfo::after_syntax);
    }
  }
  ROSE_ASSERT(separatorBelongsToCurrentParameter == false);
}

void Unparse_ExprStmt::unparseOldStyleFunctionParameterDeclarations(
    SgFunctionDeclaration *funcdecl_stmt, SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);
  if (!funcdecl_stmt->get_oldStyleDefinition() || funcdecl_stmt->isForward()) {
    return;
  }

  SgUnparse_Info parameter_info(info);
  parameter_info.unset_inArgList();
  parameter_info.set_SkipClassDefinition();
  parameter_info.set_SkipEnumDefinition();
  parameter_info.set_template_argument_qualification_context(
      funcdecl_stmt->get_parameterList());

  const SgInitializedNamePtrList &parameters = funcdecl_stmt->get_args();
  if (!parameters.empty()) {
    unp->u_sage->curprint_newline();
  }
  for (SgInitializedName *parameter : parameters) {
    unparseAttachedPreprocessingInfo(parameter, info,
                                     PreprocessingInfo::before);
    unparseFunctionParameterDeclaration(funcdecl_stmt, parameter, true,
                                        parameter_info);
    curprint(";");
    unp->u_sage->curprint_newline();
  }
}

#define DEBUG_unparse_helper 0

//!  prints out the function parameters in a function declaration or function
//  call. For now, all parameters are printed on one line since there is no
//!  file information for each parameter.
void Unparse_ExprStmt::unparse_helper(SgFunctionDeclaration *funcdecl_stmt,
                                      SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);
#if DEBUG_unparse_helper
  printf("In unparse_helper():\n");
  printf("  funcdecl_stmt = %p = %s \n", funcdecl_stmt,
         funcdecl_stmt->class_name().c_str());
  printf("    ->get_name() = %s\n", funcdecl_stmt->get_name().str());
  printf("    ->get_firstNondefiningDeclaration() = %p \n",
         funcdecl_stmt->get_firstNondefiningDeclaration());
  printf("    ->get_definingDeclaration()         = %p \n",
         funcdecl_stmt->get_definingDeclaration());
  printf("    ->decl_mod.isFriend() = %s\n",
         funcdecl_stmt->get_declarationModifier().isFriend() ? "true"
                                                             : "false");
#endif
  bool is_friend = funcdecl_stmt->get_declarationModifier().isFriend();
  bool is_1st_decl =
      funcdecl_stmt == funcdecl_stmt->get_firstNondefiningDeclaration();
  const NameQualificationResult functionNameQualification =
      exactStatementNameQualification(unp, funcdecl_stmt, info);
  bool has_qualifier =
      functionNameQualification.length > 0 || functionNameQualification.global;

  const bool has_exact_friend_source_qualifier =
      is_friend && funcdecl_stmt->get_source_name_qualification_present();
  bool need_qualifier = has_exact_friend_source_qualifier
                            ? has_qualifier
                            : (!(is_friend && is_1st_decl) || has_qualifier);
  if (need_qualifier) {
    std::string nameQualifier = functionNameQualification.qualifier;
#if DEBUG_unparse_helper
    printf("  nameQualifier = %s\n", nameQualifier.c_str());
#endif
    curprint(nameQualifier);
  }

  SgTemplateInstantiationFunctionDecl *tpl_fdecl =
      isSgTemplateInstantiationFunctionDecl(funcdecl_stmt);
  const bool parenthesize_function_name =
      tpl_fdecl == NULL &&
      funcdecl_stmt->get_source_name_parenthesized_for_macro();
  if (parenthesize_function_name && has_qualifier) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-name-source-form]: qualified "
            "function=%s cannot own an unqualified macro-guard name form\n",
            funcdecl_stmt->get_name().str());
    ROSE_ABORT();
  }
  if (parenthesize_function_name) {
    curprint("(");
  }
  const bool has_omp_declare_variant_source_name =
      !funcdecl_stmt->get_omp_declare_variant_source_name().getString().empty();
  if (has_omp_declare_variant_source_name !=
      funcdecl_stmt->get_omp_declare_variant_region_ordinal().has_value()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[omp-declare-variant-function]: "
            "function=%s has incomplete typed source identity\n",
            funcdecl_stmt->get_name().str());
    ROSE_ABORT();
  }
  if (tpl_fdecl != NULL) {
    if (has_omp_declare_variant_source_name) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[omp-declare-variant-function]: template "
              "function=%s requires a typed template source-name model\n",
              funcdecl_stmt->get_name().str());
      ROSE_ABORT();
    }
#if DEBUG_unparse_helper
    printf("  tpl_fdecl->get_templateName() = %s \n",
           tpl_fdecl->get_templateName().str());
#endif
    unp->u_exprStmt->unparseTemplateFunctionName(tpl_fdecl, info);
  } else {
    curprint(has_omp_declare_variant_source_name
                 ? funcdecl_stmt->get_omp_declare_variant_source_name().str()
                 : funcdecl_stmt->get_name().str());
  }
  if (parenthesize_function_name) {
    curprint(")");
  }

  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgUnparse_Info ninfo2(info);
  ninfo2.set_inArgList();
  // SkipBaseType suppresses only the declaration group's shared outer
  // specifier.  Every nested parameter declarator owns its complete type.
  ninfo2.unset_SkipBaseType();
  ninfo2.set_SkipClassDefinition();
  ninfo2.set_SkipEnumDefinition();
  ROSE_ASSERT(ninfo2.SkipClassDefinition() == ninfo2.SkipEnumDefinition());

  if (funcdecl_stmt->get_source_declarator_uses_wrapped_function_type()) {
    funcdecl_stmt->validate_source_declarator_form();
  } else {
    curprint("(");
    unparseFunctionArgs(funcdecl_stmt, ninfo2);
    curprint(")");
  }

#if DEBUG_unparse_helper
  printf("Leaving unparse_helper()\n");
#endif
}

void Unparse_ExprStmt::unparseLanguageSpecificStatement(SgStatement *stmt,
                                                        SgUnparse_Info &info) {
  // This function unparses the language specific parse not handled by the base
  // class unparseStatement() member function

  ASSERT_not_null(stmt);

  CxxSourceDeclarationGroup sourceDeclarationGroup;
  const bool emitSourceDeclarationGroup =
      isSgDeclarationGroupStatement(stmt) != nullptr;
  if (emitSourceDeclarationGroup) {
    sourceDeclarationGroup =
        requireCxxSourceDeclarationGroup(isSgDeclarationGroupStatement(stmt));
  }

  // curprint("In unparseLanguageSpecificStatement()");

#if DEBUG_USING_CURPRINT
  curprint(
      string(
          "\n/* Top of unparseLanguageSpecificStatement (Unparse_ExprStmt) ") +
      stmt->class_name() + " */\n");
#endif

#if DEBUG_USING_CURPRINT && 0
  ASSERT_not_null(stmt->get_startOfConstruct());
  // ASSERT_not_null(stmt->getAttachedPreprocessingInfo());
  int numberOfComments = -1;
  if (stmt->getAttachedPreprocessingInfo() != NULL)
    numberOfComments = stmt->getAttachedPreprocessingInfo()->size();
  curprint(string("/* startOfConstruct: file = ") +
           stmt->get_startOfConstruct()->get_filenameString() +
           " raw filename = " +
           stmt->get_startOfConstruct()->get_raw_filename() + " raw line = " +
           StringUtility::numberToString(
               stmt->get_startOfConstruct()->get_raw_line()) +
           " raw column = " +
           StringUtility::numberToString(
               stmt->get_startOfConstruct()->get_raw_col()) +
           " #comments = " + StringUtility::numberToString(numberOfComments) +
           " */\n");
#endif

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if ROSE_TRACK_PROGRESS_OF_ROSE_COMPILING_ROSE || 0
  printf("In unparseLanguageSpecificStatement(): file = %s line = %d \n",
         stmt->get_startOfConstruct()->get_filenameString().c_str(),
         stmt->get_startOfConstruct()->get_line());
#endif

  // DQ (12/16/2008): Added support for unparsing statements around C++ specific
  // statements unparseAttachedPreprocessingInfo(stmt, info,
  // PreprocessingInfo::before);

  // DQ (12/26/2007): Moved from language independent handling to C/C++ specific
  // handling because we don't want it to appear in the Fortran code generation.
  // DQ (added comments) this is where the new lines are introduced before
  // statements. unp->cur.format(stmt, info, FORMAT_BEFORE_STMT); if
  // (info.unparsedPartiallyUsingTokenStream() == false)
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == false &&
      !info.SkipFormatting()) {
    // DQ (11/14/2015): If we are unparsing statements in a SgBasicBlock, then
    // we want to know if the SgBasicBlock is being unparsed using the
    // partial_token_sequence so that we can supress the formatting that adds a
    // CR to the start of the current statement being unparsed.
    bool parentStatementListBeingUnparsedUsingPartialTokenSequence =
        info.parentStatementListBeingUnparsedUsingPartialTokenSequence();
    if (parentStatementListBeingUnparsedUsingPartialTokenSequence == true) {
      // ROSE_ASSERT(false);
    } else {
      const bool access_specifier = isClassAccessLabelStatement(stmt);
      if (access_specifier) {
        const int indent = std::max(unp->cur.statement_indent() - TABINDENT, 0);
        if (unp->cur.line_is_empty()) {
          if (unp->cur.current_col() < indent) {
            curprint(std::string(indent - unp->cur.current_col(), ' '));
          }
        } else {
          unp->cur.insert_newline(1, indent);
        }
      } else {
        unp->cur.format(stmt, info, FORMAT_BEFORE_STMT);
      }
    }
  }

#if DEBUG_USING_CURPRINT
  curprint("/* In Unparse_ExprStmt::unparseLanguageSpecificStatement(): "
           "Selecting an unparse function */");
#endif

  if (emitSourceDeclarationGroup) {
    SgUnparse_Info groupInfo(info);
    groupInfo.set_SkipSemiColon();
    groupInfo.unset_AddSemiColonAfterDeclaration();
    for (size_t index = 0; index < sourceDeclarationGroup.members.size();
         ++index) {
      SgDeclarationStatement *member = sourceDeclarationGroup.members[index];
      ASSERT_not_null(member);
      bool separatorAfterOpeningDirective = false;
      if (index != 0) {
        SgLocatedNode *boundaryOwner =
            sourceDeclarationGroupBoundaryOwner(member);
        separatorAfterOpeningDirective = locatedNodeHasConditionalRegionOpening(
            boundaryOwner, PreprocessingInfo::before);
        if (!separatorAfterOpeningDirective) {
          curprint(", ");
        }
        groupInfo.set_SkipBaseType();
      }
      // Each structural group member is an independent semantic use site even
      // though the group is emitted through the first statement's dispatcher.
      // Qualification records are keyed to that exact member.
      groupInfo.set_template_argument_qualification_context(member);

      // The typed group is the sole statement surface, but preprocessing
      // information between declarators is anchored to the exact member that
      // follows it.  Calling a member's declaration emitter directly bypasses
      // the language-independent statement wrapper, so replay the member-owned
      // boundary here through the typed child edge.  This deliberately does
      // not search lexical siblings or reconstruct text from source ranges.
      unparseAttachedPreprocessingInfo(member, groupInfo,
                                       PreprocessingInfo::before);

      // A non-variable member owns its preprocessing boundary directly, so
      // its conditionally present leading comma belongs here.  Variable
      // boundaries are exact SgInitializedName children; unparseVarDeclStmt
      // emits their comma immediately after that child's opening directive.
      if (separatorAfterOpeningDirective &&
          isSgVariableDeclaration(member) == nullptr) {
        curprint(", ");
      }

      if (isSgVariableDeclaration(member) != nullptr) {
        unparseVarDeclStmt(member, groupInfo);
      } else if (isSgMemberFunctionDeclaration(member) != nullptr) {
        unparseMFuncDeclStmt(member, groupInfo);
      } else if (isSgFunctionDeclaration(member) != nullptr) {
        unparseFuncDeclStmt(member, groupInfo);
      } else if (isSgTypedefDeclaration(member) != nullptr) {
        unparseTypeDefStmt(member, groupInfo);
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[source-declaration-group]: group=%p "
                "contains unsupported member=%p/%s\n",
                static_cast<void *>(sourceDeclarationGroup.owner),
                static_cast<void *>(member), member->class_name().c_str());
        ROSE_ABORT();
      }

      // Member-owned trailing preprocessing remains inside the declaration
      // surface, before the following comma or the group terminator.  Source
      // preprocessing after the terminator is owned by the group itself and
      // is emitted by the language-independent statement wrapper.
      unparseAttachedPreprocessingInfo(member, groupInfo,
                                       PreprocessingInfo::after);
    }
    if (!info.SkipSemiColon() || info.AddSemiColonAfterDeclaration()) {
      curprint(";");
    }
    return;
  }

  switch (stmt->variantT()) {
    // DQ (8/14/2007): Need to move the C and C++ specific unparse member
    // functions from the base class to this function.

    // scope
    // case V_SgGlobal:                 unparseGlobalStmt(stmt, info); break;
    // case V_SgScopeStatement:         unparseScopeStmt(stmt, info); break;

    // pragmas
    // case V_SgPragmaDeclaration:      unparsePragmaDeclStmt(stmt, info);
    // break; scope case V_SgGlobal:                 unparseGlobalStmt(stmt,
    // info); break;
    //        case V_SgScopeStatement:         unparseScopeStmt (stmt, info);
    //        break;

    // program units
    // case V_SgModuleStatement:          unparseModuleStmt (stmt, info); break;
    // case V_SgProgramHeaderStatement:   unparseProgHdrStmt(stmt, info); break;
    // case V_SgProcedureHeaderStatement: unparseProcHdrStmt(stmt, info); break;

    // declarations
    // case V_SgInterfaceStatement:     unparseInterfaceStmt(stmt, info); break;
    // case V_SgCommonBlock:            unparseCommonBlock  (stmt, info); break;
  case V_SgVariableDeclaration:
    unparseVarDeclStmt(stmt, info);
    break;
  case V_SgVariableDefinition:
    unparseVarDefnStmt(stmt, info);
    break;
    // case V_SgParameterStatement:     unparseParamDeclStmt(stmt, info); break;
    // case V_SgUseStatement:           unparseUseStmt      (stmt, info); break;

    // executable statements, control flow
  case V_SgBasicBlock:
    unparseBasicBlockStmt(stmt, info);
    break;
  case V_SgIfStmt:
    unparseIfStmt(stmt, info);
    break;
    // case V_SgFortranDo:              unparseDoStmt         (stmt, info);
    // break;
  case V_SgWhileStmt:
    unparseWhileStmt(stmt, info);
    break;
  case V_SgSwitchStatement:
    unparseSwitchStmt(stmt, info);
    break;
  case V_SgCaseOptionStmt:
    unparseCaseStmt(stmt, info);
    break;
  case V_SgDefaultOptionStmt:
    unparseDefaultStmt(stmt, info);
    break;
  case V_SgBreakStmt:
    unparseBreakStmt(stmt, info);
    break;
  case V_SgLabelStatement:
    unparseLabelStmt(stmt, info);
    break;
  case V_SgGotoStatement:
    unparseGotoStmt(stmt, info);
    break;
    // case V_SgStopOrPauseStatement:   unparseStopOrPauseStmt(stmt, info);
    // break;
  case V_SgReturnStmt:
    unparseReturnStmt(stmt, info);
    break;

    // executable statements, IO
    // case V_SgIOStatement:            unparseIOStmt    (stmt, info); break;
    // case V_SgIOControlStatement:     unparseIOCtrlStmt(stmt, info); break;

    // pragmas
  case V_SgPragmaDeclaration:
    unparsePragmaDeclStmt(stmt, info);
    break;

  case V_SgImportStatement:
    unparseImportStatement(stmt, info);
    break;

    // DQ (3/22/2019): Adding EmptyDeclaration to support addition of comments
    // and CPP directives that will permit token-based unparsing to work with
    // greater precision. For example, used to add an include directive with
    // greater precision to the global scope and permit the unparsing via the
    // token stream to be used as well.
  case V_SgEmptyDeclaration:
    unparseEmptyDeclaration(stmt, info);
    break;
  case V_SgAccessLabelStatement:
    unparseAccessLabelStatement(stmt, info);
    break;

    // case DECL_STMT:          unparseDeclStmt(stmt, info);         break;
    // case SCOPE_STMT:         unparseScopeStmt(stmt, info);        break;
    //        case V_SgFunctionTypeTable:      unparseFuncTblStmt(stmt, info);
    //        break;
    // case GLOBAL_STMT:        unparseGlobalStmt(stmt, info);       break;
    // case V_SgBasicBlock:             unparseBasicBlockStmt(stmt, info);
    // break; case IF_STMT:            unparseIfStmt(stmt, info); break;

  case V_SgForStatement:
    unparseForStmt(stmt, info);
    break;

    // DQ (3/26/2018): Adding support for C++11 IR node (previously missed).
  case V_SgRangeBasedForStatement:
    unparseRangeBasedForStmt(stmt, info);
    break;

  case V_SgFunctionDeclaration:
    unparseFuncDeclStmt(stmt, info);
    break;
  case V_SgTemplateFunctionDefinition:
    unparseTemplateFunctionDefnStmt(stmt, info);
    break;
  case V_SgFunctionDefinition:
    unparseFuncDefnStmt(stmt, info);
    break;
  case V_SgMemberFunctionDeclaration:
    unparseMFuncDeclStmt(stmt, info);
    break;
    // case VAR_DECL_STMT:      unparseVarDeclStmt(stmt, info);      break;
    // case VAR_DEFN_STMT:      unparseVarDefnStmt(stmt, info);      break;
  case V_SgClassDeclaration:
    unparseClassDeclStmt(stmt, info);
    break;
  case V_SgClassDefinition:
    unparseClassDefnStmt(stmt, info);
    break;
  case V_SgEnumDeclaration:
    unparseEnumDeclStmt(stmt, info);
    break;
  case V_SgExprStatement:
    unparseExprStmt(stmt, info);
    break;
  case V_SgAttributedStatement:
    unparseAttributedStatement(stmt, info);
    break;
    // case LABEL_STMT:         unparseLabelStmt(stmt, info);        break;
    // case WHILE_STMT:         unparseWhileStmt(stmt, info);        break;
  case V_SgDoWhileStmt:
    unparseDoWhileStmt(stmt, info);
    break;
    // case SWITCH_STMT:        unparseSwitchStmt(stmt, info);       break;
    // case CASE_STMT:          unparseCaseStmt(stmt, info);         break;
  case V_SgTryStmt:
    unparseTryStmt(stmt, info);
    break;
  case V_SgCatchOptionStmt:
    unparseCatchStmt(stmt, info);
    break;
    // case DEFAULT_STMT:       unparseDefaultStmt(stmt, info);      break;
    // case BREAK_STMT:         unparseBreakStmt(stmt, info);        break;
  case V_SgContinueStmt:
    unparseContinueStmt(stmt, info);
    break;
    // case RETURN_STMT:        unparseReturnStmt(stmt, info);       break;
    // case GOTO_STMT:          unparseGotoStmt(stmt, info);         break;
  case V_SgAsmStmt:
    unparseAsmStmt(stmt, info);
    break;
    // case SPAWN_STMT:         unparseSpawnStmt(stmt, info);        break;
  case V_SgTypedefDeclaration:
    unparseTypeDefStmt(stmt, info);
    break;
  case V_SgTemplateDeclaration:
    unparseTemplateDeclStmt(stmt, info);
    break;

    // DQ (6/11/2011): Added support for new template IR nodes.
    // case V_SgTemplateClassDeclaration: unparseTemplateDeclStmt(stmt, info);
    // break; case V_SgTemplateFunctionDeclaration:
    // unparseTemplateDeclStmt(stmt, info); break; case
    // V_SgTemplateMemberFunctionDeclaration: unparseTemplateDeclStmt(stmt,
    // info); break; case V_SgTemplateVariableDeclaration:
    // unparseTemplateDeclStmt(stmt, info); break;

    // DQ (12/26/2011): New design for template declarations (no longer derived
    // from StTemplateDeclaration).
  case V_SgTemplateClassDeclaration:
    unparseTemplateClassDeclStmt(stmt, info);
    break;
  case V_SgTemplateClassDefinition:
    unparseTemplateClassDefnStmt(stmt, info);
    break;
  case V_SgTemplateFunctionDeclaration:
    unparseTemplateFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateMemberFunctionDeclaration:
    unparseTemplateMemberFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateVariableDeclaration:
    unparseTemplateVariableDeclStmt(stmt, info);
    break;

  case V_SgTemplateInstantiationDecl:
    unparseTemplateInstantiationDeclStmt(stmt, info);
    break;
  case V_SgTemplateInstantiationFunctionDecl:
    unparseTemplateInstantiationFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateInstantiationMemberFunctionDecl:
    unparseTemplateInstantiationMemberFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateInstantiationDirectiveStatement:
    unparseTemplateInstantiationDirectiveStmt(stmt, info);
    break;

    // Lexical/auxiliary ownership is resolved before statement dispatch.  A
    // template-instantiation typedef that reaches this lexical emitter owns
    // real typedef syntax regardless of how its file information was created.
  case V_SgTemplateInstantiationTypedefDeclaration:
    unparseTypeDefStmt(isSgTemplateInstantiationTypedefDeclaration(stmt), info);
    break;

  case V_SgForInitStatement:
    unparseForInitStmt(stmt, info);
    break;

    // These are syntax containers whose delimiters and attached preprocessing
    // information are owned by their enclosing declaration or try statement.
    // Dispatching one as an independent statement would either duplicate that
    // surface or silently lose it.
  case V_SgCatchStatementSeq:     // CATCH_STATEMENT_SEQ:
  case V_SgFunctionParameterList: // FUNCTION_PARAMETER_LIST:
  case V_SgCtorInitializerList:   // CTOR_INITIALIZER_LIST:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[syntax-container-ownership]: cannot "
            "unparse %s independently of its owning construct\n",
            stmt->sage_class_name());
    ROSE_ABORT();

  case V_SgNamespaceDeclarationStatement:
    unparseNamespaceDeclarationStatement(stmt, info);
    break;
  case V_SgNamespaceDefinitionStatement:
    unparseNamespaceDefinitionStatement(stmt, info);
    break;
  case V_SgNamespaceAliasDeclarationStatement:
    unparseNamespaceAliasDeclarationStatement(stmt, info);
    break;
  case V_SgUsingDirectiveStatement:
    unparseUsingDirectiveStatement(stmt, info);
    break;
  case V_SgUsingDeclarationStatement:
    unparseUsingDeclarationStatement(stmt, info);
    break;

    // DQ (3/2/2005): Added support for unparsing template class definitions.
    // This is the case: TEMPLATE_INST_DEFN_STMT
  case V_SgTemplateInstantiationDefn:
    unparseClassDefnStmt(stmt, info);
    break;

    // case V_SgNullStatement:                      unparseNullStatement(stmt,
    // info); break;

    // Liao, 5/31/2009, add OpenMP support, TODO refactor some code to language
    // independent part
  case V_SgOmpForStatement:
    unparseOmpForStatement(stmt, info);
    break;
  case V_SgOmpForSimdStatement:
    unparseOmpForSimdStatement(stmt, info);
    break;

    // DQ (7/25/2014): Adding support for C11 static assertions.
  case V_SgStaticAssertionDeclaration:
    unparseStaticAssertionDeclaration(stmt, info);
    break;

  case V_SgFriendTypeDeclaration:
    unparseFriendTypeDeclaration(stmt, info);
    break;

    // DQ 11/3/2014): Adding C++11 templated typedef declaration support.
  case V_SgTemplateTypedefDeclaration:
    unparseTemplateTypedefDeclaration(stmt, info);
    break;

  case V_SgNonrealDecl:
    unparseNonrealDecl(stmt, info);
    break;

  case V_SgDeclarationScope:
    // Nonreal scope container for template parameters; no direct code output.
    break;

  default: {
    printf("CxxCodeGeneration_locatedNode::unparseLanguageSpecificStatement: "
           "Error: No handler for %s (variant: %d)\n",
           stmt->sage_class_name(), stmt->variantT());
    ROSE_ABORT();
  }
  }

  // DQ (12/16/2008): Added support for unparsing statements around C++ specific
  // statements unparseAttachedPreprocessingInfo(stmt, info,
  // PreprocessingInfo::after);

#if DEBUG_USING_CURPRINT
  curprint(
      string("\n/* Leaving of unparseLanguageSpecificStatement() stmt = ") +
      stmt->class_name() + " */ \n");
#endif

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if DEBUG_USING_CURPRINT
  curprint(
      string(
          "\n/* Leaving unparseLanguageSpecificStatement (Unparse_ExprStmt) ") +
      stmt->class_name() + " */\n");
#endif
}

void Unparse_ExprStmt::unparseNamespaceDeclarationStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgNamespaceDeclarationStatement *namespaceDeclaration =
      isSgNamespaceDeclarationStatement(stmt);
  ASSERT_not_null(namespaceDeclaration);
  if (locatedNodeHasInsidePreprocessingInfo(namespaceDeclaration)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-owner]: namespace "
            "declaration=%p owns inside syntax; its definition must own the "
            "namespace body boundary\n",
            static_cast<void *>(namespaceDeclaration));
    ROSE_ABORT();
  }
  SgNamespaceDefinitionStatement *namespaceDefinition =
      namespaceDeclaration->get_definition();
  if (namespaceDefinition == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-definition]: namespace "
            "declaration=%p has no definition\n",
            static_cast<void *>(namespaceDeclaration));
    ROSE_ABORT();
  }

  const namespace_source_fragment_state_enum fragment_state =
      namespaceSourceFragmentState(namespaceDeclaration, info);
  if (fragment_state == e_namespace_source_fragment_neither) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-fragment]: declaration=%p "
            "was dispatched for a file that owns neither syntax fragment\n",
            static_cast<void *>(namespaceDeclaration));
    ROSE_ABORT();
  }

  SgNamespaceSourceFragment *opening_introducer =
      namespaceDeclaration->get_opening_introducer_source_fragment();
  if (fragment_state == e_namespace_source_fragment_introducer_only) {
    ROSE_ASSERT(opening_introducer != nullptr);
    if (namespaceDeclaration->get_isInlinedNamespace()) {
      curprint("inline ");
    }
    curprint("namespace ");
    if (opening_introducer->get_contains_namespace_name()) {
      if (namespaceDeclaration->get_isUnnamedNamespace()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[namespace-source-fragment]: "
                "anonymous namespace=%p has an introducer that claims a name\n",
                static_cast<void *>(namespaceDeclaration));
        ROSE_ABORT();
      }
      curprint(namespaceDeclaration->get_name().str());
    }
    return;
  }

  const bool use_partial_tokens =
      info.unparsedPartiallyUsingTokenStream() &&
      fragment_state == e_namespace_source_fragment_complete &&
      opening_introducer == nullptr;
  if (use_partial_tokens) {
    unparseStatementFromTokenStream(stmt, namespaceDefinition,
                                    e_token_subsequence_start,
                                    e_token_subsequence_start, info);

    SgUnparse_Info ninfo(info);
    size_t extern_brace_depth = 0;
    bool extern_brace_active = ninfo.get_extern_C_with_braces();
    for (SgDeclarationStatement *declaration :
         namespaceDefinition->get_declarations()) {
      SgUnparse_Info child_info(ninfo);
      const bool inherited_partial_token_state =
          child_info.unparsedPartiallyUsingTokenStream();
      unparseStatementWithExternBraceTracking(
          declaration, child_info, extern_brace_depth, extern_brace_active);
      restoreInheritedPartialTokenState(ninfo, inherited_partial_token_state);
    }
    unparseStatementFromTokenStream(namespaceDefinition, stmt,
                                    e_token_subsequence_end,
                                    e_token_subsequence_end, info);
    return;
  }

  SgUnparse_Info fragment_info(info);
  fragment_info.unset_unparsedPartiallyUsingTokenStream();
  if (fragment_state == e_namespace_source_fragment_complete ||
      fragment_state == e_namespace_source_fragment_open_only) {
    if (opening_introducer == nullptr) {
      if (namespaceDeclaration->get_isInlinedNamespace()) {
        curprint("inline ");
      }
      curprint("namespace ");
    }
    if (!namespaceDeclaration->get_isUnnamedNamespace() &&
        (opening_introducer == nullptr ||
         !opening_introducer->get_contains_namespace_name())) {
      curprint(namespaceDeclaration->get_name().str());
    }
  }
  unparseNamespaceDefinitionStatement(namespaceDefinition, fragment_info);
}

void Unparse_ExprStmt::unparseNamespaceDefinitionStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgNamespaceDefinitionStatement *namespaceDefinition =
      isSgNamespaceDefinitionStatement(stmt);
  ASSERT_not_null(namespaceDefinition);

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(namespaceDefinition);
#endif

  SgUnparse_Info ninfo(info);

  // DQ (11/6/2004): Added support for saving current namespace!
  ASSERT_not_null(namespaceDefinition->get_namespaceDeclaration());
  SgNamespaceDeclarationStatement *saved_namespace =
      ninfo.get_current_namespace();

  // DQ (6/13/2007): Set to null before resetting to non-null value
  ninfo.set_current_namespace(NULL);
  ninfo.set_current_namespace(namespaceDefinition->get_namespaceDeclaration());

  const namespace_source_fragment_state_enum fragment_state =
      namespaceSourceFragmentState(
          namespaceDefinition->get_namespaceDeclaration(), info);
  if (fragment_state == e_namespace_source_fragment_neither) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-fragment]: definition=%p "
            "was dispatched for a file that owns neither syntax fragment\n",
            static_cast<void *>(namespaceDefinition));
    ROSE_ABORT();
  }
  if (fragment_state == e_namespace_source_fragment_introducer_only) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[namespace-source-fragment]: definition=%p "
            "cannot emit an introducer-only physical fragment\n",
            static_cast<void *>(namespaceDefinition));
    ROSE_ABORT();
  }

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream() &&
      fragment_state == e_namespace_source_fragment_complete;
  if (!saved_unparsedPartiallyUsingTokenStream) {
    ninfo.unset_unparsedPartiallyUsingTokenStream();
  }
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    if (fragment_state == e_namespace_source_fragment_complete ||
        fragment_state == e_namespace_source_fragment_open_only) {
      curprint(" {");
      unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
    }
  } else {
    unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                    e_token_subsequence_start, info);
  }

  SgStatement *last_stmt = NULL;

  // unparse all the declarations
  SgDeclarationStatementPtrList &statementList =
      namespaceDefinition->get_declarations();
  SgDeclarationStatementPtrList::iterator statementIterator =
      statementList.begin();
  size_t extern_brace_depth = 0;
  bool extern_brace_active = ninfo.get_extern_C_with_braces();
  while (statementIterator != statementList.end()) {
    SgStatement *currentStatement = *statementIterator;
    ASSERT_not_null(currentStatement);
    if (saved_unparsedPartiallyUsingTokenStream == false &&
        declarationNeedsLeadingBlankLine(currentStatement)) {
      const int indent = unp->cur.statement_indent();
      unp->cur.insert_newline(unp->cur.line_is_empty() ? 1 : 2, indent);
    }

    // DQ (11/6/2004): use ninfo instead of info for nested declarations in
    // namespace
    SgUnparse_Info child_info(ninfo);
    const bool inherited_partial_token_state =
        child_info.unparsedPartiallyUsingTokenStream();
    unparseStatementWithExternBraceTracking(
        currentStatement, child_info, extern_brace_depth, extern_brace_active);
    restoreInheritedPartialTokenState(ninfo, inherited_partial_token_state);

    // DQ (12/18/2014): Save the last statement so that we can use the
    // trailing token stream if using the token-based unparsing.
    last_stmt = currentStatement;

    // Go to the next statement
    statementIterator++;
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (3/17/2005): This helps handle cases such as void foo () { #include
    // "constant_code.h" }
    unparseAttachedPreprocessingInfo(namespaceDefinition, info,
                                     PreprocessingInfo::inside);

    if (fragment_state == e_namespace_source_fragment_complete ||
        fragment_state == e_namespace_source_fragment_close_only) {
      unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK1);
      curprint("}");
      unparseAttachedPreprocessingInfo(
          namespaceDefinition->get_namespaceDeclaration(), info,
          PreprocessingInfo::after_syntax);
    }
    curprint("\n");
    unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {

    // unparseStatementFromTokenStream (stmt, e_token_subsequence_end,
    // e_token_subsequence_end);
    if (last_stmt != NULL) {
      // Unparse the trailing white space of the last statement.
      unparseStatementFromTokenStream(last_stmt, stmt,
                                      e_trailing_whitespace_start,
                                      e_token_subsequence_end, info);
      // Unparse the final "}" for the SgNamespaceDefinitionStatement.
      unparseStatementFromTokenStream(stmt, e_token_subsequence_end,
                                      e_token_subsequence_end, info);
    } else {
      unparseStatementFromTokenStream(stmt, e_token_subsequence_end,
                                      e_token_subsequence_end, info);
    }
  }

  // DQ (11/3/2007): Since "ninfo" will go out of scope shortly, this is not
  // significant. DQ (6/13/2007): Set to null before resetting to non-null value
  // DQ (11/6/2004): Added support for saving current namespace!
  ninfo.set_current_namespace(NULL);
  ninfo.set_current_namespace(saved_namespace);
}

void Unparse_ExprStmt::unparseNamespaceAliasDeclarationStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
      isSgNamespaceAliasDeclarationStatement(stmt);
  ASSERT_not_null(namespaceAliasDeclaration);

  curprint("\nnamespace ");
  curprint(namespaceAliasDeclaration->get_name().str());
  curprint(" = ");
  ASSERT_not_null(namespaceAliasDeclaration->get_namespaceDeclaration());

  // DQ (7/8/2014): We store the information about the name qualification in the
  // reference to the namespace, and not in the namespace.  This is so that
  // multiple references to the namespace can be supported using different
  // levels of qualification.
  SgName nameQualifier(
      exactStatementNameQualification(unp, namespaceAliasDeclaration, info)
          .qualifier);

  curprint(nameQualifier);

  // DQ (4/9/2018): Added support for aliases of namespace alias namespaces.
  // curprint (
  // namespaceAliasDeclaration->get_namespaceDeclaration()->get_name().str());
  if (namespaceAliasDeclaration->get_is_alias_for_another_namespace_alias() ==
      false) {
    curprint(namespaceAliasDeclaration->get_namespaceDeclaration()
                 ->get_name()
                 .str());
  } else {
    // DQ (4/9/2018): This is the case of an alis to a namespace alias (see
    // Cxx_tests/test2018_26.C).
    curprint(namespaceAliasDeclaration->get_namespaceAliasDeclaration()
                 ->get_name()
                 .str());
  }

  curprint(string(";\n"));
}

void Unparse_ExprStmt::unparseUsingDirectiveStatement(SgStatement *stmt,
                                                      SgUnparse_Info &info) {
  SgUsingDirectiveStatement *usingDirective = isSgUsingDirectiveStatement(stmt);
  ASSERT_not_null(usingDirective);
  ASSERT_not_null(usingDirective->get_namespaceDeclaration());

  // Anonymous namespaces are emitted as `namespace { ... }` and do not need
  // a synthetic `using namespace __anonymous_namespace_...;` in generated code.
  if (usingDirective->get_namespaceDeclaration()->get_isUnnamedNamespace()) {
    return;
  }

  // DQ (8/26/2004): This should be "using namespace" instead of just "using"
  curprint(string("\nusing namespace "));

  // DQ (6/7/2007): Compute the name qualification separately.
  // curprint ( usingDirective->get_namespaceDeclaration()->get_name().str();
  // curprint (
  // usingDirective->get_namespaceDeclaration()->get_qualified_name().str();

  // DQ (5/12/2011): We store the information about the name qualification in
  // the reference to the namespace, and not in the namespace.  This is so that
  // multiple references to the namespace can be supported using different
  // levels of qualification. SgName nameQualifier =
  // unp->u_name->generateNameQualifier(
  // usingDirective->get_namespaceDeclaration() , info ); SgName nameQualifier =
  // unp->u_name->generateNameQualifier(
  // usingDirective->get_namespaceDeclaration() , tmp_info );
  SgName nameQualifier(
      exactStatementNameQualification(unp, usingDirective, info).qualifier);

  // printf ("In unparseUsingDirectiveStatement(): nameQualifier = %s
  // \n",nameQualifier.str());
  curprint(nameQualifier);

  curprint(usingDirective->get_namespaceDeclaration()->get_name().str());

  curprint(string(";\n"));
}

void Unparse_ExprStmt::unparseUsingDeclarationStatement(SgStatement *stmt,
                                                        SgUnparse_Info &info) {
  SgUsingDeclarationStatement *usingDeclaration =
      isSgUsingDeclarationStatement(stmt);
  ASSERT_not_null(usingDeclaration);

  // DQ (1/30/2019): This code is required for the output of the access
  // specifier (public, protected, private) and applies only within classes. Use
  // get_parent() instead of get_scope() since we are looking for the structural
  // position of the declaration (is it is a class).
  SgClassDefinition *classDefinition =
      isSgClassDefinition(usingDeclaration->get_parent());
  if (classDefinition != NULL) {
    // Don't output an access specifier in this is a struct or union!
    // printf ("Don't output an access specifier in this is a struct or union!
    // \n");

    // DQ and PC (6/1/2006): Added Peter's suggested fixes to support unparsing
    // fully qualified names (supporting auto-documentation). if
    // (classDefinition->get_declaration()->get_class_type() ==
    // SgClassDeclaration::e_class)
    if (classDefinition->get_declaration()->get_class_type() ==
            SgClassDeclaration::e_class &&
        !info.skipCheckAccess())
      info.set_CheckAccess();
    // inClass = true;
    // inCname =
    // isSgClassDefinition(vardecl_stmt->get_parent())->get_declaration()->get_name();
  }

  // DQ (1/30/2019): Adding support to output the access specifier when we are
  // in a class definition. info.set_CheckAccess();
  unp->u_sage->printSpecifier(usingDeclaration, info);
  info.unset_CheckAccess();

  curprint(string("\nusing "));
  if (usingDeclaration->get_source_has_typename()) {
    curprint("typename ");
  }

  // DQ (7/21/2005): Either one or the other of these are valid. A using
  // declaration can have either a reference to a declaration
  // (SgDeclarationStatement) or a variable or enum file name
  // (SgInitializedName).
  SgDeclarationStatement *declarationStatement =
      usingDeclaration->get_declaration();
  SgInitializedName *initializedName = usingDeclaration->get_initializedName();

  // Enforce that only one is a vaild pointer
  ROSE_ASSERT(declarationStatement != NULL || initializedName != NULL);
  ROSE_ASSERT(declarationStatement == NULL || initializedName == NULL);

  const std::string source_terminal_name =
      usingDeclaration->get_source_terminal_name().getString();
  if (source_terminal_name.empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[using-terminal-name]: using declaration "
            "has no exact terminal token\n");
    ROSE_ABORT();
  }
  const NameQualificationResult qualification =
      exactStatementNameQualification(unp, usingDeclaration, info);
  curprint(qualification.qualifier);
  curprint(source_terminal_name);
  if (usingDeclaration->get_source_has_pack_expansion()) {
    curprint("...");
  }
  curprint(string(";\n"));
}

void Unparse_ExprStmt::unparseTemplateInstantiationDirectiveStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (4/16/2005): Added support for explicit template instatination
  // directives
  SgTemplateInstantiationDirectiveStatement *templateInstantiationDirective =
      isSgTemplateInstantiationDirectiveStatement(stmt);
  ASSERT_not_null(templateInstantiationDirective);

  SgDeclarationStatement *declarationStatement =
      templateInstantiationDirective->get_declaration();
  ASSERT_not_null(declarationStatement);

  if (declarationStatement->get_parent() != templateInstantiationDirective) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[template-instantiation-parent]: "
            "directive=%p declaration=%p parent=%p\n",
            static_cast<void *>(templateInstantiationDirective),
            static_cast<void *>(declarationStatement),
            static_cast<void *>(declarationStatement->get_parent()));
    ROSE_ABORT();
  }

  // The wrapped declaration is a suppressed semantic child.  Its only source
  // emission site is this directive, so contextual qualification for all of
  // its template arguments is keyed to the directive owner.
  SgUnparse_Info declarationInfo(info);
  declarationInfo.set_template_argument_qualification_context(
      templateInstantiationDirective);

  // curprint ( string("template ";

  // DQ (8/2/2014): Added support for C++ directive to surpress template
  // instantiation.
  if (templateInstantiationDirective->get_do_not_instantiate() == true) {
    // syntax for C++11 "do not instantiate" directive.
    curprint("extern ");
  }

  ASSERT_not_null(declarationStatement->get_file_info());
  // declarationStatement->get_file_info()->display("Location of
  // SgTemplateInstantiationDirectiveStatement \n");

  // unparseStatement(declaration,info);
  switch (declarationStatement->variantT()) {
  case V_SgTemplateInstantiationDecl: {
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(declarationStatement);
    ASSERT_not_null(classDeclaration);

    SgUnparse_Info ninfo(declarationInfo);
    // Explicit-instantiation directives own the `template` / `extern template`
    // spelling. Replaying the wrapped declaration in an inherited partial-token
    // context can reorder it relative to neighboring explicit specializations.
    ninfo.unset_unparsedPartiallyUsingTokenStream();
    ninfo.set_AddSemiColonAfterDeclaration();
    unparseClassDeclStmt(classDeclaration, ninfo);
    break;
  }

  case V_SgTemplateInstantiationFunctionDecl: {
    // printf ("Unparsing of SgTemplateInstantiationFunctionDecl in
    // unparseTemplateInstantiationDirectiveStmt ... \n"); ROSE_ASSERT(false);
    SgFunctionDeclaration *functionDeclaration =
        isSgFunctionDeclaration(declarationStatement);
    ASSERT_not_null(functionDeclaration);
    // DQ (8/29/2005): "template" keyword now output by
    // Unparse_ExprStmt::outputTemplateSpecializationSpecifier() curprint (
    // string("template ";
    SgUnparse_Info ninfo(declarationInfo);
    ninfo.unset_unparsedPartiallyUsingTokenStream();
    ninfo.set_SkipFunctionDefinition();
    if (functionDeclaration->isForward() == false) {
      ninfo.set_AddSemiColonAfterDeclaration();
    }
    unparseFuncDeclStmt(functionDeclaration, ninfo);
    break;
  }

  case V_SgTemplateInstantiationMemberFunctionDecl: {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        isSgMemberFunctionDeclaration(declarationStatement);
    ASSERT_not_null(memberFunctionDeclaration);

    SgTemplateInstantiationMemberFunctionDecl *template_member_inst =
        isSgTemplateInstantiationMemberFunctionDecl(memberFunctionDeclaration);
    ASSERT_not_null(template_member_inst);

    const Sg_File_Info *member_file_info =
        template_member_inst->get_file_info();
    const Sg_File_Info *directive_file_info =
        templateInstantiationDirective->get_file_info();
    const Sg_File_Info *member_start =
        template_member_inst->get_startOfConstruct();
    const Sg_File_Info *member_end = template_member_inst->get_endOfConstruct();
    const Sg_File_Info *directive_start =
        templateInstantiationDirective->get_startOfConstruct();
    const Sg_File_Info *directive_end =
        templateInstantiationDirective->get_endOfConstruct();
    auto has_exact_source_provenance = [](const SgLocatedNode *node) {
      if (node == nullptr) {
        return false;
      }
      const Sg_File_Info *primary = node->get_file_info();
      const Sg_File_Info *start = node->get_startOfConstruct();
      const Sg_File_Info *end = node->get_endOfConstruct();
      auto is_exact_source = [](const Sg_File_Info *info) {
        return info != nullptr && info->get_line() > 0 && info->get_col() > 0 &&
               info->get_physical_file_id() >= 0 &&
               !info->isCompilerGenerated() && !info->isFrontendSpecific() &&
               !info->isTransformation() &&
               !info->isSourcePositionUnavailableInFrontend();
      };
      return is_exact_source(primary) && is_exact_source(start) &&
             is_exact_source(end) && primary->get_parent() == node &&
             start->get_parent() == node && end->get_parent() == node &&
             primary->get_physical_file_id() == start->get_physical_file_id() &&
             primary->get_line() == start->get_line() &&
             primary->get_col() == start->get_col() &&
             start->get_physical_file_id() == end->get_physical_file_id();
    };
    const bool member_starts_inside_directive =
        member_start != nullptr && directive_start != nullptr &&
        member_start->get_physical_file_id() ==
            directive_start->get_physical_file_id() &&
        (member_start->get_line() > directive_start->get_line() ||
         (member_start->get_line() == directive_start->get_line() &&
          member_start->get_col() > directive_start->get_col()));
    const bool member_ends_inside_directive =
        member_end != nullptr && directive_end != nullptr &&
        member_end->get_physical_file_id() ==
            directive_end->get_physical_file_id() &&
        (member_end->get_line() < directive_end->get_line() ||
         (member_end->get_line() == directive_end->get_line() &&
          member_end->get_col() < directive_end->get_col()));
    SgTemplateMemberFunctionDeclaration *primary_template =
        isSgTemplateMemberFunctionDeclaration(
            template_member_inst->get_templateDeclaration());
    SgTemplateInstantiationDefn *instantiated_class =
        isSgTemplateInstantiationDefn(template_member_inst->get_scope());
    SgTemplateClassDefinition *primary_class =
        primary_template != nullptr
            ? isSgTemplateClassDefinition(primary_template->get_scope())
            : nullptr;
    SgTemplateInstantiationDecl *instantiated_class_declaration =
        instantiated_class != nullptr
            ? isSgTemplateInstantiationDecl(
                  instantiated_class->get_declaration())
            : nullptr;
    SgTemplateClassDeclaration *primary_class_declaration =
        primary_class != nullptr
            ? isSgTemplateClassDeclaration(primary_class->get_declaration())
            : nullptr;
    SgTemplateClassDeclaration *canonical_primary_class =
        primary_class_declaration != nullptr
            ? isSgTemplateClassDeclaration(
                  primary_class_declaration->get_firstNondefiningDeclaration())
            : nullptr;
    const bool exact_instantiated_member_scope =
        instantiated_class_declaration != nullptr &&
        canonical_primary_class != nullptr &&
        instantiated_class_declaration->get_templateDeclaration() ==
            canonical_primary_class;
    const bool exact_instantiation_surface =
        templateInstantiationDirective->get_declaration() ==
            template_member_inst &&
        template_member_inst->get_parent() == templateInstantiationDirective &&
        !template_member_inst->isSpecialization() &&
        has_exact_source_provenance(templateInstantiationDirective) &&
        has_exact_source_provenance(template_member_inst) &&
        member_starts_inside_directive && member_ends_inside_directive &&
        primary_template != nullptr &&
        template_member_inst->get_scope() != nullptr &&
        (template_member_inst->get_scope() == primary_template->get_scope() ||
         exact_instantiated_member_scope);
    if (!exact_instantiation_surface) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[template-instantiation-source]: "
          "directive=%p at %s:%d:%d member=%p at %s:%d:%d does not "
          "preserve one exact source-instantiation surface "
          "(source=%d/%d begin-inside=%d end-inside=%d template=%p "
          "scopes=%p/%p)\n",
          static_cast<void *>(templateInstantiationDirective),
          directive_file_info != nullptr
              ? directive_file_info->get_filenameString().c_str()
              : "<null>",
          directive_file_info != nullptr ? directive_file_info->get_line() : 0,
          directive_file_info != nullptr ? directive_file_info->get_col() : 0,
          static_cast<void *>(template_member_inst),
          member_file_info != nullptr
              ? member_file_info->get_filenameString().c_str()
              : "<null>",
          member_file_info != nullptr ? member_file_info->get_line() : 0,
          member_file_info != nullptr ? member_file_info->get_col() : 0,
          has_exact_source_provenance(templateInstantiationDirective) ? 1 : 0,
          has_exact_source_provenance(template_member_inst) ? 1 : 0,
          member_starts_inside_directive ? 1 : 0,
          member_ends_inside_directive ? 1 : 0,
          static_cast<void *>(primary_template),
          static_cast<void *>(template_member_inst->get_scope()),
          static_cast<void *>(primary_template != nullptr
                                  ? primary_template->get_scope()
                                  : nullptr));
      ROSE_ABORT();
    }

    // Explicit instantiation directives remain directives regardless of
    // whether the instantiated declaration is itself still a template at the
    // ROSE level.  Emit the member declaration unconditionally and let the
    // directive parent drive the leading `template` / `extern template`
    // syntax.
    SgUnparse_Info ninfo(declarationInfo);
    ninfo.unset_unparsedPartiallyUsingTokenStream();
    ninfo.set_SkipFunctionDefinition();
    if (memberFunctionDeclaration->isForward() == false) {
      ninfo.set_AddSemiColonAfterDeclaration();
    }
    unparseMFuncDeclStmt(memberFunctionDeclaration, ninfo);
    break;
  }

  case V_SgVariableDeclaration: {
    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(declarationStatement);
    ASSERT_not_null(variableDeclaration);

    SgUnparse_Info ninfo(declarationInfo);
    ninfo.unset_unparsedPartiallyUsingTokenStream();
    unparseVarDeclStmt(variableDeclaration, ninfo);
    break;
  }

    // DQ (8/13/2005): Added this case because it comes up in compiling KULL
    // (KULL/src/transport/CommonMC/Particle/mcapm.cc)
  case V_SgMemberFunctionDeclaration: {
    // DQ (8/31/2005): This should be an error now!  Template instantiations
    // never generate a SgMemberFunctionDeclaration and always generate a
    // SgTemplateInstantiationMemberFunctionDecl
    printf("Error: SgMemberFunctionDeclaration case found in "
           "unparseTemplateInstantiationDirectiveStmt ... (exiting) \n");
    ROSE_ABORT();
    break;
  }

    // DQ (2/2/2018): Added case for currently unimplemented unparsing support
    // for template variable declarations.
  case V_SgTemplateVariableDeclaration: {
    SgTemplateVariableDeclaration *variableDeclaration =
        isSgTemplateVariableDeclaration(declarationStatement);
    ASSERT_not_null(variableDeclaration);
    if (variableDeclaration->get_parent() != templateInstantiationDirective ||
        variableDeclaration->get_specialization() !=
            SgDeclarationStatement::e_no_specialization ||
        variableDeclaration->get_explicitTemplateSpecializationHeaderCount() !=
            0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[variable-instantiation-directive]: "
              "directive=%p variable=%p has malformed exact ownership or "
              "template identity\n",
              static_cast<void *>(templateInstantiationDirective),
              static_cast<void *>(variableDeclaration));
      ROSE_ABORT();
    }

    SgUnparse_Info ninfo(declarationInfo);
    ninfo.unset_unparsedPartiallyUsingTokenStream();
    // The instantiated initializer is a semantic analysis subtree copied from
    // the template pattern.  An explicit-instantiation directive owns no
    // initializer syntax and must never emit that semantic child.
    ninfo.set_SkipInitializer();
    unparseTemplateVariableDeclStmt(variableDeclaration, ninfo);
    break;
  }

  default: {
    printf("Error: default reached in switch (declarationStatement = %s) \n",
           declarationStatement->class_name().c_str());
    ROSE_ABORT();
  }
  }
}

void Unparse_ExprStmt::unparseTemplateInstantiationDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (2/29/2004): New function to support templates
  SgTemplateInstantiationDecl *templateInstantiationDeclaration =
      isSgTemplateInstantiationDecl(stmt);
  ASSERT_not_null(templateInstantiationDeclaration);

  SgClassDeclaration *classDeclaration =
      isSgClassDeclaration(templateInstantiationDeclaration);
  ASSERT_not_null(classDeclaration);

#if OUTPUT_DEBUGGING_CLASS_NAME || 0
  printf(
      "Inside of unparseTemplateInstantiationDeclStmt() stmt = %p/%p name = %s "
      " templateName = %s transformed = %s/%s prototype = %s "
      "compiler-generated = %s compiler-generated and marked for output = %s "
      "\n",
      classDeclaration, templateInstantiationDeclaration,
      templateInstantiationDeclaration->get_name().str(),
      templateInstantiationDeclaration->get_templateName().str(),
      isTransformed(templateInstantiationDeclaration) ? "true" : "false",
      (templateInstantiationDeclaration->get_file_info()->isTransformation() ==
       true)
          ? "true"
          : "false",
      (templateInstantiationDeclaration->get_definition() == NULL) ? "true"
                                                                   : "false",
      (templateInstantiationDeclaration->get_file_info()
           ->isCompilerGenerated() == true)
          ? "true"
          : "false",
      (templateInstantiationDeclaration->get_file_info()
           ->isCompilerGeneratedNodeToBeUnparsed() == true)
          ? "true"
          : "false");
#endif

  // A type emitter requests only the template-id spelling.  A statement
  // emitter has already selected this declaration through exact lexical or
  // explicit-instantiation ownership and therefore must emit it.  File-info
  // output bits are not an ownership channel and must never make a selected
  // declaration disappear here.
  if (info.outputClassTemplateName()) {
    const std::string template_id =
        requireCompleteClassTemplateId(templateInstantiationDeclaration);
    const std::string qualified_name =
        templateInstantiationDeclaration->get_qualified_name().str();
    if (qualified_name.size() < template_id.size() ||
        qualified_name.compare(qualified_name.size() - template_id.size(),
                               template_id.size(), template_id) != 0) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[template-instantiation-identity]: "
                      "qualified declaration name does not end in the stored "
                      "template-id\n");
      ROSE_ABORT();
    }
    curprint(qualified_name);
    return;
  }

  unparseClassDeclStmt(classDeclaration, info);
}

void Unparse_ExprStmt::unparseTemplateInstantiationFunctionDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (6/8/2005): If this is an inlined function, we need to make sure that
  // the function has not been used anywhere before where we output it here.

  // DQ (3/24/2004): New function to support templates
  SgTemplateInstantiationFunctionDecl
      *templateInstantiationFunctionDeclaration =
          isSgTemplateInstantiationFunctionDecl(stmt);
  ASSERT_not_null(templateInstantiationFunctionDeclaration);
  ASSERT_not_null(templateInstantiationFunctionDeclaration->get_file_info());

  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(templateInstantiationFunctionDeclaration);

  ASSERT_not_null(functionDeclaration);

#if OUTPUT_DEBUGGING_FUNCTION_NAME || 0
  printf("In unparseTemplateInstantiationFunctionDeclStmt() name = %s "
         "(qualified_name = %s)  transformed = %s prototype = %s static = %s "
         "friend = %s compiler generated = %s transformed = %s output = %s \n",
         // templateInstantiationFunctionDeclaration->get_name().str(),
         templateInstantiationFunctionDeclaration->get_name().str(),
         templateInstantiationFunctionDeclaration->get_qualified_name().str(),
         isTransformed(templateInstantiationFunctionDeclaration) ? "true"
                                                                 : "false",
         (templateInstantiationFunctionDeclaration->get_definition() == NULL)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_declarationModifier()
              .get_storageModifier()
              .isStatic() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_declarationModifier()
              .isFriend() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_file_info()
              ->isCompilerGenerated() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_file_info()
              ->isTransformation() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_file_info()
              ->isOutputInCodeGeneration() == true)
             ? "true"
             : "false");
#endif

  // Reaching this language-specific emitter is itself the exact emission
  // decision.  Implicit semantic instantiations belong to auxiliary owners and
  // never enter a lexical statement walk.  Silently consulting file-info bits,
  // template-declaration availability, or definition presence here used to
  // hide malformed ownership and transformed forward declarations.
  unparseFuncDeclStmt(functionDeclaration, info);
}

void Unparse_ExprStmt::unparseTemplateInstantiationMemberFunctionDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Rules for output of member templates functions:
  //  1) When we unparse the template declaration as a string the frontend
  //  removes the member
  //     function definitions so we are forced to output all template member
  //     functions.
  //  2) If the member function is specified outside of the class then we don't
  //  have to
  //     explicitly output the instantiation.

  // DQ (3/24/2004): New function to support templates
  SgTemplateInstantiationMemberFunctionDecl
      *templateInstantiationMemberFunctionDeclaration =
          isSgTemplateInstantiationMemberFunctionDecl(stmt);
  ASSERT_not_null(templateInstantiationMemberFunctionDeclaration);

  SgMemberFunctionDeclaration *memberFunctionDeclaration =
      isSgMemberFunctionDeclaration(
          templateInstantiationMemberFunctionDeclaration);
  ASSERT_not_null(memberFunctionDeclaration);

  // DQ (3/3/2005): Commented out since it was a problem in test2004_36.C
  // DQ (5/8/2004): Make this an explicit specialization (using the newer C++
  // syntax to support this) curprint ( string("template <> \n";
  // ASSERT_not_null(templateInstantiationMemberFunctionDeclaration->get_templateArguments());
  // if
  // (templateInstantiationMemberFunctionDeclaration->get_templateArguments()->size()
  // > 0)
  if (templateInstantiationMemberFunctionDeclaration->isSpecialization() ==
      true) {
    if ((templateInstantiationMemberFunctionDeclaration->get_file_info()
             ->isCompilerGenerated() == true) &&
        (templateInstantiationMemberFunctionDeclaration->isForward() == true)) {
      // This is a ROSE generated forward declaration of a ROSE specialized
      // member function (required). It is built in
      // ROSE/src/roseSupport/templateSupport.C void
      // fixupInstantiatedTemplates ( SgProject* project ). The forward
      // declaration is placed directly after the template declaration so that
      // no uses of the function can exist prior to its declaration.  Output a
      // message into the gnerated source code identifying this
      // transformation.
#if PRINT_DEVELOPER_WARNINGS || 0
      curprint(string("\n/* ROSE generated forward declaration of the ROSE "
                      "generated member template specialization */"));
#endif
    } else {
      // This is the ROSE generated template specialization for the template
      // member function (required to be defined since the function is used
      // (called)).  This function is defined at the end of file and may be
      // defined there because a forward declaration for the specialization
      // was output directly after the template declaration (before any use of
      // the function could have been made ???).
#if PRINT_DEVELOPER_WARNINGS || 0
      curprint(string("\n/* ROSE generated member template specialization */"));
#endif
    }

    // DQ (8/27/2005): This might be required for g++ 3.4.x and optional for
    // g++ 3.3.x DQ (8/19/2005): It is incorrect when used for non-template
    // member functions on templated classes defined in the class Output the
    // syntax for template specialization (appears to be largely optional (at
    // least with GNU g++) curprint ( string("\ntemplate <> ";
  }

  // DQ (8/29/2005): This is now output by the
  // Unparse_ExprStmt::outputTemplateSpecializationSpecifier() member function
  // curprint ( string("\ntemplate <> ";
  // As for free-function instantiations, lexical/auxiliary ownership has
  // already decided whether this node owns source syntax.  Once selected, a
  // forward declaration is still real syntax and definition presence is not a
  // license to erase it.
  unparseMFuncDeclStmt(memberFunctionDeclaration, info);
}

void Unparse_ExprStmt::unparsePragmaDeclStmt(SgStatement *stmt,
                                             SgUnparse_Info &) {
  SgPragmaDeclaration *pragmaDeclaration = isSgPragmaDeclaration(stmt);
  ASSERT_not_null(pragmaDeclaration);

  SgPragma *pragma = pragmaDeclaration->get_pragma();
  ASSERT_not_null(pragma);

  const std::string &source_text = pragmaDeclaration->get_cxx_source_text();
  const std::string semantic_text = pragma->get_pragma();
  const std::string *pragma_text = nullptr;
  switch (pragmaDeclaration->get_cxx_pragma_payload_kind()) {
  case SgPragmaDeclaration::e_cxx_pragma_source_spelled:
  case SgPragmaDeclaration::e_cxx_pragma_source_file_only:
    if (source_text.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[pragma-source-text]: source-spelled "
              "C/C++ pragma has no exact typed source payload\n");
      ROSE_ABORT();
    }
    pragma_text = &source_text;
    break;
  case SgPragmaDeclaration::e_cxx_pragma_generated_semantic:
    if (!source_text.empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[pragma-source-text]: generated C/C++ "
              "pragma also owns a source-spelled payload\n");
      ROSE_ABORT();
    }
    pragma_text = &semantic_text;
    break;
  case SgPragmaDeclaration::e_cxx_pragma_payload_none:
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[pragma-payload-kind]: pragma has invalid "
            "typed C/C++ payload kind=%d\n",
            static_cast<int>(pragmaDeclaration->get_cxx_pragma_payload_kind()));
    ROSE_ABORT();
  }
  ROSE_ASSERT(pragma_text != nullptr);
  if (pragma_text->empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[pragma-source-text]: pragma has an empty "
            "typed output payload\n");
    ROSE_ABORT();
  }

  if (unp->cur.get_compact_output()) {
    unp->cur.emit_compact_directive(std::string("#pragma ") + *pragma_text);
    return;
  }

  // Request from Boyanna at ANL:
  // DQ (6/22/2006): Start all pragmas at the start of the line.  Since these
  // are handled as IR nodes (#pragma is part of the C and C++ grammar
  // afterall)they are indented for as any other sort of statements.  I have
  // added a CR to put the pragma at the start of the next line.  A better
  // solution might be to have the indent mechanism look ahead to see any
  // upcoming SgPragmaDeclarations so that the indentation (insertion of extra
  // spaces) could be skipped.  This would avoid the insertion of empty lines in
  // the generated code. curprint ( string("#pragma " + pragma->get_pragma() +
  // "\n";

  curprint(string("\n#pragma ") + *pragma_text + "\n");

  // printf ("Output the pragma = %s \n",pragma->get_pragma());
  // ROSE_ASSERT (0);
}

void Unparse_ExprStmt::unparseImportStatement(SgStatement *stmt,
                                              SgUnparse_Info &info) {
  (void)info;
  SgImportStatement *import_stmt = isSgImportStatement(stmt);
  ASSERT_not_null(import_stmt);
  ROSE_ASSERT(import_stmt->get_is_cxx_module_import());

  const std::string module_name = import_stmt->get_module_name().getString();
  ROSE_ASSERT(!module_name.empty());

  curprint("import ");
  curprint(module_name);
  curprint(";");
}

void Unparse_ExprStmt::unparseEmptyDeclaration(SgStatement *stmt,
                                               SgUnparse_Info &info) {
  SgEmptyDeclaration *emptyDeclaration = isSgEmptyDeclaration(stmt);
  ASSERT_not_null(emptyDeclaration);
  static_cast<void>(info);
  emptyDeclaration->validate_lexical_role();
  switch (emptyDeclaration->get_lexical_role()) {
  case SgEmptyDeclaration::e_empty_declaration_source_semicolon:
    curprint(";");
    return;
  case SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor:
  case SgEmptyDeclaration::e_empty_declaration_zero_width_source_replacement:
    // The common statement path emits the exactly owned preprocessing records.
    // These roles deliberately contribute no independent token.
    return;
  }
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[empty-declaration-role]: declaration=%p has "
          "invalid lexical role=%d\n",
          static_cast<void *>(emptyDeclaration),
          static_cast<int>(emptyDeclaration->get_lexical_role()));
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseAccessLabelStatement(SgStatement *stmt,
                                                   SgUnparse_Info &info) {
  SgAccessLabelStatement *label = isSgAccessLabelStatement(stmt);
  ASSERT_not_null(label);
  label->validate();
  if (isSgClassDefinition(label->get_parent()) == nullptr ||
      label->get_scope() != label->get_parent()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[access-label-owner]: access label has no "
            "exact lexical class owner\n");
    ROSE_ABORT();
  }

  if (classMemberHasPreviousSibling(label)) {
    unp->cur.insert_newline(
        2, std::max(unp->cur.statement_indent() - TABINDENT, 0));
  }
  switch (label->get_label_kind()) {
  case SgAccessLabelStatement::e_access_label_private:
    curprint("private:");
    break;
  case SgAccessLabelStatement::e_access_label_protected:
    curprint("protected:");
    break;
  case SgAccessLabelStatement::e_access_label_public:
    curprint("public:");
    break;
  }
  static_cast<void>(info);
  unp->u_sage->curprint_newline();
}

void Unparse_ExprStmt::unparseBasicBlockStmt(SgStatement *stmt,
                                             SgUnparse_Info &info) {
  SgBasicBlock *basic_stmt = isSgBasicBlock(stmt);
  ASSERT_not_null(basic_stmt);
  requireExactOwnedCxxStatementList(basic_stmt);

  if (basic_stmt->get_is_implicit_control_flow_scope()) {
    const SgStatementPtrList &statements = basic_stmt->get_statements();
    SgStatement *only_statement =
        statements.size() == 1 ? statements.front() : nullptr;
    SgNode *parent = basic_stmt->get_parent();
    const bool exact_typed_edge =
        (isSgIfStmt(parent) != nullptr &&
         (isSgIfStmt(parent)->get_true_body() == basic_stmt ||
          isSgIfStmt(parent)->get_false_body() == basic_stmt)) ||
        (isSgForStatement(parent) != nullptr &&
         isSgForStatement(parent)->get_loop_body() == basic_stmt) ||
        (isSgRangeBasedForStatement(parent) != nullptr &&
         isSgRangeBasedForStatement(parent)->get_loop_body() == basic_stmt) ||
        (isSgWhileStmt(parent) != nullptr &&
         isSgWhileStmt(parent)->get_body() == basic_stmt) ||
        (isSgDoWhileStmt(parent) != nullptr &&
         isSgDoWhileStmt(parent)->get_body() == basic_stmt) ||
        (isSgSwitchStatement(parent) != nullptr &&
         isSgSwitchStatement(parent)->get_body() == basic_stmt);
    const auto exact_synthesized_provenance = [](const Sg_File_Info *position) {
      return position != nullptr && position->isCompilerGenerated() &&
             position->isFrontendSpecific() && !position->isTransformation();
    };
    if (!exact_typed_edge || only_statement == nullptr ||
        only_statement->get_parent() != basic_stmt ||
        basic_stmt->get_is_fortran_block_construct() ||
        !exact_synthesized_provenance(basic_stmt->get_file_info()) ||
        !exact_synthesized_provenance(basic_stmt->get_startOfConstruct()) ||
        !exact_synthesized_provenance(basic_stmt->get_endOfConstruct())) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[implicit-control-flow-scope]: block=%p "
              "does not own one exact unbraced controlled statement\n",
              static_cast<void *>(basic_stmt));
      ROSE_ABORT();
    }

    SgUnparse_Info nested_info(info);
    nested_info.unset_SkipSemiColon();
    UnparseLanguageIndependentConstructs::unparseStatement(only_statement,
                                                           nested_info);
    return;
  }

#define DEBUG_BASIC_BLOCK 0

  // unparseAttachedPreprocessingInfo(basic_stmt, info,
  // PreprocessingInfo::before);

#if DEBUG_BASIC_BLOCK || 0
  printf("In unparseBasicBlock (stmt = %p) \n", stmt);
  curprint("/* In unparseBasicBlock */");
#endif

  // DQ (12/15/2014): Debugging, support to detect where this is changed between
  // the top of the block and the bottom of the block.
  bool saved_top_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  // DQ (12/16/2014): The value of info.unparsedPartiallyUsingTokenStream() is
  // used as a sort of global state, we want it to be consistant within the
  // processing of this function. SgUnparse_Info ninfo(info);
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  const bool invalid_function_body_partial_token_boundary =
      saved_unparsedPartiallyUsingTokenStream == true &&
      isSgFunctionDefinition(basic_stmt->get_parent()) != NULL &&
      (info.get_current_source_file() == NULL ||
       info.get_current_source_file()->get_unparse_tokens() == false);
  if (invalid_function_body_partial_token_boundary) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[function-body-partial-token]: function "
            "body inherited partial token replay without an active "
            "token-enabled source file\n");
    ROSE_ABORT();
  }
  bool ast_block_indentation_applied = false;

#if DEBUG_BASIC_BLOCK
  printf("In unparseBasicBlock (stmt = %p) "
         "saved_unparsedPartiallyUsingTokenStream = %s \n",
         basic_stmt,
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
#endif

  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    const bool use_compact_body_layout =
        basicBlockUsesCompactFunctionBodyLayout(basic_stmt, info);
    if (use_compact_body_layout) {
      if (unp->cur.current_col() > 0) {
        if (SgProject::get_verbose() > 0) {
          curprint(" /* syntax from AST */ {");
        } else {
          curprint(" {");
        }
      } else {
        if (SgProject::get_verbose() > 0) {
          curprint("/* syntax from AST */ {");
        } else {
          curprint("{");
        }
      }

      const SgStatementPtrList &statements = basic_stmt->get_statements();
      if (statements.empty()) {
        curprint("}");
      } else {
        const std::optional<int> saved_linewrap = unp->cur.get_linewrap();
        unp->cur.disable_linewrap();
        curprint(" ");
        unparseCompactFunctionBodyStatement(this, statements.front(), info);
        unp->cur.set_linewrap(saved_linewrap);
        curprint(" }");
      }
      return;
    }

    SgNode *parent = basic_stmt->get_parent();
    const bool is_function_body_block = isSgFunctionDefinition(parent) != NULL;
    SgStatement *function_definition = isSgStatement(parent);
    const bool transformed_function_body =
        is_function_body_block &&
        (function_definition->isTransformation() ||
         function_definition->get_containsTransformation() ||
         function_definition
             ->get_containsTransformationToSurroundingWhitespace() ||
         basic_stmt->isTransformation() ||
         basic_stmt->get_containsTransformation() ||
         basic_stmt->get_containsTransformationToSurroundingWhitespace());
    const bool prefer_same_line_open_brace =
        (is_function_body_block &&
         (transformed_function_body ||
          (functionBodyUsesInlineSameLineOpeningBrace(basic_stmt) ||
           functionBodyPrefersSimpleSameLineOpeningBrace(basic_stmt) ||
           functionBodyOriginallyUsedSameLineOpeningBrace(basic_stmt)))) ||
        isSgForStatement(parent) != NULL ||
        isSgRangeBasedForStatement(parent) != NULL ||
        isSgIfStmt(parent) != NULL || isSgDoWhileStmt(parent) != NULL ||
        isSgTryStmt(parent) != NULL || isSgCatchOptionStmt(parent) != NULL;
    const bool statement_already_formatted =
        prefer_same_line_open_brace == false &&
        useStatementFormattingForStandaloneBasicBlock(basic_stmt);
#if DEBUG_USING_CURPRINT
    curprint("\n/* unparse start of SgBasicBlock: "
             "saved_unparsedPartiallyUsingTokenStream == false: output opening "
             "{ */");
#endif

    if (prefer_same_line_open_brace && unp->cur.current_col() > 0) {
      if (SgProject::get_verbose() > 0) {
        curprint(" /* syntax from AST */ {");
      } else {
        curprint(" {");
      }
    } else if (statement_already_formatted) {
      const int indent = unp->cur.statement_indent();
      if (unp->cur.line_is_empty()) {
        if (unp->cur.current_col() < indent) {
          curprint(std::string(indent - unp->cur.current_col(), ' '));
        }
      } else {
        unp->cur.insert_newline(1, indent);
      }
      if (SgProject::get_verbose() > 0) {
        curprint("/* syntax from AST */ {");
      } else {
        curprint("{");
      }
    } else {
      unp->cur.format(basic_stmt, info, FORMAT_BEFORE_BASIC_BLOCK1);
      if (SgProject::get_verbose() > 0) {
        curprint("/* syntax from AST */ {");
      } else {
        curprint("{");
      }
    }

    unp->cur.format(basic_stmt, info, FORMAT_AFTER_BASIC_BLOCK1);
    ast_block_indentation_applied = true;
  } else {
#if DEBUG_USING_CURPRINT
    curprint(
        "\n/* unparse start of SgBasicBlock: "
        "saved_unparsedPartiallyUsingTokenStream == true: output opening { */");
#endif
    // Unparse the tokens from the end of the basic block "{".
    // DQ (6/2/2021): Comment out the opening "{".
    // DQ (1/14/2015): We need to unparse syntax instead of the initial token,
    // because this can be a macro expansion (see
    // tests/nonsmoke/functional/roseTests/astInterfaceTests/inputmoveDeclarationToInnermostScope_test2015_57.C).
    // unparseStatementFromTokenStream (stmt, e_leading_whitespace_start,
    // e_token_subsequence_start); unparseStatementFromTokenStream (stmt,
    // e_token_subsequence_start, e_token_subsequence_start); curprint("{");
    unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                    e_token_subsequence_start, info);
#if DEBUG_BASIC_BLOCK
    curprint("/* unparse start of SgBasicBlock */");
#endif
  }

  // DQ (1/9/2007): This is useful for understanding which blocks are marked as
  // compiler generated. curprint ( string(" /* block compiler generated = " +
  // (basic_stmt->get_startOfConstruct()->isCompilerGenerated() ? "true" :
  // "false") + " */ \n ";

  // curprint ( string(" /* block size = " + basic_stmt->get_statements().size()
  // + " */ \n ";

  // printf ("block scope = %p = %s
  // \n",basic_stmt,basic_stmt->class_name().c_str());
  // basic_stmt->get_file_info()->display("basic_stmt block scope: debug");

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(basic_stmt);
#endif

#if DEBUG_BASIC_BLOCK
  // DQ (1/7/2015): The funcationality to output the trailing tokens of the last
  // statement is implemented in the unparseStatementFromTokenStream() function.
  SgStatement *last_stmt = NULL;
#endif

  // DQ (10/31/2018): We need to get the current source file from the
  // SgUnparseInfo object instead of computing it through the chain of parent
  // pointers in the translation unit.  This is essential for header file
  // processing since the header file will have a copy of the trnslation unit's
  // global scope. DQ (9/28/2018): We need to get the SgSourceFile so that we
  // can use the correct map from the map of maps in the modified implementation
  // that supports multiple files for token based unparsing. SgSourceFile*
  // sourceFile = TransformationSupport::getSourceFile(basic_stmt);
  // DQ (10/31/2018): This is not always non-null (e.g. when used with the
  // unparseToString() function). ASSERT_not_null(sourceFile);

  // DQ (9/28/2018): Older code.
  // SgStatementPtrList::iterator representativeStatementForWhitespace =
  // sourceFile->get_representativeWhitespaceStatementMap()[basic_stmt];

  // DQ (9/28/2018): We need to get the SgSourceFile so that we can use the
  // correct map from the map of maps in the modified implementation that
  // supports multiple files for token based unparsing.
  SgSourceFile *sourceFile = info.get_current_source_file();

  // DQ (11/15/2015): if this is on because it is from an inherited SgBasicBlock
  // then turn off the flag to control formatting. I don't like this method of
  // handling the inherited attribute, and perhaps this poitn to why this
  // formatting should be controled using a different mechanism (though other
  // mechanisms had there problems in thinking them through).
  info.unset_parentStatementListBeingUnparsedUsingPartialTokenSequence();

  SgStatementPtrList::iterator p = basic_stmt->get_statements().begin();
  while (p != basic_stmt->get_statements().end()) {
    ASSERT_not_null((*p));

#if DEBUG_BASIC_BLOCK
    printf("In unparseBasicBlock (block = %p) statement = %p = %s "
           "saved_unparsedPartiallyUsingTokenStream = %s \n",
           basic_stmt, *p, (*p)->class_name().c_str(),
           saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
#endif
#if DEBUG_BASIC_BLOCK && 0
    curprint("/* LOOP: START unparse statement in SgBasicBlock */");
#endif

    SgUnparse_Info local_info(info);
    const bool childParticipatesInCurrentFile =
        sourceFile == NULL || statementFromFile(*p, getFileName(), local_info);

    // DQ (11/4/2015): Adding in the leading white space of the first statement
    // (whatever statement is first).
    if (saved_unparsedPartiallyUsingTokenStream == true &&
        childParticipatesInCurrentFile) {
      // DQ (11/12/2015): We don't want to do this for just the first statement.
      // if (p == basic_stmt->get_statements().begin())
      {
        // We want to output the whitespace of the first statement, but the
        // first statement may have been moved. But we can at least output the
        // leading white space for whateve is currently the first statement.
        // Unfortunately this can cause problems if this is more than just
        // whitespace (e.g. "#if 1"). So we need to check if this is only
        // whitespace and then we can unparse it.  This would be best handled by
        // adding this feature to the unparseStatementFromTokenStream() function
        // (I think).

        local_info
            .set_parentStatementListBeingUnparsedUsingPartialTokenSequence();

        // curprint("\n");
        const bool statement_is_transformation =
            sourceFile != NULL
                ? canBeUnparsedFromTokenStream(sourceFile, *p) == false
                : ((*p)->isTransformation() ||
                   (*p)->get_containsTransformation() ||
                   (*p)->get_containsTransformationToSurroundingWhitespace());
#if DEBUG_BASIC_BLOCK || 0
        printf("statement is: %p = %s isTransformation() = %s \n", (*p),
               (*p)->class_name().c_str(),
               (*p)->isTransformation() ? "true" : "false");
        string s = statement_is_transformation ? "true" : "false";
#endif
        if (statement_is_transformation == true) {
          // A transformed child has no physical token surface.  Its AST
          // emission owns a canonical statement boundary; borrowing the
          // leading interval of an unrelated source sibling made token replay
          // depend on a stale alias after declaration movement.
          curprint("\n");
        }
#if DEBUG_BASIC_BLOCK || 0
        curprint("/* unparse leading white space of first statement: END */");
#endif
      }
    }

    const bool block_is_first_statement_of_parent_block = [&]() -> bool {
      SgBasicBlock *parent_block = isSgBasicBlock(basic_stmt->get_parent());
      return parent_block != nullptr &&
             !parent_block->get_statements().empty() &&
             parent_block->get_statements().front() == basic_stmt;
    }();

    const bool first_transformed_statement_without_leading_preproc =
        saved_unparsedPartiallyUsingTokenStream == false &&
        childParticipatesInCurrentFile &&
        p == basic_stmt->get_statements().begin() &&
        block_is_first_statement_of_parent_block &&
        locatedNodeHasBeforePreprocessingInfo(basic_stmt) == false &&
        isSgDeclarationStatement(*p) != nullptr &&
        basicBlockStartsWithLeadingPreprocessingInfo(basic_stmt) == false &&
        (basic_stmt->isTransformation() ||
         basic_stmt->get_containsTransformation() ||
         basic_stmt->get_containsTransformationToSurroundingWhitespace()) &&
        ((*p)->isTransformation() || (*p)->get_containsTransformation() ||
         (*p)->get_containsTransformationToSurroundingWhitespace());
    if (first_transformed_statement_without_leading_preproc) {
      // A transformed block entry without an attached leading comment/directive
      // needs a full blank separator of its own because the downstream
      // statement formatter path is suppressed for these transformed entries.
      unp->cur.insert_newline(unp->cur.current_col() > 0 ? 2 : 3);
    }

#if DEBUG_BASIC_BLOCK || 0
    curprint("/* calling unparseStatement(): START */");
#endif
    // unparseStatement((*p), info);
    unparseStatement((*p), local_info);

#if DEBUG_BASIC_BLOCK || 0
    curprint("/* calling unparseStatement(): END */");
#endif

#if DEBUG_BASIC_BLOCK || 0
    curprint("/* LOOP: END unparse statement in SgBasicBlock */");
#endif
    // DQ (12/6/2014): Save the last statement so that we can use the trailing
    // token stream if using the token-based unparsing. last_stmt = *p;

    p++;
  }

#if DEBUG_BASIC_BLOCK || 0
  printf(
      "Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output comment \n");
  curprint("/* Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output "
           "comment */");
#endif

#if DEBUG_BASIC_BLOCK
  printf("Inside of Unparse_ExprStmt::unparseBasicBlockStmt: "
         "saved_unparsedPartiallyUsingTokenStream = %s \n",
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
#endif

  // DQ (12/16/2014): This should be controled by the
  // saved_unparsedPartiallyUsingTokenStream value. DQ (3/17/2005): This helps
  // handle cases such as void foo () { #include "constant_code.h" }
  // unparseAttachedPreprocessingInfo(basic_stmt, info,
  // PreprocessingInfo::inside);

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (3/17/2005): This helps handle cases such as void foo () { #include
    // "constant_code.h" }

#if DEBUG_BASIC_BLOCK || 0
    printf("Calling unparseAttachedPreprocessingInfo(): INSIDE: basic_stmt = "
           "%p = %s \n",
           basic_stmt, basic_stmt->class_name().c_str());
    printOutComments(basic_stmt);
    printf("In unparseBasicBlockStmt(): info.SkipFunctionDefinition() = %s \n",
           info.SkipFunctionDefinition() ? "true" : "false");
#endif

    unparseAttachedPreprocessingInfo(basic_stmt, info,
                                     PreprocessingInfo::inside);
  }

#if DEBUG_BASIC_BLOCK
  printf("DONE: Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output "
         "comment \n");
  curprint("/* DONE: Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output "
           "comment */");
#endif

#if DEBUG_BASIC_BLOCK
  printf("unparse end of SgBasicBlock: "
         "info.unparsedPartiallyUsingTokenStream() = %s last_stmt = %p \n",
         info.unparsedPartiallyUsingTokenStream() ? "true" : "false",
         last_stmt);
  if (last_stmt != NULL) {
    printf("   --- last_stmt = %p = %s \n", last_stmt,
           last_stmt->class_name().c_str());
  }
#endif

  // A child can use a copied context to cross an AST/token frontier, but must
  // not leak that choice through the enclosing block's shared context.
  ROSE_ASSERT(saved_top_unparsedPartiallyUsingTokenStream ==
              info.unparsedPartiallyUsingTokenStream());

#if DEBUG_BASIC_BLOCK
  curprint("/* unparse end of SgBasicBlock */");
#endif
#if DEBUG_USING_CURPRINT
  curprint("\n/* unparse end of SgBasicBlock: output closing } */");
#endif

  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    if (ast_block_indentation_applied) {
      unp->cur.format(basic_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
    } else {
      const int indent = unp->cur.statement_indent();
      if (unp->cur.line_is_empty()) {
        if (unp->cur.current_col() < indent) {
          curprint(std::string(indent - unp->cur.current_col(), ' '));
        }
      } else {
        unp->cur.insert_newline(1, indent);
      }
    }

#if DEBUG_USING_CURPRINT
    curprint("\n/* unparse end of SgBasicBlock: "
             "saved_unparsedPartiallyUsingTokenStream == false: output closing "
             "} */");
#endif
    // curprint ( string("}"));
    if (SgProject::get_verbose() > 0)
      curprint("/* syntax from AST */ }");
    else
      curprint("}");

    unp->cur.format(basic_stmt, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {
#if DEBUG_BASIC_BLOCK
    printf("unparse last token in SgBasicBlock \n");
    curprint("/* unparse last token in SgBasicBlock */");
#endif

#if DEBUG_USING_CURPRINT
    curprint(
        "\n/* unparse end of SgBasicBlock: "
        "saved_unparsedPartiallyUsingTokenStream == true: output closing } */");
#endif
    // DQ (1/14/2015): We need to unparse syntax instead of the initial token,
    // because this can be a macro expansion (see
    // tests/nonsmoke/functional/roseTests/astInterfaceTests/inputmoveDeclarationToInnermostScope_test2015_57.C).
    // unparseStatementFromTokenStream (stmt, e_token_subsequence_end,
    // e_token_subsequence_end);

#if DEBUG_BASIC_BLOCK
    // DQ (5/25/2021): I think this might be a mistake to output anything here,
    // since the token stream's closing brace should be output.
    printf("Should we be skipping output of closing } in "
           "unparseBasicBlockStmt() \n");
#endif

#if DEBUG_USING_CURPRINT || 0
    curprint("\n/* unparseBasicBlock(): unparse closing } (skipping) */\n");
#endif
    // Unparse the tokens from the end of the basic block "{".
    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_token_subsequence_start, info);
    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (stmt, function_definition,
    // e_token_subsequence_start, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (stmt, class_definition,
    // e_token_subsequence_end, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (class_definition, stmt,
    // e_token_subsequence_end, e_token_subsequence_end, info);

    if (sourceFile == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[block-suffix]: partial block=%p has no "
              "current source file\n",
              static_cast<void *>(basic_stmt));
      ROSE_ABORT();
    }

    SgStatement *lastMappedChild = NULL;
    const auto &tokenMap = sourceFile->get_tokenSubsequenceMap();
    for (auto child = basic_stmt->get_statements().rbegin();
         child != basic_stmt->get_statements().rend(); ++child) {
      auto mapping = tokenMap.find(*child);
      if (mapping != tokenMap.end()) {
        if (mapping->second == NULL) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[block-suffix]: file=%s child=%s@%d "
                  "has a null token mapping\n",
                  sourceFile->getFileName().c_str(),
                  (*child)->sage_class_name(),
                  (*child)->get_file_info() != nullptr
                      ? (*child)->get_file_info()->get_line()
                      : 0);
          ROSE_ABORT();
        }
        const TokenStreamHalfOpenInterval &core =
            mapping->second->halfOpenInterval(
                TokenStreamIntervalKind::token_subsequence);
        if (core.empty()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[block-suffix]: file=%s child=%s@%d "
                  "has an empty token subsequence\n",
                  sourceFile->getFileName().c_str(),
                  (*child)->sage_class_name(),
                  (*child)->get_file_info() != nullptr
                      ? (*child)->get_file_info()->get_line()
                      : 0);
          ROSE_ABORT();
        }
        lastMappedChild = *child;
        break;
      }
    }

    if (lastMappedChild != NULL) {
      // Replay the exact suffix after the final direct child through the
      // block-owned closing brace.  This interval owns inactive preprocessor
      // branches and closing directives that have no AST statement of their
      // own.
      const bool childTrailingIntervalAlreadyEmitted =
          statementsWithTokenEmittedTrailingWhitespace.erase(lastMappedChild) >
          0;
      if (childTrailingIntervalAlreadyEmitted) {
        unparseStatementFromTokenStream(
            lastMappedChild, basic_stmt, e_trailing_whitespace_end,
            e_token_subsequence_end, info, false, 1, 0);
      } else {
        unparseStatementFromTokenStream(lastMappedChild, basic_stmt,
                                        e_trailing_whitespace_start,
                                        e_token_subsequence_end, info);
      }
    } else {
      // An originally empty block has no child anchor.  Skip its already
      // emitted opening brace and replay the remainder of the exact block
      // surface, including any interior comments/directives and the closer.
      unparseStatementFromTokenStream(
          basic_stmt, basic_stmt, e_token_subsequence_start,
          e_token_subsequence_end, info, false, 1, 0);
    }
  }

#if DEBUG_USING_CURPRINT
  curprint("\n/* Leaving unparseBasicBlock */");
#endif
#if DEBUG_BASIC_BLOCK || 0
  printf("Leaving unparseBasicBlock (stmt = %p) \n", stmt);
  curprint("/* Leaving unparseBasicBlock */");
#endif
}

// True when emitting `statement` as an unbraced controlled statement would
// leave an unmatched `if` able to consume a following `else`.
static bool statementCanCaptureFollowingElse(SgStatement *statement) {
  if (statement == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[dangling-else-shape]: null controlled "
            "statement\n");
    ROSE_ABORT();
  }

  switch (statement->variantT()) {
  case V_SgCaseOptionStmt:
    return statementCanCaptureFollowingElse(
        isSgCaseOptionStmt(statement)->get_body());
  case V_SgCatchStatementSeq: {
    SgCatchStatementSeq *cs = isSgCatchStatementSeq(statement);
    const SgStatementPtrList &seq = cs->get_catch_statement_seq();
    if (seq.empty()) {
      fprintf(stderr, "REX_UNPARSE_INVARIANT[dangling-else-shape]: empty catch "
                      "sequence\n");
      ROSE_ABORT();
    }
    return statementCanCaptureFollowingElse(seq.back());
  }
  case V_SgDefaultOptionStmt:
    return statementCanCaptureFollowingElse(
        isSgDefaultOptionStmt(statement)->get_body());
  case V_SgLabelStatement:
    return statementCanCaptureFollowingElse(
        isSgLabelStatement(statement)->get_statement());
  case V_SgCatchOptionStmt:
    return statementCanCaptureFollowingElse(
        isSgCatchOptionStmt(statement)->get_body());
  case V_SgForStatement:
    return statementCanCaptureFollowingElse(
        isSgForStatement(statement)->get_loop_body());
  case V_SgIfStmt: {
    SgIfStmt *ifs = isSgIfStmt(statement);
    if (ifs->get_false_body() == nullptr) {
      return true;
    }
    return statementCanCaptureFollowingElse(ifs->get_false_body());
  }
  case V_SgWhileStmt:
    return statementCanCaptureFollowingElse(
        isSgWhileStmt(statement)->get_body());

  default:
    return false;
  }
}

void Unparse_ExprStmt::unparseIfStmt(SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (12/13/2005): I don't like this implementation with the while loop...

  SgIfStmt *if_stmt = isSgIfStmt(stmt);
  ASSERT_not_null(if_stmt);

  // The if header is one punctuation-bearing syntax unit.  A transformed
  // descendant invalidates replay of slices around the embedded condition;
  // emit the complete header from the AST in that case while leaving each body
  // free to select its own token mode.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream() &&
      !if_stmt->get_containsTransformation() && !if_stmt->get_isModified();

  while (if_stmt != NULL) {
    SgStatement *tmp_stmt = NULL;
    SgStatement *true_body = if_stmt->get_true_body();
    SgStatement *false_body = if_stmt->get_false_body();
    if (true_body == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[if-true-body]: if-statement=%p has no "
              "true body\n",
              static_cast<void *>(if_stmt));
      ROSE_ABORT();
    }
    const bool brace_true_body_for_outer_else =
        false_body != nullptr && isSgBasicBlock(true_body) == nullptr &&
        statementCanCaptureFollowingElse(true_body);
    if (saved_unparsedPartiallyUsingTokenStream &&
        brace_true_body_for_outer_else) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[token-replayed-dangling-else]: "
              "if-statement=%p exact token surface conflicts with its AST "
              "else ownership\n",
              static_cast<void *>(if_stmt));
      ROSE_ABORT();
    }

    // DQ (12/6/2014): Test for if we have unparsed partially using the token
    // stream. If so then we don't want to unparse this syntax, if not then we
    // require this syntax. curprint ( string("if (")); if
    // (info.unparsedPartiallyUsingTokenStream() == false)
    if (saved_unparsedPartiallyUsingTokenStream == false) {
      curprint("if (");

      SgUnparse_Info testInfo(info);
      testInfo.set_SkipSemiColon();
      testInfo.set_inConditional();
      // info.set_inConditional();
      tmp_stmt = if_stmt->get_conditional();
      if (tmp_stmt == nullptr) {
        std::cerr << "REX_UNPARSER_INVARIANT[if-condition]: SgIfStmt has no "
                     "condition\n";
        ROSE_ABORT();
      }
      testInfo.set_template_argument_qualification_context(tmp_stmt);
      // A condition is embedded syntax, not an independent statement.
      // Statement-level dispatch may replay the original `if (` boundary
      // beside the AST-emitted header and duplicate or drop parentheses.
      // It also means that preprocessing information lexically owned by the
      // embedded condition must be emitted here. In particular, Clang attaches
      // a conditional branch's closing #endif to the active condition as
      // `after` when #if/#else select between complete `if` headers.
      unparseAttachedPreprocessingInfo(tmp_stmt, testInfo,
                                       PreprocessingInfo::before);
      if (isSgExprStatement(tmp_stmt) != nullptr) {
        unparseExprStmt(tmp_stmt, testInfo);
      } else if (isSgVariableDeclaration(tmp_stmt) != nullptr) {
        unparseVarDeclStmt(tmp_stmt, testInfo);
      } else {
        std::cerr << "REX_UNPARSER_INVARIANT[if-condition]: unsupported "
                     "embedded condition statement kind "
                  << tmp_stmt->class_name() << "\n";
        ROSE_ABORT();
      }
      testInfo.unset_inConditional();
      // curprint ( string(") "));
      if (SgProject::get_verbose() > 0)
        curprint("/* syntax from AST */ ) ");
      else if (isSgBasicBlock(if_stmt->get_true_body()) != nullptr)
        curprint(")");
      else
        curprint(")");

      unparseAttachedPreprocessingInfo(tmp_stmt, testInfo,
                                       PreprocessingInfo::after);

      // DQ (9/24/2020): Adding support to unparse attached pragmas.
      unparsePragmaAttribute(if_stmt);
    } else {
      // DQ (12/9/2014): Adding more support for unparsing using the token
      // stream.
      SgStatement *true_body = if_stmt->get_true_body();
      if (true_body != NULL) {
        // DQ (1/18/2015): With the denormalization of SgBasicBlock in the
        // SgIfStmt false body we have to use the computed if_stmt and not the
        // outer stmt. unparseStatementFromTokenStream (stmt, true_body,
        // e_leading_whitespace_start, e_token_subsequence_start);
        // unparseStatementFromTokenStream (stmt, true_body,
        // e_token_subsequence_start, e_token_subsequence_start);
        // unparseStatementFromTokenStream (stmt, true_body,
        // e_token_subsequence_start, e_leading_whitespace_start);
        unparseStatementFromTokenStream(if_stmt, true_body,
                                        e_token_subsequence_start,
                                        e_leading_whitespace_start, info);
      }
    }

    if ((tmp_stmt = true_body)) {
      // DQ (12/6/2014): Test for if we have unparsed partially using the token
      // stream. If so then we don't want to unparse this syntax, if not then we
      // require this syntax. unp->cur.format(tmp_stmt, info,
      // FORMAT_BEFORE_NESTED_STATEMENT); if
      // (info.unparsedPartiallyUsingTokenStream() == false)
      if (saved_unparsedPartiallyUsingTokenStream == false) {
        unp->cur.format(tmp_stmt, info, FORMAT_BEFORE_NESTED_STATEMENT);
        if (brace_true_body_for_outer_else) {
          curprint("{");
        }
      }

      // Unparse using base class function so we get any required comments and
      // CPP directives. unparseStatement(tmp_stmt, info);
      SgUnparse_Info nestedInfo(info);
      nestedInfo.unset_SkipSemiColon();
      UnparseLanguageIndependentConstructs::unparseStatement(tmp_stmt,
                                                             nestedInfo);

      if (brace_true_body_for_outer_else) {
        curprint(" }");
      }

      // DQ (12/6/2014): Test for if we have unparsed partially using the token
      // stream. If so then we don't want to unparse this syntax, if not then we
      // require this syntax. unp->cur.format(tmp_stmt, info,
      // FORMAT_AFTER_NESTED_STATEMENT); if
      // (info.unparsedPartiallyUsingTokenStream() == false)
      if (saved_unparsedPartiallyUsingTokenStream == false) {
        unp->cur.format(tmp_stmt, info, FORMAT_AFTER_NESTED_STATEMENT);
      }
    }

    if ((tmp_stmt = false_body)) {
      // unp->cur.format(if_stmt, info, FORMAT_BEFORE_STMT);
      // curprint ( string("else "));
      // if (info.unparsedPartiallyUsingTokenStream() == false)
      if (saved_unparsedPartiallyUsingTokenStream == false) {
        if (!shouldEmitElseOnSameLine(if_stmt)) {
          unp->cur.format(if_stmt, info, FORMAT_BEFORE_STMT);
        }
        // curprint ( string("else "));
        if (SgProject::get_verbose() > 0) {
          curprint("/* syntax from AST (part 1) */ else ");
        } else {
          // DQ (1/4/2015): Remove trailing space to avoid redundant output of
          // whitespace in token unparsing. We actually need to extra space to
          // avoid unparsing "elseif" by mistake (likely we can address this
          // detail later). curprint(" else ");
          curprint(isSgBasicBlock(tmp_stmt) != nullptr ? " else" : " else ");
        }
      } else {
        // DQ (12/9/2014): Adding more support for unparsing using the token
        // stream.
        SgStatement *true_body = if_stmt->get_true_body();
        SgStatement *false_body = if_stmt->get_false_body();
        if (true_body != NULL && false_body != NULL) {
          // DQ (1/4/2015): If the false body is a transformation then the token
          // sequence will not exist and the unparseStatementFromTokenStream()
          // function will not output any token sequence.
          // unparseStatementFromTokenStream (true_body, false_body,
          // e_trailing_whitespace_start, e_token_subsequence_start);
          if (SgProject::get_verbose() > 0) {
            curprint("/* syntax from AST (part 2) */ else ");
          } else {
            // curprint(" else ");
            // printf ("In unparseIfStmt(): Output the else part between the
            // true and false cases of the if statement \n");
            // unparseStatementFromTokenStream (true_body, false_body,
            // e_trailing_whitespace_start, e_token_subsequence_start);
            // unparseStatementFromTokenStream (true_body, true_body,
            // e_trailing_whitespace_start, e_trailing_whitespace_end);

            // We might need to check that there are whitespace tokens assocated
            // with the trailing whitespace before the else. Also if the false
            // block is a transformation then we need to output a CR or a space.
            if (true_body->isTransformation() == true ||
                false_body->isTransformation() == true) {
              curprint(" else ");
            } else {
              // unparseStatementFromTokenStream (false_body, false_body,
              // e_leading_whitespace_start, e_leading_whitespace_end);
              // unparseStatementFromTokenStream (true_body, false_body,
              // e_trailing_whitespace_start, e_leading_whitespace_start);
              // unparseStatementFromTokenStream (true_body, false_body,
              // e_trailing_whitespace_start, e_leading_whitespace_start);
              unparseStatementFromTokenStream(true_body, false_body,
                                              e_trailing_whitespace_start,
                                              e_leading_whitespace_start, info);
            }
          }
        }
      }
      if_stmt = isSgIfStmt(tmp_stmt);
      if (if_stmt == NULL) {
        // unp->cur.format(tmp_stmt, info, FORMAT_BEFORE_NESTED_STATEMENT);
        // if (info.unparsedPartiallyUsingTokenStream() == false)
        if (saved_unparsedPartiallyUsingTokenStream == false) {
          unp->cur.format(tmp_stmt, info, FORMAT_BEFORE_NESTED_STATEMENT);
        }
        // Unparse using base class function so we get any required comments and
        // CPP directives. unparseStatement(tmp_stmt, info);
        SgUnparse_Info nestedInfo(info);
        nestedInfo.unset_SkipSemiColon();
        UnparseLanguageIndependentConstructs::unparseStatement(tmp_stmt,
                                                               nestedInfo);
        // if (info.unparsedPartiallyUsingTokenStream() == false)
        if (saved_unparsedPartiallyUsingTokenStream == false) {
          unp->cur.format(tmp_stmt, info, FORMAT_AFTER_NESTED_STATEMENT);
        }
      }
    } else {
      if_stmt = NULL;
    }

    // DQ (12/16/2008): Need to process any associated CPP directives and
    // comments
    if (if_stmt != NULL) {
      // At this point if_stmt is a nested if statement in the true and false
      // branch of the original if statement.
      if (saved_unparsedPartiallyUsingTokenStream == true) {
        // DQ (7/2/2021): I think this should be unparsed via the
        // unparseStatement called fro the false body. New code where we unparse
        // the whitespace between the else and the nested if statement.
        // unparseStatementFromTokenStream (false_body, false_body,
        // e_leading_whitespace_start, e_token_subsequence_start);
        // unparseStatementFromTokenStream (if_stmt, e_leading_whitespace_start,
        // e_token_subsequence_start, info); unparseStatementFromTokenStream
        // (if_stmt, e_leading_whitespace_start, e_leading_whitespace_end,
        // info);
      } else {
        // original code if we are not unparsing from the token stream.
        unparseAttachedPreprocessingInfo(if_stmt, info,
                                         PreprocessingInfo::before);
      }
    }
  }
}

// DQ (8/13/2007): This is no longer used, I think, however it might be required
// for the legacy array optimizer.

//--------------------------------------------------------------------------------
//  void Unparse_ExprStmt::unparseWhereStmt
//
//  This special function unparses where and elsewhere statements. Where
//  statements are actually represented as for statements in the Sage program
//  tree. Thus, the type of the where_stmt is SgForStatement. The part that
//  we are interested in unparsing is in the initializer statement of the
//  for statement. In particular, we want to unparse the arguments of the
//  rhs of the initializer. The rhs should be a function call expression.
//  The same applies for elsewhere statements.
//--------------------------------------------------------------------------------
void Unparse_ExprStmt::unparseWhereStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgForStatement *where_stmt = isSgForStatement(stmt);
  ASSERT_not_null(where_stmt);

  printf("In Unparse_ExprStmt::unparseWhereStmt() \n");

  SgStatement *tmp_stmt;
  // DQ (4/7/2001) we don't want the unparser to depend on the array grammar
  // (so comment this out and introduce the array "where" statment in some other
  // way)
  curprint(string("elsewhere ("));

  SgUnparse_Info newinfo(info);
  newinfo.set_SkipSemiColon();
  // if(where_stmt->get_init_stmt() != NULL ) {
  if (where_stmt->get_init_stmt().size() > 0) {
    SgStatementPtrList::iterator i = where_stmt->get_init_stmt().begin();
    if ((*i) != NULL && (*i)->variant() == EXPR_STMT) {
      SgExprStatement *pExprStmt = isSgExprStatement(*i);
      // SgAssignOp* pAssignOp = isSgAssignOp(pExprStmt->get_the_expr());
      SgAssignOp *pAssignOp = isSgAssignOp(pExprStmt->get_expression());
      if (pAssignOp != NULL) {
        SgFunctionCallExp *pFunctionCallExp =
            isSgFunctionCallExp(pAssignOp->get_rhs_operand());
        if (pFunctionCallExp != NULL) {
          if (pFunctionCallExp->get_args()) {
            SgExpressionPtrList &list =
                pFunctionCallExp->get_args()->get_expressions();
            SgExpressionPtrList::iterator arg = list.begin();
            while (arg != list.end()) {
              unparseExpression((*arg), newinfo);
              arg++;
              if (arg != list.end()) {
                curprint(string(","));
              }
            }
          }
        } // pFunctionCallExp != NULL
      } // pAssignOp != NULL
    } //(*i).irep() != NULL && (*i).irep()->variant() == EXPR_STMT
  } // where_stmt->get_init_stmt() != NULL

  curprint(string(")"));

  if ((tmp_stmt = where_stmt->get_loop_body())) {
    unparseStatement(tmp_stmt, info);
  } else {
    if (!info.SkipSemiColon()) {
      curprint(string(";"));
    }
  }
}

void Unparse_ExprStmt::unparseForInitStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  // DQ (7/11/2004): Added to simplify debugging for everyone (requested by
  // Willcock)

  SgForInitStatement *forInitStmt = isSgForInitStatement(stmt);
  ASSERT_not_null(forInitStmt);

  const SgStatementPtrList &initStatements = forInitStmt->get_init_stmt();
  SgStatementPtrList sourceInitStatements(initStatements.begin(),
                                          initStatements.end());
  SgStatementPtrList::iterator i = sourceInitStatements.begin();

  // DQ (12/8/2004): Build a new info object so that we can supress the
  // unparsing of the base type once the first variable has been unparsed.
  SgUnparse_Info newinfo(info);
  newinfo.set_SkipSemiColon();

  while (i != sourceInitStatements.end()) {

    unparseStatement(*i, newinfo);
    i++;

    // After unparsing the first variable declaration with the type
    // we want to unparse the rest without the base type.
    newinfo.set_SkipBaseType();

    if (i != sourceInitStatements.end()) {
      curprint(string(", "));
    }
  }

  // DQ (11/4/2015): Change the unparsing semantics to for loop initializer
  // statement to exclude the " " after the ";" so that we can more faithfully
  // represent the unparsed code when using the token-based unparsing.
  // curprint("; ");
  curprint(";");
}

void Unparse_ExprStmt::unparseForStmt(SgStatement *stmt, SgUnparse_Info &info) {
  // printf ("Unparse for loop \n");
  SgForStatement *for_stmt = isSgForStatement(stmt);
  ASSERT_not_null(for_stmt);
  SgForInitStatement *for_init_stmt = requireExactForInitializer(for_stmt);

  // A for-header is one punctuation-bearing syntactic unit.  Once its
  // structure changes, replaying token slices for the test or increment next
  // to an AST-emitted initializer can duplicate the original separators.
  // Emit the complete header from its exact AST while still allowing the loop
  // body to retain its independently selected token mode.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream() &&
      !for_stmt->get_containsTransformation() && !for_stmt->get_isModified();

#define DEBUG_FOR_STMT 0

#if DEBUG_FOR_STMT
  printf("In unparseForStmt(): saved_unparsedPartiallyUsingTokenStream = %s \n",
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
  curprint("/* Top of unparseForStmt */");
#endif

  // curprint ( string("for ("));
  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  if (saved_unparsedPartiallyUsingTokenStream == false) {
#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the \"for (\" directly (not using the "
           "token stream) \n");
#endif
    curprint("for (");
  } else {
    // unparseStatementFromTokenStream (stmt, e_leading_whitespace_start,
    // e_token_subsequence_start);

#if DEBUG_FOR_STMT
    curprint("/* unparse start of SgForStatement */");
    printf("In unparseForStmt(): unparse from token stream from start of "
           "SgForStatement to for loop initializer \n");
#endif

    // DQ (6/5/2021): The problem here is that if the for_init_stmt does not
    // have any leading whitespace the call to unparseStatementFromTokenStream()
    // will not output anything. However, I have now modified the
    // unparseStatementFromTokenStream() function to use
    // e_token_subsequence_start - 1 when e_leading_whitespace_start is note
    // defined (value == -1). More of these sorts of modifications should be
    // possible. unparseStatementFromTokenStream (for_stmt, for_init_stmt,
    // e_token_subsequence_start, e_token_subsequence_start);
    // The initializer's leading-whitespace range belongs to the initializer,
    // not to the `for (` header.  Replaying it here can inject a newline before
    // an AST-emitted transformed initializer, producing `for (\nint ...`.
    unparseStatementFromTokenStream(
        for_stmt, for_init_stmt, e_token_subsequence_start,
        e_leading_whitespace_start, info, false, 0, -1);
#if DEBUG_FOR_STMT
    curprint("/* DONE: unparse start of SgForStatement */");
#endif
  }

  // if (saved_unparsedPartiallyUsingTokenStream == false)
  {
    SgUnparse_Info newinfo(info);
    if (!saved_unparsedPartiallyUsingTokenStream) {
      newinfo.unset_unparsedPartiallyUsingTokenStream();
    }
    newinfo.set_SkipSemiColon();
    newinfo.set_inConditional(); // set to prevent printing line and file
                                 // information
    newinfo.set_SkipFormatting();

#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the for loop initializer \n");
#endif

    SgStatementPtrList &init_stmts = for_init_stmt->get_init_stmt();
    if (init_stmts.size() == 1) {
      SgVariableDeclaration *variable_decl =
          isSgVariableDeclaration(init_stmts.front());
      if (variable_decl == nullptr) {
        unparseForInitStmt(for_init_stmt, newinfo);
      } else {
        SgUnparse_Info declaration_info(newinfo);
        declaration_info.set_template_argument_qualification_context(
            variable_decl);
        unparseVarDeclStmt(variable_decl, declaration_info);
        curprint(";");
      }
    } else {
      unparseForInitStmt(for_init_stmt, newinfo);
    }

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      // DQ (11/4/2015): Change the unparsing semantics to for loop initializer
      // statement to exclude the " " after the ";" so that we can more
      // faithfully represent the unparsed code when using token-based
      // unparsing.
      curprint(" ");
    }

#if DEBUG_FOR_STMT
    curprint("/* DONE: Unparse the for_init_stmt */\n ");
#endif
    newinfo.unset_inConditional();

#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the for loop test expression (can be "
           "simple declaration statement) \n");
#endif

    // DQ (12/13/2005): New code for handling the test (which could be a
    // declaration!)
#if DEBUG_FOR_STMT
    printf("Output the test in the for statement format "
           "newinfo.inConditional() = %s \n",
           newinfo.inConditional() ? "true" : "false");
    curprint(" /* test */ ");
#endif
    SgStatement *test_stmt = for_stmt->get_test();
    ASSERT_not_null(test_stmt);
    // if ( test_stmt != NULL )
    SgUnparse_Info testinfo(info);
    if (!saved_unparsedPartiallyUsingTokenStream) {
      testinfo.unset_unparsedPartiallyUsingTokenStream();
      // The test is embedded in the for-header.  Statement-level formatting
      // would replay the source separator that preceded it and create an
      // empty condition after an AST-emitted initializer.
      testinfo.set_SkipFormatting();
    }

    // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
    // statement instead of a conditional.  This should make it processing more
    // uniform and independent of if the test statement is unparsed using either
    // the AST or the token stream. testinfo.set_SkipSemiColon();
    // testinfo.set_inConditional();

    // DQ (11/2/2015): With the new change to not set SkipSemiColon and
    // inConditional unparse info fields, we have to explicitly specify that we
    // want to skip class elaboration as well (SkipClassSpecifier).  See
    // test2015_110.C and other older test codes.  Additionally, we need to make
    // this as a conditional so that it will be unparsed as "type var = value"
    // instead of "type var(value)" in the case of a class constructor
    // initialization call. This is because the syntax required for C++ in a
    // condition is that of a simple declaration which is a part of C++ syntax
    // not directly supported in ROSE for simplicity.
    testinfo.set_SkipClassSpecifier();
    testinfo.set_inConditional();
    testinfo.set_template_argument_qualification_context(test_stmt);

#if DEBUG_FOR_STMT
    printf("Output the test in the for statement format "
           "testinfo.inConditional() = %s \n",
           testinfo.inConditional() ? "true" : "false");
#endif
    if (!saved_unparsedPartiallyUsingTokenStream) {
      // A for-test is embedded syntax, not an independent statement.  Calling
      // the statement wrapper here would replay statement-boundary source text
      // and can introduce a second separator after an AST-emitted initializer.
      if (isSgExprStatement(test_stmt) != nullptr) {
        unparseExprStmt(test_stmt, testinfo);
      } else if (isSgVariableDeclaration(test_stmt) != nullptr) {
        unparseVarDeclStmt(test_stmt, testinfo);
      } else if (isSgNullStatement(test_stmt) != nullptr) {
        unparseNullStatement(test_stmt, testinfo);
      } else {
        std::cerr << "REX_UNPARSER_INVARIANT[for-test]: unsupported embedded "
                     "test statement kind "
                  << test_stmt->class_name() << "\n";
        ROSE_ABORT();
      }
    } else {
      unparseStatement(test_stmt, testinfo);
    }
#if DEBUG_FOR_STMT
    printf("In unparseForStatement(): saved_unparsedPartiallyUsingTokenStream "
           "= %s \n",
           saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
    printf("In unparseForStatement(): test_stmt->isTransformation()           "
           "= %s \n",
           test_stmt->isTransformation() ? "true" : "false");
#endif
    // DQ (4/6/2015): If the test is a transformation, then we have to output
    // the semi-colon directly (see inliner tutorial test).
    if (saved_unparsedPartiallyUsingTokenStream == true &&
        test_stmt->isTransformation() == true) {
      // ROSE_ASSERT(test_stmt->isTransformation() == true);
      // curprint (" /* output semi-colon at end of test */ ");

      // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
      // statement instead of a conditional.  This should make it processing
      // more uniform and independe of if the test statement is unparsed using
      // either the AST or the token stream. curprint (";");
    }

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
      // statement instead of a conditional.  This should make it processing
      // more uniform and independe of if the test statement is unparsed using
      // either the AST or the token stream.  However, we want to add a space
      // after the test and before the increment to make the generated code
      // better looking.
      curprint(" ");
    }

#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the for loop increment expression \n");
#endif

    // DQ (10/14/2015): If the test was unparsed from the AST then we need to
    // unparse the increment from the AST, but it the test was unparsed as parrt
    // of a partial token stream unparse of the SgForStatement, then we can
    // unparse the increment from the token stream (as a continuation of the use
    // of the token stream in the test). DQ (12/5/2014): Test for if we have
    // unparsed partially using the token stream. If so then we don't want to
    // unparse this syntax, if not then we require this syntax. if
    // (info.unparsedPartiallyUsingTokenStream() == false) if
    // (saved_unparsedPartiallyUsingTokenStream == false) if
    // (saved_unparsedPartiallyUsingTokenStream == false &&
    // test_stmt->isTransformation() == true) if (test_stmt->isTransformation()
    // == true) if (saved_unparsedPartiallyUsingTokenStream == false ||
    // test_stmt->isTransformation() == false)
    // if (saved_unparsedPartiallyUsingTokenStream == false)
    // if (saved_unparsedPartiallyUsingTokenStream == false ||
    // test_stmt->isTransformation() == false) if (test_stmt->isTransformation()
    // == true) if (saved_unparsedPartiallyUsingTokenStream == false &&
    // test_stmt->isTransformation() == true) if (test_stmt->isTransformation()
    // == true) if (saved_unparsedPartiallyUsingTokenStream == false) if (
    // (saved_unparsedPartiallyUsingTokenStream == false ||
    // test_stmt->isTransformation() == false) )
    if (saved_unparsedPartiallyUsingTokenStream == false) {
      // curprint (" /* output semi-colon before increment */ ");

      // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
      // statement instead of a conditional.  This should make it processing
      // more uniform and independe of if the test statement is unparsed using
      // either the AST or the token stream. curprint("; ");
    }

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      SgExpression *increment_expr = for_stmt->get_increment();
      ASSERT_not_null(increment_expr);
      if (increment_expr != NULL) {
        SgUnparse_Info increment_info(info);
        increment_info.unset_unparsedPartiallyUsingTokenStream();
        unparseExpression(increment_expr, increment_info);
      }
      curprint(isSgBasicBlock(for_stmt->get_loop_body()) != NULL ? ")" : ") ");

      // DQ (9/24/2020): Adding support to unparse attached pragmas.
      unparsePragmaAttribute(for_stmt);
    } else {
      // DQ (12/15/2014): Note that the increment expression is not a Statement,
      // so it will be unparsed in the token stream of the body (assuming it is
      // unparsed via the token stream).  Note clear how to look ahead to check
      // this or make the unparsing of the increment expression conditional upon
      // this.
      // SgStatement *tmp_stmt = for_stmt->get_for_init_stmt();
      SgStatement *body = for_stmt->get_loop_body();

      // Not yet clear how to handle case where tmp_stmt == NULL.
      ASSERT_not_null(test_stmt);
      ASSERT_not_null(body);

      // If this is compiler generated this this must be handled similarly as to
      // the SgIfStmt with compiler generated body.
      ROSE_ASSERT(body->isCompilerGenerated() == false);

#if DEBUG_FOR_STMT
      curprint("/* unparse increment expression in SgForStatement header */");
#endif
      // DQ (12/16/2014): When a SgBasicBlock has been substituted for the
      // loop_body then there is not associated token stream (see
      // test2014_14.C). In this case it is better to use the start and end of
      // the trailing whitespace subsequence. unparseStatementFromTokenStream
      // (test_stmt, body, e_trailing_whitespace_start,
      // e_token_subsequence_start); unparseStatementFromTokenStream (test_stmt,
      // e_trailing_whitespace_start, e_trailing_whitespace_end, info);
      // curprint("/* syntax from partial token unparse */ )");
      // SgStatement* loopBody = for_stmt->get_loop_body();
      // unparseStatementFromTokenStream (test_stmt, loopBody,
      // e_trailing_whitespace_end, e_leading_whitespace_start);

      // DQ (6/6/2021): Note that this will unparse the original expression
      // using the token stream, if there was a transformation then it should be
      // unparsed from the AST (as an expression).
      // unparseStatementFromTokenStream (test_stmt, body,
      // e_trailing_whitespace_start, e_leading_whitespace_start, info);
      bool unparseOnlyWhitespace = false;
      int start_offset = 0;
      int end_offset = -1;
      unparseStatementFromTokenStream(
          test_stmt, body, e_trailing_whitespace_start,
          e_leading_whitespace_start, info, unparseOnlyWhitespace, start_offset,
          end_offset);
      // DQ (6/6/2021): This is no longer needed (and causes an error in the
      // generated code). curprint(")");
    }

    // Added support to output the header without the body to support the
    // addition of more context in the prefix used with the AST Rewrite
    // Mechanism. if ( (tmp_stmt = for_stmt->get_loop_body()) )

    SgStatement *loopBody = for_stmt->get_loop_body();
    ASSERT_not_null(loopBody);
    // printf ("loopBody = %p         = %s
    // \n",loopBody,loopBody->class_name().c_str()); printf
    // ("info.SkipBasicBlock() = %s \n",info.SkipBasicBlock() ? "true" :
    // "false");

    // if ( (tmp_stmt = for_stmt->get_loop_body()) && !info.SkipBasicBlock())
    if ((loopBody != NULL) && !info.SkipBasicBlock()) {
#if DEBUG_FOR_STMT
      printf("Unparse the for loop body \n");
      curprint("/* Unparse the for loop body */ ");
#endif
      // unparseStatement(tmp_stmt, info);

      SgUnparse_Info bodyInfo = nestedStatementInfo(info);
      unp->cur.format(loopBody, info, FORMAT_BEFORE_NESTED_STATEMENT);
      unparseStatement(loopBody, bodyInfo);
      unp->cur.format(loopBody, info, FORMAT_AFTER_NESTED_STATEMENT);
#if DEBUG_FOR_STMT
      curprint("/* DONE: Unparse the for loop body */ ");
#endif
    } else {
      // printf ("No for loop body to unparse! \n");
      // curprint ( string("\n/* No for loop body to unparse! */ \n";
      if (!info.SkipSemiColon()) {
        curprint(string(";"));
      }
    }
  }
}

void Unparse_ExprStmt::unparseRangeBasedForStmt(SgStatement *stmt,
                                                SgUnparse_Info &info) {
  // printf ("Unparse range-based for loop \n");
  SgRangeBasedForStatement *for_stmt = isSgRangeBasedForStatement(stmt);
  ASSERT_not_null(for_stmt);

#define DEBUG_RANGE_BASED_FOR_STMT 0

#if DEBUG_RANGE_BASED_FOR_STMT
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  printf("In unparseRangeBasedForStmt(): "
         "saved_unparsedPartiallyUsingTokenStream = %s \n",
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
  curprint("/* Top of unparseForStmt */");
#endif

  // printf ("ERROR: Range-based For statement unparseRangeBasedForStmt() not
  // implemented \n");

  curprint("for ( ");

  SgVariableDeclaration *interator_declaration =
      for_stmt->get_iterator_declaration();
  ASSERT_not_null(interator_declaration);

  SgUnparse_Info ninfo(info);

  // Need to suppress the output of the semicolon in the output of the variable
  // declaration.
  ninfo.set_SkipSemiColon();
  ninfo.set_SkipInitializer();

  unparseStatement(interator_declaration, ninfo);

  curprint(" : ");

  // SgVarRefExp* range_variable = for_stmt->range_variable_reference();
  // ASSERT_not_null(range_variable);
  SgExpression *range_expression = for_stmt->range_expression();
  ASSERT_not_null(range_expression);

  unparseExpression(range_expression, info);

  curprint(" )");

  SgStatement *loopBody = for_stmt->get_loop_body();
  ASSERT_not_null(loopBody);
  // printf ("loopBody = %p         = %s
  // \n",loopBody,loopBody->class_name().c_str()); printf
  // ("info.SkipBasicBlock() = %s \n",info.SkipBasicBlock() ? "true" : "false");

  if ((loopBody != NULL) && !info.SkipBasicBlock()) {
#if DEBUG_RANGE_BASED_FOR_STMT
    printf("Unparse the for loop body \n");
    curprint("/* Unparse the for loop body */ ");
#endif
    SgUnparse_Info bodyInfo = nestedStatementInfo(info);
    unp->cur.format(loopBody, info, FORMAT_BEFORE_NESTED_STATEMENT);
    unparseStatement(loopBody, bodyInfo);
    unp->cur.format(loopBody, info, FORMAT_AFTER_NESTED_STATEMENT);

#if DEBUG_RANGE_BASED_FOR_STMT
    curprint("/* DONE: Unparse the range-based for loop body */ ");
#endif
  } else {
    // printf ("No range-based for loop body to unparse! \n");
    // curprint ( string("\n/* No range-based for loop body to unparse! */ \n";
    if (!info.SkipSemiColon()) {
      curprint(string(";"));
    }
  }
}

void Unparse_ExprStmt::unparseExceptionSpecification(
    const SgTypePtrList &exceptionSpecifierList, SgUnparse_Info &info) {
  // DQ (6/27/2006): Added support for throw modifier and its exception
  // specification lists

  curprint(string(" throw("));
  if (!exceptionSpecifierList.empty()) {
    SgTypePtrList::const_iterator i = exceptionSpecifierList.begin();
    while (i != exceptionSpecifierList.end()) {
      // Handle class type as a special case to make sure the names are always
      // output (see test2004_91.C). unparseType(*i,info); printf ("Note: Type
      // found in function throw specifier type = %p = %s
      // \n",*i,i->class_name().c_str());

      ASSERT_not_null(*i);
      // DQ (6/2/2011): Added support for name qualification.
      info.set_reference_node_for_qualification(*i);
      ASSERT_not_null(info.get_reference_node_for_qualification());

      unp->u_type->unparseType(*i, info);

      // DQ (6/2/2011): Since we are not using a new SgUnparse_Info object,
      // clear the reference node for name qualification after it has been used.
      info.set_reference_node_for_qualification(NULL);
      i++;
      if (i != exceptionSpecifierList.end())
        curprint(string(","));
    }
  } else {
    // There was no exception specification list of types
  }
  curprint(string(")"));
}

void requireDeclarationSemanticScope(
    const SgDeclarationStatement *declarationStatement) {
  ASSERT_not_null(declarationStatement);
  SgScopeStatement *semantic_scope = declarationStatement->get_scope();
  if (semantic_scope == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[declaration-semantic-scope]: "
                    "declaration has no exact semantic scope\n");
    ROSE_ABORT();
  }
}

#define DEBUG_unparseFuncDeclStmt 0

void Unparse_ExprStmt::unparseCudaLaunchBounds(
    SgFunctionDeclaration *declaration, SgUnparse_Info &info) {
  ASSERT_not_null(declaration);
  SgExpression *expression = declaration->get_cuda_launch_bounds_expression();
  if (expression == nullptr) {
    return;
  }
  SgType *type = expression->get_type();
  if (expression->get_parent() != declaration || type == nullptr ||
      isSgTypeUnknown(type) != nullptr || isSgTypeDefault(type) != nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cuda-launch-bounds]: expression has no "
            "exact function owner and semantic type\n");
    ROSE_ABORT();
  }
  curprint("__launch_bounds__(");
  unparseExpression(expression, info);
  curprint(") ");
}

void Unparse_ExprStmt::unparseFuncDeclStmt(SgStatement *stmt,
                                           SgUnparse_Info &info) {
#if DEBUG_unparseFuncDeclStmt
  printf("Enter Unparse_ExprStmt::unparseFuncDeclStmt\n");
  printf("  stmt = %p = %s\n", stmt, stmt->class_name().c_str());
#endif
  SgFunctionDeclaration *funcdecl_stmt = isSgFunctionDeclaration(stmt);
  ASSERT_not_null(funcdecl_stmt);
  SgSourceFile *source_file = info.get_current_source_file();
  if (source_file == nullptr) {
    source_file = SageInterface::getEnclosingSourceFile(funcdecl_stmt, true);
  }
  if (source_file != nullptr) {
    requireExactCxxMacroDirectiveList(source_file);
  }

#if DEBUG_unparseFuncDeclStmt
  printf("  ->isFriend         = %s \n",
         funcdecl_stmt->get_declarationModifier().isFriend() ? "true"
                                                             : "false");
  printf("  ->isForward()      = %s \n",
         funcdecl_stmt->isForward() ? "true" : "false");
  printf("  ->get_definition() = %s \n",
         funcdecl_stmt->get_definition() ? "true" : "false");
  printf("info.SkipFunctionDefinition()   = %s \n",
         info.SkipFunctionDefinition() ? "true" : "false");
#endif

#if ENABLE_unparsedPartiallyUsingTokenStream
  // DQ (10/26/2018): We might not need this code now that I have fixed a cut
  // and paste error in the latest debugging of the
  // unparseStatementFromTokenStream() function. DQ (10/25/2018): Test for if we
  // have unparsed partially using the token stream. If so then we don't want to
  // unparse this syntax, if not then we require this syntax.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgUnparse_Info headerInfo(info);
  if (saved_unparsedPartiallyUsingTokenStream &&
      !functionCanKeepCompactPartialTokenHeader(funcdecl_stmt, info)) {
    saved_unparsedPartiallyUsingTokenStream = false;
    headerInfo.unset_unparsedPartiallyUsingTokenStream();
  }
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    SgFunctionDefinition *function_definition = funcdecl_stmt->get_definition();
    ASSERT_not_null(function_definition);
    SgStatement *function_body = function_definition->get_body();
    ASSERT_not_null(function_body);

    if (function_body != NULL) {
      unparseStatementFromTokenStream(stmt, function_body,
                                      e_token_subsequence_start,
                                      e_leading_whitespace_end, info);
      statementsWithTokenEmittedLeadingPreprocessing.insert(function_body);
      SgUnparse_Info bodyInfo(info);
      bodyInfo
          .unset_parentStatementListBeingUnparsedUsingPartialTokenSequence();
      unparseStatement(function_body, bodyInfo);
    } else {
      // We need to handle the case of a function prototype.
      printf("We need to handle the case of a function prototype \n");
      ROSE_ABORT();
    }

    return;
  }
#endif

  // DQ (1/19/2014): Adding support for attributes that must be prefixed to the
  // function declarations (e.g. "__attribute__((regnum(3)))"). It is output
  // here for non-defining declarations, but in unparseFuncDefnStmt() function
  // for the attribute to be associated with the defining declaration.
  // unp->u_sage->printPrefixAttributes(funcdecl_stmt,info);
  if (funcdecl_stmt->isForward() == true && !headerInfo.SkipBaseType()) {
    unp->u_sage->printPrefixAttributes(funcdecl_stmt, headerInfo);
  }

  SgUnparse_Info ninfo(headerInfo);

  requireDeclarationSemanticScope(funcdecl_stmt);
  const bool is_deduction_guide = funcdecl_stmt->get_is_deduction_guide();

  if ((funcdecl_stmt->isForward() == false) &&
      (funcdecl_stmt->get_definition() != NULL) &&
      (headerInfo.SkipFunctionDefinition() == false)) {
    unparseStatement(funcdecl_stmt->get_definition(), headerInfo);
  } else {
    SgClassDefinition *cdefn =
        isSgClassDefinition(cxxLexicalDeclarationParent(funcdecl_stmt));
    if (cdefn && cdefn->get_declaration()->get_class_type() ==
                     SgClassDeclaration::e_class) {
      ninfo.set_CheckAccess();
    }

    ninfo.set_SkipClassDefinition();
    ninfo.set_SkipEnumDefinition();

    SgStorageModifier &storage =
        funcdecl_stmt->get_declarationModifier().get_storageModifier();
    if (storage.isAsm() == true && !ninfo.SkipBaseType()) {
      curprint("__asm__ ");
    }

    if (!ninfo.SkipBaseType()) {
      unp->u_sage->printSpecifier(funcdecl_stmt, ninfo);
    }

    unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::inside);
    ninfo.unset_CheckAccess();
    info.set_access_attribute(ninfo.get_access_attribute());

    // get_orig_return_type queries return type from type_syntax(), if
    // available,
    //   and from type() otherwise.
    SgType *rtype = funcdecl_stmt->get_orig_return_type();
    bool use_trailing_return_type_syntax =
        !is_deduction_guide &&
        functionUsesTrailingReturnTypeSyntax(funcdecl_stmt);
    if (ninfo.SkipBaseType() &&
        (is_deduction_guide || use_trailing_return_type_syntax)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: function=%p "
              "uses a non-shareable trailing return declarator\n",
              static_cast<void *>(funcdecl_stmt));
      ROSE_ABORT();
    }
    const bool emit_return_declarator_type =
        !ninfo.SkipBaseType() || cxxSourceGroupTypeUsesDeclaratorSyntax(rtype);

    SgUnparse_Info ninfo_for_type(ninfo);
    enableInlineDefinitionForFunctionReturnTypeIfNeeded(ninfo_for_type, rtype);
    configureExactFunctionReturnTypeInfo(unp, funcdecl_stmt, ninfo_for_type);

    if (!is_deduction_guide) {
      if (use_trailing_return_type_syntax) {
        curprint("auto ");
      } else if (emit_return_declarator_type) {
        ninfo_for_type.set_isTypeFirstPart();
        unp->u_type->unparseType(rtype, ninfo_for_type);
      }
    }
    unparseCudaLaunchBounds(funcdecl_stmt, ninfo);

    if (funcdecl_stmt->isForward() == false) {
      unp->u_sage->printAttributes(funcdecl_stmt, info);
    }
    ninfo.set_declstatement_ptr(NULL);
    ninfo.set_declstatement_ptr(funcdecl_stmt);

    if (funcdecl_stmt->get_firstNondefiningDeclaration() != NULL) {
      SgFunctionDeclaration *firstNondefiningFunction = isSgFunctionDeclaration(
          funcdecl_stmt->get_firstNondefiningDeclaration());
      ASSERT_not_null(firstNondefiningFunction);
      ASSERT_not_null(
          firstNondefiningFunction->get_firstNondefiningDeclaration());
      ASSERT_not_null(
          SageInterface::getEnclosingSourceFile(funcdecl_stmt, true));
      ASSERT_not_null(SageInterface::getEnclosingSourceFile(
          firstNondefiningFunction, true));
    }

    unparse_helper(funcdecl_stmt, ninfo);

    ninfo.set_declstatement_ptr(NULL);
    if (is_deduction_guide || use_trailing_return_type_syntax) {
      curprint(" -> ");
      SgUnparse_Info trailing_type_info(ninfo_for_type);
      trailing_type_info.set_isTypeFirstPart();
      unp->u_type->unparseType(rtype, trailing_type_info);
      trailing_type_info.set_isTypeSecondPart();
      unp->u_type->unparseType(rtype, trailing_type_info);
    } else if (emit_return_declarator_type) {
      SgUnparse_Info second_type_info(ninfo);
      configureExactFunctionReturnTypeInfo(unp, funcdecl_stmt,
                                           second_type_info);
      second_type_info.set_isTypeSecondPart();
      unp->u_type->unparseType(rtype, second_type_info);
    }
    unparseOldStyleFunctionParameterDeclarations(funcdecl_stmt, ninfo);

    if (!funcdecl_stmt->get_source_declarator_uses_wrapped_function_type() &&
        funcdecl_stmt->get_declarationModifier().isThrow()) {
      const SgTypePtrList &exceptionSpecifierList =
          funcdecl_stmt->get_exceptionSpecification();
      info.set_reference_node_for_qualification(funcdecl_stmt);
      unparseExceptionSpecification(exceptionSpecifierList, info);
      info.set_reference_node_for_qualification(NULL);
    }

    if (funcdecl_stmt->get_asm_name().empty() == false) {
      curprint(" __asm__ (\"");
      curprint(funcdecl_stmt->get_asm_name());
      curprint(string("\")"));
    }

    unparseTrailingRequiresClauseIfPresent(this, funcdecl_stmt, info);

    const bool is_function_try_block =
        info.SkipFunctionDefinition() && !funcdecl_stmt->isForward() &&
        getFunctionTryStmt(funcdecl_stmt->get_definition()) != NULL;
    if (is_function_try_block) {
      curprint(" try");
    }

    if (funcdecl_stmt->isForward()) {
      unp->u_sage->printAttributes(funcdecl_stmt, info);
      unp->u_sage->printAttributesForType(funcdecl_stmt, info);

      if (!ninfo.SkipSemiColon()) {
        curprint(";");
      }
    }
  }

  if (info.AddSemiColonAfterDeclaration()) {
    curprint(";");
  }

#if DEBUG_unparseFuncDeclStmt
  printf("Leave Unparse_ExprStmt::unparseFuncDeclStmt\n");
#endif
}

void Unparse_ExprStmt::unparseTemplateFunctionDefnStmt(SgStatement *stmt_,
                                                       SgUnparse_Info &info) {
  SgTemplateFunctionDefinition *stmt = isSgTemplateFunctionDefinition(stmt_);
  assert(stmt != NULL);

  // DQ (10/27/2020): This can't be commented out since it is required for the
  // conditional below.
  // #ifndef NDEBUG
  // SgStatement *declstmt =
  // isSgTemplateFunctionDeclaration(stmt->get_declaration());
  // SgDeclarationStatement *declstmt =
  // isSgTemplateFunctionDeclaration(stmt->get_declaration());
  SgFunctionDeclaration *declstmt =
      isSgTemplateFunctionDeclaration(stmt->get_declaration());
  assert(declstmt != NULL);
  // #endif

  // unparseTemplateFunctionDeclStmt(declstmt, info); // we should not go back
  // to parent declaration and unparse it. bad logic and cause recursion.

  SgSourceFile *sourcefile =
      requireExactCxxSourceFile(unp, info, "template-function-definition");
  // DQ (10/27/2020): Added support to activate unparsing from the AST on a
  // declaration by declaration basis. if (sourcefile != NULL &&
  // sourcefile->get_unparse_template_ast() == true)
  if ((sourcefile != NULL && sourcefile->get_unparse_template_ast() == true) ||
      (declstmt->get_unparse_template_ast() == true)) {
    // Liao, 12/15/2016
    //  We should only unparse the definition, not going back to parent node to
    //  unparse the entire declaration including the header.
    SgFunctionDefinition *funcdefn_stmt = stmt;
    ASSERT_not_null(funcdefn_stmt);

#if OUTPUT_HIDDEN_LIST_DATA
    outputHiddenListData(funcdefn_stmt);
#endif

    // Unparse any comments of directives attached to the
    // SgFunctionParameterList
    ASSERT_not_null(funcdefn_stmt->get_declaration());
    if (funcdefn_stmt->get_declaration()->get_parameterList() != NULL) {
      unparseAttachedPreprocessingInfo(
          funcdefn_stmt->get_declaration()->get_parameterList(), info,
          PreprocessingInfo::before);
    }

    info.set_SkipFunctionDefinition();
    //     SgStatement *declstmt = funcdefn_stmt->get_declaration();

    // DQ (1/19/2014): Adding gnu attribute prefix support.
    ASSERT_not_null(funcdefn_stmt->get_declaration());

    unp->u_sage->printPrefixAttributes(funcdefn_stmt->get_declaration(), info);

    // DQ (3/24/2004): Need to permit SgMemberFunctionDecl and
    // SgTemplateInstantiationMemberFunctionDecl if (declstmt->variant() ==
    // MFUNC_DECL_STMT)

    // DQ (5/8/2004): Any generated specialization needed to use the
    // C++ syntax for explicit specification of specializations.
    // if (isSgTemplateInstantiationMemberFunctionDecl(declstmt) != NULL)
    //      curprint ( string("template<> ";

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // info.set_SkipQualifiedNames();

    // DQ (10/15/2006): Mark that we are unparsing a function declaration (or
    // member function declaration) this will help us know when to trim the "::"
    // prefix from the name qualiciation.  The "::" global scope qualifier is
    // not used in function declarations, but is used for function calls.
    info.set_declstatement_ptr(NULL);
    info.set_declstatement_ptr(funcdefn_stmt->get_declaration());

    // curprint ("/* Inside of
    // Unparse_ExprStmt::unparseTemplateFunctionDefnStmt: DONE calling
    // unparseMFuncDeclStmt or unparseFuncDeclStmt */ ");

    // DQ (10/15/2006): Also un-mark that we are unparsing a function
    // declaration (or member function declaration)
    info.set_declstatement_ptr(NULL);

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // info.unset_SkipQualifiedNames();

    info.unset_SkipFunctionDefinition();
    SgUnparse_Info ninfo(info);
    ninfo.unset_unparsedPartiallyUsingTokenStream();
    ninfo.unset_parentStatementListBeingUnparsedUsingPartialTokenSequence();

    // DQ (10/20/2012): Ouput the comments and CPP directives on the function
    // definition. Note must be outside of SkipFunctionDefinition to be output.
    unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                     PreprocessingInfo::before);

    // now the body of the function
    if (funcdefn_stmt->get_body()) {
      if (SgTryStmt *try_stmt = getFunctionTryStmt(funcdefn_stmt)) {
        unparseFunctionTryBlock(try_stmt, ninfo);
      } else {
        unparseStatement(funcdefn_stmt->get_body(), ninfo);
      }
    } else {
      curprint("{}");

      // DQ (9/22/2004): I think this is an error!
      printf("Error: Should be an error to not have a function body in the AST "
             "\n");
      ROSE_ABORT();
    }

    // DQ (10/20/2012): Not clear if this is in the correct location (shouldn't
    // it be BEFORE the function body?). Unparse any comments of directives
    // attached to the SgFunctionParameterList
    unparseAttachedPreprocessingInfo(
        funcdefn_stmt->get_declaration()->get_parameterList(), info,
        PreprocessingInfo::after);

    // DQ (10/20/2012): Ouput the comments and CPP directives on the function
    // definition.
    unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                     PreprocessingInfo::after);
  }
}

// NOTE: Bug in Sage: No file information provided for FuncDeclStmt.
void Unparse_ExprStmt::unparseFuncDefnStmt(SgStatement *stmt,
                                           SgUnparse_Info &info) {

  SgFunctionDefinition *funcdefn_stmt = isSgFunctionDefinition(stmt);
  ASSERT_not_null(funcdefn_stmt);

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(funcdefn_stmt);
#endif

  // Unparse any comments of directives attached to the SgFunctionParameterList
  ASSERT_not_null(funcdefn_stmt->get_declaration());
  if (funcdefn_stmt->get_declaration()->get_parameterList() != NULL) {
    unparseAttachedPreprocessingInfo(
        funcdefn_stmt->get_declaration()->get_parameterList(), info,
        PreprocessingInfo::before);
  }

  info.set_SkipFunctionDefinition();
  SgStatement *declstmt = funcdefn_stmt->get_declaration();
  // DQ (1/19/2014): Adding gnu attribute prefix support.
  ASSERT_not_null(funcdefn_stmt->get_declaration());

  unp->u_sage->printPrefixAttributes(funcdefn_stmt->get_declaration(), info);

  // DQ (3/24/2004): Need to permit SgMemberFunctionDecl and
  // SgTemplateInstantiationMemberFunctionDecl if (declstmt->variant() ==
  // MFUNC_DECL_STMT)

  // DQ (5/8/2004): Any generated specialization needed to use the
  // C++ syntax for explicit specification of specializations.
  // if (isSgTemplateInstantiationMemberFunctionDecl(declstmt) != NULL)
  //      curprint ( string("template<> ";

  // DQ (10/11/2006): As part of new implementation of qualified names we now
  // default to the generation of all qualified names unless they are skipped.
  // info.set_SkipQualifiedNames();

  // DQ (10/15/2006): Mark that we are unparsing a function declaration (or
  // member function declaration) this will help us know when to trim the "::"
  // prefix from the name qualiciation.  The "::" global scope qualifier is not
  // used in function declarations, but is used for function calls.
  info.set_declstatement_ptr(NULL);
  info.set_declstatement_ptr(funcdefn_stmt->get_declaration());

  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgUnparse_Info headerInfo(info);
  headerInfo.set_template_argument_qualification_context(
      funcdefn_stmt->get_declaration());
  if (saved_unparsedPartiallyUsingTokenStream &&
      !functionCanKeepCompactPartialTokenHeader(
          isSgFunctionDeclaration(declstmt), info)) {
    saved_unparsedPartiallyUsingTokenStream = false;
    headerInfo.unset_unparsedPartiallyUsingTokenStream();
  }
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    if (isSgMemberFunctionDeclaration(declstmt)) {
      unparseMFuncDeclStmt(declstmt, headerInfo);
    } else {
      unparseFuncDeclStmt(declstmt, headerInfo);
    }
  } else {
    // DQ (12/6/2014): Unparse the equivalent tokens instead.
    SgStatement *function_body = funcdefn_stmt->get_body();
    ASSERT_not_null(function_body);

    unparseStatementFromTokenStream(declstmt, function_body,
                                    e_token_subsequence_start,
                                    e_leading_whitespace_end, info);
    statementsWithTokenEmittedLeadingPreprocessing.insert(function_body);
  }

  // curprint ("/* Inside of Unparse_ExprStmt::unparseFuncDefnStmt: DONE calling
  // unparseMFuncDeclStmt or unparseFuncDeclStmt */ ");

  // DQ (10/15/2006): Also un-mark that we are unparsing a function declaration
  // (or member function declaration)
  info.set_declstatement_ptr(NULL);

  // DQ (10/11/2006): As part of new implementation of qualified names we now
  // default to the generation of all qualified names unless they are skipped.
  // info.unset_SkipQualifiedNames();

  info.unset_SkipFunctionDefinition();
  SgUnparse_Info ninfo(info);
  if (!saved_unparsedPartiallyUsingTokenStream) {
    ninfo.unset_unparsedPartiallyUsingTokenStream();
  }
  ninfo.unset_parentStatementListBeingUnparsedUsingPartialTokenSequence();

  // DQ (10/20/2012): Ouput the comments and CPP directives on the function
  // definition. Note must be outside of SkipFunctionDefinition to be output.
  unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                   PreprocessingInfo::before);

  // now the body of the function
  if (funcdefn_stmt->get_body()) {
    if (SgTryStmt *try_stmt = getFunctionTryStmt(funcdefn_stmt)) {
      unparseFunctionTryBlock(try_stmt, ninfo);
    } else {
      unparseStatement(funcdefn_stmt->get_body(), ninfo);
    }
  } else {
    curprint("{}");

    // DQ (9/22/2004): I think this is an error!
    printf(
        "Error: Should be an error to not have a function body in the AST \n");
    ROSE_ABORT();
  }

  // DQ (10/20/2012): Not clear if this is in the correct location (shouldn't it
  // be BEFORE the function body?). Unparse any comments of directives attached
  // to the SgFunctionParameterList
  unparseAttachedPreprocessingInfo(
      funcdefn_stmt->get_declaration()->get_parameterList(), info,
      PreprocessingInfo::after);

  // DQ (10/20/2012): Ouput the comments and CPP directives on the function
  // definition.
  unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                   PreprocessingInfo::after);
}

void Unparse_ExprStmt::unparseReturnType(SgFunctionDeclaration *funcdecl_stmt,
                                         SgType *&rtype,
                                         SgUnparse_Info &ninfo) {
  // DQ (9/7/2014): Refactored this code so we could call it from the template
  // member and non-member function declaration unparse function. Note that we
  // pass a reference to the return type so that we can call unparseType a
  // second time to unparse the second part (not yet refactored, since it is
  // much simpler).

  SgClassDefinition *parent_class =
      isSgClassDefinition(cxxLexicalDeclarationParent(funcdecl_stmt));

  // This is a test for if the member function is structurally in the class
  // where it is defined. printf ("parent_class = %p mfuncdecl_stmt->get_scope()
  // = %p \n",parent_class,mfuncdecl_stmt->get_scope());

  // DQ (11/5/2007): This test is not good enough (does not handle case of
  // nested classes and the definition of member function outside of the nested
  // class and inside of another class. if (parent_class)
  if (parent_class == funcdecl_stmt->get_scope()) {
    // JJW 10-23-2007 This member function is declared inside the
    // class, so its name should never be qualified

    // printf ("mfuncdecl_stmt->get_declarationModifier().isFriend() = %s
    // \n",mfuncdecl_stmt->get_declarationModifier().isFriend() ? "true" :
    // "false");
    if (funcdecl_stmt->get_declarationModifier().isFriend() == false) {
      // printf ("Setting SkipQualifiedNames (this is a member function located
      // in its own class) \n");
      ninfo.set_SkipQualifiedNames();
    }
  }

  ninfo.set_SkipClassDefinition();
  ninfo.set_SkipEnumDefinition();

  // DQ (6/10/2007): set the declaration pointer so that the name qualification
  // can see if this is the declaration (so that exceptions to qualification can
  // be tracked).
  ninfo.set_declstatement_ptr(NULL);
  ninfo.set_declstatement_ptr(funcdecl_stmt);

  // if (!(mfuncdecl_stmt->isConstructor() || mfuncdecl_stmt->isDestructor() ||
  // mfuncdecl_stmt->isConversion()))
  if (!(funcdecl_stmt->get_specialFunctionModifier().isConstructor() ||
        funcdecl_stmt->get_specialFunctionModifier().isDestructor() ||
        funcdecl_stmt->get_specialFunctionModifier().isConversion())) {
    rtype = funcdecl_stmt->get_orig_return_type();
    ASSERT_not_null(rtype);
    bool use_trailing_return_type_syntax =
        functionUsesTrailingReturnTypeSyntax(funcdecl_stmt);
    if (ninfo.SkipBaseType() && use_trailing_return_type_syntax) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: function=%p "
              "uses a non-shareable trailing return declarator\n",
              static_cast<void *>(funcdecl_stmt));
      ROSE_ABORT();
    }
    if (use_trailing_return_type_syntax) {
      curprint("auto ");
    } else if (!ninfo.SkipBaseType() ||
               cxxSourceGroupTypeUsesDeclaratorSyntax(rtype)) {
      ninfo.set_isTypeFirstPart();
      ninfo.set_SkipClassSpecifier();

      SgUnparse_Info ninfo_for_type(ninfo);
      ninfo_for_type.unset_SkipQualifiedNames();
      enableInlineDefinitionForFunctionReturnTypeIfNeeded(ninfo_for_type,
                                                          rtype);
      configureExactFunctionReturnTypeInfo(unp, funcdecl_stmt, ninfo_for_type);

      // unp->u_type->unparseType(rtype, ninfo);
      unp->u_type->unparseType(rtype, ninfo_for_type);

      ninfo.unset_SkipClassSpecifier();
    }

    // printf ("In unparser: DONE with NOT a constructor, destructor or
    // conversion operator \n");
  } else {
    // DQ (9/17/2004): What can we assume about the return type of a
    // constructor, destructor, or conversion operator?
    if (funcdecl_stmt->get_orig_return_type() == NULL) {
      printf("funcdecl_stmt->get_orig_return_type() == NULL funcdecl_stmt = %p "
             "= %s = %s \n",
             funcdecl_stmt, funcdecl_stmt->class_name().c_str(),
             funcdecl_stmt->get_name().str());
    }

    ASSERT_not_null(funcdecl_stmt->get_orig_return_type());
    ASSERT_not_null(funcdecl_stmt->get_type()->get_return_type());
  }
}

#define DEBUG_unparseMFuncDeclStmt 0

void Unparse_ExprStmt::unparseMFuncDeclStmt(SgStatement *stmt,
                                            SgUnparse_Info &info) {
  SgMemberFunctionDeclaration *mfuncdecl_stmt =
      isSgMemberFunctionDeclaration(stmt);
  ASSERT_not_null(mfuncdecl_stmt);

#if DEBUG_unparseMFuncDeclStmt
  printf("Enter Unparse_ExprStmt::unparseMFuncDeclStmt\n");
  printf("  stmt = %p = %s\n", stmt, stmt->class_name().c_str());
#endif

#if ENABLE_unparsedPartiallyUsingTokenStream
  // DQ (10/26/2018): We might not need this code now that I have fixed a cut
  // and paste error in the latest debugging of the
  // unparseStatementFromTokenStream() function. DQ (10/25/2018): Test for if we
  // have unparsed partially using the token stream. If so then we don't want to
  // unparse this syntax, if not then we require this syntax.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgUnparse_Info headerInfo(info);
  if (saved_unparsedPartiallyUsingTokenStream &&
      !functionCanKeepCompactPartialTokenHeader(mfuncdecl_stmt, info)) {
    saved_unparsedPartiallyUsingTokenStream = false;
    headerInfo.unset_unparsedPartiallyUsingTokenStream();
  }
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    SgFunctionDefinition *function_definition =
        mfuncdecl_stmt->get_definition();
    ASSERT_not_null(function_definition);
    SgStatement *function_body = function_definition->get_body();
    ASSERT_not_null(function_body);

    if (function_body != NULL) {
      // Unparse the tokens from the start of the member declaration through
      // any whitespace that precedes the body-opening brace. The body itself
      // is emitted structurally by the SgBasicBlock unparser so transformed
      // statements inside the body retain their AST changes.
      unparseStatementFromTokenStream(stmt, function_body,
                                      e_token_subsequence_start,
                                      e_leading_whitespace_end, info);
      statementsWithTokenEmittedLeadingPreprocessing.insert(function_body);

      SgUnparse_Info bodyInfo(info);
      bodyInfo.unset_unparsedPartiallyUsingTokenStream();
      bodyInfo
          .unset_parentStatementListBeingUnparsedUsingPartialTokenSequence();
      unparseStatement(function_body, bodyInfo);
    } else {
      printf("We need to handle the case of a function prototype \n");
      ROSE_ABORT();
    }
    return;
  }
#endif

  // DQ (12/3/2007): This causes a bug in the output of access level (public,
  // protected, private) because the inforamtion change in ninfo is not
  // propogated to info. DQ (11/3/2007): Moved construction of ninfo to start of
  // function!
  SgUnparse_Info ninfo(headerInfo);

  requireDeclarationSemanticScope(mfuncdecl_stmt);

  auto unparse_enclosing_template_headers = [&]() {
    unparseSourceSpelledTemplateHeaders(
        mfuncdecl_stmt->get_sourceSpelledTemplateHeaders(), mfuncdecl_stmt,
        info, "member function");
  };

  // Unparse any comments of directives attached to the SgCtorInitializerList
  if (mfuncdecl_stmt->get_CtorInitializerList() != NULL) {
    unparseAttachedPreprocessingInfo(mfuncdecl_stmt->get_CtorInitializerList(),
                                     ninfo, PreprocessingInfo::before);
  }

  auto const &mfuncdecl_mod = mfuncdecl_stmt->get_functionModifier();
  bool isDefaultedOrDeletedMemberFunction =
      mfuncdecl_mod.isMarkedDefault() || mfuncdecl_mod.isMarkedDelete();
  SgFunctionDefinition *mfuncdefn = mfuncdecl_stmt->get_definition();
#if DEBUG_unparseMFuncDeclStmt
  printf("  mfuncdecl_stmt->isForward()        = %s\n",
         mfuncdecl_stmt->isForward() ? "true" : "false");
  printf("  info.SkipFunctionDefinition()      = %s\n",
         info.SkipFunctionDefinition() ? "true" : "false");
  printf("  isDefaultedOrDeletedMemberFunction = %s\n",
         isDefaultedOrDeletedMemberFunction ? "true" : "false");
  printf("  mfuncdecl_stmt->get_definition()   = %p = %s\n", mfuncdefn,
         mfuncdefn ? mfuncdefn->class_name().c_str() : "");
#endif

  // DQ (4/13/2019): If this is a defaulted constructor, then we don't want to
  // unparse the body, so we want to treat it the same as a forward declaration.
  if (!mfuncdecl_stmt->isForward() && mfuncdefn &&
      !ninfo.SkipFunctionDefinition() &&
      isDefaultedOrDeletedMemberFunction == false) {
    // Class-member access control applies to the declaration itself, not to
    // local declarations nested inside the function body.
    SgUnparse_Info body_info(info);
    body_info.unset_CheckAccess();
    unparseStatement(mfuncdecl_stmt->get_definition(), body_info);
  } else {
    ASSERT_not_null(cxxLexicalDeclarationParent(mfuncdecl_stmt));
    // Access-specifier emission must be recomputed for each declaration.
    // Otherwise a prior in-class declaration can leak `CheckAccess` into
    // out-of-class member declarations and print invalid `public:`/`private:`.
    info.unset_CheckAccess();
    SgClassDefinition *parent_class =
        isSgClassDefinition(cxxLexicalDeclarationParent(mfuncdecl_stmt));
    if (parent_class &&
        parent_class->get_declaration()->get_class_type() ==
            SgClassDeclaration::e_class &&
        !info.skipCheckAccess()) {
      info.set_CheckAccess();
    }

    if (!ninfo.SkipBaseType()) {
      unparse_enclosing_template_headers();
      unp->u_sage->printSpecifier1(mfuncdecl_stmt, info);
      unp->u_sage->printSpecifier2(mfuncdecl_stmt, info);
    }
    info.unset_CheckAccess();

    SgType *rtype = NULL;
    unparseReturnType(mfuncdecl_stmt, rtype, ninfo);
    ASSERT_not_null(mfuncdecl_stmt);

    const NameQualificationResult memberNameQualification =
        exactStatementNameQualification(unp, mfuncdecl_stmt, info);
    ninfo.set_name_qualification_length(memberNameQualification.length);
    ninfo.set_global_qualification_required(memberNameQualification.global);

    SgName nameQualifier(memberNameQualification.qualifier);
    curprint(nameQualifier.str());

    // DQ (4/2/2018): Adding support for alternative and more
    // sophisticated handling of the function name (e.g. with template
    // arguments correctly qualified, etc.).
    if (isSgTemplateInstantiationMemberFunctionDecl(mfuncdecl_stmt) != NULL) {
      unp->u_exprStmt->unparseTemplateMemberFunctionName(
          isSgTemplateInstantiationMemberFunctionDecl(mfuncdecl_stmt), ninfo);
    } else {
      curprint(mfuncdecl_stmt->get_name().str());
    }

    SgUnparse_Info ninfo2(info);
    // A source declaration group may suppress this member's shared return
    // specifier, but its parameter types remain independent typed syntax.
    ninfo2.unset_SkipBaseType();
    ninfo2.set_SkipClassDefinition();
    ninfo2.set_SkipEnumDefinition();
    ninfo2.set_inArgList();
    ninfo2.set_declstatement_ptr(NULL);
    ninfo2.set_declstatement_ptr(mfuncdecl_stmt);

    const bool uses_wrapped_function_type =
        mfuncdecl_stmt->get_source_declarator_uses_wrapped_function_type();
    if (uses_wrapped_function_type) {
      mfuncdecl_stmt->validate_source_declarator_form();
    } else {
      curprint(string("("));
      unparseFunctionArgs(mfuncdecl_stmt, ninfo2);
      curprint(string(")"));
    }

    const bool use_trailing_return_type_syntax =
        rtype != NULL && functionUsesTrailingReturnTypeSyntax(mfuncdecl_stmt);
    if (ninfo.SkipBaseType() && use_trailing_return_type_syntax) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-declaration-group]: member "
              "function=%p uses a non-shareable trailing return declarator\n",
              static_cast<void *>(mfuncdecl_stmt));
      ROSE_ABORT();
    }
    if (use_trailing_return_type_syntax) {
      unparseMemberFunctionParametersAndQualifiers(mfuncdecl_stmt, info);
    }

    const bool emit_return_declarator_type =
        rtype != NULL && (!ninfo.SkipBaseType() ||
                          cxxSourceGroupTypeUsesDeclaratorSyntax(rtype));
    if (emit_return_declarator_type) {
      if (use_trailing_return_type_syntax) {
        curprint(" -> ");
        SgUnparse_Info trailing_type_info(ninfo);
        configureExactFunctionReturnTypeInfo(unp, mfuncdecl_stmt,
                                             trailing_type_info);
        trailing_type_info.set_isTypeFirstPart();
        unp->u_type->unparseType(rtype, trailing_type_info);
        trailing_type_info.set_isTypeSecondPart();
        unp->u_type->unparseType(rtype, trailing_type_info);
      } else {
        SgUnparse_Info ninfo3(ninfo);
        configureExactFunctionReturnTypeInfo(unp, mfuncdecl_stmt, ninfo3);
        ninfo3.set_isTypeSecondPart();
        unp->u_type->unparseType(rtype, ninfo3);
      }
    }

    if (use_trailing_return_type_syntax) {
      unparseMemberFunctionPostDeclaratorModifiers(mfuncdecl_stmt, info);
    } else if (uses_wrapped_function_type) {
      unparseMemberFunctionPostDeclaratorModifiers(mfuncdecl_stmt, info);
    } else {
      unparseTrailingFunctionModifiers(mfuncdecl_stmt, info);
    }

    const bool is_function_try_block =
        info.SkipFunctionDefinition() && !mfuncdecl_stmt->isForward() &&
        getFunctionTryStmt(mfuncdecl_stmt->get_definition()) != NULL;
    if (is_function_try_block) {
      curprint(" try");
    }

    auto const &ctor_inits = mfuncdecl_stmt->get_ctors();
    SgMemberFunctionDeclaration *defining_declaration =
        isSgMemberFunctionDeclaration(
            mfuncdecl_stmt->get_definingDeclaration());
    if (!mfuncdecl_stmt->isForward() && ctor_inits.empty() &&
        defining_declaration != nullptr &&
        defining_declaration != mfuncdecl_stmt &&
        !defining_declaration->get_ctors().empty()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[constructor-initializer-owner]: "
              "emitted declaration has no exact initializer list but its "
              "distinct defining declaration has %zu entries\n",
              defining_declaration->get_ctors().size());
      ROSE_ABORT();
    }
    if ((mfuncdecl_stmt->isForward() && !info.SkipSemiColon()) ||
        isDefaultedOrDeletedMemberFunction) {
      curprint(";");

    } else if (!ctor_inits.empty()) {
      auto it_ctor_init = ctor_inits.begin();
#if DEBUG_unparseMFuncDeclStmt
      printf("  Preinitialization list:\n");
#endif
      curprint(" : ");
      bool separatorBelongsToCurrentInitializer = false;
      while (it_ctor_init != ctor_inits.end()) {
        SgInitializedName *ctor_init = *it_ctor_init;
        ASSERT_not_null(ctor_init);
        SgCtorInitializerList *ctor_initializer_list =
            isSgCtorInitializerList(ctor_init->get_parent());
        if (ctor_initializer_list == NULL) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[constructor-initializer-owner]: "
                  "initialized-name=%p parent=%p parent-type=%s\n",
                  static_cast<void *>(ctor_init),
                  static_cast<void *>(ctor_init->get_parent()),
                  ctor_init->get_parent() != NULL
                      ? ctor_init->get_parent()->class_name().c_str()
                      : "<null>");
          ROSE_ABORT();
        }
        SgUnparse_Info initializer_info(ninfo2);
        initializer_info.set_template_argument_qualification_context(
            ctor_initializer_list);
        it_ctor_init++;

        unparseAttachedPreprocessingInfo(ctor_init, info,
                                         PreprocessingInfo::before);
        if (separatorBelongsToCurrentInitializer) {
          curprint(", ");
          separatorBelongsToCurrentInitializer = false;
        }

        SgName nameQualifier(
            exactStatementNameQualification(unp, ctor_init, initializer_info)
                .qualifier);
#if DEBUG_unparseMFuncDeclStmt
        printf("   - element name = %s nameQualifier = %s \n",
               ctor_init->get_name().str(),
               nameQualifier.is_null() ? "NULL" : nameQualifier.str());
#endif
        unparseCtorPreinitializerDesignator(this, unp, ctor_init,
                                            initializer_info);

        SgExpression *initializer = ctor_init->get_initializer();
        if (initializer == NULL) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[constructor-preinitializer]: "
                  "initialized-name=%p has no initializer expression\n",
                  static_cast<void *>(ctor_init));
          ROSE_ABORT();
        }
        // not used: SgAggregateInitializer   * aggr_init =
        // isSgAggregateInitializer(initializer);
        SgConstructorInitializer *ctor_initializer =
            isSgConstructorInitializer(initializer);
        bool output_parenthesis = ctor_initializer == nullptr;

#if DEBUG_unparseMFuncDeclStmt
        bool compiler_generated =
            initializer->get_startOfConstruct()->isCompilerGenerated();
        printf("     initializer = %p = %s\n", initializer,
               initializer->class_name().c_str());
        printf("     output_parenthesis = %s\n",
               output_parenthesis ? "true" : "false");
        printf("     compiler_generated = %s\n",
               compiler_generated ? "true" : "false");
#endif
        if (output_parenthesis)
          curprint(string("("));

        initializer_info.set_reference_node_for_qualification(initializer);
        unparseExpression(initializer, initializer_info);

        if (output_parenthesis)
          curprint(string(")"));

        if (it_ctor_init != ctor_inits.end()) {
          SgInitializedName *next_initializer = *it_ctor_init;
          ASSERT_not_null(next_initializer);
          if (next_initializer->get_parent() != ctor_initializer_list) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[constructor-initializer-owner]: "
                    "following initializer=%p parent=%p/%s does not belong "
                    "to list=%p\n",
                    static_cast<void *>(next_initializer),
                    static_cast<void *>(next_initializer->get_parent()),
                    next_initializer->get_parent() != nullptr
                        ? next_initializer->get_parent()->class_name().c_str()
                        : "<null>",
                    static_cast<void *>(ctor_initializer_list));
            ROSE_ABORT();
          }
          separatorBelongsToCurrentInitializer =
              locatedNodeHasConditionalRegionOpening(next_initializer,
                                                     PreprocessingInfo::before);
          if (!separatorBelongsToCurrentInitializer) {
            curprint(", ");
          }
        }
      }
      ROSE_ASSERT(separatorBelongsToCurrentInitializer == false);
    }
  }

  // DQ (1/23/03) Added option to support rewrite mechanism (generation of
  // declarations)
  if (info.AddSemiColonAfterDeclaration()) {
    curprint(string(";"));
  }

  // Unparse any comments of directives attached to the SgCtorInitializerList
  if (mfuncdecl_stmt->get_CtorInitializerList() != NULL) {
    unparseAttachedPreprocessingInfo(mfuncdecl_stmt->get_CtorInitializerList(),
                                     info, PreprocessingInfo::after);
  }

#if DEBUG_unparseMFuncDeclStmt
  printf("Leaving Unparse_ExprStmt::unparseMFuncDeclStmt(stmt = %p = %s) \n",
         stmt, stmt->class_name().c_str());
#endif
}

void Unparse_ExprStmt::unparseMemberFunctionParametersAndQualifiers(
    SgMemberFunctionDeclaration *mfuncdecl_stmt, SgUnparse_Info &info) {
  ASSERT_not_null(mfuncdecl_stmt);
  SgMemberFunctionType *mftype =
      isSgMemberFunctionType(mfuncdecl_stmt->get_type());
  if (mftype == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[member-function-restrict-role]: "
                    "member function has no exact member-function type\n");
    ROSE_ABORT();
  }
  const bool type_restrict = mftype->isRestrictFunc();
  const bool declaration_restrict =
      mfuncdecl_stmt->get_declarationModifier().get_typeModifier().isRestrict();
  if (type_restrict != declaration_restrict) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[member-function-restrict-role]: "
            "type-restrict=%s declaration-restrict=%s disagree\n",
            type_restrict ? "true" : "false",
            declaration_restrict ? "true" : "false");
    ROSE_ABORT();
  }

  // DQ (9/9/2014): Note this was using info where it was refactored from and
  // ninfo is passed to this function.
  if (!info.SkipFunctionQualifier()) {
    if (mftype->isConstFunc()) {
      curprint(" const");
    }
    if (mftype->isVolatileFunc()) {
      curprint(" volatile");
    }

    if (declaration_restrict) {
      curprint(Unparse_Type::unparseRestrictKeyword());
    }

    // DQ (1/11/2020): Adding support for lvalue reference member function
    // modifiers.
    if (mftype->isLvalueReferenceFunc()) {
      curprint(" &");
    }

    // DQ (1/11/2020): Adding support for rvalue reference member function
    // modifiers.
    if (mftype->isRvalueReferenceFunc()) {
      curprint(" &&");
    }
  }

  // DQ (4/28/2004): Added support for throw modifier
  if (mfuncdecl_stmt->get_declarationModifier().isThrow()) {
    // Unparse SgThrow
    // unparseThrowExp(mfuncdecl_stmt->get_throwExpression,info);
    // printf ("Incomplete implementation of throw specifier on function \n");
    // curprint ( string(" throw( /* from unparseTrailingFunctionModifiers()
    // type list output not implemented */ )";
    const SgTypePtrList &exceptionSpecifierList =
        mfuncdecl_stmt->get_exceptionSpecification();
    // unparseExceptionSpecification(exceptionSpecifierList,ninfo);
    unparseExceptionSpecification(exceptionSpecifierList, info);
  }
}

void Unparse_ExprStmt::unparseMemberFunctionPostDeclaratorModifiers(
    SgMemberFunctionDeclaration *mfuncdecl_stmt, SgUnparse_Info &info) {
  if (SgExpression *requires_clause =
          mfuncdecl_stmt->get_trailingRequiresClause()) {
    SgUnparse_Info rinfo(info);
    rinfo.set_SkipClassDefinition();
    rinfo.set_SkipEnumDefinition();
    curprint(" requires ");
    unparseRequiresClauseExpression(this, requires_clause, rinfo);
  }

  // if (mfuncdecl_stmt->isPure())
  if (mfuncdecl_stmt->get_functionModifier().isPureVirtual()) {
    // DQ (1/22/2013): Supress the output of the pure virtual syntax if this is
    // the defining declaration (see test2013_26.C). curprint ( string(" = 0"));
    if (mfuncdecl_stmt != mfuncdecl_stmt->get_definingDeclaration()) {
      curprint(" = 0");
    }
  }

  // DQ (7/9/2022): This can only be output for member function declarations
  // defined in the class. DQ (8/11/2014): Added support for final keyword
  // unparsing.
  if (mfuncdecl_stmt->get_declarationModifier().isFinal() == true) {
    // DQ (2/12/2019): Testing, final can't be used on prototypes (I think).
    // curprint(" /* output from test 1 */ ");
    // curprint(" final");
    // DQ (7/10/2022): "final" is not a keyword, but it can only be used like a
    // keyword with member function declarations inside of the associated class
    // definition. curprint(" override");
    SgClassDefinition *parentClassDefinition =
        isSgClassDefinition(cxxLexicalDeclarationParent(mfuncdecl_stmt));
    if (parentClassDefinition != NULL &&
        mfuncdecl_stmt->get_scope() == parentClassDefinition) {
      curprint(" final");
    }
  }

  // DQ (7/9/2022): This can only be output for member function declarations
  // defined in the class. DQ (8/11/2014): Added support for final keyword
  // unparsing.
  if (mfuncdecl_stmt->get_declarationModifier().isOverride() == true) {
    // DQ (7/10/2022): "override" is not a keyword, but it can only be used like
    // a keyword with member function declarations inside of the associated
    // class definition. curprint(" override");
    SgClassDefinition *parentClassDefinition =
        isSgClassDefinition(cxxLexicalDeclarationParent(mfuncdecl_stmt));
    if (parentClassDefinition != NULL &&
        mfuncdecl_stmt->get_scope() == parentClassDefinition) {
      curprint(" override");
    }
  }

  // DQ (4/13/2019): Added support for default keyword unparsing.
  if (mfuncdecl_stmt->get_functionModifier().isMarkedDefault() == true) {
    curprint(" = default");
  }

  // DQ (4/13/2019): Added support for delete keyword unparsing.
  if (mfuncdecl_stmt->get_functionModifier().isMarkedDelete() == true) {
    curprint(" = delete");
  }
}

void Unparse_ExprStmt::unparseTrailingFunctionModifiers(
    SgMemberFunctionDeclaration *mfuncdecl_stmt, SgUnparse_Info &info) {
  // DQ (9/9/2014): Refactored support for function modifiers.
  unparseMemberFunctionParametersAndQualifiers(mfuncdecl_stmt, info);
  unparseMemberFunctionPostDeclaratorModifiers(mfuncdecl_stmt, info);
}

void Unparse_ExprStmt::unparseVarDefnStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgVariableDefinition *vardefn_stmt = isSgVariableDefinition(stmt);
  ASSERT_not_null(vardefn_stmt);

  // DQ: (9/17/2003)
  // Although I have not seen it in any of our tests of ROSE the
  // SgVariableDefinition does appear to be used in the declaration of bit
  // fields!  Note the comment at the end of the unparseVarDeclStmt() function
  // where the bit field is unparsed! Though it appears that the
  // unparseVarDefnStmt is not required to the unparsing of the bit field, so
  // this function is never called!

  // DQ (2/3/2007): However, for the ODR check in the AST merge we require
  // something to be generated for everything that could be shared.  So we
  // should unparse something, perhaps the variable declaration?

  // DQ (1/20/2014): This has been changed to be a SgValueExp (required).  Plus
  // as a generated value expression we include the expression from which the
  // value was generated.  This is important where this is a constant expression
  // generated from sizes of machine dependent types. SgUnsignedLongVal
  // *bitfield = vardefn_stmt->get_bitfield();
  SgExpression *bitfield = vardefn_stmt->get_bitfield();
  if (bitfield != NULL) {
    curprint(string(":"));
    unparseExpression(bitfield, info);
  }
}

void Unparse_ExprStmt::initializeDeclarationsFromParent(
    SgDeclarationStatement *declarationStatement, SgClassDefinition *&cdefn,
    SgNamespaceDefinitionStatement *&namespaceDefn, int /*debugSupport*/) {
  // DQ (11/18/2004): Now that we store the scope explicitly we don't have to
  // interprete the parent pointer!
  ASSERT_not_null(declarationStatement);
  SgScopeStatement *parentScope = declarationStatement->get_scope();
  ASSERT_not_null(parentScope);

  cdefn = isSgClassDefinition(parentScope);
  namespaceDefn = isSgNamespaceDefinitionStatement(parentScope);
}

void Unparse_ExprStmt::unparseClassDeclStmt(SgStatement *stmt,
                                            SgUnparse_Info &info) {
  SgClassDeclaration *classdecl_stmt = isSgClassDeclaration(stmt);
  ASSERT_not_null(classdecl_stmt);
  const bool forward_syntax_owns_inside_preprocessing =
      locatedNodeHasInsidePreprocessingInfo(classdecl_stmt);
  if (forward_syntax_owns_inside_preprocessing &&
      (!classdecl_stmt->isForward() ||
       classdecl_stmt->get_definition() != nullptr)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[preprocessing-owner]: class "
            "declaration=%p owns inside syntax but is not an exact forward "
            "declaration boundary\n",
            static_cast<void *>(classdecl_stmt));
    ROSE_ABORT();
  }

#define DEBUG_UNPARSE_CLASS_DECLARATION 0

#if DEBUG_USING_CURPRINT
  curprint("/* Inside of Unparse_ExprStmt::unparseClassDeclStmt() */ \n");
#endif

  // info.display("Inside of unparseClassDeclStmt");

#if DEBUG_UNPARSE_CLASS_DECLARATION
  printf("At top of unparseClassDeclStmt name = %s \n",
         classdecl_stmt->get_name().str());
#endif
#if DEBUG_UNPARSE_CLASS_DECLARATION
  printf("In Unparse_ExprStmt::unparseClassDeclStmt(): classdecl_stmt = %p "
         "isForward() = %s info.SkipClassDefinition() = %s name = %s \n",
         classdecl_stmt,
         (classdecl_stmt->isForward() == true) ? "true" : "false",
         (info.SkipClassDefinition() == true) ? "true" : "false",
         classdecl_stmt->get_name().str());
#endif
  requireDirectSourceSurfaceOwnership(classdecl_stmt, "class-declaration");

  // DQ (6/2/2021): Adding support for partial token sequence unparsing.
  SgClassDefinition *class_definition = classdecl_stmt->get_definition();
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream &&
      (classdecl_stmt->isTransformation() ||
       (class_definition != NULL && class_definition->isTransformation()))) {
    // Partial token-sequence mode can be inherited from an enclosing frontier
    // statement. Reused/transformed class declarations must unparse from the
    // AST instead; otherwise the nested class-declaration path consumes the
    // token-fragment mode directly and can degrade a defining declaration back
    // into a forward declaration.
    saved_unparsedPartiallyUsingTokenStream = false;
  }
  SgSourceFile *current_source_file = info.get_current_source_file();
  const bool canReplayWholeUntouchedClassDeclaration =
      saved_unparsedPartiallyUsingTokenStream &&
      info.SkipClassDefinition() == false && current_source_file != nullptr &&
      (isSgNamespaceDefinitionStatement(classdecl_stmt->get_parent()) !=
           nullptr ||
       isSgClassDefinition(classdecl_stmt->get_parent()) != nullptr) &&
      classdecl_stmt->isTransformation() == false &&
      classdecl_stmt->get_containsTransformation() == false &&
      classdecl_stmt->get_containsTransformationToSurroundingWhitespace() ==
          false &&
      (class_definition == nullptr ||
       (class_definition->isTransformation() == false &&
        class_definition->get_containsTransformation() == false &&
        class_definition->get_containsTransformationToSurroundingWhitespace() ==
            false)) &&
      frontierRequiresPartialTokenUnparse(current_source_file,
                                          classdecl_stmt) == false &&
      canBeUnparsedFromTokenStream(current_source_file, classdecl_stmt);
  if (canReplayWholeUntouchedClassDeclaration) {
    unparseStatementFromTokenStream(classdecl_stmt, e_token_subsequence_start,
                                    e_token_subsequence_end, info);
    return;
  }
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    // unparseStatementFromTokenStream (stmt, e_token_subsequence_start,
    // e_token_subsequence_start); unparseStatementFromTokenStream (stmt,
    // e_token_subsequence_start, e_token_subsequence_end);
    ASSERT_not_null(class_definition);

    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_token_subsequence_start, info);
    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (stmt, function_definition,
    // e_token_subsequence_start, e_leading_whitespace_end, info);

    if (class_definition != NULL) {
      // Unparse the tokens from the start of the function declaration to just
      // befor the opening "{". unparseStatementFromTokenStream (stmt,
      // function_body, e_token_subsequence_start, e_token_subsequence_start,
      // info); unparseStatementFromTokenStream (stmt, function_body,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      // unparseStatementFromTokenStream (stmt, function_definition,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      // I think that we may need to tigger the output of the opening "{" here,
      // and then loop over the declarations in the class definition explicitly.
      // unparseStatementFromTokenStream (stmt, class_definition,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      unparseStatementFromTokenStream(stmt, class_definition,
                                      e_token_subsequence_start,
                                      e_token_subsequence_start, info);

      SgUnparse_Info ninfo(info);
      // unparseStatement(class_definition, info);

      SgStatement *last_member_statement = NULL;
      SgStatement *previous_member_statement = NULL;
      SgDeclarationStatementPtrList::iterator pp =
          class_definition->get_members().begin();

      while (pp != class_definition->get_members().end()) {
        if (previous_member_statement != NULL && current_source_file != NULL &&
            canBeUnparsedFromTokenStream(current_source_file,
                                         previous_member_statement) &&
            canBeUnparsedFromTokenStream(current_source_file, *pp)) {
          bool unparseOnlyWhitespace = false;
          int start_offset = 0;
          int end_offset = -1;
          unparseStatementFromTokenStream(
              previous_member_statement, *pp, e_trailing_whitespace_start,
              e_leading_whitespace_start, info, unparseOnlyWhitespace,
              start_offset, end_offset);
        }

        const bool inherited_partial_token_state =
            ninfo.unparsedPartiallyUsingTokenStream();
        unparseStatement((*pp), ninfo);
        restoreInheritedPartialTokenState(ninfo, inherited_partial_token_state);

        SgStatement *previousStatement = *pp;
        previous_member_statement = previousStatement;
        last_member_statement = previousStatement;

        pp++;
      }
      if (last_member_statement != NULL) {
        // Keep the full interval from the final class member through the class
        // body closer owned by the enclosing class. This preserves inactive
        // preprocessor branches after the last member without letting the
        // class closer interleave ahead of them.
        unparseStatementFromTokenStream(last_member_statement, class_definition,
                                        e_trailing_whitespace_start,
                                        e_token_subsequence_end, info);
        unparseStatementFromTokenStream(stmt, e_token_subsequence_end,
                                        e_token_subsequence_end, info);
      } else {
        // Empty class definitions still need the synthesized `}` and trailing
        // `;` taken directly from the original token stream.
        unparseStatementFromTokenStream(class_definition, stmt,
                                        e_token_subsequence_end,
                                        e_token_subsequence_end, info);
      }
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[partial-class-definition]: modified "
              "class declaration selected for partial token replay has no "
              "definition\n");
      ROSE_ABORT();
    }
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    SgUnparse_Info class_info(info);
    const bool embedded_decl = info.inEmbeddedDecl();
    requireDeclarationSemanticScope(classdecl_stmt);

    auto unparse_enclosing_template_headers = [&]() {
      unparseSourceSpelledTemplateHeaders(
          classdecl_stmt->get_sourceSpelledTemplateHeaders(), classdecl_stmt,
          info, "class");
    };

    // A defining class is dispatched once for the declaration and again from
    // its SgClassDefinition with SkipClassDefinition set.  The outer
    // declaration owns the source template headers; emitting them in both
    // calls duplicates every enclosing template level.  A dependent friend,
    // however, is a forward SgClassDeclaration whose header belongs directly
    // to the FriendDecl and must be emitted in this only call.
    const bool owns_source_template_headers_in_this_call =
        classdecl_stmt->isForward() || !info.SkipClassDefinition();
    if (!classdecl_stmt->get_sourceSpelledTemplateHeaders().empty() &&
        owns_source_template_headers_in_this_call) {
      unparse_enclosing_template_headers();
    }

    if (!classdecl_stmt->isForward() && classdecl_stmt->get_definition() &&
        !info.SkipClassDefinition()) {
      SgUnparse_Info ninfox(class_info);
      ninfox.unset_SkipSemiColon();
      // Class definition emission is controlled by SkipClassDefinition, not by
      // SkipBasicBlock from an outer statement/token-stream context. If a
      // class declaration is selected for full AST unparsing, leaking
      // SkipBasicBlock here collapses the defining declaration into a forward
      // declaration by suppressing the class body inside unparseClassDefnStmt.
      if (ninfox.SkipBasicBlock()) {
        ninfox.unset_SkipBasicBlock();
      }
      if (ninfox.unparsedPartiallyUsingTokenStream() &&
          (classdecl_stmt->isTransformation() ||
           (class_definition != NULL &&
            class_definition->isTransformation()))) {
        ninfox.unset_unparsedPartiallyUsingTokenStream();
      }

      // DQ (6/13/2007): Set to null before resetting to non-null value
      ninfox.set_declstatement_ptr(NULL);
      ninfox.set_declstatement_ptr(classdecl_stmt);

      // printf ("Calling unparseStatement(classdecl_stmt->get_definition(),
      // ninfox); for %s \n",classdecl_stmt->get_name().str());
      if (shouldRenderClassDefinitionInline(
              classdecl_stmt->get_definition(),
              ninfox.unparsedPartiallyUsingTokenStream())) {
        unparseClassDefnStmt(classdecl_stmt->get_definition(), ninfox);
      } else {
        ninfox.set_SkipFormatting();
        unparseStatement(classdecl_stmt->get_definition(), ninfox);
      }

      if (!info.SkipSemiColon()) {
        curprint(";");
      }
    } else {
      if (!info.inEmbeddedDecl()) {
        SgUnparse_Info ninfo(class_info);
        if (classdecl_stmt->get_parent() == NULL) {
          printf("classdecl_stmt->isForward() = %s \n",
                 (classdecl_stmt->isForward() == true) ? "true" : "false");
        }

        // DQ (5/20/2006): This is false within "stdio.h"
        if (classdecl_stmt->get_parent() == NULL) {
          classdecl_stmt->get_file_info()->display(
              "In Unparse_ExprStmt::unparseClassDeclStmt(): "
              "classdecl_stmt->get_parent() == NULL");
        }
        // ASSERT_not_null(classdecl_stmt->get_parent());
        SgClassDefinition *cdefn =
            isSgClassDefinition(classdecl_stmt->get_parent());

        if (cdefn && cdefn->get_declaration()->get_class_type() ==
                         SgClassDeclaration::e_class) {
          ninfo.set_CheckAccess();
        }

        // DQ (8/19/2004): Removed functions using old attribute mechanism (old
        // CC++ mechanism) printf ("Commented out
        // get_suppress_global(classdecl_stmt) \n"); if
        // (get_suppress_global(classdecl_stmt))
        //      ninfo.set_SkipGlobal(); //attributes.h
        // printDebugInfo("entering unp->u_sage->printSpecifier", true);
        unp->u_sage->printSpecifier(classdecl_stmt, ninfo);
        info.set_access_attribute(ninfo.get_access_attribute());
      }

      info.unset_inEmbeddedDecl();
      if (!info.SkipClassSpecifier()) {
        switch (classdecl_stmt->get_class_type()) {
        case SgClassDeclaration::e_class: {
          curprint("class ");
          break;
        }
        case SgClassDeclaration::e_struct: {
          curprint("struct ");
          break;
        }
        case SgClassDeclaration::e_union: {
          curprint("union ");
          break;
        }

          // DQ (4/17/2007): Added this enum value to the switch cases.
        case SgClassDeclaration::e_template_parameter: {
          // skip type elaboration here.
          curprint(" ");
          break;
        }

          // DQ (4/17/2007): Added this enum value to the switch cases.
        default: {
          printf("Error: default reached in unparseClassDeclStmt() \n");
          ROSE_ABORT();
        }
        }
        // GNU visibility/type_visibility attributes on a class belong in the
        // decl-specifier sequence.  The typed modifier is declaration-local;
        // emit it here before the class name rather than through the generic
        // post-declarator attribute path.
        unp->u_sage->printGnuVisibilityAttributes(classdecl_stmt,
                                                  /*leading_space=*/false);
        // GNU class alignment is a decl-specifier attribute.  This position is
        // valid for both forward declarations and definitions and is the one
        // source-emission owner for SgClassDeclaration alignment; the generic
        // post-name and post-body paths deliberately exclude class alignment.
        const short alignmentValue = classdecl_stmt->get_declarationModifier()
                                         .get_typeModifier()
                                         .get_gnu_attribute_alignment();
        if (alignmentValue >= 0) {
          curprint("__attribute__((aligned(");
          curprint(StringUtility::numberToString((int)alignmentValue));
          curprint("))) ");
        }
      }

      /* have to make sure if it needs qualifier or not */

      SgName nm = classdecl_stmt->get_name();

      // DQ (8/19/2014): Adding code to output the template instantiation with
      // template arguments processed to support name qualification.
      SgTemplateInstantiationDecl *templateInstantiation =
          isSgTemplateInstantiationDecl(classdecl_stmt);
      if (templateInstantiation != NULL) {
        nm = templateInstantiation->get_name();
      }

      // DQ (7/20/2011): Test compilation without these functions.

      // DQ (7/28/2012): This is the original code (I think it is what we really
      // want, but we need to test this. DQ (6/5/2011): Newest refactored
      // support for name qualification.
      SgName nameQualifier;
      if (classdecl_stmt->isForward() ||
          classdecl_stmt->get_parent() != classdecl_stmt->get_scope()) {
        nameQualifier =
            SgName(exactStatementNameQualification(unp, classdecl_stmt, info)
                       .qualifier);
      }

      // Anonymity is semantic AST state. A synthesized internal name must never
      // become source syntax.
      const bool needs_embedded_name_separator =
          embedded_decl ||
          isSgTypedefDeclaration(classdecl_stmt->get_parent()) != NULL ||
          isSgVariableDeclaration(classdecl_stmt->get_parent()) != NULL;
      // DQ (8/19/2014): Adding code to output the template instantiation with
      // template arguments processed to support name qualification.
      if (templateInstantiation != NULL) {
        // DQ (4/13/2019): Make this conditional upon the setting of
        // info.SkipNameQualification() curprint (nameQualifier);
        if (info.SkipNameQualification() == false) {
          curprint(nameQualifier);
          // curprint ("/* conditional output of name qualification */");
        }

        // DQ (4/13/2019): Turn this off before processing the rest of the
        // template instantiation which man contain template arguments that
        // require name qualification.
        info.unset_SkipNameQualification();

        unparseTemplateName(templateInstantiation, info);
        if (needs_embedded_name_separator) {
          curprint(" ");
        }
      } else {
        // DQ (11/21/2021): I think we can skip the name of the enum here for
        // where this is used in the typedef as a anonymous type.
        // curprint(enum_stmt->get_name() + " ");
        // printf ("We could skip the name of the enum here ... \n");
        // if (info.PrintName() == true)
        // if (isAnonymousName == false && classdecl_stmt->get_isUnNamed() ==
        // false) if (isAnonymousName == false &&
        // classdecl_stmt->get_isUnNamed() == false && info.PrintName() == true)
        if (classdecl_stmt->get_isUnNamed() == false) {
          curprint(nameQualifier.str());
          if (SgTemplateClassDeclaration *template_class_decl =
                  isSgTemplateClassDeclaration(classdecl_stmt)) {
            SgName template_name = template_class_decl->get_templateName();
            if (template_name.is_null() || template_name.getString().empty()) {
              template_name = classdecl_stmt->get_name();
            }
            curprint(template_name);

            const SgTemplateArgumentPtrList &specialization_args =
                template_class_decl->get_templateSpecializationArguments();
            if (!specialization_args.empty() &&
                template_class_decl->get_specialization() !=
                    SgDeclarationStatement::e_no_specialization) {
              SgTemplateArgumentPtrList explicit_args = specialization_args;
              SgUnparse_Info template_arg_info(info);
              template_arg_info.set_SkipClassDefinition();
              template_arg_info.set_SkipEnumDefinition();
              template_arg_info.set_SkipClassSpecifier();
              template_arg_info.set_declstatement_ptr(NULL);
              template_arg_info.set_current_context(NULL);
              unp->u_exprStmt->unparseTemplateArgumentList(
                  explicit_args, template_arg_info,
                  TemplateArgumentEmission::explicit_source_prefix);
            }
          } else {
            curprint(classdecl_stmt->get_name());
          }
          if (needs_embedded_name_separator) {
            curprint(" ");
          }
        }
      }

      // DQ (2/12/2019): The "final" keyword can ounly be output on the defining
      // declaration (at least for GNU g++ version 5.1). It is however
      // consistant in ROSE that it be marked uniformally within the defining
      // and nondefining declaration. DQ (8/11/2014): Added support for final
      // keyword unparsing. if
      // (classdecl_stmt->get_declarationModifier().isFinal() == true)
      if ((classdecl_stmt->isForward() == false) &&
          (classdecl_stmt->get_declarationModifier().isFinal() == true)) {
        // DQ (2/12/2019): Testing, final can't be used on prototypes (I think).
        // curprint(" /* output from test 2 */ ");
        curprint(" final");
      }

      unp->u_sage->printAttributes(classdecl_stmt, info);

      {
        // DQ (6/2/2021): Original code.
        if (classdecl_stmt->isForward() && !info.SkipSemiColon()) {
          curprint(";");

          if (classdecl_stmt->isExternBrace()) {
          }
        }
      }
    }

    // DQ (6/3/2021): Closing brace for if
    // (saved_unparsedPartiallyUsingTokenStream == false) above.
  }

#if DEBUG_USING_CURPRINT
  curprint("/* Leaving unparseClassDeclStmt */ \n");
#endif
}

void Unparse_ExprStmt::unparseClassInheritanceList(
    SgClassDefinition *classdefn_stmt, SgUnparse_Info &ninfo) {
  // print out the class inheritance

#define DEBUG_UNPARSE_CLASS_INHERITANCE 0

#if DEBUG_UNPARSE_CLASS_INHERITANCE
  printf("Inside of unparseClassInheritanceList \n");
  curprint("/* Inside of unparseClassInheritanceList */ \n");
#endif

  SgBaseClassPtrList::iterator p = classdefn_stmt->get_inheritances().begin();
  if (p != classdefn_stmt->get_inheritances().end()) {
    curprint(string(" : "));

    // DQ (5/9/2011): This loop structure should be rewritten...
    while (true) {
      SgBaseClass *bcls = *p;
      ASSERT_not_null(bcls);

      SgBaseClassModifier &baseClassModifier = *(bcls->get_baseClassModifier());

      if (baseClassModifier.isVirtual()) {
        curprint(string("virtual "));
      }

      if (baseClassModifier.get_accessModifier().isPublic()) {
        curprint(string("public "));
      }
      if (baseClassModifier.get_accessModifier().isPrivate()) {
        curprint(string("private "));
      }
      if (baseClassModifier.get_accessModifier().isProtected()) {
        curprint(string("protected "));
      }

      // DQ (5/12/2011): This might have to be a qualified name...
      SgUnparse_Info tmp_ninfo(ninfo);
      const NameQualificationResult baseQualification =
          unp->u_name->lookup_type_qualification(
              bcls, classdefn_stmt->get_declaration());
      tmp_ninfo.set_name_qualification_length(baseQualification.length);
      tmp_ninfo.set_global_qualification_required(baseQualification.global);
      tmp_ninfo.set_type_elaboration_required(
          baseQualification.typeElaboration);

      bool is_pack_expansion_base = bcls->get_pack_expansion();

      SgType *source_type = bcls->get_source_type();
      ASSERT_not_null(source_type);
      if (isSgNamedType(source_type) == nullptr &&
          isSgTemplateType(source_type) == nullptr &&
          isSgDeclType(source_type) == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[base-class-source-type]: base=%p "
                "source type=%s is not an exact C++ base type surface\n",
                static_cast<void *>(bcls), source_type->class_name().c_str());
        ROSE_ABORT();
      }
      tmp_ninfo.set_reference_node_for_qualification(bcls);
      tmp_ninfo.set_template_argument_qualification_context(
          classdefn_stmt->get_declaration());
      tmp_ninfo.set_SkipClassDefinition();
      tmp_ninfo.set_SkipEnumDefinition();
      tmp_ninfo.set_SkipClassSpecifier();
      tmp_ninfo.unset_isTypeFirstPart();
      tmp_ninfo.unset_isTypeSecondPart();
      unp->u_type->unparseType(source_type, tmp_ninfo);

      const SgTemplateType *template_source = isSgTemplateType(source_type);
      if (template_source != nullptr && template_source->get_packed()) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[base-class-pack-surface]: base=%p "
                "source type=%p retains declaration-owned pack identity\n",
                static_cast<void *>(bcls), static_cast<void *>(source_type));
        ROSE_ABORT();
      }
      if (is_pack_expansion_base) {
        curprint("...");
      }

      p++;

      if (p != classdefn_stmt->get_inheritances().end()) {
        curprint(string(", "));
      } else {
        break;
      }
    }
  }
}

SgStatement *Unparse_ExprStmt::unparseClassMembersWithSourceRoles(
    SgClassDefinition *classDefinition, SgUnparse_Info &info,
    bool forceAstStatementEmission) {
  ASSERT_not_null(classDefinition);
  initializeClassAccessState(classDefinition, info);
  const SgDeclarationStatementPtrList &classMembers =
      classDefinition->get_members();
  const SgUnsignedCharList &sourceMemberRoles =
      classDefinition->get_source_member_roles();
  auto includeCount = [](SgLocatedNode *owner,
                         PreprocessingInfo::RelativePositionType position) {
    size_t count = 0;
    AttachedPreprocessingInfoType *attached =
        owner != nullptr ? owner->getAttachedPreprocessingInfo() : nullptr;
    if (attached == nullptr) {
      return count;
    }
    for (PreprocessingInfo *preprocessingInfo : *attached) {
      ASSERT_not_null(preprocessingInfo);
      const PreprocessingInfo::DirectiveType type =
          preprocessingInfo->getTypeOfDirective();
      if ((type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration) &&
          preprocessingInfo->getRelativePosition() == position) {
        ++count;
      }
    }
    return count;
  };
  for (SgDeclarationStatement *member : classMembers) {
    if (member == nullptr || member->get_parent() != classDefinition) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[class-member-owner]: class=%p has a "
              "null or non-owned direct member\n",
              static_cast<void *>(classDefinition));
      ROSE_ABORT();
    }
  }
  // An attached include is not, by itself, evidence that a direct class
  // member came from that include.  Includes can occur within base specifiers,
  // nested declarators, or ordinary member boundaries without expanding a
  // declaration into the class.  The frontend publishes this vector only when
  // it has proved exact member ownership from Clang source locations.
  if (!sourceMemberRoles.empty() &&
      sourceMemberRoles.size() != classMembers.size()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[class-member-source-role]: class=%p "
            "members=%zu roles=%zu does not publish one exact "
            "include-expansion ownership map\n",
            static_cast<void *>(classDefinition), classMembers.size(),
            sourceMemberRoles.size());
    ROSE_ABORT();
  }

  SgStatement *lastMember = nullptr;
  for (size_t memberIndex = 0; memberIndex < classMembers.size();
       ++memberIndex) {
    SgDeclarationStatement *member = classMembers[memberIndex];
    const bool inheritedPartialTokenState =
        info.unparsedPartiallyUsingTokenStream();
    validateClassMemberAccess(classDefinition, member, info);
    if (const SgAccessLabelStatement *label =
            isSgAccessLabelStatement(member)) {
      applyClassAccessLabelState(label, info);
    }
    const unsigned char sourceMemberRole =
        sourceMemberRoles.empty()
            ? static_cast<unsigned char>(SgClassDefinition::e_source_member_ast)
            : sourceMemberRoles[memberIndex];
    if (sourceMemberRole ==
        SgClassDefinition::e_source_member_include_expansion) {
      const size_t beforeIncludeCount =
          includeCount(member, PreprocessingInfo::before);
      const bool startsIncludeOwnedRun =
          memberIndex == 0 || beforeIncludeCount != 0 ||
          sourceMemberRoles[memberIndex - 1] ==
              SgClassDefinition::e_source_member_ast;
      if (startsIncludeOwnedRun && beforeIncludeCount == 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[class-member-source-role]: class=%p "
                "include-owned run at member=%zu has no exact attached "
                "include boundary\n",
                static_cast<void *>(classDefinition), memberIndex);
        ROSE_ABORT();
      }
      unparseAttachedPreprocessingInfo(member, info, PreprocessingInfo::before);
    } else if (sourceMemberRole == SgClassDefinition::e_source_member_ast) {
      if (forceAstStatementEmission) {
        info.set_forceAstStatementEmission();
      }
      unparseStatement(member, info);
    } else {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[class-member-source-role]: class=%p "
              "member=%zu has invalid role=%u\n",
              static_cast<void *>(classDefinition), memberIndex,
              static_cast<unsigned>(sourceMemberRole));
      ROSE_ABORT();
    }
    restoreInheritedPartialTokenState(info, inheritedPartialTokenState);
    lastMember = member;
  }
  return lastMember;
}

void Unparse_ExprStmt::unparseClassDefnStmt(SgStatement *stmt,
                                            SgUnparse_Info &info) {

#define DEBUG_UNPARSE_CLASS_DEFINITION 0

#if DEBUG_UNPARSE_CLASS_DEFINITION
  printf("In unparseClassDefnStmt() \n");
  curprint("/* In unparseClassDefnStmt() */ \n");
#endif

  SgClassDefinition *classdefn_stmt = isSgClassDefinition(stmt);
  ASSERT_not_null(classdefn_stmt);
  const bool body_needs_leading_space =
      classDefinitionBodyNeedsLeadingSpace(classdefn_stmt);

  // DQ (5/28/2021): Adding support for partial token sequence unparsing.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    // SgStatement* body = while_stmt->get_body();
    // curprint("/* partial token sequence SgWhileStmt */ ");
    // SgStatement* condition = while_stmt->get_condition();
    // unparseStatementFromTokenStream (stmt, condition,
    // e_token_subsequence_start, e_token_subsequence_start, info);
  } else {
    // unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK2);
    // curprint("/* from AST SgWhileStmt */ while(");
    // curprint("while(");
    // unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  }

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(classdefn_stmt);
#endif

  SgUnparse_Info ninfo(info);
  ninfo.unset_SkipFormatting();
  ninfo.set_template_argument_qualification_context(
      classdefn_stmt->get_declaration());

  // curprint ( string("/* Print out class declaration */ \n";

  ninfo.set_SkipClassDefinition();

  // DQ (9/9/2016): Added to conform to unifor testing that these are always
  // equal.
  ninfo.set_SkipEnumDefinition();

  // DQ (10/13/2006): test2004_133.C demonstrates where we need to unparse
  // qualified names for class definitions (defining declaration). DQ
  // (10/11/2006): Don't generate qualified names for the class name of a
  // defining declaration ninfo.set_SkipQualifiedNames();

  // DQ (7/19/2003) skip the output of the semicolon
  ninfo.set_SkipSemiColon();

#if DEBUG_USING_CURPRINT
  curprint(
      "\n/* In unparseClassDefnStmt(): calling unparseClassDeclStmt() */ \n");
#endif

  // printf ("Calling unparseClassDeclStmt = %p isForward = %s from
  // unparseClassDefnStmt = %p \n",
  //      classdefn_stmt->get_declaration(),(classdefn_stmt->get_declaration()->isForward()
  //      == true) ? "true" : "false",classdefn_stmt);
  ASSERT_not_null(classdefn_stmt->get_declaration());
  unparseClassDeclStmt(classdefn_stmt->get_declaration(), ninfo);

#if DEBUG_USING_CURPRINT
  curprint("\n/* In unparseClassDefnStmt(): DONE: calling "
           "unparseClassDeclStmt() */ \n");
#endif

  // DQ (7/19/2003) unset the specification to skip the output of the semicolon
  ninfo.unset_SkipSemiColon();

  // DQ (10/11/2006): Don't generate qualified names for the class name of a
  // defining declaration ninfo.unset_SkipQualifiedNames();

  ninfo.unset_SkipClassDefinition();

  // DQ (9/9/2016): Added to conform to unifor testing that these are always
  // equal.
  ninfo.unset_SkipEnumDefinition();

  // curprint("/* END: Print out class declaration */ \n");

  SgNamedType *saved_context = ninfo.get_current_context();

  // DQ (11/29/2004): The use of a primary and secondary declaration casue two
  // SgClassType nodes to be generated (which should be fixed) since this is
  // compared to another SgClassType within the generateQualifiedName() function
  // we have to get the the type from the non-defining declaration uniformally.
  // Same way each time so that the pointer test will be meaningful.
  // ninfo.set_current_context(classdefn_stmt->get_declaration()->get_type());
  ASSERT_not_null(
      classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration());
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(
      classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration());
  ASSERT_not_null(classDeclaration->get_type());

  // DQ (6/13/2007): Set to null before resetting to non-null value
  // ninfo.set_current_context(classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration()->get_type());
  ninfo.set_current_context(NULL);
  ninfo.set_current_context(classDeclaration->get_type());

#if DEBUG_UNPARSE_CLASS_DEFINITION
  printf("In unparseClassDefnStmt(): Print out inheritance \n");
  curprint("/* Print out inheritance */ \n");
#endif

  // DQ (1/8/2020): Refactors the output of base classes so that it can be
  // supported in the unparseClassDefnStmt() and unparseClassType() functions.
  unparseClassInheritanceList(classdefn_stmt, ninfo);

#if DEBUG_UNPARSE_CLASS_DEFINITION
  // curprint ( string("\n/* After specification of base classes unparse the
  // declaration body */ \n";
  printf("After specification of base classes unparse the declaration body  "
         "info.SkipBasicBlock() = %s \n",
         (info.SkipBasicBlock() == true) ? "true" : "false");
#endif

  // DQ (9/28/2004): Turn this back on as the only way to prevent this from
  // being unparsed! DQ (11/22/2003): Control unparsing of the {} part of the
  // definition if ( !info.SkipBasicBlock() )
  if (info.SkipBasicBlock() == false) {
    // curprint ( string("\n/* Unparsing class definition within
    // unparseClassDefnStmt */ \n";

    // DQ (6/14/2006): Add packing pragma support (explicitly set the packing
    // alignment to the default, part of packing pragma normalization).
    unsigned int packingAlignment = classdefn_stmt->get_packingAlignment();
    if (packingAlignment != 0) {
      const std::string packDirective =
          std::string("#pragma pack(") +
          StringUtility::numberToString(packingAlignment) + ")";
      if (unp->cur.get_compact_output()) {
        unp->cur.emit_compact_directive(packDirective);
      } else {
        curprint("\n" + packDirective + "\n");
      }
    }

    const bool render_empty_inline = shouldRenderClassDefinitionInline(
        classdefn_stmt, saved_unparsedPartiallyUsingTokenStream);
    const bool inline_body_needs_leading_space =
        body_needs_leading_space && classdefn_stmt->get_inheritances().empty();

    if (render_empty_inline) {
      curprint(inline_body_needs_leading_space ? " {}" : "{}");
    } else {
      curprint(body_needs_leading_space ? " {" : "{");
      unp->cur.format(classdefn_stmt, info, FORMAT_AFTER_BASIC_BLOCK1);
    }

    SgStatement *last_member_statement =
        unparseClassMembersWithSourceRoles(classdefn_stmt, ninfo, false);

    if (render_empty_inline == false) {
      // DQ (3/17/2005): This helps handle cases such as class foo { #include
      // "constant_code.h" }
      ASSERT_not_null(classdefn_stmt->get_startOfConstruct());
      ASSERT_not_null(classdefn_stmt->get_endOfConstruct());

      unparseAttachedPreprocessingInfo(classdefn_stmt, info,
                                       PreprocessingInfo::inside);

      // DQ (5/28/2021): Fixing the unparseClassDefnStmt() function to support
      // partial unparsing from the token stream.
      // unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
      // curprint ( string("}"));
      if (saved_unparsedPartiallyUsingTokenStream == false) {
#if DEBUG_USING_CURPRINT
        curprint("\n/* saved_unparsedPartiallyUsingTokenStream == false */\n");
#endif
        unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
        curprint(string("}"));
      } else {
        ASSERT_not_null(classdefn_stmt->get_declaration());
        if (last_member_statement != NULL) {
          // Preserve any inactive-branch tokens that appear after the final
          // class member and before the class-closing brace.
          unparseStatementFromTokenStream(last_member_statement, classdefn_stmt,
                                          e_trailing_whitespace_start,
                                          e_token_subsequence_end, info);
          unparseStatementFromTokenStream(classdefn_stmt->get_declaration(),
                                          e_token_subsequence_end,
                                          e_token_subsequence_end, info);
        } else {
          unparseStatementFromTokenStream(
              classdefn_stmt, classdefn_stmt->get_declaration(),
              e_token_subsequence_end, e_token_subsequence_end, info);
        }
      }
    }

    // DQ (6/14/2006): Add packing pragma support (reset the packing
    // alignment to the default, part of packing pragma normalization).
    if (packingAlignment != 0) {
      if (unp->cur.get_compact_output()) {
        unp->cur.emit_compact_directive("#pragma pack()");
      } else {
        curprint("\n#pragma pack()\n");
      }
    }

    if (render_empty_inline == false && info.SkipSemiColon()) {
      unp->cur.format(classdefn_stmt, info, FORMAT_AFTER_BASIC_BLOCK2);
    }
  }

  // DQ (6/13/2007): Set to null before resetting to non-null value
  ninfo.set_current_context(NULL);
  ninfo.set_current_context(saved_context);

  unparseTypeAttributes(classdefn_stmt->get_declaration());

#if DEBUG_USING_CURPRINT
  curprint("/* Leaving unparseClassDefnStmt */ \n");
#endif
}

void Unparse_ExprStmt::unparseTypeAttributes(
    SgDeclarationStatement *declaration) {
  // DQ (10/4/2012): Added support for transparent unions.
  ASSERT_not_null(declaration);

  bool isGnuAttributeTransparentUnion = declaration->get_declarationModifier()
                                            .get_typeModifier()
                                            .isGnuAttributeTransparentUnion();

  // If this came from a type then declaration is the first nondefining
  // declaration (see test2012_10_4.c).
  bool definingDeclaration_isGnuAttributeTransparentUnion = false;
  if (declaration->get_definingDeclaration() != NULL)
    definingDeclaration_isGnuAttributeTransparentUnion =
        declaration->get_definingDeclaration()
            ->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributeTransparentUnion();

  if (definingDeclaration_isGnuAttributeTransparentUnion == true)
    isGnuAttributeTransparentUnion = true;

  // This should only be set for unions.
  if (isGnuAttributeTransparentUnion == true) {
    curprint(" __attribute__((__transparent_union__))");
  }

  // DQ (1/3/2014): Added support for packing attribute.
  if (declaration->get_declarationModifier()
          .get_typeModifier()
          .isGnuAttributePacked() == true) {
    // curprint(" /* from unparseTypeAttributes(SgDeclarationStatement*) */
    // __attribute__((packed))");
    curprint(" __attribute__((packed))");
  }

  // Alignment has one declaration-kind-specific emission owner.  A class
  // emits it in the decl-specifier sequence before its name; a typedef emits
  // it through printAttributes after its declarator.  Repeating it here after
  // an embedded definition duplicates typedef alignment and previously also
  // gave class definitions two competing attribute positions.
}

void Unparse_ExprStmt::unparseEnumDeclStmt(SgStatement *stmt,
                                           SgUnparse_Info &info) {
  SgEnumDeclaration *enum_stmt = isSgEnumDeclaration(stmt);
  ASSERT_not_null(enum_stmt);
  enum_stmt->validate_enumerator_source_ownership();

  struct SourceEnumerator {
    SgInitializedName *field;
    bool has_semantic_successor;
  };
  std::vector<SourceEnumerator> source_enumerators;
  source_enumerators.reserve(enum_stmt->get_enumerators().size());
  for (size_t semantic_index = 0;
       semantic_index < enum_stmt->get_enumerators().size(); ++semantic_index) {
    SgInitializedName *enumerator =
        enum_stmt->get_enumerators()[semantic_index];
    ROSE_ASSERT(enumerator != nullptr);
    switch (enumerator->get_enum_constant_source_ownership()) {
    case SgInitializedName::e_enum_constant_source_body:
      source_enumerators.push_back(
          {enumerator,
           semantic_index + 1 < enum_stmt->get_enumerators().size()});
      break;
    case SgInitializedName::e_enum_constant_source_external:
      // The owning include directive is part of this enum's typed lexical
      // surface.  The external constant remains in the semantic list but its
      // spelling belongs exclusively to that included physical source.
      break;
    case SgInitializedName::e_enum_constant_semantic_only:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[enum-source-ownership]: declaration=%p "
              "name=%s contains a semantic-only enumerator at a source "
              "emission site\n",
              static_cast<void *>(enum_stmt),
              enum_stmt->get_name().getString().c_str());
      ROSE_ABORT();
    case SgInitializedName::e_enum_constant_source_unclassified:
    case SgInitializedName::e_last_enum_constant_source_ownership:
    default:
      ROSE_ABORT();
    }
  }

  // info.display("Called inside of unparseEnumDeclStmt()");

#if DEBUG_USING_CURPRINT
  curprint("\n/* Inside of Unparse_ExprStmt::unparseEnumDeclStmt() */ \n");
#endif

  string enum_string = "enum ";

  // DQ (8/12/2014): Adding support for C++11 scoped enums (syntax is "enum
  // class ").
  if (enum_stmt->get_isScopedEnum() == true) {
    enum_string += "class ";
  }

  // Check if this enum declaration appears imbedded within another declaration
  if (!info.inEmbeddedDecl()) {
    // This is the more common declaration of an enum with the definition
    // attached.
    // If this is part of a class definition then get the access information
    SgClassDefinition *cdefn = isSgClassDefinition(enum_stmt->get_parent());
    if (cdefn && cdefn->get_declaration()->get_class_type() ==
                     SgClassDeclaration::e_class) {
      info.set_CheckAccess();
    }
    // printDebugInfo("entering unp->u_sage->printSpecifier", true);
    unp->u_sage->printSpecifier(enum_stmt, info);
    info.unset_CheckAccess();

    // DQ (2/14/2019): Adding name qualification support.
    // curprint(enum_string + enum_stmt->get_name().str() + " ");
    curprint(enum_string);

    ASSERT_not_null(enum_stmt);

    SgName nameQualifier(
        exactStatementNameQualification(unp, enum_stmt, info).qualifier);

    // DQ (11/21/2021): I think we can skip the name of the enum here for where
    // this is used in the typedef as a anonymous type.
    // curprint(enum_stmt->get_name() + " ");
    // printf ("We could skip the name of the enum here ... \n");
    const bool isAnonymousName = enum_stmt->get_isUnNamed();
    if (isAnonymousName == false) {
      curprint(nameQualifier.str());
      curprint(enum_stmt->get_name() + " ");
    } else {
      if (info.PrintName() == true) {
        curprint(nameQualifier.str());
        curprint(enum_stmt->get_name() + " ");
      }
    }
  } else {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[embedded-enum-declaration]: embedded enum "
            "declaration reached standalone enum emission\n");
    ROSE_ABORT();
  }

  unp->u_sage->printAttributes(enum_stmt, info);

  // DQ (8/12/2014): Adding support for C++11 base type specification syntax.
  if (enum_stmt->get_underlying_type_source_spelled()) {
    if (enum_stmt->get_field_type() == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[enum-underlying-type]: declaration=%p "
              "name=%s has source syntax without a semantic type\n",
              static_cast<void *>(enum_stmt),
              enum_stmt->get_name().getString().c_str());
      ROSE_ABORT();
    }
    curprint(" : ");

    // Make a new SgUnparse_Info object.
    SgUnparse_Info ninfo(info);
    unp->u_type->unparseType(enum_stmt->get_field_type(), ninfo);
  }

  // DQ (6/26/2005): Support for empty enum declarations!
  if (enum_stmt == enum_stmt->get_definingDeclaration()) {
    curprint(source_enumerators.empty() ? "{" : "{ ");
  }

  // if (!info.SkipDefinition()
  if (!info.SkipEnumDefinition()
      /* [BRN] 4/19/2002 --  part of the fix in unparsing var decl including
         enum definition */
      || enum_stmt->get_embedded()) {
    SgUnparse_Info ninfo(info);
    ninfo.set_inEnumDecl();
    SgInitializer *tmp_init = NULL;
    SgName tmp_name;

#if DEBUG_USING_CURPRINT
    curprint("\n/* In Unparse_ExprStmt::unparseEnumDeclStmt(): output the "
             "enumerators */ \n");
#endif

    for (size_t index = 0; index < source_enumerators.size(); ++index) {
      SgInitializedName *field = source_enumerators[index].field;
      if (field == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[enum-enumerator]: declaration=%p "
                "name=%s contains a null enumerator at index=%zu\n",
                static_cast<void *>(enum_stmt),
                enum_stmt->get_name().getString().c_str(), index);
        ROSE_ABORT();
      }
#if DEBUG_USING_CURPRINT
      curprint("\n/* In Unparse_ExprStmt::unparseEnumDeclStmt(): output "
               "enumerator */ \n");
#endif
      unparseAttachedPreprocessingInfo(field, info, PreprocessingInfo::before);
      tmp_name = field->get_name();
      tmp_init = field->get_initializer();
      curprint(tmp_name.str());
      if (tmp_init != NULL) {
        curprint(" = ");
        unparseExpression(tmp_init, ninfo);
      }
      unparseAttachedPreprocessingInfo(field, info, PreprocessingInfo::after);
      // An included physical source can own the next semantic constants.  The
      // comma before that include boundary remains owned by this direct enum
      // constant even when no later direct constant is emitted here.
      if (source_enumerators[index].has_semantic_successor) {
        curprint(", ");
      }
    }

    if (enum_stmt == enum_stmt->get_definingDeclaration()) {
      // The defining declaration owns the enum body boundary even when the
      // body has no enumerators. Omitting this for an empty enum silently
      // discarded source directives and comments between its braces.
      unparseAttachedPreprocessingInfo(enum_stmt, info,
                                       PreprocessingInfo::inside);
    }
    /* [BRN] 4/19/2002 -- part of fix in unparsing var decl including enum
     * definition */
    if (enum_stmt->get_embedded()) {
      curprint(" ");
    }
    /* [BRN] end */
  } /* if */

  // DQ (6/26/2005): Support for empty enum declarations!
  if (enum_stmt == enum_stmt->get_definingDeclaration()) {
    curprint(source_enumerators.empty() ? "}" : " }");
  }

  // DQ (6/26/2005): Moved to location after output of closing "}" from enum
  // definition
  if (!info.SkipSemiColon()) {
    curprint(";");
    if (enum_stmt->isExternBrace()) {
    }
  }

#if DEBUG_USING_CURPRINT
  curprint("\n/* Leaving unparseEnumDeclStmt() */ \n");
#endif
}

void Unparse_ExprStmt::unparseExprStmt(SgStatement *stmt,
                                       SgUnparse_Info &info) {
  SgExprStatement *expr_stmt = isSgExprStatement(stmt);
  ASSERT_not_null(expr_stmt);
  SgUnparse_Info newinfo(info);

  // DQ (5/9/2015): Added assertion.
  ASSERT_not_null(expr_stmt->get_expression());

  // Expressions are another place where a class definition should NEVER be
  // unparsed DQ (5/23/2007): Note that statement expressions can have class
  // definition (so they are exceptions, see test2007_51.C).
  newinfo.set_SkipClassDefinition();

  // DQ (1/9/2014): We have to make the handling of enum definitions consistant
  // with that of class definitions.
  newinfo.set_SkipEnumDefinition();

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(newinfo.SkipClassDefinition() == newinfo.SkipEnumDefinition());

  // if (expr_stmt->get_the_expr())
  if (expr_stmt->get_expression()) {
    // printDebugInfo(getSgVariant(expr_stmt->get_the_expr()->variant()), true);
    // unparseExpression(expr_stmt->get_the_expr(), newinfo);
    unparseExpression(expr_stmt->get_expression(), newinfo);
  } else {
    ROSE_ABORT();
  }

  if (newinfo.inVarDecl()) {
    curprint(",");
  } else {
    // DQ (11/2/2015): This is part of a change to support uniformity in how for
    // statement tests are unparsed. if (!newinfo.inConditional() &&
    // !newinfo.SkipSemiColon()) if (newinfo.SkipSemiColon() == false)
    if (newinfo.SkipSemiColon() == false) {
      // DQ (11/2/2015): Add a space to match previous behavior (and tests using
      // diff). No, I don't like this. curprint(";");
      curprint(";");
    }
  }
}

void Unparse_ExprStmt::unparseAttributedStatement(SgStatement *stmt,
                                                  SgUnparse_Info &info) {
  SgAttributedStatement *attributed = isSgAttributedStatement(stmt);
  ASSERT_not_null(attributed);
  attributed->validate();

  using Attribute = SgStatementAttribute;
  const SgStatementAttributePtrList &attributes = attributed->get_attributes();

  auto unparse_argument = [&](SgExpression *argument) {
    if (argument == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[statement-attribute-expression]: "
              "attributed-statement=%p has a missing expression payload\n",
              static_cast<void *>(attributed));
      ROSE_ABORT();
    }
    SgUnparse_Info expression_info(info);
    expression_info.set_SkipFormatting();
    expression_info.set_reference_node_for_qualification(argument);
    unparseExpression(argument, expression_info);
  };

  auto loop_option_name = [](Attribute::loop_hint_option_enum option) {
    switch (option) {
    case Attribute::e_loop_hint_option_vectorize:
      return "vectorize";
    case Attribute::e_loop_hint_option_vectorize_width:
      return "vectorize_width";
    case Attribute::e_loop_hint_option_interleave:
      return "interleave";
    case Attribute::e_loop_hint_option_interleave_count:
      return "interleave_count";
    case Attribute::e_loop_hint_option_unroll:
      return "unroll";
    case Attribute::e_loop_hint_option_unroll_count:
      return "unroll_count";
    case Attribute::e_loop_hint_option_unroll_and_jam:
      return "unroll_and_jam";
    case Attribute::e_loop_hint_option_unroll_and_jam_count:
      return "unroll_and_jam_count";
    case Attribute::e_loop_hint_option_pipeline_disabled:
      return "pipeline";
    case Attribute::e_loop_hint_option_pipeline_initiation_interval:
      return "pipeline_initiation_interval";
    case Attribute::e_loop_hint_option_distribute:
      return "distribute";
    case Attribute::e_loop_hint_option_vectorize_predicate:
      return "vectorize_predicate";
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[statement-loop-hint]: invalid option=%d\n",
              static_cast<int>(option));
      ROSE_ABORT();
    }
  };

  auto unparse_loop_value = [&](const Attribute *attribute) {
    ASSERT_not_null(attribute);
    curprint("(");
    switch (attribute->get_loop_hint_state()) {
    case Attribute::e_loop_hint_state_numeric:
      unparse_argument(attribute->get_expression_argument());
      break;
    case Attribute::e_loop_hint_state_fixed_width:
      if (attribute->get_expression_argument() != nullptr) {
        unparse_argument(attribute->get_expression_argument());
      } else {
        curprint("fixed");
      }
      break;
    case Attribute::e_loop_hint_state_scalable_width:
      if (attribute->get_expression_argument() != nullptr) {
        unparse_argument(attribute->get_expression_argument());
        curprint(", scalable");
      } else {
        curprint("scalable");
      }
      break;
    case Attribute::e_loop_hint_state_enable:
      curprint("enable");
      break;
    case Attribute::e_loop_hint_state_disable:
      curprint("disable");
      break;
    case Attribute::e_loop_hint_state_assume_safety:
      curprint("assume_safety");
      break;
    case Attribute::e_loop_hint_state_full:
      curprint("full");
      break;
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[statement-loop-hint]: invalid state=%d\n",
              static_cast<int>(attribute->get_loop_hint_state()));
      ROSE_ABORT();
    }
    curprint(")");
  };

  // Pragmas must occupy complete lines. Clang may place several loop-hint
  // attributes on one AttributedStmt, so emit all pragma forms before the
  // bracket/GNU attributes that directly prefix the wrapped statement.
  for (const Attribute *attribute : attributes) {
    ASSERT_not_null(attribute);
    attribute->validate();
    if (attribute->get_kind() != Attribute::e_statement_attribute_loop_hint) {
      continue;
    }

    if (unp->cur.get_compact_output()) {
      unp->cur.begin_compact_directive();
    }
    switch (attribute->get_spelling()) {
    case Attribute::e_statement_attribute_spelling_pragma_clang_loop:
      curprint("#pragma clang loop ");
      curprint(loop_option_name(attribute->get_loop_hint_option()));
      unparse_loop_value(attribute);
      break;
    case Attribute::e_statement_attribute_spelling_pragma_unroll:
      curprint("#pragma unroll");
      if (attribute->get_loop_hint_state() ==
          Attribute::e_loop_hint_state_numeric) {
        curprint(" ");
        unparse_argument(attribute->get_expression_argument());
      } else if (attribute->get_loop_hint_state() !=
                 Attribute::e_loop_hint_state_enable) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[statement-loop-hint]: pragma unroll "
                "has state=%d\n",
                static_cast<int>(attribute->get_loop_hint_state()));
        ROSE_ABORT();
      }
      break;
    case Attribute::e_statement_attribute_spelling_pragma_nounroll:
      if (attribute->get_loop_hint_state() !=
          Attribute::e_loop_hint_state_disable) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[statement-loop-hint]: pragma nounroll "
                "has state=%d\n",
                static_cast<int>(attribute->get_loop_hint_state()));
        ROSE_ABORT();
      }
      curprint("#pragma nounroll");
      break;
    case Attribute::e_statement_attribute_spelling_pragma_unroll_and_jam:
      curprint("#pragma unroll_and_jam");
      if (attribute->get_loop_hint_state() ==
          Attribute::e_loop_hint_state_numeric) {
        curprint(" ");
        unparse_argument(attribute->get_expression_argument());
      } else if (attribute->get_loop_hint_state() !=
                 Attribute::e_loop_hint_state_enable) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[statement-loop-hint]: pragma "
                "unroll_and_jam has state=%d\n",
                static_cast<int>(attribute->get_loop_hint_state()));
        ROSE_ABORT();
      }
      break;
    case Attribute::e_statement_attribute_spelling_pragma_nounroll_and_jam:
      if (attribute->get_loop_hint_state() !=
          Attribute::e_loop_hint_state_disable) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[statement-loop-hint]: pragma "
                "nounroll_and_jam has state=%d\n",
                static_cast<int>(attribute->get_loop_hint_state()));
        ROSE_ABORT();
      }
      curprint("#pragma nounroll_and_jam");
      break;
    default:
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[statement-loop-hint]: invalid spelling=%d\n",
          static_cast<int>(attribute->get_spelling()));
      ROSE_ABORT();
    }
    unp->cur.insert_newline(1, unp->cur.statement_indent());
  }

  auto attribute_name = [](Attribute::statement_attribute_kind_enum kind) {
    switch (kind) {
    case Attribute::e_statement_attribute_fallthrough:
      return "fallthrough";
    case Attribute::e_statement_attribute_likely:
      return "likely";
    case Attribute::e_statement_attribute_unlikely:
      return "unlikely";
    case Attribute::e_statement_attribute_assume:
      return "assume";
    case Attribute::e_statement_attribute_nomerge:
      return "nomerge";
    case Attribute::e_statement_attribute_musttail:
      return "musttail";
    case Attribute::e_statement_attribute_always_inline:
      return "always_inline";
    case Attribute::e_statement_attribute_opencl_unroll_hint:
      return "opencl_unroll_hint";
    default:
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[statement-attribute-kind]: invalid kind=%d\n",
          static_cast<int>(kind));
      ROSE_ABORT();
    }
  };

  for (const Attribute *attribute : attributes) {
    ASSERT_not_null(attribute);
    if (attribute->get_kind() == Attribute::e_statement_attribute_loop_hint) {
      continue;
    }

    const char *name = attribute_name(attribute->get_kind());
    const bool gnu_syntax = attribute->get_spelling() ==
                            Attribute::e_statement_attribute_spelling_gnu;
    if (gnu_syntax) {
      curprint("__attribute__((");
    } else {
      curprint("[[");
      switch (attribute->get_spelling()) {
      case Attribute::e_statement_attribute_spelling_cxx11_unscoped:
      case Attribute::e_statement_attribute_spelling_c23_unscoped:
        break;
      case Attribute::e_statement_attribute_spelling_cxx11_clang:
      case Attribute::e_statement_attribute_spelling_c23_clang:
        curprint("clang::");
        break;
      case Attribute::e_statement_attribute_spelling_cxx11_gnu:
      case Attribute::e_statement_attribute_spelling_c23_gnu:
        curprint("gnu::");
        break;
      default:
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[statement-attribute-spelling]: "
                "kind=%d spelling=%d is not direct statement syntax\n",
                static_cast<int>(attribute->get_kind()),
                static_cast<int>(attribute->get_spelling()));
        ROSE_ABORT();
      }
    }

    curprint(name);
    if (attribute->get_kind() == Attribute::e_statement_attribute_assume) {
      curprint("(");
      unparse_argument(attribute->get_expression_argument());
      curprint(")");
    } else if (attribute->get_kind() ==
                   Attribute::e_statement_attribute_opencl_unroll_hint &&
               attribute->get_integral_argument() != 0) {
      curprint("(");
      curprint(
          StringUtility::numberToString(attribute->get_integral_argument()));
      curprint(")");
    }
    curprint(gnu_syntax ? ")) " : "]] ");
  }

  SgUnparse_Info statement_info(info);
  statement_info.set_SkipFormatting();
  statement_info.set_template_argument_qualification_context(
      attributed->get_statement());
  unparseStatement(attributed->get_statement(), statement_info);
}

void Unparse_ExprStmt::unparseLabelStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgLabelStatement *label_stmt = isSgLabelStatement(stmt);
  ASSERT_not_null(label_stmt);

  curprint(string(label_stmt->get_label().str()) + ":");
  SgStatement *labeled_statement = label_stmt->get_statement();
  if (labeled_statement == nullptr) {
    std::cerr << "REX_UNPARSER_INVARIANT[label-statement-child]: label '"
              << label_stmt->get_label()
              << "' has no structurally owned statement" << std::endl;
    ROSE_ABORT();
  }
  if (labeled_statement->get_parent() != label_stmt) {
    std::cerr << "REX_UNPARSER_INVARIANT[label-statement-owner]: label '"
              << label_stmt->get_label()
              << "' does not own its labeled statement" << std::endl;
    ROSE_ABORT();
  }
  unparseStatement(labeled_statement, info);
}

void Unparse_ExprStmt::unparsePragmaAttribute(SgScopeStatement *scope_stmt) {
  ROSE_ASSERT(scope_stmt != NULL);
  if (scope_stmt->get_pragma() != NULL) {
    SgPragma *pragma = scope_stmt->get_pragma();
    string text_string = pragma->get_pragma();
    if (text_string.empty()) {
      std::cerr << "REX_UNPARSE_INVARIANT[pragma-source-text]: scope pragma "
                   "has an empty typed output payload"
                << std::endl;
      ROSE_ABORT();
    }
    if (unp->cur.get_compact_output()) {
      unp->cur.emit_compact_directive("#pragma " + text_string);
    } else {
      curprint("\n#pragma " + text_string + "\n");
    }
  }
}

void Unparse_ExprStmt::unparseWhileStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgWhileStmt *while_stmt = isSgWhileStmt(stmt);
  ASSERT_not_null(while_stmt);

  // DQ (12/17/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. curprint("while(");
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK2);
    // curprint("/* from AST SgWhileStmt */ while(");
    curprint("while (");
    // unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {
    // SgStatement* body = while_stmt->get_body();
    // curprint("/* partial token sequence SgWhileStmt */ ");
    SgStatement *condition = while_stmt->get_condition();
    unparseStatementFromTokenStream(stmt, condition, e_token_subsequence_start,
                                    e_leading_whitespace_start, info);
  }

  // DQ (10/19/2012): We now want to have more control over where ";" is output.
  // See test2012_47.c for an example of there this can't be explicitly handled
  // for all parts of a conditional. In this case we call unset_SkipSemiColon()
  // in SgClassDefinition so that they will be output properly there.
  // Build a specific SgUnparse_Info to support the conditional.
  // info.set_inConditional();
  // info.set_inConditional();
  // unparseStatement(while_stmt->get_condition(), info);
  // info.unset_inConditional();

  SgUnparse_Info ninfo(info);
  ninfo.set_inConditional();
  ninfo.set_SkipSemiColon();
  unparseStatement(while_stmt->get_condition(), ninfo);

  // curprint(")");
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint(")");
    unparsePragmaAttribute(while_stmt);

    if (while_stmt->get_body()) {
      SgUnparse_Info bodyInfo = nestedStatementInfo(info);
      unp->cur.format(while_stmt->get_body(), info,
                      FORMAT_BEFORE_NESTED_STATEMENT);
      unparseStatement(while_stmt->get_body(), bodyInfo);
      unp->cur.format(while_stmt->get_body(), info,
                      FORMAT_AFTER_NESTED_STATEMENT);
    } else {
      if (!info.SkipSemiColon()) {
        curprint(";");
      }
    }
  } else {
    SgStatement *condition = while_stmt->get_condition();
    SgStatement *body = while_stmt->get_body();

    ASSERT_not_null(condition);
    ASSERT_not_null(body);

    // unparseStatementFromTokenStream (condition, body,
    // e_trailing_whitespace_start, e_token_subsequence_start);
    // unparseStatementFromTokenStream (condition, body,
    // e_trailing_whitespace_start, e_leading_whitespace_start);
    unparseStatementFromTokenStream(condition, e_trailing_whitespace_start,
                                    e_trailing_whitespace_end, info);

    // Output syntax explicitly.
    curprint(")");

    SgUnparse_Info bodyInfo = nestedStatementInfo(info);
    unparseStatement(while_stmt->get_body(), bodyInfo);
  }
}

void Unparse_ExprStmt::unparseDoWhileStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgDoWhileStmt *dowhile_stmt = isSgDoWhileStmt(stmt);
  ASSERT_not_null(dowhile_stmt);

  curprint("do ");

  SgUnparse_Info bodyInfo = nestedStatementInfo(info);
  unp->cur.format(dowhile_stmt->get_body(), info,
                  FORMAT_BEFORE_NESTED_STATEMENT);
  unparseStatement(dowhile_stmt->get_body(), bodyInfo);
  unp->cur.format(dowhile_stmt->get_body(), info,
                  FORMAT_AFTER_NESTED_STATEMENT);

  // curprint( string("while " ) + "(");
  curprint("while (");

  SgUnparse_Info ninfo(info);
  ninfo.set_inConditional();

  // DQ (11/2/2015): Skip output of ";" in conditional.
  ninfo.set_SkipSemiColon();

  // we need to keep the properties of the prevnode (The next prevnode will set
  // the line back to where "do" was printed) SgLocatedNode* tempnode =
  // prevnode;

  unparseStatement(dowhile_stmt->get_condition(), ninfo);

  // Note that unseting this flag is not significant.
  ninfo.unset_inConditional();

  curprint(")");

  // DQ (9/24/2020): Adding support to unparse attached pragmas.
  unparsePragmaAttribute(dowhile_stmt);

  if (!info.SkipSemiColon()) {
    curprint(";");
  }
}

void Unparse_ExprStmt::unparseSwitchStmt(SgStatement *stmt,
                                         SgUnparse_Info &info) {
  SgSwitchStatement *switch_stmt = isSgSwitchStatement(stmt);

  ASSERT_not_null(switch_stmt);

  requireExactCxxStatementChild(switch_stmt, switch_stmt->get_item_selector(),
                                "switch statement", "selector");
  requireExactCxxStatementChild(switch_stmt, switch_stmt->get_body(),
                                "switch statement", "body");
  if (switch_stmt->get_item_selector() == switch_stmt->get_body()) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[cxx-statement-structure]: switch "
                    "statement aliases selector and body\n");
    ROSE_ABORT();
  }

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgStatement *switch_body = switch_stmt->get_body();
  const bool switchBodyRequiresStructuralUnparse =
      saved_unparsedPartiallyUsingTokenStream == true &&
      ((switch_stmt->isTransformation() ||
        switch_stmt->get_containsTransformation() ||
        switch_stmt->get_containsTransformationToSurroundingWhitespace()) ||
       (switch_body->isCompilerGenerated() || switch_body->isTransformation() ||
        switch_body->get_containsTransformation() ||
        switch_body->get_containsTransformationToSurroundingWhitespace()));
  if (switchBodyRequiresStructuralUnparse) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }

  // DQ (12/28/2014): Test for if we have unparsed partially using the token
  // stream.
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("switch (");
  } else {

    // SgStatement* item_selector = switch_stmt->get_item_selector();
    // unparseStatementFromTokenStream (stmt, item_selector,
    // e_token_subsequence_start, e_leading_whitespace_start, info);
    // unparseStatementFromTokenStream (stmt, item_selector,
    // e_token_subsequence_start, e_leading_whitespace_start, info);

    SgStatement *item_selector = switch_stmt->get_item_selector();
    // unparseStatementFromTokenStream (stmt, item_selector,
    // e_token_subsequence_start, e_trailing_whitespace_end, info);
    unparseStatementFromTokenStream(stmt, item_selector,
                                    e_token_subsequence_start,
                                    e_token_subsequence_end, info);

    // If there is whitespace here it will be output.
    unparseStatementFromTokenStream(item_selector, e_trailing_whitespace_start,
                                    e_trailing_whitespace_end, info);

    // DQ (6/4/2021): Note that we need to output a single token of syntax,
    // since there is no reference to this in the token stream mapping.
    curprint(")");

    // SgStatement* switch_body = switch_stmt->get_body();

    // DQ (6/4/2021): The leading whitespace will be output with the body.
    // unparseStatementFromTokenStream (stmt, switch_body,
    // e_token_subsequence_start, e_leading_whitespace_end, info);

    unparseStatement(switch_stmt->get_body(), info);
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    SgUnparse_Info ninfo(info);
    ninfo.set_SkipSemiColon();
    ninfo.set_inConditional();
    unparseStatement(switch_stmt->get_item_selector(), ninfo);

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      curprint(")");

      // DQ (9/23/2020): Adding support to unparse attached pragmas.
      unparsePragmaAttribute(switch_stmt);
    } else {
      SgStatement *item_selector = switch_stmt->get_item_selector();
      SgStatement *switch_body = switch_stmt->get_body();

      unparseStatementFromTokenStream(item_selector, switch_body,
                                      e_trailing_whitespace_start,
                                      e_leading_whitespace_start, info);
    }

    // DQ (11/5/2003): Support for skipping basic block added to support
    //                 prefix generation for AST Rewrite Mechanism
    // if(switch_stmt->get_body())
    if (!info.SkipBasicBlock()) {
      SgUnparse_Info bodyInfo = nestedStatementInfo(info);
      unparseStatement(switch_body, bodyInfo);
    }

    // DQ (6/4/2021): end of if (saved_unparsedPartiallyUsingTokenStream ==
    // false) (above)
  }
}

void Unparse_ExprStmt::unparseCaseStmt(SgStatement *stmt,
                                       SgUnparse_Info &info) {
  SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(stmt);
  ASSERT_not_null(case_stmt);

  requireExactCxxStatementChild(case_stmt, case_stmt->get_key(),
                                "case statement", "key");
  requireExactCxxStatementChild(case_stmt, case_stmt->get_body(),
                                "case statement", "body");
  requireExactOptionalCxxStatementChild(
      case_stmt, case_stmt->get_key_range_end(), "case statement", "range end");
  if (case_stmt->get_key_range_end() != nullptr &&
      case_stmt->get_key() == case_stmt->get_key_range_end()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[cxx-statement-structure]: case statement "
            "aliases key and range end\n");
    ROSE_ABORT();
  }

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgStatement *case_body = case_stmt->get_body();
  const bool caseHeaderRequiresStructuralUnparse =
      saved_unparsedPartiallyUsingTokenStream == true &&
      ((case_stmt->isTransformation() ||
        case_stmt->get_containsTransformation() ||
        case_stmt->get_containsTransformationToSurroundingWhitespace()) ||
       (case_body->isTransformation() ||
        case_body->get_containsTransformation() ||
        case_body->get_containsTransformationToSurroundingWhitespace()));
  if (caseHeaderRequiresStructuralUnparse) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }

  // DQ (12/28/2014): Test for if we have unparsed partially using the token
  // stream.
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("case ");

    unparseExpression(case_stmt->get_key(), info);

    // DQ (1/31/2014): Adding support for gnu case range extension.
    if (case_stmt->get_key_range_end() != NULL) {
      // Note that the spaces on each side of the "..." are required to avoid
      // interpretation of the case range as a floating point number by the gnu
      // parser.
      curprint(" ... ");
      unparseExpression(case_stmt->get_key_range_end(), info);
    }

    curprint(":");
  } else {
    requireExactSwitchLabelTokenBoundary(info.get_current_source_file(),
                                         case_stmt, case_body, "case");
    // The key expression has no statement mapping of its own. Split at the
    // exactly mapped body's core: the case node owns every header token
    // through the colon and any pragma immediately before the body, while the
    // body owns its first syntax token.
    unparseStatementFromTokenStream(
        case_stmt, case_body, e_token_subsequence_start,
        e_token_subsequence_start, info, false, 0, -1);
    statementsWithTokenEmittedLeadingPreprocessing.insert(case_body);
  }

  // DQ (1/3/2018): Put back this original behavior, because the case option
  // statment must be a compound statement (just like a label statement, see
  // test2017_20.c). DQ (12/20/2017): Comment this out to experiment with
  // alternative support for switch (part of new duff's device support). At the
  // very least, commenting this out permis the cases to be adjusted to have
  // defined bodies later (if that ultimately makes sense).
  // if(case_stmt->get_body())
  // if ( (case_stmt->get_body() != NULL) && !info.SkipBasicBlock())
  if (!info.SkipBasicBlock()) {
    SgUnparse_Info bodyInfo = nestedStatementInfo(info);
    unparseStatement(case_body, bodyInfo);
  }
}

void Unparse_ExprStmt::unparseTryStmt(SgStatement *stmt, SgUnparse_Info &info) {
  SgTryStmt *try_stmt = isSgTryStmt(stmt);
  ASSERT_not_null(try_stmt);

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgStatement *try_body = try_stmt->get_body();
  ASSERT_not_null(try_body);

  const bool tryBodyRequiresStructuralUnparse =
      saved_unparsedPartiallyUsingTokenStream == true &&
      (try_stmt->isTransformation() ||
       try_stmt->get_containsTransformationToSurroundingWhitespace());
  if (tryBodyRequiresStructuralUnparse) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("try ");
    unp->cur.format(try_body, info, FORMAT_BEFORE_NESTED_STATEMENT);
    unparseStatement(try_body, info);
    unp->cur.format(try_body, info, FORMAT_AFTER_NESTED_STATEMENT);
  } else {
    const bool inherited_partial_token_state =
        info.unparsedPartiallyUsingTokenStream();
    SgSourceFile *current_source_file = info.get_current_source_file();
    SgStatement *first_catch =
        try_stmt->get_catch_statement_seq().empty()
            ? nullptr
            : try_stmt->get_catch_statement_seq().front();
    const bool replayTryRegionFromTokens =
        current_source_file != nullptr && first_catch != nullptr &&
        try_stmt->isTransformation() == false &&
        try_body->isTransformation() == false &&
        try_body->get_containsTransformation() == false &&
        try_body->get_containsTransformationToSurroundingWhitespace() ==
            false &&
        canBeUnparsedFromTokenStream(current_source_file, try_stmt) &&
        canBeUnparsedFromTokenStream(current_source_file, first_catch);
    if (replayTryRegionFromTokens) {
      // Preserve the original try-body token sequence exactly and stop just
      // before the first catch block, which will be emitted separately.
      unparseStatementFromTokenStream(
          try_stmt, first_catch, e_token_subsequence_start,
          e_leading_whitespace_start, info, false, 0, -1);
    } else {
      unparseStatementFromTokenStream(
          try_stmt, try_body, e_token_subsequence_start,
          e_leading_whitespace_start, info, false, 0, -1);
    }

    if (replayTryRegionFromTokens == false) {
      SgUnparse_Info bodyInfo(info);
      restoreInheritedPartialTokenState(bodyInfo,
                                        inherited_partial_token_state);
      bodyInfo.unset_SkipSemiColon();
      const bool replayWholeBodyFromTokens =
          isSgBasicBlock(try_body) != nullptr &&
          try_body->isTransformation() == false &&
          try_body->get_containsTransformation() == false &&
          try_body->get_containsTransformationToSurroundingWhitespace() ==
              false &&
          current_source_file != nullptr &&
          canBeUnparsedFromTokenStream(current_source_file, try_body);
      if (replayWholeBodyFromTokens) {
        // The enclosing partial replay already emitted the "try" header. For
        // an untouched body block, replay the original "{...}" region
        // directly so inherited frontier state does not degrade its formatting
        // back to AST layout.
        unparseStatementFromTokenStream(try_body, e_leading_whitespace_start,
                                        e_token_subsequence_end, bodyInfo);
      } else {
        unparseStatement(try_body, bodyInfo);
      }
    }
  }

  SgStatementPtrList::iterator i = try_stmt->get_catch_statement_seq().begin();
  while (i != try_stmt->get_catch_statement_seq().end()) {
    unparseStatement(*i, info);
    i++;
  }
}

void Unparse_ExprStmt::unparseCatchStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgCatchOptionStmt *catch_statement = isSgCatchOptionStmt(stmt);
  ASSERT_not_null(catch_statement);

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgStatement *catch_body = catch_statement->get_body();
  ASSERT_not_null(catch_body);

  const bool catchHeaderRequiresStructuralUnparse =
      saved_unparsedPartiallyUsingTokenStream == true &&
      (catch_statement->isTransformation() ||
       catch_statement->get_containsTransformationToSurroundingWhitespace());
  if (catchHeaderRequiresStructuralUnparse) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("catch (");
    if (catch_statement->get_condition()) {
      SgUnparse_Info ninfo(info);
      ninfo.set_inVarDecl();
      // DQ (5/6/2004): this does not unparse correctly if the ";" is included
      ninfo.set_SkipSemiColon();
      ninfo.set_SkipClassSpecifier();
      unparseStatement(catch_statement->get_condition(), ninfo);
    } else {
      curprint("...");
    }

    curprint(")");
    // if (catch_statement->get_condition() == NULL) prevnode = catch_statement;

    unp->cur.format(catch_body, info, FORMAT_BEFORE_NESTED_STATEMENT);
    unparseStatement(catch_body, info);
    unp->cur.format(catch_body, info, FORMAT_AFTER_NESTED_STATEMENT);
  } else {
    const bool inherited_partial_token_state =
        info.unparsedPartiallyUsingTokenStream();
    SgSourceFile *current_source_file = info.get_current_source_file();
    const bool replayWholeCatchFromTokens =
        current_source_file != nullptr &&
        catch_statement->isTransformation() == false &&
        catch_body->isTransformation() == false &&
        canBeUnparsedFromTokenStream(current_source_file, catch_statement);
    if (replayWholeCatchFromTokens) {
      // The catch subtree is intact; preserve the original token spelling
      // instead of reconstructing the header/body split around the body block.
      unparseStatementFromTokenStream(catch_statement,
                                      e_token_subsequence_start,
                                      e_token_subsequence_end, info);
      return;
    }

    unparseStatementFromTokenStream(
        catch_statement, catch_body, e_token_subsequence_start,
        e_leading_whitespace_start, info, false, 0, -1);

    SgUnparse_Info bodyInfo(info);
    restoreInheritedPartialTokenState(bodyInfo, inherited_partial_token_state);
    bodyInfo.unset_SkipSemiColon();
    const bool replayWholeBodyFromTokens =
        isSgBasicBlock(catch_body) != nullptr &&
        catch_body->isTransformation() == false &&
        catch_body->get_containsTransformation() == false &&
        catch_body->get_containsTransformationToSurroundingWhitespace() ==
            false &&
        info.get_current_source_file() != nullptr &&
        canBeUnparsedFromTokenStream(info.get_current_source_file(),
                                     catch_body);
    if (replayWholeBodyFromTokens) {
      // The catch header was already replayed above; preserve the original body
      // block spelling when the block itself is untouched.
      unparseStatementFromTokenStream(catch_body, e_leading_whitespace_start,
                                      e_token_subsequence_end, bodyInfo);
    } else {
      unparseStatement(catch_body, bodyInfo);
    }
  }
}

void Unparse_ExprStmt::unparseDefaultStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgDefaultOptionStmt *default_stmt = isSgDefaultOptionStmt(stmt);
  ASSERT_not_null(default_stmt);

  requireExactCxxStatementChild(default_stmt, default_stmt->get_body(),
                                "default statement", "body");

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  SgStatement *default_body = default_stmt->get_body();
  const bool defaultHeaderRequiresStructuralUnparse =
      saved_unparsedPartiallyUsingTokenStream == true &&
      ((default_stmt->isTransformation() ||
        default_stmt->get_containsTransformation() ||
        default_stmt->get_containsTransformationToSurroundingWhitespace()) ||
       (default_body->isTransformation() ||
        default_body->get_containsTransformation() ||
        default_body->get_containsTransformationToSurroundingWhitespace()));
  if (defaultHeaderRequiresStructuralUnparse) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }

  // DQ (12/28/2014): Test for if we have unparsed partially using the token
  // stream.
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("default:");
  } else {
    requireExactSwitchLabelTokenBoundary(info.get_current_source_file(),
                                         default_stmt, default_body, "default");
    unparseStatementFromTokenStream(
        default_stmt, default_body, e_token_subsequence_start,
        e_token_subsequence_start, info, false, 0, -1);
    statementsWithTokenEmittedLeadingPreprocessing.insert(default_body);
  }

  // DQ (1/3/2018): Put back this original behavior, because the case option
  // statment must be a compound statement (just like a label statement, see
  // test2017_20.c). DQ (12/20/2017): Comment this out to experiment with
  // alternative support for switch (part of new duff's device support). At the
  // very least, commenting this out permis the cases to be adjusted to have
  // defined bodies later (if that ultimately makes sense).
  // if(default_stmt->get_body())
  if (!info.SkipBasicBlock()) {
    SgUnparse_Info bodyInfo = nestedStatementInfo(info);
    unparseStatement(default_body, bodyInfo);
  }
}

void Unparse_ExprStmt::unparseBreakStmt(SgStatement *stmt, SgUnparse_Info &) {
  SgBreakStmt *break_stmt = isSgBreakStmt(stmt);
  ASSERT_not_null(break_stmt);

  curprint("break;");
}

void Unparse_ExprStmt::unparseContinueStmt(SgStatement *stmt,
                                           SgUnparse_Info &) {
  SgContinueStmt *continue_stmt = isSgContinueStmt(stmt);
  ASSERT_not_null(continue_stmt);

  curprint("continue; ");
}

void Unparse_ExprStmt::unparseReturnStmt(SgStatement *stmt,
                                         SgUnparse_Info &info) {
  SgReturnStmt *return_stmt = isSgReturnStmt(stmt);
  ASSERT_not_null(return_stmt);

  const char *keyword = nullptr;
  switch (return_stmt->get_return_keyword_kind()) {
  case SgReturnStmt::e_return_keyword_return:
    keyword = "return";
    break;
  case SgReturnStmt::e_return_keyword_co_return:
    keyword = "co_return";
    break;
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[coroutine-return-kind]: return statement "
            "has invalid typed source kind=%d\n",
            static_cast<int>(return_stmt->get_return_keyword_kind()));
    ROSE_ABORT();
  }
  curprint(keyword);

  SgUnparse_Info ninfo(info);

  // DQ (6/4/2011): Set this in case the initializer is an expression that
  // requires name qualification (e.g. SgConstructorInitializer).  See
  // test2005_42.C for an example.
  // ninfo.set_reference_node_for_qualification(return_stmt);

  if (SgExpression *return_expr = return_stmt->get_expression();
      return_expr != nullptr && isSgNullExpression(return_expr) == nullptr) {
    curprint(" ");

    // DQ (2/8/2019): Restricting output of definitions in the return statement.
    ninfo.set_SkipDefinition();

    // DQ (2/8/2019): Double check that these are all set.
    ROSE_ASSERT(ninfo.SkipClassDefinition() == true);
    ROSE_ASSERT(ninfo.SkipEnumDefinition() == true);
    ROSE_ASSERT(ninfo.SkipDefinition() == true);

    unparseExpression(return_expr, ninfo);
  }

  if (!ninfo.SkipSemiColon()) {
    curprint(";");
  }
}

void Unparse_ExprStmt::unparseGotoStmt(SgStatement *stmt,
                                       SgUnparse_Info &info) {
  SgGotoStatement *goto_stmt = isSgGotoStatement(stmt);
  ASSERT_not_null(goto_stmt);

  if (goto_stmt->get_label() != NULL) {
    // DQ (11/22/2017): Original code.
    curprint(string("goto ") + goto_stmt->get_label()->get_label().str());

  } else {
    // DQ (11/22/2017): Added suport for GNU extension for computed goto.
    curprint("goto *");
    SgExpression *selector_expression = goto_stmt->get_selector_expression();
    ASSERT_not_null(selector_expression);

    SgUnparse_Info ninfo(info);
    unparseExpression(selector_expression, ninfo);
  }

  if (!info.SkipSemiColon()) {
    curprint(string(";"));
  }
}

static bool isOutputAsmOperand(SgAsmOp *asmOp) {
  // There are two way of evaluating if an SgAsmOp is an output operand,
  // depending of if we are using the specific mechanism that knows
  // records register details or the more general mechanism that records
  // the registers as strings.  The string based mechanism lack precision
  // and would require parsing to retrive the instruction details, but it
  // is instruction set independent.  The more precise mechanism records
  // the specific register codes and could in the future be interpreted
  // by tooling that understands ISA-specific register details.

  return (asmOp->get_recordRawAsmOperandDescriptions() == true)
             ? (asmOp->get_isOutputOperand() == true)
             : (asmOp->get_modifiers() & SgAsmOp::e_output);
}

static std::string asm_escapeString(const std::string &s) {
  // DQ (2/4/2014): We need a special version of this function for unparsing the
  // asm strings. The version of escapeString in util will expand '\' to be '\\'
  // and this should not be done to the "\n" and "\t" substrings.

  std::string result;
  for (size_t i = 0; i < s.length(); ++i) {
    switch (s[i]) {
    case '"':
      result += "\\\"";
      break;
    case '\a':
      result += "\\a";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\v':
      result += "\\v";
      break;
    default:
      if (isprint(s[i])) {
        result.push_back(s[i]);
      } else {
        std::ostringstream stream;
        stream << '\\';
        stream << std::setw(3) << std::setfill('0') << std::oct
               << (unsigned)(unsigned char)(s[i]);
        result += stream.str();
      }
      break;
    }
  }

  return result;
}

void Unparse_ExprStmt::unparseAsmStmt(SgStatement *stmt, SgUnparse_Info &info) {
  // This function is called as part of the handling of the C "asm"
  // statement.  The "asm" statement supports inline assembly in C.
  // These sorts of statements are not common in most user code
  // (except embedded code), but are common in many system header files.

  SgAsmStmt *asm_stmt = isSgAsmStmt(stmt);
  ASSERT_not_null(asm_stmt);

#define ASM_DEBUGGING 0

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): stmt = %p = %s \n", stmt,
         stmt->class_name().c_str());
#endif

  // REX targets Clang's GNU-compatible C/C++ frontend. The underscored spelling
  // is accepted in both language modes and does not depend on the host build
  // compiler.
  curprint("__asm__ ");

  curprint("(");

  // DQ (7/22/2006): This IR node has been changed to have a list of SgAsmOp IR
  // nodes unparseExpression(asm_stmt->get_expr(), info);

  // printf ("unparsing asm statement = %ld
  // \n",asm_stmt->get_operands().size()); Process the asm template (always the
  // first operand)
  string asmTemplate = asm_stmt->get_assemblyCode();

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): asmTemplate.length()      = %" PRIuPTR " \n",
         (size_t)asmTemplate.length());
#endif

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): asmTemplate               = %s \n",
         asmTemplate.c_str());
  printf("In unparseAsmStmt(): escapeString(asmTemplate) = %s \n",
         asm_escapeString(asmTemplate).c_str());
#endif

  // DQ (2/4/2014): We don't want to escape this string (see test2014_83.c,
  // test2014_84.c, and test2014_85.c).
  curprint("\"" + asm_escapeString(asmTemplate) + "\"");
  // curprint("\"" + asmTemplate + "\"");

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): asm_stmt->get_useGnuExtendedFormat() = %s \n",
         asm_stmt->get_useGnuExtendedFormat() ? "true" : "false");
#endif

  if (asm_stmt->get_useGnuExtendedFormat()) {
    size_t numOutputOperands = 0;
    size_t numInputOperands = 0;

    // Count the number of input vs. output operands
    for (SgExpressionPtrList::const_iterator i =
             asm_stmt->get_operands().begin();
         i != asm_stmt->get_operands().end(); ++i) {
      SgAsmOp *asmOp = isSgAsmOp(*i);
      ROSE_ASSERT(asmOp);
#if ASM_DEBUGGING
      printf("asmOp->get_modifiers() = %d SgAsmOp::e_output = %d "
             "asmOp->get_isOutputOperand() = %s \n",
             (int)asmOp->get_modifiers(), (int)SgAsmOp::e_output,
             asmOp->get_isOutputOperand() ? "true" : "false");
      printf("asmOp->get_recordRawAsmOperandDescriptions() = %s \n",
             asmOp->get_recordRawAsmOperandDescriptions() ? "true" : "false");
#endif
      // if (asmOp->get_modifiers() & SgAsmOp::e_output)
      // if ( (asmOp->get_modifiers() & SgAsmOp::e_output) ||
      // (asmOp->get_isOutputOperand() == true) )
      if (isOutputAsmOperand(asmOp) == true) {
        ++numOutputOperands;
#if ASM_DEBUGGING
        printf("Marking as an output operand! \n");
#endif
      } else {
        ++numInputOperands;
#if ASM_DEBUGGING
        printf("Marking as an input operand! \n");
#endif
      }
    }

    size_t numClobbers = asm_stmt->get_clobberRegisterList().size();

#if ASM_DEBUGGING
    printf("In unparseAsmStmt(): numClobbers = %" PRIuPTR " \n", numClobbers);
#endif

#if ASM_DEBUGGING
    printf("In unparseAsmStmt(): numOutputOperands = %" PRIuPTR
           " numInputOperands = %" PRIuPTR " numClobbers = %" PRIuPTR " \n",
           numOutputOperands, numInputOperands, numClobbers);
#endif

    // DQ (2/4/2014): Adding initializer (to make me feel better about this
    // code).
    bool first = false;
    if (numInputOperands == 0 && numOutputOperands == 0 && numClobbers == 0) {
#if ASM_DEBUGGING
      printf("In unparseAsmStmt(): (numInputOperands == 0 && numOutputOperands "
             "== 0 && numClobbers == 0): goto donePrintingConstraints \n");
#endif
      // DQ (9/14/2013): Output required if we branch to label (see
      // test2013_72.c).
      curprint(" :: "); // Start of output operands

      goto donePrintingConstraints;
    }
    curprint(" : "); // Start of output operands

#if ASM_DEBUGGING
    curprint(" /* asm output operands */ "); // Debugging output
#endif

    // Record if this is the first operand so that we can surpress the ","
    first = true;
    for (SgExpressionPtrList::const_iterator i =
             asm_stmt->get_operands().begin();
         i != asm_stmt->get_operands().end(); ++i) {
      SgAsmOp *asmOp = isSgAsmOp(*i);
      ASSERT_not_null(asmOp);
      // if (asmOp->get_modifiers() & SgAsmOp::e_output)
      // if ( (asmOp->get_modifiers() & SgAsmOp::e_output) ||
      // (asmOp->get_isOutputOperand() == true) )
      if (isOutputAsmOperand(asmOp) == true) {
        if (!first)
          curprint(", ");
        first = false;
        unparseExpression(asmOp, info);
      }
    }

    if (numInputOperands == 0 && numClobbers == 0) {
#if ASM_DEBUGGING
      printf("In unparseAsmStmt(): (numInputOperands == 0 && numClobbers == "
             "0): goto donePrintingConstraints \n");
#endif
      // DQ (9/14/2013): Output required if we branch to label (see
      // test2013_72.c, but this is not a good example). curprint(" : "); //
      // Start of output operands

      goto donePrintingConstraints;
    }
    curprint(" : "); // Start of input operands
#if ASM_DEBUGGING
    curprint(" /* asm input operands */ "); // Debugging output
#endif
    first = true;
    for (SgExpressionPtrList::const_iterator i =
             asm_stmt->get_operands().begin();
         i != asm_stmt->get_operands().end(); ++i) {
      SgAsmOp *asmOp = isSgAsmOp(*i);
      ASSERT_not_null(asmOp);
      // if (!(asmOp->get_modifiers() & SgAsmOp::e_output))
      if (isOutputAsmOperand(asmOp) == false) {
        if (!first)
          curprint(", ");
        first = false;
        unparseExpression(asmOp, info);
      }
    }

    if (numClobbers == 0) {
#if ASM_DEBUGGING
      printf("In unparseAsmStmt(): (numClobbers == 0): goto "
             "donePrintingConstraints \n");
#endif
      // DQ (9/14/2013): Output required if we branch to label (see
      // test2013_72.c, but this is not a good example). curprint(" : "); //
      // Start of output operands

      goto donePrintingConstraints;
    }

    curprint(" : "); // Start of clobbers

#if ASM_DEBUGGING
    curprint(" /* asm clobbers */ "); // Debugging output
#endif
    first = true;
    for (SgAsmStmt::AsmRegisterNameList::const_iterator i =
             asm_stmt->get_clobberRegisterList().begin();
         i != asm_stmt->get_clobberRegisterList().end(); ++i) {
      if (!first)
        curprint(", ");
      first = false;
      curprint("\"" + unparse_asm_clobber_name(*i) + "\"");
    }

  donePrintingConstraints: {}

#if ASM_DEBUGGING
    printf("In unparseAsmStmt(): base of conditional block: "
           "asm_stmt->get_useGnuExtendedFormat() = %s \n",
           asm_stmt->get_useGnuExtendedFormat() ? "true" : "false");
#endif
  }

  curprint(string(")"));

  if (!info.SkipSemiColon()) {
    curprint(string(";"));
  }

#if ASM_DEBUGGING
  printf("Leaving unparseAsmStmt(): stmt = %p = %s \n", stmt,
         stmt->class_name().c_str());
#endif
}

// DQ 11/3/2014): Adding C++11 templated typedef declaration support.
void Unparse_ExprStmt::unparseTemplateTypedefDeclaration(SgStatement *stmt,
                                                         SgUnparse_Info &info) {
  SgTemplateTypedefDeclaration *templateTypedef_stmt =
      isSgTemplateTypedefDeclaration(stmt);
  ASSERT_not_null(templateTypedef_stmt);

#define DEBUG_TEMPLATE_TYPEDEF 0

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt() = %p \n", templateTypedef_stmt);
#endif
#if DEBUG_TEMPLATE_TYPEDEF
  curprint(" /* Calling "
           "unparseTemplateDeclarationStatment_support<"
           "SgTemplateTypedefDeclaration>() */ ");
#endif

  // DQ (2/19/2019): The support for unparsing a SgTemplateTypedefDeclaration is
  // different enough that we always use the specialized path here.

  // Unparse_ExprStmt::unparseTemplateParameterList( const
  // SgTemplateParameterPtrList & templateParameterList, SgUnparse_Info& info,
  // bool is_template_header) bool is_template_header = false;
  // unparseTemplateParameterList(templateTypedef_stmt->get_templateParameters(),info,is_template_header);
  unparseTemplateHeader<SgTemplateTypedefDeclaration>(templateTypedef_stmt,
                                                      info);

  // DQ (2/19/2019): Not clear that I want the extra "\n".
  curprint("\nusing ");

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): templateTypedef_stmt->get_name() = "
         "%s \n",
         templateTypedef_stmt->get_name().str());
#endif

  curprint(templateTypedef_stmt->get_name().str());

  curprint(" = ");

  SgType *base_type = templateTypedef_stmt->get_base_type();
  ASSERT_not_null(base_type);

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): base_type = %p = %s \n", base_type,
         base_type->class_name().c_str());
#endif

  SgUnparse_Info ninfo(info);

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): "
         "templateTypedef_stmt->get_declaration() = %p \n",
         templateTypedef_stmt->get_declaration());
#endif
#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): set "
         "reference_node_for_qualification: templateTypedef_stmt = %p = %s \n",
         templateTypedef_stmt, templateTypedef_stmt->class_name().c_str());
#endif
  ninfo.set_reference_node_for_qualification(templateTypedef_stmt);

  // DQ (2/19/2019): Cxx_tests/test2019_153.C demonstrates that a class
  // declaration can be define in the C++11 SgTemplateTypedefDeclaration.
  // ROSE_ASSERT(templateTypedef_stmt->get_declaration() == NULL);

  if (templateTypedef_stmt->get_declaration() == NULL) {
    ninfo.set_SkipClassDefinition();
    ninfo.set_SkipEnumDefinition();
  }

  unp->u_type->unparseType(base_type, ninfo);

  curprint(";");

#if DEBUG_TEMPLATE_TYPEDEF
  printf("Leaving unparseTemplateTypeDefStmt() = %p \n", templateTypedef_stmt);
#endif
}

void Unparse_ExprStmt::unparseNonrealDecl(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgNonrealDecl *nrdecl = isSgNonrealDecl(stmt);
  ASSERT_not_null(nrdecl);

  if (nrdecl->get_is_concept()) {
    const SgTemplateParameterPtrList &params = nrdecl->get_tpl_params();
    if (!params.empty()) {
      curprint("template ");
      SgUnparse_Info tinfo(info);
      tinfo.set_declstatement_ptr(NULL);
      tinfo.set_declstatement_ptr(nrdecl);
      Unparse_ExprStmt::unparseTemplateParameterList(params, tinfo, true);
      curprint("\n");
    }

    curprint("concept ");
    curprint(nrdecl->get_name().str());

    curprint(" = ");
    SgExpression *constraint = nrdecl->get_conceptConstraint();
    if (constraint == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[concept-constraint]: concept name=%s "
              "has no exact constraint expression\n",
              nrdecl->get_name().str());
      ROSE_ABORT();
    }
    SgUnparse_Info constraint_info(info);
    constraint_info.set_SkipClassDefinition();
    constraint_info.set_SkipEnumDefinition();
    unparseExpression(constraint, constraint_info);

    if (!info.SkipSemiColon()) {
      curprint(";");
    }
    return;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[nonreal-declaration]: semantic placeholder "
          "%p (%s) reached statement emission\n",
          nrdecl, nrdecl->get_name().str());
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseTypeDefStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgTypedefDeclaration *typedef_stmt = isSgTypedefDeclaration(stmt);
  ASSERT_not_null(typedef_stmt);
  if (typedef_stmt->get_typedef_type() != SgTypedefDeclaration::e_typedef &&
      typedef_stmt->get_typedef_type() != SgTypedefDeclaration::e_using) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[typedef-source-form]: name=%s form=%d has "
            "no exact typedef/using source form\n",
            typedef_stmt->get_name().str(),
            static_cast<int>(typedef_stmt->get_typedef_type()));
    ROSE_ABORT();
  }

#define DEBUG_TYPEDEF_DECLARATIONS 0

#if DEBUG_TYPEDEF_DECLARATIONS
  printf("TOP of unp->u_type->unparseTypeDefStmt() = %p \n", typedef_stmt);
  typedef_stmt->get_file_info()->display(
      "In unp->u_type->unparseTypeDefStmt(): debug");
#endif
#if DEBUG_TYPEDEF_DECLARATIONS
  curprint("\n /* In unp->u_type->unparseTypeDefStmt() */ \n");
#endif

  // DQ (10/5/2004): This is the explicitly set boolean value which indicates
  // that a class declaration is buried inside the current variable declaration
  // (e.g. struct A { int x; } a;).  In this case we have to output the base
  // type with its definition.
  bool outputTypeDefinition =
      typedef_stmt->get_typedefBaseTypeContainsDefiningDeclaration();
  if (outputTypeDefinition == true &&
      (info.SkipBaseType() || info.SkipClassDefinition() == true ||
       info.SkipEnumDefinition() == true)) {
    outputTypeDefinition = false;
  }

#if DEBUG_TYPEDEF_DECLARATIONS
  printf(
      "In unparseTypeDefStmt(): typedef_stmt = %p outputTypeDefinition = %s \n",
      typedef_stmt, (outputTypeDefinition == true) ? "true" : "false");
#endif

  if (!info.inEmbeddedDecl() && !info.SkipBaseType()) {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || 0
    curprint("\n/* NOT an embeddedDeclaration */ \n");
#endif
    SgClassDefinition *cdefn =
        isSgClassDefinition(cxxLexicalDeclarationParent(typedef_stmt));
    if (cdefn && cdefn->get_declaration()->get_class_type() ==
                     SgClassDeclaration::e_class)
      info.set_CheckAccess();
    // printDebugInfo("entering unp->u_sage->printSpecifier", true);
    unp->u_sage->printSpecifier(typedef_stmt, info);
    info.unset_CheckAccess();
  } else {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || 0
    curprint("\n/* Found an embeddedDeclaration */ \n");
#endif
  }

  SgUnparse_Info ninfo(info);
  auto typedef_definition_requires_name_qualification =
      [&](SgTypedefDeclaration *typedef_decl) -> bool {
    if (typedef_decl == NULL) {
      return false;
    }

    SgDeclarationStatement *declaration = typedef_decl->get_declaration();
    if (declaration == NULL) {
      return false;
    }

    SgScopeStatement *decl_scope = declaration->get_scope();
    if (decl_scope == NULL) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[typedef-embedded-tag-scope]: "
              "typedef=%p embedded declaration=%p/%s has no exact semantic "
              "scope\n",
              static_cast<void *>(typedef_decl),
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }

    if (declaration->get_parent() == typedef_decl) {
      if (!typedef_decl->get_typedefBaseTypeContainsDefiningDeclaration() ||
          typedef_decl->get_declaration() != declaration ||
          typedef_decl->get_scope() == NULL) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[typedef-embedded-tag-owner]: "
                "typedef=%p declaration=%p does not form one exact embedded "
                "definition transaction\n",
                static_cast<void *>(typedef_decl),
                static_cast<void *>(declaration));
        ROSE_ABORT();
      }

      // The typedef owns the lexical definition surface, while a qualified
      // tag definition can deliberately publish its semantic identity in a
      // different scope (for example, `typedef enum class N::color ...`).
      // Preserve qualification exactly when those two typed scopes differ.
      return typedef_decl->get_scope() != decl_scope &&
             !SgScopeStatement::isEquivalentScope(typedef_decl->get_scope(),
                                                  decl_scope);
    }

    SgScopeStatement *parent_scope =
        isSgScopeStatement(declaration->get_parent());
    if (parent_scope == NULL) {
      return true;
    }

    return parent_scope != decl_scope &&
           !SgScopeStatement::isEquivalentScope(parent_scope, decl_scope);
  };

  // DQ (10/10/2006): Do output any qualified names (particularly for
  // non-defining declarations). ninfo.set_forceQualifiedNames();

  // DQ (10/5/2004): This controls the unparsing of the class definition
  // when unparsing the type within this variable declaration.
  if (outputTypeDefinition == true) {
    // printf ("Output the full definition as a basis for the typedef base type
    // \n"); DQ (10/5/2004): If this is a defining declaration then make sure
    // that we don't skip the definition
    ROSE_ASSERT(ninfo.SkipClassDefinition() == false);

    // DQ (12/22/2005): Enum definition should be handled here as well
    ROSE_ASSERT(ninfo.SkipEnumDefinition() == false);

    // DQ (10/14/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    if (!typedef_definition_requires_name_qualification(typedef_stmt)) {
      ninfo.set_SkipQualifiedNames();
    }
    // curprint ( string("\n/* Case of typedefs for outputTypeDefinition == true
    // */\n ";
  } else {
    // printf ("Skip output of the full definition as a basis for the typedef
    // base type \n"); DQ (10/5/2004): If this is a non-defining declaration
    // then skip the definition
    ninfo.set_SkipClassDefinition();
    ROSE_ASSERT(ninfo.SkipClassDefinition() == true);

    // DQ (12/22/2005): Enum definition should be handled here as well
    ninfo.set_SkipEnumDefinition();
    ROSE_ASSERT(ninfo.SkipEnumDefinition() == true);

    // DQ (10/14/2006): Force output any qualified names (particularly for
    // non-defining declarations). This is a special case for types of variable
    // declarations. ninfo.set_forceQualifiedNames(); curprint ( string("\n/*
    // Case of typedefs, should we forceQualifiedNames -- outputTypeDefinition
    // == false  */\n ";
  }

  if (typedef_stmt->get_typedef_type() == SgTypedefDeclaration::e_using) {
    SgType *btype = typedef_stmt->get_base_type();
    ASSERT_not_null(btype);

    curprint("using ");
    curprint(typedef_stmt->get_name().str());
    curprint(" = ");

    SgUnparse_Info ninfo_for_type(ninfo);
    ninfo_for_type.set_declstatement_ptr(typedef_stmt);
    if (typedef_stmt->get_requiresGlobalNameQualificationOnType() == true) {
      ninfo_for_type.set_requiresGlobalNameQualification();
    }

    if (outputTypeDefinition) {
      ninfo_for_type.set_isTypeFirstPart();
    }
    // The alias declaration is the exact source use of its base type.  An
    // auxiliary class/enum declaration describes semantic type identity, but
    // owns no independent emitted use site and therefore cannot be used to
    // retrieve the alias's contextual qualification.
    ninfo_for_type.set_reference_node_for_qualification(typedef_stmt);

    const NameQualificationResult baseTypeQualification =
        exactStatementTypeQualification(unp, typedef_stmt, info);
    ninfo_for_type.set_name_qualification_length(baseTypeQualification.length);
    ninfo_for_type.set_global_qualification_required(
        baseTypeQualification.global);
    ninfo_for_type.set_type_elaboration_required(
        baseTypeQualification.typeElaboration);
    if (!outputTypeDefinition && !baseTypeQualification.typeElaboration) {
      ninfo_for_type.set_SkipClassSpecifier();
    }

    unp->u_type->unparseType(btype, ninfo_for_type);

    if (outputTypeDefinition == true) {
      unparseTypeAttributes(typedef_stmt);
    }

    unp->u_sage->printAttributes(typedef_stmt, info);

    if (!info.SkipSemiColon()) {
      curprint(";");
    }

    return;
  }

  // A typed declaration group owns the shared declaration specifier.  A plain
  // continuation alias has no declarator punctuation of its own, so emitting
  // its complete base type would incorrectly repeat a named class/enum (for
  // example, `typedef struct A {} X, struct A Y`).  Pointer, array, and
  // function aliases still use the normal two-part type path below because that
  // path owns their declarator punctuation.
  if (info.SkipBaseType() &&
      !cxxSourceGroupTypeUsesDeclaratorSyntax(typedef_stmt->get_base_type())) {
    SgDeclarationGroupStatement *group =
        isSgDeclarationGroupStatement(typedef_stmt->get_parent());
    if (group == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[typedef-group-continuation]: typedef=%p "
              "suppresses its base type without a typed group owner\n",
              static_cast<void *>(typedef_stmt));
      ROSE_ABORT();
    }
    group->validate();
    if (std::count(group->get_declarations().begin(),
                   group->get_declarations().end(), typedef_stmt) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[typedef-group-continuation]: typedef=%p "
              "is not an exact member of group=%p\n",
              static_cast<void *>(typedef_stmt), static_cast<void *>(group));
      ROSE_ABORT();
    }
    curprint(typedef_stmt->get_name().str());
    unp->u_sage->printAttributes(typedef_stmt, info);
    if (!info.SkipSemiColon()) {
      curprint(";");
    }
    return;
  }

  // Note that typedefs of function pointers and member function pointers
  // are quite different from ordinary typedefs and so should be handled
  // separately.

  // First look for a pointer to a function
  SgPointerType *pointerToType = isSgPointerType(typedef_stmt->get_base_type());

  SgFunctionType *functionType = NULL;
  if (pointerToType != NULL)
    functionType = isSgFunctionType(pointerToType->get_base_type());

  // DQ (2/3/2019): Adding support to unparse SgPointerMemberType with
  // parenthesis now that they have been discovered to not be required in the
  // unparse type code for SgPointerMemberType.  See Cxx11_tests/test2019_77.C.
  SgPointerMemberType *pointerToMemberType =
      isSgPointerMemberType(typedef_stmt->get_base_type());

  // DQ (9/15/2004): Added to support typedefs of member pointers
  SgMemberFunctionType *pointerToMemberFunctionType =
      isSgMemberFunctionType(typedef_stmt->get_base_type());

  // DQ (2/14/2019): Adding name qualification support for C++11 enum
  // declarations in typedef types.
  SgEnumType *enumType = isSgEnumType(typedef_stmt->get_base_type());
  if (enumType != NULL) {
    SgDeclarationStatement *declarationReference =
        typedef_stmt->get_declaration();
    ASSERT_not_null(declarationReference);
    SgEnumDeclaration *enumDeclaration =
        isSgEnumDeclaration(declarationReference);
    ASSERT_not_null(enumDeclaration);
    SgEnumDeclaration *definingEnumDeclaration =
        isSgEnumDeclaration(enumDeclaration->get_definingDeclaration());

#if DEBUG_TYPEDEF_DECLARATIONS
    printf("enumDeclaration->get_name() = %s \n",
           enumDeclaration->get_name().str());
#endif
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint(string("\n/* In unparseTypeDefStmt: enum name                     "
                    "             = ") +
             enumDeclaration->get_name().str() + " */ \n ");
    curprint(string("\n/* In unparseTypeDefStmt: enumDeclaration != "
                    "definingEnumDeclaration = ") +
             ((enumDeclaration != definingEnumDeclaration) ? "true" : "false") +
             " */ \n ");
#endif
    // DQ (2/20/2019): If this is not the defining declaration referenced
    // through the declarationReference then use the usual method for name
    // qualification via the base type.
    if (enumDeclaration != definingEnumDeclaration) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("internal declarationReference != definingEnumDeclaration: so use "
             "the name qualification via the base type \n");
#endif
      enumType = NULL;
    } else {
      // If this is the defining declaration, then if it is an anonymous class
      // then output as a type.
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("enumDeclaration->get_name() = %s \n",
             enumDeclaration->get_name().str());
#endif
      const bool isAnonymousName = enumDeclaration->get_isUnNamed();
      if (isAnonymousName == true) {
#if DEBUG_TYPEDEF_DECLARATIONS
        printf("internal declarationReference == definingEnumDeclaration: "
               "isAnonymousName == true: so output enum declaration via the "
               "base type \n");
#endif
        enumType = NULL;
      }
    }
  }

  // DQ (2/18/2019): Adding name qualification support for class declarations in
  // typedef types (declared in other scopes).
  SgClassType *classType = isSgClassType(typedef_stmt->get_base_type());
  if (classType != NULL) {
    SgDeclarationStatement *declarationReference =
        typedef_stmt->get_declaration();
    ASSERT_not_null(declarationReference);
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(declarationReference);
    ASSERT_not_null(classDeclaration);
    SgClassDeclaration *definingClassDeclaration =
        isSgClassDeclaration(classDeclaration->get_definingDeclaration());

#if DEBUG_TYPEDEF_DECLARATIONS
    printf("classDeclaration->get_name() = %s \n",
           classDeclaration->get_name().str());
#endif
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint(string("\n/* In unparseTypeDefStmt: class name                    "
                    "               = ") +
             classDeclaration->get_name().str() + " */ \n ");
    curprint(
        string("\n/* In unparseTypeDefStmt: classDeclaration != "
               "definingClassDeclaration = ") +
        ((classDeclaration != definingClassDeclaration) ? "true" : "false") +
        " */ \n ");
#endif
    // DQ (2/20/2019): If this is not the defining declaration referenced
    // through the declarationReference then use the usual method for name
    // qualification via the base type.
    if (classDeclaration != definingClassDeclaration) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("internal declarationReference != definingClassDeclaration: so "
             "use the name qualification via the base type \n");
#endif
      classType = NULL;
    } else {
      // If this is the defining declaration, then if it is an anonymous class
      // then output as a type.
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("classDeclaration->get_name() = %s \n",
             classDeclaration->get_name().str());
#endif
      const bool isAnonymousName = classDeclaration->get_isUnNamed();
      if (isAnonymousName == true) {
#if DEBUG_TYPEDEF_DECLARATIONS
        printf("internal declarationReference == definingClassDeclaration: "
               "isAnonymousName == true: so output class declaration via the "
               "base type \n");
#endif
        classType = NULL;
      }
    }
  }

#if DEBUG_TYPEDEF_DECLARATIONS
  printf("In unp->u_type->unparseTypedef: functionType                = %p \n",
         functionType);
  printf("In unp->u_type->unparseTypedef: pointerToMemberType         = %p \n",
         pointerToMemberType);
  printf("In unp->u_type->unparseTypedef: pointerToMemberFunctionType = %p \n",
         pointerToMemberFunctionType);
  printf("In unp->u_type->unparseTypedef: enumType                    = %p \n",
         enumType);
  printf("In unp->u_type->unparseTypedef: classType                   = %p \n",
         classType);
#endif

  // DQ (9/22/2004): It is not clear why we need to handle this case with
  // special code. We are only putting out the return type of the function type
  // (for functions or member functions). It seems that the reason we handle
  // function pointers separately is that typedefs of non function pointers
  // could include the complexity of class declarations with definitions and
  // separating the code for function pointers allows for easier debugging. When
  // typedefs of defining class declarations is fixed we might be able to unify
  // these separate cases.

  // DQ (2/14/2019): Adding name qualification support for C++11 enum
  // declarations in typedef types. DQ (2/3/2019): See if this is a better
  // branch for handling the SgPointerMemberType. This handles pointers to
  // functions and member function (but not pointers to members!) if (
  // (functionType != NULL) || (pointerToMemberType != NULL) ) if (
  // (functionType != NULL) || (pointerToMemberFunctionType != NULL) ) if (
  // (functionType != NULL) || (pointerToMemberFunctionType != NULL) ||
  // (pointerToMemberType != NULL) ) if ( (functionType != NULL) ||
  // (pointerToMemberFunctionType != NULL) || (enumType != NULL) )

  // DQ (2/20/2019): Removing the use of classType allows the name qualification
  // to be handled properly thorugh the type. But I need to check why this was
  // added in the first place (what breaks). if ( (functionType != NULL) ||
  // (pointerToMemberFunctionType != NULL) || (enumType != NULL) || (classType
  // != NULL)) if ( (functionType != NULL) || (pointerToMemberFunctionType !=
  // NULL) || (enumType != NULL) ) if ( (functionType != NULL) ||
  // (pointerToMemberFunctionType != NULL) || (enumType != NULL) || (classType
  // != NULL)) if ( (functionType != NULL) || (pointerToMemberFunctionType !=
  // NULL) || (enumType != NULL) )
  if ((functionType != NULL) || (pointerToMemberFunctionType != NULL) ||
      (enumType != NULL) || (classType != NULL)) {
    // Newly implemented case of typedefs for function and member function
    // pointers
#if DEBUG_TYPEDEF_DECLARATIONS
    printf("In unparseTypeDefStmt(): case of typedefs for function and member "
           "function pointers \n");
#endif
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || 0
    curprint("\n/* Case of typedefs for function and member function pointers "
             "*/ \n");
#endif
    ninfo.set_SkipFunctionQualifier();
    if (!ninfo.SkipBaseType()) {
      curprint("typedef ");
    }

    // Specify that only the first part of the type shold be unparsed
    // (this will permit the introduction of the name into the member
    // function pointer declaration)
    ninfo.set_isTypeFirstPart();

#if OUTPUT_DEBUGGING_UNPARSE_INFO || 0
    curprint(string("\n/* ") +
             ninfo.displayString("After return Type now output the base type "
                                 "(first part then second part)") +
             " */ \n");
#endif

    // The base type contains the function po9inter type
    SgType *btype = typedef_stmt->get_base_type();

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (first part) (for functionType or "
             "pointerToMemberFunctionType) */ \n");
#endif

#if DEBUG_TYPEDEF_DECLARATIONS
    printf("In unparseTypeDefStmt(): btype = %p = %s \n", btype,
           btype->class_name().c_str());
#endif

    // DQ (1/10/2007): Set the current declaration statement so that if required
    // we can do context dependent searches of the AST to determine if name
    // qualification is required. This is done now for the case of function and
    // member function typedefs.
    SgUnparse_Info ninfo_for_type(ninfo);
    ninfo_for_type.set_declstatement_ptr(typedef_stmt);
    ninfo_for_type.set_type_elaboration_required(
        typedef_stmt->get_type_elaboration_required_for_base_type());

    // DQ (2/18/2019): The name qualification support sets the name
    // qualification for enums declarations using the enum declaration, so the
    // typedef declaration is not appropriate to use with
    // set_reference_node_for_qualification().
    SgDeclarationStatement *declaration = typedef_stmt->get_declaration();
    if (declaration != NULL &&
        ((isSgEnumDeclaration(declaration) != NULL &&
          isSgTemplateInstantiationDecl(declaration) == NULL) ||
         isSgClassDeclaration(declaration) != NULL)) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("Found class/enum declaration in typedef declaration; using it "
             "as reference node for qualification.\n");
#endif
      ninfo_for_type.set_reference_node_for_qualification(declaration);
      if (outputTypeDefinition) {
        if (SgEnumDeclaration *enumDeclaration =
                isSgEnumDeclaration(declaration)) {
          const NameQualificationResult declarationQualification =
              exactStatementNameQualification(unp, enumDeclaration, info);
          ninfo_for_type.set_name_qualification_length(
              declarationQualification.length);
          ninfo_for_type.set_global_qualification_required(
              declarationQualification.global);
          ninfo_for_type.set_type_elaboration_required(
              declarationQualification.typeElaboration);
        } else if (SgClassDeclaration *classDeclaration =
                       isSgClassDeclaration(declaration)) {
          const NameQualificationResult declarationQualification =
              exactStatementNameQualification(unp, classDeclaration, info);
          ninfo_for_type.set_name_qualification_length(
              declarationQualification.length);
          ninfo_for_type.set_global_qualification_required(
              declarationQualification.global);
          ninfo_for_type.set_type_elaboration_required(
              declarationQualification.typeElaboration);
        }
      }
    } else {
      // Qualify typedef base types relative to the typedef declaration context.
      // This preserves required global qualification when local declarations
      // shadow class names (e.g., typedef ::A::B inside a function-local A).
      ninfo_for_type.set_reference_node_for_qualification(typedef_stmt);
    }

#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (first part) */ \n");
#endif
    // Only pass the ninfo_for_type to support name qualification of the base
    // type. unp->u_type->unparseType(btype, ninfo);
    unp->u_type->unparseType(btype, ninfo_for_type);

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (first part) */ \n");
#endif
    curprint(typedef_stmt->get_name().str());

    // Now unparse the second part of the typedef
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (second part) */ \n");
#endif

    ninfo.set_isTypeSecondPart();

    // DQ (1/2/2020): We need to use settings for ( info.inTypedefDecl() and
    // info.inArgList()) the same as used in unparsing the first part of the
    // type. unp->u_type->unparseType(btype, ninfo);
    ninfo_for_type.set_isTypeSecondPart();

    // DQ (1/2/2020): This is required since it is used in unparsing the first
    // part of the type and causes the "(" to be unparsed, and we require this
    // to be set so that the same logic will triger the ")" to be unparsed.
    ninfo_for_type.set_inTypedefDecl();
    // DQ (1/2/2020): I think we can assert this.
    ROSE_ASSERT(ninfo_for_type.isTypeFirstPart() == false);

    unp->u_type->unparseType(btype, ninfo_for_type);

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (second part) */ \n");
#endif

  } else {
    // previously implemented case of unparsing the typedef does not handle
    // function pointers properly (so they are handled explicitly above!)
#if DEBUG_TYPEDEF_DECLARATIONS
    printf("Not a typedef for a function type or member function type \n");
#endif

    ninfo.set_SkipFunctionQualifier();
    if (!ninfo.SkipBaseType()) {
      curprint("typedef ");
    }

    ninfo.set_SkipSemiColon();
    SgType *btype = typedef_stmt->get_base_type();

    // DQ (2/3/2019): Make the unparse_info object so that the unparseType() can
    // output the required parenthesis when the base type is a pointer to
    // member.
    if (pointerToMemberType != NULL) {
      ninfo.set_inTypedefDecl();
    }

    ninfo.set_isTypeFirstPart();

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // ninfo.set_SkipQualifiedNames();
    // curprint ( string("\n/* Commented out call to
    // ninfo.set_SkipQualifiedNames() */\n ";

    // printf ("Before first part of base type (type = %p = %s)
    // \n",btype,btype->sage_class_name()); ninfo.display ("Before first part of
    // type in unp->u_type->unparseTypeDefStmt()");

    SgUnparse_Info ninfo_for_type(ninfo);

    // DQ (1/10/2007): Set the current declaration statement so that if required
    // we can do context dependent searches of the AST to determine if name
    // qualification is required.
    ninfo_for_type.set_declstatement_ptr(NULL);
    ninfo_for_type.set_declstatement_ptr(typedef_stmt);

    if (typedef_stmt->get_requiresGlobalNameQualificationOnType() == true) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("In Unparse_ExprStmt::unp->u_type->unparseTypedefStmt(): This "
             "base type requires a global qualifier \n");
      curprint("\n/* This base type requires a global qualifier, calling "
               "set_requiresGlobalNameQualification() */ \n");
#endif
      // ninfo_for_type.set_forceQualifiedNames();
      ninfo_for_type.set_requiresGlobalNameQualification();
    }

    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: Before first
    // part of type */ \n";
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (first part) (Not a typedef for a function "
             "type or member function type) */ \n");
#endif

    // DQ (5/30/2011): Added support for name qualification.
    ninfo_for_type.set_reference_node_for_qualification(typedef_stmt);
    ASSERT_not_null(ninfo_for_type.get_reference_node_for_qualification());

    // DQ (5/14/2011): Added support for newer name qualification
    // implementation.
    const NameQualificationResult baseTypeQualification =
        exactStatementTypeQualification(unp, typedef_stmt, info);
    ninfo_for_type.set_name_qualification_length(baseTypeQualification.length);
    ninfo_for_type.set_global_qualification_required(
        baseTypeQualification.global);
    ninfo_for_type.set_type_elaboration_required(
        baseTypeQualification.typeElaboration);

    // DQ (1/2/2020): Added to define symetry in handling.
    // ninfo.set_inTypedefDecl();
    ninfo_for_type.set_inTypedefDecl();

    // DQ (11/21/2021): When there is a declaration of the base type then we
    // can't unparse the declaration by going through the base type because
    // types are shared and in the case of supporting multiple files we will
    // unparse the declaration from the other file.

    if (outputTypeDefinition == true) {
      // DQ (11/21/2021): Fixing bug reported by Jim Leek, Markus, and part of
      // work with Liao. Get the defining class declaration and output it
      // directly, instead of through the shared type which can only refer to
      // one file (half the time the wrong file) and for which the body of unt e
      // class declaration will not match the current file and will not be
      // unparsed.  Note that this is not an issue of supporting shared
      // declarations across two or more files, since this is before merge, and
      // thus there is a defining declaration for the class in each file. This
      // detail is a requirement for the support of multiple source files
      // specified on the command line only.

#if DEBUG_TYPEDEF_DECLARATIONS
      printf("Adding support for outputTypeDefinition == true for multiple "
             "files \n");
#endif
      SgDeclarationStatement *declaration = typedef_stmt->get_declaration();
      ROSE_ASSERT(declaration != NULL);
      // SgScopeStatement* scope = NULL;

      if (isSgClassDeclaration(declaration) == nullptr &&
          isSgEnumDeclaration(declaration) == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[typedef-base-type-definition]: inline "
                "definition is neither a class nor enum declaration\n");
        ROSE_ABORT();
      }
      if (typedef_stmt->get_isAssociatedWithDeclarationList()) {
        ninfo_for_type.set_PrintName();
      } else {
        ninfo_for_type.unset_PrintName();
      }
      unp->u_type->unparseType(btype, ninfo_for_type);
    } else {
      // DQ (7/28/2012): This is similar to code in the variable declaration
      // unparser function and so might be refactored. DQ (7/28/2012): If this
      // is a declaration associated with a declaration list from a previous
      // (the last statement) typedef then output the name if that declaration
      // had an un-named type (class or enum).
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("In unparseTypedefStmt(): "
             "typedef_stmt->get_isAssociatedWithDeclarationList() = %s \n",
             typedef_stmt->get_isAssociatedWithDeclarationList() ? "true"
                                                                 : "false");
#endif
      if (typedef_stmt->get_isAssociatedWithDeclarationList() == true) {
        // DQ (8/2/2012): Make this consistant with the design for the variable
        // declarations. This is an alternative to permit the unparsing of the
        // type to control the name output for types. But it would have to be
        // uniform that all the pieces of the first part of the type would have
        // to be output.  E.g. "*" in "*X".
        ninfo_for_type.set_PrintName();
        unp->u_type->unparseType(btype, ninfo_for_type);
      } else {
        // DQ (7/28/2012): Output the type if this is not associated with a
        // declaration list from a previous declaration.
        // unp->u_type->unparseType(btype, ninfo);
#if DEBUG_TYPEDEF_DECLARATIONS
        printf("In unparseTypedefStmt(): (first part): btype = %p = %s \n",
               btype, btype->class_name().c_str());
#endif
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
        curprint("\n/* Output a non function pointer typedef (Not a typedef "
                 "for a function type or member function type) */ \n");
#endif

        // DQ (5/7/2013): Using ninfo allows test2013_156.C to work.
        // unp->u_type->unparseType(btype, ninfo_for_type);
        // unp->u_type->unparseType(btype, ninfo);
        unp->u_type->unparseType(btype, ninfo_for_type);
      }
    }

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (first part) */ \n");
#endif

    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: After first part
    // of type */ \n"; printf ("After first part of type \n");

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // ninfo.unset_SkipQualifiedNames();

    // DQ (10/7/2004): Moved the output of the name to before the output of the
    // second part of the type to handle the case of "typedef A* A_Type[10];"
    // (see test2004_104.C).

#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output typedef name */ \n");
#endif
    // The name of the type (X, in the following example) has to appear after
    // the declaration. Example: struct { int a; } X;
    curprint(typedef_stmt->get_name().str());
    // curprint(string("/* before 2nd part */ ") +
    // typedef_stmt->get_name().str());

    ninfo.set_isTypeSecondPart();

    // printf ("Before 2nd part of type \n");
    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: Before second
    // part of type */ \n";
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (second part) */ \n");
#endif

    // DQ (1/2/2020): This is required since it is used in unparsing the first
    // part of the type and causes the "(" to be unparsed, and we require this
    // to be set so that the same logic will triger the ")" to be unparsed.
    ninfo.set_inTypedefDecl();
    // DQ (1/2/2020): I think we can assert this.
    ROSE_ASSERT(ninfo.isTypeFirstPart() == false);

    unp->u_type->unparseType(btype, ninfo);
    // unp->u_type->unparseType(btype, ninfo_for_type);
    // unp->u_type->unparseType(btype, ninfo_for_type);

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (second part) */ \n");
#endif
    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: After second
    // part of type */ \n"; printf ("After 2nd part of type \n");

    // DQ (2/3/2019): Unset this to avoid use outside of typedef unparsing.
    ninfo.unset_inTypedefDecl();
  }

#if DEBUG_TYPEDEF_DECLARATIONS
  printf("In unparseTypedefStmt(): outputTypeDefinition = %s \n",
         outputTypeDefinition ? "true" : "false");
#endif
  if (outputTypeDefinition == true) {
    unparseTypeAttributes(typedef_stmt);
  }

  // DQ (2/26/2013): Output any attributes.
  unp->u_sage->printAttributes(typedef_stmt, info);

  if (!info.SkipSemiColon()) {
    curprint(";");
  }

  // info.display ("At base of unp->u_type->unparseTypeDefStmt()");
#if DEBUG_TYPEDEF_DECLARATIONS
  printf("Leaving unparseTypedefStmt() \n");
  curprint("/* Leaving unparseTypedefStmt */ \n");
#endif
}

void Unparse_ExprStmt::unparseTemplateDeclStmt(SgStatement *stmt,
                                               SgUnparse_Info &) {
  SgTemplateDeclaration *template_stmt = isSgTemplateDeclaration(stmt);
  ASSERT_not_null(template_stmt);
  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[template-identity-only]: base "
          "SgTemplateDeclaration name=%s kind=%d is a semantic template "
          "identity and has no standalone source declaration; source templates "
          "must use a typed SgTemplate*Declaration node\n",
          template_stmt->get_name().getString().c_str(),
          static_cast<int>(template_stmt->get_template_kind()));
  ROSE_ABORT();
}

void Unparse_ExprStmt::unparseTemplateClassDefnStmt(SgStatement *stmt_,
                                                    SgUnparse_Info &info) {
  SgTemplateClassDefinition *stmt = isSgTemplateClassDefinition(stmt_);
  assert(stmt != NULL);
  unparseTemplateClassDeclStmt(stmt->get_declaration(), info);
}

void Unparse_ExprStmt::unparseTemplateClassDeclStmt(SgStatement *stmt,
                                                    SgUnparse_Info &info) {
  unparseTemplateDeclarationStatment_support<SgTemplateClassDeclaration>(stmt,
                                                                         info);
}

void Unparse_ExprStmt::unparseTemplateFunctionDeclStmt(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  // DQ (8/6/2012): Unparse the associated comments.
  // We can't unparse comments in the templae declarations until we stop using
  // saved string form of the template declaration.  This will be done in a
  // later version of the release of the new template support.  In the mean time
  // we have to supress attaching CPP directives to the inside of template
  // declarations.

  unparseTemplateDeclarationStatment_support<SgTemplateFunctionDeclaration>(
      stmt, info);
}

void Unparse_ExprStmt::unparseTemplateMemberFunctionDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {

  unparseTemplateDeclarationStatment_support<
      SgTemplateMemberFunctionDeclaration>(stmt, info);

  // DQ (5/28/2019): If there are any attached CPP directives then unparse them.
  // This will cause then to be output twice.
  // unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::after);
}

void Unparse_ExprStmt::unparseTemplateVariableDeclStmt(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  // DQ (1/3/2016): Present this function and the associated
  // SgTemplateVariableDeclaration IR node is being used for both the template
  // and the instanatiation of variables.  We might want to have an IR node
  // specific to template variable instantition.

  unparseTemplateDeclarationStatment_support<SgTemplateVariableDeclaration>(
      stmt, info);
}

#define DEBUG_unparseTemplateHeader 0

void Unparse_ExprStmt::unparseSourceSpelledTemplateHeaders(
    const SgTemplateParameterListPtrList &headers,
    SgDeclarationStatement *declaration, SgUnparse_Info &info,
    const char *declaration_kind) {
  ASSERT_not_null(declaration);
  ASSERT_not_null(declaration_kind);
  for (SgTemplateParameterList *header : headers) {
    if (header == nullptr || header->get_parent() != declaration) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-template-header]: %s "
              "declaration=%p has a null or foreign source header\n",
              declaration_kind, static_cast<void *>(declaration));
      ROSE_ABORT();
    }

    curprint("template");
    if (header->get_args().empty()) {
      curprint("<>");
    } else {
      curprint(" ");
      SgUnparse_Info header_info(info);
      header_info.set_declstatement_ptr(declaration);
      unparseTemplateParameterList(header->get_args(), header_info, true,
                                   declaration);
    }

    switch (header->get_source_header_separator()) {
    case SgTemplateParameterList::e_source_header_separator_space:
      curprint(" ");
      break;
    case SgTemplateParameterList::e_source_header_separator_newline:
      curprint("\n");
      break;
    case SgTemplateParameterList::e_source_header_separator_unset:
    default:
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[source-template-header-separator]: %s "
              "declaration=%p header=%p has unset or invalid separator=%d\n",
              declaration_kind, static_cast<void *>(declaration),
              static_cast<void *>(header),
              static_cast<int>(header->get_source_header_separator()));
      ROSE_ABORT();
    }
  }
}

template <class T>
void Unparse_ExprStmt::unparseTemplateHeader(T *decl, SgUnparse_Info &info) {
#if DEBUG_unparseTemplateHeader
  printf("In unparseTemplateHeader(decl = %p = %s) \n", decl,
         decl->class_name().c_str());
#endif
  SgTemplateParameterPtrList tlist;
  for (SgTemplateParameter *param : decl->get_templateParameters()) {
    if (!SageInterface::isAbbreviatedFunctionTemplateParameter(param)) {
      tlist.push_back(param);
    }
  }

  if (!tlist.empty()) {
    curprint("template ");
    const int template_header_start_line = unp->cur.current_line();
    SgUnparse_Info tinfo(info);
    tinfo.set_declstatement_ptr(NULL);
    tinfo.set_declstatement_ptr(decl);
    Unparse_ExprStmt::unparseTemplateParameterList(tlist, tinfo, true, decl);
    if (SgExpression *requires_clause = decl->get_requiresClause()) {
      curprint("requires ");
      SgUnparse_Info rinfo(info);
      rinfo.set_SkipClassDefinition();
      rinfo.set_SkipEnumDefinition();
      unparseRequiresClauseExpression(this, requires_clause, rinfo);
      curprint(" ");
    }
    const bool wrap_before_declaration =
        unp->cur.current_line() != template_header_start_line;
    if (wrap_before_declaration) {
      unp->cur.insert_newline(1, unp->cur.statement_indent());
    } else {
      curprint(" ");
    }
  } else {
    bool is_explicit_specialization = false;
    if (SgTemplateClassDeclaration *class_decl =
            isSgTemplateClassDeclaration(decl)) {
      is_explicit_specialization = class_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    } else if (SgTemplateFunctionDeclaration *func_decl =
                   isSgTemplateFunctionDeclaration(decl)) {
      is_explicit_specialization = func_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    } else if (SgTemplateMemberFunctionDeclaration *member_decl =
                   isSgTemplateMemberFunctionDeclaration(decl)) {
      is_explicit_specialization = member_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    } else if (SgTemplateVariableDeclaration *var_decl =
                   isSgTemplateVariableDeclaration(decl)) {
      is_explicit_specialization = var_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    }

    if (is_explicit_specialization) {
      curprint("template<> ");
    }
  }
}

template <class T>
void Unparse_ExprStmt::unparseTemplateDeclarationStatment_support(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);

  T *template_stmt = dynamic_cast<T *>(stmt);
  ASSERT_not_null(template_stmt);

  SgTemplateClassDeclaration *templateClassDeclaration =
      isSgTemplateClassDeclaration(stmt);
  SgTemplateFunctionDeclaration *templateFunctionDeclaration =
      isSgTemplateFunctionDeclaration(stmt);
  SgTemplateMemberFunctionDeclaration *templateMemberFunctionDeclaration =
      isSgTemplateMemberFunctionDeclaration(stmt);
  SgTemplateVariableDeclaration *templateVariableDeclaration =
      isSgTemplateVariableDeclaration(stmt);
  SgTemplateTypedefDeclaration *templateTypedefDeclaration =
      isSgTemplateTypedefDeclaration(stmt);

  ROSE_ASSERT(templateClassDeclaration != nullptr ||
              templateFunctionDeclaration != nullptr ||
              templateMemberFunctionDeclaration != nullptr ||
              templateVariableDeclaration != nullptr ||
              templateTypedefDeclaration != nullptr);

  const bool class_scope_friend_template_class =
      templateClassDeclaration != NULL &&
      templateClassDeclaration->get_declarationModifier().isFriend() &&
      (isSgClassDefinition(templateClassDeclaration->get_parent()) != NULL ||
       isSgTemplateClassDefinition(templateClassDeclaration->get_parent()) !=
           NULL ||
       isSgTemplateInstantiationDefn(templateClassDeclaration->get_parent()) !=
           NULL);
  requireDirectSourceSurfaceOwnership(template_stmt, "template-declaration");

  SgSourceFile *sourcefile =
      requireExactCxxSourceFile(unp, info, "template-declaration");
  const bool template_requires_ast_unparse =
      nodeHasTransformation(template_stmt) ||
      (templateClassDeclaration != NULL &&
       nodeHasTransformation(templateClassDeclaration->get_definition()));
  const bool inherited_partial_token_context =
      info.unparsedPartiallyUsingTokenStream() &&
      (sourcefile == NULL || sourcefile->get_unparse_tokens() == false);

  const bool class_scope_template =
      isSgClassDefinition(template_stmt->get_parent()) != NULL ||
      isSgTemplateClassDefinition(template_stmt->get_parent()) != NULL ||
      isSgTemplateInstantiationDefn(template_stmt->get_parent()) != NULL;
  TokenStreamSequenceToNodeMapping *template_token_mapping =
      sourcefile != NULL
          ? lookup_token_subsequence_mapping_for_node(sourcefile, template_stmt)
          : NULL;
  const bool unmodified_template_with_token_mapping =
      sourcefile != NULL && !template_stmt->isTransformation() &&
      !template_stmt->get_containsTransformation() &&
      !template_stmt->get_containsTransformationToSurroundingWhitespace() &&
      template_token_mapping != NULL;
  if (!class_scope_template && !template_requires_ast_unparse &&
      !inherited_partial_token_context &&
      unmodified_template_with_token_mapping &&
      tokenIntervalContainsConditionalPreprocessingDirective(
          sourcefile, template_token_mapping)) {
    unparseStatementFromTokenStream(stmt, e_leading_whitespace_start,
                                    e_trailing_whitespace_end, info);
    return;
  }

  unparseAttachedPreprocessingInfo(template_stmt, info,
                                   PreprocessingInfo::inside);

  // Check to see if this is an object defined within a class
  ASSERT_not_null(template_stmt->get_parent());
  SgClassDefinition *cdefn = isSgClassDefinition(template_stmt->get_parent());
  if (cdefn != NULL && cdefn->get_declaration()->get_class_type() ==
                           SgClassDeclaration::e_class) {
    info.set_CheckAccess();
  }

  // Output access modifiers
  unp->u_sage->printSpecifier1(template_stmt, info);

  // DQ (4/29/2004): Added support for "export" keyword (not supported by g++
  // yet)
  if (template_stmt->get_declarationModifier().isExport()) {
    curprint(string("export "));
  }

  // Five cases to consider: TODO this a template function, don't we already
  // know that?

  // Unparsing template from the AST can be controlled at the sourcefile or
  // declaration level. The declaration-level flag is set on the shared
  // template declaration base and applies to all template declaration kinds.

  bool unparse_template_from_ast =
      sourcefile != NULL && sourcefile->get_unparse_template_ast();
  unparse_template_from_ast |=
      template_stmt->get_unparse_template_ast() == true;
  unparse_template_from_ast |= class_scope_friend_template_class;
  unparse_template_from_ast |= template_requires_ast_unparse;
  unparse_template_from_ast |= inherited_partial_token_context;
  {
    const bool can_replay_template_tokens =
        sourcefile != NULL && !template_stmt->isTransformation() &&
        !template_stmt->get_containsTransformation() &&
        !template_stmt->get_containsTransformationToSurroundingWhitespace() &&
        canBeUnparsedFromTokenStream(sourcefile, template_stmt);
    if (!unparse_template_from_ast) {
      if (can_replay_template_tokens) {
        unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                        e_token_subsequence_end, info);
        return;
      }

      // Saved template strings are snapshots of frontend output. Editing them
      // to repair defaults, attributes, or compiler spellings hides missing AST
      // structure. Only token replay may preserve an unmodified source surface;
      // every other declaration is emitted from the typed AST.
      unparse_template_from_ast = true;
    }
  }

  auto unparse_member_function_ctor_initializers =
      [&](SgMemberFunctionDeclaration *member_function,
          SgUnparse_Info &decl_info) {
        if (member_function == NULL) {
          return;
        }
        if (member_function->isForward()) {
          return;
        }

        auto const &ctor_inits = member_function->get_ctors();
        if (ctor_inits.empty()) {
          if (SgMemberFunctionDeclaration *def_decl =
                  isSgMemberFunctionDeclaration(
                      member_function->get_definingDeclaration())) {
            if (def_decl != member_function && !def_decl->get_ctors().empty()) {
              fprintf(stderr,
                      "REX_UNPARSE_INVARIANT[constructor-initializer-owner]: "
                      "emitted member function=%p has no initializers but "
                      "defining declaration=%p owns %zu\n",
                      static_cast<void *>(member_function),
                      static_cast<void *>(def_decl),
                      def_decl->get_ctors().size());
              ROSE_ABORT();
            }
          }
          return;
        }

        SgUnparse_Info init_info(decl_info);
        init_info.set_SkipClassDefinition();
        init_info.set_SkipEnumDefinition();
        init_info.set_inArgList();
        init_info.set_declstatement_ptr(NULL);
        init_info.set_declstatement_ptr(member_function);
        SgCtorInitializerList *ctor_initializer_list =
            member_function->get_CtorInitializerList();
        if (ctor_initializer_list == NULL) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[constructor-initializer-owner]: "
                  "member function=%p has initializers but no initializer "
                  "list\n",
                  static_cast<void *>(member_function));
          ROSE_ABORT();
        }
        init_info.set_template_argument_qualification_context(
            ctor_initializer_list);

        auto it_ctor_init = ctor_inits.begin();
        auto const first = it_ctor_init;

        curprint(" : ");
        while (it_ctor_init != ctor_inits.end()) {
          SgInitializedName *ctor_init = *it_ctor_init;
          ASSERT_not_null(ctor_init);
          if (it_ctor_init != first) {
            curprint(", ");
          }
          ++it_ctor_init;

          unparseAttachedPreprocessingInfo(ctor_init, decl_info,
                                           PreprocessingInfo::before);

          unparseCtorPreinitializerDesignator(this, unp, ctor_init, init_info);

          SgExpression *initializer = ctor_init->get_initializer();
          if (initializer == NULL) {
            fprintf(stderr,
                    "REX_UNPARSE_INVARIANT[constructor-preinitializer]: "
                    "initialized-name=%p has no initializer expression\n",
                    static_cast<void *>(ctor_init));
            ROSE_ABORT();
          }

          SgConstructorInitializer *ctor_initializer =
              isSgConstructorInitializer(initializer);
          bool output_parenthesis = ctor_initializer == nullptr;
          if (output_parenthesis) {
            curprint(string("("));
          }

          init_info.set_reference_node_for_qualification(initializer);
          unparseExpression(initializer, init_info);
          init_info.set_reference_node_for_qualification(NULL);

          if (output_parenthesis) {
            curprint(string(")"));
          }
        }
      };

  auto template_parameter_lists_match =
      [](const SgTemplateParameterPtrList &lhs,
         const SgTemplateParameterPtrList &rhs) -> bool {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
      SgTemplateParameter *lhs_param = lhs[i];
      SgTemplateParameter *rhs_param = rhs[i];
      if (lhs_param == nullptr || rhs_param == nullptr) {
        if (lhs_param != rhs_param) {
          return false;
        }
        continue;
      }
      if (lhs_param->get_parameterType() != rhs_param->get_parameterType()) {
        return false;
      }
      if (lhs_param->get_is_parameter_pack() !=
          rhs_param->get_is_parameter_pack()) {
        return false;
      }

      auto parameter_name = [](SgTemplateParameter *param) -> std::string {
        if (param == nullptr) {
          return std::string();
        }
        if (SgTemplateType *template_type =
                isSgTemplateType(param->get_type())) {
          std::string name = template_type->get_name().getString();
          if (!name.empty()) {
            return name;
          }
        }
        if (SgInitializedName *init_name = param->get_initializedName()) {
          std::string name = init_name->get_name().getString();
          if (!name.empty()) {
            return name;
          }
        }
        return std::string();
      };

      if (parameter_name(lhs_param) != parameter_name(rhs_param)) {
        return false;
      }
    }
    return true;
  };

  if (unparse_template_from_ast) {
    SgTemplateClassDeclaration *assoc_tpl_class_decl = nullptr;
    SgDeclarationStatement *associated_decl = nullptr;
    if (templateMemberFunctionDeclaration) {
      associated_decl =
          templateMemberFunctionDeclaration->get_associatedClassDeclaration();
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    } else if (templateVariableDeclaration) {
      ROSE_ASSERT(templateVariableDeclaration->get_variables().size() == 1);
      auto *iname = templateVariableDeclaration->get_variables()[0];
      associated_decl =
          iname->get_scope()
              ? isSgDeclarationStatement(iname->get_scope()->get_parent())
              : nullptr;
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    } else if (templateClassDeclaration || templateTypedefDeclaration) {
      associated_decl = template_stmt->get_scope()
                            ? isSgDeclarationStatement(
                                  template_stmt->get_scope()->get_parent())
                            : nullptr;
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    }
    if (templateClassDeclaration != nullptr) {
      associated_decl = exactTemplateClassHeaderOwner(templateClassDeclaration,
                                                      associated_decl);
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    }
    //     std::cout << "assoc_tpl_class_decl = " << std::hex <<
    //     assoc_tpl_class_decl << " : " << (assoc_tpl_class_decl ?
    //     assoc_tpl_class_decl->class_name() : "") << std::endl;

    SgNode *parent = stmt->get_parent();
    const bool parent_is_class_like_definition =
        isSgClassDefinition(parent) != nullptr ||
        isSgTemplateClassDefinition(parent) != nullptr ||
        isSgTemplateInstantiationDefn(parent) != nullptr;
    if (!parent_is_class_like_definition) {
      if (templateMemberFunctionDeclaration != nullptr &&
          (!templateMemberFunctionDeclaration->isTransformation() ||
           !templateMemberFunctionDeclaration
                ->get_sourceSpelledTemplateHeaders()
                .empty())) {
        // The frontend publishes the exact source-spelled outer headers on
        // this lexical member declaration.  Reconstructing them from the
        // associated semantic class chain invents `template<>` for members of
        // an explicitly specialized class, where the source owns no outer
        // header at all.
        unparseSourceSpelledTemplateHeaders(
            templateMemberFunctionDeclaration
                ->get_sourceSpelledTemplateHeaders(),
            templateMemberFunctionDeclaration, info,
            "template member function");
      } else if (templateClassDeclaration != nullptr &&
                 !templateClassDeclaration->get_sourceSpelledTemplateHeaders()
                      .empty()) {
        // An out-of-line nested class specialization owns its written outer
        // headers independently of the semantic class chain.  Empty headers
        // are significant `template<>` source surfaces and must not be
        // reconstructed from template ownership.
        unparseSourceSpelledTemplateHeaders(
            templateClassDeclaration->get_sourceSpelledTemplateHeaders(),
            templateClassDeclaration, info, "template class");
      } else if (templateVariableDeclaration != nullptr &&
                 !templateVariableDeclaration
                      ->get_sourceSpelledTemplateHeaders()
                      .empty()) {
        unparseSourceSpelledTemplateHeaders(
            templateVariableDeclaration->get_sourceSpelledTemplateHeaders(),
            templateVariableDeclaration, info, "template variable");
      } else {
        std::vector<SgTemplateClassDeclaration *> assoc_tpl_chain =
            collectTemplateClassChain(associated_decl);
        if (templateClassDeclaration != nullptr &&
            templateClassDeclaration->get_templateClassOwnerScopeKind() ==
                SgTemplateClassDeclaration::
                    e_template_class_owner_scope_source_spelled &&
            assoc_tpl_chain.empty()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[template-class-header-owner]: "
                  "declaration=%p name=%s source-spelled owner=%p has no "
                  "template class header chain\n",
                  static_cast<void *>(templateClassDeclaration),
                  templateClassDeclaration->get_name().getString().c_str(),
                  static_cast<void *>(associated_decl));
          ROSE_ABORT();
        }
        for (auto it = assoc_tpl_chain.rbegin(); it != assoc_tpl_chain.rend();
             ++it) {
          unparseTemplateHeader(*it, info);
        }
      }
    }

    unparseTemplateHeader(template_stmt, info);

    SgUnparse_Info ninfo(info);
    if (ninfo.unparsedPartiallyUsingTokenStream()) {
      // This template declaration has already committed to AST output.
      // Keeping an inherited partial-token flag here mixes replayed template
      // boundary text with AST-emitted class/function bodies.
      ninfo.unset_unparsedPartiallyUsingTokenStream();
    }

    if (templateClassDeclaration != NULL) {
      ninfo.unset_SkipSemiColon();
      ninfo.set_declstatement_ptr(NULL);
      ninfo.set_declstatement_ptr(templateClassDeclaration);

      // Match non-template class declaration output so friend/specifier
      // modifiers attached to template declarations are preserved.
      unp->u_sage->printSpecifier2(templateClassDeclaration, ninfo);

      SgClassDefinition *class_defn =
          templateClassDeclaration->get_definition();
      if (class_defn != NULL) {
        unparseClassDefnStmt(templateClassDeclaration->get_definition(), ninfo);
      } else {
        SgClassDeclaration::class_types class_type =
            templateClassDeclaration->get_class_type();
        switch (class_type) {
        case SgClassDeclaration::e_class:
          curprint("class ");
          break;
        case SgClassDeclaration::e_struct:
          curprint("struct ");
          break;
        case SgClassDeclaration::e_union:
          curprint("union ");
          break;
        case SgClassDeclaration::e_template_parameter:
          curprint(" ");
          break;
        default: {
          printf("Error: default reached in unparseClassDeclStmt() \n");
          ROSE_ABORT();
        }
        }

        SgName class_name = templateClassDeclaration->get_name();
        curprint(class_name.getString().c_str());
        unp->u_sage->printAttributes(templateClassDeclaration, ninfo);
      }

      ninfo.set_declstatement_ptr(NULL);

      if (!info.SkipSemiColon())
        curprint(";");

    } else if (templateFunctionDeclaration != NULL ||
               templateMemberFunctionDeclaration != NULL) {
      SgFunctionDeclaration *functionDeclaration =
          (SgFunctionDeclaration *)stmt;

      // Match non-template function declarator output (constexpr/friend/etc.).
      unp->u_sage->printSpecifier2(functionDeclaration, ninfo);

      const bool is_deduction_guide =
          functionDeclaration->get_is_deduction_guide();
      SgType *rtype = functionDeclaration->get_orig_return_type();
      ASSERT_not_null(rtype);
      if (!is_deduction_guide) {
        unparseReturnType(functionDeclaration, rtype, ninfo);
      }

      ninfo.unset_SkipSemiColon();
      ninfo.set_declstatement_ptr(NULL);
      ninfo.set_declstatement_ptr(functionDeclaration);

      unparse_helper(functionDeclaration, ninfo);

      ninfo.set_declstatement_ptr(NULL);

      const bool use_trailing_return_type_syntax =
          rtype != NULL &&
          (is_deduction_guide ||
           functionUsesTrailingReturnTypeSyntax(functionDeclaration));
      if (use_trailing_return_type_syntax &&
          templateMemberFunctionDeclaration != NULL) {
        unparseMemberFunctionParametersAndQualifiers(
            templateMemberFunctionDeclaration, ninfo);
      }

      if (rtype != NULL) {
        if (use_trailing_return_type_syntax) {
          curprint(" -> ");
          SgUnparse_Info trailing_type_info(ninfo);
          configureExactFunctionReturnTypeInfo(unp, functionDeclaration,
                                               trailing_type_info);
          trailing_type_info.set_isTypeFirstPart();
          unp->u_type->unparseType(rtype, trailing_type_info);
          trailing_type_info.set_isTypeSecondPart();
          unp->u_type->unparseType(rtype, trailing_type_info);
        } else {
          SgUnparse_Info ninfo3(ninfo);
          configureExactFunctionReturnTypeInfo(unp, functionDeclaration,
                                               ninfo3);
          ninfo3.set_isTypeSecondPart();
          unp->u_type->unparseType(rtype, ninfo3);
        }
      }
      unparseOldStyleFunctionParameterDeclarations(functionDeclaration, ninfo);

      const bool uses_wrapped_function_type =
          functionDeclaration
              ->get_source_declarator_uses_wrapped_function_type();
      if (uses_wrapped_function_type) {
        functionDeclaration->validate_source_declarator_form();
      }
      if (templateMemberFunctionDeclaration != NULL) {
        if (use_trailing_return_type_syntax) {
          unparseMemberFunctionPostDeclaratorModifiers(
              templateMemberFunctionDeclaration, ninfo);
        } else if (uses_wrapped_function_type) {
          unparseMemberFunctionPostDeclaratorModifiers(
              templateMemberFunctionDeclaration, ninfo);
        } else {
          unparseTrailingFunctionModifiers(templateMemberFunctionDeclaration,
                                           ninfo);
        }
        unparse_member_function_ctor_initializers(
            templateMemberFunctionDeclaration, ninfo);
      } else {
        if (!uses_wrapped_function_type &&
            functionDeclaration->get_declarationModifier().isThrow()) {
          const SgTypePtrList &exceptionSpecifierList =
              functionDeclaration->get_exceptionSpecification();
          ninfo.set_reference_node_for_qualification(functionDeclaration);
          unparseExceptionSpecification(exceptionSpecifierList, ninfo);
          ninfo.set_reference_node_for_qualification(NULL);
        }
        if (functionDeclaration->get_asm_name().empty() == false) {
          curprint(" __asm__ (\"");
          curprint(functionDeclaration->get_asm_name());
          curprint(string("\")"));
        }
        unparseTrailingRequiresClauseIfPresent(this, functionDeclaration,
                                               ninfo);
      }

      SgFunctionDefinition *functionDefn =
          functionDeclaration->get_definition();
      if (functionDefn != NULL) {
        SgBasicBlock *body = functionDefn->get_body();
        SgUnparse_Info body_info(info);
        body_info.unset_CheckAccess();
        // Conditional signature branches and comments that occur between a
        // template function declarator and its body are owned by the exact
        // function-definition node.  The structural template path emits the
        // declarator directly, so it must honor that node's attachments before
        // entering the body instead of bypassing their semantic owner.
        unparseAttachedPreprocessingInfo(functionDefn, info,
                                         PreprocessingInfo::before);
        unparseStatement(body, body_info);
        unparseAttachedPreprocessingInfo(
            functionDeclaration->get_parameterList(), info,
            PreprocessingInfo::after);
        unparseAttachedPreprocessingInfo(functionDefn, info,
                                         PreprocessingInfo::after);
      }

      if (functionDefn == NULL && !info.SkipSemiColon())
        curprint(";");

    } else if (templateVariableDeclaration != NULL) {
      unparseVarDeclStmt(templateVariableDeclaration, info);
    } else {
      printf("Error: unexpected node variant: %s\n",
             stmt->class_name().c_str());
      ROSE_ABORT();
    }
    curprint("\n");
  }
}

// OpenMP support
void Unparse_ExprStmt::unparseOmpPrefix(SgUnparse_Info &) {
  if (unp->cur.get_compact_output()) {
    unp->cur.begin_compact_directive();
  }
  curprint(string("#pragma omp "));
}

// OpenACC support
void Unparse_ExprStmt::unparseAccPrefix(SgUnparse_Info &) {
  if (unp->cur.get_compact_output()) {
    unp->cur.begin_compact_directive();
  }
  curprint(string("#pragma acc "));
}

void Unparse_ExprStmt::unparseOmpForStatement(SgStatement *stmt,
                                              SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpForStatement *f_stmt = isSgOmpForStatement(stmt);
  ASSERT_not_null(f_stmt);
  const std::optional<int> saved_linewrap = unp->cur.get_linewrap();
  unp->cur.disable_linewrap();

  unparseOmpDirectivePrefixAndName(stmt, info);

  unparseOmpBeginDirectiveClauses(stmt, info);
  // TODO a better way to new line? and add indentation
  curprint(string("\n"));

  SgUnparse_Info ninfo(info);
  if (f_stmt->get_body()) {
    unparseStatement(f_stmt->get_body(), ninfo);
  } else {
    cerr << "Error: empty body for:" << stmt->class_name() << " is not allowed!"
         << endl;
    ROSE_ABORT();
  }

  unp->cur.set_linewrap(saved_linewrap);
}

void Unparse_ExprStmt::unparseOmpForSimdStatement(SgStatement *stmt,
                                                  SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpForSimdStatement *f_stmt = isSgOmpForSimdStatement(stmt);
  ASSERT_not_null(f_stmt);
  const std::optional<int> saved_linewrap = unp->cur.get_linewrap();
  unp->cur.disable_linewrap();

  unparseOmpDirectivePrefixAndName(stmt, info);

  unparseOmpBeginDirectiveClauses(stmt, info);
  // TODO a better way to new line? and add indentation
  curprint(string("\n"));

  SgUnparse_Info ninfo(info);
  if (f_stmt->get_body()) {
    unparseStatement(f_stmt->get_body(), ninfo);
  } else {
    cerr << "Error: empty body for:" << stmt->class_name() << " is not allowed!"
         << endl;
    ROSE_ABORT();
  }

  unp->cur.set_linewrap(saved_linewrap);
}

void Unparse_ExprStmt::unparseOmpBeginDirectiveClauses(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  // optional clauses
  SgOmpClauseBodyStatement *bodystmt = isSgOmpClauseBodyStatement(stmt);
  SgOmpDeclareSimdStatement *simdstmt = isSgOmpDeclareSimdStatement(stmt);
  SgOmpDeclareVariantStatement *declarevariantstmt =
      isSgOmpDeclareVariantStatement(stmt);
  SgOmpBeginDeclareVariantStatement *begindeclarevariantstmt =
      isSgOmpBeginDeclareVariantStatement(stmt);
  SgOmpDeclareMapperStatement *mapperstmt = isSgOmpDeclareMapperStatement(stmt);
  SgOmpDeclareTargetStatement *declaretargetstmt =
      isSgOmpDeclareTargetStatement(stmt);
  SgOmpTaskwaitStatement *taskwaitstmt = isSgOmpTaskwaitStatement(stmt);
  SgOmpClauseStatement *clausestmt = isSgOmpClauseStatement(stmt);
  SgOmpRequiresStatement *requiresstmt = isSgOmpRequiresStatement(stmt);
  SgOmpAssumesStatement *assumesstmt = isSgOmpAssumesStatement(stmt);
  SgOmpBeginAssumesStatement *beginassumesstmt =
      isSgOmpBeginAssumesStatement(stmt);
  SgOmpGroupprivateStatement *groupprivatestmt =
      isSgOmpGroupprivateStatement(stmt);
  const SgOmpClausePtrList *clause_ptr_list = nullptr;
  if (bodystmt != nullptr) {
    clause_ptr_list =
        &requiredCxxOmpClauses(bodystmt, bodystmt->get_clause_list());
  } else if (simdstmt != nullptr) {
    clause_ptr_list = &simdstmt->get_clauses();
  } else if (declarevariantstmt != nullptr) {
    clause_ptr_list = &declarevariantstmt->get_clauses();
  } else if (begindeclarevariantstmt != nullptr) {
    clause_ptr_list = &begindeclarevariantstmt->get_clauses();
  } else if (mapperstmt != nullptr) {
    clause_ptr_list = &mapperstmt->get_clauses();
  } else if (declaretargetstmt != nullptr) {
    clause_ptr_list = &declaretargetstmt->get_clauses();
  } else if (taskwaitstmt != nullptr) {
    clause_ptr_list = &taskwaitstmt->get_clauses();
  } else if (clausestmt != nullptr) {
    clause_ptr_list =
        &requiredCxxOmpClauses(clausestmt, clausestmt->get_clause_list());
  } else if (requiresstmt != nullptr) {
    clause_ptr_list = &requiresstmt->get_clauses();
  } else if (assumesstmt != nullptr) {
    clause_ptr_list = &assumesstmt->get_clauses();
  } else if (beginassumesstmt != nullptr) {
    clause_ptr_list = &beginassumesstmt->get_clauses();
  } else if (groupprivatestmt != nullptr) {
    clause_ptr_list = &requiredCxxOmpClauses(
        groupprivatestmt, groupprivatestmt->get_clause_list());
  }
  if (mapperstmt != nullptr) {
    curprint(string("("));
    switch (mapperstmt->get_identifier()) {
    case SgOmpClause::e_omp_declare_mapper_identifier_default:
      if (mapperstmt->get_identifier_is_explicit()) {
        curprint(string("default : "));
      }
      break;
    case SgOmpClause::e_omp_declare_mapper_identifier_user:
      if (!mapperstmt->get_identifier_is_explicit() ||
          mapperstmt->get_user_defined_identifier() == nullptr) {
        cerr << "REX_UNPARSE_INVARIANT[declare-mapper-identifier]: "
                "user-defined mapper has no exact explicit identifier "
                "expression"
             << endl;
        ROSE_ABORT();
      }
      unparseExpression(mapperstmt->get_user_defined_identifier(), info);
      curprint(string(" : "));
      break;
    case SgOmpClause::e_omp_declare_mapper_identifier_unspecified:
      cerr << "REX_UNPARSE_INVARIANT[declare-mapper-identifier]: mapper "
              "identifier kind is unspecified"
           << endl;
      ROSE_ABORT();
    default:
      cerr << "REX_UNPARSE_INVARIANT[declare-mapper-identifier]: invalid "
              "mapper identifier kind="
           << static_cast<int>(mapperstmt->get_identifier()) << endl;
      ROSE_ABORT();
    }

    SgTypeExpression *mapper_type =
        isSgTypeExpression(mapperstmt->get_mapper_type());
    if (mapper_type == nullptr ||
        mapper_type->get_represented_type() == nullptr) {
      cerr << "REX_UNPARSE_INVARIANT[declare-mapper-type]: mapper has no "
              "exact semantic type expression"
           << endl;
      ROSE_ABORT();
    }
    unparseExpression(mapper_type, info);
    if (mapperstmt->get_mapper_variable() != nullptr) {
      curprint(string(" "));
      unparseExpression(mapperstmt->get_mapper_variable(), info);
    } else {
      cerr << "REX_UNPARSE_INVARIANT[declare-mapper-variable]: mapper has no "
              "semantic variable expression"
           << endl;
      ROSE_ABORT();
    }
    curprint(string(")"));
  }

  if (clause_ptr_list != nullptr && !clause_ptr_list->empty()) {
    for (SgOmpClause *c_clause : *clause_ptr_list) {
      unparseOmpClause(c_clause, info);
    }
  }
  if (declaretargetstmt != nullptr) {
    const SgOmpClause::omp_when_context_kind_enum device_type_kind =
        declaretargetstmt->get_device_type_kind();
    if (device_type_kind != SgOmpClause::e_omp_when_context_kind_unknown) {
      curprint(string(" device_type("));
      switch (device_type_kind) {
      case SgOmpClause::e_omp_when_context_kind_host:
        curprint(string("host"));
        break;
      case SgOmpClause::e_omp_when_context_kind_nohost:
        curprint(string("nohost"));
        break;
      case SgOmpClause::e_omp_when_context_kind_any:
        curprint(string("any"));
        break;
      default:
        ROSE_ABORT();
      }
      curprint(string(")"));
    }
  }
}

void Unparse_ExprStmt::unparseAccBeginDirectiveClauses(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  const SgAccClausePtrList *clause_ptr_list = nullptr;
  if (SgAccClauseBodyStatement *bodystmt = isSgAccClauseBodyStatement(stmt)) {
    clause_ptr_list = &bodystmt->get_clauses();
  } else if (SgAccClauseStatement *clausestmt = isSgAccClauseStatement(stmt)) {
    clause_ptr_list = &clausestmt->get_clauses();
  }
  if (clause_ptr_list != nullptr) {
    for (SgAccClause *c_clause : *clause_ptr_list) {
      unparseAccClause(c_clause, info);
    }
  }
}

void Unparse_ExprStmt::unparseStaticAssertionDeclaration(SgStatement *stmt,
                                                         SgUnparse_Info &info) {
  // DQ (7/25/2014): Adding support for C11 static assertions.

  // For C11 this whould be unparsed as "_Static_assert", but for C++ it should
  // be unparsed as "static_assert".
  SgStaticAssertionDeclaration *staticAssertionDeclaration =
      isSgStaticAssertionDeclaration(stmt);
  ASSERT_not_null(staticAssertionDeclaration);
  staticAssertionDeclaration->validate_static_assertion();

  SgExpression *condition = staticAssertionDeclaration->get_condition();
  SgExpression *message = staticAssertionDeclaration->get_message();

  // DQ (4/29/2017): This is the C11 syntax, and for C++11 we need the
  // alternative syntax ("static_assert"). curprint("_Static_assert(");
  if (SageInterface::is_Cxx_language() == true) {
    // This must be C++11 (or later).
    curprint("static_assert(");
  } else {
    // This must be C11 (or later).
    curprint("_Static_assert(");
  }

  unparseExpression(condition, info);
  if (message != nullptr) {
    curprint(", ");
    unparseExpression(message, info);
  }
  curprint(");");
}

void Unparse_ExprStmt::unparseFriendTypeDeclaration(SgStatement *stmt,
                                                    SgUnparse_Info &info) {
  SgFriendTypeDeclaration *friend_decl = isSgFriendTypeDeclaration(stmt);
  if (friend_decl == nullptr) {
    std::cerr << "REX_UNPARSER_INVARIANT[friend-type-declaration]: expected "
                 "SgFriendTypeDeclaration\n";
    ROSE_ABORT();
  }

  SgType *friend_type = friend_decl->get_friend_type();
  if (friend_type == nullptr) {
    std::cerr << "REX_UNPARSER_INVARIANT[friend-type-declaration]: declaration "
                 "has no exact friend type\n";
    ROSE_ABORT();
  }

  SgNode *parent = friend_decl->get_parent();
  if (isSgClassDefinition(parent) == nullptr &&
      isSgTemplateClassDefinition(parent) == nullptr &&
      isSgTemplateInstantiationDefn(parent) == nullptr) {
    std::cerr << "REX_UNPARSER_INVARIANT[friend-type-declaration]: declaration "
                 "is not structurally owned by a class definition\n";
    ROSE_ABORT();
  }

  curprint("friend ");
  SgUnparse_Info type_info(info);
  type_info.set_reference_node_for_qualification(friend_decl);
  type_info.set_SkipClassDefinition();
  type_info.set_SkipEnumDefinition();
  unp->u_type->unparseType(friend_type, type_info);
  if (!info.SkipSemiColon()) {
    curprint(";");
  }
}

// EOF
