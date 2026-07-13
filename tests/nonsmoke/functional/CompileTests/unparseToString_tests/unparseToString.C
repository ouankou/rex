#include "rose.h"

#include <iostream>

bool unparse_all = false;

class TypeUnparserOnAST : public AstSimpleProcessing {
public:
  void visit(SgNode *n) {
    SgStatement *statement = isSgStatement(n);
    Sg_File_Info *statement_info =
        statement != nullptr ? statement->get_file_info() : nullptr;
    const bool exact_source_statement =
        statement_info != nullptr && statement_info->get_line() > 0 &&
        statement_info->get_physical_file_id() >= 0 &&
        !statement_info->isCompilerGenerated() &&
        !statement_info->isFrontendSpecific() &&
        !statement_info->isSourcePositionUnavailableInFrontend() &&
        statement_info->isOutputInCodeGeneration() &&
        isSgAuxiliaryDeclarationList(statement->get_parent()) == nullptr &&
        isSgCatchStatementSeq(statement) == nullptr &&
        isSgFunctionParameterList(statement) == nullptr &&
        isSgCtorInitializerList(statement) == nullptr;
    const bool hasTypedStandaloneEmitter =
        isSgProject(n) != nullptr || isSgSourceFile(n) != nullptr ||
        isSgExpression(n) != nullptr || exact_source_statement;
    if (unparse_all && hasTypedStandaloneEmitter) {
      if (SgFunctionDeclaration *d = isSgFunctionDeclaration(n)) {
        std::cout << "function declaration, name = "
                  << d->get_name().getString() << std::endl;
      }
      std::cout << n->unparseToString() << std::endl;
    }

    SgInitializedName *initialized_name = isSgInitializedName(n);
    Sg_File_Info *name_info = initialized_name != nullptr
                                  ? initialized_name->get_file_info()
                                  : nullptr;
    if (initialized_name != nullptr &&
        initialized_name->get_type() != nullptr && name_info != nullptr &&
        name_info->get_line() > 0 && name_info->get_physical_file_id() >= 0 &&
        !name_info->isCompilerGenerated() && !name_info->isFrontendSpecific() &&
        !name_info->isSourcePositionUnavailableInFrontend()) {
      SgSourceFile *source_file =
          SageInterface::getEnclosingSourceFile(initialized_name);
      SgScopeStatement *scope = initialized_name->get_scope();
      ROSE_ASSERT(source_file != nullptr);
      ROSE_ASSERT(scope != nullptr);
      SgUnparse_Info info;
      info.set_current_source_file(source_file);
      info.set_current_scope(scope);
      info.set_template_argument_qualification_context(
          SageInterface::getEnclosingStatement(initialized_name));
      info.set_reference_node_for_qualification(initialized_name);
      std::cout << initialized_name->get_type()->unparseToString(&info)
                << std::endl;
    }
  }
};

int main(int argc, char **argv) {
  // Parse command line args for things we recognize
  for (int argno = 1; argno < argc; ++argno) {
    if (!strcmp(argv[argno], "--all")) {
      unparse_all = true;
      memmove(argv + argno, argv + argno + 1, (argc - argno) * sizeof(argv[0]));
      --argno;
      --argc;
    }
  }

  // The frontend() will parse the rest of the arguments
  SgProject *p = frontend(argc, argv);
  ROSE_ASSERT(p);

  TypeUnparserOnAST t1;
  t1.traverse(p, preorder);
}
