// Insert callbacks at direct variable references. Write callbacks precede
// assignments and increments; read callbacks precede other references and the
// read part of a compound assignment or increment. This illustrates syntactic
// instrumentation, not alias analysis of indirect memory accesses.
#include "rose.h"

#include <memory>

namespace {
SgFunctionSymbol *declareCallback(const char *name, SgGlobal *scope, bool cxx) {
  auto *declaration = SageBuilder::buildNondefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexicalAtTop(scope),
      name, SageBuilder::buildVoidType(),
      SageBuilder::buildFunctionParameterList(), scope);
  declaration->get_declarationModifier().get_storageModifier().setExtern();
  if (cxx)
    declaration->set_linkage("C");
  auto *symbol =
      isSgFunctionSymbol(declaration->get_symbol_from_symbol_table());
  ROSE_ASSERT(symbol != nullptr);
  return symbol;
}

void instrument(SgVarRefExp *reference, SgFunctionSymbol *beforeRead,
                SgFunctionSymbol *beforeWrite) {
  auto *symbol = reference->get_symbol();
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
  if (symbol->get_declaration()->get_is_predefined_identifier())
    return;
  SgNode *parent = reference->get_parent();
  // A member name in a.b or a->b is not an independent variable expression.
  if (auto *binary = isSgBinaryOp(parent)) {
    if ((isSgDotExp(binary) || isSgArrowExp(binary)) &&
        binary->get_rhs_operand() == reference)
      return;
  }
  if (isSgAddressOfOp(parent))
    return;

  bool reads = true;
  bool writes = isSgPlusPlusOp(parent) || isSgMinusMinusOp(parent);
  if (auto *assignment = isSgAssignOp(parent)) {
    writes = assignment->get_lhs_operand() == reference;
    reads = !writes;
  } else if (auto *assignment = isSgCompoundAssignOp(parent)) {
    writes = assignment->get_lhs_operand() == reference;
  }

  // Copy before replacing so every published expression has one owner. The
  // pointer form preserves the original lvalue in both C and C++.
  auto *copy = SageInterface::copyExpression(reference);
  auto *pointerType = SageBuilder::buildPointerType(reference->get_type());
  SgExpression *address = SageBuilder::buildAddressOfOp(
      copy, pointerType, SgAddressOfOp::e_address_operand_object);
  if (writes)
    address = SageBuilder::buildCommaOpExp(
        SageBuilder::buildFunctionCallExp(beforeWrite), address, pointerType);
  if (reads)
    address = SageBuilder::buildCommaOpExp(
        SageBuilder::buildFunctionCallExp(beforeRead), address, pointerType);
  auto *replacement =
      SageBuilder::buildPointerDerefExp(address, reference->get_type());
  SageInterface::replaceExpression(reference, replacement);
}

class InstrumentVariables : public AstSimpleProcessing {
public:
  InstrumentVariables(SgFunctionSymbol *read, SgFunctionSymbol *write)
      : beforeRead(read), beforeWrite(write) {}
  void visit(SgNode *node) override {
    if (auto *reference = isSgVarRefExp(node))
      instrument(reference, beforeRead, beforeWrite);
  }

private:
  SgFunctionSymbol *beforeRead;
  SgFunctionSymbol *beforeWrite;
};
} // namespace

int main(int argc, char **argv) {
  std::unique_ptr<SgProject> project(frontend(argc, argv));
  ROSE_ASSERT(project != nullptr);
  AstTests::runAllTests(project.get());
  for (SgFile *file : project->get_fileList()) {
    auto *source = isSgSourceFile(file);
    ROSE_ASSERT(source != nullptr);
    auto *scope = source->get_globalScope();
    const bool cxx = source->get_inputLanguage() == SgFile::e_Cxx_language;
    ROSE_ASSERT(cxx || source->get_inputLanguage() == SgFile::e_C_language);
    auto *read = declareCallback("beforeRead", scope, cxx);
    auto *write = declareCallback("beforeWrite", scope, cxx);
    InstrumentVariables traversal(read, write);
    traversal.traverseWithinFile(source, postorder);
  }
  AstTests::runAllTests(project.get());
  return backend(project.get());
}
