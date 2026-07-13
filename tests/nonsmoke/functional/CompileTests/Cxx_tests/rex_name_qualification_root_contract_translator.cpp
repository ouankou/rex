#include "nameQualificationSupport.h"
#include "rose.h"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace {
bool hasTokens(const SgStringList &actual,
               std::initializer_list<const char *> expected) {
  if (actual.size() != expected.size()) {
    return false;
  }
  return std::equal(
      actual.begin(), actual.end(), expected.begin(),
      [](const std::string &left, const char *right) { return left == right; });
}

SgSourceFile *mainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    if (SgSourceFile *source = isSgSourceFile(file)) {
      if (!source->get_isHeaderFile()) {
        return source;
      }
    }
  }
  return nullptr;
}

SgInitializedName *findInitializedName(SgProject *project, const SgName &name) {
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName != nullptr && initializedName->get_name() == name) {
      return initializedName;
    }
  }
  return nullptr;
}

SgInitializedName *findBasePreinitializer(SgProject *project) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName == nullptr ||
        initializedName->get_preinitialization() !=
            SgInitializedName::e_nonvirtual_base_class) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = initializedName;
  }
  return result;
}

SgMemberFunctionRefExp *findExtentReference(SgProject *project) {
  SgMemberFunctionRefExp *result = nullptr;
  auto consider = [&](SgNode *node) {
    SgMemberFunctionRefExp *reference = isSgMemberFunctionRefExp(node);
    SgMemberFunctionSymbol *symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    SgMemberFunctionDeclaration *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    if (declaration == nullptr || declaration->get_name() != "extent") {
      return;
    }
    ROSE_ASSERT(result == nullptr || result == reference);
    result = reference;
  };
  auto scan = [&](SgNode *root) {
    for (SgNode *node :
         NodeQuery::querySubTree(root, V_SgMemberFunctionRefExp)) {
      consider(node);
    }
  };
  scan(project);
  if (result == nullptr) {
    SgInitializedName *value = findInitializedName(project, "value");
    if (value != nullptr && value->get_type() != nullptr) {
      scan(value->get_type());
    }
  }
  if (result == nullptr) {
    VariantVector variants(V_SgMemberFunctionRefExp);
    for (SgNode *node : NodeQuery::queryMemoryPool(variants)) {
      consider(node);
    }
  }
  return result;
}

void clearSourceTypeQualifier(SgInitializedName *initializedName) {
  ROSE_ASSERT(initializedName != nullptr);
  initializedName->set_source_type_qualification_present(false);
  initializedName->set_source_type_global_qualification(false);
  initializedName->get_source_type_qualification_tokens().clear();
  initializedName->set_name_qualification_length_for_type(0);
  initializedName->set_global_qualification_required_for_type(false);
}
} // namespace

int main(int argc, char **argv) {
  bool rejectMissingMemberContext = false;
  if (argc > 1 && std::string(argv[1]) == "--reject-missing-member-context") {
    rejectMissingMemberContext = true;
    for (int index = 1; index + 1 < argc; ++index) {
      argv[index] = argv[index + 1];
    }
    --argc;
  }

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgMemberFunctionRefExp *extentReference = findExtentReference(project);
  ROSE_ASSERT(extentReference != nullptr);
  ROSE_ASSERT(SageInterface::isMemberFunctionMemberReference(extentReference));
  ROSE_ASSERT(!SageInterface::isAddressTaken(extentReference));
  ROSE_ASSERT(SageInterface::getEnclosingStatement(extentReference) == nullptr);

  if (rejectMissingMemberContext) {
    // Construct the exact detached type-use shape that used to be skipped:
    // a non-static member reference with an empty ambiguity chain and no
    // statement owner.  The source AST supplies the typed declarations and
    // symbols; the orphaned access deliberately supplies no emission context.
    SgInitializedName *self = findInitializedName(project, "self");
    ROSE_ASSERT(self != nullptr);
    SgVarRefExp *selfReference =
        SageBuilder::buildVarRefExp(self, self->get_scope());
    ROSE_ASSERT(selfReference != nullptr);
    SgMemberFunctionRefExp *orphanReference =
        SageBuilder::buildMemberFunctionRefExp(extentReference->get_symbol(),
                                               false, false);
    ROSE_ASSERT(orphanReference != nullptr);
    SgDotExp *orphanAccess = SageBuilder::buildDotExp(
        selfReference, orphanReference, orphanReference->get_type());
    ROSE_ASSERT(orphanAccess != nullptr);
    ROSE_ASSERT(
        SageInterface::isMemberFunctionMemberReference(orphanReference));
    ROSE_ASSERT(!SageInterface::isAddressTaken(orphanReference));
    ROSE_ASSERT(
        SageInterface::getClassTypeChainForMemberReference(orphanReference)
            .empty());
    ROSE_ASSERT(SageInterface::getEnclosingStatement(orphanReference) ==
                nullptr);

    NameQualificationTraversal::NameQualificationMapType names;
    NameQualificationTraversal::NameQualificationMapType types;
    NameQualificationTraversal::NameQualificationMapOfMapsType nestedTypes;
    NameQualificationTraversal::NameQualificationSetType referencedNames;
    NameQualificationContext qualifications;
    NameQualificationTraversal traversal(names, types, nestedTypes,
                                         referencedNames, qualifications);
    NameQualificationInheritedAttribute inherited;
    SgMemberFunctionDeclaration *declaration =
        orphanReference->get_symbol()->get_declaration();
    ROSE_ASSERT(declaration != nullptr);
    ROSE_ASSERT(declaration->get_scope() != nullptr);
    inherited.set_currentScope(declaration->get_scope());
    (void)traversal.evaluateInheritedAttribute(orphanReference, inherited);
    return 1;
  }

  SgInitializedName *basePreinitializer = findBasePreinitializer(project);
  ROSE_ASSERT(basePreinitializer != nullptr);
  ROSE_ASSERT(basePreinitializer->get_name() == "Nested");
  ROSE_ASSERT(basePreinitializer->get_source_type_qualification_present());
  ROSE_ASSERT(!basePreinitializer->get_source_type_global_qualification());
  ROSE_ASSERT(hasTokens(
      basePreinitializer->get_source_type_qualification_tokens(), {"Base::"}));

  SgInitializedName *enumUse =
      findInitializedName(project, "rex_nq_enum_value");
  ROSE_ASSERT(enumUse != nullptr);
  ROSE_ASSERT(enumUse->get_source_type_qualification_present());
  ROSE_ASSERT(!enumUse->get_source_type_global_qualification());
  ROSE_ASSERT(hasTokens(enumUse->get_source_type_qualification_tokens(),
                        {"rex_nq_enum_outer::", "a::"}));

  // Generated/transformed AST uses have no source spelling payload.  Clear
  // only this test use after proving that the frontend captured it, then
  // require semantic qualification to reconstruct the complete nested owner
  // rather than merely incrementing a local depth.
  clearSourceTypeQualifier(enumUse);

  SgSourceFile *source = mainSourceFile(project);
  ROSE_ASSERT(source != nullptr);
  SgUnorderedNodeSet referencedNames;
  NameQualificationContext qualifications;
  generateNameQualificationSupport(source, referencedNames, qualifications);

  SgVariableDeclaration *enumStatement =
      isSgVariableDeclaration(enumUse->get_declaration());
  ROSE_ASSERT(enumStatement != nullptr);
  ROSE_ASSERT(qualifications.containsType(enumUse, enumStatement));
  const NameQualificationResult enumQualification =
      qualifications.lookupType(enumUse, enumStatement);
  ROSE_ASSERT(enumQualification.qualifier == "rex_nq_enum_outer::a::" ||
              enumQualification.qualifier == "::rex_nq_enum_outer::a::");
  ROSE_ASSERT(enumQualification.length >= 2);

  SgInitializedName *value = findInitializedName(project, "value");
  ROSE_ASSERT(value != nullptr);
  SgVariableDeclaration *valueStatement =
      isSgVariableDeclaration(value->get_declaration());
  ROSE_ASSERT(valueStatement != nullptr);
  ROSE_ASSERT(qualifications.containsName(extentReference, valueStatement));

  SgCtorInitializerList *initializerList =
      isSgCtorInitializerList(basePreinitializer->get_parent());
  ROSE_ASSERT(initializerList != nullptr);
  ROSE_ASSERT(qualifications.containsType(basePreinitializer, initializerList));
  ROSE_ASSERT(qualifications.lookupType(basePreinitializer, initializerList)
                  .qualifier == "Base::");

  return backend(project);
}
