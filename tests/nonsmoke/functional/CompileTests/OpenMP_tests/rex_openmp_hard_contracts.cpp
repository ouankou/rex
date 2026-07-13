#include "ompAstConstruction.h"
#include "rose.h"

#include <type_traits>

extern void omp_exprparser_begin_fortran_exact_semantic_bindings(
    const std::vector<OmpFortranExactSemanticBindings::Binding> *bindings,
    const std::vector<OmpExactSubexpressionType> *subexpressions,
    SgType *default_integer_type);
extern void omp_exprparser_end_fortran_exact_semantic_bindings();

int main(int argc, char **argv) {
  static_assert(!std::is_default_constructible_v<OmpExactSubexpressionType>);
  static_assert(!std::is_default_constructible_v<
                OmpFortranExactSemanticBindings::Binding>);
  static_assert(!std::is_default_constructible_v<
                OmpFortranExactSemanticBindings::ExpressionTypes>);
  static_assert(
      !std::is_default_constructible_v<OmpFortranExactSemanticBindings>);

  constexpr const char *fixtureFilename = "rex_openmp_hard_contracts.cpp";
  SgSourceFile sourceFile;
  sourceFile.set_file_info(new Sg_File_Info(fixtureFilename, 1, 1));
  sourceFile.get_file_info()->set_parent(&sourceFile);
  sourceFile.set_sourceFileNameWithPath(fixtureFilename);
  sourceFile.set_sourceFileNameWithoutPath(fixtureFilename);
  SgGlobal *scope = new SgGlobal();
  scope->set_file_info(new Sg_File_Info(fixtureFilename, 1, 1));
  scope->get_file_info()->set_parent(scope);
  scope->set_parent(&sourceFile);
  sourceFile.set_globalScope(scope);
  OmpSupport::OpenMPConversionSession conversionSession(&sourceFile);

  const std::string mode = argc == 2 ? argv[1] : std::string();
  if (mode == "declare-simd-missing-target") {
    static_cast<void>(new SgOmpDeclareSimdStatement(
        new SgFunctionRefExp(static_cast<SgFunctionSymbol *>(nullptr),
                             static_cast<SgFunctionType *>(nullptr)),
        false, 0));
    return 0;
  }
  if (mode == "declare-variant-missing-target") {
    static_cast<void>(new SgOmpDeclareVariantStatement(
        new SgFunctionRefExp(static_cast<SgFunctionSymbol *>(nullptr),
                             static_cast<SgFunctionType *>(nullptr)),
        new SgFunctionRefExp(static_cast<SgFunctionSymbol *>(nullptr),
                             static_cast<SgFunctionType *>(nullptr)),
        false, 0));
    return 0;
  }
  if (mode == "openmp-nonentity-identity") {
    SgOmpRequiresStatement *statement = new SgOmpRequiresStatement();
    SageInterface::setOneSourcePositionForTransformation(statement);
    SageInterface::appendStatement(statement, scope);
    static_cast<void>(statement->get_mangled_name());
    return 0;
  }
  if (mode == "malformed-subexpression-type") {
    static_cast<void>(
        OmpExactSubexpressionType(OmpExactSubexpressionKind::add, nullptr));
    return 0;
  }
  if (mode == "combined-order-republication") {
    SgOmpNowaitClause *clause =
        new SgOmpNowaitClause(static_cast<SgExpression *>(nullptr));
    clause->initialize_combined_source_order(0);
    clause->initialize_combined_source_order(1);
    return 0;
  }
  if (mode == "fortran-range") {
    std::vector<OmpFortranExactSemanticBindings::Binding> bindings;
    bindings.emplace_back(
        4, 2, "ab", "ab",
        OmpFortranExactSemanticBindings::BindingKind::directive_local, nullptr,
        nullptr, SageBuilder::buildIntType());
    std::vector<OmpFortranExactSemanticBindings::ExpressionTypes> expressions;
    expressions.emplace_back(0, 3, "abc",
                             std::vector<OmpExactSubexpressionType>{});
    static_cast<void>(OmpFortranExactSemanticBindings(
        OmpFortranExactSemanticBindings::Producer::flang_parse_tree, "abc",
        SageBuilder::buildIntType(), std::move(bindings),
        std::move(expressions)));
    return 0;
  }
  if (mode == "fortran-overlap" || mode == "fortran-duplicate-span") {
    std::vector<OmpFortranExactSemanticBindings::Binding> bindings;
    bindings.emplace_back(
        0, mode == "fortran-overlap" ? 2 : 1,
        mode == "fortran-overlap" ? "ab" : "a",
        mode == "fortran-overlap" ? "ab" : "a",
        OmpFortranExactSemanticBindings::BindingKind::directive_local, nullptr,
        nullptr, SageBuilder::buildIntType());
    bindings.emplace_back(
        mode == "fortran-overlap" ? 1 : 0, mode == "fortran-overlap" ? 2 : 1,
        mode == "fortran-overlap" ? "bc" : "a",
        mode == "fortran-overlap" ? "bc" : "a",
        OmpFortranExactSemanticBindings::BindingKind::directive_local, nullptr,
        nullptr, SageBuilder::buildIntType());
    std::vector<OmpFortranExactSemanticBindings::ExpressionTypes> expressions;
    expressions.emplace_back(0, 3, "abc",
                             std::vector<OmpExactSubexpressionType>{});
    static_cast<void>(OmpFortranExactSemanticBindings(
        OmpFortranExactSemanticBindings::Producer::flang_parse_tree, "abc",
        SageBuilder::buildIntType(), std::move(bindings),
        std::move(expressions)));
    return 0;
  }
  if (mode.rfind("fortran-", 0) == 0) {
    Rose::is_Fortran_language = true;
    std::vector<OmpFortranExactSemanticBindings::Binding> fortranBindings;
    std::vector<OmpExactSubexpressionType> fortranSubexpressions;
    if (mode == "fortran-mismatch") {
      fortranBindings.emplace_back(
          0, std::string("producer_identity").size(), "producer_identity",
          "producer_identity",
          OmpFortranExactSemanticBindings::BindingKind::directive_local,
          nullptr, nullptr, SageBuilder::buildIntType());
    } else if (mode == "fortran-wrong-kind") {
      fortranBindings.emplace_back(
          0, std::string("consumer_identity").size(), "consumer_identity",
          "consumer_identity",
          OmpFortranExactSemanticBindings::BindingKind::common_block, nullptr,
          nullptr, nullptr);
    } else if (mode == "fortran-missing-record") {
      // Keep the exact producer sequence deliberately empty.
    } else if (mode == "fortran-stale") {
      fortranBindings.emplace_back(
          0, std::string("producer_identity").size(), "producer_identity",
          "producer_identity",
          OmpFortranExactSemanticBindings::BindingKind::directive_local,
          nullptr, nullptr, SageBuilder::buildIntType());
      omp_exprparser_begin_fortran_exact_semantic_bindings(
          &fortranBindings, &fortranSubexpressions,
          SageBuilder::buildIntType());
      omp_exprparser_end_fortran_exact_semantic_bindings();
      return 0;
    } else {
      return 2;
    }
    omp_exprparser_begin_fortran_exact_semantic_bindings(
        &fortranBindings, &fortranSubexpressions, SageBuilder::buildIntType());
    parseExpression(scope, "varlist consumer_identity\n");
    return 0;
  }
  return 2;
}
