#include "astJson/sageAstJson.h"
#include "astJson/sageAstJsonPrivate.h"
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

  const auto new_nodes = NodeQuery::querySubTree(project, V_SgNewExp);
  ROSE_ASSERT(new_nodes.size() == 15);
  unsigned template_alias_uses = 0;
  for (SgNode *node : new_nodes) {
    auto *allocation = isSgNewExp(node);
    auto *initializer = isSgAssignInitializer(allocation->get_parent());
    ROSE_ASSERT(initializer != nullptr);
    auto *declaration = isSgInitializedName(initializer->get_parent());
    ROSE_ASSERT(declaration != nullptr);
    auto *declared_pointer = isSgPointerType(declaration->get_type());
    auto *result_pointer = isSgPointerType(allocation->get_type());
    ROSE_ASSERT(declared_pointer != nullptr && result_pointer != nullptr);
    ROSE_ASSERT(
        SageInterface::isEquivalentType(result_pointer, declared_pointer));
    auto *source_nonreal =
        isSgNonrealType(allocation->get_specified_type()->stripType(
            SgType::STRIP_MODIFIER_TYPE));
    if (source_nonreal != nullptr) {
      auto *source_declaration =
          isSgNonrealDecl(source_nonreal->get_declaration());
      ROSE_ASSERT(source_declaration != nullptr);
      auto *alias = isSgTemplateInstantiationTypedefDeclaration(
          source_declaration->get_templateDeclaration());
      ROSE_ASSERT(alias != nullptr &&
                  isSgArrayType(alias->get_base_type()) != nullptr);
      ROSE_ASSERT(SageInterface::cxxSourceTypeMatchesSemanticType(
          source_nonreal, alias->get_type()));
      ROSE_ASSERT(!SgNewExp::specifiedTypeMatchesResult(
          allocation->get_specified_type(), result_pointer,
          SgNewExp::e_no_array_bound));
      ++template_alias_uses;
    }
    if (auto *bound = allocation->get_array_bound()) {
      ROSE_ASSERT(bound->get_parent() == allocation);
      ROSE_ASSERT(allocation->cfgFindChildIndex(bound) == 1);
      const auto outgoing = allocation->cfgOutEdges(1);
      ROSE_ASSERT(outgoing.size() == 1);
      ROSE_ASSERT(outgoing.front().target() == bound->cfgForBeginning());
      const auto incoming = allocation->cfgInEdges(2);
      ROSE_ASSERT(incoming.size() == 1);
      ROSE_ASSERT(incoming.front().source() == bound->cfgForEnd());
    } else {
      ROSE_ASSERT(allocation->get_array_bound_source_form() ==
                  SgNewExp::e_no_array_bound);
    }
    SgTreeCopy copy;
    auto *copied = isSgNewExp(allocation->copy(copy));
    ROSE_ASSERT(copied != nullptr && copied->get_type() == result_pointer);
    ROSE_ASSERT(copied->get_array_bound_source_form() ==
                allocation->get_array_bound_source_form());
    if (allocation->get_array_bound() != nullptr) {
      ROSE_ASSERT(copied->get_array_bound() != allocation->get_array_bound());
      ROSE_ASSERT(copied->get_array_bound()->get_parent() == copied);
    }
    SageInterface::deleteAST(copied);
  }
  ROSE_ASSERT(template_alias_uses == 5);
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  verifyExactSizeAndThisResultTypes(project);

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  const int result = backend(project);
  auto *source = isSgSourceFile(project->get_fileList().front());
  SgTypedefDeclaration *sizeAlias = nullptr;
  for (auto *node : NodeQuery::querySubTree(source, V_SgTypedefDeclaration)) {
    auto *alias = isSgTypedefDeclaration(node);
    if (alias->get_name() == "RexTargetSizeType")
      sizeAlias = alias;
  }
  ROSE_ASSERT(sizeAlias != nullptr);
  for (auto *node : Rose::AstJson::collectNodes(source)) {
    if (auto *size = isSgSizeOfOp(node))
      size->set_expression_type(sizeAlias->get_type());
    else if (auto *alignment = isSgAlignOfOp(node))
      alignment->set_expression_type(sizeAlias->get_type());
  }
  source->set_target_size_type(sizeAlias->get_type());
  verifyExactSizeAndThisResultTypes(project);
  AstTests::runAllTests(project);
  source->get_originalCommandLineArgumentList().push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  Rose::AstJson::Options options;
  options.outputDirectory = "rex-size-typedef-json";
  SgSourceFile *original = source;
  source = Rose::AstJson::roundTripSourceFile(
      source, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(source != original && !SgNode::isLiveNode(original));
  auto *restored =
      isSgTypedefType(SageInterface::requireTargetSizeType(source));
  ROSE_ASSERT(restored != nullptr);
  auto *alias = isSgTypedefDeclaration(restored->get_declaration());
  ROSE_ASSERT(alias != nullptr && alias->get_name() == "RexTargetSizeType");
  ROSE_ASSERT(alias->get_scope() == source->get_globalScope());
  verifyExactSizeAndThisResultTypes(project);
  AstTests::runAllTests(project);
  return result;
}
