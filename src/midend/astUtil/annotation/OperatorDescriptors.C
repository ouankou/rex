
#include "AstInterface.h"
#include "AstInterface_ROSE.h"

#include "CommandOptions.h"

#include "OperatorDescriptors.h"

#include "sage3basic.h"
#include "sageInterface.h"

#include <cctype>

#include <cstring>

#include <string>

#include <unordered_set>

#include <vector>

using namespace std;

DebugLog DebugOperatorDescriptor("-debugannot");

namespace {
std::string StripLeadingGlobalQualifier(std::string name) {
  if (name.rfind("::", 0) == 0) {
    name.erase(0, 2);
  }
  return name;
}

bool RequiresExactEvaluation(const AstNodePtr &argument) {
  SgExpression *root = isSgExpression(AstNodePtrImpl(argument).get_ptr());
  if (root == nullptr) {
    std::cerr << "REX_ANNOTATION_INVARIANT[argument-expression]: node="
              << AstNodePtrImpl(argument).get_ptr()
              << " is not an exact expression" << std::endl;
    ROSE_ABORT();
  }
  std::vector<SgExpression *> pending{root};
  std::unordered_set<SgExpression *> visited;
  while (!pending.empty()) {
    SgExpression *expression = pending.back();
    pending.pop_back();
    if (!visited.insert(expression).second) {
      std::cerr << "REX_ANNOTATION_INVARIANT[argument-expression-tree]: "
                << "expression=" << expression
                << " is reachable through more than one exact child edge"
                << std::endl;
      ROSE_ABORT();
    }
    if (isSgFunctionCallExp(expression) != nullptr ||
        isSgAssignOp(expression) != nullptr ||
        isSgCompoundAssignOp(expression) != nullptr ||
        isSgPlusPlusOp(expression) != nullptr ||
        isSgMinusMinusOp(expression) != nullptr ||
        isSgNewExp(expression) != nullptr ||
        isSgDeleteExp(expression) != nullptr ||
        isSgThrowOp(expression) != nullptr ||
        isSgAwaitExpression(expression) != nullptr ||
        isSgStatementExpression(expression) != nullptr ||
        isSgConstructorInitializer(expression) != nullptr)
      return true;
    SgType *expressionType = expression->get_type();
    if (expressionType == nullptr) {
      std::cerr << "REX_ANNOTATION_INVARIANT[argument-expression-type]: "
                << "expression=" << expression << " has no exact type"
                << std::endl;
      ROSE_ABORT();
    }
    if (SageInterface::isVolatileType(expressionType))
      return true;
    SgType *semanticType = expressionType->stripTypedefsAndModifiers();
    if (semanticType == nullptr) {
      std::cerr << "REX_ANNOTATION_INVARIANT[argument-expression-type]: "
                << "expression=" << expression << " has no exact semantic type"
                << std::endl;
      ROSE_ABORT();
    }
    if (semanticType->isFloatType() ||
        isSgTypeComplex(semanticType) != nullptr ||
        isSgTypeImaginary(semanticType) != nullptr)
      return true;
    for (SgNode *child : expression->get_traversalSuccessorContainer()) {
      if (SgExpression *childExpression = isSgExpression(child)) {
        if (childExpression->get_parent() != expression) {
          std::cerr << "REX_ANNOTATION_INVARIANT[argument-expression-edge]: "
                    << "parent=" << expression << " child=" << childExpression
                    << " actual-parent=" << childExpression->get_parent()
                    << std::endl;
          ROSE_ABORT();
        }
        pending.push_back(childExpression);
      }
    }
  }
  return false;
}
} // namespace

ReplaceParams::ReplaceParams(AstInterface &fa, const ParameterDeclaration &decl,
                             const AstInterface::AstNodeList &args) {
  if (decl.get_params().size() != args.size()) {
    std::cerr << "REX_ANNOTATION_INVARIANT[replacement-arity]: parameters="
              << decl.get_params().size() << " arguments=" << args.size()
              << std::endl;
    ROSE_ABORT();
  }
  int index = 0;
  for (AstInterface::AstNodeList::const_iterator p1 = args.begin();
       p1 != args.end(); ++p1, ++index) {
    AstNodePtr curAst = *p1;
    string curpar = decl.get_params()[index];
    SymbolicVal curarg = RequiresExactEvaluation(curAst)
                             ? SymbolicVal(SymbolicAstWrap(curAst))
                             : SymbolicValGenerator::GetSymbolicVal(fa, curAst);
    parmap[curpar] = curarg;
    exact_arguments[curpar] = curAst;
    partypemap[curpar] = decl.get_param_types()[index];
    DebugOperatorDescriptor([&]() {
      return "Operator parameter " + curpar + "->" + curarg.toString();
    });
  }
}

SymbolicVal ReplaceParams::operator()(const SymbolicVal &v) {
  cur = SymbolicVal();
  v.Visit(this);
  return cur;
}

SymbolicVal ReplaceParams::find(const string &varname) {
  map<string, SymbolicVal>::const_iterator p = parmap.find(varname);
  if (p != parmap.end()) {
    return (*p).second;
  }
  DebugOperatorDescriptor([&]() {
    return "Cannot find argument for parameter: " + varname +
           ". Returning empty!";
  });
  return SymbolicVal();
}

void ReplaceParams::VisitVar(const SymbolicVar &var) {
  string varname = var.GetVarName();
  SymbolicVal ast = find(varname);
  if (!ast.IsNIL())
    cur = ast;
  else
    cur = var;
}

void ReplaceParams::operator()(SymbolicValDescriptor &v) {
  v.replace_val(*this);
}

void ReplaceParams::replace_target(SymbolicValDescriptor &target) {
  std::string parameter;
  if (!target.get_val().isVar(parameter)) {
    std::cerr << "REX_ANNOTATION_INVARIANT[target-parameter]: target="
              << target.toString() << " is not a direct operator parameter"
              << std::endl;
    ROSE_ABORT();
  }
  const auto replacement = exact_arguments.find(parameter);
  if (replacement == exact_arguments.end()) {
    std::cerr << "REX_ANNOTATION_INVARIANT[target-parameter]: parameter="
              << parameter << " has no exact call argument" << std::endl;
    ROSE_ABORT();
  }
  target = SymbolicAstWrap(replacement->second);
}

// Returns signature for exp. Modifies argp with parameter values if exp is a
// function call.
std::string
OperatorDeclaration::operator_signature(AstInterface &fa, const AstNodePtr &exp,
                                        AstInterface::AstNodeList *argp,
                                        AstInterface::AstTypeList *paramp) {
  DebugOperatorDescriptor([&]() {
    return "Creating operator signature:" + AstInterface::AstToString(exp);
  });
  std::string fname;
  AstNodePtr f;
  AstNodeType t;
  AstTypeList params;
  if (paramp == 0)
    paramp = &params;
  if (AstInterface::IsVarRef(exp, &t, &fname, 0, 0, /*use_globl_name=*/true) &&
      fa.IsFunctionType(t, paramp)) {
    if (argp != 0 && paramp->size() == 1) {
      argp->push_back(exp.get_ptr());
    }
  } else if (fa.IsFunctionCall(exp, &f, argp, 0, paramp)) {
    if (AstInterface::IsVarRef(f, 0, &fname, 0, 0,
                               /*use_globl_name=*/true)) {
    } else if (SgConstructorInitializer *ctor_init =
                   isSgConstructorInitializer(f.get_ptr())) {
      SgFunctionDeclaration *decl = ctor_init->get_declaration();
      if (decl == nullptr) {
        DebugOperatorDescriptor([&]() {
          return "Unexpected constructor initializer without declaration: " +
                 AstInterface::AstToString(exp) + ". Return empty name.";
        });
        return "";
      }
      fname = StripLeadingGlobalQualifier(decl->get_qualified_name().str());
    } else {
      DebugOperatorDescriptor([&]() {
        return "Unexpected operator callee: " + AstInterface::AstToString(f) +
               ". Return empty name.";
      });
      return "";
    }
  } else if (AstInterface::IsFunctionDefinition(exp, &fname, argp, 0, 0, paramp,
                                                0,
                                                /*use_globl_name=*/true)) {
  } else {
    DebugOperatorDescriptor([&]() {
      return "Unexpected operator: not recognized:" +
             AstInterface::AstToString(exp) + ". Return empty name.";
    });
    return "";
  }
  if (argp != 0 && paramp->size() != argp->size()) {
    DebugOperatorDescriptor([&]() {
      return "Unexpected operator: not recognized:" +
             AstInterface::AstToString(exp) + ". Return empty name.";
    });
    return "";
  }
  return fname;
}

OperatorDeclaration::OperatorDeclaration(AstInterface &fa, AstNodePtr op_ast,
                                         AstInterface::AstNodeList *argp) {
  AstInterface::AstTypeList params;
  AstInterface::AstNodeList args;
  if (argp == 0) {
    argp = &args;
  }
  AstNodePtr dependent_callee;
  if (fa.IsFunctionCall(op_ast, &dependent_callee) &&
      isSgNonrealRefExp(dependent_callee.get_ptr()) != nullptr) {
    has_concrete_signature_ = false;
    argp->clear();
    return;
  }
  TypeDescriptor::get_name() = operator_signature(fa, op_ast, argp, &params);
  if (TypeDescriptor::get_name() == "") {
    DebugOperatorDescriptor([&]() {
      return "Error: Unknown/inconsistent operation: " +
             AstInterface::AstToString(op_ast) +
             ". Generating empty declaration.";
    });
    return;
  }
  AstInterface::AstNodeList::const_iterator p1 = argp->begin();
  AstInterface::AstTypeList::const_iterator p2 = params.begin();
  while (p2 != params.end() && p1 != argp->end()) {
    DebugOperatorDescriptor([&]() {
      return "Adding operator parameter:" + AstInterface::AstToString(*p1);
    });
    pars.add_param(fa.GetTypeName(*p2),
                   AstInterface::GetVariableSignature(*p1));
    ++p1;
    ++p2;
  }
  for (unsigned i = 0; i < pars.num_of_params(); ++i) {
    TypeDescriptor::get_name() += "_" + pars.get_param_type(i);
  }
  assert(params.size() == pars.num_of_params());
  if (params.size() != argp->size()) {
    std::cerr << "Error: mismatching numbers of parameters and arguments in "
                 "annotation."
              << params.size() << " vs " << argp->size() << "\n";
    assert(false);
  }
}

//! Read in an operator (function) declaration: name + a list of parameter types
//! and names)
OperatorDeclaration &OperatorDeclaration::read(istream &in) {
  // Signature is the full function name, possibly with several qualifiers
  std::string signiture = read_id(in);

  string classname, funcname;

  char c = peek_ch(in);
  if (c == ':') {
    classname = signiture;
    read_ch(in, ':');
    read_ch(in, ':');
    signiture = signiture + "::";
    c = peek_ch(in);
  }
  // Plus other special characters in the operator's name, such as <=, *,~
  while (in.good() && c != '(') {
    read_ch(in, c);
    signiture.push_back(c);
    funcname.push_back(c);
    c = peek_ch(in);
  }
  // Append () for "::operator()" ?
  const char *opstart = std::strrchr(signiture.c_str(), ':');
  if (opstart != 0 && string(opstart + 1) == "operator") {
    signiture = signiture + "()";
    read_ch(in, '(');
    read_ch(in, ')');
  }

  // Read in the parameter declaration: a list of (type, name)
  int index = 0;
  if (classname != "" && classname != funcname) {
    index = 1;
    pars.add_param(signiture, "this");
  }
  pars.read(in);

  for (unsigned i = index; i < pars.num_of_params(); ++i) {
    string partype = pars.get_param_type(i);
    signiture = signiture + "_" + partype;
  }
  TypeDescriptor::get_name() = signiture;
  return *this;
}
