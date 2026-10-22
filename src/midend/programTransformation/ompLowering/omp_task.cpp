
#include "Outliner.hh"

#include "omp_lowering.h"

#include "libxomp.h"

#include "sage3basic.h"

#include "abiStuff.h"

#include "sageBuilder.h"

#include <string>

using namespace std;
using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

static bool equals_ignore_case(const std::string &lhs, const std::string &rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
        std::tolower(static_cast<unsigned char>(rhs[i])))
      return false;
  }
  return true;
}

static bool has_fortran_external_decl(SgBasicBlock *body,
                                      const std::string &name) {
  ROSE_ASSERT(body != NULL);
  for (SgStatement *stmt : body->get_statements()) {
    SgAttributeSpecificationStatement *attr =
        isSgAttributeSpecificationStatement(stmt);
    if (attr == NULL ||
        attr->get_attribute_kind() !=
            SgAttributeSpecificationStatement::e_externalStatement)
      continue;

    SgExprListExp *parameter_list = attr->get_parameter_list();
    if (parameter_list == NULL)
      continue;

    for (SgExpression *expr : parameter_list->get_expressions()) {
      std::string symbol_name;
      if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
        ROSE_ASSERT(func_ref->get_symbol() != NULL);
        symbol_name = func_ref->get_symbol()->get_name().getString();
      } else if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
        ROSE_ASSERT(var_ref->get_symbol() != NULL);
        symbol_name = var_ref->get_symbol()->get_name().getString();
      } else {
        continue;
      }

      if (equals_ignore_case(symbol_name, name))
        return true;
    }
  }

  return false;
}

static void append_fortran_external_decl(SgBasicBlock *body,
                                         SgFunctionDeclaration *func_decl) {
  ROSE_ASSERT(body != NULL);
  ROSE_ASSERT(func_decl != NULL);

  std::string func_name = func_decl->get_name().getString();
  if (has_fortran_external_decl(body, func_name))
    return;

  SgAttributeSpecificationStatement *external_stmt =
      buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_externalStatement);
  SgFunctionRefExp *func_ref = buildFortranOutlinedFunctionRef(func_decl);
  external_stmt->get_parameter_list()->prepend_expression(func_ref);
  func_ref->set_parent(external_stmt->get_parameter_list());

  SgStatement *last_decl = findLastDeclarationStatement(body);
  if (last_decl != NULL)
    insertStatementAfter(last_decl, external_stmt);
  else
    prependStatement(external_stmt, body);
}

static void insert_libxompf_h_for_task(SgNode *start_node) {
  ROSE_ASSERT(start_node != NULL);
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  ROSE_ASSERT(isSgFunctionDefinition(start_node) == NULL);

  SgBasicBlock *body = getEnclosingRegionOrFuncDefinition(start_node);
  ROSE_ASSERT(body != NULL);

  SgStatement *existing_include = NULL;
  for (SgStatement *stmt : body->get_statements()) {
    SgFortranIncludeLine *f_inc = isSgFortranIncludeLine(stmt);
    if (f_inc == NULL)
      continue;

    std::string include_name =
        StringUtility::stripPathFromFileName(f_inc->get_filename());
    if (include_name == "libxompf.fh" || include_name == "libxompf.h") {
      existing_include = f_inc;
      break;
    }
  }

  if (existing_include == NULL) {
    SgFortranIncludeLine *inc_line = buildFortranIncludeLine("libxompf.fh");
    SgStatement *last_decl = findLastDeclarationStatement(body);
    if (last_decl != NULL)
      insertStatementAfter(last_decl, inc_line);
    else
      prependStatement(inc_line, body);
  }
}

static SgExpression *buildTaskIfClause(SgOmpTaskStatement *target) {
  const auto clauses = getClause(target, V_SgOmpIfClause);
  if (clauses.empty())
    return buildIntVal(1);
  ROSE_ASSERT(clauses.size() == 1);
  auto *clause = isSgOmpIfClause(clauses.front());
  ROSE_ASSERT(clause != nullptr);
  ROSE_ASSERT(clause->get_expression() != nullptr);
  return copyExpression(clause->get_expression());
}

static void transOmpTaskForFortran(SgOmpTaskStatement *target) {
  ROSE_ASSERT(target != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  const unsigned pointer_bits = requireTargetDefaultPointerSizeBits(target);
  ROSE_ASSERT(pointer_bits % 8 == 0);
  const int pointer_size = static_cast<int>(pointer_bits / 8);

  insert_libxompf_h_for_task(target);

  AttachedPreprocessingInfoType save_buf1, save_buf2;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  std::string wrapper_name;
  ASTtools::VarSymSet_t syms;
  ASTtools::VarSymSet_t pdSyms3;
  SgFunctionDeclaration *outlined_func =
      generateOutlinedTask(target, wrapper_name, syms, pdSyms3, false);
  ROSE_ASSERT(outlined_func != NULL);

  SgBasicBlock *enclosing_body = getEnclosingRegionOrFuncDefinition(target);
  ROSE_ASSERT(enclosing_body != NULL);
  append_fortran_external_decl(enclosing_body, outlined_func);

  SgScopeStatement *task_scope = target->get_scope();
  ROSE_ASSERT(task_scope != NULL);

  if (syms.size() > MAX_OUTLINED_FUNC_PARAMETER_COUNT) {
    MLOG_ERROR_CXX("ompLowering")
        << "transOmpTaskForFortran(): outlined task exceeds the runtime "
           "parameter limit";
    ROSE_ABORT();
  }

  SgExpression *parameter_if_clause = buildTaskIfClause(target);

  SgExpression *parameter_untied =
      hasClause(target, V_SgOmpUntiedClause) ? buildIntVal(1) : buildIntVal(0);

  SgExprListExp *parameters =
      buildExprListExp(buildFortranOutlinedFunctionRef(outlined_func),
                       parameter_if_clause, parameter_untied);
  appendExpression(parameters, buildIntVal(static_cast<int>(syms.size() * 3)));

  for (ASTtools::VarSymSet_t::iterator iter = syms.begin(); iter != syms.end();
       ++iter) {
    const SgVariableSymbol *sym = *iter;
    ROSE_ASSERT(sym != NULL);
    ROSE_ASSERT(sym->get_declaration() != NULL);

    const bool pass_by_value = pdSyms3.count(sym) == 0;
    appendExpression(parameters, buildIntVal(pass_by_value ? 1 : 0));

    if (pass_by_value) {
      const unsigned size_bits =
          requireTargetScalarSizeBits(sym->get_type(), target);
      ROSE_ASSERT(size_bits % 8 == 0);
      appendExpression(parameters,
                       buildIntVal(static_cast<int>(size_bits / 8)));
    } else {
      appendExpression(parameters, buildIntVal(pointer_size));
    }

    appendExpression(parameters,
                     buildVarRefExp(const_cast<SgVariableSymbol *>(sym)));
  }

  SgExprStatement *task_call = buildFunctionCallStmt(
      "xomp_task", buildVoidType(), parameters, task_scope);
  SageInterface::replaceStatement(target, task_call, true);

  pastePreprocessingInfo(task_call, PreprocessingInfo::before, save_buf1);
  pastePreprocessingInfo(task_call, PreprocessingInfo::after, save_buf2);
}

static SgFunctionDeclaration *
buildTaskCaptureCleanup(SgFunctionDeclaration *outlined, SgType *capture_type,
                        SgStatement *task) {
  SgScopeStatement *scope = outlined->get_scope();
  SgNode *anchor = task;
  while (anchor->get_parent() != scope) {
    anchor = anchor->get_parent();
    ROSE_ASSERT(anchor != nullptr);
  }
  ROSE_ASSERT(isSgStatement(anchor) != nullptr);
  SgType *parameter_type = buildPointerType(buildVoidType());
  const SgName name(outlined->get_name().getString() + "_destroy");
  auto *parameters = buildFunctionParameterList(
      buildInitializedName("capture", parameter_type));
  auto *canonical_parameters = buildSemanticFunctionParameterList(parameters);
  auto *canonical = buildNondefiningFunctionDeclaration(
      function_declaration_ownership::semanticAuxiliary(), name,
      buildVoidType(), canonical_parameters, scope);
  auto *cleanup = buildDefiningFunctionDeclaration(
      function_declaration_ownership::sourceLexicalBefore(
          scope, isSgStatement(anchor)),
      name, buildVoidType(), parameters, scope, false, canonical, nullptr,
      false);
  auto *body = cleanup->get_definition()->get_body();
  auto *capture = buildVarRefExp(parameters->get_args().front(), body);
  auto *typed_capture =
      buildCastExp(capture, capture_type, SgCastExp::e_static_cast);
  appendStatement(
      buildExprStatement(buildDeleteExp(typed_capture, false, false, nullptr)),
      body);
  return cleanup;
}

static void transOmpTaskForC(SgOmpTaskStatement *target) {
  ROSE_ASSERT(target != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  AttachedPreprocessingInfoType save_buf1, save_buf2;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  std::string wrapper_name;
  ASTtools::VarSymSet_t syms;
  ASTtools::VarSymSet_t pdSyms3;
  SgFunctionDeclaration *outlined_func =
      generateOutlinedTask(target, wrapper_name, syms, pdSyms3, false);
  ROSE_ASSERT(outlined_func != NULL);

  SgScopeStatement *task_scope = target->get_scope();
  ROSE_ASSERT(task_scope != NULL);

  SgExpression *parameter_data = NULL;
  SgExpression *parameter_arg_size = NULL;
  SgExpression *parameter_arg_align = NULL;
  size_t parameter_count = syms.size();
  const bool owned_capture =
      SageInterface::is_Cxx_language() && parameter_count != 0;
  SgFunctionDeclaration *capture_cleanup = nullptr;
  if (parameter_count == 0) {
    parameter_data = buildIntVal(0);
    parameter_arg_size = buildIntVal(0);
    parameter_arg_align = buildIntVal(0);
  } else {
    SgVarRefExp *data_ref = buildVarRefExp(wrapper_name, task_scope);
    ROSE_ASSERT(data_ref != NULL);
    if (owned_capture) {
      ROSE_ASSERT(isSgPointerType(data_ref->get_type()) != nullptr);
      parameter_data = data_ref;
      capture_cleanup =
          buildTaskCaptureCleanup(outlined_func, data_ref->get_type(), target);
    } else {
      parameter_data = buildAddressOfOp(
          data_ref, buildAddressOfResultType(data_ref->get_type()),
          SgAddressOfOp::e_address_operand_object);
      parameter_arg_size =
          buildSizeOfOp(data_ref->get_type(),
                        SageInterface::requireTargetSizeType(task_scope));
      parameter_arg_align =
          buildAlignOfOp(data_ref->get_type(),
                         SageInterface::requireTargetSizeType(task_scope));
    }
  }

  SgExpression *parameter_if_clause = buildTaskIfClause(target);

  SgExpression *parameter_untied =
      hasClause(target, V_SgOmpUntiedClause) ? buildIntVal(1) : buildIntVal(0);

  SgExprStatement *task_call;
  if (owned_capture) {
    auto *parameters =
        buildExprListExp(buildFunctionRefExp(outlined_func), parameter_data,
                         buildFunctionRefExp(capture_cleanup),
                         parameter_if_clause, parameter_untied);
    task_call = buildFunctionCallStmt("rex_omp_task_owned", buildVoidType(),
                                      parameters, task_scope);
  } else {
    auto *parameters = buildExprListExp(buildFunctionRefExp(outlined_func),
                                        parameter_data, buildIntVal(0),
                                        parameter_arg_size, parameter_arg_align,
                                        parameter_if_clause, parameter_untied);
    task_call = buildFunctionCallStmt("XOMP_task", buildVoidType(), parameters,
                                      task_scope);
  }
  SageInterface::replaceStatement(target, task_call, true);

  pastePreprocessingInfo(task_call, PreprocessingInfo::before, save_buf1);
  pastePreprocessingInfo(task_call, PreprocessingInfo::after, save_buf2);
}

//! Translate omp task
void OmpSupport::transOmpTask(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTaskStatement *target = isSgOmpTaskStatement(node);
  ROSE_ASSERT(target != NULL);

  if (SageInterface::is_Fortran_language()) {
    transOmpTaskForFortran(target);
  } else {
    transOmpTaskForC(target);
  }
  retireConsumedOmpDirective(target);
}
