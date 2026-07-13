#include <rose.h>

#include <flang/Parser/parse-tree-visitor.h>
#include <flang/Parser/parsing.h>
#include <flang/Parser/provenance.h>
#include <flang/Semantics/semantics.h>
#include <flang/Semantics/type.h>
#include <flang/Support/Fortran-features.h>
#include <flang/Support/LangOptions.h>
#include <flang/Support/default-kinds.h>

#include "BuildVisitor.h"
#include "sage-build.h"

#include <iostream>
#include <string>

namespace {

constexpr const char *kFixtureFilename =
    "rex_flang_function_result_contract.f90";

void setExactFixtureSource(SgLocatedNode *node, int line) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *fileInfo = new Sg_File_Info(kFixtureFilename, line, 1);
  Sg_File_Info *start = new Sg_File_Info(kFixtureFilename, line, 1);
  Sg_File_Info *end = new Sg_File_Info(kFixtureFilename, line, 2);
  for (Sg_File_Info *position : {fileInfo, start, end}) {
    position->setOutputInCodeGeneration();
    position->set_parent(node);
  }
  node->set_file_info(fileInfo);
  node->set_startOfConstruct(start);
  node->set_endOfConstruct(end);
}

void setExactFixtureVariableSource(SgVariableDeclaration *declaration,
                                   int line) {
  ROSE_ASSERT(declaration != nullptr);
  setExactFixtureSource(declaration, line);
  for (SgInitializedName *name : declaration->get_variables()) {
    ROSE_ASSERT(name != nullptr);
    ROSE_ASSERT(name->get_parent() == declaration);
    setExactFixtureSource(name, line);
    SgVariableDefinition *definition = name->get_definition();
    if (isSgFunctionType(name->get_type()) != nullptr) {
      ROSE_ASSERT(definition == nullptr);
    } else {
      ROSE_ASSERT(definition != nullptr);
      ROSE_ASSERT(definition->get_parent() == name);
      setExactFixtureSource(definition, line);
    }
  }
}

class FunctionResultFixture {
public:
  FunctionResultFixture(bool objectResult = true, bool typedResult = true)
      : cookedSources_{allSources_}, context_{defaultKinds_, languageFeatures_,
                                              languageOptions_, cookedSources_},
        procedureSpelling_{"semantic_function"},
        resultSpelling_{"semantic_result"},
        result_{
            MakeResult(context_.globalScope(), resultSpelling_, objectResult)},
        procedure_{context_.globalScope().MakeSymbol(
            Fortran::parser::CharBlock(procedureSpelling_),
            Fortran::semantics::Attrs{},
            Fortran::semantics::SubprogramDetails{})},
        procedureName_{Fortran::parser::CharBlock(procedureSpelling_),
                       &procedure_},
        visitor_{cookedSources_, defaultKinds_.doublePrecisionKind(), context_},
        global_{new SgGlobal()}, function_{nullptr}, scope_{nullptr} {
    ROSE_ASSERT(global_ != nullptr);
    sourceFile_.set_file_info(new Sg_File_Info(kFixtureFilename, 1, 1));
    sourceFile_.get_file_info()->set_parent(&sourceFile_);
    sourceFile_.set_sourceFileNameWithPath(kFixtureFilename);
    sourceFile_.set_sourceFileNameWithoutPath(kFixtureFilename);
    sourceFile_.set_Fortran_only(true);
    project_.set_file(sourceFile_);
    setExactFixtureSource(global_, 1);
    global_->set_parent(&sourceFile_);
    sourceFile_.set_globalScope(global_);
    SageInterface::ensureCaseInsensitiveSymbolTable(global_, true);

    function_ = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        SgName("function_result_fixture_owner"), SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList(), global_);
    ROSE_ASSERT(function_ != nullptr);
    ROSE_ASSERT(function_->get_definition() != nullptr);
    scope_ = function_->get_definition()->get_body();
    ROSE_ASSERT(scope_ != nullptr);
    setExactFixtureSource(function_, 2);
    setExactFixtureSource(function_->get_definition(), 2);
    setExactFixtureSource(scope_, 3);
    if (objectResult && typedResult) {
      result_.SetType(context_.globalScope().MakeNumericType(
          Fortran::common::TypeCategory::Integer,
          Fortran::semantics::KindExpr{4}));
    }
    procedure_.get<Fortran::semantics::SubprogramDetails>().set_result(result_);
    SageInterface::ensureCaseInsensitiveSymbolTable(scope_, true);
    SageBuilder::pushScopeStack(scope_);
  }

  ~FunctionResultFixture() { SageBuilder::popScopeStack(); }

  SgVariableSymbol *publishResult(SgType *type,
                                  SgScopeStatement *scope = nullptr) {
    scope = scope != nullptr ? scope : scope_;
    SgVariableDeclaration *declaration =
        SageBuilder::buildVariableDeclaration_nfi(resultSpelling_, type,
                                                  nullptr, scope);
    ROSE_ASSERT(declaration != nullptr);
    setExactFixtureVariableSource(declaration, 4);
    SageInterface::appendStatement(declaration, scope);
    SgVariableSymbol *symbol = scope->lookup_variable_symbol(resultSpelling_);
    ROSE_ASSERT(symbol != nullptr);
    visitor_.RegisterSemanticSymbol(&result_, symbol);
    return symbol;
  }

  void publishWrongKind() {
    SgClassDeclaration *declaration = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        "wrong_result_kind", scope_);
    ROSE_ASSERT(declaration != nullptr);
    SgClassSymbol *symbol = scope_->lookup_class_symbol("wrong_result_kind");
    ROSE_ASSERT(symbol != nullptr);
    visitor_.RegisterSemanticSymbol(&result_, symbol);
  }

  SgVariableSymbol *publishResultInDifferentScope() {
    differentScope_ = SageBuilder::buildBasicBlock_nfi();
    ROSE_ASSERT(differentScope_ != nullptr);
    setExactFixtureSource(differentScope_, 5);
    differentScope_->set_parent(function_->get_definition());
    SageInterface::ensureCaseInsensitiveSymbolTable(differentScope_, true);
    return publishResult(SageBuilder::buildIntType(), differentScope_);
  }

  SgVariableSymbol *publishExactResult(const std::string &parsedName) {
    return Rose::builder::detail::PublishFlangSemanticFunctionResult(
        procedureName_, parsedName, visitor_, scope_);
  }

  SgType *buildSemanticResultType() {
    return Rose::builder::BuildFortranSemanticObjectType(result_, visitor_);
  }

  const std::string &resultSpelling() const { return resultSpelling_; }
  SgScopeStatement *scope() const { return scope_; }

private:
  static Fortran::semantics::Symbol &
  MakeResult(Fortran::semantics::Scope &scope, const std::string &spelling,
             bool objectResult) {
    const Fortran::parser::CharBlock name(spelling);
    if (objectResult) {
      return scope.MakeSymbol(name, Fortran::semantics::Attrs{},
                              Fortran::semantics::ObjectEntityDetails{});
    }
    return scope.MakeSymbol(name, Fortran::semantics::Attrs{},
                            Fortran::semantics::ProcEntityDetails{});
  }

  Fortran::parser::AllSources allSources_;
  Fortran::parser::AllCookedSources cookedSources_;
  Fortran::common::IntrinsicTypeDefaultKinds defaultKinds_;
  Fortran::common::LanguageFeatureControl languageFeatures_;
  Fortran::common::LangOptions languageOptions_;
  Fortran::semantics::SemanticsContext context_;
  std::string procedureSpelling_;
  std::string resultSpelling_;
  Fortran::semantics::Symbol &result_;
  Fortran::semantics::Symbol &procedure_;
  Fortran::parser::Name procedureName_;
  Rose::builder::BuildVisitor visitor_;
  SgProject project_;
  SgSourceFile sourceFile_;
  SgGlobal *global_;
  SgFunctionDeclaration *function_;
  SgBasicBlock *scope_;
  SgBasicBlock *differentScope_{nullptr};
};

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }

  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionFrontendConstruction);
  const std::string mode = argv[1];

  if (mode == "wrong-result-kind") {
    FunctionResultFixture fixture{/*objectResult=*/false};
    fixture.publishExactResult(fixture.resultSpelling());
  }
  if (mode == "missing-result-type") {
    FunctionResultFixture fixture{/*objectResult=*/true,
                                  /*typedResult=*/false};
    fixture.publishExactResult(fixture.resultSpelling());
  }

  FunctionResultFixture fixture;
  if (mode == "valid-created") {
    SgVariableSymbol *result =
        fixture.publishExactResult(fixture.resultSpelling());
    return result != nullptr && result->get_declaration() != nullptr &&
                   result->get_declaration()->get_scope() == fixture.scope() &&
                   isSgTypeInt(result->get_declaration()->get_type()) != nullptr
               ? 0
               : 1;
  }
  if (mode == "valid-existing") {
    SgVariableSymbol *expected =
        fixture.publishResult(fixture.buildSemanticResultType());
    return fixture.publishExactResult(fixture.resultSpelling()) == expected ? 0
                                                                            : 1;
  }
  if (mode == "wrong-result-name") {
    fixture.publishExactResult("spelling_decoy");
  } else if (mode == "wrong-publication-kind") {
    fixture.publishWrongKind();
    fixture.publishExactResult(fixture.resultSpelling());
  } else if (mode == "wrong-result-type") {
    fixture.publishResult(SageBuilder::buildFloatType());
    fixture.publishExactResult(fixture.resultSpelling());
  } else if (mode == "wrong-result-scope") {
    fixture.publishResultInDifferentScope();
    fixture.publishExactResult(fixture.resultSpelling());
  } else {
    std::cerr << "unknown mode: " << mode << '\n';
    return 2;
  }

  std::cerr << "function-result contract unexpectedly returned\n";
  return 1;
}
