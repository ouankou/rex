#include <string>

#include <vector>

#include "CommandOptions.h"

#include "LoopInfoInterface.h"

#include "LoopTransformInterface.h"

#include "LoopUnroll.h"

#include "SymbolicVal.h"

#include "sageBuilder.h"
#include "sageInterface.h"

unsigned LoopUnrolling::unrollsize = 0;
LoopUnrolling::UnrollOpt LoopUnrolling::opt = DEFAULT;
std::string LoopUnrolling::cmdline_help() {
  return "-unroll [-locond] [-nvar] [factor] : unrolling innermost "
         "loops by factor (default 4)";
}

void LoopUnrolling::cmdline_configure(const std::vector<std::string> &argv,
                                      std::vector<std::string> *unknown_args) {
  unsigned index = 0;
  if (!cmdline_find(argv, index, "-unroll", unknown_args))
    return;
  opt = DEFAULT;
  ++index;
  if (index < argv.size() && argv[index] == "-locond") {
    opt = (LoopUnrolling::UnrollOpt)(opt | LoopUnrolling::COND_LEFTOVER);
    ++index;
  } else if (index < argv.size() && argv[index] == "-lo_skip") {
    ++index;
  } else
    opt = (LoopUnrolling::UnrollOpt)(opt | LoopUnrolling::COND_LEFTOVER);
  if (index < argv.size() && argv[index] == "-nvar") {
    opt = (LoopUnrolling::UnrollOpt)(opt | LoopUnrolling::USE_NEWVAR);
    ++index;
  }
  if (index < argv.size() && argv[index] == "poet") {
    std::cerr << "REX_LOOP_OPTION[unsupported-poet]: -unroll poet requires "
                 "the removed POET backend\n";
    ROSE_ABORT();
  }
  unrollsize = 4;
  readOptionalPositiveSize(argv, index, "-unroll", unrollsize);
  if (unknown_args != 0)
    append_args(argv, index, *unknown_args);
}

bool LoopUnrolling::operator()(AstInterface &fa, const AstNodePtr &s,
                               AstNodePtr &r) {
  bool isLoop = false;
  if (enclosingloop == s ||
      (enclosingloop == AST_NULL && (isLoop = fa.IsLoop(s)))) {
    for (enclosingloop = fa.GetParent(s);
         enclosingloop != AST_NULL && !fa.IsLoop(enclosingloop);
         enclosingloop = fa.GetParent(enclosingloop))
      ;
    if (!isLoop)
      return false;
  }

  AstNodePtr body;
  SymbolicVal stepval, ubval, lbval;
  SymbolicVar ivar;
  if (!SymbolicValGenerator::IsFortranLoop(fa, s, &ivar, &lbval, &ubval,
                                           &stepval, &body))
    return false;

  SymbolicVal nstepval = stepval * unrollsize;
  SymbolicVal nubval = ubval;

  bool hasleft = true, negativeStep = (stepval < 0);
  AstNodePtr leftbody, lefthead;

  int stepnum = 0, loopnum = 0;
  SymbolicVal loopval = ubval - lbval + 1;
  if (stepval.isConstInt(stepnum) && loopval.isConstInt(loopnum) &&
      !(loopnum % (stepnum * unrollsize))) {
    hasleft = false;
  } else {
    nubval = ubval - SymbolicVal(unrollsize - 1);
    if (opt & COND_LEFTOVER) {
      leftbody = fa.CreateBlock();
      lefthead = leftbody;
    } else {
      leftbody = fa.CopyAstTree(body);
      lefthead =
          fa.CreateLoop(ivar.CodeGen(fa), AstNodePtr(), ubval.CodeGen(fa),
                        stepval.CodeGen(fa), leftbody, negativeStep);
    }
  }
  AstNodePtr emptyBody(SageBuilder::buildBasicBlock());
  ROSE_ASSERT(fa.ReplaceAst(body, emptyBody));
  AstNodePtr s1 =
      fa.CreateLoop(ivar.CodeGen(fa), lbval.CodeGen(fa), nubval.CodeGen(fa),
                    nstepval.CodeGen(fa), body, negativeStep);

  AstNodePtr origbody = fa.CopyAstTree(body);
  SymbolicVal nvar;
  if (opt & USE_NEWVAR) {
    AstNodePtr nvarSymbol;
    std::string nvarname = fa.NewVar(fa.GetType("int"), "", true, false, body,
                                     ivar.CodeGen(fa), &nvarSymbol);
    nvar = SymbolicVar(nvarname, body, nvarSymbol);
  }
  for (unsigned i = 1; i < unrollsize; ++i) {
    AstNodePtr bodycopy = fa.CopyAstTree(origbody);
    if (opt & USE_NEWVAR) {
      AstNodePtr nvarassign =
          fa.CreateAssignment(nvar.CodeGen(fa), (nvar + 1).CodeGen(fa));
      fa.BlockAppendStmt(body, nvarassign);
      AstTreeReplaceVar repl(ivar, nvar);
      repl(fa, bodycopy);
    } else {
      AstTreeReplaceVar repl(ivar, ivar + i);
      repl(fa, bodycopy);
    }
    fa.BlockAppendStmt(body, bodycopy);
    if (hasleft && (opt & COND_LEFTOVER)) {
      AstNodePtr cond =
          fa.CreateBinaryOP(AstInterface::BOP_LE, ivar.CodeGen(fa),
                            (ubval - (i - 1)).CodeGen(fa));
      AstNodePtr body1 = fa.CopyAstTree(bodycopy);
      AstNodePtr ifstmt = fa.CreateIf(cond, body1);
      fa.BlockAppendStmt(leftbody, ifstmt);
      leftbody = body1;
    }
  }
  AstNodePtr replacement = s1;
  if (hasleft) {
    if (fa.GetParent(s) != AST_NULL &&
        SageInterface::isRemovableStatement(isSgStatement(s.get_ptr()))) {
      fa.InsertStmt(s, lefthead, false, true);
    } else {
      replacement = AstNodePtr(SageBuilder::buildBasicBlock());
      fa.BlockAppendStmt(replacement, s1);
      fa.BlockAppendStmt(replacement, lefthead);
    }
  }
  r = replacement;
  return true;
}
