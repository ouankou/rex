
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "Outliner.hh"

#include "RoseAst.h"

#include "omp_lowering.h"

#include "rex_llvm.h"

#include "sage3basic.h"

#include "abiStuff.h"

#include "sageBuilder.h"

#include <limits>
#include <sstream>
#include <string>

using namespace std;
using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

extern std::vector<SgFunctionDeclaration *> *target_outlined_function_list;
extern std::vector<SgDeclarationStatement *> *target_outlined_struct_list;

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

static bool is_32_bit_target(const SgNode *context) {
  SgProject *project = SageInterface::getProject(context);
  ROSE_ASSERT(project != NULL);
  return project->get_mode_32_bit();
}

static StructLayoutInfo get_target_layout_info(SgType *type,
                                               const SgNode *context) {
  ROSE_ASSERT(type != NULL);

  if (is_32_bit_target(context)) {
    I386PrimitiveTypeLayoutGenerator primitive_generator(NULL);
    NonpackedTypeLayoutGenerator layout_generator(&primitive_generator);
    return layout_generator.layoutType(type);
  }

  X86_64PrimitiveTypeLayoutGenerator primitive_generator(NULL);
  NonpackedTypeLayoutGenerator layout_generator(&primitive_generator);
  return layout_generator.layoutType(type);
}

static int get_target_type_size_bytes(SgType *type, const SgNode *context) {
  StructLayoutInfo layout = get_target_layout_info(type, context);
  if (layout.size == 0 ||
      layout.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return -1;
  return static_cast<int>(layout.size);
}

static int get_fortran_value_parameter_size(SgType *type,
                                            const SgNode *context) {
  ROSE_ASSERT(type != NULL);
  SgType *base_type =
      type->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  ROSE_ASSERT(base_type != NULL);
  return get_target_type_size_bytes(base_type, context);
}

static int get_fortran_pointer_parameter_size(const SgNode *context) {
  return get_target_type_size_bytes(buildPointerType(buildVoidType()), context);
}

static void transOmpTaskForFortran(SgOmpTaskStatement *target) {
  ROSE_ASSERT(target != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  insert_libxompf_h_for_task(target);

  AttachedPreprocessingInfoType save_buf1, save_buf2;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  std::string wrapper_name;
  ASTtools::VarSymSet_t syms;
  ASTtools::VarSymSet_t pdSyms3;
  SgFunctionDeclaration *outlined_func =
      generateOutlinedTask(target, wrapper_name, syms, pdSyms3, false, false);
  ROSE_ASSERT(outlined_func != NULL);

  SgBasicBlock *enclosing_body = getEnclosingRegionOrFuncDefinition(target);
  ROSE_ASSERT(enclosing_body != NULL);
  append_fortran_external_decl(enclosing_body, outlined_func);

  SgScopeStatement *task_scope = target->get_scope();
  ROSE_ASSERT(task_scope != NULL);

  int pointer_size = get_fortran_pointer_parameter_size(target);
  if (pointer_size <= 0) {
    MLOG_ERROR_CXX("ompLowering")
        << "transOmpTaskForFortran(): unable to determine target pointer size";
    ROSE_ABORT();
  }
  if (syms.size() >
      static_cast<size_t>(std::numeric_limits<int>::max() / pointer_size)) {
    MLOG_ERROR_CXX("ompLowering")
        << "transOmpTaskForFortran(): outlined task parameter size overflow";
    ROSE_ABORT();
  }

  SgExpression *parameter_cpyfn = buildIntVal(0);
  SgExpression *parameter_arg_size =
      buildIntVal(static_cast<int>(syms.size() * pointer_size));
  SgExpression *parameter_arg_align = buildIntVal(4);
  SgExpression *parameter_if_clause = buildIntVal(1);
  if (hasClause(target, V_SgOmpIfClause)) {
    Rose_STL_Container<SgOmpClause *> clauses =
        getClause(target, V_SgOmpIfClause);
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpIfClause *if_clause = isSgOmpIfClause(clauses[0]);
    ROSE_ASSERT(if_clause != NULL);
    ROSE_ASSERT(if_clause->get_expression() != NULL);
    parameter_if_clause = copyExpression(if_clause->get_expression());
  }

  SgExpression *parameter_untied =
      hasClause(target, V_SgOmpUntiedClause) ? buildIntVal(1) : buildIntVal(0);

  SgExprListExp *parameters =
      buildExprListExp(buildFortranOutlinedFunctionRef(outlined_func),
                       parameter_cpyfn, parameter_arg_size, parameter_arg_align,
                       parameter_if_clause, parameter_untied);
  appendExpression(parameters, buildIntVal(static_cast<int>(syms.size() * 3)));

  for (ASTtools::VarSymSet_t::iterator iter = syms.begin(); iter != syms.end();
       ++iter) {
    const SgVariableSymbol *sym = *iter;
    ROSE_ASSERT(sym != NULL);
    ROSE_ASSERT(sym->get_declaration() != NULL);

    bool pass_by_value = isLoopIndexVariable(sym->get_declaration(), target);
    appendExpression(parameters, buildIntVal(pass_by_value ? 1 : 0));

    if (pass_by_value) {
      int value_size =
          get_fortran_value_parameter_size(sym->get_type(), target);
      if (value_size < 0) {
        MLOG_ERROR_CXX("ompLowering")
            << "transOmpTaskForFortran(): unsupported pass-by-value type for "
            << "variable " << sym->get_name().getString() << " of type "
            << sym->get_type()->class_name();
        ROSE_ABORT();
      }
      appendExpression(parameters, buildIntVal(value_size));
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
      generateOutlinedTask(target, wrapper_name, syms, pdSyms3, false, false);
  ROSE_ASSERT(outlined_func != NULL);

  SgScopeStatement *task_scope = target->get_scope();
  ROSE_ASSERT(task_scope != NULL);

  SgExpression *parameter_data = NULL;
  SgExpression *parameter_cpyfn = buildIntVal(0);
  SgExpression *parameter_arg_size = NULL;
  SgExpression *parameter_arg_align = NULL;
  size_t parameter_count = syms.size();
  if (parameter_count == 0) {
    parameter_data = buildIntVal(0);
    parameter_arg_size = buildIntVal(0);
    parameter_arg_align = buildIntVal(0);
  } else {
    SgVarRefExp *data_ref = buildVarRefExp(wrapper_name, task_scope);
    ROSE_ASSERT(data_ref != NULL);
    parameter_data =
        buildAddressOfOp(data_ref, buildPointerType(data_ref->get_type()));
    parameter_arg_size = buildSizeOfOp(
        data_ref->get_type(), SageInterface::requireTargetSizeType(task_scope));
    parameter_arg_align = buildIntVal(4);
  }

  SgExpression *parameter_if_clause = buildIntVal(1);
  if (hasClause(target, V_SgOmpIfClause)) {
    Rose_STL_Container<SgOmpClause *> clauses =
        getClause(target, V_SgOmpIfClause);
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpIfClause *if_clause = isSgOmpIfClause(clauses[0]);
    ROSE_ASSERT(if_clause != NULL);
    ROSE_ASSERT(if_clause->get_expression() != NULL);
    parameter_if_clause = copyExpression(if_clause->get_expression());
  }

  SgExpression *parameter_untied =
      hasClause(target, V_SgOmpUntiedClause) ? buildIntVal(1) : buildIntVal(0);

  SgType *task_callback_type = buildPointerType(buildFunctionType(
      buildVoidType(),
      buildFunctionParameterTypeList(buildPointerType(buildVoidType()))));
  SgExpression *task_callback =
      buildCastExp(buildFunctionRefExp(outlined_func), task_callback_type,
                   SgCastExp::e_C_style_cast);

  SgExprListExp *parameters = buildExprListExp(
      task_callback, parameter_data, parameter_cpyfn, parameter_arg_size,
      parameter_arg_align, parameter_if_clause, parameter_untied);
  SgExprStatement *task_call = buildFunctionCallStmt(
      "XOMP_task", buildVoidType(), parameters, task_scope);
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
    return;
  }

  transOmpTaskForC(target);
  return;

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgGlobal *g_scope = SageInterface::getGlobalScope(body);
  ROSE_ASSERT(g_scope != NULL);

  // Make sure the rex_kmp.h header is included for C/C++.
  SgSourceFile *file = getEnclosingSourceFile(target);
  if (file != nullptr && !file->get_Fortran_only()) {
    insertHeader(file, "rex_kmp.h", false);
  }

  ////////////////////////////////////////////////
  //
  // First, we need to query arguments.
  // Start with the original function
  //
  std::vector<std::string> originalVarRefs;
  std::map<std::string, SgType *> varRefTypeMap;
  SgFunctionDeclaration *originalDec = getEnclosingFunctionDeclaration(target);
  // for (SgInitializedName *arg : originalDec->get_args()) {
  for (size_t i = 2; i < originalDec->get_args().size(); i++) {
    SgInitializedName *arg = originalDec->get_args().at(i);
    std::string name = arg->get_name();
    originalVarRefs.push_back(name);
    varRefTypeMap[name] = arg->get_type();
    /*if (isSgPointerType(arg->get_type())) {
        varRefTypeMap[name] = arg->get_type()->findBaseType();
    }*/
  }

  // Now get the shared variables
  std::vector<std::string> sharedVarRefs = originalVarRefs;
  /*for (SgOmpClause *clause : target->get_clauses()) {
      if (clause->variantT() != V_SgOmpSharedClause) continue;

      SgOmpSharedClause *shared = static_cast<SgOmpSharedClause *>(clause);
      for (SgExpression *expr : shared->get_variables()->get_expressions()) {
          if (expr->variantT() != V_SgVarRefExp) continue;
          SgVarRefExp *varRef = static_cast<SgVarRefExp *>(expr);
          std::string name = varRef->get_symbol()->get_name();
          sharedVarRefs.push_back(name);
          varRefTypeMap[name] = varRef->get_type();
      }
  }*/

  //
  // Create the needed structures for this
  //
  // The shareds structure
  SgClassDeclaration *strPshareds = buildStructDeclaration(
      declaration_ownership::sourceLexical(), "shar", g_scope);
  SgClassDefinition *strPsharedsDef = buildClassDefinition(strPshareds);

  for (std::string varName : sharedVarRefs) {
    // SgPointerType *type = buildPointerType(varRefTypeMap[varName]);
    // SgVariableDeclaration *varD = buildVariableDeclaration(varName, type,
    // NULL, strPsharedsDef);
    SgVariableDeclaration *varD = buildVariableDeclaration(
        varName, varRefTypeMap[varName], NULL, strPsharedsDef);
    appendStatement(varD, strPsharedsDef);
  }

  SgTypedefDeclaration *tyPshareds = buildTypedefDeclaration(
      typedef_declaration_ownership::sourceLexical(),
      SgTypedefDeclaration::e_typedef, "pshareds",
      buildPointerType(strPshareds->get_type()), g_scope);

  // The task structure
  SgClassDeclaration *taskStruct = buildStructDeclaration(
      declaration_ownership::sourceLexical(), "task", g_scope);
  SgClassDefinition *taskStructDef = buildClassDefinition(taskStruct);

  SgType *sharedType = tyPshareds->get_type();
  ROSE_ASSERT(sharedType != nullptr);
  SgVariableDeclaration *sharedPtr =
      buildVariableDeclaration("shareds", sharedType, NULL, taskStructDef);
  appendStatement(sharedPtr, taskStructDef);

  SgPointerType *dummyPtrType = buildPointerType(buildVoidType());
  SgVariableDeclaration *dummyPtr =
      buildVariableDeclaration("dummy", dummyPtrType, NULL, taskStructDef);
  appendStatement(dummyPtr, taskStructDef);

  SgTypedefDeclaration *taskTypedef = buildTypedefDeclaration(
      typedef_declaration_ownership::sourceLexical(),
      SgTypedefDeclaration::e_typedef, "ptask",
      buildPointerType(taskStruct->get_type()), g_scope);

  // Insert them all
  SgStatement *firstStatement = getFirstStatement(g_scope);
  removeStatement(strPshareds, false);
  insertStatementAfter(firstStatement, strPshareds);
  removeStatement(tyPshareds, false);
  insertStatementAfter(strPshareds, tyPshareds);
  removeStatement(taskStruct, false);
  insertStatementAfter(tyPshareds, taskStruct);
  removeStatement(taskTypedef, false);
  insertStatementAfter(taskStruct, taskTypedef);

  // Insert the function forward declarations
  // TODO: This should probably be in a header somewhere

  // We don't actually use this outlined function, but for some reason we need
  // it to make sure we have all the correct parameters
  //
  AttachedPreprocessingInfoType save_buf1, save_buf2;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);
  std::string wrapper_name;
  ASTtools::VarSymSet_t syms;
  ASTtools::VarSymSet_t
      pdSyms3; // store all variables which should be passed by reference
  SgFunctionDeclaration *outlined_func =
      generateOutlinedTask(node, wrapper_name, syms, pdSyms3, true);
  std::string outlinedName = outlined_func->get_name();

  SgFunctionDefinition *funcDef = outlined_func->get_definition();
  ROSE_ASSERT(funcDef != NULL);

  /*auto of_arg1 = outlined_func->get_args().at(0);
  auto of_arg2 = outlined_func->get_args().at(1);
  outlined_func->get_args().clear();
  outlined_func->get_args().push_back(of_arg1);
  outlined_func->get_args().push_back(of_arg2);*/

  funcDef->get_body()->get_statements().clear();

  /*
  // TODO: DELETE
  SgFunctionDefinition *funcDef = getEnclosingFunctionDefinition(node);
  ROSE_ASSERT(funcDef != NULL);
  SgFunctionDeclaration *outlined_func = funcDef->get_declaration();
  ROSE_ASSERT(outlined_func != NULL);
  // END DELETE
  */

  // Start with the body
  // First line: int *n = task->shareds->n;

  SgVarRefExp *taskRef = nullptr;
  SgVarRefExp *sharedsRef = nullptr;
  SgArrowExp *arrow2, *arrow1;

  for (std::string varName : originalVarRefs) {
    SgVarRefExp *nRef = buildVarRefExp(varName, g_scope);

    taskRef = buildVarRefExp("task", g_scope);
    sharedsRef = buildVarRefExp("shareds", g_scope);
    arrow2 = buildArrowExp(taskRef, sharedsRef, sharedsRef->get_type());
    arrow1 = buildArrowExp(arrow2, nRef, nRef->get_type());

    std::string varName1 = varName + "1";
    SgAssignInitializer *init =
        buildAssignInitializer(arrow1, buildPointerType(buildIntType()));
    SgVariableDeclaration *n = buildVariableDeclaration(
        varName1, buildPointerType(buildIntType()), init, funcDef);
    funcDef->append_statement(n);
  }

  // Now the body
  SgExprStatement *exprStmt = static_cast<SgExprStatement *>(body);
  SgAssignOp *expr = static_cast<SgAssignOp *>(exprStmt->get_expression());
  SgExpression *rhs = deepCopy(expr->get_rhs_operand());

  std::function<void(SgExpression *, SgScopeStatement *)> fix_names;
  fix_names = [rhs, &fix_names](SgExpression *input,
                                SgScopeStatement *g_scope) {
    if (input->variantT() == V_SgVarRefExp) {
      SgVarRefExp *ref = isSgVarRefExp(input);
      std::string name = ref->get_symbol()->get_name() + "p__1";
      SgVarRefExp *newRef = buildVarRefExp(name, g_scope);
      replaceExpression(ref, newRef);

    } else if (isSgPointerDerefExp(input)) {
      SgPointerDerefExp *deref = isSgPointerDerefExp(input);
      if (deref->get_operand()->variantT() == V_SgPointerDerefExp) {
        SgExpression *exp = deepCopy(deref->get_operand());
        replaceExpression(deref, exp);
        fix_names(exp, g_scope);
      } else {
        fix_names(deref->get_operand(), g_scope);
      }

    } else if (isSgBinaryOp(input)) {
      SgBinaryOp *op = static_cast<SgBinaryOp *>(input);
      fix_names(op->get_lhs_operand(), g_scope);
      fix_names(op->get_rhs_operand(), g_scope);

    } else if (isSgUnaryOp(input)) {
      SgUnaryOp *op = static_cast<SgUnaryOp *>(input);
      fix_names(op->get_operand(), g_scope);

    } else if (isSgFunctionCallExp(input)) {
      SgFunctionCallExp *fc = isSgFunctionCallExp(input);
      auto args = fc->get_args();
      for (auto exp : args->get_expressions()) {
        fix_names(exp, g_scope);
      }
    }
  };
  fix_names(rhs, g_scope);

  SgVarRefExp *lhs;
  if (expr->get_lhs_operand()->variantT() == V_SgPointerDerefExp) {
    SgPointerDerefExp *deref =
        static_cast<SgPointerDerefExp *>(expr->get_lhs_operand());
    // TODO: We need better control
    deref = static_cast<SgPointerDerefExp *>(deref->get_operand_i());
    lhs = static_cast<SgVarRefExp *>(deref->get_operand_i());
  } else {
    lhs = static_cast<SgVarRefExp *>(expr->get_lhs_operand());
  }
  ROSE_ASSERT(lhs != NULL);
  std::string name = lhs->get_symbol()->get_name() + "p__";
  SgVarRefExp *destRef = buildVarRefExp(name, g_scope);

  taskRef = buildVarRefExp("task", g_scope);
  sharedsRef = buildVarRefExp("shareds", g_scope);
  arrow2 = buildArrowExp(taskRef, sharedsRef, sharedsRef->get_type());
  arrow1 = buildArrowExp(arrow2, destRef, destRef->get_type());
  SgPointerType *destination_pointer_type = isSgPointerType(arrow1->get_type());
  ROSE_ASSERT(destination_pointer_type != nullptr);
  SgPointerDerefExp *deref =
      buildPointerDerefExp(arrow1, destination_pointer_type->get_base_type());

  SgExprStatement *assignBody = buildAssignStatement(deref, rhs);
  funcDef->append_statement(assignBody);

  // target_outlined_function_list->push_back(isSgFunctionDeclaration(outlined_func));

  ///////////////////////////////////
  // Now build the body
  ///////////////////////////////////
  //
  SgBasicBlock *block = buildBasicBlock();
  SageInterface::replaceStatement(target, block, true);
  // ROSE_ASSERT(outlined_func->get_args().size() > 2);

  // int gtid = *__global_tid;
  SgInitializedName *arg1 = outlined_func->get_args().at(0);
  SgVarRefExp *arg1Ref = buildVarRefExp(arg1->get_name(), g_scope);

  SgPointerType *global_thread_pointer_type =
      isSgPointerType(arg1Ref->get_type());
  ROSE_ASSERT(global_thread_pointer_type != nullptr);
  SgPointerDerefExp *gtidDeref = buildPointerDerefExp(
      arg1Ref, global_thread_pointer_type->get_base_type());
  SgAssignInitializer *gtidInit =
      buildAssignInitializer(gtidDeref, buildIntType());
  SgVariableDeclaration *gtid =
      buildVariableDeclaration("gtid", buildIntType(), gtidInit, g_scope);
  block->append_statement(gtid);

  // if (__kmpc_single(NULL, gtid)) { ... }
  SgVarRefExp *nullRef = buildVarRefExp("NULL", g_scope);
  SgVarRefExp *gtidRef = buildVarRefExp("gtid", g_scope);
  SgExprListExp *parameters = buildExprListExp(nullRef, gtidRef);
  SgFunctionCallExp *fcSingle =
      buildFunctionCallExp(getKmpcRuntimeFunctionName("__kmpc_single"),
                           buildIntType(), parameters, g_scope);

  SgBasicBlock *trueBlock = buildBasicBlock();
  SgIfStmt *ifStmt = buildIfStmt(fcSingle, trueBlock, NULL);
  block->append_statement(ifStmt);

  // ptask task;
  // pshareds psh;
  SgVariableDeclaration *taskDef =
      buildVariableDeclaration("task", taskTypedef->get_type(), NULL, g_scope);
  SgVariableDeclaration *pshDef =
      buildVariableDeclaration("psh", tyPshareds->get_type(), NULL, g_scope);
  trueBlock->append_statement(taskDef);
  trueBlock->append_statement(pshDef);

  // task = (ptask)__kmpc_omp_task_alloc(NULL, gtid, 1, sizeof(task) * 4,
  //                      sizeof(psh) * 2, &OUT__1__3690__fib__14__);
  taskRef = buildVarRefExp("task", g_scope);

  // TODO: This needs to be changed. It should be sizeof the types
  SgSizeOfOp *taskSizeOf =
      buildSizeOfOp(buildVarRefExp("task", g_scope),
                    SageInterface::requireTargetSizeType(g_scope));
  SgType *target_size_type = SageInterface::requireTargetSizeType(g_scope);
  SgMultiplyOp *taskSizeMul =
      buildMultiplyOp(taskSizeOf, buildIntVal(4), target_size_type);

  SgSizeOfOp *sharSizeOf =
      buildSizeOfOp(buildVarRefExp("psh", g_scope),
                    SageInterface::requireTargetSizeType(g_scope));
  SgMultiplyOp *sharSizeMul =
      buildMultiplyOp(sharSizeOf, buildIntVal(4), target_size_type);

  SgFunctionRefExp *outlinedRef =
      buildFunctionRefExp(outlined_func->get_name(), g_scope);
  SgAddressOfOp *outlinedAddr =
      buildAddressOfOp(outlinedRef, buildPointerType(outlinedRef->get_type()));
  parameters = buildExprListExp(buildVarRefExp("NULL", g_scope),
                                buildVarRefExp("gtid", g_scope), buildIntVal(1),
                                taskSizeMul, sharSizeMul, outlinedAddr);

  SgFunctionCallExp *fcTaskAlloc = buildFunctionCallExp(
      getKmpcRuntimeFunctionName("__kmpc_omp_task_alloc"),
      buildPointerType(buildVoidType()), parameters, g_scope);
  SgCastExp *taskCastExp = buildCastExp(fcTaskAlloc, taskTypedef->get_type());
  SgExprStatement *taskAllocAssign = buildAssignStatement(taskRef, taskCastExp);
  trueBlock->append_statement(taskAllocAssign);

  // task->shareds->n = n;
  // task->shareds->x = x;
  for (std::string varName : sharedVarRefs) {
    taskRef = buildVarRefExp("task", g_scope);
    sharedsRef = buildVarRefExp("shareds", g_scope);
    arrow2 = buildArrowExp(taskRef, sharedsRef, sharedsRef->get_type());
    SgVarRefExp *memberRef = buildVarRefExp(varName, g_scope);
    arrow1 = buildArrowExp(arrow2, memberRef, memberRef->get_type());
    SgExprStatement *arrowAssign =
        buildAssignStatement(arrow1, buildVarRefExp(varName, g_scope));
    trueBlock->append_statement(arrowAssign);
  }

  // __kmpc_omp_task(NULL, gtid, task);
  parameters = buildExprListExp(buildVarRefExp("NULL", g_scope),
                                buildVarRefExp("gtid", g_scope),
                                buildVarRefExp("task", g_scope));
  SgExprStatement *ompTask =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_omp_task"),
                            buildVoidType(), parameters, g_scope);
  trueBlock->append_statement(ompTask);

  // Move everything inside the body of the if statement
  std::vector<SgStatement *> toMove;
  SgStatement *nextStmt = SageInterface::getNextStatement(block);
  while (nextStmt != nullptr) {
    toMove.push_back(nextStmt);
    nextStmt = SageInterface::getNextStatement(nextStmt);
  }

  for (SgStatement *stmt : toMove) {
    trueBlock->append_statement(deepCopy(stmt));
    removeStatement(stmt);
  }

  // __kmpc_omp_taskwait(NULL, gtid);
  // __kmpc_end_single(NULL, gtid);
  parameters = buildExprListExp(nullRef, gtidRef);
  SgExprStatement *taskWait =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_omp_taskwait"),
                            buildVoidType(), parameters, g_scope);
  trueBlock->append_statement(taskWait);

  parameters = buildExprListExp(nullRef, gtidRef);
  SgExprStatement *endSingle =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_end_single"),
                            buildVoidType(), parameters, g_scope);
  trueBlock->append_statement(endSingle);

  // __kmpc_barrier(NULL, gtid);
  // This goes outside the if-statement just after it
  parameters = buildExprListExp(nullRef, gtidRef);
  SgExprStatement *barrierFc =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_barrier"),
                            buildVoidType(), parameters, g_scope);
  block->append_statement(barrierFc);

  //
  // Check for return statements
  //
  SgFunctionDefinition *parentDec = getEnclosingFunctionDefinition(block);
  Rose_STL_Container<SgNode *> bodyList =
      NodeQuery::querySubTree(parentDec->get_body(), V_SgStatement);
  for (Rose_STL_Container<SgNode *>::iterator i = bodyList.begin();
       i != bodyList.end(); i++) {
    SgStatement *stmt = static_cast<SgStatement *>((*i));
    if (stmt->variantT() != V_SgIfStmt) {
      continue;
    }

    Rose_STL_Container<SgNode *> ifStmtBody =
        NodeQuery::querySubTree(stmt, V_SgStatement);
    for (Rose_STL_Container<SgNode *>::iterator j = ifStmtBody.begin();
         j != ifStmtBody.end(); j++) {
      SgStatement *stmt2 = static_cast<SgStatement *>((*j));
      if (stmt2->variantT() != V_SgReturnStmt) {
        continue;
      }

      SgReturnStmt *ret = buildReturnStmt();
      replaceStatement(stmt2, ret);
    }
  }
  //*/

  // SgReturnStmt *ret = buildReturnStmt();
  // replaceStatement(isSgStatement(node), buildReturnStmt());
}
