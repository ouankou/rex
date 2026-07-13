//! PDF generator for AST
// This is the same as exampleTranslators/PDFGenerator/pdfGenerator.C
#include "rose.h"

#include "RoseAst.h"

using namespace std;
int main(int argc, char **argv) {
  vector<string> argvList(argv, argv + argc);
  if (CommandlineProcessing::isOption(argvList, "", "--help", false)) {
    printf("--------------Tool specific instructions for pdfGenerator----\n");
    printf("To dump AST from input files only :  pdfGenerator "
           "your_input_file.c\n");
    printf("To dump full AST, including headers: pdfGenerator "
           "-rose:convertFullAST your_input_file.c\n");
    printf("---------------end of tool specific instructions --------------\n");
    return 1;
  }

  // accept -rose:convertFullAST
  if (CommandlineProcessing::isOption(argvList, "-rose:", "convertFullAST",
                                      true))
    bool dumpFullAST = true;

  SgProject *project = frontend(argvList);
  ROSE_ASSERT(project != NULL);

  RoseAst ast(project);
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    SgExpression *expression = isSgExpression(*i);
    if (expression == nullptr || !expression->has_semantic_value_type() ||
        isSgTypeUnknown(expression->get_type()) == nullptr) {
      continue;
    }
    SgInitializedName *owner = isSgInitializedName(expression->get_parent());
    SgExpression *operand = nullptr;
    if (SgAssignInitializer *initializer = isSgAssignInitializer(expression)) {
      operand = initializer->get_operand_i();
    }
    fprintf(stderr,
            "REX_TEST_INVARIANT[expression-type]: expression=%p type=%s "
            "owner=%s operand=%p operand-type=%s operand-result-type=%s "
            "file=%s line=%d owns SgTypeUnknown\n",
            static_cast<void *>(expression), expression->class_name().c_str(),
            owner != nullptr ? owner->get_name().getString().c_str() : "",
            static_cast<void *>(operand),
            operand != nullptr ? operand->class_name().c_str() : "<null>",
            operand != nullptr && operand->get_type() != nullptr
                ? operand->get_type()->class_name().c_str()
                : "<null>",
            expression->get_file_info() != nullptr
                ? expression->get_file_info()->get_filenameString().c_str()
                : "<unknown>",
            expression->get_file_info() != nullptr
                ? expression->get_file_info()->get_line()
                : 0);
    ROSE_ABORT();
  }

  std::string filename = SageInterface::generateProjectName(project);

  if (project->get_verbose() > 0) {
    cout << "Generating AST tree (" << numberOfNodes() << " nodes) in file "
         << filename << ".pdf.\n";
  }

  AstPDFGeneration astpdfgen;
  astpdfgen.generateInputFiles(project);
  return 0;
}
