/**
 *  \file Preprocess/Preprocess.cc
 *  \brief Outlining preprocessor.
 *
 *  This module implements functionality to transform an arbitrary
 *  outlineable statement into a canonical form that simplifies the
 *  core outlining algorithm.
 */
#include "sage3basic.h"

#include <iostream>

#include "Outliner.hh"

#include "Preprocess.hh"

using namespace std;
using namespace SageInterface;

void Outliner::Preprocess::checkAndPatchUpOptions() {
  // useStructureWrapper is a sub option of useParameterWrapper
  // Setting useStructureWrapper means useParameterWrapper should be true also
  if (useStructureWrapper && !useParameterWrapper) {
    cout << "Warning: Outliner::useParameterWrapper is automatically set to be "
            "true since useStructureWrapper is set."
         << endl;
    useParameterWrapper = true;
  }
}

SgBasicBlock *Outliner::Preprocess::preprocessOutlineTarget(SgStatement *s) {
#ifdef __linux__
  if (enable_debug)
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
#endif

  checkAndPatchUpOptions();

  ROSE_ASSERT(s);
  SgBasicBlock *s_post = isSgBasicBlock(s);
  if (s_post == NULL) {
    s_post = createBlock(s);
  }
  ROSE_ASSERT(s_post);

  // Extract preprocessing control structure.
  // We skip this during OpenMP lowering, it has some bug
  // Liao, 9/8/2009
  if (!SageInterface::getEnclosingFileNode(s)->get_openmp_lowering())
    s_post = transformPreprocIfs(s_post);

  // Convert 'this' expressions into references to a local pointer.
  s_post = transformThisExprs(s_post);

  // Transform non-local control flow.
  // We skip this during OpenMP lowering for two reasons:
  //  1. non-local jump within a parallel region is illegal
  //  2. The support for Fortran is lacking.
  // TODO better solution is to add Fortran support and report illegal non-local
  // jumps Liao, 1/10/2011
  if (!SageInterface::getEnclosingFileNode(s)->get_openmp_lowering())
    s_post = transformNonLocalControlFlow(s_post);

  // Make sure we duplicate any locally declared function prototypes.
  gatherNonLocalDecls(s_post);

  ROSE_ASSERT(s_post);
  return s_post;
}
