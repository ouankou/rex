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

constexpr const char *kFixtureFilename = "rex_flang_semantic_name_contract.f90";

void setExactFixtureSource(SgLocatedNode *node, int line) {
  ROSE_ASSERT(node != nullptr);
  Sg_File_Info *fileInfo = new Sg_File_Info(kFixtureFilename, line, 1);
  Sg_File_Info *start = new Sg_File_Info(kFixtureFilename, line, 1);
  Sg_File_Info *end = new Sg_File_Info(kFixtureFilename, line, 1);
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

class SemanticFixture {
public:
  SemanticFixture()
      : cookedSources_{allSources_}, context_{defaultKinds_, languageFeatures_,
                                              languageOptions_, cookedSources_},
        spelling_{"semantic_object"},
        semantic_{context_.globalScope().MakeSymbol(
            Fortran::parser::CharBlock(spelling_), Fortran::semantics::Attrs{},
            Fortran::semantics::ObjectEntityDetails{})},
        name_{Fortran::parser::CharBlock(spelling_), &semantic_},
        visitor_{cookedSources_, defaultKinds_.doublePrecisionKind(), context_},
        global_{new SgGlobal()}, function_{nullptr}, scope_{nullptr},
        lengthSymbol_{nullptr} {
    ROSE_ASSERT(global_ != nullptr);
    sourceFile_.set_file_info(new Sg_File_Info(kFixtureFilename, 1, 1));
    sourceFile_.get_file_info()->set_parent(&sourceFile_);
    sourceFile_.set_sourceFileNameWithPath(kFixtureFilename);
    sourceFile_.set_sourceFileNameWithoutPath(kFixtureFilename);
    sourceFile_.set_Fortran_only(true);
    project_.set_file(sourceFile_);
    ROSE_ASSERT(sourceFile_.get_parent() == project_.get_fileList_ptr());
    ROSE_ASSERT(project_.get_fileList().size() == 1);
    ROSE_ASSERT(project_.get_fileList().front() == &sourceFile_);
    setExactFixtureSource(global_, 1);
    global_->set_parent(&sourceFile_);
    sourceFile_.set_globalScope(global_);
    SageInterface::ensureCaseInsensitiveSymbolTable(global_, true);

    function_ = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        SgName("semantic_fixture_owner"), SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList(), global_);
    ROSE_ASSERT(function_ != nullptr);
    ROSE_ASSERT(function_->get_parent() == global_);
    ROSE_ASSERT(function_->get_scope() == global_);
    ROSE_ASSERT(function_->get_definition() != nullptr);
    scope_ = function_->get_definition()->get_body();
    ROSE_ASSERT(scope_ != nullptr);
    ROSE_ASSERT(scope_->get_parent() == function_->get_definition());
    setExactFixtureSource(function_, 2);
    setExactFixtureSource(function_->get_definition(), 2);
    setExactFixtureSource(scope_, 3);
    semantic_.SetType(context_.globalScope().MakeNumericType(
        Fortran::common::TypeCategory::Integer,
        Fortran::semantics::KindExpr{4}));
    SageInterface::ensureCaseInsensitiveSymbolTable(scope_, true);
    SgVariableDeclaration *lengthDeclaration =
        SageBuilder::buildVariableDeclaration_nfi(
            "dynamic_length", SageBuilder::buildIntType(), nullptr, scope_);
    ROSE_ASSERT(lengthDeclaration != nullptr);
    setExactFixtureVariableSource(lengthDeclaration, 4);
    SageInterface::appendStatement(lengthDeclaration, scope_);
    lengthSymbol_ = scope_->lookup_variable_symbol("dynamic_length");
    ROSE_ASSERT(lengthSymbol_ != nullptr);
    SageBuilder::pushScopeStack(scope_);
  }

  ~SemanticFixture() { SageBuilder::popScopeStack(); }

  SgVariableSymbol *publishVariable() {
    return publishVariableWithType(
        Rose::builder::BuildFortranSemanticObjectType(semantic_, visitor_));
  }

  SgVariableSymbol *publishVariableWithType(SgType *type) {
    ROSE_ASSERT(type != nullptr);
    SgVariableDeclaration *declaration =
        SageBuilder::buildVariableDeclaration_nfi(spelling_, type, nullptr,
                                                  scope_);
    ROSE_ASSERT(declaration != nullptr);
    setExactFixtureVariableSource(declaration, 5);
    SageInterface::appendStatement(declaration, scope_);
    SgVariableSymbol *symbol = scope_->lookup_variable_symbol(spelling_);
    ROSE_ASSERT(symbol != nullptr);
    visitor_.RegisterSemanticSymbol(&semantic_, symbol);
    return symbol;
  }

  SgVariableSymbol *publishVariableBehindSameSpellingDecoy() {
    SgVariableDeclaration *decoy = SageBuilder::buildVariableDeclaration_nfi(
        spelling_, SageBuilder::buildFloatType(), nullptr, scope_);
    ROSE_ASSERT(decoy != nullptr);
    setExactFixtureVariableSource(decoy, 6);
    SageInterface::appendStatement(decoy, scope_);

    SgVariableDeclaration *declaration =
        SageBuilder::buildVariableDeclaration_nfi(
            spelling_, SageBuilder::buildIntType(), nullptr, global_);
    ROSE_ASSERT(declaration != nullptr);
    setExactFixtureVariableSource(declaration, 7);
    SageInterface::appendStatement(declaration, global_);
    SgVariableSymbol *symbol = global_->lookup_variable_symbol(spelling_);
    ROSE_ASSERT(symbol != nullptr);
    visitor_.RegisterSemanticSymbol(&semantic_, symbol);
    return symbol;
  }

  SgClassSymbol *publishClass() {
    SgClassDeclaration *declaration = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        "wrong_semantic_kind", scope_);
    ROSE_ASSERT(declaration != nullptr);
    SgClassSymbol *symbol = scope_->lookup_class_symbol("wrong_semantic_kind");
    ROSE_ASSERT(symbol != nullptr);
    visitor_.RegisterSemanticSymbol(&semantic_, symbol);
    return symbol;
  }

  SgVariableSymbol *publishClassMember() {
    SgClassDeclaration *owner = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(), "semantic_owner",
        scope_);
    ROSE_ASSERT(owner != nullptr);
    SgClassDefinition *definition = owner->get_definition();
    ROSE_ASSERT(definition != nullptr);
    SgVariableDeclaration *declaration =
        SageBuilder::buildVariableDeclaration_nfi(
            spelling_, SageBuilder::buildIntType(), nullptr, definition);
    ROSE_ASSERT(declaration != nullptr);
    setExactFixtureVariableSource(declaration, 8);
    SageInterface::appendStatement(declaration, definition);
    SgVariableSymbol *symbol = definition->lookup_variable_symbol(spelling_);
    ROSE_ASSERT(symbol != nullptr);
    visitor_.RegisterSemanticSymbol(&semantic_, symbol);
    return symbol;
  }

  Fortran::parser::Name &name() { return name_; }
  Rose::builder::BuildVisitor &visitor() { return visitor_; }
  SgScopeStatement *scope() { return scope_; }

  Fortran::semantics::Symbol &semantic() { return semantic_; }

  SgTypeString *buildDynamicCharacter(bool sourceSyntax) {
    ROSE_ASSERT(lengthSymbol_ != nullptr);
    SgVarRefExp *length = SageBuilder::buildVarRefExp_nfi(lengthSymbol_);
    ROSE_ASSERT(length != nullptr);
    setExactFixtureSource(length, 8);
    SgTypeString *type = new SgTypeString(length);
    ROSE_ASSERT(type != nullptr);
    length->set_parent(type);
    if (!sourceSyntax) {
      SgIntVal *kind = SageBuilder::buildIntVal_nfi("1");
      ROSE_ASSERT(kind != nullptr);
      setExactFixtureSource(kind, 8);
      kind->set_parent(type);
      type->set_type_kind(kind);
    }
    type->set_fortran_source_syntax(sourceSyntax);
    return type;
  }

  SgTypeString *buildPendingDynamicCharacter() {
    SgTypeString *type = new SgTypeString(nullptr);
    ROSE_ASSERT(type != nullptr);
    type->set_fortran_dynamic_length_pending(true);
    SgIntVal *kind = SageBuilder::buildIntVal_nfi("1");
    ROSE_ASSERT(kind != nullptr);
    setExactFixtureSource(kind, 8);
    kind->set_parent(type);
    type->set_type_kind(kind);
    return type;
  }

  SgTypeString *buildStaticSourceCharacter() {
    SgIntVal *length = SageBuilder::buildIntVal_nfi("1");
    ROSE_ASSERT(length != nullptr);
    setExactFixtureSource(length, 9);
    length->set_fortran_integer_constant_value(1);
    length->set_fortran_integer_constant_value_is_available(true);
    SgTypeString *type = new SgTypeString(length);
    ROSE_ASSERT(type != nullptr);
    length->set_parent(type);
    type->set_fortran_source_syntax(true);
    return type;
  }

private:
  Fortran::parser::AllSources allSources_;
  Fortran::parser::AllCookedSources cookedSources_;
  Fortran::common::IntrinsicTypeDefaultKinds defaultKinds_;
  Fortran::common::LanguageFeatureControl languageFeatures_;
  Fortran::common::LangOptions languageOptions_;
  Fortran::semantics::SemanticsContext context_;
  std::string spelling_;
  Fortran::semantics::Symbol &semantic_;
  Fortran::parser::Name name_;
  Rose::builder::BuildVisitor visitor_;
  SgProject project_;
  SgSourceFile sourceFile_;
  SgGlobal *global_;
  SgFunctionDeclaration *function_;
  SgBasicBlock *scope_;
  SgVariableSymbol *lengthSymbol_;
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
  SemanticFixture fixture;

  if (mode == "valid-object") {
    SgVariableSymbol *expected = fixture.publishVariable();
    SgVarRefExp *reference =
        isSgVarRefExp(Rose::builder::detail::BuildFlangSemanticNameReference(
            fixture.name(), fixture.visitor(), fixture.scope()));
    return reference != nullptr && reference->get_symbol() == expected ? 0 : 1;
  }
  if (mode == "valid-component") {
    SgVariableSymbol *expected = fixture.publishClassMember();
    return Rose::builder::detail::RequireFlangSemanticComponentSymbol(
               fixture.name(), fixture.visitor()) == expected
               ? 0
               : 1;
  }
  if (mode == "valid-shadowed-object") {
    SgVariableSymbol *expected =
        fixture.publishVariableBehindSameSpellingDecoy();
    SgVarRefExp *reference =
        isSgVarRefExp(Rose::builder::detail::BuildFlangSemanticNameReference(
            fixture.name(), fixture.visitor(), fixture.scope()));
    return reference != nullptr && reference->get_symbol() == expected ? 0 : 1;
  }
  if (mode == "valid-semantic-type") {
    SgVariableSymbol *expected = fixture.publishVariable();
    fixture.visitor().PublishSemanticNameBeforeConsumption(fixture.name());
    return fixture.visitor().LookupSemanticSymbol(fixture.name().symbol) ==
                   expected
               ? 0
               : 1;
  }
  if (mode == "valid-dynamic-character-transaction") {
    SgTypeString *semantic = fixture.buildDynamicCharacter(false);
    SgTypeString *source = fixture.buildDynamicCharacter(true);
    fixture.visitor().RegisterSemanticObjectType(&fixture.semantic(), semantic);
    fixture.visitor().RegisterProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic);
    fixture.visitor().FinalizeProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic, source, "semantic_object");
    SgExpression *finalLength = semantic->get_lengthExpression();
    return finalLength != nullptr &&
                   finalLength != source->get_lengthExpression() &&
                   finalLength->get_parent() == semantic &&
                   SageInterface::fortranSourceTypeMatchesSemanticType(source,
                                                                       semantic)
               ? 0
               : 1;
  }
  if (mode == "valid-dynamic-character-first-producer") {
    SgTypeString *semantic = fixture.buildPendingDynamicCharacter();
    SgTypeString *source = fixture.buildDynamicCharacter(true);
    fixture.visitor().RegisterSemanticObjectType(&fixture.semantic(), semantic);
    fixture.visitor().FinalizeProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic, source, "semantic_object");
    SgExpression *finalLength = semantic->get_lengthExpression();
    return finalLength != nullptr &&
                   finalLength != source->get_lengthExpression() &&
                   finalLength->get_parent() == semantic &&
                   !semantic->get_fortran_dynamic_length_pending() &&
                   SageInterface::fortranSourceTypeMatchesSemanticType(source,
                                                                       semantic)
               ? 0
               : 1;
  }
  if (mode == "missing-name-symbol") {
    fixture.name().symbol = nullptr;
    Rose::builder::detail::BuildFlangSemanticNameReference(
        fixture.name(), fixture.visitor(), fixture.scope());
  } else if (mode == "missing-object-publication") {
    Rose::builder::detail::BuildFlangSemanticNameReference(
        fixture.name(), fixture.visitor(), fixture.scope());
  } else if (mode == "wrong-object-kind") {
    fixture.publishClass();
    Rose::builder::detail::BuildFlangSemanticNameReference(
        fixture.name(), fixture.visitor(), fixture.scope());
  } else if (mode == "missing-component-publication") {
    Rose::builder::detail::RequireFlangSemanticComponentSymbol(
        fixture.name(), fixture.visitor());
  } else if (mode == "wrong-component-owner") {
    fixture.publishVariable();
    Rose::builder::detail::RequireFlangSemanticComponentSymbol(
        fixture.name(), fixture.visitor());
  } else if (mode == "wrong-semantic-object-type") {
    fixture.publishVariableWithType(SageBuilder::buildFloatType());
    fixture.visitor().PublishSemanticNameBeforeConsumption(fixture.name());
  } else if (mode == "duplicate-dynamic-character-transaction") {
    SgTypeString *semantic = fixture.buildDynamicCharacter(false);
    fixture.visitor().RegisterProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic);
    fixture.visitor().RegisterProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic);
  } else if (mode == "contradictory-dynamic-character-first-producer") {
    SgTypeString *semantic = fixture.buildDynamicCharacter(false);
    SgTypeString *source = fixture.buildDynamicCharacter(true);
    fixture.visitor().RegisterSemanticObjectType(&fixture.semantic(), semantic);
    fixture.visitor().FinalizeProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic, source, "semantic_object");
  } else if (mode == "stale-dynamic-character-transaction") {
    SgTypeString *semantic = fixture.buildDynamicCharacter(false);
    fixture.visitor().RegisterSemanticObjectType(&fixture.semantic(), semantic);
    fixture.visitor().RegisterProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic);
    fixture.visitor().FinalizeProvisionalDynamicCharacterLength(
        &fixture.semantic(), semantic, fixture.buildStaticSourceCharacter(),
        "semantic_object");
  } else {
    std::cerr << "unknown mode: " << mode << '\n';
    return 2;
  }

  std::cerr << "semantic-name contract unexpectedly returned\n";
  return 1;
}
