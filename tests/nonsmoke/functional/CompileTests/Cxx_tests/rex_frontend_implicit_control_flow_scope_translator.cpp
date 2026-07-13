#include "RoseAst.h"
#include "rose.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct OwnerCounts {
  size_t ifStatements = 0;
  size_t forStatements = 0;
  size_t rangeForStatements = 0;
  size_t whileStatements = 0;
  size_t doWhileStatements = 0;
  size_t switchStatements = 0;
};

bool hasExactControlledEdge(SgBasicBlock *block, OwnerCounts &counts) {
  SgNode *parent = block->get_parent();
  if (SgIfStmt *statement = isSgIfStmt(parent)) {
    if (statement->get_true_body() == block ||
        statement->get_false_body() == block) {
      ++counts.ifStatements;
      return true;
    }
  } else if (SgRangeBasedForStatement *statement =
                 isSgRangeBasedForStatement(parent)) {
    if (statement->get_loop_body() == block) {
      ++counts.rangeForStatements;
      return true;
    }
  } else if (SgForStatement *statement = isSgForStatement(parent)) {
    if (statement->get_loop_body() == block) {
      ++counts.forStatements;
      return true;
    }
  } else if (SgWhileStmt *statement = isSgWhileStmt(parent)) {
    if (statement->get_body() == block) {
      ++counts.whileStatements;
      return true;
    }
  } else if (SgDoWhileStmt *statement = isSgDoWhileStmt(parent)) {
    if (statement->get_body() == block) {
      ++counts.doWhileStatements;
      return true;
    }
  } else if (SgSwitchStatement *statement = isSgSwitchStatement(parent)) {
    if (statement->get_body() == block) {
      ++counts.switchStatements;
      return true;
    }
  }
  return false;
}

bool hasExactSynthesizedProvenance(const Sg_File_Info *position) {
  return position != nullptr && position->isCompilerGenerated() &&
         position->isFrontendSpecific() && !position->isTransformation() &&
         position->isOutputInCodeGeneration() &&
         position->get_physical_file_id() < 0;
}

SgFunctionDefinition *findTargetFunction(SgProject *project) {
  SgFunctionDefinition *result = nullptr;
  for (SgNode *node : RoseAst(project)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    if (declaration == nullptr ||
        declaration->get_name() != "rex_implicit_control_flow_scope" ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = declaration->get_definition();
  }
  return result;
}

SgClassDeclaration *validateImplicitScope(SgBasicBlock *block,
                                          OwnerCounts &counts) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(block->get_is_implicit_control_flow_scope());
  ROSE_ASSERT(hasExactControlledEdge(block, counts));
  ROSE_ASSERT(!block->get_is_fortran_block_construct());
  ROSE_ASSERT(hasExactSynthesizedProvenance(block->get_file_info()));
  ROSE_ASSERT(hasExactSynthesizedProvenance(block->get_startOfConstruct()));
  ROSE_ASSERT(hasExactSynthesizedProvenance(block->get_endOfConstruct()));
  ROSE_ASSERT(block->get_statements().size() == 1);

  SgVariableDeclaration *variable =
      isSgVariableDeclaration(block->get_statements().front());
  ROSE_ASSERT(variable != nullptr);
  ROSE_ASSERT(variable->get_parent() == block);
  ROSE_ASSERT(variable->get_scope() == block);
  ROSE_ASSERT(variable->get_variables().size() == 1);
  SgInitializedName *initializedName = variable->get_variables().front();
  ROSE_ASSERT(initializedName != nullptr);
  ROSE_ASSERT(initializedName->get_parent() == variable);
  ROSE_ASSERT(initializedName->get_scope() == block);

  SgClassDeclaration *inlineClass =
      isSgClassDeclaration(variable->get_baseTypeDefiningDeclaration());
  ROSE_ASSERT(inlineClass != nullptr);
  ROSE_ASSERT(inlineClass->get_parent() == variable);
  ROSE_ASSERT(inlineClass->get_scope() == block);
  ROSE_ASSERT(inlineClass->get_definition() != nullptr);
  ROSE_ASSERT(inlineClass->get_definition()->get_parent() == inlineClass);
  return inlineClass;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgFunctionDefinition *function = findTargetFunction(project);
  ROSE_ASSERT(function != nullptr);
  SgBasicBlock *functionBody = function->get_body();
  ROSE_ASSERT(functionBody != nullptr);
  ROSE_ASSERT(!functionBody->get_is_implicit_control_flow_scope());

  OwnerCounts counts;
  std::vector<SgBasicBlock *> implicitScopes;
  std::vector<SgClassDeclaration *> ifClasses;
  size_t explicitBlocks = 0;
  size_t directExpressionBodies = 0;
  for (SgNode *node : RoseAst(functionBody)) {
    SgBasicBlock *block = isSgBasicBlock(node);
    if (block != nullptr && block->get_is_implicit_control_flow_scope()) {
      SgClassDeclaration *inlineClass = validateImplicitScope(block, counts);
      implicitScopes.push_back(block);
      if (inlineClass->get_name() == "RexIfLocal") {
        ifClasses.push_back(inlineClass);
      }
    }
    SgIfStmt *ifStatement = isSgIfStmt(node);
    if (ifStatement == nullptr || ifStatement->get_false_body() != nullptr) {
      continue;
    }
    if (SgBasicBlock *trueBlock =
            isSgBasicBlock(ifStatement->get_true_body())) {
      if (!trueBlock->get_is_implicit_control_flow_scope()) {
        ++explicitBlocks;
      }
    } else if (isSgExprStatement(ifStatement->get_true_body()) != nullptr) {
      ++directExpressionBodies;
    }
  }

  ROSE_ASSERT(implicitScopes.size() == 7);
  ROSE_ASSERT(counts.ifStatements == 2);
  ROSE_ASSERT(counts.forStatements == 1);
  ROSE_ASSERT(counts.rangeForStatements == 1);
  ROSE_ASSERT(counts.whileStatements == 1);
  ROSE_ASSERT(counts.doWhileStatements == 1);
  ROSE_ASSERT(counts.switchStatements == 1);
  ROSE_ASSERT(explicitBlocks == 1);
  ROSE_ASSERT(directExpressionBodies == 1);
  ROSE_ASSERT(ifClasses.size() == 2);
  ROSE_ASSERT(ifClasses[0] != ifClasses[1]);
  ROSE_ASSERT(ifClasses[0]->get_type() != ifClasses[1]->get_type());
  ROSE_ASSERT(ifClasses[0]->get_scope() != ifClasses[1]->get_scope());

  if (std::getenv("REX_TEST_MALFORMED_IMPLICIT_CONTROL_FLOW_SCOPE") !=
      nullptr) {
    implicitScopes.front()->set_is_fortran_block_construct(true);
  } else {
    AstTests::runAllTests(project);
  }

  return backend(project);
}
