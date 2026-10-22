#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"
#include "unparseCxx.h"

#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Surface = SgFunctionCallExp::source_operator_surface_enum;
using CalleeForm = SgFunctionCallExp::source_operator_callee_form_enum;

SgFunctionDefinition *findDefinition(SgProject *project,
                                     const std::string &name) {
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDefinition *definition = isSgFunctionDefinition(node);
    if (definition != nullptr && definition->get_declaration() != nullptr &&
        definition->get_declaration()->get_name() == name) {
      return definition;
    }
  }
  return nullptr;
}

void checkDispatch(SgProject *project) {
  unsigned destructors = 0;
  for (SgNode *node : RoseAst(project)) {
    auto *reference = isSgNonrealRefExp(node);
    if (reference == nullptr ||
        reference->get_name_kind() != SgNonrealRefExp::e_destructor_id)
      continue;
    ROSE_ASSERT(reference->get_semantic_role() ==
                SgNonrealRefExp::e_dependent_callable);
    ROSE_ASSERT(reference->get_symbol()->get_name() == "value_type");
    auto *receiver = isSgBinaryOp(reference->get_parent());
    ROSE_ASSERT(isSgDotExp(receiver) || isSgArrowExp(receiver));
    ROSE_ASSERT(receiver->get_rhs_operand() == reference);
    ROSE_ASSERT(isSgNonrealType(reference->get_type()));
    auto *copy = isSgBinaryOp(SageInterface::copyExpression(receiver));
    auto *copied_reference = isSgNonrealRefExp(copy->get_rhs_operand());
    ROSE_ASSERT(copied_reference != reference &&
                copied_reference->get_parent() == copy);
    ROSE_ASSERT(copied_reference->get_name_kind() ==
                SgNonrealRefExp::e_destructor_id);
    SageInterface::deleteAST(copy);
    using namespace Rose::AstJson;
    auto *declaration = reference->get_symbol()->get_declaration();
    const auto encoded =
        parseJson(rawTypeOwnedExpressionRef(reference, {{declaration, 1}}));
    auto *restored =
        isSgNonrealRefExp(expressionFromRef(encoded, {{1, declaration}}));
    ROSE_ASSERT(restored != nullptr &&
                restored->get_name_kind() == SgNonrealRefExp::e_destructor_id);
    ROSE_ASSERT(restored->get_symbol() == reference->get_symbol());
    SageInterface::deleteAST(restored);
    for (const auto &invalid : std::map<std::string, JsonValue>{
             {"semantic_role", JsonValue::number("0")},
             {"explicit_template_argument_list", JsonValue::boolean(true)},
             {"template_arguments", JsonValue::null()},
             {"resolved_function_declaration", JsonValue::number("1")},
             {"resolved_variable_declaration", JsonValue::number("1")}}) {
      auto malformed = encoded;
      malformed.object.at("properties").object.at(invalid.first) =
          invalid.second;
      bool rejected = false;
      try {
        expressionFromRef(malformed, {});
      } catch (const std::runtime_error &error) {
        rejected = std::string(error.what()) ==
                   "AST JSON dependent destructor has invalid identity";
      }
      ROSE_ASSERT(rejected);
    }
    ++destructors;
  }
  ROSE_ASSERT(destructors == 2);
  const std::pair<const char *, bool> cases[] = {
      {"rex_dispatch_reference", false},
      {"rex_dispatch_pointer", false},
      {"rex_dispatch_dereference", false},
      {"rex_dispatch_operator", false},
      {"rex_dispatch_qualified_reference", true},
      {"rex_dispatch_qualified_pointer", true}};
  for (const auto &test : cases) {
    auto *definition = findDefinition(project, test.first);
    ROSE_ASSERT(definition != nullptr);
    unsigned calls = 0;
    for (SgNode *node : RoseAst(definition)) {
      if (auto *call = isSgFunctionCallExp(node)) {
        ROSE_ASSERT(call->getAssociatedFunctionDeclaration() != nullptr);
        ROSE_ASSERT((call->getAssociatedFunctionSymbol() != nullptr) ==
                    test.second);
        ++calls;
      }
    }
    ROSE_ASSERT(calls == 1);
  }
  auto *lambda_function = findDefinition(project, "rex_dispatch_lambda");
  ROSE_ASSERT(lambda_function != nullptr);
  unsigned lambdas = 0;
  for (SgNode *node : RoseAst(lambda_function)) {
    if (auto *lambda = isSgLambdaExp(node)) {
      auto *copy = isSgLambdaExp(SageInterface::copyExpression(lambda));
      ROSE_ASSERT(copy != nullptr);
      auto *call = SageBuilder::buildFunctionCallExp_nfi(
          copy, SageBuilder::buildIntType(),
          SageBuilder::buildExprListExp_nfi());
      auto *symbol = call->getAssociatedFunctionSymbol();
      ROSE_ASSERT(symbol != nullptr);
      ROSE_ASSERT(
          symbol->get_declaration() ==
          copy->get_lambda_function()->get_firstNondefiningDeclaration());
      SageInterface::deleteAST(call);
      ++lambdas;
    }
  }
  ROSE_ASSERT(lambdas == 1);

  auto *named_operator = findDefinition(project, "rex_explicit_named_operator");
  ROSE_ASSERT(named_operator != nullptr);
  auto *file = SageInterface::getEnclosingSourceFile(named_operator);
  ROSE_ASSERT(file != nullptr);
  std::ostringstream output;
  Unparser unparser(&output, file->getFileName(), Unparser_Opt{});
  unparser.currentFile = file;
  SgUnparse_Info info;
  info.set_current_source_file(file);
  unsigned named_calls = 0;
  for (SgNode *node : RoseAst(named_operator)) {
    auto *call = isSgFunctionCallExp(node);
    if (call == nullptr ||
        call->getAssociatedFunctionDeclaration()->get_name() != "operator+")
      continue;
    ROSE_ASSERT(!call->get_uses_operator_syntax());
    auto *argument = call->get_args()->get_expressions().at(1);
    ROSE_ASSERT(isSgFunctionCallExp(argument) || isSgAddOp(argument));
    ROSE_ASSERT(!unparser.u_exprStmt->requiresParentheses(argument, info));

    auto *copy = isSgFunctionCallExp(SageInterface::copyExpression(call));
    ROSE_ASSERT(copy != nullptr);
    auto *old_callee = copy->get_function();
    auto *reference = SageBuilder::buildFunctionRefExp(
        call->getAssociatedFunctionDeclaration());
    copy->set_function(reference);
    reference->set_parent(copy);
    old_callee->set_parent(nullptr);
    SageInterface::deleteAST(old_callee);
    ROSE_ASSERT(!unparser.u_exprStmt->requiresParentheses(
        copy->get_args()->get_expressions().at(1), info));
    auto *old_argument = copy->get_args()->get_expressions().at(1);
    auto *comma = SageBuilder::buildCommaOpExp(SageBuilder::buildIntVal(1),
                                               SageBuilder::buildIntVal(2),
                                               SageBuilder::buildIntType());
    copy->get_args()->get_expressions().at(1) = comma;
    comma->set_parent(copy->get_args());
    old_argument->set_parent(nullptr);
    SageInterface::deleteAST(old_argument);
    ROSE_ASSERT(!comma->get_need_paren());
    ROSE_ASSERT(unparser.u_exprStmt->requiresParentheses(comma, info));
    SageInterface::deleteAST(copy);
    ++named_calls;
  }
  ROSE_ASSERT(named_calls == 2);
}

void roundTrip(SgProject *project) {
  SgSourceFile *source = nullptr;
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *candidate = isSgSourceFile(file);
        candidate != nullptr && !candidate->get_isHeaderFile()) {
      ROSE_ASSERT(source == nullptr);
      source = candidate;
    }
  }
  ROSE_ASSERT(source != nullptr);
  using namespace Rose::AstJson;
  constexpr Checkpoint checkpoint = Checkpoint::PreOmpConstruction;
  AstFileRecord ast = parseAstFileJson(buildJson(source, checkpoint, source),
                                       checkpointName(checkpoint));
  SgSourceFile *copy = reconstructSourceFile(ast, source);
  replaceFileInProject(source, copy);
}

std::vector<unsigned char> roles(SgFunctionCallExp *call) {
  const SgUnsignedCharList stored = call->get_source_operator_operand_roles();
  return std::vector<unsigned char>(stored.begin(), stored.end());
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  checkDispatch(project);
  roundTrip(project);
  checkDispatch(project);

  SgFunctionDefinition *definition =
      findDefinition(project, "rex_frontend_operator_surface_contract");
  ROSE_ASSERT(definition != nullptr);

  const unsigned char source = SgFunctionCallExp::e_source_operator_operand;
  const unsigned char semantic = SgFunctionCallExp::e_semantic_operator_operand;
  struct Expected {
    CalleeForm callee_form;
    std::vector<unsigned char> operand_roles;
  };
  const std::map<Surface, Expected> expected = {
      {SgFunctionCallExp::e_prefix_plus,
       {SgFunctionCallExp::e_member_operator_callee, {}}},
      {SgFunctionCallExp::e_prefix_minus,
       {SgFunctionCallExp::e_nonmember_operator_callee, {source}}},
      {SgFunctionCallExp::e_binary_plus,
       {SgFunctionCallExp::e_member_operator_callee, {source}}},
      {SgFunctionCallExp::e_binary_multiply,
       {SgFunctionCallExp::e_nonmember_operator_callee, {source, source}}},
      {SgFunctionCallExp::e_binary_spaceship,
       {SgFunctionCallExp::e_nonmember_operator_callee, {source, source}}},
      {SgFunctionCallExp::e_prefix_increment,
       {SgFunctionCallExp::e_member_operator_callee, {}}},
      {SgFunctionCallExp::e_postfix_increment,
       {SgFunctionCallExp::e_member_operator_callee, {semantic}}},
      {SgFunctionCallExp::e_prefix_decrement,
       {SgFunctionCallExp::e_nonmember_operator_callee, {source}}},
      {SgFunctionCallExp::e_postfix_decrement,
       {SgFunctionCallExp::e_nonmember_operator_callee, {source, semantic}}},
      {SgFunctionCallExp::e_call_operator_surface,
       {SgFunctionCallExp::e_member_operator_callee, {source, source}}},
      {SgFunctionCallExp::e_subscript_operator_surface,
       {SgFunctionCallExp::e_member_operator_callee, {source}}},
      {SgFunctionCallExp::e_arrow_operator_surface,
       {SgFunctionCallExp::e_member_operator_callee, {}}},
      {SgFunctionCallExp::e_user_defined_literal_surface,
       {SgFunctionCallExp::e_nonmember_operator_callee, {semantic, semantic}}},
  };

  std::map<Surface, SgFunctionCallExp *> calls;
  std::size_t additional_nonmember_binary_plus = 0;
  for (SgNode *node : RoseAst(definition)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    if (call == nullptr || call->get_source_operator_surface() ==
                               SgFunctionCallExp::e_no_operator_surface) {
      continue;
    }
    const Surface surface = call->get_source_operator_surface();
    auto expected_surface = expected.find(surface);
    ROSE_ASSERT(expected_surface != expected.end());
    const bool prefix = surface == SgFunctionCallExp::e_prefix_plus ||
                        surface == SgFunctionCallExp::e_prefix_minus ||
                        surface == SgFunctionCallExp::e_prefix_increment ||
                        surface == SgFunctionCallExp::e_prefix_decrement;
    const bool postfix = surface == SgFunctionCallExp::e_postfix_increment ||
                         surface == SgFunctionCallExp::e_postfix_decrement;
    const bool index =
        surface == SgFunctionCallExp::e_call_operator_surface ||
        surface == SgFunctionCallExp::e_subscript_operator_surface;
    std::vector<SgExpression *> expressions{call};
    if (isSgFunctionCallExp(call->get_function()) == nullptr) {
      expressions.push_back(call->get_function());
    }
    for (SgExpression *expression : expressions) {
      ROSE_ASSERT(SageInterface::isPrefixOperator(expression) == prefix);
      ROSE_ASSERT(SageInterface::isPostfixOperator(expression) == postfix);
      ROSE_ASSERT(SageInterface::isIndexOperator(expression) == index);
    }
    if (SgBinaryOp *access = isSgBinaryOp(call->get_function());
        isSgDotExp(access) != nullptr || isSgArrowExp(access) != nullptr) {
      SgExpression *reference = access->get_rhs_operand();
      ROSE_ASSERT(SageInterface::isPrefixOperator(reference) == prefix);
      ROSE_ASSERT(SageInterface::isPostfixOperator(reference) == postfix);
      ROSE_ASSERT(SageInterface::isIndexOperator(reference) == index);
    }
    if (surface == SgFunctionCallExp::e_binary_plus &&
        call->get_source_operator_callee_form() ==
            SgFunctionCallExp::e_nonmember_operator_callee &&
        roles(call) == std::vector<unsigned char>{source, source}) {
      ROSE_ASSERT(call->get_uses_operator_syntax());
      ROSE_ASSERT(call->get_args() != nullptr);
      ROSE_ASSERT(call->get_args()->get_expressions().size() == 2);
      ++additional_nonmember_binary_plus;
      continue;
    }
    ROSE_ASSERT(calls.emplace(surface, call).second);
    ROSE_ASSERT(call->get_uses_operator_syntax());
    ROSE_ASSERT(call->get_source_operator_callee_form() ==
                expected_surface->second.callee_form);
    ROSE_ASSERT(roles(call) == expected_surface->second.operand_roles);
    ROSE_ASSERT(call->get_args() != nullptr);
    ROSE_ASSERT(call->get_args()->get_expressions().size() ==
                call->get_source_operator_operand_roles().size());
  }
  ROSE_ASSERT(calls.size() == expected.size());
  ROSE_ASSERT(additional_nonmember_binary_plus == 1);

  auto requireCanonicalPostfixDummy = [](SgFunctionCallExp *call,
                                         std::size_t index) {
    ROSE_ASSERT(call != nullptr && call->get_args() != nullptr);
    const SgExpressionPtrList &arguments = call->get_args()->get_expressions();
    ROSE_ASSERT(index < arguments.size());
    SgIntVal *dummy = isSgIntVal(arguments[index]);
    ROSE_ASSERT(dummy != nullptr);
    ROSE_ASSERT(dummy->get_parent() == call->get_args());
    ROSE_ASSERT(dummy->get_value() == 0);
    ROSE_ASSERT(dummy->get_valueString().empty());
    ROSE_ASSERT(dummy->get_literal_spelling_form() ==
                SgValueExp::e_literal_canonical_generated);

    Sg_File_Info *primary = dummy->get_file_info();
    Sg_File_Info *start = dummy->get_startOfConstruct();
    Sg_File_Info *end = dummy->get_endOfConstruct();
    Sg_File_Info *operator_position = dummy->get_operatorPosition();
    ROSE_ASSERT(primary != nullptr && start != nullptr && end != nullptr &&
                operator_position != nullptr);
    ROSE_ASSERT(primary == operator_position);
    ROSE_ASSERT(start != end && start != operator_position &&
                end != operator_position);
    for (Sg_File_Info *position : {start, end, operator_position}) {
      ROSE_ASSERT(position->get_parent() == dummy);
      ROSE_ASSERT(position->isCompilerGenerated());
      ROSE_ASSERT(position->isFrontendSpecific());
      ROSE_ASSERT(!position->isTransformation());
      ROSE_ASSERT(!position->isSourcePositionUnavailableInFrontend());
      ROSE_ASSERT(position->get_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(position->get_physical_file_id() ==
                  Sg_File_Info::COMPILER_GENERATED_FILE_ID);
      ROSE_ASSERT(position->get_source_sequence_number() == 0);
    }
  };
  requireCanonicalPostfixDummy(calls.at(SgFunctionCallExp::e_postfix_increment),
                               0);
  requireCanonicalPostfixDummy(calls.at(SgFunctionCallExp::e_postfix_decrement),
                               1);

  SgFunctionCallExp *literal_call =
      calls.at(SgFunctionCallExp::e_user_defined_literal_surface);
  ROSE_ASSERT(literal_call->get_source_user_defined_literal_operands() !=
              nullptr);
  ROSE_ASSERT(
      literal_call->get_source_user_defined_literal_operands()->get_parent() ==
      literal_call);
  ROSE_ASSERT(literal_call->get_source_user_defined_literal_operands()
                  ->get_tokens()
                  .size() == 1);
  ROSE_ASSERT(literal_call->get_source_user_defined_literal_suffix_roles() ==
              SgUnsignedCharList{
                  SgFunctionCallExp::e_user_defined_literal_token_with_suffix});

  int explicit_spaceship_calls = 0;
  for (SgNode *node : RoseAst(definition)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    if (call == nullptr || call->get_source_operator_surface() !=
                               SgFunctionCallExp::e_no_operator_surface) {
      continue;
    }
    SgCastExp *decay = isSgCastExp(call->get_function());
    if (decay == nullptr ||
        decay->get_semantic_conversion_kind() !=
            SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
      continue;
    }
    SgNonrealRefExp *reference = isSgNonrealRefExp(decay->get_operand());
    if (reference == nullptr) {
      continue;
    }
    SgFunctionDeclaration *resolved =
        SageInterface::requireResolvedFunctionTemplateReference(
            reference, "operator surface regression");
    SgTemplateInstantiationFunctionDecl *instantiation =
        isSgTemplateInstantiationFunctionDecl(resolved);
    if (instantiation != nullptr &&
        instantiation->get_templateName() == "operator<=>") {
      ROSE_ASSERT(call->getAssociatedFunctionDeclaration() ==
                  resolved->get_firstNondefiningDeclaration());
      ROSE_ASSERT(!call->get_uses_operator_syntax());
      ROSE_ASSERT(reference->get_templateArguments().size() == 1);
      ROSE_ASSERT(reference->get_templateArguments().front() != nullptr);
      ROSE_ASSERT(reference->get_templateArguments().front()->get_parent() ==
                  reference);
      ROSE_ASSERT(reference->get_templateArguments()
                      .front()
                      ->get_explicitlySpecified());
      ++explicit_spaceship_calls;
    }
  }
  ROSE_ASSERT(explicit_spaceship_calls == 1);

  int explicit_member_template_calls = 0;
  for (SgNode *node : RoseAst(definition)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    if (call == nullptr) {
      continue;
    }
    SgExpression *callee = call->get_function();
    while (SgCastExp *cast = isSgCastExp(callee)) {
      cast->validate_semantic_conversion();
      ROSE_ASSERT(cast->get_cast_type() == SgCastExp::e_implicit_cast);
      ROSE_ASSERT(cast->get_operand() != nullptr);
      callee = cast->get_operand();
    }
    SgNonrealRefExp *reference = nullptr;
    if (SgDotExp *dot = isSgDotExp(callee)) {
      reference = isSgNonrealRefExp(dot->get_rhs_operand());
    } else if (SgArrowExp *arrow_call = isSgArrowExp(callee)) {
      reference = isSgNonrealRefExp(arrow_call->get_rhs_operand());
    }
    if (reference == nullptr ||
        reference->get_resolved_function_declaration() == nullptr) {
      continue;
    }
    SgFunctionDeclaration *resolved =
        SageInterface::requireResolvedFunctionTemplateReference(
            reference, "explicit member template regression");
    SgTemplateInstantiationMemberFunctionDecl *instantiation =
        isSgTemplateInstantiationMemberFunctionDecl(resolved);
    if (instantiation == nullptr ||
        instantiation->get_templateName() != "convert") {
      continue;
    }
    ROSE_ASSERT(call->getAssociatedFunctionDeclaration() ==
                resolved->get_firstNondefiningDeclaration());
    ROSE_ASSERT(reference->get_templateArguments().size() == 1);
    ROSE_ASSERT(reference->get_templateArguments().front() != nullptr);
    ROSE_ASSERT(reference->get_templateArguments().front()->get_parent() ==
                reference);
    ROSE_ASSERT(
        reference->get_templateArguments().front()->get_explicitlySpecified());
    ++explicit_member_template_calls;
  }
  ROSE_ASSERT(explicit_member_template_calls == 2);

  const char *malformed = std::getenv("REX_TEST_MALFORMED_OPERATOR_SURFACE");
  if (malformed != nullptr) {
    const std::string mode = malformed;
    if (mode == "missing-role") {
      calls.at(SgFunctionCallExp::e_binary_plus)
          ->set_source_operator_operand_roles(SgUnsignedCharList{});
    } else if (mode == "semantic-source") {
      SgUnsignedCharList binary_roles =
          calls.at(SgFunctionCallExp::e_binary_plus)
              ->get_source_operator_operand_roles();
      ROSE_ASSERT(binary_roles.size() == 1);
      binary_roles.front() = semantic;
      calls.at(SgFunctionCallExp::e_binary_plus)
          ->set_source_operator_operand_roles(binary_roles);
    } else if (mode == "wrong-surface") {
      calls.at(SgFunctionCallExp::e_prefix_minus)
          ->set_source_operator_surface(SgFunctionCallExp::e_binary_divide);
    } else if (mode == "wrong-callee") {
      calls.at(SgFunctionCallExp::e_binary_plus)
          ->set_source_operator_callee_form(
              SgFunctionCallExp::e_nonmember_operator_callee);
    } else if (mode == "udl-missing-lexical") {
      calls.at(SgFunctionCallExp::e_user_defined_literal_surface)
          ->set_source_user_defined_literal_operands(nullptr);
    } else if (mode == "udl-bad-suffix-role") {
      calls.at(SgFunctionCallExp::e_user_defined_literal_surface)
          ->set_source_user_defined_literal_suffix_roles(SgUnsignedCharList{2});
    } else {
      ROSE_ABORT();
    }
  }

  AstTests::runAllTests(project);
  return backend(project);
}
