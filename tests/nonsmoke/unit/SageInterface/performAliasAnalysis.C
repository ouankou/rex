// testing alias analysis
#include "RoseAst.h"

#include "rose.h"

#include "CommandOptions.h"

// Array Annotation headers
#include "ArrayAnnot.h"

#include "ArrayRewrite.h"

#include "AstInterface.h"

#include "CPPAstInterface.h"

// Dependence graph headers
#include "AstInterface_ROSE.h"
// #include <LoopTransformInterface.h>

#include "AnnotCollect.h"

#include "OperatorAnnotation.h"
// #include <LoopTreeDepComp.h>

#include <iostream>

#include "test_output_path.h"
#include <set>
using namespace std;

// using a log file to avoid new screen output from interfering with correctness
// checking
ofstream ofile;

// a helper function
string toString(const SageInterface::VariableReferenceUse &use,
                SgSourceFile *sourceFile) {
  SgVarRefExp *ref = use.reference();
  ROSE_ASSERT(ref != NULL);
  ROSE_ASSERT(sourceFile != NULL);
  ROSE_ASSERT(ref->get_symbol() != NULL);
  SgInitializedName *declaration = ref->get_symbol()->get_declaration();
  ROSE_ASSERT(declaration != NULL);
  SgStatement *useSite = use.statement();
  ROSE_ASSERT(useSite != NULL);
  SgUnparse_Info info;
  info.set_current_source_file(sourceFile);
  info.set_template_argument_qualification_context(useSite);
  info.set_reference_node_for_qualification(ref);
  string ret;
  ret += ref->unparseToString(&info);
  ret += "@";
  ret += std::to_string(ref->get_file_info()->get_line());
  ret += ":";
  ret += std::to_string(ref->get_file_info()->get_col());
  return ret;
}

int main(int argc, char *argv[]) {
  vector<string> remainingArgs(argv, argv + argc);
  // Support -debugaliasanal
  // We must processing options first, before calling frontend!!
  //  work with the parser of the ArrayAbstraction module
  // Read in annotation files after -annot
  CmdOptions::GetInstance()->SetOptions(remainingArgs);
  bool dumpAnnot =
      CommandlineProcessing::isOption(remainingArgs, "", "-dumpannot", true);
  ArrayAnnotation *annot = ArrayAnnotation::get_inst();
  annot->register_annot();
  ReadAnnotation::get_inst()->read();
  if (dumpAnnot)
    annot->Dump();
  // Strip off custom options and their values to enable backend compiler
  CommandlineProcessing::removeArgsWithParameters(remainingArgs, "-annot");

  SgProject *project = frontend(remainingArgs);

  // our tests only use one single file for now.
  SgFilePtrList fl = project->get_files();
  SgSourceFile *firstfile = isSgSourceFile(fl[0]);
  ROSE_ASSERT(firstfile != NULL);

  string filename =
      Rose::StringUtility::stripPathFromFileName(firstfile->getFileName());
  string ofilename = filename + ".performAliasAnalysis.output";
  ofile.open(resolveTestOutputPath(ofilename).c_str());

  RoseAst ast(project);

  // function level side effect results
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    SgFunctionDeclaration *func = isSgFunctionDeclaration(*i);
    if (!func)
      continue;
    if (func->get_definition() == NULL)
      continue; // skip prototype declaration
    ofile << "--------------------------------------------------" << endl;
    ofile << "Function name:" << func->get_qualified_name() << endl;

    SgFunctionDefinition *defn = func->get_definition();

    SgBasicBlock *body = defn->get_body();
    AstInterfaceImpl faImpl_1(body);
    AstInterface fa(&faImpl_1);
    CPPAstInterface fa_body(&faImpl_1);

    // Pass annotations to arrayInterface and use them to collect
    // alias info. function info etc.
    ArrayAnnotation *annot = ArrayAnnotation::get_inst();
    ArrayInterface array_interface(*annot);

    array_interface.analyze(fa_body, AstNodePtrImpl(defn));
    // array_interface.initialize(fa_body, AstNodePtrImpl(defn));
    // array_interface.observe(fa_body);

    // LoopTransformInterface::set_aliasInfo(&array_interface);
    //  TODO: alternatively, dump the sets, less flexible later to test
    //  annotations query pointers for aliasing info. find all variable
    //  references  set1 ,  query mutual relation out put the pair if true
    //  relation is found.
    std::vector<SageInterface::VariableReferenceUse> uses;
    SageInterface::collectVariableReferenceUses(body, uses);

    for (size_t i = 0; i < uses.size(); i++)
      for (size_t j = 0; j < uses.size(); j++) {
        if (i >= j)
          continue; // only need to enumerate half.
        // we want to test what will happen if ref1 == ref2
        SgVarRefExp *ref1, *ref2;
        ref1 = uses[i].reference();
        ref2 = uses[j].reference();
        // skip references to the same variable
        if (ref1->get_symbol() == ref2->get_symbol())
          continue;

        AstNodePtr node1 = AstNodePtrImpl(ref1);
        AstNodePtr node2 = AstNodePtrImpl(ref2);
        if (array_interface.may_alias(fa, node1, node2))
          ofile << toString(uses[i], firstfile) << " <--> "
                << toString(uses[j], firstfile) << endl;
        else
          ofile << toString(uses[i], firstfile) << " No Aliasing  "
                << toString(uses[j], firstfile) << endl;
      }
  }

  ofile.close();
  // Generate source code from AST and call the vendor's compiler
  return backend(project);
}
