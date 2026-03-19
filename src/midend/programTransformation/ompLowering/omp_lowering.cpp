// tps (01/14/2010) : Switching from rose.h to sage3.
#include "omp_lowering.h"

#include "Outliner.hh"

#include "RoseAst.h"

#include "rex_llvm.h"

#include "sage3basic.h"

#include "abiStuff.h"

#include "astUnparseAttribute.h"

#include "sageBuilder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <unordered_set>

using namespace std;
using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

static void insert_fortran_statement_in_specification_part(SgStatement *stmt,
                                                           SgBasicBlock *body);
static void
insert_fortran_declaration_into_procedure(SgVariableDeclaration *decl,
                                          SgScopeStatement *scope);

namespace {
std::map<const SgOmpClauseBodyStatement *, std::set<const SgInitializedName *>>
    implicit_target_map_variables;

SgVarRefExp *extractVarRefFromExpression(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }
  if (SgVarRefExp *vref = isSgVarRefExp(expr)) {
    return vref;
  }
  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return extractVarRefFromExpression(aref->get_lhs_operand());
  }
  if (SgDotExp *dot = isSgDotExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(dot->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(dot->get_lhs_operand());
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(arrow->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(arrow->get_lhs_operand());
  }
  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return extractVarRefFromExpression(deref->get_operand());
  }
  if (SgAddressOfOp *addr = isSgAddressOfOp(expr)) {
    return extractVarRefFromExpression(addr->get_operand());
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    return extractVarRefFromExpression(cast->get_operand());
  }
  if (SgCommaOpExp *comma = isSgCommaOpExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(comma->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(comma->get_lhs_operand());
  }
  if (SgExprListExp *list = isSgExprListExp(expr)) {
    for (SgExpression *elem : list->get_expressions()) {
      if (SgVarRefExp *vref = extractVarRefFromExpression(elem)) {
        return vref;
      }
    }
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    return extractVarRefFromExpression(unary->get_operand());
  }
  return nullptr;
}

SgVariableSymbol *extractClauseVariableSymbol(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  if (SgVarRefExp *vref = isSgVarRefExp(expr)) {
    return isSgVariableSymbol(vref->get_symbol());
  }
  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return extractClauseVariableSymbol(aref->get_lhs_operand());
  }
  if (SgDotExp *dot = isSgDotExp(expr)) {
    if (SgVariableSymbol *lhs =
            extractClauseVariableSymbol(dot->get_lhs_operand())) {
      return lhs;
    }
    return extractClauseVariableSymbol(dot->get_rhs_operand());
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    if (SgVariableSymbol *lhs =
            extractClauseVariableSymbol(arrow->get_lhs_operand())) {
      return lhs;
    }
    return extractClauseVariableSymbol(arrow->get_rhs_operand());
  }
  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return extractClauseVariableSymbol(deref->get_operand());
  }
  if (SgAddressOfOp *addr = isSgAddressOfOp(expr)) {
    return extractClauseVariableSymbol(addr->get_operand());
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    return extractClauseVariableSymbol(cast->get_operand());
  }
  if (SgCommaOpExp *comma = isSgCommaOpExp(expr)) {
    if (SgVariableSymbol *rhs =
            extractClauseVariableSymbol(comma->get_rhs_operand())) {
      return rhs;
    }
    return extractClauseVariableSymbol(comma->get_lhs_operand());
  }
  if (SgExprListExp *list = isSgExprListExp(expr)) {
    for (SgExpression *elem : list->get_expressions()) {
      if (SgVariableSymbol *sym = extractClauseVariableSymbol(elem)) {
        return sym;
      }
    }
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    return extractClauseVariableSymbol(unary->get_operand());
  }
  return nullptr;
}

bool isInAnyClauseVariableList(const std::vector<SgOmpMapClause *> &clauses,
                               SgSymbol *var) {
  for (std::vector<SgOmpMapClause *>::const_iterator iter = clauses.begin();
       iter != clauses.end(); ++iter) {
    if (*iter != NULL && isInClauseVariableList(*iter, var)) {
      return true;
    }
  }
  return false;
}

bool isOmpLibUseStatement(const SgStatement *stmt) {
  const SgUseStatement *use_stmt = isSgUseStatement(stmt);
  if (use_stmt == nullptr) {
    return false;
  }

  std::string module_name = use_stmt->get_name().getString();
  std::transform(module_name.begin(), module_name.end(), module_name.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return module_name == "omp_lib";
}

bool isOmpLibIncludeStatement(const SgStatement *stmt) {
  const SgFortranIncludeLine *include_stmt = isSgFortranIncludeLine(stmt);
  if (include_stmt == nullptr) {
    return false;
  }

  const std::filesystem::path include_path(include_stmt->get_filename());
  return include_path.filename() == "omp_lib.h";
}

SgBasicBlock *getEnclosingFortranProcedureBody(SgScopeStatement *scope) {
  ROSE_ASSERT(scope != nullptr);
  SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
  ROSE_ASSERT(func_def != nullptr);
  SgBasicBlock *body = func_def->get_body();
  ROSE_ASSERT(body != nullptr);
  return body;
}

void ensureFortranOmpAllocatorInterfaces(SgScopeStatement *scope) {
  SgBasicBlock *body = getEnclosingFortranProcedureBody(scope);
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgStatement *stmt = *it;
    if (stmt == nullptr) {
      continue;
    }
    if (isOmpLibUseStatement(stmt) || isOmpLibIncludeStatement(stmt)) {
      return;
    }
  }

  SgFortranIncludeLine *include_stmt = buildFortranIncludeLine("omp_lib.h");
  insert_fortran_statement_in_specification_part(include_stmt, body);
}

std::string
ompAllocatorModifierName(SgOmpClause::omp_allocator_modifier_enum modifier) {
  switch (modifier) {
  case SgOmpClause::e_omp_allocator_default_mem_alloc:
    return "omp_default_mem_alloc";
  case SgOmpClause::e_omp_allocator_large_cap_mem_alloc:
    return "omp_large_cap_mem_alloc";
  case SgOmpClause::e_omp_allocator_const_mem_alloc:
    return "omp_const_mem_alloc";
  case SgOmpClause::e_omp_allocator_high_bw_mem_alloc:
    return "omp_high_bw_mem_alloc";
  case SgOmpClause::e_omp_allocator_low_lat_mem_alloc:
    return "omp_low_lat_mem_alloc";
  case SgOmpClause::e_omp_allocator_cgroup_mem_alloc:
    return "omp_cgroup_mem_alloc";
  case SgOmpClause::e_omp_allocator_pteam_mem_alloc:
    return "omp_pteam_mem_alloc";
  case SgOmpClause::e_omp_allocator_thread_mem_alloc:
    return "omp_thread_mem_alloc";
  default:
    return "";
  }
}

SgExpression *
buildAllocatorArgumentExpression(const SgOmpAllocatorClause *clause,
                                 SgScopeStatement *scope) {
  ROSE_ASSERT(clause != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const SgOmpClause::omp_allocator_modifier_enum modifier =
      clause->get_modifier();
  if (modifier == SgOmpClause::e_omp_allocator_user_defined_modifier) {
    SgExpression *user_defined = clause->get_user_defined_modifier();
    ROSE_ASSERT(user_defined != nullptr);
    return copyExpression(user_defined);
  }

  const std::string allocator_name = ompAllocatorModifierName(modifier);
  if (!allocator_name.empty()) {
    return buildOpaqueVarRefExp(allocator_name, scope);
  }

  MLOG_ERROR_CXX("ompLowering")
      << "Unsupported allocator modifier in OpenMP allocate lowering: "
      << static_cast<int>(modifier);
  ROSE_ABORT();
}

SgOmpAllocatorClause *
getAllocatorClauseOrAbort(SgOmpAllocateStatement *target) {
  ROSE_ASSERT(target != nullptr);

  SgOmpAllocatorClause *allocator_clause = nullptr;
  const SgOmpClausePtrList &clauses = target->get_clauses();
  for (SgOmpClausePtrList::const_iterator it = clauses.begin();
       it != clauses.end(); ++it) {
    SgOmpClause *clause = *it;
    if (clause == nullptr) {
      continue;
    }
    if (SgOmpAllocatorClause *current = isSgOmpAllocatorClause(clause)) {
      if (allocator_clause != nullptr) {
        MLOG_ERROR_CXX("ompLowering")
            << "OpenMP allocate lowering expects at most one allocator clause";
        ROSE_ABORT();
      }
      allocator_clause = current;
      continue;
    }

    MLOG_ERROR_CXX("ompLowering")
        << "Unsupported clause on OpenMP allocate statement in lowering: "
        << clause->sage_class_name();
    ROSE_ABORT();
  }

  if (allocator_clause == nullptr) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate lowering requires an explicit allocator clause";
    ROSE_ABORT();
  }

  return allocator_clause;
}

std::set<SgInitializedName *>
collectReferencedBaseObjects(const SgExpressionPtrList &expressions) {
  std::set<SgInitializedName *> result;
  for (SgExpressionPtrList::const_iterator it = expressions.begin();
       it != expressions.end(); ++it) {
    SgExpression *expr = *it;
    if (expr == nullptr) {
      continue;
    }
    SgInitializedName *name = SageInterface::convertRefToInitializedName(expr);
    if (name == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Unable to resolve allocate object from expression '"
          << expr->unparseToString() << "'";
      ROSE_ABORT();
    }
    result.insert(name);
  }
  return result;
}

std::set<SgInitializedName *>
collectAllocateStatementBaseObjects(const SgAllocateStatement *stmt) {
  ROSE_ASSERT(stmt != nullptr);
  SgExprListExp *expr_list = stmt->get_expr_list();
  ROSE_ASSERT(expr_list != nullptr);

  SgExpressionPtrList allocate_objects;
  const SgExpressionPtrList &exprs = expr_list->get_expressions();
  for (SgExpressionPtrList::const_iterator it = exprs.begin();
       it != exprs.end(); ++it) {
    SgExpression *expr = *it;
    if (expr == nullptr || isSgTypeExpression(expr) != nullptr) {
      continue;
    }
    allocate_objects.push_back(expr);
  }

  return collectReferencedBaseObjects(allocate_objects);
}

bool requiresOnlyDynamicAllocators(const SgOmpRequiresStatement *stmt) {
  ROSE_ASSERT(stmt != nullptr);
  const SgOmpClausePtrList &clauses = stmt->get_clauses();
  if (clauses.empty()) {
    return false;
  }

  for (SgOmpClausePtrList::const_iterator it = clauses.begin();
       it != clauses.end(); ++it) {
    if (isSgOmpDynamicAllocatorsClause(*it) == nullptr) {
      return false;
    }
  }

  return true;
}

SgExpression *stripNoopCastsAndParens(SgExpression *expr) {
  SgExpression *result = expr;
  while (result != nullptr) {
    if (SgCastExp *cast = isSgCastExp(result)) {
      result = cast->get_operand();
      continue;
    }
    if (SgExprListExp *list = isSgExprListExp(result)) {
      if (list->get_expressions().size() == 1) {
        result = list->get_expressions().front();
        continue;
      }
    }
    break;
  }
  return result;
}

bool extractPointerDerefChain(SgExpression *expr, SgVarRefExp *&base_ref,
                              size_t &deref_depth) {
  base_ref = nullptr;
  deref_depth = 0;
  SgExpression *cursor = stripNoopCastsAndParens(expr);
  while (SgPointerDerefExp *deref = isSgPointerDerefExp(cursor)) {
    ++deref_depth;
    cursor = stripNoopCastsAndParens(deref->get_operand());
  }
  base_ref = isSgVarRefExp(cursor);
  return base_ref != nullptr && deref_depth > 0;
}

void normalizeScalarLocalDerefUses(
    SgBasicBlock *bb,
    const std::set<SgVariableSymbol *> &scalar_locals_from_pointer_symbols) {
  if (bb == nullptr || scalar_locals_from_pointer_symbols.empty()) {
    return;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    typedef Rose_STL_Container<SgNode *> NodeList_t;
    NodeList_t derefs = NodeQuery::querySubTree(bb, V_SgPointerDerefExp);
    for (NodeList_t::iterator i = derefs.begin(); i != derefs.end(); ++i) {
      SgPointerDerefExp *deref = isSgPointerDerefExp(*i);
      if (deref == nullptr || deref->get_parent() == nullptr) {
        continue;
      }
      SgExpression *operand = stripNoopCastsAndParens(deref->get_operand());
      SgVarRefExp *var_ref = isSgVarRefExp(operand);
      if (var_ref == nullptr || var_ref->get_symbol() == nullptr) {
        continue;
      }
      if (scalar_locals_from_pointer_symbols.count(var_ref->get_symbol()) ==
          0) {
        continue;
      }
      replaceExpression(deref, buildVarRefExp(var_ref->get_symbol()));
      changed = true;
    }
  }
}

SgType *stripTypeAliases(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }
  return type->stripType(SgType::STRIP_MODIFIER_TYPE |
                         SgType::STRIP_TYPEDEF_TYPE);
}

SgType *stripTypeAliasesAndReferences(SgType *type) {
  SgType *result = stripTypeAliases(type);
  while (SgReferenceType *ref_type = isSgReferenceType(result)) {
    result = stripTypeAliases(ref_type->get_base_type());
  }
  return result;
}

bool isPointerBackedType(SgType *type) {
  return isSgPointerType(stripTypeAliasesAndReferences(type)) != nullptr;
}

bool isDirectVarRefToSymbol(SgExpression *expr, SgVariableSymbol *sym) {
  if (expr == nullptr || sym == nullptr) {
    return false;
  }
  SgVarRefExp *ref = isSgVarRefExp(stripNoopCastsAndParens(expr));
  return ref != nullptr && ref->get_symbol() == sym;
}

SgExpression *getLValueChainRoot(SgExpression *expr) {
  SgExpression *current = expr;
  while (current != nullptr && current->get_parent() != nullptr) {
    SgNode *parent = current->get_parent();
    if (SgDotExp *dot = isSgDotExp(parent)) {
      if (dot->get_lhs_operand() == current) {
        current = dot;
        continue;
      }
    }
    if (SgArrowExp *arrow = isSgArrowExp(parent)) {
      if (arrow->get_lhs_operand() == current) {
        current = arrow;
        continue;
      }
    }
    if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(parent)) {
      if (aref->get_lhs_operand() == current) {
        current = aref;
        continue;
      }
    }
    break;
  }
  return current;
}

bool isWriteUseOfExpression(SgExpression *expr) {
  if (expr == nullptr || expr->get_parent() == nullptr) {
    return false;
  }

  SgNode *parent = expr->get_parent();
  if (SgAssignOp *assign = isSgAssignOp(parent)) {
    return assign->get_lhs_operand() == expr;
  }
  if (SgCompoundAssignOp *compound = isSgCompoundAssignOp(parent)) {
    return compound->get_lhs_operand() == expr;
  }
  if (SgPlusPlusOp *inc = isSgPlusPlusOp(parent)) {
    return inc->get_operand() == expr;
  }
  if (SgMinusMinusOp *dec = isSgMinusMinusOp(parent)) {
    return dec->get_operand() == expr;
  }
  return false;
}

bool isExpressionWrittenThroughChain(SgExpression *expr) {
  return isWriteUseOfExpression(getLValueChainRoot(expr));
}

bool isExpressionAddressTaken(SgExpression *expr) {
  if (expr == nullptr) {
    return false;
  }
  SgExpression *root = getLValueChainRoot(expr);
  if (root == nullptr || root->get_parent() == nullptr) {
    return false;
  }
  return isSgAddressOfOp(root->get_parent()) != nullptr;
}

SgExpression *getEnclosingReadOnlyAccessRoot(SgExpression *expr,
                                             SgVariableSymbol *base_sym) {
  SgExpression *cursor = stripNoopCastsAndParens(expr);
  while (cursor != nullptr && cursor->get_parent() != nullptr) {
    SgExpression *parent = isSgExpression(cursor->get_parent());
    if (parent == nullptr) {
      break;
    }

    if (base_sym != nullptr &&
        extractClauseVariableSymbol(parent) != base_sym) {
      break;
    }

    if (SgCastExp *cast = isSgCastExp(parent)) {
      if (cast->get_operand() == cursor) {
        cursor = cast;
        continue;
      }
    }

    if (SgPointerDerefExp *deref = isSgPointerDerefExp(parent)) {
      if (deref->get_operand() == cursor) {
        cursor = deref;
        continue;
      }
    }

    if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(parent)) {
      if (aref->get_lhs_operand() == cursor ||
          aref->get_rhs_operand() == cursor) {
        cursor = aref;
        continue;
      }
    }

    if (SgDotExp *dot = isSgDotExp(parent)) {
      if (dot->get_lhs_operand() == cursor ||
          dot->get_rhs_operand() == cursor) {
        cursor = dot;
        continue;
      }
    }

    if (SgArrowExp *arrow = isSgArrowExp(parent)) {
      if (arrow->get_lhs_operand() == cursor ||
          arrow->get_rhs_operand() == cursor) {
        cursor = arrow;
        continue;
      }
    }

    break;
  }
  return cursor;
}

bool mappedArrayUsesAreReadOnlyInScope(SgBasicBlock *body,
                                       SgVariableSymbol *base_sym) {
  if (body == nullptr || base_sym == nullptr ||
      !isPointerBackedType(base_sym->get_type())) {
    return false;
  }

  bool saw_supported_access = false;
  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = var_refs.begin();
       it != var_refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == nullptr || ref->get_symbol() != base_sym) {
      continue;
    }

    SgExpression *root = getEnclosingReadOnlyAccessRoot(ref, base_sym);
    if (root == nullptr || extractClauseVariableSymbol(root) != base_sym) {
      return false;
    }

    if (isSgPntrArrRefExp(root) == nullptr && isSgDotExp(root) == nullptr &&
        isSgArrowExp(root) == nullptr && isSgPointerDerefExp(root) == nullptr) {
      return false;
    }

    if (isExpressionWrittenThroughChain(root) ||
        isExpressionAddressTaken(root)) {
      return false;
    }

    SgNode *parent = root->get_parent();
    if (isSgAssignInitializer(parent) != nullptr) {
      return false;
    }
    if (SgExprListExp *args = isSgExprListExp(parent)) {
      if (isSgFunctionCallExp(args->get_parent()) != nullptr) {
        return false;
      }
    }

    saw_supported_access = true;
  }

  return saw_supported_access;
}

bool containsUnsupportedDirectGridStrideControlFlow(SgBasicBlock *body) {
  if (body == nullptr) {
    return true;
  }
  return !NodeQuery::querySubTree(body, V_SgBreakStmt).empty() ||
         !NodeQuery::querySubTree(body, V_SgContinueStmt).empty() ||
         !NodeQuery::querySubTree(body, V_SgGotoStatement).empty() ||
         !NodeQuery::querySubTree(body, V_SgReturnStmt).empty();
}

bool isScalarizableDirectGridStrideElementType(SgType *type) {
  SgType *stripped = stripTypeAliasesAndReferences(type);
  return stripped != nullptr && isSgClassType(stripped) == nullptr &&
         isSgArrayType(stripped) == nullptr &&
         isSgFunctionType(stripped) == nullptr &&
         isSgTypeVoid(stripped) == nullptr;
}

bool isAggregateDirectGridStrideElementType(SgType *type) {
  return isSgClassType(stripTypeAliasesAndReferences(type)) != nullptr;
}

bool symbolWrittenInsideScope(SgBasicBlock *body, SgVariableSymbol *sym);

bool isPointerToConstType(SgType *type) {
  SgPointerType *ptr_type =
      isSgPointerType(stripTypeAliasesAndReferences(type));
  if (ptr_type == nullptr) {
    return false;
  }

  SgType *base_type = ptr_type->get_base_type();
  return base_type != nullptr && SageInterface::isConstType(base_type);
}

SgInitializedName *
findMatchingEnclosingFunctionParameter(SgInitializedName *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  SgFunctionDeclaration *func = getEnclosingFunctionDeclaration(decl);
  if (func == nullptr || func->get_parameterList() == nullptr) {
    return nullptr;
  }

  const SgInitializedNamePtrList &params =
      func->get_parameterList()->get_args();
  for (SgInitializedNamePtrList::const_iterator it = params.begin();
       it != params.end(); ++it) {
    SgInitializedName *param = *it;
    if (param == nullptr || param == decl) {
      continue;
    }
    if (param->get_name() == decl->get_name()) {
      return param;
    }
  }

  return nullptr;
}

bool exprDerivesFromReadOnlyDevicePointer(
    SgExpression *expr, SgBasicBlock *kernel_body,
    std::set<SgVariableSymbol *> &visiting_syms);

bool symbolIsReadOnlyDevicePointer(
    SgVariableSymbol *sym, SgBasicBlock *kernel_body,
    std::set<SgVariableSymbol *> &visiting_syms) {
  if (sym == nullptr || kernel_body == nullptr ||
      !isPointerToConstType(sym->get_type())) {
    return false;
  }

  if (symbolWrittenInsideScope(kernel_body, sym)) {
    return false;
  }

  SgInitializedName *decl = sym->get_declaration();
  if (decl == nullptr) {
    return false;
  }

  if (isSgFunctionParameterList(decl->get_parent()) != nullptr) {
    return true;
  }

  if (SgInitializedName *param = findMatchingEnclosingFunctionParameter(decl)) {
    if (isPointerToConstType(param->get_type())) {
      return true;
    }
  }

  if (isSgVariableDeclaration(decl->get_parent()) != nullptr &&
      decl->get_initializer() == nullptr) {
    return true;
  }

  if (!visiting_syms.insert(sym).second) {
    return false;
  }

  bool derives_from_read_only_input = false;
  if (SgAssignInitializer *init =
          isSgAssignInitializer(decl->get_initializer())) {
    derives_from_read_only_input = exprDerivesFromReadOnlyDevicePointer(
        init->get_operand_i(), kernel_body, visiting_syms);
  }

  visiting_syms.erase(sym);
  return derives_from_read_only_input;
}

bool exprDerivesFromReadOnlyDevicePointer(
    SgExpression *expr, SgBasicBlock *kernel_body,
    std::set<SgVariableSymbol *> &visiting_syms) {
  expr = stripNoopCastsAndParens(expr);
  if (expr == nullptr) {
    return false;
  }

  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    return symbolIsReadOnlyDevicePointer(
        isSgVariableSymbol(var_ref->get_symbol()), kernel_body, visiting_syms);
  }

  if (SgAddressOfOp *addr = isSgAddressOfOp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(addr->get_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(aref->get_lhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgDotExp *dot = isSgDotExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(dot->get_lhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(arrow->get_lhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(deref->get_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgAddOp *add = isSgAddOp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(add->get_lhs_operand(),
                                                kernel_body, visiting_syms) ||
           exprDerivesFromReadOnlyDevicePointer(add->get_rhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgSubtractOp *sub = isSgSubtractOp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(sub->get_lhs_operand(),
                                                kernel_body, visiting_syms) ||
           exprDerivesFromReadOnlyDevicePointer(sub->get_rhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgConditionalExp *cond = isSgConditionalExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(cond->get_true_exp(),
                                                kernel_body, visiting_syms) &&
           exprDerivesFromReadOnlyDevicePointer(cond->get_false_exp(),
                                                kernel_body, visiting_syms);
  }

  return false;
}

int computeAstDepth(SgNode *node) {
  int depth = 0;
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    ++depth;
  }
  return depth;
}

bool isNodeWithinSubtree(SgNode *root, SgNode *node) {
  if (root == nullptr || node == nullptr) {
    return false;
  }

  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    if (cursor == root) {
      return true;
    }
  }
  return false;
}

bool isReadOnlyDeviceLoadCandidate(SgExpression *expr,
                                   SgBasicBlock *kernel_body) {
  if (expr == nullptr || kernel_body == nullptr ||
      !isScalarizableDirectGridStrideElementType(expr->get_type()) ||
      isExpressionWrittenThroughChain(expr) || isExpressionAddressTaken(expr)) {
    return false;
  }

  SgVariableSymbol *base_sym = extractClauseVariableSymbol(expr);
  if (base_sym == nullptr) {
    return false;
  }

  std::set<SgVariableSymbol *> visiting_syms;
  return symbolIsReadOnlyDevicePointer(base_sym, kernel_body, visiting_syms);
}

SgExpression *buildReadOnlyDeviceLoadExpr(SgExpression *expr,
                                          SgScopeStatement *scope) {
  ROSE_ASSERT(expr != nullptr);
  ROSE_ASSERT(scope != nullptr);

  SgType *value_type = stripTypeAliasesAndReferences(expr->get_type());
  ROSE_ASSERT(value_type != nullptr);

  return buildFunctionCallExp(
      "__ldg", value_type,
      buildExprListExp(buildAddressOfOp(copyExpression(expr))), scope);
}

void rewriteReadOnlyDeviceLoadsWithLdg(SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    return;
  }

  SgBasicBlock *outer_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  if (outer_body == nullptr) {
    return;
  }

  std::vector<SgExpression *> candidates;
  Rose_STL_Container<SgNode *> expr_nodes =
      NodeQuery::querySubTree(outer_body, V_SgExpression);
  for (Rose_STL_Container<SgNode *>::const_iterator it = expr_nodes.begin();
       it != expr_nodes.end(); ++it) {
    SgExpression *expr = isSgExpression(*it);
    if (expr == nullptr) {
      continue;
    }

    if (isSgPntrArrRefExp(expr) == nullptr && isSgDotExp(expr) == nullptr &&
        isSgArrowExp(expr) == nullptr && isSgPointerDerefExp(expr) == nullptr) {
      continue;
    }

    if (!isReadOnlyDeviceLoadCandidate(expr, outer_body)) {
      continue;
    }
    candidates.push_back(expr);
  }

  std::sort(candidates.begin(), candidates.end(),
            [](SgExpression *lhs, SgExpression *rhs) {
              return computeAstDepth(lhs) > computeAstDepth(rhs);
            });
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());

  for (std::vector<SgExpression *>::const_iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    SgExpression *expr = *it;
    if (!isNodeWithinSubtree(outer_body, expr)) {
      continue;
    }

    SgScopeStatement *scope = getEnclosingScope(expr);
    if (scope == nullptr) {
      scope = outer_body;
    }

    replaceExpression(expr, buildReadOnlyDeviceLoadExpr(expr, scope));
  }
}

bool candidateCoversAllBaseUses(SgBasicBlock *body, SgVariableSymbol *base_sym,
                                const std::vector<SgPntrArrRefExp *> &refs) {
  if (body == nullptr || base_sym == nullptr) {
    return false;
  }

  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = var_refs.begin();
       it != var_refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == nullptr || ref->get_symbol() != base_sym) {
      continue;
    }
    bool covered = false;
    for (std::vector<SgPntrArrRefExp *>::const_iterator ref_it = refs.begin();
         ref_it != refs.end(); ++ref_it) {
      if (isAncestor(*ref_it, ref)) {
        covered = true;
        break;
      }
    }
    if (!covered) {
      return false;
    }
  }
  return true;
}

bool symbolWrittenInsideScope(SgBasicBlock *body, SgVariableSymbol *sym) {
  if (body == nullptr || sym == nullptr) {
    return false;
  }

  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = var_refs.begin();
       it != var_refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == nullptr || ref->get_symbol() != sym) {
      continue;
    }
    if (isWriteUseOfExpression(ref)) {
      return true;
    }
  }
  return false;
}

bool isClosestEnclosingLoop(SgForStatement *loop, SgNode *node) {
  if (loop == nullptr || node == nullptr) {
    return false;
  }
  SgStatement *stmt = getEnclosingStatement(node);
  if (stmt == nullptr) {
    return false;
  }
  return findEnclosingLoop(stmt) == loop;
}

struct DirectGridStrideScalarCandidate {
  SgVariableSymbol *base_sym = nullptr;
  SgType *element_type = nullptr;
  std::vector<SgPntrArrRefExp *> refs;
  bool has_write = false;
  bool disallowed = false;
};

void scalarizeDirectGridStrideOuterIndexAccesses(
    SgForStatement *outer_loop, SgVariableSymbol *outer_index_sym) {
  if (outer_loop == nullptr || outer_index_sym == nullptr) {
    return;
  }

  SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  if (containsUnsupportedDirectGridStrideControlFlow(loop_body)) {
    return;
  }

  std::map<SgVariableSymbol *, DirectGridStrideScalarCandidate> candidates;
  Rose_STL_Container<SgNode *> refs =
      NodeQuery::querySubTree(loop_body, V_SgPntrArrRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = refs.begin();
       it != refs.end(); ++it) {
    SgPntrArrRefExp *aref = isSgPntrArrRefExp(*it);
    if (aref == nullptr ||
        !isDirectVarRefToSymbol(aref->get_rhs_operand(), outer_index_sym)) {
      continue;
    }

    SgVariableSymbol *base_sym =
        extractClauseVariableSymbol(aref->get_lhs_operand());
    if (base_sym == nullptr) {
      continue;
    }

    DirectGridStrideScalarCandidate &candidate = candidates[base_sym];
    candidate.base_sym = base_sym;
    if (candidate.element_type == nullptr) {
      candidate.element_type = stripTypeAliasesAndReferences(aref->get_type());
    }
    candidate.refs.push_back(aref);
    candidate.has_write =
        candidate.has_write || isExpressionWrittenThroughChain(aref);
    candidate.disallowed =
        candidate.disallowed || isExpressionAddressTaken(aref) ||
        !isScalarizableDirectGridStrideElementType(aref->get_type());
  }

  for (std::map<SgVariableSymbol *, DirectGridStrideScalarCandidate>::iterator
           it = candidates.begin();
       it != candidates.end(); ++it) {
    DirectGridStrideScalarCandidate &candidate = it->second;
    if (candidate.disallowed || candidate.refs.empty()) {
      continue;
    }
    if (candidate.refs.size() < 2 && !candidate.has_write) {
      continue;
    }
    if (!candidateCoversAllBaseUses(loop_body, candidate.base_sym,
                                    candidate.refs)) {
      continue;
    }

    std::string cache_name = generateUniqueVariableName(
        loop_body,
        "__rex_cached_" + candidate.base_sym->get_name().getString() + "_");
    SgType *cache_type = candidate.has_write
                             ? candidate.element_type
                             : buildConstType(candidate.element_type);
    SgExpression *init_expr = copyExpression(candidate.refs.front());
    SgVariableDeclaration *cache_decl = buildVariableDeclaration(
        cache_name, cache_type, buildAssignInitializer(init_expr, cache_type),
        loop_body);
    prependStatement(cache_decl, loop_body);
    SgVariableSymbol *cache_sym = getFirstVarSym(cache_decl);
    ROSE_ASSERT(cache_sym != nullptr);

    SgExpression *writeback_lhs = nullptr;
    if (candidate.has_write) {
      writeback_lhs = copyExpression(candidate.refs.front());
    }

    for (std::vector<SgPntrArrRefExp *>::reverse_iterator ref_it =
             candidate.refs.rbegin();
         ref_it != candidate.refs.rend(); ++ref_it) {
      replaceExpression(*ref_it, buildVarRefExp(cache_sym));
    }

    if (candidate.has_write && writeback_lhs != nullptr) {
      appendStatement(
          buildAssignStatement(writeback_lhs, buildVarRefExp(cache_sym)),
          loop_body);
    }
  }
}

struct InvariantAggregateRefKey {
  SgVariableSymbol *base_sym = nullptr;
  SgVariableSymbol *index_sym = nullptr;

  bool operator<(const InvariantAggregateRefKey &other) const {
    if (base_sym != other.base_sym) {
      return base_sym < other.base_sym;
    }
    return index_sym < other.index_sym;
  }
};

struct InvariantAggregateRefCandidate {
  SgVariableSymbol *base_sym = nullptr;
  SgVariableSymbol *index_sym = nullptr;
  SgType *element_type = nullptr;
  std::vector<SgPntrArrRefExp *> refs;
  bool disallowed = false;
};

struct InvariantFieldAccessKey {
  SgVariableSymbol *base_sym = nullptr;
  SgSymbol *field_sym = nullptr;

  bool operator<(const InvariantFieldAccessKey &other) const {
    if (base_sym != other.base_sym) {
      return base_sym < other.base_sym;
    }
    return field_sym < other.field_sym;
  }
};

struct InvariantFieldAccessCandidate {
  SgVariableSymbol *base_sym = nullptr;
  SgSymbol *field_sym = nullptr;
  SgType *cache_type = nullptr;
  std::vector<SgExpression *> refs;
  bool disallowed = false;
};

void hoistReadOnlyInvariantAggregateRefsBeforeLoop(SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    return;
  }

  SgBasicBlock *outer_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(outer_body, V_SgForStatement);
  for (Rose_STL_Container<SgNode *>::const_iterator loop_it = loops.begin();
       loop_it != loops.end(); ++loop_it) {
    SgForStatement *loop = isSgForStatement(*loop_it);
    if (loop == nullptr || loop == outer_loop) {
      continue;
    }

    SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(loop);
    SgBasicBlock *parent_block = isSgBasicBlock(loop->get_parent());
    if (loop_body == nullptr || parent_block == nullptr) {
      continue;
    }

    std::map<InvariantAggregateRefKey, InvariantAggregateRefCandidate>
        candidates;
    Rose_STL_Container<SgNode *> refs =
        NodeQuery::querySubTree(loop_body, V_SgPntrArrRefExp);
    for (Rose_STL_Container<SgNode *>::const_iterator ref_it = refs.begin();
         ref_it != refs.end(); ++ref_it) {
      SgPntrArrRefExp *aref = isSgPntrArrRefExp(*ref_it);
      if (aref == nullptr || !isClosestEnclosingLoop(loop, aref) ||
          !isAggregateDirectGridStrideElementType(aref->get_type())) {
        continue;
      }

      SgVariableSymbol *index_sym = nullptr;
      SgVarRefExp *index_ref =
          isSgVarRefExp(stripNoopCastsAndParens(aref->get_rhs_operand()));
      if (index_ref != nullptr) {
        index_sym = isSgVariableSymbol(index_ref->get_symbol());
      }
      SgVariableSymbol *base_sym =
          extractClauseVariableSymbol(aref->get_lhs_operand());
      if (base_sym == nullptr || index_sym == nullptr) {
        continue;
      }

      InvariantAggregateRefKey key;
      key.base_sym = base_sym;
      key.index_sym = index_sym;
      InvariantAggregateRefCandidate &candidate = candidates[key];
      candidate.base_sym = base_sym;
      candidate.index_sym = index_sym;
      if (candidate.element_type == nullptr) {
        candidate.element_type =
            stripTypeAliasesAndReferences(aref->get_type());
      }
      candidate.refs.push_back(aref);
      candidate.disallowed = candidate.disallowed ||
                             isExpressionAddressTaken(aref) ||
                             isExpressionWrittenThroughChain(aref);
    }

    for (std::map<InvariantAggregateRefKey,
                  InvariantAggregateRefCandidate>::iterator cand_it =
             candidates.begin();
         cand_it != candidates.end(); ++cand_it) {
      InvariantAggregateRefCandidate &candidate = cand_it->second;
      if (candidate.disallowed || candidate.refs.size() < 2 ||
          symbolWrittenInsideScope(loop_body, candidate.index_sym)) {
        continue;
      }

      std::string cache_name = generateUniqueVariableName(
          parent_block,
          "__rex_ref_" + candidate.base_sym->get_name().getString() + "_");
      SgType *ptr_type =
          buildPointerType(buildConstType(candidate.element_type));
      SgExpression *init_expr =
          buildAddressOfOp(copyExpression(candidate.refs.front()));
      SgVariableDeclaration *cache_decl = buildVariableDeclaration(
          cache_name, ptr_type, buildAssignInitializer(init_expr, ptr_type),
          parent_block);
      insertStatementBefore(loop, cache_decl);
      SgVariableSymbol *cache_sym = getFirstVarSym(cache_decl);
      ROSE_ASSERT(cache_sym != nullptr);

      for (std::vector<SgPntrArrRefExp *>::reverse_iterator ref_it =
               candidate.refs.rbegin();
           ref_it != candidate.refs.rend(); ++ref_it) {
        replaceExpression(*ref_it,
                          buildPointerDerefExp(buildVarRefExp(cache_sym)));
      }
    }
  }
}

void hoistReadOnlyInvariantFieldAccessesBeforeLoop(SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    return;
  }

  SgBasicBlock *outer_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(outer_body, V_SgForStatement);
  for (Rose_STL_Container<SgNode *>::const_iterator loop_it = loops.begin();
       loop_it != loops.end(); ++loop_it) {
    SgForStatement *loop = isSgForStatement(*loop_it);
    if (loop == nullptr || loop == outer_loop) {
      continue;
    }

    SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(loop);
    SgBasicBlock *parent_block = isSgBasicBlock(loop->get_parent());
    if (loop_body == nullptr || parent_block == nullptr) {
      continue;
    }

    std::map<InvariantFieldAccessKey, InvariantFieldAccessCandidate> candidates;
    Rose_STL_Container<SgNode *> field_refs =
        NodeQuery::querySubTree(loop_body, V_SgDotExp);
    for (Rose_STL_Container<SgNode *>::const_iterator ref_it =
             field_refs.begin();
         ref_it != field_refs.end(); ++ref_it) {
      SgDotExp *dot = isSgDotExp(*ref_it);
      if (dot == nullptr || !isClosestEnclosingLoop(loop, dot)) {
        continue;
      }

      SgVarRefExp *base_ref = nullptr;
      size_t deref_depth = 0;
      if (!extractPointerDerefChain(dot->get_lhs_operand(), base_ref,
                                    deref_depth) ||
          base_ref == nullptr || deref_depth != 1) {
        continue;
      }

      SgSymbol *field_sym = nullptr;
      if (SgVarRefExp *field_ref =
              isSgVarRefExp(stripNoopCastsAndParens(dot->get_rhs_operand()))) {
        field_sym = field_ref->get_symbol();
      }
      if (field_sym == nullptr) {
        continue;
      }

      SgType *raw_field_type = stripTypeAliases(dot->get_type());
      if (raw_field_type == nullptr) {
        continue;
      }

      SgType *cache_type = nullptr;
      if (SgArrayType *array_type = isSgArrayType(raw_field_type)) {
        cache_type =
            buildPointerType(buildConstType(array_type->findBaseType()));
      } else {
        SgType *field_type = stripTypeAliasesAndReferences(raw_field_type);
        if (field_type == nullptr || isSgClassType(field_type) != nullptr ||
            isSgTypeVoid(field_type) != nullptr) {
          continue;
        }
        cache_type = buildConstType(field_type);
      }

      InvariantFieldAccessKey key;
      key.base_sym = isSgVariableSymbol(base_ref->get_symbol());
      key.field_sym = field_sym;
      InvariantFieldAccessCandidate &candidate = candidates[key];
      candidate.base_sym = key.base_sym;
      candidate.field_sym = field_sym;
      if (candidate.cache_type == nullptr) {
        candidate.cache_type = cache_type;
      }
      candidate.refs.push_back(dot);
      candidate.disallowed = candidate.disallowed ||
                             isExpressionAddressTaken(dot) ||
                             isExpressionWrittenThroughChain(dot);
    }

    for (std::map<InvariantFieldAccessKey,
                  InvariantFieldAccessCandidate>::iterator cand_it =
             candidates.begin();
         cand_it != candidates.end(); ++cand_it) {
      InvariantFieldAccessCandidate &candidate = cand_it->second;
      if (candidate.disallowed || candidate.refs.size() < 2 ||
          candidate.base_sym == nullptr || candidate.cache_type == nullptr ||
          symbolWrittenInsideScope(loop_body, candidate.base_sym)) {
        continue;
      }

      SgVariableSymbol *field_var_sym = isSgVariableSymbol(candidate.field_sym);
      const std::string field_name = field_var_sym != nullptr
                                         ? field_var_sym->get_name().getString()
                                         : std::string("field");
      std::string cache_name = generateUniqueVariableName(
          parent_block, "__rex_field_" + field_name + "_");
      SgVariableDeclaration *cache_decl = buildVariableDeclaration(
          cache_name, candidate.cache_type,
          buildAssignInitializer(copyExpression(candidate.refs.front()),
                                 candidate.cache_type),
          parent_block);
      insertStatementBefore(loop, cache_decl);
      SgVariableSymbol *cache_sym = getFirstVarSym(cache_decl);
      ROSE_ASSERT(cache_sym != nullptr);

      for (std::vector<SgExpression *>::reverse_iterator ref_it =
               candidate.refs.rbegin();
           ref_it != candidate.refs.rend(); ++ref_it) {
        replaceExpression(*ref_it, buildVarRefExp(cache_sym));
      }
    }
  }
}

bool is_32_bit_target(const SgNode *context) {
  SgProject *project = SageInterface::getProject(context);
  ROSE_ASSERT(project != nullptr);
  return project->get_mode_32_bit();
}

StructLayoutInfo get_target_layout_info(SgType *type, const SgNode *context) {
  ROSE_ASSERT(type != nullptr);

  if (is_32_bit_target(context)) {
    I386PrimitiveTypeLayoutGenerator primitive_generator(nullptr);
    NonpackedTypeLayoutGenerator layout_generator(&primitive_generator);
    return layout_generator.layoutType(type);
  }

  X86_64PrimitiveTypeLayoutGenerator primitive_generator(nullptr);
  NonpackedTypeLayoutGenerator layout_generator(&primitive_generator);
  return layout_generator.layoutType(type);
}

size_t get_target_type_size_bytes(SgType *type, const SgNode *context) {
  StructLayoutInfo layout = get_target_layout_info(type, context);
  ROSE_ASSERT(layout.size > 0);
  return layout.size;
}

bool use_kmpc_loop_64bit_runtime(SgType *loop_var_type, const SgNode *context) {
  return get_target_type_size_bytes(loop_var_type, context) > 4;
}

const char *get_kmpc_for_static_init_name(bool use_64_runtime) {
  return use_64_runtime ? "__kmpc_for_static_init_8"
                        : "__kmpc_for_static_init_4";
}

const char *get_kmpc_dispatch_init_name(bool use_64_runtime) {
  return use_64_runtime ? "__kmpc_dispatch_init_8" : "__kmpc_dispatch_init_4";
}

const char *get_kmpc_dispatch_next_name(bool use_64_runtime) {
  return use_64_runtime ? "__kmpc_dispatch_next_8" : "__kmpc_dispatch_next_4";
}

SgType *resolvePointerBaseType(SgType *pointer_type, size_t deref_depth) {
  SgType *result = pointer_type;
  for (size_t i = 0; i < deref_depth; ++i) {
    result = stripTypeAliases(result);
    SgPointerType *ptr = isSgPointerType(result);
    if (ptr == nullptr) {
      return nullptr;
    }
    result = ptr->get_base_type();
  }
  return stripTypeAliases(result);
}

bool buildExpressionMatchingTypeFromActiveSymbol(
    SgVariableSymbol *active_symbol, SgType *expected_type,
    SgExpression *&value_expr) {
  ROSE_ASSERT(active_symbol != nullptr);
  ROSE_ASSERT(expected_type != nullptr);

  SgType *expected = stripTypeAliases(expected_type);
  ROSE_ASSERT(expected != nullptr);

  SgExpression *candidate = buildVarRefExp(active_symbol);
  SgType *current = stripTypeAliases(active_symbol->get_type());

  while (current != nullptr) {
    if (current == expected) {
      value_expr = candidate;
      return true;
    }

    if (SgReferenceType *ref_type = isSgReferenceType(current)) {
      current = stripTypeAliases(ref_type->get_base_type());
      continue;
    }

    SgPointerType *ptr_type = isSgPointerType(current);
    if (ptr_type == nullptr)
      break;

    candidate = buildPointerDerefExp(candidate);
    current = stripTypeAliases(ptr_type->get_base_type());
  }

  return false;
}

struct ResolvedMapperInfo {
  SgOmpDeclareMapperStatement *declaration = nullptr;
  std::string identifier_text;
  std::string formal_name;
  SgType *formal_type = nullptr;
};

enum class MapperUseKind { map_clause, to_clause, from_clause };

struct ResolvedMapItem {
  SgExpression *expression = nullptr;
  SgOmpClause::omp_map_operator_enum map_operator =
      SgOmpClause::e_omp_map_unknown;
  int runtime_flag_bits = 0;
  bool is_implicit_base_pointer = false;
  bool is_implicit_target_variable = false;
  bool use_literal_target_param = false;
  SgVariableSymbol *direct_variable_symbol = nullptr;
};

enum class ExpandedMapEntryKind { direct_item, dynamic_mapper_section };

struct ExpandedMapEntry {
  ExpandedMapEntryKind kind = ExpandedMapEntryKind::direct_item;
  ResolvedMapItem direct_item;
  ResolvedMapperInfo resolved_mapper;
  SgExpression *section_base_expression = nullptr;
  std::vector<std::pair<SgExpression *, SgExpression *>> section_dimensions;
  MapperUseKind use_kind = MapperUseKind::map_clause;
  SgOmpClause::omp_map_operator_enum use_map_op =
      SgOmpClause::e_omp_map_unknown;
  int runtime_flag_bits = 0;
  SgStatement *anchor_stmt = nullptr;
};

SgVariableSymbol *getDirectResolvedMapItemVariableSymbol(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  SgVarRefExp *var_ref = isSgVarRefExp(stripNoopCastsAndParens(expr));
  if (var_ref == nullptr) {
    return nullptr;
  }

  return isSgVariableSymbol(var_ref->get_symbol());
}

SgExpression *buildLiteralTargetParamArgExpression(SgVariableSymbol *var_sym,
                                                   SgScopeStatement *scope) {
  ROSE_ASSERT(var_sym != NULL);
  ROSE_ASSERT(scope != NULL);

  SgType *type = stripTypeAliasesAndReferences(var_sym->get_type());
  ROSE_ASSERT(type != NULL);

  return buildFunctionCallExp(
      "rex_pack_literal_arg_bytes", buildPointerType(buildVoidType()),
      buildExprListExp(buildAddressOfOp(buildVarRefExp(var_sym)),
                       buildSizeOfOp(type)),
      scope);
}

std::string trimMapperCopy(const std::string &value) {
  const std::string::size_type begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const std::string::size_type end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string getVarRefNameText(const SgVarRefExp *vref) {
  if (vref == nullptr) {
    return "";
  }
  if (vref->get_symbol() != nullptr) {
    return vref->get_symbol()->get_name().getString();
  }
  return trimMapperCopy(vref->unparseToString());
}

std::string normalizeMapperIdentifierString(const std::string &value) {
  std::string trimmed = trimMapperCopy(value);
  std::transform(
      trimmed.begin(), trimmed.end(), trimmed.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return trimmed;
}

std::string getMapperIdentifierText(const SgExpression *expr) {
  if (expr == nullptr) {
    return "";
  }
  if (const SgVarRefExp *vref = isSgVarRefExp(expr)) {
    return normalizeMapperIdentifierString(getVarRefNameText(vref));
  }
  return normalizeMapperIdentifierString(expr->unparseToString());
}

bool isDefaultDeclareMapperIdentifier(
    SgOmpClause::omp_declare_mapper_identifier_enum identifier) {
  return identifier == SgOmpClause::e_omp_declare_mapper_identifier_default ||
         identifier == SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
}

std::string
getDeclareMapperFormalName(const SgOmpDeclareMapperStatement *mapper_stmt) {
  if (mapper_stmt == nullptr || mapper_stmt->get_mapper_variable() == nullptr) {
    return "";
  }
  if (const SgVarRefExp *vref =
          isSgVarRefExp(mapper_stmt->get_mapper_variable())) {
    return trimMapperCopy(getVarRefNameText(vref));
  }
  return trimMapperCopy(mapper_stmt->get_mapper_variable()->unparseToString());
}

SgType *
getDeclareMapperFormalType(const SgOmpDeclareMapperStatement *mapper_stmt) {
  if (mapper_stmt == nullptr || mapper_stmt->get_mapper_type() == nullptr) {
    return nullptr;
  }
  if (SgTypeExpression *type_expr =
          isSgTypeExpression(mapper_stmt->get_mapper_type())) {
    return type_expr->get_type();
  }
  return mapper_stmt->get_mapper_type()->get_type();
}

SgStatement *findDirectChildStatementInScope(SgStatement *anchor,
                                             SgScopeStatement *scope) {
  if (anchor == nullptr || scope == nullptr) {
    return nullptr;
  }

  SgNode *cursor = anchor;
  while (cursor != nullptr && cursor->get_parent() != scope) {
    cursor = cursor->get_parent();
  }
  return isSgStatement(cursor);
}

bool collectEffectiveArraySectionDimensions(
    SgExpression *expression,
    std::vector<std::pair<SgExpression *, SgExpression *>> &dimensions) {
  if (expression == nullptr) {
    return false;
  }

  if (SgCastExp *cast_exp = isSgCastExp(expression)) {
    return collectEffectiveArraySectionDimensions(cast_exp->get_operand(),
                                                  dimensions);
  }

  if (SgUnaryOp *unary_op = isSgUnaryOp(expression)) {
    return collectEffectiveArraySectionDimensions(unary_op->get_operand(),
                                                  dimensions);
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(expression)) {
    if (!collectEffectiveArraySectionDimensions(array_ref->get_lhs_operand(),
                                                dimensions)) {
      return false;
    }

    if (SgSubscriptExpression *subscript =
            isSgSubscriptExpression(array_ref->get_rhs_operand())) {
      SgExpression *lower = subscript->get_lowerBound();
      SgExpression *length = subscript->get_upperBound();
      SgExpression *stride = subscript->get_stride();
      if (stride != nullptr && isSgNullExpression(stride) == nullptr) {
        const SgIntVal *int_stride = isSgIntVal(stride);
        if (int_stride == nullptr || int_stride->get_value() != 1) {
          dimensions.clear();
          return false;
        }
      }
      if (lower != nullptr && isSgNullExpression(lower) == nullptr &&
          length != nullptr && isSgNullExpression(length) == nullptr) {
        dimensions.push_back(std::make_pair(lower, length));
      }
    }
    return true;
  }

  return true;
}

bool isArraySectionReference(SgExpression *expr) {
  std::vector<std::pair<SgExpression *, SgExpression *>> dims;
  return collectEffectiveArraySectionDimensions(expr, dims) && !dims.empty();
}

SgExpression *materializeArraySectionExpression(
    SgExpression *base_expression,
    const std::vector<std::pair<SgExpression *, SgExpression *>> &dimensions) {
  if (base_expression == nullptr) {
    return nullptr;
  }

  SgExpression *result = copyExpression(base_expression);
  for (const auto &dimension : dimensions) {
    SgExpression *lower = dimension.first != nullptr
                              ? copyExpression(dimension.first)
                              : buildNullExpression();
    SgExpression *length = dimension.second != nullptr
                               ? copyExpression(dimension.second)
                               : buildNullExpression();
    SgExpression *stride = buildIntVal(1);
    result = buildPntrArrRefExp(
        result, buildSubscriptExpression_nfi(lower, length, stride));
  }
  return result;
}

SgExpression *buildArraySectionBaseExpression(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  if (SgCastExp *cast_exp = isSgCastExp(expr)) {
    return buildArraySectionBaseExpression(cast_exp->get_operand());
  }

  if (SgUnaryOp *unary_op = isSgUnaryOp(expr)) {
    return buildArraySectionBaseExpression(unary_op->get_operand());
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(expr)) {
    if (isArraySectionReference(array_ref)) {
      return buildArraySectionBaseExpression(array_ref->get_lhs_operand());
    }
  }

  return copyExpression(expr);
}

void appendArrayDimensionsFromType(
    SgType *type,
    std::vector<std::pair<SgExpression *, SgExpression *>> &dimensions) {
  SgArrayType *array_type = isSgArrayType(stripTypeAliases(type));
  if (array_type == nullptr) {
    return;
  }

  std::vector<SgExpression *> type_dims = get_C_array_dimensions(array_type);
  for (SgExpression *dim : type_dims) {
    if (dim == nullptr || isSgNullExpression(dim) != nullptr) {
      continue;
    }
    dimensions.push_back(std::make_pair(buildIntVal(0), copyExpression(dim)));
  }
}

bool appendUniqueMapperCandidateType(std::vector<SgType *> &candidate_types,
                                     SgType *candidate_type) {
  if (candidate_type == nullptr) {
    return false;
  }

  for (SgType *existing : candidate_types) {
    if (existing == candidate_type ||
        SageInterface::isEquivalentType(existing, candidate_type)) {
      return false;
    }
  }
  candidate_types.push_back(candidate_type);
  return true;
}

SgExpression *buildEffectiveClauseItemExpression(const SgOmpClause *clause,
                                                 SgExpression *expr) {
  ROSE_ASSERT(expr != nullptr);
  if (isArraySectionReference(expr)) {
    return copyExpression(expr);
  }

  std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
      array_dimensions;
  if (clause == nullptr) {
    return copyExpression(expr);
  }
  switch (clause->variantT()) {
  case V_SgOmpMapClause:
    array_dimensions = isSgOmpMapClause(clause)->get_array_dimensions();
    break;
  case V_SgOmpToClause:
    array_dimensions = isSgOmpToClause(clause)->get_array_dimensions();
    break;
  case V_SgOmpFromClause:
    array_dimensions = isSgOmpFromClause(clause)->get_array_dimensions();
    break;
  default:
    return copyExpression(expr);
  }

  SgVariableSymbol *base_symbol = extractClauseVariableSymbol(expr);
  if (base_symbol == nullptr) {
    return copyExpression(expr);
  }

  const auto dims_iter = array_dimensions.find(base_symbol);
  if (dims_iter == array_dimensions.end() || dims_iter->second.empty()) {
    return copyExpression(expr);
  }

  return materializeArraySectionExpression(expr, dims_iter->second);
}

std::vector<SgType *> getMapperCandidateTypes(SgExpression *expr) {
  std::vector<SgType *> candidate_types;
  if (expr == nullptr) {
    return candidate_types;
  }

  appendUniqueMapperCandidateType(
      candidate_types, stripTypeAliasesAndReferences(expr->get_type()));

  std::vector<std::pair<SgExpression *, SgExpression *>> dims;
  if (collectEffectiveArraySectionDimensions(expr, dims) && !dims.empty()) {
    SgExpression *base_expr = buildArraySectionBaseExpression(expr);
    if (base_expr != nullptr) {
      SgType *base_type = stripTypeAliasesAndReferences(base_expr->get_type());
      if (SgPointerType *ptr_type = isSgPointerType(base_type)) {
        appendUniqueMapperCandidateType(
            candidate_types,
            stripTypeAliasesAndReferences(ptr_type->get_base_type()));
      } else if (SgArrayType *array_type = isSgArrayType(base_type)) {
        appendUniqueMapperCandidateType(
            candidate_types,
            stripTypeAliasesAndReferences(array_type->findBaseType()));
      }
    }
  }

  return candidate_types;
}

SgOmpClause::omp_map_operator_enum
normalizeMapperMapOperator(SgOmpClause::omp_map_operator_enum op) {
  switch (op) {
  case SgOmpClause::e_omp_map_present:
  case SgOmpClause::e_omp_map_self:
  case SgOmpClause::e_omp_map_unknown:
    return SgOmpClause::e_omp_map_tofrom;
  case SgOmpClause::e_omp_map_storage:
    return SgOmpClause::e_omp_map_alloc;
  default:
    return op;
  }
}

int runtimeFlagsFromMapClauseModifiers(const SgOmpMapClause *clause) {
  if (clause == nullptr) {
    return 0;
  }

  const SgOmpClause::omp_map_modifier_enum modifiers[] = {
      clause->get_modifier1(), clause->get_modifier2(),
      clause->get_modifier3()};
  int flags = 0;
  for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
    switch (modifier) {
    case SgOmpClause::e_omp_map_modifier_always:
      flags |= OMP_TGT_MAPTYPE_ALWAYS;
      break;
    case SgOmpClause::e_omp_map_modifier_close:
      flags |= OMP_TGT_MAPTYPE_CLOSE;
      break;
    case SgOmpClause::e_omp_map_modifier_present:
      flags |= OMP_TGT_MAPTYPE_PRESENT;
      break;
    default:
      break;
    }
  }
  return flags;
}

bool mapClauseHasModifier(const SgOmpMapClause *clause,
                          SgOmpClause::omp_map_modifier_enum modifier) {
  if (clause == nullptr) {
    return false;
  }

  return clause->get_modifier1() == modifier ||
         clause->get_modifier2() == modifier ||
         clause->get_modifier3() == modifier;
}

bool mapClauseUsesExplicitMapper(const SgOmpMapClause *clause) {
  return mapClauseHasModifier(clause, SgOmpClause::e_omp_map_modifier_mapper);
}

bool mapClauseUsesIteratorModifier(const SgOmpMapClause *clause) {
  return clause != nullptr &&
         (mapClauseHasModifier(clause,
                               SgOmpClause::e_omp_map_modifier_iterator) ||
          !clause->get_iterator().empty());
}

std::string getRequestedMapperIdentifier(const SgOmpMapClause *clause) {
  if (clause == nullptr) {
    return "";
  }
  return getMapperIdentifierText(clause->get_mapper_identifier());
}

bool motionClauseUsesExplicitMapper(const SgOmpClause *motion_clause) {
  if (const SgOmpToClause *to_clause = isSgOmpToClause(motion_clause)) {
    return to_clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper;
  }
  if (const SgOmpFromClause *from_clause = isSgOmpFromClause(motion_clause)) {
    return from_clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper;
  }
  return false;
}

bool motionClauseUsesIterator(const SgOmpClause *motion_clause) {
  if (const SgOmpToClause *to_clause = isSgOmpToClause(motion_clause)) {
    return to_clause->get_kind() == SgOmpClause::e_omp_to_kind_iterator ||
           !to_clause->get_iterator().empty();
  }
  if (const SgOmpFromClause *from_clause = isSgOmpFromClause(motion_clause)) {
    return from_clause->get_kind() == SgOmpClause::e_omp_from_kind_iterator ||
           !from_clause->get_iterator().empty();
  }
  return false;
}

std::string getRequestedMapperIdentifier(const SgOmpClause *motion_clause) {
  if (const SgOmpToClause *to_clause = isSgOmpToClause(motion_clause)) {
    return getMapperIdentifierText(to_clause->get_mapper_identifier());
  }
  if (const SgOmpFromClause *from_clause = isSgOmpFromClause(motion_clause)) {
    return getMapperIdentifierText(from_clause->get_mapper_identifier());
  }
  return "";
}

SgOmpClause::omp_map_operator_enum
decayMapperMapOperator(SgOmpClause::omp_map_operator_enum use_op,
                       SgOmpClause::omp_map_operator_enum mapper_item_op) {
  const SgOmpClause::omp_map_operator_enum normalized_use =
      normalizeMapperMapOperator(use_op);
  const SgOmpClause::omp_map_operator_enum normalized_item =
      normalizeMapperMapOperator(mapper_item_op);

  switch (normalized_use) {
  case SgOmpClause::e_omp_map_alloc:
    return normalized_item;
  case SgOmpClause::e_omp_map_to:
    switch (normalized_item) {
    case SgOmpClause::e_omp_map_alloc:
      return SgOmpClause::e_omp_map_alloc;
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
      return SgOmpClause::e_omp_map_to;
    case SgOmpClause::e_omp_map_release:
      return SgOmpClause::e_omp_map_release;
    case SgOmpClause::e_omp_map_delete:
      return SgOmpClause::e_omp_map_delete;
    default:
      break;
    }
    break;
  case SgOmpClause::e_omp_map_from:
    switch (normalized_item) {
    case SgOmpClause::e_omp_map_alloc:
      return SgOmpClause::e_omp_map_alloc;
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
      return SgOmpClause::e_omp_map_from;
    case SgOmpClause::e_omp_map_release:
      return SgOmpClause::e_omp_map_release;
    case SgOmpClause::e_omp_map_delete:
      return SgOmpClause::e_omp_map_delete;
    default:
      break;
    }
    break;
  case SgOmpClause::e_omp_map_tofrom:
    switch (normalized_item) {
    case SgOmpClause::e_omp_map_alloc:
      return SgOmpClause::e_omp_map_alloc;
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
      return SgOmpClause::e_omp_map_tofrom;
    case SgOmpClause::e_omp_map_release:
      return SgOmpClause::e_omp_map_release;
    case SgOmpClause::e_omp_map_delete:
      return SgOmpClause::e_omp_map_delete;
    default:
      break;
    }
    break;
  case SgOmpClause::e_omp_map_release:
    if (normalized_item == SgOmpClause::e_omp_map_delete) {
      return SgOmpClause::e_omp_map_delete;
    }
    return SgOmpClause::e_omp_map_release;
  case SgOmpClause::e_omp_map_delete:
    return SgOmpClause::e_omp_map_delete;
  default:
    break;
  }

  MLOG_ERROR_CXX("ompLowering")
      << "Unsupported mapper map-type decay for use operator "
      << static_cast<int>(use_op) << " and mapper operator "
      << static_cast<int>(mapper_item_op);
  ROSE_ABORT();
}

int buildRuntimeMapTypeFlags(SgOmpClause::omp_map_operator_enum op,
                             int runtime_flag_bits) {
  const SgOmpClause::omp_map_operator_enum normalized_op =
      normalizeMapperMapOperator(op);
  int flags = OMP_TGT_MAPTYPE_TARGET_PARAM | runtime_flag_bits;

  switch (normalized_op) {
  case SgOmpClause::e_omp_map_alloc:
  case SgOmpClause::e_omp_map_release:
    return flags;
  case SgOmpClause::e_omp_map_to:
    return flags | OMP_TGT_MAPTYPE_TO;
  case SgOmpClause::e_omp_map_from:
    return flags | OMP_TGT_MAPTYPE_FROM;
  case SgOmpClause::e_omp_map_tofrom:
    return flags | OMP_TGT_MAPTYPE_TO | OMP_TGT_MAPTYPE_FROM;
  case SgOmpClause::e_omp_map_delete:
    return flags | OMP_TGT_MAPTYPE_DELETE;
  default:
    break;
  }

  MLOG_ERROR_CXX("ompLowering")
      << "Unsupported mapper runtime map operator " << static_cast<int>(op);
  ROSE_ABORT();
}

bool isImplicitBasePointerMapItem(SgStatement *anchor_stmt,
                                  SgExpression *expr) {
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(anchor_stmt);
  if (target == nullptr || expr == nullptr) {
    return false;
  }

  if (!isPointerType(expr->get_type()) || isArraySectionReference(expr)) {
    return false;
  }

  SgVariableSymbol *base_symbol = extractClauseVariableSymbol(expr);
  if (base_symbol == nullptr) {
    return false;
  }

  return isImplicitTargetMapVariable(target, base_symbol);
}

SgVariableSymbol *resolveMapperMemberSymbol(SgExpression *lhs_expression,
                                            const std::string &member_name,
                                            bool is_arrow) {
  if (lhs_expression == nullptr || member_name.empty()) {
    return nullptr;
  }

  SgType *base_type = stripTypeAliasesAndReferences(lhs_expression->get_type());
  if (is_arrow) {
    SgPointerType *pointer_type = isSgPointerType(base_type);
    if (pointer_type == nullptr) {
      return nullptr;
    }
    base_type = stripTypeAliasesAndReferences(pointer_type->get_base_type());
  }

  SgClassType *class_type = isSgClassType(base_type);
  if (class_type == nullptr || class_type->get_declaration() == nullptr) {
    return nullptr;
  }

  SgClassDeclaration *class_decl =
      isSgClassDeclaration(class_type->get_declaration());
  SgClassDefinition *class_def = class_decl->get_definition();
  if (class_def == nullptr &&
      class_decl->get_definingDeclaration() != nullptr) {
    SgClassDeclaration *defining_decl =
        isSgClassDeclaration(class_decl->get_definingDeclaration());
    if (defining_decl != nullptr) {
      class_def = defining_decl->get_definition();
    }
  }
  if (class_def == nullptr) {
    return nullptr;
  }

  return class_def->lookup_variable_symbol(member_name);
}

void repairMaterializedMapperMemberAccesses(SgExpression *expr) {
  if (expr == nullptr) {
    return;
  }

  std::vector<SgExpression *> accesses;
  Rose_STL_Container<SgNode *> dots = NodeQuery::querySubTree(expr, V_SgDotExp);
  for (SgNode *node : dots) {
    accesses.push_back(isSgExpression(node));
  }
  Rose_STL_Container<SgNode *> arrows =
      NodeQuery::querySubTree(expr, V_SgArrowExp);
  for (SgNode *node : arrows) {
    accesses.push_back(isSgExpression(node));
  }

  std::sort(accesses.begin(), accesses.end(),
            [](SgExpression *lhs, SgExpression *rhs) {
              return computeAstDepth(lhs) > computeAstDepth(rhs);
            });

  for (SgExpression *access_expr : accesses) {
    if (access_expr == nullptr) {
      continue;
    }

    SgBinaryOp *access = isSgBinaryOp(access_expr);
    ROSE_ASSERT(access != nullptr);
    SgVarRefExp *rhs_ref =
        isSgVarRefExp(stripNoopCastsAndParens(access->get_rhs_operand()));
    if (rhs_ref == nullptr) {
      continue;
    }

    const bool is_arrow = isSgArrowExp(access_expr) != nullptr;
    SgVariableSymbol *member_symbol = resolveMapperMemberSymbol(
        access->get_lhs_operand(), getVarRefNameText(rhs_ref), is_arrow);
    if (member_symbol == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Failed to resolve mapper member '" << getVarRefNameText(rhs_ref)
          << "' within expression " << access_expr->unparseToString();
      ROSE_ABORT();
    }

    rhs_ref->set_symbol(member_symbol);
  }
}

bool isFormalMapperVarRef(SgExpression *expr, const std::string &formal_name) {
  SgVarRefExp *vref = isSgVarRefExp(stripNoopCastsAndParens(expr));
  return vref != nullptr && getVarRefNameText(vref) == formal_name;
}

SgExpression *materializeMapperExpression(SgExpression *template_expr,
                                          const std::string &formal_name,
                                          SgExpression *actual_expr) {
  ROSE_ASSERT(template_expr != nullptr);
  ROSE_ASSERT(actual_expr != nullptr);

  if (isFormalMapperVarRef(template_expr, formal_name)) {
    return copyExpression(actual_expr);
  }

  SgExpression *clone = copyExpression(template_expr);
  ROSE_ASSERT(clone != nullptr);

  Rose_STL_Container<SgNode *> refs =
      NodeQuery::querySubTree(clone, V_SgVarRefExp);
  std::vector<SgVarRefExp *> formal_refs;
  formal_refs.reserve(refs.size());
  for (SgNode *node : refs) {
    SgVarRefExp *vref = isSgVarRefExp(node);
    if (vref != nullptr && getVarRefNameText(vref) == formal_name) {
      formal_refs.push_back(vref);
    }
  }
  std::sort(formal_refs.begin(), formal_refs.end(),
            [](SgVarRefExp *lhs, SgVarRefExp *rhs) {
              return computeAstDepth(lhs) > computeAstDepth(rhs);
            });

  for (SgVarRefExp *formal_ref : formal_refs) {
    if (formal_ref == nullptr || formal_ref->get_parent() == nullptr) {
      continue;
    }
    replaceExpression(formal_ref, copyExpression(actual_expr));
  }

  repairMaterializedMapperMemberAccesses(clone);
  return clone;
}

bool isDirectMapperSelfItem(SgExpression *expr,
                            const std::string &formal_name) {
  return isFormalMapperVarRef(expr, formal_name);
}

ResolvedMapperInfo resolveVisibleMapperForExpression(
    SgExpression *mapped_expr, const std::string &requested_identifier,
    bool identifier_was_explicit, SgStatement *anchor_stmt) {
  ResolvedMapperInfo result;
  if (mapped_expr == nullptr || anchor_stmt == nullptr) {
    return result;
  }

  const std::string normalized_identifier =
      normalizeMapperIdentifierString(requested_identifier);
  const bool request_user_mapper = identifier_was_explicit &&
                                   !normalized_identifier.empty() &&
                                   normalized_identifier != "default";

  const std::vector<SgType *> candidate_types =
      getMapperCandidateTypes(mapped_expr);
  if (candidate_types.empty()) {
    return result;
  }

  SgStatement *anchor = anchor_stmt;
  for (SgScopeStatement *scope = getEnclosingScope(anchor_stmt);
       scope != NULL;) {
    SgStatement *anchor_child = findDirectChildStatementInScope(anchor, scope);
    const SgStatementPtrList statements = scope->generateStatementList();
    size_t scope_match_count = 0;

    for (SgStatement *stmt : statements) {
      if (stmt == nullptr) {
        continue;
      }
      if (stmt == anchor_child) {
        break;
      }

      SgOmpDeclareMapperStatement *mapper_stmt =
          isSgOmpDeclareMapperStatement(stmt);
      if (mapper_stmt == nullptr) {
        continue;
      }

      const bool mapper_is_default =
          isDefaultDeclareMapperIdentifier(mapper_stmt->get_identifier());
      std::string mapper_identifier_text = "default";
      if (!mapper_is_default) {
        mapper_identifier_text =
            getMapperIdentifierText(mapper_stmt->get_user_defined_identifier());
      }

      if (request_user_mapper) {
        if (mapper_is_default ||
            mapper_identifier_text != normalized_identifier) {
          continue;
        }
      } else if (!mapper_is_default) {
        continue;
      }

      SgType *formal_type = stripTypeAliasesAndReferences(
          getDeclareMapperFormalType(mapper_stmt));
      if (formal_type == nullptr) {
        continue;
      }

      bool type_matches = false;
      for (SgType *candidate_type : candidate_types) {
        if (candidate_type != nullptr &&
            SageInterface::isEquivalentType(formal_type, candidate_type)) {
          type_matches = true;
          break;
        }
      }
      if (!type_matches) {
        continue;
      }

      ++scope_match_count;
      if (scope_match_count > 1) {
        MLOG_ERROR_CXX("ompLowering")
            << "Ambiguous declare mapper resolution for "
            << mapped_expr->unparseToString() << " using identifier '"
            << (normalized_identifier.empty() ? "default"
                                              : normalized_identifier)
            << "' in scope " << scope->sage_class_name();
        ROSE_ABORT();
      }

      result.declaration = mapper_stmt;
      result.identifier_text = mapper_identifier_text;
      result.formal_name = getDeclareMapperFormalName(mapper_stmt);
      result.formal_type = formal_type;
    }

    if (scope_match_count > 0) {
      return result;
    }

    anchor = isSgStatement(scope);
    SgScopeStatement *next_scope = scope->get_scope();
    if (next_scope == scope) {
      break;
    }
    scope = next_scope;
  }

  return result;
}

std::vector<const ResolvedMapItem *>
getOrderedResolvedMapItems(const std::vector<ResolvedMapItem> &items) {
  std::vector<const ResolvedMapItem *> ordered_items;
  ordered_items.reserve(items.size());
  for (const ResolvedMapItem &item : items) {
    ordered_items.push_back(&item);
  }
  std::stable_sort(ordered_items.begin(), ordered_items.end(),
                   [](const ResolvedMapItem *lhs, const ResolvedMapItem *rhs) {
                     return lhs->is_implicit_base_pointer &&
                            !rhs->is_implicit_base_pointer;
                   });
  return ordered_items;
}

struct MapArgumentExpressions {
  SgExpression *mapping_expression = nullptr;
  SgExpression *mapping_base_expression = nullptr;
  SgExpression *mapping_size_expression = nullptr;
  SgExpression *mapping_type_expression = nullptr;
};

MapArgumentExpressions
buildResolvedMapItemArgumentExpressions(const ResolvedMapItem &item,
                                        SgScopeStatement *scope) {
  ROSE_ASSERT(item.expression != nullptr);
  ROSE_ASSERT(scope != nullptr);

  std::vector<std::pair<SgExpression *, SgExpression *>> dimensions;
  collectEffectiveArraySectionDimensions(item.expression, dimensions);

  SgExpression *base_expression = nullptr;
  if (!dimensions.empty()) {
    base_expression = buildArraySectionBaseExpression(item.expression);
  } else if (isSgArrayType(stripTypeAliases(item.expression->get_type())) !=
             nullptr) {
    base_expression = copyExpression(item.expression);
    appendArrayDimensionsFromType(item.expression->get_type(), dimensions);
  } else {
    base_expression = copyExpression(item.expression);
  }
  ROSE_ASSERT(base_expression != nullptr);

  MapArgumentExpressions result;
  result.mapping_base_expression = copyExpression(base_expression);
  int runtime_flag_bits = item.runtime_flag_bits;
  if (item.is_implicit_base_pointer) {
    runtime_flag_bits |= OMP_TGT_MAPTYPE_IMPLICIT;
  }

  const bool treat_as_array =
      !dimensions.empty() ||
      isSgArrayType(stripTypeAliases(base_expression->get_type())) != nullptr;
  const bool is_implicit_pointer_map =
      item.is_implicit_base_pointer && !treat_as_array &&
      isPointerType(item.expression->get_type());
  if (item.use_literal_target_param) {
    ROSE_ASSERT(item.direct_variable_symbol != nullptr);
    SgInitializedName *mapping_variable =
        item.direct_variable_symbol->get_declaration();
    ROSE_ASSERT(mapping_variable != nullptr);
    SgType *mapping_variable_type = mapping_variable->get_type();
    ROSE_ASSERT(mapping_variable_type != nullptr);

    result.mapping_expression = buildLiteralTargetParamArgExpression(
        item.direct_variable_symbol, scope);
    result.mapping_base_expression = copyExpression(result.mapping_expression);
    result.mapping_size_expression =
        buildCastExp(buildSizeOfOp(mapping_variable_type),
                     buildOpaqueType("int64_t", scope));
  } else if (treat_as_array) {
    SgType *base_type = stripTypeAliases(base_expression->get_type());
    SgType *element_type = nullptr;
    if (SgPointerType *pointer_type = isSgPointerType(base_type)) {
      element_type =
          stripTypeAliasesAndReferences(pointer_type->get_base_type());
    } else if (SgArrayType *array_type = isSgArrayType(base_type)) {
      element_type = stripTypeAliasesAndReferences(array_type->findBaseType());
    }
    if (element_type == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Unsupported mapped array base type for expression "
          << item.expression->unparseToString();
      ROSE_ABORT();
    }

    SgExpression *offset_expression = nullptr;
    SgExpression *extent_expression = nullptr;
    for (const auto &dimension : dimensions) {
      SgExpression *length = dimension.second;
      if (length == nullptr || isSgNullExpression(length) != nullptr) {
        MLOG_ERROR_CXX("ompLowering") << "Missing array-section length for "
                                      << item.expression->unparseToString();
        ROSE_ABORT();
      }

      if (offset_expression == nullptr) {
        if (dimension.first != nullptr &&
            isSgNullExpression(dimension.first) == nullptr) {
          offset_expression = copyExpression(dimension.first);
        } else {
          offset_expression = buildIntVal(0);
        }
      }

      if (extent_expression == nullptr) {
        extent_expression = copyExpression(length);
      } else {
        extent_expression =
            buildMultiplyOp(extent_expression, copyExpression(length));
      }
    }
    if (offset_expression == nullptr) {
      offset_expression = buildIntVal(0);
    }
    if (extent_expression == nullptr) {
      extent_expression = buildIntVal(1);
    }

    if (isSgIntVal(offset_expression) != nullptr &&
        isSgIntVal(offset_expression)->get_value() == 0) {
      result.mapping_expression = copyExpression(base_expression);
    } else {
      result.mapping_expression =
          buildAddOp(copyExpression(base_expression), offset_expression);
    }
    result.mapping_size_expression = buildCastExp(
        buildMultiplyOp(buildSizeOfOp(element_type), extent_expression),
        buildOpaqueType("int64_t", scope));
  } else if (is_implicit_pointer_map) {
    result.mapping_expression = copyExpression(item.expression);
    result.mapping_size_expression =
        buildCastExp(buildIntVal(0), buildOpaqueType("int64_t", scope));
  } else if (isPointerType(item.expression->get_type())) {
    result.mapping_expression = copyExpression(item.expression);
    result.mapping_size_expression =
        buildCastExp(buildSizeOfOp(item.expression->get_type()),
                     buildOpaqueType("int64_t", scope));
  } else {
    result.mapping_expression =
        buildAddressOfOp(copyExpression(item.expression));
    result.mapping_base_expression = copyExpression(result.mapping_expression);
    result.mapping_size_expression =
        buildCastExp(buildSizeOfOp(item.expression->get_type()),
                     buildOpaqueType("int64_t", scope));
  }

  if (item.use_literal_target_param) {
    int literal_flags = OMP_TGT_MAPTYPE_TARGET_PARAM | OMP_TGT_MAPTYPE_LITERAL;
    if (item.is_implicit_target_variable) {
      literal_flags |= OMP_TGT_MAPTYPE_IMPLICIT;
    }
    result.mapping_type_expression = buildIntVal(literal_flags);
  } else if (is_implicit_pointer_map) {
    result.mapping_type_expression =
        buildIntVal(OMP_TGT_MAPTYPE_TARGET_PARAM | runtime_flag_bits);
  } else {
    result.mapping_type_expression = buildIntVal(
        buildRuntimeMapTypeFlags(item.map_operator, runtime_flag_bits));
  }

  return result;
}

void appendResolvedMapItemArguments(const std::vector<ResolvedMapItem> &items,
                                    SgExprListExp *map_variable_list,
                                    SgExprListExp *map_variable_base_list,
                                    SgExprListExp *map_variable_size_list,
                                    SgExprListExp *map_variable_type_list,
                                    SgScopeStatement *scope) {
  ROSE_ASSERT(map_variable_list != nullptr);
  ROSE_ASSERT(map_variable_base_list != nullptr);
  ROSE_ASSERT(map_variable_size_list != nullptr);
  ROSE_ASSERT(map_variable_type_list != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const std::vector<const ResolvedMapItem *> ordered_items =
      getOrderedResolvedMapItems(items);
  for (const ResolvedMapItem *item_ptr : ordered_items) {
    ROSE_ASSERT(item_ptr != nullptr);
    MapArgumentExpressions expressions =
        buildResolvedMapItemArgumentExpressions(*item_ptr, scope);
    map_variable_list->append_expression(expressions.mapping_expression);
    map_variable_base_list->append_expression(
        expressions.mapping_base_expression);
    map_variable_size_list->append_expression(
        expressions.mapping_size_expression);
    map_variable_type_list->append_expression(
        expressions.mapping_type_expression);
  }
}

void collectExpandedMapEntriesForExpression(
    SgExpression *mapped_expr, const std::string &requested_identifier,
    bool identifier_was_explicit, MapperUseKind use_kind,
    SgOmpClause::omp_map_operator_enum use_map_op, int runtime_flag_bits,
    SgStatement *anchor_stmt, std::vector<ExpandedMapEntry> &items,
    std::vector<const SgOmpDeclareMapperStatement *> &active_mappers);

void collectExpandedMapEntriesUsingResolvedMapper(
    SgExpression *mapped_expr, const ResolvedMapperInfo &resolved_mapper,
    MapperUseKind use_kind, SgOmpClause::omp_map_operator_enum use_map_op,
    int runtime_flag_bits, SgStatement *anchor_stmt,
    std::vector<ExpandedMapEntry> &items,
    std::vector<const SgOmpDeclareMapperStatement *> &active_mappers) {
  ROSE_ASSERT(mapped_expr != nullptr);
  ROSE_ASSERT(anchor_stmt != nullptr);
  ROSE_ASSERT(resolved_mapper.declaration != nullptr);

  if (resolved_mapper.formal_name.empty()) {
    MLOG_ERROR_CXX("ompLowering")
        << "Declare mapper is missing a formal variable for "
        << mapped_expr->unparseToString();
    ROSE_ABORT();
  }

  if (std::find(active_mappers.begin(), active_mappers.end(),
                resolved_mapper.declaration) != active_mappers.end()) {
    MLOG_ERROR_CXX("ompLowering")
        << "Recursive declare mapper expansion detected for "
        << mapped_expr->unparseToString();
    ROSE_ABORT();
  }

  active_mappers.push_back(resolved_mapper.declaration);
  const SgOmpClausePtrList &mapper_clauses =
      resolved_mapper.declaration->get_clauses();
  for (SgOmpClause *clause : mapper_clauses) {
    SgOmpMapClause *mapper_clause = isSgOmpMapClause(clause);
    if (mapper_clause == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Unsupported non-map clause within declare mapper: "
          << clause->class_name();
      ROSE_ABORT();
    }

    const SgOmpClause::omp_map_operator_enum mapper_item_op =
        normalizeMapperMapOperator(mapper_clause->get_operation());
    const int combined_flag_bits =
        runtime_flag_bits | runtimeFlagsFromMapClauseModifiers(mapper_clause);
    const std::string nested_requested_identifier =
        getRequestedMapperIdentifier(mapper_clause);
    const bool nested_identifier_was_explicit =
        mapClauseUsesExplicitMapper(mapper_clause);
    if (mapClauseUsesIteratorModifier(mapper_clause)) {
      MLOG_ERROR_CXX("ompLowering")
          << "Iterator-based mapper expansion is not implemented for "
          << mapper_clause->unparseToString();
      ROSE_ABORT();
    }
    const SgExpressionPtrList &mapper_items =
        mapper_clause->get_variables()->get_expressions();
    for (SgExpression *mapper_item : mapper_items) {
      ROSE_ASSERT(mapper_item != nullptr);

      SgExpression *materialized_item = materializeMapperExpression(
          mapper_item, resolved_mapper.formal_name, mapped_expr);
      const bool is_direct_self_item =
          isDirectMapperSelfItem(mapper_item, resolved_mapper.formal_name);

      if (use_kind == MapperUseKind::map_clause) {
        const SgOmpClause::omp_map_operator_enum derived_op =
            decayMapperMapOperator(use_map_op, mapper_item_op);
        if (is_direct_self_item && !nested_identifier_was_explicit) {
          ResolvedMapItem direct_item;
          direct_item.expression = materialized_item;
          direct_item.map_operator = derived_op;
          direct_item.runtime_flag_bits = combined_flag_bits;
          direct_item.is_implicit_base_pointer =
              isImplicitBasePointerMapItem(anchor_stmt, materialized_item);
          direct_item.direct_variable_symbol =
              getDirectResolvedMapItemVariableSymbol(materialized_item);
          ExpandedMapEntry direct_entry;
          direct_entry.direct_item = direct_item;
          items.push_back(direct_entry);
        } else {
          collectExpandedMapEntriesForExpression(
              materialized_item, nested_requested_identifier,
              nested_identifier_was_explicit, MapperUseKind::map_clause,
              derived_op, combined_flag_bits, anchor_stmt, items,
              active_mappers);
        }
        continue;
      }

      const bool keep_for_to = mapper_item_op == SgOmpClause::e_omp_map_to ||
                               mapper_item_op == SgOmpClause::e_omp_map_tofrom;
      const bool keep_for_from =
          mapper_item_op == SgOmpClause::e_omp_map_from ||
          mapper_item_op == SgOmpClause::e_omp_map_tofrom;
      if ((use_kind == MapperUseKind::to_clause && !keep_for_to) ||
          (use_kind == MapperUseKind::from_clause && !keep_for_from)) {
        continue;
      }

      const SgOmpClause::omp_map_operator_enum update_op =
          use_kind == MapperUseKind::to_clause ? SgOmpClause::e_omp_map_to
                                               : SgOmpClause::e_omp_map_from;
      if (is_direct_self_item && !nested_identifier_was_explicit) {
        ResolvedMapItem direct_item;
        direct_item.expression = materialized_item;
        direct_item.map_operator = update_op;
        direct_item.runtime_flag_bits = combined_flag_bits;
        direct_item.is_implicit_base_pointer =
            isImplicitBasePointerMapItem(anchor_stmt, materialized_item);
        direct_item.direct_variable_symbol =
            getDirectResolvedMapItemVariableSymbol(materialized_item);
        ExpandedMapEntry direct_entry;
        direct_entry.direct_item = direct_item;
        items.push_back(direct_entry);
      } else {
        collectExpandedMapEntriesForExpression(
            materialized_item, nested_requested_identifier,
            nested_identifier_was_explicit, use_kind, update_op,
            combined_flag_bits, anchor_stmt, items, active_mappers);
      }
    }
  }
  active_mappers.pop_back();
}

void collectExpandedMapEntriesForExpression(
    SgExpression *mapped_expr, const std::string &requested_identifier,
    bool identifier_was_explicit, MapperUseKind use_kind,
    SgOmpClause::omp_map_operator_enum use_map_op, int runtime_flag_bits,
    SgStatement *anchor_stmt, std::vector<ExpandedMapEntry> &items,
    std::vector<const SgOmpDeclareMapperStatement *> &active_mappers) {
  ROSE_ASSERT(mapped_expr != nullptr);
  ROSE_ASSERT(anchor_stmt != nullptr);

  ResolvedMapperInfo resolved_mapper = resolveVisibleMapperForExpression(
      mapped_expr, requested_identifier, identifier_was_explicit, anchor_stmt);
  if (resolved_mapper.declaration == nullptr) {
    const std::string normalized_identifier =
        normalizeMapperIdentifierString(requested_identifier);
    const bool requires_user_mapper = identifier_was_explicit &&
                                      !normalized_identifier.empty() &&
                                      normalized_identifier != "default";
    if (requires_user_mapper) {
      MLOG_ERROR_CXX("ompLowering")
          << "Failed to resolve mapper '" << requested_identifier
          << "' for expression " << mapped_expr->unparseToString();
      ROSE_ABORT();
    }

    ResolvedMapItem direct_item;
    direct_item.expression = mapped_expr;
    direct_item.map_operator = use_map_op;
    direct_item.runtime_flag_bits = runtime_flag_bits;
    direct_item.is_implicit_base_pointer =
        isImplicitBasePointerMapItem(anchor_stmt, mapped_expr);
    direct_item.direct_variable_symbol =
        getDirectResolvedMapItemVariableSymbol(mapped_expr);
    ExpandedMapEntry direct_entry;
    direct_entry.direct_item = direct_item;
    items.push_back(direct_entry);
    return;
  }

  if (isArraySectionReference(mapped_expr)) {
    ExpandedMapEntry dynamic_entry;
    dynamic_entry.kind = ExpandedMapEntryKind::dynamic_mapper_section;
    dynamic_entry.resolved_mapper = resolved_mapper;
    dynamic_entry.section_base_expression =
        buildArraySectionBaseExpression(mapped_expr);
    dynamic_entry.use_kind = use_kind;
    dynamic_entry.use_map_op = use_map_op;
    dynamic_entry.runtime_flag_bits = runtime_flag_bits;
    dynamic_entry.anchor_stmt = anchor_stmt;
    collectEffectiveArraySectionDimensions(mapped_expr,
                                           dynamic_entry.section_dimensions);
    if (dynamic_entry.section_base_expression == nullptr ||
        dynamic_entry.section_dimensions.empty()) {
      MLOG_ERROR_CXX("ompLowering")
          << "Failed to materialize mapper array-section expansion for "
          << mapped_expr->unparseToString();
      ROSE_ABORT();
    }
    items.push_back(dynamic_entry);
    return;
  }

  collectExpandedMapEntriesUsingResolvedMapper(
      mapped_expr, resolved_mapper, use_kind, use_map_op, runtime_flag_bits,
      anchor_stmt, items, active_mappers);
}

bool hasDynamicExpandedMapEntries(const std::vector<ExpandedMapEntry> &items) {
  for (const ExpandedMapEntry &item : items) {
    if (item.kind == ExpandedMapEntryKind::dynamic_mapper_section) {
      return true;
    }
  }
  return false;
}

void collectDirectResolvedMapItems(const std::vector<ExpandedMapEntry> &items,
                                   std::vector<ResolvedMapItem> &direct_items) {
  for (const ExpandedMapEntry &item : items) {
    if (item.kind == ExpandedMapEntryKind::direct_item) {
      direct_items.push_back(item.direct_item);
    }
  }
}

std::vector<ExpandedMapEntry>
collectExpandedMapItemsForClause(SgStatement *anchor_stmt,
                                 const SgOmpMapClause *map_clause) {
  std::vector<ExpandedMapEntry> items;
  if (anchor_stmt == nullptr || map_clause == nullptr ||
      map_clause->get_variables() == nullptr) {
    return items;
  }

  if (mapClauseUsesIteratorModifier(map_clause)) {
    MLOG_ERROR_CXX("ompLowering")
        << "Iterator-based map clause lowering is not implemented for "
        << map_clause->unparseToString();
    ROSE_ABORT();
  }

  const std::string requested_identifier =
      getRequestedMapperIdentifier(map_clause);
  const bool identifier_was_explicit = mapClauseUsesExplicitMapper(map_clause);
  const int runtime_flag_bits = runtimeFlagsFromMapClauseModifiers(map_clause);
  std::vector<const SgOmpDeclareMapperStatement *> active_mappers;
  const SgExpressionPtrList &variables =
      map_clause->get_variables()->get_expressions();
  for (SgExpression *expr : variables) {
    if (expr == nullptr) {
      continue;
    }
    SgExpression *effective_expr =
        buildEffectiveClauseItemExpression(map_clause, expr);
    collectExpandedMapEntriesForExpression(
        effective_expr, requested_identifier, identifier_was_explicit,
        MapperUseKind::map_clause, map_clause->get_operation(),
        runtime_flag_bits, anchor_stmt, items, active_mappers);
  }
  return items;
}

std::vector<ExpandedMapEntry>
collectExpandedMotionItemsForClause(SgStatement *anchor_stmt,
                                    const SgOmpClause *motion_clause) {
  std::vector<ExpandedMapEntry> items;
  if (anchor_stmt == nullptr || motion_clause == nullptr) {
    return items;
  }

  if (motion_clause->variantT() != V_SgOmpToClause &&
      motion_clause->variantT() != V_SgOmpFromClause) {
    MLOG_ERROR_CXX("ompLowering")
        << "Unexpected motion clause in mapper expansion: "
        << motion_clause->sage_class_name();
    ROSE_ABORT();
  }

  const SgOmpVariablesClause *vars_clause =
      isSgOmpVariablesClause(motion_clause);
  if (vars_clause == nullptr || vars_clause->get_variables() == nullptr) {
    return items;
  }

  if (motionClauseUsesIterator(motion_clause)) {
    MLOG_ERROR_CXX("ompLowering")
        << "Iterator-based target update lowering is not implemented for "
        << motion_clause->unparseToString();
    ROSE_ABORT();
  }

  const std::string requested_identifier =
      getRequestedMapperIdentifier(motion_clause);
  const bool identifier_was_explicit =
      motionClauseUsesExplicitMapper(motion_clause);
  const MapperUseKind use_kind = motion_clause->variantT() == V_SgOmpToClause
                                     ? MapperUseKind::to_clause
                                     : MapperUseKind::from_clause;
  const SgOmpClause::omp_map_operator_enum use_map_op =
      use_kind == MapperUseKind::to_clause ? SgOmpClause::e_omp_map_to
                                           : SgOmpClause::e_omp_map_from;

  std::vector<const SgOmpDeclareMapperStatement *> active_mappers;
  const SgExpressionPtrList &variables =
      vars_clause->get_variables()->get_expressions();
  for (SgExpression *expr : variables) {
    if (expr == nullptr) {
      continue;
    }
    SgExpression *effective_expr =
        buildEffectiveClauseItemExpression(motion_clause, expr);
    collectExpandedMapEntriesForExpression(
        effective_expr, requested_identifier, identifier_was_explicit, use_kind,
        use_map_op, 0, anchor_stmt, items, active_mappers);
  }
  return items;
}

static void normalize_fortran_parallel_outlined_pointer_formals(
    SgFunctionDeclaration *outlined_func,
    const ASTtools::VarSymSet_t &captured_symbols) {
  if (!SageInterface::is_Fortran_language() || outlined_func == NULL)
    return;

  SgFunctionDefinition *func_def = outlined_func->get_definition();
  ROSE_ASSERT(func_def != NULL);

  std::map<std::string, SgVariableSymbol *> captured_by_name;
  for (ASTtools::VarSymSet_t::const_iterator it = captured_symbols.begin();
       it != captured_symbols.end(); ++it) {
    SgVariableSymbol *sym = const_cast<SgVariableSymbol *>(*it);
    if (sym == NULL)
      continue;
    std::string lowered_name =
        StringUtility::convertToLowerCase(sym->get_name().getString());
    if (captured_by_name.count(lowered_name) == 0)
      captured_by_name[lowered_name] = sym;
  }

  SgFunctionParameterList *params = outlined_func->get_parameterList();
  ROSE_ASSERT(params != NULL);
  SgInitializedNamePtrList &args = params->get_args();
  for (SgInitializedNamePtrList::iterator it = args.begin(); it != args.end();
       ++it) {
    SgInitializedName *arg = *it;
    if (arg == NULL)
      continue;

    const std::string name = arg->get_name().getString();
    const std::string lowered_arg_name =
        StringUtility::convertToLowerCase(name);
    if (lowered_arg_name == "__global_tid" ||
        lowered_arg_name == "__bound_tid" || lowered_arg_name == "task")
      continue;

    std::map<std::string, SgVariableSymbol *>::const_iterator captured_it =
        captured_by_name.find(lowered_arg_name);
    if (captured_it == captured_by_name.end())
      continue;

    SgType *captured_type =
        stripTypeAliasesAndReferences(captured_it->second->get_type());
    SgPointerType *captured_ptr = isSgPointerType(captured_type);
    if (captured_ptr == NULL)
      continue;

    SgType *arg_type = stripTypeAliasesAndReferences(arg->get_type());
    SgPointerType *arg_ptr = isSgPointerType(arg_type);
    if (arg_ptr == NULL)
      continue;

    SgType *arg_base_type =
        stripTypeAliasesAndReferences(arg_ptr->get_base_type());
    ROSE_ASSERT(arg_base_type != NULL);
    arg->set_type(arg_base_type);
  }

  // Keep non-defining declarations and symbol links synchronized after the
  // parameter type updates.
  SgFunctionDeclaration *nondef =
      isSgFunctionDeclaration(outlined_func->get_firstNondefiningDeclaration());
  if (nondef != NULL && nondef != outlined_func) {
    SgInitializedNamePtrList &ndef_args =
        nondef->get_parameterList()->get_args();
    if (ndef_args.size() == args.size()) {
      for (size_t i = 0; i < args.size(); ++i) {
        ROSE_ASSERT(ndef_args[i] != NULL);
        ROSE_ASSERT(args[i] != NULL);
        ndef_args[i]->set_type(args[i]->get_type());
      }
    }
  }

  // Keep function type signatures synchronized with updated formal types.
  SgFunctionType *updated_def_type = buildFunctionType(
      outlined_func->get_type()->get_return_type(),
      buildFunctionParameterTypeList(outlined_func->get_parameterList()));
  outlined_func->set_type(updated_def_type);
  if (nondef != NULL && nondef != outlined_func) {
    SgInitializedNamePtrList &ndef_args =
        nondef->get_parameterList()->get_args();
    if (ndef_args.size() == args.size()) {
      nondef->set_type(updated_def_type);
    } else {
      nondef->set_type(buildFunctionType(
          nondef->get_type()->get_return_type(),
          buildFunctionParameterTypeList(nondef->get_parameterList())));
    }
  }

  SageInterface::fixVariableReferences(func_def->get_body());
}

bool rewritePointerBasedForIndex(SgForStatement *for_loop) {
  if (for_loop == nullptr || for_loop->get_for_init_stmt() == nullptr) {
    return false;
  }

  const SgStatementPtrList &inits =
      for_loop->get_for_init_stmt()->get_init_stmt();
  if (inits.size() != 1) {
    return false;
  }

  SgExprStatement *init_stmt = isSgExprStatement(inits[0]);
  if (init_stmt == nullptr) {
    return false;
  }

  SgAssignOp *assign = isSgAssignOp(init_stmt->get_expression());
  if (assign == nullptr) {
    return false;
  }

  SgVarRefExp *pointer_ref = nullptr;
  size_t index_deref_depth = 0;
  if (!extractPointerDerefChain(assign->get_lhs_operand(), pointer_ref,
                                index_deref_depth)) {
    return false;
  }

  if (pointer_ref == nullptr || pointer_ref->get_symbol() == nullptr) {
    return false;
  }

  SgVariableSymbol *pointer_sym = pointer_ref->get_symbol();
  SgType *index_type =
      resolvePointerBaseType(pointer_sym->get_type(), index_deref_depth);
  if (index_type == nullptr) {
    return false;
  }

  static unsigned long loop_index_counter = 0;
  ++loop_index_counter;
  const std::string local_name =
      "__target_loop_index_" +
      StringUtility::numberToString(loop_index_counter);

  SgScopeStatement *scope = for_loop->get_scope();
  ROSE_ASSERT(scope != nullptr);
  SgVariableDeclaration *index_decl =
      buildVariableDeclaration(local_name, index_type, nullptr, scope);
  insertStatementBefore(for_loop, index_decl);
  SgVariableSymbol *index_sym = getFirstVarSym(index_decl);
  ROSE_ASSERT(index_sym != nullptr);

  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t derefs = NodeQuery::querySubTree(for_loop, V_SgPointerDerefExp);
  for (NodeList_t::iterator i = derefs.begin(); i != derefs.end(); ++i) {
    SgPointerDerefExp *deref = isSgPointerDerefExp(*i);
    if (deref == nullptr) {
      continue;
    }
    if (deref->get_parent() == nullptr) {
      continue;
    }
    SgVarRefExp *candidate_base = nullptr;
    size_t candidate_depth = 0;
    if (!extractPointerDerefChain(deref, candidate_base, candidate_depth)) {
      continue;
    }
    if (candidate_base->get_symbol() != pointer_sym ||
        candidate_depth != index_deref_depth) {
      continue;
    }
    replaceExpression(deref, buildVarRefExp(index_sym));
  }

  return true;
}

void rewritePointerBasedForIndices(SgForStatement *for_loop) {
  if (for_loop == nullptr) {
    return;
  }
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t loops = NodeQuery::querySubTree(for_loop, V_SgForStatement);
  for (NodeList_t::iterator i = loops.begin(); i != loops.end(); ++i) {
    rewritePointerBasedForIndex(isSgForStatement(*i));
  }
}

bool isConditionalPreprocessingDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
    return true;
  default:
    break;
  }

  std::string text = info->getString();
  const std::string::size_type first_non_space =
      text.find_first_not_of(" \t\r\n");
  if (first_non_space != std::string::npos) {
    text = text.substr(first_non_space);
  }
  if (!text.empty() && text[0] == '#') {
    if (text.rfind("#if", 0) == 0 || text.rfind("#ifdef", 0) == 0 ||
        text.rfind("#ifndef", 0) == 0 || text.rfind("#elif", 0) == 0 ||
        text.rfind("#else", 0) == 0 || text.rfind("#endif", 0) == 0) {
      return true;
    }
  }

  return false;
}

static std::string trimCopy(const std::string &input) {
  const std::string::size_type first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return std::string();
  const std::string::size_type last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

static std::string toLowerCopy(const std::string &input) {
  std::string lowered(input);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return lowered;
}

static bool isOpenMPPragmaText(const std::string &raw_text) {
  const std::string text = toLowerCopy(trimCopy(raw_text));
  if (text.empty())
    return false;

  if (text == "omp")
    return true;

  if (text.rfind("omp ", 0) == 0 || text.rfind("omp\t", 0) == 0 ||
      text.rfind("omp\n", 0) == 0 || text.rfind("omp\r", 0) == 0) {
    return true;
  }

  return false;
}

static bool isOpenMPDirectivePreprocessingInfo(const PreprocessingInfo *info) {
  if (info == nullptr)
    return false;

  const std::string text = toLowerCopy(trimCopy(info->getString()));
  if (text.empty())
    return false;

  // C/C++ pragma forms.
  if (text.rfind("#pragma omp", 0) == 0 ||
      text.rfind("// #pragma omp", 0) == 0 ||
      text.rfind("/* #pragma omp", 0) == 0) {
    return true;
  }

  // Fortran directive sentinel forms (free/fixed form).
  if (text.rfind("!$omp", 0) == 0 || text.rfind("c$omp", 0) == 0 ||
      text.rfind("*$omp", 0) == 0) {
    return true;
  }

  return false;
}

static void removeOpenMPPragmaDeclarations(SgNode *root) {
  if (root == nullptr)
    return;

  Rose_STL_Container<SgNode *> pragmas =
      NodeQuery::querySubTree(root, V_SgPragmaDeclaration);
  std::vector<SgPragmaDeclaration *> to_remove;
  for (Rose_STL_Container<SgNode *>::const_iterator it = pragmas.begin();
       it != pragmas.end(); ++it) {
    SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(*it);
    if (pragma_decl == nullptr)
      continue;
    SgPragma *pragma = pragma_decl->get_pragma();
    if (pragma == nullptr)
      continue;
    if (isOpenMPPragmaText(pragma->get_pragma()))
      to_remove.push_back(pragma_decl);
  }

  for (std::vector<SgPragmaDeclaration *>::const_iterator it =
           to_remove.begin();
       it != to_remove.end(); ++it) {
    SageInterface::removeStatement(*it, true);
  }
}

static void removeOpenMPDirectivePreprocessingInfo(SgNode *root) {
  if (root == nullptr)
    return;

  auto filter_attached = [](AttachedPreprocessingInfoType *attached) {
    if (attached == nullptr)
      return;
    attached->erase(std::remove_if(attached->begin(), attached->end(),
                                   [](PreprocessingInfo *info) {
                                     return isOpenMPDirectivePreprocessingInfo(
                                         info);
                                   }),
                    attached->end());
  };

  if (SgLocatedNode *located_root = isSgLocatedNode(root))
    filter_attached(located_root->getAttachedPreprocessingInfo());

  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    if (SgLocatedNode *located = isSgLocatedNode(*it))
      filter_attached(located->getAttachedPreprocessingInfo());
  }
}

void stripConditionalDirectivesFromList(AttachedPreprocessingInfoType &list) {
  AttachedPreprocessingInfoType filtered;
  filtered.reserve(list.size());
  for (AttachedPreprocessingInfoType::const_iterator it = list.begin();
       it != list.end(); ++it) {
    PreprocessingInfo *info = *it;
    if (info == nullptr || isConditionalPreprocessingDirective(info)) {
      continue;
    }
    filtered.push_back(info);
  }
  list.swap(filtered);
}

void stripConditionalDirectivesFromNode(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }
  if (AttachedPreprocessingInfoType *attached =
          node->getAttachedPreprocessingInfo()) {
    AttachedPreprocessingInfoType filtered;
    filtered.reserve(attached->size());
    for (AttachedPreprocessingInfoType::const_iterator it = attached->begin();
         it != attached->end(); ++it) {
      PreprocessingInfo *info = *it;
      if (info == nullptr || isConditionalPreprocessingDirective(info)) {
        continue;
      }
      filtered.push_back(info);
    }
    attached->swap(filtered);
  }
}

void rewriteCudaSiblingIncludeDirectives(
    AttachedPreprocessingInfoType *attached,
    const std::filesystem::path &source_dir) {
  if (attached == nullptr) {
    return;
  }

  for (AttachedPreprocessingInfoType::iterator it = attached->begin();
       it != attached->end(); ++it) {
    PreprocessingInfo *info = *it;
    if (info == nullptr ||
        info->getTypeOfDirective() !=
            PreprocessingInfo::CpreprocessorIncludeDeclaration) {
      continue;
    }

    const std::string include_text = info->getString();
    const std::size_t include_pos = include_text.find("#include");
    if (include_pos == std::string::npos) {
      continue;
    }

    const std::size_t quote_begin = include_text.find('"', include_pos);
    if (quote_begin == std::string::npos) {
      continue;
    }
    const std::size_t quote_end = include_text.find('"', quote_begin + 1);
    if (quote_end == std::string::npos || quote_end <= quote_begin + 1) {
      continue;
    }

    const std::string include_name =
        include_text.substr(quote_begin + 1, quote_end - quote_begin - 1);
    std::filesystem::path include_path(include_name);
    if (include_path.extension() != ".c") {
      continue;
    }

    std::filesystem::path cuda_path(include_path);
    cuda_path.replace_extension(".cu");

    std::error_code ec;
    if (!std::filesystem::exists(source_dir / cuda_path, ec) || ec) {
      continue;
    }

    info->setString(include_text.substr(0, quote_begin + 1) +
                    cuda_path.generic_string() +
                    include_text.substr(quote_end));
  }
}

void rewriteCudaSiblingIncludesInOutlinedFile(
    SgSourceFile *new_file, const std::filesystem::path &source_path) {
  if (new_file == nullptr) {
    return;
  }

  const std::filesystem::path source_dir =
      source_path.has_parent_path() ? source_path.parent_path()
                                    : std::filesystem::current_path();

  if (SgGlobal *global = new_file->get_globalScope()) {
    rewriteCudaSiblingIncludeDirectives(global->getAttachedPreprocessingInfo(),
                                        source_dir);
  }

  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(new_file, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    if (SgLocatedNode *located = isSgLocatedNode(*it)) {
      rewriteCudaSiblingIncludeDirectives(
          located->getAttachedPreprocessingInfo(), source_dir);
    }
  }
}

void stripConditionalDirectivesFromSubtree(SgNode *root) {
  if (root == nullptr) {
    return;
  }
  if (SgLocatedNode *located_root = isSgLocatedNode(root)) {
    stripConditionalDirectivesFromNode(located_root);
  }
  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    if (SgLocatedNode *located = isSgLocatedNode(*it)) {
      stripConditionalDirectivesFromNode(located);
    }
  }
}

bool isConditionalBeginDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const PreprocessingInfo::DirectiveType t = info->getTypeOfDirective();
  return t == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
         t == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
         t == PreprocessingInfo::CpreprocessorIfDeclaration;
}

bool isConditionalMiddleDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const PreprocessingInfo::DirectiveType t = info->getTypeOfDirective();
  return t == PreprocessingInfo::CpreprocessorElseDeclaration ||
         t == PreprocessingInfo::CpreprocessorElifDeclaration;
}

bool isConditionalEndDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  return info->getTypeOfDirective() ==
         PreprocessingInfo::CpreprocessorEndifDeclaration;
}

void removeUnbalancedConditionalDirectives(SgNode *root) {
  if (root == nullptr) {
    return;
  }

  std::vector<PreprocessingInfo *> ordered_infos;
  SageInterface::preOrderCollectPreprocessingInfo(root, ordered_infos, 0);

  struct ConditionalBlock {
    PreprocessingInfo *begin;
    std::vector<PreprocessingInfo *> middles;
  };

  std::vector<ConditionalBlock> stack;
  std::unordered_set<PreprocessingInfo *> to_remove;

  for (std::vector<PreprocessingInfo *>::const_iterator it =
           ordered_infos.begin();
       it != ordered_infos.end(); ++it) {
    PreprocessingInfo *info = *it;
    if (!isConditionalPreprocessingDirective(info)) {
      continue;
    }
    if (isConditionalBeginDirective(info)) {
      ConditionalBlock block;
      block.begin = info;
      stack.push_back(block);
      continue;
    }
    if (isConditionalMiddleDirective(info)) {
      if (stack.empty()) {
        to_remove.insert(info);
      } else {
        stack.back().middles.push_back(info);
      }
      continue;
    }
    if (isConditionalEndDirective(info)) {
      if (stack.empty()) {
        to_remove.insert(info);
      } else {
        stack.pop_back();
      }
    }
  }

  for (std::vector<ConditionalBlock>::const_iterator it = stack.begin();
       it != stack.end(); ++it) {
    to_remove.insert(it->begin);
    for (std::vector<PreprocessingInfo *>::const_iterator mit =
             it->middles.begin();
         mit != it->middles.end(); ++mit) {
      to_remove.insert(*mit);
    }
  }

  if (to_remove.empty()) {
    return;
  }

  if (SgLocatedNode *located_root = isSgLocatedNode(root)) {
    if (AttachedPreprocessingInfoType *attached =
            located_root->getAttachedPreprocessingInfo()) {
      attached->erase(std::remove_if(attached->begin(), attached->end(),
                                     [&](PreprocessingInfo *info) {
                                       return to_remove.count(info) != 0;
                                     }),
                      attached->end());
    }
  }

  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    SgLocatedNode *located = isSgLocatedNode(*it);
    if (located == nullptr) {
      continue;
    }
    AttachedPreprocessingInfoType *attached =
        located->getAttachedPreprocessingInfo();
    if (attached == nullptr) {
      continue;
    }
    attached->erase(std::remove_if(attached->begin(), attached->end(),
                                   [&](PreprocessingInfo *info) {
                                     return to_remove.count(info) != 0;
                                   }),
                    attached->end());
  }
}

bool declarationsMatch(const SgVariableSymbol *lhs_sym,
                       const SgInitializedName *rhs_decl) {
  if (lhs_sym == nullptr || rhs_decl == nullptr) {
    return false;
  }
  return lhs_sym->get_declaration() == rhs_decl;
}

bool recoverCanonicalForLoopControl(SgForStatement *for_loop,
                                    SgInitializedName **orig_index,
                                    SgExpression **orig_lower,
                                    SgExpression **orig_upper,
                                    SgExpression **orig_stride,
                                    bool *is_incremental) {
  if (for_loop == nullptr || orig_index == nullptr || orig_lower == nullptr ||
      orig_upper == nullptr || orig_stride == nullptr ||
      is_incremental == nullptr) {
    return false;
  }

  SgInitializedName *index_decl = nullptr;
  SgExpression *lower_expr = nullptr;

  SgStatementPtrList &init_stmts = for_loop->get_init_stmt();
  for (SgStatementPtrList::const_iterator it = init_stmts.begin();
       it != init_stmts.end(); ++it) {
    if (SgVariableDeclaration *decl = isSgVariableDeclaration(*it)) {
      if (decl->get_variables().size() != 1) {
        continue;
      }
      SgInitializedName *candidate = decl->get_variables().front();
      if (candidate == nullptr) {
        continue;
      }
      if (SgAssignInitializer *assign_init =
              isSgAssignInitializer(candidate->get_initializer())) {
        index_decl = candidate;
        lower_expr = assign_init->get_operand();
        break;
      }
      continue;
    }
    if (SgExprStatement *expr_stmt = isSgExprStatement(*it)) {
      SgAssignOp *assign =
          isSgAssignOp(stripNoopCastsAndParens(expr_stmt->get_expression()));
      if (assign == nullptr) {
        continue;
      }
      SgVarRefExp *lhs_ref =
          isSgVarRefExp(stripNoopCastsAndParens(assign->get_lhs_operand()));
      if (lhs_ref == nullptr || lhs_ref->get_symbol() == nullptr) {
        continue;
      }
      index_decl = lhs_ref->get_symbol()->get_declaration();
      lower_expr = assign->get_rhs_operand();
      break;
    }
  }

  if (index_decl == nullptr || lower_expr == nullptr) {
    return false;
  }

  SgBinaryOp *test_expr = isSgBinaryOp(for_loop->get_test_expr());
  if (test_expr == nullptr) {
    return false;
  }
  switch (test_expr->variantT()) {
  case V_SgLessOrEqualOp:
  case V_SgLessThanOp:
    *is_incremental = true;
    break;
  case V_SgGreaterOrEqualOp:
  case V_SgGreaterThanOp:
    *is_incremental = false;
    break;
  default:
    return false;
  }
  SgVarRefExp *test_lhs =
      isSgVarRefExp(stripNoopCastsAndParens(test_expr->get_lhs_operand()));
  if (test_lhs == nullptr ||
      !declarationsMatch(test_lhs->get_symbol(), index_decl)) {
    return false;
  }

  SgExpression *stride_expr = nullptr;
  SgExpression *incr_expr = for_loop->get_increment();
  if (incr_expr == nullptr) {
    return false;
  }
  if (SgPlusAssignOp *plus_assign = isSgPlusAssignOp(incr_expr)) {
    SgVarRefExp *lhs_ref =
        isSgVarRefExp(stripNoopCastsAndParens(plus_assign->get_lhs_operand()));
    if (lhs_ref == nullptr ||
        !declarationsMatch(lhs_ref->get_symbol(), index_decl)) {
      return false;
    }
    stride_expr = plus_assign->get_rhs_operand();
  } else if (SgMinusAssignOp *minus_assign = isSgMinusAssignOp(incr_expr)) {
    SgVarRefExp *lhs_ref =
        isSgVarRefExp(stripNoopCastsAndParens(minus_assign->get_lhs_operand()));
    if (lhs_ref == nullptr ||
        !declarationsMatch(lhs_ref->get_symbol(), index_decl)) {
      return false;
    }
    stride_expr = minus_assign->get_rhs_operand();
  } else if (SgPlusPlusOp *plusplus = isSgPlusPlusOp(incr_expr)) {
    SgVarRefExp *operand =
        isSgVarRefExp(stripNoopCastsAndParens(plusplus->get_operand()));
    if (operand == nullptr ||
        !declarationsMatch(operand->get_symbol(), index_decl)) {
      return false;
    }
    stride_expr = buildIntVal(1);
  } else if (SgMinusMinusOp *minusminus = isSgMinusMinusOp(incr_expr)) {
    SgVarRefExp *operand =
        isSgVarRefExp(stripNoopCastsAndParens(minusminus->get_operand()));
    if (operand == nullptr ||
        !declarationsMatch(operand->get_symbol(), index_decl)) {
      return false;
    }
    stride_expr = buildIntVal(1);
  } else {
    return false;
  }

  if (stride_expr == nullptr) {
    return false;
  }

  *orig_index = index_decl;
  *orig_lower = lower_expr;
  *orig_upper = test_expr->get_rhs_operand();
  *orig_stride = stride_expr;
  return true;
}

void prependGlobalDeclPreservingLeadingPreproc(SgStatement *decl,
                                               SgGlobal *global_scope) {
  ROSE_ASSERT(decl != nullptr);
  ROSE_ASSERT(global_scope != nullptr);

  if (SgStatement *first_stmt = SageInterface::getFirstStatement(global_scope);
      first_stmt != nullptr) {
    SageInterface::insertStatementBefore(first_stmt, decl,
                                         /*autoMovePreprocessingInfo=*/true);
  } else {
    SageInterface::prependStatement(decl, global_scope);
  }
}

bool hasTargetOffloadConstructs(SgSourceFile *file) {
  ROSE_ASSERT(file != nullptr);

  static const VariantT target_variants[] = {
      V_SgOmpTargetStatement,
      V_SgOmpTargetTeamsStatement,
      V_SgOmpTargetParallelStatement,
      V_SgOmpTargetDataStatement,
      V_SgOmpTargetUpdateStatement,
      V_SgOmpTargetTeamsDistributeStatement,
      V_SgOmpTargetParallelForStatement,
      V_SgOmpTargetTeamsDistributeParallelForStatement,
  };

  for (VariantT variant : target_variants) {
    if (!NodeQuery::querySubTree(file, variant).empty()) {
      return true;
    }
  }
  return false;
}

bool hasOpenMPRuntimeConstructs(SgSourceFile *file) {
  ROSE_ASSERT(file != nullptr);

  Rose_STL_Container<SgNode *> omp_nodes =
      NodeQuery::querySubTree(file, V_SgOmpExecStatement);
  omp_nodes = mergeSgNodeList(
      omp_nodes, NodeQuery::querySubTree(file, V_SgOmpThreadprivateStatement));

  return !omp_nodes.empty();
}
} // namespace

size_t get_host_pointer_size_bytes(const SgNode *context) {
  return is_32_bit_target(context) ? 4 : 8;
}

bool canUseLiteralTargetParam(const SgOmpClauseBodyStatement *target,
                              SgVariableSymbol *var_sym,
                              SgOmpClause::omp_map_operator_enum map_operator) {
  if (target == NULL || var_sym == NULL) {
    return false;
  }

  SgType *type = stripTypeAliasesAndReferences(var_sym->get_type());
  if (type == NULL || !SageInterface::isScalarType(type) ||
      isPointerType(type) || isSgTypeLongDouble(type) != NULL) {
    return false;
  }

  const bool is_implicit = isImplicitTargetMapVariable(target, var_sym);
  const SgOmpClause::omp_map_operator_enum normalized_op =
      normalizeMapperMapOperator(map_operator);
  const bool need_copy_from = normalized_op == SgOmpClause::e_omp_map_from ||
                              normalized_op == SgOmpClause::e_omp_map_tofrom;
  if (need_copy_from && !is_implicit) {
    return false;
  }

  return get_target_type_size_bytes(type, target) <=
         get_host_pointer_size_bytes(target);
}

static bool isLiteralTargetParamPackCall(const SgExpression *expr) {
  const SgFunctionCallExp *call = isSgFunctionCallExp(expr);
  if (call == NULL) {
    return false;
  }

  const SgFunctionRefExp *callee = isSgFunctionRefExp(call->get_function());
  if (callee == NULL || callee->get_symbol() == NULL) {
    return false;
  }

  return callee->get_symbol()->get_name().getString() ==
         "rex_pack_literal_arg_bytes";
}

static void materializeLiteralTargetArgExpressions(
    SgExprListExp *map_variable_list, SgExprListExp *map_variable_base_list,
    SgBasicBlock *outlined_driver_body, SgScopeStatement *scope) {
  ROSE_ASSERT(map_variable_list != NULL);
  ROSE_ASSERT(map_variable_base_list != NULL);
  ROSE_ASSERT(outlined_driver_body != NULL);
  ROSE_ASSERT(scope != NULL);

  SgExpressionPtrList &arg_exprs = map_variable_list->get_expressions();
  SgExpressionPtrList &base_exprs = map_variable_base_list->get_expressions();
  ROSE_ASSERT(arg_exprs.size() == base_exprs.size());

  int literal_arg_counter = 0;
  for (size_t i = 0; i < arg_exprs.size(); ++i) {
    SgExpression *packed_expr = nullptr;
    if (isLiteralTargetParamPackCall(arg_exprs[i])) {
      packed_expr = arg_exprs[i];
    } else if (isLiteralTargetParamPackCall(base_exprs[i])) {
      packed_expr = base_exprs[i];
    }

    if (packed_expr == nullptr) {
      continue;
    }

    const std::string packed_name =
        "__rex_packed_literal_arg_" + std::to_string(literal_arg_counter++);
    SgVariableDeclaration *packed_decl = buildVariableDeclaration(
        packed_name, buildPointerType(buildVoidType()),
        buildAssignInitializer(copyExpression(packed_expr)), scope);
    outlined_driver_body->append_statement(packed_decl);
    SgVariableSymbol *packed_sym = getFirstVarSym(packed_decl);
    ROSE_ASSERT(packed_sym != NULL);

    arg_exprs[i] = buildVarRefExp(packed_sym);
    arg_exprs[i]->set_parent(map_variable_list);
    base_exprs[i] = buildVarRefExp(packed_sym);
    base_exprs[i]->set_parent(map_variable_base_list);
  }
}

static void
lowerLiteralTargetKernelParameters(SgFunctionDeclaration *outlined_func,
                                   const ASTtools::VarSymSet_t &literal_syms) {
  ROSE_ASSERT(outlined_func != NULL);

  SgFunctionDefinition *func_def = outlined_func->get_definition();
  ROSE_ASSERT(func_def != NULL);
  SgBasicBlock *body = func_def->get_body();
  ROSE_ASSERT(body != NULL);

  // LLVM's __tgt_target_kernel ABI prepends a hidden kernel-launch-environment
  // parameter even for bare kernels. Add it explicitly so REX-generated CUDA
  // kernels match the runtime's argument layout.
  SgFunctionParameterList *params = outlined_func->get_parameterList();
  ROSE_ASSERT(params != NULL);
  SgFunctionDeclaration *nondef_decl =
      isSgFunctionDeclaration(outlined_func->get_firstNondefiningDeclaration());
  if (params->get_args().empty() ||
      params->get_args().front()->get_name().getString() !=
          "__rex_kernel_launch_env") {
    SgInitializedName *kernel_launch_env_param =
        SageBuilder::buildInitializedName("__rex_kernel_launch_env",
                                          buildPointerType(buildVoidType()));
    setOneSourcePositionForTransformation(kernel_launch_env_param);
    prependArg(params, kernel_launch_env_param);

    outlined_func->set_type(buildFunctionType(
        outlined_func->get_type()->get_return_type(),
        buildFunctionParameterTypeList(outlined_func->get_parameterList())));

    if (nondef_decl != NULL) {
      nondef_decl->set_type(outlined_func->get_type());
    }
  }

  if (literal_syms.empty()) {
    return;
  }

  std::set<std::string> literal_param_names;
  for (ASTtools::VarSymSet_t::const_iterator it = literal_syms.begin();
       it != literal_syms.end(); ++it) {
    const SgVariableSymbol *var_sym = *it;
    if (var_sym == NULL) {
      continue;
    }
    literal_param_names.insert(var_sym->get_name().getString());
  }

  std::vector<SgStatement *> original_body_stmts = body->get_statements();
  SgInitializedNamePtrList &param_args = params->get_args();
  for (SgInitializedNamePtrList::iterator it = param_args.begin();
       it != param_args.end(); ++it) {
    SgInitializedName *param = *it;
    if (param == NULL) {
      continue;
    }

    if (literal_param_names.find(param->get_name().getString()) ==
        literal_param_names.end()) {
      continue;
    }

    SgType *original_type = stripTypeAliasesAndReferences(param->get_type());
    ROSE_ASSERT(original_type != NULL);

    SgVariableSymbol *param_sym =
        isSgVariableSymbol(param->get_symbol_from_symbol_table());
    ROSE_ASSERT(param_sym != NULL);

    SgType *transport_type =
        get_host_pointer_size_bytes(body) <= 4
            ? static_cast<SgType *>(buildUnsignedIntType())
            : static_cast<SgType *>(buildUnsignedLongLongType());
    param->set_type(transport_type);

    if (nondef_decl != NULL && nondef_decl != outlined_func) {
      SgInitializedNamePtrList &nondef_args =
          nondef_decl->get_parameterList()->get_args();
      for (SgInitializedNamePtrList::iterator nondef_it = nondef_args.begin();
           nondef_it != nondef_args.end(); ++nondef_it) {
        SgInitializedName *nondef_param = *nondef_it;
        if (nondef_param == NULL) {
          continue;
        }
        if (nondef_param->get_name() == param->get_name()) {
          nondef_param->set_type(transport_type);
          break;
        }
      }
    }

    outlined_func->set_type(buildFunctionType(
        outlined_func->get_type()->get_return_type(),
        buildFunctionParameterTypeList(outlined_func->get_parameterList())));
    if (nondef_decl != NULL) {
      nondef_decl->set_type(outlined_func->get_type());
    }

    std::string shadow_name = param->get_name().getString() + "__rex_value";
    SgVariableDeclaration *shadow_decl =
        buildVariableDeclaration(shadow_name, original_type, NULL, body);
    prependStatement(shadow_decl, body);
    SgVariableSymbol *shadow_sym = getFirstVarSym(shadow_decl);
    ROSE_ASSERT(shadow_sym != NULL);

    SgExprListExp *memcpy_args =
        buildExprListExp(buildAddressOfOp(buildVarRefExp(shadow_sym)),
                         buildAddressOfOp(buildVarRefExp(param_sym)),
                         buildSizeOfOp(original_type));
    SgExprStatement *memcpy_stmt = buildFunctionCallStmt(
        "__builtin_memcpy", buildPointerType(buildVoidType()), memcpy_args,
        body);
    insertStatementAfter(shadow_decl, memcpy_stmt);

    for (std::vector<SgStatement *>::const_iterator stmt_it =
             original_body_stmts.begin();
         stmt_it != original_body_stmts.end(); ++stmt_it) {
      SgStatement *stmt = *stmt_it;
      if (stmt == NULL) {
        continue;
      }
      Rose_STL_Container<SgNode *> refs =
          NodeQuery::querySubTree(stmt, V_SgVarRefExp);
      for (Rose_STL_Container<SgNode *>::const_iterator ref_it = refs.begin();
           ref_it != refs.end(); ++ref_it) {
        SgVarRefExp *ref = isSgVarRefExp(*ref_it);
        if (ref == NULL || ref->get_symbol() == NULL) {
          continue;
        }
        if (ref->get_symbol()->get_name() == param->get_name()) {
          ref->set_symbol(shadow_sym);
        }
      }
    }
  }
}

static void
maybeRecordTargetKernelLaunchBounds(SgFunctionDeclaration *outlined_func,
                                    SgExpression *omp_num_threads) {
  if (outlined_func == NULL || omp_num_threads == NULL) {
    return;
  }

  std::string launch_bounds_expr = trimCopy(omp_num_threads->unparseToString());
  if (launch_bounds_expr.empty()) {
    return;
  }

  const const_int_expr_t const_eval =
      SageInterface::evaluateConstIntegerExpression(omp_num_threads);
  if (!const_eval.hasValue_) {
    class LaunchBoundsExprValidator : public AstSimpleProcessing {
    public:
      explicit LaunchBoundsExprValidator(const SgExpression *root_expr)
          : root_expr_(root_expr), is_valid_(true) {}

      bool isValid() const { return is_valid_; }

      void visit(SgNode *node) override {
        if (!is_valid_ || node == NULL) {
          return;
        }

        if (isSgValueExp(node) != NULL || isSgExprListExp(node) != NULL ||
            isSgCastExp(node) != NULL || isSgAddOp(node) != NULL ||
            isSgSubtractOp(node) != NULL || isSgMultiplyOp(node) != NULL ||
            isSgDivideOp(node) != NULL || isSgModOp(node) != NULL ||
            isSgBitAndOp(node) != NULL || isSgBitOrOp(node) != NULL ||
            isSgBitXorOp(node) != NULL || isSgLshiftOp(node) != NULL ||
            isSgRshiftOp(node) != NULL || isSgUnaryAddOp(node) != NULL ||
            isSgMinusOp(node) != NULL || isSgBitComplementOp(node) != NULL ||
            isSgNotOp(node) != NULL || isSgConditionalExp(node) != NULL) {
          return;
        }

        if (SgVarRefExp *var_ref = isSgVarRefExp(node)) {
          SgVariableSymbol *sym = isSgVariableSymbol(var_ref->get_symbol());
          SgInitializedName *decl = sym != NULL ? sym->get_declaration() : NULL;
          if (decl == NULL || decl->get_name().is_null()) {
            is_valid_ = false;
            return;
          }

          const bool is_macro_placeholder =
              decl->get_file_info() != NULL &&
              decl->get_file_info()->get_filenameString() == "transformation" &&
              decl->get_initializer() == NULL;
          if (!is_macro_placeholder) {
            is_valid_ = false;
          }
          return;
        }

        if (isSgExpression(node) != NULL && node != root_expr_) {
          is_valid_ = false;
        }
      }

    private:
      const SgExpression *root_expr_;
      bool is_valid_;
    };

    LaunchBoundsExprValidator validator(omp_num_threads);
    validator.traverse(omp_num_threads, preorder);
    if (!validator.isValid()) {
      return;
    }
  }

  addTextForUnparser(outlined_func,
                     "__launch_bounds__(" + launch_bounds_expr + ") ",
                     AstUnparseAttribute::e_before_syntax);
}

static int computeMaxNestedForDepth(SgNode *node) {
  if (node == NULL) {
    return 0;
  }

  int best_depth = 0;
  if (SgForStatement *for_stmt = isSgForStatement(node)) {
    best_depth = 1 + computeMaxNestedForDepth(for_stmt->get_loop_body());
  }

  SgNodePtrList children = node->get_traversalSuccessorContainer();
  for (SgNodePtrList::const_iterator child_it = children.begin();
       child_it != children.end(); ++child_it) {
    best_depth = std::max(best_depth, computeMaxNestedForDepth(*child_it));
  }

  return best_depth;
}

std::map<SgOmpExecStatement *, std::map<SgInitializedName *, SgExpression *> *>
    clause_variable_renaming_record;

// Liao 1/23/2015
// when translating mapped variables using
// xomp_deviceDataEnvironmentPrepareVariable(), the original variable reference
// will be used as a parameter. However, later
// replaceVariablesWithPointerDereference () will find it and replace it with a
// device version reference, which is not desired. In order to avoid this, we
// keep track of these few references to the original Host CPU side variables
// and don't replace them later on. This may not be elegant, but let's get
// something working first.
static set<SgVarRefExp *> preservedHostVarRefs;

static SgVariableDeclaration *get_kmpc_global_tid(SgNode *, SgScopeStatement *,
                                                  SgStatement **);
static void insert_function_parameter(std::string, SgType *,
                                      SgFunctionDeclaration *, bool);
static void ensure_fortran_variable_declaration(SgBasicBlock *, const SgName &,
                                                SgType *);
static void insert_fortran_statement_in_specification_part(SgStatement *,
                                                           SgBasicBlock *);
static void insert_fortran_declaration_into_procedure(SgVariableDeclaration *,
                                                      SgScopeStatement *);
static void normalize_fortran_external_subroutine_declarations(SgBasicBlock *);
static void normalize_fortran_if_statements(SgSourceFile *);
// move the outlined function to a separate file

static SgFunctionDeclaration *move_outlined_function(SgFunctionDeclaration *,
                                                     SgSourceFile *);
std::vector<SgFunctionDeclaration *> *outlined_function_list = NULL;
std::vector<SgDeclarationStatement *> *outlined_struct_list = NULL;

std::vector<SgFunctionDeclaration *> *target_outlined_function_list = NULL;
std::vector<SgDeclarationStatement *> *target_outlined_struct_list = NULL;
static void post_processing(SgSourceFile *);
static SgSourceFile *generate_outlined_function_file(SgFunctionDeclaration *,
                                                     std::string);
static void fix_storage_modifier(SgSourceFile *);
static unsigned int kmpc_global_tid_counter = 0;
static unsigned int kmpc_kernel_id_counter = 0;

static SgSourceFile *cpu_outlined_file = NULL;

#define ENABLE_XOMP                                                            \
  1 // Enable the middle layer (XOMP) of OpenMP runtime libraries
//! Generate a symbol set from an initialized name list,
// filter out struct/class typed names
static void convertAndFilter(const SgInitializedNamePtrList input,
                             ASTtools::VarSymSet_t &output) {
  for (SgInitializedNamePtrList::const_iterator iter = input.begin();
       iter != input.end(); iter++) {
    const SgInitializedName *iname = *iter;
    SgVariableSymbol *symbol =
        isSgVariableSymbol(iname->get_symbol_from_symbol_table());
    ROSE_ASSERT(symbol != NULL);
    if (!isSgClassType(symbol->get_type()))
      output.insert(symbol);
  }
}

namespace OmpSupport {
bool enable_accelerator = false; /* default is to not recognize and lowering
                                    OpenMP accelerator directives */
bool enable_debugging = false;   /* default is not to debug the process */

// A flag to control if device data environment runtime functions are used to
// automatically manage data as much as possible. instead of generating explicit
// data allocation, copy, free functions.
bool useDDE = true;

unsigned int nCounter = 0;

struct GpuOffloadLoweringContext {
  std::map<SgVariableSymbol *, int> per_block_reduction_map;
  std::vector<SgVariableDeclaration *> per_block_declarations;
  ASTtools::VarSymSet_t literal_target_param_syms;
};

static void
transOmpVariablesWithContext(SgStatement *ompStmt, SgBasicBlock *bb1,
                             SgExpression *orig_loop_upper = NULL,
                             bool isAcceleratorModel = false,
                             GpuOffloadLoweringContext *offload_ctx = NULL);

void markImplicitTargetMapVariable(SgOmpClauseBodyStatement *target,
                                   SgInitializedName *var) {
  if (target == NULL || var == NULL) {
    return;
  }
  implicit_target_map_variables[target].insert(var);
}

bool isImplicitTargetMapVariable(const SgOmpClauseBodyStatement *target,
                                 const SgSymbol *sym) {
  if (target == NULL || sym == NULL) {
    return false;
  }

  std::map<const SgOmpClauseBodyStatement *,
           std::set<const SgInitializedName *>>::const_iterator map_iter =
      implicit_target_map_variables.find(target);
  if (map_iter == implicit_target_map_variables.end()) {
    return false;
  }

  const SgVariableSymbol *var_sym =
      isSgVariableSymbol(const_cast<SgSymbol *>(sym));
  if (var_sym == NULL) {
    return false;
  }

  return map_iter->second.find(var_sym->get_declaration()) !=
         map_iter->second.end();
}

void clearImplicitTargetMapVariables() {
  implicit_target_map_variables.clear();
}
//------------------------------------
// Add include "xxxx.h" into source files, right before the first statement from
// users Lazy approach: assume all files will contain OpenMP runtime library
// calls
// TODO: (low priority) a better way is to only insert Headers when OpenMP is
// used. 2/1/2008, try to use MiddleLevelRewrite to parse the content of the
// header, which
//  should generate function symbols used for runtime function calls
//  But it is not stable!

//! This makeDataSharingExplicit() is added by Hongyi on July/23/2012.
//! Consider private, firstprivate, lastprivate, shared, reduction  is it
//! correct?@Leo
// TODO: consider the initialized name of variable in function call or
// definitions

/** Algorithm for patchUpSharedVariables edited by Hongyi Ma on August 7th 2012
 *   1. find all variables references in  parallel region
 *   2. find all variable declarations in this parallel region
 *   3. check whether these variables has been in private or shared clause
 * already
 *   4. if not, add them into shared clause
 */

//! function prototypes for  patch up shared variables

/*    Get name of varrefexp  */
string getName(SgNode *n) {
  string name;
  SgVarRefExp *var = isSgVarRefExp(n);
  if (var)
    name = var->get_symbol()->get_name().getString();

  return name;
}

/*    Remove duplicate list entries  */
void getUnique(Rose_STL_Container<SgNode *> &list) {
  Rose_STL_Container<SgNode *>::iterator start = list.begin();
  unsigned int size = list.size();
  unsigned int i, j;

  if (size > 1) {
    for (i = 0; i < size - 1; i++) {
      j = i + 1;
      while (j < size) {
        SgVarRefExp *iis = isSgVarRefExp(list.at(i));
        SgVarRefExp *jjs = isSgVarRefExp(list.at(j));

        SgInitializedName *is =
            isSgInitializedName(iis->get_symbol()->get_declaration());
        SgInitializedName *js =
            isSgInitializedName(jjs->get_symbol()->get_declaration());
        if (is == js) {
          list.erase(start + j);
          size--;
          continue;
        }

        j++;
      }
    }
  }
}
/* the end of getUnique name */

/* gather varaible references from remaining expressions */

void gatherReferences(const Rose_STL_Container<SgNode *> &expr,
                      Rose_STL_Container<SgNode *> &vars) {
  Rose_STL_Container<SgNode *>::const_iterator iter = expr.begin();

  while (iter != expr.end()) {

    Rose_STL_Container<SgNode *> tempList =
        NodeQuery::querySubTree(*iter, V_SgVarRefExp);

    Rose_STL_Container<SgNode *>::iterator ti = tempList.begin();
    while (ti != tempList.end()) {
      vars.push_back(*ti);
      ti++;
    }
    iter++;
  }
  /* then remove the duplicate variables */
  getUnique(vars);
}
/* the end of gatherReferences function*/

// Check if a variable is explicitly specified by clauses of
// omp_clause_body_stmt. Return e_unknown if not.
static omp_construct_enum getExplicitDataSharingAttribute(
    SgInitializedName *iname, SgOmpClauseBodyStatement *omp_clause_body_stmt) {
  ROSE_ASSERT(iname != NULL);
  ROSE_ASSERT(omp_clause_body_stmt != NULL);

  omp_construct_enum rt_val = e_unknown;
  if (isInClauseVariableList(iname, omp_clause_body_stmt,
                             V_SgOmpPrivateClause)) {
    rt_val = e_private;
  } else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                    V_SgOmpSharedClause)) {
    rt_val = e_shared;
  } else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                    V_SgOmpReductionClause)) {
    rt_val = e_reduction;
  }

  else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                  V_SgOmpCopyinClause)) {
    rt_val = e_copyin;
  } else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                    V_SgOmpCopyprivateClause)) {
    rt_val = e_copyprivate;
  } else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                    V_SgOmpFirstprivateClause)) {
    rt_val = e_firstprivate;
  } else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                    V_SgOmpLastprivateClause)) {
    rt_val = e_lastprivate;
  } else if (isInClauseVariableList(iname, omp_clause_body_stmt,
                                    V_SgOmpMapClause)) {
    rt_val = e_map;
  }

  return rt_val;
}

//! Check if a variable access is a shared access , assuming it is already
//! within an OpenMP region.
bool isSharedAccess(SgVarRefExp *varRef) {
  return (getDataSharingAttribute(varRef) == e_shared);
}

omp_construct_enum getDataSharingAttribute(SgVarRefExp *varRef) {
  ROSE_ASSERT(varRef != NULL);
  SgSymbol *s = varRef->get_symbol();
  return getDataSharingAttribute(s, varRef);
}

// TODO: expose to header
// From collapse(Integer), find all affected for loops of a 'omp for' or 'omp
// simd' directive In this case, normalizing combined constructs like 'parallel
// for' is convenient, less directives to consider.
vector<SgForStatement *>
getAffectedForLoops(SgOmpClauseBodyStatement *forOrSimd) {
  vector<SgForStatement *> loops;
  ROSE_ASSERT(forOrSimd != NULL);
  int loop_count = 1; // by default, only one loop is affected.
  SgExpression *exp = getClauseExpression(forOrSimd, V_SgOmpCollapseClause);
  SgExpression *exp_ordered =
      getClauseExpression(forOrSimd, V_SgOmpOrderedClause);
  if (exp != NULL) {
    SgIntVal *ival = isSgIntVal(exp);
    if (ival == NULL) {
      cerr << "Error. Expecting SgIntVal of Collapse(exp), seeing "
           << exp->class_name() << " instead." << endl;
      ROSE_ABORT();
    }
    loop_count = ival->get_value();
  } else if (exp_ordered != NULL) {
    SgIntVal *ival = isSgIntVal(exp_ordered);
    if (ival == NULL) // ordered clause may have no expression specified at all.
                      // default to 1 loop affected.
      loop_count = 1;
    else
      loop_count = ival->get_value();
  }
  // TODO: what if both ordered() and collapse() appear??

  // Now obtain all loops within forOrSimd, up to loop_count
  RoseAst ast(forOrSimd);
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    if (loop_count == 0)
      break;
    if (SgForStatement *fs = isSgForStatement(*i)) {
      loops.push_back(fs);
      loop_count--;
    }
  }
  return loops;
}

// TODO: expose to header
vector<SgInitializedName *>
getAffectedForLoopIndexVars(SgOmpClauseBodyStatement *forOrSimd) {
  vector<SgInitializedName *> result;
  // use a map to cache results, avoid repetitive analysis of OpenMP regions
  static map<SgOmpClauseBodyStatement *, vector<SgInitializedName *>>
      Region2Index;
  static map<SgOmpClauseBodyStatement *, bool> RegionAnalyzed;

  if (!RegionAnalyzed[forOrSimd]) {
    RegionAnalyzed[forOrSimd] = true;
    vector<SgForStatement *> loops = getAffectedForLoops(forOrSimd);
    for (size_t i = 0; i < loops.size(); i++)
      result.push_back(getLoopIndexVariable(loops[i]));
    Region2Index[forOrSimd] = result;
  } else
    result = Region2Index[forOrSimd];

  return result;
}

// TODO: expose to header
// Check if a variable is a loop index variable of a loop affected by OpenMP for
// or simd directives.
bool isAffectedForLoopIndexVariable(SgOmpClauseBodyStatement *forOrSimd,
                                    SgInitializedName *iname) {
  vector<SgInitializedName *> loopIndexVars =
      getAffectedForLoopIndexVars(forOrSimd);
  vector<SgInitializedName *>::iterator where =
      find(loopIndexVars.begin(), loopIndexVars.end(), iname);
  return (where != loopIndexVars.end());
}

//! Return the data sharing attribute type of a variable within a context node
//! (anchor_stmt indicates the start search location within AST) Possible values
//! include: e_shared, e_private,  e_firstprivate,  e_lastprivate,  e_reduction,
//! e_threadprivate, e_copyin, and e_copyprivate.
// The rules are defined in OpenMP 4.5 specification,  page 179,
//    2.15.1 Data-sharing Attribute Rules
omp_construct_enum getDataSharingAttribute(SgSymbol *sym, SgNode *anchor_node) {
  omp_construct_enum rt_val = e_shared; // shared by default for now
  // TODO: if default() is present, we have to change this.
  ROSE_ASSERT(sym != NULL);
  ROSE_ASSERT(anchor_node != NULL);
  SgStatement *anchor_stmt = getEnclosingStatement(anchor_node);
  ROSE_ASSERT(anchor_stmt != NULL);

  SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
  ROSE_ASSERT(var_sym != NULL);

  SgInitializedName *iname = isSgInitializedName(var_sym->get_declaration());
  // TODO: what to do with SgOmpWorkshareStatement ?  it is a
  // region/SgOmpBodyStatement, but it does not belong to OmpClauseBodyStatement

  // obtain the enclosing OpenMP clause body statement: SgOmpForStatement,
  // parallel, sections, single, target, target data, task, etc.
  // TODO: this may not be reliable:  region {stmtlist ;  loop; stmtlist; }
  SgOmpClauseBodyStatement *omp_clause_body_stmt =
      findEnclosingOmpClauseBodyStatement(anchor_stmt);

  if (omp_clause_body_stmt != NULL) {
    omp_construct_enum temp_val =
        getExplicitDataSharingAttribute(iname, omp_clause_body_stmt);
    // We assume the input code is correct. So all predetermined variables
    // listed in clauses are conforming to the spec.
    if (temp_val != e_unknown) {
      rt_val = temp_val;
      return rt_val; // use direct return to avoid messy if-else logic
    }
    // not explicitly specified, using the rules for predetermined and
    // implicitly determined
    else {
      // Not in explicit data-sharing attribute clause at this level,

      // Apply implicit rules :
      // check if it is locally declared  (the declaration is inside of the
      // omp_clause_body_stmt )
      SgVariableDeclaration *var_decl =
          isSgVariableDeclaration(iname->get_declaration());
      // ROSE_ASSERT (var_decl != NULL);
      // it could also be SgFunctionParameterList or other declarations
      // if declared at function parameters, the scope is outside, it should be
      // shared by default if no other rules apply.
      if (var_decl && isAncestor(omp_clause_body_stmt, var_decl)) {
        // declared in a scope inside the construct:
        // Variables with automatic storage duration are private
        // Variables with static storage duration are shared.
        if (isStatic(var_decl))
          rt_val = e_shared;
        else
          rt_val = e_private;
        return rt_val;
      }

      if (isThreadprivate(sym))
      // Variables appearing in threadprivate directives are threadprivate.
      {
        rt_val = e_threadprivate;
        return rt_val;
      }

      // Check if a SgInitializedName is used as a loop index within a AST
      // subtree. This function will use a bottom-up traverse starting from the
      // subtree_root to find all enclosing loops and check if ivar is used as
      // an index for either of them.
      //        if (isLoopIndexVariable (iname, anchor_stmt)) // TODO: need more
      //        work here
      //  not just any loop variables, but these affected by the OpenMP
      //  directives
      if (isAffectedForLoopIndexVariable(omp_clause_body_stmt, iname)) {
        /*  loop iteration variable
          private: The loop iteration variable(s) in the associated for-loop(s)
          of a for, parallel for, taskloop, or distribute construct.

          linear: The loop iteration variable in the associated for-loop of a
          simd construct with just one associated for-loop is linear with a
          linear-step that is the increment of the associated for-loop.

          lastprivate: The loop iteration variables in the associated for-loops
          of a simd construct with multiple associated for-loops are
          lastprivate.
        */
        if (isSgOmpForStatement(omp_clause_body_stmt))
        // TODO: check other types of constructs here: taskloop, distribute
        // construct
        {
          rt_val = e_private;
          return rt_val;
        } else if (SgOmpSimdStatement *simd_stmt =
                       isSgOmpSimdStatement(omp_clause_body_stmt)) {
          // if simd+ multiple affected loops:  lastprivate().  We check
          // collapse() to see if multiple loops are affected.
          // TODO: we need to check if collapse(val) val >=1
          if (hasClause(simd_stmt, V_SgOmpCollapseClause)) {
            rt_val = e_lastprivate;
          } else
            rt_val = e_linear;
          return rt_val;
        } else {
          // cerr<<"found a loop index, but enclosing body statement is not omp
          // for, but "<<omp_clause_body_stmt->class_name() <<endl;
        }
      }
      // Important algorithm step here:
      // No this logic in the specification, but I split the combined parallel
      // for into two constructs, need to double check this another case is
      // parallel region + single region, we need to get the parallel region's
      // attribute Similar handling for simd directives, going after parent omp
      // parallel or omp for if there is any, to find out the attributes.
      //   parallel+ for + simd: three levels
      //
      //    #pragma omp parallel private(i,j)
      //      {
      //        for (i = 0; i < LOOPCOUNT; i++)
      //          {
      //    #pragma omp single copyprivate(j)
      //            {
      //              nr_iterations++;
      //              j = i;   // i should be private, based on enclosing
      //              parallel region's info.
      //            }
      //       }
      //
      // If implicit rules do not apply at this level (worksharing regions like
      // single), Go to find higher level: most omp parallel
      if (SgOmpClauseBodyStatement *parent_clause_body_stmt =
              findEnclosingOmpClauseBodyStatement(
                  getEnclosingStatement(omp_clause_body_stmt->get_parent()))) {
        // TODO: add other directives which may be nested within others
        if (isSgOmpForStatement(omp_clause_body_stmt) ||
            isSgOmpSimdStatement(omp_clause_body_stmt) ||
            isSgOmpSingleStatement(omp_clause_body_stmt)) {
          // we need to consider the variable's data sharing attribute in the
          // new context the body of parallel can be the single region again,
          // causing infinite recursive calls.
          // rt_val = getDataSharingAttribute (sym,
          // parent_clause_body_stmt->get_body());
          rt_val = getDataSharingAttribute(sym, parent_clause_body_stmt);
          return rt_val;
        }
      }

      // TODO: If an array section is a list item in a map clause on the target
      // construct and the array section is derived from a variable for which
      // the type is pointer then that variable is firstprivate.
    } // end explicit unknown

    // the rest is shared by default
    // TODO Objects with dynamic storage duration are shared.
    // TODO Static data members are shared.

  } // end of has an OpenMP enclosing clause body statement
  else // orphaned code segments
  {
    /*
        For the data race detection project, we choose to inline everything. So
      the implementation of orphaned segs is lower priority.
      //TODO: handle more cases as needed.
      Variables with static storage duration that are declared in called
      routines in the region are shared.

      File-scope or namespace-scope variables referenced in called routines in
      the region are shared unless they appear in a threadprivate directive.

       Objects with dynamic storage duration are shared.

       Static data members are shared unless they appear in a threadprivate
      directive.

       In C++, formal arguments of called routines in the region that are passed
      by reference have the same data-sharing attributes as the associated
      actual arguments.

       Other variables declared in called routines in the region are private.
    */
    if (isThreadprivate(sym)) {
      rt_val = e_threadprivate;
      return rt_val;
    } else {
      // find locally declared variables
      SgDeclarationStatement *var_decl = iname->get_declaration();
      SgFunctionDefinition *func_def =
          getEnclosingFunctionDefinition(anchor_stmt);
      ROSE_ASSERT(func_def != NULL);
      if (isAncestor(func_def, var_decl)) {
        rt_val = e_private;
        return rt_val;
      }
      // if it is within a main function, it should be private no matter what.
      // Single sequential region, not shared with others.
      if (isMain(func_def->get_declaration())) {
        rt_val = e_private;
        return rt_val;
      }
    }
  } // end of orphaned code segments

  return rt_val;
}

bool isThreadprivate(SgSymbol *sym) {
  bool rt_val = false;

  ROSE_ASSERT(sym != NULL);
  SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
  ROSE_ASSERT(var_sym != NULL);
  std::set<SgInitializedName *> var_set = collectThreadprivateVariables();
  SgInitializedName *iname = var_sym->get_declaration();
  ROSE_ASSERT(iname != NULL);

  if (var_set.find(iname) != var_set.end())
    rt_val = true;
  return rt_val;
}

//! Patch up all variables to make them explicit in data-sharing explicit
int patchUpSharedVariables(SgFile *file) {

  int result = 0; // record for the number of shared variables added

  ROSE_ASSERT(file != NULL);
  Rose_STL_Container<SgNode *> allParallelRegion =
      NodeQuery::querySubTree(file, V_SgOmpParallelStatement);
  Rose_STL_Container<SgNode *>::iterator allParallelRegionItr =
      allParallelRegion.begin();

  for (; allParallelRegionItr != allParallelRegion.end();
       allParallelRegionItr++) {
    //! Gather all expressions statements
    Rose_STL_Container<SgNode *> expressions =
        NodeQuery::querySubTree(*allParallelRegionItr, V_SgExprStatement);
    //! Store all variable references
    // TODO: this may miss the constant variables referenced in data type
    // declaration. e.g. int a[length];
    Rose_STL_Container<SgNode *> allRef;
    gatherReferences(expressions, allRef);

    //! Find all local variable declarations in the parallel region
    Rose_STL_Container<SgNode *> localVariables =
        NodeQuery::querySubTree(*allParallelRegionItr, V_SgVariableDeclaration);

    //! Check variables are not local, not variables in clauses already
    Rose_STL_Container<SgNode *>::iterator allRefItr = allRef.begin();
    while (allRefItr != allRef.end()) {
      SgVarRefExp *item = isSgVarRefExp(*allRefItr);
      string varName = item->get_symbol()->get_name().getString();

      Rose_STL_Container<SgNode *>::iterator localVariablesItr =
          localVariables.begin();

      bool isLocal = false; // record whether this variable should be added into
                            // shared clause

      while (localVariablesItr != localVariables.end()) {
        SgInitializedNamePtrList vars =
            ((SgVariableDeclaration *)(*localVariablesItr))->get_variables();

        string localName = vars.at(0)->get_name().getString();
        if (varName == localName) {
          isLocal = true;
        }
        localVariablesItr++;
      }

      bool isInPrivate = false;
      SgInitializedName *reg =
          isSgInitializedName(item->get_symbol()->get_declaration());

      isInPrivate = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpPrivateClause);

      bool isInShared = false;

      isInShared = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpSharedClause);

      bool isInFirstprivate = false;

      isInFirstprivate = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpFirstprivateClause);

      bool isInReduction = false;

      isInReduction = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpReductionClause);

      if (!isLocal && !isInShared && !isInPrivate && !isInFirstprivate &&
          !isInReduction) {
        MLOG_DEBUG_CXX("ompLowering")
            << "add shared clause variable: " << item->unparseToString();
        addClauseVariable(reg,
                          isSgOmpClauseBodyStatement(*allParallelRegionItr),
                          V_SgOmpSharedClause);
        result++;
        MLOG_DEBUG_CXX("ompLowering") << "shared clause insertion succeeded";
      }
      allRefItr++;
    }

  } // end of all parallel region

  return result;
} // the end of patchUpSharedVariables()

//! make all data-sharing attribute explicit

int makeDataSharingExplicit(SgFile *file) {
  int result = 0; // to record the number of varbaile added
  ROSE_ASSERT(file != NULL);

  int p = patchUpPrivateVariables(file); // private variable first

  int f = patchUpFirstprivateVariables(file); // then firstprivate variable

  int s = patchUpSharedVariables(file); // consider shared variables

  // TODO:  patchUpDefaultVariables(file);

  result = p + f + s;
  return result;

} //! the end of makeDataSharingExplicit()

void insertRTLHeaders(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  if (!file->get_Fortran_only() &&
      (hasOpenMPRuntimeConstructs(file) || hasTargetOffloadConstructs(file))) {
    SageInterface::insertHeader(file, "rex_kmp.h",
                                /*isSystemHeader=*/false,
                                /*asLastHeader=*/true);
    file->set_processedToIncludeCppDirectivesAndComments(true);
  }
  if (enable_accelerator) {
    SageInterface::insertHeader(file, "xomp_cuda_lib_inlined.cu",
                                /*isSystemHeader=*/false,
                                /*asLastHeader=*/true);
    file->set_processedToIncludeCppDirectivesAndComments(true);
  }
}

void insertAcceleratorInit(SgSourceFile *sgfile) {
  bool hasMain = false;
  // find the main entry
  SgFunctionDefinition *mainDef = NULL;
  string mainName = "::main";
  ROSE_ASSERT(sgfile != NULL);

  SgFunctionDeclaration *mainDecl = findMain(sgfile);
  if (mainDecl != NULL) {
    // printf ("Found main function setting hasMain == true \n");
    mainDef = mainDecl->get_definition();
    hasMain = true;
  }

  // TODO declare pointers for threadprivate variables and global lock
  // addGlobalOmpDeclarations(ompfrontend, sgfile->get_globalScope(), hasMain );

  if (!hasMain)
    return;
  ROSE_ASSERT(mainDef != NULL); // Liao, at this point, we expect a defining
                                // declaration of main() is
  // look up symbol tables for symbols
  SgScopeStatement *currentscope = mainDef->get_body();
  SgBasicBlock *body = isSgBasicBlock(currentscope);
  ROSE_ASSERT(body != NULL);

  SgExprStatement *expStmt = buildFunctionCallStmt(
      SgName("rex_offload_init"), buildVoidType(), NULL, currentscope);
  setSourcePositionForTransformation(expStmt);
  // Insert before all user statements so one-time cubin registration is not
  // counted inside declaration initializers such as `long long time0 =
  // clock();`.
  prependStatement(expStmt, currentscope);

  // Do not auto-insert rex_offload_fini() at end of main. For standalone
  // processes the OS reclaims the registered image and device-side state on
  // exit, and forcing teardown into user-visible process lifetime adds a
  // measurable fixed cost to short-running GPU programs. Explicit teardown
  // remains available through rex_offload_fini() for callers that need it.

  return;
}

//----------------------------
// tasks:
// * find the main entry for the application
// * add (int argc, char *argv[]) if not exist(?)
// * add runtime system init code at the begin
// * find all return points and append cleanup code
// * add global declarations for threadprivate variables
// * add global declarations for lock variables

void insertRTLinitAndCleanCode(SgSourceFile *sgfile) {
  bool hasMain = false;
  // find the main entry
  SgFunctionDefinition *mainDef = NULL;
  string mainName = "::main";
  ROSE_ASSERT(sgfile != NULL);

  SgFunctionDeclaration *mainDecl = findMain(sgfile);
  if (mainDecl != NULL) {
    // printf ("Found main function setting hasMain == true \n");
    mainDef = mainDecl->get_definition();
    hasMain = true;
  }

  // TODO declare pointers for threadprivate variables and global lock
  // addGlobalOmpDeclarations(ompfrontend, sgfile->get_globalScope(), hasMain );

  if (!hasMain)
    return;
  ROSE_ASSERT(mainDef != NULL); // Liao, at this point, we expect a defining
                                // declaration of main() is found
  // add parameter  int argc , char* argv[] if not exist
  SgInitializedNamePtrList args = mainDef->get_declaration()->get_args();
  SgType *intType = SgTypeInt::createType();
  SgType *charType = SgTypeChar::createType();

  // patch up argc, argv if they do not exit yet
  if (args.size() == 0) {
    SgFunctionParameterList *parameterList =
        mainDef->get_declaration()->get_parameterList();
    ROSE_ASSERT(parameterList);

    // int argc
    SgName name1("argc");
    SgInitializedName *arg1 = buildInitializedName(name1, intType);

    // char** argv
    SgName name2("argv");
    SgPointerType *pType1 = buildPointerType(charType);
    SgPointerType *pType2 = buildPointerType(pType1);
    SgInitializedName *arg2 = buildInitializedName(name2, pType2);

    appendArg(parameterList, arg1);
    appendArg(parameterList, arg2);

  } // end if (args.size() ==0)
  // add statements to prepare the runtime system
  // int status=0;
  SgIntVal *intVal = buildIntVal(0);

  SgAssignInitializer *init2 = buildAssignInitializer(intVal);
  SgName *name1 = new SgName("status");
  SgVariableDeclaration *varDecl1 = buildVariableDeclaration(
      *name1, SgTypeInt::createType(), init2, mainDef->get_body());

  // cout<<"debug:"<<varDecl1->unparseToString()<<endl;

  //_ompc_init(argc, argv);
  SgType *voidtype = SgTypeVoid::createType();
  SgFunctionType *myFuncType = new SgFunctionType(voidtype, false);
  ROSE_ASSERT(myFuncType != NULL);

  // SgExprListExp, two parameters (argc, argv)
  //  look up symbol tables for symbols
  SgScopeStatement *currentscope = mainDef->get_body();

  SgInitializedNamePtrList mainArgs =
      mainDef->get_declaration()->get_parameterList()->get_args();
  Rose_STL_Container<SgInitializedName *>::iterator i = mainArgs.begin();
  ROSE_ASSERT(mainArgs.size() == 2);

  SgExprListExp *exp_list_exp = buildExprListExp();
  if (!SageInterface::is_Fortran_language()) {
    SgVarRefExp *var1 =
        buildVarRefExp(isSgInitializedName(*i), mainDef->get_body());
    SgVarRefExp *var2 =
        buildVarRefExp(isSgInitializedName(*++i), mainDef->get_body());

    appendExpression(exp_list_exp, var1);
    appendExpression(exp_list_exp, var2);
  }

  if (SageInterface::is_Fortran_language()) {
    SgStatement *l_stmt = findLastDeclarationStatement(currentscope);
    if (l_stmt != NULL)
      insertStatementAfter(l_stmt, varDecl1);
    else
      prependStatement(varDecl1, currentscope);
  } else // C/C++, we can always prepend it.
    prependStatement(varDecl1, currentscope);

  //---------------------- termination part

  //  cout<<"debug:"<<mainDef->unparseToString()<<endl;

  // search all return statements and add terminate() before them
  // the body of this function is empty in the runtime library
  // _ompc_terminate(status);

  // SgExprListExp, 1 parameters (status)
  SgInitializedName *initName1 = varDecl1->get_decl_item(*name1);
  ROSE_ASSERT(initName1);

  SgVarRefExp *var3 = buildVarRefExp(initName1, currentscope);
  SgExprListExp *exp_list_exp2 = buildExprListExp();
  appendExpression(exp_list_exp2, var3);

  //   AstPostProcessing(mainDef->get_declaration());

  return;
}

//! Replace references to oldVar within root with references to newVar
int replaceVariableReferences(SgNode *root, SgVariableSymbol *oldVar,
                              SgVariableSymbol *newVar) {
  ROSE_ASSERT(oldVar != NULL);
  ROSE_ASSERT(newVar != NULL);

  VariableSymbolMap_t varRemap;
  varRemap.insert(VariableSymbolMap_t::value_type(oldVar, newVar));
  return replaceVariableReferences(root, varRemap);
}

static bool shouldSkipOpenMPClauseVarRefRewrite(const SgVarRefExp *ref_orig) {
  return ref_orig != NULL && getEnclosingNode<SgOmpClause>(
                                 const_cast<SgVarRefExp *>(ref_orig)) != NULL;
}

static void clearOpenMPClauseOriginalExpressionTrees(SgNode *root) {
  if (root == NULL) {
    return;
  }

  Rose_STL_Container<SgNode *> expr_nodes =
      NodeQuery::querySubTree(root, V_SgExpression);
  for (Rose_STL_Container<SgNode *>::const_iterator it = expr_nodes.begin();
       it != expr_nodes.end(); ++it) {
    SgExpression *expr = isSgExpression(*it);
    if (expr == NULL) {
      continue;
    }
    if (getEnclosingNode<SgOmpClause>(expr) == NULL) {
      continue;
    }
    if (expr->get_originalExpressionTree() != NULL) {
      expr->set_originalExpressionTree(NULL);
    }
  }
}

//! Replace variable references within root based on a map from old symbols to
//! new symbols
/* This function is mostly used by transOmpVariables() to handle private,
 * firstprivate, reduction, etc.
 *
 *
 */
int replaceVariableReferences(SgNode *root, VariableSymbolMap_t varRemap) {
  int result = 0;
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t refs = NodeQuery::querySubTree(root, V_SgVarRefExp);
  for (NodeList_t::iterator i = refs.begin(); i != refs.end(); ++i) {
    SgVarRefExp *ref_orig = isSgVarRefExp(*i);
    ROSE_ASSERT(ref_orig);
    if (shouldSkipOpenMPClauseVarRefRewrite(ref_orig)) {
      continue;
    }
    VariableSymbolMap_t::const_iterator iter =
        varRemap.find(ref_orig->get_symbol());
    if (iter != varRemap.end()) {
      SgVariableSymbol *newSym = iter->second;
      ref_orig->set_symbol(newSym);
      result++;
    }
  }
  return result;
}

int replaceVariablesWithPointerDereference(SgNode *root,
                                           ASTtools::VarSymSet_t vars) {
  int result = 0;
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t refs = NodeQuery::querySubTree(root, V_SgVarRefExp);
  for (NodeList_t::iterator i = refs.begin(); i != refs.end(); ++i) {
    SgVarRefExp *ref_orig = isSgVarRefExp(*i);
    ROSE_ASSERT(ref_orig);
    if (shouldSkipOpenMPClauseVarRefRewrite(ref_orig)) {
      continue;
    }
    ASTtools::VarSymSet_t::const_iterator ii =
        vars.find(ref_orig->get_symbol());
    if (ii != vars.end()) {
      SgExpression *ptr_ref = buildPointerDerefExp(copyExpression(ref_orig));
      ptr_ref->set_need_paren(true);
      SageInterface::replaceExpression(ref_orig, ptr_ref);
      result++;
    }
  }
  return result;
}

//! Create a stride expression from an existing stride expression based on the
//! loop iteration's order (incremental or decremental)
// The assumption is orig_stride is just the raw operand of the condition
// expression of a loop so it has to be adjusted to reflect the real stride:
// *(-1) if decremental
static SgExpression *createAdjustedStride(SgExpression *orig_stride,
                                          bool isIncremental) {
  ROSE_ASSERT(orig_stride);
  if (isIncremental)
    return copyExpression(orig_stride); // never share expressions
  else {
    /*  I changed the normalization phase to generate consistent incremental
     * expressions it should be i+= -1  for decremental loops no need to adjust
     * it anymore.
     *  */
    //      printf("Found a decremental case: orig_stride is\n");
    //      cout<<"\t"<<orig_stride->unparseToString()<<endl;
    return copyExpression(orig_stride);
    // return buildMultiplyOp(buildIntVal(-1),copyExpression(orig_stride));
  }
}

static SgStatement *generateTargetReduceOnCPU(std::string orig_var,
                                              SgVariableSymbol *buffer_decl,
                                              SgVariableDeclaration *num_blocks,
                                              int r_operator) {
  SgVariableDeclaration *init_stmt = buildVariableDeclaration(
      "i", buildIntType(), buildAssignInitializer(buildIntVal(0)),
      num_blocks->get_scope());
  SgStatement *cond_stmt = buildExprStatement(
      buildLessThanOp(buildVarRefExp(init_stmt), buildVarRefExp(num_blocks)));
  SgExpression *incr_exp =
      buildPlusPlusOp(buildVarRefExp(init_stmt), SgUnaryOp::postfix);
  SgStatement *loop_body = NULL;
  switch (r_operator) {
  case 6: // SgOmpClause::e_omp_reduction_plus
  case 7: // SgOmpClause::e_omp_reduction_minus
    loop_body = buildExprStatement(
        buildPlusAssignOp(buildVarRefExp(orig_var),
                          buildPntrArrRefExp(buildVarRefExp(buffer_decl),
                                             buildVarRefExp(init_stmt))));
    break;
  default:
    ROSE_ASSERT(0 && "Unsupported reduction operator is met.");
  }
  ROSE_ASSERT(loop_body != NULL);
  SgStatement *for_stmt =
      buildForStatement_nfi(init_stmt, cond_stmt, incr_exp, loop_body);

  return for_stmt;
}

//! check if an omp for/do loop use static schedule or not
// Static schedule include: default schedule, or schedule(static[,chunk_size])
bool useStaticSchedule(SgOmpClauseBodyStatement *omp_loop) {
  ROSE_ASSERT(omp_loop);
  bool result = false;
  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(omp_loop, V_SgOmpScheduleClause);
  if (clauses.size() == 0) {
    result = true; // default schedule is static
  } else {
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    if (s_clause->get_kind() == SgOmpClause::e_omp_schedule_kind_static)
      result = true;
  }
  return result;
}

// Chunk size  for dynamic and guided schedule should be 1 if not specified.
static SgExpression *createAdjustedChunkSize(SgExpression *orig_chunk_size) {
  SgExpression *result = NULL;
  if (orig_chunk_size)
    result = copyExpression(orig_chunk_size);
  else
    result = buildIntVal(1);
  ROSE_ASSERT(result != NULL);
  return result;
}
// Convert a schedule kind enum value to a small case string
string toString(SgOmpClause::omp_schedule_kind_enum s_kind) {
  string result;
  if (s_kind == SgOmpClause::e_omp_schedule_kind_static) {
    result = "static";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_dynamic) {
    result = "dynamic";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_guided) {
    result = "guided";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_runtime) {
    result = "runtime";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_auto) {
    //      cerr<<"GOMP does not provide an implementation for
    //      schedule(auto)....."<<endl;
    result = "auto";
  } else {
    cerr << "Error: illegal or unhandled schedule kind:" << s_kind << endl;
    ROSE_ABORT();
  }
  return result;
}

//! Generate XOMP loop schedule init function's name
string
generateGOMPLoopInitFuncName(bool isOrdered,
                             SgOmpClause::omp_schedule_kind_enum s_kind) {
  // XOMP_loop_static_init()
  // XOMP_loop_ordered_static_init ()
  // XOMP_loop_dynamic_init ()
  // XOMP_loop_ordered_dynamic_init ()
  // .....
  string result;
  result = "XOMP_loop_";
  // Handled ordered
  if (isOrdered)
    result += "ordered_";
  result += toString(s_kind);
  result += "_init";
  return result;
}

//! Generate XOMP loop schedule start function's name
string
generateXOMPLoopStartFuncName(bool isOrdered,
                              SgOmpClause::omp_schedule_kind_enum s_kind) {
  // XOMP_loop_static_start ()
  // XOMP_loop_ordered_static_start ()
  // XOMP_loop_dynamic_start ()
  // XOMP_loop_ordered_dynamic_start ()
  // .....
  string result;
  result = "XOMP_loop_";
  // Handled ordered
  if (isOrdered)
    result += "ordered_";
  result += toString(s_kind);
  result += "_start";
  return result;
}

//! Generate XOMP loop schedule next function's name
string
generateXOMPLoopNextFuncName(bool isOrdered,
                             SgOmpClause::omp_schedule_kind_enum s_kind) {
  string result;
  // XOMP_loop_static_next()
  // XOMP_loop_ordered_static_next ()
  // XOMP_loop_dynamic_next ()
  // XOMP_loop_ordered_dynamic_next()
  // .....

  result = "XOMP_loop_";
  if (isOrdered)
    result += "ordered_";
  result += toString(s_kind);
  result += "_next";
  return result;
}

//! Fortran only action: insert include "libxompf.fh" into the function body
//! with calls to XOMP_loop_* functions
// This is necessary since XOMP_loop_* functions will be treated as returning
// REAL by implicit rules (starting with X) This function finds the function
// definition enclosing a start node, check if there is any existing include
// 'libxompf.fh' then insert one if there is none.
static void insert_libxompf_h(SgNode *startNode) {
  ROSE_ASSERT(startNode != NULL);
  // This function should not be used for other than Fortran
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  // we don't expect input node is a func def already
  ROSE_ASSERT(isSgFunctionDefinition(startNode) == NULL);

  SgBasicBlock *t_body = getEnclosingRegionOrFuncDefinition(startNode);
  ROSE_ASSERT(t_body != NULL);
  // Try to find an existing include 'libxompf.fh'
  // Assumptions:
  //   1. It only shows up at the top level, not within other SgBasicBlock
  //   2. The startNode is after the include line
  SgStatement *s_include = NULL; // existing include
  SgStatementPtrList stmt_list = t_body->get_statements();
  SgStatementPtrList::iterator iter;
  for (iter = stmt_list.begin(); iter != stmt_list.end(); iter++) {
    SgStatement *stmt = *iter;
    ROSE_ASSERT(stmt != NULL);
    SgFortranIncludeLine *f_inc = isSgFortranIncludeLine(stmt);
    if (f_inc) {
      string f_name =
          StringUtility::stripPathFromFileName(f_inc->get_filename());
      if (f_name == "libxompf.fh" || f_name == "libxompf.h") {
        s_include = f_inc;
        break;
      }
    }
  }
  if (s_include == NULL) {
    s_include = buildFortranIncludeLine("libxompf.fh");
    SgStatement *l_stmt = findLastDeclarationStatement(t_body);
    if (l_stmt)
      insertStatementAfter(l_stmt, s_include);
    else
      prependStatement(s_include, t_body);
  }
}
//! Translate an omp for loop with non-static scheduling clause or with ordered
//! clause ()
// bb1 is the basic block to insert the translated loop
// bb1 already has compiler-generated variable declarations for new loop control
// variables
/*
 * start, end, incremental, chunk_size, own_start, own_end
 XOMP_loop_static_init(int lower, int upper, int stride, int chunk_size);

 if (GOMP_loop_dynamic_start (orig_lower, orig_upper, adj_stride, orig_chunk,
&_p_lower, &_p_upper))
//  if (GOMP_loop_ordered_dynamic_start (S, E, INCR, CHUNK, &_p_lower,
&_p_upper))
{
do
{
for (_p_index = _p_lower; _p_index < _p_upper; _p_index += orig_stride)
set_data (_p_index, iam);
}
while (GOMP_loop_dynamic_next (&_p_lower, &_p_upper));
// while (GOMP_loop_ordered_dynamic_next (&_p_lower, &_p_upper));
}
GOMP_loop_end ();
//  GOMP_loop_end_nowait ();
//
// More explanation: -------------------------------------------
// Omni uses the following translation
_ompc_dynamic_sched_init(_p_loop_lower,_p_loop_upper,_p_loop_stride,5);
while(_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper)){
for (_p_loop_index = _p_loop_lower; (_p_loop_index) < _p_loop_upper;
_p_loop_index += _p_loop_stride) { k_3++;
}
}
// In order to merge two kinds of translations into one scheme.
// we split
while(_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper)){
for (_p_loop_index = _p_loop_lower; (_p_loop_index) < _p_loop_upper;
_p_loop_index += _p_loop_stride) { k_3++;
}
}

// to
if (_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper)){
do {
for (_p_loop_index = _p_loop_lower; (_p_loop_index) < _p_loop_upper;
_p_loop_index += _p_loop_stride) { k_3++;
}
} while (_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper));
}
// and XOMP layer will compensate for the difference.
*/
static void transOmpLoop_others(SgOmpClauseBodyStatement *target,
                                SgVariableDeclaration *index_decl,
                                SgVariableDeclaration *lower_decl,
                                SgVariableDeclaration *upper_decl,
                                SgVariableDeclaration *stride_decl,
                                SgVariableDeclaration *last_iter_decl,
                                SgBasicBlock *bb1) {
  ROSE_ASSERT(target != NULL);
  ROSE_ASSERT(index_decl != NULL);
  ROSE_ASSERT(lower_decl != NULL);
  ROSE_ASSERT(upper_decl != NULL);
  ROSE_ASSERT(bb1 != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // The OpenMP syntax requires that the omp for pragma is immediately followed
  // by the for loop.
  SgForStatement *for_loop = isSgForStatement(body);
  SgFortranDo *do_loop = isSgFortranDo(body);
  SgStatement *loop =
      for_loop != NULL ? (SgStatement *)for_loop : (SgStatement *)do_loop;

  SgExprListExp *parameters = NULL;
  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(target, bb1, &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), bb1);
  if (SageInterface::is_Fortran_language())
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration, bb1);
  else
    appendStatement(kmpc_global_tid_declaration, bb1);
  if (kmpc_global_tid_init != NULL)
    appendStatement(kmpc_global_tid_init, bb1);
  SgExpression *source_location_info = buildIntVal(0);

  SgInitializedName *orig_index;
  SgExpression *orig_lower, *orig_upper, *orig_stride;
  bool isIncremental = true; // if the loop iteration space is incremental
  // grab the original loop 's controlling information
  bool is_canonical = false;
  if (for_loop)
    is_canonical =
        isCanonicalForLoop(for_loop, &orig_index, &orig_lower, &orig_upper,
                           &orig_stride, NULL, &isIncremental);
  else if (do_loop) {
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, &isIncremental, NULL);
  } else {
    cerr << "error! transOmpLoop_others(). loop is neither for_loop nor "
            "do_loop. Aborting.."
         << endl;
    ROSE_ABORT();
  }
  ROSE_ASSERT(is_canonical == true);

  const bool use_64_runtime =
      for_loop != NULL && use_kmpc_loop_64bit_runtime(
                              getFirstVariable(*lower_decl).get_type(), target);

  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(target, V_SgOmpScheduleClause);

  // the case of with the ordered schedule, but without any schedule policy
  // specified treat it as (static, 0) based on GCC's translation
  SgOmpClause::omp_schedule_kind_enum s_kind =
      SgOmpClause::e_omp_schedule_kind_static;
  SgExpression *orig_chunk_size = NULL;
  string func_init_name = get_kmpc_for_static_init_name(use_64_runtime);
  int32_t schedule_type = 0;
  bool hasOrder = false;
  if (hasClause(target, V_SgOmpOrderedClause))
    hasOrder = true;
  ROSE_ASSERT(hasOrder || clauses.size() != 0);
  // Most cases: with schedule(kind,chunk_size)
  if (clauses.size() != 0) {
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    s_kind = s_clause->get_kind();
    orig_chunk_size = s_clause->get_chunk_size();
    SgOmpClause::omp_schedule_modifier_enum schedule_modifier =
        s_clause->get_modifier();
    if ((hasOrder || s_kind == SgOmpClause::e_omp_schedule_kind_static) &&
        schedule_modifier !=
            SgOmpClause::e_omp_schedule_modifier_nonmonotonic) {
      schedule_type = kmp_sched_modifier_monotonic;
    } else {
      schedule_type = kmp_sched_modifier_nonmonotonic;
    };

    // chunk size is 1 for dynamic and guided schedule, if not specified.
    if (s_kind == SgOmpClause::e_omp_schedule_kind_dynamic ||
        s_kind == SgOmpClause::e_omp_schedule_kind_guided) {
      orig_chunk_size = createAdjustedChunkSize(orig_chunk_size);
      func_init_name = get_kmpc_dispatch_init_name(use_64_runtime);
      if (s_kind == SgOmpClause::e_omp_schedule_kind_dynamic) {
        schedule_type += kmp_sched_dynamic;
      } else {
        schedule_type += kmp_sched_guided;
      };
      parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildIntVal(schedule_type),
          buildVarRefExp(lower_decl), buildVarRefExp(upper_decl),
          buildVarRefExp(stride_decl), orig_chunk_size);

    } else if (s_kind == SgOmpClause::e_omp_schedule_kind_auto ||
               s_kind == SgOmpClause::e_omp_schedule_kind_runtime) {
      orig_chunk_size = buildIntVal(1);
      func_init_name = get_kmpc_dispatch_init_name(use_64_runtime);
      if (s_kind == SgOmpClause::e_omp_schedule_kind_auto) {
        schedule_type += kmp_sched_auto;
      } else {
        schedule_type += kmp_sched_runtime;
      };
      parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildIntVal(schedule_type),
          buildVarRefExp(lower_decl), buildVarRefExp(upper_decl),
          buildVarRefExp(stride_decl), orig_chunk_size);

    } else {
      if (orig_chunk_size == NULL)
        orig_chunk_size = buildIntVal(0);
      schedule_type += kmp_sched_static_chunk;
      SgExpression *e_last_iter =
          buildAddressOfOp(buildVarRefExp(last_iter_decl));
      SgExpression *e_lower = buildAddressOfOp(buildVarRefExp(lower_decl));
      SgExpression *e_upper = buildAddressOfOp(buildVarRefExp(upper_decl));
      SgExpression *e_stride = buildAddressOfOp(buildVarRefExp(stride_decl));
      if (do_loop) {
        // Fortran arguments are pass-by-reference already.
        e_last_iter = buildVarRefExp(last_iter_decl);
        e_lower = buildVarRefExp(lower_decl);
        e_upper = buildVarRefExp(upper_decl);
        e_stride = buildVarRefExp(stride_decl);
      }
      parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildIntVal(schedule_type),
          e_last_iter, e_lower, e_upper, e_stride, copyExpression(orig_stride),
          orig_chunk_size);
    }
  } else
    orig_chunk_size = buildIntVal(0);

  // schedule(auto) does not have chunk size
  if (s_kind != SgOmpClause::e_omp_schedule_kind_auto &&
      s_kind != SgOmpClause::e_omp_schedule_kind_runtime)
    ROSE_ASSERT(orig_chunk_size != NULL);
  const bool use_dispatch_runtime =
      s_kind == SgOmpClause::e_omp_schedule_kind_dynamic ||
      s_kind == SgOmpClause::e_omp_schedule_kind_guided ||
      s_kind == SgOmpClause::e_omp_schedule_kind_auto ||
      s_kind == SgOmpClause::e_omp_schedule_kind_runtime;

  if (SageInterface::is_Fortran_language() && use_dispatch_runtime) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(bb1);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(
        func_def->get_body(),
        SgName(get_kmpc_dispatch_next_name(use_64_runtime)), buildIntType());
  }

  SgExprStatement *func_init_stmt =
      buildFunctionCallStmt(func_init_name, buildVoidType(), parameters, bb1);
  appendStatement(func_init_stmt, bb1);

  auto build_dispatch_next_expr = [&]() -> SgExpression * {
    SgExprListExp *dispatch_parameters = NULL;
    if (for_loop) {
      dispatch_parameters =
          buildExprListExp(copyExpression(source_location_info),
                           copyExpression(thread_global_tid),
                           buildAddressOfOp(buildVarRefExp(last_iter_decl)),
                           buildAddressOfOp(buildVarRefExp(lower_decl)),
                           buildAddressOfOp(buildVarRefExp(upper_decl)),
                           buildAddressOfOp(buildVarRefExp(stride_decl)));
    } else {
      dispatch_parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildVarRefExp(last_iter_decl),
          buildVarRefExp(lower_decl), buildVarRefExp(upper_decl),
          buildVarRefExp(stride_decl));
    }
    return buildFunctionCallExp(get_kmpc_dispatch_next_name(use_64_runtime),
                                buildIntType(), dispatch_parameters, bb1);
  };

  auto build_static_chunk_continue_expr = [&]() -> SgExpression * {
    SgExpression *stride_positive =
        buildGreaterThanOp(copyExpression(orig_stride), buildIntVal(0));
    SgExpression *stride_negative =
        buildLessThanOp(copyExpression(orig_stride), buildIntVal(0));
    SgExpression *incremental_bounds = buildLessOrEqualOp(
        buildVarRefExp(lower_decl), buildVarRefExp(upper_decl));
    SgExpression *decremental_bounds = buildGreaterOrEqualOp(
        buildVarRefExp(lower_decl), buildVarRefExp(upper_decl));
    return buildOrOp(buildAndOp(stride_positive, incremental_bounds),
                     buildAndOp(stride_negative, decremental_bounds));
  };

  auto build_upper_clamp_stmt = [&]() -> SgStatement * {
    SgExpression *stride_positive =
        buildGreaterThanOp(copyExpression(orig_stride), buildIntVal(0));
    SgExpression *stride_negative =
        buildLessThanOp(copyExpression(orig_stride), buildIntVal(0));
    SgExpression *incremental_clamp = buildAndOp(
        stride_positive, buildGreaterThanOp(buildVarRefExp(upper_decl),
                                            copyExpression(orig_upper)));
    SgExpression *decremental_clamp = buildAndOp(
        stride_negative, buildLessThanOp(buildVarRefExp(upper_decl),
                                         copyExpression(orig_upper)));
    SgExpression *if_condition =
        buildOrOp(incremental_clamp, decremental_clamp);
    SgExprStatement *update_upper_bound_stmt = buildAssignStatement(
        buildVarRefExp(upper_decl), copyExpression(orig_upper));
    SgStatement *if_true_body = update_upper_bound_stmt;
    if (SageInterface::is_Fortran_language()) {
      SgBasicBlock *if_body = buildBasicBlock();
      appendStatement(update_upper_bound_stmt, if_body);
      if_true_body = if_body;
    }
    return buildIfStmt(if_condition, if_true_body, NULL);
  };

  auto append_static_chunk_advance = [&](SgBasicBlock *scope) {
    if (SageInterface::is_Fortran_language()) {
      appendStatement(
          buildAssignStatement(buildVarRefExp(lower_decl),
                               buildAddOp(buildVarRefExp(lower_decl),
                                          buildVarRefExp(stride_decl))),
          scope);
      appendStatement(
          buildAssignStatement(buildVarRefExp(upper_decl),
                               buildAddOp(buildVarRefExp(upper_decl),
                                          buildVarRefExp(stride_decl))),
          scope);
      return;
    }

    appendStatement(
        buildExprStatement(buildPlusAssignOp(buildVarRefExp(lower_decl),
                                             buildVarRefExp(stride_decl))),
        scope);
    appendStatement(
        buildExprStatement(buildPlusAssignOp(buildVarRefExp(upper_decl),
                                             buildVarRefExp(stride_decl))),
        scope);
  };

  SgBasicBlock *true_body = buildBasicBlock();
  if (SageInterface::is_Fortran_language()) {
    SgExpression *entry_cond = NULL;
    if (use_dispatch_runtime) {
      entry_cond = buildEqualityOp(build_dispatch_next_expr(), buildIntVal(1));
    } else {
      entry_cond = build_static_chunk_continue_expr();
    }
    SgIfStmt *if_stmt = buildIfStmt(entry_cond, true_body, NULL);
    appendStatement(if_stmt, bb1);
  } else {
    appendStatement(true_body, bb1);
  }

  // do {} while (__kmpc_dispatch_next_*(...)) or while (lower <= upper)
  if (for_loop) {
    SgExpression *func_next_exp = NULL;
    if (use_dispatch_runtime) {
      func_next_exp = build_dispatch_next_expr();
    } else {
      func_next_exp = build_static_chunk_continue_expr();
    }
    SgBasicBlock *do_body = buildBasicBlock();
    SgWhileStmt *while_do_stmt = buildWhileStmt(func_next_exp, do_body);
    appendStatement(while_do_stmt, true_body);

    appendStatement(build_upper_clamp_stmt(), do_body);

    // insert the loop into do-while
    appendStatement(loop, do_body);
    if (!use_dispatch_runtime) {
      append_static_chunk_advance(do_body);
      parameters =
          buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
      appendStatement(buildFunctionCallStmt("__kmpc_for_static_fini",
                                            buildVoidType(), parameters, bb1),
                      bb1);
    };
  }
  // Liao 1/7/2011, Fortran does not support SgDoWhileStmt
  // We use the following control flow as an alternative:
  //   label  continue
  //          loop_here
  //          if (GOMP_loop_static_next (&_p_lower, &_p_upper))
  //             goto label
  else if (do_loop) {
    SgFunctionDefinition *funcDef = getEnclosingFunctionDefinition(bb1);
    ROSE_ASSERT(funcDef != NULL);
    // label  CONTINUE
    SgLabelStatement *label_stmt_1 = buildLabelStatement("", NULL);
    appendStatement(label_stmt_1, true_body);
    int l_val = suggestNextNumericLabel(funcDef);
    setFortranNumericLabel(label_stmt_1, l_val);
    appendStatement(build_upper_clamp_stmt(), true_body);
    // loop here
    appendStatement(loop, true_body);

    if (!use_dispatch_runtime)
      append_static_chunk_advance(true_body);

    // if () goto label
    SgExpression *func_next_exp = NULL;
    if (use_dispatch_runtime) {
      func_next_exp =
          buildEqualityOp(build_dispatch_next_expr(), buildIntVal(1));
    } else {
      func_next_exp = build_static_chunk_continue_expr();
    }
    SgIfStmt *if_stmt_2 =
        buildIfStmt(func_next_exp, buildBasicBlock(), buildBasicBlock());
    SgGotoStatement *gt_stmt =
        buildGotoStatement(label_stmt_1->get_numeric_label()->get_symbol());
    appendStatement(gt_stmt, isSgScopeStatement(if_stmt_2->get_true_body()));
    appendStatement(if_stmt_2, true_body);
    // assertion from unparser
    SgStatementPtrList &statementList =
        isSgBasicBlock(if_stmt_2->get_true_body())->get_statements();
    ROSE_ASSERT(statementList.size() == 1);

    if (!use_dispatch_runtime) {
      parameters =
          buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
      appendStatement(buildFunctionCallStmt("__kmpc_for_static_fini",
                                            buildVoidType(), parameters, bb1),
                      bb1);
    }
  }

  // Rewrite loop control variables
  replaceVariableReferences(
      loop, isSgVariableSymbol(orig_index->get_symbol_from_symbol_table()),
      getFirstVarSym(index_decl));
  SageInterface::setLoopLowerBound(loop, buildVarRefExp(lower_decl));
  SageInterface::setLoopUpperBound(loop, buildVarRefExp(upper_decl));
  ROSE_ASSERT(orig_upper != NULL);
  transOmpVariables(
      target, bb1,
      orig_upper); // This should happen before the barrier is inserted.
  if (!hasClause(target, V_SgOmpNowaitClause)) {
    parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    appendStatement(buildFunctionCallStmt("__kmpc_barrier", buildVoidType(),
                                          parameters, bb1),
                    bb1);
  }
}

// Expected AST
// * OmpForStatement
// ** SgForStatement
// Algorithm:
// Loop normalization first  for stop condition expressions
//   <: for (i= 0;i <20; i++) --> for (i= 0;i <20; i+=1)  [0,20, +1] to pass to
//   runtime calls
//  <=: for (i= 0;i<=20; i++) --> for (i= 0;i <21; i+=1)
//   >: for (i=20;i >-1; i--) --> for (i=20;i >-1; i-=1) [20, -1, -1]
//  >=: for (i=20;i>= 0; i--) --> for (i=20;i >-1; i-=1)
// We have a SageInterface::forLoopNormalization() which does the opposite
// (normalizing a C loop to a Fortran style loop) < --> <= and > --> >=,
// GCC-GOMP use compiler-generated statements to schedule loop iterations using
// static schedule All other schedule policies use runtime calls instead. We
// translate static schedule here and non-static ones in transOmpLoop_others()
//
// Static schedule, including:
// 1. default (static even) case
// 2. schedule(static[, chunk_size]): == static even if chunk_size is not
// specified
// gomp does not provide a runtime call to calculate loop control values
// for the default (static even) scheduling
// compilers have to generate the statements to do this. I HATE THIS!!!
// the loop scheduling algorithm for the default case is
/*
// calculate loop iteration count from lower, upper and stride , no -1 if upper
is an inclusive bound int _p_iter_count = (stride + -1 + upper - lower )/stride;
// calculate a proper chunk size
// two cases: evenly divisible  20/5 =4
//   not evenly divisible 20/3= 6
// Initial candidate

int _p_num_threads = omp_get_num_threads ();
_p_chunk_size = _p_iter_count / _p_num_threads;
int _p_ck_temp = (_p_chunk_size * _p_num_threads) != _p_iter_count;
// increase the chunk size by 1 if not evenly divisible
_p_chunk_size = _p_ck_temp + _p_chunk_size;

// decide on the lower and upper bound for the current thread
int _p_thread_id = omp_get_thread_num ();
_p_lower = lower + _p_chunk_size * _p_thread_id * stride;a
// -1 if upper is an inclusive bound
_p_upper = _p_lower + _p_chunk_size * stride;

// adjust the upper bound
_p_upper = MIN_EXPR <_p_upper, upper>;
// _p_upper = _p_upper<upper? _p_upper: upper;
// Note: decremental iteration space needs some minor changes to the algorithm
above.
// stride should be negated
// MIN_EXP should be MAX_EXP
// upper bound adjustment should be +1 instead of -1
*/
void transOmpLoop(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpForStatement *target1 = isSgOmpForStatement(node);
  SgOmpDoStatement *target2 = isSgOmpDoStatement(node);

  SgOmpClauseBodyStatement *target =
      (target1 != NULL ? (SgOmpClauseBodyStatement *)target1
                       : (SgOmpClauseBodyStatement *)target2);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // The OpenMP syntax requires that the omp for pragma is immediately followed
  // by the for loop.
  SgForStatement *for_loop = isSgForStatement(body);
  SgFortranDo *do_loop = isSgFortranDo(body);

  if (for_loop == NULL && do_loop == NULL) {
    if (SgBasicBlock *body_bb = isSgBasicBlock(body)) {
      const SgStatementPtrList &stmts = body_bb->get_statements();
      for (SgStatementPtrList::const_iterator it = stmts.begin();
           it != stmts.end(); ++it) {
        for_loop = isSgForStatement(*it);
        do_loop = isSgFortranDo(*it);
        if (for_loop != NULL || do_loop != NULL) {
          break;
        }
      }
    }
  }
  if (for_loop == NULL && do_loop == NULL) {
    VariantVector loop_variants(V_SgForStatement);
    loop_variants.push_back(V_SgFortranDo);
    Rose_STL_Container<SgNode *> loops =
        NodeQuery::querySubTree(body, loop_variants);
    for (Rose_STL_Container<SgNode *>::const_iterator it = loops.begin();
         it != loops.end(); ++it) {
      if (for_loop == NULL)
        for_loop = isSgForStatement(*it);
      if (do_loop == NULL)
        do_loop = isSgFortranDo(*it);
      if (for_loop != NULL || do_loop != NULL)
        break;
    }
  }

  SgStatement *loop =
      (for_loop != NULL ? (SgStatement *)for_loop : (SgStatement *)do_loop);
  ROSE_ASSERT(loop != NULL);

  if (for_loop != NULL) {
    // Outlined OpenMP regions can represent induction variables as pointer
    // dereferences (e.g., *i, *(*ip__)). Rewrite them to local scalar indices
    // before canonical normalization/analysis.
    rewritePointerBasedForIndices(for_loop);
  }

  SgExprListExp *parameters = NULL;
  SgExpression *source_location_info = buildIntVal(0);

  // Step 1. Loop normalization
  // we reuse the normalization from SageInterface, though it is different from
  // what gomp expects. the point is to have a consistent loop form. We can
  // adjust the difference later on.
  if (for_loop)
    SageInterface::forLoopNormalization(for_loop);
  else if (do_loop)
    SageInterface::doLoopNormalization(do_loop);
  else {
    cerr << "error! transOmpLoop(). loop is neither for_loop nor do_loop. "
            "Aborting.."
         << endl;
    ROSE_ABORT();
  }

  SgInitializedName *orig_index = NULL;
  SgExpression *orig_lower = NULL;
  SgExpression *orig_upper = NULL;
  SgExpression *orig_stride = NULL;
  bool isIncremental = true; // if the loop iteration space is incremental
  // grab the original loop 's controlling information
  bool is_canonical = false;
  if (for_loop)
    is_canonical =
        isCanonicalForLoop(for_loop, &orig_index, &orig_lower, &orig_upper,
                           &orig_stride, NULL, &isIncremental);
  else if (do_loop)
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, &isIncremental, NULL);
  if (!is_canonical && for_loop != NULL) {
    is_canonical = recoverCanonicalForLoopControl(for_loop, &orig_index,
                                                  &orig_lower, &orig_upper,
                                                  &orig_stride, &isIncremental);
  }
  if (!is_canonical) {
    MLOG_WARN_CXX("ompLowering")
        << "transOmpLoop: non-canonical loop after normalization";
    if (for_loop) {
      MLOG_WARN_CXX("ompLowering")
          << "for-loop: " << for_loop->unparseToString();
      if (for_loop->get_for_init_stmt())
        MLOG_WARN_CXX("ompLowering")
            << "for-init: " << for_loop->get_for_init_stmt()->unparseToString();
      if (for_loop->get_test())
        MLOG_WARN_CXX("ompLowering")
            << "for-test: " << for_loop->get_test()->unparseToString();
      if (for_loop->get_increment())
        MLOG_WARN_CXX("ompLowering")
            << "for-increment: "
            << for_loop->get_increment()->unparseToString();
    } else if (do_loop) {
      MLOG_WARN_CXX("ompLowering") << "do-loop: " << do_loop->unparseToString();
    }
    if (loop->get_file_info())
      loop->get_file_info()->display("non-canonical transOmpLoop");
  }
  ROSE_ASSERT(is_canonical == true);

  // step 2. Insert a basic block to replace OmpForStatement
  // This newly introduced scope is used to hold loop variables, private
  // variables ,etc
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();

  replaceStatement(target, bb1, true);

  // TODO handle preprocessing information
  //  Save some preprocessing information for later restoration.
  //   AttachedPreprocessingInfoType ppi_before, ppi_after;
  //   ASTtools::cutPreprocInfo (s, PreprocessingInfo::before, ppi_before);
  //   ASTtools::cutPreprocInfo (s, PreprocessingInfo::after, ppi_after);

  // Declare local loop control variables: _p_loop_index _p_loop_lower
  // _p_loop_upper , no change to the original stride
  SgType *loop_var_type = NULL;
  // Use 64-bit loop controls only when the target ABI requires it.
  if (for_loop) {
    bool use_64bit_loop_vars =
        use_kmpc_loop_64bit_runtime(buildLongType(), target);
    if (use_64bit_loop_vars)
      loop_var_type = buildLongType();
    else
      loop_var_type = buildIntType();
  } else if (do_loop) // No long integer in Fortran
    loop_var_type = buildIntType();
  SgVariableDeclaration *index_decl = NULL;
  SgVariableDeclaration *lower_decl = NULL;
  SgVariableDeclaration *upper_decl = NULL;
  SgVariableDeclaration *last_iter_decl = NULL;
  SgVariableDeclaration *stride_decl = NULL;

  if (SageInterface::is_Fortran_language()) { // special rules to insert
                                              // variable declarations in
                                              // Fortran
    // They have to be inserted to enclosing function body or enclosing parallel
    // region body and after existing declaration statement sequence, if any.
    nCounter++;
    index_decl = buildAndInsertDeclarationForOmp(
        "p_index_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    lower_decl = buildAndInsertDeclarationForOmp(
        "p_lower_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    upper_decl = buildAndInsertDeclarationForOmp(
        "p_upper_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    stride_decl = buildAndInsertDeclarationForOmp(
        "p_stride_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    last_iter_decl = buildAndInsertDeclarationForOmp(
        "p_last_iter_" + StringUtility::numberToString(nCounter),
        buildIntType(), NULL, bb1);
  } else {
    index_decl = buildVariableDeclaration("__index_", loop_var_type, NULL, bb1);
    lower_decl = buildVariableDeclaration(
        "__lower_", loop_var_type, buildAssignInitializer(orig_lower), bb1);
    upper_decl = buildVariableDeclaration(
        "__upper_", loop_var_type, buildAssignInitializer(orig_upper), bb1);
    stride_decl = buildVariableDeclaration(
        "__stride_", loop_var_type, buildAssignInitializer(orig_stride), bb1);
    last_iter_decl =
        buildVariableDeclaration("__last_iter_", buildIntType(),
                                 buildAssignInitializer(buildIntVal(0)), bb1);

    appendStatement(index_decl, bb1);
    appendStatement(lower_decl, bb1);
    appendStatement(upper_decl, bb1);
    appendStatement(stride_decl, bb1);
    appendStatement(last_iter_decl, bb1);
  }

  if (SageInterface::is_Fortran_language()) {
    // LLVM runtime loop init expects input lower/upper/stride to be
    // initialized.
    appendStatement(buildAssignStatement(buildVarRefExp(lower_decl),
                                         copyExpression(orig_lower)),
                    bb1);
    appendStatement(buildAssignStatement(buildVarRefExp(upper_decl),
                                         copyExpression(orig_upper)),
                    bb1);
    appendStatement(
        buildAssignStatement(buildVarRefExp(stride_decl),
                             createAdjustedStride(orig_stride, isIncremental)),
        bb1);
    appendStatement(
        buildAssignStatement(buildVarRefExp(last_iter_decl), buildIntVal(0)),
        bb1);
  }

  bool hasOrder = false;
  if (hasClause(target, V_SgOmpOrderedClause))
    hasOrder = true;

  // Grab or calculate chunk_size
  //    SgExpression* my_chunk_size = NULL;
  bool hasSpecifiedSize = false;
  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(target, V_SgOmpScheduleClause);
  if (clauses.size() != 0) {
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    // SgOmpClause::omp_schedule_kind_enum s_kind = s_clause->get_kind();
    //  ROSE_ASSERT(s_kind == SgOmpClause::e_omp_schedule_static);
    SgExpression *orig_chunk_size = s_clause->get_chunk_size();
    //  ROSE_ASSERT(orig_chunk_size->get_parent() != NULL);
    if (orig_chunk_size) {
      hasSpecifiedSize = true;
      // my_chunk_size = orig_chunk_size;
    }
  }

  const bool use_64_runtime =
      for_loop != NULL && use_kmpc_loop_64bit_runtime(
                              getFirstVariable(*lower_decl).get_type(), target);

  //  step 3. Translation for omp for
  if (!useStaticSchedule(target) || hasOrder || hasSpecifiedSize) {
    transOmpLoop_others(target, index_decl, lower_decl, upper_decl, stride_decl,
                        last_iter_decl, bb1);
  } else {
    SgStatement *kmpc_global_tid_init = NULL;
    SgVariableDeclaration *kmpc_global_tid_declaration =
        get_kmpc_global_tid(node, bb1, &kmpc_global_tid_init);
    SgExpression *thread_global_tid = buildVarRefExp(
        getFirstVariable(*kmpc_global_tid_declaration).get_name(), bb1);
    if (SageInterface::is_Fortran_language())
      insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                                bb1);
    else
      appendStatement(kmpc_global_tid_declaration, bb1);
    if (kmpc_global_tid_init != NULL)
      appendStatement(kmpc_global_tid_init, bb1);

    // void XOMP_loop_default(int lower, int upper, int stride, long *n_lower,
    // long * n_upper)
    //  XOMP_loop_default (lower, upper, stride, &_p_lower, &_p_upper );
    //  lower:  copyExpression(orig_lower)
    //  upper: copyExpression(orig_upper)
    //  stride: copyExpression(orig_stride)
    //  n_lower: buildVarRefExp(lower_decl)
    //  n_upper: buildVarRefExp(upper_decl)
    SgExpression *e4 = NULL;
    SgExpression *e5 = NULL;
    if (for_loop) {
      e4 = buildAddressOfOp(buildVarRefExp(lower_decl));
      e5 = buildAddressOfOp(buildVarRefExp(upper_decl));
    } else if (do_loop) { // Fortran, pass-by-reference by default
      e4 = buildVarRefExp(lower_decl);
      e5 = buildVarRefExp(upper_decl);
    }
    ROSE_ASSERT(e4 && e5);
    // by default, LLVM uses 34 as the scheduling policy enum
    SgExpression *schedule_type = buildIntVal(kmp_sched_static_nochunk);
    SgExpression *e_last_iter =
        buildAddressOfOp(buildVarRefExp(last_iter_decl));
    SgExpression *e_stride = buildAddressOfOp(buildVarRefExp(stride_decl));
    if (do_loop) {
      // Fortran call arguments are already passed by reference.
      e_last_iter = buildVarRefExp(last_iter_decl);
      e_stride = buildVarRefExp(stride_decl);
    }
    parameters = buildExprListExp(source_location_info, thread_global_tid,
                                  schedule_type, e_last_iter, e4, e5, e_stride,
                                  copyExpression(orig_stride), buildIntVal(1));
    SgStatement *call_stmt =
        buildFunctionCallStmt(get_kmpc_for_static_init_name(use_64_runtime),
                              buildVoidType(), parameters, bb1);
    appendStatement(call_stmt, bb1);

    // insert the upper bound checking
    SgExpression *stride_positive =
        buildGreaterThanOp(copyExpression(orig_stride), buildIntVal(0));
    SgExpression *stride_negative =
        buildLessThanOp(copyExpression(orig_stride), buildIntVal(0));
    SgExpression *incremental_clamp = buildAndOp(
        stride_positive, buildGreaterThanOp(buildVarRefExp(upper_decl),
                                            copyExpression(orig_upper)));
    SgExpression *decremental_clamp = buildAndOp(
        stride_negative, buildLessThanOp(buildVarRefExp(upper_decl),
                                         copyExpression(orig_upper)));
    SgExpression *if_condition =
        buildOrOp(incremental_clamp, decremental_clamp);
    SgExprStatement *update_upper_bound_stmt = buildAssignStatement(
        buildVarRefExp(upper_decl), copyExpression(orig_upper));
    SgStatement *if_true_body = update_upper_bound_stmt;
    if (SageInterface::is_Fortran_language()) {
      SgBasicBlock *if_body = buildBasicBlock();
      appendStatement(update_upper_bound_stmt, if_body);
      if_true_body = if_body;
    }
    SgIfStmt *if_statement = buildIfStmt(if_condition, if_true_body, NULL);
    appendStatement(if_statement, bb1);

    // add loop here
    SgStatement *new_loop = deepCopy(loop);
    appendStatement(new_loop, bb1);
    // replace loop index with the new one
    replaceVariableReferences(
        new_loop,
        isSgVariableSymbol(orig_index->get_symbol_from_symbol_table()),
        getFirstVarSym(index_decl));
    // rewrite the lower and upper bounds
    SageInterface::setLoopLowerBound(new_loop, buildVarRefExp(lower_decl));
    SageInterface::setLoopUpperBound(new_loop, buildVarRefExp(upper_decl));

    transOmpVariables(
        target, bb1,
        orig_upper); // This should happen before the barrier is inserted.
    parameters = buildExprListExp(buildIntVal(0), thread_global_tid);
    appendStatement(buildFunctionCallStmt("__kmpc_for_static_fini",
                                          buildVoidType(), parameters, bb1),
                    bb1);
    // insert barrier if there is no nowait clause
    if (!hasClause(target, V_SgOmpNowaitClause)) {
      appendStatement(buildFunctionCallStmt("__kmpc_barrier", buildVoidType(),
                                            parameters, bb1),
                      bb1);
    }
  }

} // end trans omp for

//! Translate omp for or omp do loops affected by the "omp target" directive,
//! Liao 1/28/2013
/*

Example:
// for (i = 0; i < N; i++)
{ // top level block, prepare to be outlined.
// int i ; // = blockDim.x * blockIdx.x + threadIdx.x; // this CUDA declaration
can be inserted later i = getLoopIndexFromCUDAVariables(1);

if (i<SIZE)  // boundary checking to avoid invalid memory accesses
{
for (j = 0; j < M; j++)
for (k = 0; k < K; k++)
c[i][j]= c[i][j]+a[i][k]*b[k][j];
}
} // end of top level block

Algorithm:
 * check if it is a OmpTargetLoop
 * loop normalization
 * replace OmpForStatement with a block: bb1
 * declare int _dev_i within bb1;  replace for loop body’s loop index with
_dev_i;
 * build if stmt with correct condition
 * move loop body to if-stmt’s true body
 * remove for_loop
 */
void transOmpTargetLoop(SgNode *node) {
  // step 0: Sanity check
  ROSE_ASSERT(node != NULL);
  SgOmpForStatement *target1 = isSgOmpForStatement(node);
  SgOmpDoStatement *target2 = isSgOmpDoStatement(node);

  SgOmpClauseBodyStatement *target =
      (target1 != NULL ? (SgOmpClauseBodyStatement *)target1
                       : (SgOmpClauseBodyStatement *)target2);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // The OpenMP syntax requires that the omp for pragma is immediately followed
  // by the for loop.
  SgForStatement *for_loop = isSgForStatement(body);
  SgFortranDo *do_loop = isSgFortranDo(body);

  SgStatement *loop =
      (for_loop != NULL ? (SgStatement *)for_loop : (SgStatement *)do_loop);
  ROSE_ASSERT(loop != NULL);

  // make sure this is really a loop affected by "omp target"
  // bool is_target_loop = false;
  SgNode *parent = node->get_parent();
  ROSE_ASSERT(parent != NULL);
  if (isSgBasicBlock(
          parent)) // skip one possible BB between omp parallel and omp for.
    parent = parent->get_parent();
  SgNode *grand_parent = parent->get_parent();
  ROSE_ASSERT(grand_parent != NULL);
  SgOmpParallelStatement *parent_parallel = isSgOmpParallelStatement(parent);
  SgOmpTargetStatement *grand_target = isSgOmpTargetStatement(grand_parent);
  ROSE_ASSERT(parent_parallel != NULL);
  ROSE_ASSERT(grand_target != NULL);

  // Step 1. Loop normalization
  // For the init statement: for (int i=0;... ) becomes int i; for (i=0;..)
  // For test expression: i<x is normalized to i<= (x-1) and i>x is normalized
  // to i>= (x+1) For increment expression: i++ is normalized to i+=1 and i-- is
  // normalized to i+=-1 i-=s is normalized to i+= -s
  if (for_loop)
    SageInterface::forLoopNormalization(for_loop);
  else if (do_loop)
    SageInterface::doLoopNormalization(do_loop);
  else {
    cerr << "error! transOmpLoop(). loop is neither for_loop nor do_loop. "
            "Aborting.."
         << endl;
    ROSE_ABORT();
  }

  SgInitializedName *orig_index = NULL;
  SgExpression *orig_lower = NULL;
  SgExpression *orig_upper = NULL;
  SgExpression *orig_stride = NULL;
  bool isIncremental = true; // if the loop iteration space is incremental
  // grab the original loop 's controlling information
  bool is_canonical = false;

  if (for_loop)
    is_canonical =
        isCanonicalForLoop(for_loop, &orig_index, &orig_lower, &orig_upper,
                           &orig_stride, NULL, &isIncremental);
  else if (do_loop)
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, &isIncremental, NULL);
  ROSE_ASSERT(is_canonical == true);

  // also make sure the loop body is a block
  // TODO: we consider peeling off 1 level loop control only, need to be
  // conditional on what the spec. can provide at pragma level
  // TODO: Fortran support later on
  ROSE_ASSERT(for_loop != NULL);
  SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(for_loop);

  // Step 2. Insert a basic block to replace SgOmpForStatement
  // This newly introduced scope is used to hold loop variables ,etc
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();
  replaceStatement(target, bb1, true);

  // Step 3. Using device thread id and replace reference of original loop index
  // with the thread index
  //  Declare device thread id variable
  // int i = blockDim.x * blockIdx.x + threadIdx.x;
  // SgAssignInitializer* init_idx =  buildAssignInitializer(
  //                                      buildAddOp( buildMultiplyOp
  //                                      (buildVarRefExp("blockDim.x"),
  //                                      buildVarRefExp("blockIdx.x")) ,
  //                                       buildVarRefExp("threadIdx.x", bb1)));
  // Better build of CUDA variables within a runtime library call so these
  // variables are hidden from the translation
  //   getLoopIndexFromCUDAVariables(1)
  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getLoopIndexFromCUDAVariables"),
                           buildIntType(), buildExprListExp(buildIntVal(1)),
                           bb1),
      buildIntType());

  SgVariableDeclaration *dev_i_decl =
      buildVariableDeclaration("_dev_i", buildIntType(), init_idx, bb1);
  prependStatement(dev_i_decl, bb1);
  SgVariableSymbol *dev_i_symbol = getFirstVarSym(dev_i_decl);
  ROSE_ASSERT(dev_i_symbol != NULL);

  // replace reference to loop index with reference to device i variable
  ROSE_ASSERT(orig_index != NULL);
  SgSymbol *orig_symbol = orig_index->get_symbol_from_symbol_table();
  ROSE_ASSERT(orig_symbol != NULL);

  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(loop_body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    if (vRef->get_symbol() == orig_symbol)
      vRef->set_symbol(dev_i_symbol);
  }

  // Step 4. build the if () condition statement, move the loop body into the
  // true body Liao, 2/21/2013. We must be accurate about the range of
  // iterations or the computation may result in WRONG results!! A classic
  // example is the Jacobi iteration: in which the first and last iterations are
  // not executed to make sure elements have boundaries. After normalization, we
  // have inclusive lower and upper bounds of the input loop the condition of
  // if() should look like something: if (_dev_i >=0+1 &&_dev_i <= (n - 1) - 1)
  // {...}
  SgBasicBlock *true_body = buildBasicBlock();
  SgExprStatement *cond_stmt = NULL;
  if (isIncremental) {
    SgExpression *lhs = buildGreaterOrEqualOp(buildVarRefExp(dev_i_symbol),
                                              deepCopy(orig_lower));
    SgExpression *rhs =
        buildLessOrEqualOp(buildVarRefExp(dev_i_symbol), deepCopy(orig_upper));
    cond_stmt = buildExprStatement(buildAndOp(lhs, rhs));
  } else {
    cerr << "error. transOmpTargetLoop(): decremental case is not yet handled !"
         << endl;
    ROSE_ABORT();
  }
  SgIfStmt *if_stmt = buildIfStmt(cond_stmt, true_body, NULL);
  appendStatement(if_stmt, bb1);
  moveStatementsBetweenBlocks(loop_body, true_body);
  // Peel off the original loop
  removeStatement(for_loop);

  // handle private variables at this loop level, mostly loop index variables.
  // TODO: this is not very elegant since the outer most loop's loop variable is
  // still translated.
  GpuOffloadLoweringContext offload_ctx;
  transOmpVariablesWithContext(target, bb1, NULL, true, &offload_ctx);
}

//! Translate omp for or omp do loops affected by the "omp target" directive,
//! using a round robin-scheduler Liao 7/10/2014
/*  Algorithm

// original loop info. grab from the loop structure
int orig_start =0;
int orig_end = n-1; // inclusive upper bound
int orig_step = 1;
int orig_chunk_size = 1;// fixed at 1

// new lower and upper bound, to be filled out by the loop scheduler
int _dev_lower;
int _dev_upper;
int _dev_loop_chunk_size;
int _dev_loop_sched_index;
int _dev_loop_stride;

// CUDA thread count and ID for the 1-D block
int _dev_thread_num = getCUDABlockThreadCount(1);
int _dev_thread_id = getLoopIndexFromCUDAVariables(1);

//initialize scheduler
XOMP_static_sched_init (orig_start, orig_end, orig_step, orig_chunk_size,
_dev_thread_num, _dev_thread_id, \ & _dev_loop_chunk_size , &
_dev_loop_sched_index, & _dev_loop_stride);

while (XOMP_static_sched_next (&_dev_loop_sched_index, orig_end,
orig_step,_dev_loop_stride, _dev_loop_chunk_size, _dev_thread_num,
_dev_thread_id, & _dev_lower , & _dev_upper))
{
for (i= _dev_lower ; i <= _dev_upper; i ++ ) { // rewrite lower and upper bound
and step normalized to 1
// original loop body here
}
}
}

*/
void transOmpTargetLoop_RoundRobin(SgNode *node) {
  // step 0: Sanity check
  ROSE_ASSERT(node != NULL);
  SgOmpForStatement *target1 = isSgOmpForStatement(node);
  SgOmpDoStatement *target2 = isSgOmpDoStatement(node);

  // the target of the translation is a SgOmpForStatement
  SgOmpClauseBodyStatement *target =
      (target1 != NULL ? (SgOmpClauseBodyStatement *)target1
                       : (SgOmpClauseBodyStatement *)target2);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // The OpenMP syntax requires that the omp for pragma is immediately followed
  // by the for loop.
  SgForStatement *for_loop = isSgForStatement(body);
  SgFortranDo *do_loop = isSgFortranDo(body);

  SgStatement *loop =
      (for_loop != NULL ? (SgStatement *)for_loop : (SgStatement *)do_loop);
  ROSE_ASSERT(loop != NULL);

  // make sure this is really a loop affected by "omp target"
  // bool is_target_loop = false;
  SgNode *parent = node->get_parent();
  ROSE_ASSERT(parent != NULL);
  if (isSgBasicBlock(
          parent)) // skip one possible BB between omp parallel and omp for.
    parent = parent->get_parent();
  SgNode *grand_parent = parent->get_parent();
  if (isSgBasicBlock(grand_parent)) // skip one possible BB between omp target
                                    // and omp parallel.
    grand_parent = grand_parent->get_parent();
  ROSE_ASSERT(grand_parent != NULL);
  SgOmpParallelStatement *parent_parallel = isSgOmpParallelStatement(parent);
  SgOmpTargetStatement *grand_target = isSgOmpTargetStatement(grand_parent);
  ROSE_ASSERT(parent_parallel != NULL);
  ROSE_ASSERT(grand_target != NULL);

  // Step 1. Loop normalization
  // For the init statement: for (int i=0;... ) becomes int i; for (i=0;..)
  // For test expression: i<x is normalized to i<= (x-1) and i>x is normalized
  // to i>= (x+1) For increment expression: i++ is normalized to i+=1 and i-- is
  // normalized to i+=-1 i-=s is normalized to i+= -s
  if (for_loop)
    SageInterface::forLoopNormalization(for_loop);
  else if (do_loop)
    SageInterface::doLoopNormalization(do_loop);
  else {
    cerr << "error! transOmpLoop(). loop is neither for_loop nor do_loop. "
            "Aborting.."
         << endl;
    ROSE_ABORT();
  }

  SgInitializedName *orig_index = NULL;
  SgExpression *orig_lower = NULL;
  SgExpression *orig_upper = NULL;
  SgExpression *orig_stride = NULL;
  bool isIncremental = true; // if the loop iteration space is incremental
  // grab the original loop 's controlling information
  bool is_canonical = false;

  if (for_loop)
    is_canonical =
        isCanonicalForLoop(for_loop, &orig_index, &orig_lower, &orig_upper,
                           &orig_stride, NULL, &isIncremental);
  else if (do_loop)
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, &isIncremental, NULL);
  ROSE_ASSERT(is_canonical == true);

  // TODO: Fortran support later on
  ROSE_ASSERT(for_loop != NULL);
  // SgBasicBlock* loop_body = ensureBasicBlockAsBodyOfFor (for_loop);

  // Step 2. Insert a basic block to replace SgOmpForStatement
  // This newly introduced scope is used to hold loop variables ,etc
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();
  replaceStatement(target, bb1, true);

  // Insert variables used by the two scheduler functions
  /* int _dev_lower;
     int _dev_upper;
     int _dev_loop_chunk_size;
     int _dev_loop_sched_index;
     int _dev_loop_stride;
  */
  SgVariableDeclaration *dev_lower_decl =
      buildVariableDeclaration("_dev_lower", buildIntType(), NULL, bb1);
  appendStatement(dev_lower_decl, bb1);
  SgVariableDeclaration *dev_upper_decl =
      buildVariableDeclaration("_dev_upper", buildIntType(), NULL, bb1);
  appendStatement(dev_upper_decl, bb1);
  SgVariableDeclaration *dev_loop_chunk_size_decl = buildVariableDeclaration(
      "_dev_loop_chunk_size", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_chunk_size_decl, bb1);
  SgVariableDeclaration *dev_loop_sched_index_decl = buildVariableDeclaration(
      "_dev_loop_sched_index", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_sched_index_decl, bb1);
  SgVariableDeclaration *dev_loop_stride_decl =
      buildVariableDeclaration("_dev_loop_stride", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_stride_decl, bb1);

  // Insert CUDA thread id and count declarations
  // int _dev_thread_num = getCUDABlockThreadCount(1);
  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getCUDABlockThreadCount"), buildIntType(),
                           buildExprListExp(buildIntVal(1)), bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_num_decl = buildVariableDeclaration(
      "_dev_thread_num", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_num_decl, bb1);
  SgVariableSymbol *dev_thread_num_symbol = getFirstVarSym(dev_thread_num_decl);
  ROSE_ASSERT(dev_thread_num_symbol != NULL);

  // int _dev_thread_id = getLoopIndexFromCUDAVariables(1);
  init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getLoopIndexFromCUDAVariables"),
                           buildIntType(), buildExprListExp(buildIntVal(1)),
                           bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_id_decl =
      buildVariableDeclaration("_dev_thread_id", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_id_decl, bb1);
  SgVariableSymbol *dev_thread_id_symbol = getFirstVarSym(dev_thread_id_decl);
  ROSE_ASSERT(dev_thread_id_symbol != NULL);

  // initialize scheduler
  // XOMP_static_sched_init (orig_start, orig_end, orig_step, orig_chunk_size,
  // _dev_thread_num, _dev_thread_id,
  //                       & _dev_loop_chunk_size , & _dev_loop_sched_index, &
  //                       _dev_loop_stride);
  SgExprListExp *parameters =
      buildExprListExp(copyExpression(orig_lower), copyExpression(orig_upper),
                       copyExpression(orig_stride), buildIntVal(1),
                       buildVarRefExp(dev_thread_num_symbol),
                       buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_chunk_size_decl))));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_sched_index_decl))));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_stride_decl))));
  SgStatement *call_stmt = buildFunctionCallStmt(
      "XOMP_static_sched_init", buildVoidType(), parameters, bb1);
  appendStatement(call_stmt, bb1);

  // function call exp as while (condition)
  // XOMP_static_sched_next (&_dev_loop_sched_index, orig_end,
  // orig_step,_dev_loop_stride, _dev_loop_chunk_size,
  //                       _dev_thread_num, _dev_thread_id, & _dev_lower , &
  //                       _dev_upper)
  parameters = buildExprListExp(
      buildAddressOfOp(
          buildVarRefExp(getFirstVarSym(dev_loop_sched_index_decl))),
      copyExpression(orig_upper), copyExpression(orig_stride),
      buildVarRefExp(getFirstVarSym(dev_loop_stride_decl)),
      buildVarRefExp(getFirstVarSym(dev_loop_chunk_size_decl)));
  appendExpression(parameters, buildVarRefExp(dev_thread_num_symbol));
  appendExpression(parameters, buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_lower_decl))));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_upper_decl))));
  SgExpression *func_call_exp = buildFunctionCallExp(
      "XOMP_static_sched_next", buildBoolType(), parameters, bb1);

  SgStatement *new_loop = deepCopy(for_loop);
  SgWhileStmt *w_stmt = buildWhileStmt(func_call_exp, new_loop);
  appendStatement(w_stmt, bb1);
  //  moveStatementsBetweenBlocks (loop_body,
  //  isSgBasicBlock(w_stmt->get_body()));

  // rewrite upper, lower bounds, TODO how about step? normalized to 1 already ?
  setLoopLowerBound(new_loop, buildVarRefExp(getFirstVarSym(dev_lower_decl)));
  setLoopUpperBound(new_loop, buildVarRefExp(getFirstVarSym(dev_upper_decl)));
  removeStatement(for_loop);

  // handle private variables at this loop level, mostly loop index variables.
  // TODO: this is not very elegant since the outer most loop's loop variable is
  // still translated.
  GpuOffloadLoweringContext offload_ctx;
  transOmpVariablesWithContext(target, bb1, NULL, true, &offload_ctx);
}

//! Check if an OpenMP statement has a clause of type vvt
Rose_STL_Container<SgOmpClause *> getClause(SgStatement *clause_stmt,
                                            const VariantVector &vvt) {
  ROSE_ASSERT(clause_stmt != NULL);
  SgOmpClausePtrList clauses;
  if (isSgOmpClauseBodyStatement(clause_stmt)) {
    clauses = (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses();
  } else if (isSgOmpClauseStatement(clause_stmt)) {
    clauses = (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  } else {
    ROSE_ABORT();
  };
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clauses, vvt);
  return p_clause;
}

//! Check if an OpenMP statement has a clause of type vt
Rose_STL_Container<SgOmpClause *> getClause(SgStatement *clause_stmt,
                                            const VariantT &vt) {
  return getClause(clause_stmt, VariantVector(vt));
}

//! Check if an OpenMP statement has a clause of type vt
bool hasClause(SgStatement *clause_stmt, const VariantT &vt) {
  ROSE_ASSERT(clause_stmt != NULL);
  SgOmpClausePtrList clauses;
  if (isSgOmpClauseBodyStatement(clause_stmt)) {
    clauses = (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses();
  } else if (isSgOmpClauseStatement(clause_stmt)) {
    clauses = (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  } else {
    ROSE_ABORT();
  };
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clauses, vt);
  return (p_clause.size() != 0);
}

//! A helper function to generate implicit or explicit task for either omp
//! parallel or omp task
//  Parameters:  SgNode* node: the OMP Parallel or OMP Parallel
//               std::string& wrapper_name: for C/C++, structure wrapper is used
//               to wrap all parameters. This is to return the struct name
//               ASTtools::VarSymSet_t& syms :  all variables to be passed
//               in/out the outlined function ASTtools::VarSymSet_t&pdSyms3 :
//               variables which must be passed by references, used to guide the
//               creation of struct wrapper: member using base type vs. using
//               pointer type.  The algorithm to generate this set is already
//               very conservative: after transOmpVariables() , the only exclude
//               firstprivate. In the context of OpenMP, it is equivalent to say
//               this is a set of variables which are to be passed by
//               references.
// Algorithms:
//    Set flags of the outliner to indicate desired behaviors: parameter
//    wrapping or not? translate OpenMP variables (first private, private,
//    reduction, etc) so the code to be outlined is already as simple as
//    possible (without OpenMP-specific semantics)
//
// It calls the ROSE AST outliner internally.
SgFunctionDeclaration *generateOutlinedTask(SgNode *node,
                                            std::string &wrapper_name,
                                            ASTtools::VarSymSet_t &syms,
                                            ASTtools::VarSymSet_t &pdSyms3,
                                            bool use_task_param,
                                            bool insert_runtime_ids) {
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);

  // must be either omp task or omp parallel
  SgOmpTaskStatement *target1 = isSgOmpTaskStatement(node);
  SgOmpParallelStatement *target2 = isSgOmpParallelStatement(node);
  ROSE_ASSERT(target1 != NULL || target2 != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // Outliner::preprocess() only accepts a subset of statement kinds.  Parallel
  // and task bodies can legally be a bare OpenMP directive (e.g., omp single),
  // so normalize to a basic block first and lower nested directives later.
  if (isSgBasicBlock(body) == NULL) {
    SgOmpBodyStatement *body_stmt = isSgOmpBodyStatement(target);
    ROSE_ASSERT(body_stmt != NULL);
    body = ensureBasicBlockAsBodyOfOmpBodyStmt(body_stmt);
  }
  SgFunctionDeclaration *result = NULL;
  // Initialize outliner
  Outliner::enable_classic = false; // we need use parameter wrapping, which is
                                    // not classic behavior of outlining
  // We pass one variable per parameter, at least for Fortran 77.
  // For both C/C++ and Fortran, we use the same method to pass parameters
  // separately instead of a struct or array wrapper.
  Outliner::useParameterWrapper = false;

  // TODO there should be some semantics check for the regions to be outlined
  // for example, multiple entries or exists are not allowed for OpenMP
  // This is however of low priority since most vendor compilers have this
  // already.
  SgBasicBlock *body_block = Outliner::preprocess(body);

  //---------------------------------------------------------------
  //  Key step: handling special variables BEFORE actual outlining is done!!
  // Variable handling is done after Outliner::preprocess() to ensure a basic
  // block for the body, but before calling the actual outlining This simplifies
  // the outlining since firstprivate, private variables are replaced
  // with their local copies before outliner is used
  transOmpVariables(target, body_block);

  // Normalize symbol links introduced by clause-variable rewrites before
  // collecting outlined captures.
  SageInterface::fixVariableReferences(body_block);

  // variable sets for private, firstprivate, reduction, and pointer
  // dereferencing (pd)
  ASTtools::VarSymSet_t pSyms, fpSyms, reductionSyms, pdSyms;

  string func_name = Outliner::generateFuncName(target);

  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  //-----------------------------------------------------------------
  // Generic collection of variables to be passed as parameters of the outlined
  // functions semantically equivalent to shared variables in OpenMP
  Outliner::collectVars(body_block, syms);

  // Now decide on the parameter convention for all the parameters:
  // pass-by-value vs. pass-by-reference (pointer dereferencing)

  //     SageInterface::collectReadOnlyVariables(body_block,readOnlyVars);
  // We choose to be conservative about the variables needing pointer
  // dereferencing first AllParameters - readOnlyVars  - private -firstprivate
  // Union ASTtools::collectPointerDereferencingVarSyms(body_block, pdSyms)

  // Assume all parameters need to be passed by reference/pointers first
  std::copy(syms.begin(), syms.end(), std::inserter(pdSyms, pdSyms.begin()));

  // exclude firstprivate variables: they are read only in fact
  // TODO keep class typed variables!!!  even if they are firstprivate or
  // private!!
  SgInitializedNamePtrList fp_vars =
      collectClauseVariables(target, V_SgOmpFirstprivateClause);
  ASTtools::VarSymSet_t fp_syms, pdSyms2;
  convertAndFilter(fp_vars, fp_syms);
  set_difference(pdSyms.begin(), pdSyms.end(), fp_syms.begin(), fp_syms.end(),
                 std::inserter(pdSyms2, pdSyms2.begin()));
  //  ROSE_ASSERT (pdSyms.size() == pdSyms2.size());  this means the previous
  //  set_difference is neccesary !

  pdSyms3 = pdSyms2;

  // lastprivate and reduction variables cannot be excluded  since write access
  // to their shared copies

  // Sara Royuela 24/04/2012
  // When unpacking array variables in the outlined function, it is needed to
  // have access to the size of the array. When this size is a variable (or a
  // operation containing variables), this variable must be added to the
  // arguments of the outlined function. Example:
  //    Input snippet:                      Outlined function:
  //        int N = 1;                          static void OUT__1__5493__(void
  //        *__out_argv) { int a[N];                               int (*a)[N] =
  //        (int (*)[N])(((struct OUT__1__5493___data *)__out_argv) -> a_p);
  //        #pragma omp task shared(a)              ( *a)[0] = 1;
  //            a[0] = 1;                       }
  ASTtools::VarSymSet_t new_syms;
  for (ASTtools::VarSymSet_t::const_iterator i = syms.begin(); i != syms.end();
       ++i) {
    SgType *i_type = (*i)->get_declaration()->get_type();

    while (isSgArrayType(i_type)) {
      // Get most significant dimension
      SgExpression *index = ((SgArrayType *)i_type)->get_index();

      // Get the variables used to compute the dimension
      // FIXME We insert a new statement and delete it afterwards in order to
      // use "collectVars" function
      //       Think about implementing an specific function for expressions
      ASTtools::VarSymSet_t a_syms, a_pSyms;
      SgExprStatement *index_stmt = buildExprStatement(index);
      appendStatement(index_stmt, body_block);
      Outliner::collectVars(index_stmt, a_syms);
      SageInterface::removeStatement(index_stmt);
      for (ASTtools::VarSymSet_t::iterator j = a_syms.begin();
           j != a_syms.end(); ++j) {
        const SgVariableSymbol *s = *j;
        new_syms.insert(
            s); // If the symbol is not in the symbol list, it is added
      }

      // Advance over the type
      i_type = ((SgArrayType *)i_type)->get_base_type();
    }
  }

  for (ASTtools::VarSymSet_t::const_iterator i = new_syms.begin();
       i != new_syms.end(); ++i) {
    const SgVariableSymbol *s = *i;
    syms.insert(s);
  }

  // a data structure used to wrap parameters
  SgClassDeclaration *struct_decl = NULL;

  // Generate the outlined function
  /* Parameter list
       SgBasicBlock* s,  // block to be outlined
       const string& func_name_str, // function name
       const ASTtools::VarSymSet_t& syms, // parameter list for all variables to
    be passed around const ASTtools::VarSymSet_t& pdSyms, // variables must use
    pointer dereferencing (pass-by-reference) const ASTtools::VarSymSet_t&
    psyms, // private or dead variables (not live-in, not live-out)
       SgClassDeclaration* struct_decl,  // an optional wrapper structure for
    parameters Depending on the internal flag, unpacking/unwrapping statements
    are generated inside the outlined function to use wrapper parameters.
  */
  std::set<SgInitializedName *> restoreVars;
  result = Outliner::generateFunction(body_block, func_name, syms, pdSyms3,
                                      restoreVars, struct_decl, g_scope);

  if (insert_runtime_ids) {
    SgPointerType *int_pointer_type = buildPointerType(SgTypeInt::createType());
    SgType *thread_id_type = SageInterface::is_Fortran_language()
                                 ? buildIntType()
                                 : static_cast<SgType *>(int_pointer_type);
    // insert the kmpc ids as the first two parameters
    if (use_task_param) {
      auto *taskType = buildOpaqueType("ptask", g_scope);
      insert_function_parameter("task", taskType, result, false);
    } else {
      insert_function_parameter("__bound_tid", thread_id_type, result, false);
    }

    insert_function_parameter("__global_tid", thread_id_type, result, false);
  }

  if (SageInterface::is_Fortran_language()) {
    normalize_fortran_parallel_outlined_pointer_formals(result, syms);
  }

  // insert the forward declaration
  Outliner::insert(result, g_scope, body_block);

  // Generate packing statements
  // must pass target , not body_block to get the right scope in which the
  // declarations are inserted
  if (!SageInterface::is_Fortran_language())
    wrapper_name =
        Outliner::generatePackingStatements(target, syms, pdSyms3, struct_decl);
  ROSE_ASSERT(result != NULL);

  // 12/7/2010
  // For Fortran outlined subroutines,
  // add INCLUDE 'omp_lib.h' in case OpenMP runtime routines are called within
  // the outlined subroutines
  if (SageInterface::is_Fortran_language()) {
    SgBasicBlock *body = result->get_definition()->get_body();
    ROSE_ASSERT(body != NULL);
    normalize_fortran_external_subroutine_declarations(body);
    SgFortranIncludeLine *inc_line = buildFortranIncludeLine("omp_lib.h");
    prependStatement(inc_line, body);
  }
  return result;
}

/* GCC's libomp uses the following translation method:
 *
 *
#include "libgomp_g.h"


#include <omp.h>

#include <stdio.h>

//void main_omp_fn_0 (struct _omp_data_s_0* _omp_data_i);
void main_omp_fn_0 (void ** __out_argv);

int main (void)
{
int i;
//  struct _omp_data_s_0 _omp_data_o_1;

i = 0;
// wrap shared variables
//  _omp_data_o_1.i = i;
void *__out_argv1__5876__[1];
__out_argv1__5876__[0] = ((void *)(&i));

//GOMP_parallel_start (main_omp_fn_0, &_omp_data_o_1, 0);
GOMP_parallel_start (main_omp_fn_0, &__out_argv1__5876__, 0); // must use &
here!!!
//main_omp_fn_0 (&_omp_data_o_1);
//main_omp_fn_0 ((void *)__out_argv1__5876__); //best type match
main_omp_fn_0 (__out_argv1__5876__);
GOMP_parallel_end ();

// grab the changed value
//  i = _omp_data_o_1.i;
return 0;
}

//void main_omp_fn_0(void *__out_argvp)
void main_omp_fn_0(void **__out_argv)
//void OUT__1__5876__(void **__out_argv)
{
// void **__out_argv = (void **) __out_argvp;
int *i = (int *)(__out_argv[0]);
 *i = omp_get_thread_num();
 printf("Hello,world! I am thread %d\n", *i);
 }
 */

void transOmpParallel(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpParallelStatement *target = isSgOmpParallelStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *cached_if_condition = NULL;
  if (hasClause(target, V_SgOmpIfClause)) {
    Rose_STL_Container<SgOmpClause *> if_clauses =
        getClause(target, V_SgOmpIfClause);
    ROSE_ASSERT(if_clauses.size() == 1);
    SgOmpIfClause *if_clause = isSgOmpIfClause(if_clauses[0]);
    ROSE_ASSERT(if_clause != NULL);
    ROSE_ASSERT(if_clause->get_expression() != NULL);
    cached_if_condition = copyExpression(if_clause->get_expression());
  }

  SgExpression *cached_num_threads = NULL;
  if (hasClause(target, V_SgOmpNumThreadsClause)) {
    Rose_STL_Container<SgOmpClause *> num_threads_clauses =
        getClause(target, V_SgOmpNumThreadsClause);
    ROSE_ASSERT(num_threads_clauses.size() == 1);
    SgOmpNumThreadsClause *num_threads_clause =
        isSgOmpNumThreadsClause(num_threads_clauses[0]);
    ROSE_ASSERT(num_threads_clause != NULL);
    ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
    cached_num_threads = copyExpression(num_threads_clause->get_expression());
  }

  // Liao 12/7/2010
  // For Fortran code, we have to insert EXTERNAL OUTLINED_FUNC into
  // the function body containing the parallel region
  SgFunctionDefinition *func_def = NULL;
  if (SageInterface::is_Fortran_language()) {
    func_def = getEnclosingFunctionDefinition(target);
    ROSE_ASSERT(func_def != NULL);
  }
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // The AST retains only the active branch after preprocessing. Carrying
  // conditional directives through outlining can therefore split unmatched
  // #if/#endif fragments across host and outlined functions.
  stripConditionalDirectivesFromSubtree(body);
  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner
  AttachedPreprocessingInfoType save_buf1, save_buf2;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);
  stripConditionalDirectivesFromList(save_buf1);
  stripConditionalDirectivesFromList(save_buf2);

  // some #endif may be attached to the body, we should not move it with the
  // body into the outlined funcion!! cutPreprocessingInfo(body,
  // PreprocessingInfo::before, save_buf_body) ;

  //-----------------------------------------------------------------
  // step 1: generated an outlined function as the task
  std::string wrapper_name;
  ASTtools::VarSymSet_t syms; // store all variables in the outlined task ???
  ASTtools::VarSymSet_t
      pdSyms3; // store all variables which should be passed by references (pd
               // means pointer dereferencing)
  std::set<SgInitializedName *>
      readOnlyVars; // not used since OpenMP provides all variable controlling
                    // details already. side effect analysis is essentially not
                    // being used.
  SgFunctionDeclaration *outlined_func =
      generateOutlinedTask(node, wrapper_name, syms, pdSyms3);

  if (SageInterface::is_Fortran_language()) { // EXTERNAL outlined_function ,
                                              // otherwise the function name
                                              // will be interpreted as a
                                              // integer/real variable
    ROSE_ASSERT(func_def != NULL);
    SgBasicBlock *func_body = func_def->get_body();
    ROSE_ASSERT(func_body != NULL);
    SgAttributeSpecificationStatement *external_stmt1 =
        buildAttributeSpecificationStatement(
            SgAttributeSpecificationStatement::e_externalStatement);
    SgFunctionRefExp *func_ref1 = buildFunctionRefExp(outlined_func);
    external_stmt1->get_parameter_list()->prepend_expression(func_ref1);
    func_ref1->set_parent(external_stmt1->get_parameter_list());
    // must put it into the declaration statement part, after possible
    // implicit/include statements, if any
    SgStatement *l_stmt = findLastDeclarationStatement(func_body);
    if (l_stmt)
      insertStatementAfter(l_stmt, external_stmt1);
    else
      prependStatement(external_stmt1, func_body);
  }

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  //-----------------------------------------------------------------
  // step 2: generate call to the outlined function

  // Generate the parameter list for the call to the XOMP runtime function
  SgExprListExp *parameters = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration = NULL;
  SgExpression *thread_global_tid = NULL;

  // add __kmpc_fork_call (0, 2, OUT_func_xxx, &a, &sum);
  // or __kmpc_fork_call (0, 0, OUT_func_xxx); // if no variables need to be
  // passed
  SgExpression *source_location_info = buildIntVal(0);
  SgExpression *outlined_function_parameter_amount = buildIntVal(syms.size());
  SgExpression *outlined_function_argument = buildFunctionRefExp(outlined_func);
  if (!SageInterface::is_Fortran_language()) {
    outlined_function_argument = buildCastExp(
        outlined_function_argument, buildOpaqueType("kmpc_micro_t", p_scope),
        SgCastExp::e_C_style_cast);
  }
  parameters =
      buildExprListExp(source_location_info, outlined_function_parameter_amount,
                       outlined_function_argument);
  ASTtools::VarSymSet_t::iterator iter;
  for (iter = syms.begin(); iter != syms.end(); iter++) {
    const SgVariableSymbol *sb = *iter;
    SgExpression *actual_arg = NULL;
    if (SageInterface::is_Fortran_language()) {
      actual_arg = buildVarRefExp(const_cast<SgVariableSymbol *>(sb));
    } else {
      actual_arg =
          buildAddressOfOp(buildVarRefExp(const_cast<SgVariableSymbol *>(sb)));
    }
    ROSE_ASSERT(actual_arg != NULL);
    appendExpression(parameters, actual_arg);
  }
  ROSE_ASSERT(parameters != NULL);

  // extern void XOMP_parallel_start (void (*func) (void *), void *data,
  // unsigned ifClauseValue, unsigned numThreadsSpecified);
  // * func: pointer to a function which will be run in parallel
  // * data: pointer to a data segment which will be used as the arguments of
  // func
  // * ifClauseValue: set to if-clause-expression if if-clause exists, or
  // default is 1.
  // * numThreadsSpecified: set to the expression of num_threads clause if the
  // clause exists, or default is 0

  SgStatement *outlined_function_call = buildFunctionCallStmt(
      "__kmpc_fork_call", buildVoidType(), parameters, p_scope);
  // the head of transformed code
  SgStatement *s1 = outlined_function_call;
  // the tail of transformed code
  SgStatement *s2 = s1;

  // if num_threads clause exists, we need to set up the omp number of threads
  // first. therefore, the head will be the function call of setting up
  // num_threads.
  SgExprStatement *set_num_threads_statement = NULL;
  SgExpression *omp_num_threads = cached_num_threads;
  if (omp_num_threads != NULL) {
    SgStatement *kmpc_global_tid_init = NULL;
    kmpc_global_tid_declaration =
        get_kmpc_global_tid(target, p_scope, &kmpc_global_tid_init);
    thread_global_tid = buildVarRefExp(
        getFirstVariable(*kmpc_global_tid_declaration).get_name(), p_scope);
    if (SageInterface::is_Fortran_language()) {
      insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                                p_scope);
    } else {
      insertStatement(target, kmpc_global_tid_declaration);
      kmpc_global_tid_declaration->set_parent(target->get_parent());
    }
    if (kmpc_global_tid_init != NULL) {
      if (SageInterface::is_Fortran_language())
        insertStatement(target, kmpc_global_tid_init);
      else
        insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
    }
    parameters =
        buildExprListExp(buildIntVal(0), thread_global_tid, omp_num_threads);
    set_num_threads_statement = buildFunctionCallStmt(
        "__kmpc_push_num_threads", buildVoidType(), parameters, p_scope);
    // set up the head of transformed code to num_threads setter
    // the tail is still the outlined function call
    s1 = set_num_threads_statement;
  };

  // transform the if clause
  // the head of transformed code will be the if statement in this case
  SgExpression *if_condition = cached_if_condition;
  if (if_condition != NULL) {
    if (omp_num_threads == NULL) {
      SgStatement *kmpc_global_tid_init = NULL;
      kmpc_global_tid_declaration =
          get_kmpc_global_tid(target, p_scope, &kmpc_global_tid_init);
      thread_global_tid = buildVarRefExp(
          getFirstVariable(*kmpc_global_tid_declaration).get_name(), p_scope);
      if (SageInterface::is_Fortran_language()) {
        insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                                  p_scope);
      } else {
        insertStatement(target, kmpc_global_tid_declaration);
        kmpc_global_tid_declaration->set_parent(target->get_parent());
      }
      if (kmpc_global_tid_init != NULL) {
        if (SageInterface::is_Fortran_language())
          insertStatement(target, kmpc_global_tid_init);
        else
          insertStatementAfter(kmpc_global_tid_declaration,
                               kmpc_global_tid_init);
      }
    };
    SgIfStmt *if_statement = buildIfStmt(if_condition, s1, NULL);
    SgExprStatement *else_stmt = NULL;
    SgBasicBlock *false_body = buildBasicBlock();
    if (SageInterface::is_Fortran_language()) {
      parameters =
          buildExprListExp(copyExpression(thread_global_tid), buildIntVal(0));
    } else {
      parameters =
          buildExprListExp(buildAddressOfOp(thread_global_tid), buildIntVal(0));
    }
    ASTtools::VarSymSet_t::iterator iter;
    for (iter = syms.begin(); iter != syms.end(); iter++) {
      const SgVariableSymbol *sb = *iter;
      SgExpression *actual_arg = NULL;
      if (SageInterface::is_Fortran_language()) {
        actual_arg = buildVarRefExp(const_cast<SgVariableSymbol *>(sb));
      } else {
        actual_arg = buildAddressOfOp(
            buildVarRefExp(const_cast<SgVariableSymbol *>(sb)));
      }
      ROSE_ASSERT(actual_arg != NULL);
      appendExpression(parameters, actual_arg);
    }
    else_stmt = buildFunctionCallStmt(outlined_func->get_name(),
                                      buildVoidType(), parameters, p_scope);
    false_body->append_statement(else_stmt);
    if_statement->set_false_body(false_body);
    false_body->set_parent(if_statement);

    // the head and tail are both changed to the if statement because all the
    // other transformed code are included as children of if statement
    s1 = if_statement;
    s2 = s1;
  };

  SageInterface::replaceStatement(target, s1, true);

  // Keep preprocessing information
  // I have to use cut-paste instead of direct move since
  // the preprocessing information may be moved to a wrong place during
  // outlining while the destination node is unknown until the outlining is
  // done.
  pastePreprocessingInfo(s1, PreprocessingInfo::before, save_buf1);

  // we can only set up the relationship between these two statements now,
  // because ROSE requires that the targeting location must have the parent
  // info, which is not available until "pastePreprocessingInfo" right above.
  if (set_num_threads_statement != NULL) {
    SageInterface::insertStatementAfter(set_num_threads_statement,
                                        outlined_function_call);
  };

  SgExprListExp *parameters2 = buildExprListExp();
  if (!SageInterface::is_Fortran_language()) {
    string file_name = target->get_endOfConstruct()->get_filenameString();
    int line = target->get_endOfConstruct()->get_line();
    parameters2->append_expression(buildStringVal(file_name));
    parameters2->append_expression(buildIntVal(line));
  }

  pastePreprocessingInfo(s2, PreprocessingInfo::after, save_buf2);

  // Defensive cleanup: conditional directives are already resolved by the
  // frontend, and carrying stale #if/#endif fragments across outlining can
  // leave unbalanced directives in host output.
  stripConditionalDirectivesFromSubtree(s1);
  stripConditionalDirectivesFromSubtree(
      outlined_func->get_definition()->get_body());
  // Keep outlined procedures in the original file for Fortran and C++.
  // Fortran needs declaration-link consistency; C++ currently hits
  // qualification/ODR issues when outlined functions are moved to a synthesized
  // source file.
  if (!enable_accelerator && !SageInterface::is_Fortran_language() &&
      !SageInterface::is_Cxx_language()) {
    // Generate a new source file for the outlined function if necessary
    if (cpu_outlined_file == NULL) {
      cpu_outlined_file = generate_outlined_function_file(outlined_func, "");
    }
    // Move the outlined function to the new source file
    SgFunctionDeclaration *new_outlined_func =
        move_outlined_function(outlined_func, cpu_outlined_file);
    if (new_outlined_func != NULL &&
        new_outlined_func->get_definition() != NULL &&
        new_outlined_func->get_definition()->get_body() != NULL) {
      SageInterface::fixVariableReferences(
          new_outlined_func->get_definition()->get_body());
    }
    Rose_STL_Container<SgNode *> old_directives =
        NodeQuery::querySubTree(outlined_func, V_SgOmpExecStatement);
    Rose_STL_Container<SgNode *> new_directives =
        NodeQuery::querySubTree(new_outlined_func, V_SgOmpExecStatement);
    ROSE_ASSERT(old_directives.size() == new_directives.size());
    for (int i = 0; i < new_directives.size(); i++) {
      SgOmpExecStatement *old_directive =
          isSgOmpExecStatement(old_directives[i]);
      SgOmpExecStatement *new_directive =
          isSgOmpExecStatement(new_directives[i]);
      ROSE_ASSERT(old_directive != NULL);
      ROSE_ASSERT(new_directive != NULL);
      clause_variable_renaming_record[new_directive] =
          clause_variable_renaming_record[old_directive];
      clause_variable_renaming_record.erase(old_directive);
    }
  }
}

//! A helper function to categorize variables collected from map clauses
void categorizeMapClauseVariables(
    const SgInitializedNamePtrList
        &all_vars, // all variables collected from map clauses
    std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
        &array_dimensions,            // array bounds  info
    std::set<SgSymbol *> &array_syms, // variable symbols which are array types
                                      // (explicit or as a pointer)
    std::set<SgSymbol *> &atom_syms) // variable symbols which are non-aggregate
                                     // types: scalar, pointer, etc
{
  // categorize the variables:
  for (SgInitializedNamePtrList::const_iterator iter = all_vars.begin();
       iter != all_vars.end(); iter++) {
    SgInitializedName *i_name = *iter;
    ROSE_ASSERT(i_name != NULL);

    // In C/C++, an array can have a pointer type or SgArrayType.
    // We collect SgArrayType for sure. But for pointer type, we consult the
    // array_dimension to decide.
    SgSymbol *sym = i_name->get_symbol_from_symbol_table();
    ROSE_ASSERT(sym != NULL);
    SgType *type = sym->get_type();
    // TODO handle complex types like structure, typedef, cast, etc. here
    if (isSgArrayType(type))
      array_syms.insert(sym);
    else if (isSgPointerType(type)) {
      if (array_dimensions[sym].size() !=
          0) // if we have bound information for the pointer type, it represents
             // an array
        array_syms.insert(sym);
      else // otherwise a pointer pointing to non-array types
        atom_syms.insert(sym);
    } else if (isScalarType(type)) {
      atom_syms.insert(sym);
    } else if (isSgTypedefType(type)) {
      atom_syms.insert(sym);
    } else {
      cerr << "Error. transOmpMapVariables() of omp_lowering.cpp: unhandled "
              "map clause variable type:"
           << type->class_name() << endl;
    }
  }
  // make sure the categorization is complete
  ROSE_ASSERT(all_vars.size() == (array_syms.size() + atom_syms.size()));
}

// Check if a variable is in the clause's variable list
bool isInClauseVariableList(SgOmpClause *cls, SgSymbol *var) {
  ROSE_ASSERT(cls && var);
  SgOmpVariablesClause *var_cls = isSgOmpVariablesClause(cls);
  ROSE_ASSERT(var_cls);
  SgExpressionPtrList refs =
      isSgOmpVariablesClause(var_cls)->get_variables()->get_expressions();

  std::vector<SgSymbol *> var_list;
  for (size_t j = 0; j < refs.size(); j++) {
    SgVariableSymbol *symbol = extractClauseVariableSymbol(refs[j]);
    if (symbol == nullptr) {
      continue;
    }
    var_list.push_back(symbol);
  }

  if (find(var_list.begin(), var_list.end(), var) != var_list.end())
    return true;
  else
    return false;
}

// ! Replace all references to original symbol with references to new symbol
// return the number of references being replaced.
// TODO: move to SageInterface
// static int replaceVariableReferences(SgNode* subtree, const SgVariableSymbol*
// origin_sym, SgVariableSymbol* new_sym )
static int replaceVariableReferences(
    SgNode *subtree,
    std::map<SgVariableSymbol *, SgVariableSymbol *> symbol_map) {
  int result = 0;
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(subtree, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    if (shouldSkipOpenMPClauseVarRefRewrite(vRef)) {
      continue;
    }
    // skip compiler generated references to the original variables which meant
    // to be kept.
    // TODO: maybe a better way is to match a pattern: if it is the first
    // parameter of xomp_deviceDataEnvironmentPrepareVariable()
    if (preservedHostVarRefs.find(vRef) != preservedHostVarRefs.end())
      continue;
    SgVariableSymbol *orig_sym = vRef->get_symbol();
    if (symbol_map[orig_sym] != NULL) {
      result++;
      vRef->set_symbol(symbol_map[orig_sym]);
    }
  }
  return result;
}

// TODO: move to sageinterface, the current one has wrong reference type, and
// has undesired effect!!
//  grab the list of dimension sizes for an input array type, store them in the
//  vector container
static void getArrayDimensionSizes(const SgArrayType *array_type,
                                   std::vector<SgExpression *> &result) {
  ROSE_ASSERT(array_type != NULL);

  const SgType *cur_type = array_type;
  do {
    ROSE_ASSERT(isSgArrayType(cur_type) != NULL);
    SgExpression *index_exp = isSgArrayType(cur_type)->get_index();
    result.push_back(
        index_exp); // could be NULL, especially for the first dimension
    cur_type = isSgArrayType(cur_type)->get_base_type();
  } while (isSgArrayType(cur_type));
}

// TODO move to SageInterface
//  Liao 2/8/2013
//  rewrite array reference using multiple-dimension subscripts to a reference
//  using one-dimension subscripts e.g. a[i][j] is changed to a[i*col_size +j]
//       a [i][j][k]  is changed to a [(i*col_size + j)*K_size +k]
//  The parameter is the array reference expression to be changed
//  Note the array reference expression must be the top one since there will be
//  inner ones for a multi-dimensional array references in AST.
static void linearizeArrayAccess(SgPntrArrRefExp *top_array_ref) {
  // Sanity check
  //  TODO check language compatibility for C/C++ only: row major storage
  ROSE_ASSERT(top_array_ref != NULL);
  // ROSE_ASSERT (top_array_ref->get_lhs_operand_i() != NULL);
  ROSE_ASSERT(top_array_ref->get_parent() != NULL);
  ROSE_ASSERT(
      isSgPntrArrRefExp(top_array_ref->get_parent()) ==
      NULL); // top ==> must not be a child of a higher level array ref exp

  // must be a canonical array reference, not like (a+10)[10]
  SgExpression *arrayNameExp = NULL;
  std::vector<SgExpression *> *subscripts = new vector<SgExpression *>;
  bool is_array_ref =
      isArrayReference(top_array_ref, &arrayNameExp, &subscripts);
  ROSE_ASSERT(is_array_ref);
  SgInitializedName *i_name = convertRefToInitializedName(arrayNameExp);
  ROSE_ASSERT(i_name != NULL);
  SgType *var_type = i_name->get_type();
  SgArrayType *array_type = isSgArrayType(var_type);
  SgPointerType *pointer_type = isSgPointerType(var_type);
  // pointer type can also be used as pointer[i], which is represented as
  // SgPntrArrRefExp. In this case, we don't need to linearized it any more
  if (pointer_type != NULL)
    return;
  if (array_type == NULL) {
    cerr << "Error. linearizeArrayAccess() found unhandled variable type:"
         << var_type->class_name() << endl;
  }

  ROSE_ASSERT(array_type != NULL);

  std::vector<SgExpression *> dimensions;
  getArrayDimensionSizes(array_type, dimensions);

  ROSE_ASSERT((*subscripts).size() == dimensions.size());
  ROSE_ASSERT((*subscripts).size() >
              1); // we only accept 2-D or above for processing. Caller should
                  // check this in advance

  // left hand operand
  SgExpression *new_lhs = buildVarRefExp(i_name);
  SgExpression *new_rhs = deepCopy((*subscripts)[0]); // initialized to be i;

  // build rhs, like (i*col_size + j)*K_size +k
  for (size_t i = 1; i < dimensions.size();
       i++) // only repeat dimension count -1 times
  {
    new_rhs = buildAddOp(buildMultiplyOp(new_rhs, deepCopy(dimensions[i])),
                         deepCopy((*subscripts)[i]));
  }

  // set new lhs and rhs for the top ref
  deepDelete(top_array_ref->get_lhs_operand_i());
  deepDelete(top_array_ref->get_rhs_operand_i());

  top_array_ref->set_lhs_operand_i(new_lhs);
  new_lhs->set_parent(top_array_ref);

  top_array_ref->set_rhs_operand_i(new_rhs);
  new_rhs->set_parent(top_array_ref);
}

// Find all top level array references within the body block,
// we do the following:
//   if it is within the set of arrays (array_syms) to be rewritten: arrays on
//   map() clause, if it is more than 1-D change it to be linearized subscript
//   access
static void
rewriteArraySubscripts(SgBasicBlock *body_block,
                       const std::set<SgSymbol *> mapped_array_syms) {
  std::vector<SgPntrArrRefExp *> candidate_refs; // store eligible references
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(body_block, V_SgPntrArrRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgPntrArrRefExp *vRef = isSgPntrArrRefExp((*i));
    ROSE_ASSERT(vRef != NULL);
    SgNode *parent = vRef->get_parent();
    // if it is top level ref?
    if (isSgPntrArrRefExp(parent)) // has a higher level array ref, skip it
      continue;
    // TODO: move this logic into a function in SageInterface
    //  If it is a canonical array reference we can handle?
    vector<SgExpression *> *subscripts = new vector<SgExpression *>;
    SgExpression *array_name_exp = NULL;
    isArrayReference(vRef, &array_name_exp, &subscripts);
    SgInitializedName *a_name = convertRefToInitializedName(array_name_exp);
    if (a_name == NULL)
      continue;
    // if it is within the mapped array set?
    ROSE_ASSERT(a_name != NULL);
    SgSymbol *array_sym = a_name->get_symbol_from_symbol_table();
    ROSE_ASSERT(array_sym != NULL);

    if (mapped_array_syms.find(array_sym) != mapped_array_syms.end())
      candidate_refs.push_back(vRef);
  }

  // To be safe, we use reverse order iteration when changing them
  for (std::vector<SgPntrArrRefExp *>::reverse_iterator riter =
           candidate_refs.rbegin();
       riter != candidate_refs.rend(); riter++) {
    SgExpression *arrayNameExp = NULL;
    std::vector<SgExpression *> *subscripts = new vector<SgExpression *>;
    bool is_array_ref = isArrayReference(*riter, &arrayNameExp, &subscripts);
    ROSE_ASSERT(is_array_ref);
    if ((*subscripts).size() > 1)
      linearizeArrayAccess(*riter);
  }
}

// Liao, 2/28/2013
// A helper function to collect variables used within a code portion
// To facilitate faster query into the variable collection, we use a map.
// TODO : move to SageInterface ?
std::map<SgVariableSymbol *, bool> collectVariableAppearance(SgNode *root) {
  std::map<SgVariableSymbol *, bool> result;
  ROSE_ASSERT(root != NULL);
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(root, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    SgVariableSymbol *sym = vRef->get_symbol();
    ROSE_ASSERT(sym != NULL);
    result[sym] = true;
  }
  return result;
}

// find different map clauses from the clause list, and all array information
// dimension map is the same for all the map clauses under the same omp target
// directive
void extractMapClauses(
    Rose_STL_Container<SgOmpClause *> map_clauses,
    std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
        &array_dimensions,
    std::map<SgSymbol *,
             std::vector<std::pair<SgOmpClause::omp_map_dist_data_enum,
                                   SgExpression *>>> &dist_data_policies,
    std::vector<SgOmpMapClause *> &map_alloc_clauses,
    std::vector<SgOmpMapClause *> &map_to_clauses,
    std::vector<SgOmpMapClause *> &map_from_clauses,
    std::vector<SgOmpMapClause *> &map_tofrom_clauses) {
  if (map_clauses.size() == 0)
    return; // stop if no map clauses at all

  for (Rose_STL_Container<SgOmpClause *>::const_iterator iter =
           map_clauses.begin();
       iter != map_clauses.end(); iter++) {
    SgOmpMapClause *m_cls = isSgOmpMapClause(*iter);
    ROSE_ASSERT(m_cls != NULL);
    if (iter == map_clauses.begin()) // retrieve once is enough
    {
      array_dimensions = m_cls->get_array_dimensions();
      dist_data_policies = m_cls->get_dist_data_policies();
    } else // array sections in other MAP clauses should be retrieved as well
    {
      std::map<SgSymbol *,
               std::vector<std::pair<SgExpression *, SgExpression *>>>
          new_array_dimensions = m_cls->get_array_dimensions();
      array_dimensions.insert(new_array_dimensions.begin(),
                              new_array_dimensions.end());
    };

    SgOmpClause::omp_map_operator_enum map_operator = m_cls->get_operation();
    if (map_operator == SgOmpClause::e_omp_map_alloc ||
        map_operator == SgOmpClause::e_omp_map_storage ||
        map_operator == SgOmpClause::e_omp_map_release ||
        map_operator == SgOmpClause::e_omp_map_delete)
      map_alloc_clauses.push_back(m_cls);
    else if (map_operator == SgOmpClause::e_omp_map_to)
      map_to_clauses.push_back(m_cls);
    else if (map_operator == SgOmpClause::e_omp_map_from)
      map_from_clauses.push_back(m_cls);
    else if (map_operator == SgOmpClause::e_omp_map_tofrom ||
             map_operator == SgOmpClause::e_omp_map_present ||
             map_operator == SgOmpClause::e_omp_map_self ||
             map_operator == SgOmpClause::e_omp_map_unknown)
      map_tofrom_clauses.push_back(m_cls);
    else {
      cerr << "Error. transOmpMapVariables() from omp_lowering.cpp: found "
              "unacceptable map operator type:"
           << map_operator << endl;
      ROSE_ABORT();
    }
  } // end for
}

static int generate_mapping_variable_type(
    /* the array and the map information */
    SgSymbol *sym, const std::vector<SgOmpMapClause *> & /*map_alloc_clauses*/,
    const std::vector<SgOmpMapClause *> &map_to_clauses,
    const std::vector<SgOmpMapClause *> &map_from_clauses,
    const std::vector<SgOmpMapClause *> &map_tofrom_clauses,
    std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
        &array_dimensions,
    SgExpression *device_expression,
    /*Where to insert generated function calls*/
    SgBasicBlock *insertion_scope, SgStatement *insertion_anchor_stmt) {
  bool needCopyTo = false;
  bool needCopyFrom = false;
  if (isInAnyClauseVariableList(map_to_clauses, sym) ||
      isInAnyClauseVariableList(map_tofrom_clauses, sym))
    needCopyTo = true;

  if (isInAnyClauseVariableList(map_from_clauses, sym) ||
      isInAnyClauseVariableList(map_tofrom_clauses, sym))
    needCopyFrom = true;

  int type_value = OMP_TGT_MAPTYPE_TARGET_PARAM;

  if (needCopyTo) {
    type_value = type_value | OMP_TGT_MAPTYPE_TO;
  };

  if (needCopyFrom) {
    type_value = type_value | OMP_TGT_MAPTYPE_FROM;
  };

  return type_value;
}

// Translated a single mapped array variable, knowing the map clauses , where to
// insert, etc. Only generate memory allocation, deallocation, copy, functions,
// not the declaration since decl involves too many variable bookkeeping. This
// is intended to be called by a for loop going through all mapped array
// variables.
//  Essentially, we have to decide if we need to do the following steps for each
//  variable
//
//  Data handling: declaration, allocation, and copy
//    1. declared a pointer type to the device copy : pass by pointer type vs.
//    pass by value
//    2. allocate device copy using the dimension bound info: for array types
//    (pointers used for linearized arrays)
//    3. copy the data from CPU to the device (GPU) copy:
//
//    4. replace references to the CPU copies with references to the GPU copy
//    5. replace original multidimensional element indexing with linearized
//    address indexing (for 2-D and more dimension arrays)
//
//  Data handling: copy back, de-allocation
//    6. copy GPU_copy back to CPU variables
//    7. de-allocate the GPU variables
//
//   Step 1,2,3 and 6, 7 should generate statements before or after the
//   SgOmpTargetStatement Step 4 and 5 should change the body of the affected
//   SgOmpParallelStatement
// Revised Algorithm (version 3)    1/23/2015, optionally use device data
// environment (DDE) functions to manage data automatically. Instead of generate
// explicit data allocation, copy, free functions, using the following three DDE
// functions:
//   1. xomp_deviceDataEnvironmentEnter()
//   2. xomp_deviceDataEnvironmentPrepareVariable ()
//   3. xomp_deviceDataEnvironmentExit()
// This is necessary to have a consistent translation for mapped data showing up
// in both "target data" and "target" directives. These DDE functions internally
// will keep track of data allocated and try to reuse enclosing data
// environment.
static void generateMappedArrayMemoryHandling(
    /* the array and the map information */
    SgSymbol *sym, const std::vector<SgOmpMapClause *> &map_alloc_clauses,
    const std::vector<SgOmpMapClause *> &map_to_clauses,
    const std::vector<SgOmpMapClause *> &map_from_clauses,
    const std::vector<SgOmpMapClause *> &map_tofrom_clauses,
    std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
        &array_dimensions,
    SgExpression *device_expression,
    /*Where to insert generated function calls*/
    SgBasicBlock *insertion_scope, SgStatement *insertion_anchor_stmt,
    bool need_generate_data_stmt,
    std::vector<SgExpression *> *map_variable_list,
    std::vector<SgExpression *> *map_variable_base_list,
    std::vector<SgExpression *> *map_variable_size_list,
    std::vector<SgExpression *> *map_variable_type_list) {
  ROSE_ASSERT(sym != NULL);
  ROSE_ASSERT(device_expression !=
              NULL); // runtime now needs explicit device ID to work
  SgType *orig_type = sym->get_type();

  // Step 1: declare a pointer type to array variables in map clauses, we
  // linearize all arrays to be a 1-D pointer
  //   Element_type * _dev_var;
  //   e.g.: double* _dev_array;
  // I believe that all array variables need allocations on GPUs, regardless
  // their map operations (alloc, to, from, or tofrom)

  // TODO: is this a safe assumption here??
  SgType *element_type =
      orig_type->findBaseType(); // recursively strip away non-base type to get
                                 // the bottom type
  string orig_name = (sym->get_name()).getString();
  string dev_var_name = "_dev_" + orig_name;

  // Step 2.1  generate linear size calculation based on array dimension info
  // int dev_array_size = sizeof (double) *dim_size1 * dim_size2;
  string dev_var_size_name = "_dev_" + orig_name + "_size";
  SgVariableDeclaration *dev_var_size_decl = NULL;

  SgVariableSymbol *dev_var_size_sym =
      insertion_scope->lookup_variable_symbol(dev_var_size_name);
  std::vector<SgExpression *> v_size;
  int dimSize = 0;
  if (dev_var_size_sym == NULL) {
    SgExprListExp *initializer = buildExprListExp();
    if (array_dimensions[sym].size() > 0) {
      dimSize = array_dimensions[sym].size();
      for (std::vector<
               std::pair<SgExpression *, SgExpression *>>::const_iterator iter =
               array_dimensions[sym].begin();
           iter != array_dimensions[sym].end(); iter++) {
        std::pair<SgExpression *, SgExpression *> bound_pair = *iter;
        initializer->append_expression(deepCopy(bound_pair.second));
        v_size.push_back(deepCopy(bound_pair.second));
      }
    } else {
      ROSE_ASSERT(sym != NULL);
      SgArrayType *a_type = isSgArrayType(orig_type);
      ROSE_ASSERT(a_type != NULL);
      std::vector<SgExpression *> dims = get_C_array_dimensions(a_type);
      for (std::vector<SgExpression *>::const_iterator iter = dims.begin();
           iter != dims.end(); iter++) {
        SgExpression *length_exp = *iter;
        // TODO: get_C_array_dimensions returns one extra null expression
        // somehow.
        if (!isSgNullExpression(length_exp)) {
          dimSize++;
          initializer->append_expression(deepCopy(length_exp));
          v_size.push_back(deepCopy(length_exp));
        }
      }
    }
    // dev_var_size_decl = buildVariableDeclaration (dev_var_size_name,
    // buildArrayType(buildIntType(),buildIntVal(dimSize)),
    // buildAggregateInitializer(initializer), insertion_scope);
    // insertStatementBefore (insertion_anchor_stmt, dev_var_size_decl);
  } else
    dev_var_size_decl = isSgVariableDeclaration(
        dev_var_size_sym->get_declaration()->get_declaration());

  // ROSE_ASSERT (dev_var_size_decl != NULL);

  SgExpression *mapping_array_size = NULL;
  for (std::vector<SgExpression *>::const_iterator iter = v_size.begin();
       iter != v_size.end(); iter++) {
    if (mapping_array_size == NULL) {
      mapping_array_size = *iter;
    } else {
      mapping_array_size = buildMultAssignOp(mapping_array_size, *iter);
    };
  };

  // generate offset array
  string dev_var_offset_name = "_dev_" + orig_name + "_offset";
  SgVariableDeclaration *dev_var_offset_decl = NULL;

  SgVariableSymbol *dev_var_offset_sym =
      insertion_scope->lookup_variable_symbol(dev_var_offset_name);
  // vector to store all offset values
  std::vector<SgExpression *> v_offset;
  if (dev_var_offset_sym == NULL) {
    SgExprListExp *arrayInitializer = buildExprListExp();
    if (array_dimensions[sym].size() > 0) {
      for (std::vector<
               std::pair<SgExpression *, SgExpression *>>::const_iterator iter =
               array_dimensions[sym].begin();
           iter != array_dimensions[sym].end(); iter++) {
        std::pair<SgExpression *, SgExpression *> bound_pair = *iter;
        arrayInitializer->append_expression(deepCopy(bound_pair.first));
        v_offset.push_back(deepCopy(bound_pair.first));
      }
    } else {
      for (int i = 0; i < dimSize; ++i) {
        arrayInitializer->append_expression(buildIntVal(0));
        v_offset.push_back(buildIntVal(0));
      }
    }
    // dev_var_offset_decl = buildVariableDeclaration (dev_var_offset_name,
    // buildArrayType(buildIntType(),buildIntVal(dimSize)),
    // buildAggregateInitializer(arrayInitializer), insertion_scope);
    // insertStatementBefore (insertion_anchor_stmt, dev_var_offset_decl);
  } else
    dev_var_offset_decl = isSgVariableDeclaration(
        dev_var_offset_sym->get_declaration()->get_declaration());

  // ROSE_ASSERT (dev_var_offset_decl != NULL);

  // for now, we take the first offset as the final offset.
  // it only works for 1D array.
  // TODO: implement an helper to determine the correct offset in general
  SgExpression *mapping_array_offset = NULL;
  for (std::vector<SgExpression *>::const_iterator iter = v_offset.begin();
       iter != v_offset.end(); iter++) {
    if (mapping_array_offset == NULL) {
      mapping_array_offset = *iter;
    };
  };

  // generate Dim array
  string dev_var_Dim_name = "_dev_" + orig_name + "_Dim";
  SgVariableDeclaration *dev_var_Dim_decl = NULL;

  SgVariableSymbol *dev_var_Dim_sym =
      insertion_scope->lookup_variable_symbol(dev_var_Dim_name);
  std::vector<SgExpression *> v_dimSize;
  if (dev_var_Dim_sym == NULL) {
    SgExprListExp *arrayInitializer = buildExprListExp();
    {
      ROSE_ASSERT(sym != NULL);
      SgArrayType *a_type = isSgArrayType(orig_type);
      if (a_type != NULL) {
        std::vector<SgExpression *> dims = get_C_array_dimensions(a_type);
        for (std::vector<SgExpression *>::const_iterator iter = dims.begin();
             iter != dims.end(); iter++) {
          SgExpression *length_exp = *iter;
          // TODO: get_C_array_dimensions returns one extra null expression
          // somehow.
          if (!isSgNullExpression(length_exp)) {
            arrayInitializer->append_expression(deepCopy(length_exp));
            v_dimSize.push_back(deepCopy(length_exp));
          }
        }
      } else {
        for (int i = 0; i < dimSize; ++i) {
          arrayInitializer->append_expression(deepCopy(v_size[i]));
          v_dimSize.push_back(deepCopy(v_size[i]));
        }
      }
    }
    // dev_var_Dim_decl = buildVariableDeclaration (dev_var_Dim_name,
    // buildArrayType(buildIntType(),buildIntVal(dimSize)),
    // buildAggregateInitializer(arrayInitializer), insertion_scope);
    // insertStatementBefore (insertion_anchor_stmt, dev_var_Dim_decl);
  } else
    dev_var_Dim_decl = isSgVariableDeclaration(
        dev_var_Dim_sym->get_declaration()->get_declaration());

  // ROSE_ASSERT (dev_var_Dim_decl != NULL);
  // Only if we are in the mode of inserting data handling statements
  if (!need_generate_data_stmt)
    return;

  bool needCopyTo = false;
  bool needCopyFrom = false;
  if (isInAnyClauseVariableList(map_to_clauses, sym) ||
      isInAnyClauseVariableList(map_tofrom_clauses, sym))
    needCopyTo = true;

  if (isInAnyClauseVariableList(map_from_clauses, sym) ||
      isInAnyClauseVariableList(map_tofrom_clauses, sym))
    needCopyFrom = true;

  if (useDDE) {
    // a single function call does all things transparently: reuse first, if not
    // then allocation, copy data e.g. float* _dev_u = (float*)
    // xomp_deviceDataEnvironmentPrepareVariable ((void*)u, _dev_u_size, true,
    // false);
    SgExpression *copyToExp = NULL;
    SgExpression *copyFromExp = NULL;
    if (needCopyTo)
      copyToExp = buildBoolValExp(1);
    else
      copyToExp = buildBoolValExp(0);

    if (needCopyFrom)
      copyFromExp = buildBoolValExp(1);
    else
      copyFromExp = buildBoolValExp(0);

    SgVarRefExp *host_var_ref = buildVarRefExp(isSgVariableSymbol(sym));
    preservedHostVarRefs.insert(host_var_ref);
    // cout<<"Debug: inserting var ref to be
    // preserved:"<<sym->get_name()<<"@"<<host_var_ref <<endl;
    //  should not be done here. Only one call for a whole device data
    //  environment Now insert xomp_deviceDataEnvironmentEnter() before
    //  xomp_deviceDataEnvironmentPrepareVariable()
    // SgExprStatement* dde_enter_stmt = buildFunctionCallStmt
    // (SgName("xomp_deviceDataEnvironmentEnter"), buildVoidType(), NULL,
    // insertion_scope);
    // insertStatementBefore (dde_prep_stmt, dde_enter_stmt);
  } else {
    // Step 2.5 generate memory allocation on GPUs
    // e.g.:  _dev_m1 = (double *)xomp_deviceMalloc (_dev_m1_size);
    SgExprStatement *mem_alloc_stmt = buildAssignStatement(
        buildVarRefExp(dev_var_name, insertion_scope),
        buildCastExp(
            buildFunctionCallExp(SgName("xomp_deviceMalloc"),
                                 buildPointerType(buildVoidType()),
                                 buildExprListExp(buildVarRefExp(
                                     dev_var_size_name, insertion_scope)),
                                 insertion_scope),
            buildPointerType(element_type)));
    insertStatementBefore(insertion_anchor_stmt, mem_alloc_stmt);

    // Step 3. copy the data from CPU to GPU
    // Only for variable in map(to:), or map(tofrom:)
    // e.g. xomp_memcpyHostToDevice ((void*)dev_m1, (const void*)a, array_size);
    if (needCopyTo) {
      SgExprListExp *parameters = buildExprListExp(
          buildCastExp(buildVarRefExp(dev_var_name, insertion_scope),
                       buildPointerType(buildVoidType())),
          buildCastExp(buildVarRefExp(orig_name, insertion_scope),
                       buildPointerType(buildConstType(buildVoidType()))),
          buildVarRefExp(dev_var_size_name, insertion_scope));
      SgExprStatement *mem_copy_to_stmt = buildFunctionCallStmt(
          SgName("xomp_memcpyHostToDevice"), buildPointerType(buildVoidType()),
          parameters, insertion_scope);
      // insertStatementBefore (insertion_anchor_stmt, mem_copy_to_stmt);
    }
  }

  if (useDDE) { // call xomp_deviceDataEnvironmentExit() and it will
                // automatically copy back data and deallocate.
                // SgExprStatement* dde_exit_stmt = buildFunctionCallStmt
                // (SgName("xomp_deviceDataEnvironmentExit"), buildVoidType(),
                // NULL, insertion_scope);
                // appendStatement(dde_exit_stmt ,
                // insertion_anchor_stmt->get_scope()); do nothing here or we
                // will get multiple exit() for a single DDE.
  } else {      // or explicitly control copy back and deallocation
    // Step 6. copy back data from GPU to CPU, only for variable in
    // map(out:var_list) e.g. xomp_memcpyDeviceToHost ((void*)c, (const
    // void*)dev_m3, array_size); Note: insert this AFTER the target directive
    // stmt SgStatement* prev_stmt = target_parallel_stmt;
    if (needCopyFrom) {
      SgExprListExp *parameters = buildExprListExp(
          buildCastExp(buildVarRefExp(orig_name, insertion_scope),
                       buildPointerType(buildVoidType())),
          buildCastExp(buildVarRefExp(dev_var_name, insertion_scope),
                       buildPointerType(buildConstType(buildVoidType()))),
          buildVarRefExp(dev_var_size_name, insertion_scope));
      SgExprStatement *mem_copy_back_stmt = buildFunctionCallStmt(
          SgName("xomp_memcpyDeviceToHost"), buildPointerType(buildVoidType()),
          parameters, insertion_scope);
      // appendStatement(mem_copy_back_stmt,
      // insertion_anchor_stmt->get_scope()); prev_stmt = mem_copy_back_stmt;
    }

    // Step 7, de-allocate GPU memory
    // e.g. xomp_freeDevice(dev_m1);
    // Note: insert this AFTER the target directive stmt or the copy back stmt
    SgExprStatement *mem_dealloc_stmt = buildFunctionCallStmt(
        SgName("xomp_freeDevice"), buildBoolType(),
        buildExprListExp(buildVarRefExp(dev_var_name, insertion_scope)),
        insertion_scope);
    appendStatement(mem_dealloc_stmt, insertion_anchor_stmt->get_scope());
  }

  // check the type of current array symbol and calculate the desired data size
  SgExpression *mapping_variable_expression = NULL;
  mapping_variable_expression =
      buildVarRefExp(sym->get_name(), sym->get_scope());
  map_variable_list->push_back(
      buildAddOp(mapping_variable_expression, mapping_array_offset));
  map_variable_base_list->push_back(mapping_variable_expression);
  SgExpression *mapping_variable_total_size = buildCastExp(
      buildMultiplyOp(buildSizeOfOp(element_type), mapping_array_size),
      buildOpaqueType("int64_t", insertion_scope));
  map_variable_size_list->push_back(mapping_variable_total_size);

  int mapping_variable_type_enum = generate_mapping_variable_type(
      sym, map_alloc_clauses, map_to_clauses, map_from_clauses,
      map_tofrom_clauses, array_dimensions, device_expression, insertion_scope,
      insertion_anchor_stmt);
  SgExpression *mapping_variable_value =
      buildIntVal(mapping_variable_type_enum);
  map_variable_type_list->push_back(mapping_variable_value);
}

// trans OpenMP map variables
// return all generated or remaining variables to be passed to the outliner
// Liao, 2/4/2013
// Translate the map clause variables associated with "omp target parallel"
// We only support combined "target parallel" or "parallel" immediately
// following "target" So we handle outlining and data handling for two
// directives at the same time
// TODO: move to the header
// Input:
//
//  map(alloc|to|from|tofrom:var_list)
//  array variable in var_list should have dimension bounds information like
//  [0:N-1][0:K-1]
//
//  Essentially, we have to decide if we need to do the following steps for each
//  variable
//
//  Data handling: declaration, allocation, and copy
//    1. declared a pointer type to the device copy : pass by pointer type vs.
//    pass by value
//    2. allocate device copy using the dimension bound info: for array types
//    (pointers used for linearized arrays)
//    3. copy the data from CPU to the device (GPU) copy:
//
//    4. replace references to the CPU copies with references to the GPU copy
//    5. replace original multidimensional element indexing with linearized
//    address indexing (for 2-D and more dimension arrays)
//
//  Data handling: copy back, de-allocation
//    6. copy GPU_copy back to CPU variables
//    7. de-allocate the GPU variables
//
//   Step 1,2,3 and 6, 7 should generate statements before or after the
//   SgOmpTargetStatement Step 4 and 5 should change the body of the affected
//   SgOmpParallelStatement
//
//  Algorithm 1:
//   collect all variables in map clauses: they should be either scalar or
//   arrays with bound info. For each array variable,
//       we generate memory handling statements for them: declaration,
//       allocation, copy back-forth, de-allocation
//   For the use of array variable,
//       we replace the original references with references to new pointer typed
//       variables Linearize the access when 2-D or more dimensions are used.
//
//   Based on the mapped variables, we output the variables to be passed to the
//   outlined function to be generated later on
//         variables which will be passed by their original data types
//         variables which will be passed by their address of type: pointer type
//         pointing to their original data type
//
//  Revised Algorithm (version 2):  To translate "omp target" + "omp parallel
//  for" enclosed within "omp target data" region: New facts:
//        the map clauses are now associated with "omp target data" instead of
//        "omp target" Only a subset of all mapped variables at "omp target
//        data" level will be used within "omp target":
//           a single data region contains multiple "omp target" regions
//        When translating "omp target" + "omp parallel for", we don't need to
//        generate data handling statements
//            but we need to refer to the declarations for device variables.
//        Memory declaration, allocation, copy back-forth, de-allocation is
//        generated within the body of the "omp target data" region.
//            we can still try to generate them when translating "omp parallel
//            for" under "omp target", if not yet generated before.
//
// Revised Algorithm (V3): using Device Data Environment (DDE) runtime support
// to manage nested data regions
//       To simplify the handling, we assume
//         1. Both "target data"  and "target parallel for " should have map()
//         clauses
//         2. Using DDE, the translation is simplified as is identical for both
//         directive
ASTtools::VarSymSet_t transOmpMapVariables(
    SgStatement *node, SgExprListExp *map_variable_list,
    SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list,
    SgExprListExp *map_variable_type_list,
    GpuOffloadLoweringContext *offload_ctx = NULL,
    std::vector<ExpandedMapEntry> *dynamic_entries_out = NULL) {
  ASTtools::VarSymSet_t all_syms;
  ROSE_ASSERT(all_syms.size() == 0); // it should be empty

  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);

  // collect map clauses and their variables
  // ----------------------------------------------------------
  // Some notes for the relevant AST input:
  // we store a map clause for each variant/operator (alloc, to, from, and
  // tofrom), so there should be up to 4 SgOmpMapClause.
  //    SgOmpClause::omp_map_operator_enum
  // each map clause has
  //   a variable list (SgExpression), accessible through get_variables()
  //   a pointer to array_dimensions, accessible through get_array_dimensions().
  //   the array_dimensions is identical among all map clause of a same "omp
  //   target"
  //     std::map<SgSymbol*,  std::vector < std::pair <SgExpression*,
  //     SgExpression*> > >  array_dimensions

  Rose_STL_Container<SgOmpClause *> map_clauses =
      getClause(target, V_SgOmpMapClause);
  Rose_STL_Container<SgOmpClause *> device_clauses =
      getClause(target, V_SgOmpDeviceClause);

  if (map_clauses.size() == 0)
    return all_syms; // stop if no map clauses at all

  // store each time of map clause explicitly
  std::vector<SgOmpMapClause *> map_alloc_clauses;
  std::vector<SgOmpMapClause *> map_to_clauses;
  std::vector<SgOmpMapClause *> map_from_clauses;
  std::vector<SgOmpMapClause *> map_tofrom_clauses;
  // dimension map is the same for all the map clauses under the same omp target
  // directive
  std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
      array_dimensions;
  std::map<SgSymbol *,
           std::vector<
               std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>>
      dist_data_policies; // no in use, for compatible reason

  // a map between original symbol and its device version : used for variable
  // replacement
  std::map<SgVariableSymbol *, SgVariableSymbol *> cpu_gpu_var_map;

  // store all variables showing up in any of the map clauses
  SgInitializedNamePtrList all_mapped_vars =
      collectClauseVariables(target, VariantVector(V_SgOmpMapClause));

  // store all variables showing up in any of the device clauses
  SgExpression *device_expression =
      getClauseExpression(target, VariantVector(V_SgOmpDeviceClause));

  extractMapClauses(map_clauses, array_dimensions, dist_data_policies,
                    map_alloc_clauses, map_to_clauses, map_from_clauses,
                    map_tofrom_clauses);
  std::set<SgSymbol *> array_syms; // store clause variable symbols which are
                                   // array types (explicit or as a pointer)
  std::set<SgSymbol *> atom_syms;  // store clause variable symbols which are
                                   // non-aggregate types: scalar, pointer, etc

  // categorize the variables:
  categorizeMapClauseVariables(all_mapped_vars, array_dimensions, array_syms,
                               atom_syms);

  // set the scope and anchor statement we will focus on based on the
  // availability of an enclosing target data region
  SgBasicBlock *insertion_scope = NULL; // the body
  SgStatement *insertion_anchor_stmt =
      NULL; // the single statement within the body

  // at this point, the body should already be normalized to be a BB
  SgBasicBlock *body_block = ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(body_block != NULL);

  SgStatement *target_child_stmt = NULL;
  // We cannot assert this since the body of "omp target data" may already be
  // expanded as part of a previous translation
  //    ROSE_ASSERT( (target_data_stmt_body->get_statements()).size() ==1);
  target_child_stmt = (body_block->get_statements())[0];

  insertion_scope = body_block;
  insertion_anchor_stmt = target_child_stmt;
  ROSE_ASSERT(insertion_scope != NULL);
  ROSE_ASSERT(insertion_anchor_stmt != NULL);

  // collect used variables in the insertion scope
  std::map<SgVariableSymbol *, bool> variable_map =
      collectVariableAppearance(insertion_scope);

  if (device_expression == NULL) {
    device_expression = buildIntVal(0);
  };

  std::vector<SgExpression *> side_effect_map_variable_list;
  std::vector<SgExpression *> side_effect_map_variable_base_list;
  std::vector<SgExpression *> side_effect_map_variable_size_list;
  std::vector<SgExpression *> side_effect_map_variable_type_list;
  // handle array variables showing up in the map clauses:
  for (std::set<SgSymbol *>::const_iterator iter = array_syms.begin();
       iter != array_syms.end(); iter++) {
    SgSymbol *sym = *iter;
    ROSE_ASSERT(sym != NULL);
    SgType *orig_type = sym->get_type();

    // Step 1: declare a pointer type to array variables in map clauses, we
    // linearize all arrays to be a 1-D pointer
    //   Element_type * _dev_var;
    //   e.g.: double* _dev_array;
    // I believe that all array variables need allocations on GPUs, regardless
    // their map operations (alloc, to, from, or tofrom)

    // TODO: is this a safe assumption here??
    SgType *element_type =
        orig_type->findBaseType(); // recursively strip away non-base type to
                                   // get the bottom type
    string orig_name = (sym->get_name()).getString();
    string dev_var_name = "_dev_" + orig_name;

    const bool use_const_device_pointer = mappedArrayUsesAreReadOnlyInScope(
        insertion_scope, isSgVariableSymbol(sym));
    SgType *dev_var_type = buildPointerType(
        use_const_device_pointer ? buildConstType(element_type) : element_type);

    SgVariableDeclaration *dev_var_decl = NULL;
    dev_var_decl = buildVariableDeclaration(dev_var_name, dev_var_type, NULL,
                                            insertion_scope);
    insertStatementBefore(insertion_anchor_stmt, dev_var_decl);
    ROSE_ASSERT(dev_var_decl != NULL);

    SgVariableSymbol *orig_sym = isSgVariableSymbol(sym);
    ROSE_ASSERT(orig_sym != NULL);
    SgVariableSymbol *new_sym = getFirstVarSym(dev_var_decl);
    cpu_gpu_var_map[orig_sym] = new_sym; // store the mapping, this is always
                                         // needed to guide the outlining

    // Not all map variables from "omp target data" will be used within the
    // current parallel region We only need to find out the used one only.

    // linearized array pointers should be directly passed to the outliner later
    // on, without adding & operator in front of them we assume AST is
    // normalized and all target regions have explicit and correct map() clauses
    // Still some transformation like loop collapse will change the variables
    if (variable_map[orig_sym])
      all_syms.insert(new_sym);
    // generate memory allocation, copy, free function calls.
    generateMappedArrayMemoryHandling(
        sym, map_alloc_clauses, map_to_clauses, map_from_clauses,
        map_tofrom_clauses, array_dimensions, device_expression,
        insertion_scope, insertion_anchor_stmt, true,
        &side_effect_map_variable_list, &side_effect_map_variable_base_list,
        &side_effect_map_variable_size_list,
        &side_effect_map_variable_type_list);

    // map variables will be passed as kernel arguments later.
    // they are only temporarily used and should be removed to prevent
    // duplicated declaration.
    removeStatement(dev_var_decl);
  } // end for

  // C/C++ mapping arguments are produced by the resolved-item path below.
  // Fortran lowering still needs the legacy array side effects emitted while
  // materializing mapped array handling above.
  if (SageInterface::is_Fortran_language()) {
    for (SgExpression *expr : side_effect_map_variable_list) {
      map_variable_list->append_expression(expr);
    }
    for (SgExpression *expr : side_effect_map_variable_base_list) {
      map_variable_base_list->append_expression(expr);
    }
    for (SgExpression *expr : side_effect_map_variable_size_list) {
      map_variable_size_list->append_expression(expr);
    }
    for (SgExpression *expr : side_effect_map_variable_type_list) {
      map_variable_type_list->append_expression(expr);
    }
  }

  // Step 5. TODO  replace indexing element access with address calculation
  // (only needed for 2/3 -D) We switch the order of 4 and 5 since we want to
  // rewrite the subscripts before the arrays are replaced
  rewriteArraySubscripts(insertion_scope, array_syms);

  // Step 4. replace references to old with new variables,
  // The omp target data region is still executed on the host. We don't need to
  // outline it or rename its variables. Thus, the original body should be
  // preserved.
  if (!isSgOmpTargetDataStatement(node))
    replaceVariableReferences(insertion_scope, cpu_gpu_var_map);

  // TODO handle scalar, separate or merged into previous loop ?

  // store remaining variables so outliner can readily use this information
  // for pointers to linearized arrays, they should passed by their original
  // form, not using & operator, regardless the map operator types
  // (to|from|alloc|tofrom) for a scalar, two cases: to vs. from | tofrom if in
  // only, pass by value is good if either from or tofrom: two possible
  // solutions: 1) we need to treat it as an array of size 1 or any other
  // choices. TODO!!
  //  we also have to replace the reference to scalar to the array element
  //  access: be cautious about using by value (a) vs. using by address  (&a)
  // 2) try to still pass by value, but copy the final value back to the CPU
  // version right now we assume they are not on from|tofrom, until we face a
  // real input applications with map(from:scalar_a) For all scalars, we
  // directly copy them into all_syms for now
  for (std::set<SgSymbol *>::iterator iter = atom_syms.begin();
       iter != atom_syms.end(); iter++) {
    SgVariableSymbol *var_sym = isSgVariableSymbol(*iter);
    if (variable_map[var_sym] ==
        true) // we should only collect map variables which show up in the
              // current parallel region
      all_syms.insert(var_sym);
  }

  std::vector<ExpandedMapEntry> expanded_entries;
  for (SgOmpClause *clause : map_clauses) {
    SgOmpMapClause *map_clause = isSgOmpMapClause(clause);
    if (map_clause == NULL)
      continue;
    std::vector<ExpandedMapEntry> clause_items =
        collectExpandedMapItemsForClause(target, map_clause);
    expanded_entries.insert(expanded_entries.end(), clause_items.begin(),
                            clause_items.end());
  }
  if (!isSgOmpTargetDataStatement(node) && offload_ctx != NULL) {
    for (ExpandedMapEntry &entry : expanded_entries) {
      if (entry.kind != ExpandedMapEntryKind::direct_item) {
        continue;
      }
      ResolvedMapItem &item = entry.direct_item;
      if (item.direct_variable_symbol == NULL ||
          variable_map[item.direct_variable_symbol] != true) {
        continue;
      }

      item.is_implicit_target_variable =
          isImplicitTargetMapVariable(target, item.direct_variable_symbol);
      if (!canUseLiteralTargetParam(target, item.direct_variable_symbol,
                                    item.map_operator)) {
        continue;
      }

      item.use_literal_target_param = true;
      offload_ctx->literal_target_param_syms.insert(
          item.direct_variable_symbol);
    }
  }
  const bool has_dynamic_entries =
      hasDynamicExpandedMapEntries(expanded_entries);
  if (dynamic_entries_out != NULL) {
    dynamic_entries_out->clear();
    if (has_dynamic_entries) {
      *dynamic_entries_out = expanded_entries;
    }
  }
  if (!has_dynamic_entries) {
    std::vector<ResolvedMapItem> resolved_items;
    collectDirectResolvedMapItems(expanded_entries, resolved_items);
    appendResolvedMapItemArguments(
        resolved_items, map_variable_list, map_variable_base_list,
        map_variable_size_list, map_variable_type_list, target->get_scope());
  }

  return all_syms;
} // end transOmpMapVariables() for omp target data's map clauses for now

void collectOmpFromToVariablesInfo(
    SgInitializedNamePtrList all_mapped_vars,
    std::map<SgSymbol *, std::vector<std::pair<SgExpression *, SgExpression *>>>
        array_dimensions,
    SgExprListExp *map_variable_list, SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list, SgBasicBlock *insertion_scope) {

  std::set<SgSymbol *> array_syms; // store clause variable symbols which are
                                   // array types (explicit or as a pointer)
  std::set<SgSymbol *> atom_syms;  // store clause variable symbols which are
                                   // non-aggregate types: scalar, pointer, etc

  // categorize the variables:
  categorizeMapClauseVariables(all_mapped_vars, array_dimensions, array_syms,
                               atom_syms);

  for (std::set<SgSymbol *>::const_iterator iter = array_syms.begin();
       iter != array_syms.end(); iter++) {
    SgSymbol *sym = *iter;
    ROSE_ASSERT(sym != NULL);
    SgType *orig_type = sym->get_type();

    // TODO: is this a safe assumption here??
    SgType *element_type =
        orig_type->findBaseType(); // recursively strip away non-base type to
                                   // get the bottom type
    string orig_name = (sym->get_name()).getString();

    SgVariableSymbol *orig_sym = isSgVariableSymbol(sym);
    ROSE_ASSERT(orig_sym != NULL);

    std::vector<SgExpression *> v_size;
    int dimSize = 0;
    SgExprListExp *initializer = buildExprListExp();
    if (array_dimensions[sym].size() > 0) {
      dimSize = array_dimensions[sym].size();
      for (std::vector<
               std::pair<SgExpression *, SgExpression *>>::const_iterator iter =
               array_dimensions[sym].begin();
           iter != array_dimensions[sym].end(); iter++) {
        std::pair<SgExpression *, SgExpression *> bound_pair = *iter;
        initializer->append_expression(deepCopy(bound_pair.second));
        v_size.push_back(deepCopy(bound_pair.second));
      }
    } else {
      ROSE_ASSERT(sym != NULL);
      SgArrayType *a_type = isSgArrayType(orig_type);
      ROSE_ASSERT(a_type != NULL);
      std::vector<SgExpression *> dims = get_C_array_dimensions(a_type);
      for (std::vector<SgExpression *>::const_iterator iter = dims.begin();
           iter != dims.end(); iter++) {
        SgExpression *length_exp = *iter;
        // TODO: get_C_array_dimensions returns one extra null expression
        // somehow.
        if (!isSgNullExpression(length_exp)) {
          dimSize++;
          initializer->append_expression(deepCopy(length_exp));
          v_size.push_back(deepCopy(length_exp));
        }
      }
    }

    SgExpression *mapping_array_size = NULL;
    for (std::vector<SgExpression *>::const_iterator iter = v_size.begin();
         iter != v_size.end(); iter++) {
      if (mapping_array_size == NULL) {
        mapping_array_size = *iter;
      } else {
        mapping_array_size = buildMultAssignOp(mapping_array_size, *iter);
      };
    };

    // vector to store all offset values
    std::vector<SgExpression *> v_offset;
    {
      SgExprListExp *arrayInitializer = buildExprListExp();
      if (array_dimensions[sym].size() > 0) {
        for (std::vector<std::pair<SgExpression *, SgExpression *>>::
                 const_iterator iter = array_dimensions[sym].begin();
             iter != array_dimensions[sym].end(); iter++) {
          std::pair<SgExpression *, SgExpression *> bound_pair = *iter;
          arrayInitializer->append_expression(deepCopy(bound_pair.first));
          v_offset.push_back(deepCopy(bound_pair.first));
        }
      } else {
        for (int i = 0; i < dimSize; ++i) {
          arrayInitializer->append_expression(buildIntVal(0));
          v_offset.push_back(buildIntVal(0));
        }
      }
    }

    // for now, we take the first offset as the final offset.
    // it only works for 1D array.
    // TODO: implement an helper to determine the correct offset in general
    SgExpression *mapping_array_offset = NULL;
    for (std::vector<SgExpression *>::const_iterator iter = v_offset.begin();
         iter != v_offset.end(); iter++) {
      if (mapping_array_offset == NULL) {
        mapping_array_offset = *iter;
        break;
      };
    };

    // check the type of current array symbol and calculate the desired data
    // size
    SgExpression *mapping_variable_expression = NULL;
    mapping_variable_expression =
        buildVarRefExp(sym->get_name(), sym->get_scope());
    map_variable_list->append_expression(
        buildAddOp(mapping_variable_expression, mapping_array_offset));
    map_variable_base_list->append_expression(mapping_variable_expression);
    SgExpression *mapping_variable_total_size = buildCastExp(
        buildMultiplyOp(buildSizeOfOp(element_type), mapping_array_size),
        buildOpaqueType("int64_t", insertion_scope));
    map_variable_size_list->append_expression(mapping_variable_total_size);

  } // end for

  for (std::set<SgSymbol *>::iterator iter = atom_syms.begin();
       iter != atom_syms.end(); iter++) {
    SgVariableSymbol *var_sym = isSgVariableSymbol(*iter);

    // check the type of current variable symbol and calculate its size
    SgInitializedName *mapping_variable = var_sym->get_declaration();
    SgType *mapping_variable_type = mapping_variable->get_type();
    SgExpression *mapping_variable_expression = NULL;
    if (isPointerType(mapping_variable_type)) {
      mapping_variable_expression = buildVarRefExp(var_sym);
    } else {
      mapping_variable_expression = buildAddressOfOp(buildVarRefExp(var_sym));
    };
    map_variable_list->append_expression(mapping_variable_expression);
    map_variable_base_list->append_expression(mapping_variable_expression);
    SgExpression *mapping_variable_size =
        buildCastExp(buildSizeOfOp(mapping_variable_type),
                     buildOpaqueType("int64_t", insertion_scope));
    map_variable_size_list->append_expression(mapping_variable_size);
  }
}

// Collect mapping variables information in from/to clauses.
void collectOmpTargetUpdateInfo(
    SgStatement *target, SgExprListExp *map_variable_list,
    SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list,
    SgExprListExp *map_variable_type_list,
    std::vector<ExpandedMapEntry> *dynamic_entries_out = NULL) {
  ROSE_ASSERT(target != NULL);
  Rose_STL_Container<SgOmpClause *> from_clauses =
      getClause(target, V_SgOmpFromClause);
  Rose_STL_Container<SgOmpClause *> to_clauses =
      getClause(target, V_SgOmpToClause);

  std::vector<ExpandedMapEntry> expanded_entries;
  for (SgOmpClause *clause : from_clauses) {
    std::vector<ExpandedMapEntry> clause_items =
        collectExpandedMotionItemsForClause(target, clause);
    expanded_entries.insert(expanded_entries.end(), clause_items.begin(),
                            clause_items.end());
  }
  for (SgOmpClause *clause : to_clauses) {
    std::vector<ExpandedMapEntry> clause_items =
        collectExpandedMotionItemsForClause(target, clause);
    expanded_entries.insert(expanded_entries.end(), clause_items.begin(),
                            clause_items.end());
  }

  const bool has_dynamic_entries =
      hasDynamicExpandedMapEntries(expanded_entries);
  if (dynamic_entries_out != NULL) {
    dynamic_entries_out->clear();
    if (has_dynamic_entries) {
      *dynamic_entries_out = expanded_entries;
    }
  }
  if (!has_dynamic_entries) {
    std::vector<ResolvedMapItem> resolved_items;
    collectDirectResolvedMapItems(expanded_entries, resolved_items);
    appendResolvedMapItemArguments(
        resolved_items, map_variable_list, map_variable_base_list,
        map_variable_size_list, map_variable_type_list, target->get_scope());
  }
} // collectOmpTargetUpdateInfo()

struct RuntimeMapArgumentArrayDeclarations {
  SgVariableDeclaration *args_base_decl = nullptr;
  SgVariableDeclaration *args_decl = nullptr;
  SgVariableDeclaration *arg_sizes_decl = nullptr;
  SgVariableDeclaration *arg_types_decl = nullptr;
  SgVariableDeclaration *arg_number_decl = nullptr;
  bool uses_heap_storage = false;
};

size_t getMapArgumentListCount(SgExprListExp *map_variable_list,
                               SgExprListExp *map_variable_base_list,
                               SgExprListExp *map_variable_size_list,
                               SgExprListExp *map_variable_type_list) {
  if (map_variable_list == nullptr || map_variable_base_list == nullptr ||
      map_variable_size_list == nullptr || map_variable_type_list == nullptr) {
    return 0;
  }

  const size_t arg_count = map_variable_list->get_expressions().size();
  ROSE_ASSERT(map_variable_base_list->get_expressions().size() == arg_count);
  ROSE_ASSERT(map_variable_size_list->get_expressions().size() == arg_count);
  ROSE_ASSERT(map_variable_type_list->get_expressions().size() == arg_count);
  return arg_count;
}

SgExpression *
buildArraySectionElementIndexExpression(SgExpression *lower_bound,
                                        SgVariableSymbol *index_symbol) {
  ROSE_ASSERT(index_symbol != nullptr);

  SgExpression *index_expr = buildVarRefExp(index_symbol);
  if (lower_bound == nullptr || isSgNullExpression(lower_bound) != nullptr) {
    return index_expr;
  }

  SgType *lower_type = stripTypeAliasesAndReferences(lower_bound->get_type());
  if (lower_type != nullptr) {
    index_expr = buildCastExp(index_expr, lower_type);
  }
  return buildAddOp(copyExpression(lower_bound), index_expr);
}

SgExpression *buildArraySectionElementExpression(
    SgExpression *base_expression,
    const std::vector<std::pair<SgExpression *, SgExpression *>> &dimensions,
    const std::vector<SgVariableSymbol *> &index_symbols) {
  ROSE_ASSERT(base_expression != nullptr);
  ROSE_ASSERT(dimensions.size() == index_symbols.size());

  SgExpression *result = copyExpression(base_expression);
  for (size_t i = 0; i < dimensions.size(); ++i) {
    result =
        buildPntrArrRefExp(result, buildArraySectionElementIndexExpression(
                                       dimensions[i].first, index_symbols[i]));
  }
  return result;
}

SgExpression *buildMallocArrayInitializer(SgType *element_type,
                                          SgExpression *element_count,
                                          SgScopeStatement *scope) {
  ROSE_ASSERT(element_type != nullptr);
  ROSE_ASSERT(element_count != nullptr);
  ROSE_ASSERT(scope != nullptr);

  SgExpression *allocation_size = buildMultiplyOp(
      buildSizeOfOp(element_type), copyExpression(element_count));
  return buildCastExp(
      buildFunctionCallExp(SgName("malloc"), buildPointerType(buildVoidType()),
                           buildExprListExp(allocation_size), scope),
      buildPointerType(element_type));
}

void appendMapArgumentArrayAssignment(SgBasicBlock *block,
                                      SgScopeStatement *scope,
                                      SgVariableDeclaration *target_decl,
                                      SgVariableDeclaration *index_decl,
                                      SgExpression *value_expr,
                                      SgType *element_type) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(target_decl != nullptr);
  ROSE_ASSERT(index_decl != nullptr);
  ROSE_ASSERT(value_expr != nullptr);
  ROSE_ASSERT(element_type != nullptr);

  block->append_statement(buildAssignStatement(
      buildPntrArrRefExp(buildVarRefExp(target_decl),
                         buildVarRefExp(index_decl)),
      buildCastExp(copyExpression(value_expr), element_type)));
}

void appendRawMapArgumentListsToDynamicArrays(
    SgExprListExp *map_variable_list, SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list,
    SgExprListExp *map_variable_type_list, SgBasicBlock *block,
    SgScopeStatement *scope, SgVariableDeclaration *args_base_decl,
    SgVariableDeclaration *args_decl, SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl) {
  if (map_variable_list == nullptr || map_variable_base_list == nullptr ||
      map_variable_size_list == nullptr || map_variable_type_list == nullptr) {
    return;
  }

  const SgExpressionPtrList &args = map_variable_list->get_expressions();
  const SgExpressionPtrList &bases = map_variable_base_list->get_expressions();
  const SgExpressionPtrList &sizes = map_variable_size_list->get_expressions();
  const SgExpressionPtrList &types = map_variable_type_list->get_expressions();
  ROSE_ASSERT(args.size() == bases.size());
  ROSE_ASSERT(args.size() == sizes.size());
  ROSE_ASSERT(args.size() == types.size());

  SgType *void_ptr_type = buildPointerType(buildVoidType());
  SgType *int64_type = buildOpaqueType("int64_t", scope);
  for (size_t i = 0; i < args.size(); ++i) {
    appendMapArgumentArrayAssignment(block, scope, args_base_decl,
                                     arg_index_decl, bases[i], void_ptr_type);
    appendMapArgumentArrayAssignment(block, scope, args_decl, arg_index_decl,
                                     args[i], void_ptr_type);
    appendMapArgumentArrayAssignment(block, scope, arg_sizes_decl,
                                     arg_index_decl, sizes[i], int64_type);
    appendMapArgumentArrayAssignment(block, scope, arg_types_decl,
                                     arg_index_decl, types[i], int64_type);
    block->append_statement(buildExprStatement(
        buildPlusPlusOp(buildVarRefExp(arg_index_decl), SgUnaryOp::postfix)));
  }
}

enum class DynamicMapExpansionPass { count_only, populate };

void appendExpandedMapEntriesDynamicPass(
    const std::vector<ExpandedMapEntry> &entries, DynamicMapExpansionPass pass,
    SgBasicBlock *block, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl,
    SgVariableDeclaration *args_base_decl, SgVariableDeclaration *args_decl,
    SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl, size_t &loop_counter,
    size_t &literal_counter);

void appendExpandedMapEntryDynamicPass(
    const ExpandedMapEntry &entry, DynamicMapExpansionPass pass,
    SgBasicBlock *block, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl,
    SgVariableDeclaration *args_base_decl, SgVariableDeclaration *args_decl,
    SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl, size_t &loop_counter,
    size_t &literal_counter) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(arg_number_decl != nullptr);

  if (entry.kind == ExpandedMapEntryKind::direct_item) {
    if (pass == DynamicMapExpansionPass::count_only) {
      block->append_statement(buildExprStatement(
          buildPlusAssignOp(buildVarRefExp(arg_number_decl), buildIntVal(1))));
      return;
    }

    ROSE_ASSERT(args_base_decl != nullptr);
    ROSE_ASSERT(args_decl != nullptr);
    ROSE_ASSERT(arg_sizes_decl != nullptr);
    ROSE_ASSERT(arg_types_decl != nullptr);
    ROSE_ASSERT(arg_index_decl != nullptr);

    MapArgumentExpressions expressions =
        buildResolvedMapItemArgumentExpressions(entry.direct_item, scope);
    if (isLiteralTargetParamPackCall(expressions.mapping_expression) ||
        isLiteralTargetParamPackCall(expressions.mapping_base_expression)) {
      const std::string packed_name =
          "__rex_packed_literal_arg_dyn_" + std::to_string(literal_counter++);
      SgVariableDeclaration *packed_decl = buildVariableDeclaration(
          packed_name, buildPointerType(buildVoidType()),
          buildAssignInitializer(
              copyExpression(expressions.mapping_expression)),
          block);
      block->append_statement(packed_decl);
      SgVariableSymbol *packed_symbol = getFirstVarSym(packed_decl);
      ROSE_ASSERT(packed_symbol != nullptr);
      expressions.mapping_expression = buildVarRefExp(packed_symbol);
      expressions.mapping_base_expression = buildVarRefExp(packed_symbol);
    }

    SgType *void_ptr_type = buildPointerType(buildVoidType());
    SgType *int64_type = buildOpaqueType("int64_t", scope);
    appendMapArgumentArrayAssignment(
        block, scope, args_base_decl, arg_index_decl,
        expressions.mapping_base_expression, void_ptr_type);
    appendMapArgumentArrayAssignment(block, scope, args_decl, arg_index_decl,
                                     expressions.mapping_expression,
                                     void_ptr_type);
    appendMapArgumentArrayAssignment(
        block, scope, arg_sizes_decl, arg_index_decl,
        expressions.mapping_size_expression, int64_type);
    appendMapArgumentArrayAssignment(
        block, scope, arg_types_decl, arg_index_decl,
        expressions.mapping_type_expression, int64_type);
    block->append_statement(buildExprStatement(
        buildPlusPlusOp(buildVarRefExp(arg_index_decl), SgUnaryOp::postfix)));
    return;
  }

  ROSE_ASSERT(entry.kind == ExpandedMapEntryKind::dynamic_mapper_section);
  ROSE_ASSERT(entry.section_base_expression != nullptr);
  ROSE_ASSERT(!entry.section_dimensions.empty());
  ROSE_ASSERT(entry.resolved_mapper.declaration != nullptr);

  std::function<void(size_t, SgBasicBlock *, std::vector<SgVariableSymbol *> &)>
      build_loop_nest;
  build_loop_nest = [&](size_t dim_index, SgBasicBlock *current_block,
                        std::vector<SgVariableSymbol *> &index_symbols) {
    if (dim_index == entry.section_dimensions.size()) {
      SgExpression *element_expr = buildArraySectionElementExpression(
          entry.section_base_expression, entry.section_dimensions,
          index_symbols);
      std::vector<ExpandedMapEntry> nested_entries;
      std::vector<const SgOmpDeclareMapperStatement *> active_mappers;
      collectExpandedMapEntriesUsingResolvedMapper(
          element_expr, entry.resolved_mapper, entry.use_kind, entry.use_map_op,
          entry.runtime_flag_bits, entry.anchor_stmt, nested_entries,
          active_mappers);
      appendExpandedMapEntriesDynamicPass(
          nested_entries, pass, current_block, scope, arg_number_decl,
          args_base_decl, args_decl, arg_sizes_decl, arg_types_decl,
          arg_index_decl, loop_counter, literal_counter);
      return;
    }

    const std::pair<SgExpression *, SgExpression *> &dimension =
        entry.section_dimensions[dim_index];
    SgExpression *length_expr = dimension.second;
    if (length_expr == nullptr || isSgNullExpression(length_expr) != nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Missing mapper section length while expanding "
          << entry.section_base_expression->unparseToString();
      ROSE_ABORT();
    }

    const std::string index_name =
        "__rex_mapper_section_index_" + std::to_string(loop_counter++);
    SgType *index_type = buildOpaqueType("int64_t", scope);
    SgVariableDeclaration *index_decl = buildVariableDeclaration(
        index_name, index_type, buildAssignInitializer(buildLongLongIntVal(0)),
        current_block);
    SgVariableSymbol *index_symbol = getFirstVarSym(index_decl);
    ROSE_ASSERT(index_symbol != nullptr);
    index_symbols.push_back(index_symbol);

    SgBasicBlock *loop_body = buildBasicBlock();
    build_loop_nest(dim_index + 1, loop_body, index_symbols);
    current_block->append_statement(buildForStatement_nfi(
        index_decl,
        buildExprStatement(buildLessThanOp(
            buildVarRefExp(index_symbol),
            buildCastExp(copyExpression(length_expr), index_type))),
        buildPlusPlusOp(buildVarRefExp(index_symbol), SgUnaryOp::postfix),
        loop_body));
    index_symbols.pop_back();
  };

  std::vector<SgVariableSymbol *> index_symbols;
  build_loop_nest(0, block, index_symbols);
}

void appendExpandedMapEntriesDynamicPass(
    const std::vector<ExpandedMapEntry> &entries, DynamicMapExpansionPass pass,
    SgBasicBlock *block, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl,
    SgVariableDeclaration *args_base_decl, SgVariableDeclaration *args_decl,
    SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl, size_t &loop_counter,
    size_t &literal_counter) {
  for (const ExpandedMapEntry &entry : entries) {
    appendExpandedMapEntryDynamicPass(
        entry, pass, block, scope, arg_number_decl, args_base_decl, args_decl,
        arg_sizes_decl, arg_types_decl, arg_index_decl, loop_counter,
        literal_counter);
  }
}

RuntimeMapArgumentArrayDeclarations buildDynamicRuntimeMapArgumentArrays(
    SgBasicBlock *block, SgScopeStatement *scope,
    SgExprListExp *prefix_map_variable_list,
    SgExprListExp *prefix_map_variable_base_list,
    SgExprListExp *prefix_map_variable_size_list,
    SgExprListExp *prefix_map_variable_type_list,
    const std::vector<ExpandedMapEntry> &dynamic_entries,
    SgExprListExp *suffix_map_variable_list = NULL,
    SgExprListExp *suffix_map_variable_base_list = NULL,
    SgExprListExp *suffix_map_variable_size_list = NULL,
    SgExprListExp *suffix_map_variable_type_list = NULL) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const size_t prefix_count = getMapArgumentListCount(
      prefix_map_variable_list, prefix_map_variable_base_list,
      prefix_map_variable_size_list, prefix_map_variable_type_list);
  const size_t suffix_count = getMapArgumentListCount(
      suffix_map_variable_list, suffix_map_variable_base_list,
      suffix_map_variable_size_list, suffix_map_variable_type_list);

  RuntimeMapArgumentArrayDeclarations result;
  result.uses_heap_storage = true;
  result.arg_number_decl = buildVariableDeclaration(
      "__arg_num", buildOpaqueType("int32_t", scope),
      buildAssignInitializer(
          buildIntVal(static_cast<int>(prefix_count + suffix_count))),
      block);
  block->append_statement(result.arg_number_decl);

  size_t loop_counter = 0;
  size_t literal_counter = 0;
  appendExpandedMapEntriesDynamicPass(
      dynamic_entries, DynamicMapExpansionPass::count_only, block, scope,
      result.arg_number_decl, NULL, NULL, NULL, NULL, NULL, loop_counter,
      literal_counter);

  SgExpression *arg_count_expr = buildVarRefExp(result.arg_number_decl);
  SgType *void_ptr_type = buildPointerType(buildVoidType());
  SgType *void_ptr_ptr_type = buildPointerType(void_ptr_type);
  SgType *int64_type = buildOpaqueType("int64_t", scope);
  SgType *int64_ptr_type = buildPointerType(int64_type);

  result.args_base_decl = buildVariableDeclaration(
      "__args_base", void_ptr_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(void_ptr_type, arg_count_expr, scope)),
      block);
  block->append_statement(result.args_base_decl);

  result.args_decl = buildVariableDeclaration(
      "__args", void_ptr_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(void_ptr_type, arg_count_expr, scope)),
      block);
  block->append_statement(result.args_decl);

  result.arg_sizes_decl = buildVariableDeclaration(
      "__arg_sizes", int64_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(int64_type, arg_count_expr, scope)),
      block);
  block->append_statement(result.arg_sizes_decl);

  result.arg_types_decl = buildVariableDeclaration(
      "__arg_types", int64_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(int64_type, arg_count_expr, scope)),
      block);
  block->append_statement(result.arg_types_decl);

  SgVariableDeclaration *arg_index_decl =
      buildVariableDeclaration("__arg_index", buildOpaqueType("int32_t", scope),
                               buildAssignInitializer(buildIntVal(0)), block);
  block->append_statement(arg_index_decl);

  appendRawMapArgumentListsToDynamicArrays(
      prefix_map_variable_list, prefix_map_variable_base_list,
      prefix_map_variable_size_list, prefix_map_variable_type_list, block,
      scope, result.args_base_decl, result.args_decl, result.arg_sizes_decl,
      result.arg_types_decl, arg_index_decl);

  loop_counter = 0;
  literal_counter = 0;
  appendExpandedMapEntriesDynamicPass(
      dynamic_entries, DynamicMapExpansionPass::populate, block, scope,
      result.arg_number_decl, result.args_base_decl, result.args_decl,
      result.arg_sizes_decl, result.arg_types_decl, arg_index_decl,
      loop_counter, literal_counter);

  appendRawMapArgumentListsToDynamicArrays(
      suffix_map_variable_list, suffix_map_variable_base_list,
      suffix_map_variable_size_list, suffix_map_variable_type_list, block,
      scope, result.args_base_decl, result.args_decl, result.arg_sizes_decl,
      result.arg_types_decl, arg_index_decl);

  return result;
}

void appendDynamicRuntimeMapArgumentArrayCleanup(
    const RuntimeMapArgumentArrayDeclarations &arrays, SgBasicBlock *block,
    SgScopeStatement *scope) {
  if (!arrays.uses_heap_storage) {
    return;
  }

  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const SgVariableDeclaration *declarations[] = {
      arrays.arg_types_decl, arrays.arg_sizes_decl, arrays.args_decl,
      arrays.args_base_decl};
  for (const SgVariableDeclaration *decl : declarations) {
    ROSE_ASSERT(decl != nullptr);
    block->append_statement(buildFunctionCallStmt(
        "free", buildVoidType(),
        buildExprListExp(buildCastExp(
            buildVarRefExp(const_cast<SgVariableDeclaration *>(decl)),
            buildPointerType(buildVoidType()))),
        scope));
  }
}

static SgVariableDeclaration *buildTargetKernelArgsDeclaration(
    SgGlobal *global_scope, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl, SgVariableDeclaration *args_base,
    SgVariableDeclaration *args, SgVariableDeclaration *arg_sizes,
    SgVariableDeclaration *arg_types, SgVariableDeclaration *num_blocks_decl,
    SgVariableDeclaration *threads_per_block_decl, SgExpression *tripcount);

// Translate a parallel region under "omp target"
/*

 call customized outlining, the generateTask() for omp task or regular omp
 parallel is not compatible since we want to use the classic outlining support:
 each variable is passed as a separate parameter.

 We also use the revised generateFunc() to explicitly specify pass by original
 type vs. pass using pointer type

 */
void transOmpTargetSpmd(SgNode *node, SgExpression *omp_num_teams,
                        SgExpression *omp_num_threads) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // device expression
  SgExpression *device_expression = NULL;
  device_expression =
      getClauseExpression(target, VariantVector(V_SgOmpDeviceClause));
  // If not found, use the default ID 0
  if (device_expression == NULL)
    device_expression = buildIntVal(0);

  // Now we need to ensure that "omp target " has a basic block as its body
  // so we can insert declarations into an inner block, instead of colliding
  // declarations within the scope of "omp target" This is important since we
  // often have consecutive "omp target" regions within one big scope We cannot
  // just insert things into that big scope.
  SgBasicBlock *omp_target_stmt_body_block =
      ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(isSgBasicBlock(target->get_body()));

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  // 1/15/2009, Liao, also handle the last #endif, which is attached inside of
  // the target
  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  //-----------------------------------------------------------------
  // step 1: generated an outlined function and make it a CUDA function
  SgOmpClauseBodyStatement *target_parallel_stmt =
      isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target_parallel_stmt);
  // Prepare the outliner
  Outliner::enable_classic = true;
  //    Outliner::useParameterWrapper = false; //TODO: better handling of the
  //    dependence among flags
  SgBasicBlock *body_block = Outliner::preprocess(body);
  // translator OpenMP 3.0 and earlier variables.
  transOmpVariables(target, body_block);

  ASTtools::VarSymSet_t all_syms; // all generated or remaining variables to be
                                  // passed to the outliner
  // This addressOf_syms does not apply to CUDA kernel generation: since we
  // cannot use pass-by-reference for CUDA kernel. If we want to copy back
  // value, we have to use memory copy  since they are in two different memory
  // spaces.
  ASTtools::VarSymSet_t
      addressOf_syms; // generated or remaining variables should be passed by
                      // using their addresses

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  all_syms = transOmpMapVariables(
      target, map_variable_list, map_variable_base_list, map_variable_size_list,
      map_variable_type_list, &offload_ctx,
      &dynamic_map_entries); //, addressOf_syms);

  ASTtools::VarSymSet_t
      per_block_reduction_syms; // translation generated per block reduction
                                // symbols with name like _dev_per_block within
                                // the enclosed for loop

  // collect possible per block reduction variables introduced by
  // transOmpTargetLoop() we rely on the pattern of such variables:
  // _dev_per_block_* these variables are arrays already, we pass them by their
  // original types, not addressOf types
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(body_block, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    SgName var_name = vRef->get_symbol()->get_name();
    string var_name_str = var_name.getString();
    if (var_name_str.find("_dev_per_block_", 0) == 0) {
      all_syms.insert(vRef->get_symbol());
      per_block_reduction_syms.insert(vRef->get_symbol());
    }
  }

  string func_name = Outliner::generateFuncName(target);
  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  // pass all the parameters by reference
  for (std::set<const SgVariableSymbol *>::iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    if (!isPointerType((*iter)->get_type()) &&
        !isSgArrayType((*iter)->get_type()) &&
        offload_ctx.literal_target_param_syms.find(
            const_cast<SgVariableSymbol *>(*iter)) ==
            offload_ctx.literal_target_param_syms.end()) {
      addressOf_syms.insert(*iter);
    };
  };

  std::set<SgInitializedName *> restoreVars;
  SgFunctionDeclaration *result =
      Outliner::generateFunction(body_block, func_name + "kernel__", all_syms,
                                 addressOf_syms, restoreVars, NULL, g_scope);
  SgFunctionDeclaration *result_decl =
      isSgFunctionDeclaration(result->get_firstNondefiningDeclaration());
  ROSE_ASSERT(result_decl != NULL);
  lowerLiteralTargetKernelParameters(result,
                                     offload_ctx.literal_target_param_syms);
  maybeRecordTargetKernelLaunchBounds(result, omp_num_threads);
  result_decl->get_functionModifier()
      .setCudaKernel(); // add __global__ modifier

  result->get_functionModifier().setCudaKernel();

  // This one is not desired. It inserts the function to the end and prepend a
  // prototype Outliner::insert(result, g_scope, body_block);
  // TODO: better interface to specify where exactly to insert the function!
  // Custom insertion:  insert right before the enclosing function of "omp
  // target"
  SgFunctionDeclaration *target_func = const_cast<SgFunctionDeclaration *>(
      SageInterface::getEnclosingFunctionDeclaration(target));
  ROSE_ASSERT(target_func != NULL);
  insertStatementAfter(target_func, result);
  // TODO: this really should be done within Outliner::generateFunction()
  // TODO: we have to patch up first nondefining function declaration since
  // custom insertion is used
  SgGlobal *glob_scope = getGlobalScope(target);
  ROSE_ASSERT(glob_scope != NULL);
  SgFunctionSymbol *func_symbol =
      glob_scope->lookup_function_symbol(result->get_name());
  ROSE_ASSERT(func_symbol != NULL);

  SgScopeStatement *p_scope =
      target->get_scope(); // the scope of "omp parallel" will be destroyed
                           // later, so we use scope of "omp target"
  ROSE_ASSERT(p_scope != NULL);

  // create the outlined driver for GPU offloading, which is empty at this point
  SgBasicBlock *outlined_driver_body = buildBasicBlock();

  // insert dim3 threadsPerBlock(xomp_get_maxThreadsPerBlock());
  // TODO: for 1-D mapping, int type is enough,  //TODO: a better interface
  // accepting expression as initializer!!
  SgVariableDeclaration *threads_per_block_decl = buildVariableDeclaration(
      "_threads_per_block_", buildIntType(),
      buildAssignInitializer(omp_num_threads), p_scope);
  outlined_driver_body->append_statement(threads_per_block_decl);
  attachComment(threads_per_block_decl, string("Launch CUDA kernel ..."));

  SgVariableDeclaration *num_blocks_decl =
      buildVariableDeclaration("_num_blocks_", buildIntType(),
                               buildAssignInitializer(omp_num_teams), p_scope);
  outlined_driver_body->append_statement(num_blocks_decl);

  // Now we have num_block declaration, we can insert the per block declaration
  // used for reduction variables
  SgExpression *shared_data = NULL; // shared data size expression for CUDA
                                    // kernel execution configuration
  for (std::vector<SgVariableDeclaration *>::iterator iter =
           offload_ctx.per_block_declarations.begin();
       iter != offload_ctx.per_block_declarations.end(); iter++) {
    SgVariableDeclaration *decl = *iter;
    insertStatementAfter(num_blocks_decl, decl);
    SgVariableSymbol *sym = getFirstVarSym(decl);
    SgPointerType *pointer_type = isSgPointerType(sym->get_type());
    ROSE_ASSERT(pointer_type != NULL);
    SgType *base_type = pointer_type->get_base_type();
    if (offload_ctx.per_block_declarations.size() > 1) {
      cerr << "Error. multiple reduction variables are not yet handled."
           << endl;
      ROSE_ABORT();
      // threadsPerBlock.x*sizeof(REAL)  //TODO: how to handle multiple shared
      // data blocks, each for a reduction variable??
    }
    shared_data = buildMultiplyOp(buildVarRefExp(threads_per_block_decl),
                                  buildSizeOfOp(base_type));
  }

  // func_symbol =
  // isSgFunctionSymbol(result->get_firstNondefiningDeclaration()->get_symbol_from_symbol_table
  // ());
  ROSE_ASSERT(func_symbol != NULL);
  SgExprListExp *exp_list_exp = SageBuilder::buildExprListExp();

  std::set<SgInitializedName *> varsUsingOriginalForm;
  for (ASTtools::VarSymSet_t::const_iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    const SgVariableSymbol *current_symbol = *iter;
    // this addressOf_syms does not apply to CUDA kernel generation: since we
    // cannot use pass-by-reference for CUDA kernel. If we want to copy back
    // value, we have to use memory copy  since they are in two different memory
    // spaces. So all variables should use original form in this context.
    if (addressOf_syms.find(current_symbol) ==
        addressOf_syms.end()) // not found in Address Of variable set
      varsUsingOriginalForm.insert(current_symbol->get_declaration());
  }
  // TODO: alternative mirror form using varUsingAddress as parameter
  Outliner::appendIndividualFunctionCallArgs(all_syms, varsUsingOriginalForm,
                                             exp_list_exp);
  // TODO: builder interface without _nfi, and match function call exp builder
  // interface convention:

  // in the original function, we call the outlined driver and pass all the
  // required variables by reference prepare all the parameters for using LLVM
  // GPU offloading
  SgClassDeclaration *tgt_offload_entry =
      buildStructDeclaration("__tgt_offload_entry", getGlobalScope(target));

  kmpc_kernel_id_counter += 1;
  SgVariableDeclaration *outlined_kernel_id_decl =
      buildVariableDeclaration(func_name + "id__", buildCharType(),
                               buildAssignInitializer(buildIntVal(0)), g_scope);

  // Use the OpenMP runtime's default device sentinel.
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", buildOpaqueType("int64_t", p_scope),
      buildAssignInitializer(buildLongLongIntVal(-1)), p_scope);
  outlined_driver_body->append_statement(device_id_decl);

  // define the entry point
  SgExprListExp *offload_entry_parameters = buildExprListExp(
      buildCastExp(buildAddressOfOp(buildVarRefExp(outlined_kernel_id_decl)),
                   buildPointerType(buildVoidType())),
      buildStringVal(func_name + "kernel__"), buildIntVal(0), buildIntVal(0),
      buildIntVal(0));
  SgBracedInitializer *offload_entry_initilization =
      buildBracedInitializer(offload_entry_parameters);
  SgVariableDeclaration *offload_entry_decl = buildVariableDeclaration(
      func_name + "omp_offload_entry__", tgt_offload_entry->get_type(),
      buildAssignInitializer(offload_entry_initilization), g_scope);
  offload_entry_decl->get_decl_item(SgName(func_name + "omp_offload_entry__"))
      ->set_gnu_attribute_section_name("omp_offloading_entries");

  prependGlobalDeclPreservingLeadingPreproc(offload_entry_decl, g_scope);
  prependGlobalDeclPreservingLeadingPreproc(outlined_kernel_id_decl, g_scope);

  SgVariableDeclaration *host_point_decl = buildVariableDeclaration(
      "__host_ptr", buildPointerType(buildVoidType()),
      buildAssignInitializer(buildCastExp(
          buildAddressOfOp(buildVarRefExp(outlined_kernel_id_decl)),
          buildPointerType(buildVoidType()))),
      p_scope);
  outlined_driver_body->append_statement(host_point_decl);

  SgVariableDeclaration *args_base_decl = nullptr;
  SgVariableDeclaration *args_decl = nullptr;
  SgVariableDeclaration *arg_sizes = nullptr;
  SgVariableDeclaration *arg_types = nullptr;
  SgVariableDeclaration *arg_number_decl = nullptr;
  RuntimeMapArgumentArrayDeclarations dynamic_arrays;
  if (!dynamic_map_entries.empty()) {
    dynamic_arrays = buildDynamicRuntimeMapArgumentArrays(
        outlined_driver_body, p_scope, map_variable_list,
        map_variable_base_list, map_variable_size_list, map_variable_type_list,
        dynamic_map_entries);
    args_base_decl = dynamic_arrays.args_base_decl;
    args_decl = dynamic_arrays.args_decl;
    arg_sizes = dynamic_arrays.arg_sizes_decl;
    arg_types = dynamic_arrays.arg_types_decl;
    arg_number_decl = dynamic_arrays.arg_number_decl;
  } else {
    materializeLiteralTargetArgExpressions(map_variable_list,
                                           map_variable_base_list,
                                           outlined_driver_body, p_scope);

    SgBracedInitializer *offloading_variables_base =
        buildBracedInitializer(map_variable_base_list);
    args_base_decl = buildVariableDeclaration(
        "__args_base", buildArrayType(buildPointerType(buildVoidType())),
        buildAssignInitializer(offloading_variables_base), p_scope);
    outlined_driver_body->append_statement(args_base_decl);

    SgBracedInitializer *offloading_variables =
        buildBracedInitializer(map_variable_list);
    args_decl = buildVariableDeclaration(
        "__args", buildArrayType(buildPointerType(buildVoidType())),
        buildAssignInitializer(offloading_variables), p_scope);
    outlined_driver_body->append_statement(args_decl);

    SgBracedInitializer *map_variable_sizes =
        buildBracedInitializer(map_variable_size_list);
    arg_sizes = buildVariableDeclaration(
        "__arg_sizes", buildArrayType(buildOpaqueType("int64_t", p_scope)),
        buildAssignInitializer(map_variable_sizes), p_scope);
    outlined_driver_body->append_statement(arg_sizes);

    SgBracedInitializer *map_variable_types =
        buildBracedInitializer(map_variable_type_list);
    arg_types = buildVariableDeclaration(
        "__arg_types", buildArrayType(buildOpaqueType("int64_t", p_scope)),
        buildAssignInitializer(map_variable_types), p_scope);
    outlined_driver_body->append_statement(arg_types);

    int kernel_arg_num = map_variable_base_list->get_expressions().size();
    arg_number_decl = buildVariableDeclaration(
        "__arg_num", buildOpaqueType("int32_t", p_scope),
        buildAssignInitializer(buildIntVal(kernel_arg_num)), p_scope);
    outlined_driver_body->append_statement(arg_number_decl);
  }

  SgVariableDeclaration *kernel_args_decl = buildTargetKernelArgsDeclaration(
      g_scope, p_scope, arg_number_decl, args_base_decl, args_decl, arg_sizes,
      arg_types, num_blocks_decl, threads_per_block_decl, NULL);
  outlined_driver_body->append_statement(kernel_args_decl);

  // call __tgt_target_kernel to execute the CUDA kernel
  SgVariableSymbol *kernel_args_sym = getFirstVarSym(kernel_args_decl);
  ROSE_ASSERT(kernel_args_sym != NULL);
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(num_blocks_decl),
      buildVarRefExp(threads_per_block_decl), buildVarRefExp(host_point_decl),
      buildAddressOfOp(buildVarRefExp(kernel_args_sym)));
  string func_offloading_name = "__tgt_target_kernel";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildIntType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  outlined_driver_body->append_statement(func_offloading_stmt);

  appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays,
                                              outlined_driver_body, p_scope);

  SageInterface::fixStatement(outlined_driver_body, p_scope);
  //------------now remove omp parallel since everything within it has been
  // outlined to a function
  replaceStatement(target, outlined_driver_body, true);

  target_outlined_function_list->push_back(isSgFunctionDeclaration(result));
}

static SgExpression *buildKernelArgNullPtrExpr() {
  return buildCastExp(buildIntVal(0),
                      buildPointerType(buildPointerType(buildVoidType())));
}

static SgExpression *buildKernelLaunchDimInitializer(SgExpression *x_dim_expr) {
  return buildAggregateInitializer(buildExprListExp(
      copyExpression(x_dim_expr), buildIntVal(1), buildIntVal(1)));
}

static SgVariableDeclaration *buildTargetKernelArgsDeclaration(
    SgGlobal *global_scope, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl, SgVariableDeclaration *args_base,
    SgVariableDeclaration *args, SgVariableDeclaration *arg_sizes,
    SgVariableDeclaration *arg_types, SgVariableDeclaration *num_blocks_decl,
    SgVariableDeclaration *threads_per_block_decl, SgExpression *tripcount) {
  ROSE_ASSERT(global_scope != NULL);
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(arg_number_decl != NULL);
  ROSE_ASSERT(args_base != NULL);
  ROSE_ASSERT(args != NULL);
  ROSE_ASSERT(arg_sizes != NULL);
  ROSE_ASSERT(arg_types != NULL);
  ROSE_ASSERT(num_blocks_decl != NULL);
  ROSE_ASSERT(threads_per_block_decl != NULL);

  SgClassDeclaration *kernel_args_decl =
      buildStructDeclaration("__tgt_kernel_arguments", global_scope);
  ROSE_ASSERT(kernel_args_decl != NULL);

  SgType *int64_type = buildOpaqueType("int64_t", scope);
  SgExpression *tripcount_expr =
      tripcount != NULL ? buildCastExp(copyExpression(tripcount), int64_type)
                        : buildCastExp(buildLongLongIntVal(0), int64_type);

  std::vector<SgExpression *> kernel_args_exprs;
  kernel_args_exprs.push_back(buildIntVal(3));
  kernel_args_exprs.push_back(buildVarRefExp(arg_number_decl));
  kernel_args_exprs.push_back(buildVarRefExp(args_base));
  kernel_args_exprs.push_back(buildVarRefExp(args));
  kernel_args_exprs.push_back(buildVarRefExp(arg_sizes));
  kernel_args_exprs.push_back(buildVarRefExp(arg_types));
  kernel_args_exprs.push_back(buildKernelArgNullPtrExpr());
  kernel_args_exprs.push_back(buildKernelArgNullPtrExpr());
  kernel_args_exprs.push_back(tripcount_expr);
  kernel_args_exprs.push_back(buildLongLongIntVal(0));
  kernel_args_exprs.push_back(
      buildKernelLaunchDimInitializer(buildVarRefExp(num_blocks_decl)));
  kernel_args_exprs.push_back(
      buildKernelLaunchDimInitializer(buildVarRefExp(threads_per_block_decl)));
  kernel_args_exprs.push_back(buildIntVal(0));

  SgBracedInitializer *kernel_args_init =
      buildBracedInitializer(buildExprListExp(kernel_args_exprs));

  return buildVariableDeclaration("__kernel_args", kernel_args_decl->get_type(),
                                  buildAssignInitializer(kernel_args_init),
                                  scope);
}

struct TargetLoopLoweringInfo {
  SgInitializedName *orig_index = nullptr;
  SgExpression *orig_lower = nullptr;
  SgExpression *orig_upper = nullptr;
  SgExpression *orig_stride = nullptr;
  bool is_incremental = true;
  bool is_inclusive_bound = true;
};

static SgExpression *
buildTargetLoopTripCountExpr(const TargetLoopLoweringInfo &info) {
  SgExpression *distance = nullptr;
  if (info.is_incremental) {
    distance =
        buildSubtractOp(deepCopy(info.orig_upper), deepCopy(info.orig_lower));
  } else {
    distance =
        buildSubtractOp(deepCopy(info.orig_lower), deepCopy(info.orig_upper));
  }
  if (info.is_inclusive_bound) {
    distance = buildAddOp(distance, buildIntVal(1));
  }
  return distance;
}

static SgExpression *buildCudaDimXRef(const std::string &name,
                                      SgScopeStatement *scope) {
  return buildOpaqueVarRefExp(name, scope);
}

static SgExpression *buildCudaGlobalThreadIdXExpr(SgScopeStatement *scope) {
  return buildAddOp(buildMultiplyOp(buildCudaDimXRef("blockDim.x", scope),
                                    buildCudaDimXRef("blockIdx.x", scope)),
                    buildCudaDimXRef("threadIdx.x", scope));
}

static SgExpression *buildCudaGlobalThreadCountXExpr(SgScopeStatement *scope) {
  return buildMultiplyOp(buildCudaDimXRef("gridDim.x", scope),
                         buildCudaDimXRef("blockDim.x", scope));
}

static TargetLoopLoweringInfo
analyzeTargetLoopForGpu(SgForStatement *for_loop) {
  ROSE_ASSERT(for_loop != NULL);

  // In target-offloading outlined kernels, loop indices can appear as pointer
  // dereferences (e.g., *ip__). Rewrite them to local scalar indices first so
  // canonical-loop analysis and normalization can proceed.
  rewritePointerBasedForIndices(for_loop);

  // For the init statement: for (int i=0;... ) becomes int i; for (i=0;..)
  // For test expression: i<x is normalized to i<= (x-1) and i>x is normalized
  // to i>= (x+1). For increment expression: i++ is normalized to i+=1 and
  // i-- is normalized to i+=-1.
  SageInterface::forLoopNormalization(for_loop);

  TargetLoopLoweringInfo info;
  bool is_canonical = isCanonicalForLoop(
      for_loop, &info.orig_index, &info.orig_lower, &info.orig_upper,
      &info.orig_stride, NULL, &info.is_incremental);
  ROSE_ASSERT(is_canonical == true);
  info.is_inclusive_bound = true;
  return info;
}

static bool analyzeTargetLoopForGpuReadOnly(SgForStatement *for_loop,
                                            TargetLoopLoweringInfo *info) {
  if (for_loop == NULL || info == NULL) {
    return false;
  }

  bool is_canonical = SageInterface::isCanonicalForLoop(
      for_loop, &info->orig_index, &info->orig_lower, &info->orig_upper,
      &info->orig_stride, NULL, &info->is_incremental,
      &info->is_inclusive_bound);
  if (is_canonical) {
    return true;
  }

  is_canonical = recoverCanonicalForLoopControl(
      for_loop, &info->orig_index, &info->orig_lower, &info->orig_upper,
      &info->orig_stride, &info->is_incremental);
  if (!is_canonical) {
    return false;
  }

  SgBinaryOp *test_expr = isSgBinaryOp(for_loop->get_test_expr());
  if (test_expr == NULL) {
    return false;
  }
  info->is_inclusive_bound = isSgLessOrEqualOp(test_expr) != NULL ||
                             isSgGreaterOrEqualOp(test_expr) != NULL;
  return true;
}

static bool expressionDependsOnVarsDeclaredInside(SgExpression *expr,
                                                  SgNode *region_root) {
  if (expr == NULL || region_root == NULL) {
    return false;
  }

  Rose_STL_Container<SgNode *> refs =
      NodeQuery::querySubTree(expr, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = refs.begin();
       it != refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == NULL || ref->get_symbol() == NULL) {
      continue;
    }
    SgInitializedName *decl = ref->get_symbol()->get_declaration();
    if (decl != NULL && isAncestor(region_root, decl)) {
      return true;
    }
  }
  return false;
}

static bool canUseDirectTargetLoopFastPath(const TargetLoopLoweringInfo &info) {
  return info.orig_index != nullptr && info.orig_lower != nullptr &&
         info.orig_upper != nullptr && info.orig_stride != nullptr;
}

static SgVariableDeclaration *
findHoistedTargetLoopIndexDeclaration(SgForStatement *for_loop,
                                      const TargetLoopLoweringInfo &info) {
  if (for_loop == nullptr || info.orig_index == nullptr) {
    return nullptr;
  }

  SgVariableDeclaration *index_decl =
      isSgVariableDeclaration(info.orig_index->get_declaration());
  if (index_decl == nullptr) {
    return nullptr;
  }

  // forLoopNormalization() and rewritePointerBasedForIndices() both hoist
  // the loop index declaration to the statement immediately preceding the
  // transformed loop. Only move that tightly-coupled declaration.
  if (SageInterface::getPreviousStatement(for_loop, false) != index_decl) {
    return nullptr;
  }

  const SgInitializedNamePtrList &decl_vars = index_decl->get_variables();
  if (decl_vars.size() != 1 || decl_vars.front() != info.orig_index) {
    return nullptr;
  }

  return index_decl;
}

static void
lowerTargetLoopDirectGridStride(SgForStatement *for_loop, SgBasicBlock *bb1,
                                const TargetLoopLoweringInfo &info) {
  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildCudaGlobalThreadCountXExpr(bb1), buildIntType());
  SgVariableDeclaration *dev_thread_num_decl = buildVariableDeclaration(
      "_dev_thread_num", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_num_decl, bb1);
  SgVariableSymbol *dev_thread_num_symbol = getFirstVarSym(dev_thread_num_decl);
  ROSE_ASSERT(dev_thread_num_symbol != NULL);

  init_idx =
      buildAssignInitializer(buildCudaGlobalThreadIdXExpr(bb1), buildIntType());
  SgVariableDeclaration *dev_thread_id_decl =
      buildVariableDeclaration("_dev_thread_id", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_id_decl, bb1);
  SgVariableSymbol *dev_thread_id_symbol = getFirstVarSym(dev_thread_id_decl);
  ROSE_ASSERT(dev_thread_id_symbol != NULL);

  setLoopLowerBound(
      for_loop, buildAddOp(deepCopy(info.orig_lower),
                           buildMultiplyOp(buildVarRefExp(dev_thread_id_symbol),
                                           deepCopy(info.orig_stride))));
  setLoopUpperBound(for_loop, deepCopy(info.orig_upper));
  setLoopStride(for_loop, buildMultiplyOp(buildVarRefExp(dev_thread_num_symbol),
                                          deepCopy(info.orig_stride)));

  appendStatement(for_loop, bb1);

  SgInitializedName *outer_index = getLoopIndexVariable(for_loop);
  SgVariableSymbol *outer_index_sym =
      outer_index != nullptr
          ? isSgVariableSymbol(outer_index->get_symbol_from_symbol_table())
          : nullptr;
  scalarizeDirectGridStrideOuterIndexAccesses(for_loop, outer_index_sym);
  hoistReadOnlyInvariantAggregateRefsBeforeLoop(for_loop);
  hoistReadOnlyInvariantFieldAccessesBeforeLoop(for_loop);
  rewriteReadOnlyDeviceLoadsWithLdg(for_loop);
}

static void lowerTargetLoopRoundRobin(SgForStatement *for_loop,
                                      SgBasicBlock *bb1,
                                      const TargetLoopLoweringInfo &info) {
  SgVariableDeclaration *dev_lower_decl =
      buildVariableDeclaration("_dev_lower", buildIntType(), NULL, bb1);
  appendStatement(dev_lower_decl, bb1);
  SgVariableDeclaration *dev_upper_decl =
      buildVariableDeclaration("_dev_upper", buildIntType(), NULL, bb1);
  appendStatement(dev_upper_decl, bb1);
  SgVariableDeclaration *dev_loop_chunk_size_decl = buildVariableDeclaration(
      "_dev_loop_chunk_size", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_chunk_size_decl, bb1);
  SgVariableDeclaration *dev_loop_sched_index_decl = buildVariableDeclaration(
      "_dev_loop_sched_index", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_sched_index_decl, bb1);
  SgVariableDeclaration *dev_loop_stride_decl =
      buildVariableDeclaration("_dev_loop_stride", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_stride_decl, bb1);

  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getCUDABlockThreadCount"), buildIntType(),
                           buildExprListExp(buildIntVal(1)), bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_num_decl = buildVariableDeclaration(
      "_dev_thread_num", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_num_decl, bb1);
  SgVariableSymbol *dev_thread_num_symbol = getFirstVarSym(dev_thread_num_decl);
  ROSE_ASSERT(dev_thread_num_symbol != NULL);

  init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getLoopIndexFromCUDAVariables"),
                           buildIntType(), buildExprListExp(buildIntVal(1)),
                           bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_id_decl =
      buildVariableDeclaration("_dev_thread_id", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_id_decl, bb1);
  SgVariableSymbol *dev_thread_id_symbol = getFirstVarSym(dev_thread_id_decl);
  ROSE_ASSERT(dev_thread_id_symbol != NULL);

  SgExprListExp *parameters = buildExprListExp(
      copyExpression(info.orig_lower), copyExpression(info.orig_upper),
      copyExpression(info.orig_stride), buildIntVal(1),
      buildVarRefExp(dev_thread_num_symbol),
      buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_chunk_size_decl))));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_sched_index_decl))));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_stride_decl))));
  SgStatement *call_stmt = buildFunctionCallStmt(
      "XOMP_static_sched_init", buildVoidType(), parameters, bb1);
  appendStatement(call_stmt, bb1);

  parameters = buildExprListExp(
      buildAddressOfOp(
          buildVarRefExp(getFirstVarSym(dev_loop_sched_index_decl))),
      copyExpression(info.orig_upper), copyExpression(info.orig_stride),
      buildVarRefExp(getFirstVarSym(dev_loop_stride_decl)),
      buildVarRefExp(getFirstVarSym(dev_loop_chunk_size_decl)));
  appendExpression(parameters, buildVarRefExp(dev_thread_num_symbol));
  appendExpression(parameters, buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_lower_decl))));
  appendExpression(parameters, buildAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_upper_decl))));
  SgExpression *func_call_exp = buildFunctionCallExp(
      "XOMP_static_sched_next", buildBoolType(), parameters, bb1);

  SgWhileStmt *w_stmt = buildWhileStmt(func_call_exp, for_loop);
  appendStatement(w_stmt, bb1);

  setLoopLowerBound(for_loop, buildVarRefExp(getFirstVarSym(dev_lower_decl)));
  setLoopUpperBound(for_loop, buildVarRefExp(getFirstVarSym(dev_upper_decl)));
}

// Transform the worksharing loop in a target spmd region
SgBasicBlock *transOmpTargetLoopBlock(SgNode *node,
                                      bool *used_direct_grid_stride,
                                      GpuOffloadLoweringContext *offload_ctx) {
  // step 0: Sanity check
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(offload_ctx != NULL);
  (void)offload_ctx;
  SgForStatement *for_loop = isSgForStatement(node);
  ROSE_ASSERT(for_loop != NULL);

  TargetLoopLoweringInfo info = analyzeTargetLoopForGpu(for_loop);

  // TODO: Fortran support later on
  ROSE_ASSERT(for_loop != NULL);
  // SgBasicBlock* loop_body = ensureBasicBlockAsBodyOfFor (for_loop);

  // Step 2. Insert a basic block to replace SgOmpForStatement
  //  This newly introduced scope is used to hold loop variables ,etc
  SgVariableDeclaration *hoisted_index_decl =
      findHoistedTargetLoopIndexDeclaration(for_loop, info);
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();
  replaceStatement(for_loop, bb1, true);
  if (hoisted_index_decl != NULL) {
    SageInterface::removeStatement(hoisted_index_decl, false);
    appendStatement(hoisted_index_decl, bb1);
  }

  bool use_direct_grid_stride = canUseDirectTargetLoopFastPath(info);
  if (used_direct_grid_stride != NULL) {
    *used_direct_grid_stride = use_direct_grid_stride;
  }
  if (use_direct_grid_stride) {
    lowerTargetLoopDirectGridStride(for_loop, bb1, info);
  } else {
    lowerTargetLoopRoundRobin(for_loop, bb1, info);
  }

  // handle private variables at this loop level, mostly loop index variables.
  // TODO: this is not very elegant since the outer most loop's loop variable is
  // still translated.
  return bb1;
}

// transformation for combined directive
// omp target parallel for
// omp target teams distribute parallel for
void transOmpTargetSpmdWorksharing(SgNode *node, SgExpression *omp_num_teams,
                                   SgExpression *omp_num_threads,
                                   bool has_explicit_num_teams,
                                   bool has_explicit_num_threads) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // device expression
  SgExpression *device_expression = NULL;
  device_expression =
      getClauseExpression(target, VariantVector(V_SgOmpDeviceClause));
  // If not found, use the default ID 0
  if (device_expression == NULL)
    device_expression = buildIntVal(0);

  // Now we need to ensure that "omp target " has a basic block as its body
  // so we can insert declarations into an inner block, instead of colliding
  // declarations within the scope of "omp target" This is important since we
  // often have consecutive "omp target" regions within one big scope We cannot
  // just insert things into that big scope.
  SgBasicBlock *omp_target_stmt_body_block =
      ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(isSgBasicBlock(target->get_body()));

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  Sg_File_Info *new_info = new Sg_File_Info(*(target->get_startOfConstruct()));
  Sg_File_Info *old_info = body->get_startOfConstruct();
  ROSE_ASSERT(old_info != NULL);
  body->set_startOfConstruct(new_info);
  new_info->set_parent(body);

  if (hasClause(target, V_SgOmpCollapseClause))
    transOmpCollapse(target);

  delete (new_info);
  body->set_startOfConstruct(old_info);
  old_info->set_parent(body);

  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  // 1/15/2009, Liao, also handle the last #endif, which is attached inside of
  // the target
  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  //-----------------------------------------------------------------
  // step 1: generated an outlined function and make it a CUDA function
  SgOmpClauseBodyStatement *target_parallel_stmt =
      isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target_parallel_stmt);
  (void)has_explicit_num_teams;

  // Prepare the outliner
  Outliner::enable_classic = true;
  // Outliner::useParameterWrapper = false; //TODO: better handling of the
  // dependence among flags
  SgBasicBlock *body_block = Outliner::preprocess(body);

  // The combined directive only has one code block and should only process omp
  // variables once
  transOmpVariablesWithContext(target, body_block, NULL, true, &offload_ctx);

  SgExpression *host_loop_iter_count_expr = NULL;
  int direct_launch_thread_cap = 0;
  {
    Rose_STL_Container<SgNode *> host_for_loops =
        NodeQuery::querySubTree(body_block, V_SgForStatement);
    if (!host_for_loops.empty()) {
      SgForStatement *host_for_loop = isSgForStatement(host_for_loops[0]);
      if (host_for_loop != NULL) {
        TargetLoopLoweringInfo host_loop_info;
        if (analyzeTargetLoopForGpuReadOnly(host_for_loop, &host_loop_info) &&
            canUseDirectTargetLoopFastPath(host_loop_info)) {
          host_loop_iter_count_expr =
              buildTargetLoopTripCountExpr(host_loop_info);
          if (expressionDependsOnVarsDeclaredInside(host_loop_iter_count_expr,
                                                    body_block)) {
            host_loop_iter_count_expr = NULL;
          }

          if (!has_explicit_num_threads) {
            const int nested_loop_depth =
                computeMaxNestedForDepth(host_for_loop->get_loop_body());
            if (nested_loop_depth >= 2) {
              direct_launch_thread_cap = 128;
            } else if (nested_loop_depth >= 1) {
              direct_launch_thread_cap = 256;
            }
          }
        }
      }
    }
  }

  ASTtools::VarSymSet_t all_syms; // all generated or remaining variables to be
                                  // passed to the outliner
  // This addressOf_syms does not apply to CUDA kernel generation: since we
  // cannot use pass-by-reference for CUDA kernel. If we want to copy back
  // value, we have to use memory copy  since they are in two different memory
  // spaces.
  ASTtools::VarSymSet_t
      addressOf_syms; // generated or remaining variables should be passed by
                      // using their addresses

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  all_syms = transOmpMapVariables(
      target, map_variable_list, map_variable_base_list, map_variable_size_list,
      map_variable_type_list, &offload_ctx,
      &dynamic_map_entries); //, addressOf_syms);
  /*
  for (std::set<const SgVariableSymbol*>::iterator iter = all_syms.begin(); iter
  != all_syms.end(); iter++) { std::cout << "SPMD worksharing variable: " <<
  (*iter)->get_name() << "...\n";
  };
  */

  ASTtools::VarSymSet_t
      per_block_reduction_syms; // translation generated per block reduction
                                // symbols with name like _dev_per_block within
                                // the enclosed for loop

  // collect possible per block reduction variables introduced by
  // transOmpTargetLoop() we rely on the pattern of such variables:
  // _dev_per_block_* these variables are arrays already, we pass them by their
  // original types, not addressOf types
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(body_block, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    SgName var_name = vRef->get_symbol()->get_name();
    string var_name_str = var_name.getString();
    if (var_name_str.find("__reduction_buffer_", 0) == 0) {
      all_syms.insert(vRef->get_symbol());
      per_block_reduction_syms.insert(vRef->get_symbol());
    }
  }

  string func_name = Outliner::generateFuncName(target);
  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  // pass all the parameters by reference
  for (std::set<const SgVariableSymbol *>::iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    if (!isPointerType((*iter)->get_type()) &&
        !isSgArrayType((*iter)->get_type()) &&
        offload_ctx.literal_target_param_syms.find(
            const_cast<SgVariableSymbol *>(*iter)) ==
            offload_ctx.literal_target_param_syms.end()) {
      addressOf_syms.insert(*iter);
    };
  };

  std::set<SgInitializedName *> restoreVars;
  SgFunctionDeclaration *result =
      Outliner::generateFunction(body_block, func_name + "kernel__", all_syms,
                                 addressOf_syms, restoreVars, NULL, g_scope);
  SgFunctionDeclaration *result_decl =
      isSgFunctionDeclaration(result->get_firstNondefiningDeclaration());
  ROSE_ASSERT(result_decl != NULL);
  lowerLiteralTargetKernelParameters(result,
                                     offload_ctx.literal_target_param_syms);
  maybeRecordTargetKernelLaunchBounds(result, omp_num_threads);
  result_decl->get_functionModifier()
      .setCudaKernel(); // add __global__ modifier

  result->get_functionModifier().setCudaKernel();

  // This one is not desired. It inserts the function to the end and prepend a
  // prototype Outliner::insert(result, g_scope, body_block);
  // TODO: better interface to specify where exactly to insert the function!
  // Custom insertion:  insert right before the enclosing function of "omp
  // target"
  SgFunctionDeclaration *target_func = const_cast<SgFunctionDeclaration *>(
      SageInterface::getEnclosingFunctionDeclaration(target));
  ROSE_ASSERT(target_func != NULL);
  insertStatementAfter(target_func, result);
  // TODO: this really should be done within Outliner::generateFunction()
  // TODO: we have to patch up first nondefining function declaration since
  // custom insertion is used
  SgGlobal *glob_scope = getGlobalScope(target);
  ROSE_ASSERT(glob_scope != NULL);
  SgFunctionSymbol *func_symbol =
      glob_scope->lookup_function_symbol(result->get_name());
  ROSE_ASSERT(func_symbol != NULL);

  SgScopeStatement *p_scope =
      omp_target_stmt_body_block; // the scope of "omp parallel" will be
                                  // destroyed later, so we use scope of "omp
                                  // target"
  ROSE_ASSERT(p_scope != NULL);

  // At this point, the for loop has been moved to the outlined function.
  // It's the very first loop statement in that function.
  Rose_STL_Container<SgNode *> for_loops =
      NodeQuery::querySubTree(result, V_SgForStatement);
  transOmpTargetLoopBlock(for_loops[0], NULL, &offload_ctx);

  // create the outlined driver for GPU offloading, which is empty at this point
  SgBasicBlock *outlined_driver_body = omp_target_stmt_body_block;

  // Use the OpenMP runtime's default device sentinel.
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", buildOpaqueType("int64_t", p_scope),
      buildAssignInitializer(buildLongLongIntVal(-1)), p_scope);
  outlined_driver_body->append_statement(device_id_decl);
  attachComment(device_id_decl, string("Launch CUDA kernel ..."));

  SgVariableDeclaration *threads_per_block_decl = NULL;
  SgVariableDeclaration *num_blocks_decl = NULL;
  SgVariableDeclaration *tripcount_decl = NULL;
  // insert dim3 threadsPerBlock(xomp_get_maxThreadsPerBlock());
  // TODO: for 1-D mapping, int type is enough.
  threads_per_block_decl = buildVariableDeclaration(
      "_threads_per_block_", buildIntType(),
      buildAssignInitializer(omp_num_threads), p_scope);
  outlined_driver_body->append_statement(threads_per_block_decl);

  // dim3 numBlocks (xomp_get_max1DBlock(VEC_LEN));
  num_blocks_decl =
      buildVariableDeclaration("_num_blocks_", buildIntType(),
                               buildAssignInitializer(omp_num_teams), p_scope);
  outlined_driver_body->append_statement(num_blocks_decl);

  if (host_loop_iter_count_expr != NULL) {
    tripcount_decl = buildVariableDeclaration(
        "__rex_tripcount", buildOpaqueType("int64_t", p_scope),
        buildAssignInitializer(copyExpression(host_loop_iter_count_expr)),
        p_scope);
    outlined_driver_body->append_statement(tripcount_decl);

    if (!has_explicit_num_threads) {
      SgBasicBlock *cap_launch_body = buildBasicBlock();

      SgBasicBlock *cap_threads_body = buildBasicBlock();
      SgVariableDeclaration *launch_granularity_decl = buildVariableDeclaration(
          "__rex_launch_granularity", buildOpaqueType("int64_t", p_scope),
          buildAssignInitializer(buildLongLongIntVal(32)), cap_threads_body);
      cap_threads_body->append_statement(launch_granularity_decl);

      SgBasicBlock *use_block_granularity_body = buildBasicBlock();
      use_block_granularity_body->append_statement(buildAssignStatement(
          buildVarRefExp(launch_granularity_decl),
          buildCastExp(buildVarRefExp(threads_per_block_decl),
                       buildOpaqueType("int64_t", p_scope))));
      cap_threads_body->append_statement(buildIfStmt(
          buildLessThanOp(buildCastExp(buildVarRefExp(threads_per_block_decl),
                                       buildOpaqueType("int64_t", p_scope)),
                          buildLongLongIntVal(32)),
          use_block_granularity_body, NULL));

      SgExpression *rounded_threads_expr = buildMultiplyOp(
          buildDivideOp(buildSubtractOp(
                            buildAddOp(buildVarRefExp(tripcount_decl),
                                       buildVarRefExp(launch_granularity_decl)),
                            buildLongLongIntVal(1)),
                        buildVarRefExp(launch_granularity_decl)),
          buildVarRefExp(launch_granularity_decl));
      SgVariableDeclaration *rounded_threads_decl = buildVariableDeclaration(
          "__rex_rounded_threads", buildOpaqueType("int64_t", p_scope),
          buildAssignInitializer(rounded_threads_expr), cap_threads_body);
      cap_threads_body->append_statement(rounded_threads_decl);

      SgBasicBlock *clamp_threads_body = buildBasicBlock();
      clamp_threads_body->append_statement(buildAssignStatement(
          buildVarRefExp(rounded_threads_decl),
          buildCastExp(buildVarRefExp(threads_per_block_decl),
                       buildOpaqueType("int64_t", p_scope))));
      cap_threads_body->append_statement(
          buildIfStmt(buildGreaterThanOp(
                          buildVarRefExp(rounded_threads_decl),
                          buildCastExp(buildVarRefExp(threads_per_block_decl),
                                       buildOpaqueType("int64_t", p_scope))),
                      clamp_threads_body, NULL));

      cap_threads_body->append_statement(buildAssignStatement(
          buildVarRefExp(threads_per_block_decl),
          buildCastExp(buildVarRefExp(rounded_threads_decl), buildIntType())));
      cap_launch_body->append_statement(
          buildIfStmt(buildGreaterThanOp(
                          buildCastExp(buildVarRefExp(threads_per_block_decl),
                                       buildOpaqueType("int64_t", p_scope)),
                          buildVarRefExp(tripcount_decl)),
                      cap_threads_body, NULL));

      outlined_driver_body->append_statement(
          buildIfStmt(buildGreaterThanOp(buildVarRefExp(tripcount_decl),
                                         buildLongLongIntVal(0)),
                      cap_launch_body, NULL));
    }
  }

  if (!has_explicit_num_threads && direct_launch_thread_cap > 0) {
    SgBasicBlock *cap_direct_threads_body = buildBasicBlock();
    cap_direct_threads_body->append_statement(
        buildAssignStatement(buildVarRefExp(threads_per_block_decl),
                             buildIntVal(direct_launch_thread_cap)));
    outlined_driver_body->append_statement(
        buildIfStmt(buildGreaterThanOp(buildVarRefExp(threads_per_block_decl),
                                       buildIntVal(direct_launch_thread_cap)),
                    cap_direct_threads_body, NULL));
  }

  // Now we have num_block declaration, we can insert the per block declaration
  // used for reduction variables
  SgExpression *shared_data = NULL; // shared data size expression for CUDA
                                    // kernel execution configuration
  SgExprListExp *map_variable_list_suffix = buildExprListExp();
  SgExprListExp *map_variable_base_list_suffix = buildExprListExp();
  SgExprListExp *map_variable_size_list_suffix = buildExprListExp();
  SgExprListExp *map_variable_type_list_suffix = buildExprListExp();
  for (std::vector<SgVariableDeclaration *>::iterator iter =
           offload_ctx.per_block_declarations.begin();
       iter != offload_ctx.per_block_declarations.end(); iter++) {
    SgVariableDeclaration *decl = *iter;
    insertStatementAfter(num_blocks_decl, decl);
    SgVariableSymbol *sym = getFirstVarSym(decl);
    SgPointerType *pointer_type = isSgPointerType(sym->get_type());
    ROSE_ASSERT(pointer_type != NULL);
    SgType *base_type = pointer_type->get_base_type();
    if (offload_ctx.per_block_declarations.size() > 1) {
      cerr << "Error. multiple reduction variables are not yet handled."
           << endl;
      ROSE_ABORT();
      // threadsPerBlock.x*sizeof(REAL)  //TODO: how to handle multiple shared
      // data blocks, each for a reduction variable??
    }
    shared_data = buildMultiplyOp(buildVarRefExp(threads_per_block_decl),
                                  buildSizeOfOp(base_type));

    // insert reduction buffer array to variable mapping list
    string reduction_buffer_name = (sym->get_name()).getString();
    SgExprListExp *reduction_map_variable_list = dynamic_map_entries.empty()
                                                     ? map_variable_list
                                                     : map_variable_list_suffix;
    SgExprListExp *reduction_map_variable_base_list =
        dynamic_map_entries.empty() ? map_variable_base_list
                                    : map_variable_base_list_suffix;
    SgExprListExp *reduction_map_variable_size_list =
        dynamic_map_entries.empty() ? map_variable_size_list
                                    : map_variable_size_list_suffix;
    SgExprListExp *reduction_map_variable_type_list =
        dynamic_map_entries.empty() ? map_variable_type_list
                                    : map_variable_type_list_suffix;
    reduction_map_variable_list->append_expression(
        buildVarRefExp(reduction_buffer_name, p_scope));
    reduction_map_variable_base_list->append_expression(
        buildVarRefExp(reduction_buffer_name, p_scope));
    SgExpression *reduction_variable_size =
        buildCastExp(buildMultiplyOp(buildVarRefExp(num_blocks_decl),
                                     buildSizeOfOp(base_type)),
                     buildOpaqueType("int64_t", p_scope));
    reduction_map_variable_size_list->append_expression(
        reduction_variable_size);
    SgExpression *reduction_variable_value =
        buildIntVal(OMP_TGT_MAPTYPE_TARGET_PARAM | OMP_TGT_MAPTYPE_FROM);
    reduction_map_variable_type_list->append_expression(
        reduction_variable_value);
  }

  // generate the cuda kernel launch statement
  // e.g.  axpy_ompacc_cuda <<<numBlocks, threadsPerBlock>>>(dev_x,  dev_y,
  // VEC_LEN, a);

  // func_symbol =
  // isSgFunctionSymbol(result->get_firstNondefiningDeclaration()->get_symbol_from_symbol_table
  // ());
  ROSE_ASSERT(func_symbol != NULL);
  SgExprListExp *exp_list_exp = SageBuilder::buildExprListExp();

  std::set<SgInitializedName *> varsUsingOriginalForm;
  for (ASTtools::VarSymSet_t::const_iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    const SgVariableSymbol *current_symbol = *iter;
    // this addressOf_syms does not apply to CUDA kernel generation: since we
    // cannot use pass-by-reference for CUDA kernel. If we want to copy back
    // value, we have to use memory copy  since they are in two different memory
    // spaces. So all variables should use original form in this context.
    if (addressOf_syms.find(current_symbol) ==
        addressOf_syms.end()) // not found in Address Of variable set
      varsUsingOriginalForm.insert(current_symbol->get_declaration());
  }
  // TODO: alternative mirror form using varUsingAddress as parameter
  Outliner::appendIndividualFunctionCallArgs(all_syms, varsUsingOriginalForm,
                                             exp_list_exp);
  // TODO: builder interface without _nfi, and match function call exp builder
  // interface convention:

  // in the original function, we call the outlined driver and pass all the
  // required variables by reference prepare all the parameters for using LLVM
  // GPU offloading
  SgClassDeclaration *tgt_offload_entry =
      buildStructDeclaration("__tgt_offload_entry", getGlobalScope(target));

  kmpc_kernel_id_counter += 1;
  SgVariableDeclaration *outlined_kernel_id_decl =
      buildVariableDeclaration(func_name + "id__", buildCharType(),
                               buildAssignInitializer(buildIntVal(0)), g_scope);

  // define the entry point
  SgExprListExp *offload_entry_parameters = buildExprListExp(
      buildCastExp(buildAddressOfOp(buildVarRefExp(outlined_kernel_id_decl)),
                   buildPointerType(buildVoidType())),
      buildStringVal(func_name + "kernel__"), buildIntVal(0), buildIntVal(0),
      buildIntVal(0));
  SgBracedInitializer *offload_entry_initilization =
      buildBracedInitializer(offload_entry_parameters);
  SgVariableDeclaration *offload_entry_decl = buildVariableDeclaration(
      func_name + "omp_offload_entry__", tgt_offload_entry->get_type(),
      buildAssignInitializer(offload_entry_initilization), g_scope);
  offload_entry_decl->get_decl_item(SgName(func_name + "omp_offload_entry__"))
      ->set_gnu_attribute_section_name("omp_offloading_entries");

  prependGlobalDeclPreservingLeadingPreproc(offload_entry_decl, g_scope);
  prependGlobalDeclPreservingLeadingPreproc(outlined_kernel_id_decl, g_scope);

  SgVariableDeclaration *host_point_decl = buildVariableDeclaration(
      "__host_ptr", buildPointerType(buildVoidType()),
      buildAssignInitializer(buildCastExp(
          buildAddressOfOp(buildVarRefExp(outlined_kernel_id_decl)),
          buildPointerType(buildVoidType()))),
      p_scope);
  outlined_driver_body->append_statement(host_point_decl);

  SgVariableDeclaration *args_base_decl = nullptr;
  SgVariableDeclaration *args_decl = nullptr;
  SgVariableDeclaration *arg_sizes = nullptr;
  SgVariableDeclaration *arg_types = nullptr;
  SgVariableDeclaration *arg_number_decl = nullptr;
  RuntimeMapArgumentArrayDeclarations dynamic_arrays;
  if (!dynamic_map_entries.empty()) {
    dynamic_arrays = buildDynamicRuntimeMapArgumentArrays(
        outlined_driver_body, p_scope, map_variable_list,
        map_variable_base_list, map_variable_size_list, map_variable_type_list,
        dynamic_map_entries, map_variable_list_suffix,
        map_variable_base_list_suffix, map_variable_size_list_suffix,
        map_variable_type_list_suffix);
    args_base_decl = dynamic_arrays.args_base_decl;
    args_decl = dynamic_arrays.args_decl;
    arg_sizes = dynamic_arrays.arg_sizes_decl;
    arg_types = dynamic_arrays.arg_types_decl;
    arg_number_decl = dynamic_arrays.arg_number_decl;
  } else {
    materializeLiteralTargetArgExpressions(map_variable_list,
                                           map_variable_base_list,
                                           outlined_driver_body, p_scope);

    SgBracedInitializer *offloading_variables_base =
        buildBracedInitializer(map_variable_base_list);
    args_base_decl = buildVariableDeclaration(
        "__args_base", buildArrayType(buildPointerType(buildVoidType())),
        buildAssignInitializer(offloading_variables_base), p_scope);
    outlined_driver_body->append_statement(args_base_decl);

    SgBracedInitializer *offloading_variables =
        buildBracedInitializer(map_variable_list);
    args_decl = buildVariableDeclaration(
        "__args", buildArrayType(buildPointerType(buildVoidType())),
        buildAssignInitializer(offloading_variables), p_scope);
    outlined_driver_body->append_statement(args_decl);

    SgBracedInitializer *map_variable_sizes =
        buildBracedInitializer(map_variable_size_list);
    arg_sizes = buildVariableDeclaration(
        "__arg_sizes", buildArrayType(buildOpaqueType("int64_t", p_scope)),
        buildAssignInitializer(map_variable_sizes), p_scope);
    outlined_driver_body->append_statement(arg_sizes);

    SgBracedInitializer *map_variable_types =
        buildBracedInitializer(map_variable_type_list);
    arg_types = buildVariableDeclaration(
        "__arg_types", buildArrayType(buildOpaqueType("int64_t", p_scope)),
        buildAssignInitializer(map_variable_types), p_scope);
    outlined_driver_body->append_statement(arg_types);

    int kernel_arg_num = map_variable_base_list->get_expressions().size();
    arg_number_decl = buildVariableDeclaration(
        "__arg_num", buildOpaqueType("int32_t", p_scope),
        buildAssignInitializer(buildIntVal(kernel_arg_num)), p_scope);
    outlined_driver_body->append_statement(arg_number_decl);
  }

  SgVariableDeclaration *kernel_args_decl = buildTargetKernelArgsDeclaration(
      g_scope, p_scope, arg_number_decl, args_base_decl, args_decl, arg_sizes,
      arg_types, num_blocks_decl, threads_per_block_decl,
      tripcount_decl != NULL ? buildVarRefExp(tripcount_decl) : NULL);
  outlined_driver_body->append_statement(kernel_args_decl);

  // call __tgt_target_kernel to execute the CUDA kernel
  SgVariableSymbol *kernel_args_sym = getFirstVarSym(kernel_args_decl);
  ROSE_ASSERT(kernel_args_sym != NULL);
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(num_blocks_decl),
      buildVarRefExp(threads_per_block_decl), buildVarRefExp(host_point_decl),
      buildAddressOfOp(buildVarRefExp(kernel_args_sym)));
  string func_offloading_name = "__tgt_target_kernel";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildIntType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  outlined_driver_body->append_statement(func_offloading_stmt);

  appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays,
                                              outlined_driver_body, p_scope);

  for (ASTtools::VarSymSet_t::const_iterator iter =
           per_block_reduction_syms.begin();
       iter != per_block_reduction_syms.end(); iter++) {
    const SgVariableSymbol *current_symbol = *iter;
    SgPointerType *pointer_type = isSgPointerType(
        current_symbol->get_type()); // must be a pointer to simple type
    ROSE_ASSERT(pointer_type != NULL);
    SgType *orig_type = pointer_type->get_base_type();
    ROSE_ASSERT(orig_type != NULL);

    string per_block_var_name = (current_symbol->get_name()).getString();
    // get the original var name by stripping of the leading "_dev_per_block_"
    string leading_pattern = string("__reduction_buffer_");
    string orig_var_name = per_block_var_name.substr(
        leading_pattern.length(),
        per_block_var_name.length() - leading_pattern.length());
    //      cout<<"debug: "<<per_block_var_name <<" after "<< orig_var_name
    //      <<endl;
    SgExprListExp *parameter_list = buildExprListExp(
        buildVarRefExp(const_cast<SgVariableSymbol *>(current_symbol)),
        buildVarRefExp("_num_blocks_", target->get_scope()),
        buildIntVal(
            offload_ctx.per_block_reduction_map[const_cast<SgVariableSymbol *>(
                current_symbol)]));
    SgStatement *reduce_on_cpu_stmt = generateTargetReduceOnCPU(
        orig_var_name, const_cast<SgVariableSymbol *>(current_symbol),
        num_blocks_decl,
        offload_ctx.per_block_reduction_map[const_cast<SgVariableSymbol *>(
            current_symbol)]);
    outlined_driver_body->append_statement(reduce_on_cpu_stmt);

    // insert memory free for the _dev_per_block_variables
    // TODO: need runtime support to automatically free memory
    SgFunctionCallExp *func_call_exp2 = buildFunctionCallExp(
        "free", buildVoidType(),
        buildExprListExp(
            buildVarRefExp(const_cast<SgVariableSymbol *>(current_symbol))),
        omp_target_stmt_body_block);
    outlined_driver_body->append_statement(buildExprStatement(func_call_exp2));
  }

  // num_blocks is referenced before the declaration is inserted. So we must fix
  // it, otherwise the symbol of unkown type will be cleaned up later.
  SageInterface::fixVariableReferences(num_blocks_decl->get_scope());

  //------------now remove omp parallel since everything within it has been
  // outlined to a function
  replaceStatement(target, outlined_driver_body, true);

  target_outlined_function_list->push_back(isSgFunctionDeclaration(result));
}

void transOmpLoopInTargetRegion(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // At this point, the for loop has been moved to the outlined function.
  // It's the very first loop statement in that function.
  Rose_STL_Container<SgNode *> for_loops =
      NodeQuery::querySubTree(node, V_SgForStatement);
  SgBasicBlock *loop_block =
      transOmpTargetLoopBlock(for_loops[0], NULL, &offload_ctx);

  replaceStatement(target, loop_block, true);
}

// FIXME: It's still work-in-progress.
void transOmpSpmdInTargetRegion(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // Now we need to ensure that "omp target " has a basic block as its body
  // so we can insert declarations into an inner block, instead of colliding
  // declarations within the scope of "omp target" This is important since we
  // often have consecutive "omp target" regions within one big scope We cannot
  // just insert things into that big scope.
  SgBasicBlock *omp_target_stmt_body_block =
      ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(isSgBasicBlock(target->get_body()));

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  // 1/15/2009, Liao, also handle the last #endif, which is attached inside of
  // the target
  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  //-----------------------------------------------------------------
  // step 1: generated an outlined function and make it a CUDA function
  SgOmpClauseBodyStatement *target_parallel_stmt =
      isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target_parallel_stmt);

  // Prepare the outliner
  Outliner::enable_classic = true;
  //    Outliner::useParameterWrapper = false; //TODO: better handling of the
  //    dependence among flags
  SgBasicBlock *body_block = Outliner::preprocess(body);
  // translator OpenMP 3.0 and earlier variables.
  transOmpVariables(target, body_block);

  ASTtools::VarSymSet_t all_syms; // all generated or remaining variables to be
                                  // passed to the outliner
  // This addressOf_syms does not apply to CUDA kernel generation: since we
  // cannot use pass-by-reference for CUDA kernel. If we want to copy back
  // value, we have to use memory copy  since they are in two different memory
  // spaces.
  ASTtools::VarSymSet_t
      addressOf_syms; // generated or remaining variables should be passed by
                      // using their addresses

  SageInterface::fixVariableReferences(body_block);
  Outliner::collectVars(body_block, all_syms);
  ASTtools::VarSymSet_t::iterator iter;
  for (iter = all_syms.begin(); iter != all_syms.end(); iter++) {
    const SgVariableSymbol *var_sym = *iter;
    MLOG_DEBUG_CXX("ompLowering")
        << "candidate outlined symbol: " << var_sym->get_name();
    SgType *i_type = var_sym->get_declaration()->get_type();
    if (!isSgPointerType(i_type) && !isSgArrayType(i_type))
      addressOf_syms.insert(var_sym);
  }

  // if num_threads clause exists, we need to set up the omp number of threads
  // first. therefore, the head will be the function call of setting up
  // num_threads.
  SgExpression *omp_num_threads = NULL;
  if (hasClause(target, V_SgOmpNumThreadsClause)) {
    Rose_STL_Container<SgOmpClause *> num_threads_clauses =
        getClause(target, V_SgOmpNumThreadsClause);
    ROSE_ASSERT(num_threads_clauses.size() ==
                1); // should only have one num_threads()
    SgOmpNumThreadsClause *num_threads_clause =
        isSgOmpNumThreadsClause(num_threads_clauses[0]);
    ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
    omp_num_threads = copyExpression(num_threads_clause->get_expression());
  }

  string func_name = Outliner::generateFuncName(target);
  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  // pass all the parameters by reference
  for (std::set<const SgVariableSymbol *>::iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    if (!isPointerType((*iter)->get_type()) &&
        !isSgArrayType((*iter)->get_type()) &&
        offload_ctx.literal_target_param_syms.find(
            const_cast<SgVariableSymbol *>(*iter)) ==
            offload_ctx.literal_target_param_syms.end()) {
      addressOf_syms.insert(*iter);
    };
  };

  std::set<SgInitializedName *> restoreVars;
  SgFunctionDeclaration *result =
      Outliner::generateFunction(body_block, func_name + "kernel__", all_syms,
                                 addressOf_syms, restoreVars, NULL, g_scope);
  SgFunctionDeclaration *result_decl =
      isSgFunctionDeclaration(result->get_firstNondefiningDeclaration());
  ROSE_ASSERT(result_decl != NULL);
  maybeRecordTargetKernelLaunchBounds(result, omp_num_threads);
  result_decl->get_functionModifier()
      .setCudaKernel(); // add __global__ modifier

  result->get_functionModifier().setCudaKernel();

  // This one is not desired. It inserts the function to the end and prepend a
  // prototype Outliner::insert(result, g_scope, body_block);
  // TODO: better interface to specify where exactly to insert the function!
  // Custom insertion:  insert right before the enclosing function of "omp
  // target"
  SgFunctionDeclaration *target_func = const_cast<SgFunctionDeclaration *>(
      SageInterface::getEnclosingFunctionDeclaration(target));
  ROSE_ASSERT(target_func != NULL);
  insertStatementAfter(target_func, result);
  // TODO: this really should be done within Outliner::generateFunction()
  // TODO: we have to patch up first nondefining function declaration since
  // custom insertion is used
  SgGlobal *glob_scope = getGlobalScope(target);
  ROSE_ASSERT(glob_scope != NULL);
  SgFunctionSymbol *func_symbol =
      glob_scope->lookup_function_symbol(result->get_name());
  ROSE_ASSERT(func_symbol != NULL);

  SgScopeStatement *p_scope =
      target->get_scope(); // the scope of "omp parallel" will be destroyed
                           // later, so we use scope of "omp target"
  ROSE_ASSERT(p_scope != NULL);

  // Generate the parameter list for the call to the XOMP runtime function
  SgExprListExp *parameters = buildExprListExp();
  for (iter = all_syms.begin(); iter != all_syms.end(); iter++) {
    const SgVariableSymbol *var_sym = *iter;
    SgVarRefExp *var_ref =
        buildVarRefExp(const_cast<SgVariableSymbol *>(var_sym));
    SgType *i_type = var_sym->get_declaration()->get_type();
    if (!isSgPointerType(i_type) && !isSgArrayType(i_type))
      appendExpression(parameters, buildAddressOfOp(var_ref));
    else
      appendExpression(parameters, var_ref);
  }
  // create the outlined driver for GPU offloading, which is empty at this point
  SgBasicBlock *outlined_driver_body = buildBasicBlock();

  SgCudaKernelExecConfig *cuda_kernel_config =
      buildCudaKernelExecConfig_nfi(buildIntVal(1), omp_num_threads);
  SgCudaKernelCallExp *cuda_kernel_call_expression = buildCudaKernelCallExp_nfi(
      buildFunctionRefExp(result), parameters, cuda_kernel_config);
  SgStatement *outlined_function_call =
      buildExprStatement(cuda_kernel_call_expression);

  setSourcePositionForTransformation(outlined_function_call);
  outlined_driver_body->append_statement(outlined_function_call);

  SageInterface::fixStatement(outlined_driver_body, p_scope);
  //------------now remove omp parallel since everything within it has been
  // outlined to a function
  replaceStatement(target, outlined_driver_body, true);

  target_outlined_function_list->push_back(isSgFunctionDeclaration(result));
}

// transformation for combined directive omp target teams
void transOmpTargetTeams(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetTeamsStatement *target = isSgOmpTargetTeamsStatement(node);
  ROSE_ASSERT(target != NULL);

  Rose_STL_Container<SgOmpClause *> num_teams_clauses =
      getClause(target, V_SgOmpNumTeamsClause);
  ROSE_ASSERT(num_teams_clauses.size() ==
              1); // should only have one num_teams()
  SgOmpNumTeamsClause *num_teams_clause =
      isSgOmpNumTeamsClause(num_teams_clauses[0]);
  ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  SgExpression *omp_num_teams =
      copyExpression(num_teams_clause->get_expression());

  SgExpression *omp_num_threads = buildIntVal(1);

  transOmpTargetSpmd(target, omp_num_teams, omp_num_threads);
}

// transformation for combined directive omp target parallel
void transOmpTargetParallel(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetParallelStatement *target = isSgOmpTargetParallelStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *omp_num_teams = buildIntVal(1);

  Rose_STL_Container<SgOmpClause *> num_threads_clauses =
      getClause(target, V_SgOmpNumThreadsClause);
  ROSE_ASSERT(num_threads_clauses.size() ==
              1); // should only have one num_threads()
  SgOmpNumThreadsClause *num_threads_clause =
      isSgOmpNumThreadsClause(num_threads_clauses[0]);
  ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  SgExpression *omp_num_threads =
      copyExpression(num_threads_clause->get_expression());

  transOmpTargetSpmd(target, omp_num_teams, omp_num_threads);
}

// transformation for omp target
void transOmpTarget(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetStatement *target = isSgOmpTargetStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *omp_num_teams = buildIntVal(1);
  SgExpression *omp_num_threads = buildIntVal(1);

  transOmpTargetSpmd(target, omp_num_teams, omp_num_threads);
}

// transformation for combined directive omp target teams distribute
void transOmpTargetTeamsDistribute(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetTeamsDistributeStatement *target =
      isSgOmpTargetTeamsDistributeStatement(node);
  ROSE_ASSERT(target != NULL);

  Rose_STL_Container<SgOmpClause *> num_teams_clauses =
      getClause(target, V_SgOmpNumTeamsClause);
  ROSE_ASSERT(num_teams_clauses.size() ==
              1); // should only have one num_teams()
  SgOmpNumTeamsClause *num_teams_clause =
      isSgOmpNumTeamsClause(num_teams_clauses[0]);
  ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  SgExpression *omp_num_teams =
      copyExpression(num_teams_clause->get_expression());

  SgExpression *omp_num_threads = buildIntVal(1);

  transOmpTargetSpmdWorksharing(target, omp_num_teams, omp_num_threads,
                                /*has_explicit_num_teams=*/true,
                                /*has_explicit_num_threads=*/false);
}

// transformation for combined directive omp target parallel for
void transOmpTargetParallelFor(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetParallelForStatement *target =
      isSgOmpTargetParallelForStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *omp_num_teams = buildIntVal(1);

  Rose_STL_Container<SgOmpClause *> num_threads_clauses =
      getClause(target, V_SgOmpNumThreadsClause);
  ROSE_ASSERT(num_threads_clauses.size() ==
              1); // should only have one num_threads()
  SgOmpNumThreadsClause *num_threads_clause =
      isSgOmpNumThreadsClause(num_threads_clauses[0]);
  ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  SgExpression *omp_num_threads =
      copyExpression(num_threads_clause->get_expression());

  transOmpTargetSpmdWorksharing(target, omp_num_teams, omp_num_threads,
                                /*has_explicit_num_teams=*/false,
                                /*has_explicit_num_threads=*/true);
}

// transformation for combined directive omp target teams distribute parallel
// for
void transOmpTargetTeamsDistributeParallelFor(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetTeamsDistributeParallelForStatement *target =
      isSgOmpTargetTeamsDistributeParallelForStatement(node);
  ROSE_ASSERT(target != NULL);

  Rose_STL_Container<SgOmpClause *> num_teams_clauses =
      getClause(target, V_SgOmpNumTeamsClause);
  ROSE_ASSERT(num_teams_clauses.size() ==
              1); // should only have one num_teams()
  SgOmpNumTeamsClause *num_teams_clause =
      isSgOmpNumTeamsClause(num_teams_clauses[0]);
  ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  SgExpression *omp_num_teams =
      copyExpression(num_teams_clause->get_expression());

  Rose_STL_Container<SgOmpClause *> num_threads_clauses =
      getClause(target, V_SgOmpNumThreadsClause);
  ROSE_ASSERT(num_threads_clauses.size() ==
              1); // should only have one num_threads()
  SgOmpNumThreadsClause *num_threads_clause =
      isSgOmpNumThreadsClause(num_threads_clauses[0]);
  ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  SgExpression *omp_num_threads =
      copyExpression(num_threads_clause->get_expression());

  transOmpTargetSpmdWorksharing(target, omp_num_teams, omp_num_threads,
                                /*has_explicit_num_teams=*/true,
                                /*has_explicit_num_threads=*/true);
}

/*
 * Expected AST layout:
 *  SgOmpSectionsStatement
 *    SgBasicBlock
 *      SgOmpSectionStatement (1 or more section statements here)
 *        SgBasicBlock
 *          SgStatement
 *
 * Lowering strategy:
 *   Translate sections as a static-scheduled iteration space [0, N-1] using
 *   __kmpc_for_static_init_4/__kmpc_for_static_fini so host lowering uses the
 *   LLVM OpenMP runtime for both C/C++ and Fortran.
 * */
void transOmpSections(SgNode *node) {
  //    cout<<"Entering transOmpSections() ..."<<endl;
  ROSE_ASSERT(node != NULL);
  // verify the AST is expected
  SgOmpSectionsStatement *target = isSgOmpSectionsStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgBasicBlock *bb1 = buildBasicBlock();

  SgBasicBlock *sections_block = isSgBasicBlock(body);
  ROSE_ASSERT(sections_block != NULL);
  // verify each statement under sections is SgOmpSectionStatement
  SgStatementPtrList section_list = sections_block->get_statements();
  int section_count = static_cast<int>(section_list.size());
  for (int i = 0; i < section_count; i++) {
    SgStatement *stmt = section_list[i];
    ROSE_ASSERT(isSgOmpSectionStatement(stmt));
  }

  std::string sec_var_name;
  if (SageInterface::is_Fortran_language())
    sec_var_name = "_section_";
  else
    sec_var_name = "xomp_section_";

  sec_var_name += StringUtility::numberToString(++gensym_counter);
  const int sec_max_value = section_count - 1;
  std::string sec_lower_name = sec_var_name + "_lower";
  std::string sec_upper_name = sec_var_name + "_upper";
  std::string sec_stride_name = sec_var_name + "_stride";
  std::string sec_last_iter_name = sec_var_name + "_last_iter";

  replaceStatement(target, bb1, true);

  SgScopeStatement *kmpc_tid_decl_scope = scope;
  if (!SageInterface::is_Fortran_language())
    kmpc_tid_decl_scope = bb1;

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(node, kmpc_tid_decl_scope, &kmpc_global_tid_init);
  SgExpression *thread_global_tid =
      buildVarRefExp(getFirstVariable(*kmpc_global_tid_declaration).get_name(),
                     kmpc_tid_decl_scope);
  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    appendStatement(kmpc_global_tid_declaration, bb1);
  }
  if (kmpc_global_tid_init != NULL)
    appendStatement(kmpc_global_tid_init, bb1);

  // Declare a variable to store the current section id
  // Only used to support lastprivate
  SgVariableDeclaration *sec_var_decl_save = NULL;
  if (hasClause(target, V_SgOmpLastprivateClause)) {
    sec_var_decl_save = buildVariableDeclaration(sec_var_name + "_save",
                                                 buildIntType(), NULL, bb1);
    if (SageInterface::is_Fortran_language())
      insert_fortran_declaration_into_procedure(sec_var_decl_save, scope);
    else
      appendStatement(sec_var_decl_save, bb1);
  }

  SgVariableDeclaration *sec_var_decl =
      buildVariableDeclaration(sec_var_name, buildIntType(), NULL, bb1);
  SgVariableDeclaration *sec_lower_decl =
      buildVariableDeclaration(sec_lower_name, buildIntType(), NULL, bb1);
  SgVariableDeclaration *sec_upper_decl =
      buildVariableDeclaration(sec_upper_name, buildIntType(), NULL, bb1);
  SgVariableDeclaration *sec_stride_decl =
      buildVariableDeclaration(sec_stride_name, buildIntType(), NULL, bb1);
  SgVariableDeclaration *sec_last_iter_decl =
      buildVariableDeclaration(sec_last_iter_name, buildIntType(), NULL, bb1);

  if (SageInterface::is_Fortran_language())
    insert_fortran_declaration_into_procedure(sec_var_decl, scope);
  else
    appendStatement(sec_var_decl, bb1);

  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(sec_lower_decl, scope);
    insert_fortran_declaration_into_procedure(sec_upper_decl, scope);
    insert_fortran_declaration_into_procedure(sec_stride_decl, scope);
    insert_fortran_declaration_into_procedure(sec_last_iter_decl, scope);
  } else {
    appendStatement(sec_lower_decl, bb1);
    appendStatement(sec_upper_decl, bb1);
    appendStatement(sec_stride_decl, bb1);
    appendStatement(sec_last_iter_decl, bb1);
  }

  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_lower_decl), buildIntVal(0)),
      bb1);
  appendStatement(buildAssignStatement(buildVarRefExp(sec_upper_decl),
                                       buildIntVal(sec_max_value)),
                  bb1);
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_stride_decl), buildIntVal(1)),
      bb1);
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_last_iter_decl), buildIntVal(0)),
      bb1);

  SgExpression *e_last_iter =
      buildAddressOfOp(buildVarRefExp(sec_last_iter_decl));
  SgExpression *e_lower = buildAddressOfOp(buildVarRefExp(sec_lower_decl));
  SgExpression *e_upper = buildAddressOfOp(buildVarRefExp(sec_upper_decl));
  SgExpression *e_stride = buildAddressOfOp(buildVarRefExp(sec_stride_decl));
  if (SageInterface::is_Fortran_language()) {
    // Fortran scalar arguments are pass-by-reference.
    e_last_iter = buildVarRefExp(sec_last_iter_decl);
    e_lower = buildVarRefExp(sec_lower_decl);
    e_upper = buildVarRefExp(sec_upper_decl);
    e_stride = buildVarRefExp(sec_stride_decl);
  }
  SgExprListExp *init_parameters = buildExprListExp(
      buildIntVal(0), copyExpression(thread_global_tid),
      buildIntVal(kmp_sched_static_chunk), e_last_iter, e_lower, e_upper,
      e_stride, buildIntVal(1), buildIntVal(1));
  appendStatement(buildFunctionCallStmt("__kmpc_for_static_init_4",
                                        buildVoidType(), init_parameters,
                                        scope),
                  bb1);

  SgIfStmt *clamp_upper =
      buildIfStmt(buildGreaterThanOp(buildVarRefExp(sec_upper_decl),
                                     buildIntVal(sec_max_value)),
                  buildAssignStatement(buildVarRefExp(sec_upper_decl),
                                       buildIntVal(sec_max_value)),
                  NULL);
  appendStatement(clamp_upper, bb1);

  SgIfStmt *init_section_id = buildIfStmt(
      buildLessOrEqualOp(buildVarRefExp(sec_lower_decl),
                         buildVarRefExp(sec_upper_decl)),
      buildAssignStatement(buildVarRefExp(sec_var_decl),
                           buildVarRefExp(sec_lower_decl)),
      buildAssignStatement(buildVarRefExp(sec_var_decl), buildIntVal(-1)));
  appendStatement(init_section_id, bb1);

  // while (_section_1 >=0) {}
  SgWhileStmt *while_stmt = buildWhileStmt(
      buildGreaterOrEqualOp(buildVarRefExp(sec_var_decl), buildIntVal(0)),
      buildBasicBlock());
  if (SageInterface::is_Fortran_language()) {
    while_stmt->set_has_end_statement(true);
  }
  appendStatement(while_stmt, bb1);
  // switch () {}
  SgSwitchStatement *switch_stmt = buildSwitchStatement(
      buildExprStatement(buildVarRefExp(sec_var_decl)), buildBasicBlock());
  appendStatement(switch_stmt, isSgBasicBlock(while_stmt->get_body()));
  // case 0, case 1, ...
  for (int i = 0; i < section_count; i++) {
    SgCaseOptionStmt *option_stmt =
        buildCaseOptionStmt(buildIntVal(i), buildBasicBlock());
    // Move SgOmpSectionStatement's body to Case OptionStmt's body
    SgOmpSectionStatement *section_statement =
        isSgOmpSectionStatement(section_list[i]);
    // Sara Royuela (Nov 19th, 2012)
    // The section statement might not be a Basic Block if there is only one
    // statement and it is not wrapped with braces In that case, we build here
    // the Basic Block
    SgBasicBlock *src_bb = isSgBasicBlock(section_statement->get_body());
    if (src_bb == NULL) {
      src_bb = ensureBasicBlockAsBodyOfOmpBodyStmt(section_statement);
    }
    SgBasicBlock *target_bb = isSgBasicBlock(option_stmt->get_body());
    moveStatementsBetweenBlocks(src_bb, target_bb);
    appendStatement(buildBreakStmt(), target_bb);

    // cout<<"source BB
    // address:"<<isSgBasicBlock(isSgOmpSectionStatement(section_list[i])->get_body())<<endl;
    // Now we have to delete the source BB since its symbol table is moved into
    // the target BB.
    SgBasicBlock *fake_src_bb = buildBasicBlock();
    isSgOmpSectionStatement(section_list[i])->set_body(fake_src_bb);
    fake_src_bb->set_parent(section_list[i]);
    delete (src_bb);

    appendStatement(option_stmt, isSgBasicBlock(switch_stmt->get_body()));
  } // end case 0, 1, ...
  // default option:
  SgDefaultOptionStmt *default_stmt = buildDefaultOptionStmt(buildBasicBlock(
      buildFunctionCallStmt("abort", buildVoidType(), NULL, scope)));
  appendStatement(default_stmt, isSgBasicBlock(switch_stmt->get_body()));

  // save the current section id before checking for next available one
  // This is only useful to support lastprivate clause
  if (hasClause(target, V_SgOmpLastprivateClause)) {
    SgStatement *save_stmt = buildAssignStatement(
        buildVarRefExp(sec_var_decl_save), buildVarRefExp(sec_var_decl));
    appendStatement(save_stmt, isSgBasicBlock(while_stmt->get_body()));
  }

  SgBasicBlock *while_body = isSgBasicBlock(while_stmt->get_body());
  appendStatement(buildAssignStatement(
                      buildVarRefExp(sec_var_decl),
                      buildAddOp(buildVarRefExp(sec_var_decl), buildIntVal(1))),
                  while_body);

  SgBasicBlock *advance_block = buildBasicBlock();
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_lower_decl),
                           buildAddOp(buildVarRefExp(sec_lower_decl),
                                      buildVarRefExp(sec_stride_decl))),
      advance_block);
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_upper_decl),
                           buildAddOp(buildVarRefExp(sec_upper_decl),
                                      buildVarRefExp(sec_stride_decl))),
      advance_block);
  appendStatement(
      buildIfStmt(buildGreaterThanOp(buildVarRefExp(sec_upper_decl),
                                     buildIntVal(sec_max_value)),
                  buildAssignStatement(buildVarRefExp(sec_upper_decl),
                                       buildIntVal(sec_max_value)),
                  NULL),
      advance_block);
  appendStatement(
      buildIfStmt(
          buildLessOrEqualOp(buildVarRefExp(sec_lower_decl),
                             buildVarRefExp(sec_upper_decl)),
          buildAssignStatement(buildVarRefExp(sec_var_decl),
                               buildVarRefExp(sec_lower_decl)),
          buildAssignStatement(buildVarRefExp(sec_var_decl), buildIntVal(-1))),
      advance_block);
  appendStatement(
      buildIfStmt(buildGreaterThanOp(buildVarRefExp(sec_var_decl),
                                     buildVarRefExp(sec_upper_decl)),
                  advance_block, NULL),
      while_body);

  transOmpVariables(
      target, bb1,
      buildIntVal(section_count -
                  1)); // This should happen before the barrier is inserted.

  SgExprListExp *fini_parameters =
      buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
  appendStatement(buildFunctionCallStmt("__kmpc_for_static_fini",
                                        buildVoidType(), fini_parameters,
                                        scope),
                  bb1);

  if (!hasClause(target, V_SgOmpNowaitClause)) {
    SgExprListExp *barrier_parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    appendStatement(buildFunctionCallStmt("__kmpc_barrier", buildVoidType(),
                                          barrier_parameters, scope),
                    bb1);
  }
  //    removeStatement(target);
}

// Two ways
// 1. builtin function TODO
//    __sync_fetch_and_add_4(&shared, (unsigned int)local);
// 2. using atomic runtime call:
//    GOMP_atomic_start (); // void GOMP_atomic_start (void);
//    shared = shared op local;
//    GOMP_atomic_end (); // void GOMP_atomic_end (void);
// We use the 2nd method only for now, for simplicity and portability
void transOmpAtomic(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpAtomicStatement *target = isSgOmpAtomicStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  replaceStatement(target, body, true);
  SgExprStatement *func_call_stmt1 = buildFunctionCallStmt(
      "__kmpc_atomic_start", buildVoidType(), NULL, scope);
  SgExprStatement *func_call_stmt2 =
      buildFunctionCallStmt("__kmpc_atomic_end", buildVoidType(), NULL, scope);
  insertStatementBefore(body, func_call_stmt1);
  // this is actually sensitive to the type of preprocessing Info
  // In most cases, we want to move up them (such as #ifdef etc)
  moveUpPreprocessingInfo(func_call_stmt1, body, PreprocessingInfo::before);
  insertStatementAfter(body, func_call_stmt2);
}

//! Translate the ordered directive, (not the ordered clause)
void transOmpOrdered(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpOrderedStatement *target = isSgOmpOrderedStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  replaceStatement(target, body, true);
  SgExprStatement *func_call_stmt1 =
      buildFunctionCallStmt("XOMP_ordered_start", buildVoidType(), NULL, scope);
  SgExprStatement *func_call_stmt2 =
      buildFunctionCallStmt("XOMP_ordered_end", buildVoidType(), NULL, scope);
  insertStatementBefore(body, func_call_stmt1);
  insertStatementAfter(body, func_call_stmt2);
}

// Two cases:
// unnamed one
//   GOMP_critical_start ();
//   work()
//   GOMP_critical_end ();
//
// named one:
//  static gomp_mutex_t  &gomp_critical_user_aaa;
//  GOMP_critical_name_start (&gomp_critical_user_aaa);
//  work()
//  GOMP_critical_name_end (&gomp_critical_user_aaa);
//
static const int kKmpCriticalNameWords = 8; // kmp_critical_name is int32_t[8]

void transOmpCritical(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpCriticalStatement *target = isSgOmpCriticalStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgExprStatement *func_call_stmt1 = NULL, *func_call_stmt2 = NULL;
  string c_name = target->get_name().getString();

  // Assign a default lock variable name for unnamed critical directives.
  string g_lock_name = "xomp_critical_user_" + c_name;
  SgVariableSymbol *sym = NULL;
  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    SgBasicBlock *proc_body = func_def->get_body();
    ROSE_ASSERT(proc_body != NULL);

    auto is_direct_module_scope = [](SgScopeStatement *candidate) -> bool {
      if (candidate == NULL)
        return false;
      if (isSgModuleStatement(candidate) != NULL)
        return true;
      if (SgDeclarationStatement *parent_decl =
              isSgDeclarationStatement(candidate->get_parent())) {
        if (isSgModuleStatement(parent_decl) != NULL)
          return true;
      }
      if (SgClassDefinition *class_def = isSgClassDefinition(candidate)) {
        SgDeclarationStatement *decl = class_def->get_declaration();
        return isSgModuleStatement(decl) != NULL;
      }
      return false;
    };

    auto is_symbol_from_current_procedure =
        [&](SgVariableSymbol *candidate) -> bool {
      if (candidate == NULL)
        return false;

      SgScopeStatement *decl_scope = NULL;
      if (SgInitializedName *candidate_decl = candidate->get_declaration())
        decl_scope = candidate_decl->get_scope();
      if (decl_scope == NULL)
        decl_scope = candidate->get_scope();
      if (decl_scope == NULL)
        return false;

      SgFunctionDefinition *decl_func_def =
          getEnclosingFunctionDefinition(decl_scope);
      return decl_func_def != NULL && decl_func_def == func_def;
    };

    auto ensure_local_fortran_lock_symbol = [&]() -> SgVariableSymbol * {
      SgExprListExp *lock_dims =
          buildExprListExp(buildIntVal(kKmpCriticalNameWords));
      SgType *lock_type = buildArrayType(buildIntType(), lock_dims);
      SgVariableDeclaration *vardecl =
          buildVariableDeclaration(g_lock_name, lock_type, NULL, proc_body);
      insert_fortran_declaration_into_procedure(vardecl, proc_body);
      return getFirstVarSym(vardecl);
    };

    sym = lookupVariableSymbolInParentScopes(SgName(g_lock_name), scope);
    bool symbol_is_module_entity = false;
    bool symbol_is_current_procedure_entity = false;
    if (sym != NULL) {
      symbol_is_module_entity = is_direct_module_scope(sym->get_scope());
      if (!symbol_is_module_entity) {
        if (SgInitializedName *sym_decl = sym->get_declaration()) {
          symbol_is_module_entity =
              is_direct_module_scope(sym_decl->get_scope());
        }
      }
      symbol_is_current_procedure_entity =
          is_symbol_from_current_procedure(sym);
    }

    // Host-associated variables from parent procedures cannot appear in COMMON
    // inside this procedure. If we found such a symbol, create a local lock
    // declaration to provide valid COMMON-backed global storage semantics.
    if (sym == NULL ||
        (!symbol_is_module_entity && !symbol_is_current_procedure_entity)) {
      sym = ensure_local_fortran_lock_symbol();
      symbol_is_module_entity = false;
      symbol_is_current_procedure_entity = true;
    }
    ROSE_ASSERT(sym != NULL);

    if (!symbol_is_module_entity) {
      // Fortran COMMON provides global storage semantics for named/unnamed
      // critical locks across procedures in a translation unit.
      const std::string common_block_name = "xomp_critical_block_" + c_name;
      bool has_common_block = false;
      const SgStatementPtrList &stmts = proc_body->get_statements();
      for (SgStatementPtrList::const_iterator it = stmts.begin();
           it != stmts.end(); ++it) {
        SgCommonBlock *common_block = isSgCommonBlock(*it);
        if (common_block == NULL)
          continue;

        const SgCommonBlockObjectPtrList &blocks =
            common_block->get_block_list();
        for (SgCommonBlockObjectPtrList::const_iterator bit = blocks.begin();
             bit != blocks.end(); ++bit) {
          if ((*bit)->get_block_name() == common_block_name) {
            has_common_block = true;
            break;
          }
        }
        if (has_common_block)
          break;
      }

      if (!has_common_block) {
        SgExprListExp *common_vars = buildExprListExp(buildVarRefExp(sym));
        SgCommonBlockObject *common_obj =
            buildCommonBlockObject(common_block_name, common_vars);
        SgCommonBlock *common_decl = buildCommonBlock(common_obj);
        insert_fortran_statement_in_specification_part(common_decl, proc_body);
      }
    }
  } else {
    SgGlobal *global = getGlobalScope(target);
    ROSE_ASSERT(global != NULL);
    sym = lookupVariableSymbolInParentScopes(SgName(g_lock_name), global);
    if (sym == NULL) {
      SgType *lock_type =
          buildArrayType(buildIntType(), buildIntVal(kKmpCriticalNameWords));
      SgVariableDeclaration *vardecl =
          buildVariableDeclaration(g_lock_name, lock_type, NULL, global);
      setStatic(vardecl);
      prependStatement(vardecl, global);
      sym = getFirstVarSym(vardecl);
    }
  }
  ROSE_ASSERT(sym != NULL);

  if (SageInterface::is_Fortran_language()) {
    SgExprListExp *param1 = buildExprListExp(buildVarRefExp(sym));
    SgExprListExp *param2 = buildExprListExp(buildVarRefExp(sym));

    func_call_stmt1 = buildFunctionCallStmt("XOMP_critical_start",
                                            buildVoidType(), param1, scope);
    func_call_stmt2 = buildFunctionCallStmt("XOMP_critical_end",
                                            buildVoidType(), param2, scope);
  } else {
    SgStatement *kmpc_global_tid_init = NULL;
    SgVariableDeclaration *kmpc_global_tid_declaration =
        get_kmpc_global_tid(node, scope, &kmpc_global_tid_init);
    SgName tid_name = getFirstVariable(*kmpc_global_tid_declaration).get_name();

    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
    if (kmpc_global_tid_init != NULL)
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);

    SgExpression *lock_ref1 =
        buildCastExp(buildVarRefExp(sym), buildPointerType(buildVoidType()),
                     SgCastExp::e_C_style_cast);
    SgExpression *lock_ref2 =
        buildCastExp(buildVarRefExp(sym), buildPointerType(buildVoidType()),
                     SgCastExp::e_C_style_cast);

    SgExprListExp *param1 = buildExprListExp(
        buildIntVal(0), buildVarRefExp(tid_name, scope), lock_ref1);
    SgExprListExp *param2 = buildExprListExp(
        buildIntVal(0), buildVarRefExp(tid_name, scope), lock_ref2);

    func_call_stmt1 = buildFunctionCallStmt("__kmpc_critical", buildVoidType(),
                                            param1, scope);
    func_call_stmt2 = buildFunctionCallStmt("__kmpc_end_critical",
                                            buildVoidType(), param2, scope);
  }

  replaceStatement(target, body, true);
  insertStatementBefore(body, func_call_stmt1);
  insertStatementAfter(body, func_call_stmt2);
}

//! Simply replace the pragma with a function call to void GOMP_taskwait(void);
void transOmpTaskwait(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTaskwaitStatement *target = isSgOmpTaskwaitStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(node, scope, &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), scope);

  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }

  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }

  SgExprListExp *parameters =
      buildExprListExp(buildIntVal(0), thread_global_tid);
  SgExprStatement *func_call_stmt = buildFunctionCallStmt(
      "__kmpc_omp_taskwait", buildVoidType(), parameters, scope);
  replaceStatement(target, func_call_stmt, true);
}

//! Simply replace the pragma with a function call to void GOMP_barrier (void);
void transOmpBarrier(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpBarrierStatement *target = isSgOmpBarrierStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(node, scope, &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), scope);

  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }

  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }

  SgExprListExp *parameters =
      buildExprListExp(buildIntVal(0), thread_global_tid);
  SgExprStatement *func_call_stmt = buildFunctionCallStmt(
      "__kmpc_barrier", buildVoidType(), parameters, scope);
  replaceStatement(target, func_call_stmt, true);
}

//! Simply replace the pragma with a function call to __sync_synchronize ();
void transOmpFlush(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpFlushStatement *target = isSgOmpFlushStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgExprStatement *func_call_stmt = NULL;
  if (SageInterface::is_Fortran_language()) {
    func_call_stmt =
        buildFunctionCallStmt("XOMP_flush", buildVoidType(), NULL, scope);
  } else
    func_call_stmt = buildFunctionCallStmt("__sync_synchronize",
                                           buildVoidType(), NULL, scope);
  replaceStatement(target, func_call_stmt, true);
}

// TODO: translate if() and device() clauses
void transOmpTargetData(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTargetDataStatement *target = isSgOmpTargetDataStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  transOmpMapVariables(target, map_variable_list, map_variable_base_list,
                       map_variable_size_list, map_variable_type_list,
                       &offload_ctx, &dynamic_map_entries);

  SgBasicBlock *body = isSgBasicBlock(target->get_body());
  ROSE_ASSERT(body != NULL);

  if (!dynamic_map_entries.empty()) {
    SgBasicBlock *translated_body = buildBasicBlock();

    SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
        "__device_id", buildOpaqueType("int64_t", p_scope),
        buildAssignInitializer(buildLongLongIntVal(-1)), p_scope);
    translated_body->append_statement(device_id_decl);

    RuntimeMapArgumentArrayDeclarations dynamic_arrays =
        buildDynamicRuntimeMapArgumentArrays(
            translated_body, p_scope, map_variable_list, map_variable_base_list,
            map_variable_size_list, map_variable_type_list,
            dynamic_map_entries);

    SgExprListExp *parameters =
        buildExprListExp(buildVarRefExp(device_id_decl),
                         buildVarRefExp(dynamic_arrays.arg_number_decl),
                         buildVarRefExp(dynamic_arrays.args_base_decl),
                         buildVarRefExp(dynamic_arrays.args_decl),
                         buildVarRefExp(dynamic_arrays.arg_sizes_decl),
                         buildVarRefExp(dynamic_arrays.arg_types_decl));
    SgExprStatement *begin_stmt = buildFunctionCallStmt(
        "__tgt_target_data_begin", buildVoidType(), parameters, p_scope);
    setSourcePositionForTransformation(begin_stmt);
    translated_body->append_statement(begin_stmt);

    body->set_parent(NULL);
    target->set_body(NULL);
    translated_body->append_statement(body);

    SgExprStatement *end_stmt = buildFunctionCallStmt(
        "__tgt_target_data_end", buildVoidType(),
        buildExprListExp(buildVarRefExp(device_id_decl),
                         buildVarRefExp(dynamic_arrays.arg_number_decl),
                         buildVarRefExp(dynamic_arrays.args_base_decl),
                         buildVarRefExp(dynamic_arrays.args_decl),
                         buildVarRefExp(dynamic_arrays.arg_sizes_decl),
                         buildVarRefExp(dynamic_arrays.arg_types_decl)),
        p_scope);
    setSourcePositionForTransformation(end_stmt);
    translated_body->append_statement(end_stmt);

    appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays, translated_body,
                                                p_scope);

    replaceStatement(target, translated_body, true);
    attachComment(translated_body,
                  "Translated from #pragma omp target data ...");
    return;
  }

  SgBasicBlock *target_data_begin_block = body;

  // Use the OpenMP runtime's default device sentinel.
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", buildOpaqueType("int64_t", p_scope),
      buildAssignInitializer(buildLongLongIntVal(-1)), p_scope);
  target_data_begin_block->prepend_statement(device_id_decl);

  SgBracedInitializer *offloading_variables_base =
      buildBracedInitializer(map_variable_base_list);
  SgVariableDeclaration *args_base_decl = buildVariableDeclaration(
      "__args_base", buildArrayType(buildPointerType(buildVoidType())),
      buildAssignInitializer(offloading_variables_base), p_scope);
  target_data_begin_block->prepend_statement(args_base_decl);

  SgBracedInitializer *offloading_variables =
      buildBracedInitializer(map_variable_list);
  SgVariableDeclaration *args_decl = buildVariableDeclaration(
      "__args", buildArrayType(buildPointerType(buildVoidType())),
      buildAssignInitializer(offloading_variables), p_scope);
  target_data_begin_block->prepend_statement(args_decl);

  SgBracedInitializer *map_variable_sizes =
      buildBracedInitializer(map_variable_size_list);
  SgVariableDeclaration *arg_sizes = buildVariableDeclaration(
      "__arg_sizes", buildArrayType(buildOpaqueType("int64_t", p_scope)),
      buildAssignInitializer(map_variable_sizes), p_scope);
  target_data_begin_block->prepend_statement(arg_sizes);

  SgBracedInitializer *map_variable_types =
      buildBracedInitializer(map_variable_type_list);
  SgVariableDeclaration *arg_types = buildVariableDeclaration(
      "__arg_types", buildArrayType(buildOpaqueType("int64_t", p_scope)),
      buildAssignInitializer(map_variable_types), p_scope);
  target_data_begin_block->prepend_statement(arg_types);

  int kernel_arg_num = map_variable_base_list->get_expressions().size();
  SgVariableDeclaration *arg_number_decl = buildVariableDeclaration(
      "__arg_num", buildOpaqueType("int32_t", p_scope),
      buildAssignInitializer(buildIntVal(kernel_arg_num)), p_scope);
  target_data_begin_block->prepend_statement(arg_number_decl);

  // call __tgt_target_data_begin to start the data mapping region for GPU
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(arg_number_decl),
      buildVarRefExp(args_base_decl), buildVarRefExp(args_decl),
      buildVarRefExp(arg_sizes), buildVarRefExp(arg_types));
  string func_offloading_name = "__tgt_target_data_begin";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildVoidType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  insertStatementAfter(device_id_decl, func_offloading_stmt);

  // call __tgt_target_data_end to end the data mapping region for GPU
  func_offloading_name = "__tgt_target_data_end";
  func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildVoidType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  body->append_statement(func_offloading_stmt);
  body->set_parent(NULL);
  target->set_body(NULL);

  replaceStatement(target, body, true);
  attachComment(body, "Translated from #pragma omp target data ...");
}

void transOmpTargetUpdate(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTargetUpdateStatement *target = isSgOmpTargetUpdateStatement(node);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  collectOmpTargetUpdateInfo(target, map_variable_list, map_variable_base_list,
                             map_variable_size_list, map_variable_type_list,
                             &dynamic_map_entries);

  if (!dynamic_map_entries.empty()) {
    SgBasicBlock *translated_block = buildBasicBlock();
    SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
        "__device_id", buildOpaqueType("int64_t", p_scope),
        buildAssignInitializer(buildLongLongIntVal(-1)), p_scope);
    translated_block->append_statement(device_id_decl);

    RuntimeMapArgumentArrayDeclarations dynamic_arrays =
        buildDynamicRuntimeMapArgumentArrays(
            translated_block, p_scope, map_variable_list,
            map_variable_base_list, map_variable_size_list,
            map_variable_type_list, dynamic_map_entries);

    SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
        "__tgt_target_data_update", buildVoidType(),
        buildExprListExp(buildVarRefExp(device_id_decl),
                         buildVarRefExp(dynamic_arrays.arg_number_decl),
                         buildVarRefExp(dynamic_arrays.args_base_decl),
                         buildVarRefExp(dynamic_arrays.args_decl),
                         buildVarRefExp(dynamic_arrays.arg_sizes_decl),
                         buildVarRefExp(dynamic_arrays.arg_types_decl)),
        p_scope);
    setSourcePositionForTransformation(func_offloading_stmt);
    translated_block->append_statement(func_offloading_stmt);

    appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays,
                                                translated_block, p_scope);

    translated_block->set_parent(target->get_parent());
    replaceStatement(target, translated_block, true);
    attachComment(func_offloading_stmt,
                  "Translated from #pragma omp target update ...");
    return;
  }

  SgBasicBlock *target_data_begin_block = buildBasicBlock();
  // Use the OpenMP runtime's default device sentinel.
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", buildOpaqueType("int64_t", p_scope),
      buildAssignInitializer(buildLongLongIntVal(-1)), p_scope);
  target_data_begin_block->prepend_statement(device_id_decl);

  SgBracedInitializer *offloading_variables_base =
      buildBracedInitializer(map_variable_base_list);
  SgVariableDeclaration *args_base_decl = buildVariableDeclaration(
      "__args_base", buildArrayType(buildPointerType(buildVoidType())),
      buildAssignInitializer(offloading_variables_base), p_scope);
  target_data_begin_block->prepend_statement(args_base_decl);

  SgBracedInitializer *offloading_variables =
      buildBracedInitializer(map_variable_list);
  SgVariableDeclaration *args_decl = buildVariableDeclaration(
      "__args", buildArrayType(buildPointerType(buildVoidType())),
      buildAssignInitializer(offloading_variables), p_scope);
  target_data_begin_block->prepend_statement(args_decl);

  SgBracedInitializer *map_variable_sizes =
      buildBracedInitializer(map_variable_size_list);
  SgVariableDeclaration *arg_sizes = buildVariableDeclaration(
      "__arg_sizes", buildArrayType(buildOpaqueType("int64_t", p_scope)),
      buildAssignInitializer(map_variable_sizes), p_scope);
  target_data_begin_block->prepend_statement(arg_sizes);

  SgBracedInitializer *map_variable_types =
      buildBracedInitializer(map_variable_type_list);
  SgVariableDeclaration *arg_types = buildVariableDeclaration(
      "__arg_types", buildArrayType(buildOpaqueType("int64_t", p_scope)),
      buildAssignInitializer(map_variable_types), p_scope);
  target_data_begin_block->prepend_statement(arg_types);

  int kernel_arg_num = map_variable_base_list->get_expressions().size();
  SgVariableDeclaration *arg_number_decl = buildVariableDeclaration(
      "__arg_num", buildOpaqueType("int32_t", p_scope),
      buildAssignInitializer(buildIntVal(kernel_arg_num)), p_scope);
  target_data_begin_block->prepend_statement(arg_number_decl);

  // call __tgt_target_data_begin to start the data mapping region for GPU
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(arg_number_decl),
      buildVarRefExp(args_base_decl), buildVarRefExp(args_decl),
      buildVarRefExp(arg_sizes), buildVarRefExp(arg_types));
  string func_offloading_name = "__tgt_target_data_update";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildVoidType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  insertStatementAfter(device_id_decl, func_offloading_stmt);

  target_data_begin_block->set_parent(target->get_parent());
  replaceStatement(target, target_data_begin_block, true);
  attachComment(func_offloading_stmt,
                "Translated from #pragma omp target update ...");
}

void transOmpAllocate(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpAllocateStatement *target = isSgOmpAllocateStatement(node);
  ROSE_ASSERT(target != NULL);

  if (!SageInterface::is_Fortran_language()) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate statement lowering is currently implemented only "
           "for Fortran allocate statements";
    ROSE_ABORT();
  }

  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  ensureFortranOmpAllocatorInterfaces(scope);

  SgStatement *next_stmt = SageInterface::getNextStatement(target);
  SgAllocateStatement *allocate_stmt = isSgAllocateStatement(next_stmt);
  if (allocate_stmt == NULL) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate statement lowering expects the next statement to "
           "be a Fortran allocate statement";
    ROSE_ABORT();
  }

  const std::set<SgInitializedName *> target_objects =
      collectReferencedBaseObjects(target->get_variables());
  const std::set<SgInitializedName *> allocate_objects =
      collectAllocateStatementBaseObjects(allocate_stmt);
  if (target_objects.empty() || target_objects != allocate_objects) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate lowering currently requires the directive variable "
           "list to match the following allocate statement exactly";
    ROSE_ABORT();
  }

  SgOmpAllocatorClause *allocator_clause = getAllocatorClauseOrAbort(target);
  SgExpression *allocator_expr =
      buildAllocatorArgumentExpression(allocator_clause, scope);
  SgType *allocator_type = allocator_expr->get_type();
  if (allocator_type == NULL) {
    allocator_type = buildIntType();
  }

  SgBasicBlock *procedure_body = getEnclosingFortranProcedureBody(scope);
  const std::string saved_name =
      generateUniqueVariableName(procedure_body, "__rex_saved_allocator_");
  SgVariableDeclaration *saved_decl = buildVariableDeclaration(
      saved_name, allocator_type, NULL, procedure_body);
  insert_fortran_declaration_into_procedure(saved_decl, scope);

  SgInitializedName &saved_var = getFirstVariable(*saved_decl);
  SgExprStatement *save_stmt = buildAssignStatement(
      buildVarRefExp(saved_var.get_name(), scope),
      buildFunctionCallExp("omp_get_default_allocator", allocator_type,
                           buildExprListExp(), scope));
  SgExprStatement *set_stmt =
      buildFunctionCallStmt("omp_set_default_allocator", buildVoidType(),
                            buildExprListExp(allocator_expr), scope);
  SgExprStatement *restore_stmt = buildFunctionCallStmt(
      "omp_set_default_allocator", buildVoidType(),
      buildExprListExp(buildVarRefExp(saved_var.get_name(), scope)), scope);

  attachComment(save_stmt,
                "Translated from OpenMP allocate using explicit allocator "
                "runtime calls.");
  insertStatementBefore(allocate_stmt, save_stmt);
  insertStatementAfter(save_stmt, set_stmt);
  insertStatementAfter(allocate_stmt, restore_stmt);
  removeStatement(target);
}

void transOmpRequires(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpRequiresStatement *target = isSgOmpRequiresStatement(node);
  ROSE_ASSERT(target != NULL);

  if (!requiresOnlyDynamicAllocators(target)) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP requires lowering currently supports only "
           "requires(dynamic_allocators)";
    ROSE_ABORT();
  }

  if (SgStatement *next_stmt = SageInterface::getNextStatement(target)) {
    attachComment(
        next_stmt,
        "Translated from OpenMP requires(dynamic_allocators); allocator "
        "semantics are lowered to explicit runtime calls.");
  }

  removeStatement(target);
}

//! Add __thread for each threadprivate variable's declaration statement and
//! remove the #pragma omp threadprivate(...)
void transOmpThreadprivate(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpThreadprivateStatement *target = isSgOmpThreadprivateStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpressionPtrList nameList = target->get_variables();
  for (size_t i = 0; i < nameList.size(); i++) {
    SgVarRefExp *vref = extractVarRefFromExpression(nameList[i]);
    if (vref == nullptr) {
      continue;
    }
    SgInitializedName *init_name = vref->get_symbol()->get_declaration();
    ROSE_ASSERT(init_name != NULL);
    SgVariableDeclaration *decl =
        isSgVariableDeclaration(init_name->get_declaration());
    ROSE_ASSERT(decl != NULL);
    // cout<<"setting TLS for decl:"<<decl->unparseToString()<< endl;
    decl->get_declarationModifier()
        .get_storageModifier()
        .set_thread_local_storage(true);
    // choice between set TLS to declaration or init_name (not working) ?
    // init_name-> get_storageModifier ().set_thread_local_storage (true);
  }

  // 6/8/2010, handling #if attached to #pragma omp threadprivate
  SgStatement *n_stmt = SageInterface::getNextStatement(target);
  if (n_stmt == NULL) {
    cerr << "Warning: found an omp threadprivate directive without a following "
            "statement."
         << endl;
    cerr << "Warning: the attached preprocessing information to the directive "
            "may get lost during translation!"
         << endl;
  } else {
    // preserve preprocessing information attached to the pragma,
    // by moving it to the beginning of the preprocessing info list of the next
    // statement .
    movePreprocessingInfo(target, n_stmt, PreprocessingInfo::before,
                          PreprocessingInfo::before, true);
  }

  removeStatement(target);
}

//! Lowers the OMP unroll statement
void transOmpUnroll(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpUnrollStatement *target = isSgOmpUnrollStatement(node);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  SgNode *cur_parent = target->get_parent();

  // Get the for loop
  SgForStatement *for_loop;
  if (isSgOmpBodyStatement(body)) {
    SgOmpBodyStatement *target2 = isSgOmpBodyStatement(body);
    std::vector<SgNode *> loop_list =
        NodeQuery::querySubTree(target->get_body(), V_SgForStatement);
    ROSE_ASSERT(loop_list.size() >= 1);
    for_loop = isSgForStatement(loop_list.front());
  } else {
    for_loop = isSgForStatement(body);
  }

  ROSE_ASSERT(for_loop != NULL);
  SageInterface::forLoopNormalization(for_loop);

  // Get the clause so we can figure out the unrolling factor
  SgOmpClause *clause = target->get_clauses().front();
  if (clause->variantT() == V_SgOmpFullClause) {
    SgExprStatement *test_stmt = isSgExprStatement(for_loop->get_test());
    SgBinaryOp *test = isSgBinaryOp(test_stmt->get_expression());
    ROSE_ASSERT(test != NULL);

    SgIntVal *val = isSgIntVal(test->get_rhs_operand());
    ROSE_ASSERT(val != NULL);

    SageInterface::loopUnrolling(for_loop, val->get_value() + 1);
    test->set_rhs_operand(val);
  } else if (clause->variantT() == V_SgOmpPartialClause) {
    SgOmpPartialClause *partial = static_cast<SgOmpPartialClause *>(clause);
    SgExpression *partial_expr = partial->get_expression();
    if (partial_expr->variantT() == V_SgIntVal) {
      SgIntVal *val = static_cast<SgIntVal *>(partial_expr);
      SageInterface::loopUnrolling(for_loop, val->get_value());
    } else {
      puts("Expected integer in OMP Partial Clause.");
    }
  } else {
    puts("Unknown clause in OMP unroll.");
  }

  if (isSgOmpBodyStatement(body)) {
    SgOmpBodyStatement *ompstmt = isSgOmpBodyStatement(body);
    ompstmt->set_body(for_loop);
    replaceStatement(for_loop, ompstmt, true);
    replaceStatement(target, body, true);
  } else {
    replaceStatement(target, body, true);
  }

  body->set_parent(cur_parent);
}

void transOmpTileSub(SgForStatement *for_loop, SgExprListExp *list,
                     int loop_level) {
  std::vector<SgNode *> loop_list =
      NodeQuery::querySubTree(getLoopBody(for_loop), V_SgForStatement);
  for (std::vector<SgNode *>::iterator i = loop_list.begin();
       i != loop_list.end(); i++) {
    SgForStatement *loop = isSgForStatement(*i);
    ROSE_ASSERT(loop != NULL);

    SgIntVal *tile_size =
        isSgIntVal(list->get_expressions().at(loop_level - 1));
    SageInterface::loopTiling(loop, 1, tile_size->get_value());

    transOmpTileSub(loop, list, loop_level + 1);
  }
}

//! Lowers the OMP tile statement
// Yes, this is basically the same as the unroll
void transOmpTile(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTileStatement *target = isSgOmpTileStatement(node);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  // Get the for loop
  SgForStatement *for_loop;
  if (isSgOmpBodyStatement(body)) {
    SgOmpBodyStatement *target2 = isSgOmpBodyStatement(body);
    std::vector<SgNode *> loop_list =
        NodeQuery::querySubTree(target->get_body(), V_SgForStatement);
    ROSE_ASSERT(loop_list.size() >= 1);
    for_loop = isSgForStatement(loop_list.front());
  } else {
    for_loop = isSgForStatement(body);
  }

  ROSE_ASSERT(for_loop != NULL);
  SageInterface::forLoopNormalization(for_loop);

  SgOmpSizesClause *sizes =
      static_cast<SgOmpSizesClause *>(target->get_clauses().front());
  SgExprListExp *list = static_cast<SgExprListExp *>(sizes->get_expression());

  // There should always be at least one size
  SgIntVal *tile_size =
      static_cast<SgIntVal *>(list->get_expressions().front());
  SageInterface::loopTiling(for_loop, 1, tile_size->get_value());

  // Get any sub loops
  transOmpTileSub(for_loop, list, 2);

  // Temporary workaround. It should be updated according to the SgInterface
  // tiling API.
  SgBasicBlock *new_tile_body = isSgBasicBlock(target->get_body());
  SgForStatement *new_for_loop = deepCopy(for_loop);
  SgStatement *old_body = deepCopy(body);
  replaceStatement(for_loop, old_body, true);
  SgOmpBodyStatement *ompstmt = isSgOmpBodyStatement(old_body);
  if (ompstmt)
    ompstmt->set_body(new_for_loop);
  // ompstmt->set_body(new_for_loop);
  replaceStatement(target, new_tile_body, true);
  if (ompstmt)
    removeStatement(body);
}

//! Collect variables from OpenMP clauses: including private, firstprivate,
//! lastprivate, reduction, etc.
SgInitializedNamePtrList collectClauseVariables(SgStatement *clause_stmt,
                                                const VariantT &vt) {
  return collectClauseVariables(clause_stmt, VariantVector(vt));
}

// Collect variables from an OpenMP clause: including private, firstprivate,
// lastprivate, reduction, etc.
SgInitializedNamePtrList collectClauseVariables(SgStatement *clause_stmt,
                                                const VariantVector &vvt) {
  SgInitializedNamePtrList result, result2;
  ROSE_ASSERT(clause_stmt != NULL);
  Rose_STL_Container<SgOmpClause *> p_clause = getClause(clause_stmt, vvt);
  for (size_t i = 0; i < p_clause.size();
       i++) // can have multiple reduction clauses of different reduction
            // operations
  {
    SgOmpVariablesClause *vars_clause = isSgOmpVariablesClause(p_clause[i]);
    if (vars_clause == NULL)
      continue;
    SgExprListExp *vars = vars_clause->get_variables();
    if (vars == NULL)
      continue;
    // get initialized name from varRefExp
    SgExpressionPtrList refs = vars->get_expressions();
    const VariantT clause_variant = p_clause[i]->variantT();
    const bool allow_designators = clause_variant == V_SgOmpMapClause ||
                                   clause_variant == V_SgOmpToClause ||
                                   clause_variant == V_SgOmpFromClause;
    result2.clear();
    for (size_t j = 0; j < refs.size(); j++) {
      SgVariableSymbol *symbol = NULL;
      if (allow_designators) {
        symbol = extractClauseVariableSymbol(refs[j]);
      } else {
        SgExpression *expr = stripNoopCastsAndParens(refs[j]);
        SgVarRefExp *var_ref = isSgVarRefExp(expr);
        if (var_ref != NULL) {
          symbol = isSgVariableSymbol(var_ref->get_symbol());
        }
      }
      if (symbol == NULL) {
        continue;
      }
      result2.push_back(symbol->get_declaration());
    }
    std::copy(result2.begin(), result2.end(), back_inserter(result));
  }
  return result;
}

SgExpression *getClauseExpression(SgStatement *clause_stmt,
                                  const VariantVector &vvt) {
  SgExpression *expr = NULL;
  ROSE_ASSERT(clause_stmt != NULL);
  SgOmpClausePtrList clauses;
  if (isSgOmpClauseBodyStatement(clause_stmt)) {
    clauses = (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses();
  } else if (isSgOmpClauseStatement(clause_stmt)) {
    clauses = (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  } else {
    ROSE_ABORT();
  }
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clauses, vvt);
  // It is possible that the requested clauses are not found. We allow returning
  // NULL expression. Liao, 6/16/2015
  if (p_clause.size() >= 1)
    expr = isSgOmpExpressionClause(p_clause[0])->get_expression();
  return expr;
}

//! Collect all variables from OpenMP clauses associated with an omp statement:
//! private, reduction, etc
SgInitializedNamePtrList collectAllClauseVariables(SgStatement *clause_stmt) {
  ROSE_ASSERT(clause_stmt != NULL);

  VariantVector vvt = VariantVector(V_SgOmpCopyinClause);
  vvt.push_back(V_SgOmpCopyprivateClause);
  vvt.push_back(V_SgOmpFirstprivateClause);
  vvt.push_back(V_SgOmpLastprivateClause);
  vvt.push_back(V_SgOmpPrivateClause);
  vvt.push_back(V_SgOmpReductionClause);
  // TODO : do we care about shared(var_list)?

  return collectClauseVariables(clause_stmt, vvt);
}

bool isInClauseVariableList(SgInitializedName *var,
                            SgOmpClauseBodyStatement *clause_stmt,
                            const VariantVector &vvt) {
  SgInitializedNamePtrList var_list = collectClauseVariables(clause_stmt, vvt);
  if (find(var_list.begin(), var_list.end(), var) != var_list.end())
    return true;
  else
    return false;
}

//! Return a reduction variable's reduction operation type
SgOmpClause::omp_reduction_identifier_enum
getReductionOperationType(SgInitializedName *init_name,
                          SgOmpClauseBodyStatement *clause_stmt) {
  SgOmpClause::omp_reduction_identifier_enum result =
      SgOmpClause::e_omp_reduction_unknown;
  bool found = false;
  ROSE_ASSERT(init_name != NULL);
  ROSE_ASSERT(clause_stmt != NULL);
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clause_stmt->get_clauses(),
                                            V_SgOmpReductionClause);
  ROSE_ASSERT(p_clause.size() > 0); // must be have at least reduction clause

  for (size_t i = 0; i < p_clause.size();
       i++) // can have multiple reduction clauses of different reduction
            // operations
  {
    SgOmpReductionClause *r_clause = isSgOmpReductionClause(p_clause[i]);
    ROSE_ASSERT(r_clause != NULL);
    SgExpressionPtrList refs =
        isSgOmpVariablesClause(r_clause)->get_variables()->get_expressions();
    SgInitializedNamePtrList
        var_list; //= isSgOmpVariablesClause(r_clause)->get_variables();
    for (size_t j = 0; j < refs.size(); j++) {
      SgExpression *expr = stripNoopCastsAndParens(refs[j]);
      SgVarRefExp *var_ref = isSgVarRefExp(expr);
      if (var_ref == NULL || var_ref->get_symbol() == NULL)
        continue;
      var_list.push_back(var_ref->get_symbol()->get_declaration());
    }
    SgInitializedNamePtrList::const_iterator iter =
        find(var_list.begin(), var_list.end(), init_name);
    if (iter != var_list.end()) {
      result = r_clause->get_identifier();
      found = true;
      break;
    }
  }
  // Must have a hit
  ROSE_ASSERT(found == true);
  return result;
}

//! Create an initial value according to reduction operator type
SgExpression *
createInitialValueExp(SgOmpClause::omp_reduction_identifier_enum r_operator) {
  SgExpression *result = NULL;
  switch (r_operator) {
  // 0: + - ! ^ ||  ior ieor
  case SgOmpClause::e_omp_reduction_plus:
  case SgOmpClause::e_omp_reduction_minus:
  case SgOmpClause::e_omp_reduction_bitor:
  case SgOmpClause::e_omp_reduction_bitxor:
  case SgOmpClause::e_omp_reduction_or:
  case SgOmpClause::e_omp_reduction_ior:
  case SgOmpClause::e_omp_reduction_ieor:
    result = buildIntVal(0);
    break;
  // 1: * &&
  case SgOmpClause::e_omp_reduction_mul:
  case SgOmpClause::e_omp_reduction_bitand:
    result = buildIntVal(1);
    break;
    // TODO
  case SgOmpClause::e_omp_reduction_logand:
  case SgOmpClause::e_omp_reduction_logor:
  case SgOmpClause::e_omp_reduction_and:
  case SgOmpClause::e_omp_reduction_eqv:
  case SgOmpClause::e_omp_reduction_neqv:
  case SgOmpClause::e_omp_reduction_max:
  case SgOmpClause::e_omp_reduction_min:
  case SgOmpClause::e_omp_reduction_iand:

  case SgOmpClause::e_omp_reduction_unknown:
  case SgOmpClause::e_omp_reduction_last:
  default:
    cerr << "Illegal or unhandled reduction operator kind: " << r_operator
         << endl;
    ROSE_ABORT();
  }

  return result;
}

//! Check if a variable is in a variable list of a given clause type
bool isInClauseVariableList(SgInitializedName *var,
                            SgOmpClauseBodyStatement *clause_stmt,
                            const VariantT &vt) {
  return isInClauseVariableList(var, clause_stmt, VariantVector(vt));
}

// lastprivate can be used with loop constructs or sections.
/* if (i is the last iteration)
 *   *shared_i_p = local_i
 *
 * The judge of last iteration is based on the iteration space increment
 * direction and loop stop conditions Incremental loops < upper:   last
 * iteration ==> i >= upper
 *      <=     :                      i> upper
 * Decremental loops
 *      > upper:   last iteration ==> i <= upper
 *      >=     :                      i < upper
 * AST: Orphaned worksharing OmpStatement is SgOmpForStatement->get_body() is
 * SgForStatement
 *
 *  We use bottom up traversal, the inner omp for loop has already been
 * translated, so we have to get the original upper bound via parameter
 *
 *  Another tricky case is that when some threads don't get any iterations to
 * work on, the initial _p_index may still trigger the lastprivate 's if
 * (_p_index>orig_bound) statement We add a condition to test if the thread
 * really worked on at least on iteration before compare the _p_index and the
 * original boundary if (_p_index != p_lower_ && _p_index>orig_bound) statement
 *
 *  Parameters:
 *    ompStmt: the OpenMP statement node with a lastprivate clause
 *    end_stmt_list: a list of statement which will be append to the end of bb1.
 * The generated if-stmt will be added to the end of this list bb1: the basic
 * block affected by the lastprivate clause orig_var: the initialized name for
 * the original lastprivate variable. Necessary since transOmpLoop will replace
 * loop index with changed one local_decl: the variable declaration for the
 * local copy of the lastprivate variable orig_loop_upper: the worksharing
 * construct's upper limit: for-loop: the loop upper value, sections: the
 * section count - 1
 *
 * */
static void insertOmpLastprivateCopyBackStmts(
    SgStatement *ompStmt, vector<SgStatement *> &end_stmt_list,
    SgBasicBlock *bb1, SgInitializedName *orig_var,
    SgVariableDeclaration *local_decl, SgExpression *orig_loop_upper) {
  SgStatement *save_stmt = NULL;
  SgExpression *orig_var_exp = buildVarRefExp(orig_var, bb1);
  if (SgOmpExecStatement *target = isSgOmpExecStatement(ompStmt)) {
    std::map<SgOmpExecStatement *, std::map<SgInitializedName *, SgExpression *>
                                       *>::const_iterator map_iter =
        clause_variable_renaming_record.find(target);
    if (map_iter != clause_variable_renaming_record.end()) {
      std::map<SgInitializedName *, SgExpression *>::const_iterator var_iter =
          map_iter->second->find(orig_var);
      if (var_iter != map_iter->second->end())
        orig_var_exp = copyExpression(var_iter->second);
    }
  }
  if (isSgOmpForStatement(ompStmt) || isSgOmpDoStatement(ompStmt)) {
    ROSE_ASSERT(orig_loop_upper != NULL);
    SgInitializedName *loop_index = NULL;
    SgExpression *loop_lower = NULL;
    SgExpression *loop_upper = NULL;
    SgExpression *loop_step = NULL;
    SgStatement *loop_body = NULL;
    bool isIncremental = true;
    bool isInclusiveBound = false;
    bool isCanonical = false;

    SgStatement *selected_loop = NULL;
    size_t selected_loop_depth = 0;
    bool has_selected_loop = false;
    auto consider_loop_candidate = [&](SgStatement *candidate) {
      if (candidate == NULL)
        return;
      size_t depth = 0;
      SgNode *cursor = candidate;
      while (cursor != NULL && cursor != bb1) {
        cursor = cursor->get_parent();
        ++depth;
      }
      if (cursor != bb1)
        return;
      if (!has_selected_loop || depth < selected_loop_depth) {
        selected_loop = candidate;
        selected_loop_depth = depth;
        has_selected_loop = true;
      }
    };

    Rose_STL_Container<SgNode *> c_loops =
        NodeQuery::querySubTree(bb1, V_SgForStatement);
    for (Rose_STL_Container<SgNode *>::const_iterator it = c_loops.begin();
         it != c_loops.end(); ++it)
      consider_loop_candidate(isSgStatement(*it));

    Rose_STL_Container<SgNode *> f_loops =
        NodeQuery::querySubTree(bb1, V_SgFortranDo);
    for (Rose_STL_Container<SgNode *>::const_iterator it = f_loops.begin();
         it != f_loops.end(); ++it)
      consider_loop_candidate(isSgStatement(*it));

    if (selected_loop == NULL) {
      MLOG_ERROR_CXX("ompLowering") << "Failed to find a lowered loop under "
                                    << ompStmt->sage_class_name()
                                    << " while inserting lastprivate copy-back";
      ROSE_ABORT();
      return;
    }

    if (SgForStatement *top_loop = isSgForStatement(selected_loop)) {
      isCanonical = SageInterface::isCanonicalForLoop(
          top_loop, &loop_index, &loop_lower, &loop_upper, &loop_step,
          &loop_body, &isIncremental, &isInclusiveBound);
    } else if (SgFortranDo *top_loop = isSgFortranDo(selected_loop)) {
      isCanonical = SageInterface::isCanonicalDoLoop(
          top_loop, &loop_index, &loop_lower, &loop_upper, &loop_step,
          &loop_body, &isIncremental, &isInclusiveBound);
    } else {
      MLOG_ERROR_CXX("ompLowering")
          << "Selected non-loop node " << selected_loop->sage_class_name()
          << " while inserting lastprivate copy-back";
      ROSE_ABORT();
      return;
    }
    if (!isCanonical) {
      MLOG_ERROR_CXX("ompLowering")
          << "Non-canonical lowered loop under " << ompStmt->sage_class_name()
          << " while inserting lastprivate copy-back";
      ROSE_ABORT();
      return;
    }
    SgExpression *if_cond = NULL;
    SgStatement *if_cond_stmt = NULL;
    // we need the original upper bound!!
    if (isIncremental) {
      if (isInclusiveBound) // <= --> >
      {
        if_cond = buildGreaterThanOp(buildVarRefExp(loop_index, bb1),
                                     copyExpression(orig_loop_upper));
      } else // < --> >=
      {
        if_cond = buildGreaterOrEqualOp(buildVarRefExp(loop_index, bb1),
                                        copyExpression(orig_loop_upper));
      }
    } else {                // decremental loop
      if (isInclusiveBound) // >= --> <
      {
        if_cond = buildLessThanOp(buildVarRefExp(loop_index, bb1),
                                  copyExpression(orig_loop_upper));
      } else // > --> <=
      {
        if_cond = buildLessOrEqualOp(buildVarRefExp(loop_index, bb1),
                                     copyExpression(orig_loop_upper));
      }
    }
    // Add (_p_index != _p_lower) as another condition, making sure the current
    // thread really worked on at least one iteration Otherwise some thread
    // which does not run any iteration may have a big initial _p_index and
    // trigger the if statement's condition
    if_cond_stmt = buildExprStatement(
        buildAndOp(buildNotEqualOp(buildVarRefExp(loop_index, bb1),
                                   copyExpression(loop_lower)),
                   if_cond));
    SgStatement *true_body = buildAssignStatement(copyExpression(orig_var_exp),
                                                  buildVarRefExp(local_decl));
    save_stmt = buildIfStmt(if_cond_stmt, true_body, NULL);
  } else if (isSgOmpSectionsStatement(ompStmt)) {
    ROSE_ASSERT(orig_loop_upper != NULL);
    Rose_STL_Container<SgNode *> while_stmts =
        NodeQuery::querySubTree(bb1, V_SgWhileStmt);
    ROSE_ASSERT(while_stmts.size() != 0);
    SgWhileStmt *top_while_stmt = isSgWhileStmt(while_stmts[0]);
    ROSE_ASSERT(top_while_stmt != NULL);
    // Get the section id variable from while-stmt  while(section_id >= 0) {}
    //  SgWhileStmt -> SgExprStatement -> SgGreaterOrEqualOp-> SgVarRefExp
    SgExprStatement *exp_stmt =
        isSgExprStatement(top_while_stmt->get_condition());
    ROSE_ASSERT(exp_stmt != NULL);
    SgGreaterOrEqualOp *ge_op =
        isSgGreaterOrEqualOp(exp_stmt->get_expression());
    ROSE_ASSERT(ge_op != NULL);
    SgVarRefExp *var_ref = isSgVarRefExp(ge_op->get_lhs_operand());
    ROSE_ASSERT(var_ref != NULL);
    string switch_index_name = (var_ref->get_symbol()->get_name()).getString();
    SgExpression *if_cond = NULL;
    SgStatement *if_cond_stmt = NULL;
    if_cond =
        buildEqualityOp(buildVarRefExp((switch_index_name + "_save"), bb1),
                        orig_loop_upper); // no need copy orig_loop_upper here
    if_cond_stmt = buildExprStatement(if_cond);
    SgStatement *true_body = buildAssignStatement(copyExpression(orig_var_exp),
                                                  buildVarRefExp(local_decl));
    save_stmt = buildIfStmt(if_cond_stmt, true_body, NULL);
  } else {
    cerr << "Illegal SgOmpxx for lastprivate variable: \nOmpStatement is:"
         << ompStmt->class_name() << endl;
    cerr << "lastprivate variable is:" << orig_var->get_name().getString()
         << endl;
    ROSE_ABORT();
  }
  end_stmt_list.push_back(save_stmt);
}

//! Generate copy-back statements for reduction variables
// end_stmt_list: the statement lists to be appended
// bb1: the affected code block by the reduction clause
// orig_var: the reduction variable's original copy
// local_decl: the local copy of the reduction variable
// Two ways to do the reduction operation:
// 1. builtin function TODO
//    __sync_fetch_and_add_4(&shared, (unsigned int)local);
// 2. using atomic runtime call:
//    GOMP_atomic_start ();
//    shared = shared op local;
//    GOMP_atomic_end ();
// We use the 2nd method only for now for simplicity and portability
static void insertOmpReductionCopyBackStmts(
    SgOmpClause::omp_reduction_identifier_enum r_operator,
    vector<SgStatement *> &end_stmt_list, SgBasicBlock *bb1,
    SgInitializedName *orig_var, SgVariableDeclaration *local_decl,
    SgStatement *node) {
  SgExprStatement *atomic_start_stmt =
      buildFunctionCallStmt("__kmpc_atomic_start", buildVoidType(), NULL, bb1);
  end_stmt_list.push_back(atomic_start_stmt);
  SgExpression *r_exp = NULL;
  SgExpression *orig_var_exp_template = buildVarRefExp(orig_var, bb1);
  SgOmpExecStatement *target = isSgOmpExecStatement(node);
  if (clause_variable_renaming_record.count(target)) {
    std::map<SgInitializedName *, SgExpression *> *name_mapping =
        clause_variable_renaming_record[target];
    std::map<SgInitializedName *, SgExpression *>::const_iterator map_iter =
        name_mapping->find(orig_var);
    if (map_iter != name_mapping->end())
      orig_var_exp_template = map_iter->second;
  }

  // Build distinct trees for assignment lhs and rhs to avoid reusing the same
  // expression node in two places.
  SgExpression *orig_var_lhs_exp = copyExpression(orig_var_exp_template);
  SgExpression *orig_var_rhs_exp = copyExpression(orig_var_exp_template);

  switch (r_operator) {
  case SgOmpClause::e_omp_reduction_plus:
    r_exp = buildAddOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_mul:
    r_exp = buildMultiplyOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_minus:
    r_exp = buildSubtractOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_bitand:
    r_exp = buildBitAndOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_bitor:
    r_exp = buildBitOrOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_bitxor:
    r_exp = buildBitXorOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_logand:
    r_exp = buildAndOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
  case SgOmpClause::e_omp_reduction_logor:
    r_exp = buildOrOp(orig_var_rhs_exp, buildVarRefExp(local_decl));
    break;
    // TODO Fortran operators.
  case SgOmpClause::e_omp_reduction_and: // Fortran .and.
  case SgOmpClause::e_omp_reduction_or:  // Fortran .or.
  case SgOmpClause::e_omp_reduction_eqv:
  case SgOmpClause::e_omp_reduction_neqv:
  case SgOmpClause::e_omp_reduction_max:
  case SgOmpClause::e_omp_reduction_min:
  case SgOmpClause::e_omp_reduction_iand:
  case SgOmpClause::e_omp_reduction_ior:
  case SgOmpClause::e_omp_reduction_ieor:
  case SgOmpClause::e_omp_reduction_unknown:
  case SgOmpClause::e_omp_reduction_last:
  default:
    cerr << "Illegal or unhandled reduction operator type:" << r_operator
         << endl;
  }
  SgStatement *reduction_stmt = buildAssignStatement(orig_var_lhs_exp, r_exp);
  end_stmt_list.push_back(reduction_stmt);
  SgExprStatement *atomic_end_stmt =
      buildFunctionCallStmt("__kmpc_atomic_end", buildVoidType(), NULL, bb1);
  end_stmt_list.push_back(atomic_end_stmt);
}

//! Liao 2/12/2013. Insert the thread-block inner level reduction statement into
//! the end of the end_stmt_list
// e.g.  xomp_inner_block_reduction_float (local_error, per_block_error,
// XOMP_REDUCTION_PLUS);
static void insertInnerThreadBlockReduction(
    SgOmpClause::omp_reduction_identifier_enum r_operator,
    vector<SgStatement *> &end_stmt_list, SgBasicBlock *bb1,
    SgInitializedName *orig_var, SgVariableDeclaration *local_decl,
    SgVariableDeclaration *per_block_decl,
    GpuOffloadLoweringContext *offload_ctx) {
  ROSE_ASSERT(bb1 && orig_var && local_decl && per_block_decl);
  ROSE_ASSERT(offload_ctx != NULL);
  // the integer value representing different reduction operations, defined
  // within libxomp.h for accelerator model
  // TODO refactor the code to have a function converting operand types to
  // integers
  int op_value = -1;
  switch (r_operator) {
  case SgOmpClause::e_omp_reduction_plus:
    op_value = 6;
    break;
  case SgOmpClause::e_omp_reduction_minus:
    op_value = 7;
    break;
  case SgOmpClause::e_omp_reduction_mul:
    op_value = 8;
    break;
  case SgOmpClause::e_omp_reduction_bitand:
    op_value = 9;
    break;
  case SgOmpClause::e_omp_reduction_bitor:
    op_value = 10;
    break;
  case SgOmpClause::e_omp_reduction_bitxor:
    op_value = 11;
    break;
  case SgOmpClause::e_omp_reduction_logand:
    op_value = 12;
    break;
  case SgOmpClause::e_omp_reduction_logor:
    op_value = 13;
    break;
    // TODO: more operation types
  case SgOmpClause::e_omp_reduction_and: // Fortran .and.
  case SgOmpClause::e_omp_reduction_or:  // Fortran .or.
  case SgOmpClause::e_omp_reduction_eqv:
  case SgOmpClause::e_omp_reduction_neqv:
  case SgOmpClause::e_omp_reduction_max:
  case SgOmpClause::e_omp_reduction_min:
  case SgOmpClause::e_omp_reduction_iand:
  case SgOmpClause::e_omp_reduction_ior:
  case SgOmpClause::e_omp_reduction_ieor:
  case SgOmpClause::e_omp_reduction_unknown:
  case SgOmpClause::e_omp_reduction_last:
  default:
    cerr << "Error. insertThreadBlockReduction() in omp_lowering.cpp: Illegal "
            "or unhandled reduction operator type:"
         << r_operator << endl;
  }

  SgVariableSymbol *var_sym = getFirstVarSym(per_block_decl);
  ROSE_ASSERT(var_sym != NULL);
  SgPointerType *var_type = isSgPointerType(var_sym->get_type());
  ROSE_ASSERT(var_type != NULL);
  // TODO: this could be risky. It is better to have our own conversion function
  // to have full control over it.
  string type_str = var_type->get_base_type()->unparseToString();
  offload_ctx->per_block_reduction_map[var_sym] =
      op_value; // save the per block symbol and its corresponding reduction
                // integer value defined in the libxomp.h
  SgIntVal *reduction_op = buildIntVal(op_value);
  SgExprListExp *parameter_list = buildExprListExp(
      buildVarRefExp(local_decl), buildVarRefExp(per_block_decl), reduction_op);
  SgStatement *func_call_stmt =
      buildFunctionCallStmt("xomp_inner_block_reduction_" + type_str,
                            buildVoidType(), parameter_list, bb1);
  end_stmt_list.push_back(func_call_stmt);
}
// TODO move to sageInterface advanced transformation ???
//! Generate element-by-element assignment from a right-hand array to left_hand
//! array variable.
//
// e.g.  for int a[M][N], b[M][N],  a=b is implemented as follows:
//
//  int element_count = ...;
//  int *a_ap = (int *)a;
//  int *b_ap = (int *)b;
//  int i;
//  for (i=0;i<element_count; i++)
//    *(b_ap+i) = *(a_ap+i);
//
static SgBasicBlock *
generateArrayAssignmentStatements(SgInitializedName *left_operand,
                                  SgInitializedName *right_operand,
                                  SgScopeStatement *scope) {
  // parameter validation
  ROSE_ASSERT(scope !=
              NULL); // enforce top-down AST construction here for simplicity
  ROSE_ASSERT(left_operand != NULL);
  ROSE_ASSERT(right_operand != NULL);

  SgType *left_type = left_operand->get_type();
  SgType *right_type = right_operand->get_type();
  SgArrayType *left_array_type = isSgArrayType(left_type);
  SgArrayType *right_array_type = isSgArrayType(right_type);

  ROSE_ASSERT(left_array_type != NULL);
  ROSE_ASSERT(right_array_type != NULL);
  // make sure two array are compatible: same dimension, bounds, and element
  // types, etc.
  ROSE_ASSERT(getElementType(left_array_type) ==
              getElementType(right_array_type));
  int dim_count = getDimensionCount(left_array_type);
  ROSE_ASSERT(dim_count == getDimensionCount(right_array_type));
  int element_count = getArrayElementCount(left_array_type);
  ROSE_ASSERT(element_count == (int)getArrayElementCount(right_array_type));

  SgBasicBlock *bb = buildBasicBlock();
  // front_stmt_list.push_back() will handle this later on.
  // Keep this will cause duplicated appendStatement()
  // appendStatement(bb, scope);

  // int *a_ap = (int*) a;
  string right_name = right_operand->get_name().getString();
  string right_name_p = right_name + "_ap"; // array pointer (ap)
  SgType *elementPointerType = buildPointerType(buildIntType());
  SgAssignInitializer *initor = buildAssignInitializer(
      buildCastExp(buildVarRefExp(right_operand, scope), elementPointerType),
      elementPointerType);
  SgVariableDeclaration *decl_right =
      buildVariableDeclaration(right_name_p, elementPointerType, initor, bb);
  appendStatement(decl_right, bb);

  // int *b_ap = (int*) b;
  string left_name = left_operand->get_name().getString();
  string left_name_p = left_name + "_ap";
  SgAssignInitializer *initor2 = buildAssignInitializer(
      buildCastExp(buildVarRefExp(left_operand, scope), elementPointerType),
      elementPointerType);
  SgVariableDeclaration *decl_left =
      buildVariableDeclaration(left_name_p, elementPointerType, initor2, bb);
  appendStatement(decl_left, bb);

  // int i;
  SgVariableDeclaration *decl_i =
      buildVariableDeclaration("_p_i", buildIntType(), NULL, bb);
  appendStatement(decl_i, bb);

  //  for (i=0;i<element_count; i++)
  //    *(b_ap+i) = *(a_ap+i);
  SgStatement *init_stmt =
      buildAssignStatement(buildVarRefExp(decl_i), buildIntVal(0));
  SgStatement *test_stmt = buildExprStatement(
      buildLessThanOp(buildVarRefExp(decl_i), buildIntVal(element_count)));
  SgExpression *incr_exp =
      buildPlusPlusOp(buildVarRefExp(decl_i), SgUnaryOp::postfix);
  SgStatement *loop_body = buildAssignStatement(
      buildPointerDerefExp(
          buildAddOp(buildVarRefExp(decl_left), buildVarRefExp(decl_i))),
      buildPointerDerefExp(
          buildAddOp(buildVarRefExp(decl_right), buildVarRefExp(decl_i))));
  SgForStatement *for_stmt =
      buildForStatement(init_stmt, test_stmt, incr_exp, loop_body);
  appendStatement(for_stmt, bb);

  return bb;
}

// SgBasicBlock * getEnclosingRegionOrFuncDefinition(SgBasicBlock *orig_scope)
SgBasicBlock *getEnclosingRegionOrFuncDefinition(SgNode *orig_scope) {
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  // find the right scope (target body) to insert the declaration, start from
  // the original scope
  SgBasicBlock *t_body = NULL;

  // find enclosing parallel region's body
  SgOmpParallelStatement *omp_stmt = isSgOmpParallelStatement(
      getEnclosingNode<SgOmpParallelStatement>(orig_scope));
  if (omp_stmt) {
    SgBasicBlock *omp_body = isSgBasicBlock(omp_stmt->get_body());
    ROSE_ASSERT(omp_body != NULL);
    t_body = omp_body;
  } else {
    // Find enclosing function body
    SgFunctionDefinition *func_def = getEnclosingProcedure(orig_scope);
    ROSE_ASSERT(func_def != NULL);
    SgBasicBlock *f_body = func_def->get_body();
    ROSE_ASSERT(f_body != NULL);
    t_body = f_body;
  }
  ROSE_ASSERT(t_body != NULL);
  return t_body;
}

//! This is a highly specialized operation which can find the right place to
//! insert a Fortran variable declaration
//  during OpenMP lowering.
//
//  The reasons are:
//    1)Fortran (at least F77) requires declaration statements to be consecutive
//    within an enclosing function definition. The C99-style generation of 'int
//    loop_index' within a SgBasicBlock in the middle of some executable
//    statement is illegal
//     for Fortran. We have to find the enclosing function body, located the
//     declaration sequence, and add the new declaration after it.
//
//    2) When translating OpenMP constructs within a parallel region, the
//    declaration (such as those for private variables of the construct )
//       should be inserted into the declaration part of the body of the
//       parallel region, which will become function body of the outlined
//       function when translating the region later on.
//       Insert the declaration to the current enclosing function definition is
//       not correct.
//
// Liao 1/12/2011
SgVariableDeclaration *
buildAndInsertDeclarationForOmp(const std::string &name, SgType *type,
                                SgInitializer *varInit,
                                SgBasicBlock *orig_scope) {
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  SgVariableDeclaration *result = NULL;

  // find the right scope (target body) to insert the declaration, start from
  // the original scope

  SgBasicBlock *t_body = NULL;

  t_body = getEnclosingRegionOrFuncDefinition(orig_scope);
  // Build the required variable declaration
  result = buildVariableDeclaration(name, type, varInit, t_body);

  // Insert to be the declaration after current declaration sequence, if any
  insertStatementAfterLastDeclaration(result, t_body);
  ROSE_ASSERT(result != NULL);
  return result;
}
//! Translate clauses with variable lists, such as private, firstprivate,
//! lastprivate, reduction, etc.
// bb1 is the affected code block by the clause.
// Command steps are: insert local declarations for the variables:(all)
//                    initialize the local declaration:(firstprivate, reduction)
//                    variable substitution for the variables:(all)
//                    save local copy back to its global one:(reduction,
//                    lastprivate)
//  Note that a variable could be both firstprivate and lastprivate
//  Parameters:
//      ompStmt: the OpenMP statement node with variable clauses
//      bb1: the translation-generated basic block to implement ompStmt
//      orig_loop_upper:
//        if ompStmt is loop construct, pass the original loop upper bound
//        if ompStmt is omp sections, pass the section count - 1
//  This function is later extended to support OpenMP accelerator model. In this
//  model,
//     We have no concept of firstprivate or lastprivate
//     reduction is implemented using a two-level reduction algorithm
static void transOmpVariablesWithContext(
    SgStatement *ompStmt, SgBasicBlock *bb1,
    SgExpression *orig_loop_upper /*= NULL*/,
    bool isAcceleratorModel /*= false*/,
    GpuOffloadLoweringContext *offload_ctx /*= NULL*/) {
  ROSE_ASSERT(ompStmt != NULL);
  ROSE_ASSERT(bb1 != NULL);
  SgOmpClauseBodyStatement *clause_stmt = isSgOmpClauseBodyStatement(ompStmt);
  ROSE_ASSERT(clause_stmt != NULL);

  // collect variables
  SgInitializedNamePtrList var_list = collectAllClauseVariables(clause_stmt);
  // Only keep the unique ones
  sort(var_list.begin(), var_list.end());
  ;
  SgInitializedNamePtrList::iterator new_end =
      unique(var_list.begin(), var_list.end());
  var_list.erase(new_end, var_list.end());
  VariableSymbolMap_t var_map;
  ASTtools::VarSymSet_t var_set;
  std::set<SgVariableSymbol *> scalar_locals_from_pointer_symbols;

  vector<SgStatement *> front_stmt_list, end_stmt_list, front_init_list;

  std::map<std::string, SgVariableSymbol *> visible_symbols_by_name;
  if (const SgFunctionDeclaration *enclosing_decl =
          getEnclosingFunctionDeclaration(bb1)) {
    ASTtools::VarSymSet_t visible_syms;
    ASTtools::collectLocalVisibleVarSyms(enclosing_decl, bb1, visible_syms);
    for (ASTtools::VarSymSet_t::const_iterator i = visible_syms.begin();
         i != visible_syms.end(); ++i) {
      const SgVariableSymbol *sym = *i;
      if (sym == NULL)
        continue;
      const std::string name = sym->get_name().getString();
      if (visible_symbols_by_name.count(name) == 0)
        visible_symbols_by_name[name] = const_cast<SgVariableSymbol *>(sym);
    }
  }

  for (size_t i = 0; i < var_list.size(); i++) {
    SgInitializedName *orig_var = var_list[i];
    ROSE_ASSERT(orig_var != NULL);
    SgVariableSymbol *visible_symbol =
        lookupVariableSymbolInParentScopes(orig_var->get_name(), bb1);
    if (visible_symbol == NULL) {
      std::map<std::string, SgVariableSymbol *>::const_iterator visible_it =
          visible_symbols_by_name.find(orig_var->get_name().getString());
      if (visible_it != visible_symbols_by_name.end())
        visible_symbol = visible_it->second;
    }
    string orig_name = orig_var->get_name().getString();
    SgVariableSymbol *orig_symbol =
        isSgVariableSymbol(orig_var->get_symbol_from_symbol_table());
    if (orig_symbol == NULL) {
      SgScopeStatement *decl_scope = orig_var->get_scope();
      if (decl_scope != NULL)
        orig_symbol = decl_scope->lookup_var_symbol(orig_var->get_name());
    }
    if (orig_symbol == NULL && visible_symbol != NULL)
      orig_symbol = visible_symbol;
    ROSE_ASSERT(orig_symbol != NULL);
    SgVariableSymbol *active_symbol =
        visible_symbol != NULL ? visible_symbol : orig_symbol;
    ROSE_ASSERT(active_symbol != NULL);
    SgType *orig_type = orig_var->get_type();
    if (orig_type == NULL ||
        isSgTypeUnknown(stripTypeAliases(orig_type)) != NULL) {
      SgType *active_type = active_symbol->get_type();
      if (active_type != NULL &&
          isSgTypeUnknown(stripTypeAliases(active_type)) == NULL) {
        orig_type = active_type;
      }
    }
    SgExpression *orig_var_exp = buildVarRefExp(active_symbol);
    if (SgOmpExecStatement *target = isSgOmpExecStatement(clause_stmt)) {
      std::map<SgOmpExecStatement *,
               std::map<SgInitializedName *, SgExpression *> *>::const_iterator
          map_iter = clause_variable_renaming_record.find(target);
      if (map_iter != clause_variable_renaming_record.end() &&
          map_iter->second != NULL) {
        std::map<SgInitializedName *, SgExpression *>::const_iterator var_iter =
            map_iter->second->find(orig_var);
        if (var_iter != map_iter->second->end()) {
          orig_var_exp = copyExpression(var_iter->second);
        }
      }
    }

    VariantVector vvt(V_SgOmpPrivateClause);
    vvt.push_back(V_SgOmpReductionClause);
    vvt.push_back(V_SgOmpFirstprivateClause);

    // TODO: No such concept of firstprivate and lastprivate in accelerator
    // model??
    if (!isAcceleratorModel) // we actually already has enable_accelerator, but
                             // it is too global for handling both CPU and GPU
                             // translation
    {
      vvt.push_back(V_SgOmpLastprivateClause);
    }

    // a local private copy
    SgVariableDeclaration *local_decl = NULL;
    SgOmpClause::omp_reduction_identifier_enum r_operator =
        SgOmpClause::e_omp_reduction_unknown;
    bool isReductionVar =
        isInClauseVariableList(orig_var, clause_stmt, V_SgOmpReductionClause);

    // step 1. Insert local declaration for private, firstprivate, lastprivate
    // and reduction Sara, 5/31/2013: if variable is in Function Scope ( a
    // parameter ) and array, we don't want a private copy, since the only thing
    // private is the pointer, not the pointed data We had a variable passed as
    // private that has to be used as shared We create a pointer to the variable
    // and replace all the occurrences of the variable by the pointer Example:
    // source code:
    // void outlining( int M[10][10] ) {
    //   #pragma omp task firstprivate( M )
    //   M[0][0] = 4;
    // }
    // outlined parameters struct
    // struct OUT__17__7038___data {
    //   int (*M)[10UL];
    // };
    // outlined function:
    // static void OUT__17__7038__(void *__out_argv) {
    //   int (**M)[10UL] = (int (**)[10UL])(&(((struct OUT__17__7038___data
    //   *)__out_argv) -> M));
    //   (*M)[0][0] = 4;
    // }
    if (isInClauseVariableList(orig_var, clause_stmt, vvt)) {
      SgType *effective_type = orig_type;
      if (SgReferenceType *ref_type = isSgReferenceType(orig_type))
        effective_type = ref_type->get_base_type();

      const bool is_function_scope_array =
          isSgArrayType(effective_type) &&
          isSgFunctionDefinition(orig_var->get_scope());
      const bool is_firstprivate = isInClauseVariableList(
          orig_var, clause_stmt, V_SgOmpFirstprivateClause);

      if (!is_function_scope_array) {
        SgInitializer *init = NULL;
        SgExpression *fortran_firstprivate_value = NULL;
        // use copy constructor for firstprivate on C++ class object variables
        // For simplicity, we handle C and C++ scalar variables the same way
        //
        // But here is one exception: an array type firstprivate variable should
        // be initialized element-by-element
        // Liao, 4/12/2010
        if (is_firstprivate && !isSgArrayType(effective_type)) {
          SgExpression *init_value = NULL;
          // Nested task outlining can leave firstprivate clause variables bound
          // to stale declaration types while body references use the visible
          // in-scope symbol. Keep the local firstprivate declaration type and
          // initializer consistent with the active symbol in this specific
          // situation.
          if (isSgOmpTaskStatement(clause_stmt) != NULL &&
              stripTypeAliases(active_symbol->get_type()) !=
                  stripTypeAliases(effective_type)) {
            SgExpression *active_value = NULL;
            if (buildExpressionMatchingTypeFromActiveSymbol(
                    active_symbol, effective_type, active_value)) {
              init_value = active_value;
            } else {
              init_value = copyExpression(orig_var_exp);
            }
          } else {
            init_value = copyExpression(orig_var_exp);
          }

          if (SageInterface::is_Fortran_language()) {
            fortran_firstprivate_value = init_value;
          } else {
            init = buildAssignInitializer(init_value);
          }
        }

        string private_name;
        if (SageInterface::is_Fortran_language()) {
          // leading _ is not allowed in Fortran
          private_name = "i_" + orig_name;
          nCounter++; // Fortran does not have basic block as a scope at source
                      // level
          // I have to generated all declarations at the same flat level under
          // function definitions So a name counter is needed to avoid name
          // collision
          private_name =
              private_name + "_" + StringUtility::numberToString(nCounter);

          // Special handling for variable declarations in Fortran
          local_decl = buildAndInsertDeclarationForOmp(
              private_name, effective_type, init, bb1);
        } else {
          private_name = "_p_" + orig_name;
          local_decl =
              buildVariableDeclaration(private_name, effective_type, init, bb1);
          front_stmt_list.push_back(local_decl);
        }
        // record the map from old to new symbol
        SgVariableSymbol *local_symbol = getFirstVarSym(local_decl);
        ROSE_ASSERT(local_symbol != NULL);

        if (fortran_firstprivate_value != NULL) {
          SgExprStatement *init_stmt = buildAssignStatement(
              buildVarRefExp(local_symbol), fortran_firstprivate_value);
          front_init_list.push_back(init_stmt);
        }

        var_map.insert(
            VariableSymbolMap_t::value_type(active_symbol, local_symbol));
        if (orig_symbol != NULL && orig_symbol != active_symbol)
          var_map.insert(
              VariableSymbolMap_t::value_type(orig_symbol, local_symbol));
        if (isPointerBackedType(active_symbol->get_type()) &&
            isSgPointerType(stripTypeAliases(local_symbol->get_type())) ==
                NULL) {
          scalar_locals_from_pointer_symbols.insert(local_symbol);
        }
      } else if (is_firstprivate && !SageInterface::is_Fortran_language()) {
        // C/C++ function parameters declared as arrays decay to pointers. For
        // firstprivate, create a local pointer copy instead of rewriting uses
        // with an extra dereference, which can create invalid forms such as
        // *(*M) or *(*v2) after outlining.
        SgArrayType *array_type = isSgArrayType(effective_type);
        ROSE_ASSERT(array_type != NULL);
        SgType *local_type = buildPointerType(array_type->get_base_type());
        SgInitializer *init =
            buildAssignInitializer(copyExpression(orig_var_exp));
        string private_name = "_p_" + orig_name;
        local_decl =
            buildVariableDeclaration(private_name, local_type, init, bb1);
        front_stmt_list.push_back(local_decl);
        SgVariableSymbol *local_symbol = getFirstVarSym(local_decl);
        ROSE_ASSERT(local_symbol != NULL);
        var_map.insert(
            VariableSymbolMap_t::value_type(active_symbol, local_symbol));
        if (orig_symbol != NULL && orig_symbol != active_symbol)
          var_map.insert(
              VariableSymbolMap_t::value_type(orig_symbol, local_symbol));
        if (isPointerBackedType(active_symbol->get_type()) &&
            isSgPointerType(stripTypeAliases(local_symbol->get_type())) ==
                NULL) {
          scalar_locals_from_pointer_symbols.insert(local_symbol);
        }
      } else {
        var_set.insert(active_symbol);
        if (orig_symbol != NULL && orig_symbol != active_symbol)
          var_set.insert(orig_symbol);
      }
    }
    // step 2. Initialize the local copy for array-type firstprivate variables
    // TODO copyin, copyprivate
    if (isInClauseVariableList(orig_var, clause_stmt,
                               V_SgOmpFirstprivateClause) &&
        isSgArrayType(orig_type) &&
        !isSgFunctionDefinition(orig_var->get_scope())) {
      SgInitializedName *leftArray = getFirstInitializedName(local_decl);
      SgBasicBlock *arrayAssign =
          generateArrayAssignmentStatements(leftArray, orig_var, bb1);
      front_stmt_list.push_back(arrayAssign);
    }
    if (isReductionVar) // create initial value assignment for the local
                        // reduction variable
    {
      r_operator = getReductionOperationType(orig_var, clause_stmt);
      SgExprStatement *init_stmt = buildAssignStatement(
          buildVarRefExp(local_decl), createInitialValueExp(r_operator));
      if (SageInterface::is_Fortran_language()) {
        // Fortran initialization statements  cannot be interleaved with
        // declaration statements. We save them here and insert them after all
        // declaration statements are inserted.
        front_init_list.push_back(init_stmt);
      } else {
        front_stmt_list.push_back(init_stmt);
      }
    }

    // Liao, 2/12/2013. For an omp for loop within "omp target". We translate
    // its reduction variable by using a two-level reduction method:
    // thread-block level (within kernel) and beyond-block level (done on CPU
    // side). So we have to insert a pointer to the array of per-block reduction
    // results right before its enclosing "omp target" directive The insertion
    // point is decided so that the outliner invoked by transOmpTargetParallel()
    // can later catch this newly introduced variable and handle it in the
    // parameter list properly.
    //
    // e.g. REAL* per_block_results = (REAL *)xomp_deviceMalloc (numBlocks.x*
    // sizeof(REAL));
    SgVariableDeclaration *per_block_decl = NULL;
    if (isReductionVar && isAcceleratorModel) {
      ROSE_ASSERT(offload_ctx != NULL);
      // SgOmpParallelStatement* enclosing_omp_parallel =
      // getEnclosingNode<SgOmpParallelStatement> (ompStmt);
      SgOmpClauseBodyStatement *enclosing_omp_parallel =
          isSgOmpClauseBodyStatement(ompStmt);
      ROSE_ASSERT(enclosing_omp_parallel != NULL);
      // SgScopeStatement* scope_for_insertion =
      // enclosing_omp_target->get_scope();
      SgScopeStatement *scope_for_insertion =
          isSgScopeStatement(enclosing_omp_parallel->get_scope());
      ROSE_ASSERT(scope_for_insertion != NULL);
      SgVarRefExp *num_block_ref =
          buildVarRefExp("_num_blocks_", scope_for_insertion);
      SgExpression *multi_exp =
          buildMultiplyOp(num_block_ref, buildSizeOfOp(orig_type));
      SgExprListExp *parameter_list = buildExprListExp(multi_exp);
      SgExpression *init_exp = buildCastExp(
          buildFunctionCallExp(SgName("malloc"),
                               buildPointerType(buildPointerType(orig_type)),
                               parameter_list, scope_for_insertion),
          buildPointerType(orig_type));
      per_block_decl = buildVariableDeclaration(
          "__reduction_buffer_" + orig_name, buildPointerType(orig_type),
          buildAssignInitializer(init_exp), scope_for_insertion);
      // the prefix of "_dev_per_block_" is important for later handling when
      // calling outliner: add them into the parameter list per_block_decl =
      // buildVariableDeclaration ("_dev_per_block_"+orig_name,
      // buildPointerType(orig_type), buildAssignInitializer(init_exp),
      // scope_for_insertion); this statement refers to _num_blocks_, which will
      // be declared later on when translating "omp parallel" enclosed in "omp
      // target" so we insert it  later when the kernel launch statement is
      // inserted. insertStatementAfter(enclosing_omp_parallel, per_block_decl);
      offload_ctx->per_block_declarations.push_back(per_block_decl);
      // store all reduction variables at the loop level, they will be used
      // later when translating the enclosing "omp target" to help decide on the
      // variables being passed
    }

    // step 3. Save the value back for lastprivate and reduction
    if (isInClauseVariableList(orig_var, clause_stmt,
                               V_SgOmpLastprivateClause)) {
      insertOmpLastprivateCopyBackStmts(ompStmt, end_stmt_list, bb1, orig_var,
                                        local_decl, orig_loop_upper);
    } else if (isReductionVar) {
      // two-level reduction is used for accelerator model
      if (isAcceleratorModel)
        insertInnerThreadBlockReduction(r_operator, end_stmt_list, bb1,
                                        orig_var, local_decl, per_block_decl,
                                        offload_ctx);
      else
        insertOmpReductionCopyBackStmts(r_operator, end_stmt_list, bb1,
                                        orig_var, local_decl, ompStmt);
    }

  } // end for (each variable)

  // step 4. Variable replacement for all original bb1
  replaceVariableReferences(bb1, var_map);
  replaceVariablesWithPointerDereference(
      bb1,
      var_set); // Variables that must be replaced by a pointer to the variable
  normalizeScalarLocalDerefUses(bb1, scalar_locals_from_pointer_symbols);

  // We delay the insertion of declaration, initialization , and save-back
  // statements until variable replacement is done in order to avoid replacing
  // variables of these newly generated statements.
  prependStatementList(front_stmt_list, bb1);
  // Fortran: add initialization statements after all front statements are
  // inserted
  if (SageInterface::is_Fortran_language()) {
    SgBasicBlock *target_bb = getEnclosingRegionOrFuncDefinition(bb1);
    insertStatementAfterLastDeclaration(front_init_list, target_bb);
  } else {
    ROSE_ASSERT(front_init_list.size() == 0);
  }
  appendStatementList(end_stmt_list, bb1);
  // Liao 1/7/2010 , add assertion here, useful when generating outlined
  // functions by moving statements to a function body
  SgStatementPtrList &srcStmts = bb1->get_statements();
  for (SgStatementPtrList::iterator i = srcStmts.begin(); i != srcStmts.end();
       i++) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(*i);
    if (declaration != NULL)
      switch (declaration->variantT()) {
      case V_SgVariableDeclaration: {
        // Reset the scopes on any SgInitializedName objects.
        SgVariableDeclaration *varDecl = isSgVariableDeclaration(declaration);
        bool is_extern_decl =
            varDecl->get_declarationModifier().get_storageModifier().isExtern();
        SgInitializedNamePtrList &l = varDecl->get_variables();
        for (SgInitializedNamePtrList::iterator i = l.begin(); i != l.end();
             i++) {
          // This might be an issue for extern variable declaration that have a
          // scope in a separate namespace of a static class member defined
          // external to its class, etc. I don't want to worry about those cases
          // right now.
          if (!is_extern_decl && (*i)->get_scope() != bb1) {
            (*i)->set_scope(bb1);
          }
          ROSE_ASSERT((*i)->get_scope() == bb1);
        }
        break;
      }

      default:
        break;
      }

  } // end for
} // end void transOmpVariablesWithContext()

void transOmpVariables(SgStatement *ompStmt, SgBasicBlock *bb1,
                       SgExpression *orig_loop_upper /*= NULL*/,
                       bool isAcceleratorModel /*= false*/) {
  transOmpVariablesWithContext(ompStmt, bb1, orig_loop_upper,
                               isAcceleratorModel, NULL);
}

//  if (omp_get_thread_num () == 0)
//     { ... }
//  Or if (XOMP_master())
//     { ...  }
void transOmpMaster(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpMasterStatement *target = isSgOmpMasterStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  bool isLast =
      isLastStatement(target); // check this now before any transformation

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgIfStmt *if_stmt = NULL;
  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(node, scope, &kmpc_global_tid_init);
  SgName tid_name = getFirstVariable(*kmpc_global_tid_declaration).get_name();

  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(
        func_def->get_body(), SgName("__kmpc_master"), buildIntType());
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }

  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }

  SgExprListExp *parameters =
      buildExprListExp(buildIntVal(0), buildVarRefExp(tid_name, scope));
  SgExpression *func_exp =
      buildFunctionCallExp("__kmpc_master", buildIntType(), parameters, scope);
  if (SageInterface::is_Fortran_language()) {
    if_stmt =
        buildIfStmt(buildEqualityOp(func_exp, buildIntVal(1)), body, NULL);
  } else {
    if_stmt = buildIfStmt(func_exp, body, NULL);
  }

  replaceStatement(target, if_stmt, true);
  SgExprListExp *end_parameters =
      buildExprListExp(buildIntVal(0), buildVarRefExp(tid_name, scope));
  SgExprStatement *end_master_call = buildFunctionCallStmt(
      "__kmpc_end_master", buildVoidType(), end_parameters, scope);
  SgBasicBlock *true_body = ensureBasicBlockAsTrueBodyOfIf(if_stmt);
  appendStatement(end_master_call, true_body);
  moveUpPreprocessingInfo(if_stmt, target, PreprocessingInfo::before);
  if (isLast) // the preprocessing info after the last statement may be attached
              // to the inside of its parent scope
  {
    //    cout<<"Found a last stmt. scope is: "<<scope->class_name()<<endl;
    //    dumpPreprocInfo(scope);
    // move preprecessing info. from inside position to an after position
    moveUpPreprocessingInfo(if_stmt, scope, PreprocessingInfo::inside,
                            PreprocessingInfo::after);
  }
}

// Two cases: without or with copyprivate clause
// without it:
//  if (GOMP_single_start ()) //bool GOMP_single_start (void)
//     { ...       }
// with it: TODO
// TODO other clauses
void transOmpSingle(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpSingleStatement *target = isSgOmpSingleStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  // target vs. if_stmt should not share a subtree of AST (the body)
  // We need to disconnect it from old statement (target)
  // Later replaceStement has the logic to move dangling directives. repeated
  // subtree will cause troubles.
  target->set_body(NULL);

  SgIfStmt *if_stmt = NULL;

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(node, scope, &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), scope);
  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }
  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }
  SgExprListExp *single_parameters =
      buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));

  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(
        func_def->get_body(), SgName("__kmpc_single"), buildIntType());
    SgExpression *func_exp = buildFunctionCallExp(
        "__kmpc_single", buildIntType(), single_parameters, scope);
    if_stmt =
        buildIfStmt(buildEqualityOp(func_exp, buildIntVal(1)), body, NULL);
  } else // C/C++
  {
    SgExpression *func_exp = buildFunctionCallExp(
        "__kmpc_single", buildBoolType(), single_parameters, scope);
    if_stmt = buildIfStmt(func_exp, body, NULL);
  }

  replaceStatement(target, if_stmt, true);
  SgBasicBlock *true_body = ensureBasicBlockAsTrueBodyOfIf(if_stmt);
  transOmpVariables(target, true_body);

  SgExprListExp *end_single_parameters =
      buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
  SgExprStatement *end_single_call = buildFunctionCallStmt(
      "__kmpc_end_single", buildVoidType(), end_single_parameters, scope);
  insertStatementAfter(body, end_single_call);

  // handle nowait
  if (!hasClause(target, V_SgOmpNowaitClause)) {
    SgExprListExp *barrier_parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    SgExprStatement *barrier_call = buildFunctionCallStmt(
        "__kmpc_barrier", buildVoidType(), barrier_parameters, scope);
    insertStatementAfter(if_stmt, barrier_call);
  }
}

//! Build a non-reduction variable clause for a given OpenMP directive. It
//! directly returns the clause if the clause already exists
SgOmpVariablesClause *
buildOmpVariableClause(SgOmpClauseBodyStatement *clause_stmt,
                       const VariantT &vt) {
  SgOmpVariablesClause *result = NULL;
  ROSE_ASSERT(clause_stmt != NULL);
  ROSE_ASSERT(vt != V_SgOmpReductionClause);
  Rose_STL_Container<SgOmpClause *> clauses = getClause(clause_stmt, vt);

  if (clauses.size() == 0) {
    switch (vt) {
    case V_SgOmpCopyinClause:
      result = new SgOmpCopyinClause(buildExprListExp());
      break;
    case V_SgOmpCopyprivateClause:
      result = new SgOmpCopyprivateClause(buildExprListExp());
      break;
    case V_SgOmpFirstprivateClause:
      result = new SgOmpFirstprivateClause(buildExprListExp());
      break;
    case V_SgOmpLastprivateClause:
      result = new SgOmpLastprivateClause(
          buildExprListExp(),
          SgOmpClause::e_omp_lastprivate_modifier_unspecified);
      break;
    case V_SgOmpPrivateClause:
      result = new SgOmpPrivateClause(buildExprListExp());
      break;
    case V_SgOmpSharedClause:
      result = new SgOmpSharedClause(buildExprListExp());
      break;
    case V_SgOmpReductionClause:
    default:
      cerr << "Unacceptable clause type in "
              "OmpSupport::buildOmpVariableClause(): "
           << vt << endl;
      ROSE_ABORT();
    }
  } else {
    result = isSgOmpVariablesClause(clauses[0]);
  }
  ROSE_ASSERT(result != NULL);
  setOneSourcePositionForTransformation(result);

  clause_stmt->get_clauses().push_back(result);
  result->set_parent(clause_stmt); // is This right?

  return result;
}

//! Remove one or more clauses of type vt
int removeClause(SgStatement *clause_stmt, const VariantT &vt) {
  ROSE_ASSERT(clause_stmt != NULL);
  ROSE_ASSERT(isSgOmpClauseBodyStatement(clause_stmt) ||
              isSgOmpClauseStatement(clause_stmt));
  SgOmpClausePtrList &clause_list =
      (isSgOmpClauseBodyStatement(clause_stmt))
          ? (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses()
          : (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  std::vector<Rose_STL_Container<SgOmpClause *>::iterator> iter_vec;
  Rose_STL_Container<SgOmpClause *>::iterator iter;
  // collect iterators pointing the matching clauses
  for (iter = clause_list.begin(); iter != clause_list.end(); iter++) {
    SgOmpClause *c_clause = *iter;
    if (c_clause->variantT() == vt)
      iter_vec.push_back(iter);
  }

  // erase them one by one
  std::vector<Rose_STL_Container<SgOmpClause *>::iterator>::reverse_iterator
      r_iter;
  for (r_iter = iter_vec.rbegin(); r_iter != iter_vec.rend(); r_iter++)
    clause_list.erase(*r_iter);
  return iter_vec.size();
}

//! Add a variable into a non-reduction clause of an OpenMP statement, create
//! the clause transparently if it does not exist
void addClauseVariable(SgInitializedName *var,
                       SgOmpClauseBodyStatement *clause_stmt,
                       const VariantT &vt) {
  ROSE_ASSERT(var != NULL);
  ROSE_ASSERT(clause_stmt != NULL);
  ROSE_ASSERT(vt != V_SgOmpReductionClause);
  Rose_STL_Container<SgOmpClause *> clauses = getClause(clause_stmt, vt);
  SgOmpVariablesClause *target_clause = NULL;
  // create the clause if it does not exist
  if (clauses.size() == 0) {
    target_clause = buildOmpVariableClause(clause_stmt, vt);
  } else {
    target_clause = isSgOmpVariablesClause(clauses[0]);
  }
  ROSE_ASSERT(target_clause != NULL);

  // Insert only if the variable is not in the list
  if (!isInClauseVariableList(var, clause_stmt, vt)) {
    target_clause->get_variables()->get_expressions().push_back(
        buildVarRefExp(var));
  }
}

// Patch up private variables for a single OpenMP For or DO loop
// return the number of private variables added.
int patchUpPrivateVariables(SgStatement *omp_loop) {
  int result = 0;
  ROSE_ASSERT(omp_loop != NULL);

  SgOmpDoStatement *do_node = NULL;
  SgOmpClauseBodyStatement *for_node = NULL;
  switch (omp_loop->variantT()) {
  case V_SgOmpDoStatement:
    do_node = isSgOmpDoStatement(omp_loop);
    break;
  case V_SgOmpForStatement:
  case V_SgOmpTargetParallelForStatement:
  case V_SgOmpTargetTeamsDistributeParallelForStatement:
  case V_SgOmpTargetTeamsDistributeStatement:
    for_node = isSgOmpClauseBodyStatement(omp_loop);
    break;
  default:
    MLOG_ERROR_CXX("ompLowering")
        << "Unexpected statement kind in patchUpPrivateVariables(): "
        << omp_loop->sage_class_name();
    ROSE_ABORT();
  }

  if (do_node)
    omp_loop = do_node;
  else
    omp_loop = for_node;

  SgScopeStatement *directive_scope = omp_loop->get_scope();
  ROSE_ASSERT(directive_scope != NULL);
  // Collected nested loops and their indices
  // skip the top level loop?
  Rose_STL_Container<SgNode *> loops;
  if (do_node)
    loops = NodeQuery::querySubTree(do_node->get_body(), V_SgFortranDo);
  else
    loops = NodeQuery::querySubTree(for_node->get_body(), V_SgForStatement);
  // For all loops within the OpenMP loop
  Rose_STL_Container<SgNode *>::iterator loopIter = loops.begin();
  for (; loopIter != loops.end(); loopIter++) {
    SgInitializedName *index_var = getLoopIndexVariable(*loopIter);
    ROSE_ASSERT(index_var != NULL);
    SgVariableSymbol *variable_symbol =
        isSgVariableSymbol(index_var->get_symbol_from_symbol_table());
    ROSE_ASSERT(variable_symbol != NULL);
    SgScopeStatement *var_scope = index_var->get_scope();
    // Only loop index variables declared in higher or the same scopes
    // matter
    if (isAncestor(var_scope, directive_scope) ||
        var_scope == directive_scope) {
      // Grab possible enclosing parallel region
      bool isPrivateInRegion = false;
      SgOmpClauseBodyStatement *omp_stmt = NULL;
      switch (omp_loop->variantT()) {
      case V_SgOmpTargetParallelForStatement:
      case V_SgOmpTargetTeamsDistributeStatement:
      case V_SgOmpTargetTeamsDistributeParallelForStatement:
        omp_stmt = isSgOmpClauseBodyStatement(omp_loop);
        break;
      case V_SgOmpForStatement:
      case V_SgOmpDoStatement:
        omp_stmt = isSgOmpParallelStatement(
            getEnclosingNode<SgOmpParallelStatement>(omp_loop));
        break;
      default:
        ROSE_ABORT();
      }
      // Orphaned omp do/for constructs can be outside an explicit enclosing
      // parallel clause body in the local AST context.
      if (omp_stmt != NULL) {
        isPrivateInRegion = isInClauseVariableList(
            index_var, isSgOmpClauseBodyStatement(omp_stmt),
            V_SgOmpPrivateClause);
      }
      // Keep enclosing parallel region consistent with worksharing default
      // loop-index privatization so outlining does not treat loop indices as
      // shared parameters.
      if (omp_stmt != NULL && !isPrivateInRegion) {
        addClauseVariable(index_var, isSgOmpClauseBodyStatement(omp_stmt),
                          V_SgOmpPrivateClause);
        isPrivateInRegion = true;
        result++;
      }
      // add it into the private variable list only if it is not specified as
      // private in both the loop and region levels.
      if (!isPrivateInRegion &&
          !isInClauseVariableList(index_var,
                                  isSgOmpClauseBodyStatement(omp_loop),
                                  V_SgOmpPrivateClause)) {
        result++;
        addClauseVariable(index_var, isSgOmpClauseBodyStatement(omp_loop),
                          V_SgOmpPrivateClause);
      }
    }

  } // end for loops
  return result;
}

/*
 * Winnie, Handle collapse clause before openmp and openmp accelerator
 * add new variables inserted by SageInterface::loopCollasping() into mapin
 * clause
 *
 * This function passes target for loop of collpase clause and the collapse
 * factor to the function SageInterface::loopCollapse. After return from
 * SageInterface::loopCollapse, this function will insert new
 * variables(generated by loopCollapse()) into map to or map tofrom clause, if
 * the collapse clause comes with target directive.
 *
 */
void transOmpCollapse(SgStatement *node) {

  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  // The OpenMP syntax requires that the omp for pragma is immediately followed
  // by the for loop.
  SgForStatement *for_loop = isSgForStatement(body);

  SgBasicBlock *for_bb = isSgBasicBlock(body);
  if (for_bb) {
    ROSE_ASSERT(for_bb->get_statements().size() == 1);
    for_loop = isSgForStatement(for_bb->get_statements()[0]);
  }

  if (for_loop == NULL)
    return;

  // Keep collapse normalization/canonical checks working in target outlined
  // kernels where induction variables can be represented as pointer
  // dereferences.
  rewritePointerBasedForIndices(for_loop);

  ROSE_ASSERT(getScope(for_loop)->get_parent()->get_parent() != NULL);

  Rose_STL_Container<SgOmpClause *> collapse_clauses =
      getClause(node, V_SgOmpCollapseClause);

  int collapse_factor = atoi(isSgOmpCollapseClause(collapse_clauses[0])
                                 ->get_expression()
                                 ->unparseToString()
                                 .c_str());
  SgExprListExp *new_var_list =
      SageInterface::loopCollapsing(for_loop, collapse_factor);

  // remove the collapse clause
  removeClause(node, V_SgOmpCollapseClause);
  // we need to insert the loop index variable of the collapsed loop into the
  // private() clause
  patchUpPrivateVariables(node);

  /*
   *Winnie, we need to add the new variables into the map in list, if there is a
   *SgOmpTargetStatement
   */
  /*For OmpTarget, we need to create SgOmpMapClause if there is no such clause
   * in the original code. target_stmt, #pragma omp target or, #pragma omp
   * parallel, when is not OmpTarget inside this if condition, ompacc=false
   * means there is no map clause, we need to create one outside this if
   * condition, ompacc=false means, no need to add new variables in the map in
   * clause
   *   TODO: adding the variables into the map() clause is not sufficient.
   *         we have to move the corresponding variable declarations to be in
   * front of the directive containing map().
   */
  SgStatement *target_stmt = isSgStatement(node->get_parent()->get_parent());
  if (isSgOmpTargetStatement(target_stmt)) {
    Rose_STL_Container<SgOmpClause *> map_clauses;
    SgOmpMapClause *map_to = NULL;

    /*get the data clause of this target statement*/
    SgOmpClauseBodyStatement *target_clause_body =
        isSgOmpClauseBodyStatement(target_stmt);

    map_clauses = target_clause_body->get_clauses();
    if (map_clauses.size() == 0) {
      SgOmpTargetDataStatement *target_data_stmt =
          getEnclosingNode<SgOmpTargetDataStatement>(target_stmt);

      target_clause_body = isSgOmpClauseBodyStatement(target_data_stmt);
      map_clauses = target_clause_body->get_clauses();
    }

    assert(map_clauses.size() != 0);

    for (Rose_STL_Container<SgOmpClause *>::const_iterator iter =
             map_clauses.begin();
         iter != map_clauses.end(); iter++) {
      SgOmpMapClause *temp_map_clause = isSgOmpMapClause(*iter);
      if (temp_map_clause !=
          NULL) // Winnie, look for the map(to) or map(tofrom) clause
      {
        SgOmpClause::omp_map_operator_enum map_operator =
            temp_map_clause->get_operation();
        if (map_operator == SgOmpClause::e_omp_map_to ||
            map_operator == SgOmpClause::e_omp_map_present ||
            map_operator == SgOmpClause::e_omp_map_self ||
            map_operator == SgOmpClause::e_omp_map_unknown ||
            map_operator == SgOmpClause::e_omp_map_tofrom) {
          map_to = temp_map_clause;
          break;
        }
      }
    }

    if (map_to == NULL) {
      cerr << "prepare to create a map in clause" << endl;
    }

    if (map_to != NULL) {
      SgExpressionPtrList &mapto_var_list =
          map_to->get_variables()->get_expressions();
      SgExpressionPtrList new_vars = new_var_list->get_expressions();
      for (size_t i = 0; i < new_vars.size(); i++) {
        mapto_var_list.push_back(deepCopy(isSgVarRefExp(new_vars[i])));
      }

      // TODO We also have to move the relevant variable declarations to sit in
      // front of the map() clause Liao 7/9/2014
    }

  } // end if target
} // Winnie, end of loop collapse

bool isInOmpTargetOffloadingFunc(SgNode *node) {
  SgNode *parent = node->get_parent();
  do {
    if (isSgFunctionDeclaration(parent))
      break;
    parent = parent->get_parent();
  } while (parent);

  if (std::find(target_outlined_function_list->begin(),
                target_outlined_function_list->end(),
                parent) != target_outlined_function_list->end())
    return true;
  else
    return false;
}

//! Bottom-up processing AST tree to translate all OpenMP constructs
// the major interface of omp_lowering
// We now operation on scoped OpenMP regions and blocks
//    SgBasicBlock
//      /                   #
//     /                    #
// SgOmpParallelStatement   #
//          \               #
//           \              #
//           SgBasicBlock   #
//               \          #
//                \         #
//                SgOmpParallelStatement
void lower_omp(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  bool saved_case_insensitive =
      SageBuilder::symbol_table_case_insensitive_semantics;
  const bool has_target_offload = hasTargetOffloadConstructs(file);
  if (file->get_Fortran_only())
    SageBuilder::symbol_table_case_insensitive_semantics = true;

  // Liao 12/2/2010, Fortran does not require function prototypes
  if (!SageInterface::is_Fortran_language())
    insertRTLHeaders(file);
  if (!enable_accelerator)
    insertRTLinitAndCleanCode(file);
  if (has_target_offload)
    insertAcceleratorInit(file);

  target_outlined_function_list = new std::vector<SgFunctionDeclaration *>();

  Rose_STL_Container<SgNode *> omp_nodes;
  do {
    omp_nodes.clear();
    // Fix the parent-children relationship between UPIR nodes
    OmpSupport::createOmpStatementTree(file);
    if (cpu_outlined_file != NULL) {
      OmpSupport::createOmpStatementTree(cpu_outlined_file);
    }
    clearOpenMPClauseOriginalExpressionTrees(file);
    if (cpu_outlined_file != NULL) {
      clearOpenMPClauseOriginalExpressionTrees(cpu_outlined_file);
    }
    Rose_STL_Container<SgNode *>::iterator iter;
    // Collect all the OpenMP nodes
    Rose_STL_Container<SgNode *> nodeList =
        NodeQuery::querySubTree(file, V_SgOmpExecStatement);
    nodeList = mergeSgNodeList(
        nodeList, NodeQuery::querySubTree(file, V_SgOmpThreadprivateStatement));
    nodeList = mergeSgNodeList(
        nodeList, NodeQuery::querySubTree(file, V_SgOmpRequiresStatement));
    if (cpu_outlined_file != NULL) {
      nodeList = mergeSgNodeList(
          nodeList,
          NodeQuery::querySubTree(cpu_outlined_file, V_SgOmpExecStatement));
      nodeList = mergeSgNodeList(
          nodeList, NodeQuery::querySubTree(cpu_outlined_file,
                                            V_SgOmpThreadprivateStatement));
      nodeList = mergeSgNodeList(
          nodeList,
          NodeQuery::querySubTree(cpu_outlined_file, V_SgOmpRequiresStatement));
    }
    // Collect all the OpenMP nodes without OpenMP parent
    for (iter = nodeList.begin(); iter != nodeList.end(); iter++) {
      SgOmpExecStatement *omp_node = isSgOmpExecStatement(*iter);
      if (omp_node != NULL) {
        SgOmpExecStatement *omp_parent =
            isSgOmpExecStatement(omp_node->get_omp_parent());
        if (omp_parent == NULL) {
          omp_nodes.push_back(omp_node);
        }
      } else if (isSgOmpRequiresStatement(*iter) != NULL) {
        omp_nodes.push_back(*iter);
      } else if (isSgOmpThreadprivateStatement(*iter) != NULL) {
        omp_nodes.push_back(*iter);
      }
    }

    // Some transformed trees can carry stale OpenMP parent links for orphaned
    // nodes. If no roots are detected but OpenMP nodes still exist, force
    // progress by processing one node and rebuilding the OpenMP tree in the
    // next iteration.
    if (omp_nodes.empty() && !nodeList.empty()) {
      SgStatement *fallback = isSgStatement(nodeList.front());
      ROSE_ASSERT(fallback != NULL);
      MLOG_WARN_CXX("ompLowering")
          << "Recovering from stale OpenMP parent links while lowering "
          << fallback->sage_class_name();
      omp_nodes.push_back(fallback);
    }

    for (iter = omp_nodes.begin(); iter != omp_nodes.end(); iter++) {
      SgStatement *node = isSgStatement(*iter);
      ROSE_ASSERT(node != NULL);

      // check if it is a variant
      bool isVariant = isSgOmpWhenClause(node->get_parent()) ||
                       isSgOmpDefaultClause(node->get_parent());
      if (isVariant) {
        MLOG_ERROR_CXX("ompLowering")
            << "Unexpected variant node in lowering pipeline; expected prior "
            << "metadirective transformation";
        ROSE_ABORT();
      }

      if (!isVariant)
        switch (node->variantT()) {
        case V_SgOmpParallelStatement: {
          // check if this parallel region is under "omp target"
          SgNode *parent = node->get_parent();
          ROSE_ASSERT(parent != NULL);
          if (isSgBasicBlock(parent)) // skip the padding block in between.
            parent = parent->get_parent();
          if (isSgOmpTargetStatement(parent))
            transOmpTargetParallel(node);
          /*
          if (isInOmpTargetOffloadingFunc(node))
            transOmpSpmdInTargetRegion(node);
          */
          else
            transOmpParallel(node);
          break;
        }
        case V_SgOmpSectionsStatement: {
          transOmpSections(node);
          break;
        }

        case V_SgOmpTaskStatement: {
          transOmpTask(node);
          break;
        }
        case V_SgOmpForStatement:
        case V_SgOmpDoStatement: {
          /*Winnie, handle Collapse clause.*/
          if (hasClause(node, V_SgOmpCollapseClause))
            transOmpCollapse(node);

          if (isInOmpTargetOffloadingFunc(node))
            transOmpLoopInTargetRegion(node);
          else
            transOmpLoop(node);

          break;
        }
        case V_SgOmpBarrierStatement: {
          transOmpBarrier(node);
          break;
        }
        case V_SgOmpFlushStatement: {
          transOmpFlush(node);
          break;
        }
        case V_SgOmpAllocateStatement: {
          transOmpAllocate(node);
          break;
        }
        case V_SgOmpRequiresStatement: {
          transOmpRequires(node);
          break;
        }

        case V_SgOmpThreadprivateStatement: {
          transOmpThreadprivate(node);
          break;
        }
        case V_SgOmpTaskwaitStatement: {
          transOmpTaskwait(node);
          break;
        }
        case V_SgOmpSingleStatement: {
          transOmpSingle(node);
          break;
        }
        case V_SgOmpMasterStatement: {
          transOmpMaster(node);
          break;
        }
        case V_SgOmpAtomicStatement: {
          transOmpAtomic(node);
          break;
        }
        case V_SgOmpOrderedStatement: {
          transOmpOrdered(node);
          break;
        }
        case V_SgOmpCriticalStatement: {
          transOmpCritical(node);
          break;
        }
        case V_SgOmpTargetStatement: {
          transOmpTarget(node);
          break;
        }
        case V_SgOmpTargetTeamsStatement: {
          transOmpTargetTeams(node);
          break;
        }
        case V_SgOmpTargetParallelStatement: {
          transOmpTargetParallel(node);
          break;
        }
        case V_SgOmpTargetDataStatement: {
          transOmpTargetData(node);
          break;
        }
        case V_SgOmpTargetUpdateStatement: {
          transOmpTargetUpdate(node);
          break;
        }
        case V_SgOmpTargetTeamsDistributeStatement: {
          transOmpTargetTeamsDistribute(node);
          break;
        }
        case V_SgOmpTargetParallelForStatement: {
          transOmpTargetParallelFor(node);
          break;
        }
        case V_SgOmpTargetTeamsDistributeParallelForStatement: {
          transOmpTargetTeamsDistributeParallelFor(node);
          break;
        }
        case V_SgOmpSimdStatement:
        case V_SgOmpUnrollStatement:
        case V_SgOmpTileStatement: {
          std::vector<SgStatement *> loop_trans_nodes;
          SgStatement *frontier = node;
          while (frontier != NULL) {
            bool is_omp_loop_transformation = false;
            switch (frontier->variantT()) {
            case V_SgOmpSimdStatement:
            case V_SgOmpUnrollStatement:
            case V_SgOmpTileStatement:
              loop_trans_nodes.push_back(frontier);
              is_omp_loop_transformation = true;
              break;
            default:;
            }
            if (is_omp_loop_transformation == false)
              break;

            frontier = isSgOmpBodyStatement(frontier)->get_body();
            // skip basic blocks if any
            SgBasicBlock *body = isSgBasicBlock(frontier);
            while (body != NULL) {
              const SgStatementPtrList &bb_statements = body->get_statements();
              if (bb_statements.size() == 1) {
                body = isSgBasicBlock(bb_statements[0]);
                frontier = bb_statements[0];
              } else {
                frontier = NULL;
                break;
              }
            }
          }
          for (int i = loop_trans_nodes.size() - 1; i >= 0; i--) {
            switch (loop_trans_nodes[i]->variantT()) {
            case V_SgOmpSimdStatement:
              if (hasClause(loop_trans_nodes[i], V_SgOmpCollapseClause))
                transOmpCollapse(loop_trans_nodes[i]);
              transOmpSimd(loop_trans_nodes[i]);
              break;
            case V_SgOmpUnrollStatement:
              transOmpUnroll(loop_trans_nodes[i]);
              break;
            case V_SgOmpTileStatement:
              transOmpTile(loop_trans_nodes[i]);
              break;
            default:
              // Only specific OpenMP loop transformation directives are handled
              break;
            }
          }
          break;
        }
        default: {
          MLOG_ERROR_CXX("ompLowering")
              << "Unexpected OpenMP construct in lowering pass: "
              << node->sage_class_name();
          ROSE_ABORT();
        }
        } // switch
    }
  } while (omp_nodes.size() != 0);

  if (file->get_Fortran_only()) {
    normalize_fortran_if_statements(file);
    Rose_STL_Container<SgNode *> scopes =
        NodeQuery::querySubTree(file, V_SgScopeStatement);
    for (Rose_STL_Container<SgNode *>::const_iterator it = scopes.begin();
         it != scopes.end(); ++it) {
      SgScopeStatement *scope = isSgScopeStatement(*it);
      ROSE_ASSERT(scope != NULL);
      if (!scope->isCaseInsensitive() && scope->symbol_table_size() == 0)
        scope->setCaseInsensitive(true);
    }
  }

  // post processing
  post_processing(file);
  SageBuilder::symbol_table_case_insensitive_semantics = saved_case_insensitive;
}

} // namespace OmpSupport

// global_tid is required as a parameter in many kmpc function calls
// we always use the function "__kmpc_global_thread_num" to get the global_tid.
// each OpenMP statement has such an id with unique name
// "__global_tid_<enclosing function name>_<original statement line number>_<tid
// index>"
static std::string sanitize_identifier_component(const std::string &name) {
  std::string result;
  result.reserve(name.size());
  for (char c : name) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == '_')
      result.push_back(c);
    else
      result.push_back('_');
  }

  if (result.empty())
    return std::string("scope");

  if (std::isdigit(static_cast<unsigned char>(result[0])))
    result.insert(result.begin(), '_');

  return result;
}

static SgVariableDeclaration *get_kmpc_global_tid(SgNode *target,
                                                  SgScopeStatement *scope,
                                                  SgStatement **init_stmt) {

  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      sanitize_identifier_component(enclosing_function->get_name().getString());
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  std::stringstream kmpc_global_tid_number;
  kmpc_global_tid_number << kmpc_global_tid_counter;
  kmpc_global_tid_counter += 1;
  std::string kmpc_tid_name = "__global_tid_" + enclosing_function_name + "_" +
                              statement_line_number.str() + "_" +
                              kmpc_global_tid_number.str();
  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(func_def->get_body(),
                                        SgName("__kmpc_global_thread_num"),
                                        buildIntType());
  }
  SgExprStatement *global_tid_statement =
      buildFunctionCallStmt("__kmpc_global_thread_num", buildIntType(),
                            buildExprListExp(buildIntVal(0)), scope);
  SgExpression *get_thread_global_tid = global_tid_statement->get_expression();
  SgVariableDeclaration *kmpc_tid_declaration = NULL;
  if (SageInterface::is_Fortran_language()) {
    kmpc_tid_declaration = buildVariableDeclaration(
        SgName(kmpc_tid_name), buildIntType(), NULL, scope);
    if (init_stmt != NULL)
      *init_stmt = buildAssignStatement(
          buildVarRefExp(getFirstVariable(*kmpc_tid_declaration).get_name(),
                         scope),
          copyExpression(get_thread_global_tid));
  } else {
    kmpc_tid_declaration = buildVariableDeclaration(
        SgName(kmpc_tid_name), buildIntType(),
        buildAssignInitializer(get_thread_global_tid), scope);
    if (init_stmt != NULL)
      *init_stmt = NULL;
  }

  return kmpc_tid_declaration;
}

static bool has_fortran_variable_declaration(SgBasicBlock *body,
                                             const SgName &name) {
  ROSE_ASSERT(body != NULL);
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgVariableDeclaration *decl = isSgVariableDeclaration(*it);
    if (decl == NULL)
      continue;
    const SgInitializedNamePtrList &vars = decl->get_variables();
    for (SgInitializedNamePtrList::const_iterator vit = vars.begin();
         vit != vars.end(); ++vit) {
      if ((*vit)->get_name() == name)
        return true;
    }
  }
  return false;
}

static SgProcedureHeaderStatement *
find_fortran_procedure_declaration(SgBasicBlock *body, const SgName &name) {
  ROSE_ASSERT(body != NULL);
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgProcedureHeaderStatement *proc = isSgProcedureHeaderStatement(*it);
    if (proc != NULL && proc->get_name() == name)
      return proc;
  }
  return NULL;
}

static bool is_fortran_data_specification_statement(const SgStatement *stmt) {
  // Some ROSE trees represent DATA as dedicated nodes, others attach DATA
  // groups under SgAttributeSpecificationStatement.
  if (std::string(stmt->sage_class_name()) == "SgDataStatement")
    return true;

  const SgAttributeSpecificationStatement *attr_spec =
      isSgAttributeSpecificationStatement(stmt);
  if (attr_spec == NULL)
    return false;

  if (attr_spec->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_dataStatement)
    return true;

  return !attr_spec->get_data_statement_group_list().empty();
}

static SgStatement *
find_fortran_specification_insertion_anchor(SgBasicBlock *body) {
  ROSE_ASSERT(body != NULL);
  SgStatement *anchor = NULL;
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgStatement *stmt = *it;
    ROSE_ASSERT(stmt != NULL);

    // Declarations and COMMON must remain in the specification part and before
    // DATA statements or internal subprogram definitions.
    if (is_fortran_data_specification_statement(stmt))
      break;
    if (SgProcedureHeaderStatement *proc = isSgProcedureHeaderStatement(stmt)) {
      if (proc->get_definition() != NULL)
        break;
    }
    if (!isSgDeclarationStatement(stmt))
      break;

    anchor = stmt;
  }
  return anchor;
}

static void insert_fortran_statement_in_specification_part(SgStatement *stmt,
                                                           SgBasicBlock *body) {
  ROSE_ASSERT(stmt != NULL);
  ROSE_ASSERT(body != NULL);

  SgStatement *anchor = find_fortran_specification_insertion_anchor(body);
  if (anchor != NULL)
    insertStatementAfter(anchor, stmt);
  else
    prependStatement(stmt, body);
}

static void ensure_fortran_variable_declaration(SgBasicBlock *body,
                                                const SgName &name,
                                                SgType *type) {
  ROSE_ASSERT(body != NULL);
  ROSE_ASSERT(type != NULL);
  if (has_fortran_variable_declaration(body, name))
    return;

  SgProcedureHeaderStatement *proc =
      find_fortran_procedure_declaration(body, name);
  if (proc != NULL) {
    if (proc->get_subprogram_kind() ==
        SgProcedureHeaderStatement::e_function_subprogram_kind)
      return;

    fprintf(stderr,
            "REX OpenMP lowering: conflicting Fortran declaration for '%s' "
            "(existing subroutine declaration)\n",
            name.getString().c_str());
    ROSE_ABORT();
  }

  SgVariableDeclaration *decl =
      buildVariableDeclaration(name, type, NULL, body);
  insert_fortran_statement_in_specification_part(decl, body);
}

static void
insert_fortran_declaration_into_procedure(SgVariableDeclaration *decl,
                                          SgScopeStatement *scope) {
  ROSE_ASSERT(decl != NULL);
  ROSE_ASSERT(scope != NULL);
  SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
  ROSE_ASSERT(func_def != NULL);
  SgBasicBlock *func_body = func_def->get_body();
  ROSE_ASSERT(func_body != NULL);

  insert_fortran_statement_in_specification_part(decl, func_body);
}

static void
normalize_fortran_external_subroutine_declarations(SgBasicBlock *body) {
  ROSE_ASSERT(body != NULL);
  std::vector<SgProcedureHeaderStatement *> declarations;
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgProcedureHeaderStatement *proc = isSgProcedureHeaderStatement(*it);
    if (proc == NULL)
      continue;
    if (proc->get_definition() != NULL)
      continue;
    if (proc->get_subprogram_kind() !=
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind)
      continue;
    declarations.push_back(proc);
  }

  for (std::vector<SgProcedureHeaderStatement *>::const_iterator it =
           declarations.begin();
       it != declarations.end(); ++it) {
    SgProcedureHeaderStatement *proc = *it;
    // These nondefining subroutine declarations are outlining artifacts.
    // Keeping them changes procedure binding semantics (e.g., forcing CALL
    // abort to resolve as abort_) and can also emit invalid declaration forms
    // in fixed-form Fortran. Drop them and preserve the original unit's
    // implicit procedure resolution.
    SageInterface::removeStatement(proc, true);
  }
}

static void normalize_fortran_if_statements(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  Rose_STL_Container<SgNode *> if_nodes =
      NodeQuery::querySubTree(file, V_SgIfStmt);
  for (Rose_STL_Container<SgNode *>::const_iterator it = if_nodes.begin();
       it != if_nodes.end(); ++it) {
    SgIfStmt *if_stmt = isSgIfStmt(*it);
    ROSE_ASSERT(if_stmt != NULL);

    SgStatement *true_body = if_stmt->get_true_body();
    ROSE_ASSERT(true_body != NULL);
    if (!isSgBasicBlock(true_body)) {
      SgBasicBlock *wrapped_true = buildBasicBlock();
      appendStatement(true_body, wrapped_true);
      if_stmt->set_true_body(wrapped_true);
      wrapped_true->set_parent(if_stmt);
    }

    SgStatement *false_body = if_stmt->get_false_body();
    if (false_body != NULL && !isSgBasicBlock(false_body) &&
        !isSgIfStmt(false_body)) {
      SgBasicBlock *wrapped_false = buildBasicBlock();
      appendStatement(false_body, wrapped_false);
      if_stmt->set_false_body(wrapped_false);
      wrapped_false->set_parent(if_stmt);
    }

    if_stmt->set_use_then_keyword(true);
    if (!isSgIfStmt(if_stmt->get_false_body()))
      if_stmt->set_has_end_statement(true);
  }
}

// insert a parameter to the outlined function
// it doesn't affect the forward declaration but the definition itself
// please use it before inserting the forward declaration
static void insert_function_parameter(std::string name, SgType *parameter_type,
                                      SgFunctionDeclaration *function,
                                      bool to_append) {

  // prepare the parameter
  SgName parameter_name(name);
  SgFunctionParameterList *params = function->get_parameterList();
  SgInitializedName *parameter =
      SageBuilder::buildInitializedName(parameter_name, parameter_type);
  setOneSourcePositionForTransformation(parameter);

  // insert the parameter at the end or the beginning
  if (to_append) {
    appendArg(params, parameter);
  } else {
    prependArg(params, parameter);
  };

  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = function->get_definition();
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(func_def->get_body(), parameter_name,
                                        parameter_type);
  }

  // update the function metadata
  SgType *stale_func_type = function->get_type();
  function->set_type(buildFunctionType(
      function->get_type()->get_return_type(),
      buildFunctionParameterTypeList(function->get_parameterList())));
  SgFunctionDeclaration *non_def_func =
      isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
  ROSE_ASSERT(non_def_func != NULL);
  ROSE_ASSERT(stale_func_type == non_def_func->get_type());
  non_def_func->set_type(function->get_type());
}

static SgFunctionDeclaration *
move_outlined_function(SgFunctionDeclaration *outlined_func,
                       SgSourceFile *new_file) {

  // prepare the required information of original file
  SgGlobal *original_scope = getGlobalScope(outlined_func);
  std::string original_name = outlined_func->get_name().getString();
  SgFile *cur_file = getEnclosingNode<SgFile>(outlined_func);
  std::string original_file_name = StringUtility::stripFileSuffixFromFileName(
      StringUtility::stripPathFromFileName(
          cur_file->get_file_info()->get_filenameString()));

  // prepare the required information of new file
  SgGlobal *new_scope = new_file->get_globalScope();

  // copy the outlined function to the new file and remove the static modifier
  SgFunctionDeclaration *new_outlined_function =
      isSgFunctionDeclaration(deepCopy(outlined_func));
  new_outlined_function->get_declarationModifier()
      .get_storageModifier()
      .setUnspecified();
  new_outlined_function->set_scope(new_scope);
  SageInterface::fixVariableReferences(new_file, false);
  appendStatement(new_outlined_function, new_scope);

  // set the function declaration in the original file as extern
  SgFunctionDeclaration *extern_header =
      isSgFunctionDeclaration(findFunctionDeclaration(
          original_scope->get_parent(), original_name, original_scope, false));
  extern_header->get_declarationModifier().get_storageModifier().setExtern();

  // remove the outlined function in the original file and perform post
  // processing later once the outlined-file transformations are complete
  removeStatement(outlined_func);
  return new_outlined_function;
}

static SgSourceFile *
generate_outlined_function_file(SgFunctionDeclaration *outlined_func,
                                std::string file_extension) {

  // prepare the required information of original file
  std::string original_name = outlined_func->get_name().getString();
  SgBasicBlock *function_block = outlined_func->get_definition()->get_body();
  SgSourceFile *new_file = NULL;
  SgFile *cur_file = getEnclosingNode<SgFile>(outlined_func);
  std::string original_file_name = StringUtility::stripFileSuffixFromFileName(
      StringUtility::stripPathFromFileName(
          cur_file->get_file_info()->get_filenameString()));
  if (file_extension == "") {
    file_extension = StringUtility::fileNameSuffix(
        cur_file->get_file_info()->get_filenameString());
  };

  // create a new file with all the function declaration and preprocessing
  // information of the original file
  new_file = Outliner::getLibSourceFile(function_block);
  ROSE_ASSERT(new_file != NULL);
  // reset the name of new outlined function file
  std::string new_file_name =
      "rex_lib_" + original_file_name + "." + file_extension;
  new_file->get_file_info()->set_filenameString(new_file_name);
  new_file->set_unparse_output_filename(new_file_name);
  // Outlined files are synthesized/renamed after parsing, so token-stream
  // mappings from the original source are not valid for them.
  new_file->set_unparse_tokens(false);

  // insert REX runtime header to the new file (C/C++ only)
  SgGlobal *new_scope = new_file->get_globalScope();
  bool inserted_header = false;
  if (!new_file->get_Fortran_only()) {
    if (file_extension == "cu") {
      SageInterface::insertHeader(new_file, "rex_nvidia.h",
                                  /*isSystemHeader=*/false,
                                  /*asLastHeader=*/true);
      inserted_header = true;
    } else {
      SageInterface::insertHeader(new_file, "rex_kmp.h",
                                  /*isSystemHeader=*/false,
                                  /*asLastHeader=*/true);
      inserted_header = true;
    }
  }
  if (inserted_header) {
    new_file->set_processedToIncludeCppDirectivesAndComments(true);
  }

  if (file_extension == "cu") {
    rewriteCudaSiblingIncludesInOutlinedFile(
        new_file,
        std::filesystem::path(cur_file->get_file_info()->get_filenameString()));
  }

  fix_storage_modifier(new_file);

  return new_file;
}

static void fix_storage_modifier(SgSourceFile *new_file) {
  // set the regular global variables in the new file to extern and remove their
  // definition
  Rose_STL_Container<SgNode *> global_variable_list =
      NodeQuery::querySubTree(new_file, V_SgVariableDeclaration);
  Rose_STL_Container<SgNode *>::iterator global_variable_list_iterator;
  for (global_variable_list_iterator = global_variable_list.begin();
       global_variable_list_iterator != global_variable_list.end();
       global_variable_list_iterator++) {
    SgVariableDeclaration *global_variable =
        isSgVariableDeclaration(*global_variable_list_iterator);
    if (isSgGlobal(global_variable->get_scope())) {
      SgStorageModifier &variable_modifier =
          global_variable->get_declarationModifier().get_storageModifier();
      if (!variable_modifier.isStatic()) {
        variable_modifier.setExtern();
        global_variable->reset_initializer(NULL);
      };
    };
  };
};

static void post_processing(SgSourceFile *file) {
  SgSourceFile *new_file = NULL;

  // handle the outlined functions for NVIDIA GPU
  if (target_outlined_function_list->size() > 0) {
    // create a new file
    new_file = generate_outlined_function_file(
        target_outlined_function_list->at(0), "cu");
    SgGlobal *new_scope = new_file->get_globalScope();
    SgFile *cur_file =
        getEnclosingNode<SgFile>(target_outlined_function_list->at(0));
    std::string file_extension = StringUtility::fileNameSuffix(
        cur_file->get_file_info()->get_filenameString());

    if (CommandlineProcessing::isCFileNameSuffix(file_extension) ||
        CommandlineProcessing::isCppFileNameSuffix(file_extension)) {
      PreprocessingInfo *c_linkage_start = new PreprocessingInfo(
          PreprocessingInfo::ClinkageSpecificationStart,
          "#ifdef __cplusplus\nextern \"C\" {\n#endif",
          "Transformation generated", 0, 0, 0, PreprocessingInfo::after);
      SageInterface::insertHeader(new_scope->lastStatement(), c_linkage_start,
                                  1);
    };

    // move the outlined functions
    std::vector<SgFunctionDeclaration *>::reverse_iterator i;
    for (i = target_outlined_function_list->rbegin();
         i != target_outlined_function_list->rend(); i++) {
      // set up an omp target parameter for each generated CUDA kernel
      // the naming pattern is "<kernel name>_exec_mode"
      SgVariableDeclaration *kernel_exec_mode_decl = buildVariableDeclaration(
          (*i)->get_name().getString() + "_exec_mode", buildCharType(),
          buildAssignInitializer(buildIntVal(0)), new_scope);
      SgStorageModifier &kernel_exec_mode_modifier =
          kernel_exec_mode_decl->get_declarationModifier()
              .get_storageModifier();
      kernel_exec_mode_modifier.setCudaGlobal();
      appendStatement(kernel_exec_mode_decl, new_scope);

      move_outlined_function(*i, new_file);
    };

    if (CommandlineProcessing::isCFileNameSuffix(file_extension) ||
        CommandlineProcessing::isCppFileNameSuffix(file_extension)) {
      PreprocessingInfo *c_linkage_end = new PreprocessingInfo(
          PreprocessingInfo::ClinkageSpecificationEnd,
          "#ifdef __cplusplus\n}\n#endif", "Transformation generated", 0, 0, 0,
          PreprocessingInfo::after);
      SageInterface::insertHeader(new_scope->lastStatement(), c_linkage_end, 1);
    };
  };

  if (new_file != NULL) {
    removeOpenMPPragmaDeclarations(new_file);
    if (new_file->get_Fortran_only())
      removeOpenMPDirectivePreprocessingInfo(new_file);
    removeUnbalancedConditionalDirectives(new_file);
    AstPostProcessing(new_file);
  };
  removeOpenMPPragmaDeclarations(file);
  if (file->get_Fortran_only())
    removeOpenMPDirectivePreprocessingInfo(file);
  removeUnbalancedConditionalDirectives(file);
  AstPostProcessing(file);
};
