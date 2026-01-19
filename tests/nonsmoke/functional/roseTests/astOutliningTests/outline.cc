/*!
 *  \file outline.cc
 *
 *  \brief Demonstrates the pragma-interface of the Outliner.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 *          Chunhua Liao <liao6@llnl.gov>
 *  This utility has a special option, "-rose:outline:preproc-only",
 *  which can be used just to see the results of the outliner's
 *  preprocessing phase.
 */

#include "rose.h"

#include <algorithm>

#include <cstdlib>

#include <iostream>

#include <string>

#include "Outliner.hh"

#include "commandline_processing.h"

//! Generates a PDF into the specified file.
static void makePDF(const SgProject *proj,
                    const std::string & = std::string(""));

// =====================================================================

using namespace std;

// =====================================================================

int main(int argc, char *argv[]) {
  // Accepting command line options to the outliner
  vector<string> argvList(argv, argv + argc);
  Outliner::commandLineProcessing(argvList);

  SgProject *project = frontend(argvList);
  ROSE_ASSERT(project != NULL);

  // Make sure we have a valid AST before we do the outlining.
  AstTests::runAllTests(project);

  bool make_pdfs = project->get_verbose() >= 2;
  if (make_pdfs)
    makePDF(project);

  if (!project->get_skip_transformation()) {
    Outliner::outlineAll(project);

    // Rerun the test on the AST with the outlined code
    // AstTests::runAllTests(project);
  }

  const int MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH = 3000;
  // printf ("Generate whole AST graph if small enough (size = %d)
  // \n",numberOfNodes());
  generateAstGraph(project, MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH);
  // printf ("DONE: Generate whole AST graph if small enough \n");

  if (numberOfNodes() < 4000) {
    std::string filename =
        SageInterface::generateProjectName(project) + "_copy_graph";

    ROSE_ASSERT(project->get_fileList().empty() == false);

    SgFile *originalSourceFile = project->get_fileList()[0];
    ROSE_ASSERT(originalSourceFile != NULL);

    set<SgNode *> originalSourceFileNodes = getAllNodes(originalSourceFile);

    graphNodesAfterCopy(originalSourceFileNodes, filename);
  }

  // Rerun the test on the AST with the outlined code
  AstTests::runAllTests(project);

  return backend(project);
}

// =====================================================================

static void makePDF_SgFile(const SgFile *f, string fn_prefix) {
  ROSE_ASSERT(f);

  string filename = fn_prefix + f->get_sourceFileNameWithoutPath();
  AstPDFGeneration pdf_gen;
  pdf_gen.generateWithinFile(filename, const_cast<SgFile *>(f));
}

static void makePDF(const SgProject *proj, const string &fn_prefix) {
  ROSE_ASSERT(proj);
  const SgFilePtrList &files = const_cast<SgProject *>(proj)->get_fileList();
  for_each(files.begin(), files.end(),
           [&](SgFile *file) { makePDF_SgFile(file, fn_prefix); });
}

// eof
