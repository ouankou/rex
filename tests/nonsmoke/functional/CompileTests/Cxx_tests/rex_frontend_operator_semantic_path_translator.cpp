#include "RoseAst.h"
#include "nodeQuery.h"
#include "rose.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
SgFunctionDefinition *findFunctionDefinition(SgProject *project,
                                             const std::string &name) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    SgFunctionDefinition *definition = isSgFunctionDefinition(node);
    SgFunctionDeclaration *declaration =
        definition != nullptr ? definition->get_declaration() : nullptr;
    if (declaration != nullptr && declaration->get_name() == name) {
      return definition;
    }
  }
  return nullptr;
}

bool parentHasChild(SgNode *parent, SgNode *child) {
  for (const std::pair<SgNode *, std::string> &entry :
       parent->returnDataMemberPointers()) {
    if (entry.first == child) {
      return true;
    }
  }
  return false;
}

SgFunctionDeclaration *canonicalFunction(SgFunctionDeclaration *declaration) {
  if (declaration == nullptr) {
    return nullptr;
  }
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  return canonical != nullptr ? canonical : declaration;
}

bool sameFunctionFamily(SgFunctionDeclaration *lhs,
                        SgFunctionDeclaration *rhs) {
  return lhs != nullptr && rhs != nullptr &&
         canonicalFunction(lhs) == canonicalFunction(rhs);
}

SgFunctionDeclaration *symbolFunctionDeclaration(SgSymbol *symbol) {
  if (SgTemplateFunctionSymbol *template_symbol =
          isSgTemplateFunctionSymbol(symbol)) {
    return isSgFunctionDeclaration(template_symbol->get_declaration());
  }
  if (SgFunctionSymbol *function_symbol = isSgFunctionSymbol(symbol)) {
    return function_symbol->get_declaration();
  }
  return nullptr;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDefinition *operator_test =
      findFunctionDefinition(project, "rex_frontend_operator_semantic_path");
  ROSE_ASSERT(operator_test != nullptr);

  std::map<std::string, int> direct_operator_call_counts;
  std::map<std::string, SgFunctionDeclaration *> direct_friend_operators;
  int arrow_call_count = 0;
  int explicit_operator_template_call_count = 0;
  RoseAst operator_ast(operator_test);
  for (RoseAst::iterator node = operator_ast.begin();
       node != operator_ast.end(); ++node) {
    if (SgExpression *expression = isSgExpression(*node)) {
      ROSE_ASSERT(expression->get_originalExpressionTree() == nullptr);
    }

    SgFunctionCallExp *call = isSgFunctionCallExp(*node);
    SgFunctionDeclaration *callee =
        call != nullptr ? call->getAssociatedFunctionDeclaration() : nullptr;
    const std::string name =
        callee != nullptr ? callee->get_name().getString() : "";
    if (call != nullptr && call->get_uses_operator_syntax() &&
        name != "operator->") {
      ++direct_operator_call_counts[name];
      if (name == "operator%" || name == "operator==") {
        direct_friend_operators[name] = callee;
      }
    }
    if (SgTemplateInstantiationFunctionDecl *instantiation =
            isSgTemplateInstantiationFunctionDecl(callee)) {
      if (instantiation->get_templateName() == "operator<<") {
        ROSE_ASSERT(!call->get_uses_operator_syntax());
        // The implicit specialization is a semantic declaration shared by
        // every call.  Explicitness and written type spelling belong to this
        // call's exact reference, not to that declaration.
        ROSE_ASSERT(!instantiation->get_template_argument_list_is_explicit());
        ROSE_ASSERT(!instantiation->get_templateArguments().empty());
        for (SgTemplateArgument *argument :
             instantiation->get_templateArguments()) {
          ROSE_ASSERT(argument != nullptr);
          if (argument->get_argumentType() ==
              SgTemplateArgument::start_of_pack_expansion_argument) {
            ROSE_ASSERT(!argument->get_explicitlySpecified());
            ROSE_ASSERT(argument->get_sourceSpelledType() == nullptr);
            continue;
          }
          ROSE_ASSERT(!argument->get_explicitlySpecified());
          ROSE_ASSERT(argument->get_sourceSpelledType() == nullptr);
        }

        SgCastExp *decay = isSgCastExp(call->get_function());
        ROSE_ASSERT(decay != nullptr);
        SgNonrealRefExp *reference = isSgNonrealRefExp(decay->get_operand());
        ROSE_ASSERT(reference != nullptr);
        ROSE_ASSERT(reference->get_explicit_template_argument_list());
        ROSE_ASSERT(!reference->get_templateArguments().empty());
        for (SgTemplateArgument *argument :
             reference->get_templateArguments()) {
          ROSE_ASSERT(argument != nullptr);
          ROSE_ASSERT(argument->get_explicitlySpecified());
          if (argument->get_argumentType() ==
              SgTemplateArgument::type_argument) {
            ROSE_ASSERT(argument->get_sourceSpelledType() != nullptr);
          }
        }
        ++explicit_operator_template_call_count;
      }
    }
    if (name == "operator->") {
      ROSE_ASSERT(call->get_uses_operator_syntax());
      ROSE_ASSERT(isSgArrowExp(call->get_parent()) != nullptr);
      ++arrow_call_count;
    }
  }

  const std::vector<std::string> expected_direct_calls{
      "operator=",  "operator+",  "operator-", "operator%",
      "operator==", "operator++", "operator[]"};
  for (const std::string &name : expected_direct_calls) {
    ROSE_ASSERT(direct_operator_call_counts[name] == 1);
  }
  ROSE_ASSERT(direct_operator_call_counts.size() ==
              expected_direct_calls.size());
  const std::vector<std::string> expected_friend_operators{"operator%",
                                                           "operator=="};
  for (const std::string &name : expected_friend_operators) {
    SgFunctionDeclaration *direct_friend_operator =
        direct_friend_operators[name];
    ROSE_ASSERT(direct_friend_operator != nullptr);
    SgFunctionDeclaration *canonical =
        canonicalFunction(direct_friend_operator);
    ROSE_ASSERT(canonical != nullptr);
    ROSE_ASSERT(canonical->get_firstNondefiningDeclaration() == canonical);

    SgFunctionDeclaration *defining =
        isSgFunctionDeclaration(canonical->get_definingDeclaration());
    if (defining != nullptr) {
      ROSE_ASSERT(defining != canonical);
      ROSE_ASSERT(defining->get_definition() != nullptr);
      ROSE_ASSERT(defining->get_definingDeclaration() == defining);
      ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == canonical);
    }

    SgSymbol *symbol = canonical->get_symbol_from_symbol_table();
    ROSE_ASSERT(symbol != nullptr);
    ROSE_ASSERT(symbolFunctionDeclaration(symbol) == canonical);
    SgSymbolTable *symbol_table = isSgSymbolTable(symbol->get_parent());
    ROSE_ASSERT(symbol_table != nullptr);
    SgScopeStatement *symbol_owner =
        isSgScopeStatement(symbol_table->get_parent());
    ROSE_ASSERT(isSgGlobal(symbol_owner) != nullptr ||
                isSgNamespaceDefinitionStatement(symbol_owner) != nullptr);
    ROSE_ASSERT(canonical->get_scope() == symbol_owner);

    bool found_lexical_friend = false;
    RoseAst project_ast(project);
    for (RoseAst::iterator candidate_node = project_ast.begin();
         candidate_node != project_ast.end(); ++candidate_node) {
      SgFunctionDeclaration *candidate =
          isSgFunctionDeclaration(*candidate_node);
      if (candidate == nullptr || candidate->get_name() != name ||
          !candidate->get_declarationModifier().isFriend()) {
        continue;
      }
      const bool exactSemanticFamily = sameFunctionFamily(candidate, canonical);
      if ((name == "operator==" && exactSemanticFamily) ||
          (name == "operator%" && !exactSemanticFamily)) {
        SgScopeStatement *lexical_parent =
            isSgScopeStatement(candidate->get_parent());
        if (isSgClassDefinition(lexical_parent) != nullptr ||
            isSgTemplateClassDefinition(lexical_parent) != nullptr ||
            isSgTemplateInstantiationDefn(lexical_parent) != nullptr) {
          found_lexical_friend = true;
        }
      }
    }
    ROSE_ASSERT(found_lexical_friend);
  }
  ROSE_ASSERT(arrow_call_count == 1);
  ROSE_ASSERT(explicit_operator_template_call_count == 1);

  SgFunctionDefinition *pointer_member_test = findFunctionDefinition(
      project, "rex_frontend_pointer_member_callable_type");
  ROSE_ASSERT(pointer_member_test != nullptr);
  size_t dot_star_count = 0;
  size_t arrow_star_count = 0;
  RoseAst pointer_member_ast(pointer_member_test);
  for (RoseAst::iterator node = pointer_member_ast.begin();
       node != pointer_member_ast.end(); ++node) {
    SgBinaryOp *member_selection = nullptr;
    if (SgDotStarOp *dot_star = isSgDotStarOp(*node)) {
      member_selection = dot_star;
      ++dot_star_count;
    } else if (SgArrowStarOp *arrow_star = isSgArrowStarOp(*node)) {
      member_selection = arrow_star;
      ++arrow_star_count;
    }
    if (member_selection == nullptr) {
      continue;
    }

    SgExpression *lhs = member_selection->get_lhs_operand();
    SgExpression *rhs = member_selection->get_rhs_operand();
    ROSE_ASSERT(lhs != nullptr);
    ROSE_ASSERT(rhs != nullptr);
    ROSE_ASSERT(lhs->get_parent() == member_selection);
    ROSE_ASSERT(rhs->get_parent() == member_selection);
    SgType *rhs_type = rhs->get_type();
    ROSE_ASSERT(rhs_type != nullptr);
    ROSE_ASSERT(isSgPointerMemberType(rhs_type->stripTypedefsAndModifiers()) !=
                nullptr);
    SgType *callable_type = member_selection->get_type();
    ROSE_ASSERT(callable_type != nullptr);
    ROSE_ASSERT(isSgFunctionType(callable_type) != nullptr ||
                isSgMemberFunctionType(callable_type) != nullptr);
    ROSE_ASSERT(isSgPointerMemberType(callable_type) == nullptr);
  }
  ROSE_ASSERT(dot_star_count == 1);
  ROSE_ASSERT(arrow_star_count == 1);

  SgFunctionDefinition *case_test =
      findFunctionDefinition(project, "rex_frontend_case_range_parent");
  ROSE_ASSERT(case_test != nullptr);
  std::vector<SgNode *> cases =
      NodeQuery::querySubTree(case_test, V_SgCaseOptionStmt);
  ROSE_ASSERT(cases.size() == 1);
  SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(cases.front());
  ROSE_ASSERT(case_stmt != nullptr);
  SgExpression *range_end = case_stmt->get_key_range_end();
  ROSE_ASSERT(range_end != nullptr);
  ROSE_ASSERT(range_end->get_parent() == case_stmt);
  ROSE_ASSERT(case_stmt->isChild(range_end));
  ROSE_ASSERT(parentHasChild(case_stmt, range_end));

  return backend(project);
}
