#include "RoseAst.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <cstdlib>
#include <map>
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
  roundTrip(project);

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
    ROSE_ASSERT(dummy->get_valueString() == "0");
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
                  ->get_expressions()
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
