#include "rose.h"

#include <iostream>
#include <string>

namespace {

class OwnedClausePayloadValidator : public AstSimpleProcessing {
public:
  bool valid() const { return valid_; }
  size_t iteratorCount() const { return iterator_count_; }
  size_t userReductionIdentifierCount() const {
    return user_reduction_identifier_count_;
  }
  size_t mapperIdentifierCount() const { return mapper_identifier_count_; }
  size_t declareTargetExtendedListCount() const {
    return declare_target_extended_list_count_;
  }
  size_t sinkVectorCount() const { return sink_vector_count_; }
  size_t collidingIteratorRoleCount() const {
    return colliding_iterator_role_count_;
  }

private:
  bool valid_ = true;
  size_t iterator_count_ = 0;
  size_t user_reduction_identifier_count_ = 0;
  size_t mapper_identifier_count_ = 0;
  size_t declare_target_extended_list_count_ = 0;
  size_t sink_vector_count_ = 0;
  size_t colliding_iterator_role_count_ = 0;

  void fail(SgNode *node, const std::string &message) {
    std::cerr << "invalid "
              << (node != nullptr ? node->sage_class_name() : "OpenMP node")
              << " after AST JSON reconstruction: " << message << "\n";
    valid_ = false;
  }

  void checkIterator(SgOmpIteratorDefinition *definition) {
    SgOmpClause *owner = definition != nullptr
                             ? isSgOmpClause(definition->get_parent())
                             : nullptr;
    const SgOmpIteratorDefinitionPtrList *definitions = nullptr;
    if (SgOmpMapClause *clause = isSgOmpMapClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpDependClause *clause = isSgOmpDependClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpAffinityClause *clause = isSgOmpAffinityClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpToClause *clause = isSgOmpToClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    } else if (SgOmpFromClause *clause = isSgOmpFromClause(owner)) {
      definitions = &clause->get_iterator_definitions();
    }
    if (definitions == nullptr || definitions->size() != 1 ||
        definitions->front() != definition ||
        definition->get_iterator_type() == nullptr ||
        definition->get_iterator_name() == nullptr ||
        definition->get_begin() == nullptr ||
        definition->get_end() == nullptr || definition->get_step() == nullptr ||
        definition->get_iterator_type()->get_parent() != definition ||
        definition->get_iterator_name()->get_parent() != definition ||
        definition->get_begin()->get_parent() != definition ||
        definition->get_end()->get_parent() != definition ||
        definition->get_step()->get_parent() != definition) {
      fail(definition, "typed iterator fields lost exact structural ownership");
      return;
    }
    if (definition->get_iterator_name()->get_spelling() ==
        "rex_omp_iterator_role") {
      ++colliding_iterator_role_count_;
    }
    ++iterator_count_;
  }

  template <typename ClauseT>
  void checkUserReductionIdentifier(ClauseT *clause,
                                    const std::string &expected) {
    SgOmpNameExpression *identifier =
        clause != nullptr ? clause->get_user_defined_identifier() : nullptr;
    if (identifier == nullptr || identifier->get_parent() != clause ||
        identifier->get_spelling() != expected) {
      fail(clause, "user reduction identifier lost its structural syntax node");
      return;
    }
    ++user_reduction_identifier_count_;
  }

  template <typename ClauseT> void checkMapperIdentifier(ClauseT *clause) {
    SgOmpNameExpression *identifier =
        clause != nullptr ? clause->get_mapper_identifier() : nullptr;
    if (identifier == nullptr || identifier->get_parent() != clause ||
        identifier->get_spelling() != "default") {
      fail(clause, "mapper identifier lost its typed structural syntax node");
      return;
    }
    ++mapper_identifier_count_;
  }

  void visit(SgNode *node) override {
    if (SgOmpIteratorDefinition *definition = isSgOmpIteratorDefinition(node)) {
      checkIterator(definition);
    }
    if (SgOmpReductionClause *clause = isSgOmpReductionClause(node)) {
      if (clause->get_identifier() ==
          SgOmpClause::e_omp_reduction_user_defined_identifier) {
        checkUserReductionIdentifier(clause, "rex_reduction");
      }
    }
    if (SgOmpInReductionClause *clause = isSgOmpInReductionClause(node)) {
      if (clause->get_identifier() ==
          SgOmpClause::e_omp_in_reduction_user_defined_identifier) {
        checkUserReductionIdentifier(clause, "rex_in_reduction");
      }
    }
    if (SgOmpTaskReductionClause *clause = isSgOmpTaskReductionClause(node)) {
      if (clause->get_identifier() ==
          SgOmpClause::e_omp_task_reduction_user_defined_identifier) {
        checkUserReductionIdentifier(clause, "rex_task_reduction");
      }
    }
    if (SgOmpMapClause *clause = isSgOmpMapClause(node)) {
      if (clause->get_mapper_identifier() != nullptr) {
        checkMapperIdentifier(clause);
      }
    }
    if (SgOmpToClause *clause = isSgOmpToClause(node)) {
      if (clause->get_mapper_identifier() != nullptr) {
        checkMapperIdentifier(clause);
      }
      if (clause->get_declare_target_extended_list()) {
        if (clause->get_kind() != SgOmpClause::e_omp_to_kind_unknown ||
            clause->get_mapper_identifier() != nullptr ||
            !clause->get_iterator_definitions().empty()) {
          fail(clause,
               "declare-target extended-list discriminator was not restored");
          return;
        }
        ++declare_target_extended_list_count_;
      }
    }
    if (SgOmpFromClause *clause = isSgOmpFromClause(node)) {
      if (clause->get_mapper_identifier() != nullptr) {
        checkMapperIdentifier(clause);
      }
    }
    if (SgOmpDependClause *clause = isSgOmpDependClause(node)) {
      if (clause->get_dependence_type() != SgOmpClause::e_omp_depend_sink) {
        return;
      }
      SgExprListExp *vectors = clause->get_sink_vectors();
      if (vectors == nullptr || vectors->get_parent() != clause ||
          vectors->get_expressions().size() != 1 ||
          vectors->get_expressions().front() == nullptr ||
          vectors->get_expressions().front()->get_parent() != vectors) {
        fail(clause, "sink vector lost its structural list ownership");
        return;
      }
      ++sink_vector_count_;
    }
  }
};

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  if (project == nullptr || frontendExitStatus(project) != 0) {
    std::cerr
        << "OpenMP owned-clause AST JSON fixture failed in the frontend\n";
    return 1;
  }

  OwnedClausePayloadValidator validator;
  validator.traverse(project, preorder);
  if (!validator.valid() || validator.iteratorCount() != 5 ||
      validator.userReductionIdentifierCount() != 3 ||
      validator.mapperIdentifierCount() != 3 ||
      validator.declareTargetExtendedListCount() != 1 ||
      validator.sinkVectorCount() != 1 ||
      validator.collidingIteratorRoleCount() != 1) {
    std::cerr << "OpenMP AST JSON round trip did not reconstruct every owned "
                 "clause payload\n";
    return 1;
  }

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
