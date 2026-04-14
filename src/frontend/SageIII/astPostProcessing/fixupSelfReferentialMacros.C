#include "fixupSelfReferentialMacros.h"

#include "sage3basic.h"

#include "rose_config.h"

void fixupSelfReferentialMacrosInAST(SgNode *node) {
  TimingPerformance timer1("Fixup known self-referential macros:");
  (void)node;

  // REX root-cause fix:
  //
  // Clang already gives ROSE the fully expanded semantic AST for accesses such
  // as `x.sa_handler`, while the frontend separately tracks when exact
  // token-roundtrip must be preserved for self-referential macros. Injecting
  // synthetic `#undef` directives here mutates source spelling with
  // preprocessing attributes and breaks valid backend-conditional roundtrips
  // such as `test2014_66.c`.
  //
  // Leave the AST unchanged and let the frontend/unparser decide between
  // source-token replay and AST-based expanded output from real source
  // conditions.
}

void FixupSelfReferentialMacrosInAST::visit(SgNode *node) { (void)node; }
