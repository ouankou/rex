#include "rose.h"

#include "resetParentPointers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct OpenMPOwners {
  std::vector<SgOmpClauseBodyStatement *> clause_body_statements;
  std::vector<SgOmpClauseStatement *> clause_statements;
  std::vector<SgOmpGroupprivateStatement *> groupprivate_statements;
  std::vector<SgOmpFlushStatement *> flush_statements;
  std::vector<SgOmpAllocateStatement *> allocate_statements;
  std::vector<SgOmpMapClause *> map_clauses;
};

class CollectOpenMPOwners : public AstSimpleProcessing {
public:
  OpenMPOwners owners;

  void visit(SgNode *node) override {
    if (SgOmpClauseBodyStatement *statement =
            isSgOmpClauseBodyStatement(node)) {
      owners.clause_body_statements.push_back(statement);
    }
    if (SgOmpClauseStatement *statement = isSgOmpClauseStatement(node)) {
      owners.clause_statements.push_back(statement);
    }
    if (SgOmpGroupprivateStatement *statement =
            isSgOmpGroupprivateStatement(node)) {
      owners.groupprivate_statements.push_back(statement);
    }
    if (SgOmpFlushStatement *statement = isSgOmpFlushStatement(node)) {
      owners.flush_statements.push_back(statement);
    }
    if (SgOmpAllocateStatement *statement = isSgOmpAllocateStatement(node)) {
      owners.allocate_statements.push_back(statement);
    }
    if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
      owners.map_clauses.push_back(clause);
    }
  }
};

size_t successorCount(SgNode *owner, SgNode *child) {
  ROSE_ASSERT(owner != nullptr && child != nullptr);
  const std::vector<SgNode *> successors =
      owner->get_traversalSuccessorContainer();
  return static_cast<size_t>(
      std::count(successors.begin(), successors.end(), child));
}

SgOmpClauseList *clauseList(SgNode *owner) {
  ROSE_ASSERT(owner != nullptr);
  if (SgOmpClauseBodyStatement *statement = isSgOmpClauseBodyStatement(owner)) {
    return statement->get_clause_list();
  }
  if (SgOmpClauseStatement *statement = isSgOmpClauseStatement(owner)) {
    return statement->get_clause_list();
  }
  if (SgOmpGroupprivateStatement *statement =
          isSgOmpGroupprivateStatement(owner)) {
    return statement->get_clause_list();
  }
  ROSE_ABORT();
}

const SgOmpClausePtrList &forwardedClauses(SgNode *owner) {
  ROSE_ASSERT(owner != nullptr);
  if (SgOmpClauseBodyStatement *statement = isSgOmpClauseBodyStatement(owner)) {
    return statement->get_clauses();
  }
  if (SgOmpClauseStatement *statement = isSgOmpClauseStatement(owner)) {
    return statement->get_clauses();
  }
  if (SgOmpGroupprivateStatement *statement =
          isSgOmpGroupprivateStatement(owner)) {
    return statement->get_clauses();
  }
  ROSE_ABORT();
}

void validateClauseOwnership(SgNode *owner) {
  SgOmpClauseList *list = clauseList(owner);
  ROSE_ASSERT(list != nullptr && list->get_parent() == owner);
  ROSE_ASSERT(successorCount(owner, list) == 1);

  const SgOmpClausePtrList &clauses = forwardedClauses(owner);
  ROSE_ASSERT(&clauses == &list->get_clauses());
  const std::vector<SgNode *> list_successors =
      list->get_traversalSuccessorContainer();
  ROSE_ASSERT(list_successors.size() == clauses.size());
  for (SgOmpClause *clause : clauses) {
    ROSE_ASSERT(clause != nullptr && clause->get_parent() == list);
    ROSE_ASSERT(successorCount(list, clause) == 1);
    ROSE_ASSERT(successorCount(owner, clause) == 0);
  }
}

SgExprListExp *variableList(SgNode *owner) {
  ROSE_ASSERT(owner != nullptr);
  if (SgOmpFlushStatement *statement = isSgOmpFlushStatement(owner)) {
    return statement->get_variable_list();
  }
  if (SgOmpAllocateStatement *statement = isSgOmpAllocateStatement(owner)) {
    return statement->get_variable_list();
  }
  ROSE_ABORT();
}

const SgExpressionPtrList &forwardedVariables(SgNode *owner) {
  ROSE_ASSERT(owner != nullptr);
  if (SgOmpFlushStatement *statement = isSgOmpFlushStatement(owner)) {
    return statement->get_variables();
  }
  if (SgOmpAllocateStatement *statement = isSgOmpAllocateStatement(owner)) {
    return statement->get_variables();
  }
  ROSE_ABORT();
}

void validateVariableOwnership(SgNode *owner) {
  SgExprListExp *list = variableList(owner);
  ROSE_ASSERT(list != nullptr && list->get_parent() == owner);
  ROSE_ASSERT(successorCount(owner, list) == 1);

  const SgExpressionPtrList &variables = forwardedVariables(owner);
  ROSE_ASSERT(&variables == &list->get_expressions());
  for (SgExpression *variable : variables) {
    ROSE_ASSERT(variable != nullptr && variable->get_parent() == list);
    ROSE_ASSERT(successorCount(list, variable) == 1);
    ROSE_ASSERT(successorCount(owner, variable) == 0);
  }
}

void validateOwner(SgNode *owner) {
  validateClauseOwnership(owner);
  if (isSgOmpFlushStatement(owner) != nullptr ||
      isSgOmpAllocateStatement(owner) != nullptr) {
    validateVariableOwnership(owner);
  }
}

void validateDeepCopyOwnership(SgNode *original) {
  SgNode *copy = SageInterface::deepCopy(original);
  ROSE_ASSERT(copy != nullptr && copy != original &&
              copy->variantT() == original->variantT());

  SgOmpClauseList *original_list = clauseList(original);
  SgOmpClauseList *copy_list = clauseList(copy);
  ROSE_ASSERT(copy_list != original_list);
  const SgOmpClausePtrList &original_clauses = original_list->get_clauses();
  const SgOmpClausePtrList &copy_clauses = copy_list->get_clauses();
  ROSE_ASSERT(copy_clauses.size() == original_clauses.size());
  for (size_t index = 0; index < copy_clauses.size(); ++index) {
    ROSE_ASSERT(copy_clauses[index] != original_clauses[index]);
  }

  SgExprListExp *copy_variables = nullptr;
  if (isSgOmpFlushStatement(copy) != nullptr ||
      isSgOmpAllocateStatement(copy) != nullptr) {
    SgExprListExp *original_variables = variableList(original);
    copy_variables = variableList(copy);
    ROSE_ASSERT(copy_variables != original_variables);
    ROSE_ASSERT(copy_variables->get_expressions().size() ==
                original_variables->get_expressions().size());
    for (size_t index = 0; index < copy_variables->get_expressions().size();
         ++index) {
      ROSE_ASSERT(copy_variables->get_expressions()[index] !=
                  original_variables->get_expressions()[index]);
    }
  }

  validateOwner(copy);

  SgStatement *copy_statement = isSgStatement(copy);
  ROSE_ASSERT(copy_statement != nullptr &&
              copy_statement->get_parent() == nullptr);
  SgBasicBlock *transaction_owner =
      SageBuilder::buildBasicBlock(copy_statement);
  ROSE_ASSERT(transaction_owner != nullptr &&
              copy_statement->get_parent() == transaction_owner);
  validateFreshSubtreeOwnership(copy_statement, transaction_owner);
  validateOwner(copy);
}

void validateAllOwners(const OpenMPOwners &owners) {
  ROSE_ASSERT(owners.clause_body_statements.size() >= 2);
  ROSE_ASSERT(owners.clause_statements.size() >= 3);
  ROSE_ASSERT(owners.groupprivate_statements.size() == 1);
  ROSE_ASSERT(owners.flush_statements.size() == 1);
  ROSE_ASSERT(owners.allocate_statements.size() == 1);
  ROSE_ASSERT(!owners.map_clauses.empty());

  size_t mapperCount = 0;
  for (SgOmpMapClause *clause : owners.map_clauses) {
    ROSE_ASSERT(clause != nullptr);
    if (SgExpression *mapper = clause->get_mapper_identifier()) {
      ++mapperCount;
      ROSE_ASSERT(mapper->get_parent() == clause);
      ROSE_ASSERT(successorCount(clause, mapper) == 1);
      SgOmpNameExpression *name = isSgOmpNameExpression(mapper);
      ROSE_ASSERT(name != nullptr && name->get_spelling() == "rex_owner");
    }
  }
  ROSE_ASSERT(mapperCount == 1);

  for (SgOmpClauseBodyStatement *statement : owners.clause_body_statements) {
    validateOwner(statement);
  }
  for (SgOmpClauseStatement *statement : owners.clause_statements) {
    validateOwner(statement);
  }
  for (SgOmpGroupprivateStatement *statement : owners.groupprivate_statements) {
    validateOwner(statement);
  }
}

void validateAllCopies(const OpenMPOwners &owners) {
  for (SgOmpClauseBodyStatement *statement : owners.clause_body_statements) {
    validateDeepCopyOwnership(statement);
  }
  for (SgOmpClauseStatement *statement : owners.clause_statements) {
    validateDeepCopyOwnership(statement);
  }
  for (SgOmpGroupprivateStatement *statement : owners.groupprivate_statements) {
    validateDeepCopyOwnership(statement);
  }
}

void validateCheckedReplacement() {
  SgExprListExp *groupprivate_variables = SageBuilder::buildExprListExp();
  SgOmpGroupprivateStatement *groupprivate =
      new SgOmpGroupprivateStatement(groupprivate_variables);
  groupprivate_variables->set_parent(groupprivate);
  SgOmpClauseList *replacement_clauses = new SgOmpClauseList();
  groupprivate->replace_clause_list(replacement_clauses);
  ROSE_ASSERT(groupprivate->get_clause_list() == replacement_clauses);
  validateOwner(groupprivate);

  SgOmpFlushStatement *flush = new SgOmpFlushStatement();
  SgExprListExp *replacement_variables = SageBuilder::buildExprListExp();
  SgIntVal *variable = SageBuilder::buildIntVal(17);
  replacement_variables->get_expressions().push_back(variable);
  variable->set_parent(replacement_variables);
  flush->replace_variable_list(replacement_variables);
  ROSE_ASSERT(flush->get_variable_list() == replacement_variables);
  validateOwner(flush);
}

void validateAppendRemove(const OpenMPOwners &owners) {
  SgOmpClauseList *source_list =
      clauseList(owners.clause_body_statements.front());
  ROSE_ASSERT(!source_list->get_clauses().empty());
  SgOmpClause *clause =
      SageInterface::deepCopy(source_list->get_clauses().front());
  ROSE_ASSERT(clause != nullptr && clause->get_parent() == nullptr);

  SgOmpClauseList *list = new SgOmpClauseList();
  list->append_clause(clause);
  ROSE_ASSERT(list->get_clauses().size() == 1 &&
              list->get_clauses().front() == clause &&
              clause->get_parent() == list);
  list->remove_clause(clause);
  ROSE_ASSERT(list->get_clauses().empty() && clause->get_parent() == nullptr);
  list->append_clause(clause);
  ROSE_ASSERT(clause->get_parent() == list);
}

void triggerExpectedFailure(const std::string &mode,
                            const OpenMPOwners &owners) {
  SgOmpClauseBodyStatement *first = owners.clause_body_statements.at(0);
  SgOmpClauseBodyStatement *second = owners.clause_body_statements.at(1);
  SgOmpClauseList *first_list = clauseList(first);
  SgOmpClauseList *second_list = clauseList(second);
  ROSE_ASSERT(!first_list->get_clauses().empty());
  SgOmpClause *first_clause = first_list->get_clauses().front();

  if (mode == "--null-clause") {
    first_list->append_clause(nullptr);
  } else if (mode == "--duplicate-clause") {
    first_list->append_clause(first_clause);
  } else if (mode == "--foreign-clause") {
    second_list->append_clause(first_clause);
  } else if (mode == "--direct-clause-owner") {
    first_clause->set_parent(first);
    (void)first_list->get_clauses();
  } else if (mode == "--wrong-list-owner") {
    first_list->set_parent(second);
    (void)first->get_clauses();
  } else if (mode == "--wrong-variable-owner") {
    SgOmpFlushStatement *flush = owners.flush_statements.front();
    ROSE_ASSERT(!flush->get_variables().empty());
    flush->get_variables().front()->set_parent(flush);
    (void)flush->get_variables();
  } else if (mode == "--null-variable") {
    owners.flush_statements.front()->append_variable(nullptr);
  } else if (mode == "--replace-nonempty-list") {
    first->replace_clause_list(new SgOmpClauseList());
  } else if (mode == "--replace-owned-list") {
    owners.groupprivate_statements.front()->replace_clause_list(second_list);
  } else if (mode == "--replace-null-list") {
    owners.groupprivate_statements.front()->replace_clause_list(nullptr);
  } else if (mode == "--replace-nonempty-variable-list") {
    owners.flush_statements.front()->replace_variable_list(
        SageBuilder::buildExprListExp());
  } else if (mode == "--replace-owned-variable-list") {
    SgOmpFlushStatement *empty_flush = new SgOmpFlushStatement();
    empty_flush->replace_variable_list(
        owners.allocate_statements.front()->get_variable_list());
  } else if (mode == "--replace-malformed-variable-list") {
    SgOmpFlushStatement *empty_flush = new SgOmpFlushStatement();
    SgExprListExp *malformed = SageBuilder::buildExprListExp();
    malformed->get_expressions().push_back(SageBuilder::buildIntVal(23));
    empty_flush->replace_variable_list(malformed);
  } else if (mode == "--remove-null-clause") {
    first_list->remove_clause(nullptr);
  } else if (mode == "--remove-foreign-clause") {
    ROSE_ASSERT(!second_list->get_clauses().empty());
    first_list->remove_clause(second_list->get_clauses().front());
  } else {
    ROSE_ABORT();
  }

  ROSE_ABORT();
}

} // namespace

int main(int argc, char **argv) {
  std::string mode;
  std::vector<char *> frontend_arguments;
  frontend_arguments.push_back(argv[0]);
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--null-clause" || argument == "--duplicate-clause" ||
        argument == "--foreign-clause" || argument == "--direct-clause-owner" ||
        argument == "--wrong-list-owner" ||
        argument == "--wrong-variable-owner" || argument == "--null-variable" ||
        argument == "--replace-nonempty-list" ||
        argument == "--replace-owned-list" ||
        argument == "--replace-null-list" ||
        argument == "--replace-nonempty-variable-list" ||
        argument == "--replace-owned-variable-list" ||
        argument == "--replace-malformed-variable-list" ||
        argument == "--remove-null-clause" ||
        argument == "--remove-foreign-clause") {
      ROSE_ASSERT(mode.empty());
      mode = argument;
    } else {
      frontend_arguments.push_back(argv[index]);
    }
  }

  SgProject *project = frontend(static_cast<int>(frontend_arguments.size()),
                                frontend_arguments.data());
  ROSE_ASSERT(project != nullptr);

  CollectOpenMPOwners collector;
  collector.traverse(project, preorder);
  validateAllOwners(collector.owners);
  AstTests::runAllTests(project);

  if (!mode.empty()) {
    triggerExpectedFailure(mode, collector.owners);
  }

  validateAllCopies(collector.owners);
  validateCheckedReplacement();
  validateAppendRemove(collector.owners);
  return 0;
}
