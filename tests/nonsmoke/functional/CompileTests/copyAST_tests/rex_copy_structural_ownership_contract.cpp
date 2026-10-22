#include "RoseAst.h"
#include "rose.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace {

SgTemplateClassDeclaration *findTemplateOwner(SgNode *root) {
  for (SgNode *node : RoseAst(root)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration != nullptr &&
        declaration->get_name() == "rex_copy_template_owner" &&
        declaration->get_templateParameters().size() == 2) {
      return declaration;
    }
  }
  return nullptr;
}

SgTemplateClassDeclaration *findTemplateDefaultTypeOwner(SgNode *root) {
  for (SgNode *node : RoseAst(root)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration != nullptr &&
        declaration->get_name() == "rex_copy_template_default_type_owner" &&
        declaration->get_templateParameters().size() == 3) {
      return declaration;
    }
  }
  return nullptr;
}

SgTryStmt *findTryStatement(SgNode *root) {
  for (SgNode *node : RoseAst(root)) {
    if (SgTryStmt *statement = isSgTryStmt(node)) {
      return statement;
    }
  }
  return nullptr;
}

SgFunctionDefinition *findFunctionDefinition(SgNode *root, const char *name) {
  for (SgNode *node : RoseAst(root)) {
    SgFunctionDefinition *definition = isSgFunctionDefinition(node);
    SgFunctionDeclaration *declaration =
        definition != nullptr ? definition->get_declaration() : nullptr;
    if (declaration != nullptr && declaration->get_name() == name) {
      return definition;
    }
  }
  return nullptr;
}

SgCatchOptionStmt *requireSingleCatch(SgTryStmt *statement) {
  ROSE_ASSERT(statement != nullptr);
  SgCatchStatementSeq *sequence = statement->get_catch_statement_seq_root();
  ROSE_ASSERT(sequence != nullptr);
  ROSE_ASSERT(sequence->get_parent() == statement);
  ROSE_ASSERT(sequence->get_catch_statement_seq().size() == 1);
  SgCatchOptionStmt *handler =
      isSgCatchOptionStmt(sequence->get_catch_statement_seq().front());
  ROSE_ASSERT(handler != nullptr);
  ROSE_ASSERT(handler->get_parent() == sequence);
  return handler;
}

SgTemplateParameter *
requireTemplateTemplateParameter(SgTemplateClassDeclaration *owner) {
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(owner->get_templateParameters().size() == 2);
  SgTemplateParameter *parameter = owner->get_templateParameters().front();
  ROSE_ASSERT(parameter != nullptr);
  ROSE_ASSERT(parameter->get_parameterType() ==
              SgTemplateParameter::template_parameter);
  return parameter;
}

SgTemplateDeclaration *
requirePublishedTemplateIdentity(SgTemplateClassDeclaration *owner,
                                 SgTemplateParameter *parameter) {
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(parameter != nullptr);
  SgTemplateDeclaration *identity =
      isSgTemplateDeclaration(parameter->get_templateDeclaration());
  SgDeclarationScope *scope = owner->get_nonreal_decl_scope();
  ROSE_ASSERT(identity != nullptr);
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(scope->get_parent() == owner);
  ROSE_ASSERT(parameter->get_parent() == owner);
  ROSE_ASSERT(identity->get_parent() == scope);
  ROSE_ASSERT(identity->get_scope() == scope);
  ROSE_ASSERT(scope->statementExistsInScope(identity));
  ROSE_ASSERT(identity->get_templateParameters().size() == 1);
  ROSE_ASSERT(identity->get_templateParameters().front()->get_parent() ==
              identity);
  return identity;
}

class CorruptTemplateSemanticCopy final : public SgTreeCopy {
public:
  explicit CorruptTemplateSemanticCopy(
      const SgTemplateParameter *originalParameter)
      : originalParameter_(originalParameter) {
    ROSE_ASSERT(originalParameter_ != nullptr);
  }

  void prepareRootCopyForFixup() override {
    SgTreeCopy::prepareRootCopyForFixup();
    SgCopyHelp::copiedNodeMapTypeIterator found = get_copiedNodeMap().find(
        const_cast<SgTemplateParameter *>(originalParameter_));
    SgTemplateParameter *copiedParameter =
        found != get_copiedNodeMap().end()
            ? isSgTemplateParameter(found->second)
            : nullptr;
    if (copiedParameter == nullptr) {
      std::fprintf(stderr,
                   "REX_TEST_ERROR: template parameter was not copied\n");
      ROSE_ABORT();
    }
    copiedParameter->set_templateDeclaration(nullptr);
  }

private:
  const SgTemplateParameter *originalParameter_;
};

class CorruptOwnedDeclarationScopeCopy final : public SgTreeCopy {
public:
  explicit CorruptOwnedDeclarationScopeCopy(
      const SgDeclarationStatement *originalOwner)
      : originalOwner_(originalOwner) {
    ROSE_ASSERT(originalOwner_ != nullptr);
  }

  void prepareRootCopyForFixup() override {
    SgTreeCopy::prepareRootCopyForFixup();
    SgCopyHelp::copiedNodeMapTypeIterator found = get_copiedNodeMap().find(
        const_cast<SgDeclarationStatement *>(originalOwner_));
    SgDeclarationStatement *copiedOwner =
        found != get_copiedNodeMap().end()
            ? isSgDeclarationStatement(found->second)
            : nullptr;
    if (copiedOwner == nullptr) {
      std::fprintf(stderr,
                   "REX_TEST_ERROR: declaration owner was not copied\n");
      ROSE_ABORT();
    }
    copiedOwner->set_nonreal_decl_scope(nullptr);
  }

private:
  const SgDeclarationStatement *originalOwner_;
};

class CorruptCatchSemanticCopy final : public SgTreeCopy {
public:
  explicit CorruptCatchSemanticCopy(const SgCatchOptionStmt *originalHandler)
      : originalHandler_(originalHandler) {
    ROSE_ASSERT(originalHandler_ != nullptr);
  }

  void prepareRootCopyForFixup() override {
    SgTreeCopy::prepareRootCopyForFixup();
    SgCopyHelp::copiedNodeMapTypeIterator found = get_copiedNodeMap().find(
        const_cast<SgCatchOptionStmt *>(originalHandler_));
    SgCatchOptionStmt *copiedHandler = found != get_copiedNodeMap().end()
                                           ? isSgCatchOptionStmt(found->second)
                                           : nullptr;
    if (copiedHandler == nullptr) {
      std::fprintf(stderr, "REX_TEST_ERROR: catch handler was not copied\n");
      ROSE_ABORT();
    }
    copiedHandler->set_trystmt(nullptr);
  }

private:
  const SgCatchOptionStmt *originalHandler_;
};

void validateTemplateCopy(SgTemplateClassDeclaration *originalOwner) {
  SgTemplateParameter *originalParameter =
      requireTemplateTemplateParameter(originalOwner);
  SgTemplateDeclaration *originalIdentity =
      requirePublishedTemplateIdentity(originalOwner, originalParameter);

  SgTreeCopy copyHelp;
  SgTemplateClassDeclaration *copiedOwner =
      isSgTemplateClassDeclaration(originalOwner->copy(copyHelp));
  ROSE_ASSERT(copiedOwner != nullptr);
  ROSE_ASSERT(copiedOwner != originalOwner);
  SgTemplateParameter *copiedParameter =
      requireTemplateTemplateParameter(copiedOwner);
  SgTemplateDeclaration *copiedIdentity =
      requirePublishedTemplateIdentity(copiedOwner, copiedParameter);
  ROSE_ASSERT(copiedParameter != originalParameter);
  ROSE_ASSERT(copiedIdentity != originalIdentity);
  ROSE_ASSERT(copiedParameter->get_templateDeclaration() == copiedIdentity);
  ROSE_ASSERT(copiedOwner->get_nonreal_decl_scope() !=
              originalOwner->get_nonreal_decl_scope());

  // A parameter copied outside its structural declaration transaction retains
  // the exact external template identity and never acquires ownership of it.
  SgTreeCopy externalCopyHelp;
  SgTemplateParameter *externalCopy =
      isSgTemplateParameter(originalParameter->copy(externalCopyHelp));
  ROSE_ASSERT(externalCopy != nullptr);
  ROSE_ASSERT(externalCopy != originalParameter);
  ROSE_ASSERT(externalCopy->get_templateDeclaration() == originalIdentity);
  ROSE_ASSERT(originalIdentity->get_parent() ==
              originalOwner->get_nonreal_decl_scope());
}

void validateTemplateDefaultTypeCopy(SgTemplateClassDeclaration *owner) {
  ROSE_ASSERT(owner != nullptr);
  ROSE_ASSERT(owner->get_templateParameters().size() == 3);
  SgTemplateParameter *originalTypeParameter =
      owner->get_templateParameters()[0];
  SgTemplateParameter *originalDefaultedParameter =
      owner->get_templateParameters()[1];
  SgTemplateParameter *originalWrappedDefaultedParameter =
      owner->get_templateParameters()[2];
  ROSE_ASSERT(originalTypeParameter != nullptr);
  ROSE_ASSERT(originalDefaultedParameter != nullptr);
  ROSE_ASSERT(originalWrappedDefaultedParameter != nullptr);
  ROSE_ASSERT(originalTypeParameter->get_parameterType() ==
              SgTemplateParameter::type_parameter);
  ROSE_ASSERT(originalDefaultedParameter->get_parameterType() ==
              SgTemplateParameter::type_parameter);
  ROSE_ASSERT(originalWrappedDefaultedParameter->get_parameterType() ==
              SgTemplateParameter::type_parameter);
  SgType *originalType = originalTypeParameter->get_type();
  SgType *originalDefault =
      originalDefaultedParameter->get_defaultTypeParameter();
  SgPointerType *originalWrappedDefault = isSgPointerType(
      originalWrappedDefaultedParameter->get_defaultTypeParameter());
  ROSE_ASSERT(originalType != nullptr);
  SgTemplateType *originalParameterType = isSgTemplateType(originalType);
  SgTemplateType *originalDefaultUse = isSgTemplateType(originalDefault);
  SgTemplateType *originalWrappedDefaultUse =
      originalWrappedDefault != nullptr
          ? isSgTemplateType(originalWrappedDefault->get_base_type())
          : nullptr;
  ROSE_ASSERT(originalParameterType != nullptr);
  ROSE_ASSERT(originalDefaultUse != nullptr);
  ROSE_ASSERT(originalWrappedDefault != nullptr);
  ROSE_ASSERT(originalWrappedDefaultUse != nullptr);
  ROSE_ASSERT(originalDefaultUse != originalParameterType);
  ROSE_ASSERT(originalWrappedDefaultUse != originalParameterType);
  ROSE_ASSERT(originalWrappedDefaultUse != originalDefaultUse);
  ROSE_ASSERT(originalParameterType->get_template_parameter() ==
              originalTypeParameter);
  if (originalDefaultUse->get_template_parameter() != originalTypeParameter) {
    std::fprintf(
        stderr,
        "REX_TEST_ERROR[template-default-type-owner]: owner=%p first=%p "
        "defining=%p type-parameter=%p parent=%p default-parameter=%p "
        "parent=%p default-use=%p semantic-parameter=%p parent=%p\n",
        static_cast<void *>(owner),
        static_cast<void *>(owner->get_firstNondefiningDeclaration()),
        static_cast<void *>(owner->get_definingDeclaration()),
        static_cast<void *>(originalTypeParameter),
        static_cast<void *>(originalTypeParameter->get_parent()),
        static_cast<void *>(originalDefaultedParameter),
        static_cast<void *>(originalDefaultedParameter->get_parent()),
        static_cast<void *>(originalDefaultUse),
        static_cast<void *>(originalDefaultUse->get_template_parameter()),
        static_cast<void *>(
            originalDefaultUse->get_template_parameter() != nullptr
                ? originalDefaultUse->get_template_parameter()->get_parent()
                : nullptr));
    ROSE_ABORT();
  }
  ROSE_ASSERT(originalDefaultUse->get_name() ==
              originalParameterType->get_name());
  ROSE_ASSERT(originalDefaultUse->get_template_parameter_position() ==
              originalParameterType->get_template_parameter_position());
  ROSE_ASSERT(originalDefaultUse->get_template_parameter_depth() ==
              originalParameterType->get_template_parameter_depth());
  ROSE_ASSERT(originalDefaultUse->get_canonical_source_identity() ==
              originalParameterType->get_canonical_source_identity());
  ROSE_ASSERT(originalWrappedDefaultUse->get_template_parameter() ==
              originalTypeParameter);
  ROSE_ASSERT(originalWrappedDefaultUse->get_name() ==
              originalParameterType->get_name());
  ROSE_ASSERT(originalWrappedDefaultUse->get_template_parameter_position() ==
              originalParameterType->get_template_parameter_position());
  ROSE_ASSERT(originalWrappedDefaultUse->get_template_parameter_depth() ==
              originalParameterType->get_template_parameter_depth());
  ROSE_ASSERT(originalWrappedDefaultUse->get_canonical_source_identity() ==
              originalParameterType->get_canonical_source_identity());

  SgTreeCopy copyHelp;
  SgTemplateClassDeclaration *copy =
      isSgTemplateClassDeclaration(owner->copy(copyHelp));
  ROSE_ASSERT(copy != nullptr);
  ROSE_ASSERT(copy != owner);
  ROSE_ASSERT(copy->get_templateParameters().size() == 3);
  SgTemplateParameter *copiedTypeParameter = copy->get_templateParameters()[0];
  SgTemplateParameter *copiedDefaultedParameter =
      copy->get_templateParameters()[1];
  SgTemplateParameter *copiedWrappedDefaultedParameter =
      copy->get_templateParameters()[2];
  ROSE_ASSERT(copiedTypeParameter != nullptr);
  ROSE_ASSERT(copiedDefaultedParameter != nullptr);
  ROSE_ASSERT(copiedWrappedDefaultedParameter != nullptr);
  ROSE_ASSERT(copiedTypeParameter != originalTypeParameter);
  ROSE_ASSERT(copiedDefaultedParameter != originalDefaultedParameter);
  ROSE_ASSERT(copiedWrappedDefaultedParameter !=
              originalWrappedDefaultedParameter);
  SgType *copiedType = copiedTypeParameter->get_type();
  SgType *copiedDefault = copiedDefaultedParameter->get_defaultTypeParameter();
  SgPointerType *copiedWrappedDefault = isSgPointerType(
      copiedWrappedDefaultedParameter->get_defaultTypeParameter());
  ROSE_ASSERT(copiedType != nullptr);
  SgTemplateType *copiedParameterType = isSgTemplateType(copiedType);
  SgTemplateType *copiedDefaultUse = isSgTemplateType(copiedDefault);
  SgTemplateType *copiedWrappedDefaultUse =
      copiedWrappedDefault != nullptr
          ? isSgTemplateType(copiedWrappedDefault->get_base_type())
          : nullptr;
  ROSE_ASSERT(copiedParameterType != nullptr);
  ROSE_ASSERT(copiedDefaultUse != nullptr);
  ROSE_ASSERT(copiedWrappedDefault != nullptr);
  ROSE_ASSERT(copiedWrappedDefaultUse != nullptr);
  ROSE_ASSERT(copiedType != originalType);
  ROSE_ASSERT(copiedDefault != copiedType);
  ROSE_ASSERT(copiedDefault != originalDefault);
  ROSE_ASSERT(copiedWrappedDefault != originalWrappedDefault);
  ROSE_ASSERT(copiedWrappedDefaultUse != copiedParameterType);
  ROSE_ASSERT(copiedWrappedDefaultUse != copiedDefaultUse);
  ROSE_ASSERT(copiedWrappedDefaultUse != originalWrappedDefaultUse);
  ROSE_ASSERT(copiedParameterType->get_template_parameter() ==
              copiedTypeParameter);
  ROSE_ASSERT(copiedDefaultUse->get_template_parameter() ==
              copiedTypeParameter);
  ROSE_ASSERT(copiedDefaultUse->get_name() == copiedParameterType->get_name());
  ROSE_ASSERT(copiedDefaultUse->get_template_parameter_position() ==
              copiedParameterType->get_template_parameter_position());
  ROSE_ASSERT(copiedDefaultUse->get_template_parameter_depth() ==
              copiedParameterType->get_template_parameter_depth());
  ROSE_ASSERT(copiedDefaultUse->get_canonical_source_identity() ==
              copiedParameterType->get_canonical_source_identity());
  ROSE_ASSERT(copiedWrappedDefaultUse->get_template_parameter() ==
              copiedTypeParameter);
  ROSE_ASSERT(copiedWrappedDefaultUse->get_name() ==
              copiedParameterType->get_name());
  ROSE_ASSERT(copiedWrappedDefaultUse->get_template_parameter_position() ==
              copiedParameterType->get_template_parameter_position());
  ROSE_ASSERT(copiedWrappedDefaultUse->get_template_parameter_depth() ==
              copiedParameterType->get_template_parameter_depth());
  ROSE_ASSERT(copiedWrappedDefaultUse->get_canonical_source_identity() ==
              copiedParameterType->get_canonical_source_identity());
}

void validateCatchPair(SgTryStmt *originalTry, SgTryStmt *copiedTry) {
  SgCatchOptionStmt *originalHandler = requireSingleCatch(originalTry);
  ROSE_ASSERT(copiedTry != nullptr);
  ROSE_ASSERT(copiedTry != originalTry);
  SgCatchOptionStmt *copiedHandler = requireSingleCatch(copiedTry);
  ROSE_ASSERT(copiedHandler != originalHandler);
  ROSE_ASSERT(copiedHandler->get_trystmt() == copiedTry);
  ROSE_ASSERT(copiedHandler->get_condition() !=
              originalHandler->get_condition());
  ROSE_ASSERT(copiedHandler->get_body() != originalHandler->get_body());

  SgInitializedName *copiedException =
      copiedHandler->get_condition()->get_variables().front();
  ROSE_ASSERT(copiedException != nullptr);
  bool foundExactReference = false;
  for (SgNode *node : RoseAst(copiedHandler->get_body())) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    if (reference != nullptr && reference->get_symbol() != nullptr &&
        reference->get_symbol()->get_declaration() == copiedException) {
      foundExactReference = true;
    }
  }
  ROSE_ASSERT(foundExactReference);
  SgVariableSymbol *copiedSymbol =
      isSgVariableSymbol(copiedException->get_symbol_from_symbol_table());
  ROSE_ASSERT(copiedSymbol != nullptr);
  ROSE_ASSERT(copiedSymbol->get_declaration() == copiedException);
  ROSE_ASSERT(copiedSymbol->get_parent() == copiedHandler->get_symbol_table());
  ROSE_ASSERT(copiedHandler->get_symbol_table()->get_parent() == copiedHandler);
  ROSE_ASSERT(copiedHandler->get_symbol_table()->get_table()->size() == 1);
  ROSE_ASSERT(originalHandler->get_trystmt() == originalTry);
}

void validateCatchCopy(SgTryStmt *originalTry) {
  SgTreeCopy copyHelp;
  SgTryStmt *copiedTry = isSgTryStmt(originalTry->copy(copyHelp));
  validateCatchPair(originalTry, copiedTry);
}

void validateDetachedFunctionParameterListCopy(SgProject *project) {
  SgFunctionDefinition *definition =
      findFunctionDefinition(project, "rex_copy_parameter_owner");
  ROSE_ASSERT(definition != nullptr);
  SgFunctionDeclaration *function = definition->get_declaration();
  ROSE_ASSERT(function != nullptr);
  SgFunctionParameterList *original = function->get_parameterList();
  ROSE_ASSERT(original != nullptr);
  ROSE_ASSERT(original->get_parent() == function);
  ROSE_ASSERT(original->get_definingDeclaration() == nullptr);
  ROSE_ASSERT(original->get_firstNondefiningDeclaration() == original);
  ROSE_ASSERT(original->get_args().size() == 2);

  auto validateCopy = [&](SgFunctionParameterList *copy) {
    ROSE_ASSERT(copy != nullptr);
    ROSE_ASSERT(copy != original);
    ROSE_ASSERT(copy->get_parent() == nullptr);
    ROSE_ASSERT(copy->get_definingDeclaration() == nullptr);
    ROSE_ASSERT(copy->get_firstNondefiningDeclaration() == copy);
    ROSE_ASSERT(copy->get_args().size() == original->get_args().size());
    for (size_t index = 0; index < original->get_args().size(); ++index) {
      SgInitializedName *originalParameter = original->get_args()[index];
      SgInitializedName *copiedParameter = copy->get_args()[index];
      ROSE_ASSERT(originalParameter != nullptr);
      ROSE_ASSERT(copiedParameter != nullptr);
      ROSE_ASSERT(copiedParameter != originalParameter);
      ROSE_ASSERT(originalParameter->get_parent() == original);
      ROSE_ASSERT(copiedParameter->get_parent() == copy);
      ROSE_ASSERT(copiedParameter->get_type() == originalParameter->get_type());
      ROSE_ASSERT(originalParameter->get_declptr() == function);
      ROSE_ASSERT(copiedParameter->get_declptr() == function);
      ROSE_ASSERT(originalParameter->get_scope() != nullptr);
      ROSE_ASSERT(copiedParameter->get_scope() ==
                  originalParameter->get_scope());
      ROSE_ASSERT(originalParameter->get_definition() == nullptr);
      ROSE_ASSERT(copiedParameter->get_definition() == nullptr);
    }
  };

  SgTreeCopy copyHelp;
  SgFunctionParameterList *directCopy =
      isSgFunctionParameterList(original->copy(copyHelp));
  validateCopy(directCopy);

  SgFunctionParameterList *interfaceCopy =
      isSgFunctionParameterList(SageInterface::deepCopyNode(original));
  validateCopy(interfaceCopy);
}

void validateFunctionDeclarationCycleCopy(SgProject *project) {
  SgFunctionDefinition *originalDefinition =
      findFunctionDefinition(project, "rex_copy_parameter_owner");
  ROSE_ASSERT(originalDefinition != nullptr);
  SgFunctionDeclaration *originalFunction =
      originalDefinition->get_declaration();
  ROSE_ASSERT(originalFunction != nullptr);
  SgFunctionParameterList *originalParameters =
      originalFunction->get_parameterList();
  ROSE_ASSERT(originalParameters != nullptr);
  ROSE_ASSERT(originalParameters->get_parent() == originalFunction);
  ROSE_ASSERT(originalParameters->get_args().size() == 2);

  struct SignatureCopy : SgTreeCopy {
    SgFunctionDefinition *source;
    bool checked = false;
    explicit SignatureCopy(SgFunctionDefinition *definition)
        : source(definition) {}
    SgNode *copyAst(const SgNode *node) override {
      if (node == source->get_body()) {
        auto *definition =
            isSgFunctionDefinition(get_copiedNodeMap().at(source));
        auto *declaration = isSgFunctionDeclaration(
            get_copiedNodeMap().at(source->get_declaration()));
        ROSE_ASSERT(definition->get_declaration() == declaration &&
                    declaration->get_definition() == definition);
        for (auto *parameter : source->get_declaration()->get_args()) {
          auto *copy = isSgInitializedName(get_copiedNodeMap().at(parameter));
          ROSE_ASSERT(copy->get_declptr() == declaration &&
                      copy->get_scope() == definition);
        }
        checked = true;
      }
      return SgTreeCopy::copyAst(node);
    }
  } copyHelp(originalDefinition);
  SgFunctionDeclaration *copiedFunction =
      isSgFunctionDeclaration(originalFunction->copy(copyHelp));
  ROSE_ASSERT(copyHelp.checked);
  ROSE_ASSERT(copiedFunction != nullptr);
  ROSE_ASSERT(copiedFunction != originalFunction);
  SgFunctionDefinition *copiedDefinition = copiedFunction->get_definition();
  SgFunctionParameterList *copiedParameters =
      copiedFunction->get_parameterList();
  ROSE_ASSERT(copiedDefinition != nullptr);
  ROSE_ASSERT(copiedDefinition != originalDefinition);
  ROSE_ASSERT(copiedDefinition->get_parent() == copiedFunction);
  ROSE_ASSERT(copiedDefinition->get_declaration() == copiedFunction);
  ROSE_ASSERT(copiedParameters != nullptr);
  ROSE_ASSERT(copiedParameters != originalParameters);
  ROSE_ASSERT(copiedParameters->get_parent() == copiedFunction);
  ROSE_ASSERT(copiedParameters->get_args().size() ==
              originalParameters->get_args().size());

  std::map<SgName, SgInitializedName *> copiedParameterByName;
  for (size_t index = 0; index < copiedParameters->get_args().size(); ++index) {
    SgInitializedName *originalParameter =
        originalParameters->get_args()[index];
    SgInitializedName *copiedParameter = copiedParameters->get_args()[index];
    ROSE_ASSERT(originalParameter != nullptr);
    ROSE_ASSERT(copiedParameter != nullptr);
    ROSE_ASSERT(copiedParameter != originalParameter);
    ROSE_ASSERT(copiedParameter->get_parent() == copiedParameters);
    ROSE_ASSERT(copiedParameter->get_scope() == copiedDefinition);
    ROSE_ASSERT(copiedParameterByName
                    .emplace(copiedParameter->get_name(), copiedParameter)
                    .second);
  }

  std::set<SgName> referencedParameters;
  for (SgNode *node : RoseAst(copiedDefinition->get_body())) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    SgVariableSymbol *symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    SgInitializedName *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    if (declaration == nullptr) {
      continue;
    }
    const auto expected = copiedParameterByName.find(declaration->get_name());
    if (expected == copiedParameterByName.end()) {
      continue;
    }
    ROSE_ASSERT(declaration == expected->second);
    ROSE_ASSERT(symbol->get_parent() == copiedDefinition->get_symbol_table());
    referencedParameters.insert(declaration->get_name());
  }
  ROSE_ASSERT(referencedParameters.size() == copiedParameterByName.size());
}

void validateCopiedSignatureDependencies(SgProject *project) {
  auto *original =
      findFunctionDefinition(project, "rex_copy_lambda_signature_owner");
  ROSE_ASSERT(original != nullptr);
  struct Call {
    SgFunctionCallExp *expression;
    SgFunctionDeclaration *declaration;
    SgFunctionType *type;
    SgSymbol *symbol;
  };
  std::vector<Call> calls;
  for (auto *node : RoseAst(original->get_body())) {
    if (auto *call = isSgFunctionCallExp(node)) {
      auto *declaration = call->getAssociatedFunctionDeclaration();
      ROSE_ASSERT(declaration != nullptr);
      calls.push_back({call, declaration, declaration->get_type(),
                       declaration->get_symbol_from_symbol_table()});
    }
  }
  ROSE_ASSERT(calls.size() == 3);
  SgTreeCopy help;
  ROSE_ASSERT(isSgFunctionDefinition(original->copy(help)) != nullptr);
  for (const auto &call : calls) {
    auto *copiedCall =
        isSgFunctionCallExp(help.get_copiedNodeMap().at(call.expression));
    ROSE_ASSERT(copiedCall != nullptr);
    if (call.type->get_argument_list()->get_arguments().empty()) {
      ROSE_ASSERT(copiedCall->getAssociatedFunctionDeclaration() ==
                  call.declaration);
      ROSE_ASSERT(call.declaration->get_symbol_from_symbol_table() ==
                  call.symbol);
      continue;
    }
    auto *copy =
        isSgFunctionDeclaration(help.get_copiedNodeMap().at(call.declaration));
    ROSE_ASSERT(copy != nullptr && copy != call.declaration);
    ROSE_ASSERT(copiedCall->getAssociatedFunctionDeclaration() == copy);
    ROSE_ASSERT(call.declaration->get_type() == call.type);
    ROSE_ASSERT(call.declaration->get_symbol_from_symbol_table() ==
                call.symbol);
    ROSE_ASSERT(copy->get_scope() == call.declaration->get_scope());
    ROSE_ASSERT(copy->get_type() != call.type);
    auto *first =
        isSgFunctionDeclaration(copy->get_firstNondefiningDeclaration());
    auto *defining = isSgFunctionDeclaration(copy->get_definingDeclaration());
    ROSE_ASSERT(first != nullptr && defining != nullptr);
    ROSE_ASSERT(first->get_firstNondefiningDeclaration() == first);
    ROSE_ASSERT(first->get_definingDeclaration() == defining);
    ROSE_ASSERT(defining->get_firstNondefiningDeclaration() == first);
    ROSE_ASSERT(defining->get_definingDeclaration() == defining);
    ROSE_ASSERT(defining->get_definition()->get_declaration() == defining);
    auto *symbol = first->get_symbol_from_symbol_table();
    ROSE_ASSERT(symbol != nullptr && symbol != call.symbol);
    ROSE_ASSERT(symbol->get_symbol_basis() == first);
    ROSE_ASSERT(symbol->get_parent() == copy->get_scope()->get_symbol_table());
    ROSE_ASSERT(first->get_parent() ==
                copy->get_scope()->get_auxiliary_declarations());
    ROSE_ASSERT(defining->get_parent() == first->get_parent());
    auto *oldArgument = call.type->get_argument_list()->get_arguments().front();
    auto *newArgument =
        copy->get_type()->get_argument_list()->get_arguments().front();
    ROSE_ASSERT(oldArgument != newArgument);
    ROSE_ASSERT(help.get_copiedNodeMap().at(oldArgument) == newArgument);
    auto *oldClass = isSgClassType(oldArgument);
    auto *newClass = isSgClassType(newArgument);
    ROSE_ASSERT(oldClass != nullptr && newClass != nullptr);
    ROSE_ASSERT(help.get_copiedNodeMap().at(oldClass->get_declaration()) ==
                newClass->get_declaration());
  }
}

void validateSeparateFileCatchCopy(SgProject *project, SgTryStmt *originalTry) {
  SgFunctionDefinition *targetDefinition =
      findFunctionDefinition(project, "rex_copy_separate_file_catch_target");
  ROSE_ASSERT(targetDefinition != nullptr);
  SgBasicBlock *targetBody = targetDefinition->get_body();
  ROSE_ASSERT(targetBody != nullptr);

  SgTreeCopy copyHelp;
  SgTryStmt *copiedTry = isSgTryStmt(originalTry->copy(copyHelp));
  ROSE_ASSERT(copiedTry != nullptr);
  SageInterface::relocateGeneratedSubtreePhysicalOutputOwner(
      copiedTry, originalTry, targetBody);
  SageInterface::appendStatement(copiedTry, targetBody);
  ROSE_ASSERT(SageInterface::getEnclosingFileNode(copiedTry) !=
              SageInterface::getEnclosingFileNode(originalTry));

  validateCatchPair(originalTry, copiedTry);

  SgCatchOptionStmt *copiedHandler = requireSingleCatch(copiedTry);
  SgInitializedName *copiedException =
      copiedHandler->get_condition()->get_variables().front();
  SgVariableSymbol *copiedSymbol =
      isSgVariableSymbol(copiedException->get_symbol_from_symbol_table());
  ROSE_ASSERT(copiedException->get_scope() == copiedHandler);
  ROSE_ASSERT(copiedSymbol != nullptr);
  ROSE_ASSERT(copiedSymbol->get_parent() == copiedHandler->get_symbol_table());
  ROSE_ASSERT(!targetBody->get_symbol_table()->exists(copiedSymbol));
}

} // namespace

int main(int argc, char **argv) {
  const bool validateSeparateFileCatch =
      argc == 4 && std::strcmp(argv[1], "--validate-separate-file-catch") == 0;
  const bool validateDetachedParameterList =
      argc == 3 &&
      std::strcmp(argv[1], "--validate-detached-parameter-list") == 0;
  const bool validateFunctionOwnerCycle =
      argc == 3 && std::strcmp(argv[1], "--validate-function-owner-cycle") == 0;
  const bool validateTemplateDefaultType =
      argc == 3 &&
      std::strcmp(argv[1], "--validate-template-default-type") == 0;
  const bool rejectTemplateSemantic =
      argc == 3 && std::strcmp(argv[1], "--reject-template-semantic-edge") == 0;
  const bool rejectOwnedScope =
      argc == 3 && std::strcmp(argv[1], "--reject-missing-owned-scope") == 0;
  const bool rejectCatchSemantic =
      argc == 3 && std::strcmp(argv[1], "--reject-catch-semantic-edge") == 0;
  const bool rejectShallowBody =
      argc == 3 && std::strcmp(argv[1], "--reject-shallow-body") == 0;
  const bool rejectShallowInitializer =
      argc == 3 && std::strcmp(argv[1], "--reject-shallow-initializer") == 0;
  if ((!validateSeparateFileCatch &&
       argc != ((validateDetachedParameterList || validateFunctionOwnerCycle ||
                 validateTemplateDefaultType || rejectTemplateSemantic ||
                 rejectOwnedScope || rejectCatchSemantic || rejectShallowBody ||
                 rejectShallowInitializer)
                    ? 3
                    : 2)) ||
      (validateSeparateFileCatch && argc != 4)) {
    return 2;
  }

  std::vector<char *> frontendArguments{argv[0]};
  if (validateSeparateFileCatch) {
    frontendArguments.push_back(argv[2]);
    frontendArguments.push_back(argv[3]);
  } else {
    frontendArguments.push_back(argv[argc - 1]);
  }
  SgProject *project = frontend(static_cast<int>(frontendArguments.size()),
                                frontendArguments.data());
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  if (rejectShallowBody) {
    auto *definition =
        findFunctionDefinition(project, "rex_copy_parameter_owner");
    ROSE_ASSERT(definition != nullptr);
    auto *body = definition->get_body();
    ROSE_ASSERT(body != nullptr && body->get_parent() == definition);
    struct SharedBodyCopy : SgTreeCopy {
      SgBasicBlock *body;
      explicit SharedBodyCopy(SgBasicBlock *source) : body(source) {}
      SgNode *copyAst(const SgNode *node) override {
        return node == body ? body : SgTreeCopy::copyAst(node);
      }
    } copy(body);
    (void)definition->copy(copy);
    return 1;
  }
  if (rejectShallowInitializer) {
    auto *definition =
        findFunctionDefinition(project, "rex_copy_lambda_signature_owner");
    ROSE_ASSERT(definition != nullptr);
    auto *body = definition->get_body();
    ROSE_ASSERT(body != nullptr && body->get_parent() == definition);
    ROSE_ASSERT(!body->get_statements().empty());
    auto *declaration = isSgVariableDeclaration(body->get_statements().front());
    ROSE_ASSERT(declaration != nullptr && declaration->get_parent() == body);
    ROSE_ASSERT(declaration->get_variables().size() == 1);
    auto *name = declaration->get_variables().front();
    ROSE_ASSERT(name != nullptr && name->get_parent() == declaration);
    auto *initializer = name->get_initptr();
    ROSE_ASSERT(initializer != nullptr && initializer->get_parent() == name);
    SgShallowCopy copy;
    (void)name->copy(copy);
    return 1;
  }
  if (validateDetachedParameterList) {
    validateDetachedFunctionParameterListCopy(project);
    return 0;
  }
  if (validateFunctionOwnerCycle) {
    validateFunctionDeclarationCycleCopy(project);
    return 0;
  }
  if (validateTemplateDefaultType) {
    validateTemplateDefaultTypeCopy(findTemplateDefaultTypeOwner(project));
    return 0;
  }
  SgTemplateClassDeclaration *templateOwner = findTemplateOwner(project);
  SgTryStmt *tryStatement = findTryStatement(project);
  ROSE_ASSERT(templateOwner != nullptr);
  ROSE_ASSERT(tryStatement != nullptr);

  SgTemplateParameter *templateParameter =
      requireTemplateTemplateParameter(templateOwner);
  requirePublishedTemplateIdentity(templateOwner, templateParameter);
  SgCatchOptionStmt *catchHandler = requireSingleCatch(tryStatement);
  ROSE_ASSERT(catchHandler->get_trystmt() == tryStatement);

  if (rejectTemplateSemantic) {
    CorruptTemplateSemanticCopy copyHelp(templateParameter);
    (void)templateOwner->copy(copyHelp);
    return 1;
  }
  if (rejectOwnedScope) {
    CorruptOwnedDeclarationScopeCopy copyHelp(templateOwner);
    (void)templateOwner->copy(copyHelp);
    return 1;
  }
  if (rejectCatchSemantic) {
    CorruptCatchSemanticCopy copyHelp(catchHandler);
    (void)tryStatement->copy(copyHelp);
    return 1;
  }

  validateTemplateCopy(templateOwner);
  validateTemplateDefaultTypeCopy(findTemplateDefaultTypeOwner(project));
  validateCatchCopy(tryStatement);
  validateDetachedFunctionParameterListCopy(project);
  validateFunctionDeclarationCycleCopy(project);
  validateCopiedSignatureDependencies(project);
  if (validateSeparateFileCatch) {
    validateSeparateFileCatchCopy(project, tryStatement);
  }
  return 0;
}
