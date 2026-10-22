#include "astJson/sageAstJson.h"
#include "rose.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

std::string takeOption(int &argc, char **argv, const char *prefix) {
  const std::size_t prefixLength = std::strlen(prefix);
  std::string result;
  std::vector<char *> filtered{argv[0]};
  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, prefixLength) == 0) {
      ROSE_ASSERT(result.empty());
      result = argv[index] + prefixLength;
    } else {
      filtered.push_back(argv[index]);
    }
  }
  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

std::optional<std::int64_t> integerLiteral(const SgExpression *expression) {
  if (const SgIntVal *value = isSgIntVal(expression)) {
    return value->get_value();
  }
  if (const SgLongLongIntVal *value = isSgLongLongIntVal(expression)) {
    return value->get_value();
  }
  return std::nullopt;
}

void requireFoldedSelector(const SgExpression *expression,
                           std::int64_t expected) {
  ROSE_ASSERT(expression != nullptr);
  ROSE_ASSERT(expression->get_fortran_integer_constant_value_is_available());
  ROSE_ASSERT(expression->get_fortran_integer_constant_value() == expected);
  if (const std::optional<std::int64_t> literal = integerLiteral(expression)) {
    ROSE_ASSERT(*literal == expected);
  }
}

void requireNoFoldedSelector(const SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  ROSE_ASSERT(!expression->get_fortran_integer_constant_value_is_available());
  ROSE_ASSERT(expression->get_fortran_integer_constant_value() == 0);
}

struct TypeSurface {
  SgType *base = nullptr;
  std::size_t rank = 0;
  bool pointer = false;
};

TypeSurface typeSurface(SgType *type, bool requireSourceIdentity) {
  TypeSurface result;
  while (type != nullptr) {
    ROSE_ASSERT(type->get_fortran_source_syntax() == requireSourceIdentity);
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      result.pointer = true;
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        result.rank += static_cast<std::size_t>(array->get_rank());
      }
      type = array->get_base_type();
    } else {
      result.base = type;
      return result;
    }
  }
  ROSE_ABORT();
}

void requireSourceTypeNotInterned(SgType *type) {
  SgTypeTable *table = SgNode::get_globalTypeTable();
  ROSE_ASSERT(table != nullptr);
  SgSymbolTable *symbols = table->get_type_table();
  ROSE_ASSERT(symbols != nullptr && symbols->get_table() != nullptr);
  while (type != nullptr) {
    ROSE_ASSERT(type->get_fortran_source_syntax());
    for (const auto &entry : *symbols->get_table()) {
      SgFunctionTypeSymbol *symbol = isSgFunctionTypeSymbol(entry.second);
      ROSE_ASSERT(symbol != nullptr);
      ROSE_ASSERT(symbol->get_type() != type);
    }
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      type = array->get_base_type();
    } else if (SgTypeComplex *complex = isSgTypeComplex(type)) {
      type = complex->get_base_type();
    } else {
      return;
    }
  }
  ROSE_ABORT();
}

SgInitializedName *sourceName(SgSourceFile *file, const std::string &name) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate != nullptr && candidate->get_name() == name &&
        candidate->get_fortran_source_type() != nullptr) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_type() != nullptr);
  ROSE_ASSERT(result->get_fortran_source_type() != result->get_type());
  requireSourceTypeNotInterned(result->get_fortran_source_type());
  return result;
}

SgProcedureHeaderStatement *typedProcedure(SgSourceFile *file,
                                           const std::string &name) {
  SgProcedureHeaderStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    if (candidate != nullptr && candidate->get_name() == name &&
        candidate->get_fortran_procedure_source_form() !=
            SgProcedureHeaderStatement::
                e_fortran_procedure_source_form_header &&
        candidate->get_fortran_procedure_source_form() !=
            SgProcedureHeaderStatement::
                e_fortran_procedure_source_form_semantic_only) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgProcedureHeaderStatement *
headerProcedure(SgSourceFile *file, const std::string &name, bool defining) {
  SgProcedureHeaderStatement *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    if (candidate != nullptr && candidate->get_name() == name &&
        candidate->get_fortran_procedure_source_form() ==
            SgProcedureHeaderStatement::
                e_fortran_procedure_source_form_header &&
        (candidate->get_definition() != nullptr) == defining) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

std::int64_t semanticKind(SgType *type) {
  TypeSurface surface = typeSurface(type, false);
  SgType *base = surface.base;
  ROSE_ASSERT(base != nullptr);
  SgExpression *kind = base->get_type_kind();
  if (kind == nullptr) {
    if (SgTypeComplex *complex = isSgTypeComplex(base)) {
      kind = complex->get_base_type()->get_type_kind();
    }
  }
  const std::optional<std::int64_t> value = integerLiteral(kind);
  ROSE_ASSERT(value && *value > 0);
  requireNoFoldedSelector(kind);
  return *value;
}

void verifyProcedureSourceForms(SgSourceFile *file) {
  std::size_t semanticOnly = 0;
  std::size_t headers = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *procedure = isSgProcedureHeaderStatement(node);
    ROSE_ASSERT(procedure != nullptr);
    if (procedure->get_fortran_procedure_source_form() ==
        SgProcedureHeaderStatement::
            e_fortran_procedure_source_form_semantic_only) {
      ++semanticOnly;
      ROSE_ASSERT(isSgAuxiliaryDeclarationList(procedure->get_parent()) !=
                  nullptr);
      ROSE_ASSERT(procedure->get_file_info()->isCompilerGenerated());
    } else if (procedure->get_fortran_procedure_source_form() ==
               SgProcedureHeaderStatement::
                   e_fortran_procedure_source_form_header) {
      ++headers;
      ROSE_ASSERT(!procedure->get_file_info()->isCompilerGenerated());
    }
  }
  ROSE_ASSERT(semanticOnly >= 3);
  ROSE_ASSERT(headers >= 3);

  SgProcedureHeaderStatement *iTwice = typedProcedure(file, "i_twice");
  SgProcedureHeaderStatement *jTwice = typedProcedure(file, "j_twice");
  SgProcedureHeaderStatement *kTwice = typedProcedure(file, "k_twice");
  SgProcedureHeaderStatement *mTwice = typedProcedure(file, "m_twice");
  for (SgProcedureHeaderStatement *procedure :
       {iTwice, jTwice, kTwice, mTwice}) {
    SgFunctionType *semanticType = procedure->get_type();
    SgFunctionType *sourceType = procedure->get_type_syntax();
    ROSE_ASSERT(semanticType != nullptr && sourceType != nullptr);
    ROSE_ASSERT(sourceType->get_fortran_source_syntax());
    ROSE_ASSERT(!semanticType->get_fortran_source_syntax());
    ROSE_ASSERT(procedure->get_type_syntax_is_available());
    ROSE_ASSERT(procedure->get_orig_return_type() ==
                sourceType->get_return_type());
    ROSE_ASSERT(sourceType->get_return_type() !=
                semanticType->get_return_type());
    ROSE_ASSERT(sourceType->get_arguments().size() ==
                semanticType->get_arguments().size());
    ROSE_ASSERT(semanticKind(semanticType->get_return_type()) == 4);
    requireSourceTypeNotInterned(sourceType->get_return_type());
    ROSE_ASSERT(
        procedure->get_fortran_result_type_spec() ==
        SgProcedureHeaderStatement::e_fortran_result_type_spec_intrinsic);
  }
  ROSE_ASSERT(
      !iTwice->get_declarationModifier().get_storageModifier().isExtern());
  ROSE_ASSERT(
      jTwice->get_declarationModifier().get_storageModifier().isExtern());
  ROSE_ASSERT(
      kTwice->get_declarationModifier().get_storageModifier().isExtern());
  ROSE_ASSERT(
      mTwice->get_declarationModifier().get_storageModifier().isExtern());
  ROSE_ASSERT(iTwice->get_type_syntax()->get_return_type()->get_type_kind() ==
              nullptr);
  ROSE_ASSERT(jTwice->get_type_syntax()->get_return_type()->get_type_kind() ==
              nullptr);
  requireFoldedSelector(
      kTwice->get_type_syntax()->get_return_type()->get_type_kind(), 4);
  SgExpression *expressionKind =
      mTwice->get_type_syntax()->get_return_type()->get_type_kind();
  requireFoldedSelector(expressionKind, 4);
  ROSE_ASSERT(isSgValueExp(expressionKind) == nullptr);
  ROSE_ASSERT(kTwice->get_type_syntax()->get_return_type() !=
              mTwice->get_type_syntax()->get_return_type());

  SgProcedureHeaderStatement *iTwiceDefinition =
      headerProcedure(file, "i_twice", true);
  SgProcedureHeaderStatement *jTwiceDefinition =
      headerProcedure(file, "j_twice", true);
  SgProcedureHeaderStatement *kTwiceDefinition =
      headerProcedure(file, "k_twice", true);
  SgProcedureHeaderStatement *mTwiceDefinition =
      headerProcedure(file, "m_twice", true);
  for (SgProcedureHeaderStatement *procedure :
       {iTwiceDefinition, jTwiceDefinition, kTwiceDefinition,
        mTwiceDefinition}) {
    SgFunctionType *semanticType = procedure->get_type();
    SgFunctionType *sourceType = procedure->get_type_syntax();
    ROSE_ASSERT(semanticType != nullptr && sourceType != nullptr);
    ROSE_ASSERT(procedure->get_type_syntax_is_available());
    ROSE_ASSERT(sourceType->get_fortran_source_syntax());
    ROSE_ASSERT(!semanticType->get_fortran_source_syntax());
    ROSE_ASSERT(SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
        sourceType, semanticType));
    ROSE_ASSERT(procedure->get_orig_return_type() ==
                sourceType->get_return_type());
    ROSE_ASSERT(sourceType->get_return_type() !=
                semanticType->get_return_type());
    ROSE_ASSERT(
        procedure->get_fortran_result_type_spec() ==
        SgProcedureHeaderStatement::e_fortran_result_type_spec_intrinsic);
    requireSourceTypeNotInterned(sourceType->get_return_type());
  }
  ROSE_ASSERT(
      iTwiceDefinition->get_type_syntax()->get_return_type()->get_type_kind() ==
      nullptr);
  ROSE_ASSERT(
      jTwiceDefinition->get_type_syntax()->get_return_type()->get_type_kind() ==
      nullptr);
  requireFoldedSelector(
      kTwiceDefinition->get_type_syntax()->get_return_type()->get_type_kind(),
      4);
  SgExpression *definitionExpressionKind =
      mTwiceDefinition->get_type_syntax()->get_return_type()->get_type_kind();
  requireFoldedSelector(definitionExpressionKind, 4);
  ROSE_ASSERT(isSgValueExp(definitionExpressionKind) == nullptr);

  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(iTwice->get_firstNondefiningDeclaration());
  SgFunctionSymbol *symbol =
      canonical != nullptr
          ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
          : nullptr;
  ROSE_ASSERT(canonical == iTwice);
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == canonical);

  SgProcedureHeaderStatement *makeBox =
      typedProcedure(file, "make_selector_box");
  ROSE_ASSERT(makeBox->get_fortran_result_type_spec() ==
              SgProcedureHeaderStatement::e_fortran_result_type_spec_type);
  SgType *makeBoxSourceResult = makeBox->get_type_syntax()->get_return_type();
  ROSE_ASSERT(isSgClassType(makeBoxSourceResult) != nullptr);
  ROSE_ASSERT(!makeBoxSourceResult->get_fortran_source_syntax());
  ROSE_ASSERT(makeBoxSourceResult == makeBox->get_type()->get_return_type());

  SgProcedureHeaderStatement *unlimitedFactory = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    if (candidate != nullptr && candidate->get_name() == "unlimited_factory" &&
        candidate->get_fortran_procedure_source_form() ==
            SgProcedureHeaderStatement::
                e_fortran_procedure_source_form_header) {
      ROSE_ASSERT(unlimitedFactory == nullptr);
      unlimitedFactory = candidate;
    }
  }
  ROSE_ASSERT(unlimitedFactory != nullptr);
  ROSE_ASSERT(unlimitedFactory->get_fortran_result_type_spec() ==
              SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown);
  ROSE_ASSERT(unlimitedFactory->get_type_syntax() == nullptr);
  ROSE_ASSERT(!unlimitedFactory->get_type_syntax_is_available());

  SgProcedureHeaderStatement *copySelectorBox =
      headerProcedure(file, "copy_selector_box", true);
  SgProcedureHeaderStatement *allocateSelectorBox =
      headerProcedure(file, "allocate_selector_box", true);
  for (SgProcedureHeaderStatement *procedure :
       {copySelectorBox, allocateSelectorBox}) {
    SgFunctionType *sourceType = procedure->get_type_syntax();
    ROSE_ASSERT(sourceType != nullptr);
    ROSE_ASSERT(procedure->get_type_syntax_is_available());
    ROSE_ASSERT(sourceType->get_fortran_source_syntax());
    ROSE_ASSERT(SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
        sourceType, procedure->get_type()));
    ROSE_ASSERT(isSgClassType(sourceType->get_return_type()) != nullptr);
  }
  ROSE_ASSERT(copySelectorBox->get_fortran_result_type_spec() ==
              SgProcedureHeaderStatement::e_fortran_result_type_spec_type);
  ROSE_ASSERT(allocateSelectorBox->get_fortran_result_type_spec() ==
              SgProcedureHeaderStatement::e_fortran_result_type_spec_class);
  SgInitializedName *unlimitedResult = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(unlimitedFactory, V_SgInitializedName)) {
    SgInitializedName *candidate = isSgInitializedName(node);
    if (candidate != nullptr && candidate->get_name() == "value" &&
        candidate->get_fortran_source_type() != nullptr) {
      ROSE_ASSERT(unlimitedResult == nullptr);
      unlimitedResult = candidate;
    }
  }
  ROSE_ASSERT(unlimitedResult != nullptr);
  ROSE_ASSERT(unlimitedResult->get_fortran_type_spec() ==
              SgInitializedName::e_fortran_type_spec_class_star);
  ROSE_ASSERT(
      isSgTypeFortranUnlimitedPolymorphic(
          typeSurface(unlimitedResult->get_fortran_source_type(), true).base) !=
      nullptr);
  ROSE_ASSERT(isSgTypeFortranUnlimitedPolymorphic(
                  typeSurface(unlimitedResult->get_type(), false).base) !=
              nullptr);

  std::size_t namedArguments = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgActualArgumentExpression)) {
    auto *argument = isSgActualArgumentExpression(node);
    ROSE_ASSERT(argument->get_argument_name() == "value");
    ROSE_ASSERT(argument->get_expression() != nullptr);
    ROSE_ASSERT(argument->get_expression()->get_parent() == argument);
    ++namedArguments;
  }
  ROSE_ASSERT(namedArguments == 1);
}

void verifyFunctionSourceVisibleBindings(SgSourceFile *file) {
  const std::set<std::string> expectedExact{"i_twice", "j_twice", "k_twice",
                                            "m_twice"};
  const std::set<std::string> expectedSemanticPublications{
      "lbound", "semantic_publication_length"};
  std::set<std::string> observedExact;
  std::set<std::string> observedSemanticPublications;
  std::size_t intrinsicShadows = 0;
  std::vector<SgFunctionRefExp *> references;
  std::set<SgFunctionRefExp *> uniqueReferences;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgFunctionRefExp)) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    if (uniqueReferences.insert(reference).second) {
      references.push_back(reference);
    }
  }
  SgProcedureHeaderStatement *publication = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgProcedureHeaderStatement)) {
    SgProcedureHeaderStatement *candidate = isSgProcedureHeaderStatement(node);
    if (candidate != nullptr &&
        candidate->get_name() == "semantic_publication" &&
        candidate->get_definition() != nullptr) {
      ROSE_ASSERT(publication == nullptr);
      publication = candidate;
    }
  }
  ROSE_ASSERT(publication != nullptr);
  SgFunctionType *publicationType = publication->get_type();
  auto appendSemanticPublicationReferences = [&](SgType *type,
                                                 bool sourceSurface) {
    if (type == nullptr) {
      return;
    }
    TypeSurface surface = typeSurface(type, sourceSurface);
    SgTypeString *stringType = isSgTypeString(surface.base);
    SgExpression *length =
        stringType != nullptr ? stringType->get_lengthExpression() : nullptr;
    if (length == nullptr) {
      return;
    }
    for (SgNode *node : NodeQuery::querySubTree(length, V_SgFunctionRefExp)) {
      SgFunctionRefExp *reference = isSgFunctionRefExp(node);
      if (reference != nullptr &&
          reference->get_fortran_source_visible_binding_kind() ==
              SgFunctionRefExp::
                  e_fortran_source_visible_binding_semantic_publication &&
          uniqueReferences.insert(reference).second) {
        references.push_back(reference);
      }
    }
  };
  appendSemanticPublicationReferences(publicationType->get_return_type(),
                                      false);
  if (publication->get_type_syntax_is_available()) {
    SgFunctionType *syntaxType = publication->get_type_syntax();
    appendSemanticPublicationReferences(
        syntaxType != nullptr ? syntaxType->get_return_type() : nullptr, true);
  }
  SgInitializedName *publicationResult = publication->get_result_name();
  ROSE_ASSERT(publicationResult != nullptr);
  ROSE_ASSERT(SageInterface::fortranSourceTypeMatchesSemanticType(
      publicationResult->get_fortran_source_type(),
      publicationResult->get_type()));
  appendSemanticPublicationReferences(publicationResult->get_type(), false);
  appendSemanticPublicationReferences(
      publicationResult->get_fortran_source_type(), true);

  for (SgFunctionRefExp *reference : references) {
    SgFunctionSymbol *semantic =
        reference != nullptr ? reference->get_symbol() : nullptr;
    if (semantic == nullptr) {
      continue;
    }
    const std::string name = lower(semantic->get_name().getString());
    if (expectedExact.count(name) == 0 &&
        expectedSemanticPublications.count(name) == 0 && name != "abs") {
      continue;
    }

    SgFunctionSymbol *sourceVisible =
        reference->get_fortran_source_visible_symbol();
    SgFunctionDeclaration *semanticDeclaration = semantic->get_declaration();
    SgFunctionDeclaration *sourceDeclaration =
        sourceVisible != nullptr ? sourceVisible->get_declaration() : nullptr;
    SgScopeStatement *sourceScope =
        sourceVisible != nullptr ? sourceVisible->get_scope() : nullptr;
    SgSymbolTable *sourceTable =
        sourceScope != nullptr ? sourceScope->get_symbol_table() : nullptr;
    const bool semanticPublicationKind =
        reference->get_fortran_source_visible_binding_kind() ==
        SgFunctionRefExp::e_fortran_source_visible_binding_semantic_publication;
    SgScopeStatement *useScope =
        semanticPublicationKind ? nullptr
                                : SageInterface::getEnclosingScope(reference);
    SgStatement *useStatement =
        semanticPublicationKind
            ? nullptr
            : SageInterface::getEnclosingStatement(reference);
    ROSE_ASSERT(sourceVisible != nullptr && semanticDeclaration != nullptr &&
                sourceDeclaration != nullptr && sourceScope != nullptr &&
                sourceTable != nullptr);
    ROSE_ASSERT(sourceVisible->get_name() == semantic->get_name());
    ROSE_ASSERT(sourceVisible->get_parent() == sourceTable);
    ROSE_ASSERT(sourceTable->exists(sourceVisible));

    SgFunctionSymbol *visibleBySemanticType =
        useScope != nullptr ? SageInterface::lookupFunctionSymbolInParentScopes(
                                  semantic->get_name(),
                                  semanticDeclaration->get_type(), useScope)
                            : nullptr;
    if (name == "abs") {
      ROSE_ASSERT(useScope != nullptr && useStatement != nullptr);
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_intrinsic_shadow);
      ROSE_ASSERT(semantic != sourceVisible);
      ROSE_ASSERT(visibleBySemanticType != sourceVisible);
      ROSE_ASSERT(sourceScope == useScope ||
                  SageInterface::isAncestor(sourceScope, useStatement));
      ++intrinsicShadows;
    } else if (expectedExact.count(name) != 0) {
      ROSE_ASSERT(useScope != nullptr);
      ROSE_ASSERT(
          reference->get_fortran_source_visible_binding_kind() ==
          SgFunctionRefExp::e_fortran_source_visible_binding_exact_typed);
      ROSE_ASSERT(visibleBySemanticType == sourceVisible);
      observedExact.insert(name);
    } else {
      SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(semanticDeclaration);
      SgAuxiliaryDeclarationList *owner =
          procedure != nullptr
              ? isSgAuxiliaryDeclarationList(procedure->get_parent())
              : nullptr;
      Sg_File_Info *source =
          procedure != nullptr ? procedure->get_file_info() : nullptr;
      ROSE_ASSERT(reference->get_fortran_source_visible_binding_kind() ==
                  SgFunctionRefExp::
                      e_fortran_source_visible_binding_semantic_publication);
      ROSE_ASSERT(semantic == sourceVisible);
      ROSE_ASSERT(procedure != nullptr);
      ROSE_ASSERT(procedure->get_firstNondefiningDeclaration() == procedure);
      ROSE_ASSERT(procedure->get_fortran_procedure_source_form() ==
                  SgProcedureHeaderStatement::
                      e_fortran_procedure_source_form_semantic_only);
      ROSE_ASSERT(owner != nullptr && owner->get_parent() == sourceScope);
      ROSE_ASSERT(sourceScope->get_auxiliary_declarations() == owner);
      ROSE_ASSERT(source != nullptr && source->isCompilerGenerated());
      ROSE_ASSERT(source->isOutputInCodeGeneration());
      observedSemanticPublications.insert(name);
    }
  }
  ROSE_ASSERT(observedExact == expectedExact);
  ROSE_ASSERT(observedSemanticPublications == expectedSemanticPublications);
  ROSE_ASSERT(intrinsicShadows == 1);
}

void verifyCharacterSelector(SgInitializedName *name,
                             std::optional<std::int64_t> length, bool assumed,
                             bool deferred) {
  TypeSurface source = typeSurface(name->get_fortran_source_type(), true);
  TypeSurface semantic = typeSurface(name->get_type(), false);
  SgTypeString *sourceString = isSgTypeString(source.base);
  SgTypeString *semanticString = isSgTypeString(semantic.base);
  ROSE_ASSERT(sourceString != nullptr && semanticString != nullptr);
  SgExpression *sourceLength = sourceString->get_lengthExpression();
  SgExpression *semanticLength = semanticString->get_lengthExpression();
  if (assumed) {
    ROSE_ASSERT(isSgAsteriskShapeExp(sourceLength) != nullptr);
    ROSE_ASSERT(isSgAsteriskShapeExp(semanticLength) != nullptr);
    ROSE_ASSERT(!sourceLength->has_semantic_value_type());
    ROSE_ASSERT(!semanticLength->has_semantic_value_type());
    requireNoFoldedSelector(sourceLength);
    requireNoFoldedSelector(semanticLength);
  } else if (deferred) {
    ROSE_ASSERT(isSgColonShapeExp(sourceLength) != nullptr);
    ROSE_ASSERT(isSgColonShapeExp(semanticLength) != nullptr);
    ROSE_ASSERT(!sourceLength->has_semantic_value_type());
    ROSE_ASSERT(!semanticLength->has_semantic_value_type());
    requireNoFoldedSelector(sourceLength);
    requireNoFoldedSelector(semanticLength);
  } else if (length) {
    requireFoldedSelector(sourceLength, *length);
    ROSE_ASSERT(integerLiteral(semanticLength) == length);
    requireNoFoldedSelector(semanticLength);
  } else {
    ROSE_ASSERT(sourceLength == nullptr);
    ROSE_ASSERT(integerLiteral(semanticLength) ==
                std::optional<std::int64_t>{1});
    requireNoFoldedSelector(semanticLength);
  }
}

void verifyProcedureComponentBindings(SgSourceFile *file) {
  using Kind = SgInitializedName::FortranProcedurePassKind;
  std::set<std::string> observed;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    SgInitializedName *callback = isSgInitializedName(node);
    const auto &binding = callback->get_fortran_procedure_component_binding();
    if (!binding)
      continue;
    const std::string name = callback->get_name().getString();
    ROSE_ASSERT(observed.insert(name).second);
    ROSE_ASSERT(binding->isValidForType(callback->get_type()));
    SgPointerType *semantic = isSgPointerType(callback->get_type());
    SgPointerType *source =
        isSgPointerType(callback->get_fortran_source_type());
    ROSE_ASSERT(semantic != nullptr && source != nullptr && source != semantic);
    ROSE_ASSERT(source->get_fortran_source_syntax());
    ROSE_ASSERT(!semantic->get_fortran_source_syntax());
    ROSE_ASSERT(source->get_base_type() == semantic->get_base_type());
    if (name == "callback") {
      ROSE_ASSERT(binding->kind == Kind::no_pass);
      ROSE_ASSERT(!binding->passed_object_index);
    } else if (name == "implicit_callback") {
      ROSE_ASSERT(binding->kind == Kind::implicit_pass);
      ROSE_ASSERT(binding->passed_object_index == 0);
    } else if (name == "explicit_callback") {
      ROSE_ASSERT(binding->kind == Kind::explicit_pass);
      ROSE_ASSERT(binding->passed_object_index == 0);
    } else {
      ROSE_ASSERT(name == "named_callback");
      ROSE_ASSERT(binding->kind == Kind::named_pass);
      ROSE_ASSERT(binding->passed_object_index == 1);
      ROSE_ASSERT(binding->written_dummy_name == "self");
    }
  }
  ROSE_ASSERT(observed ==
              std::set<std::string>({"callback", "implicit_callback",
                                     "explicit_callback", "named_callback"}));
}

void verifyObjectSourceForms(SgSourceFile *file) {
  verifyProcedureComponentBindings(file);
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgFunctionParameterList)) {
    SgFunctionParameterList *parameters = isSgFunctionParameterList(node);
    ROSE_ASSERT(parameters != nullptr);
    for (SgInitializedName *parameter : parameters->get_args()) {
      ROSE_ASSERT(parameter != nullptr);
      ROSE_ASSERT(parameter->get_type() != nullptr);
      ROSE_ASSERT(parameter->get_fortran_source_type() == nullptr);
      ROSE_ASSERT(parameter->get_fortran_source_derived_type_symbol() ==
                  nullptr);
      ROSE_ASSERT(parameter->get_fortran_type_spec() ==
                  SgInitializedName::e_fortran_type_spec_default);
      ROSE_ASSERT(parameter->get_fortran_procedure_interface() == nullptr);
      ROSE_ASSERT(parameter->get_fortran_separate_shape_declaration() ==
                  nullptr);
    }
  }

  auto verifyParameter = [&](const char *name) {
    SgInitializedName *parameter = sourceName(file, name);
    SgVariableDeclaration *declaration =
        isSgVariableDeclaration(parameter->get_parent());
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_declarationModifier()
                    .get_typeModifier()
                    .get_constVolatileModifier()
                    .isConst());
    ROSE_ASSERT(parameter->get_initializer() != nullptr);
    SgModifierType *semanticType = isSgModifierType(parameter->get_type());
    ROSE_ASSERT(semanticType != nullptr);
    ROSE_ASSERT(
        semanticType->get_typeModifier().get_constVolatileModifier().isConst());
  };
  verifyParameter("source_kind");
  verifyParameter("source_len");
  SgInitializedName *standalone = sourceName(file, "standalone_count");
  ROSE_ASSERT(SageInterface::isConstType(standalone->get_type()));
  ROSE_ASSERT(standalone->get_initializer() == nullptr);
  SgInitializedName *constantShape = sourceName(file, "constant_shape");
  ROSE_ASSERT(SageInterface::isConstType(constantShape->get_type()));
  ROSE_ASSERT(constantShape->get_fortran_separate_shape_declaration() !=
              nullptr);
  ROSE_ASSERT(typeSurface(constantShape->get_type(), false).rank == 1);
  SgInitializedName *pointerMember = sourceName(file, "pointer_member");
  ROSE_ASSERT(typeSurface(pointerMember->get_type(), false).pointer);
  ROSE_ASSERT(typeSurface(pointerMember->get_type(), false).rank == 1);

  std::size_t explicitDerivedArrayConstructors = 0;
  std::size_t explicitDerivedStructureConstructors = 0;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgAggregateInitializer)) {
    SgAggregateInitializer *aggregate = isSgAggregateInitializer(node);
    ROSE_ASSERT(aggregate != nullptr);
    if (!aggregate->get_fortran_has_source_explicit_type()) {
      ROSE_ASSERT(aggregate->get_fortran_source_explicit_type() == nullptr);
      continue;
    }
    SgClassType *sourceType =
        isSgClassType(aggregate->get_fortran_source_explicit_type());
    if (sourceType == nullptr || sourceType->get_name() != "empty_box") {
      continue;
    }
    TypeSurface semantic = typeSurface(aggregate->get_expression_type(), false);
    ROSE_ASSERT(semantic.base == sourceType);
    if (aggregate->get_source_form() ==
        SgAggregateInitializer::e_aggregate_initializer_source_fortran) {
      ROSE_ASSERT(semantic.rank == 1);
      ++explicitDerivedArrayConstructors;
    } else {
      ROSE_ASSERT(aggregate->get_source_form() ==
                  SgAggregateInitializer::
                      e_aggregate_initializer_source_fortran_structure);
      ROSE_ASSERT(semantic.rank == 0);
      ++explicitDerivedStructureConstructors;
    }
  }
  ROSE_ASSERT(explicitDerivedArrayConstructors == 1);
  ROSE_ASSERT(explicitDerivedStructureConstructors == 1);

  auto verifyIntrinsic = [&](const char *name, std::int64_t sourceKind,
                             std::int64_t resolvedKind, bool star,
                             VariantT expectedVariant) {
    SgInitializedName *object = sourceName(file, name);
    TypeSurface source = typeSurface(object->get_fortran_source_type(), true);
    ROSE_ASSERT(source.base->variantT() == expectedVariant);
    ROSE_ASSERT(source.base->get_hasTypeKindStar() == star);
    requireFoldedSelector(source.base->get_type_kind(), sourceKind);
    if (SgTypeComplex *complex = isSgTypeComplex(source.base)) {
      SgType *component = complex->get_base_type();
      ROSE_ASSERT(component != nullptr);
      ROSE_ASSERT(complex->get_type_kind() == component->get_type_kind());
      ROSE_ASSERT(complex->get_type_kind()->get_parent() == component);
    }
    ROSE_ASSERT(semanticKind(object->get_type()) == resolvedKind);
  };
  verifyIntrinsic("star_integer", 4, 4, true, V_SgTypeInt);
  verifyIntrinsic("explicit_real", 8, 8, false, V_SgTypeFloat);
  verifyIntrinsic("star_real", 8, 8, true, V_SgTypeFloat);
  verifyIntrinsic("explicit_complex", 8, 8, false, V_SgTypeComplex);
  verifyIntrinsic("star_complex", 16, 8, true, V_SgTypeComplex);
  verifyIntrinsic("explicit_logical", 4, 4, false, V_SgTypeBool);
  verifyIntrinsic("star_logical", 4, 4, true, V_SgTypeBool);

  SgInitializedName *kindOnly = sourceName(file, "kind_only_char");
  verifyCharacterSelector(kindOnly, std::nullopt, false, false);
  SgTypeString *kindOnlySource = isSgTypeString(
      typeSurface(kindOnly->get_fortran_source_type(), true).base);
  ROSE_ASSERT(kindOnlySource != nullptr);
  requireFoldedSelector(kindOnlySource->get_type_kind(),
                        semanticKind(kindOnly->get_type()));

  SgInitializedName *explicitChar = sourceName(file, "explicit_char");
  verifyCharacterSelector(explicitChar, 3, false, false);
  SgTypeString *explicitSource = isSgTypeString(
      typeSurface(explicitChar->get_fortran_source_type(), true).base);
  requireFoldedSelector(explicitSource->get_type_kind(),
                        semanticKind(explicitChar->get_type()));

  SgInitializedName *first = sourceName(file, "first");
  SgInitializedName *second = sourceName(file, "second");
  verifyCharacterSelector(first, 2, false, false);
  verifyCharacterSelector(second, 3, false, false);
  ROSE_ASSERT(first->get_fortran_source_type() !=
              second->get_fortran_source_type());
  SgInitializedName *left = sourceName(file, "left");
  SgInitializedName *right = sourceName(file, "right");
  verifyCharacterSelector(left, 2, false, false);
  verifyCharacterSelector(right, 3, false, false);
  ROSE_ASSERT(left->get_fortran_source_type() !=
              right->get_fortran_source_type());
  SgType *firstSemanticBase = typeSurface(first->get_type(), false).base;
  SgType *leftSemanticBase = typeSurface(left->get_type(), false).base;
  SgTypeTable *typeTable = SgNode::get_globalTypeTable();
  ROSE_ASSERT(firstSemanticBase != nullptr &&
              firstSemanticBase == leftSemanticBase);
  ROSE_ASSERT(typeTable != nullptr &&
              typeTable->lookup_type(firstSemanticBase->get_mangled()) ==
                  firstSemanticBase);
  SgInitializedName *componentText = sourceName(file, "component_text");
  verifyCharacterSelector(componentText, 3, false, false);
  ROSE_ASSERT(
      typeSurface(componentText->get_fortran_source_type(), true).rank == 1);
  ROSE_ASSERT(typeSurface(componentText->get_type(), false).rank == 1);

  SgInitializedName *pointerText = sourceName(file, "pointer_text");
  verifyCharacterSelector(pointerText, 3, false, false);
  TypeSurface pointerTextSource =
      typeSurface(pointerText->get_fortran_source_type(), true);
  TypeSurface pointerTextSemantic = typeSurface(pointerText->get_type(), false);
  ROSE_ASSERT(pointerTextSource.pointer && pointerTextSemantic.pointer);
  ROSE_ASSERT(pointerTextSource.rank == 1 && pointerTextSemantic.rank == 1);

  verifyCharacterSelector(sourceName(file, "assumed_text"), std::nullopt, true,
                          false);
  verifyCharacterSelector(sourceName(file, "deferred_text"), std::nullopt,
                          false, true);

  SgInitializedName *dynamicText = sourceName(file, "dynamic_text");
  TypeSurface dynamicSource =
      typeSurface(dynamicText->get_fortran_source_type(), true);
  TypeSurface dynamicSemantic = typeSurface(dynamicText->get_type(), false);
  SgTypeString *dynamicSourceString = isSgTypeString(dynamicSource.base);
  SgTypeString *dynamicSemanticString = isSgTypeString(dynamicSemantic.base);
  ROSE_ASSERT(dynamicSourceString != nullptr &&
              dynamicSemanticString != nullptr);
  SgExpression *dynamicSourceLength =
      dynamicSourceString->get_lengthExpression();
  SgExpression *dynamicSemanticLength =
      dynamicSemanticString->get_lengthExpression();
  ROSE_ASSERT(dynamicSourceLength != nullptr &&
              dynamicSemanticLength != nullptr &&
              dynamicSourceLength != dynamicSemanticLength);
  ROSE_ASSERT(isSgValueExp(dynamicSourceLength) == nullptr &&
              isSgValueExp(dynamicSemanticLength) == nullptr);
  requireNoFoldedSelector(dynamicSourceLength);
  requireNoFoldedSelector(dynamicSemanticLength);
  ROSE_ASSERT(dynamicSourceLength->get_parent() == dynamicSourceString);
  ROSE_ASSERT(dynamicSemanticLength->get_parent() == dynamicSemanticString);
  ROSE_ASSERT(!dynamicSemanticString->get_fortran_dynamic_length_pending());
  ROSE_ASSERT(!dynamicSemanticString->get_fortran_dynamic_result_length());
  SgTypeTable *dynamicTypeTable = SgNode::get_globalTypeTable();
  ROSE_ASSERT(dynamicTypeTable != nullptr);
  ROSE_ASSERT(
      dynamicTypeTable->lookup_type(dynamicSemanticString->get_mangled()) !=
      dynamicSemanticString);
  ROSE_ASSERT(SageInterface::fortranSourceTypeMatchesSemanticType(
      dynamicSource.base, dynamicSemantic.base));

  SgInitializedName *assumedAny = sourceName(file, "assumed_any");
  TypeSurface assumedSource =
      typeSurface(assumedAny->get_fortran_source_type(), true);
  TypeSurface assumedSemantic = typeSurface(assumedAny->get_type(), false);
  ROSE_ASSERT(isSgTypeFortranAssumed(assumedSource.base) != nullptr);
  ROSE_ASSERT(isSgTypeFortranAssumed(assumedSemantic.base) != nullptr);
  ROSE_ASSERT(assumedAny->get_fortran_type_spec() ==
              SgInitializedName::e_fortran_type_spec_type_star);
  ROSE_ASSERT(SageInterface::fortranSourceTypeMatchesSemanticType(
      assumedSource.base, assumedSemantic.base));

  SgInitializedName *unlimitedAny = sourceName(file, "unlimited_any");
  TypeSurface unlimitedSource =
      typeSurface(unlimitedAny->get_fortran_source_type(), true);
  TypeSurface unlimitedSemantic = typeSurface(unlimitedAny->get_type(), false);
  ROSE_ASSERT(isSgTypeFortranUnlimitedPolymorphic(unlimitedSource.base) !=
              nullptr);
  ROSE_ASSERT(isSgTypeFortranUnlimitedPolymorphic(unlimitedSemantic.base) !=
              nullptr);
  ROSE_ASSERT(unlimitedAny->get_fortran_type_spec() ==
              SgInitializedName::e_fortran_type_spec_class_star);
  ROSE_ASSERT(SageInterface::fortranSourceTypeMatchesSemanticType(
      unlimitedSource.base, unlimitedSemantic.base));

  bool sawDynamicCharacterResult = false;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    ROSE_ASSERT(call != nullptr);
    SgTypeString *result =
        isSgTypeString(typeSurface(call->get_type(), false).base);
    if (result == nullptr || !result->get_fortran_dynamic_result_length()) {
      continue;
    }
    ROSE_ASSERT(result->get_fortran_dynamic_result_length());
    ROSE_ASSERT(!result->get_fortran_dynamic_length_pending());
    ROSE_ASSERT(result->get_lengthExpression() == nullptr);
    ROSE_ASSERT(result->get_type_kind() != nullptr);
    ROSE_ASSERT(result->get_type_kind()->get_parent() == result);
    sawDynamicCharacterResult = true;
  }
  ROSE_ASSERT(sawDynamicCharacterResult);

  SgInitializedName *member = sourceName(file, "member");
  TypeSurface memberSource =
      typeSurface(member->get_fortran_source_type(), true);
  requireFoldedSelector(memberSource.base->get_type_kind(),
                        semanticKind(member->get_type()));

  for (const std::string &name : {"fixed_real", "fixed_complex"}) {
    SgInitializedName *fixed = sourceName(file, name);
    TypeSurface source = typeSurface(fixed->get_fortran_source_type(), true);
    const std::int64_t kind = semanticKind(fixed->get_type());
    ROSE_ASSERT(source.base->get_fortran_fixed_kind_value_is_available());
    ROSE_ASSERT(source.base->get_fortran_fixed_kind_value() == kind);
    if (SgTypeComplex *complex = isSgTypeComplex(source.base)) {
      SgType *component = complex->get_base_type();
      ROSE_ASSERT(component->get_fortran_source_syntax());
      ROSE_ASSERT(component->get_fortran_fixed_kind_value_is_available());
      ROSE_ASSERT(component->get_fortran_fixed_kind_value() == kind);
    } else {
      ROSE_ASSERT(isSgTypeDouble(source.base) != nullptr);
    }

    SgTreeCopy copyHelp;
    SgType *copy = isSgType(source.base->copy(copyHelp));
    ROSE_ASSERT(copy != nullptr && copy != source.base);
    ROSE_ASSERT(copy->get_fortran_source_syntax());
    ROSE_ASSERT(copy->get_fortran_fixed_kind_value_is_available());
    ROSE_ASSERT(copy->get_fortran_fixed_kind_value() == kind);
    SageInterface::deleteAST(copy,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }

  SgInitializedName *pointer = sourceName(file, "pointer_values");
  TypeSurface pointerSource =
      typeSurface(pointer->get_fortran_source_type(), true);
  TypeSurface pointerSemantic = typeSurface(pointer->get_type(), false);
  ROSE_ASSERT(pointerSource.pointer && pointerSemantic.pointer);
  ROSE_ASSERT(pointerSource.rank == 1 && pointerSemantic.rank == 1);
  requireFoldedSelector(pointerSource.base->get_type_kind(),
                        semanticKind(pointer->get_type()));

  SgInitializedName *dimensioned = sourceName(file, "dimensioned");
  TypeSurface dimensionSource =
      typeSurface(dimensioned->get_fortran_source_type(), true);
  TypeSurface dimensionSemantic = typeSurface(dimensioned->get_type(), false);
  ROSE_ASSERT(dimensionSource.rank == 0);
  ROSE_ASSERT(dimensionSemantic.rank == 1);
  SgAttributeSpecificationStatement *dimension =
      isSgAttributeSpecificationStatement(
          dimensioned->get_fortran_separate_shape_declaration());
  ROSE_ASSERT(dimension != nullptr);
  SgArrayType *semanticArray = isSgArrayType(dimensioned->get_type());
  ROSE_ASSERT(semanticArray != nullptr);
  ROSE_ASSERT(semanticArray->get_dim_info() != nullptr);
  ROSE_ASSERT(semanticArray->get_dim_info()->get_expressions().size() == 1);
  SgExpression *dimensionExpression =
      semanticArray->get_dim_info()->get_expressions().front();
  ROSE_ASSERT(isSgColonShapeExp(dimensionExpression) != nullptr);
  ROSE_ASSERT(!dimensionExpression->has_semantic_value_type());
  SgExprListExp *parameters = dimension->get_parameter_list();
  ROSE_ASSERT(parameters != nullptr &&
              parameters->get_expressions().size() == 1);
  SgPntrArrRefExp *shape =
      isSgPntrArrRefExp(parameters->get_expressions().front());
  SgVarRefExp *shapeObject =
      shape != nullptr ? isSgVarRefExp(shape->get_lhs_operand_i()) : nullptr;
  SgExprListExp *shapeIndices =
      shape != nullptr ? isSgExprListExp(shape->get_rhs_operand_i()) : nullptr;
  ROSE_ASSERT(shape != nullptr && shape->get_parent() == parameters);
  ROSE_ASSERT(shapeObject != nullptr && shapeObject->get_parent() == shape);
  ROSE_ASSERT(shapeObject->get_symbol() ==
              dimensioned->get_symbol_from_symbol_table());
  ROSE_ASSERT(shapeIndices != nullptr && shapeIndices->get_parent() == shape &&
              shapeIndices->get_expressions().size() == 1);
  SgSubscriptExpression *shapeRange =
      isSgSubscriptExpression(shapeIndices->get_expressions().front());
  ROSE_ASSERT(shapeRange != nullptr &&
              shapeRange->get_parent() == shapeIndices);
  ROSE_ASSERT(integerLiteral(shapeRange->get_lowerBound()) ==
              std::optional<std::int64_t>{2});
  ROSE_ASSERT(integerLiteral(shapeRange->get_upperBound()) ==
              std::optional<std::int64_t>{5});
  ROSE_ASSERT(integerLiteral(shapeRange->get_stride()) ==
              std::optional<std::int64_t>{1});
  ROSE_ASSERT(shapeRange->get_lowerBound()->get_parent() == shapeRange);
  ROSE_ASSERT(shapeRange->get_upperBound()->get_parent() == shapeRange);
  ROSE_ASSERT(shapeRange->get_stride()->get_parent() == shapeRange);

  SgTreeCopy selectorCopyHelp;
  SgExpression *selectorCopy = isSgExpression(
      explicitSource->get_lengthExpression()->copy(selectorCopyHelp));
  requireFoldedSelector(selectorCopy, 3);
  SageInterface::deleteAST(selectorCopy,
                           SageInterface::DeleteAstMode::kRequireIsolated);
}

std::map<std::string, SgInitializedName *>
verifyInterfaceRoles(SgSourceFile *file) {
  std::map<std::string, SgInitializedName *> entities;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    auto *name = isSgInitializedName(node);
    if (name->get_fortran_procedure_interface() != nullptr)
      ROSE_ASSERT(entities.emplace(name->get_name().getString(), name).second);
  }
  for (const char *name :
       {"abstract_proc", "callback", "implicit_callback", "explicit_callback",
        "named_callback", "typed_proc", "renamed_proc", "pointer_interface",
        "derived_proc"}) {
    auto *entity = entities.at(name);
    auto *sourceType = isSgPointerType(entity->get_fortran_source_type());
    auto *semanticType = isSgPointerType(entity->get_type());
    ROSE_ASSERT(sourceType != nullptr && semanticType != nullptr);
    ROSE_ASSERT(sourceType != semanticType &&
                sourceType->get_fortran_source_syntax());
    ROSE_ASSERT(sourceType->get_base_type() == semanticType->get_base_type());
    SgExpression *interface = entity->get_fortran_procedure_interface();
    ROSE_ASSERT(interface != nullptr && interface->get_parent() == entity);
    ROSE_ASSERT(SgInitializedName::is_valid_fortran_procedure_interface(
        interface, entity->get_type()));
    if (entity->get_name() == "typed_proc") {
      auto *typeExpression = isSgTypeExpression(interface);
      ROSE_ASSERT(typeExpression != nullptr);
      SgType *type = typeExpression->get_represented_type();
      ROSE_ASSERT(type->get_fortran_source_syntax());
      ROSE_ASSERT(type->get_parent() == typeExpression);
      ROSE_ASSERT(type->get_type_kind() != nullptr);
    } else if (entity->get_name() == "derived_proc") {
      auto *typeExpression = isSgTypeExpression(interface);
      ROSE_ASSERT(typeExpression != nullptr);
      ROSE_ASSERT(isSgClassType(typeExpression->get_represented_type()) !=
                  nullptr);
      ROSE_ASSERT(entity->get_fortran_type_spec() ==
                  SgInitializedName::e_fortran_type_spec_type);
      SgSymbol *source = entity->get_fortran_source_derived_type_symbol();
      ROSE_ASSERT(source != nullptr && source->get_name() == "local_box");
      ROSE_ASSERT(source->get_type() == typeExpression->get_represented_type());
    } else if (entity->get_name() == "pointer_interface") {
      auto *reference = isSgVarRefExp(interface);
      ROSE_ASSERT(reference != nullptr);
      ROSE_ASSERT(reference->get_symbol()->get_declaration() ==
                  entities.at("abstract_proc"));
    } else {
      ROSE_ASSERT(isSgFunctionRefExp(interface) != nullptr);
      if (entity->get_name() == "renamed_proc")
        ROSE_ASSERT(
            isSgFunctionRefExp(interface)
                ->get_fortran_source_visible_binding_kind() ==
            SgFunctionRefExp::e_fortran_source_visible_binding_use_rename);
    }
  }
  std::set<std::string> signatures;
  std::set<std::string> operators;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInterfaceStatement)) {
    auto *interface = isSgInterfaceStatement(node);
    ROSE_ASSERT(interface != nullptr);
    const auto &bodies = interface->get_interface_body_list();
    const bool isAbstract =
        interface->get_declarationModifier().get_typeModifier().isAbstract();
    if (interface->get_generic_spec() ==
        SgInterfaceStatement::e_operator_interface_type) {
      const auto name = lower(interface->get_name().getString());
      ROSE_ASSERT(operators.insert(name).second);
      ROSE_ASSERT(!isAbstract &&
                  bodies.size() == (name == ".rexchoice." ? 2 : 1));
      for (auto *body : bodies) {
        ROSE_ASSERT(body != nullptr && body->get_use_function_name());
        ROSE_ASSERT(body->get_functionDeclaration() == nullptr);
        ROSE_ASSERT(body->get_function_name() == "integer_choice" ||
                    body->get_function_name() == "box_choice");
      }
      continue;
    }
    ROSE_ASSERT(interface->get_name().is_null());
    ROSE_ASSERT(interface->get_generic_spec() ==
                SgInterfaceStatement::e_unnamed_interface_type);
    ROSE_ASSERT(!bodies.empty() && bodies.front() != nullptr);
    auto *first = bodies.front()->get_functionDeclaration();
    ROSE_ASSERT(first != nullptr);
    if (first->get_name() == "rex_default_pass_signature") {
      ROSE_ASSERT(isAbstract && bodies.size() == 2);
      ROSE_ASSERT(bodies.back() != nullptr);
      ROSE_ASSERT(bodies.back()->get_functionDeclaration() != nullptr);
      ROSE_ASSERT(bodies.back()->get_functionDeclaration()->get_name() ==
                  "rex_named_pass_signature");
    } else {
      ROSE_ASSERT(bodies.size() == 1);
    }
    for (auto *body : bodies) {
      ROSE_ASSERT(body != nullptr && !body->get_use_function_name());
      auto *procedure = body->get_functionDeclaration();
      ROSE_ASSERT(procedure != nullptr);
      const std::string name = procedure->get_name().getString();
      ROSE_ASSERT(signatures.insert(name).second);
      ROSE_ASSERT(isAbstract == (name != "unlimited_factory"));
    }
  }
  ROSE_ASSERT(signatures ==
              std::set<std::string>(
                  {"unlimited_factory", "rex_abstract_signature",
                   "rex_default_pass_signature", "rex_named_pass_signature"}));
  ROSE_ASSERT(operators ==
              std::set<std::string>({".rexchoice.", ".not.", "+", "-"}));
  return entities;
}

void verifyUnaryOperatorBindings(SgSourceFile *file) {
  unsigned integers = 0, arrays = 0;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgUserDefinedUnaryOp)) {
    auto *operation = isSgUserDefinedUnaryOp(node);
    ROSE_ASSERT(operation->get_symbol() != nullptr);
    auto *declaration = operation->get_symbol()->get_declaration();
    ROSE_ASSERT(declaration != nullptr);
    TypeSurface result = typeSurface(operation->get_type(), false);
    if (declaration->get_name() == "integer_choice") {
      ROSE_ASSERT(result.rank == 0 && isSgTypeInt(result.base) != nullptr);
      ++integers;
    } else {
      ROSE_ASSERT(declaration->get_name() == "box_choice");
      ROSE_ASSERT(result.rank == 1 && isSgTypeFloat(result.base) != nullptr);
      ++arrays;
    }
  }
  ROSE_ASSERT(integers == 2 && arrays == 5);
}

void verifyOutput(const std::string &path) {
  std::ifstream output(path);
  ROSE_ASSERT(output.good());
  bool sawExpressionKind = false;
  bool sawKindOnlyCharacter = false;
  bool sawFirstEntityLength = false;
  bool sawSecondEntityLength = false;
  bool sawDimension = false;
  bool sawDoublePrecision = false;
  bool sawDoubleComplex = false;
  bool sawStarInteger = false;
  bool sawStarComplex = false;
  bool sawExplicitReal = false;
  bool sawDynamicLength = false;
  bool sawAssumedType = false;
  bool sawUnlimitedPolymorphic = false;
  bool sawIntrinsicFunctionPrefix = false;
  bool sawDerivedTypeFunctionPrefix = false;
  bool sawClassFunctionPrefix = false;
  bool sawDerivedArrayConstructorType = false;
  int abstractInterfaces = 0;
  int ordinaryInterfaces = 0;
  std::string line;
  while (std::getline(output, line)) {
    const std::string text = lower(line);
    const std::size_t firstToken = text.find_first_not_of(" \t");
    if (firstToken != std::string::npos &&
        text.compare(firstToken, 18, "abstract interface") == 0) {
      ++abstractInterfaces;
    } else if (firstToken != std::string::npos &&
               text.compare(firstToken, 9, "interface") == 0) {
      ++ordinaryInterfaces;
    }
    const bool endProcedure = firstToken != std::string::npos &&
                              text.compare(firstToken, 3, "end") == 0;
    if (text.find("m_twice") != std::string::npos &&
        text.find("external") != std::string::npos) {
      ROSE_ASSERT(text.find("source_kind") != std::string::npos);
      sawExpressionKind = true;
    }
    if (!endProcedure && text.find("function m_twice") != std::string::npos) {
      ROSE_ASSERT(text.find("integer") != std::string::npos);
      ROSE_ASSERT(text.find("kind(0)") != std::string::npos);
      sawIntrinsicFunctionPrefix = true;
    }
    if (!endProcedure &&
        text.find("function copy_selector_box") != std::string::npos) {
      ROSE_ASSERT(text.find("type(selector_box)") != std::string::npos);
      sawDerivedTypeFunctionPrefix = true;
    }
    if (!endProcedure &&
        text.find("function allocate_selector_box") != std::string::npos) {
      ROSE_ASSERT(text.find("class(selector_box)") != std::string::npos);
      sawClassFunctionPrefix = true;
    }
    if (text.find("empty_box ::") != std::string::npos) {
      ROSE_ASSERT(text.find("type(empty_box)") == std::string::npos);
      sawDerivedArrayConstructorType = true;
    }
    if (text.find("kind_only_char") != std::string::npos) {
      ROSE_ASSERT(text.find("kind") != std::string::npos);
      ROSE_ASSERT(text.find("len") == std::string::npos);
      sawKindOnlyCharacter = true;
    }
    if (text.find(":: first") != std::string::npos) {
      ROSE_ASSERT(text.find("len=2") != std::string::npos ||
                  text.find("*2") != std::string::npos ||
                  text.find("*(2)") != std::string::npos);
      sawFirstEntityLength = true;
    }
    if (text.find(":: second") != std::string::npos) {
      ROSE_ASSERT(text.find("len=3") != std::string::npos ||
                  text.find("*3") != std::string::npos ||
                  text.find("*(3)") != std::string::npos);
      sawSecondEntityLength = true;
    }
    if (text.find("dimension ") != std::string::npos &&
        text.find("dimensioned") != std::string::npos) {
      ROSE_ASSERT(text.find("2:5") != std::string::npos);
      sawDimension = true;
    }
    if (text.find("star_integer") != std::string::npos) {
      ROSE_ASSERT(text.find("*4") != std::string::npos);
      sawStarInteger = true;
    }
    if (text.find("star_complex") != std::string::npos) {
      ROSE_ASSERT(text.find("*16") != std::string::npos);
      sawStarComplex = true;
    }
    if (text.find("explicit_real") != std::string::npos) {
      ROSE_ASSERT(text.find("source_kind") != std::string::npos);
      sawExplicitReal = true;
    }
    if (text.find("dynamic_text") != std::string::npos) {
      ROSE_ASSERT(text.find("runtime_length") != std::string::npos);
      sawDynamicLength = true;
    }
    if (text.find(":: assumed_any") != std::string::npos) {
      ROSE_ASSERT(text.find("type(*)") != std::string::npos);
      sawAssumedType = true;
    }
    if (text.find(":: unlimited_any") != std::string::npos) {
      ROSE_ASSERT(text.find("class(*)") != std::string::npos);
      sawUnlimitedPolymorphic = true;
    }
    sawDoublePrecision = sawDoublePrecision ||
                         text.find("double precision") != std::string::npos;
    sawDoubleComplex =
        sawDoubleComplex || text.find("double complex") != std::string::npos;
  }
  ROSE_ASSERT(sawExpressionKind && sawKindOnlyCharacter &&
              sawFirstEntityLength && sawSecondEntityLength && sawDimension &&
              sawDoublePrecision && sawDoubleComplex && sawStarInteger &&
              sawStarComplex && sawExplicitReal && sawDynamicLength &&
              sawAssumedType && sawUnlimitedPolymorphic &&
              sawIntrinsicFunctionPrefix && sawDerivedTypeFunctionPrefix &&
              sawClassFunctionPrefix && sawDerivedArrayConstructorType);
  ROSE_ASSERT(abstractInterfaces == 2 && ordinaryInterfaces == 5);
}

void verifyUnsignedSourceForms(SgSourceFile *file) {
  SgInitializedName *explicitUnsigned = sourceName(file, "explicit_unsigned");
  TypeSurface explicitSource =
      typeSurface(explicitUnsigned->get_fortran_source_type(), true);
  ROSE_ASSERT(isSgTypeUnsignedInt(explicitSource.base) != nullptr);
  ROSE_ASSERT(!explicitSource.base->get_hasTypeKindStar());
  SgExpression *explicitKind = explicitSource.base->get_type_kind();
  requireFoldedSelector(explicitKind, 4);
  ROSE_ASSERT(isSgValueExp(explicitKind) == nullptr);
  ROSE_ASSERT(semanticKind(explicitUnsigned->get_type()) == 4);

  SgInitializedName *starUnsigned = sourceName(file, "star_unsigned");
  TypeSurface starSource =
      typeSurface(starUnsigned->get_fortran_source_type(), true);
  ROSE_ASSERT(isSgTypeUnsignedInt(starSource.base) != nullptr);
  ROSE_ASSERT(starSource.base->get_hasTypeKindStar());
  requireFoldedSelector(starSource.base->get_type_kind(), 4);
  ROSE_ASSERT(semanticKind(starUnsigned->get_type()) == 4);
}

void verifyUnsignedOutput(const std::string &path) {
  std::ifstream output(path);
  ROSE_ASSERT(output.good());
  bool sawExplicit = false;
  bool sawStar = false;
  std::string line;
  while (std::getline(output, line)) {
    const std::string text = lower(line);
    if (text.find("explicit_unsigned") != std::string::npos) {
      ROSE_ASSERT(text.find("source_kind") != std::string::npos);
      sawExplicit = true;
    }
    if (text.find("star_unsigned") != std::string::npos) {
      ROSE_ASSERT(text.find("*4") != std::string::npos);
      sawStar = true;
    }
  }
  ROSE_ASSERT(sawExplicit && sawStar);
}

SgExpression *typedProcedureSelector(SgSourceFile *file) {
  SgProcedureHeaderStatement *procedure = typedProcedure(file, "k_twice");
  SgFunctionType *source = procedure->get_type_syntax();
  ROSE_ASSERT(source != nullptr);
  SgExpression *selector = source->get_return_type()->get_type_kind();
  requireFoldedSelector(selector, 4);
  return selector;
}

void verifyAllocationAndNullifyStatements(SgSourceFile *file) {
  const auto allocations = NodeQuery::querySubTree(file, V_SgAllocateStatement);
  ROSE_ASSERT(allocations.size() == 2);
  size_t withStat = 0;
  for (SgNode *node : allocations) {
    auto *statement = isSgAllocateStatement(node);
    auto *list = statement->get_expr_list();
    ROSE_ASSERT(list != nullptr && list->get_parent() == statement);
    ROSE_ASSERT(!list->get_expressions().empty());
    for (SgExpression *expression : list->get_expressions())
      ROSE_ASSERT(expression != nullptr && expression->get_parent() == list);
    if (SgExpression *stat = statement->get_stat_expression()) {
      ROSE_ASSERT(stat->get_parent() == statement);
      ++withStat;
    }
  }
  ROSE_ASSERT(withStat == 1);

  const auto deallocations =
      NodeQuery::querySubTree(file, V_SgDeallocateStatement);
  ROSE_ASSERT(deallocations.size() == 1);
  auto *statement = isSgDeallocateStatement(deallocations.front());
  auto *list = statement->get_expr_list();
  ROSE_ASSERT(list != nullptr && list->get_parent() == statement);
  ROSE_ASSERT(list->get_expressions().size() == 1);
  ROSE_ASSERT(list->get_expressions().front()->get_parent() == list);
  ROSE_ASSERT(statement->get_stat_expression() != nullptr &&
              statement->get_stat_expression()->get_parent() == statement);

  const auto nullifications =
      NodeQuery::querySubTree(file, V_SgNullifyStatement);
  ROSE_ASSERT(nullifications.size() == 1);
  auto *nullify = isSgNullifyStatement(nullifications.front());
  auto *pointers = nullify->get_pointer_list();
  ROSE_ASSERT(pointers != nullptr && pointers->get_parent() == nullify);
  ROSE_ASSERT(pointers->get_expressions().size() == 1);
  ROSE_ASSERT(pointers->get_expressions().front()->get_parent() == pointers);
}

void verifyDataInitialization(SgSourceFile *file) {
  size_t initializers = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(file, V_SgFortranDataInitializer)) {
    auto *initializer = isSgFortranDataInitializer(node);
    initializer->validate_initializer();
    auto *owner = isSgInitializedName(initializer->get_parent());
    ROSE_ASSERT(owner != nullptr &&
                owner->get_type() == initializer->get_type());
    const auto &values = initializer->get_values();
    if (owner->get_name() == "slash_values") {
      ROSE_ASSERT(values.size() == 2);
      auto *repeat = isSgVarRefExp(values[0]->get_repeat_expression());
      ROSE_ASSERT(repeat != nullptr &&
                  repeat->get_symbol()->get_name() == "data_copies");
      ROSE_ASSERT(repeat->get_fortran_integer_constant_value_is_available());
      ROSE_ASSERT(repeat->get_fortran_integer_constant_value() == 2);
      ROSE_ASSERT(values[1]->get_repeat_expression() == nullptr);
    } else {
      ROSE_ASSERT(owner->get_name() == "slash_scalar" && values.size() == 1);
      ROSE_ASSERT(values[0]->get_repeat_expression() == nullptr);
    }
    ++initializers;
  }
  ROSE_ASSERT(initializers == 2);
  const auto groups = NodeQuery::querySubTree(file, V_SgDataStatementGroup);
  ROSE_ASSERT(groups.size() == 1);
  auto *group = isSgDataStatementGroup(groups.front());
  group->validate_group();
  ROSE_ASSERT(group->get_object_list().size() == 1);
  const auto &objects = group->get_object_list()[0]
                            ->get_variableReference_list()
                            ->get_expressions();
  ROSE_ASSERT(objects.size() == 1);
  auto *object = isSgVarRefExp(objects[0]);
  ROSE_ASSERT(object != nullptr &&
              object->get_symbol()->get_name() == "data_values");
  ROSE_ASSERT(group->get_value_list().size() == 2);
  auto *repeat =
      isSgVarRefExp(group->get_value_list()[0]->get_repeat_expression());
  ROSE_ASSERT(repeat != nullptr &&
              repeat->get_symbol()->get_name() == "data_copies");
  ROSE_ASSERT(repeat->get_fortran_integer_constant_value_is_available());
  ROSE_ASSERT(repeat->get_fortran_integer_constant_value() == 2);
  ROSE_ASSERT(group->get_value_list()[1]->get_repeat_expression() == nullptr);
}

void verifyImplicitRanges(SgSourceFile *file) {
  size_t specifications = 0;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgImplicitStatement)) {
    auto *statement = isSgImplicitStatement(node);
    statement->validate_implicit_statement();
    if (statement->get_implicit_spec() !=
        SgImplicitStatement::e_has_implicit_spec_list)
      continue;
    ++specifications;
    const auto &ranges = statement->get_ranges();
    ROSE_ASSERT(ranges.size() == 6);
    ROSE_ASSERT(ranges[0]->get_first_letter() == 'a');
    ROSE_ASSERT(ranges[0]->get_last_letter() == 'a');
    ROSE_ASSERT(ranges[1]->get_first_letter() == 'b');
    ROSE_ASSERT(!ranges[1]->get_last_letter());
    ROSE_ASSERT(ranges[2]->get_first_letter() == 'c');
    ROSE_ASSERT(ranges[2]->get_last_letter() == 'd');
    ROSE_ASSERT(ranges[3]->get_first_letter() == 'e');
    ROSE_ASSERT(ranges[3]->get_last_letter() == 'f');
    ROSE_ASSERT(ranges[4]->get_first_letter() == 'h');
    ROSE_ASSERT(ranges[4]->get_last_letter() == 'h');
    ROSE_ASSERT(ranges[5]->get_first_letter() == 'x');
    ROSE_ASSERT(ranges[5]->get_last_letter() == 'z');
    for (size_t index = 0; index != 5; ++index) {
      ROSE_ASSERT(ranges[index]->get_type_spec() ==
                  SgFortranImplicitRange::e_intrinsic);
      ROSE_ASSERT(ranges[index]->get_type()->get_fortran_source_syntax());
      ROSE_ASSERT(ranges[index]->get_source_derived_type_symbol() == nullptr);
    }
    ROSE_ASSERT(isSgTypeInt(ranges[0]->get_type()) != nullptr);
    ROSE_ASSERT(isSgTypeInt(ranges[1]->get_type()) != nullptr);
    ROSE_ASSERT(ranges[0]->get_type() == ranges[1]->get_type());
    ROSE_ASSERT(isSgTypeFloat(ranges[2]->get_type()) != nullptr);
    auto *complex = isSgTypeComplex(ranges[3]->get_type());
    ROSE_ASSERT(complex != nullptr && ranges[4]->get_type() == complex);
    auto *component = complex->get_base_type();
    ROSE_ASSERT(component != nullptr && component->get_fortran_source_syntax());
    auto *selector = component->get_type_kind();
    ROSE_ASSERT(selector != nullptr && selector->get_parent() == component);
    ROSE_ASSERT(selector->get_fortran_integer_constant_value_is_available());
    ROSE_ASSERT(selector->get_fortran_integer_constant_value() == 4);
    ROSE_ASSERT(ranges[5]->get_type_spec() == SgFortranImplicitRange::e_type);
    ROSE_ASSERT(isSgClassType(ranges[5]->get_type()) != nullptr);
    ROSE_ASSERT(ranges[5]->get_source_derived_type_symbol() != nullptr);
    ROSE_ASSERT(ranges[5]->get_source_derived_type_symbol()->get_name() ==
                "local_box");
  }
  ROSE_ASSERT(specifications == 1);
}

} // namespace

int main(int argc, char **argv) {
  const std::string jsonDir = takeOption(argc, argv, "--rex-ast-json-dir=");
  const std::string contractMode =
      takeOption(argc, argv, "--rex-contract-mode=");
  ROSE_ASSERT(!jsonDir.empty());
  std::filesystem::remove_all(jsonDir);
  std::filesystem::create_directories(jsonDir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);

  SgSourceFile *original = firstSourceFile(project);
  if (contractMode == "unsigned-roundtrip") {
    verifyUnsignedSourceForms(original);
    Rose::AstJson::Options options;
    options.outputDirectory = jsonDir;
    SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
        original, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    ROSE_ASSERT(restored != nullptr && restored != original);
    verifyUnsignedSourceForms(restored);
    const std::string outputPath = restored->get_unparse_output_filename();
    const int status = backend(project);
    if (status == 0) {
      verifyUnsignedOutput(outputPath);
    }
    return status;
  }
  verifyProcedureSourceForms(original);
  verifyFunctionSourceVisibleBindings(original);
  verifyObjectSourceForms(original);
  verifyAllocationAndNullifyStatements(original);
  verifyImplicitRanges(original);
  verifyDataInitialization(original);
  const auto originalInterfaces = verifyInterfaceRoles(original);
  verifyUnaryOperatorBindings(original);
  SgProject *copy = SageInterface::deepCopy(project);
  ROSE_ASSERT(copy != nullptr && copy != project);
  const auto copiedInterfaces = verifyInterfaceRoles(firstSourceFile(copy));
  verifyUnaryOperatorBindings(firstSourceFile(copy));
  for (const char *name :
       {"abstract_proc", "typed_proc", "renamed_proc", "pointer_interface"}) {
    auto *originalInterface =
        originalInterfaces.at(name)->get_fortran_procedure_interface();
    auto *copiedInterface =
        copiedInterfaces.at(name)->get_fortran_procedure_interface();
    ROSE_ASSERT(copiedInterface != originalInterface);
    if (auto *source = isSgTypeExpression(originalInterface)) {
      auto *copied = isSgTypeExpression(copiedInterface);
      ROSE_ASSERT(copied != nullptr);
      ROSE_ASSERT(copied->get_represented_type() !=
                  source->get_represented_type());
    }
  }
  verifyObjectSourceForms(firstSourceFile(copy));
  AstTests::runAllTests(copy);
  SageInterface::deepDelete(copy);

  if (contractMode == "unparser-procedure-kind-mismatch") {
    typedProcedureSelector(original)->set_fortran_integer_constant_value(8);
    return backend(project);
  }
  if (contractMode == "unparser-assumed-type-spec-mismatch") {
    sourceName(original, "assumed_any")
        ->set_fortran_type_spec(
            SgInitializedName::e_fortran_type_spec_class_star);
    return backend(project);
  }
  if (contractMode == "unparser-header-prefix-type-spec-mismatch") {
    headerProcedure(original, "k_twice", true)
        ->set_fortran_result_type_spec(
            SgProcedureHeaderStatement::e_fortran_result_type_spec_type);
    return backend(project);
  }
  if (contractMode == "unparser-derived-type-spec-missing") {
    typedProcedure(original, "make_selector_box")
        ->set_fortran_result_type_spec(
            SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown);
    return backend(project);
  }
  if (contractMode == "json-procedure-kind-mismatch") {
    auto *selector = typedProcedureSelector(original);
    if (auto *literal = isSgIntVal(selector)) {
      literal->set_value(8);
    } else {
      auto *wide_literal = isSgLongLongIntVal(selector);
      ROSE_ASSERT(wide_literal != nullptr);
      wide_literal->set_value(8);
    }
    selector->set_fortran_integer_constant_value(8);
  } else if (contractMode == "json-selector-availability-mismatch") {
    typedProcedureSelector(original)
        ->set_fortran_integer_constant_value_is_available(false);
  } else if (contractMode == "json-semantic-star-kind") {
    typeSurface(sourceName(original, "explicit_real")->get_type(), false)
        .base->set_hasTypeKindStar(true);
  } else if (contractMode == "json-parameter-source-type") {
    SgType *sourceType =
        sourceName(original, "assumed_text")->get_fortran_source_type();
    ROSE_ASSERT(sourceType != nullptr);
    bool mutated = false;
    for (SgNode *node :
         NodeQuery::querySubTree(original, V_SgProcedureHeaderStatement)) {
      SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(node);
      if (procedure == nullptr || procedure->get_name() != "selector_dummies" ||
          procedure->get_definition() == nullptr) {
        continue;
      }
      SgFunctionParameterList *parameters = procedure->get_parameterList();
      ROSE_ASSERT(parameters != nullptr);
      for (SgInitializedName *parameter : parameters->get_args()) {
        if (parameter != nullptr && parameter->get_name() == "assumed_text") {
          ROSE_ASSERT(!mutated);
          parameter->set_fortran_source_type(sourceType);
          mutated = true;
        }
      }
    }
    ROSE_ASSERT(mutated);
  } else if (contractMode == "json-dynamic-result-source") {
    bool mutated = false;
    for (SgNode *node :
         NodeQuery::querySubTree(original, V_SgFunctionCallExp)) {
      SgFunctionCallExp *call = isSgFunctionCallExp(node);
      SgTypeString *result =
          isSgTypeString(typeSurface(call->get_type(), false).base);
      if (result != nullptr && result->get_fortran_dynamic_result_length()) {
        result->set_fortran_source_syntax(true);
        mutated = true;
        break;
      }
    }
    ROSE_ASSERT(mutated);
  } else if (!contractMode.empty()) {
    return 2;
  }

  Rose::AstJson::Options options;
  options.outputDirectory = jsonDir;
  SgSourceFile *restored = nullptr;
  try {
    restored = Rose::AstJson::roundTripSourceFile(
        original, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  } catch (const std::exception &error) {
    if (contractMode.rfind("json-", 0) != 0) {
      throw;
    }
    std::cerr << "REX_AST_JSON_REJECTION[" << contractMode
              << "]: " << error.what() << '\n';
    ROSE_ABORT();
  }
  ROSE_ASSERT(restored != nullptr && restored != original);
  verifyProcedureSourceForms(restored);
  verifyFunctionSourceVisibleBindings(restored);
  verifyObjectSourceForms(restored);
  verifyAllocationAndNullifyStatements(restored);
  verifyImplicitRanges(restored);
  verifyDataInitialization(restored);
  verifyInterfaceRoles(restored);
  verifyUnaryOperatorBindings(restored);

  const std::string outputPath = restored->get_unparse_output_filename();
  const int status = backend(project);
  if (status == 0) {
    verifyOutput(outputPath);
  }
  return status;
}
