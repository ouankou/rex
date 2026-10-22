#include "sage3basic.h"

#include "DepInfoAnal.h"

#include "ROSE_ASSERT.h"

#include "ReuseAnalysis.h"

#include "SymbolicBound.h"

#include <cstdio>
#include <limits>

class AstRefGetAccessStride : public ProcessAstNode<AstNodePtr> {
  AstNodePtr inductionDeclaration;
  bool Traverse(AstInterface &fa, const AstNodePtr &v,
                AstInterface::TraversalVisitType) override {
    return !fa.IsSameVarRef(v, inductionDeclaration);
  }

  int coefficient(AstInterface &fa, SgExpression *index,
                  const SymbolicVar &induction) {
    ASSERT_not_null(index);
    if (ReadAstTraverse(fa, index, *this, AstInterface::PostOrder))
      return 0;
    SymbolicBound bound;
    const SymbolicVal factor = UnwrapVarCond(
        SymbolicCond(REL_LE, SymbolicValGenerator::GetSymbolicVal(fa, index),
                     0),
        induction, bound);
    int value;
    if (!factor.isConstInt(value))
      unsupported(index, "index has no constant affine induction coefficient");
    return value;
  }

  [[noreturn]] static void unsupported(SgExpression *expression,
                                       const char *reason) {
    fprintf(stderr, "REX_LOCALITY_UNSUPPORTED[physical-stride]: %s: %s\n",
            expression->class_name().c_str(), reason);
    ROSE_ABORT();
  }

  static void addTerm(long long &stride, long long bits, int factor) {
    const long long magnitude =
        factor < 0 ? -static_cast<long long>(factor) : factor;
    const unsigned long long bitMagnitude =
        bits < 0 ? static_cast<unsigned long long>(-(bits + 1)) + 1
                 : static_cast<unsigned long long>(bits);
    const bool negative = (bits < 0) != (factor < 0);
    const unsigned long long limit =
        static_cast<unsigned long long>(std::numeric_limits<long long>::max()) +
        negative;
    ROSE_ASSERT(magnitude == 0 || bitMagnitude <= limit / magnitude);
    const long long term = bits * factor;
    ROSE_ASSERT(term <= 0 ||
                stride <= std::numeric_limits<long long>::max() - term);
    ROSE_ASSERT(term >= 0 ||
                stride >= std::numeric_limits<long long>::min() - term);
    stride += term;
  }

public:
  long long operator()(const AstNodePtr &r, const SymbolicVar &induction) {
    inductionDeclaration = induction.GetVarDeclaration();
    ASSERT_require(inductionDeclaration != AST_NULL);
    AstInterface &ai = LoopTransformInterface::getAstInterface();
    if (!LoopTransformInterface::IsArrayAccess(r))
      return 0;
    SgExpression *expression = isSgExpression(r.get_ptr());
    ASSERT_not_null(expression);
    if (isSgDotExp(expression) || isSgArrowExp(expression))
      expression = isSgBinaryOp(expression)->get_lhs_operand();
    if (!isSgPntrArrRefExp(expression))
      unsupported(expression, "array abstraction has no typed physical access");
    long long stride = 0;
    bool indirect = false;
    while (true) {
      ASSERT_not_null(expression);
      if (auto *cast = isSgCastExp(expression)) {
        cast->validate_semantic_conversion();
        switch (cast->get_semantic_conversion_kind()) {
        case SgCastExp::e_semantic_conversion_NoOp:
        case SgCastExp::e_semantic_conversion_ArrayToPointerDecay:
          break;
        case SgCastExp::e_semantic_conversion_LValueToRValue:
          if (isSgPointerType(cast->get_type()->stripTypedefsAndModifiers()))
            indirect = true;
          break;
        default:
          unsupported(cast, "address conversion has no exact stride model");
        }
        expression = cast->get_operand();
        continue;
      }
      auto *access = isSgPntrArrRefExp(expression);
      if (access == nullptr) {
        if (!ReadAstTraverse(ai, expression, *this, AstInterface::PostOrder))
          unsupported(expression, "base address depends on the induction");
        break;
      }
      SgExpression *index = access->get_rhs_operand();
      ASSERT_not_null(index);
      const auto *list = isSgExprListExp(index);
      const auto &strides = access->get_subscript_stride_bits();
      const std::size_t count = list ? list->get_expressions().size() : 1;
      for (std::size_t axis = 0; axis < count; ++axis) {
        auto *subscript = list ? list->get_expressions()[axis] : index;
        if (isSgSubscriptExpression(subscript))
          unsupported(access, "section requires typed address-set semantics");
        const int factor = coefficient(ai, subscript, induction);
        if (factor != 0) {
          if (indirect)
            unsupported(access, "varying pointer load has no constant stride");
          if (strides.size() != count || !strides[axis])
            unsupported(access, "axis has no constant target bit stride");
          addTerm(stride, *strides[axis], factor);
        }
      }
      expression = access->get_lhs_operand();
    }
    return stride;
  }
};

float SelfSpatialReuse(const AstNodePtr &r, const SymbolicVar &induction,
                       unsigned linesize) {
  ROSE_ASSERT(linesize > 0);
  const long long signed_size = AstRefGetAccessStride()(r, induction);
  const unsigned long long size =
      signed_size < 0 ? static_cast<unsigned long long>(-(signed_size + 1)) + 1
                      : static_cast<unsigned long long>(signed_size);
  const auto lineBits = static_cast<unsigned long long>(linesize) * 8;
  return size > 0 && size < lineBits ? 1.0f - float(size) / lineBits : 0;
}

DepType TemporaryReuseRefs(DepInfoConstIterator ep,
                           Map2Object<AstNodePtr, DepDirection, int> &loopmap,
                           CollectObject<AstNodePtr> &refCollect, int *dist) {
  int mdist = 0;
  DepType reuseType = DEPTYPE_NONE;

  for (ep.Reset(); !ep.ReachEnd(); ++ep) {
    DepInfo d = ep.Current();
    DepType t = d.GetDepType();
    if (t != DEPTYPE_OUTPUT && t != DEPTYPE_INPUT && t != DEPTYPE_TRUE &&
        t != DEPTYPE_ANTI)
      continue;
    reuseType = (DepType)(reuseType | t);
    AstNodePtr src = d.SrcRef(), snk = d.SnkRef();
    ROSE_ASSERT(src != AST_NULL && snk != AST_NULL);
    int loop1 = loopmap(src, DEP_SRC), loop2 = loopmap(snk, DEP_SINK);
    ROSE_ASSERT(loop1 >= 0 && loop2 >= 0);
    DepRel r = d.Entry(loop1, loop2);
    int a = r.GetMaxAlign();
    if (a <= 0 && (dist == 0 || (*dist) < 0 || a >= -(*dist))) {
      refCollect(snk);
      if (mdist > 0 && mdist < -a)
        mdist = -a;
    } else if (dist == 0 || (*dist) < 0) {
      mdist = -1;
      refCollect(snk);
    }
  }
  if (dist != 0)
    *dist = mdist;
  return reuseType;
}
