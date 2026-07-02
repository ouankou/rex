#include "sage3basic.h"

#include "accAstConstruction.h"

extern OpenACCDirective *parseOpenACC(std::string);

// the vector of pairs of OpenACC pragma and accparser IR.
extern std::vector<std::pair<SgPragmaDeclaration *, OpenACCDirective *>>
    OpenACCIR_list;

extern int omp_exprparser_parse();
extern SgExpression *parseExpression(SgNode *, bool, const char *);
extern SgExpression *parseArraySectionExpression(SgNode *, bool, const char *);
extern void omp_exprparser_parser_init(SgNode *aNode, const char *str);
static void buildVariableList(SgAccVariablesClause *);

extern bool copyStartFileInfo(SgNode *, SgNode *);
extern bool copyEndFileInfo(SgNode *, SgNode *);
extern SgExpression *checkOmpExpressionClause(SgExpression *, SgGlobal *,
                                              OmpSupport::omp_construct_enum);

// TODO: Fortran OpenACC parser interface

// store temporary expression pairs for ompparser.
extern std::vector<std::pair<std::string, SgNode *>> omp_variable_list;
static std::vector<std::pair<std::string, SgNode *>> *acc_variable_list =
    &omp_variable_list;
extern std::map<SgSymbol *,
                std::vector<std::pair<SgExpression *, SgExpression *>>>
    array_dimensions;
extern SgExpression *omp_expression;
static SgExpression *parseAccExpression(SgPragmaDeclaration *, std::string);
static void
    parseAccVariable(std::pair<SgPragmaDeclaration *, OpenACCDirective *>,
                     OpenACCClauseKind, std::string);
static SgExpression *
    parseAccArraySection(std::pair<SgPragmaDeclaration *, OpenACCDirective *>,
                         OpenACCClauseKind, std::string);
static void
convertOpenACCClauses(SgStatement *,
                      std::pair<SgPragmaDeclaration *, OpenACCDirective *>);
static SgStatement *convertOpenACCClauseDirective(
    std::pair<SgPragmaDeclaration *, OpenACCDirective *>);

namespace {
SgExpression *buildAccVarExprFromNode(SgNode *node) {
  if (SgInitializedName *iname = isSgInitializedName(node)) {
    return SageBuilder::buildVarRefExp(iname);
  }
  if (SgExpression *expr = isSgExpression(node)) {
    return expr;
  }
  return nullptr;
}

SgAccClausePtrList *getAccClauseList(SgStatement *directive) {
  if (SgAccClauseBodyStatement *body = isSgAccClauseBodyStatement(directive)) {
    return &body->get_clauses();
  }
  if (SgAccClauseStatement *clause_stmt = isSgAccClauseStatement(directive)) {
    return &clause_stmt->get_clauses();
  }
  return nullptr;
}

void attachAccClause(SgStatement *directive, SgAccClause *clause) {
  ROSE_ASSERT(directive != NULL);
  ROSE_ASSERT(clause != NULL);
  SgAccClausePtrList *clause_list = getAccClauseList(directive);
  ROSE_ASSERT(clause_list != NULL);
  clause_list->push_back(clause);
  clause->set_parent(directive);
}
} // namespace

SgStatement *
convertOpenACCDirective(std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                            current_OpenACCIR_to_SageIII) {
  printf("accparser directive is ready.\n");
  OpenACCDirectiveKind directive_kind =
      current_OpenACCIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;

  switch (directive_kind) {
  case ACCD_parallel:
  case ACCD_parallel_loop:
  case ACCD_data:
  case ACCD_kernels:
  case ACCD_atomic: {
    result = convertOpenACCBodyDirective(current_OpenACCIR_to_SageIII);
    break;
  }
  case ACCD_enter_data:
  case ACCD_exit_data:
  case ACCD_routine:
  case ACCD_wait:
  case ACCD_cache: {
    result = convertOpenACCClauseDirective(current_OpenACCIR_to_SageIII);
    break;
  }
  case ACCD_end: {
    printf("Unexpected OpenACC end directive during AST construction.\n");
    ROSE_ABORT();
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
    ROSE_ABORT();
  }
  }

  ROSE_ASSERT(result != NULL);
  SageInterface::setOneSourcePositionForTransformation(result);
  SgPragmaDeclaration *pdecl = current_OpenACCIR_to_SageIII.first;
  copyStartFileInfo(pdecl, result);
  copyEndFileInfo(pdecl, result);

  if (AstAttribute *attr = pdecl->getAttribute("acc_fortran_end")) {
    pdecl->removeAttribute("acc_fortran_end");
    result->setAttribute("acc_fortran_end", attr);
  }

  //! For C/C++ replace OpenACC pragma declaration with an SgAccxxStatement
  SgScopeStatement *scope = pdecl->get_scope();
  ROSE_ASSERT(scope != NULL);
  SageInterface::moveUpPreprocessingInfo(
      result, pdecl); // keep #ifdef etc attached to the pragma
  SageInterface::replaceStatement(pdecl, result);

  return result;
}

SgAccClauseBodyStatement *
convertOpenACCBodyDirective(std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                                current_OpenACCIR_to_SageIII) {

  OpenACCDirectiveKind directive_kind =
      current_OpenACCIR_to_SageIII.second->getKind();
  // directives like parallel and for have a following code block beside the
  // pragma itself.
  SgStatement *body =
      SageInterface::getNextStatement(current_OpenACCIR_to_SageIII.first);
  if (body == NULL) {
    printf("error: OpenACC directive requires a body but none was found.\n");
    ROSE_ABORT();
  }
  SageInterface::removeStatement(body, false);
  SgAccClauseBodyStatement *result = NULL;

  switch (directive_kind) {
  // TODO: insert SgAccTargetStatement first when available.
  case ACCD_parallel: {
    result = new SgAccParallelStatement(NULL, body);
    // not correct
    // should be target teams + parallel
    break;
  }
  case ACCD_parallel_loop: {
    result = new SgAccParallelLoopStatement(NULL, body);
    break;
  }
  case ACCD_data: {
    result = new SgAccDataStatement(NULL, body);
    break;
  }
  case ACCD_kernels: {
    result = new SgAccKernelsStatement(NULL, body);
    break;
  }
  case ACCD_atomic: {
    result = new SgAccAtomicStatement(NULL, body);
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
    ROSE_ABORT();
  }
  }
  body->set_parent(result);
  convertOpenACCClauses(result, current_OpenACCIR_to_SageIII);

  return result;
}

SgAccExpressionClause *convertOpenACCExpressionClause(
    SgStatement *directive,
    std::pair<SgPragmaDeclaration *, OpenACCDirective *>
        current_OpenACCIR_to_SageIII,
    OpenACCClause *current_acc_clause) {
  printf("accparser expression clause is ready.\n");
  SgAccExpressionClause *result = NULL;
  SgExpression *clause_expression = NULL;
  OpenACCClauseKind clause_kind = current_acc_clause->getKind();
  if (clause_kind == ACCC_async) {
    if (OpenACCAsyncClause *async_clause =
            dynamic_cast<OpenACCAsyncClause *>(current_acc_clause)) {
      if (async_clause->getModifier() == ACCC_ASYNC_expr &&
          !async_clause->getAsyncExpr().text.empty()) {
        clause_expression =
            parseAccExpression(current_OpenACCIR_to_SageIII.first,
                               async_clause->getAsyncExpr().text);
      }
    }
  } else if (clause_kind == ACCC_if) {
    if (OpenACCIfClause *if_clause =
            dynamic_cast<OpenACCIfClause *>(current_acc_clause)) {
      if (!if_clause->getCondition().text.empty()) {
        clause_expression = parseAccExpression(
            current_OpenACCIR_to_SageIII.first, if_clause->getCondition().text);
      }
    }
  } else if (clause_kind == ACCC_vector) {
    if (OpenACCVectorClause *vec_clause =
            dynamic_cast<OpenACCVectorClause *>(current_acc_clause)) {
      if (!vec_clause->getLengthExpr().text.empty()) {
        clause_expression =
            parseAccExpression(current_OpenACCIR_to_SageIII.first,
                               vec_clause->getLengthExpr().text);
      }
    }
  } else {
    std::vector<OpenACCExpressionItem> *current_expressions =
        current_acc_clause->getExpressions();
    if (current_expressions != NULL && !current_expressions->empty()) {
      if (clause_kind == ACCC_wait && current_expressions->size() > 1) {
        SgExprListExp *expr_list = SageBuilder::buildExprListExp();
        for (const auto &expr_item : *current_expressions) {
          SageInterface::appendExpression(
              expr_list, parseAccExpression(current_OpenACCIR_to_SageIII.first,
                                            expr_item.text));
        }
        clause_expression = expr_list;
      } else {
        if (clause_kind != ACCC_wait) {
          ROSE_ASSERT(current_expressions->size() == 1);
        }
        clause_expression =
            parseAccExpression(current_OpenACCIR_to_SageIII.first,
                               current_expressions->back().text);
      }
    }
  }

  switch (clause_kind) {
  case ACCC_collapse: {
    result = new SgAccCollapseClause(clause_expression);
    printf("collapse Clause added!\n");
    break;
  }
  case ACCC_num_gangs: {
    result = new SgAccNumGangsClause(clause_expression);
    printf("num_gangs Clause added!\n");
    break;
  }
  case ACCC_num_workers: {
    result = new SgAccNumWorkersClause(clause_expression);
    printf("num_units Clause added!\n");
    break;
  }
  case ACCC_vector_length: {
    result = new SgAccVectorLengthClause(clause_expression);
    printf("simdlen Clause added!\n");
    break;
  }
  case ACCC_async: {
    result = new SgAccAsyncClause(clause_expression);
    break;
  }
  case ACCC_if: {
    result = new SgAccIfClause(clause_expression);
    break;
  }
  case ACCC_vector: {
    result = new SgAccVectorClause(clause_expression);
    break;
  }
  default: {
    printf("Unknown OpenACC Clause!\n");
    ROSE_ABORT();
  }
  }
  SageInterface::setOneSourcePositionForTransformation(result);

  attachAccClause(directive, result);

  return result;
}

SgAccClause *
convertOpenACCClause(SgStatement *directive,
                     std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                         current_OpenACCIR_to_SageIII,
                     OpenACCClause *current_acc_clause) {
  printf("accparser variables clause is ready.\n");
  SgAccClause *result = NULL;

  OpenACCClauseKind clause_kind = current_acc_clause->getKind();
  const std::vector<OpenACCExpressionItem> *current_expressions = nullptr;
  if (OpenACCVarListClause *var_clause =
          dynamic_cast<OpenACCVarListClause *>(current_acc_clause)) {
    current_expressions = &var_clause->getVars();
  } else {
    current_expressions = current_acc_clause->getExpressions();
  }
  if (current_expressions != NULL && !current_expressions->empty()) {
    for (const auto &expr_item : *current_expressions) {
      parseAccVariable(current_OpenACCIR_to_SageIII,
                       current_acc_clause->getKind(), expr_item.text);
    }
  }

  SgExprListExp *var_list = SageBuilder::buildExprListExp();
  switch (clause_kind) {
  case ACCC_copy: {
    result = new SgAccCopyClause(var_list);
    break;
  }
  case ACCC_copyin: {
    result = new SgAccCopyinClause(var_list);
    break;
  }
  case ACCC_copyout: {
    result = new SgAccCopyoutClause(var_list);
    break;
  }
  case ACCC_create: {
    result = new SgAccCreateClause(var_list);
    break;
  }
  case ACCC_present: {
    result = new SgAccPresentClause(var_list);
    break;
  }
  case ACCC_private: {
    result = new SgAccPrivateClause(var_list);
    break;
  }
  case ACCC_deviceptr: {
    result = new SgAccDeviceptrClause(var_list);
    break;
  }
  case ACCC_delete: {
    result = new SgAccDeleteClause(var_list);
    break;
  }
  case ACCC_reduction: {
    SgAccReductionClause *reduction_clause =
        new SgAccReductionClause(var_list, 0);
    if (OpenACCReductionClause *acc_reduction =
            dynamic_cast<OpenACCReductionClause *>(current_acc_clause)) {
      reduction_clause->set_reduction_operator(
          static_cast<int>(acc_reduction->getOperator()));
    }
    result = reduction_clause;
    break;
  }
  default: {
    printf("Unknown OpenACC Clause!\n");
    ROSE_ABORT();
  }
  }

  ROSE_ASSERT(result != NULL);
  buildVariableList(isSgAccVariablesClause(result));
  var_list->set_parent(result);

  SageInterface::setOneSourcePositionForTransformation(result);
  attachAccClause(directive, result);

  acc_variable_list->clear();
  array_dimensions.clear();
  return result;
}

static SgAccClause *
convertOpenACCSimpleClause(SgStatement *directive,
                           std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                               current_OpenACCIR_to_SageIII,
                           OpenACCClause *current_acc_clause) {
  SgAccClause *result = NULL;
  OpenACCClauseKind clause_kind = current_acc_clause->getKind();
  switch (clause_kind) {
  case ACCC_default: {
    SgAccDefaultClause *default_clause = new SgAccDefaultClause(0);
    if (OpenACCDefaultClause *acc_default =
            dynamic_cast<OpenACCDefaultClause *>(current_acc_clause)) {
      default_clause->set_default_kind(
          static_cast<int>(acc_default->getKind()));
    }
    result = default_clause;
    break;
  }
  case ACCC_gang: {
    if (OpenACCGangClause *gang_clause =
            dynamic_cast<OpenACCGangClause *>(current_acc_clause)) {
      if (!gang_clause->getArgs().empty()) {
        printf("Unsupported OpenACC gang clause arguments.\n");
        ROSE_ABORT();
      }
    }
    result = new SgAccGangClause();
    break;
  }
  case ACCC_seq: {
    result = new SgAccSeqClause();
    break;
  }
  case ACCC_update: {
    result = new SgAccUpdateClause();
    break;
  }
  case ACCC_read: {
    result = new SgAccReadClause();
    break;
  }
  case ACCC_write: {
    result = new SgAccWriteClause();
    break;
  }
  case ACCC_capture: {
    result = new SgAccCaptureClause();
    break;
  }
  default: {
    printf("Unknown OpenACC clause is found.\n");
    ROSE_ABORT();
  }
  }

  SageInterface::setOneSourcePositionForTransformation(result);
  attachAccClause(directive, result);
  return result;
}

static void
convertOpenACCClauses(SgStatement *directive,
                      std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                          current_OpenACCIR_to_SageIII) {
  std::vector<OpenACCClause *> *all_clauses =
      current_OpenACCIR_to_SageIII.second->getClausesInOriginalOrder();
  if (all_clauses == nullptr) {
    return;
  }
  for (OpenACCClause *clause : *all_clauses) {
    OpenACCClauseKind clause_kind = clause->getKind();
    switch (clause_kind) {
    case ACCC_collapse:
    case ACCC_num_gangs:
    case ACCC_num_workers:
    case ACCC_vector_length:
    case ACCC_async:
    case ACCC_if:
    case ACCC_vector: {
      convertOpenACCExpressionClause(directive, current_OpenACCIR_to_SageIII,
                                     clause);
      break;
    }
    case ACCC_copy:
    case ACCC_copyin:
    case ACCC_copyout:
    case ACCC_create:
    case ACCC_present:
    case ACCC_private:
    case ACCC_deviceptr:
    case ACCC_delete:
    case ACCC_reduction: {
      convertOpenACCClause(directive, current_OpenACCIR_to_SageIII, clause);
      break;
    }
    case ACCC_default:
    case ACCC_gang:
    case ACCC_seq:
    case ACCC_update:
    case ACCC_read:
    case ACCC_write:
    case ACCC_capture: {
      convertOpenACCSimpleClause(directive, current_OpenACCIR_to_SageIII,
                                 clause);
      break;
    }
    default: {
      printf("Unknown OpenACC clause is found.\n");
      ROSE_ABORT();
    }
    }
  }
}

SgStatement *convertOpenACCClauseDirective(
    std::pair<SgPragmaDeclaration *, OpenACCDirective *>
        current_OpenACCIR_to_SageIII) {
  OpenACCDirectiveKind directive_kind =
      current_OpenACCIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;

  switch (directive_kind) {
  case ACCD_enter_data: {
    result = new SgAccEnterDataStatement();
    break;
  }
  case ACCD_exit_data: {
    result = new SgAccExitDataStatement();
    break;
  }
  case ACCD_routine: {
    SgName routine_name;
    if (OpenACCRoutineDirective *routine_dir =
            dynamic_cast<OpenACCRoutineDirective *>(
                current_OpenACCIR_to_SageIII.second)) {
      const OpenACCIdentifier &name = routine_dir->getName();
      if (!name.text.empty()) {
        routine_name = name.text;
      }
    }
    SgAccRoutineStatement *routine_stmt =
        new SgAccRoutineStatement(routine_name);
    result = routine_stmt;
    break;
  }
  case ACCD_wait: {
    SgAccWaitStatement *wait_stmt =
        new SgAccWaitStatement(static_cast<SgExprListExp *>(NULL),
                               static_cast<SgExpression *>(NULL), false);
    if (OpenACCWaitDirective *wait_dir = dynamic_cast<OpenACCWaitDirective *>(
            current_OpenACCIR_to_SageIII.second)) {
      const auto &async_ids = wait_dir->getAsyncIds();
      if (!async_ids.empty()) {
        SgExprListExp *expr_list = SageBuilder::buildExprListExp();
        for (const auto &expr_item : async_ids) {
          SageInterface::appendExpression(
              expr_list, parseAccExpression(current_OpenACCIR_to_SageIII.first,
                                            expr_item.text));
        }
        wait_stmt->set_wait_list(expr_list);
        expr_list->set_parent(wait_stmt);
      }
      const auto &devnum = wait_dir->getDevnum();
      if (!devnum.text.empty()) {
        SgExpression *devnum_expr =
            parseAccExpression(current_OpenACCIR_to_SageIII.first, devnum.text);
        wait_stmt->set_devnum(devnum_expr);
        if (devnum_expr != NULL) {
          devnum_expr->set_parent(wait_stmt);
        }
      }
      wait_stmt->set_queues(wait_dir->getQueues());
    }
    result = wait_stmt;
    break;
  }
  case ACCD_cache: {
    SgAccCacheStatement *cache_stmt =
        new SgAccCacheStatement(static_cast<SgExprListExp *>(NULL), 0);
    if (OpenACCCacheDirective *cache_dir =
            dynamic_cast<OpenACCCacheDirective *>(
                current_OpenACCIR_to_SageIII.second)) {
      const auto &vars = cache_dir->getVars();
      if (!vars.empty()) {
        SgExprListExp *expr_list = SageBuilder::buildExprListExp();
        for (const auto &expr_item : vars) {
          SageInterface::appendExpression(
              expr_list, parseAccExpression(current_OpenACCIR_to_SageIII.first,
                                            expr_item.text));
        }
        cache_stmt->set_variables(expr_list);
        expr_list->set_parent(cache_stmt);
      }
      cache_stmt->set_modifier(static_cast<int>(cache_dir->getModifier()));
    }
    result = cache_stmt;
    break;
  }
  default: {
    printf("Unknown OpenACC directive is found.\n");
    ROSE_ABORT();
  }
  }

  convertOpenACCClauses(result, current_OpenACCIR_to_SageIII);
  return result;
}

SgExpression *parseAccExpression(SgPragmaDeclaration *directive,
                                 std::string expression) {

  // TODO: merge OpenMP and OpenACC expression parsing helpers
  bool look_forward = false;

  std::string expr_string = std::string() + "expr (" + expression + ")\n";
  SgExpression *sg_expression =
      parseExpression(directive, look_forward, expr_string.c_str());

  return sg_expression;
}

void parseAccVariable(std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                          current_OpenACCIR_to_SageIII,
                      OpenACCClauseKind clause_kind, std::string expression) {
  bool look_forward = false;
  std::string expr_string = std::string() + "varlist " + expression + "\n";
  parseExpression(current_OpenACCIR_to_SageIII.first, look_forward,
                  expr_string.c_str());
}

SgExpression *
parseAccArraySection(std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                         current_OpenACCIR_to_SageIII,
                     OpenACCClauseKind clause_kind, std::string expression) {
  bool look_forward = false;
  std::string expr_string =
      std::string() + "array_section (" + expression + ")\n";
  SgExpression *sg_expression = parseArraySectionExpression(
      current_OpenACCIR_to_SageIII.first, look_forward, expr_string.c_str());

  return sg_expression;
}

void buildVariableList(SgAccVariablesClause *current_acc_clause) {

  std::vector<std::pair<std::string, SgNode *>>::iterator iter;
  for (iter = omp_variable_list.begin(); iter != omp_variable_list.end();
       iter++) {
    if (current_acc_clause == NULL) {
      break;
    }
    if (current_acc_clause->get_variables() == NULL) {
      current_acc_clause->set_variables(SageBuilder::buildExprListExp());
    }
    SgExprListExp *var_list = current_acc_clause->get_variables();
    if (SgExpression *expr = buildAccVarExprFromNode((*iter).second)) {
      var_list->get_expressions().push_back(expr);
      expr->set_parent(var_list);
    } else {
      std::cerr << "error: unhandled type of variable within a list:"
                << ((*iter).second)->class_name();
    }
  }
  if (current_acc_clause != NULL &&
      current_acc_clause->get_variables() != NULL) {
    current_acc_clause->get_variables()->set_parent(current_acc_clause);
  }
}

bool checkOpenACCIR(OpenACCDirective *directive) {

  if (directive == NULL) {
    return false;
  };
  OpenACCDirectiveKind directive_kind = directive->getKind();
  switch (directive_kind) {
  case ACCD_parallel: {
    break;
  }
  case ACCD_parallel_loop: {
    break;
  }
  case ACCD_data:
  case ACCD_kernels:
  case ACCD_atomic:
  case ACCD_enter_data:
  case ACCD_exit_data:
  case ACCD_routine:
  case ACCD_wait:
  case ACCD_cache:
  case ACCD_end: {
    break;
  }
  default: {
    return false;
  }
  };
  std::map<OpenACCClauseKind, std::vector<OpenACCClause *>> *clauses =
      directive->getAllClauses();
  if (clauses != NULL) {
    std::map<OpenACCClauseKind, std::vector<OpenACCClause *>>::iterator it;
    for (it = clauses->begin(); it != clauses->end(); it++) {
      switch (it->first) {
      case ACCC_collapse:
      case ACCC_copy:
      case ACCC_copyin:
      case ACCC_copyout:
      case ACCC_create:
      case ACCC_present:
      case ACCC_private:
      case ACCC_deviceptr:
      case ACCC_delete:
      case ACCC_reduction:
      case ACCC_num_gangs:
      case ACCC_num_workers:
      case ACCC_vector_length:
      case ACCC_async:
      case ACCC_if:
      case ACCC_vector:
      case ACCC_default:
      case ACCC_gang:
      case ACCC_seq:
      case ACCC_update:
      case ACCC_read:
      case ACCC_write:
      case ACCC_capture: {
        break;
      }
      default: {
        return false;
      }
      };
    };
  };
  return true;
}
