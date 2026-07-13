#include "rose.h"

#include "Clang/clang-expanded-token-order.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool endsWith(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

SgNamespaceDeclarationStatement *findNamespace(SgNode *root,
                                               const std::string &name) {
  SgNamespaceDeclarationStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgNamespaceDeclarationStatement)) {
    SgNamespaceDeclarationStatement *declaration =
        isSgNamespaceDeclarationStatement(node);
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  result->validate_source_fragments();
  return result;
}

std::vector<SgNamespaceDeclarationStatement *>
findNamespaces(SgNode *root, const std::string &name) {
  std::vector<SgNamespaceDeclarationStatement *> result;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgNamespaceDeclarationStatement)) {
    SgNamespaceDeclarationStatement *declaration =
        isSgNamespaceDeclarationStatement(node);
    if (declaration != nullptr && declaration->get_name().getString() == name) {
      declaration->validate_source_fragments();
      result.push_back(declaration);
    }
  }
  return result;
}

SgVariableDeclaration *findVariable(SgNode *root, const std::string &name) {
  SgVariableDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgVariableDeclaration)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(node);
    ROSE_ASSERT(declaration != nullptr);
    for (SgInitializedName *initialized_name : declaration->get_variables()) {
      ROSE_ASSERT(initialized_name != nullptr);
      if (initialized_name->get_name().getString() != name) {
        continue;
      }
      ROSE_ASSERT(result == nullptr);
      result = declaration;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

std::vector<SgFunctionDeclaration *> findFunctions(SgNode *root,
                                                   const SgName &name) {
  std::vector<SgFunctionDeclaration *> result;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration != nullptr && declaration->get_name() == name &&
        isSgNamespaceDefinitionStatement(declaration->get_parent()) !=
            nullptr) {
      result.push_back(declaration);
    }
  }
  std::sort(result.begin(), result.end(),
            [](SgFunctionDeclaration *lhs, SgFunctionDeclaration *rhs) {
              ROSE_ASSERT(lhs != nullptr);
              ROSE_ASSERT(rhs != nullptr);
              ROSE_ASSERT(lhs->get_startOfConstruct() != nullptr);
              ROSE_ASSERT(rhs->get_startOfConstruct() != nullptr);
              return lhs->get_startOfConstruct()->get_line() <
                     rhs->get_startOfConstruct()->get_line();
            });
  return result;
}

std::vector<SgFunctionSymbol *>
findDirectFunctionSymbols(SgScopeStatement *scope, const SgName &name) {
  std::vector<SgFunctionSymbol *> result;
  ROSE_ASSERT(scope != nullptr);
  SgSymbolTable *table = scope->get_symbol_table();
  ROSE_ASSERT(table != nullptr);
  ROSE_ASSERT(table->get_parent() == scope);
  rose_hash_multimap *symbols = table->get_table();
  ROSE_ASSERT(symbols != nullptr);
  auto range = symbols->equal_range(name);
  for (auto iterator = range.first; iterator != range.second; ++iterator) {
    if (SgFunctionSymbol *symbol = isSgFunctionSymbol(iterator->second)) {
      result.push_back(symbol);
    }
  }
  return result;
}

size_t declarationOccurrences(SgScopeStatement *scope,
                              SgDeclarationStatement *declaration) {
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(declaration != nullptr);
  if (scope->containsOnlyDeclarations()) {
    const SgDeclarationStatementPtrList &declarations =
        scope->getDeclarationList();
    return std::count(declarations.begin(), declarations.end(), declaration);
  }
  const SgStatementPtrList &statements = scope->getStatementList();
  return std::count(statements.begin(), statements.end(), declaration);
}

void requireCanonicalTemplateInstantiationIdentity(SgNode *root,
                                                   const SgName &name) {
  SgTemplateInstantiationDecl *canonical_declaration = nullptr;
  SgClassType *canonical_type = nullptr;
  bool saw_instantiation = false;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateInstantiationDecl)) {
    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != name) {
      continue;
    }

    saw_instantiation = true;
    SgTemplateInstantiationDecl *first = isSgTemplateInstantiationDecl(
        declaration->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr);
    ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
    ROSE_ASSERT(first->get_type() != nullptr);
    ROSE_ASSERT(first->get_type()->get_declaration() == first);
    ROSE_ASSERT(declaration->get_type() == first->get_type());

    if (canonical_declaration == nullptr) {
      canonical_declaration = first;
      canonical_type = first->get_type();
    } else {
      ROSE_ASSERT(first == canonical_declaration);
      ROSE_ASSERT(first->get_type() == canonical_type);
    }
  }
  ROSE_ASSERT(saw_instantiation);
}

void requireSemanticBooleanTemplateArgumentProvenance(SgNode *root) {
  std::size_t canonical_boolean_count = 0;
  std::size_t matching_instantiation_count = 0;
  const SgName expected_name("rex_semantic_boolean_argument<true>");
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateInstantiationDecl)) {
    SgTemplateInstantiationDecl *declaration =
        isSgTemplateInstantiationDecl(node);
    ROSE_ASSERT(declaration != nullptr);
    if (declaration->get_name() != expected_name) {
      continue;
    }
    ++matching_instantiation_count;
    for (SgTemplateArgument *argument :
         declaration->get_deducedTemplateArguments()) {
      ROSE_ASSERT(argument != nullptr);
      if (argument->get_argumentType() !=
          SgTemplateArgument::nontype_argument) {
        continue;
      }
      SgBoolValExp *literal = isSgBoolValExp(argument->get_expression());
      if (literal == nullptr || argument->get_expression() != literal ||
          literal->get_literal_spelling_form() !=
              SgValueExp::e_literal_canonical_generated) {
        fprintf(stderr,
                "REX_TEST_INVARIANT[semantic-boolean-template-argument]: "
                "instantiation=%p argument=%p expression=%p/%s is not one "
                "canonical generated boolean literal\n",
                static_cast<void *>(declaration), static_cast<void *>(argument),
                static_cast<void *>(argument->get_expression()),
                argument->get_expression() != nullptr
                    ? argument->get_expression()->class_name().c_str()
                    : "<null>");
        ROSE_ABORT();
      }

      Sg_File_Info *primary = literal->get_file_info();
      Sg_File_Info *start = literal->get_startOfConstruct();
      Sg_File_Info *end = literal->get_endOfConstruct();
      Sg_File_Info *operator_position = literal->get_operatorPosition();
      ROSE_ASSERT(primary != nullptr && start != nullptr && end != nullptr &&
                  operator_position != nullptr);
      ROSE_ASSERT(primary == operator_position);
      ROSE_ASSERT(start != end && start != operator_position &&
                  end != operator_position);
      for (Sg_File_Info *position : {start, end, operator_position}) {
        if (position->get_parent() != literal ||
            !position->isCompilerGenerated() ||
            !position->isFrontendSpecific() || position->isTransformation() ||
            position->isSourcePositionUnavailableInFrontend() ||
            !position->isOutputInCodeGeneration() ||
            position->get_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
            position->get_physical_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
            position->get_source_sequence_number() != 0) {
          fprintf(stderr,
                  "REX_TEST_INVARIANT[semantic-boolean-template-argument]: "
                  "literal=%p position=%p parent=%p generated=%d "
                  "frontend=%d transformation=%d unavailable=%d output=%d "
                  "file=%d physical=%d sequence=%u\n",
                  static_cast<void *>(literal), static_cast<void *>(position),
                  static_cast<void *>(position->get_parent()),
                  position->isCompilerGenerated(),
                  position->isFrontendSpecific(), position->isTransformation(),
                  position->isSourcePositionUnavailableInFrontend(),
                  position->isOutputInCodeGeneration(), position->get_file_id(),
                  position->get_physical_file_id(),
                  position->get_source_sequence_number());
          ROSE_ABORT();
        }
      }
      ++canonical_boolean_count;
    }
  }
  ROSE_ASSERT(matching_instantiation_count > 0);
  ROSE_ASSERT(canonical_boolean_count > 0);
}

void requireFriendParameterTagSourceOwner(SgNode *root) {
  SgFunctionDeclaration *friend_function = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(node);
    if (candidate == nullptr ||
        candidate->get_name() != "rex_friend_parameter_function" ||
        candidate->get_file_info() == nullptr ||
        !candidate->get_file_info()->isOutputInCodeGeneration()) {
      continue;
    }
    ROSE_ASSERT(friend_function == nullptr);
    friend_function = candidate;
  }
  ROSE_ASSERT(friend_function != nullptr);

  SgFunctionParameterList *parameters = friend_function->get_parameterList();
  SgDeclarationScope *declarator_scope =
      friend_function->get_function_declarator_scope();
  ROSE_ASSERT(parameters != nullptr &&
              parameters->get_parent() == friend_function);
  ROSE_ASSERT(parameters->get_args().size() == 1);
  ROSE_ASSERT(declarator_scope != nullptr &&
              declarator_scope->get_parent() == friend_function);

  SgInitializedName *parameter = parameters->get_args().front();
  ROSE_ASSERT(parameter != nullptr && parameter->get_parent() == parameters);
  SgReferenceType *reference_type = isSgReferenceType(parameter->get_type());
  SgClassType *class_type = reference_type != nullptr
                                ? isSgClassType(reference_type->get_base_type())
                                : nullptr;
  SgClassDeclaration *canonical =
      class_type != nullptr
          ? isSgClassDeclaration(class_type->get_declaration())
          : nullptr;
  canonical =
      canonical != nullptr
          ? isSgClassDeclaration(canonical->get_firstNondefiningDeclaration())
          : nullptr;
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(canonical->get_name() == "rex_friend_parameter_tag");
  ROSE_ASSERT(canonical->get_parent() == declarator_scope);
  ROSE_ASSERT(!canonical->get_isAutonomousDeclaration());
  ROSE_ASSERT(std::count(declarator_scope->get_declarations().begin(),
                         declarator_scope->get_declarations().end(),
                         canonical) == 1);
  ROSE_ASSERT(canonical->get_scope() == friend_function->get_scope());
}

void requireNamespaceProducerContract(SgNode *root) {
  bool saw_standard_library_namespace = false;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgNamespaceDeclarationStatement)) {
    SgNamespaceDeclarationStatement *declaration =
        isSgNamespaceDeclarationStatement(node);
    ROSE_ASSERT(declaration != nullptr);

    if (isSgAuxiliaryDeclarationList(declaration->get_parent()) != nullptr) {
      ROSE_ASSERT(!declaration->has_source_fragments());
      continue;
    }

    SgScopeStatement *lexical_scope =
        isSgScopeStatement(declaration->get_parent());
    ROSE_ASSERT(lexical_scope != nullptr);
    ROSE_ASSERT(declaration->get_scope() == lexical_scope);
    ROSE_ASSERT(declarationOccurrences(lexical_scope, declaration) == 1);
    declaration->validate_source_fragments();

    SgNamespaceDefinitionStatement *definition = declaration->get_definition();
    ROSE_ASSERT(definition != nullptr);
    ROSE_ASSERT(definition->get_parent() == declaration);
    ROSE_ASSERT(declaration->get_opening_source_fragment()->get_parent() ==
                declaration);
    ROSE_ASSERT(declaration->get_closing_source_fragment()->get_parent() ==
                declaration);
    if (declaration->get_opening_source_fragment()->get_source_form() ==
        SgNamespaceSourceFragment::e_namespace_source_fragment_source_spelled) {
      ROSE_ASSERT(declaration->get_translation_unit_source_order().has_value());
    }

    const std::string name = declaration->get_name().getString();
    saw_standard_library_namespace |= name == "std" || name == "__gnu_cxx";
  }
  ROSE_ASSERT(saw_standard_library_namespace);
}

void requirePhysicalRange(const SgLocatedNode *node, const std::string &suffix,
                          int line, int start_column, int end_column) {
  ROSE_ASSERT(node != nullptr);
  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(!start->isCompilerGenerated());
  ROSE_ASSERT(!start->isTransformation());
  ROSE_ASSERT(start->isSameFile(*end));
  ROSE_ASSERT(endsWith(start->get_physical_filename(), suffix));
  ROSE_ASSERT(endsWith(end->get_physical_filename(), suffix));
  ROSE_ASSERT(start->get_line() == line);
  ROSE_ASSERT(end->get_line() == line);
  ROSE_ASSERT(start->get_col() == start_column);
  ROSE_ASSERT(end->get_col() == end_column);
}

void requireCopiedFragments(SgNamespaceDeclarationStatement *original) {
  ROSE_ASSERT(original != nullptr);
  SgTreeCopy tree_copy;
  SgNamespaceDeclarationStatement *copy =
      isSgNamespaceDeclarationStatement(original->copy(tree_copy));
  ROSE_ASSERT(copy != nullptr);
  ROSE_ASSERT(copy != original);
  copy->validate_source_fragments();
  ROSE_ASSERT(copy->get_opening_source_fragment() !=
              original->get_opening_source_fragment());
  ROSE_ASSERT(copy->get_closing_source_fragment() !=
              original->get_closing_source_fragment());
  ROSE_ASSERT(copy->get_opening_source_fragment()->get_parent() == copy);
  ROSE_ASSERT(copy->get_closing_source_fragment()->get_parent() == copy);
  ROSE_ASSERT(copy->get_opening_source_fragment()->get_kind() ==
              SgNamespaceSourceFragment::e_namespace_source_fragment_opening);
  ROSE_ASSERT(copy->get_closing_source_fragment()->get_kind() ==
              SgNamespaceSourceFragment::e_namespace_source_fragment_closing);
  SgNamespaceSourceFragment *original_introducer =
      original->get_opening_introducer_source_fragment();
  SgNamespaceSourceFragment *copied_introducer =
      copy->get_opening_introducer_source_fragment();
  ROSE_ASSERT((original_introducer == nullptr) ==
              (copied_introducer == nullptr));
  if (original_introducer != nullptr) {
    ROSE_ASSERT(copied_introducer != original_introducer);
    ROSE_ASSERT(copied_introducer->get_parent() == copy);
    ROSE_ASSERT(copied_introducer->get_kind() ==
                SgNamespaceSourceFragment::
                    e_namespace_source_fragment_opening_introducer);
    ROSE_ASSERT(copied_introducer->get_contains_namespace_name() ==
                original_introducer->get_contains_namespace_name());
  }
  ROSE_ASSERT(copy->get_translation_unit_source_order() ==
              original->get_translation_unit_source_order());
  // A detached deep-copy root is not a valid finished AST statement.  Destroy
  // the isolated verification copy before the global memory-pool consistency
  // checks inspect live nodes.
  SageInterface::deleteAST(
      copy, SageInterface::DeleteAstMode::kSkipExternalReferences);
  ROSE_ASSERT(!SgNode::isLiveNode(copy));
}

void requireClosingComment(SgNamespaceDeclarationStatement *declaration,
                           const std::string &comment_text, int line) {
  ROSE_ASSERT(declaration != nullptr);
  AttachedPreprocessingInfoType *attached =
      declaration->getAttachedPreprocessingInfo();
  ROSE_ASSERT(attached != nullptr);
  PreprocessingInfo *match = nullptr;
  for (PreprocessingInfo *entry : *attached) {
    ROSE_ASSERT(entry != nullptr);
    const std::string text = entry->getString();
    const size_t first = text.find_first_not_of(" \t\r\n");
    ROSE_ASSERT(first == std::string::npos || text[first] != '}');
    if (text.find(comment_text) == std::string::npos) {
      continue;
    }
    ROSE_ASSERT(match == nullptr);
    match = entry;
  }
  ROSE_ASSERT(match != nullptr);
  ROSE_ASSERT(match->getRelativePosition() == PreprocessingInfo::after_syntax);
  Sg_File_Info *comment_info = match->get_file_info();
  ROSE_ASSERT(comment_info != nullptr);
  ROSE_ASSERT(comment_info->get_physical_file_id() >= 0);
  ROSE_ASSERT(comment_info->get_line() == line);
  ROSE_ASSERT(comment_info->isSameFile(
      *declaration->get_closing_source_fragment()->get_endOfConstruct()));
}
} // namespace

int main(int argc, char **argv) {
  if (std::getenv("REX_TEST_ZERO_EXPANDED_TOKEN_ORDER") != nullptr) {
    (void)ClangExpandedTokenOrder::unique(0);
    return 1;
  }

  ClangExpandedTokenOrder uniqueOrder = ClangExpandedTokenOrder::unique(7);
  uniqueOrder.publish(7);
  ROSE_ASSERT(uniqueOrder.uniqueOrder().has_value());
  ROSE_ASSERT(*uniqueOrder.uniqueOrder() == 7);
  uniqueOrder.publish(8);
  ROSE_ASSERT(!uniqueOrder.uniqueOrder().has_value());

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  requireNamespaceProducerContract(project);
  // Translating std::vector<int> first reaches std::initializer_list<int>
  // through a re-entrant record placeholder; the initializer-list expression
  // in main reaches the same specialization through its type spelling.  Both
  // producer paths must publish one exact declaration/type identity.
  requireCanonicalTemplateInstantiationIdentity(
      project, SgName("initializer_list<int>"));
  requireSemanticBooleanTemplateArgumentProvenance(project);
  requireFriendParameterTagSourceOwner(project);

  SgNamespaceDeclarationStatement *split =
      findNamespace(project, "rex_split_fragment");
  ROSE_ASSERT(split->get_opening_source_fragment()->get_kind() ==
              SgNamespaceSourceFragment::e_namespace_source_fragment_opening);
  ROSE_ASSERT(split->get_closing_source_fragment()->get_kind() ==
              SgNamespaceSourceFragment::e_namespace_source_fragment_closing);
  requirePhysicalRange(split->get_opening_source_fragment(),
                       "rex_frontend_namespace_source_fragments.hpp", 4, 1, 30);
  requirePhysicalRange(split->get_closing_source_fragment(),
                       "rex_frontend_namespace_source_fragments.cpp", 4, 1, 1);
  requirePhysicalRange(split, "rex_frontend_namespace_source_fragments.hpp", 4,
                       1, 30);
  requirePhysicalRange(split->get_definition(),
                       "rex_frontend_namespace_source_fragments.hpp", 4, 30,
                       30);
  requireClosingComment(split, "namespace rex_split_fragment", 4);
  requireCopiedFragments(split);

  SgNamespaceDeclarationStatement *nested =
      findNamespace(project, "rex_nested_split_fragment");
  requirePhysicalRange(nested->get_opening_source_fragment(),
                       "rex_frontend_namespace_source_fragments.hpp", 5, 1, 37);
  requirePhysicalRange(nested->get_closing_source_fragment(),
                       "rex_frontend_namespace_source_fragments.cpp", 3, 1, 1);
  requirePhysicalRange(nested, "rex_frontend_namespace_source_fragments.hpp", 5,
                       1, 37);
  requirePhysicalRange(nested->get_definition(),
                       "rex_frontend_namespace_source_fragments.hpp", 5, 37,
                       37);
  requireClosingComment(nested, "namespace rex_nested_split_fragment", 3);
  requireCopiedFragments(nested);

  SgNamespaceDeclarationStatement *split_introducer =
      findNamespace(project, "rex_split_introducer");
  SgNamespaceSourceFragment *introducer =
      split_introducer->get_opening_introducer_source_fragment();
  ROSE_ASSERT(introducer != nullptr);
  ROSE_ASSERT(introducer->get_kind() ==
              SgNamespaceSourceFragment::
                  e_namespace_source_fragment_opening_introducer);
  ROSE_ASSERT(!introducer->get_contains_namespace_name());
  requirePhysicalRange(introducer,
                       "rex_frontend_namespace_split_introducer.hpp", 1, 1, 1);
  requirePhysicalRange(split_introducer->get_opening_source_fragment(),
                       "rex_frontend_namespace_source_fragments.cpp", 73, 1,
                       22);
  ROSE_ASSERT(
      split_introducer->get_translation_unit_source_order() ==
      std::optional<unsigned int>(
          introducer->get_startOfConstruct()->get_source_sequence_number()));
  requireCopiedFragments(split_introducer);

  SgNamespaceDeclarationStatement *macro =
      findNamespace(project, "rex_macro_fragment");
  SgVariableDeclaration *after_split =
      findVariable(project, "rex_after_split_namespace");
  SgGlobal *global_scope = isSgGlobal(split->get_parent());
  ROSE_ASSERT(global_scope != nullptr);
  ROSE_ASSERT(macro->get_parent() == global_scope);
  ROSE_ASSERT(after_split->get_parent() == global_scope);
  const SgDeclarationStatementPtrList &global_declarations =
      global_scope->get_declarations();
  const auto split_position =
      std::find(global_declarations.begin(), global_declarations.end(), split);
  const auto after_split_position = std::find(
      global_declarations.begin(), global_declarations.end(), after_split);
  const auto macro_position =
      std::find(global_declarations.begin(), global_declarations.end(), macro);
  ROSE_ASSERT(split_position != global_declarations.end());
  ROSE_ASSERT(after_split_position != global_declarations.end());
  ROSE_ASSERT(macro_position != global_declarations.end());
  ROSE_ASSERT(std::distance(global_declarations.begin(), split_position) <
              std::distance(global_declarations.begin(), after_split_position));
  ROSE_ASSERT(std::distance(global_declarations.begin(), after_split_position) <
              std::distance(global_declarations.begin(), macro_position));
  ROSE_ASSERT(split->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(after_split->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(macro->get_translation_unit_source_order().has_value());
  ROSE_ASSERT(*split->get_translation_unit_source_order() <
              *after_split->get_translation_unit_source_order());
  ROSE_ASSERT(*after_split->get_translation_unit_source_order() <
              *macro->get_translation_unit_source_order());
  requirePhysicalRange(macro->get_opening_source_fragment(),
                       "rex_frontend_namespace_source_fragments.cpp", 11, 1,
                       38);
  requirePhysicalRange(macro->get_closing_source_fragment(),
                       "rex_frontend_namespace_source_fragments.cpp", 13, 1,
                       21);
  ROSE_ASSERT(
      macro->get_startOfConstruct()->isSameFile(macro->get_endOfConstruct()));
  ROSE_ASSERT(macro->get_startOfConstruct()->get_line() == 11);
  ROSE_ASSERT(macro->get_endOfConstruct()->get_line() == 13);
  requireCopiedFragments(macro);

  SgNamespaceDeclarationStatement *inline_fragment =
      findNamespace(project, "rex_inline_fragment");
  ROSE_ASSERT(inline_fragment->get_isInlinedNamespace());
  SgNamespaceDefinitionStatement *inline_owner =
      isSgNamespaceDefinitionStatement(inline_fragment->get_parent());
  ROSE_ASSERT(inline_owner != nullptr);
  ROSE_ASSERT(inline_owner->get_namespaceDeclaration() != nullptr);
  ROSE_ASSERT(
      inline_owner->get_namespaceDeclaration()->get_name().getString() ==
      "rex_inline_owner");
  requireCopiedFragments(inline_fragment);

  const std::vector<SgNamespaceDeclarationStatement *> reopened =
      findNamespaces(project, "rex_reopened_fragment");
  ROSE_ASSERT(reopened.size() == 2);
  ROSE_ASSERT(reopened[0] != reopened[1]);
  ROSE_ASSERT(reopened[0]->get_firstNondefiningDeclaration() == reopened[0]);
  ROSE_ASSERT(reopened[1]->get_firstNondefiningDeclaration() == reopened[0]);
  ROSE_ASSERT(reopened[0]->get_definition()->get_nextNamespaceDefinition() ==
              reopened[1]->get_definition());
  ROSE_ASSERT(
      reopened[1]->get_definition()->get_previousNamespaceDefinition() ==
      reopened[0]->get_definition());
  ROSE_ASSERT(reopened[0]->get_parent() == reopened[1]->get_parent());
  ROSE_ASSERT(reopened[0]->get_scope() == reopened[0]->get_parent());
  ROSE_ASSERT(reopened[1]->get_scope() == reopened[1]->get_parent());
  ROSE_ASSERT(
      declarationOccurrences(isSgScopeStatement(reopened[0]->get_parent()),
                             reopened[0]) == 1);
  ROSE_ASSERT(
      declarationOccurrences(isSgScopeStatement(reopened[1]->get_parent()),
                             reopened[1]) == 1);

  const std::vector<SgNamespaceDeclarationStatement *> function_reopenings =
      findNamespaces(project, "rex_reopened_function_fragment");
  const std::vector<SgFunctionDeclaration *> fragment_functions =
      findFunctions(project, "rex_cross_fragment");
  ROSE_ASSERT(function_reopenings.size() == 3);
  ROSE_ASSERT(fragment_functions.size() == 3);
  SgNamespaceDefinitionStatement *canonical_function_scope =
      function_reopenings.front()->get_definition()->get_global_definition();
  ROSE_ASSERT(canonical_function_scope != nullptr);
  SgFunctionDeclaration *canonical_function = fragment_functions.front();
  SgFunctionDeclaration *function_redeclaration = fragment_functions[1];
  SgFunctionDeclaration *function_definition = fragment_functions[2];
  ROSE_ASSERT(canonical_function->get_definition() == nullptr);
  ROSE_ASSERT(function_redeclaration->get_definition() == nullptr);
  ROSE_ASSERT(function_definition->get_definition() != nullptr);
  ROSE_ASSERT(canonical_function->get_firstNondefiningDeclaration() ==
              canonical_function);
  ROSE_ASSERT(function_redeclaration->get_firstNondefiningDeclaration() ==
              canonical_function);
  ROSE_ASSERT(function_definition->get_firstNondefiningDeclaration() ==
              canonical_function);
  ROSE_ASSERT(canonical_function->get_definingDeclaration() ==
              function_definition);
  ROSE_ASSERT(function_redeclaration->get_definingDeclaration() ==
              function_definition);
  ROSE_ASSERT(function_definition->get_definingDeclaration() ==
              function_definition);
  for (size_t index = 0; index < fragment_functions.size(); ++index) {
    SgNamespaceDefinitionStatement *lexical_owner =
        isSgNamespaceDefinitionStatement(
            fragment_functions[index]->get_parent());
    ROSE_ASSERT(lexical_owner == function_reopenings[index]->get_definition());
    ROSE_ASSERT(lexical_owner->get_global_definition() ==
                canonical_function_scope);
    if (fragment_functions[index]->get_scope() != canonical_function_scope) {
      fprintf(stderr,
              "REX_TEST_INVARIANT[reopened-function-semantic-scope]: "
              "index=%zu function=%p parent=%p lexical=%p global=%p "
              "scope=%p explicit=%d\n",
              index, static_cast<void *>(fragment_functions[index]),
              static_cast<void *>(fragment_functions[index]->get_parent()),
              static_cast<void *>(lexical_owner),
              static_cast<void *>(canonical_function_scope),
              static_cast<void *>(fragment_functions[index]->get_scope()),
              fragment_functions[index]->hasExplicitScope() ? 1 : 0);
      ROSE_ABORT();
    }
    if (lexical_owner != canonical_function_scope) {
      ROSE_ASSERT(findDirectFunctionSymbols(lexical_owner, "rex_cross_fragment")
                      .empty());
    }
  }
  std::vector<SgFunctionSymbol *> fragment_symbols =
      findDirectFunctionSymbols(canonical_function_scope, "rex_cross_fragment");
  ROSE_ASSERT(fragment_symbols.size() == 1);
  ROSE_ASSERT(fragment_symbols.front()->get_declaration() ==
              canonical_function);
  ROSE_ASSERT(fragment_symbols.front()->get_symbol_basis() ==
              canonical_function);
  ROSE_ASSERT(fragment_symbols.front()->get_parent() ==
              canonical_function_scope->get_symbol_table());
  ROSE_ASSERT(fragment_symbols.front()->get_scope() ==
              canonical_function_scope);

  const std::vector<SgNamespaceDeclarationStatement *> macro_reopened =
      findNamespaces(project, "rex_macro_reopened_fragment");
  ROSE_ASSERT(macro_reopened.size() == 2);
  SgVariableDeclaration *macro_first =
      findVariable(project, "macro_first_value");
  SgVariableDeclaration *macro_second =
      findVariable(project, "macro_second_value");
  SgNamespaceDefinitionStatement *macro_first_owner =
      isSgNamespaceDefinitionStatement(macro_first->get_parent());
  SgNamespaceDefinitionStatement *macro_second_owner =
      isSgNamespaceDefinitionStatement(macro_second->get_parent());
  ROSE_ASSERT(macro_first_owner == macro_reopened[0]->get_definition());
  ROSE_ASSERT(macro_second_owner == macro_reopened[1]->get_definition());
  ROSE_ASSERT(macro_first_owner != macro_second_owner);

  const Sg_File_Info *macro_first_opening =
      macro_reopened[0]->get_opening_source_fragment()->get_startOfConstruct();
  const Sg_File_Info *macro_second_opening =
      macro_reopened[1]->get_opening_source_fragment()->get_startOfConstruct();
  ROSE_ASSERT(macro_first_opening != nullptr);
  ROSE_ASSERT(macro_second_opening != nullptr);
  ROSE_ASSERT(macro_first_opening->get_physical_file_id() ==
              macro_second_opening->get_physical_file_id());
  ROSE_ASSERT(macro_first_opening->get_line() ==
              macro_second_opening->get_line());
  ROSE_ASSERT(macro_first_opening->get_col() ==
              macro_second_opening->get_col());

  for (size_t index = 0; index < macro_reopened.size(); ++index) {
    SgNamespaceDeclarationStatement *namespace_declaration =
        macro_reopened[index];
    SgVariableDeclaration *member = index == 0 ? macro_first : macro_second;
    ROSE_ASSERT(
        namespace_declaration->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(member->get_translation_unit_source_order().has_value());
    ROSE_ASSERT(*namespace_declaration->get_translation_unit_source_order() <
                *member->get_translation_unit_source_order());
  }
  ROSE_ASSERT(*macro_reopened[0]->get_translation_unit_source_order() <
              *macro_reopened[1]->get_translation_unit_source_order());

  AstTests::runAllTests(project);
  return 0;
}
