#include "rose.h"

#include <memory>
#include <unordered_set>

namespace {
void collectLexicalTypes(SgType *type, std::unordered_set<SgType *> &seen,
                         std::unordered_set<SgType *> &lexical) {
  ROSE_ASSERT(type != nullptr);
  if (!seen.insert(type).second)
    return;
  SgType *base = nullptr;
  if (auto *decl = isSgDeclType(type)) {
    ROSE_ASSERT(decl->get_base_expression() != nullptr);
    ROSE_ASSERT(decl->get_base_expression()->get_parent() == decl);
    lexical.insert(type);
    base = decl->get_base_type();
  } else if (auto *typeOf = isSgTypeOfType(type)) {
    if (auto *operand = typeOf->get_base_expression())
      ROSE_ASSERT(operand->get_parent() == typeOf);
    lexical.insert(type);
    base = typeOf->get_base_type();
  } else if (auto *modifier = isSgModifierType(type)) {
    base = modifier->get_base_type();
  } else if (auto *pointer = isSgPointerType(type)) {
    base = pointer->get_base_type();
  } else if (auto *reference = isSgReferenceType(type)) {
    base = reference->get_base_type();
  } else if (auto *reference = isSgRvalueReferenceType(type)) {
    base = reference->get_base_type();
  } else if (auto *array = isSgArrayType(type)) {
    base = array->get_base_type();
  }
  if (base != nullptr)
    collectLexicalTypes(base, seen, lexical);
}

std::unordered_set<SgType *> lexicalTypes(SgProject *project) {
  std::unordered_set<SgType *> seen, lexical;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName))
    collectLexicalTypes(isSgInitializedName(node)->get_type(), seen, lexical);
  return lexical;
}

void requireTypeSourceContext(SgType *type, SgSourceFile *source,
                              std::unordered_set<SgType *> &seen,
                              size_t &statements) {
  if (!seen.insert(type).second)
    return;
  if (auto *typeOf = isSgTypeOfType(type)) {
    if (auto *operand = typeOf->get_base_expression()) {
      for (SgNode *node :
           NodeQuery::querySubTree(operand, V_SgStatementExpression)) {
        auto *expression = isSgStatementExpression(node);
        auto *body = isSgBasicBlock(expression->get_statement());
        ROSE_ASSERT(body != nullptr && body->get_parent() == expression);
        ROSE_ASSERT(body->get_statement_expression_semantic_scope() != nullptr);
        ROSE_ASSERT(SageInterface::getEnclosingSourceFile(body) == source);
        ROSE_ASSERT(SageInterface::getEnclosingFileNode(body) == source);
        ++statements;
      }
    }
    requireTypeSourceContext(typeOf->get_base_type(), source, seen, statements);
  } else if (auto *modifier = isSgModifierType(type)) {
    requireTypeSourceContext(modifier->get_base_type(), source, seen,
                             statements);
  } else if (auto *pointer = isSgPointerType(type)) {
    requireTypeSourceContext(pointer->get_base_type(), source, seen,
                             statements);
  } else if (auto *array = isSgArrayType(type)) {
    requireTypeSourceContext(array->get_base_type(), source, seen, statements);
  }
}

void requireProjectSourceContext(SgProject *project) {
  size_t statements = 0;
  std::unordered_set<SgType *> seen;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *name = isSgInitializedName(node);
    auto *source = SageInterface::getEnclosingSourceFile(name);
    ROSE_ASSERT(source != nullptr);
    requireTypeSourceContext(name->get_type(), source, seen, statements);
  }
  auto *source = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(source != nullptr);
  if (source->get_C_only()) {
    ROSE_ASSERT(statements > 0);
    size_t inlineTraits = 0;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgSizeOfOp)) {
      auto *operation = isSgSizeOfOp(node);
      if (operation->get_type_defining_declaration() != nullptr) {
        ROSE_ASSERT(operation->get_operand_type() != nullptr);
        ROSE_ASSERT(operation->get_type_defining_declaration()->get_parent() ==
                    operation);
        ++inlineTraits;
      }
    }
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgAlignOfOp)) {
      auto *operation = isSgAlignOfOp(node);
      if (operation->get_type_defining_declaration() != nullptr) {
        ROSE_ASSERT(operation->get_operand_type() != nullptr);
        ROSE_ASSERT(operation->get_type_defining_declaration()->get_parent() ==
                    operation);
        ++inlineTraits;
      }
    }
    ROSE_ASSERT(inlineTraits == 2);
  }
}
} // namespace

int main(int argc, char **argv) {
  auto *table = SgNode::get_globalTypeTable();
  auto *functions = SgNode::get_globalFunctionTypeTable();
  auto *pointer = SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  const auto tableName = SageInterface::generateUniqueName(table, false);
  const auto functionTableName =
      SageInterface::generateUniqueName(functions, false);
  ROSE_ASSERT(!tableName.empty() && !functionTableName.empty());
  ROSE_ASSERT(tableName != functionTableName);
  for (int iteration = 0; iteration != 2; ++iteration) {
    {
      std::unique_ptr<SgProject> project(frontend(argc, argv));
      ROSE_ASSERT(table == SgNode::get_globalTypeTable());
      ROSE_ASSERT(functions == SgNode::get_globalFunctionTypeTable());
      ROSE_ASSERT(table->get_parent() == nullptr);
      ROSE_ASSERT(functions->get_parent() == nullptr);
      ROSE_ASSERT(SageInterface::generateUniqueName(table, false) == tableName);
      ROSE_ASSERT(SageInterface::generateUniqueName(functions, false) ==
                  functionTableName);
      ROSE_ASSERT(SageBuilder::buildPointerType(SageBuilder::buildVoidType()) ==
                  pointer);
      AstTests::runAllTests(project.get());
      requireProjectSourceContext(project.get());
      const auto originalTypes = lexicalTypes(project.get());
      std::unique_ptr<SgProject> copy(
          isSgProject(SageInterface::deepCopy(project.get())));
      ROSE_ASSERT(copy != nullptr && copy.get() != project.get());
      AstTests::runAllTests(copy.get());
      requireProjectSourceContext(copy.get());
      ROSE_ASSERT(lexicalTypes(project.get()) == originalTypes);
      const auto copiedTypes = lexicalTypes(copy.get());
      ROSE_ASSERT(copiedTypes.size() == originalTypes.size());
      for (auto *type : copiedTypes)
        ROSE_ASSERT(originalTypes.count(type) == 0);
      auto *source = isSgSourceFile(&project->get_file(0));
      if (source->get_Cxx_only()) {
        size_t decltypes = 0;
        for (auto *type : originalTypes)
          decltypes += isSgDeclType(type) != nullptr;
        ROSE_ASSERT(decltypes >= 3);
      }
      if (iteration == 1) {
        const int status = backend(project.get());
        if (status != 0)
          return status;
      }
      SgFileList *copiedFiles = copy->get_fileList_ptr();
      SageInterface::deleteAST(copy.release(),
                               SageInterface::DeleteAstMode::kRequireIsolated);
      ROSE_ASSERT(!SgNode::isLiveNode(copiedFiles));
      AstTests::runAllTests(project.get());
      SgFileList *originalFiles = project->get_fileList_ptr();
      SageInterface::deleteAST(project.release(),
                               SageInterface::DeleteAstMode::kRequireIsolated);
      ROSE_ASSERT(!SgNode::isLiveNode(originalFiles));
    }
    ROSE_ASSERT(table->get_parent() == nullptr);
    ROSE_ASSERT(functions->get_parent() == nullptr);
    ROSE_ASSERT(SageInterface::generateUniqueName(table, false) == tableName);
    ROSE_ASSERT(SageInterface::generateUniqueName(functions, false) ==
                functionTableName);
    ROSE_ASSERT(SageBuilder::buildPointerType(SageBuilder::buildVoidType()) ==
                pointer);
  }
  return 0;
}
