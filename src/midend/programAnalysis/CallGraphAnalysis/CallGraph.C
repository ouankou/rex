#include "AstNodes/Expression/ExpressionTypeClassification.h"
#include "sage3basic.h"

#include "CallGraph.h"

#include "rose_config.h"

#include <mutex>
#include <set>

using namespace std;
using namespace Rose;

static bool
isUninstantiatedTemplateFunctionPattern(SgFunctionDeclaration *fdecl) {
  return isSgTemplateFunctionDeclaration(fdecl) != NULL ||
         isSgTemplateMemberFunctionDeclaration(fdecl) != NULL;
}

static bool
callGraphTypeContainsDependentTemplateType(SgType *type,
                                           std::set<SgType *> &seen);

static bool callGraphTypeContainsDependentTemplateType(SgType *type) {
  std::set<SgType *> seen;
  return callGraphTypeContainsDependentTemplateType(type, seen);
}

static bool
callGraphTypeContainsDependentTemplateType(SgType *type,
                                           std::set<SgType *> &seen) {
  if (type == NULL || !seen.insert(type).second) {
    return false;
  }

  if (isSgTemplateType(type) != NULL || isSgNonrealType(type) != NULL) {
    return true;
  }

  if (SgModifierType *modifierType = isSgModifierType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        modifierType->get_base_type(), seen);
  }
  if (SgPointerType *pointerType = isSgPointerType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        pointerType->get_base_type(), seen);
  }
  if (SgPointerMemberType *memberPointerType = isSgPointerMemberType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        memberPointerType->get_base_type(), seen);
  }
  if (SgReferenceType *referenceType = isSgReferenceType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        referenceType->get_base_type(), seen);
  }
  if (SgRvalueReferenceType *referenceType = isSgRvalueReferenceType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        referenceType->get_base_type(), seen);
  }
  if (SgArrayType *arrayType = isSgArrayType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        arrayType->get_base_type(), seen);
  }
  if (SgTypedefType *typedefType = isSgTypedefType(type)) {
    return callGraphTypeContainsDependentTemplateType(
        typedefType->get_base_type(), seen);
  }

  if (SgFunctionType *functionType = isSgFunctionType(type)) {
    if (callGraphTypeContainsDependentTemplateType(
            functionType->get_return_type(), seen)) {
      return true;
    }
    for (SgType *argType : functionType->get_arguments()) {
      if (callGraphTypeContainsDependentTemplateType(argType, seen)) {
        return true;
      }
    }
  }

  return false;
}

static bool
isUnresolvedTemplateInstantiationCallable(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return false;
  }

  if (isSgTemplateInstantiationFunctionDecl(fdecl) == NULL &&
      isSgTemplateInstantiationMemberFunctionDecl(fdecl) == NULL) {
    return false;
  }

  if (SgTemplateInstantiationMemberFunctionDecl *instMember =
          isSgTemplateInstantiationMemberFunctionDecl(fdecl)) {
    SgTemplateMemberFunctionDeclaration *templateDecl =
        instMember->get_templateDeclaration();
    if (templateDecl == NULL) {
      templateDecl = isSgTemplateMemberFunctionDeclaration(
          instMember->get_specializedTemplateDeclaration());
    }

    if (callGraphInstantiatedMemberHasClassTemplateArguments(instMember) &&
        callGraphDeclHasDefinitionOrDefiningDeclaration(templateDecl)) {
      return false;
    }
  }

  return !callGraphDeclHasDefinitionOrDefiningDeclaration(fdecl) &&
         callGraphTypeContainsDependentTemplateType(fdecl->get_type());
}

static bool isNestedInUninstantiatedTemplateClassContext(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (isSgTemplateInstantiationDefn(cursor) != NULL ||
        isSgTemplateInstantiationDecl(cursor) != NULL) {
      return false;
    }
    if (isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateClassDeclaration(cursor) != NULL) {
      return true;
    }
  }

  return false;
}

static bool isNestedInTemplateInstantiationContext(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (isSgTemplateInstantiationDefn(cursor) != NULL ||
        isSgTemplateInstantiationDecl(cursor) != NULL) {
      return true;
    }
    if (isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateClassDeclaration(cursor) != NULL) {
      return false;
    }
  }

  return false;
}

static bool
isUninstantiatedTemplateClassMemberPattern(SgFunctionDeclaration *fdecl) {
  SgMemberFunctionDeclaration *member = isSgMemberFunctionDeclaration(fdecl);
  if (member == NULL) {
    return false;
  }

  if (SgClassDeclaration *associatedClass =
          isSgClassDeclaration(member->get_associatedClassDeclaration())) {
    if (isNestedInUninstantiatedTemplateClassContext(associatedClass)) {
      return true;
    }
  }

  return isNestedInUninstantiatedTemplateClassContext(member);
}

static SgFunctionDeclaration *
canonicalCallableFunctionDecl(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return NULL;
  }

  fdecl = canonicalFunctionDeclForCallGraph(fdecl);
  if (isUninstantiatedTemplateFunctionPattern(fdecl) ||
      isUninstantiatedTemplateClassMemberPattern(fdecl) ||
      isUnresolvedTemplateInstantiationCallable(fdecl)) {
    return NULL;
  }

  return fdecl;
}

bool callGraphDeclHasIndependentCallableIdentity(SgFunctionDeclaration *fdecl) {
  return canonicalCallableFunctionDecl(fdecl) != NULL;
}

static SgMemberFunctionDeclaration *
resolveTemplateMemberFunctionDeclarationFromRef(
    SgTemplateMemberFunctionRefExp *ref) {
  if (ref == NULL) {
    return NULL;
  }
  // The symbol deliberately preserves the source template identity.  Call
  // analysis consumes the independently published semantic specialization;
  // resolving through the symbol silently drops the selected callable.
  return ref->getAssociatedMemberFunctionDeclaration();
}

static SgMemberFunctionDeclaration *
resolveMemberFunctionDeclarationFromExpression(SgExpression *expr) {
  if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    return ref->getAssociatedMemberFunctionDeclaration();
  }

  if (SgTemplateMemberFunctionRefExp *ref =
          isSgTemplateMemberFunctionRefExp(expr)) {
    return resolveTemplateMemberFunctionDeclarationFromRef(ref);
  }

  if (SgNonrealRefExp *ref = isSgNonrealRefExp(expr)) {
    return isSgMemberFunctionDeclaration(
        ref->get_resolved_function_declaration());
  }

  return NULL;
}

static bool memberFunctionRefNeedsQualifier(SgExpression *expr) {
  if (SgMemberFunctionRefExp *ref = isSgMemberFunctionRefExp(expr)) {
    return ref->get_need_qualifier() != 0;
  }

  if (SgTemplateMemberFunctionRefExp *ref =
          isSgTemplateMemberFunctionRefExp(expr)) {
    return ref->get_need_qualifier() != 0;
  }

  return true;
}

static SgClassType *
getClassTypeFromDeclaration(SgClassDeclaration *class_decl) {
  if (class_decl == NULL) {
    return NULL;
  }

  if (class_decl->get_type() != NULL) {
    return isSgClassType(class_decl->get_type());
  }

  if (SgClassDeclaration *nonDefDecl =
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration())) {
    if (nonDefDecl->get_type() != NULL) {
      return isSgClassType(nonDefDecl->get_type());
    }
  }

  if (SgClassDeclaration *defDecl =
          isSgClassDeclaration(class_decl->get_definingDeclaration())) {
    if (defDecl->get_type() != NULL) {
      return isSgClassType(defDecl->get_type());
    }
  }

  return NULL;
}

static SgClassType *
resolveClassTypeFromMemberDecl(SgMemberFunctionDeclaration *member_decl) {
  if (member_decl == NULL)
    return NULL;

  if (SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
          member_decl->get_firstNondefiningDeclaration())) {
    member_decl = nonDefDecl;
  }

  if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
          isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
    if (SgClassDeclaration *associated_class = isSgClassDeclaration(
            inst_member->get_associatedClassDeclaration())) {
      if (SgClassType *associated_type =
              getClassTypeFromDeclaration(associated_class)) {
        return associated_type;
      }
    }
  }

  SgScopeStatement *scope = member_decl->get_scope();
  if (scope == NULL)
    scope = isSgScopeStatement(member_decl->get_parent());

  if (SgClassDefinition *class_def = isSgClassDefinition(scope))
    return getClassTypeFromDeclaration(class_def->get_declaration());

  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope))
    return getClassTypeFromDeclaration(inst_def->get_declaration());

  if (SgTemplateClassDefinition *tmpl_def = isSgTemplateClassDefinition(scope))
    return getClassTypeFromDeclaration(tmpl_def->get_declaration());

  return NULL;
}

static SgMemberFunctionDeclaration *
enclosingMemberFunctionDeclaration(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (SgFunctionDefinition *def = isSgFunctionDefinition(cursor)) {
      return isSgMemberFunctionDeclaration(def->get_declaration());
    }

    if (SgMemberFunctionDeclaration *decl =
            isSgMemberFunctionDeclaration(cursor)) {
      return decl;
    }
  }

  return NULL;
}

static SgClassDeclaration *
canonicalClassDeclaration(SgClassDeclaration *class_decl) {
  if (class_decl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *nonDefDecl =
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration())) {
    return nonDefDecl;
  }

  return class_decl;
}

static SgType *receiverBindingType(SgExpression *expression) {
  ASSERT_not_null(expression);
  SgBinaryOp *access = isSgDotExp(expression);
  if (access == NULL)
    access = isSgArrowExp(expression);
  if (access != NULL) {
    if (auto *field = isSgVarRefExp(access->get_rhs_operand()))
      return field->get_type();
  }
  if (auto *call = isSgFunctionCallExp(expression)) {
    ASSERT_not_null(call->get_function());
    ASSERT_not_null(call->get_function()->get_type());
    auto *signature =
        isSgFunctionType(call->get_function()->get_type()->findBaseType());
    ASSERT_not_null(signature);
    return signature->get_return_type();
  }
  return expression->get_type();
}

static SgClassType *receiverClassType(SgType *type) {
  ASSERT_not_null(type);
  return Rose::AST::resolvedClassType(type, true);
}

static bool is_callable_signatures_equal(SgFunctionType *expected,
                                         SgFunctionType *candidate,
                                         bool erase_static_member_owner) {
  ASSERT_not_null(expected);
  ASSERT_not_null(candidate);
  if (!erase_static_member_owner) {
    return SageInterface::isEquivalentFunctionType(expected, candidate);
  }

  SgMemberFunctionType *expected_member = isSgMemberFunctionType(expected);
  SgMemberFunctionType *candidate_member = isSgMemberFunctionType(candidate);
  if (expected_member != NULL && candidate_member != NULL) {
    if (expected_member->get_mfunc_specifier() !=
        candidate_member->get_mfunc_specifier()) {
      return false;
    }
  } else {
    // Erasing the class owner is valid for a static-member/function-pointer
    // comparison only.  A non-static member's cv/ref qualifiers remain part
    // of its callable identity.
    SgMemberFunctionType *member =
        expected_member != NULL ? expected_member : candidate_member;
    if (member != NULL && member->get_mfunc_specifier() != 0) {
      return false;
    }
  }
  if (expected->get_has_ellipses() != candidate->get_has_ellipses() ||
      !SageInterface::isEquivalentType(expected->get_return_type(),
                                       candidate->get_return_type()))
    return false;

  const SgTypePtrList &expected_arguments = expected->get_arguments();
  const SgTypePtrList &candidate_arguments = candidate->get_arguments();
  return expected_arguments.size() == candidate_arguments.size() &&
         std::equal(expected_arguments.begin(), expected_arguments.end(),
                    candidate_arguments.begin(),
                    SageInterface::isEquivalentType);
}

static bool isOverridingFunction(SgMemberFunctionDeclaration *candidate,
                                 SgMemberFunctionDeclaration *member) {
  ASSERT_not_null(candidate);
  ASSERT_not_null(member);
  if (!candidate->get_functionModifier().isVirtual() ||
      candidate->get_name() != member->get_name())
    return false;

  auto *derived = isSgMemberFunctionType(candidate->get_type());
  auto *base = isSgMemberFunctionType(member->get_type());
  ASSERT_not_null(derived);
  ASSERT_not_null(base);
  if (derived->get_mfunc_specifier() != base->get_mfunc_specifier() ||
      derived->get_has_ellipses() != base->get_has_ellipses())
    return false;

  const auto &derivedArguments = derived->get_arguments();
  const auto &baseArguments = base->get_arguments();
  return derivedArguments.size() == baseArguments.size() &&
         std::equal(derivedArguments.begin(), derivedArguments.end(),
                    baseArguments.begin(), SageInterface::isEquivalentType);
}

static std::vector<SgMemberFunctionDeclaration *>
classMemberFunctions(SgClassDefinition *definition) {
  ASSERT_not_null(definition);
  auto declarations = definition->get_members();
  if (auto *auxiliary = definition->get_auxiliary_declarations()) {
    ASSERT_require(auxiliary->get_parent() == definition);
    for (auto *declaration : auxiliary->get_declarations()) {
      ASSERT_not_null(declaration);
      ASSERT_require(
          declaration->get_scope() == definition &&
          SageInterface::hasExactSemanticAuxiliaryOwnership(declaration));
      declarations.push_back(declaration);
    }
  }
  std::set<SgFunctionDeclaration *> seen;
  std::vector<SgMemberFunctionDeclaration *> functions;
  for (auto *declaration : declarations) {
    if (auto *member = isSgMemberFunctionDeclaration(declaration)) {
      if (seen.insert(canonicalFunctionDeclForCallGraph(member)).second)
        functions.push_back(member);
    }
  }
  return functions;
}

SgFunctionDeclaration *
is_function_exists(SgClassDefinition *cls,
                   SgMemberFunctionDeclaration *memberFunctionDeclaration) {
  ASSERT_not_null(cls);
  ASSERT_not_null(memberFunctionDeclaration);
  for (auto *candidate : classMemberFunctions(cls)) {
    if (!isOverridingFunction(candidate, memberFunctionDeclaration) ||
        candidate->get_functionModifier().isPureVirtual())
      continue;
    return canonicalCallableFunctionDecl(candidate);
  }
  return nullptr;
}

bool dummyFilter::operator()(SgFunctionDeclaration *) const { return true; }

bool builtinFilter::operator()(SgFunctionDeclaration *funcDecl) const {
  if (funcDecl == NULL) {
    return false;
  }
  const Sg_File_Info *fileInfo = funcDecl->get_file_info();
  ASSERT_not_null(fileInfo);
  return !fileInfo->isCompilerGenerated() && !fileInfo->isFrontendSpecific() &&
         !SageInterface::insideSystemHeader(funcDecl);
}

bool FunctionData::isDefined() { return hasDefinition; }

CallGraphBuilder::CallGraphBuilder(SgProject *proj) {
  project = proj;
  graph = NULL;
}

SgIncidenceDirectedGraph *CallGraphBuilder::getGraph() { return graph; }

/**
 * CallTargetSet::solveFunctionPointerCallsFunctional
 *
 * \brief Checks if the functionDeclaration (node) matches functionType
 *
 * This is a filter called by solveFunctionPointerCall. It checks that node is
 * a functiondeclaration (or template instantiation) of type functionType.
 * If it does, it is added to a functionList and returned.  So function list can
 * have at most 1 entry.
 *
 * @param[in] node : The node we are checking.  It must be an
 *SgFunctionDeclaration
 * @param[in] functionType : The function type being checked.
 * @return: If node matched functionType, it is added on functionList and
 *returned.  Otherwise functionList is empty.
 **/
Rose_STL_Container<SgFunctionDeclaration *>
CallTargetSet::solveFunctionPointerCallsFunctional(
    SgNode *node, SgFunctionType *functionType) {
  ASSERT_not_null(functionType);

  Rose_STL_Container<SgFunctionDeclaration *> functionList;

  SgFunctionDeclaration *fctDecl =
      canonicalCallableFunctionDecl(isSgFunctionDeclaration(node));
  if (fctDecl == NULL) {
    return functionList;
  }
  SgMemberFunctionDeclaration *static_member =
      isSgMemberFunctionDeclaration(fctDecl);
  if (static_member != NULL) {
    if (!static_member->get_declarationModifier()
             .get_storageModifier()
             .isStatic()) {
      return functionList;
    }
  }

  SgFunctionType *candidate_type = isSgFunctionType(fctDecl->get_type());
  if (candidate_type != NULL &&
      is_callable_signatures_equal(functionType, candidate_type,
                                   static_member != NULL)) {
    functionList.push_back(fctDecl);
  }
  return functionList;
}

static void appendFunctionPointerSignatureTargets(
    SgExpression *expression, const char *context,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList);

std::vector<SgFunctionDeclaration *>
CallTargetSet::solveFunctionPointerCall(SgPointerDerefExp *pointerDerefExp) {
  std::vector<SgFunctionDeclaration *> functions;
  appendFunctionPointerSignatureTargets(pointerDerefExp, "dereference",
                                        functions);
  return functions;
}

std::vector<SgFunctionDeclaration *>
CallTargetSet::solveMemberFunctionPointerCall(
    SgExpression *functionExp, ClassHierarchyWrapper *classHierarchy) {
  ASSERT_require(isSgArrowStarOp(functionExp) || isSgDotStarOp(functionExp));
  std::set<SgFunctionDeclaration *> functions;
  if (auto *enclosing =
          SageInterface::getEnclosingFunctionDeclaration(functionExp)) {
    if (!callGraphDeclHasIndependentCallableIdentity(enclosing))
      return {};
  }

  auto *access = isSgBinaryOp(functionExp);
  ASSERT_not_null(access->get_rhs_operand());
  ASSERT_not_null(access->get_rhs_operand()->get_type());
  auto *pointer = isSgPointerMemberType(
      access->get_rhs_operand()->get_type()->stripTypedefsAndModifiers());
  ASSERT_not_null(pointer);
  auto *signature = isSgMemberFunctionType(
      pointer->get_base_type()->stripTypedefsAndModifiers());
  ASSERT_not_null(signature);
  auto *owner =
      isSgClassType(pointer->get_class_type()->stripTypedefsAndModifiers());
  ASSERT_not_null(owner);
  ASSERT_not_null(owner->get_declaration());
  auto *defining =
      isSgClassDeclaration(owner->get_declaration()->get_definingDeclaration());
  // A forward declaration has no declared member candidates in this project.
  if (defining == nullptr)
    return {};
  auto *definition = defining->get_definition();
  ASSERT_not_null(definition);

  ASSERT_not_null(classHierarchy);
  auto owners = classHierarchy->getAncestorClasses(definition);
  owners.insert(definition);
  auto dispatchClasses = classHierarchy->getSubclasses(definition);
  dispatchClasses.insert(definition);
  for (auto *candidateOwner : owners) {
    for (auto *declaration : classMemberFunctions(candidateOwner)) {
      auto *member = isSgMemberFunctionDeclaration(
          canonicalCallableFunctionDecl(declaration));
      if (member == nullptr)
        continue;
      auto *memberSignature = isSgMemberFunctionType(member->get_type());
      ASSERT_not_null(memberSignature);
      if (!is_callable_signatures_equal(signature, memberSignature,
                                        /*erase_static_member_owner=*/true))
        continue;
      if (!member->get_functionModifier().isPureVirtual())
        functions.insert(member);
      if (!member->get_functionModifier().isVirtual())
        continue;

      for (auto *subclass : dispatchClasses) {
        for (auto *overrider : classMemberFunctions(subclass)) {
          if (!isOverridingFunction(overrider, member))
            continue;
          overrider = isSgMemberFunctionDeclaration(
              canonicalCallableFunctionDecl(overrider));
          ASSERT_not_null(overrider);
          if (!overrider->get_functionModifier().isPureVirtual())
            functions.insert(overrider);
        }
      }
    }
  }
  return {functions.begin(), functions.end()};
}

static bool isPureVirtual(SgMemberFunctionDeclaration *dcl) {
  ASSERT_not_null(dcl);

  return dcl->get_functionModifier().isPureVirtual();
}

static bool callGraphDeclFileInfoIsUsable(SgDeclarationStatement *decl) {
  if (decl == NULL || decl->get_file_info() == NULL) {
    return false;
  }

  Sg_File_Info *fileInfo = decl->get_file_info();
  return fileInfo->get_physical_file_id() >= 0 &&
         !fileInfo->get_physical_filename().empty();
}

static std::string
callGraphUsableDeclarationFilename(SgDeclarationStatement *decl) {
  if (!callGraphDeclFileInfoIsUsable(decl)) {
    return "";
  }

  return decl->get_file_info()->get_physical_filename();
}

static bool callGraphProjectContainsSourceFile(SgProject *project,
                                               const std::string &filename) {
  if (project == NULL || filename.empty()) {
    return false;
  }

  for (SgFile *file : project->get_fileList()) {
    if (file == NULL) {
      continue;
    }

    if (filename == file->getFileName() ||
        filename == file->get_sourceFileNameWithPath()) {
      return true;
    }
  }

  return false;
}

std::optional<bool>
callGraphFunctionHasUserSourceOwnership(SgFunctionDeclaration *declaration) {
  if (declaration == NULL) {
    return std::nullopt;
  }

  const auto ownership = declaration->get_frontend_source_ownership();
  switch (ownership) {
  case SgFunctionDeclaration::e_frontend_source_main_file:
  case SgFunctionDeclaration::e_frontend_source_application_header:
    return true;
  case SgFunctionDeclaration::e_frontend_source_system_header:
  case SgFunctionDeclaration::e_frontend_source_external:
  case SgFunctionDeclaration::e_frontend_source_support:
  case SgFunctionDeclaration::e_frontend_source_pseudo_file:
  case SgFunctionDeclaration::e_frontend_source_implicit:
    return false;
  case SgFunctionDeclaration::e_frontend_source_unclassified:
    break;
  }

  Sg_File_Info *fileInfo = declaration->get_file_info();
  SgSourceFile *sourceFile =
      SageInterface::getEnclosingSourceFile(declaration, true);
  const bool clangLanguage =
      sourceFile != NULL &&
      (sourceFile->get_C_only() || sourceFile->get_C99_only() ||
       sourceFile->get_Cxx_only() || sourceFile->get_Cuda_only() ||
       sourceFile->get_OpenCL_only());
  const bool semanticFrontendDeclaration =
      fileInfo != NULL && fileInfo->isCompilerGenerated() &&
      fileInfo->isFrontendSpecific() && !fileInfo->isTransformation();
  if (clangLanguage && semanticFrontendDeclaration) {
    fprintf(stderr,
            "REX_CALLGRAPH_INVARIANT[function-source-ownership]: semantic "
            "Clang function=%p/%s name=%s has no producer-published frontend "
            "source ownership\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            declaration->get_qualified_name().getString().c_str());
    ROSE_ABORT();
  }
  return std::nullopt;
}

static SgClassDeclaration *
callGraphAssociatedClassDeclaration(SgMemberFunctionDeclaration *memberDecl) {
  if (memberDecl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *associatedClass =
          isSgClassDeclaration(memberDecl->get_associatedClassDeclaration())) {
    return associatedClass;
  }

  if (SgClassDefinition *classDefinition =
          isSgClassDefinition(memberDecl->get_scope())) {
    return classDefinition->get_declaration();
  }

  if (SgClassDefinition *classDefinition =
          isSgClassDefinition(memberDecl->get_parent())) {
    return classDefinition->get_declaration();
  }

  return NULL;
}

static SgTemplateInstantiationDecl *
callGraphEnclosingTemplateInstantiationDeclaration(SgNode *node) {
  for (SgNode *cursor = node; cursor != NULL; cursor = cursor->get_parent()) {
    if (SgTemplateInstantiationDefn *instDefn =
            isSgTemplateInstantiationDefn(cursor)) {
      return isSgTemplateInstantiationDecl(instDefn->get_declaration());
    }
    if (SgTemplateInstantiationDecl *instDecl =
            isSgTemplateInstantiationDecl(cursor)) {
      return instDecl;
    }
    if (isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateClassDeclaration(cursor) != NULL) {
      return NULL;
    }
  }

  return NULL;
}

static std::string
callGraphClassDeclarationSourceFilename(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return "";
  }

  if (std::string filename = callGraphUsableDeclarationFilename(classDecl);
      !filename.empty()) {
    return filename;
  }

  if (SgClassDeclaration *firstNondef =
          isSgClassDeclaration(classDecl->get_firstNondefiningDeclaration())) {
    if (std::string filename = callGraphUsableDeclarationFilename(firstNondef);
        !filename.empty()) {
      return filename;
    }
  }

  if (SgClassDeclaration *definingDecl =
          isSgClassDeclaration(classDecl->get_definingDeclaration())) {
    if (std::string filename = callGraphUsableDeclarationFilename(definingDecl);
        !filename.empty()) {
      return filename;
    }
  }

  return "";
}

static std::string
callGraphFunctionDeclarationSourceFilename(SgFunctionDeclaration *function) {
  if (function == NULL) {
    return "";
  }

  if (std::string filename = callGraphUsableDeclarationFilename(function);
      !filename.empty()) {
    return filename;
  }
  if (SgFunctionDeclaration *first = isSgFunctionDeclaration(
          function->get_firstNondefiningDeclaration())) {
    if (std::string filename = callGraphUsableDeclarationFilename(first);
        !filename.empty()) {
      return filename;
    }
  }
  if (SgFunctionDeclaration *defining =
          isSgFunctionDeclaration(function->get_definingDeclaration())) {
    if (std::string filename = callGraphUsableDeclarationFilename(defining);
        !filename.empty()) {
      return filename;
    }
  }
  return "";
}

static std::string
callGraphInstantiatedClassPatternSourceFilename(SgClassDeclaration *classDecl) {
  SgTemplateInstantiationDecl *instDecl =
      callGraphEnclosingTemplateInstantiationDeclaration(classDecl);
  if (instDecl == NULL || instDecl->get_templateDeclaration() == NULL) {
    return "";
  }

  return callGraphClassDeclarationSourceFilename(
      isSgClassDeclaration(instDecl->get_templateDeclaration()));
}

static bool
callGraphMemberFunctionIsDefaultConstructor(SgMemberFunctionDeclaration *decl) {
  if (decl == NULL || !decl->get_specialFunctionModifier().isConstructor()) {
    return false;
  }

  return decl->get_functionModifier().isDefaultConstructor();
}

bool CallGraphBuilder::shouldMaterializeImplicitCallTarget(
    SgFunctionDeclaration *fdecl) const {
  if (fdecl == NULL) {
    return false;
  }

  SgMemberFunctionDeclaration *memberDecl =
      isSgMemberFunctionDeclaration(canonicalFunctionDeclForCallGraph(fdecl));
  if (memberDecl == NULL ||
      !callGraphMemberFunctionIsDefaultConstructor(memberDecl)) {
    return false;
  }

  Sg_File_Info *fileInfo = memberDecl->get_file_info();
  if (fileInfo == NULL ||
      (!fileInfo->isCompilerGenerated() && !fileInfo->isFrontendSpecific())) {
    return false;
  }

  const std::optional<bool> userSource =
      callGraphFunctionHasUserSourceOwnership(memberDecl);
  const auto origin = memberDecl->get_frontend_declaration_origin();
  if (userSource.has_value()) {
    if (origin == SgFunctionDeclaration::e_frontend_declaration_unclassified) {
      fprintf(stderr,
              "REX_CALLGRAPH_INVARIANT[function-declaration-origin]: "
              "producer-classified semantic constructor=%p/%s name=%s has no "
              "producer-published explicit/implicit origin\n",
              static_cast<void *>(memberDecl), memberDecl->class_name().c_str(),
              memberDecl->get_qualified_name().getString().c_str());
      ROSE_ABORT();
    }
    if (*userSource &&
        origin == SgFunctionDeclaration::e_frontend_declaration_explicit) {
      return true;
    }
    if (origin != SgFunctionDeclaration::e_frontend_declaration_explicit &&
        origin != SgFunctionDeclaration::e_frontend_declaration_implicit) {
      ROSE_ABORT();
    }
  }

  SgClassDeclaration *classDecl =
      callGraphAssociatedClassDeclaration(memberDecl);
  std::string classFilename =
      callGraphClassDeclarationSourceFilename(classDecl);
  if (classFilename.empty()) {
    classFilename = callGraphInstantiatedClassPatternSourceFilename(classDecl);
  }
  if (!callGraphProjectContainsSourceFile(project, classFilename)) {
    return false;
  }
  if (origin == SgFunctionDeclaration::e_frontend_declaration_explicit) {
    return true;
  }
  if (origin == SgFunctionDeclaration::e_frontend_declaration_implicit) {
    return !memberDecl->get_functionModifier().isTrivialSpecialMember();
  }
  if (userSource.has_value()) {
    fprintf(stderr,
            "REX_CALLGRAPH_INVARIANT[function-declaration-origin]: "
            "semantic default constructor=%p/%s name=%s has no exact "
            "explicit/implicit origin\n",
            static_cast<void *>(memberDecl), memberDecl->class_name().c_str(),
            memberDecl->get_qualified_name().getString().c_str());
    ROSE_ABORT();
  }
  return !memberDecl->get_functionModifier().isTrivialSpecialMember();
}

bool CallGraphBuilder::shouldMaterializeResolvedCallTarget(
    SgFunctionDeclaration *fdecl) const {
  fdecl = canonicalCallableFunctionDecl(fdecl);
  if (fdecl == NULL || !callGraphNodeBelongsToProject(fdecl, project)) {
    return false;
  }

  if (shouldMaterializeImplicitCallTarget(fdecl)) {
    return true;
  }

  SgMemberFunctionDeclaration *memberDecl =
      isSgMemberFunctionDeclaration(fdecl);
  if (memberDecl != NULL &&
      callGraphMemberFunctionIsDefaultConstructor(memberDecl)) {
    return false;
  }

  const std::optional<bool> userSource =
      callGraphFunctionHasUserSourceOwnership(fdecl);
  if (userSource.has_value()) {
    if (*userSource) {
      return true;
    }
    // A semantic specialization or implicit special member does not own a
    // physical function declaration, but a resolved call still has an exact
    // callable target.  Validate that target through its associated
    // class/template source owner below instead of discarding it.
  }

  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(fdecl)) {
    SgFunctionDeclaration *pattern = instantiation->get_templateDeclaration();
    if (pattern == NULL) {
      pattern = isSgFunctionDeclaration(
          instantiation->get_specializedTemplateDeclaration());
    }
    std::string patternFilename =
        pattern != NULL ? callGraphFunctionDeclarationSourceFilename(pattern)
                        : std::string();
    return !patternFilename.empty() &&
           callGraphProjectContainsSourceFile(project, patternFilename);
  }

  if (memberDecl == NULL) {
    return false;
  }

  SgClassDeclaration *associatedClass =
      callGraphAssociatedClassDeclaration(memberDecl);
  if (!isNestedInTemplateInstantiationContext(associatedClass)) {
    return false;
  }

  std::string classFilename =
      callGraphClassDeclarationSourceFilename(associatedClass);
  if (classFilename.empty()) {
    classFilename =
        callGraphInstantiatedClassPatternSourceFilename(associatedClass);
  }
  return callGraphProjectContainsSourceFile(project, classFilename);
}

static void solveVirtualFunctionCall(
    SgClassType *crtClass, ClassHierarchyWrapper *classHierarchy,
    SgMemberFunctionDeclaration *memberFunctionDeclaration,
    SgMemberFunctionDeclaration *functionDeclarationInClass,
    bool includePureVirtualFunc, std::vector<SgFunctionDeclaration *> &result) {
  ASSERT_not_null(classHierarchy);
  ASSERT_not_null(memberFunctionDeclaration);
  ASSERT_not_null(functionDeclarationInClass);

  // If it's not pure virtual then the current function declaration is a
  // candidate function to be called
  if (includePureVirtualFunc || !isPureVirtual(functionDeclarationInClass))
    result.push_back(functionDeclarationInClass);

  // Search down the class hierarchy to get all redeclarations of the current
  // member function which may be the ones being called via polymorphism.
  SgClassDefinition *crtClsDef = nullptr;

  // selecting the root of the hierarchy
  if (crtClass) {
    SgClassDeclaration *tmp = isSgClassDeclaration(crtClass->get_declaration());
    ASSERT_not_null(tmp);
    SgClassDeclaration *tmp2 =
        isSgClassDeclaration(tmp->get_definingDeclaration());
    ASSERT_not_null(tmp2);

    crtClsDef = tmp2->get_definition();
  } else {
    crtClsDef = isSgClassDefinition(memberFunctionDeclaration->get_scope());
  }

  ASSERT_not_null(crtClsDef);
  // For virtual functions, we need to search down in the hierarchy of classes
  // and retrieve all declarations of member functions with the same name and
  // type.  Names are not important for destructors.
  const ClassHierarchyWrapper::ClassDefSet &subclasses =
      classHierarchy->getSubclasses(crtClsDef);

  ASSERT_require(!memberFunctionDeclaration->get_name().is_null());
  const bool isDestructor1 =
      memberFunctionDeclaration->get_specialFunctionModifier().isDestructor();

  for (ClassHierarchyWrapper::ClassDefSet::const_iterator sci =
           subclasses.begin();
       sci != subclasses.end(); ++sci) {
    SgClassDefinition *cls = isSgClassDefinition(*sci);
    for (auto *cls_mb_decl : classMemberFunctions(cls)) {
      ASSERT_require(!cls_mb_decl->get_name().is_null());
      const bool isDestructor2 =
          cls_mb_decl->get_specialFunctionModifier().isDestructor();
      const bool keep =
          ((isDestructor1 && isDestructor2) ||
           isOverridingFunction(cls_mb_decl, memberFunctionDeclaration));

      if (keep) {
        SgMemberFunctionDeclaration *nonDefDecl = isSgMemberFunctionDeclaration(
            cls_mb_decl->get_firstNondefiningDeclaration());
        SgMemberFunctionDeclaration *defDecl = isSgMemberFunctionDeclaration(
            cls_mb_decl->get_definingDeclaration());

        // MD 2010/07/08 defDecl might be NULL
        SgMemberFunctionDeclaration *candidate =
            nonDefDecl ? nonDefDecl : defDecl;
        ASSERT_not_null(candidate);

        if (includePureVirtualFunc || !isPureVirtual(candidate))
          result.push_back(candidate);
      }
    }
  }
}

std::vector<SgFunctionDeclaration *> CallTargetSet::solveMemberFunctionCall(
    SgClassType *crtClass, ClassHierarchyWrapper *classHierarchy,
    SgMemberFunctionDeclaration *memberFunctionDeclaration, bool polymorphic,
    bool includePureVirtualFunc) {
  ASSERT_not_null(memberFunctionDeclaration);

  std::vector<SgFunctionDeclaration *> functionList;

  SgMemberFunctionDeclaration *functionDeclarationInClass = NULL;
  if (SgDeclarationStatement *nonDefDeclInClass =
          memberFunctionDeclaration->get_firstNondefiningDeclaration()) {
    // memberFunctionDeclaration is outside the class
    functionDeclarationInClass =
        isSgMemberFunctionDeclaration(nonDefDeclInClass);
  } else {
    // In class declaration, since there is no non-defining declaration
    functionDeclarationInClass = memberFunctionDeclaration;
  }
  ASSERT_not_null(functionDeclarationInClass);

  if (functionDeclarationInClass->get_functionModifier().isVirtual() &&
      polymorphic) {
    solveVirtualFunctionCall(
        crtClass, classHierarchy, memberFunctionDeclaration,
        functionDeclarationInClass, includePureVirtualFunc, functionList);
  } else {
    functionList.push_back(functionDeclarationInClass);
  }
  return functionList;
}

static SgType *stripCallableValueWrappers(SgType *type) {
  for (;;) {
    if (SgTypedefType *wrapper = isSgTypedefType(type)) {
      type = wrapper->get_base_type();
    } else if (SgModifierType *wrapper = isSgModifierType(type)) {
      type = wrapper->get_base_type();
    } else if (SgReferenceType *wrapper = isSgReferenceType(type)) {
      type = wrapper->get_base_type();
    } else if (SgRvalueReferenceType *wrapper = isSgRvalueReferenceType(type)) {
      type = wrapper->get_base_type();
    } else {
      return type;
    }
    if (type == NULL) {
      return NULL;
    }
  }
}

static SgFunctionType *callableIndirectSignature(SgType *type) {
  type = stripCallableValueWrappers(type);
  if (SgFunctionType *function = isSgFunctionType(type)) {
    // Fortran dummy procedures and procedure variables carry the callable
    // signature directly.  C and C++ function-pointer variables wrap the same
    // signature in SgPointerType.
    return function;
  }
  SgPointerType *pointer = isSgPointerType(type);
  if (pointer == NULL) {
    return NULL;
  }
  return isSgFunctionType(stripCallableValueWrappers(pointer->get_base_type()));
}

static void appendFunctionPointerSignatureTargets(
    SgExpression *expression, const char *context,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList) {
  ASSERT_not_null(expression);
  SgProject *project = SageInterface::getProject(expression);
  ASSERT_not_null(project);
  SgFunctionType *signature = callableIndirectSignature(expression->get_type());
  if (signature == NULL) {
    std::cerr << "REX_CALLGRAPH_INVARIANT[indirect-call-type]: context="
              << (context != NULL ? context : "<null>")
              << " callee does not have one exact function-pointer signature"
              << std::endl;
    ROSE_ABORT();
  }

  VariantVector variants;
  variants.push_back(V_SgFunctionDeclaration);
  variants.push_back(V_SgTemplateInstantiationFunctionDecl);
  variants.push_back(V_SgMemberFunctionDeclaration);
  variants.push_back(V_SgTemplateInstantiationMemberFunctionDecl);
  Rose_STL_Container<SgFunctionDeclaration *> matches =
      AstQueryNamespace::queryMemoryPool(
          [project, signature](SgNode *node) {
            if (!callGraphNodeBelongsToProject(node, project))
              return Rose_STL_Container<SgFunctionDeclaration *>();
            return CallTargetSet::solveFunctionPointerCallsFunctional(
                node, signature);
          },
          &variants);
  functionList.insert(functionList.end(), matches.begin(), matches.end());
}

std::vector<SgFunctionDeclaration *> CallTargetSet::solveConstructorInitializer(
    SgConstructorInitializer *sgCtorInit) {
  ASSERT_not_null(sgCtorInit);
  auto *constructor = sgCtorInit->get_declaration();
  if (constructor == nullptr)
    return {};
  return {canonicalFunctionDeclForCallGraph(constructor)};
}

/** Add the declaration for functionCallExp to functionList. In the case of
 * function pointers and virtual functions, append the set of declarations to
 * functionList. */
SgExpression *
CallTargetSet::unwrapExactImplicitCalleeConversions(SgExpression *functionExp) {
  ASSERT_not_null(functionExp);

  while (SgCommaOpExp *comma = isSgCommaOpExp(functionExp)) {
    if (comma->get_lhs_operand() == NULL ||
        comma->get_lhs_operand()->get_parent() != comma ||
        comma->get_rhs_operand() == NULL ||
        comma->get_rhs_operand()->get_parent() != comma) {
      std::cerr << "REX_CALLGRAPH_INVARIANT[callee-comma]: comma callee has "
                   "no exact owned left and right operands"
                << std::endl;
      ROSE_ABORT();
    }
    functionExp = comma->get_rhs_operand();
  }

  while (SgAddressOfOp *address_of = isSgAddressOfOp(functionExp)) {
    if (address_of->get_operand() == NULL ||
        address_of->get_operand()->get_parent() != address_of) {
      std::cerr << "REX_CALLGRAPH_INVARIANT[callee-address]: address-of "
                   "callee has no exact owned operand"
                << std::endl;
      ROSE_ABORT();
    }
    functionExp = address_of->get_operand();
  }

  while (SgCastExp *cast = isSgCastExp(functionExp)) {
    cast->validate_semantic_conversion();
    if (cast->get_cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }

    const SgCastExp::semantic_conversion_kind_enum conversion_kind =
        cast->get_semantic_conversion_kind();
    switch (conversion_kind) {
    case SgCastExp::e_semantic_conversion_FunctionToPointerDecay: {
      SgPointerType *target_pointer =
          isSgPointerType(cast->get_type()->stripTypedefsAndModifiers());
      SgType *operand_type = cast->get_operand()->get_type();
      SgType *target_callable_type =
          target_pointer != NULL && target_pointer->get_base_type() != NULL
              ? target_pointer->get_base_type()->stripTypedefsAndModifiers()
              : NULL;
      SgType *operand_callable_type =
          operand_type != NULL ? operand_type->stripTypedefsAndModifiers()
                               : NULL;
      SgFunctionType *target_function = isSgFunctionType(target_callable_type);
      SgFunctionType *operand_function =
          isSgFunctionType(operand_callable_type);
      SgMemberFunctionType *operand_member_function =
          isSgMemberFunctionType(operand_callable_type);
      if (target_function == NULL || operand_function == NULL ||
          !is_callable_signatures_equal(target_function, operand_function,
                                        operand_member_function != NULL)) {
        std::cerr
            << "REX_CALLGRAPH_INVARIANT[callee-conversion]: function-to-"
               "pointer decay does not preserve one exact callable signature"
            << std::endl;
        ROSE_ABORT();
      }
      if (operand_member_function != NULL) {
        SgMemberFunctionDeclaration *member =
            resolveMemberFunctionDeclarationFromExpression(cast->get_operand());
        SgMemberFunctionDeclaration *first = isSgMemberFunctionDeclaration(
            member != NULL ? member->get_firstNondefiningDeclaration() : NULL);
        const bool exact_family =
            member != NULL && first != NULL &&
            first->get_firstNondefiningDeclaration() == first &&
            member->get_name() == first->get_name() &&
            member->get_type() == first->get_type();
        const bool static_family =
            exact_family &&
            first->get_declarationModifier().get_storageModifier().isStatic();
        if (!static_family) {
          std::cerr
              << "REX_CALLGRAPH_INVARIANT[callee-conversion]: member "
                 "function-to-pointer decay does not name one exact static "
                 "member declaration (operand="
              << cast->get_operand() << "/" << cast->get_operand()->class_name()
              << " parent=" << cast->get_operand()->get_parent()
              << " member=" << member << " name="
              << (member != NULL ? member->get_name().getString()
                                 : std::string("<null>"))
              << " first=" << first << " exact-family=" << exact_family
              << " static-family=" << static_family
              << " operand-type=" << operand_callable_type << "/"
              << (operand_callable_type != NULL
                      ? operand_callable_type->class_name()
                      : std::string("<null>"))
              << " target-type=" << target_callable_type << "/"
              << (target_callable_type != NULL
                      ? target_callable_type->class_name()
                      : std::string("<null>"))
              << ")" << std::endl;
          ROSE_ABORT();
        }
      }
      break;
    }

    case SgCastExp::e_semantic_conversion_LValueToRValue:
    case SgCastExp::e_semantic_conversion_NoOp:
      if (cast->get_operand()->get_type() == NULL ||
          SageInterface::containsUnknownType(cast->get_operand()->get_type()) ||
          SageInterface::containsUnknownType(cast->get_type())) {
        std::cerr << "REX_CALLGRAPH_INVARIANT[callee-conversion]: transparent "
                     "callee conversion has no exact source/result type"
                  << std::endl;
        ROSE_ABORT();
      }
      break;

    case SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr: {
      SgType *source_type =
          cast->get_operand()->get_type()->stripTypedefsAndModifiers();
      SgType *result_type = cast->get_type()->stripTypedefsAndModifiers();
      SgPointerType *result_pointer = isSgPointerType(result_type);
      SgFunctionType *source_function = isSgFunctionType(source_type);
      SgFunctionType *result_function =
          result_pointer != NULL && result_pointer->get_base_type() != NULL
              ? isSgFunctionType(result_pointer->get_base_type()
                                     ->stripTypedefsAndModifiers())
              : isSgFunctionType(result_type);
      if (source_function == NULL || result_function == NULL ||
          !is_callable_signatures_equal(result_function, source_function,
                                        false)) {
        std::cerr << "REX_CALLGRAPH_INVARIANT[callee-conversion]: builtin "
                     "callee conversion does not preserve one exact "
                     "function or function-pointer signature (cast="
                  << cast << " operand=" << cast->get_operand() << "/"
                  << cast->get_operand()->class_name()
                  << " source=" << source_type << "/"
                  << (source_type != NULL ? source_type->class_name()
                                          : std::string("<null>"))
                  << " source-dependent="
                  << (source_function != NULL
                          ? callGraphTypeContainsDependentTemplateType(
                                source_function)
                          : false)
                  << " source-return="
                  << (source_function != NULL
                          ? source_function->get_return_type()
                          : NULL)
                  << " source-args="
                  << (source_function != NULL
                          ? source_function->get_arguments().size()
                          : 0)
                  << " result=" << result_type << "/"
                  << (result_type != NULL ? result_type->class_name()
                                          : std::string("<null>"))
                  << " result-dependent="
                  << (result_function != NULL
                          ? callGraphTypeContainsDependentTemplateType(
                                result_function)
                          : false)
                  << " result-return="
                  << (result_function != NULL
                          ? result_function->get_return_type()
                          : NULL)
                  << " result-args="
                  << (result_function != NULL
                          ? result_function->get_arguments().size()
                          : 0)
                  << ")" << std::endl;
        ROSE_ABORT();
      }
    } break;

    default:
      std::cerr << "REX_CALLGRAPH_INVARIANT[callee-conversion]: unsupported "
                   "callee conversion kind="
                << static_cast<int>(conversion_kind) << std::endl;
      ROSE_ABORT();
    }
    functionExp = cast->get_operand();
  }

  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(functionExp)) {
    // Macro spelling is the lexical surface.  Call-target analysis follows
    // the exact typed expression owned by that surface, then applies the same
    // conversion validation recursively because either side of an implicit
    // conversion can own the macro invocation.
    return unwrapExactImplicitCalleeConversions(
        macro->get_expanded_expression_checked());
  }

  return functionExp;
}

void getPropertiesForSgFunctionCallExp(
    SgFunctionCallExp *sgFunCallExp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList,
    bool includePureVirtualFunc = false) {
  SgExpression *functionExp =
      CallTargetSet::unwrapExactImplicitCalleeConversions(
          sgFunCallExp->get_function());
  while (SgPointerDerefExp *dereference = isSgPointerDerefExp(functionExp)) {
    SgExpression *operand = dereference->get_operand_i();
    if (operand == nullptr || operand->get_parent() != dereference) {
      std::cerr << "REX_CALLGRAPH_INVARIANT[callee-dereference]: pointer "
                   "dereference callee has no exact owned operand"
                << std::endl;
      ROSE_ABORT();
    }
    functionExp = CallTargetSet::unwrapExactImplicitCalleeConversions(operand);
  }

  switch (functionExp->variantT()) {
  case V_SgArrowStarOp:
  case V_SgDotStarOp: {
    std::vector<SgFunctionDeclaration *> fD =
        CallTargetSet::solveMemberFunctionPointerCall(functionExp,
                                                      classHierarchy);
    functionList.insert(functionList.end(), fD.begin(), fD.end());
    break;
  }

  case V_SgDotExp:
  case V_SgArrowExp: {
    auto *access = isSgBinaryOp(functionExp);
    ASSERT_not_null(access);
    SgExpression *memberRefExp = access->get_rhs_operand();
    ASSERT_not_null(memberRefExp);
    if (isSgVarRefExp(memberRefExp) != nullptr) {
      appendFunctionPointerSignatureTargets(functionExp, "member field",
                                            functionList);
      break;
    }
    if (auto *qualified = isSgScopeOp(memberRefExp)) {
      if (auto *pseudo =
              isSgPseudoDestructorRefExp(qualified->get_rhs_operand())) {
        auto *qualifier = isSgTypeExpression(qualified->get_lhs_operand());
        ASSERT_not_null(qualifier);
        ROSE_ASSERT(qualified->get_parent() == access);
        ROSE_ASSERT(qualifier->get_parent() == qualified);
        ROSE_ASSERT(pseudo->get_parent() == qualified);
        ROSE_ASSERT(qualified->get_type() == pseudo->get_type());
        ROSE_ASSERT(access->get_type() == pseudo->get_type());
        memberRefExp = pseudo;
      }
    }
    if (isSgPseudoDestructorRefExp(memberRefExp) != nullptr) {
      ROSE_ASSERT(sgFunCallExp->get_args()->get_expressions().empty());
      break;
    }
    if (auto *reference = isSgFunctionRefExp(memberRefExp)) {
      auto *member = isSgMemberFunctionDeclaration(
          reference->getAssociatedFunctionDeclaration());
      ASSERT_not_null(member);
      ROSE_ASSERT(
          member->get_declarationModifier().get_storageModifier().isStatic());
      auto *target = canonicalCallableFunctionDecl(member);
      ASSERT_not_null(target);
      functionList.push_back(target);
      break;
    }

    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        resolveMemberFunctionDeclarationFromExpression(memberRefExp);
    ASSERT_not_null(memberFunctionDeclaration);
    SgType *receiverType = receiverBindingType(access->get_lhs_operand());
    ASSERT_not_null(receiverType);
    SgClassType *receiverClass = receiverClassType(receiverType);
    ASSERT_not_null(receiverClass);

    SgType *receiverBaseType = receiverType->stripTypedefsAndModifiers();
    auto *receiverCast = isSgCastExp(access->get_lhs_operand());
    const bool referenceCast =
        receiverCast != nullptr && (receiverCast->get_value_category() ==
                                        SgCastExp::e_value_category_lvalue ||
                                    receiverCast->get_value_category() ==
                                        SgCastExp::e_value_category_xvalue);
    const bool polymorphicType = isSgPointerType(receiverBaseType) ||
                                 isSgReferenceType(receiverBaseType) ||
                                 isSgRvalueReferenceType(receiverBaseType) ||
                                 isSgArrayType(receiverBaseType) ||
                                 referenceCast;
    const bool polymorphicCall =
        polymorphicType && !memberFunctionRefNeedsQualifier(memberRefExp);
    std::vector<SgFunctionDeclaration *> targets =
        CallTargetSet::solveMemberFunctionCall(
            receiverClass, classHierarchy, memberFunctionDeclaration,
            polymorphicCall, includePureVirtualFunc);
    functionList.insert(functionList.end(), targets.begin(), targets.end());
    break;
  }

  case V_SgMemberFunctionRefExp:
  case V_SgTemplateMemberFunctionRefExp:
  case V_SgFunctionRefExp:
  case V_SgTemplateFunctionRefExp:
  case V_SgNonrealRefExp: {
    if (SgNonrealRefExp *dependent = isSgNonrealRefExp(functionExp)) {
      if (dependent->get_resolved_function_declaration() == nullptr &&
          dependent->get_semantic_role() ==
              SgNonrealRefExp::e_dependent_callable) {
        SgNonrealSymbol *symbol = dependent->get_symbol();
        SgNonrealDecl *declaration =
            symbol != nullptr ? symbol->get_declaration() : nullptr;
        if (dependent->get_resolved_variable_declaration() != nullptr ||
            symbol == nullptr || declaration == nullptr ||
            symbol->get_symbol_basis() != declaration ||
            isSgNonrealType(dependent->get_type()) == nullptr) {
          std::cerr << "REX_CALLGRAPH_INVARIANT[dependent-callable]: reference="
                    << dependent
                    << " does not have one exact unresolved callable identity"
                    << std::endl;
          ROSE_ABORT();
        }

        // Without a concrete instantiation, ADL intentionally leaves no exact
        // target. This is an explicitly represented dependent call, not a
        // failed direct-call lookup.
        break;
      }
    }

    if (SgMemberFunctionDeclaration *memberFunctionDeclaration =
            resolveMemberFunctionDeclarationFromExpression(functionExp)) {
      SgClassType *crtClass = NULL;
      if (SgMemberFunctionDeclaration *enclosingMember =
              enclosingMemberFunctionDeclaration(functionExp)) {
        crtClass = resolveClassTypeFromMemberDecl(enclosingMember);
      }
      if (crtClass == NULL) {
        crtClass = resolveClassTypeFromMemberDecl(memberFunctionDeclaration);
      }

      const bool polymorphicCall =
          !memberFunctionRefNeedsQualifier(functionExp);

      std::vector<SgFunctionDeclaration *> fD =
          CallTargetSet::solveMemberFunctionCall(
              crtClass, classHierarchy, memberFunctionDeclaration,
              polymorphicCall, includePureVirtualFunc);
      functionList.insert(functionList.end(), fD.begin(), fD.end());
      break;
    }

    SgFunctionDeclaration *fctDecl = NULL;
    if (SgFunctionRefExp *reference = isSgFunctionRefExp(functionExp)) {
      fctDecl = reference->getAssociatedFunctionDeclaration();
    } else if (SgTemplateFunctionRefExp *reference =
                   isSgTemplateFunctionRefExp(functionExp)) {
      fctDecl = reference->getAssociatedFunctionDeclaration();
    } else if (SgNonrealRefExp *nonrealRef = isSgNonrealRefExp(functionExp)) {
      fctDecl = nonrealRef->get_resolved_function_declaration();
    }

    SgFunctionDeclaration *resolvedTemplateCallable = fctDecl;
    fctDecl = canonicalCallableFunctionDecl(fctDecl);
    if (fctDecl == NULL && isSgTemplateFunctionRefExp(functionExp) != nullptr &&
        isSgTemplateFunctionDeclaration(resolvedTemplateCallable) != nullptr) {
      // A source-only template pattern has no concrete call-graph vertex.  A
      // deduced call carries its selected specialization through the
      // reference's typed semantic edge and therefore does not take this
      // branch.
      break;
    }
    if (fctDecl == NULL) {
      SgSymbol *symbol = nullptr;
      SgFunctionDeclaration *resolved = nullptr;
      if (SgMemberFunctionRefExp *reference =
              isSgMemberFunctionRefExp(functionExp)) {
        symbol = reference->get_symbol();
        resolved = reference->getAssociatedMemberFunctionDeclaration();
      } else if (SgTemplateFunctionRefExp *reference =
                     isSgTemplateFunctionRefExp(functionExp)) {
        symbol = reference->get_symbol();
        resolved = reference->get_symbol() != nullptr
                       ? reference->get_symbol()->get_declaration()
                       : nullptr;
      } else if (SgTemplateMemberFunctionRefExp *reference =
                     isSgTemplateMemberFunctionRefExp(functionExp)) {
        symbol = reference->get_symbol();
        resolved = reference->getAssociatedMemberFunctionDeclaration();
      } else if (SgNonrealRefExp *reference = isSgNonrealRefExp(functionExp)) {
        symbol = reference->get_symbol();
        resolved = reference->get_resolved_function_declaration();
      }
      std::cerr << "REX_CALLGRAPH_INVARIANT[direct-function-association]: "
                   "direct function reference="
                << functionExp << "/" << functionExp->class_name()
                << " symbol=" << symbol << "/"
                << (symbol != nullptr ? symbol->class_name() : "<null>")
                << " raw-declaration=" << resolved << "/"
                << (resolved != nullptr ? resolved->class_name() : "<null>")
                << " associated=" << resolvedTemplateCallable << "/"
                << (resolvedTemplateCallable != nullptr
                        ? resolvedTemplateCallable->class_name()
                        : "<null>")
                << " associated-name="
                << (resolvedTemplateCallable != nullptr
                        ? resolvedTemplateCallable->get_name().getString()
                        : std::string("<null>"))
                << " associated-definition="
                << (resolvedTemplateCallable != nullptr
                        ? resolvedTemplateCallable->get_definition()
                        : NULL)
                << " associated-defining="
                << (resolvedTemplateCallable != nullptr
                        ? resolvedTemplateCallable->get_definingDeclaration()
                        : NULL)
                << " associated-dependent-type="
                << (resolvedTemplateCallable != nullptr &&
                            callGraphTypeContainsDependentTemplateType(
                                resolvedTemplateCallable->get_type())
                        ? 1
                        : 0)
                << " parent=" << functionExp->get_parent() << "/"
                << (functionExp->get_parent() != nullptr
                        ? functionExp->get_parent()->class_name()
                        : "<null>")
                << " source="
                << functionExp->get_file_info()->get_filenameString() << ":"
                << functionExp->get_file_info()->get_line() << ":"
                << functionExp->get_file_info()->get_col()
                << " type=" << functionExp->get_type() << "/"
                << (functionExp->get_type() != nullptr
                        ? functionExp->get_type()->class_name()
                        : "<null>")
                << " has no exact associated declaration" << std::endl;
      ROSE_ABORT();
    }

    functionList.push_back(fctDecl);
    break;
  }

  case V_SgVarRefExp: {
    // This is an indirect function call, as in:
    //    |void f() {
    //    |    void (*g)();
    //    |    g();              <------------
    //    |}
    // We don't know what is being called, only its type.  So assume that all
    // functions whose type matches could be called. [Robb P. Matzke 2013-01-24]
    appendFunctionPointerSignatureTargets(functionExp, "variable",
                                          functionList);
    break;
  }

  case V_SgTemplateParameterVal: {
    // Calls through non-type template parameters (e.g. `FUNC();` inside
    // `template<void (*FUNC)()>`) are indirect calls whose concrete target is
    // not always encoded as a direct function-ref in the generic AST. Resolve
    // them conservatively from the parameter's callable type, just like we do
    // for ordinary function-pointer calls.
    appendFunctionPointerSignatureTargets(
        functionExp, "non-type template parameter", functionList);
    break;
  }

  case V_SgPntrArrRefExp:
    appendFunctionPointerSignatureTargets(functionExp, "indexed expression",
                                          functionList);
    break;

  case V_SgCastExp: {
    SgCastExp *cast = isSgCastExp(functionExp);
    if (cast->get_cast_type() == SgCastExp::e_implicit_cast ||
        cast->get_operand() == NULL ||
        cast->get_operand()->get_parent() != cast) {
      std::cerr << "REX_CALLGRAPH_INVARIANT[callee-conversion]: explicit "
                   "callee cast has malformed syntax/ownership state"
                << std::endl;
      ROSE_ABORT();
    }
    appendFunctionPointerSignatureTargets(cast, "explicit cast", functionList);
    break;
  }

  case V_SgConstructorInitializer: {
    std::vector<SgFunctionDeclaration *> constructors =
        CallTargetSet::solveConstructorInitializer(
            isSgConstructorInitializer(functionExp));
    functionList.insert(functionList.end(), constructors.begin(),
                        constructors.end());
    break;
  }

  case V_SgFunctionCallExp: {
    appendFunctionPointerSignatureTargets(functionExp, "nested call result",
                                          functionList);
    break;
  }

  default: {
    Sg_File_Info *source = functionExp->get_file_info();
    std::cerr << "REX_CALLGRAPH_INVARIANT[callee-expression-kind]: call="
              << sgFunCallExp << " callee=" << functionExp << "/"
              << functionExp->class_name()
              << " variant=" << static_cast<int>(functionExp->variantT())
              << " type=" << functionExp->get_type() << "/"
              << (functionExp->get_type() != NULL
                      ? functionExp->get_type()->class_name()
                      : std::string("<null>"))
              << " parent=" << functionExp->get_parent() << "/"
              << (functionExp->get_parent() != NULL
                      ? functionExp->get_parent()->class_name()
                      : std::string("<null>"))
              << " source="
              << (source != NULL ? source->get_filenameString()
                                 : std::string("<null>"))
              << ":" << (source != NULL ? source->get_line() : 0) << ":"
              << (source != NULL ? source->get_col() : 0)
              << " has no exact call-target dispatch" << std::endl;
    ROSE_ABORT();
  }
  }
}
void CallTargetSet::getPropertiesForExpression(
    SgExpression *sgexp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &functionList,
    bool includePureVirtualFunc) {
  const auto firstTarget = functionList.size();
  if (SgFunctionCallExp *fncall = isSgFunctionCallExp(sgexp)) {
    getPropertiesForSgFunctionCallExp(fncall, classHierarchy, functionList,
                                      includePureVirtualFunc);
  } else if (auto *destruction = isSgCxxSubobjectDestruction(sgexp)) {
    destruction->validate();
    functionList.push_back(destruction->get_destructor_declaration());
  } else if (SgConstructorInitializer *ctorini =
                 isSgConstructorInitializer(sgexp)) {
    const auto constructors = solveConstructorInitializer(ctorini);
    functionList.insert(functionList.end(), constructors.begin(),
                        constructors.end());
  } else if (isSgPntrArrRefExp(sgexp) != NULL ||
             isSgPointerDerefExp(sgexp) != NULL) {
    // Built-in subscripts/dereferences are not calls.
  } else {
    std::cerr << "Cannot determine Properties for " << sgexp->class_name()
              << std::endl;
  }
  for (auto i = firstTarget; i < functionList.size(); ++i)
    functionList[i] = canonicalFunctionDeclForCallGraph(functionList[i]);
}

void CallTargetSet::getDeclarationsForExpression(
    SgExpression *exp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &defList,
    bool includePureVirtualFunc) {
  Rose_STL_Container<SgFunctionDeclaration *> props;
  CallTargetSet::getPropertiesForExpression(exp, classHierarchy, props,
                                            includePureVirtualFunc);

  for (SgFunctionDeclaration *candidateDecl : props) {
    candidateDecl = canonicalCallableFunctionDecl(candidateDecl);
    if (candidateDecl != NULL) {
      defList.push_back(candidateDecl);
    }
  }
}

void CallTargetSet::getDefinitionsForExpression(
    SgExpression *sgexp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDefinition *> &defList) {
  if (SgFunctionDefinition *owner =
          SageInterface::getEnclosingProcedure(sgexp)) {
    if (canonicalCallableFunctionDecl(owner->get_declaration()) == NULL)
      return;
  }
  Rose_STL_Container<SgFunctionDeclaration *> props;
  CallTargetSet::getPropertiesForExpression(sgexp, classHierarchy, props);

  for (SgFunctionDeclaration *candidateDecl : props) {
    candidateDecl = canonicalCallableFunctionDecl(candidateDecl);
    if (candidateDecl == NULL) {
      continue;
    }
    candidateDecl =
        isSgFunctionDeclaration(candidateDecl->get_definingDeclaration());
    if (candidateDecl != NULL) {
      SgFunctionDefinition *candidateDef = candidateDecl->get_definition();
      if (candidateDef != NULL) {
        defList.push_back(candidateDef);
      }
    }
  }
}

bool CallTargetSet::isNonExecutableCallExpression(SgExpression *expression) {
  ASSERT_not_null(expression);
  for (SgNode *child = expression, *owner = child->get_parent();
       owner != NULL && isSgFunctionDefinition(owner) == NULL;
       child = owner, owner = owner->get_parent()) {
    if (isSgDeclType(owner) != nullptr || isSgNoexceptOp(owner) != nullptr ||
        isSgAlignOfOp(owner) != nullptr)
      return true;
    if (auto *size_of = isSgSizeOfOp(owner)) {
      if (size_of->get_operand_expr() == child &&
          !size_of->expression_operand_is_evaluated())
        return true;
    }
    if (auto *type_id = isSgTypeIdOp(owner)) {
      (void)type_id->get_type();
      if (!type_id->get_operand_is_evaluated())
        return true;
    }
    if (SgInitializedName *declaration = isSgInitializedName(owner)) {
      if (declaration->get_initializer() != child)
        continue;
      if (isSgFunctionParameterList(declaration->get_parent()) != NULL)
        return true;
      if (auto *variable = isSgVariableDeclaration(declaration->get_parent())) {
        if (isSgClassDefinition(variable->get_scope()) != NULL &&
            !variable->get_declarationModifier()
                 .get_storageModifier()
                 .isStatic())
          return true;
      }
    }
  }
  return false;
}

namespace {

using DefinitionExpressionIndex =
    std::map<SgFunctionDefinition *, Rose_STL_Container<SgExpression *>>;

} // namespace

struct SgProject::CallGraphState {
  std::recursive_mutex mutex;
  DefinitionExpressionIndex index;
  bool initialized = false;
  uint64_t astModificationSequence = 0;
  const ClassHierarchyWrapper *classHierarchy = nullptr;
};

SgProject::CallGraphState &SgProject::get_callGraphState() {
  static std::mutex install_mutex;
  std::lock_guard<std::mutex> install_guard(install_mutex);
  if (p_callGraphState == nullptr)
    p_callGraphState = new CallGraphState;
  return *p_callGraphState;
}

void SgProject::release_callGraphState() {
  delete p_callGraphState;
  p_callGraphState = nullptr;
}

namespace {

static void
indexDefinitionExpression(SgExpression *exp,
                          ClassHierarchyWrapper *classHierarchy,
                          DefinitionExpressionIndex &definitionIndex) {
  ASSERT_not_null(exp);
  ASSERT_not_null(classHierarchy);

  Rose_STL_Container<SgFunctionDefinition *> candidateDefs;
  CallTargetSet::getDefinitionsForExpression(exp, classHierarchy,
                                             candidateDefs);

  std::set<SgFunctionDefinition *> uniqueCandidateDefs;
  for (SgFunctionDefinition *candidateDef : candidateDefs) {
    if (candidateDef == NULL ||
        !uniqueCandidateDefs.insert(candidateDef).second) {
      continue;
    }

    definitionIndex[candidateDef].push_back(exp);
  }
}

class DefinitionExpressionIndexBuilder : public AstSimpleProcessing {
public:
  DefinitionExpressionIndexBuilder(ClassHierarchyWrapper *classHierarchy,
                                   DefinitionExpressionIndex &definitionIndex)
      : classHierarchy_(classHierarchy), definitionIndex_(definitionIndex) {
    ASSERT_not_null(classHierarchy_);
  }

  void visit(SgNode *node) override {
    SgExpression *expression = isSgExpression(node);
    if (isSgFunctionCallExp(expression) != NULL ||
        isSgConstructorInitializer(expression) != NULL ||
        isSgCxxSubobjectDestruction(expression) != nullptr) {
      if (CallTargetSet::isNonExecutableCallExpression(expression))
        return;
      indexDefinitionExpression(expression, classHierarchy_, definitionIndex_);
    }
  }

private:
  ClassHierarchyWrapper *classHierarchy_ = nullptr;
  DefinitionExpressionIndex &definitionIndex_;
};

static void
buildDefinitionExpressionIndex(SgProject *project,
                               ClassHierarchyWrapper *classHierarchy,
                               DefinitionExpressionIndex &definitionIndex) {
  ASSERT_not_null(project);
  ASSERT_not_null(classHierarchy);

  definitionIndex.clear();

  DefinitionExpressionIndexBuilder builder(classHierarchy, definitionIndex);
  builder.traverse(project, preorder);
}

} // namespace

std::vector<SgFunctionDeclaration *>
CallGraphBuilder::collectNonFunctionConstructorTargets(
    ClassHierarchyWrapper *classHierarchy) {
  ASSERT_not_null(classHierarchy);
  std::vector<SgFunctionDeclaration *> targets;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgConstructorInitializer)) {
    auto *expression = isSgConstructorInitializer(node);
    ASSERT_not_null(expression);
    if (SageInterface::getEnclosingFunctionDefinition(expression, false) !=
            nullptr ||
        CallTargetSet::isNonExecutableCallExpression(expression)) {
      continue;
    }
    CallTargetSet::getPropertiesForExpression(expression, classHierarchy,
                                              targets);
  }
  return targets;
}

void CallTargetSet::getExpressionsForDefinition(
    SgFunctionDefinition *targetDef, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgExpression *> &exps) {
  ASSERT_not_null(targetDef);
  ASSERT_not_null(classHierarchy);

  SgProject *project = SageInterface::getProject(targetDef);
  ASSERT_not_null(project);

  auto &state = project->get_callGraphState();
  std::lock_guard<std::recursive_mutex> index_guard(state.mutex);
  uint64_t currentSequence = SgNode::get_globalAstModificationSequence();
  if (!state.initialized || state.astModificationSequence != currentSequence ||
      state.classHierarchy != classHierarchy) {
    buildDefinitionExpressionIndex(project, classHierarchy, state.index);
    state.initialized = true;
    state.astModificationSequence = currentSequence;
    state.classHierarchy = classHierarchy;
  }

  const auto expressions = state.index.find(targetDef);
  if (expressions != state.index.end())
    exps.insert(exps.end(), expressions->second.begin(),
                expressions->second.end());
}

FunctionData::FunctionData(SgFunctionDeclaration *inputFunctionDeclaration,
                           SgProject *project,
                           ClassHierarchyWrapper *classHierarchy) {
  hasDefinition = false;

  functionDeclaration = canonicalCallableFunctionDecl(inputFunctionDeclaration);
  ASSERT_not_null(functionDeclaration);

  SgFunctionDeclaration *defDecl =
      (inputFunctionDeclaration->get_definition() != NULL
           ? inputFunctionDeclaration
           : isSgFunctionDeclaration(
                 functionDeclaration->get_definingDeclaration()));

  if (defDecl != NULL) {
    ASSERT_not_null(defDecl->get_definition());
    hasDefinition = true;

    for (VariantT kind : {V_SgFunctionCallExp, V_SgConstructorInitializer,
                          V_SgCxxSubobjectDestruction}) {
      for (SgNode *node : NodeQuery::querySubTree(defDecl, kind)) {
        SgExpression *expression = isSgExpression(node);
        ASSERT_not_null(expression);
        if (SageInterface::getEnclosingFunctionDeclaration(expression) !=
                defDecl ||
            CallTargetSet::isNonExecutableCallExpression(expression))
          continue;
        CallTargetSet::getPropertiesForExpression(expression, classHierarchy,
                                                  functionList);
      }
    }
  }
}
SgFunctionDeclaration *
CallTargetSet::getFirstVirtualFunctionDefinitionFromAncestors(
    SgClassType *crtClass,
    SgMemberFunctionDeclaration *memberFunctionDeclaration,
    ClassHierarchyWrapper *classHierarchy) {

  ASSERT_not_null(memberFunctionDeclaration);
  ASSERT_not_null(classHierarchy);

  SgFunctionDeclaration *resultDecl = NULL;

  SgDeclarationStatement *nonDefDeclInClass = NULL;

  nonDefDeclInClass =
      memberFunctionDeclaration->get_firstNondefiningDeclaration();
  SgMemberFunctionDeclaration *functionDeclarationInClass = NULL;

  // memberFunctionDeclaration is outside the class
  if (nonDefDeclInClass) {
    functionDeclarationInClass =
        isSgMemberFunctionDeclaration(nonDefDeclInClass);
  }
  // in class declaration, since there is no non-defining declaration
  else {
    functionDeclarationInClass = memberFunctionDeclaration;
  }

  ASSERT_not_null(functionDeclarationInClass);

  if (!(functionDeclarationInClass->get_functionModifier().isVirtual()))
    return NULL;
  // for virtual functions, we need to search down in the hierarchy of classes
  // and retrieve all declarations of member functions with the same type

  SgClassDefinition *crtClsDef = nullptr;

  if (crtClass) {
    SgClassDeclaration *tmp = isSgClassDeclaration(crtClass->get_declaration());
    ASSERT_not_null(tmp);
    SgClassDeclaration *tmp2 =
        isSgClassDeclaration(tmp->get_definingDeclaration());
    ASSERT_not_null(tmp2);
    crtClsDef = tmp2->get_definition();
  } else {
    crtClsDef = isSgClassDefinition(memberFunctionDeclaration->get_scope());
  }

  ASSERT_not_null(crtClsDef);

  functionDeclarationInClass = NULL;
  vector<SgClassDefinition *> worklist;
  worklist.push_back(crtClsDef);

  while (!worklist.empty()) {
    SgClassDefinition *ancestor = worklist.back();
    worklist.pop_back();
    SgClassDefinition *cls = isSgClassDefinition(ancestor);
    resultDecl = is_function_exists(cls, memberFunctionDeclaration);
    if (resultDecl != NULL)
      return resultDecl;
    const ClassHierarchyWrapper::ClassDefSet &ancestors =
        classHierarchy->getAncestorClasses(cls);
    worklist.insert(worklist.end(), ancestors.begin(), ancestors.end());
  }

  return NULL;
}

void CallGraphBuilder::buildCallGraph() { buildCallGraph(dummyFilter()); }

/**
 *  CallGraphBuilder::hasGraphNodeFor
 *
 * \brief Checks the graphNodes map for a graph node match fdecl
 *
 * This does a lookup on the CallGraph map to see if a given function is in it.
 * This is used in constructing the CallGraph.  If multiple files are input on
 * the command line firstNondefiningDeclartion may not be unique, so not finding
 * a graphNode for fdecl does not guarantee that the target function does not
 *exist in the graph, only that it is not in the lookup map.  Use
 *getGraphNodeFor to be sure.  Again, this is mainly used in constructing the
 *call graph.
 *
 * \param[in] fdecl The declaration of the function to look for in the graph
 *
 **/
SgGraphNode *
CallGraphBuilder::hasGraphNodeFor(SgFunctionDeclaration *fdecl) const {
  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  GraphNodes::const_iterator lookedup = graphNodes.find(unique);
  if (lookedup != graphNodes.end()) {
    return lookedup->second;
  }
  return NULL;
}

void CallGraphBuilder::registerGraphNode(SgFunctionDeclaration *fdecl,
                                         SgGraphNode *graphNode) {
  ASSERT_not_null(fdecl);
  ASSERT_not_null(graphNode);
  ASSERT_require(callGraphNodeBelongsToProject(fdecl, project));
  if (isSgTemplateFunctionDeclaration(fdecl) != nullptr ||
      isSgTemplateMemberFunctionDeclaration(fdecl) != nullptr) {
    fprintf(stderr,
            "REX_CALLGRAPH_INVARIANT[graph-node-callable]: attempted to "
            "register template pattern=%p/%s name=%s as a concrete graph "
            "vertex\n",
            static_cast<void *>(fdecl), fdecl->class_name().c_str(),
            fdecl->get_name().str());
    ROSE_ABORT();
  }

  graphNodes[fdecl] = graphNode;
  graphNodesByMangledName[fdecl->get_mangled_name()].push_back(
      std::make_pair(fdecl, graphNode));
}

void CallGraphBuilder::addGraphNodeMapping(SgFunctionDeclaration *fdecl,
                                           SgGraphNode *graphNode) {
  registerGraphNode(fdecl, graphNode);
}

SgGraphNode *CallGraphBuilder::getGraphNodeForMangledName(
    SgFunctionDeclaration *fdecl) const {
  if (fdecl == NULL) {
    return NULL;
  }

  GraphNodesByMangledName::const_iterator lookedup =
      graphNodesByMangledName.find(fdecl->get_mangled_name());
  if (lookedup == graphNodesByMangledName.end()) {
    return NULL;
  }

  ASSERT_require(!lookedup->second.empty());
  return lookedup->second.front().second;
}

/**
 *  CallGraphBuilder::getGraphNodeFor
 *
 * \brief Double checks that the Call Graph has the function
 *
 * This double checks for a call graph node.  If multiple files are input on the
 *command line firstNondefiningDeclartion may not be unique.  (As expected) So
 *we can double check on the name.  This is useful only outside the CallGraph,
 *if the CallGraph is fully constructed, and we need a particular node from it.
 *
 * \param[in] fdecl The declaration of the function to look for in the graph
 *
 **/
SgGraphNode *
CallGraphBuilder::getGraphNodeFor(SgFunctionDeclaration *fdecl) const {
  SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
  if (!callGraphNodeBelongsToProject(unique, project))
    return NULL;
  GraphNodes::const_iterator lookedup = graphNodes.find(unique);
  if (lookedup != graphNodes.end()) {
    return lookedup->second;
  }

  return getGraphNodeForMangledName(unique);
}

GetOneFuncDeclarationPerFunction::result_type
GetOneFuncDeclarationPerFunction::operator()(SgNode *node) {
  result_type returnType;
  SgFunctionDeclaration *funcDecl = isSgFunctionDeclaration(node);
  if (funcDecl != NULL && canonicalCallableFunctionDecl(funcDecl) != NULL) {
    if (funcDecl == canonicalFunctionDeclForCallGraph(funcDecl)) {
      returnType.push_back(node);
    }
  }
  return returnType;
}
