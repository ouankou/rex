// This is the location of all the name qualification support functions
// required for code generation (unparser) (only applicable to C++).

#include "sage3basic.h"

#include "unparser.h"

#include <algorithm>
#include <optional>

using namespace std;

namespace {

SgDeclarationStatement *
referencedDeclarationForTemplateArgument(const SgTemplateArgument *arg) {
  if (arg == NULL) {
    return NULL;
  }

  switch (arg->get_argumentType()) {
  case SgTemplateArgument::type_argument: {
    SgType *type = arg->get_type();
    return type != NULL ? type->getAssociatedDeclaration() : NULL;
  }

  case SgTemplateArgument::template_template_argument:
    return isSgDeclarationStatement(arg->get_templateDeclaration());

  default:
    return NULL;
  }
}

std::optional<bool>
exactSourceTypeElaboration(const SgNode *constReferenceNode) {
  SgNode *referenceNode = const_cast<SgNode *>(constReferenceNode);
  if (SgFunctionDeclaration *function =
          isSgFunctionDeclaration(referenceNode)) {
    const auto sourceKind = function->get_source_return_type_elaboration_kind();
    if (sourceKind ==
        SgFunctionDeclaration::e_source_return_type_elaboration_unspecified) {
      return std::nullopt;
    }
    const bool expected =
        sourceKind !=
        SgFunctionDeclaration::e_source_return_type_elaboration_none;
    const bool payload =
        function->get_type_elaboration_required_for_return_type();
    if (payload != expected) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[function-return-source-type]: "
              "function=%p/%s source-kind=%d contradicts its typed boolean "
              "payload=%d\n",
              static_cast<void *>(function), function->get_name().str(),
              static_cast<int>(sourceKind), payload ? 1 : 0);
      ROSE_ABORT();
    }
    return expected;
  }

  SgNonrealDecl::source_elaboration_kind_enum sourceKind =
      SgNonrealDecl::e_source_elaboration_unspecified;
  bool payload = false;
  bool ownsSourceKind = false;
  if (SgCastExp *cast = isSgCastExp(referenceNode)) {
    sourceKind = cast->get_source_type_elaboration_kind();
    payload = cast->get_type_elaboration_required();
    ownsSourceKind = true;
  } else if (SgConstructorInitializer *constructor =
                 isSgConstructorInitializer(referenceNode)) {
    sourceKind = constructor->get_source_type_elaboration_kind();
    payload = constructor->get_type_elaboration_required();
    ownsSourceKind = true;
  } else if (SgSizeOfOp *sizeofOperation = isSgSizeOfOp(referenceNode)) {
    sourceKind = sizeofOperation->get_source_type_elaboration_kind();
    payload = sizeofOperation->get_type_elaboration_required();
    ownsSourceKind = true;
  } else if (SgAlignOfOp *alignofOperation = isSgAlignOfOp(referenceNode)) {
    sourceKind = alignofOperation->get_source_type_elaboration_kind();
    payload = alignofOperation->get_type_elaboration_required();
    ownsSourceKind = true;
  }
  if (!ownsSourceKind ||
      sourceKind == SgNonrealDecl::e_source_elaboration_unspecified) {
    return std::nullopt;
  }

  const bool expected = sourceKind != SgNonrealDecl::e_source_elaboration_none;
  if (payload != expected) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[source-type-elaboration]: reference=%p/%s "
            "source-kind=%d contradicts its typed boolean payload=%d\n",
            static_cast<void *>(referenceNode),
            referenceNode->class_name().c_str(), static_cast<int>(sourceKind),
            payload ? 1 : 0);
    ROSE_ABORT();
  }
  return expected;
}

} // namespace

SgStatement *
exactQualificationUseSiteForEmission(const SgNode *node,
                                     SgStatement *emissionStatement) {
  ASSERT_not_null(node);

  auto normalizeSemanticDeclaratorState =
      [](SgStatement *statement) -> SgStatement * {
    SgVariableDefinition *definition = isSgVariableDefinition(statement);
    if (definition == nullptr) {
      return statement;
    }
    SgInitializedName *name = isSgInitializedName(definition->get_parent());
    SgVariableDeclaration *declaration =
        name != nullptr ? isSgVariableDeclaration(name->get_parent()) : nullptr;
    if (name == nullptr || name->get_definition() != definition ||
        declaration == nullptr ||
        std::count(declaration->get_variables().begin(),
                   declaration->get_variables().end(), name) != 1) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[qualification-emission-owner]: "
              "variable-definition=%p has no exact emitting declaration\n",
              static_cast<void *>(definition));
      ROSE_ABORT();
    }
    return declaration;
  };

  auto isSemanticOnlyDeclaration = [](const SgStatement *statement) {
    const SgDeclarationStatement *declaration =
        isSgDeclarationStatement(const_cast<SgStatement *>(statement));
    if (declaration == nullptr) {
      return false;
    }
    if (isSgAuxiliaryDeclarationList(declaration->get_parent()) != nullptr) {
      return true;
    }
    return isSgNonrealDecl(const_cast<SgDeclarationStatement *>(declaration)) !=
               nullptr &&
           isSgDeclarationScope(declaration->get_parent()) != nullptr;
  };

  SgStatement *structuralStatement = isSgStatement(const_cast<SgNode *>(node));
  if (structuralStatement == nullptr) {
    structuralStatement =
        SageInterface::getEnclosingStatement(const_cast<SgNode *>(node));
  }
  structuralStatement = normalizeSemanticDeclaratorState(structuralStatement);
  emissionStatement = normalizeSemanticDeclaratorState(emissionStatement);

  if (isSgStatement(const_cast<SgNode *>(node)) != nullptr &&
      structuralStatement != nullptr &&
      !isSemanticOnlyDeclaration(structuralStatement)) {
    return structuralStatement;
  }

  // Expressions are exclusively owned syntax nodes unless their statement
  // owner is explicitly semantic-only.  Other qualification reference nodes
  // (base-class edges, template arguments, function-type positions, and
  // initialized names) are emitted by the statement carried in the unparse
  // context rather than by their structural support-node parent.
  if (isSgExpression(const_cast<SgNode *>(node)) != nullptr &&
      structuralStatement != nullptr &&
      !isSemanticOnlyDeclaration(structuralStatement)) {
    return structuralStatement;
  }

  if (emissionStatement != nullptr &&
      !isSemanticOnlyDeclaration(emissionStatement) &&
      emissionStatement != structuralStatement) {
    return emissionStatement;
  }

  if (structuralStatement != nullptr &&
      !isSemanticOnlyDeclaration(structuralStatement)) {
    return structuralStatement;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[contextual-name-qualification]: node=%p/%s "
          "structural-statement=%p/%s emission-statement=%p/%s has no exact "
          "source emission use site\n",
          static_cast<const void *>(node), node->class_name().c_str(),
          static_cast<void *>(structuralStatement),
          structuralStatement != nullptr
              ? structuralStatement->class_name().c_str()
              : "<null>",
          static_cast<void *>(emissionStatement),
          emissionStatement != nullptr ? emissionStatement->class_name().c_str()
                                       : "<null>");
  ROSE_ABORT();
}

// DQ (5/11/2011): New name qualification for ROSE (the 4th try).
// This is a part of a rewrite of the name qualification support in ROSE with
// the follwoing properties:
//    1) It is exact (no over qualification).
//    2) It handles visibility of names constructs
//    3) It resolves ambiguity of named constructs.
//    4) It resolves where type elaboration is required.
//    5) The inputs are carried in the SgUnparse_Info object for uniform
//    handling. 6) The the values in the SgUnparse_Info object are copied from
//    the AST references to the named
//       constructs to avoid where named constructs are referenced from multiple
//       locations and the name qualification might be different.
//
//    7) What about base class qualification? I might have forgotten this one!
//    No, this works,
//       but might not generate the minimum length qualified name.

void NameQualificationContext::clear() {
  qualifications.clear();
  nameChannelQualifications.clear();
  typeChannelQualifications.clear();
  pointerMemberBaseQualifications.clear();
}

void NameQualificationContext::recordChannel(
    std::map<Key, NameQualificationResult> &channel, const char *channelName,
    const SgNode *node, const SgStatement *useSiteStatement,
    const NameQualificationResult &result) {
  ASSERT_not_null(channelName);
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  const Key key(node, useSiteStatement);
  auto existing = channel.find(key);
  if (existing == channel.end()) {
    channel.emplace(key, result);
    return;
  }
  if (existing->second.qualifier != result.qualifier ||
      existing->second.length != result.length ||
      existing->second.global != result.global ||
      existing->second.typeElaboration != result.typeElaboration) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[contextual-name-qualification]: %s "
        "channel has conflicting records for node=%p/%s use-site=%p/%s "
        "existing=(qualifier='%s',length=%d,global=%d,elaboration=%d) "
        "new=(qualifier='%s',length=%d,global=%d,elaboration=%d)\n",
        channelName, static_cast<const void *>(node),
        node->class_name().c_str(), static_cast<const void *>(useSiteStatement),
        useSiteStatement->class_name().c_str(),
        existing->second.qualifier.c_str(), existing->second.length,
        existing->second.global ? 1 : 0,
        existing->second.typeElaboration ? 1 : 0, result.qualifier.c_str(),
        result.length, result.global ? 1 : 0, result.typeElaboration ? 1 : 0);
    if (const SgVarRefExp *reference =
            isSgVarRefExp(const_cast<SgNode *>(node))) {
      SgVariableSymbol *symbol = reference->get_symbol();
      SgInitializedName *declaration =
          symbol != nullptr ? symbol->get_declaration() : nullptr;
      Sg_File_Info *referenceInfo = reference->get_file_info();
      Sg_File_Info *useInfo = useSiteStatement->get_file_info();
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[contextual-name-qualification-detail]: "
          "variable=%s reference-source=%s:%d:%d parent=%p/%s "
          "declaration=%p parent=%p/%s scope=%p/%s "
          "use-source=%s:%d:%d\n",
          symbol != nullptr ? symbol->get_name().str() : "<null>",
          referenceInfo != nullptr ? referenceInfo->get_filenameString().c_str()
                                   : "<unknown>",
          referenceInfo != nullptr ? referenceInfo->get_line() : -1,
          referenceInfo != nullptr ? referenceInfo->get_col() : -1,
          static_cast<void *>(reference->get_parent()),
          reference->get_parent() != nullptr
              ? reference->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(declaration),
          declaration != nullptr
              ? static_cast<void *>(declaration->get_parent())
              : nullptr,
          declaration != nullptr && declaration->get_parent() != nullptr
              ? declaration->get_parent()->class_name().c_str()
              : "<null>",
          declaration != nullptr ? static_cast<void *>(declaration->get_scope())
                                 : nullptr,
          declaration != nullptr && declaration->get_scope() != nullptr
              ? declaration->get_scope()->class_name().c_str()
              : "<null>",
          useInfo != nullptr ? useInfo->get_filenameString().c_str()
                             : "<unknown>",
          useInfo != nullptr ? useInfo->get_line() : -1,
          useInfo != nullptr ? useInfo->get_col() : -1);
    }
    ROSE_ABORT();
  }
}

NameQualificationResult NameQualificationContext::lookupChannel(
    const std::map<Key, NameQualificationResult> &channel,
    const char *channelName, const SgNode *node,
    const SgStatement *useSiteStatement) const {
  ASSERT_not_null(channelName);
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  auto result = channel.find(Key(node, useSiteStatement));
  if (result == channel.end()) {
    const SgNode *parent = node->get_parent();
    const SgInitializedName *initializedName =
        isSgInitializedName(const_cast<SgNode *>(node));
    const SgVarRefExp *variableReference =
        isSgVarRefExp(const_cast<SgNode *>(node));
    const SgVariableSymbol *variableSymbol =
        variableReference != nullptr ? variableReference->get_symbol()
                                     : nullptr;
    const SgInitializedName *variableName =
        variableSymbol != nullptr ? variableSymbol->get_declaration() : nullptr;
    const SgLocatedNode *locatedNode =
        isSgLocatedNode(const_cast<SgNode *>(node));
    const Sg_File_Info *nodeStart =
        initializedName != nullptr
            ? initializedName->get_startOfConstruct()
            : (locatedNode != nullptr ? locatedNode->get_startOfConstruct()
                                      : nullptr);
    const Sg_File_Info *useStart = useSiteStatement->get_startOfConstruct();
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[required-name-qualification]: required "
            "contextual qualification is missing\n");
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-name-qualification]: requested "
            "%s-channel record is missing for node=%p/%s name=%s parent=%p/%s "
            "declaration=%p/%s declaration-parent=%p/%s node-line=%d "
            "use-site=%p/%s use-line=%d\n",
            channelName, static_cast<const void *>(node),
            node->class_name().c_str(),
            initializedName != nullptr
                ? initializedName->get_name().str()
                : (variableName != nullptr ? variableName->get_name().str()
                                           : "<unknown-name>"),
            static_cast<const void *>(parent),
            parent != nullptr ? parent->class_name().c_str() : "<null>",
            static_cast<const void *>(variableName),
            variableName != nullptr ? variableName->class_name().c_str()
                                    : "<null>",
            static_cast<const void *>(
                variableName != nullptr ? variableName->get_parent() : nullptr),
            variableName != nullptr && variableName->get_parent() != nullptr
                ? variableName->get_parent()->class_name().c_str()
                : "<null>",
            nodeStart != nullptr ? nodeStart->get_raw_line() : 0,
            static_cast<const void *>(useSiteStatement),
            useSiteStatement->class_name().c_str(),
            useStart != nullptr ? useStart->get_raw_line() : 0);
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
        "structural use site=%p/%s recorded use sites for this "
        "node:",
        static_cast<void *>(
            SageInterface::getEnclosingStatement(const_cast<SgNode *>(node))),
        SageInterface::getEnclosingStatement(const_cast<SgNode *>(node)) !=
                nullptr
            ? SageInterface::getEnclosingStatement(const_cast<SgNode *>(node))
                  ->class_name()
                  .c_str()
            : "<null>");
    for (const auto &entry : channel) {
      if (entry.first.first == node) {
        fprintf(stderr, " %p/%s(parent=%p/%s)",
                static_cast<const void *>(entry.first.second),
                entry.first.second->class_name().c_str(),
                static_cast<const void *>(entry.first.second->get_parent()),
                entry.first.second->get_parent() != nullptr
                    ? entry.first.second->get_parent()->class_name().c_str()
                    : "<null>");
      }
    }
    fprintf(stderr, "\n");
    ROSE_ABORT();
  }
  return result->second;
}

void NameQualificationContext::recordName(
    const SgNode *node, const SgStatement *useSiteStatement,
    const NameQualificationResult &result) {
  recordChannel(nameChannelQualifications, "name", node, useSiteStatement,
                result);
}

NameQualificationResult NameQualificationContext::lookupName(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  return lookupChannel(nameChannelQualifications, "name", node,
                       useSiteStatement);
}

void NameQualificationContext::recordType(
    const SgNode *node, const SgStatement *useSiteStatement,
    const NameQualificationResult &result) {
  recordChannel(typeChannelQualifications, "type", node, useSiteStatement,
                result);
}

NameQualificationResult NameQualificationContext::lookupType(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  return lookupChannel(typeChannelQualifications, "type", node,
                       useSiteStatement);
}

bool NameQualificationContext::containsName(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  return nameChannelQualifications.find(Key(node, useSiteStatement)) !=
         nameChannelQualifications.end();
}

bool NameQualificationContext::containsType(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  return typeChannelQualifications.find(Key(node, useSiteStatement)) !=
         typeChannelQualifications.end();
}

bool NameQualificationContext::containsPointerMemberBase(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  return pointerMemberBaseQualifications.find(Key(node, useSiteStatement)) !=
         pointerMemberBaseQualifications.end();
}

bool NameQualificationContext::contains(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  return qualifications.find(Key(node, useSiteStatement)) !=
         qualifications.end();
}

void NameQualificationContext::record(const SgNode *node,
                                      const SgStatement *useSiteStatement,
                                      const NameQualificationResult &result) {
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  const Key key(node, useSiteStatement);
  auto existing = qualifications.find(key);
  if (existing == qualifications.end()) {
    qualifications.emplace(key, result);
    return;
  }

  if (existing->second.qualifier != result.qualifier ||
      existing->second.length != result.length ||
      existing->second.global != result.global ||
      existing->second.typeElaboration != result.typeElaboration) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
            "node=%p(%s) use=%p(%s) has conflicting records "
            "old={qualifier='%s',length=%d,global=%d,elaboration=%d} "
            "new={qualifier='%s',length=%d,global=%d,elaboration=%d}\n",
            static_cast<const void *>(node), node->class_name().c_str(),
            static_cast<const void *>(useSiteStatement),
            useSiteStatement->class_name().c_str(),
            existing->second.qualifier.c_str(), existing->second.length,
            existing->second.global ? 1 : 0,
            existing->second.typeElaboration ? 1 : 0, result.qualifier.c_str(),
            result.length, result.global ? 1 : 0,
            result.typeElaboration ? 1 : 0);
    ROSE_ABORT();
  }
}

NameQualificationResult
NameQualificationContext::lookup(const SgNode *node,
                                 const SgStatement *useSiteStatement) const {
  ASSERT_not_null(node);
  ASSERT_not_null(useSiteStatement);
  const Key key(node, useSiteStatement);
  auto result = qualifications.find(key);
  if (result == qualifications.end()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[required-name-qualification]: required "
            "contextual qualification is missing\n");
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
            "node=%p(%s) parent=%p parent-type=%s requested-use=%p "
            "requested-use-type=%s has no exact contextual "
            "qualification; recorded-use-sites:",
            static_cast<const void *>(node), node->class_name().c_str(),
            static_cast<const void *>(node->get_parent()),
            node->get_parent() != nullptr
                ? node->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<const void *>(useSiteStatement),
            useSiteStatement->class_name().c_str());
    for (const auto &entry : qualifications) {
      if (entry.first.first == node) {
        fprintf(stderr, " %p(%s,name=%s)",
                static_cast<const void *>(entry.first.second),
                entry.first.second != nullptr
                    ? entry.first.second->class_name().c_str()
                    : "<null>",
                entry.first.second != nullptr
                    ? SageInterface::get_name(entry.first.second).c_str()
                    : "<null>");
      }
    }
    fprintf(stderr, "\n");
    SgTemplateArgument *argument =
        isSgTemplateArgument(const_cast<SgNode *>(node));
    if (argument != nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
              "template-argument-kind=%d\n",
              static_cast<int>(argument->get_argumentType()));
      if (SgTemplateType *parentTemplateType =
              isSgTemplateType(argument->get_parent())) {
        const SgTemplateArgumentPtrList &arguments =
            parentTemplateType->get_tpl_args();
        fprintf(
            stderr,
            "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
            "parent-template-type=%p name=%s depth=%d position=%d "
            "arguments=%zu contains=%d template-parameter=%p\n",
            static_cast<void *>(parentTemplateType),
            parentTemplateType->get_name().getString().c_str(),
            parentTemplateType->get_template_parameter_depth(),
            parentTemplateType->get_template_parameter_position(),
            arguments.size(),
            std::find(arguments.begin(), arguments.end(), argument) !=
                    arguments.end()
                ? 1
                : 0,
            static_cast<void *>(parentTemplateType->get_template_parameter()));
      }
    }
    if (argument != nullptr) {
      if (SgDeclarationStatement *referenced =
              referencedDeclarationForTemplateArgument(argument)) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
                "referenced-declaration=%p type=%s name=%s scope=%p(%s)\n",
                static_cast<void *>(referenced),
                referenced->class_name().c_str(),
                SageInterface::get_name(referenced).c_str(),
                static_cast<void *>(referenced->get_scope()),
                referenced->get_scope() != nullptr
                    ? referenced->get_scope()->class_name().c_str()
                    : "<null>");
      }
    }
    if (SgDeclarationStatement *use_declaration = isSgDeclarationStatement(
            const_cast<SgStatement *>(useSiteStatement))) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
              "use-name=%s use-scope=%p(%s)\n",
              SageInterface::get_name(use_declaration).c_str(),
              static_cast<void *>(use_declaration->get_scope()),
              use_declaration->get_scope() != nullptr
                  ? use_declaration->get_scope()->class_name().c_str()
                  : "<null>");
    }
    if (argument != nullptr) {
      if (SgNonrealDecl *parent_nonreal =
              isSgNonrealDecl(argument->get_parent())) {
        const auto &arguments = parent_nonreal->get_tpl_args();
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
                "parent-nonreal-name=%s semantic-name=%s arguments=%zu "
                "contains=%d\n",
                parent_nonreal->get_name().getString().c_str(),
                parent_nonreal->get_semantic_name().getString().c_str(),
                arguments.size(),
                std::find(arguments.begin(), arguments.end(), argument) !=
                        arguments.end()
                    ? 1
                    : 0);
      } else if (SgTemplateInstantiationDecl *parent_instantiation =
                     isSgTemplateInstantiationDecl(argument->get_parent())) {
        const auto &arguments = parent_instantiation->get_templateArguments();
        const auto &deduced_arguments =
            parent_instantiation->get_deducedTemplateArguments();
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
                "parent-instantiation-name=%s template-name=%s arguments=%zu "
                "contains=%d deduced-arguments=%zu deduced-contains=%d\n",
                parent_instantiation->get_name().getString().c_str(),
                parent_instantiation->get_templateName().getString().c_str(),
                arguments.size(),
                std::find(arguments.begin(), arguments.end(), argument) !=
                        arguments.end()
                    ? 1
                    : 0,
                deduced_arguments.size(),
                std::find(deduced_arguments.begin(), deduced_arguments.end(),
                          argument) != deduced_arguments.end()
                    ? 1
                    : 0);
      }
    }
    if (argument != nullptr) {
      fprintf(
          stderr,
          "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
          "type=%p(%s) expression=%p(%s) initialized-name=%p(%s) "
          "template-declaration=%p(%s)\n",
          static_cast<void *>(argument->get_type()),
          argument->get_type() != nullptr
              ? argument->get_type()->class_name().c_str()
              : "<null>",
          static_cast<void *>(argument->get_expression()),
          argument->get_expression() != nullptr
              ? argument->get_expression()->class_name().c_str()
              : "<null>",
          static_cast<void *>(argument->get_initializedName()),
          argument->get_initializedName() != nullptr
              ? argument->get_initializedName()->get_name().getString().c_str()
              : "<null>",
          static_cast<void *>(argument->get_templateDeclaration()),
          argument->get_templateDeclaration() != nullptr
              ? argument->get_templateDeclaration()->class_name().c_str()
              : "<null>");
      if (SgTemplateType *argumentTemplateType =
              isSgTemplateType(argument->get_type())) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[contextual-name-qualification]: "
                "argument-template-type=%p name=%s depth=%d position=%d "
                "arguments=%zu template-parameter=%p\n",
                static_cast<void *>(argumentTemplateType),
                argumentTemplateType->get_name().getString().c_str(),
                argumentTemplateType->get_template_parameter_depth(),
                argumentTemplateType->get_template_parameter_position(),
                argumentTemplateType->get_tpl_args().size(),
                static_cast<void *>(
                    argumentTemplateType->get_template_parameter()));
      }
    }
    ROSE_ABORT();
  }

  return result->second;
}

void NameQualificationContext::recordPointerMemberBase(
    const SgNode *node, const SgStatement *useSiteStatement,
    const NameQualificationResult &result) {
  ASSERT_not_null(node);
  ASSERT_not_null(isSgPointerMemberType(const_cast<SgNode *>(node)));
  ASSERT_not_null(useSiteStatement);
  const Key key(node, useSiteStatement);
  auto existing = pointerMemberBaseQualifications.find(key);
  if (existing == pointerMemberBaseQualifications.end()) {
    pointerMemberBaseQualifications.emplace(key, result);
    return;
  }

  if (existing->second.qualifier != result.qualifier ||
      existing->second.length != result.length ||
      existing->second.global != result.global ||
      existing->second.typeElaboration != result.typeElaboration) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-pointer-member-base-"
            "qualification]: node=%p(%s) use=%p(%s) has conflicting "
            "records old={qualifier='%s',length=%d,global=%d,elaboration=%d} "
            "new={qualifier='%s',length=%d,global=%d,elaboration=%d}\n",
            static_cast<const void *>(node), node->class_name().c_str(),
            static_cast<const void *>(useSiteStatement),
            useSiteStatement->class_name().c_str(),
            existing->second.qualifier.c_str(), existing->second.length,
            existing->second.global ? 1 : 0,
            existing->second.typeElaboration ? 1 : 0, result.qualifier.c_str(),
            result.length, result.global ? 1 : 0,
            result.typeElaboration ? 1 : 0);
    ROSE_ABORT();
  }
}

NameQualificationResult NameQualificationContext::lookupPointerMemberBase(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  ASSERT_not_null(node);
  ASSERT_not_null(isSgPointerMemberType(const_cast<SgNode *>(node)));
  ASSERT_not_null(useSiteStatement);
  const Key key(node, useSiteStatement);
  auto result = pointerMemberBaseQualifications.find(key);
  if (result == pointerMemberBaseQualifications.end()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[contextual-pointer-member-base-"
            "qualification]: node=%p(%s) parent=%p parent-type=%s "
            "requested-use=%p requested-use-type=%s has no exact contextual "
            "base-type qualification; recorded-use-sites:",
            static_cast<const void *>(node), node->class_name().c_str(),
            static_cast<const void *>(node->get_parent()),
            node->get_parent() != nullptr
                ? node->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<const void *>(useSiteStatement),
            useSiteStatement->class_name().c_str());
    for (const auto &entry : pointerMemberBaseQualifications) {
      if (entry.first.first == node) {
        fprintf(stderr, " %p(%s)",
                static_cast<const void *>(entry.first.second),
                entry.first.second->class_name().c_str());
      }
    }
    fprintf(stderr, "\n");
    ROSE_ABORT();
  }
  return result->second;
}

NameQualificationResult Unparser_Nameq::lookup_qualification(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  return nameQualifications.lookup(node, useSiteStatement);
}

NameQualificationResult Unparser_Nameq::lookup_name_qualification(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  return nameQualifications.lookupName(node, useSiteStatement);
}

NameQualificationResult Unparser_Nameq::lookup_type_qualification(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  NameQualificationResult result =
      nameQualifications.lookupType(node, useSiteStatement);
  if (const std::optional<bool> sourceElaboration =
          exactSourceTypeElaboration(node)) {
    result.typeElaboration = *sourceElaboration;
  }
  return result;
}

NameQualificationResult Unparser_Nameq::lookup_type_qualification_for_output(
    const SgNode *node, const SgStatement *useSiteStatement,
    bool skipGeneratedQualification) const {
  ASSERT_not_null(node);
  NameQualificationResult result =
      skipGeneratedQualification
          ? NameQualificationResult{"", 0, false, false}
          : lookup_type_qualification(node, useSiteStatement);
  if (const std::optional<bool> sourceElaboration =
          exactSourceTypeElaboration(node)) {
    result.typeElaboration = *sourceElaboration;
  }
  if (const SgTemplateArgument *templateArgument =
          isSgTemplateArgument(const_cast<SgNode *>(node))) {
    if (templateArgument->get_source_type_qualification_present()) {
      if (templateArgument->get_sourceSpelledType() == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[template-argument-source-type]: "
                "argument=%p owns exact source qualification without an exact "
                "source-spelled type\n",
                static_cast<const void *>(templateArgument));
        ROSE_ABORT();
      }
      // The elaborated keyword is source syntax owned by this exact template
      // argument. Skipping generated qualification cannot discard source
      // syntax, and contextual qualification cannot replace the immutable
      // TypeLoc spelling recorded by the frontend.
      result.typeElaboration =
          templateArgument->get_type_elaboration_required();
    }
  }
  return result;
}

NameQualificationResult
Unparser_Nameq::lookup_pointer_member_base_qualification(
    const SgNode *node, const SgStatement *useSiteStatement) const {
  return nameQualifications.lookupPointerMemberBase(node, useSiteStatement);
}

// DQ (3/14/2019): Adding debugging support to output the map of names.
void Unparser_Nameq::outputNameQualificationMap(
    const SgUnorderedMapNodeToString &qualifiedNameMap) {
  printf("qualifiedNameMap.size() = %zu \n", qualifiedNameMap.size());
  SgUnorderedMapNodeToString::const_iterator i = qualifiedNameMap.begin();
  while (i != qualifiedNameMap.end()) {
    ASSERT_not_null(i->first);

    printf(" --- *i = i->first = %p = %s i->second = %s \n", i->first,
           i->first->class_name().c_str(), i->second.c_str());

    i++;
  }
}
