#include "RoseAst.h"
#include "rose.h"
#include "sageAstJson.h"

#include <map>
#include <memory>
#include <string>

namespace {
std::map<std::string, SgFunctionCallExp *> calls(SgProject *project) {
  std::map<std::string, SgFunctionCallExp *> result;
  for (SgNode *node : RoseAst(project)) {
    auto *name = isSgInitializedName(node);
    if (name == nullptr ||
        name->get_name().getString().rfind("rex_udl_", 0) != 0)
      continue;
    auto *initializer = isSgAssignInitializer(name->get_initializer());
    ROSE_ASSERT(initializer != nullptr);
    auto *call = isSgFunctionCallExp(initializer->get_operand());
    ROSE_ASSERT(call != nullptr);
    ROSE_ASSERT(result.emplace(name->get_name().getString(), call).second);
  }
  ROSE_ASSERT(result.size() == 5);
  return result;
}

SgStringVal *stringArgument(SgFunctionCallExp *call) {
  SgStringVal *result = nullptr;
  for (SgNode *node : RoseAst(call->get_args())) {
    if (auto *literal = isSgStringVal(node)) {
      ROSE_ASSERT(result == nullptr);
      result = literal;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void verify(SgProject *project, bool edited) {
  auto literals = calls(project);
  for (const auto &[name, call] : literals) {
    ROSE_ASSERT(call->get_source_user_defined_literal_is_modified() ==
                (edited && name != "rex_udl_other_template"));
  }
  auto *cooked = isSgUnsignedLongLongIntVal(
      literals.at("rex_udl_cooked")->get_args()->get_expressions().front());
  ROSE_ASSERT(cooked != nullptr && cooked->get_value() == (edited ? 43 : 42));
  ROSE_ASSERT(stringArgument(literals.at("rex_udl_raw"))->get_byte_value() ==
              (edited ? "4567" : "123"));
  auto *raw_type =
      isSgArrayType(stringArgument(literals.at("rex_udl_raw"))->get_type());
  ROSE_ASSERT(raw_type != nullptr);
  auto *raw_bound = isSgValueExp(raw_type->get_index());
  ROSE_ASSERT(raw_bound != nullptr && SageInterface::getIntegerConstantValue(
                                          raw_bound) == (edited ? 5 : 4));
  ROSE_ASSERT(stringArgument(literals.at("rex_udl_text"))->get_byte_value() ==
              (edited ? "B\n" : "A\n"));
  AstTests::runAllTests(project);
}
} // namespace

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(frontendExitStatus(project.get()) == 0);
  verify(project.get(), false);
  auto literals = calls(project.get());
  auto *template_call = literals.at("rex_udl_template");
  auto *specialization = isSgTemplateInstantiationFunctionDecl(
      template_call->getAssociatedFunctionDeclaration());
  ROSE_ASSERT(specialization != nullptr);
  ROSE_ASSERT(specialization->get_templateArguments().empty());
  const auto &arguments = specialization->get_deducedTemplateArguments();
  ROSE_ASSERT(!arguments.empty());
  auto *digit = isSgCharVal(arguments.front()->get_expression());
  ROSE_ASSERT(digit != nullptr);
  digit->set_value(digit->get_value());
  ROSE_ASSERT(template_call->get_source_user_defined_literal_is_modified());
  ROSE_ASSERT(!literals.at("rex_udl_other_template")
                   ->get_source_user_defined_literal_is_modified());
  isSgUnsignedLongLongIntVal(
      literals.at("rex_udl_cooked")->get_args()->get_expressions().front())
      ->set_value(43);
  stringArgument(literals.at("rex_udl_raw"))
      ->set_value(SgStringVal::code_units_from_bytes("4567"));
  stringArgument(literals.at("rex_udl_text"))
      ->set_value(SgStringVal::code_units_from_bytes("B\n"));

  auto *decay = isSgCastExp(template_call->get_function());
  ROSE_ASSERT(decay != nullptr);
  auto *reference = isSgTemplateFunctionRefExp(decay->get_operand());
  ROSE_ASSERT(reference != nullptr);
  reference->set_semantic_function_declaration(
      literals.at("rex_udl_other_template")
          ->getAssociatedFunctionDeclaration());
  verify(project.get(), true);

  std::unique_ptr<SgProject> copy(SageInterface::deepCopy(project.get()));
  verify(copy.get(), true);
  SgFileList *copied_files = copy->get_fileList_ptr();
  SageInterface::deleteAST(copy.release(),
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  verify(project.get(), true);
  auto *file = isSgSourceFile(&project->get_file(0));
  file->get_originalCommandLineArgumentList().push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction);
  ROSE_ASSERT(restored != nullptr && restored != file);
  verify(project.get(), true);
  return backend(project.get());
}
