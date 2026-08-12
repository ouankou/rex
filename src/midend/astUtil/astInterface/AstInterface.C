#include "sage3basic.h"
#include "sageInterface.h"

#include "AstInterface.h"

#include "AstInterface_ROSE.h"

#include "AstTraversal.h"

#include "CommandOptions.h"

#include "OperatorAnnotation.h"

#include "ROSE_FALLTHROUGH.h"

#include "astPostProcessing.h"

#include "unparser.h"

#include "unparser_opt.h"

#include <iostream>

#include <stdexcept>

#include <stdlib.h>

#include <string.h>

// jichi (9/29/2009): Add test for Fortran language
#define IS_FORTRAN_LANGUAGE() SageInterface::is_Fortran_language()

#define NEW_EXPR_STMT(stmt, exp)                                               \
  stmt = new SgExprStatement(GetFileInfo(), exp);                              \
  exp->set_parent(stmt);                                                       \
  stmt->set_endOfConstruct(stmt->get_file_info())

#define NEW_SYMBOL(sym, className, scope, name)                                \
  sym = new className(name);                                                   \
  name->set_scope(scope);                                                      \
  scope->insert_symbol(name->get_name(), sym)

#define NEW_BLOCK(block)                                                       \
  block = new SgBasicBlock(GetFileInfo());                                     \
  block->set_endOfConstruct(block->get_file_info())

#define NEW_BLOCK1(block, stmt)                                                \
  NEW_BLOCK(block);                                                            \
  block->append_statement(stmt);                                               \
  stmt->set_parent(block)

#define NEW_FUNCTION_REF(fr, fsym)                                             \
  fr = new SgFunctionRefExp(GetFileInfo(), fsym);                              \
  fr->set_endOfConstruct(fr->get_file_info())

#define NEW_MFUNCTION_REF(fr, fsym)                                            \
  fr = new SgMemberFunctionRefExp(GetFileInfo(), fsym);                        \
  fr->set_endOfConstruct(fr->get_file_info());                                 \
  fr->set_need_qualifier(false)

#define NEW_EXPR_LIST(explist)                                                 \
  explist = new SgExprListExp(GetFileInfo());                                  \
  explist->set_endOfConstruct(explist->get_file_info())

#define NEW_FUNCTION_CALL(fcall, fref, args)                                   \
  SgFunctionType *fcall##_function_type = isSgFunctionType(fref->get_type());  \
  ROSE_ASSERT(fcall##_function_type != nullptr);                               \
  ROSE_ASSERT(fcall##_function_type->get_return_type() != nullptr);            \
  fcall = new SgFunctionCallExp(GetFileInfo(), fref, args,                     \
                                fcall##_function_type->get_return_type());     \
  fcall->set_endOfConstruct(fcall->get_file_info());                           \
  fref->set_parent(fcall);                                                     \
  args->set_parent(fcall)

#define NEW_VAR_INIT(init, var, exp)                                           \
  init = new SgAssignInitializer(GetFileInfo(), exp, var->get_type());         \
  init->set_endOfConstruct(init->get_file_info());                             \
  exp->set_parent(init);                                                       \
  var->set_initializer(init);                                                  \
  exp->set_parent(init);                                                       \
  init->set_parent(var)

#define NEW_ASSIGN(exp, lhs, rhs)                                              \
  exp = new SgAssignOp(GetFileInfo(), lhs, rhs, lhs->get_type());              \
  exp->set_endOfConstruct(exp->get_file_info());                               \
  lhs->set_parent(exp);                                                        \
  rhs->set_parent(exp);

#define NEW_BIN_OP(op, className, lhs, rhs, result_type)                       \
  op = new className(GetFileInfo(), lhs, rhs, result_type);                    \
  op->set_endOfConstruct(op->get_file_info());                                 \
  lhs->set_parent(op);                                                         \
  rhs->set_parent(op)

#define NEW_IF(r, cond, tbody)                                                 \
  r = new SgIfStmt(GetFileInfo(), cond, tbody,                                 \
                   new SgBasicBlock(GetFileInfo()));                           \
  r->set_has_end_statement(true);                                              \
  r->set_endOfConstruct(r->get_file_info());                                   \
  r->get_false_body()->set_endOfConstruct(r->get_file_info());                 \
  cond->set_parent(r);                                                         \
  tbody->set_parent(r);                                                        \
  r->get_false_body()->set_parent(r)

#define NEW_IF_ELSE(r, cond, tbody, fbody)                                     \
  r = new SgIfStmt(GetFileInfo(), cond, tbody, fbody);                         \
  r->set_has_end_statement(true);                                              \
  r->set_endOfConstruct(r->get_file_info());                                   \
  cond->set_parent(r);                                                         \
  tbody->set_parent(r);                                                        \
  fbody->set_parent(r)

namespace {
DebugLog DebugVariable("-debugvariable");
DebugLog DebugScope("-debugscope");
DebugLog DebugDiff("-debugdiff");
DebugLog DebugDecl("-debugdecl");
static std::function<std::string(const SgFunctionDeclaration *)>
    function_name_mangling_;

SgType *logicalOperatorResultType() {
  return SageInterface::is_C_language()
             ? static_cast<SgType *>(SgTypeInt::createType())
             : static_cast<SgType *>(SgTypeBool::createType());
}

SgType *requireElementResultType(SgType *operand_type, const char *producer) {
  SgType *result = SageInterface::getElementType(operand_type);
  if (result == nullptr || isSgTypeUnknown(result) != nullptr ||
      isSgTypeDefault(result) != nullptr) {
    std::cerr << "REX_AST_INVARIANT[unary-result-type-producer]: " << producer
              << " has no exact pointee or element result type" << std::endl;
    ROSE_ABORT();
  }
  return result;
}

bool hasExactValueIdentityConversion(const SgCastExp *cast,
                                     const char *consumer) {
  if (cast == nullptr || consumer == nullptr) {
    std::cerr << "REX_AST_INVARIANT[cast-value-identity]: consumer="
              << (consumer != nullptr ? consumer : "null")
              << " has no exact cast" << std::endl;
    ROSE_ABORT();
  }
  cast->validate_semantic_conversion();
  switch (cast->get_semantic_conversion_kind()) {
  case SgCastExp::e_semantic_conversion_NoOp:
  case SgCastExp::e_semantic_conversion_LValueToRValue:
    return true;
  default:
    return false;
  }
}

SgNode *stripExactValueIdentityConversions(SgNode *expression,
                                           const char *consumer,
                                           bool require_transparent) {
  while (SgCastExp *cast = isSgCastExp(expression)) {
    if (!hasExactValueIdentityConversion(cast, consumer)) {
      if (!require_transparent)
        return expression;
      std::cerr << "REX_AST_INVARIANT[cast-value-identity]: consumer="
                << consumer << " cast=" << cast << " kind="
                << static_cast<int>(cast->get_semantic_conversion_kind())
                << " changes the value or identity and cannot be discarded"
                << std::endl;
      ROSE_ABORT();
    }
    expression = cast->get_operand();
  }
  return expression;
}

SgNode *stripExactAddressOriginConversions(SgNode *expression,
                                           const char *consumer) {
  while (SgCastExp *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    switch (cast->get_semantic_conversion_kind()) {
    case SgCastExp::e_semantic_conversion_NoOp:
    case SgCastExp::e_semantic_conversion_LValueToRValue:
      expression = cast->get_operand();
      break;

    case SgCastExp::e_semantic_conversion_DerivedToBase: {
      SgType *source_type = cast->get_operand()->get_type();
      SgType *target_type = cast->get_type();
      SgPointerType *source_pointer = isSgPointerType(
          source_type != nullptr ? source_type->stripTypedefsAndModifiers()
                                 : nullptr);
      SgPointerType *target_pointer = isSgPointerType(
          target_type != nullptr ? target_type->stripTypedefsAndModifiers()
                                 : nullptr);
      SgType *source_pointee =
          source_pointer != nullptr ? source_pointer->get_base_type() : nullptr;
      SgType *target_pointee =
          target_pointer != nullptr ? target_pointer->get_base_type() : nullptr;
      if (source_pointer == nullptr || target_pointer == nullptr ||
          source_pointee == nullptr || target_pointee == nullptr ||
          isSgClassType(source_pointee->stripTypedefsAndModifiers()) ==
              nullptr ||
          isSgClassType(target_pointee->stripTypedefsAndModifiers()) ==
              nullptr ||
          cast->get_conversion_base_path().empty() ||
          cast->get_value_category() != SgCastExp::e_value_category_prvalue) {
        std::cerr
            << "REX_AST_INVARIANT[address-origin-conversion]: consumer="
            << consumer << " cast=" << cast
            << " claims a derived-to-base address conversion without exact "
               "pointer, class, base-path, and value-category semantics"
            << std::endl;
        ROSE_ABORT();
      }
      expression = cast->get_operand();
      break;
    }

    case SgCastExp::e_semantic_conversion_BitCast: {
      SgType *source_type = cast->get_operand()->get_type();
      SgType *target_type = cast->get_type();
      SgPointerType *source_pointer = isSgPointerType(
          source_type != nullptr ? source_type->stripTypedefsAndModifiers()
                                 : nullptr);
      SgPointerType *target_pointer = isSgPointerType(
          target_type != nullptr ? target_type->stripTypedefsAndModifiers()
                                 : nullptr);
      if (source_pointer == nullptr || target_pointer == nullptr ||
          cast->get_value_category() != SgCastExp::e_value_category_prvalue) {
        std::cerr
            << "REX_AST_INVARIANT[address-origin-bitcast]: consumer="
            << consumer << " cast=" << cast
            << " claims an address-preserving bitcast without exact pointer "
               "source, pointer target, and prvalue semantics"
            << std::endl;
        ROSE_ABORT();
      }
      expression = cast->get_operand();
      break;
    }

    default:
      return expression;
    }
  }
  return expression;
}
} // namespace

SgType *AstInterfaceImpl::GetTypeInt() {
  static SgType *typeint = 0;
  if (typeint == 0)
    typeint = new SgTypeInt();
  return typeint;
}

std::string StripGlobalQualifier(std::string name) {
  if (name.size() > 2 && name[0] == ':' && name[1] == ':') {
    return std::string(name.c_str() + 2);
  }
  return name;
}

std::string StripQualifier(std::string name) {
  unsigned i = name.size();
  for (; i > 0; --i) {
    if (name[i - 1] == ':' && i > 1 && name[i - 2] == ':')
      break;
  }
  return std::string(name.c_str() + i);
}

void AstInterface ::SetRoot(const AstNodePtr &root) {
  impl->set_top(AstNodePtrImpl(root).get_ptr());
}

AstNodePtr AstInterface ::GetRoot() const {
  return AstNodePtrImpl(impl->get_scope(0));
}

void AstInterface ::AttachObserver(AstObserver *ob) {
  impl->AttachObserver(ob);
}

void AstInterface ::DetachObserver(AstObserver *ob) {
  impl->DetachObserver(ob);
}

AstNodePtr AstInterface::GetFunctionDefinition(const AstNodePtr &n,
                                               std::string *name) {
  AstNodePtr r = n;
  while (r != AST_NULL && !IsFunctionDefinition(r, name)) {
    r = GetParent(r);
  }
  return r;
}

AstNodePtr AstInterface::GetFunctionDefinitionFromDeclaration(
    const AstNodePtr &decl_node) {
  auto *decl = isSgFunctionDeclaration(decl_node.get_ptr());
  if (decl == 0) {
    return AST_NULL;
  }
  SgFunctionDefinition *def = decl->get_definition();
  if (def == 0) {
    auto *def_decl = isSgFunctionDeclaration(decl->get_definingDeclaration());
    if (def_decl != 0) {
      def = def_decl->get_definition();
    }
  }
  return def == 0 ? AST_NULL : AstNodePtrImpl(def);
}

using namespace std;
bool DebugNewVar() {
  static int r = 0;
  if (r == 0) {
    if (CmdOptions::GetInstance()->HasOption("-debugnewvar"))
      r = 1;
    else
      r = -1;
  }
  return r == 1;
}

bool DebugType() {
  static int r = 0;
  if (r == 0) {
    if (CmdOptions::GetInstance()->HasOption("-debugtype"))
      r = 1;
    else
      r = -1;
  }
  return r == 1;
}

bool DebugSymbol() {
  static int r = 0;
  if (r == 0) {
    if (CmdOptions::GetInstance()->HasOption("-debugsymbol"))
      r = 1;
    else
      r = -1;
  }
  return r == 1;
}

Sg_File_Info *GetFileInfo() {
  // DQ (3/8/2006): This is the easiest way to represent a transformation
  // since we have to both mark the file info object as a transformation
  // AND to be output in the code generation phase as well.
  return Sg_File_Info::generateDefaultFileInfoForTransformationNode();
}

SgScopeStatement *GetNullScope() {
  static SgGlobal *global = 0;
  if (global == 0) {
    global = new SgGlobal(GetFileInfo());
  }
  return global;
}

inline bool HasNullParent(SgNode *n) {
  return n->get_parent() == 0 || n->get_parent() == GetNullScope();
}

inline SgVarRefExp *ToVarRef(AstInterfaceImpl &fa, SgNode *exp) {
  switch (exp->variantT()) {
  case V_SgVarRefExp:
    return isSgVarRefExp(exp);
  case V_SgInitializedName: {
    SgInitializedName *var = isSgInitializedName(exp);
    SgName varname = var->get_name();
    SgNode *r = fa.CreateVarRef(std::string(varname.str()), exp);
    r->set_parent(exp->get_parent());
    return isSgVarRefExp(r);
  }
  default:
    break;
  }
  return 0;
}

inline SgExpression *ToExpression(AstInterfaceImpl &fa, SgNode *s) {
  SgExpression *exp = ToVarRef(fa, s);
  if (exp == 0)
    exp = isSgExpression(s);
  return exp;
}

template <class const_iterator>
SgExprListExp *AstNodeList2ExpressionList(const_iterator b, const_iterator e) {
  SgExprListExp *NEW_EXPR_LIST(explist);
  for (const_iterator p = b; p != e; ++p) {
    SgExpression *e = isSgExpression(AstNodePtrImpl(*p).get_ptr());
    assert(e);
    explist->append_expression(e);
    e->set_parent(explist);
  }
  return explist;
}

SgStatement *ToStatement(SgNode *_stmts) {
  SgStatement *stmts = isSgStatement(_stmts);
  if (stmts == 0) {
    SgExpression *exp = isSgExpression(_stmts);
    assert(exp != 0);
    NEW_EXPR_STMT(stmts, exp);
  }
  return stmts;
}

SgClassDefinition *GetClassDefn(SgClassDeclaration *classDecl) {
  SgDeclarationStatement *decl = classDecl->get_definingDeclaration();
  assert(decl != 0);
  classDecl = isSgClassDeclaration(decl);
  assert(classDecl != 0);
  SgClassDefinition *classDefn = classDecl->get_definition();
  return classDefn;
}

SgClassDefinition *GetClassDefinition(SgNamedType *classtype) {
  if (classtype->variantT() == V_SgTypedefType) {
    return GetClassDefinition(
        isSgNamedType(isSgTypedefType(classtype)->get_base_type()));
  }
  SgDeclarationStatement *decl = classtype->get_declaration();
  if (decl->variantT() == V_SgClassDeclaration ||
      decl->variantT() == V_SgTemplateClassDeclaration)
    return GetClassDefn(isSgClassDeclaration(decl));
  else {
    cerr << "unexpected class declaration type: " << decl->sage_class_name()
         << endl;
    ROSE_ABORT();
  }
}

SgStatement *GetScope(SgNode *loc) {
  // DQ (3/23/2006): It is particularly dangerous in C++ to
  // interperate the scope from the structure.  Places where this
  // could be a problem now carry the scope explicitly (this step
  // was introduced after Qing's work on the AstInterface).
  // I have fixed up this code to report the correct scope
  // using the virtual get_scope() function for SgStatements.

  if (loc == 0 || loc->get_parent() == 0)
    return 0;

  if (loc->variantT() == V_SgThisExp) {
    SgMemberFunctionDeclaration *r = 0;
    while (loc != 0 && r == 0 && loc->get_parent() != 0) {
      loc = loc->get_parent();
      r = isSgMemberFunctionDeclaration(loc);
    }
    return (r == 0) ? 0 : r->get_class_scope();
  }
  if (loc->get_parent() != 0 &&
      loc->get_parent()->variantT() == V_SgLambdaCapture) {
    // Go up to the expression chain to the enclosing statement.
    while (loc != 0 && loc->get_parent() != 0 && isSgStatement(loc) == 0) {
      loc = loc->get_parent();
    }
    assert(loc != 0);
    return GetScope(loc);
  }
  {
    const SgInitializedName *initializedName = isSgInitializedName(loc);
    if (initializedName != NULL) {
      if (loc->get_parent() != 0 &&
          loc->get_parent()->variantT() == V_SgFunctionParameterList) {
        return initializedName->get_scope();
      }
      DebugScope([loc, initializedName]() {
        return "GetScope invoked: loc is " + AstInterface::AstToString(loc) +
               "; parent->parent is " +
               loc->get_parent()->get_parent()->class_name() + "; scope=" +
               AstInterface::AstToString(initializedName->get_scope());
      });
      return GetScope(loc->get_parent());
    }
  }
  if (isSgSourceFile(loc)) {
    return 0;
  }
  {
    SgScopeStatement *stmt = isSgScopeStatement(loc->get_parent());
    while (stmt == 0 && loc->get_parent() != 0) {
      stmt = isSgScopeStatement(loc->get_parent());
      loc = loc->get_parent();
    }
    return stmt;
  }
}

SgStatement *AstInterfaceImpl::GetScope(SgNode *loc) { return ::GetScope(loc); }

// Strip leading "const" and tailing '&'
std::string StripParameterType(const std::string &name) {
  std::string r = name;
  if (name.substr(0, 5) == "const")
    r = name.substr(5, name.size() - 5);
  ROSE_ASSERT(!r.empty());
  size_t end = r.size() - 1;
  if (r[end] == '&') {
    r[end] = ' ';
  }
  std::string result = "";
  for (size_t i = 0; i < r.size(); ++i) {
    if (r[i] != ' ')
      result.push_back(r[i]);
  }
  return result;
}

SgNode *CreateAssignment(AstInterfaceImpl &fa, SgExpression *lhsexp,
                         SgExpression *rhsexp) {
  // assert(HasNullParent(lhsexp));
  // assert(HasNullParent(rhsexp));
  SgExpression *exp = 0;
  SgType *lhstype = lhsexp->get_type();

  if (lhstype->variantT() == V_SgClassType) {
    SgClassType *lhstype1 = isSgClassType(lhstype);
    SgName classname = lhstype1->get_name();
    SgClassDeclaration *c = isSgClassDeclaration(fa.LookupNestedDeclaration(
        std::string(classname.str()), fa.get_scope(lhsexp)));
    assert(c != 0);
    SgExpressionPtrList args;
    args.push_back(rhsexp);
    SgMemberFunctionSymbol *f = fa.GetMemberFunc(c, "operator=", &args);
    if (f != 0) {
      SgMemberFunctionRefExp *NEW_MFUNCTION_REF(fr, f);
      SgExpression *NEW_BIN_OP(func, SgDotExp, lhsexp, fr, fr->get_type());
      SgExprListExp *NEW_EXPR_LIST(argexp);
      SgExpressionPtrList &l = argexp->get_expressions();
      l = args;
      NEW_FUNCTION_CALL(exp, func, argexp);
    }
  }
  if (exp == 0) {
    NEW_ASSIGN(exp, lhsexp, rhsexp);
  }
  return exp;
}

std::string unparseToString(SgNode *s) {
  if (s == 0)
    return "";
  string r = "";
  switch (s->variantT()) {
  case V_SgName:
    return isSgName(s)->str();
  case V_SgVarRefExp: {
    SgVarRefExp *var = isSgVarRefExp(s);
    SgVariableSymbol *sb = var->get_symbol();
    r = r + sb->get_name().str();
  } break;
  case V_SgInitializedName: {
    SgInitializedName *var = isSgInitializedName(s);
    r = r + var->get_name();
  } break;
  case V_SgProject: {
    SgProject *sageProject = static_cast<SgProject *>(s);
    for (int i = 0; i < sageProject->numberOfFiles(); ++i) {
      SgSourceFile *sageFile = isSgSourceFile(sageProject->get_fileList()[i]);
      r = r + unparseToString(sageFile);
    }
    break;
  }
  default:
    r = r + s->unparseToString();
  }
  return r;
}

// We now allow fully qualified names in annotations for side effects
// e.g.  VectorXY::a   : static vs. non-static class members
//  Namespace1::space2::y
//  Start from the simplest case first, to be extended later on.
//  Case 1:  class::member
SgVariableSymbol *LookupQualifiedVar(const std::string &name,
                                     SgScopeStatement *loc) {
  int sz = name.size();
  assert(sz != 0);
  assert(loc);

  int pos = 0;
  // skip leading :: if they are present.
  if (sz >= 2 && name[0] == ':' && name[1] == ':')
    pos = 2;

  assert(sz - 2 != 0);

  SgScopeStatement *cur_scope = SageInterface::getGlobalScope(loc);

  // split the name into segments
  string currentname;
  SgDeclarationStatement *matched_decl = NULL; // matched decl
  SgInitializedName *initname = NULL;
  while (pos <= sz) // we reach the last + 1 pos, very tricky here!!
  {
    if (name[pos] == ':' || pos == sz) // reached last char +1 or current is :.
                                       // we have a complete name so far.
    {
      assert(currentname.size() != 0);
      if (name[pos] == ':') {
        if (name[pos - 1] != ':') // this is the first : of ::
        {
          assert(pos + 1 < sz && name[pos + 1] == ':');
          pos += 2; // skip two chars
        } else // this is the second :, impossible if we always skip by two :
        {
          cerr << "Error: unexpected : appears in LookUpQualifiedVar()" << endl;
          ROSE_ABORT();
        }
      } else // last char?
        pos++;

      // we now have find a full name, use it to find the declaration matching
      // the name
      assert(cur_scope);
      SgDeclarationStatementPtrList decl_ptr_list =
          cur_scope->getDeclarationList();
      for (size_t i = 0; i < decl_ptr_list.size(); i++) {
        SgDeclarationStatement *cur_decl = decl_ptr_list[i];
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(cur_decl)) {
          // must be a defining class declaration
          class_decl =
              isSgClassDeclaration(class_decl->get_definingDeclaration());
          if (!class_decl)
            continue;

          if (class_decl->get_name().getString() == currentname) {
            matched_decl = cur_decl;
            // update the scope to be the new declaration, when applicable
            cur_scope = class_decl->get_definition();
            break;
          }
        } else if (SgNamespaceDeclarationStatement *ns_decl =
                       isSgNamespaceDeclarationStatement(cur_decl)) {
          // must be a defining declaration
          ns_decl = isSgNamespaceDeclarationStatement(
              ns_decl->get_definingDeclaration());
          if (ns_decl->get_name().getString() == currentname) {
            matched_decl = cur_decl;
            cur_scope = ns_decl->get_definition();
            break;
          }
        } else if (SgVariableDeclaration *var_decl =
                       isSgVariableDeclaration(cur_decl)) {
          // var declaration only has a nondefining one
          if ((initname = var_decl->get_decl_item(SgName(currentname)))) {
            matched_decl = cur_decl;
            cur_scope = NULL;
            break;
          }
        }
        // other types of declarations, we just skip them. no use in qualified
        // names TODO: double check this
      }
      if (!matched_decl) {
        if (cur_scope != NULL) {
          cerr << "Warning: cannot find qualified name for " << currentname
               << " within scope " << cur_scope->class_name() << " @ "
               << cur_scope->get_file_info()->get_line() << endl;
        } else {
          cerr << "Warning: cannot find qualified name " << currentname << endl;
        }
        return NULL; // cannot find the declaration
      }

      // reset name to accept next name
      currentname = "";
    } else // characters other than :, accumulate to current name
    {
      currentname.push_back(name[pos++]);
    }
  }

  assert(initname);
  return isSgVariableSymbol(initname->search_for_symbol_from_symbol_table());
}

SgVariableSymbol *LookupVar(const std::string &name, SgScopeStatement *loc) {
  const char *start = name.c_str();

  // check if it is a fully qualified name, if yes, use the special lookup
  // function instead
  if (name.find("::") != string::npos)
    return LookupQualifiedVar(name, loc);

  SgClassDefinition *cdef = isSgClassDefinition(loc);
  if (cdef != 0) {
    SgVariableSymbol *r =
        dynamic_cast<SgVariableSymbol *>(cdef->lookup_symbol(start));
    if (DebugSymbol()) {
      if (r == 0)
        std::cerr << "failed to find variable " << start;
      else
        std::cerr << "found variable " << start;
      std::cerr << " in scope " << unparseToString(loc) << "\n";
      std::cerr << " symbols of which include: ";
      for (SgSymbol *p = cdef->first_any_symbol(); p != 0;
           p = cdef->next_any_symbol())
        std::cerr << p->get_name().str() << ";";
      std::cerr << "\n";
    }
    if (r != 0)
      return r;
    SgBaseClassPtrList &l = cdef->get_inheritances();
    for (SgBaseClassPtrList::iterator p = l.begin(); p != l.end(); ++p) {
      SgBaseClass *cur = *p;
      SgClassDeclaration *decl = cur->get_base_class();
      assert(decl != 0);
      SgClassDefinition *def = GetClassDefn(decl);
      assert(def != 0);
      r = LookupVar(name, def);
      if (r != 0)
        return r;
    }
    return 0;
  } else {
    SgVariableSymbol *f = 0;
    do {
      f = dynamic_cast<SgVariableSymbol *>(loc->lookup_symbol(start));
      if (DebugSymbol()) {
        if (f == 0)
          std::cerr << "failed to find variable ";
        else
          std::cerr << "found variable ";
        std::cerr << start << " in scope " << unparseToString(loc) << "\n";
      }
      if (loc->variantT() == V_SgGlobal || f != 0)
        break;
      loc = loc->get_scope();
    } while (loc != 0 && f == 0);
    return f;
  }
}

SgNode *AstInterfaceImpl::LookupNestedDeclaration(const std::string &name,
                                                  SgNode *loc) {
  int sz = name.size();
  assert(sz != 0);
  assert(loc);

  int pos = 0;
  // skip leading :: if they are present.
  if (sz >= 2 && name[0] == ':' && name[1] == ':')
    pos = 2;

  assert(sz - 2 != 0);

  // Save the current matching declarations in cur_results.
  // Save the declarations to sewarch in to_search_next.
  AstInterface::AstNodeList cur_results, to_search_next;
  to_search_next.push_back(SageInterface::getGlobalScope(loc));

  // split the name into segments
  std::string currentname;
  DebugVariable([&name]() { return "Looking for variable:" + name; });
  // Search for each scope name and advance the search accordingly.
  while (pos <= sz) { // we reach the last + 1 pos, very tricky here!!
    if (name[pos] != ':' && pos < sz) {
      // characters other than :, accumulate to current name
      // We have not yet reached the end of a scope name.
      currentname.push_back(name[pos++]);
      continue;
    }
    // We have a complete scope name. Double checking.
    assert(currentname.size() != 0);
    // First, advance the given name to the next scope if needed.
    if (pos < sz && name[pos] == ':') {
      if (name[pos - 1] != ':') { // this is the first : of ::
        assert(pos + 1 < sz && name[pos + 1] == ':');
        pos += 2; // skip two chars
      }
    } else {
      // last char. Advance pos to exit the surrounding while loop.
      pos++;
    }

    // Use decl_ptr_list as a work list to store all declarations to check.
    // Use matched_decls to save all declarations that match the current name.
    // Use new_decls to save new declarations to search for current name.
    AstInterface::AstNodeList decl_ptr_list;
    // Start from all matching declarations before reaching current name.
    decl_ptr_list = to_search_next;
    cur_results.clear();
    to_search_next.clear();
    // Now search for the scope that have the given scope name..
    DebugVariable(
        [&currentname]() { return "Looking for name:" + currentname; });
    // Iterate until the list is empty.
    while (!decl_ptr_list.empty()) {
      // Pop out the current declaration.
      auto cur_decl = decl_ptr_list.back();
      decl_ptr_list.pop_back();

      DebugVariable([&cur_decl]() {
        return "processing decl:" +
               ((cur_decl == 0) ? "NULL" : AstInterface::AstToString(cur_decl));
      });
      std::string tmp_name;
      AstInterface::AstNodeList new_decls;
      if (AstInterface::IsBlock(cur_decl, &tmp_name, &new_decls)) {
        size_t i = tmp_name.rfind("::");
        if (i < tmp_name.size()) { // strip qualified names.
          tmp_name = tmp_name.substr(i + 2, tmp_name.size() - i + 2);
        }
        DebugVariable([&tmp_name]() { return "Is Block " + tmp_name; });
        if (tmp_name == currentname) {
          cur_results.push_back(cur_decl);
          DebugVariable(
              [&currentname]() { return "Found name : " + currentname; });
          to_search_next.insert(to_search_next.end(), new_decls.begin(),
                                new_decls.end());
        } else {
          decl_ptr_list.insert(decl_ptr_list.end(), new_decls.begin(),
                               new_decls.end());
        }
      } else if (SgVariableDeclaration *var_decl =
                     isSgVariableDeclaration(cur_decl.get_ptr())) {
        DebugVariable([]() { return "Is variable declaration."; });
        auto vars = var_decl->get_variables();
        decl_ptr_list.insert(decl_ptr_list.end(), vars.begin(), vars.end());
      } else if (SgInitializedName *initname =
                     isSgInitializedName(cur_decl.get_ptr())) {
        DebugVariable([&initname]() {
          return "name:" + initname->get_name().getString();
        });
        if (initname->get_name().getString() == currentname) {
          cur_results.push_back(cur_decl);
          DebugVariable(
              [&currentname]() { return "Found name : " + currentname; });
        }
      } else if (auto *typedef_decl = isSgTypedefType(cur_decl.get_ptr())) {
        if (typedef_decl->get_name().str() == currentname) {
          cur_results.push_back(cur_decl);
          DebugVariable(
              [&currentname]() { return "Found name : " + currentname; });
          auto *decl1 = typedef_decl->get_base_type();
          to_search_next.push_back(decl1);
        }
      } else {
        DebugVariable([&cur_decl]() {
          return "Not looking at:" + AstInterface::AstToString(cur_decl);
        });
      }
    }
    if (cur_results.empty()) {
      // The search fails. Output a warning and return NULL.
      std::cerr << "Warning: cannot find qualified name for " << currentname
                << "\n";
      return NULL; // cannot find the declaration
    } else {
      // reset scope name to accept next name
      currentname = "";
    }
  }
  assert(!cur_results.empty());
  return cur_results[0].get_ptr();
}

SgVariableSymbol *AstInterfaceImpl::LookupVar(const std::string &name,
                                              SgNode *loc) {
  assert(loc != 0);
  const char *start = name.c_str();

  // check if it is a fully qualified name, if yes, use the special lookup
  // function instead
  if (name.find("::") != std::string::npos) {
    AstNodePtr result = LookupNestedDeclaration(name, loc);
    SgInitializedName *initname = isSgInitializedName(result.get_ptr());
    if (initname != 0) {
      SgVariableSymbol *varsym =
          isSgVariableSymbol(initname->search_for_symbol_from_symbol_table());
      if (varsym == 0) {
        NEW_SYMBOL(varsym, SgVariableSymbol, initname->get_scope(), initname);
      }
      return varsym;
    }
    return 0;
  }
  {
    SgNamedType *t = isSgNamedType(loc);
    if (t != 0) {
      return LookupVar(name, t->getAssociatedDeclaration());
    }
  }
  {
    SgClassDeclaration *class_decl = isSgClassDeclaration(loc);
    if (class_decl != 0) {
      return LookupVar(name, GetClassDefn(class_decl));
    }
  }
  SgClassDefinition *cdef = isSgClassDefinition(loc);
  if (cdef != 0) {
    SgVariableSymbol *r =
        dynamic_cast<SgVariableSymbol *>(cdef->lookup_symbol(start));
    if (DebugSymbol()) {
      if (r == 0)
        std::cerr << "failed to find variable " << start;
      else
        std::cerr << "found variable " << start;
      std::cerr << " in scope " << unparseToString(loc) << "\n";
      std::cerr << " symbols of which include: ";
      for (SgSymbol *p = cdef->first_any_symbol(); p != 0;
           p = cdef->next_any_symbol())
        std::cerr << p->get_name().str() << ";";
      std::cerr << "\n";
    }
    if (r != 0)
      return r;
    SgBaseClassPtrList &l = cdef->get_inheritances();
    for (SgBaseClassPtrList::iterator p = l.begin(); p != l.end(); ++p) {
      SgBaseClass *cur = *p;
      SgClassDeclaration *decl = cur->get_base_class();
      assert(decl != 0);
      SgClassDefinition *def = GetClassDefn(decl);
      assert(def != 0);
      r = LookupVar(name, def);
      if (r != 0)
        return r;
    }
    return 0;
  } else {
    SgScopeStatement *loc_scope = isSgScopeStatement(loc);
    assert(loc_scope != 0);
    SgVariableSymbol *f = 0;
    do {
      f = dynamic_cast<SgVariableSymbol *>(loc_scope->lookup_symbol(start));
      if (DebugSymbol()) {
        if (f == 0)
          std::cerr << "failed to find variable ";
        else
          std::cerr << "found variable ";
        std::cerr << start << " in scope " << unparseToString(loc) << "\n";
      }
      if (loc_scope->variantT() == V_SgGlobal || f != 0)
        break;
      loc_scope = loc_scope->get_scope();
    } while (loc_scope != 0 && f == 0);
    return f;
  }
}

class SageSetTransformation : public AstTopDownProcessing<AstNodePtrImpl> {
  AstNodePtrImpl evaluateInheritedAttribute(SgNode *astNode,
                                            AstNodePtrImpl inheritedValue) {
    Sg_File_Info *r = astNode->get_file_info();
    r->setTransformation();
    r->setCompilerGenerated();
    r->setOutputInCodeGeneration();
    return astNode;
  }

public:
  SageSetTransformation() {}
  void operator()(SgNode *node) {
    AstTopDownProcessing<AstNodePtrImpl>::traverse(node, node->get_parent());
  }
};
class SageResetParent : public AstTopDownProcessing<AstNodePtrImpl> {
  AstNodePtrImpl evaluateInheritedAttribute(SgNode *astNode,
                                            AstNodePtrImpl inheritedValue) {
    if (inheritedValue != 0) {
      // assert(astNode->get_parent() == inheritedValue || astNode->get_parent()
      // == 0);
      astNode->set_parent(inheritedValue.get_ptr());
    }
    return astNode;
  }

public:
  SageResetParent() {}
  void operator()(SgNode *node) {
    AstTopDownProcessing<AstNodePtrImpl>::traverse(node, node->get_parent());
  }
};

/* QY: 7/2011 This function is not invoked anywhere
SgSymbol* AddDecls( AstInterfaceImpl* scope, const
SgDeclarationStatementPtrList& decls)
{
     SgSymbol* result = 0;
     for (SgDeclarationStatementPtrList::const_iterator p = decls.begin(); p !=
decls.end(); ++p) { SgDeclarationStatement* cur = *p; cur->set_file_info(
GetFileInfo()); SgFunctionDeclaration* d1 = isSgFunctionDeclaration(cur); if (d1
!= 0) { result = scope->AddFunc(d1);
         }
         else {
            SgVariableDeclaration* d2 = isSgVariableDeclaration(cur);
            if (d2 != 0) {
                result = scope->AddVar(d2);
            }
            else  {
               SgClassDeclaration* d3 = isSgClassDeclaration(cur);
               if (d3 != 0)
                   result = scope->AddClass(d3);
               else
                   ROSE_ABORT();
            }
         }
    }
    return result;
}
*/

SgMemberFunctionSymbol *
AstInterfaceImpl::GetMemberFunc(SgClassDeclaration *decl,
                                const std::string &funcname,
                                SgExpressionPtrList *args) {
  SgName classname = decl->get_name();
  SgClassDefinition *def = GetClassDefn(decl);
  if (def == 0) {
    cerr << "no definition in locating member function " << funcname << endl;
    return 0;
  }
  const char *start = funcname.c_str();
  if (args == 0) {
    SgFunctionSymbol *f = def->lookup_function_symbol(start);
    if (f != 0) {
      SgMemberFunctionSymbol *mf = isSgMemberFunctionSymbol(f);
      assert(mf != 0);
      return mf;
    } else {
      cerr << "AstInterface.C GetMemberFunc() cannot find a symbol for "
           << funcname << " within a class " << classname << endl;
      return 0;
    }
  } else {
    SgDeclarationStatementPtrList &decls = def->get_members();
    for (SgDeclarationStatementPtrList::iterator p = decls.begin();
         p != decls.end(); ++p) {
      SgDeclarationStatement *cur = *p;
      if (cur->variantT() != V_SgMemberFunctionDeclaration)
        continue;
      SgMemberFunctionDeclaration *md = isSgMemberFunctionDeclaration(cur);
      SgName name = md->get_name();
      if (std::string(name.str()) != funcname)
        continue;
      SgInitializedNamePtrList &pars = md->get_args();
      if (pars.size() != args->size())
        continue;
      SgInitializedNamePtrList::iterator pp = pars.begin();
      SgExpressionPtrList::iterator pa = args->begin();
      bool match = true;
      for (; pp != pars.end(); ++pp, ++pa) {
        SgType *tp = (*pp)->get_type();
        SgType *ta = (*pa)->get_type();
        ASSERT_not_null(tp);
        ASSERT_not_null(ta);
        if (tp->get_mangled() != ta->get_mangled()) {
          match = false;
          break;
        }
      }
      if (match) {
        // QY:1/7/08: this should be only temporary. should not create a new
        // symbol if the symbol is already in the symbol table (no search
        // mechanism available yet?)
        SgMemberFunctionSymbol *f = new SgMemberFunctionSymbol(md);
        return f;
      }
    }
  }
  return 0;
}

void AstInterfaceImpl::set_top(SgNode *top) {
  global = 0;
  scope = 0;
  if (top != 0) {
    scope = isSgScopeStatement(top);
    if (scope == 0) {
      SgStatement *t = GetScope(top);
      while (scope == 0 && t != 0) {
        scope = isSgScopeStatement(t);
        t = GetScope(t);
      }
    }
    SgStatement *cur = scope;
    while (cur != 0 && global == 0) {
      global = isSgGlobal(cur);
      if (cur->get_parent() != 0)
        cur = cur->get_scope();
      else
        cur = 0;
    }
  }
}

SgFunctionSymbol *AstInterfaceImpl::LookupFunction(const char *start,
                                                   SgScopeStatement *in_scope) {
  assert(in_scope != 0);
  SgScopeStatement *cur = in_scope;
  SgFunctionSymbol *f = 0;
  do {
    f = cur->lookup_function_symbol(start);
    if (DebugSymbol()) {
      if (f == 0)
        std::cerr << "failed to find function symbol " << start << " in scope "
                  << cur->sage_class_name() << " : " << cur->unparseToString()
                  << "\n";
      else
        std::cerr << "found function symbol " << start << " in scope "
                  << cur->sage_class_name() << " : " << cur->unparseToString()
                  << "\n";
    }

    if (cur->variantT() == V_SgGlobal)
      break;
    assert(cur->get_scope() != cur);
    cur = cur->get_scope();
  } while (cur != 0 && f == 0);
  if (DebugSymbol()) {
    if (cur == 0 || cur == GetNullScope())
      std::cerr << "exit with cur = " << (cur == 0 ? "NULL" : "NULL scope")
                << "\n";
    else
      std::cerr << "exit with cur = " << cur->sage_class_name() << "\n";
  }
  return f;
}

SgClassSymbol *AstInterfaceImpl::AddClass(SgClassDeclaration *d) {
  assert(global != 0);
  SgDeclarationStatementPtrList &l = global->get_declarations();
  l.insert(l.begin(), d);
  SgClassSymbol *NEW_SYMBOL(c, SgClassSymbol, global, d);
  return c;
}

SgFunctionSymbol *AstInterfaceImpl::AddFunc(SgFunctionDeclaration *d) {
  assert(global != 0);
  SgDeclarationStatementPtrList &l = global->get_declarations();
  l.insert(l.begin(), d);
  SgFunctionSymbol *NEW_SYMBOL(f, SgFunctionSymbol, global, d);
  d->set_parent(global);
  return f;
}

SgMemberFunctionSymbol *
AstInterfaceImpl::AddMemberFunc(SgClassDefinition *def,
                                SgMemberFunctionDeclaration *d) {
  SgMemberFunctionSymbol *NEW_SYMBOL(f, SgMemberFunctionSymbol, def, d);
  d->set_parent(def);
  return f;
}

void AstInterfaceImpl::SaveVarDecl(SgVariableDeclaration *d,
                                   SgScopeStatement *curscope) {
  if (curscope == 0)
    curscope = scope;
  assert(curscope != 0);

  d->set_parent(curscope);
  newVarList.push_back(
      std::pair<SgScopeStatement *, SgVariableDeclaration *>(curscope, d));
}

SgVariableSymbol *AstInterfaceImpl::InsertVar(SgInitializedName *d,
                                              SgScopeStatement *curscope) {
  if (curscope == 0)
    curscope = scope;
  assert(curscope != 0);
  SgVariableSymbol *NEW_SYMBOL(v, SgVariableSymbol, curscope, d);
  return v;
}

SgVariableSymbol *AstInterfaceImpl::NewVar(SgType *type,
                                           const std::string &_name,
                                           bool makeunique, bool delayDecl,
                                           SgExpression *initexp,
                                           SgScopeStatement *loc) {
  std::string varname = _name;
  if (varname == "") {
    varname = "_var_";
    makeunique = true;
  }
  if (makeunique) {
    char buf[20];
    sprintf(buf, "%d", newVarIndex);
    varname = varname + std::string(buf);
    ++newVarIndex;
  }

  SgVariableSymbol *v = LookupVar(varname, (loc == 0) ? scope : loc);
  if (v == 0) {
    // variable declaration has not been inserted
    SgName name(varname.c_str());
    SgType *t = isSgType(type);
    assert(t != 0);
    SgInitializedName *def = new SgInitializedName(GetFileInfo(), name, t);
    def->set_endOfConstruct(def->get_file_info());
    v = InsertVar(def, loc);
    SgVariableDeclaration *decl = new SgVariableDeclaration(GetFileInfo());
    decl->set_endOfConstruct(decl->get_file_info());

    if (initexp != 0) {
      SgAssignInitializer *NEW_VAR_INIT(init, def, initexp);
      decl->append_variable(def, init);
    } else
      decl->append_variable(def, 0);
    def->set_parent(decl);
    if (delayDecl)
      SaveVarDecl(decl, loc);
    else if (loc != 0) {
      loc->insertStatementInScope(decl, true);
      decl->set_parent(loc);
    } else
      ROSE_ABORT();

  } else {
    std::cerr << "Warning: new var has already been initialized: " << varname
              << "\n";
  }
  return v;
}

SgFunctionSymbol *AstInterfaceImpl::GetFunc(const std::string &name) {
  const char *start = name.c_str();
  SgFunctionSymbol *f = LookupFunction(start, scope);
  return f;
}

SgFunctionSymbol *
AstInterfaceImpl::NewFunc(const std::string &name, SgType *rtype,
                          const list<SgInitializedName *> &args) {
  const char *start = name.c_str();
  SgFunctionType *ft = new SgFunctionType(rtype, false);
  SgFunctionDeclaration *d =
      new SgFunctionDeclaration(GetFileInfo(), start, ft);
  for (list<SgInitializedName *>::const_iterator p = args.begin();
       p != args.end(); ++p) {
    SgInitializedName *cur = *p;
    d->append_arg(cur);
  }
  return AddFunc(d);
}

SgClassSymbol *AstInterfaceImpl ::NewClass(const std::string &classname) {
  if (DebugSymbol())
    std::cerr << "adding new class " << classname << "\n";
  SgClassDeclaration *decl =
      new SgClassDeclaration(GetFileInfo(), classname.c_str());

  return AddClass(decl);
}

SgMemberFunctionSymbol *
AstInterfaceImpl ::NewMemberFunc(SgClassDeclaration *classDecl,
                                 const std::string &name, SgType *rtype,
                                 const list<SgInitializedName *> &args) {
  const char *start = name.c_str();
  SgClassDefinition *classDefn = GetClassDefn(classDecl);
  if (classDefn == 0) {
    if (DebugSymbol())
      std::cerr << " creating new class defn " << classDecl->get_name().str()
                << "when member function " << start << "was not found. \n";
    classDefn = new SgClassDefinition(GetFileInfo(), classDecl);
    classDefn->set_endOfConstruct(classDefn->get_file_info());
    assert(scope != 0);
    classDecl->set_parent(scope);
    classDecl->set_definition(classDefn);
  }

  SgMemberFunctionType *ft = new SgMemberFunctionType(rtype, false);
  SgMemberFunctionDeclaration *d =
      new SgMemberFunctionDeclaration(GetFileInfo(), start, ft, 0);
  d->set_scope(classDefn);
  for (list<SgInitializedName *>::const_iterator p = args.begin();
       p != args.end(); ++p) {
    SgInitializedName *cur = *p;
    d->append_arg(cur);
  }

  return AddMemberFunc(classDefn, d);
}

AstNodePtr GetFunctionDecl(const AstNodePtr &_s);

std::string AstInterface::GetGlobalUniqueName(const AstNodePtr &_scope,
                                              std::string expname,
                                              bool do_not_add_file_name) {
  SgNode *scope = AstNodePtrImpl(_scope).get_ptr();
  assert(scope != 0);
  std::string result = expname;
  std::string scopename = expname;
  if (function_name_mangling_ && IsFunctionDefinition(_scope)) {
    auto *decl = isSgFunctionDeclaration(GetFunctionDecl(_scope).get_ptr());
    assert(decl != 0);
    scopename = function_name_mangling_(decl);
    if (expname != "") {
      return scopename + "::" + expname;
    }
    return scopename;
  }
  while (scope != 0 && scope->variantT() != V_SgGlobal) {
    DebugVariable([&scope]() {
      return "GetGlobalUniqueName:scope:" + AstToString(scope);
    });
    if (IsBlock(scope, &scopename) && scopename != "" &&
        result.find(scopename + "::") >= result.size()) {
      DebugVariable([&scopename]() {
        return "GetGlobalUniqueName:scope_name:" + scopename;
      });
      if (result == "")
        result = scopename;
      else {
        auto result_in_scopename_index = scopename.find(result);
        bool result_in_scopename = result_in_scopename_index < scopename.size();
        // Check that result is indeed part of the scope name on both ends (b0
        // and b1).
        bool b0_is_good = result_in_scopename_index == 0 ||
                          (result_in_scopename &&
                           scopename[result_in_scopename_index - 1] == ':');
        bool b1_is_good =
            result_in_scopename &&
            (result_in_scopename_index + result.size() == scopename.size() ||
             scopename[result_in_scopename_index + result.size()] == '_');
        if (b0_is_good && b1_is_good)
          result = scopename;
        else
          result = scopename + "::" + result;
      }
    }
    scope = AstInterfaceImpl::GetScope(scope);
  }
  if (!do_not_add_file_name) {
    if (scopename == "main" ||
        CmdOptions::GetInstance()->HasOption("-global_via_filename")) {
      std::string filename = scope->get_file_info()->get_filenameString();
      auto location = filename.rfind("/");
      if (location < filename.size()) {
        filename = filename.substr(location + 1);
      }
      if (result.find(filename) >= result.size()) {
        result = filename + "::" + result;
      }
    }
  }
  result.erase(std::remove_if(result.begin(), result.end(), ::isspace),
               result.end());
  return result;
}

bool AstInterface::IsExprStmt(const AstNodePtr &n, AstNodePtr *exp) {
  SgExprStatement *s = isSgExprStatement((SgNode *)n.get_ptr());
  if (s == 0)
    return false;
  if (exp != 0)
    *exp = s->get_expression();
  return true;
}

std::string AstInterface::toString(OperatorEnum op) {
  const char *nameList[] = {"OP_NONE",
                            "UOP_MINUS",
                            "UOP_ADDR",
                            "UOP_DEREF",
                            "UOP_ALLOCATE",
                            "UOP_NOT",
                            "UOP_SEMANTIC_CONVERSION",
                            "UOP_CAST_C",
                            "UOP_CAST_CONST",
                            "UOP_CAST_STATIC",
                            "UOP_CAST_DYNAMIC",
                            "UOP_CAST_REINTERP",
                            "UOP_CAST_BUILTIN_BIT",
                            "UOP_CAST_FUNCTIONAL",
                            "UOP_CAST_FUNCTIONAL_LIST",
                            "UOP_INCR1",
                            "UOP_INCR1_POST",
                            "UOP_DECR1",
                            "UOP_DECR1_POST",
                            "UOP_BIT_COMPLEMENT",
                            "BOP_DOT_ACCESS",
                            "BOP_ARROW_ACCESS",
                            "BOP_TIMES",
                            "BOP_DIVIDE",
                            "BOP_MOD",
                            "BOP_PLUS",
                            "BOP_MINUS",
                            "BOP_EQ",
                            "BOP_LE",
                            "BOP_LT",
                            "BOP_NE",
                            "BOP_GT",
                            "BOP_GE",
                            "BOP_AND",
                            "BOP_OR",
                            "BOP_BIT_AND",
                            "BOP_BIT_OR",
                            "BOP_BIT_RSHIFT",
                            "BOP_BIT_LSHIFT",
                            "OP_ARRAY_ACCESS",
                            "OP_ASSIGN",
                            "OP_UNKNOWN"};
  return std::string(nameList[op]);
}

std::string AstInterface::unparseToString(const AstNodePtr &n) {
  SgNode *s = (SgNode *)n.get_ptr();
  return ::unparseToString(s);
}

std::string AstInterface::AstTypeToString(const AstNodePtr &n) {
  std::string res;
  switch (n.get_type()) {
  case AstNodePtr::SpecialAstType::NULL_AST:
    res = "_NULL_";
    break;
  case AstNodePtr::SpecialAstType::UNKNOWN_AST:
    res = "_UNKNOWN_";
    break;
  case AstNodePtr::SpecialAstType::UNKNOWN_FUNCTION_CALL:
    res = "_UNKNOWN_FUNCTION_CALL_";
    break;
  case AstNodePtr::SpecialAstType::UNKNOWN_PTR_REF:
    res = "_UNKNOWN_PTR_REF_";
    break;
  case AstNodePtr::SpecialAstType::SG_AST:
    res = "";
    break;
  case AstNodePtr::SpecialAstType::GLOBAL_SIGNATURE:
    res = "_GLOBAL_" + n.get_signature();
    break;
  default:
    std::cerr << "Error: Unhandled case."
              << "\n";
    assert(0);
  }
  return res;
}

std::string AstInterface::AstToString(const AstNodePtr &n, bool withClassName) {
  std::string res = AstTypeToString(n);
  SgNode *s = (SgNode *)n.get_ptr();
  if (s == 0)
    return res;
  if (withClassName)
    res = res + string(s->sage_class_name()) + ":";
  res = res + ::unparseToString(s);
  return res;
}

// Return "@line_number:column_number" for an AST node
// Used for debugging or pretty-printing an node
std::string AstInterface::getAstLocation(const AstNodePtr &_s) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == 0)
    return "";
  string r = "";

  // Add line:column info.
  Sg_File_Info *fileInfo = s->get_file_info();
  stringstream sline, scol;
  sline << fileInfo->get_line();
  scol << fileInfo->get_col();
  r = r + "@" + sline.str() + ":" + scol.str();
  return r;
}

void AstInterface::FreeAstTree(const AstNodePtr &n) {}

void NotifyTreeCopy(AstInterfaceImpl &fa, const AstNodePtr &_orig,
                    const AstNodePtr &_n) {
  AstNodePtrImpl orig(_orig), n(_n);
  vector<SgNode *> childvec = orig->get_traversalSuccessorContainer();
  vector<SgNode *> childvec1 = n->get_traversalSuccessorContainer();
  assert(childvec.size() == childvec1.size());
  for (size_t i = 0; i < childvec.size(); ++i) {
    AstNodePtrImpl c = childvec[i], c1 = childvec1[i];
    if (c != c1)
      NotifyTreeCopy(fa, c, c1);
  }
  CopyAstRecord info(fa, orig, n);
  fa.Notify(info);
};

bool AstInterface ::get_fileInfo(const AstNodePtr &_n, std::string *fname,
                                 int *lineno) {
  SgNode *n = AstNodePtrImpl(_n).get_ptr();
  Sg_File_Info *f = n->get_file_info();
  if (fname != NULL) {
    *fname = f->get_filename();
  }
  if (lineno != NULL)
    *lineno = f->get_line();
  return true;
}

AstNodePtr AstInterface ::CopyAstTree(const AstNodePtr &_orig) {
  SgNode *orig = AstNodePtrImpl(_orig).get_ptr();
  if (orig == 0) {
    return _orig;
  }
  if (orig->variantT() == V_SgInitializedName) {
    AstNodePtrImpl r(ToVarRef(*impl, orig));
    return r;
  }
  SgTreeCopy copyOption;
  SgNode *r = orig->copy(copyOption);
  if (impl->NumberOfObservers())
    NotifyTreeCopy(*impl, _orig, AstNodePtrImpl(r));
  return AstNodePtrImpl(r);
}

AstInterface::AstNodeList AstInterface ::GetChildrenList(const AstNodePtr &_n) {
  SgNode *n = AstNodePtrImpl(_n).get_ptr();
  AstNodeList childlist;
  const vector<SgNode *> &childvec = n->get_traversalSuccessorContainer();
  for (size_t i = 0; i < childvec.size(); ++i) {
    childlist.push_back(childvec[i]);
  }
  return childlist;
}

void AstInterface ::SetParent(const AstNodePtr &n, const AstNodePtr &p) {
  SgNode *node = AstNodePtrImpl(n).get_ptr();
  assert(node != NULL);
  SgNode *parent = AstNodePtrImpl(p).get_ptr();
  node->set_parent(parent);
}

AstNodePtr AstInterface ::GetParent(const AstNodePtr &n) {
  AstNodePtrImpl node(n);
  if (node == GetRoot())
    return AST_NULL;
  return AstNodePtrImpl(node->get_parent());
}

bool AstInterface::IsDecls(const AstNodePtr &_s) {
  AstNodePtrImpl s(_s);
  switch (s->variantT()) {
  case V_SgVariableDeclaration:
  case V_SgClassDeclaration:
  case V_SgFunctionDeclaration:
  case V_SgDeclarationStatement:
  case V_SgEnumDeclaration:
  case V_SgTypedefDeclaration:
  case V_SgTemplateDeclaration:
    return true;
  default:
    DebugVariable(
        [&s]() { return std::string("Not decl: ") + s->class_name(); });
    return false;
  }
}
bool AstInterface::IsStatement(const AstNodePtr &_s) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  return isSgStatement(s) != 0;
}

bool AstInterface::IsExecutableStmt(const AstNodePtr &_s) {
  // jichi(9/11/2009): Add in support for fortran loops.
  AstNodePtrImpl s(_s);
  switch (s->variantT()) {
  case V_SgFortranDo:
    // case V_SgFortranNonBlockedDo:        // This kind of Fortran block is
    // temporarily not supported.

  case V_SgForStatement:
  case V_SgCaseOptionStmt:
  case V_SgExprStatement:
  case V_SgExpression:
  case V_SgGotoStatement:
  case V_SgIfStmt:
  case V_SgWhileStmt:
  case V_SgDoWhileStmt:
  case V_SgTryStmt:
  case V_SgBreakStmt:
  case V_SgContinueStmt:
  case V_SgReturnStmt:
  case V_SgSpawnStmt:
    // case V_SgVariableDeclaration:
    //  QY: Do not consider vardecl as executable or it will break loopProcessor
    return true;

  case V_SgLabelStatement:
    // jichi (10/9/2009): Disable process of empty Fortran label statement.
    // Mostly the node is replaced from SgContinueStmt paired with FortranDo.
    if (IS_FORTRAN_LANGUAGE() &&
        isSgLabelStatement(s.get_ptr())->get_statement() == NULL)
      return false;
    else
      return true;

  default:
    return false;
  }
}

AstNodePtr AstInterface::GetPrevStmt(const AstNodePtr &s) {
  SgNode *n = AstNodePtrImpl(s).get_ptr();
  SgNode *p = n->get_parent();
  assert(p != 0);
  vector<SgNode *> childvec = p->get_traversalSuccessorContainer();
  size_t i = 0;
  for (; i < childvec.size(); ++i)
    if (childvec[i] == n)
      break;
  if (i == 0)
    return AST_NULL;
  else {
    AstNodePtrImpl r = childvec[i - 1];
    return r;
  }
}

AstNodePtr AstInterface::GetNextStmt(const AstNodePtr &s) {
  SgNode *n = AstNodePtrImpl(s).get_ptr();
  SgNode *p = n->get_parent();
  assert(p != 0);
  vector<SgNode *> childvec = p->get_traversalSuccessorContainer();
  size_t i = 0;
  for (; i < childvec.size(); ++i)
    if (childvec[i] == n)
      break;
  if (i == childvec.size() - 1)
    return AST_NULL;
  else {
    AstNodePtrImpl r = childvec[i + 1];
    return r;
  }
}

bool AstInterface::IsIf(const AstNodePtr &_s, AstNodePtr *cond,
                        AstNodePtr *truebody, AstNodePtr *falsebody) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  int t = s->variantT();
  switch (t) {
  case V_SgIfStmt: {
    SgIfStmt *is = isSgIfStmt(s);
    if (cond != 0)
      *cond = AstNodePtrImpl(is->get_conditional());
    if (truebody != 0)
      *truebody = AstNodePtrImpl(is->get_true_body());
    if (falsebody != 0)
      *falsebody = AstNodePtrImpl(is->get_false_body());
  } break;
  case V_SgCaseOptionStmt: {
    SgCaseOptionStmt *cs = isSgCaseOptionStmt(s);
    if (cond != 0)
      *cond = AstNodePtrImpl(cs->get_key());
    if (truebody != 0)
      *truebody = AstNodePtrImpl(cs->get_body());
    if (falsebody != 0)
      *falsebody = AST_NULL;
  } break;
  default:
    return false;
  }
  return true;
}

bool AstInterface::IsLabelStatement(const AstNodePtr &_s) {
  AstNodePtrImpl s(_s);
  return s->variantT() == V_SgLabelStatement;
}

bool AstInterface::IsReturn(const AstNodePtr &_s, AstNodePtr *val) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  switch (s->variantT()) {
  case V_SgReturnStmt:
    if (val != 0) {
      *val = AstNodePtrImpl(isSgReturnStmt(s)->get_expression());
    }
    return true;
  default:
    return false;
  }
}

bool AstInterface::IsGoto(const AstNodePtr &_s, AstNodePtr *dest) {
  // TODO jichi(9/11/2009): Add in support for fortran loops.
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  switch (s->variantT()) {
  case V_SgGotoStatement:
    if (dest != 0) {
      SgLabelStatement *label = isSgGotoStatement(s)->get_label();
      *dest = AstNodePtrImpl(label);
    }
    break;
  case V_SgReturnStmt:
    if (dest != 0) {
      SgNode *scope = NULL;
      for (scope = s->get_parent();
           ((scope != NULL) && (scope->variantT() != V_SgFunctionDefinition));
           scope = scope->get_parent()) {
        assert(scope != NULL);
      }
      *dest = AstNodePtrImpl(scope);
    }
    break;
  case V_SgContinueStmt:
    // jichi (10/9/2009): Add in FortranDo support
    if (dest != 0) {
      SgNode *scope = 0;
      for (scope = s->get_parent();; scope = scope->get_parent()) {
        int t = scope->variantT();
        if (t == V_SgForStatement || t == V_SgWhileStmt || t == V_SgDoWhileStmt)
          break;
      }
      if (scope->variantT() == V_SgFortranDo)
        return false;

      if (scope->variantT() == V_SgForStatement)
        scope = isSgForStatement(scope)->get_increment();
      *dest = AstNodePtrImpl(scope);
    }
    break;
  case V_SgBreakStmt:
    if (dest != 0) {
      SgNode *scope = 0;
      for (scope = s->get_parent();; scope = scope->get_parent()) {
        int t = scope->variantT();
        if (t == V_SgForStatement || t == V_SgWhileStmt ||
            t == V_SgDoWhileStmt || t == V_SgSwitchStatement)
          break;
      }
      *dest = AstNodePtrImpl(scope);
    }
    break;
  default:
    return false;
  }
  return true;
}
// goto the point before destination
bool AstInterface::IsGotoBefore(const AstNodePtr &_s) {
  AstNodePtrImpl s(_s);
  switch (s->variantT()) {
  case V_SgGotoStatement:
  case V_SgContinueStmt:
    return true;
  default:
    return false;
  }
}
bool AstInterface::IsGotoAfter(const AstNodePtr &_s) {
  AstNodePtrImpl s(_s);
  switch (s->variantT()) {
  case V_SgReturnStmt:
  case V_SgBreakStmt:
    return true;
  default:
    return false;
  }
}

AstNodePtr GetFunctionDecl(const AstNodePtr &_s) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  int t = s->variantT();
  switch (t) {
  case V_SgFunctionDefinition:
    return AstNodePtrImpl(isSgFunctionDefinition(s)->get_declaration());
  case V_SgTemplateFunctionRefExp:
    return AstNodePtrImpl(isSgTemplateFunctionDefinition(s)->get_declaration());
  case V_SgFunctionDeclaration:
  case V_SgMemberFunctionDeclaration:
  case V_SgTemplateMemberFunctionDeclaration:
    return _s;
  case V_SgMemberFunctionRefExp:
    return AstNodePtrImpl(
        isSgMemberFunctionRefExp(s)->get_symbol()->get_declaration());
  case V_SgNonrealRefExp:
    return AstNodePtrImpl(
        isSgNonrealRefExp(s)->get_symbol()->get_declaration());
  case V_SgFunctionSymbol:
    return AstNodePtrImpl(isSgFunctionSymbol(s)->get_declaration());
  case V_SgFunctionRefExp:
    return AstNodePtrImpl(
        isSgFunctionRefExp(s)->get_symbol()->get_declaration());
  case V_SgMemberFunctionSymbol:
    return AstNodePtrImpl(isSgMemberFunctionSymbol(s)->get_declaration());
  case V_SgConstructorInitializer:
    return AstNodePtrImpl(isSgConstructorInitializer(s)->get_declaration());
  case V_SgDotExp:
    return GetFunctionDecl(AstNodePtrImpl(isSgDotExp(s)->get_rhs_operand()));
  }
  MLOG_ERROR_CXX("astInterface")
      << "Error: not recognizable function type: " << s->sage_class_name()
      << " at " << s->get_file_info()->get_filenameString() << ":"
      << s->get_file_info()->get_line() << endl;
  MLOG_ERROR_MORE_CXX() << s->unparseToString() << endl;
  ROSE_ABORT();
}

bool AstInterface::IsFunctionDefinition(const AstNodePtr &_s, std::string *name,
                                        AstNodeList *params,
                                        AstNodeList *outpars, AstNodePtr *body,
                                        AstTypeList *paramtype,
                                        AstNodeType *returntype,
                                        bool use_global_unique_name,
                                        bool skip_function_declaration)

{
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == 0)
    return false;
  (void)use_global_unique_name;
  SgFunctionParameterList *l = 0;
  SgNode *d = s;
  SgFunctionDefinition *def = 0;
  if (s->variantT() == V_SgFunctionDefinition) {
    def = isSgFunctionDefinition(s);
    d = def->get_declaration();
  }

  switch (d->variantT()) {
  case V_SgTemplateInstantiationFunctionDecl:
  case V_SgProcedureHeaderStatement:
  case V_SgFunctionDeclaration: {
    SgFunctionDeclaration *decl = isSgFunctionDeclaration(d);
    if (skip_function_declaration && def == 0 && decl->get_definition() == 0) {
      return false;
    }
    if (returntype != 0)
      *returntype = AstNodeTypeImpl(decl->get_type()->get_return_type());
    if (name != 0)
      *name = string(decl->get_name().str());
    if (paramtype != 0 || params != 0)
      l = decl->get_parameterList();
    if (def == 0) {
      AstNodePtr def_node = AstInterface::GetFunctionDefinitionFromDeclaration(
          AstNodePtrImpl(decl));
      def = isSgFunctionDefinition(def_node.get_ptr());
    }
    break;
  }
  case V_SgNonrealDecl: {
    SgNonrealDecl *decl = isSgNonrealDecl(d);
    if (returntype != 0)
      *returntype = AstNodeTypeImpl(decl->get_type());
    if (name != 0)
      *name = string(decl->get_name().str());
    // I can't seem to get parameters from an SgNonrealDecl -Jim Leek
    //      if (paramtype != 0 || params != 0)
    //  l = decl->get_parameterList();
    break;
  }
  // Liao 2/6/2015, try to extend to support Fortran
  case V_SgProgramHeaderStatement: {
    SgProgramHeaderStatement *decl = isSgProgramHeaderStatement(d);
    if (returntype != 0)
      *returntype = AstNodeTypeImpl(decl->get_type()->get_return_type());
    if (name != 0)
      *name = string(decl->get_name().str());
    if (paramtype != 0 || params != 0)
      l = decl->get_parameterList();
    break;
  }
  case V_SgTemplateInstantiationMemberFunctionDecl:
  case V_SgMemberFunctionDeclaration: {
    SgMemberFunctionDeclaration *decl = isSgMemberFunctionDeclaration(d);
    if (skip_function_declaration && def == 0 && decl->get_definition() == 0) {
      return false;
    }
    if (returntype != 0)
      *returntype = AstNodeTypeImpl(decl->get_type()->get_return_type());
    if (name != 0) {
      SgName cn = decl->get_scope()->get_qualified_name();
      SgName fn = decl->get_name();
      *name = StripGlobalQualifier(string(cn.str())) +
              "::" + ::StripGlobalQualifier(string(fn.str()));
    }
    if (paramtype != 0 || params != 0)
      l = decl->get_parameterList();
    if (outpars != 0) {
      auto init = decl->get_CtorInitializerList()->get_ctors();
      for (auto p = init.begin(); p != init.end(); ++p) {
        SgInitializedName *name = *p;
        outpars->push_back(name);
      }
    }
    if (def == 0) {
      AstNodePtr def_node = AstInterface::GetFunctionDefinitionFromDeclaration(
          AstNodePtrImpl(decl));
      def = isSgFunctionDefinition(def_node.get_ptr());
    }
    break;
  }

  default:
    return false;
  }
  if (body != 0 && def != 0) {
    *body = AstNodePtrImpl(def->get_body());
  }
  if (l != 0) {
    SgInitializedNamePtrList &names = l->get_args();
    for (SgInitializedNamePtrList::iterator p = names.begin(); p != names.end();
         ++p) {
      SgInitializedName *cur = *p;
      if (paramtype != 0)
        paramtype->push_back(AstNodeTypeImpl(cur->get_type()));
      if (params != 0)
        params->push_back(cur);
      if (outpars != 0 && cur->get_type()->variantT() == V_SgReferenceType)
        outpars->push_back(cur);
    }
  }
  return true;
}

//! Check if a node is an assignment statement/expression, grab its lhs and rhs.
//! Use readlhs to tell whether the value of lhs is read before being modified
//! in the assignment (e.g., whether the assignment is +=, -= etc.)
bool AstInterfaceImpl::IsAssignment(const SgNode *s, SgNode **lhs, SgNode **rhs,
                                    bool *readlhs) {
  if (s == 0)
    return false;
  if (s->variantT() == V_SgInitializedName) {
    const SgNode *parent = s->get_parent();
    if (parent != 0 && parent->variantT() == V_SgCtorInitializerList) {
      if (rhs != 0) {
        *rhs = isSgInitializedName(s)->get_initializer();
      }
      if (lhs != 0) {
        *lhs = const_cast<SgNode *>(s);
      }
      return true;
    }
    return false;
  }
  const SgExprStatement *n = isSgExprStatement(s);
  const SgExpression *exp = (n != 0) ? n->get_expression() : isSgExpression(s);
  if (exp != 0) {
    switch (exp->variantT()) {
    case V_SgPlusAssignOp:
    case V_SgMinusAssignOp:
    case V_SgAndAssignOp:
    case V_SgIorAssignOp:
    case V_SgMultAssignOp:
    case V_SgDivAssignOp:
    case V_SgModAssignOp:
    case V_SgXorAssignOp: {
      const SgBinaryOp *s2 = isSgBinaryOp(exp);
      if (lhs != 0)
        *lhs = s2->get_lhs_operand();
      if (rhs != 0) {
        *rhs = const_cast<SgExpression *>(exp);
      }
      if (readlhs != 0)
        *readlhs = true;
      return true;
    }
    case V_SgAssignOp: {
      const SgBinaryOp *s2 = isSgBinaryOp(exp);
      if (lhs != 0)
        *lhs = s2->get_lhs_operand();
      if (rhs != 0) {
        SgNode *init = s2->get_rhs_operand();
        if (init->variantT() == V_SgAssignInitializer)
          init = isSgAssignInitializer(init)->get_operand();
        *rhs = init;
      }
      if (readlhs != 0)
        *readlhs = false;
      return true;
    }
    default:
      return false;
    }
  }
  return false;
}

bool AstInterface::IsAssignment(const AstNodePtr &_s, AstNodePtr *lhs,
                                AstNodePtr *rhs, bool *readlhs) {
  SgNode *local_lhs = 0;
  SgNode *local_rhs = 0;
  SgNode **_lhs = (lhs == 0) ? ((SgNode **)0) : &local_lhs;
  SgNode **_rhs = (rhs == 0) ? ((SgNode **)0) : &local_rhs;
  if (AstInterfaceImpl::IsAssignment(_s.get_ptr(), _lhs, _rhs, readlhs)) {
    if (lhs) {
      *lhs = AstNodePtr(*_lhs);
    }
    if (rhs) {
      *rhs = AstNodePtr(*_rhs);
    }
    return true;
  }
  return false;
}

//! Check if $_s$ is a variable declaration node;
//! If yes, return the declared variables and their initial values
bool AstInterface::IsVariableDecl(const AstNodePtr &_s, AstNodeList *vars,
                                  AstNodeList *init) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == 0)
    return false;

  if (SgVariableDeclaration *decl = isSgVariableDeclaration(s)) {
    DebugDecl([&_s]() { return "Finding variable decl:" + AstToString(_s); });
    if (vars == 0 && init == 0)
      return true;
    SgInitializedNamePtrList &names = decl->get_variables();
    for (SgInitializedName *name : names) {
      ASSERT_not_null(name);
      const bool recognized = IsVariableDecl(name, vars, init);
      ROSE_ASSERT(recognized);
    }
    return true;
  }

  if (SgInitializedName *var = isSgInitializedName(s)) {
    DebugDecl([&_s]() { return "Finding variable decl:" + AstToString(_s); });
    if (vars == 0 && init == 0)
      return true;
    SgExpression *def = var->get_initializer();
    if (def != 0) {
      switch (def->variantT()) {
      case V_SgAssignInitializer:
        def = isSgAssignInitializer(def)->get_operand();
        break;
      default:
        break;
      }
    }
    if (vars != 0)
      vars->push_back(var);
    if (init != 0)
      init->push_back(def);
    return true;
  }

  return false;
}

bool AstInterface::IsAliasingDecl(const AstNodePtr &_s, AstNodeList *vars,
                                  AstNodeList *aliases) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == 0)
    return false;
  DebugVariable(
      [&_s]() { return "IsAliasingDecl:" + AstInterface::AstToString(_s); });
  switch (s->variantT()) {
  case V_SgVariableDeclaration: {
    bool has_alias = false;
    SgInitializedNamePtrList &names =
        isSgVariableDeclaration(s)->get_variables();
    for (SgInitializedNamePtrList::iterator p = names.begin(); p != names.end();
         ++p) {
      if (IsAliasingDecl(*p, vars, aliases)) {
        has_alias = true;
      }
    }
    return has_alias;
  }
  case V_SgInitializedName: {
    SgInitializedName *var = isSgInitializedName(s);
    SgType *type = var->get_type();
    if (type == 0) {
      return false;
    }
    type = type->stripType(SgType::STRIP_MODIFIER_TYPE |
                           SgType::STRIP_TYPEDEF_TYPE);
    if (type == 0) {
      return false;
    }
    SgExpression *init = var->get_initializer();
    if (init != 0 && init->variantT() == V_SgAssignInitializer) {
      init = isSgAssignInitializer(init)->get_operand();
    }
    switch (type->variantT()) {
    case V_SgPointerType: {
      if (IsAddressOfOp(init)) {
        if (vars != 0) {
          vars->push_back(var);
        }
        if (aliases != 0) {
          aliases->push_back(init);
        }
        return true;
      }
      return false;
    }
    case V_SgReferenceType: {
      if (init != 0 && IsMemoryAccess(init)) {
        if (vars != 0) {
          vars->push_back(var);
        }
        if (aliases != 0) {
          aliases->push_back(init);
        }
        return true;
      }
      return false;
    }
    default:
      return false;
    }
  }
  case V_SgCommonBlockObject: {
    if (vars != 0 || aliases != 0) {
      SgCommonBlockObject *comm = isSgCommonBlockObject(s);
      assert(comm != 0);
      for (auto *e : comm->get_variable_reference_list()->get_expressions()) {
        SgVarRefExp *v = isSgVarRefExp(e);
        assert(v != 0);
        if (vars != 0)
          vars->push_back(v->get_symbol()->get_declaration());
        if (aliases != 0) {
          AstNodePtr global("_COMMON_" + comm->get_block_name() +
                            "::" + v->get_symbol()->get_name().str());
          aliases->push_back(global);
        }
      }
    }
    return true;
  }
  case V_SgCommonBlock:
    for (auto *e : isSgCommonBlock(s)->get_block_list()) {
      bool r = IsAliasingDecl(e, vars, aliases);
      assert(r);
    }
    return true;
  default:
    return false;
  }
}

AstNodePtr AstInterface::CreateAllocateArray(const AstNodePtr &_arr,
                                             const AstNodeType &_elemtype,
                                             const AstNodeList &indexsize) {
  SgType *elemtype = AstNodeTypeImpl(_elemtype).get_ptr();
  SgType *atype = elemtype;
  for (AstNodeList::const_iterator p = indexsize.begin(); p != indexsize.end();
       ++p) {
    SgExpression *exp = isSgExpression(AstNodePtrImpl(*p).get_ptr());
    assert(exp != 0);
    atype = new SgArrayType(atype, exp);
  }
  SgType *baseType = elemtype;
  assert(baseType != NULL);
  SgExpression *arr = ToExpression(*impl, AstNodePtrImpl(_arr).get_ptr());
  SgNewExp *rhs = new SgNewExp(
      GetFileInfo(), atype, 0,
      new SgConstructorInitializer(GetFileInfo(), NULL, NULL, baseType, false,
                                   false, false, true));
  return AstNodePtrImpl(::CreateAssignment(*impl, arr, rhs));
}

AstNodePtr AstInterface::CreateDeleteArray(const AstNodePtr &_arr) {
  SgNode *arr = AstNodePtrImpl(_arr).get_ptr();
  SgExpression *var = isSgExpression(arr);
  assert(var != 0);
  return AstNodePtrImpl(new SgDeleteExp(GetFileInfo(), var, true));
}

AstNodePtr AstInterface::CreateLoop(const AstNodePtr &_cond,
                                    const AstNodePtr &_body) {
  SgStatement *cond = isSgStatement(AstNodePtrImpl(_cond).get_ptr());
  assert(cond != 0);
  SgStatement *bstmt = isSgStatement(AstNodePtrImpl(_body).get_ptr());
  assert(bstmt != 0);
  SgBasicBlock *body = isSgBasicBlock(AstNodePtrImpl(_body).get_ptr());
  if (body == 0) {
    NEW_BLOCK1(body, bstmt);
  }
  SgNode *result = new SgWhileStmt(GetFileInfo(), cond, body);
  cond->set_parent(result);
  body->set_parent(result);
  return AstNodePtrImpl(result);
}

AstNodePtr AstInterface::CreateAssignment(const AstNodePtr &_lhs,
                                          const AstNodePtr &_rhs) {
  SgNode *lhs = AstNodePtrImpl(_lhs).get_ptr(),
         *rhs = AstNodePtrImpl(_rhs).get_ptr();
  SgExpression *lhsexp = ToExpression(*impl, lhs);
  SgExpression *rhsexp = ToExpression(*impl, rhs);
  AstNodePtrImpl res = ::CreateAssignment(*impl, lhsexp, rhsexp);
  if (impl->NumberOfObservers()) {
    CopyAstRecord info(*impl, _rhs, _lhs);
    impl->Notify(info);
  }
  return res;
}

bool AstInterface::IsIOInputStmt(const AstNodePtr &s, AstNodeList *varlist) {
  return false;
}
bool AstInterface::IsIOOutputStmt(const AstNodePtr &s, AstNodeList *explist) {
  return false;
}

//! Check if $_exp$ is a single integer constant; if yes, return the constant
//! value in $val$.
bool AstInterface::IsConstInt(const AstNodePtr &_exp, int *val) {
  SgNode *exp = stripExactValueIdentityConversions(
      AstNodePtrImpl(_exp).get_ptr(), "AstInterface::IsConstInt", false);
  if (exp == 0)
    return false;
  if (exp->variantT() == V_SgIntVal) {
    if (val != 0)
      *val = isSgIntVal(exp)->get_value();
    return true;
  }
  return false;
}

bool AstInterface::IsConstant(const AstNodePtr &_exp, string *valtype,
                              string *val) {
  SgNode *exp = stripExactValueIdentityConversions(
      AstNodePtrImpl(_exp).get_ptr(), "AstInterface::IsConstant", false);
  if (exp == 0)
    return false;
  switch (exp->variantT()) {
  case V_SgStringVal:
    if (valtype != 0)
      *valtype = "string";
    break;
  case V_SgCharVal:
  case V_SgWcharVal:
  case V_SgSignedCharVal:
  case V_SgUnsignedCharVal:
    if (valtype != 0)
      *valtype = "char";
    break;
  case V_SgShortVal:
  case V_SgUnsignedShortVal:
  case V_SgIntVal:
  case V_SgEnumVal:
  case V_SgUnsignedIntVal:
  case V_SgLongIntVal:
  case V_SgLongLongIntVal:
  case V_SgUnsignedLongLongIntVal:
  case V_SgUnsignedLongVal:
    if (valtype != 0)
      *valtype = "int";
    break;
  case V_SgFloatVal:
    if (valtype != 0)
      *valtype = "float";
    break;
  case V_SgDoubleVal:
  case V_SgLongDoubleVal:
    if (valtype != 0)
      *valtype = "double";
    break;
  case V_SgSizeOfOp: /* consider size of a constant b/c it's value doesn't
                        change */
  {
    if (valtype != 0)
      *valtype = "int";
    break;
  }
  default:
    return false;
  };
  if (val != 0) {
    assert(isSgType(exp) == 0);
    *val = exp->unparseToString();
  }
  return true;
}

//! Two references are the same if they have the same name and same scope
bool AstInterface::IsSameVarRef(const AstNodePtr &_n1, const AstNodePtr &_n2) {
  AstNodePtrImpl n1(_n1), n2(_n2);
  string name1, name2;
  if (IsVarRef(n1, 0, &name1, 0, 0, /*use_global_unique_name=*/true) &&
      IsVarRef(n2, 0, &name2, 0, 0, /*use_global_unique_name=*/true)) {
    return name1 == name2;
  }
  return false;
}

bool AstInterface::IsMin(const AstNodePtr &_exp) {
  std::string name;
  if (!IsVarRef(_exp, 0, &name, 0, 0)) {
    return false;
  }
  if (name == "min" || name == "min2" || name == "min3")
    return true;
  return false;
}

bool AstInterface::IsMax(const AstNodePtr &_exp) {
  std::string name;
  if (!IsVarRef(_exp, 0, &name, 0, 0))
    return false;
  if (name == "max" || name == "max2" || name == "max3")
    return true;
  return false;
}

//! Check if $_exp$ is a variable reference (including all name references which
//! may have functions or objects have values)
bool AstInterfaceImpl::IsVarRef(SgNode *exp, SgType **vartype,
                                std::string *varname, SgNode **_scope,
                                bool *defined_in_global,
                                bool use_global_unique_name,
                                bool *has_ptr_deref) {
  if (exp == 0)
    return false;
  SgNode *decl = 0, *scope = 0;
  switch (exp->variantT()) {
  case V_SgNonrealRefExp: {
    SgNonrealRefExp *reference = isSgNonrealRefExp(exp);
    SgNonrealSymbol *sb = reference->get_symbol();
    assert(sb != 0);
    if (reference->get_resolved_variable_declaration() != nullptr) {
      SgInitializedName *resolved_name =
          SageInterface::requireResolvedVariableTemplateReference(
              reference, "AstInterface::IsVarRef");
      decl = resolved_name;
      scope = AstInterfaceImpl::GetScope(resolved_name);
      if (vartype != 0)
        *vartype = resolved_name->get_type();
      if (varname != 0)
        *varname = resolved_name->get_name().str();
      break;
    }
    SgScopeStatement *cdef = sb->get_scope();
    assert(cdef != 0);
    if (varname != 0) {
      *varname = StripGlobalQualifier(cdef->get_qualified_name()) +
                 "::" + StripGlobalQualifier(sb->get_name().str());
    }
    if (vartype != 0)
      *vartype = sb->get_type();
  } break;
  case V_SgMemberFunctionRefExp: {
    const SgMemberFunctionRefExp *var = isSgMemberFunctionRefExp(exp);
    assert(var != 0);
    SgFunctionSymbol *sb = var->get_symbol();
    assert(sb != 0);
    decl = sb->get_declaration();
    if (vartype != 0)
      *vartype = sb->get_type();
    if (varname != 0)
      *varname = sb->get_name().str();
    scope = decl;
  } break;
  case V_SgTemplateMemberFunctionRefExp: {
    const SgTemplateMemberFunctionRefExp *var =
        isSgTemplateMemberFunctionRefExp(exp);
    assert(var != 0);
    SgFunctionSymbol *sb = var->get_symbol();
    assert(sb != 0);
    decl = sb->get_declaration();
    if (vartype != 0)
      *vartype = sb->get_type();
    if (varname != 0)
      *varname = sb->get_name().str();
    scope = decl;
  } break;
  case V_SgTemplateFunctionRefExp: {
    const SgTemplateFunctionRefExp *var = isSgTemplateFunctionRefExp(exp);
    assert(var != 0);
    SgFunctionSymbol *sb = var->get_symbol();
    assert(sb != 0);
    decl = sb->get_declaration();
    if (vartype != 0)
      *vartype = sb->get_type();
    if (varname != 0)
      *varname = sb->get_name().str();
    scope = decl;
  } break;
  case V_SgFunctionRefExp: {
    const SgFunctionRefExp *var = isSgFunctionRefExp(exp);
    assert(var != 0);
    SgFunctionSymbol *sb = var->get_symbol();
    assert(sb != 0);
    decl = sb->get_declaration();
    if (vartype != 0)
      *vartype = sb->get_type();
    if (varname != 0)
      *varname = sb->get_name().str();
    scope = decl;
  } break;
  case V_SgVarRefExp: {
    const SgVarRefExp *var = isSgVarRefExp(exp);
    SgVariableSymbol *sb = var->get_symbol();
    if (vartype != 0)
      *vartype = sb->get_type();
    if (varname != 0)
      *varname = sb->get_name().str();
    decl = sb->get_declaration();
    scope = AstInterfaceImpl::GetScope(decl);
  } break;
  case V_SgThisExp: {
    const SgThisExp *var = isSgThisExp(exp);
    if (vartype != 0)
      *vartype = var->get_type();
    if (varname != 0)
      *varname = "this";
    scope = GetScope(exp);
  } break;
  case V_SgInitializedName: {
    SgInitializedName *var = isSgInitializedName(exp);
    if (var->get_name().str() == 0) {
      std::cerr << "no name for " << var->class_name() << "\n";
      return false;
    }
    SgType *t = var->get_type();
    assert(t != 0);
    if (vartype != 0)
      *vartype = t;
    if (varname != 0)
      *varname = var->get_name().str();
    decl = var;
    scope = AstInterfaceImpl::GetScope(var);
  } break;
  case V_SgPointerDerefExp:
    if (has_ptr_deref != 0) {
      *has_ptr_deref = true;
    }
    if (IsVarRef(isSgPointerDerefExp(exp)->get_operand(), vartype, varname,
                 _scope, defined_in_global, use_global_unique_name)) {
      if (varname != 0) {
        (*varname) = "_deref_(" + (*varname) + ")";
      }
      if (vartype != 0) {
        SgPointerType *ptype =
            isSgPointerType(AstNodeTypeImpl(*vartype).get_ptr());
        if (ptype != 0) {
          *vartype = AstNodeTypeImpl(ptype->get_base_type()).get_ptr();
        }
      }
      break;
    }
    return false;
  case V_SgDotStarOp: {
    const SgBinaryOp *exp1 = isSgBinaryOp(exp);
    SgVarRefExp *var2 = isSgVarRefExp(exp1->get_rhs_operand());
    if (var2 == 0)
      return false;
    if (has_ptr_deref != 0) {
      *has_ptr_deref = true;
    }
    SgVariableSymbol *sb2 = var2->get_symbol();
    if (vartype != 0)
      *vartype = sb2->get_type();
    if (varname != 0) {
      *varname = StripQualifier(std::string(sb2->get_name().str()));
    }
  } break;
  case V_SgArrowExp:
    if (has_ptr_deref != 0 &&
        !isSgThisExp(isSgBinaryOp(exp)->get_lhs_operand())) {
      *has_ptr_deref = true;
    }
    ROSE_FALLTHROUGH;
  case V_SgDotExp: {
    const SgBinaryOp *exp1 = isSgBinaryOp(exp);
    SgNode *lhs = exp1->get_lhs_operand();
    if (isSgThisExp(lhs) != 0) {
      if (!IsVarRef(exp1->get_rhs_operand(), vartype, varname, _scope,
                    defined_in_global, use_global_unique_name, has_ptr_deref)) {
        return false;
      }
      return true;
    }
    while (SgCastExp *cast = isSgCastExp(lhs)) {
      cast->validate_semantic_conversion();
      SgExpression *operand = cast->get_operand();
      Sg_File_Info *cast_info = cast->get_file_info();
      if (cast->cast_type() != SgCastExp::e_implicit_cast) {
        if (cast_info != nullptr && cast_info->isImplicitCast()) {
          std::cerr << "REX_AST_INVARIANT[var-ref-implicit-cast-kind]: "
                       "source-implicit member-access base has an explicit "
                       "cast kind"
                    << std::endl;
          ROSE_ABORT();
        }
        return false;
      }
      for (Sg_File_Info *position :
           {cast->get_file_info(), cast->get_startOfConstruct(),
            cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
        if (position == nullptr || !position->isCompilerGenerated() ||
            !position->isOutputInCodeGeneration() ||
            !position->isImplicitCast() || position->isTransformation()) {
          std::cerr << "REX_AST_INVARIANT[var-ref-implicit-cast-source]: "
                       "semantic implicit member-access base lacks exact "
                       "synthesized provenance"
                    << std::endl;
          ROSE_ABORT();
        }
      }

      switch (cast->get_semantic_conversion_kind()) {
      case SgCastExp::e_semantic_conversion_NoOp:
      case SgCastExp::e_semantic_conversion_LValueToRValue:
      case SgCastExp::e_semantic_conversion_DerivedToBase:
      case SgCastExp::e_semantic_conversion_UncheckedDerivedToBase:
        break;
      default:
        std::cerr << "REX_AST_INVARIANT[var-ref-implicit-cast-transparency]: "
                     "member-access base cast="
                  << cast << " kind="
                  << static_cast<int>(cast->get_semantic_conversion_kind())
                  << " does not preserve the exact member-base identity"
                  << std::endl;
        ROSE_ABORT();
      }
      lhs = operand;
    }
    std::string varname1;
    if (!IsVarRef(lhs, 0, &varname1, &scope, defined_in_global,
                  use_global_unique_name, has_ptr_deref)) {
      return false;
    }
    SgNode *rhs = exp1->get_rhs_operand();
    SgVarRefExp *var2 = isSgVarRefExp(rhs);
    SgNonrealRefExp *nonreal_var = isSgNonrealRefExp(rhs);
    SgType *member_type = nullptr;
    std::string member_name;
    if (var2 != nullptr) {
      SgVariableSymbol *sb2 = var2->get_symbol();
      if (sb2 == nullptr)
        return false;
      member_type = sb2->get_type();
      member_name = sb2->get_name().str();
    } else if (nonreal_var != nullptr &&
               nonreal_var->get_resolved_variable_declaration() != nullptr) {
      SgInitializedName *resolved_name =
          SageInterface::requireResolvedVariableTemplateReference(
              nonreal_var, "AstInterface::IsVarRef member access");
      member_type = resolved_name->get_type();
      member_name = resolved_name->get_name().str();
    } else {
      return false;
    }
    if (vartype != 0)
      *vartype = member_type;
    if (varname != 0) {
      auto dot = (exp->variantT() == V_SgDotExp) ? "." : "->";
      *varname = varname1 + dot + StripQualifier(member_name);
    }
  } break;
  default:
    return false;
  }
  if (_scope != 0 || defined_in_global != 0 || use_global_unique_name) {
    if (scope == 0 && decl != 0) {
      std::cerr << "Both should be defined, or neither should. \n";
      assert(false);
    }
    if (_scope != 0 && scope != 0) {
      *_scope = (isSgScopeStatement(scope) ? scope
                                           : AstInterfaceImpl::GetScope(scope));
    }
    if (defined_in_global != 0)
      *defined_in_global = (scope == 0 || scope->variantT() == V_SgGlobal);
    if (use_global_unique_name && varname != 0 && (*varname) != "") {
      DebugVariable([varname, scope]() {
        return "Variable-scope:" + *varname + ":" +
               AstInterface::AstToString(scope);
      });
      if (scope != 0) {
        if (decl != scope) {
          *varname = AstInterface::GetGlobalUniqueName(scope, *varname);
        } else {
          *varname = AstInterface::GetGlobalUniqueName(scope, "");
        }
      }
      DebugVariable([varname]() { return "global Variable name:" + *varname; });
    }
  }
  if (varname != 0) {
    DebugVariable([exp, varname]() {
      return "IsVarRef:" + exp->class_name() + ":" + *varname;
    });
  }
  return true;
}

//! Strip the casting operations to get to the real expression.
SgNode *AstInterface::SkipCasting(SgNode *exp) {
  return stripExactValueIdentityConversions(exp, "AstInterface::SkipCasting",
                                            true);
}

bool AstInterface::IsVarRef(const AstNodePtr &_exp, AstNodeType *vartype,
                            string *varname, AstNodePtr *scope,
                            bool *defined_in_global,
                            bool use_global_unique_name, bool *has_ptr_deref) {
  if (_exp == AST_NULL)
    return false;
  SgNode *var_scope = 0;
  SgType *var_type = 0;
  SgType **_vartype = (vartype == 0) ? (SgType **)0 : &var_type;
  SgNode **_scope = (scope == 0) ? (SgNode **)0 : &var_scope;
  if (AstInterfaceImpl::IsVarRef(_exp.get_ptr(), _vartype, varname, _scope,
                                 defined_in_global, use_global_unique_name,
                                 has_ptr_deref)) {
    if (_vartype != 0)
      *vartype = *_vartype;
    if (_scope != 0)
      *scope = *_scope;
    return true;
  }
  return false;
}

std::string AstInterface::GetScopeName(const AstNodePtr &_scope) {
  SgNode *s = AstNodePtrImpl(_scope).get_ptr();
  SgScopeStatement *scope = isSgScopeStatement(s);
  assert(scope != 0);
  return StripGlobalQualifier(scope->get_qualified_name().str());
}

string AstInterface::GetVarName(const AstNodePtr &exp,
                                bool use_global_unique_name) {
  string name;
  if (IsVarRef(exp, 0, &name, 0, 0, use_global_unique_name)) {
    return name;
  }
  std::cerr << "Error: expecting a variable reference but getting:" +
                   AstToString(exp);
  assert(false);
  return "";
}

AstNodeType AstInterface::GetExpressionType(const AstNodePtr &s) {
  AstNodeType t;
  if (!IsExpression(s, &t))
    ROSE_ABORT();
  return t;
}

bool AstInterface::IsFunctionType(const AstNodeType &t,
                                  AstTypeList *paramtypes) {
  SgType *type = AstNodeTypeImpl(t).get_ptr();
  if (type == nullptr) {
    return false;
  }
  type =
      type->stripType(SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);
  if (SgPointerType *ptr = isSgPointerType(type)) {
    type = ptr->get_base_type();
  }
  SgFunctionType *ftype = isSgFunctionType(type);
  if (ftype == nullptr) {
    return false;
  }
  if (paramtypes != nullptr) {
    const SgTypePtrList &args = ftype->get_arguments();
    for (SgTypePtrList::const_iterator it = args.begin(); it != args.end();
         ++it) {
      paramtypes->push_back(AstNodeTypeImpl(*it));
    }
  }
  return true;
}

string AstInterface::NewVar(const AstNodeType &_type, const string &name,
                            bool makeunique, bool delayInsert,
                            const AstNodePtr &_declLoc,
                            const AstNodePtr &_init) {
  if (DebugNewVar())
    std::cerr << "Enter NewVar:" << name << "\n";
  SgType *type = AstNodeTypeImpl(_type).get_ptr();

  SgNode *declLoc = AstNodePtrImpl(_declLoc).get_ptr();
  if (DebugNewVar())
    std::cerr << "declLoc=" << declLoc << "\n";

  SgScopeStatement *scope = (declLoc == 0) ? 0 : isSgScopeStatement(declLoc);
  if (scope == 0 && declLoc != 0) {
    scope = isSgScopeStatement(GetScope(declLoc));
  }
  if (DebugNewVar())
    std::cerr << "scope=" << scope << "\n";

  SgExpression *e = 0;
  if (_init != AST_NULL)
    e = ToExpression(*impl, (SgNode *)_init.get_ptr());
  SgVariableSymbol *sb =
      impl->NewVar(isSgType(type), name, makeunique, delayInsert, e, scope);
#ifndef NDEBUG
  SgInitializedName *def = sb->get_declaration();
  assert(def != 0 && !HasNullParent(def));
#endif

  if (DebugNewVar())
    std::cerr << "Finish creating NewVar:" << name << "\n";
  SgName n = sb->get_name();
  string varname = string(n.str());
  return varname;
}

void AstInterface::AddNewVarDecls() { impl->AddNewVarDecls(); }

void AstInterface::CopyNewVarDecls(const AstNodePtr &nblock, bool clear) {
  SgBasicBlock *blk = isSgBasicBlock((SgNode *)nblock.get_ptr());
  if (blk == 0) {
    std::cerr << "nblock is not a block: " << AstToString(nblock) << "\n";
    ROSE_ABORT();
  }
  impl->CopyNewVarDecls(blk, clear);
}

void AstInterfaceImpl::AddNewVarDecls() {
  for (size_t i = newVarList.size(); i > 0; --i) {
    std::pair<SgScopeStatement *, SgVariableDeclaration *> cur =
        newVarList[i - 1];
    cur.first->insertStatementInScope(cur.second, true);
    cur.second->set_parent(cur.first);
  }
  newVarList.clear();
}

void AstInterfaceImpl::CopyNewVarDecls(SgBasicBlock *blk, bool clear) {
  for (size_t i = 0; i < newVarList.size(); ++i) {
    std::pair<SgScopeStatement *, SgVariableDeclaration *> cur = newVarList[i];
    SgVariableDeclaration *s = cur.second;
    if (!clear) {
      SgTreeCopy copyOption;
      s = isSgVariableDeclaration(cur.second->copy(copyOption));
    }
    assert(s != 0);
    blk->append_statement(s);
    s->set_parent(blk);
    // QY: each new decl has only one variable*/
    const SgInitializedNamePtrList &names = s->get_variables();
    assert(names.size() == 1);
    SgInitializedName *n = *names.begin();
    assert(n != 0);
    n->set_parent(s);
    InsertVar(n, blk);
  }
  if (clear)
    newVarList.clear();
}

SgVarRefExp *AstInterfaceImpl::CreateFieldRef(SgNode *decl, std::string name2) {
  assert(decl != 0);
  SgVariableSymbol *vs = LookupVar(name2, decl);
  SgVarRefExp *r = new SgVarRefExp(GetFileInfo(), vs);
  r->set_endOfConstruct(r->get_file_info());
  return r;
}

AstNodePtr AstInterface::CreateFieldRef(std::string name1, std::string name2) {
  auto *decl = impl->LookupNestedDeclaration(name1, impl->get_scope(0));
  return AstNodePtrImpl(impl->CreateFieldRef(decl, name2));
}

AstNodePtr AstInterface::CreateMethodRef(std::string classname,
                                         std::string fieldname,
                                         bool createIfNotFound) {
  SgClassDeclaration *c = isSgClassDeclaration(
      impl->LookupNestedDeclaration(classname, impl->get_scope(0)));
  if (c == 0) {
    std::cerr << "Error: cannot find class declaration for " << classname
              << std::endl;
    ROSE_ABORT();
  }
  SgMemberFunctionSymbol *f1 = impl->GetMemberFunc(c, fieldname);
  if (f1 == 0) {
    if (!createIfNotFound) {
      std::cerr << "Error: cannot find member function " << fieldname
                << std::endl;
      ROSE_ABORT();
    } else {
      f1 = impl->NewMemberFunc(c, fieldname, impl->GetTypeInt(),
                               std::list<SgInitializedName *>());
    }
  }
  SgMemberFunctionRefExp *NEW_MFUNCTION_REF(fr, f1);
  return AstNodePtrImpl(fr);
}

SgDotExp *AstInterfaceImpl::CreateVarMemberRef(std::string name1,
                                               std::string name2, SgNode *loc) {
  auto *obj = CreateVarRef(name1, loc);
  if (obj == 0)
    return 0;
  SgType *vartype = AstInterface::GetBaseType(obj->get_type()).get_ptr();
  assert(vartype != 0);
  auto *field = CreateFieldRef(vartype, name2);
  SgDotExp *NEW_BIN_OP(r, SgDotExp, obj, field, field->get_type());
  return r;
}

SgExpression *AstInterfaceImpl::CreateVarRef(std::string varname, SgNode *loc) {
  SgNode *loc1 = AstNodePtrImpl(loc).get_ptr();
  if (loc1 == 0)
    loc1 = scope;
  int hasdot = varname.rfind(".", varname.size() - 1);
  if (hasdot > 0) {
    std::string name1 = varname.substr(0, hasdot);
    std::string name2 = varname.substr(hasdot + 1, varname.size() - hasdot);
    return CreateVarMemberRef(name1, name2, loc1);
  }
  int hasarrow = varname.rfind("->", varname.size() - 1);
  if (hasarrow > 0) {
    std::string name1 = varname.substr(0, hasarrow);
    std::string name2 = varname.substr(hasarrow + 2, varname.size() - hasarrow);
    return CreateVarMemberRef(name1, name2, loc1);
  }
  SgScopeStatement *loc1_s = isSgScopeStatement(loc1);
  if (loc1_s == 0) {
    loc1_s = isSgScopeStatement(GetScope(loc1));
  }
  assert(loc1_s != 0);
  int is_this = varname.rfind("::this", varname.size() - 1);
  if (is_this > 0) {
    std::string name1 = varname.substr(0, is_this);
    auto *decl = LookupNestedDeclaration(name1, loc1_s);
    assert(decl != 0);
    SgClassDeclaration *decl1 = isSgClassDeclaration(decl);
    assert(decl1 != 0);
    SgSymbol *class_symbol = decl1->get_symbol_from_symbol_table();
    ROSE_ASSERT(class_symbol != NULL);
    SgMemberFunctionDeclaration *member = isSgMemberFunctionDeclaration(
        SageInterface::getEnclosingFunctionDeclaration(loc1_s, true));
    SgMemberFunctionType *member_type =
        member != nullptr ? isSgMemberFunctionType(member->get_type())
                          : nullptr;
    SgType *this_base_type = class_symbol->get_type();
    if (member_type == nullptr || this_base_type == nullptr) {
      fprintf(stderr, "REX_AST_INVARIANT[ast-interface-this-type]: Class::this "
                      "requires an exact enclosing member function type\n");
      ROSE_ABORT();
    }
    if (member_type->isConstFunc() || member_type->isVolatileFunc() ||
        member_type->isRestrictFunc()) {
      SgModifierType *qualified_type = new SgModifierType(this_base_type);
      SgTypeModifier &modifier = qualified_type->get_typeModifier();
      if (member_type->isConstFunc()) {
        modifier.get_constVolatileModifier().setConst();
      }
      if (member_type->isVolatileFunc()) {
        modifier.get_constVolatileModifier().setVolatile();
      }
      if (member_type->isRestrictFunc()) {
        modifier.setRestrict();
      }
      SgModifierType *canonical =
          SgModifierType::insertModifierTypeIntoTypeTable(qualified_type);
      if (canonical != qualified_type) {
        delete qualified_type;
      }
      this_base_type = canonical;
    }
    SgType *this_pointer_type = SgPointerType::createType(this_base_type);
    SgThisExp *p =
        new SgThisExp(GetFileInfo(), isSgClassSymbol(class_symbol),
                      isSgNonrealSymbol(class_symbol), 0, this_pointer_type);
    p->set_endOfConstruct(p->get_file_info());
    return p;
  }
  size_t deref_count = 0;
  while (deref_count < varname.size() && varname[deref_count] == '*') {
    ++deref_count;
  }
  std::string lookup_name = varname.substr(deref_count);
  SgVariableSymbol *sym = LookupVar(lookup_name, loc1_s);
  if (sym == 0) {
    std::cerr << "Error : variable " << varname << " not found in scope "
              << loc1->class_name() << ", which is derived from "
              << ((loc == 0) ? "NULL" : loc->class_name()) << "\n";
    ROSE_ABORT();
  }
  SgVarRefExp *ref = new SgVarRefExp(GetFileInfo(), sym);
  ref->set_endOfConstruct(ref->get_file_info());
  SgExpression *r = ref;
  for (size_t i = 0; i < deref_count; ++i) {
    r = new SgPointerDerefExp(
        GetFileInfo(), r,
        requireElementResultType(r->get_type(), "AstInterface::CreateVarRef"));
  }
  return r;
}

AstNodePtr AstInterface::CreateVarRef(std::string varname,
                                      const AstNodePtr &loc) {
  auto result = impl->CreateVarRef(varname, loc.get_ptr());
  if (result == 0) {
    return AST_UNKNOWN;
  }
  return AstNodePtrImpl(result);
}

AstNodeType AstInterface::GetType(const string &name) {
  if (name[name.size() - 1] == '*') {
    string name1 = name.substr(0, name.size() - 1);
    SgType *t = isSgType(AstNodeTypeImpl(GetType(name1)).get_ptr());
    SgPointerType *ptr = t->get_ptr_to();
    if (ptr == 0) {
      ptr = new SgPointerType(t);
      t->set_ptr_to(ptr);
    }
    return AstNodeTypeImpl(ptr);
  } else if (name == "char")
    return AstNodeTypeImpl(new SgTypeChar());
  else if (name == "int")
    return AstNodeTypeImpl(AstInterfaceImpl::GetTypeInt());
  else if (name == "long")
    return AstNodeTypeImpl(new SgTypeLong());
  else if (name == "void")
    return AstNodeTypeImpl(new SgTypeVoid());
  else if (name == "float")
    return AstNodeTypeImpl(new SgTypeFloat());
  else if (name == "double")
    return AstNodeTypeImpl(new SgTypeDouble());
  else if (name == "string")
    return AstNodeTypeImpl(new SgTypeString());
  else if (name == "bool")
    return AstNodeTypeImpl(new SgTypeBool());
  else {
    SgClassDeclaration *c = isSgClassDeclaration(
        impl->LookupNestedDeclaration(name, impl->get_scope(0)));
    if (c == 0) {
      cerr << "Error: not recognize type name : " << name << endl;
      ROSE_ABORT();
    } else
      return AstNodeTypeImpl(new SgClassType(c));
  }
}

AstNodeType AstInterface::GetArrayType(const AstNodeType &base,
                                       const AstNodeList &index) {
  if (IS_FORTRAN_LANGUAGE()) {
    SgType *btype = AstNodeTypeImpl(base).get_ptr();
    SgArrayType *atype = new SgArrayType(btype);

    SgExprListExp *NEW_EXPR_LIST(dim);
    for (AstNodeList::const_iterator p = index.begin(); p != index.end(); ++p) {
      SgExpression *i = isSgExpression(AstNodePtrImpl(*p).get_ptr());
      assert(i);
      dim->append_expression(i);
      i->set_parent(dim);
    }
    atype->set_dim_info(dim);
    dim->set_parent(atype);

    atype->set_rank(1);
    return AstNodeTypeImpl(atype);

  } else {

    SgType *r = AstNodeTypeImpl(base).get_ptr();
    for (AstNodeList::const_iterator p = index.begin(); p != index.end(); ++p) {
      if (AstNodePtrImpl(*p).get_ptr()->variantT() != V_SgIntVal) {
        return AstNodeTypeImpl(new SgPointerType(r));
      }
    }
    for (AstNodeList::const_iterator p1 = index.begin(); p1 != index.end();
         ++p1) {
      SgExpression *ie = isSgExpression(AstNodePtrImpl(*p1).get_ptr());
      assert(ie != 0);
      r = new SgArrayType(r, ie);
    }

    return AstNodeTypeImpl(r);
  }
}

bool AstInterface::IsAddressOfOp(const AstNodePtr &_s, AstNodePtr *ref) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == 0)
    return false;
  if (s->variantT() == V_SgAssignInitializer) {
    s = isSgAssignInitializer(s)->get_operand();
  }
  // Address-origin analysis has a different contract from value-identity
  // analysis.  A checked pointer up-cast may adjust the pointer value while
  // still preserving the exact source object whose address was taken.
  s = stripExactAddressOriginConversions(s, "AstInterface::IsAddressOfOp");
  if (s == 0)
    return false;
  if (s->variantT() == V_SgAddressOfOp) {
    if (ref != 0) {
      *ref = isSgAddressOfOp(s)->get_operand();
    }
    return true;
  }
  return false;
}

bool AstInterface::IsMemoryAllocation(const AstNodePtr &s, AstNodeType *exptype,
                                      AstNodePtr *init) {
  AstNodePtrImpl s1 = stripExactValueIdentityConversions(
      s.get_ptr(), "AstInterface::IsMemoryAllocation", false);
  AstNodePtr f;
  SgNode *node = s1.get_ptr();
  if (node == nullptr) {
    return false;
  }
  if (isSgExpression(node) == nullptr && isSgExprStatement(node) == nullptr) {
    return false;
  }
  AstInterfaceImpl astImpl(s1.get_ptr());
  AstInterface fa(&astImpl);
  if (fa.IsFunctionCall(s1, &f)) {
    std::string name;
    if (!IsVarRef(f, 0, &name)) {
      return false;
    }
    if (name == "malloc") {
      if (exptype != 0) {
        *exptype = GetExpressionType(s);
      }
      if (init != 0) {
        *init = AST_NULL;
      }
      return true;
    }
    return false;
  }
  SgNewExp *is_new = isSgNewExp(s1.get_ptr());
  if (is_new != 0) {
    if (exptype != 0) {
      *exptype = AstNodeTypeImpl(is_new->get_type());
    }
    if (init != 0) {
      *init = is_new->get_constructor_args();
    }
    return true;
  }
  return false;
}

bool AstInterface::IsMemoryFree(const AstNodePtr &s, AstNodeType *exptype,
                                AstNodePtr *variable) {
  AstNodePtrImpl s1 = SkipCasting(s.get_ptr()), f;
  AstNodeList params;
  SgNode *node = s1.get_ptr();
  if (node == nullptr) {
    return false;
  }
  if (isSgExpression(node) == nullptr && isSgExprStatement(node) == nullptr) {
    return false;
  }
  AstInterfaceImpl astImpl(s1.get_ptr());
  AstInterface fa(&astImpl);
  if (fa.IsFunctionCall(s1, &f, &params)) {
    std::string name;
    if (!IsVarRef(f, 0, &name)) {
      return false;
    }
    if (name == "free") {
      assert(params.size() == 1);
      AstNodePtrImpl param = AstNodePtrImpl(params.front());
      if (variable != 0) {
        *variable = SkipCasting(param.get_ptr());
      }
      if (exptype != 0) {
        *exptype = GetExpressionType(param);
      }
      return true;
    }
    return false;
  }
  SgDeleteExp *is_delete = isSgDeleteExp(s1.get_ptr());
  if (is_delete != 0) {
    if (variable != 0) {
      *variable = is_delete->get_variable();
    }
    if (exptype != 0) {
      *exptype = AstNodeTypeImpl(is_delete->get_variable()->get_type());
    }
    return true;
  }
  return false;
}

bool AstInterface::IsMemoryAccess(const AstNodePtr &_s, AstNodeList *subrefs) {
  if (_s.is_unknown()) {
    if (subrefs != 0) {
      subrefs->push_back(_s);
    }
    return true;
  }
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == 0)
    return false;
  switch (s->variantT()) {
  case V_SgConstructorInitializer:
    return false;
  case V_SgAssignInitializer:
    if (subrefs != 0) {
      IsMemoryAccess(isSgAssignInitializer(s)->get_operand(), subrefs);
    }
    return false;
  case V_SgCastExp: {
    SgCastExp *cast = isSgCastExp(s);
    cast->validate_semantic_conversion();
    // Every cast evaluates its operand.  Memory-access discovery therefore
    // descends through the checked edge without claiming that the conversion
    // itself is value-transparent.
    return IsMemoryAccess(cast->get_operand(), subrefs);
  }
  case V_SgAddressOfOp:
    if (subrefs != 0) {
      IsMemoryAccess(isSgUnaryOp(s)->get_operand(), subrefs);
    }
    return false;
  case V_SgCommaOpExp: {
    if (subrefs != 0) {
      SgCommaOpExp *comma = isSgCommaOpExp(s);
      IsMemoryAccess(comma->get_lhs_operand(), subrefs);
      IsMemoryAccess(comma->get_rhs_operand(), subrefs);
    }
    return false;
  }
  case V_SgConditionalExp: {
    if (subrefs != 0) {
      SgConditionalExp *conditional = isSgConditionalExp(s);
      conditional->validate();
      IsMemoryAccess(conditional->get_true_value_exp(), subrefs);
      IsMemoryAccess(conditional->get_false_exp(), subrefs);
    }
    return false;
  }
  case V_SgPointerDerefExp:
    if (subrefs != 0) {
      subrefs->push_back(_s);
    }
    return true;
  case V_SgDotExp:
  case V_SgArrowExp: {
    SgNode *rhs = isSgBinaryOp(s)->get_rhs_operand();
    if (rhs != nullptr && rhs->variantT() == V_SgVarRefExp) {
      if (subrefs != 0) {
        subrefs->push_back(s);
      }
      return true;
    }
    return false;
  }
  case V_SgFunctionRefExp:
  case V_SgTemplateFunctionRefExp:
  case V_SgMemberFunctionRefExp:
  case V_SgTemplateMemberFunctionRefExp:
    // A function designator is callable identity, not a memory location.
    // Function-to-pointer decay must not turn evaluation of a direct callee
    // into a variable read.
    return false;
  default: { // Function call returning C++ reference type is a memory access
    if (IsVarRef(_s) || IsArrayAccess(_s)) {
      if (subrefs != 0) {
        subrefs->push_back(s);
      }
      return true;
    }
    AstNodeTypeImpl t;
    if (s->variantT() == V_SgFunctionCallExp && IsExpression(_s, &t)) {
      // member function's return type may have several levels of typedef
      // Strip SgTypedefType off to get the real base type
      SgType *base_type = t.get_ptr();
      assert(base_type != 0);
      while (isSgTypedefType(base_type))
        base_type = isSgTypedefType(base_type)->get_base_type();
      if (base_type->variantT() == V_SgReferenceType) {
        if (subrefs != 0) {
          subrefs->push_back(s);
        }
        return true;
      }
    }
    return false;
  }
  } // end switch
}

//! Check if _s is an array access.
// If so, store array name in array, and subscripts into index[]
bool AstInterface::IsArrayAccess(const AstNodePtr &_s, AstNodePtr *array,
                                 AstList *index) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (s == nullptr) {
    return false;
  }
  switch (s->variantT()) {
  case V_SgDotExp:
  case V_SgArrowExp: {
    SgBinaryOp *dot = isSgBinaryOp(s);
    if (!IsVarRef(AstNodePtrImpl(dot->get_rhs_operand()))) {
      return false;
    }
    s = dot->get_lhs_operand();
    break;
  }
  default:
    break;
  }
  if (s == nullptr) {
    return false;
  }
  switch (s->variantT()) {
  case V_SgPntrArrRefExp:
    if (index != 0 || array != 0) {
      SgNode *n = s;
      while (true) {
        SgPntrArrRefExp *arr = isSgPntrArrRefExp(n);
        if (arr == 0)
          break;
        n = arr->get_lhs_operand();
        if (array != 0)
          *array = AstNodePtrImpl(n);
        if (index != 0) {
          // jichi(9/25/2009): Add Support for SgExprListExp for Fortran array
          // index.
          SgNode *exp = arr->get_rhs_operand();
          switch (exp->variantT()) {
          case V_SgExprListExp: // Fortan indices as expression list.
          {
            SgExprListExp *indexexp = isSgExprListExp(exp);
            assert(indexexp);

            SgExpressionPtrList &l = indexexp->get_expressions();
            SgExpressionPtrList::const_iterator p = l.begin();
            for (; p != l.end(); ++p) {
              SgExpression *pr = isSgExpression((SgNode *)(*p));
              assert(pr);
              index->push_back(pr);
            }
          } break;
          default:
            index->push_back(exp);
          }
        }
      }
    }
    return true;
  default:
    break;
  }
  return false;
}

bool AstInterface::IsBinaryOp(const AstNodePtr &_exp, OperatorEnum *opr,
                              AstNodePtr *opd1, AstNodePtr *opd2) {
  SgNode *exp = AstNodePtrImpl(_exp).get_ptr();
  if (exp == 0)
    return false;
  SgBinaryOp *op = isSgBinaryOp(exp);
  switch (exp->variantT()) {
  case V_SgEqualityOp:
    if (opr != 0)
      *opr = BOP_EQ;
    break;
  case V_SgNotEqualOp:
    if (opr != 0)
      *opr = BOP_NE;
    break;
  case V_SgGreaterOrEqualOp:
    if (opr != 0)
      *opr = BOP_GE;
    break;
  case V_SgLessOrEqualOp:
    if (opr != 0)
      *opr = BOP_LE;
    break;
  case V_SgLessThanOp:
    if (opr != 0)
      *opr = BOP_LT;
    break;
  case V_SgGreaterThanOp:
    if (opr != 0)
      *opr = BOP_GT;
    break;
  case V_SgAndOp:
    if (opr != 0)
      *opr = BOP_AND;
    break;
  case V_SgOrOp:
    if (opr != 0)
      *opr = BOP_OR;
    break;
  case V_SgMultiplyOp:
    if (opr != 0)
      *opr = BOP_TIMES;
    break;
  case V_SgDivideOp:
    if (opr != 0)
      *opr = BOP_DIVIDE;
    break;
  case V_SgModOp:
    if (opr != 0)
      *opr = BOP_MOD;
    break;
  case V_SgAddOp:
    if (opr != 0)
      *opr = BOP_PLUS;
    break;
  case V_SgSubtractOp:
    if (opr != 0)
      *opr = BOP_MINUS;
    break;
  case V_SgDotExp:
    if (opr != 0)
      *opr = BOP_DOT_ACCESS;
    break;
  case V_SgArrowExp:
    if (opr != 0)
      *opr = BOP_ARROW_ACCESS;
    break;
  case V_SgBitOrOp:
    if (opr != 0)
      *opr = BOP_BIT_OR;
    break;
  case V_SgBitAndOp:
    if (opr != 0)
      *opr = BOP_BIT_AND;
    break;
  case V_SgRshiftOp:
    if (opr != 0)
      *opr = BOP_BIT_RSHIFT;
    break;
  case V_SgLshiftOp:
    if (opr != 0)
      *opr = BOP_BIT_LSHIFT;
    break;
  default:
    return false;
  }
  if (opd1 != 0)
    *opd1 = AstNodePtrImpl(op->get_lhs_operand());
  if (opd2 != 0)
    *opd2 = AstNodePtrImpl(op->get_rhs_operand());
  return true;
}

//! Check if $_exp$ is an unary operation; if yes, return its operation type and
//! operand
bool AstInterface::IsUnaryOp(const AstNodePtr &_exp, OperatorEnum *opr,
                             AstNodePtr *opd) {
  SgNode *exp = AstNodePtrImpl(_exp).get_ptr();
  if (exp == 0)
    return false;
  switch (exp->variantT()) {
  case V_SgMinusOp:
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgMinusOp(exp)->get_operand());
    if (opr != 0) {
      *opr = UOP_MINUS;
    }
    return true;
  case V_SgAddressOfOp:
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgAddressOfOp(exp)->get_operand());
    if (opr != 0)
      *opr = UOP_ADDR;
    return true;
  case V_SgPointerDerefExp:
    if (opr != 0)
      *opr = UOP_DEREF;
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgPointerDerefExp(exp)->get_operand());
    return true;
  case V_SgNewExp:
    if (opr != 0)
      *opr = UOP_ALLOCATE;
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgNewExp(exp)->get_constructor_args());
    return true;
  case V_SgCastExp:
    isSgCastExp(exp)->validate_semantic_conversion();
    if (opr != 0) {
      switch (isSgCastExp(exp)->cast_type()) {
      case SgCastExp::cast_type_enum::e_implicit_cast:
        *opr = UOP_SEMANTIC_CONVERSION;
        break;
      case SgCastExp::cast_type_enum::e_C_style_cast:
        *opr = UOP_CAST_C;
        break;
      case SgCastExp::cast_type_enum::e_const_cast:
        *opr = UOP_CAST_CONST;
        break;
      case SgCastExp::cast_type_enum::e_static_cast:
        *opr = UOP_CAST_STATIC;
        break;
      case SgCastExp::cast_type_enum::e_dynamic_cast:
        *opr = UOP_CAST_DYNAMIC;
        break;
      case SgCastExp::cast_type_enum::e_reinterpret_cast:
        *opr = UOP_CAST_REINTERP;
        break;
      case SgCastExp::cast_type_enum::e_builtin_bit_cast:
        *opr = UOP_CAST_BUILTIN_BIT;
        break;
      case SgCastExp::cast_type_enum::e_functional_cast:
        *opr = UOP_CAST_FUNCTIONAL;
        break;
      case SgCastExp::cast_type_enum::e_functional_list_cast:
        *opr = UOP_CAST_FUNCTIONAL_LIST;
        break;
      default:
        std::cerr
            << "REX_AST_INVARIANT[unary-cast-surface]: AstInterface has no "
               "operator role for checked cast surface="
            << static_cast<int>(isSgCastExp(exp)->cast_type()) << std::endl;
        ROSE_ABORT();
      }
    }
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgCastExp(exp)->get_operand());
    return true;
  case V_SgMinusMinusOp:
    if (opr != 0)
      *opr = (isSgMinusMinusOp(exp)->get_mode() == SgUnaryOp::Sgop_mode::prefix)
                 ? UOP_DECR1
                 : UOP_DECR1_POST;
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgMinusMinusOp(exp)->get_operand());
    return true;
  case V_SgPlusPlusOp:
    if (opr != 0)
      *opr = (isSgPlusPlusOp(exp)->get_mode() == SgUnaryOp::Sgop_mode::prefix)
                 ? UOP_INCR1
                 : UOP_INCR1_POST;
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgPlusPlusOp(exp)->get_operand());
    return true;
  case V_SgAsmOp:
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgAsmOp(exp)->get_expression());
    if (opr != 0)
      *opr = OP_UNKNOWN;
    return true;
  case V_SgBitComplementOp:
    if (opd != 0)
      *opd = AstNodePtrImpl(isSgBitComplementOp(exp)->get_operand());
    if (opr != 0) {
      *opr = UOP_BIT_COMPLEMENT;
    }
    return true;
  default:
    return false;
  }
}

bool AstInterface::IsBlock(const AstNodePtr &_n, std::string *blockname,
                           AstNodeList *_stmts) {
  if (_n.is_null()) {
    return false;
  }
  AstNodePtr body;
  AstTypeList param_types;
  if (IsFunctionDefinition(_n, blockname, _stmts, 0, &body, &param_types)) {
    if (body != 0 && _stmts != 0) {
      _stmts->push_back(AstNodePtrImpl(body).get_ptr());
    }
    return true;
  }
  SgNode *n = AstNodePtrImpl(_n).get_ptr();
  if (n == 0) {
    return false;
  }
  switch (n->variantT()) {
  case V_SgBasicBlock:
  case V_SgSwitchStatement:
  case V_SgForInitStatement:
  case V_SgGlobal:
    if (_stmts != 0) {
      *_stmts = GetBlockStmtList(_n);
    }
    if (blockname != 0) {
      *blockname = "";
    }
    return true;
  case V_SgDeclarationScope:
    if (_stmts != 0) {
      SgDeclarationScope *declScope = isSgDeclarationScope(n);
      if (declScope != 0) {
        SgDeclarationStatementPtrList &decls = declScope->get_declarations();
        for (SgDeclarationStatementPtrList::iterator p = decls.begin();
             p != decls.end(); ++p) {
          _stmts->push_back(*p);
        }
      }
    }
    if (blockname != 0) {
      *blockname = isSgDeclarationScope(n)->get_qualified_name();
    }
    return true;
  case V_SgTemplateInstantiationDefn:
  case V_SgTemplateClassDefinition:
  case V_SgClassDefinition: {
    SgClassDefinition *def = isSgClassDefinition(n);
    if (blockname != 0) {
      *blockname = def->get_declaration()->get_name().getString();
    }
    if (_stmts != 0) {
      SgDeclarationStatementPtrList &decls = def->get_members();
      for (SgDeclarationStatementPtrList::iterator p = decls.begin();
           p != decls.end(); ++p) {
        _stmts->push_back(*p);
      }
    }
    return true;
  }
  case V_SgClassDeclaration:
  case V_SgTemplateClassDeclaration: {
    SgClassDeclaration *decl = isSgClassDeclaration(n);
    if (decl != 0) {
      if (blockname != 0) {
        *blockname = decl->get_name().getString();
      }
      if (_stmts != 0) {
        SgClassDefinition *def = GetClassDefn(decl);
        if (def != 0) {
          SgDeclarationStatementPtrList &decls = def->get_members();
          for (SgDeclarationStatementPtrList::iterator p = decls.begin();
               p != decls.end(); ++p) {
            _stmts->push_back(*p);
          }
        }
      }
      return true;
    }
    break;
  }
  case V_SgNamespaceDeclarationStatement: {
    SgNamespaceDeclarationStatement *decl =
        isSgNamespaceDeclarationStatement(n);
    if (blockname != 0) {
      *blockname = decl->get_name().getString();
    }
    if (_stmts != 0) {
      SgNamespaceDefinitionStatement *def = decl->get_definition();
      if (def != 0) {
        SgDeclarationStatementPtrList &decls = def->get_declarations();
        for (SgDeclarationStatementPtrList::iterator p = decls.begin();
             p != decls.end(); ++p) {
          _stmts->push_back(*p);
        }
      }
    }
    return true;
  }
  case V_SgNamespaceDefinitionStatement: {
    SgNamespaceDefinitionStatement *def = isSgNamespaceDefinitionStatement(n);
    if (blockname != 0) {
      *blockname = def->get_namespaceDeclaration()->get_name().getString();
    }
    if (_stmts != 0) {
      SgDeclarationStatementPtrList &decls = def->get_declarations();
      for (SgDeclarationStatementPtrList::iterator p = decls.begin();
           p != decls.end(); ++p) {
        _stmts->push_back(*p);
      }
    }
    return true;
  }
  default:
    break;
  }
  return false;
}

bool AstInterface::IsBlock(const AstNodePtr &_exp) {
  AstNodePtrImpl exp(_exp);
  switch (exp->variantT()) {
  case V_SgBasicBlock:
  case V_SgSwitchStatement:
  case V_SgForInitStatement:
    return true;
  default:
    break;
  };
  return false;
}

//! Check if $s$ is a function call; if yes, return the function and arguments
bool AstInterfaceImpl::IsFunctionCall(SgNode *s, SgNode **func,
                                      AstNodeList *args) {
  if (s == nullptr) {
    return false;
  }
  SgNode *exp = s;
  SgNode *f = 0;
  SgExprListExp *argexp = 0;

  switch (exp->variantT()) {
    //  case V_SgNonrealRefExp:
    // SgNonrealRef is from an uninstantiated template, so ignore it.  -Jim Leek
    // return false;
  case V_SgExprStatement:
    exp = isSgExprStatement(exp)->get_expression();
    return IsFunctionCall(exp, func, args);
  case V_SgAssignInitializer:
    exp = isSgAssignInitializer(exp)->get_operand();
    return IsFunctionCall(exp, func, args);
  case V_SgFunctionCallExp: {
    SgFunctionCallExp *fs = isSgFunctionCallExp(exp);
    f = fs->get_function();
    if (f == nullptr) {
      std::cerr << "REX_AST_INVARIANT[function-call-callee]: function call="
                << fs << " has no callee expression" << std::endl;
      ROSE_ABORT();
    }
    while (SgCastExp *cast = isSgCastExp(f)) {
      cast->validate_semantic_conversion();
      const SgCastExp::semantic_conversion_kind_enum conversion =
          cast->get_semantic_conversion_kind();
      if (conversion !=
              SgCastExp::e_semantic_conversion_FunctionToPointerDecay &&
          conversion != SgCastExp::e_semantic_conversion_LValueToRValue &&
          conversion != SgCastExp::e_semantic_conversion_NoOp) {
        break;
      }
      if (cast->cast_type() != SgCastExp::e_implicit_cast) {
        std::cerr
            << "REX_AST_INVARIANT[function-call-callee-conversion]: function "
               "call="
            << fs << " has a source-explicit cast classified as transparent "
            << static_cast<int>(conversion) << std::endl;
        ROSE_ABORT();
      }
      f = cast->get_operand();
    }
    SgExpression *callee_expression = isSgExpression(f);
    SgType *callee_type =
        callee_expression != nullptr ? callee_expression->get_type() : nullptr;
    SgType *stripped_callee_type =
        callee_type != nullptr ? callee_type->stripTypedefsAndModifiers()
                               : nullptr;
    if (SgPointerType *pointer_type = isSgPointerType(stripped_callee_type)) {
      stripped_callee_type = pointer_type->get_base_type();
      if (stripped_callee_type != nullptr) {
        stripped_callee_type =
            stripped_callee_type->stripTypedefsAndModifiers();
      }
    }
    const bool is_dependent_callee =
        isSgNonrealRefExp(f) != nullptr &&
        isSgNonrealType(stripped_callee_type) != nullptr;
    if (isSgFunctionType(stripped_callee_type) == nullptr &&
        !is_dependent_callee) {
      std::cerr << "REX_AST_INVARIANT[function-call-callee-type]: function "
                   "call="
                << fs << " callee=" << f << "/"
                << (f != nullptr ? f->class_name() : "<null>")
                << " has non-callable type="
                << (callee_type != nullptr ? callee_type->class_name()
                                           : "<null>")
                << std::endl;
      ROSE_ABORT();
    }
    argexp = fs->get_args();
    if (argexp == nullptr) {
      std::cerr << "REX_AST_INVARIANT[function-call-arguments]: function call="
                << fs << " has no argument-list expression" << std::endl;
      ROSE_ABORT();
    }
  } break;
  case V_SgConstructorInitializer: {
    SgConstructorInitializer *isinit_exp = isSgConstructorInitializer(exp);
    if (isinit_exp->get_class_decl() != 0 &&
        isinit_exp->get_declaration() != 0) {
      f = exp;
      argexp = isSgConstructorInitializer(exp)->get_args();
    } else
      return false;
  } break;
  default:
    return false;
  }

  switch (f->variantT()) {
  case V_SgDotExp: {
    SgDotExp *dot = isSgDotExp(f);
    SgNode *cur = dot->get_lhs_operand();
    f = dot->get_rhs_operand();
    if (args != 0)
      args->push_back(cur);
  } break;
  case V_SgArrowExp: {
    SgArrowExp *arrow = isSgArrowExp(f);
    SgNode *cur = arrow->get_lhs_operand();
    f = arrow->get_rhs_operand();
    if (args != 0)
      args->push_back(cur);
  } break;
  case V_SgArrowStarOp: {
    SgArrowStarOp *arrow = isSgArrowStarOp(f);
    SgNode *cur = arrow->get_lhs_operand();
    f = arrow->get_rhs_operand();
    if (args != 0)
      args->push_back(cur);
  } break;
  case V_SgPntrArrRefExp: {
    SgPntrArrRefExp *arrow = isSgPntrArrRefExp(f);
    SgNode *cur = arrow->get_lhs_operand();
    f = arrow->get_rhs_operand();
    if (args != 0)
      args->push_back(cur);
  } break;
  case V_SgMemberFunctionRefExp: {
    if (args != 0)
      args->push_back(0);
    break;
  }
  case V_SgTemplateMemberFunctionRefExp: {
    SgTemplateMemberFunctionSymbol *sym =
        isSgTemplateMemberFunctionRefExp(f)->get_symbol();
    if (sym != nullptr && sym->get_name() == "operator()") {
      break;
    }
    if (args != 0)
      args->push_back(0);
    break;
  }
  default:
    break;
  }
  if (argexp != 0) {
    SgExpressionPtrList l = argexp->get_expressions();
    for (SgExpressionPtrList::iterator p = l.begin(); p != l.end(); ++p) {
      if (args != 0) {
        args->push_back(*p);
      }
    }
  }
  if (func != 0)
    *func = f;
  return true;
}
/* Does not deal correctly with templates SgNorealExp */
bool AstInterface::IsFunctionCall(const AstNodePtr &_s, AstNodePtr *fname,
                                  AstNodeList *args, AstNodeList *outargs,
                                  AstTypeList *paramtypes,
                                  AstNodeType *returntype) {
  AstNodePtrImpl s(_s);
  AstNodeList Args;
  if (outargs != 0 && args == 0)
    args = &Args;
  SgNode *f;
  // Grab functionRefExp and argument expression list
  if (!AstInterfaceImpl::IsFunctionCall(s.get_ptr(), &f, args))
    return false;

  if (f->variantT() == V_SgPointerDerefExp)
    f = isSgPointerDerefExp(f)->get_operand();
  if (fname != 0) {
    *fname = AstNodePtrImpl(f);
  }
  if (outargs != 0 || paramtypes != 0 || returntype != 0) {
    if (SgNonrealRefExp *dependent_callee = isSgNonrealRefExp(f)) {
      SgNonrealSymbol *dependent_symbol = dependent_callee->get_symbol();
      Sg_File_Info *position = dependent_callee->get_startOfConstruct();
      std::cerr
          << "REX_AST_INVARIANT[dependent-function-call-details]: dependent "
             "callee="
          << f << " name="
          << (dependent_symbol != nullptr
                  ? dependent_symbol->get_name().getString()
                  : std::string("<no-symbol>"))
          << " at "
          << (position != nullptr ? position->get_filenameString()
                                  : std::string("<no-file>"))
          << ":" << (position != nullptr ? position->get_line() : 0)
          << " cannot provide concrete parameter, out-argument, or "
             "return-type details"
          << std::endl;
      ROSE_ABORT();
    }
    AstTypeList PTlist;
    if (paramtypes == 0)
      paramtypes = &PTlist;
    if (SgConstructorInitializer *ctor_init = isSgConstructorInitializer(f)) {
      SgFunctionDeclaration *decl = ctor_init->get_declaration();
      if (decl == nullptr) {
        MLOG_ERROR_C("astInterface",
                     "Could not get constructor declaration from %s, "
                     "Expression is %s at %s:%d\n",
                     f->class_name().c_str(), f->unparseToString().c_str(),
                     f->get_file_info()->get_filenameString().c_str(),
                     f->get_file_info()->get_line());
        ROSE_ABORT();
      }
      for (SgInitializedName *param : decl->get_args()) {
        paramtypes->push_back(AstNodeTypeImpl(param->get_type()));
      }
      if (returntype != 0) {
        *returntype = AstNodeTypeImpl(ctor_init->get_expression_type());
      }
    } else {
      SgExpression *function_expression = isSgExpression(f);
      SgType *t = function_expression != nullptr
                      ? function_expression->get_type()
                      : nullptr;
      if (t != nullptr) {
        t = t->stripTypedefsAndModifiers();
      }
      if (SgPointerType *pointer_type = isSgPointerType(t)) {
        t = pointer_type->get_base_type();
        if (t != nullptr) {
          t = t->stripTypedefsAndModifiers();
        }
      }
      SgFunctionType *ftype = isSgFunctionType(t);
      if (ftype != 0) {
        SgTypePtrList atypes = ftype->get_arguments();
        for (SgTypePtrList::const_iterator p = atypes.begin();
             p != atypes.end(); ++p) {
          paramtypes->push_back(AstNodeTypeImpl(*p));
        }
        if (returntype != 0)
          *returntype = AstNodeTypeImpl(ftype->get_return_type());
      } else {
        std::cerr
            << "REX_AST_INVARIANT[function-call-callee-type]: extracted callee="
            << f << "/" << (f != nullptr ? f->class_name() : "<null>")
            << " has no exact function type for call=" << s.get_ptr()
            << std::endl;
        ROSE_ABORT();
      }
    }
    // Store arguments of reference types into outargs
    if (outargs != 0) {
      if (paramtypes == nullptr || args == nullptr) {
        SgNode *node = s.get_ptr();
        auto *file_info = node != nullptr ? node->get_file_info() : nullptr;
        const std::string filename = file_info != nullptr
                                         ? file_info->get_filenameString()
                                         : "<unknown>";
        const int line = file_info != nullptr ? file_info->get_line() : 0;
        const std::string class_name =
            node != nullptr ? node->class_name() : "<unknown>";
        const std::string expression =
            node != nullptr ? node->unparseToString() : "<unknown>";
        MLOG_ERROR_C("astInterface",
                     "Could not collect function call out-arguments for %s, "
                     "Expression is %s at %s:%d\n",
                     class_name.c_str(), expression.c_str(), filename.c_str(),
                     line);
        ROSE_ABORT();
      }
      AstNodeList::const_iterator p1 = args->begin();
      SgMemberFunctionRefExp *member_ref = isSgMemberFunctionRefExp(f);
      SgTemplateMemberFunctionRefExp *template_member_ref =
          isSgTemplateMemberFunctionRefExp(f);
      SgMemberFunctionDeclaration *member_decl =
          member_ref != nullptr
              ? member_ref->getAssociatedMemberFunctionDeclaration()
              : (template_member_ref != nullptr
                     ? template_member_ref
                           ->getAssociatedMemberFunctionDeclaration()
                     : nullptr);
      if (member_decl != nullptr) {
        const bool is_static = member_decl->get_declarationModifier()
                                   .get_storageModifier()
                                   .isStatic();
        SgMemberFunctionType *member_type =
            isSgMemberFunctionType(member_decl->get_type());
        if (member_type == nullptr || p1 == args->end()) {
          std::cerr << "REX_AST_INVARIANT[member-call-object-argument]: member "
                       "callee="
                    << f
                    << " has no exact member-function type or implicit object "
                       "argument"
                    << std::endl;
          ROSE_ABORT();
        }

        AstNodePtr object_argument = *p1++;
        if (!is_static) {
          if (object_argument == AST_NULL) {
            // An unqualified member call uses the current object.  Its
            // mutation cannot be represented as an independent expression
            // argument; interprocedural member effects carry that identity.
          } else if (!member_type->isConstFunc()) {
            outargs->push_back(object_argument);
          }
        }
      }
      for (AstTypeList::const_iterator p = paramtypes->begin();
           p != paramtypes->end() && p1 != args->end(); ++p, ++p1) {
        SgType *t = AstNodeTypeImpl(*p).get_ptr();
        if (t != nullptr) {
          t = t->stripTypedefsAndModifiers();
        }
        AstNodePtr ref = *p1;
        SgType *referenced_type = nullptr;
        if (SgReferenceType *ref_type = isSgReferenceType(t)) {
          referenced_type = ref_type->get_base_type();
        } else if (SgPointerType *ptr_type = isSgPointerType(t)) {
          if (IsAddressOfOp(*p1, &ref)) {
            referenced_type = ptr_type->get_base_type();
          }
        }
        if (referenced_type != nullptr) {
          if (!SageInterface::isConstType(referenced_type)) {
            outargs->push_back(ref);
          }
        }
      }
    }
  }
  return true;
}

AstNodeType AstInterface::GetBaseType(const AstNodeType &t) {
  {
    SgPointerType *pointer_type = isSgPointerType(t.get_ptr());
    if (pointer_type != 0) {
      return GetBaseType(pointer_type->get_base_type());
    }
  }
  {
    SgModifierType *mod_type = isSgModifierType(t.get_ptr());
    if (mod_type != 0) {
      return GetBaseType(mod_type->get_base_type());
    }
  }
  return t;
}

namespace {
std::string astInterfaceSemanticTypeName(SgType *type) {
  ASSERT_not_null(type);

  if (SgNamedType *named_type = isSgNamedType(type)) {
    const std::string name = named_type->get_name();
    if (name.empty()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[ast-interface-type-name]: named type=%s has "
              "no semantic name\n",
              type->class_name().c_str());
      ROSE_ABORT();
    }
    return name;
  }
  if (SgReferenceType *reference_type = isSgReferenceType(type)) {
    return astInterfaceSemanticTypeName(reference_type->get_base_type()) + "&";
  }
  if (SgRvalueReferenceType *reference_type = isSgRvalueReferenceType(type)) {
    return astInterfaceSemanticTypeName(reference_type->get_base_type()) + "&&";
  }
  if (SgPointerType *pointer_type = isSgPointerType(type)) {
    return astInterfaceSemanticTypeName(pointer_type->get_base_type()) + "*";
  }
  if (SgModifierType *modifier_type = isSgModifierType(type)) {
    const SgTypeModifier &modifier = modifier_type->get_typeModifier();
    std::string result;
    if (modifier.get_constVolatileModifier().isConst()) {
      result += "const ";
    }
    if (modifier.get_constVolatileModifier().isVolatile()) {
      result += "volatile ";
    }
    if (modifier.isRestrict()) {
      result += "restrict ";
    }
    return result +
           astInterfaceSemanticTypeName(modifier_type->get_base_type());
  }

  switch (type->variant()) {
  case T_CHAR:
    return "char";
  case T_SIGNED_CHAR:
    return "signed char";
  case T_UNSIGNED_CHAR:
    return "unsigned char";
  case T_SHORT:
    return "short";
  case T_SIGNED_SHORT:
    return "signed short";
  case T_UNSIGNED_SHORT:
    return "unsigned short";
  case T_INT:
    return "int";
  case T_SIGNED_INT:
    return "signed int";
  case T_UNSIGNED_INT:
    return "unsigned int";
  case T_LONG:
    return "long";
  case T_SIGNED_LONG:
    return "signed long";
  case T_UNSIGNED_LONG:
    return "unsigned long";
  case T_LONG_LONG:
    return "long long";
  case T_SIGNED_LONG_LONG:
    return "signed long long";
  case T_UNSIGNED_LONG_LONG:
    return "unsigned long long";
  case T_SIGNED_128BIT_INTEGER:
    return "__int128";
  case T_UNSIGNED_128BIT_INTEGER:
    return "unsigned __int128";
  case T_VOID:
    return "void";
  case T_GLOBAL_VOID:
    return "global void";
  case T_WCHAR:
    return "wchar_t";
  case T_CHAR8:
    return "char8_t";
  case T_CHAR16:
    return "char16_t";
  case T_CHAR32:
    return "char32_t";
  case T_FLOAT:
    return "float";
  case T_DOUBLE:
    return "double";
  case T_LONG_DOUBLE:
    return "long double";
  case T_FLOAT80:
    return "__float80";
  case T_FLOAT128:
    return "__float128";
  case T_FLOAT16:
    return "_Float16";
  case T_FP16:
    return "__fp16";
  case T_BFLOAT16:
    return "__bf16";
  case T_FLOAT32X:
    return "_Float32x";
  case T_FLOAT64X:
    return "_Float64x";
  case T_FLOAT32:
    return "_Float32";
  case T_FLOAT64:
    return "_Float64";
  case T_BOOL:
    return "bool";
  case T_NULLPTR:
    return "decltype(nullptr)";
  case T_AUTO:
    return "auto";
  case T_STRING:
    return "string";
  case T_ELLIPSE:
    return "...";
  case T_COMPLEX: {
    SgTypeComplex *complex_type = isSgTypeComplex(type);
    ASSERT_not_null(complex_type);
    return astInterfaceSemanticTypeName(complex_type->get_base_type()) +
           " _Complex";
  }
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[ast-interface-type-name]: type=%s requires a "
            "typed semantic operation, not a reconstructed name\n",
            type->class_name().c_str());
    ROSE_ABORT();
  }
}
} // namespace

void AstInterfaceImpl::GetTypeInfo(SgType *t, std::string *tname,
                                   std::string *stripname, int *size,
                                   bool use_global_name) {
  {
    SgPointerType *pointer_type = isSgPointerType(t);
    if (pointer_type != 0) {
      GetTypeInfo(pointer_type->get_base_type(), tname, stripname, size,
                  use_global_name);
      if (tname != 0) {
        *tname = (*tname) + "_ptr_";
      }
      return;
    }
  }
  std::string typeName = astInterfaceSemanticTypeName(t);
  // For instantiated template types, return the original template type name.
  if (isSgNamedType(t)) {
    SgDeclarationStatement *decl = isSgNamedType(t)->get_declaration();
    SgTemplateInstantiationDecl *insDecl = isSgTemplateInstantiationDecl(decl);
    if (insDecl) {
      typeName = insDecl->get_templateDeclaration()->get_qualified_name();
    } else if (use_global_name) {
      typeName =
          AstInterface::GetGlobalUniqueName(decl, typeName,
                                            /*do_not_add_file_name=*/true);
    }
  }

  std::string r1 = ::StripGlobalQualifier(typeName);
  std::string result = "";
  for (size_t i = 0; i < r1.size(); ++i) {
    if (r1[i] == '[' || r1[i] == ']' || r1[i] == '{' || r1[i] == '}' ||
        r1[i] == ',' || r1[i] == '.') {
      result.push_back('_');
    } else if (r1[i] == '&') {
      result += "_ref_";
    } else if (r1[i] == '*') {
      result += "_deref_";
    } else if (r1[i] != ' ')
      result.push_back(r1[i]);
    else if (i + 2 < r1.size() && r1[i + 1] == ':' && r1[i + 2] == ':') {
      i += 2;
    }
  }
  if (tname != 0) {
    *tname = result;
  }
  if (stripname != 0)
    *stripname = StripParameterType(result);
  if (size != 0)
    *size = 4;
}

void AstInterface::GetTypeInfo(const AstNodeType &t, string *tname,
                               string *stripname, int *size,
                               bool use_global_name) {
  AstInterfaceImpl::GetTypeInfo(AstNodeTypeImpl(t).get_ptr(), tname, stripname,
                                size, use_global_name);
}

bool AstInterface::IsPointerType(const AstNodeType &__type) {
  AstNodeTypeImpl type(__type);
  return type.get_ptr()->variantT() == V_SgPointerType;
}

/*
bool AstInterface::
IsArrayType( const AstNodeType& s, AstNodeType* base)
{
  if (s->variantT() ==  V_SgArrayType) {
      if (base != 0) {
        SgType* n = s;
        while (true) {
          SgArrayType *arr = isSgArrayType(n);
          if (arr == 0)
            break;
          n = arr->get_base_type();
        }
        *base = n;
      }
      return true;
  }
  return false;
}
*/

bool AstInterface::IsArrayType(const AstNodeType &__type, int *__dim,
                               AstNodeType *__base_type,
                               std::string *annotation) {
  AstNodeTypeImpl type(__type);
  SgArrayType *t = isSgArrayType(type.get_ptr());
  if (t == 0)
    return false;

  if (__base_type)
    (*__base_type) = AstNodeTypeImpl(t->get_base_type());
  if (__dim)
    (*__dim) = t->get_rank();
  if (annotation != 0) {
    /*
        SgDeclarationStatement *d = t->getAssociatedDeclaration ();
        if (p != NULL) {
          *annotation = p->getString();
    std::cerr << "ANNOTATION:" << *annotation << "\n";
        }
    */
  }
  return true;
}

bool AstInterface::IsScalarType(const AstNodeType &__type) {
  AstNodeTypeImpl type(__type);
  if (type.get_ptr() == 0)
    return false;
  switch (type->variantT()) {
  case V_SgTypeChar:
  case V_SgTypeSignedChar:
  case V_SgTypeUnsignedChar:
  case V_SgTypeShort:
  case V_SgTypeSignedShort:
  case V_SgTypeUnsignedShort:
  case V_SgTypeInt:
  case V_SgTypeSignedInt:
  case V_SgTypeUnsignedInt:
  case V_SgTypeLong:
  case V_SgTypeSignedLong:
  case V_SgTypeUnsignedLong:
  case V_SgTypeVoid:
  case V_SgTypeWchar:
  case V_SgTypeFloat:
  case V_SgTypeDouble:
  case V_SgTypeLongLong:
  case V_SgTypeUnsignedLongLong:
  case V_SgTypeLongDouble:
  case V_SgTypeString:
  case V_SgTypeBool:
  case V_SgTypeComplex:
  case V_SgTypeImaginary:
    return true;
  default:
    break;
  }
  return false;
}

bool AstInterface::GetArrayBound(const AstNodePtr &_arrayref, int dim, int &lb,
                                 int &ub) {
  AstNodePtrImpl arrayref(_arrayref);
  SgNode *n = arrayref.get_ptr();
  SgArrayType *t = 0;
  if (dim == 0) {
    SgVarRefExp *var = isSgVarRefExp(n);
    if (var == 0)
      return false;
    SgVariableSymbol *sb = var->get_symbol();
    SgType *vt = sb->get_type();
    t = isSgArrayType(vt);
  } else {
    for (int i = 0; i < dim; ++i) {
      n = n->get_parent();
    }
    SgPntrArrRefExp *ar = isSgPntrArrRefExp(n);
    if (ar == 0)
      return false;
    t = isSgArrayType(ar->get_type());
  }
  if (t == 0)
    return false;
  SgExpression *exp = t->get_index();
  lb = 0;
  return IsConstInt(AstNodePtrImpl(exp), &ub);
}

//! Check whether $_s$ is an expression; If yes, return the expression
// (strip off SgExpressionRoot) and grab its type
bool AstInterface::IsExpression(const AstNodePtr &_s, AstNodeType *exptype,
                                AstNodePtr *strip_exp) {
  AstNodePtrImpl s(_s);
  if (s.get_ptr() == 0)
    return false;
  {
    SgExprStatement *is_expstmt = isSgExprStatement(s.get_ptr());
    if (is_expstmt != 0) {
      s = AstNodePtrImpl(is_expstmt->get_expression());
    }
  }
  if (IsVarRef(s, exptype)) {
    if (strip_exp != 0)
      *strip_exp = s;
    return true;
  }
  SgExpression *exp = isSgExpression(s.get_ptr());
  if (exp != 0) {
    switch (exp->variantT()) {
    case V_SgExpressionRoot:
      exp = isSgExpressionRoot(exp)->get_operand();
      break;
    default:
      break;
    }
    if (exptype != 0) {
      if (exp->get_type() != 0) {
        *exptype = AstNodeTypeImpl(exp->get_type());
      } else {
        *exptype = AstNodeType(AstNodeType::SpecialAstType::UNKNOWN_TYPE);
      }
    }
    if (strip_exp != 0)
      *strip_exp = AstNodePtrImpl(exp);
    return true;
  }
  return false;
}

// if yes, grab init, condition, increment, and body
bool AstInterface::IsLoop(const AstNodePtr &_s, AstNodePtr *init,
                          AstNodePtr *cond, AstNodePtr *incr,
                          AstNodePtr *body) {
  AstNodePtrImpl s(_s);
  if (s.get_ptr() == 0)
    return false;
  switch (s->variantT()) {
  case V_SgForStatement: {
    SgForStatement *f = isSgForStatement(s.get_ptr());
    if (init != 0) {
      SgForInitStatement *pinit = f->get_for_init_stmt();
      if (pinit != 0 && pinit->get_init_stmt().size() == 0)
        pinit = 0;
      *init = AstNodePtrImpl(pinit);
    }
    if (incr != 0)
      *incr = AstNodePtrImpl(f->get_increment());
    if (cond != 0)
      *cond = AstNodePtrImpl(f->get_test_expr());
    if (body != 0)
      *body = AstNodePtrImpl(f->get_loop_body());
  } break;
  case V_SgWhileStmt: {
    SgWhileStmt *w = isSgWhileStmt(s.get_ptr());
    if (init != 0)
      *init = AST_NULL;
    if (incr != 0)
      *incr = AST_NULL;
    if (cond != 0)
      *cond = AstNodePtrImpl(w->get_condition());
    if (body != 0)
      *body = AstNodePtrImpl(w->get_body());
  } break;
  case V_SgDoWhileStmt: {
    SgDoWhileStmt *w = isSgDoWhileStmt(s.get_ptr());
    if (init != 0)
      *init = AST_NULL;
    if (incr != 0)
      *incr = AST_NULL;
    if (cond != 0)
      *cond = AstNodePtrImpl(w->get_condition());
    if (body != 0)
      *body = AstNodePtrImpl(w->get_body());
  } break;

  case V_SgFortranDo:
    // FIXME: increment/bound in fortran are not equivalent to incr/cond in Cxx.
    {
      SgFortranDo *f = isSgFortranDo(s.get_ptr());
      if (init != 0)
        *init = AstNodePtrImpl(f->get_initialization());
      if (incr != 0)
        *incr = AstNodePtrImpl(f->get_increment());
      if (cond != 0)
        *cond = AstNodePtrImpl(f->get_bound());
      if (body != 0)
        *body = AstNodePtrImpl(f->get_body());
    }
    break;

  default:
    return false;
  }
  return true;
}

// The loop must be in the format: for (ivar=lb; ivar <= ub; ivar += step)
bool AstInterfaceImpl::IsFortranLoop(const SgNode *s, SgNode **ivar,
                                     SgNode **lb, SgNode **ub, SgNode **step,
                                     SgNode **body) {
  if (s == 0)
    return false;
  switch (s->variantT()) {
  case V_SgFortranDo: {
    const SgFortranDo *f = isSgFortranDo(s);
    SgExpression *init = f->get_initialization();
    SgNode *ivarast, *lbast;
    if (!IsAssignment(init, &ivarast, &lbast))
      ROSE_ABORT();

    if (ivar != 0)
      *ivar = ivarast;
    if (lb != 0)
      *lb = lbast;
    if (ub != 0)
      *ub = f->get_bound();
    if (step != 0)
      *step = f->get_increment();
    if (body != 0)
      *body = f->get_body();
  }
    return true;

  case V_SgForStatement: {
    const SgForStatement *fs = isSgForStatement(s);
    const SgStatementPtrList &init = fs->get_init_stmt();
    if (init.size() != 1)
      return false;

    SgNode *init1 = init.front();
    SgNode *ivarast, *lbast, *ubast, *stepast;

    if (!IsAssignment(init1, &ivarast, &lbast)) {
      SgVariableDeclaration *decl = isSgVariableDeclaration(init1);
      if (decl != 0) {
        SgInitializedNamePtrList &names = decl->get_variables();
        if (names.size() != 1)
          return false;
        SgInitializedName *var = names.front();
        ivarast = var;
        SgExpression *def = var->get_initializer();
        if (def != 0 && def->variantT() != V_SgAssignInitializer)
          return false;
        lbast = isSgAssignInitializer(def)->get_operand();
      } else
        return false;
    }
    string varname;
    if (!IsVarRef(ivarast, 0, &varname))
      return false;

    SgExpression *test = fs->get_test_expr();
    int t = test->variantT();
    switch (t) {
    case V_SgLessOrEqualOp:
    case V_SgGreaterOrEqualOp:
    case V_SgNotEqualOp:
      break;
    default:
      return false;
    }

    SgNode *testlhs = isSgBinaryOp(test)->get_lhs_operand();
    string testvarname;
    if (!IsVarRef(AstInterface::SkipCasting(testlhs), 0, &testvarname) ||
        varname != testvarname)
      return false;

    ubast = isSgBinaryOp(test)->get_rhs_operand();
    SgExpression *incr = fs->get_increment();
    switch (incr->variantT()) {
    case V_SgPlusAssignOp:
      break;
    default:
      return false;
    }

    SgNode *incrlhs = isSgBinaryOp(incr)->get_lhs_operand();
    string incrvarname;
    if (!IsVarRef(AstInterface::SkipCasting(incrlhs), 0, &incrvarname) ||
        varname != incrvarname)
      return false;
    stepast = isSgBinaryOp(incr)->get_rhs_operand();
    if (ivar != 0)
      *ivar = ivarast;
    if (lb != 0)
      *lb = lbast;
    if (ub != 0)
      *ub = ubast;
    if (step != 0)
      *step = stepast;
    if (body != 0)
      *body = fs->get_loop_body();
  }
    return true;

  default:
    return false;
  }
}

bool AstInterface::IsFortranLoop(const AstNodePtr &_s, AstNodePtr *ivar,
                                 AstNodePtr *lb, AstNodePtr *ub,
                                 AstNodePtr *step, AstNodePtr *body) {
  AstNodePtrImpl s(_s);
  if (s.get_ptr() == 0)
    return false;

  SgNode *local_ivar = 0;
  SgNode *local_lb = 0;
  SgNode *local_ub = 0;
  SgNode *local_step = 0;
  SgNode *local_body = 0;

  if (AstInterfaceImpl::IsFortranLoop(s.get_ptr(), &local_ivar, &local_lb,
                                      &local_ub, &local_step, &local_body)) {
    if (ivar != 0)
      *ivar = local_ivar;
    if (lb != 0)
      *lb = local_lb;
    if (ub != 0)
      *ub = local_ub;
    if (step != 0)
      *step = local_step;
    if (body != 0)
      *body = local_body;
    return true;
  }
  return false;
}

bool AstInterface::IsPostTestLoop(const AstNodePtr &_s) {
  AstNodePtrImpl s(_s);
  if (s.get_ptr() == 0)
    return false;
  switch (s->variantT()) {
  case V_SgDoWhileStmt:
    return true;
  default:
    break;
  }
  return false;
}

AstNodePtr
AstInterface::CreateLoop(const AstNodePtr &_ivar, const AstNodePtr &_lb,
                         const AstNodePtr &_ub, const AstNodePtr &_step,
                         const AstNodePtr &_stmts, bool decrementIvar) {
  // jichi(9/11/2009): Add in support for SgFortranDo.
  if (IS_FORTRAN_LANGUAGE()) { // Generate fortran loop.
    AstNodePtrImpl ivar(_ivar), lb(_lb), ub(_ub), step(_step), stmts(_stmts);
    assert(lb != 0);

    // Create new loop.
    SgFortranDo *result = new SgFortranDo(GetFileInfo());
    result->set_endOfConstruct(result->get_file_info());

    // Set loop expressions.
    SgExpression *ivarexp = ToExpression(*impl, ivar.get_ptr());
    SgExpression *lbexp = ToExpression(*impl, lb.get_ptr());
    SgExpression *ubexp = ToExpression(*impl, ub.get_ptr());
    SgExpression *stepexp = ToExpression(*impl, step.get_ptr());

    SgExpression *initexp =
        isSgExpression(::CreateAssignment(*impl, ivarexp, lbexp));
    assert(initexp != 0);
    ivarexp->set_parent(initexp);
    ubexp->set_parent(initexp);

    result->set_initialization(initexp);
    initexp->set_parent(result);

    result->set_bound(ubexp);
    ubexp->set_parent(result);

    result->set_increment(stepexp);
    stepexp->set_parent(result);

    // set loop body
    SgStatement *stmtptr = ToStatement(stmts.get_ptr());
    assert(stmtptr != 0);
    SgBasicBlock *b = isSgBasicBlock(stmtptr);

    if (b == 0) {
      NEW_BLOCK1(b, stmtptr);
    } else
      assert(HasNullParent(b));

    result->set_body(b);
    b->set_parent(result);

    result->set_has_end_statement(true);
    result->set_parent(GetNullScope());
    return AstNodePtrImpl(result);

  } else {
    AstNodePtrImpl ivar(_ivar), lb(_lb), ub(_ub), step(_step), stmts(_stmts);
    SgForStatement *result = new SgForStatement(GetFileInfo());
    result->set_endOfConstruct(result->get_file_info());
    SgExpression *ivarexp = ToExpression(*impl, ivar.get_ptr());
    SgExpression *lbexp = (lb == 0) ? 0 : ToExpression(*impl, lb.get_ptr());
    SgExpression *ubexp = ToExpression(*impl, ub.get_ptr());
    SgExpression *stepexp = ToExpression(*impl, step.get_ptr());
    SgNode *init = 0;
    if (lbexp != 0)
      init = ::CreateAssignment(*impl, ivarexp, lbexp);
    SgStatement *initstmt = isSgStatement(init);
    if (initstmt == 0 && init != 0) {
      SgExpression *initexp = ToExpression(*impl, init);
      NEW_EXPR_STMT(initstmt, initexp);
    }
    if (initstmt != 0) {
      result->append_init_stmt(initstmt);
      initstmt->set_parent(result->get_for_init_stmt());
    }
    SgExpression *ivarexp1 = isSgExpression(
        AstNodePtrImpl(CopyAstTree(AstNodePtrImpl(ivarexp))).get_ptr());
    if (decrementIvar) {
      assert(HasNullParent(ubexp));

      SgExpression *testExp = new SgGreaterOrEqualOp(
          GetFileInfo(), ivarexp1, ubexp, logicalOperatorResultType());
      ivarexp1->set_parent(testExp);
      ubexp->set_parent(testExp);
      SgExprStatement *NEW_EXPR_STMT(test, testExp);
      result->set_test(test);
      test->set_parent(result);

    } else {
      assert(HasNullParent(ubexp));

      SgExpression *NEW_BIN_OP(testExp, SgLessOrEqualOp, ivarexp1, ubexp,
                               logicalOperatorResultType());
      SgExprStatement *NEW_EXPR_STMT(test, testExp);
      result->set_test(test);
      test->set_parent(result);
    }

    SgExpression *ivarexp2 = isSgExpression(
        AstNodePtrImpl(CopyAstTree(AstNodePtrImpl(ivarexp))).get_ptr());
    assert(HasNullParent(stepexp));
    SgPlusAssignOp *NEW_BIN_OP(incr, SgPlusAssignOp, ivarexp2, stepexp,
                               ivarexp2->get_type());
    result->set_increment(incr);
    incr->set_parent(result);
    SgStatement *stmtptr = ToStatement(stmts.get_ptr());
    assert(stmtptr != 0);
    SgBasicBlock *b = isSgBasicBlock(stmtptr);

    if (b == 0) {
      NEW_BLOCK1(b, stmtptr);
    }
    result->set_loop_body(b);
    b->set_parent(result);
    result->set_parent(GetNullScope());
    return AstNodePtrImpl(result);
  }
}

AstInterface::AstNodeList AstInterface::GetBlockStmtList(const AstNodePtr &_n) {
  AstNodePtrImpl n(_n);
  AstNodeList result;
  SgStatementPtrList l;
  switch (n->variantT()) {
  case V_SgBasicBlock:
    l = isSgBasicBlock(n.get_ptr())->get_statements();
    break;
  case V_SgForInitStatement:
    l = isSgForInitStatement(n.get_ptr())->get_init_stmt();
    break;
  case V_SgSwitchStatement:
    result.push_back(isSgSwitchStatement(n.get_ptr())->get_body());
    return result;
  case V_SgGlobal: {
    SgDeclarationStatementPtrList &l1 =
        isSgGlobal(n.get_ptr())->get_declarations();
    for (SgDeclarationStatementPtrList::iterator p = l1.begin(); p != l1.end();
         ++p) {
      result.push_back(*p);
    }
    return result;
  }
  default:
    ROSE_ABORT();
  }
  for (SgStatementPtrList::iterator p = l.begin(); p != l.end(); ++p) {
    result.push_back(*p);
  }
  return result;
}

int AstInterface::GetBlockSize(const AstNodePtr &_n) {
  AstNodePtrImpl n(_n);
  SgStatementPtrList l;
  switch (n->variantT()) {
  case V_SgBasicBlock:
    l = isSgBasicBlock(n.get_ptr())->get_statements();
    break;
  case V_SgForInitStatement:
    l = isSgForInitStatement(n.get_ptr())->get_init_stmt();
    break;
  case V_SgSwitchStatement:
    return 1;
  default:
    ROSE_ABORT();
  }
  return l.size();
}

AstNodePtr AstInterface::GetBlockFirstStmt(const AstNodePtr &_n) {
  AstNodePtrImpl n(_n);
  SgStatementPtrList l;
  switch (n->variantT()) {
  case V_SgBasicBlock:
    l = isSgBasicBlock(n.get_ptr())->get_statements();
    break;
  case V_SgForInitStatement:
    l = isSgForInitStatement(n.get_ptr())->get_init_stmt();
    break;
  case V_SgSwitchStatement:
    return AstNodePtrImpl(isSgSwitchStatement(n.get_ptr())->get_body());
  default:
    ROSE_ABORT();
  }
  return (l.size() == 0) ? AST_NULL : AstNodePtrImpl(l.front());
}

AstNodePtr AstInterface::GetBlockLastStmt(const AstNodePtr &_n) {
  AstNodePtrImpl n(_n);
  SgStatementPtrList l;
  switch (n->variantT()) {
  case V_SgBasicBlock:
    l = isSgBasicBlock(n.get_ptr())->get_statements();
    break;
  case V_SgForInitStatement:
    l = isSgForInitStatement(n.get_ptr())->get_init_stmt();
    break;
  case V_SgSwitchStatement:
    return AstNodePtrImpl(isSgSwitchStatement(n.get_ptr())->get_body());
  default:
    ROSE_ABORT();
  }
  if (l.size() > 0)
    return AstNodePtrImpl(l.back());
  return AST_NULL;
}

AstNodePtr AstInterface::CreateConstInt(int val) {
  SgIntVal *res = new SgIntVal(GetFileInfo(), val);
  res->set_endOfConstruct(res->get_file_info());
  return AstNodePtrImpl(res);
}

AstNodePtr AstInterface::CreateConstant(const string &valtype,
                                        const string &val) {
  if (valtype == "int") {
    int intval = atoi(val.c_str());
    return CreateConstInt(intval);
  } else if (valtype == "bool") {
    int i = atoi(val.c_str());
    if (val == "true" || i != 0)
      return AstNodePtrImpl(new SgBoolValExp(GetFileInfo(), -1));
    else
      return AstNodePtrImpl(new SgBoolValExp(GetFileInfo(), 0));
  } else if (valtype == "string") {
    char *r = new char[val.size() + 1];
    strcpy(r, val.c_str());
    SgStringVal *tmp = new SgStringVal(GetFileInfo(), r);
    assert(tmp != NULL);
    return AstNodePtrImpl(tmp);
  } else if (valtype == "char") {
    return AstNodePtrImpl(new SgCharVal(GetFileInfo(), val[0]));
  } else if (valtype == "float") {
    istringstream in(val);
    float num = 0;
    in >> num;
    return AstNodePtrImpl(new SgFloatVal(GetFileInfo(), num));
  } else if (valtype == "double") {
    istringstream in(val);
    double num = 0;
    in >> num;
    return AstNodePtrImpl(new SgDoubleVal(GetFileInfo(), num));
  } else if (valtype == "function") {
    SgFunctionSymbol *fsym = impl->GetFunc(val);
    if (fsym == 0) {
      return AST_NULL;
    }
    SgFunctionRefExp *NEW_FUNCTION_REF(fr, fsym);
    return AstNodePtrImpl(fr);
  } else {
    cerr << "Error: non-recognized value type for creating constant AST: "
         << valtype << endl;
    ROSE_ABORT();
    abort();
  }
}

SgFunctionSymbol *CreateMinMaxFunction(AstInterfaceImpl *impl,
                                       const std::string &name, int numOfPars,
                                       bool isMin) {
  std::cerr << "MinMax create \n";
  SgType *typeint = AstInterfaceImpl::GetTypeInt();
  std::list<SgInitializedName *> pars;
  for (int i = 0; i < numOfPars; ++i) {
    std::string parname = "a";
    parname.push_back(i + '0');
    SgName curname(parname.c_str());
    SgInitializedName *curVar =
        new SgInitializedName(GetFileInfo(), curname, typeint);
    pars.push_back(curVar);
  }
  SgFunctionSymbol *funcSymbol = impl->NewFunc(name, typeint, pars);
  if (IS_FORTRAN_LANGUAGE())
    return funcSymbol;

  SgFunctionDeclaration *funcDecl = funcSymbol->get_declaration();
  funcDecl->set_requiresNameQualificationOnReturnType(false);
  funcDecl->set_definingDeclaration(funcDecl);
  SgBasicBlock *NEW_BLOCK(funcBody);
  SgFunctionDefinition *funcDefn =
      new SgFunctionDefinition(GetFileInfo(), funcBody);
  funcBody->set_parent(funcDefn);
  funcDefn->set_parent(funcDecl);

  std::list<SgVariableSymbol *> parSymbols;
  for (std::list<SgInitializedName *>::const_iterator iterPars = pars.begin();
       iterPars != pars.end(); ++iterPars) {
    SgInitializedName *curPar = *iterPars;
    SgVariableSymbol *NEW_SYMBOL(curSymbol, SgVariableSymbol, funcDefn, curPar);
    parSymbols.push_back(curSymbol);
  }
  funcDecl->set_definition(funcDefn);
  funcDecl->set_endOfConstruct(funcDecl->get_file_info());
  if (numOfPars == 2) {
    SgVarRefExp *v1 = new SgVarRefExp(GetFileInfo(), parSymbols.front());
    SgVarRefExp *v2 = new SgVarRefExp(GetFileInfo(), parSymbols.back());
    SgExpression *cond = 0;
    if (isMin)
      cond =
          new SgLessThanOp(GetFileInfo(), v1, v2, logicalOperatorResultType());
    else
      cond = new SgGreaterThanOp(GetFileInfo(), v1, v2,
                                 logicalOperatorResultType());
    v1->set_parent(cond);
    v2->set_parent(cond);
    v1 = new SgVarRefExp(GetFileInfo(), parSymbols.front());
    v2 = new SgVarRefExp(GetFileInfo(), parSymbols.back());
    SgExpression *returnExp =
        new SgConditionalExp(GetFileInfo(), cond, v1, v2, typeint);
    SgConditionalExp *conditional = isSgConditionalExp(returnExp);
    ROSE_ASSERT(conditional != nullptr);
    conditional->set_operator_kind(
        SgConditionalExp::e_conditional_operator_standard);
    cond->set_parent(returnExp);
    v1->set_parent(returnExp);
    v2->set_parent(returnExp);
    conditional->validate();
    SgStatement *returnStmt = new SgReturnStmt(GetFileInfo(), returnExp);
    funcBody->append_statement(returnStmt);
    returnStmt->set_parent(funcBody);
  } else {
    SgName resName("res");
    SgInitializedName *resVar =
        new SgInitializedName(GetFileInfo(), resName, typeint);
    SgVariableSymbol *NEW_SYMBOL(resSymbol, SgVariableSymbol, funcBody, resVar);
    std::list<SgVariableSymbol *>::const_iterator iterParSymbols =
        parSymbols.begin();
    SgVarRefExp *parRef = new SgVarRefExp(GetFileInfo(), *iterParSymbols);
    ++iterParSymbols;
    SgAssignInitializer *NEW_VAR_INIT(resInit, resVar, parRef);
    SgVariableDeclaration *resDecl = new SgVariableDeclaration(GetFileInfo());
    resDecl->append_variable(resVar, resInit);
    resVar->set_parent(resDecl);
    resDecl->set_endOfConstruct(resDecl->get_file_info());
    funcBody->append_statement(resDecl);

    for (int i = 1; i < numOfPars; ++i) {
      SgVarRefExp *resRef = new SgVarRefExp(GetFileInfo(), resSymbol);
      parRef = new SgVarRefExp(GetFileInfo(), *iterParSymbols);
      SgExpression *cond = 0;
      if (isMin)
        cond = new SgLessThanOp(GetFileInfo(), parRef, resRef,
                                logicalOperatorResultType());
      else
        cond = new SgGreaterThanOp(GetFileInfo(), parRef, resRef,
                                   logicalOperatorResultType());
      resRef->set_parent(cond);
      parRef->set_parent(cond);
      SgStatement *NEW_EXPR_STMT(condStmt, cond);
      resRef = new SgVarRefExp(GetFileInfo(), resSymbol);
      parRef = new SgVarRefExp(GetFileInfo(), *iterParSymbols);
      ++iterParSymbols;
      SgExpression *assignExp =
          new SgAssignOp(GetFileInfo(), resRef, parRef, resRef->get_type());
      resRef->set_parent(assignExp);
      parRef->set_parent(assignExp);
      SgStatement *NEW_EXPR_STMT(assignStmt, assignExp);
      SgBasicBlock *NEW_BLOCK1(assignBlock, assignStmt);
      SgIfStmt *NEW_IF(ifStmt, condStmt, assignBlock);
      funcBody->append_statement(ifStmt);
    }
    SgVarRefExp *resRef = new SgVarRefExp(GetFileInfo(), resSymbol);
    SgStatement *returnStmt = new SgReturnStmt(GetFileInfo(), resRef);
    resRef->set_parent(returnStmt);
    funcBody->append_statement(returnStmt);
  }
  return funcSymbol;
}

SgNode *AstInterfaceImpl ::CreateFunction(string name, int numOfPars) {
  bool isMin = (name == "min");
  bool isMax = (name == "max");
  if ((isMin || isMax) && !IS_FORTRAN_LANGUAGE())
    name.push_back((char)numOfPars + '0');
  SgFunctionSymbol *funcSymbol = GetFunc(name);
  if (funcSymbol == 0) {
    if (isMin || isMax) {
      funcSymbol = CreateMinMaxFunction(this, name, numOfPars, isMin);
    } else {
      std::cerr << "Unknown function: " << name << "\n";
      ROSE_ABORT();
    }
  }
  SgFunctionRefExp *NEW_FUNCTION_REF(result, funcSymbol);
  return result;
}

AstNodePtr AstInterface::CreateUnaryOP(OperatorEnum op, const AstNodePtr &_a0) {
  AstNodePtrImpl a0(_a0);
  assert(HasNullParent(a0.get_ptr()));
  SgExpression *e = ToExpression(*impl, a0.get_ptr());
  auto require_exact_operand_type = [&]() -> SgType * {
    SgType *type = e->get_type();
    if (type == nullptr || isSgTypeUnknown(type) != nullptr ||
        isSgTypeDefault(type) != nullptr) {
      std::cerr << "REX_AST_INVARIANT[unary-result-type-producer]: operator="
                << AstInterface::toString(op) << " has no exact operand type"
                << std::endl;
      ROSE_ABORT();
    }
    return type;
  };
  SgNode *result = 0;
  switch (op) {
  case UOP_ADDR:
    result = new SgAddressOfOp(GetFileInfo(), e, e->get_type()->get_ptr_to());
    break;
  case UOP_MINUS:
    result = new SgMinusOp(GetFileInfo(), e, e->get_type());
    break;
  case UOP_NOT:
    result = new SgNotOp(GetFileInfo(), e, logicalOperatorResultType());
    break;
  case UOP_BIT_COMPLEMENT:
    result =
        new SgBitComplementOp(GetFileInfo(), e, require_exact_operand_type());
    break;
  case UOP_INCR1:
  case UOP_INCR1_POST: {
    SgPlusPlusOp *increment =
        new SgPlusPlusOp(GetFileInfo(), e, require_exact_operand_type());
    increment->set_mode(op == UOP_INCR1 ? SgUnaryOp::Sgop_mode::prefix
                                        : SgUnaryOp::Sgop_mode::postfix);
    result = increment;
    break;
  }
  case UOP_DECR1:
  case UOP_DECR1_POST: {
    SgMinusMinusOp *decrement =
        new SgMinusMinusOp(GetFileInfo(), e, require_exact_operand_type());
    decrement->set_mode(op == UOP_DECR1 ? SgUnaryOp::Sgop_mode::prefix
                                        : SgUnaryOp::Sgop_mode::postfix);
    result = decrement;
    break;
  }
  case UOP_DEREF:
    result = new SgPointerDerefExp(
        GetFileInfo(), e,
        requireElementResultType(e->get_type(), "AstInterface::CreateUnaryOP"));
    break;
  case UOP_CAST_C:
  case UOP_CAST_CONST:
  case UOP_CAST_STATIC:
  case UOP_CAST_DYNAMIC:
  case UOP_CAST_REINTERP:
  case UOP_CAST_BUILTIN_BIT:
  case UOP_CAST_FUNCTIONAL:
  case UOP_CAST_FUNCTIONAL_LIST:
  case UOP_SEMANTIC_CONVERSION: {
    std::cerr << "REX_AST_INVARIANT[unary-cast-rebuild]: AstInterface unary "
                 "operators do not carry an exact cast target type, semantic "
                 "conversion kind, or value category"
              << std::endl;
    ROSE_ABORT();
  }
  default:
    std::cerr << "unexpected uop:" << op << "\n";
    ROSE_ABORT();
  }
  e->set_parent(result);
  return AstNodePtrImpl(result);
}

AstNodePtr AstInterface::CreateBinaryOP(OperatorEnum op, const AstNodePtr &_a0,
                                        const AstNodePtr &_a1) {
  SgNode *a0 = AstNodePtrImpl(_a0).get_ptr();
  SgNode *a1 = AstNodePtrImpl(_a1).get_ptr();
  // assert( HasNullParent(a1) && HasNullParent(a0));
  SgExpression *e0 = ToExpression(*impl, a0);
  SgExpression *e1 = ToExpression(*impl, a1);
  assert(e0 != 0 && e1 != 0);
  auto require_exact_type = [](SgType *type, const char *producer) -> SgType * {
    if (type == nullptr || isSgTypeUnknown(type) != nullptr ||
        isSgTypeDefault(type) != nullptr) {
      std::cerr << "REX_AST_INVARIANT[binary-result-type-producer]: "
                << producer << " has no exact semantic result type"
                << std::endl;
      ROSE_ABORT();
    }
    return type;
  };
  auto require_matching_arithmetic_type =
      [&](const char *producer) -> SgType * {
    SgType *lhs_type = require_exact_type(e0->get_type(), producer);
    SgType *rhs_type = require_exact_type(e1->get_type(), producer);
    if (lhs_type->stripTypedefsAndModifiers() !=
        rhs_type->stripTypedefsAndModifiers()) {
      std::cerr << "REX_AST_INVARIANT[binary-result-type-producer]: "
                << producer
                << " requires an explicit result type after arithmetic "
                   "conversions"
                << std::endl;
      ROSE_ABORT();
    }
    return lhs_type;
  };
  auto require_matching_integral_type = [&](const char *producer) -> SgType * {
    SgType *result_type = require_matching_arithmetic_type(producer);
    if (!result_type->stripTypedefsAndModifiers()->isIntegerType()) {
      std::cerr << "REX_AST_INVARIANT[binary-result-type-producer]: "
                << producer << " requires an exact integral result type"
                << std::endl;
      ROSE_ABORT();
    }
    return result_type;
  };
  auto require_additive_type = [&](bool subtraction) -> SgType * {
    SgType *lhs_type = require_exact_type(
        e0->get_type(), "AstInterface::CreateBinaryOP additive operator");
    SgType *rhs_type = require_exact_type(
        e1->get_type(), "AstInterface::CreateBinaryOP additive operator");
    const bool lhs_pointer =
        isSgPointerType(lhs_type->stripTypedefsAndModifiers()) != nullptr;
    const bool rhs_pointer =
        isSgPointerType(rhs_type->stripTypedefsAndModifiers()) != nullptr;
    if (lhs_pointer && rhs_pointer) {
      std::cerr << "REX_AST_INVARIANT[binary-result-type-producer]: "
                   "AstInterface pointer subtraction requires the target's "
                   "explicit ptrdiff result type"
                << std::endl;
      ROSE_ABORT();
    }
    if (lhs_pointer) {
      return lhs_type;
    }
    if (!subtraction && rhs_pointer) {
      return rhs_type;
    }
    return require_matching_arithmetic_type(
        "AstInterface::CreateBinaryOP additive operator");
  };
  SgBinaryOp *n = 0;
  switch (op) {
  case BOP_DOT_ACCESS:
    n = new SgDotExp(GetFileInfo(), e0, e1, e1->get_type());
    break;
  case BOP_ARROW_ACCESS:
    n = new SgArrowExp(GetFileInfo(), e0, e1, e1->get_type());
    break;
  case BOP_DIVIDE:
    n = new SgDivideOp(
        GetFileInfo(), e0, e1,
        require_matching_arithmetic_type("AstInterface::CreateBinaryOP /"));
    break;
  case BOP_TIMES:
    n = new SgMultiplyOp(
        GetFileInfo(), e0, e1,
        require_matching_arithmetic_type("AstInterface::CreateBinaryOP *"));
    break;
  case BOP_MOD:
    n = new SgModOp(
        GetFileInfo(), e0, e1,
        require_matching_integral_type("AstInterface::CreateBinaryOP %"));
    break;
  case BOP_PLUS:
    n = new SgAddOp(GetFileInfo(), e0, e1, require_additive_type(false));
    break;
  case BOP_MINUS:
    n = new SgSubtractOp(GetFileInfo(), e0, e1, require_additive_type(true));
    break;
  case BOP_EQ:
    n = new SgEqualityOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_NE:
    n = new SgNotEqualOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_LT:
    n = new SgLessThanOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_GT:
    n = new SgGreaterThanOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_LE:
    n = new SgLessOrEqualOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_GE:
    n = new SgGreaterOrEqualOp(GetFileInfo(), e0, e1,
                               logicalOperatorResultType());
    break;
  case BOP_AND:
    n = new SgAndOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_OR:
    n = new SgOrOp(GetFileInfo(), e0, e1, logicalOperatorResultType());
    break;
  case BOP_BIT_AND:
    n = new SgBitAndOp(
        GetFileInfo(), e0, e1,
        require_matching_integral_type("AstInterface::CreateBinaryOP &"));
    break;
  case BOP_BIT_OR:
    n = new SgBitOrOp(
        GetFileInfo(), e0, e1,
        require_matching_integral_type("AstInterface::CreateBinaryOP |"));
    break;
  case BOP_BIT_RSHIFT:
    n = new SgRshiftOp(
        GetFileInfo(), e0, e1,
        require_exact_type(e0->get_type(), "AstInterface::CreateBinaryOP >>"));
    break;
  case BOP_BIT_LSHIFT:
    n = new SgLshiftOp(
        GetFileInfo(), e0, e1,
        require_exact_type(e0->get_type(), "AstInterface::CreateBinaryOP <<"));
    break;
  default:
    cerr << "Error: non-recognized binary operator: \n";
    ROSE_ABORT();
  }
  e0->set_parent(n);
  e1->set_parent(n);
  n->set_endOfConstruct(n->get_file_info());
  return AstNodePtrImpl(n);
}

AstNodePtr AstInterface::CreateArrayAccess(const AstNodePtr &arr,
                                           const AstNodePtr &index) {
  SgExpression *r = isSgExpression(AstNodePtrImpl(arr).get_ptr());
  SgExpression *e2 = isSgExpression((SgNode *)index.get_ptr());
  assert(r);
  if (IS_FORTRAN_LANGUAGE()) {
    AstNodePtr arr_ref;
    AstList arr_index;
    arr_index.push_back(e2); // QY: prepend the new dimension now

    // If arr is array access, append offsets to its first dimension.
    if (IsArrayAccess(arr, &arr_ref, &arr_index)) {
      return CreateArrayAccess(
          arr_ref,
          AstNodeList2ExpressionList(arr_index.begin(), arr_index.end()));
    }

    SgType *element_type = SageInterface::getArrayElementType(r->get_type());
    if (element_type == nullptr || isSgTypeUnknown(element_type) != nullptr ||
        isSgTypeDefault(element_type) != nullptr) {
      std::cerr << "REX_AST_INVARIANT[binary-result-type-producer]: Fortran "
                   "array access has no exact element result type"
                << std::endl;
      ROSE_ABORT();
    }
    SgExpression *aref =
        new SgPntrArrRefExp(GetFileInfo(), r, e2, element_type);
    assert(aref);
    aref->set_endOfConstruct(aref->get_file_info());

    r->set_parent(aref);
    e2->set_parent(aref);
    return AstNodePtrImpl(aref);

  } else {
    assert(e2 != 0);
    SgExpression *r1 = new SgPntrArrRefExp(
        GetFileInfo(), r, e2,
        requireElementResultType(r->get_type(),
                                 "AstInterface::CreateArrayAccess"));
    r1->set_endOfConstruct(r1->get_file_info());
    r->set_parent(r1);
    e2->set_parent(r1);
    r = r1;
    return AstNodePtrImpl(r);
  }
}

AstNodePtr GetOverloadOperatorOpd1(const AstNodePtr &_exp) {
  AstNodePtrImpl exp(_exp);
  SgFunctionCallExp *fs = isSgFunctionCallExp(exp.get_ptr());
  assert(fs != 0);
  SgExpression *func = fs->get_function();
  if (func->variantT() == V_SgDotExp) {
    return AstNodePtrImpl(isSgDotExp(func)->get_lhs_operand());
  }
  SgExpressionPtrList &l = fs->get_args()->get_expressions();
  return AstNodePtrImpl(l.front());
}

AstNodePtr GetOverloadOperatorOpd2(const AstNodePtr &_exp) {
  AstNodePtrImpl exp(_exp);
  SgFunctionCallExp *fs = isSgFunctionCallExp(exp.get_ptr());
  assert(fs != 0);
  SgExpressionPtrList &l = fs->get_args()->get_expressions();
  return AstNodePtrImpl(l.back());
}

AstNodePtr AstInterface::CreateFunctionCall(const AstNodePtr &func,
                                            AstList::const_iterator b,
                                            AstList::const_iterator e) {
  return AstNodePtrImpl(
      impl->CreateFunctionCall(AstNodePtrImpl(func).get_ptr(), b, e));
}

AstNodePtr AstInterface::CreateFunctionCall(const string &fname,
                                            AstList::const_iterator b,
                                            AstList::const_iterator e) {
  unsigned num = 0;
  for (AstList::const_iterator p = b; p != e; ++p)
    ++num;
  SgNode *f = impl->CreateFunction(fname, num);
  return AstNodePtrImpl(impl->CreateFunctionCall(f, b, e));
}

SgNode *AstInterfaceImpl::CreateFunctionCall(SgNode *func,
                                             AstNodeList::const_iterator b,
                                             AstNodeList::const_iterator e) {
  assert(
      HasNullParent(func)); // which implies func is not in the global AST now.
  SgExpression *fr = isSgExpression(func);

  AstNodeList::const_iterator p = b;
  if (fr->variantT() == V_SgMemberFunctionRefExp) {
    SgExpression *obj = isSgExpression(AstNodePtrImpl(*p).get_ptr());
    assert(obj != 0 && HasNullParent(obj));
    ++p;
    SgExpression *fr1 = 0;
    if (obj->get_type()->variantT() == V_SgPointerType) {
      NEW_BIN_OP(fr1, SgArrowExp, obj, fr, fr->get_type());
    } else {
      NEW_BIN_OP(fr1, SgDotExp, obj, fr, fr->get_type());
    }
    obj->set_parent(fr1);
    fr->set_parent(fr1);
    fr = fr1;
  }
  SgExprListExp *argexp = AstNodeList2ExpressionList(b, e);
  SgFunctionCallExp *result = nullptr;
  NEW_FUNCTION_CALL(result, fr, argexp);
  return result;
}

AstNodePtr AstInterface::CreateReadStatement(const AstNodeList &l) const {
  SgExprListExp *explist = AstNodeList2ExpressionList(l.begin(), l.end());
  SgReadStatement *ret = new SgReadStatement(GetFileInfo());
  assert(ret);
  ret->set_endOfConstruct(ret->get_file_info());

  ret->set_io_stmt_list(explist);
  explist->set_parent(ret);

  SgExpression *f = new SgAsteriskShapeExp(GetFileInfo());
  f->set_endOfConstruct(f->get_file_info());
  ret->set_format(f);
  f->set_parent(ret);

  return AstNodePtrImpl(ret);
}

AstNodePtr AstInterface::CreateNullStatement() const {
  SgStatement *s = new SgNullStatement(GetFileInfo());
  s->set_endOfConstruct(s->get_file_info());
  return AstNodePtrImpl(s);
}

AstNodePtr AstInterface::CreateWriteStatement(const AstNodeList &__l) const {
  assert(!__l.empty());

  SgExprListExp *NEW_EXPR_LIST(explist);
  for (AstNodeList::const_iterator p = __l.begin(); p != __l.end(); ++p) {
    SgExpression *e = isSgExpression(AstNodePtrImpl(*p).get_ptr());
    assert(e);
    explist->append_expression(e);
    e->set_parent(explist);
  }

  SgWriteStatement *ret = new SgWriteStatement(GetFileInfo());
  assert(ret);
  ret->set_endOfConstruct(ret->get_file_info());

  ret->set_io_stmt_list(explist);
  explist->set_parent(ret);

  SgExpression *f = new SgAsteriskShapeExp(GetFileInfo());
  f->set_endOfConstruct(f->get_file_info());
  ret->set_format(f);
  f->set_parent(ret);

  return AstNodePtrImpl(ret);
}

AstNodePtr AstInterface::CreatePrintStatement(const AstNodeList &l) const {
  SgExprListExp *explist = AstNodeList2ExpressionList(l.begin(), l.end());
  SgPrintStatement *ret = new SgPrintStatement(GetFileInfo());
  assert(ret);
  ret->set_endOfConstruct(ret->get_file_info());

  ret->set_io_stmt_list(explist);
  explist->set_parent(ret);

  SgExpression *f = new SgAsteriskShapeExp(GetFileInfo());
  f->set_endOfConstruct(f->get_file_info());
  ret->set_format(f);
  f->set_parent(ret);

  return AstNodePtrImpl(ret);
}

AstNodePtr AstInterface::CreateIf(const AstNodePtr &__cond,
                                  const AstNodePtr &__istmt,
                                  const AstNodePtr &__estmt) const {
  AstNodePtrImpl cond(__cond), istmt(__istmt);
  assert(HasNullParent(cond.get_ptr()) && HasNullParent(istmt.get_ptr()));

  SgStatement *c = ToStatement(cond.get_ptr());
  SgBasicBlock *ib = isSgBasicBlock(istmt.get_ptr()); // if-block
  if (!ib) {
    SgStatement *p = ToStatement(istmt.get_ptr());
    NEW_BLOCK1(ib, p);
  }

  SgIfStmt *ret;
  if (__estmt == AST_NULL) { // no else-statement
    NEW_IF(ret, c, ib);

  } else {
    AstNodePtrImpl estmt(__estmt);
    assert(HasNullParent(estmt.get_ptr()));

    SgBasicBlock *eb = isSgBasicBlock(estmt.get_ptr()); // else-block
    if (!eb) {
      SgStatement *p = ToStatement(estmt.get_ptr());
      NEW_BLOCK1(eb, p);
    }

    NEW_IF_ELSE(ret, c, ib, eb);
  }

  return AstNodePtrImpl(ret);
}

AstNodePtr AstInterface::CreateBlock(const AstNodePtr &_orig) {
  AstNodePtrImpl orig(_orig);
  SgBasicBlock *NEW_BLOCK(r);
  r->set_parent(GetNullScope());
  if (orig != 0) {
    SgBasicBlock *r1 = isSgBasicBlock(orig.get_ptr());
    if (r1 != 0) {
      r->getAttachedPreprocessingInfo() = r1->getAttachedPreprocessingInfo();
    }
  }
  return AstNodePtrImpl(r);
}

void BlockPrependAppendStmt(AstInterfaceImpl *impl, AstNodePtr &_b,
                            const AstNodePtr &_s, bool isAppend,
                            bool flatten = true) {
  AstNodePtrImpl b(_b), s(_s);
  SgStatement *stmt = ToStatement(s.get_ptr());
  assert(stmt != 0);

  SgBasicBlock *sb = isSgBasicBlock(stmt);
  SgBasicBlock *basicBlock = isSgBasicBlock(b.get_ptr());
  assert(basicBlock != 0);

  stmt->set_parent(basicBlock);

  if (sb == 0) {
    if (!isAppend)
      basicBlock->prepend_statement(stmt);
    else
      basicBlock->append_statement(stmt);
  } else {
    SgStatementPtrList l = sb->get_statements();
    bool hasdecl = false;
    if (!flatten) {
      for (SgStatementPtrList::iterator p = l.begin(); p != l.end(); ++p) {
        if (isSgDeclarationStatement(*p) != 0) {
          hasdecl = true;
          break;
        }
      }
    }
    if (hasdecl) {
      if (!isAppend)
        basicBlock->prepend_statement(sb);
      else
        basicBlock->append_statement(sb);
    } else if (isAppend) {
      for (SgStatementPtrList::iterator p = l.begin(); p != l.end(); ++p) {
        SgStatement *cur = *p;
        basicBlock->append_statement(cur);
        cur->set_parent(basicBlock);
      }
    } else {
      for (SgStatementPtrList::reverse_iterator p = l.rbegin(); p != l.rend();
           ++p) {
        SgStatement *cur = *p;
        basicBlock->prepend_statement(cur);
        cur->set_parent(basicBlock);
      }
    }
  }
}

void AstInterface::BlockAppendStmt(AstNodePtr &_b, const AstNodePtr &_s,
                                   bool flatten) {
  BlockPrependAppendStmt(impl, _b, _s, true, flatten);
}

void AstInterface::BlockPrependStmt(AstNodePtr &_b, const AstNodePtr &_s) {
  BlockPrependAppendStmt(impl, _b, _s, false);
}

void AstInterface::InsertStmt(AstNodePtr const &_orig, AstNodePtr const &_n,
                              bool insertbefore, bool extractfromBlock) {
  AstNodePtrImpl n(_n), orig(_orig);
  assert(HasNullParent(n.get_ptr()));
  SgStatement *s = isSgStatement(orig.get_ptr()),
              *ns = ToStatement(n.get_ptr());
  assert(s != 0);
  SgStatement *p = isSgStatement(s->get_parent());
  assert(p != 0);
  SgBasicBlock *nb = isSgBasicBlock(ns);
  if (extractfromBlock && nb != 0) {
    p->insert_statement_from_basicBlock(s, nb, insertbefore);
    SgStatementPtrList l = nb->get_statements();
    for (SgStatementPtrList::iterator pn = l.begin(); pn != l.end(); ++pn) {
      SgStatement *cur = *pn;
      cur->set_parent(p);
    }
  } else {
    p->insert_statement(s, ns, insertbefore);
    ns->set_parent(p);
  }
}

void AstInterface::InsertAnnot(AstNodePtr const &_n, const std::string &annot,
                               bool insertbefore) {
  SgNode *n = AstNodePtrImpl(_n).get_ptr();
  SgLocatedNode *loc = isSgLocatedNode(n);
  assert(loc != 0);
  {
    Sg_File_Info *nf = loc->get_file_info();
    if (nf == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[ast-interface-annotation-owner]: target "
              "has no exact source information\n");
      ROSE_ABORT();
    }

    // DQ (7/19/2008): Modified interface to PreprocessingInfo
    // Note that this function could directly call
    // SageInterface::attachComment(SgLocatedNode*,std::string);
    PreprocessingInfo *info = new PreprocessingInfo(
        PreprocessingInfo::C_StyleComment, annot, nf->get_filename(),
        nf->get_line(), nf->get_col(), 1,
        (insertbefore) ? (PreprocessingInfo::before)
                       : (PreprocessingInfo::after));
    ROSE_ASSERT(info->get_file_info() != NULL);
    info->get_file_info()->set_physical_file_id(Sg_File_Info::NULL_FILE_ID);
    SageInterface::publishGeneratedPreprocessingInfo(info, loc);
    loc->addToAttachedPreprocessingInfo(info);
  }
}

bool AstInterface::RemoveStmt(const AstNodePtr &_n) {
  AstNodePtrImpl n(_n);
  SgStatement *s = isSgStatement(n.get_ptr());
  assert(s != 0);
  SgStatement *p = isSgStatement(n->get_parent());
  assert(p != 0);
  p->remove_statement(s);
  // s->set_parent(GetNullScope()); /*QY: not reseting parent due to dangling
  // pointers from symbols etc. */
  return true;
}

bool AstInterfaceImpl::ReplaceAst(SgNode *orig, SgNode *n) {
  /*
      if (!HasNullParent(n)) {
           std::cerr << "SgNode does not have null parent: " <<
     n->unparseToString() << "\n"; ROSE_ABORT();
      }
  */
  SgNode *p = orig->get_parent();
  if (p == 0)
    return false;
  n->set_parent(p);

  SgStatement *stmtOrig = isSgStatement(orig);
  SgStatement *stmtParent = isSgStatement(p);
  if (stmtOrig != 0) {
    SgStatement *stmtNew = isSgStatement(n);
    assert(stmtParent != 0 && stmtNew != 0);
    stmtParent->replace_statement(stmtOrig, stmtNew);
    stmtNew->set_parent(stmtParent);
  } else {
    SgExpression *expOrig = isSgExpression(orig);
    SgExpression *expNew = isSgExpression(n);
    assert(expOrig != 0 && expNew != 0);
    if (stmtParent != 0) {
      stmtParent->replace_expression(expOrig, expNew);
      expNew->set_parent(stmtParent);
    } else {
      SgExpression *expParent = isSgExpression(p);
      if (expParent != 0) {
        expParent->replace_expression(expOrig, expNew);
        expNew->set_parent(expParent);
      } else {
        SgInitializedName *nameParent = isSgInitializedName(p);
        assert(nameParent != 0);
        SgAssignInitializer *NEW_VAR_INIT(init, nameParent, expNew);
      }
    }
  }
  // orig->set_parent(GetNullScope()); /*QY: not reseting parent due to dangling
  // pointers from symbols etc. */
  return true;
}
bool AstInterface::ReplaceAst(const AstNodePtr &_orig, const AstNodePtr &_n) {
  SgNode *orig = AstNodePtrImpl(_orig).get_ptr();
  SgNode *n = AstNodePtrImpl(_n).get_ptr();
  return impl->ReplaceAst(orig, n);
}

// typedef bool BoolAttribute;
class BoolAttribute {
  bool val;

public:
  BoolAttribute(bool v = true) : val(v) {}
  operator bool() const { return val; }
};

class SageProcessAstNode
    : public AstTopDownBottomUpProcessing<BoolAttribute, BoolAttribute> {
  AstInterface *fa;
  AstInterface::TraversalOrderType t;
  ProcessAstNode<AstNodePtr> &op;
  BoolAttribute evaluateInheritedAttribute(SgNode *astNode,
                                           BoolAttribute inheritedValue) {
    if (t == AstInterface::PostOrder)
      return inheritedValue;
    return inheritedValue ? op.Traverse(*fa, AstNodePtrImpl(astNode),
                                        AstInterface::PreVisit)
                          : false;
  }
  BoolAttribute evaluateSynthesizedAttribute(SgNode *astNode,
                                             BoolAttribute inheritedValue,
                                             SynthesizedAttributesList l) {
    if (t == AstInterface::PreOrder)
      return inheritedValue;
    if (!inheritedValue)
      return false;
    for (size_t i = 0; i < l.size(); ++i)
      if (!l[i])
        return false;
    return op.Traverse(*fa, AstNodePtrImpl(astNode), AstInterface::PostVisit);
  }

public:
  SageProcessAstNode(ProcessAstNode<AstNodePtr> &_op) : op(_op) {}
  bool Traverse(AstInterface *_fa, SgNode *node,
                AstInterface::TraversalOrderType _t) {
    fa = _fa;
    t = _t;
    return AstTopDownBottomUpProcessing<BoolAttribute, BoolAttribute>::traverse(
        node, true);
  }
};

bool ReadAstTraverse(AstInterface &fa, const AstNodePtr &_root,
                     ProcessAstNode<AstNodePtr> &op,
                     AstInterface::TraversalOrderType t) {
  AstNodePtrImpl root(_root);
  return SageProcessAstNode(op).Traverse(&fa, root.get_ptr(), t);
}

template <class Transform>
class PerformPreTransformationTraversal
    : public AstTopDownBottomUpProcessing<_DummyAttribute, AstNodePtrImpl> {
  SgNode *head;
  AstNodePtrImpl result, orig;
  bool succ;
  Transform &op;
  AstInterface &fa;
  _DummyAttribute evaluateInheritedAttribute(SgNode *astNode,
                                             _DummyAttribute a) {
    if (!succ) {
      // std::cerr << "pre operating on " << astNode << "::" <<
      // astNode->unparseToString() << "\n";
      succ = op(fa, AstNodePtrImpl(astNode), result);
      if (succ) {
        assert(result != 0);
        orig = astNode;
      }
    }
    return _DummyAttribute();
  }
  AstNodePtrImpl evaluateSynthesizedAttribute(SgNode *astNode,
                                              _DummyAttribute a,
                                              SynthesizedAttributesList l) {
    // std::cerr << "post operating on " << astNode << "::" <<
    // astNode->unparseToString() << "\n";
    if (orig == astNode) {
      orig = 0;
      succ = false;
      if (result != astNode &&
          (astNode != head || astNode->get_parent() != 0)) {
        fa.ReplaceAst(AstNodePtrImpl(astNode), result);
      }
      return result;
    } else
      return astNode;
  }

public:
  PerformPreTransformationTraversal(AstInterface &_fa, Transform &_op)
      : result(0), orig(0), succ(false), op(_op), fa(_fa) {}
  AstNodePtrImpl operator()(SgNode *n) {
    succ = false;
    head = n;
    AstNodePtrImpl r =
        AstTopDownBottomUpProcessing<_DummyAttribute, AstNodePtrImpl>::traverse(
            n, _DummyAttribute());
    return r;
  }
};

template <class Transform>
class PerformPostTransformationTraversal
    : public AstBottomUpProcessing<AstNodePtrImpl> {
  SgNode *head;
  AstInterface &fa;
  Transform &op;
  AstNodePtrImpl evaluateSynthesizedAttribute(SgNode *astNode,
                                              SynthesizedAttributesList l) {
    AstNodePtrImpl r = astNode;
    if (op(fa, AstNodePtrImpl(astNode), r) && r != astNode) {
      assert(r != 0);
      if (r != astNode && (astNode != head || astNode->get_parent() != 0))
        fa.ReplaceAst(AstNodePtrImpl(astNode), r);
      return r;
    } else
      return astNode;
  }

public:
  PerformPostTransformationTraversal(AstInterface &_fa, Transform &_op)
      : fa(_fa), op(_op) {}
  AstNodePtrImpl operator()(SgNode *n) {
    head = n;
    AstNodePtrImpl r = AstBottomUpProcessing<AstNodePtrImpl>::traverse(n);
    return r;
  }
};

AstNodePtr TransformAstTraverse(AstInterface &fa, const AstNodePtr &r,
                                bool (*op)(AstInterface &, const AstNodePtr &,
                                           AstNodePtr &),
                                AstInterface::TraversalVisitType t) {
  if (t == AstInterface::PreVisit) {
    PerformPreTransformationTraversal<bool (*)(
        AstInterface &, const AstNodePtr &, AstNodePtr &)>
        traverse(fa, op);
    return traverse(AstNodePtrImpl(r).get_ptr());
  } else {
    PerformPostTransformationTraversal<bool (*)(
        AstInterface &, const AstNodePtr &, AstNodePtr &)>
        traverse(fa, op);
    return traverse(AstNodePtrImpl(r).get_ptr());
  }
}

AstNodePtr TransformAstTraverse(AstInterface &fa, const AstNodePtr &r,
                                TransformAstTree &op,
                                AstInterface::TraversalVisitType t) {
  if (t == AstInterface::PreVisit) {
    PerformPreTransformationTraversal<TransformAstTree> traverse(fa, op);
    AstNodePtr result = traverse(AstNodePtrImpl(r).get_ptr());
    return result;
  } else {
    PerformPostTransformationTraversal<TransformAstTree> traverse(fa, op);
    AstNodePtr result = traverse(AstNodePtrImpl(r).get_ptr());
    return result;
  }
}

template class PerformPreTransformationTraversal<bool (*)(
    AstInterface &, AstNodePtr const &, AstNodePtr &)>;
template class PerformPostTransformationTraversal<bool (*)(
    AstInterface &, AstNodePtr const &, AstNodePtr &)>;
template class PerformPreTransformationTraversal<TransformAstTree>;
template class PerformPostTransformationTraversal<TransformAstTree>;
template class std::list<SgExpression *, allocator<SgExpression *>>;
template class std::vector<AstNodePtr, allocator<AstNodePtr>>;
template class AstTopDownBottomUpProcessing<_DummyAttribute, AstNodePtr>;
template class AstBottomUpProcessing<AstNodePtr>;
template class SgTreeTraversal<_DummyAttribute, AstNodePtr>;

class CheckSymbolTable : public AstTopDownProcessing<AstNodePtrImpl> {
public:
  void operator()(SgNode *n) {
    AstTopDownProcessing<AstNodePtrImpl>::traverse(n, n->get_parent());
  }

  static void fix_vardecl(SgVariableDeclaration *d) {
    SgScopeStatement *scope = d->get_scope();
    if (DebugSymbol()) {
      cerr << "Adding symbol info for variable declaration: ";
      cerr << d->unparseToString() << " : into scope " << scope << endl;
    }
    SgInitializedNamePtrList &l = d->get_variables();
    for (SgInitializedNamePtrList::iterator p = l.begin(); p != l.end(); ++p) {
      SgInitializedName *n = *p;
      SgName name = n->get_name();
      SgVariableSymbol *sb = scope->lookup_var_symbol(name);
      if (sb == 0) {
        if (DebugSymbol())
          cerr << "Adding symbol for variable: " << name.str() << endl;
        NEW_SYMBOL(sb, SgVariableSymbol, scope, n);
      } else if (n != sb->get_declaration()) {
        ROSE_ABORT();
      }
    }
  }

  static void fix_classdecl(SgClassDeclaration *d1) {
    SgScopeStatement *scope = d1->get_scope();
    if (DebugSymbol()) {
      cerr << "Adding symbol info for class declaration: ";
      cerr << d1->unparseToString() << endl;
    }
    SgName name = d1->get_name();
    SgClassSymbol *sb = scope->lookup_class_symbol(name);
    if (sb == 0) {
      if (DebugSymbol())
        cerr << "Adding symbol for class: " << name.str() << endl;
      NEW_SYMBOL(sb, SgClassSymbol, scope, d1);
    }
  }

private:
  AstNodePtrImpl evaluateInheritedAttribute(SgNode *ast, AstNodePtrImpl v) {
    if (v.get_ptr() != 0 && v != ast->get_parent()) {
      if (ast->get_parent() == NULL)
        ast->set_parent(v.get_ptr());
      std::cerr << "Incorrect parent for AST: "
                << AstInterface::AstToString(AstNodePtrImpl(ast)) << "\n";
      std::cerr << "It has parent : "
                << ((ast->get_parent() == v.get_ptr())
                        ? "NULL"
                        : AstInterface::AstToString(
                              AstNodePtrImpl(ast->get_parent())))
                << "\n";
      std::cerr << "It should have parent: " << AstInterface::AstToString(v)
                << "\n";
      ROSE_ABORT();
    }
    switch (ast->variantT()) {
    case V_SgVariableDeclaration:
      fix_vardecl(isSgVariableDeclaration(ast));
      break;
    case V_SgClassDeclaration:
      fix_classdecl(isSgClassDeclaration(ast));
      break;
    case V_SgVarRefExp: {
      SgVarRefExp *var = isSgVarRefExp(ast);
      SgScopeStatement *scope =
          isSgScopeStatement(AstInterfaceImpl::GetScope(ast));
      assert(scope != 0);
      string name = var->get_symbol()->get_name().str();
      SgVariableSymbol *r =
          isSgVariableSymbol(AstInterfaceImpl::LookupVar(name, scope));
      if (r == 0) {
        cerr << "failed to find symbol for variable: " << name << " in scope "
             << scope << endl;
        // ROSE_ABORT();
      } else
        var->set_symbol(r);
    } break;
    default:
      break;
    }
    return ast;
  }
};

void FixSgTree(SgNode *r) {
  assert(r != 0);
  // AstPostProcessing(r);

  if (r->get_parent() != 0 && isSgScopeStatement(r) != 0) {
    CheckSymbolTable symbolfix;
    symbolfix(r);
  }
}

ROSE_DLL_API void FixSgProject(SgProject &sageProject) {
  int filenum = sageProject.numberOfFiles();
  for (int i = 0; i < filenum; ++i) {
    SgFile &sageFile = sageProject.get_file(i);
    FixSgTree(&sageFile);
  }
}

std::string AstInterface::GetVariableSignature(const AstNodePtr &_variable) {
  if (_variable.get_type() == AstNodePtr::SpecialAstType::GLOBAL_SIGNATURE) {
    return _variable.get_signature();
  }
  SgNode *variable = _variable.get_ptr();
  if (variable == 0)
    return AstInterface::AstTypeToString(_variable);
  AstInterfaceImpl astImpl(variable);
  AstInterface fa(&astImpl);
  std::string res;
  SgType *variable_is_type = isSgType(variable);
  if (variable_is_type != 0) {
    const std::string semantic_type_id =
        variable_is_type->get_mangled().getString();
    if (semantic_type_id.empty()) {
      fprintf(stderr,
              "REX_AST_INVARIANT[variable-signature-type]: type=%s has an "
              "empty semantic mangled identity\n",
              variable_is_type->class_name().c_str());
      ROSE_ABORT();
    }
    return semantic_type_id;
  }
  switch (variable->variantT()) {
  case V_SgNamespaceDeclarationStatement:
    return isSgNamespaceDeclarationStatement(variable)->get_name().getString();
  case V_SgUsingDirectiveStatement:
    return "using_" + isSgUsingDirectiveStatement(variable)
                          ->get_namespaceDeclaration()
                          ->get_name()
                          .getString();
  case V_SgEnumDeclaration: {
    SgEnumDeclaration *enum_decl = isSgEnumDeclaration(variable);
    std::string name = enum_decl->get_name();
    if (name.empty()) {
      SgInitializedNamePtrList &enumerators = enum_decl->get_enumerators();
      if (!enumerators.empty()) {
        name = enumerators.front()->get_name();
      }
    }
    return "enum_" + name;
  }
  case V_SgTypedefDeclaration:
  case V_SgTemplateTypedefDeclaration:
    return "typedef_" +
           AstInterface::GetGlobalUniqueName(
               variable->get_parent(),
               isSgTypedefDeclaration(variable)->get_name().getString(),
               /*do_not_add_file_name=*/true);
  case V_SgStaticAssertionDeclaration:
    return OperatorDeclaration::operator_signature(fa, variable);
  case V_SgClassDefinition:
    return GetVariableSignature(
        isSgClassDefinition(variable)->get_declaration());
  case V_SgAssignInitializer:
    return GetVariableSignature(isSgAssignInitializer(variable)->get_operand());
  case V_SgConstructorInitializer: {
    SgConstructorInitializer *ctor_init = isSgConstructorInitializer(variable);
    if (SgMemberFunctionDeclaration *decl = ctor_init->get_declaration()) {
      return GetVariableSignature(decl);
    }
    if (SgClassDeclaration *class_decl = ctor_init->get_class_decl()) {
      return GetVariableSignature(class_decl);
    }
    if (SgType *expr_type = ctor_init->get_expression_type()) {
      return GetVariableSignature(expr_type);
    }
    return "_UNKNOWN_" + variable->class_name();
  }
  case V_SgClassDeclaration:
  case V_SgTemplateClassDeclaration:
    return "class_" +
           std::string(isSgClassDeclaration(variable)->get_name().str());
  default:
    break;
  }

  if (AstInterface::IsFunctionDefinition(variable)) {
    return OperatorDeclaration::operator_signature(fa, variable);
  }
  {
    std::string value, valtype;
    if (fa.IsConstant(variable, &valtype, &value)) {
      if (valtype == "int") {
        return value;
      }
      return "CONSTANT";
    }
  }
  {
    AstNodePtr f;
    AstNodeList args;
    if (AstInterface::IsArrayAccess(variable, &f)) {
      return "_deref_(" + GetVariableSignature(f.get_ptr()) + ")";
    }
    if (fa.IsFunctionCall(variable, &f, &args)) {
      res += GetVariableSignature(f.get_ptr()) + "(";
      bool is_first = true;
      for (auto x : args) {
        if (!is_first) {
          res += ",";
        } else {
          is_first = false;
        }
        res += AstInterface::GetVariableSignature(x);
      }
      res += ")";
      return res;
    }
  }
  {
    AstNodeType alloc_type;
    if (IsMemoryAllocation(variable, &alloc_type)) {
      return res + "new_" +
             GetGlobalUniqueName(variable, GetTypeName(alloc_type),
                                 /*do_not_add_file_name=*/true);
    }
  }
  {
    std::string name;
    if (IsVarRef(variable, 0, &name, 0, 0, /*use_global_unique_name=*/true)) {
      return res + name;
    }
  }
  res += "_UNKNOWN_" + variable->class_name();
  return res;
}

bool AstInterface::IsLocalRef(const AstNodePtr &ref, const AstNodePtr &scope,
                              bool *has_ptr_deref) {
  if (ref.is_unknown()) {
    return false;
  }
  if (ref == AST_NULL) {
    return true;
  }
  if (ref.get_ptr() == 0) {
    return false;
  }

  std::string scope_name;
  if (!AstInterface::IsBlock(scope, &scope_name)) {
    std::cerr << "Expecting a block but getting :" << scope->class_name()
              << "\n";
  }
  DebugScope([&ref, &scope]() {
    return "IsLocalRef invoked: var is " + AstInterface::AstToString(ref) +
           "; scope is " + AstInterface::AstToString(scope);
  });
  AstNodePtr cur_scope;
  if (!AstInterface::IsVarRef(ref, 0, 0, &cur_scope, 0, false, has_ptr_deref)) {
    return false;
  }
  std::string cur_scope_name;
  while (cur_scope != AST_NULL && cur_scope->variantT() != V_SgGlobal) {
    if (!AstInterface::IsBlock(cur_scope, &cur_scope_name)) {
      std::cerr << "Expecting a block but getting :" << cur_scope->class_name()
                << "\n";
    }
    if (cur_scope.get_ptr() == scope.get_ptr() ||
        (scope_name != "" && cur_scope_name == scope_name)) {
      DebugScope([&ref]() {
        return "variable is local:" + AstInterface::AstToString(ref);
      });
      return true;
    }
    DebugScope([&cur_scope]() {
      return "IsLocalRef current scope:" + cur_scope->class_name();
    });
    SgNode *n = AstInterfaceImpl::GetScope(cur_scope.get_ptr());
    if (n == 0) {
      break;
    }
    cur_scope = n;
  }
  DebugScope([&ref]() {
    return "variable is not local:" + AstInterface::AstToString(ref);
  });
  return false;
}

SgVariableSymbol *AstInterfaceImpl::LookupVar(const std::string &name) {
  return LookupVar(name, scope);
}

void AstInterface::SetFunctionNameMangling(
    std::function<std::string(const SgFunctionDeclaration *)> f) {
  function_name_mangling_ = f;
}

/* EOF */
