#include "nodeQuery.h"
#include "rose.h"

namespace {

void verifyExactSizeAndThisResultTypes(SgProject *project) {
  const Rose_STL_Container<SgNode *> files =
      NodeQuery::querySubTree(project, V_SgSourceFile);
  ROSE_ASSERT(files.size() == 1);
  SgSourceFile *source_file = isSgSourceFile(files.front());
  ROSE_ASSERT(source_file != nullptr);

  SgType *target_size_type = SageInterface::requireTargetSizeType(source_file);
  ROSE_ASSERT(target_size_type != nullptr);
  ROSE_ASSERT(source_file->get_target_size_type() == target_size_type);

  const Rose_STL_Container<SgNode *> sizeof_nodes =
      NodeQuery::querySubTree(project, V_SgSizeOfOp);
  ROSE_ASSERT(sizeof_nodes.size() >= 2);
  for (SgNode *node : sizeof_nodes) {
    SgSizeOfOp *size_of = isSgSizeOfOp(node);
    ROSE_ASSERT(size_of != nullptr);
    ROSE_ASSERT(size_of->get_expression_type() == target_size_type);
    ROSE_ASSERT(size_of->get_type() == target_size_type);
  }

  const Rose_STL_Container<SgNode *> alignof_nodes =
      NodeQuery::querySubTree(project, V_SgAlignOfOp);
  ROSE_ASSERT(!alignof_nodes.empty());
  for (SgNode *node : alignof_nodes) {
    SgAlignOfOp *align_of = isSgAlignOfOp(node);
    ROSE_ASSERT(align_of != nullptr);
    ROSE_ASSERT(align_of->get_expression_type() == target_size_type);
    ROSE_ASSERT(align_of->get_type() == target_size_type);
  }

  bool found_unqualified_this = false;
  bool found_const_volatile_this = false;
  const Rose_STL_Container<SgNode *> this_nodes =
      NodeQuery::querySubTree(project, V_SgThisExp);
  ROSE_ASSERT(this_nodes.size() >= 3);
  for (SgNode *node : this_nodes) {
    SgThisExp *this_expression = isSgThisExp(node);
    ROSE_ASSERT(this_expression != nullptr);
    SgType *stored_type = this_expression->get_expression_type();
    ROSE_ASSERT(stored_type != nullptr);
    ROSE_ASSERT(this_expression->get_type() == stored_type);
    SgPointerType *pointer_type = isSgPointerType(stored_type);
    ROSE_ASSERT(pointer_type != nullptr);
    SgType *pointee_type = pointer_type->get_base_type();
    ROSE_ASSERT(pointee_type != nullptr);
    SgSymbol *this_symbol =
        this_expression->get_class_symbol() != nullptr
            ? static_cast<SgSymbol *>(this_expression->get_class_symbol())
            : static_cast<SgSymbol *>(this_expression->get_nonreal_symbol());
    ROSE_ASSERT(this_symbol != nullptr);
    SgType *symbol_type = this_symbol->get_type();
    ROSE_ASSERT(symbol_type != nullptr);
    ROSE_ASSERT(pointee_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                        SgType::STRIP_TYPEDEF_TYPE) ==
                symbol_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                       SgType::STRIP_TYPEDEF_TYPE));
    const bool is_const = SageInterface::isConstType(pointee_type);
    const bool is_volatile = SageInterface::isVolatileType(pointee_type);
    found_unqualified_this |= !is_const && !is_volatile;
    found_const_volatile_this |= is_const && is_volatile;
  }
  ROSE_ASSERT(found_unqualified_this);
  ROSE_ASSERT(found_const_volatile_this);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  verifyExactSizeAndThisResultTypes(project);

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
