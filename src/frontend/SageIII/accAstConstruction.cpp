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

SgStatement *
convertOpenACCDirective(std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                            current_OpenACCIR_to_SageIII) {
  printf("accparser directive is ready.\n");
  OpenACCDirectiveKind directive_kind =
      current_OpenACCIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;

  switch (directive_kind) {
  case ACCD_parallel: {
    result = convertOpenACCBodyDirective(current_OpenACCIR_to_SageIII);
    break;
  }
  case ACCD_parallel_loop: {
    result = convertOpenACCBodyDirective(current_OpenACCIR_to_SageIII);
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
  }

  SageInterface::setOneSourcePositionForTransformation(result);
  SgPragmaDeclaration *pdecl = current_OpenACCIR_to_SageIII.first;
  copyStartFileInfo(pdecl, result);
  copyEndFileInfo(pdecl, result);

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
  SageInterface::removeStatement(body, false);
  SgAccClauseBodyStatement *result = NULL;
  OpenACCClauseKind clause_kind;

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
  default: {
    printf("Unknown directive is found.\n");
    ROSE_ABORT();
  }
  }
  body->set_parent(result);
  // extract all the clauses based on the vector of clauses in the original
  // order
  std::vector<OpenACCClause *> *all_clauses =
      current_OpenACCIR_to_SageIII.second->getClausesInOriginalOrder();
  std::vector<OpenACCClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case ACCC_collapse:
    case ACCC_num_gangs:
    case ACCC_num_workers:
    case ACCC_vector_length: {
      convertOpenACCExpressionClause(isSgAccClauseBodyStatement(result),
                                     current_OpenACCIR_to_SageIII,
                                     *clause_iter);
      break;
    }
    case ACCC_copyin:
    case ACCC_copyout:
    case ACCC_copy: {
      convertOpenACCClause(isSgAccClauseBodyStatement(result),
                           current_OpenACCIR_to_SageIII, *clause_iter);
      break;
    }
    default: {
      printf("Unknown OpenACC clause is found.\n");
      ROSE_ABORT();
    }
    };
  };

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
      clause_expression = parseAccExpression(current_OpenACCIR_to_SageIII.first,
                                             current_expressions->back().text);
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
  default: {
    printf("Unknown OpenACC Clause!\n");
  }
  }
  SageInterface::setOneSourcePositionForTransformation(result);

  SgAccClause *sg_clause = result;
  SgAccClauseBodyStatement *acc_stmt = isSgAccClauseBodyStatement(directive);
  ROSE_ASSERT(acc_stmt != NULL);
  acc_stmt->get_clauses().push_back(sg_clause);

  sg_clause->set_parent(directive);

  return result;
}

SgAccClause *
convertOpenACCClause(SgStatement *directive,
                     std::pair<SgPragmaDeclaration *, OpenACCDirective *>
                         current_OpenACCIR_to_SageIII,
                     OpenACCClause *current_acc_clause) {
  printf("accparser variables clause is ready.\n");
  SgAccClause *result = NULL;
  SgAccClauseBodyStatement *target = isSgAccClauseBodyStatement(directive);
  ROSE_ASSERT(target != NULL);

  OpenACCClauseKind clause_kind = current_acc_clause->getKind();
  std::vector<OpenACCExpressionItem> *current_expressions =
      current_acc_clause->getExpressions();
  if (current_expressions != NULL && !current_expressions->empty()) {
    for (const auto &expr_item : *current_expressions) {
      parseAccArraySection(current_OpenACCIR_to_SageIII,
                           current_acc_clause->getKind(), expr_item.text);
    }
  }

  switch (clause_kind) {
  case ACCC_copy: {
    printf("copy Clause added!\n");
    break;
  }
  case ACCC_copyin: {
    printf("copyin Clause added!\n");
    break;
  }
  case ACCC_copyout: {
    printf("copyout Clause added!\n");
    break;
  }
  default: {
    printf("Unknown OpenACC Clause!\n");
    ROSE_ABORT();
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
  default: {
    ROSE_ABORT();
  }
  }

  ROSE_ASSERT(result != NULL);
  buildVariableList(isSgAccVariablesClause(result));
  var_list->set_parent(result);

  SageInterface::setOneSourcePositionForTransformation(result);
  target->get_clauses().push_back(result);
  result->set_parent(target);

  acc_variable_list->clear();
  array_dimensions.clear();
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
    if (SgInitializedName *iname = isSgInitializedName((*iter).second)) {
      SgVarRefExp *var_ref = SageBuilder::buildVarRefExp(iname);
      var_list->get_expressions().push_back(var_ref);
      var_ref->set_parent(var_list);
    } else if (SgPntrArrRefExp *aref = isSgPntrArrRefExp((*iter).second)) {
      var_list->get_expressions().push_back(aref);
      aref->set_parent(var_list);
    } else if (SgVarRefExp *vref = isSgVarRefExp((*iter).second)) {
      var_list->get_expressions().push_back(vref);
      vref->set_parent(var_list);
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
      case ACCC_num_gangs:
      case ACCC_num_workers:
      case ACCC_vector_length: {
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
