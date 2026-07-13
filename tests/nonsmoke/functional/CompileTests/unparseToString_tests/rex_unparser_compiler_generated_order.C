#include "rose.h"

#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char **argv) {
  std::string output_name;
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "-rose:output") {
      output_name = argv[i + 1];
      break;
    }
  }
  if (output_name.empty()) {
    return 1;
  }

  SgProject *project = frontend(argc, argv);
  if (project == nullptr || project->get_fileList().size() != 1) {
    return 2;
  }
  SgSourceFile *source = isSgSourceFile(project->get_fileList().front());
  SgGlobal *global = source != nullptr ? source->get_globalScope() : nullptr;
  if (global == nullptr) {
    return 3;
  }

  SgVariableDeclaration *after = nullptr;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgVariableDeclaration *variable = isSgVariableDeclaration(declaration);
    if (variable != nullptr && !variable->get_variables().empty() &&
        variable->get_variables().front()->get_name() == "rex_order_after") {
      after = variable;
      break;
    }
  }
  if (after == nullptr) {
    return 4;
  }
  if (SageInterface::attachComment(after, "REX_ORDER_MARKER",
                                   PreprocessingInfo::C_StyleComment,
                                   PreprocessingInfo::before) == nullptr) {
    return 5;
  }

  SageBuilder::pushScopeStack(global);
  SgVariableDeclaration *generated = SageBuilder::buildVariableDeclaration(
      "rex_order_generated", SageBuilder::buildIntType(), nullptr, global);
  SageBuilder::popScopeStack();
  if (generated == nullptr) {
    return 6;
  }
  generated->setCompilerGenerated();
  generated->setOutputInCodeGeneration();
  SageInterface::insertStatementBefore(after, generated,
                                       /*autoMovePreprocessingInfo=*/false);

  project->unparse();

  std::ifstream output(output_name, std::ios::in | std::ios::binary);
  if (!output.is_open()) {
    return 7;
  }
  const std::string text((std::istreambuf_iterator<char>(output)),
                         std::istreambuf_iterator<char>());
  const size_t generated_position = text.find("rex_order_generated");
  const size_t marker_position = text.find("REX_ORDER_MARKER");
  const size_t after_position = text.find("rex_order_after");
  return !output.bad() && generated_position != std::string::npos &&
                 marker_position != std::string::npos &&
                 after_position != std::string::npos &&
                 generated_position < marker_position &&
                 marker_position < after_position
             ? 0
             : 8;
}
