#include "rose.h"

#include <cctype>
#include <iostream>
#include <set>
#include <string>

namespace {

class WrapperOwnershipValidator : public AstSimpleProcessing {
public:
  bool valid() const { return valid_; }

  size_t clauseStatementCount() const { return clause_statement_count_; }
  size_t clauseBodyStatementCount() const {
    return clause_body_statement_count_;
  }
  size_t groupprivateCount() const { return groupprivate_count_; }
  size_t flushCount() const { return flush_count_; }
  size_t allocateCount() const { return allocate_count_; }

private:
  bool valid_ = true;
  size_t clause_statement_count_ = 0;
  size_t clause_body_statement_count_ = 0;
  size_t groupprivate_count_ = 0;
  size_t flush_count_ = 0;
  size_t allocate_count_ = 0;
  std::set<SgOmpClauseList *> clause_lists_;
  std::set<SgExprListExp *> variable_lists_;

  void fail(SgNode *node, const std::string &message) {
    std::cerr << "invalid "
              << (node != nullptr ? node->sage_class_name() : "OpenMP node")
              << " after AST JSON reconstruction: " << message << "\n";
    valid_ = false;
  }

  void checkClauseList(SgNode *owner, SgOmpClauseList *list) {
    if (list == nullptr) {
      fail(owner, "missing structural clause-list wrapper");
      return;
    }
    if (list->get_parent() != owner) {
      fail(owner, "clause-list wrapper has the wrong parent");
      return;
    }
    if (!clause_lists_.insert(list).second) {
      fail(owner, "clause-list wrapper is shared by multiple statements");
      return;
    }
    for (SgOmpClause *clause : list->get_clauses()) {
      if (clause == nullptr || clause->get_parent() != list) {
        fail(owner, "clause is not owned by its structural wrapper");
      }
    }
  }

  void checkVariableList(SgNode *owner, SgExprListExp *list) {
    if (list == nullptr) {
      fail(owner, "missing structural variable-list wrapper");
      return;
    }
    if (list->get_parent() != owner) {
      fail(owner, "variable-list wrapper has the wrong parent");
      return;
    }
    if (!variable_lists_.insert(list).second) {
      fail(owner, "variable-list wrapper is shared by multiple statements");
      return;
    }
    if (list->get_expressions().empty()) {
      fail(owner, "test fixture produced an empty variable-list wrapper");
    }
    for (SgExpression *expression : list->get_expressions()) {
      if (expression == nullptr || expression->get_parent() != list) {
        fail(owner, "variable expression is not owned by its wrapper");
      }
    }
  }

  void visit(SgNode *node) override {
    if (SgOmpClauseStatement *statement = isSgOmpClauseStatement(node)) {
      ++clause_statement_count_;
      checkClauseList(statement, statement->get_clause_list());
    } else if (SgOmpClauseBodyStatement *statement =
                   isSgOmpClauseBodyStatement(node)) {
      ++clause_body_statement_count_;
      checkClauseList(statement, statement->get_clause_list());
    }
    if (SgOmpGroupprivateStatement *statement =
            isSgOmpGroupprivateStatement(node)) {
      ++groupprivate_count_;
      checkClauseList(statement, statement->get_clause_list());
    }
    if (SgOmpFlushStatement *statement = isSgOmpFlushStatement(node)) {
      ++flush_count_;
      checkVariableList(statement, statement->get_variable_list());
    }
    if (SgOmpAllocateStatement *statement = isSgOmpAllocateStatement(node)) {
      ++allocate_count_;
      checkVariableList(statement, statement->get_variable_list());
    }
  }
};

std::string collapseWhitespace(const std::string &text) {
  std::string result;
  bool pending_space = false;
  for (unsigned char ch : text) {
    if (std::isspace(ch)) {
      pending_space = !result.empty();
      continue;
    }
    if (pending_space) {
      result += ' ';
      pending_space = false;
    }
    result += static_cast<char>(ch);
  }
  return result;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  if (project == nullptr || frontendExitStatus(project) != 0) {
    std::cerr << "OpenMP AST JSON wrapper fixture failed in the frontend\n";
    return 1;
  }

  WrapperOwnershipValidator validator;
  validator.traverse(project, preorder);
  if (!validator.valid() || validator.clauseStatementCount() < 3 ||
      validator.clauseBodyStatementCount() < 1 ||
      validator.groupprivateCount() != 1 || validator.flushCount() != 1 ||
      validator.allocateCount() != 1) {
    std::cerr << "OpenMP AST JSON wrapper fixture did not reconstruct the "
                 "expected statements\n";
    return 1;
  }

  const std::string unparsed = collapseWhitespace(project->unparseToString());
  for (const std::string &surface :
       {"omp groupprivate", "omp allocate", "omp flush", "omp target update",
        "omp parallel"}) {
    if (unparsed.find(surface) == std::string::npos) {
      std::cerr << "OpenMP AST JSON wrapper round trip lost unparse surface '"
                << surface << "'\n";
      return 1;
    }
  }

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
