#include "SymbolicVal.h"

#include "sage3basic.h"

#include "mlog.h"

#include "BooleanOperators.h"

#include "CommandOptions.h"

#include "SymbolicMultiply.h"

#include "SymbolicPlus.h"

#include "SymbolicSelect.h"

#include "UnaryOperators.h"

#include <list>

#include <sstream>

#include <stdio.h>
using namespace std;
void SymbolicValImpl ::Dump() const { std::cerr << toString(); }

void SymbolicVal::Dump() const { std::cerr << toString(); }

std::string SymbolicVal::toString() const {
  return (ConstPtr() != 0) ? ConstRef().toString() : std::string("");
}

SymbolicVal ::SymbolicVal(int val)
    : CountRefHandle<SymbolicValImpl>(new SymbolicConst(val)) {}

SymbolicConst::SymbolicConst(int _val, int _d)
    : val(""), type(_d == 1 ? "int" : "fraction"), intval(_val), dval(_d) {
  char buf[40];
  if (_d == 1)
    snprintf(buf, sizeof(buf), "%d", _val);
  else
    snprintf(buf, sizeof(buf), "%d/%d", _val, _d);
  val = buf;
}

SymbolicConst::SymbolicConst(std::string _val, std::string t)
    : val(_val), type(t) {
  if (type == "int") {
    intval = atoi(_val.c_str());
    dval = 1;
  }
}

std::string SymbolicConst ::toString() const { return val; }

AstNodePtr SymbolicConst ::CodeGen(AstInterface &fa) const {
  if (type == "int")
    return fa.CreateConstInt(intval);
  else
    return fa.CreateConstant(type, val);
}

std::string SymbolicVar ::toString() const { return varname; }

AstNodePtr SymbolicVar ::CodeGen(AstInterface &fa) const {
  if (exp_ != 0) {
    return fa.CopyAstTree(exp_);
  }
  return fa.CreateVarRef(varname, scope);
}

AstNodePtr SymbolicAstWrap::CodeGen(AstInterface &fa) const {
  if (codegen == 0)
    return fa.CopyAstTree(ast);
  else
    return (*codegen)(&fa, ast);
}

std::string SymbolicAstWrap::toString() const {
  return "AstWrap(" + AstInterface::AstToString(ast) + ")";
}

void SymbolicAstWrap::Dump() const {
  std::cerr << "AstWrap: " << ast.get_ptr();
  if (codegen != 0)
    std::cerr << "(codegen = " << codegen << ")\n";
}

bool SymbolicVar ::operator==(const SymbolicVar &that) const {
  return varname == that.varname &&
         (scope == that.scope || scope == AST_NULL || that.scope == AST_NULL);
}

std::string SymbolicFunction ::toString() const {
  std::string r = "(";
  if (args.size() == 0)
    r = "()";
  for (const_iterator i = args.begin(); i != args.end(); ++i) {
    r = r + (*i).toString() + ",";
  }
  r[r.size() - 1] = ')';
  return op.toString() + r;
}

bool SymbolicFunction::operator==(const SymbolicFunction &that) const {
  if (args.size() != that.args.size())
    return false;
  if (op != that.op)
    return false;
  for (const_iterator i = args.begin(), j = that.args.begin(); i != args.end();
       ++i, ++j) {
    if (*i != *j)
      return false;
  }
  return true;
}

AstNodePtr SymbolicFunction::CodeGen(AstInterface &_fa) const {
  AstNodeList l;
  for (const_iterator i = args.begin(); i != args.end(); ++i) {
    SymbolicVal cur = *i;
    AstNodePtr curast = cur.CodeGen(_fa);
    l.push_back(curast.get_ptr());
  }
  if (t == AstInterface::OP_NONE) {
    return _fa.CreateFunctionCall(op.CodeGen(_fa), l.begin(), l.end());
  } else if (t == AstInterface::OP_ARRAY_ACCESS) {
    if (l.size() < 2) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[codegen-arity]: operator="
                << AstInterface::toString(t) << " arguments=" << l.size()
                << std::endl;
      ROSE_ABORT();
    }
    AstNodeList::const_iterator b = l.begin();
    AstNodePtr arr = *b;
    for (++b; b != l.end(); ++b) {
      arr = _fa.CreateArrayAccess(arr, *b);
    }
    return arr;
  } else if (t == AstInterface::OP_ASSIGN) {
    if (l.size() != 2) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[codegen-arity]: operator="
                << AstInterface::toString(t) << " arguments=" << l.size()
                << std::endl;
      ROSE_ABORT();
    }
    return _fa.CreateAssignment(l.front(), l.back());
  } else if (t >= AstInterface::BOP_DOT_ACCESS &&
             t <= AstInterface::BOP_BIT_LSHIFT) {
    if (l.size() != 2) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[codegen-arity]: operator="
                << AstInterface::toString(t) << " arguments=" << l.size()
                << std::endl;
      ROSE_ABORT();
    }
    return _fa.CreateBinaryOP(t, l.front(), l.back());
  } else if (t >= AstInterface::UOP_MINUS &&
             t <= AstInterface::UOP_BIT_COMPLEMENT) {
    if (l.size() != 1) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[codegen-arity]: operator="
                << AstInterface::toString(t) << " arguments=" << l.size()
                << std::endl;
      ROSE_ABORT();
    }
    if (t == AstInterface::UOP_SEMANTIC_CONVERSION ||
        (t >= AstInterface::UOP_CAST_C &&
         t <= AstInterface::UOP_CAST_FUNCTIONAL_LIST)) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[codegen-conversion]: operator="
                << AstInterface::toString(t)
                << " requires an exact typed conversion representation"
                << std::endl;
      ROSE_ABORT();
    }
    return _fa.CreateUnaryOP(t, l.front());
  }
  std::cerr << "REX_SYMBOLIC_INVARIANT[codegen-operator]: operator="
            << AstInterface::toString(t) << std::endl;
  ROSE_ABORT();
}

SymbolicSemanticConversion::SymbolicSemanticConversion(
    const AstNodePtr &prototype, const SymbolicVal &operand)
    : SymbolicFunction(AstInterface::UOP_SEMANTIC_CONVERSION,
                       SymbolicVal(SymbolicAstWrap(prototype)),
                       Arguments{operand}),
      prototype(prototype) {
  SgCastExp *cast = isSgCastExp(prototype.get_ptr());
  if (cast == nullptr) {
    std::cerr << "REX_SYMBOLIC_INVARIANT[cast-prototype]: node="
              << prototype.get_ptr() << " is not an exact SgCastExp"
              << std::endl;
    ROSE_ABORT();
  }
  cast->validate_semantic_conversion();
}

AstNodePtr SymbolicSemanticConversion::CodeGen(AstInterface &fa) const {
  if (NumOfArgs() != 1) {
    std::cerr << "REX_SYMBOLIC_INVARIANT[cast-arity]: prototype="
              << prototype.get_ptr() << " arguments=" << NumOfArgs()
              << std::endl;
    ROSE_ABORT();
  }
  SgCastExp *original = isSgCastExp(prototype.get_ptr());
  SgCastExp *copy = isSgCastExp(fa.CopyAstTree(prototype).get_ptr());
  SgExpression *operand = isSgExpression(first_arg().CodeGen(fa).get_ptr());
  if (original == nullptr || copy == nullptr || operand == nullptr ||
      copy->get_operand() == nullptr ||
      !SageInterface::isEquivalentType(original->get_operand()->get_type(),
                                       operand->get_type())) {
    std::cerr << "REX_SYMBOLIC_INVARIANT[cast-rebuild]: prototype="
              << prototype.get_ptr() << " copy=" << copy
              << " operand=" << operand
              << " does not preserve the exact conversion operand type"
              << std::endl;
    ROSE_ABORT();
  }
  copy->get_operand()->set_parent(nullptr);
  copy->set_operand_i(operand);
  operand->set_parent(copy);
  copy->validate_semantic_conversion();
  return AstNodePtr(copy);
}

SymbolicVal
SymbolicSemanticConversion::cloneFunction(const Arguments &arguments) const {
  if (arguments.size() != 1) {
    std::cerr << "REX_SYMBOLIC_INVARIANT[cast-arity]: prototype="
              << prototype.get_ptr() << " arguments=" << arguments.size()
              << std::endl;
    ROSE_ABORT();
  }
  return new SymbolicSemanticConversion(prototype, arguments.front());
}

AstNodePtr SymbolicSelect::CodeGen(AstInterface &fa) const {
  int size = 0;
  AstInterface::AstNodeList list;
  for (OpdIterator iter = GetOpdIterator(); !iter.ReachEnd(); iter.Advance()) {
    AstNodePtr p = Term2Val(iter.Current()).CodeGen(fa);
    list.push_back(p.get_ptr());
    ++size;
  }
  assert(size > 1);
  std::string func = (opt < 0) ? "min" : "max";

  return fa.CreateFunctionCall(func, list.begin(), list.end());
}

void SymbolicBound::Union(const SymbolicBound &b2,
                          MapObject<SymbolicVal, SymbolicBound> *f) {
  lb = Min(lb, b2.lb, f);
  ub = Max(ub, b2.ub, f);
}

void SymbolicBound::Intersect(const SymbolicBound &b2,
                              MapObject<SymbolicVal, SymbolicBound> *f) {
  lb = Max(lb, b2.lb, f);
  ub = Min(ub, b2.ub, f);
}

void SymbolicBound::ReplaceVars(MapObject<SymbolicVal, SymbolicBound> &f) {
  lb = GetValLB(lb, f);
  ub = GetValUB(ub, f);
}

std::string RelToString(CompareRel r) {
  switch (r) {
  case REL_EQ:
    return "==";
  case REL_NE:
    return "!=";
  case REL_LT:
    return "<";
  case REL_GT:
    return ">";
  case REL_LE:
    return "<=";
  case REL_GE:
    return ">=";
  case REL_UNKNOWN:
    return "?";
  default:
    ROSE_ABORT();
  }
}

std::string SymbolicCond ::toString() const {
  std::string r = val1.toString() + RelToString(GetRelType()) + val2.toString();
  return r;
}

void SymbolicCond::Dump() const { std::cerr << toString(); }

AstNodePtr SymbolicCond ::CodeGen(AstInterface &fa) const {
  switch (GetRelType()) {
  case REL_EQ:
    return fa.CreateBinaryOP(AstInterface::BOP_EQ, val1.CodeGen(fa),
                             val2.CodeGen(fa));
  case REL_NE:
    return fa.CreateBinaryOP(AstInterface::BOP_NE, val1.CodeGen(fa),
                             val2.CodeGen(fa));
  case REL_LT:
    return fa.CreateBinaryOP(AstInterface::BOP_LT, val1.CodeGen(fa),
                             val2.CodeGen(fa));
  case REL_LE:
    return fa.CreateBinaryOP(AstInterface::BOP_LE, val1.CodeGen(fa),
                             val2.CodeGen(fa));
  case REL_GT:
    return fa.CreateBinaryOP(AstInterface::BOP_GT, val1.CodeGen(fa),
                             val2.CodeGen(fa));
  case REL_GE:
    return fa.CreateBinaryOP(AstInterface::BOP_GE, val1.CodeGen(fa),
                             val2.CodeGen(fa));
  default:
    ROSE_ABORT();
  }
}

AstNodePtr SymbolicMultiply::CodeGenOP(AstInterface &fa, const AstNodePtr &a1,
                                       const AstNodePtr &a2) const {
  int val = 0;
  if (fa.IsConstInt(a1, &val) && val == -1)
    return fa.CreateUnaryOP(AstInterface::UOP_MINUS, a2);
  else if (fa.IsConstInt(a2, &val) && val == -1)
    return fa.CreateUnaryOP(AstInterface::UOP_MINUS, a1);
  return fa.CreateBinaryOP(AstInterface::BOP_TIMES, a1, a2);
}

AstNodePtr SymbolicPlus::CodeGenOP(AstInterface &fa, const AstNodePtr &a1,
                                   const AstNodePtr &a2) const {
  AstNodePtr opd;
  AstInterface::OperatorEnum opr;
  if (fa.IsUnaryOp(a2, &opr, &opd) && opr == AstInterface::UOP_MINUS) {
    return fa.CreateBinaryOP(AstInterface::BOP_MINUS, a1, fa.CopyAstTree(opd));
  } else if (fa.IsUnaryOp(a1, &opr, &opd) && opr == AstInterface::UOP_MINUS) {
    return fa.CreateBinaryOP(AstInterface::BOP_MINUS, a2, fa.CopyAstTree(opd));
  }
  return fa.CreateBinaryOP(AstInterface::BOP_PLUS, a1, a2);
}

AstNodePtr SymbolicAnd::CodeGenOP(AstInterface &fa, const AstNodePtr &a1,
                                  const AstNodePtr &a2) const {
  return fa.CreateBinaryOP(AstInterface::BOP_AND, a1, a2);
}

AstNodePtr SymbolicEq::CodeGenOP(AstInterface &fa, const AstNodePtr &a1,
                                 const AstNodePtr &a2) const {
  return fa.CreateBinaryOP(AstInterface::BOP_EQ, a1, a2);
}

SymbolicVal SymbolicValGenerator::GetSymbolicVal(const std::string &sig) {
  DebugLog debugval("-debugsym");
  if (sig == "_NULL_") {
    return SymbolicVal();
  } else if (sig == "_UNKNOWN_") {
    return SymbolicVal(new SymbolicValImpl());
  } else if (!sig.empty() && sig[0] == '*') {
    debugval([&sig]() { return "creating pointer deref:" + sig; });
    auto r = GetSymbolicVal(sig.substr(1));
    return new SymbolicFunction(AstInterface::UOP_DEREF, "*", r);
  } else {
    debugval([&sig]() { return "creating variable:" + sig; });
    return SymbolicVal(new SymbolicVar(sig, NULL));
  }
}

bool SymbolicValGenerator::IsFortranLoop(AstInterface &fa, const AstNodePtr &s,
                                         SymbolicVar *ivar, SymbolicVal *lb,
                                         SymbolicVal *ub, SymbolicVal *step,
                                         AstNodePtr *body) {
  AstNodePtr ivarast, lbast, ubast, stepast, ivarscope;
  if (!fa.IsFortranLoop(s, &ivarast, &lbast, &ubast, &stepast, body))
    return false;
  std::string varname;
  if (!fa.IsVarRef(ivarast, 0, &varname, &ivarscope)) {
    return false;
  }
  if (ivar != 0)
    *ivar = SymbolicVar(varname, ivarscope, ivarast);
  if (lb != 0)
    *lb = SymbolicValGenerator::GetSymbolicVal(fa, lbast);
  if (ub != 0)
    *ub = SymbolicValGenerator::GetSymbolicVal(fa, ubast);
  if (step != 0) {
    if (stepast != AST_NULL)
      *step = SymbolicValGenerator::GetSymbolicVal(fa, stepast);
    else
      *step = SymbolicVal(1);
  }
  return true;
}

namespace {

void requireSymbolicOperatorArity(AstInterface::OperatorEnum operation,
                                  const std::vector<SymbolicVal> &arguments,
                                  size_t expected) {
  if (arguments.size() == expected)
    return;
  std::cerr << "REX_SYMBOLIC_INVARIANT[operator-arity]: operator="
            << AstInterface::toString(operation) << " expected=" << expected
            << " actual=" << arguments.size() << std::endl;
  ROSE_ABORT();
}

} // namespace

SymbolicVal SymbolicValGenerator::GetSymbolicVal(
    AstInterface::OperatorEnum operation,
    const std::vector<SymbolicVal> &arguments) {
  switch (operation) {
  case AstInterface::BOP_TIMES:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return arguments[0] * arguments[1];
  case AstInterface::BOP_PLUS:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return arguments[0] + arguments[1];
  case AstInterface::BOP_MINUS:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return arguments[0] - arguments[1];
  case AstInterface::BOP_MOD:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "%", arguments);
  case AstInterface::BOP_DOT_ACCESS:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, ".", arguments);
  case AstInterface::BOP_ARROW_ACCESS:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "->", arguments);
  case AstInterface::BOP_DIVIDE:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "/", arguments);
  case AstInterface::BOP_EQ:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "==", arguments);
  case AstInterface::BOP_LE:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "<=", arguments);
  case AstInterface::BOP_LT:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "<", arguments);
  case AstInterface::BOP_NE:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "!=", arguments);
  case AstInterface::BOP_GT:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, ">", arguments);
  case AstInterface::BOP_GE:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, ">=", arguments);
  case AstInterface::BOP_AND:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "&&", arguments);
  case AstInterface::BOP_OR:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "||", arguments);
  case AstInterface::BOP_BIT_RSHIFT:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, ">>", arguments);
  case AstInterface::BOP_BIT_LSHIFT:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "<<", arguments);
  case AstInterface::BOP_BIT_AND:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "&", arguments);
  case AstInterface::BOP_BIT_OR:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "|", arguments);
  case AstInterface::UOP_MINUS:
    requireSymbolicOperatorArity(operation, arguments, 1);
    return new SymbolicFunction(operation, "-", arguments);
  case AstInterface::UOP_ADDR:
  case AstInterface::UOP_DEREF: {
    requireSymbolicOperatorArity(operation, arguments, 1);
    AstInterface::OperatorEnum nestedOperation = AstInterface::OP_NONE;
    std::vector<SymbolicVal> nestedArguments;
    if (arguments[0].isFunction(&nestedOperation, 0, &nestedArguments) &&
        ((operation == AstInterface::UOP_ADDR &&
          nestedOperation == AstInterface::UOP_DEREF) ||
         (operation == AstInterface::UOP_DEREF &&
          nestedOperation == AstInterface::UOP_ADDR))) {
      requireSymbolicOperatorArity(nestedOperation, nestedArguments, 1);
      return nestedArguments[0];
    }
    return new SymbolicFunction(
        operation, operation == AstInterface::UOP_ADDR ? "&" : "*", arguments);
  }
  case AstInterface::UOP_ALLOCATE:
    requireSymbolicOperatorArity(operation, arguments, 1);
    std::cerr << "REX_SYMBOLIC_INVARIANT[operator-kind]: operator="
              << AstInterface::toString(operation)
              << " requires an exact typed AST representation" << std::endl;
    ROSE_ABORT();
  case AstInterface::UOP_NOT:
    requireSymbolicOperatorArity(operation, arguments, 1);
    return new SymbolicFunction(operation, "!", arguments);
  case AstInterface::UOP_BIT_COMPLEMENT:
    requireSymbolicOperatorArity(operation, arguments, 1);
    return new SymbolicFunction(operation, "~", arguments);
  case AstInterface::UOP_SEMANTIC_CONVERSION:
  case AstInterface::UOP_CAST_C:
  case AstInterface::UOP_CAST_REINTERP:
  case AstInterface::UOP_CAST_STATIC:
  case AstInterface::UOP_CAST_DYNAMIC:
  case AstInterface::UOP_CAST_CONST:
  case AstInterface::UOP_CAST_BUILTIN_BIT:
  case AstInterface::UOP_CAST_FUNCTIONAL:
  case AstInterface::UOP_CAST_FUNCTIONAL_LIST:
    std::cerr << "REX_SYMBOLIC_INVARIANT[cast-transparency]: cast operator "
                 "escaped exact semantic conversion handling"
              << std::endl;
    ROSE_ABORT();
  case AstInterface::UOP_DECR1:
  case AstInterface::UOP_DECR1_POST:
    requireSymbolicOperatorArity(operation, arguments, 1);
    return new SymbolicFunction(operation, "--", arguments);
  case AstInterface::UOP_INCR1:
  case AstInterface::UOP_INCR1_POST:
    requireSymbolicOperatorArity(operation, arguments, 1);
    return new SymbolicFunction(operation, "++", arguments);
  case AstInterface::OP_ARRAY_ACCESS:
    if (arguments.size() < 2) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[operator-arity]: operator="
                << AstInterface::toString(operation)
                << " expected-at-least=2 actual=" << arguments.size()
                << std::endl;
      ROSE_ABORT();
    }
    return new SymbolicFunction(operation, "[]", arguments);
  case AstInterface::OP_ASSIGN:
    requireSymbolicOperatorArity(operation, arguments, 2);
    return new SymbolicFunction(operation, "=", arguments);
  default:
    std::cerr << "REX_SYMBOLIC_INVARIANT[operator-kind]: unsupported operator="
              << AstInterface::toString(operation) << std::endl;
    ROSE_ABORT();
  }
}

SymbolicVal SymbolicValGenerator ::GetSymbolicVal(AstInterface &fa,
                                                  const AstNodePtr &exp) {
  std::string name;
  AstNodePtr scope;
  int val = 0;
  AstNodePtr s1, s2;
  AstInterface::AstNodeList l;
  AstInterface::OperatorEnum opr = (AstInterface::OperatorEnum)0;
  if (SgCastExp *cast = isSgCastExp(exp.get_ptr())) {
    cast->validate_semantic_conversion();
    SymbolicVal operand = GetSymbolicVal(fa, AstNodePtr(cast->get_operand()));
    switch (cast->get_semantic_conversion_kind()) {
    case SgCastExp::e_semantic_conversion_NoOp:
    case SgCastExp::e_semantic_conversion_LValueToRValue:
      return operand;
    default:
      return new SymbolicSemanticConversion(AstNodePtr(cast), operand);
    }
  } else if (SgPntrArrRefExp *arrayAccess = isSgPntrArrRefExp(exp.get_ptr())) {
    if (arrayAccess->get_lhs_operand() == nullptr ||
        arrayAccess->get_rhs_operand() == nullptr) {
      std::cerr << "REX_SYMBOLIC_INVARIANT[array-access-operands]: access="
                << arrayAccess << " has a null exact operand" << std::endl;
      ROSE_ABORT();
    }
    return GetSymbolicVal(
        AstInterface::OP_ARRAY_ACCESS,
        {GetSymbolicVal(fa, AstNodePtr(arrayAccess->get_lhs_operand())),
         GetSymbolicVal(fa, AstNodePtr(arrayAccess->get_rhs_operand()))});
  } else if (fa.IsConstInt(exp, &val)) {
    return new SymbolicConst(val);
  } else if (fa.IsBinaryOp(exp, &opr, &s1, &s2)) {
    SymbolicVal v1 = GetSymbolicVal(fa, s1), v2 = GetSymbolicVal(fa, s2);
    return GetSymbolicVal(opr, {v1, v2});
  } else if (fa.IsUnaryOp(exp, &opr, &s1)) {
    if (opr == AstInterface::UOP_ALLOCATE)
      return new SymbolicAstWrap(exp);
    SymbolicVal v = GetSymbolicVal(fa, s1);
    return GetSymbolicVal(opr, {v});
  } else if (fa.IsFunctionCall(exp, &s1, &l)) {
    bool ismin = fa.IsMin(s1), ismax = fa.IsMax(s1);
    AstInterface::AstNodeList::const_iterator p = l.begin();
    if (ismin || ismax) {
      AstNodePtr s = *p;
      SymbolicVal v = GetSymbolicVal(fa, s);
      for (++p; p != l.end(); ++p) {
        s = *p;
        v = (ismin) ? Min(v, GetSymbolicVal(fa, s))
                    : Max(v, GetSymbolicVal(fa, s));
      }
      return v;
    }
    if (fa.IsVarRef(s1, 0, &name)) {
      SymbolicFunction::Arguments args;
      for (; p != l.end(); ++p) {
        SymbolicVal cur = GetSymbolicVal(fa, *p);
        args.push_back(cur);
      }
      return new SymbolicFunction(AstInterface::OP_NONE,
                                  new SymbolicAstWrap(s1), args);
    }
  } else if (fa.IsVarRef(exp, 0, &name, &scope)) {
    return new SymbolicVar(name, scope, exp);
  }
  return new SymbolicAstWrap(exp);
}
