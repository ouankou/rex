#include "clang-to-dot-private.hpp"
#include "sage3basic.h"

std::string ClangToDotTranslator::Traverse(clang::Decl *decl) {
  if (decl == NULL) {
    return "";
  }

  // Look for previous translation
  std::map<clang::Decl *, std::string>::iterator it =
      p_decl_translation_map.find(decl);
  if (it != p_decl_translation_map.end())
    return it->second;

  // If first time, create a new entry
  std::string node_ident = genNextIdent();
  p_decl_translation_map.insert(
      std::pair<clang::Decl *, std::string>(decl, node_ident));
  NodeDescriptor &node_desc =
      p_node_desc
          .insert(std::pair<std::string, NodeDescriptor>(
              node_ident, NodeDescriptor(node_ident)))
          .first->second;

  bool ret_status = false;

  // CLANG_ROSE_Graph::graph (decl);

  switch (decl->getKind()) {
  case clang::Decl::AccessSpec:
    ret_status = VisitAccessSpecDecl((clang::AccessSpecDecl *)decl, node_desc);
    break;
  case clang::Decl::Block:
    ret_status = VisitBlockDecl((clang::BlockDecl *)decl, node_desc);
    break;
  case clang::Decl::Captured:
    ret_status = VisitCapturedDecl((clang::CapturedDecl *)decl, node_desc);
    break;
  case clang::Decl::Empty:
    ret_status = VisitEmptyDecl((clang::EmptyDecl *)decl, node_desc);
    break;
  case clang::Decl::Export:
    ret_status = VisitExportDecl((clang::ExportDecl *)decl, node_desc);
    break;
  case clang::Decl::ExternCContext:
    ret_status =
        VisitExternCContextDecl((clang::ExternCContextDecl *)decl, node_desc);
    break;
  case clang::Decl::FileScopeAsm:
    ret_status =
        VisitFileScopeAsmDecl((clang::FileScopeAsmDecl *)decl, node_desc);
    break;
  case clang::Decl::Friend:
    ret_status = VisitFriendDecl((clang::FriendDecl *)decl, node_desc);
    break;
  case clang::Decl::FriendTemplate:
    ret_status =
        VisitFriendTemplateDecl((clang::FriendTemplateDecl *)decl, node_desc);
    break;
  case clang::Decl::Import:
    ret_status = VisitImportDecl((clang::ImportDecl *)decl, node_desc);
    break;
  case clang::Decl::Label:
    ret_status = VisitLabelDecl((clang::LabelDecl *)decl, node_desc);
    break;
  case clang::Decl::NamespaceAlias:
    ret_status =
        VisitNamespaceAliasDecl((clang::NamespaceAliasDecl *)decl, node_desc);
    break;
  case clang::Decl::Namespace:
    ret_status = VisitNamespaceDecl((clang::NamespaceDecl *)decl, node_desc);
    break;
  case clang::Decl::BuiltinTemplate:
    ret_status =
        VisitBuiltinTemplateDecl((clang::BuiltinTemplateDecl *)decl, node_desc);
    break;
  case clang::Decl::Concept:
    ret_status = VisitConceptDecl((clang::ConceptDecl *)decl, node_desc);
    break;
  case clang::Decl::ClassTemplate:
    ret_status =
        VisitClassTemplateDecl((clang::ClassTemplateDecl *)decl, node_desc);
    break;
  case clang::Decl::FunctionTemplate:
    ret_status = VisitFunctionTemplateDecl((clang::FunctionTemplateDecl *)decl,
                                           node_desc);
    break;
  case clang::Decl::TypeAliasTemplate:
    ret_status = VisitTypeAliasTemplateDecl(
        (clang::TypeAliasTemplateDecl *)decl, node_desc);
    break;
  case clang::Decl::VarTemplate:
    ret_status =
        VisitVarTemplateDecl((clang::VarTemplateDecl *)decl, node_desc);
    break;
  case clang::Decl::TemplateTemplateParm:
    ret_status = VisitTemplateTemplateParmDecl(
        (clang::TemplateTemplateParmDecl *)decl, node_desc);
    break;
  case clang::Decl::Record:
    ret_status = VisitRecordDecl((clang::RecordDecl *)decl, node_desc);
    break;
  case clang::Decl::CXXRecord:
    ret_status = VisitCXXRecordDecl((clang::CXXRecordDecl *)decl, node_desc);
    break;
  case clang::Decl::ClassTemplateSpecialization:
    ret_status = VisitClassTemplateSpecializationDecl(
        (clang::ClassTemplateSpecializationDecl *)decl, node_desc);
    break;
  case clang::Decl::ClassTemplatePartialSpecialization:
    ret_status = VisitClassTemplatePartialSpecializationDecl(
        (clang::ClassTemplatePartialSpecializationDecl *)decl, node_desc);
    break;
  case clang::Decl::Enum:
    ret_status = VisitEnumDecl((clang::EnumDecl *)decl, node_desc);
    break;
  case clang::Decl::TemplateTypeParm:
    ret_status = VisitTemplateTypeParmDecl((clang::TemplateTypeParmDecl *)decl,
                                           node_desc);
    break;
  case clang::Decl::Typedef:
    ret_status = VisitTypedefDecl((clang::TypedefDecl *)decl, node_desc);
    break;
  case clang::Decl::TypeAlias:
    ret_status = VisitTypeAliasDecl((clang::TypeAliasDecl *)decl, node_desc);
    break;
  case clang::Decl::UnresolvedUsingTypename:
    ret_status = VisitUnresolvedUsingTypenameDecl(
        (clang::UnresolvedUsingTypenameDecl *)decl, node_desc);
    break;
  case clang::Decl::Using:
    ret_status = VisitUsingDecl((clang::UsingDecl *)decl, node_desc);
    break;
  case clang::Decl::UsingDirective:
    ret_status =
        VisitUsingDirectiveDecl((clang::UsingDirectiveDecl *)decl, node_desc);
    break;
  case clang::Decl::UsingPack:
    ret_status = VisitUsingPackDecl((clang::UsingPackDecl *)decl, node_desc);
    break;
  case clang::Decl::ConstructorUsingShadow:
    ret_status = VisitConstructorUsingShadowDecl(
        (clang::ConstructorUsingShadowDecl *)decl, node_desc);
    break;
  case clang::Decl::Binding:
    ret_status = VisitBindingDecl((clang::BindingDecl *)decl, node_desc);
    break;
  case clang::Decl::Field:
    ret_status = VisitFieldDecl((clang::FieldDecl *)decl, node_desc);
    break;
  case clang::Decl::Function:
    ret_status = VisitFunctionDecl((clang::FunctionDecl *)decl, node_desc);
    break;
  case clang::Decl::CXXDeductionGuide:
    ret_status = VisitCXXDeductionGuideDecl(
        (clang::CXXDeductionGuideDecl *)decl, node_desc);
    break;
  case clang::Decl::CXXMethod:
    ret_status = VisitCXXMethodDecl((clang::CXXMethodDecl *)decl, node_desc);
    break;
  case clang::Decl::CXXConstructor:
    ret_status =
        VisitCXXConstructorDecl((clang::CXXConstructorDecl *)decl, node_desc);
    break;
  case clang::Decl::CXXConversion:
    ret_status =
        VisitCXXConversionDecl((clang::CXXConversionDecl *)decl, node_desc);
    break;
  case clang::Decl::CXXDestructor:
    ret_status =
        VisitCXXDestructorDecl((clang::CXXDestructorDecl *)decl, node_desc);
    break;
  case clang::Decl::MSProperty:
    ret_status = VisitMSPropertyDecl((clang::MSPropertyDecl *)decl, node_desc);
    break;
  case clang::Decl::NonTypeTemplateParm:
    ret_status = VisitNonTypeTemplateParmDecl(
        (clang::NonTypeTemplateParmDecl *)decl, node_desc);
    break;
  case clang::Decl::Decomposition:
    ret_status =
        VisitDecompositionDecl((clang::DecompositionDecl *)decl, node_desc);
    break;
  case clang::Decl::ImplicitParam:
    ret_status =
        VisitImplicitParamDecl((clang::ImplicitParamDecl *)decl, node_desc);
    break;
  case clang::Decl::OMPCapturedExpr:
    ret_status =
        VisitOMPCaptureExprDecl((clang::OMPCapturedExprDecl *)decl, node_desc);
    break;
  case clang::Decl::ParmVar:
    ret_status = VisitParmVarDecl((clang::ParmVarDecl *)decl, node_desc);
    break;
  case clang::Decl::VarTemplatePartialSpecialization:
    ret_status = VisitVarTemplatePartialSpecializationDecl(
        (clang::VarTemplatePartialSpecializationDecl *)decl, node_desc);
    break;
  case clang::Decl::EnumConstant:
    ret_status =
        VisitEnumConstantDecl((clang::EnumConstantDecl *)decl, node_desc);
    break;
  case clang::Decl::IndirectField:
    ret_status =
        VisitIndirectFieldDecl((clang::IndirectFieldDecl *)decl, node_desc);
    break;
  case clang::Decl::OMPDeclareMapper:
    ret_status = VisitOMPDeclareMapperDecl((clang::OMPDeclareMapperDecl *)decl,
                                           node_desc);
    break;
  case clang::Decl::OMPDeclareReduction:
    ret_status = VisitOMPDeclareReductionDecl(
        (clang::OMPDeclareReductionDecl *)decl, node_desc);
    break;
  case clang::Decl::UnresolvedUsingValue:
    ret_status = VisitUnresolvedUsingValueDecl(
        (clang::UnresolvedUsingValueDecl *)decl, node_desc);
    break;
  case clang::Decl::OMPAllocate:
    ret_status =
        VisitOMPAllocateDecl((clang::OMPAllocateDecl *)decl, node_desc);
    break;
  case clang::Decl::OMPRequires:
    ret_status =
        VisitOMPRequiresDecl((clang::OMPRequiresDecl *)decl, node_desc);
    break;
  case clang::Decl::OMPThreadPrivate:
    ret_status = VisitOMPThreadPrivateDecl((clang::OMPThreadPrivateDecl *)decl,
                                           node_desc);
    break;
  case clang::Decl::PragmaComment:
    ret_status =
        VisitPragmaCommentDecl((clang::PragmaCommentDecl *)decl, node_desc);
    break;
  case clang::Decl::PragmaDetectMismatch:
    ret_status = VisitPragmaDetectMismatchDecl(
        (clang::PragmaDetectMismatchDecl *)decl, node_desc);
    break;
  case clang::Decl::StaticAssert:
    ret_status =
        VisitStaticAssertDecl((clang::StaticAssertDecl *)decl, node_desc);
    break;
  case clang::Decl::TranslationUnit:
    ret_status =
        VisitTranslationUnitDecl((clang::TranslationUnitDecl *)decl, node_desc);
    break;
  case clang::Decl::Var:
    ret_status = VisitVarDecl((clang::VarDecl *)decl, node_desc);
    break;
  default:
    std::cerr << "Unknown declacaration kind: " << decl->getDeclKindName()
              << " !" << std::endl;
    ROSE_ABORT();
  }

  // DQ (11/27/2020): Added debugging support.
  // printf ("ret_status = %s \n",ret_status ? "true" : "false");

  // ROSE_ASSERT(ret_status == false || result != NULL);
  // ROSE_ASSERT(ret_status == false);

  // p_decl_translation_map.insert(std::pair<clang::Decl *, SgNode *>(decl,
  // result));

#if DEBUG_TRAVERSE_DECL
  std::cerr << "Traverse(clang::Decl : " << decl << " ";
  if (clang::NamedDecl::classof(decl)) {
    std::cerr << ": " << ((clang::NamedDecl *)decl)->getNameAsString() << ") ";
  }
  // std::cerr << " visit done : node = " << result << std::endl;
#endif

  // return ret_status ? result : NULL;
  // return ret_status;
  return node_ident;
}

/**********************/
/* Visit Declarations */
/**********************/

bool ClangToDotTranslator::VisitDecl(clang::Decl *decl,
                                     NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitDecl" << std::endl;
#endif

  node_desc.kind_hierarchy.push_back("Decl");

  switch (decl->getAccess()) {
  case clang::AS_public:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_specifier", "public"));
    break;
  case clang::AS_protected:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_specifier", "protected"));
    break;
  case clang::AS_private:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("access_specifier", "private`"));
    break;
  case clang::AS_none:
    break;
  }

  clang::Decl::attr_iterator it;
  unsigned cnt = 0;
  for (it = decl->attr_begin(); it != decl->attr_end(); it++) {
    std::ostringstream oss;
    oss << "attribute[" << cnt++ << "]";
    // DQ (11/27/2020): I'm not clear what code this is exapanding into.
    switch ((*it)->getKind()) {
#define ATTR(X)                                                                \
  case clang::attr::X:                                                         \
    node_desc.attributes.push_back(                                            \
        std::pair<std::string, std::string>(oss.str(), "X"));                  \
    break;
#include "clang/Basic/AttrList.inc"
    }
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "most_recent_decl", Traverse(decl->getMostRecentDecl())));
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "previous_decl", Traverse(decl->getPreviousDecl())));
  clang::DeclContext *declContext = decl->getDeclContext();
  if (declContext)
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "DeclContext",
        Traverse(clang::Decl::castFromDeclContext(declContext))));

  if (decl->isCanonicalDecl()) {
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "canonical_decl", Traverse(decl->getCanonicalDecl())));
  }

  if (decl->hasBody()) {
    node_desc.successors.push_back(
        std::pair<std::string, std::string>("body", Traverse(decl->getBody())));
  }
  if (decl->isImplicit()) {
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("isImplicit", "Yes"));
  }
  if (decl->isFirstDecl()) {
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("isFirstDecl", "Yes"));
  }

  return true;
}

bool ClangToDotTranslator::VisitAccessSpecDecl(
    clang::AccessSpecDecl *access_spec_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitAccessSpecDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("AccessSpecDecl");

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(access_spec_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitBlockDecl(clang::BlockDecl *block_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitBlockDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BlockDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(block_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitCapturedDecl(clang::CapturedDecl *captured_decl,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCapturedDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CapturedDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(captured_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitEmptyDecl(clang::EmptyDecl *empty_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitEmptyDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("EmptyDecl");

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO
  printf("ClangToDotTranslator::VisitEmptyDecl called but not implemented! \n");

  return VisitDecl(empty_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitExportDecl(clang::ExportDecl *export_decl,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitExportDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ExportDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(export_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitExternCContextDecl(
    clang::ExternCContextDecl *ccontent_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCContextDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ExternCContextDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(ccontent_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitFileScopeAsmDecl(
    clang::FileScopeAsmDecl *file_scope_asm_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitFileScopeAsmDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FileScopeAsmDecl");

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO
  printf("ClangToDotTranslator::VisitFileScopeAsmDecl called but not "
         "implemented! \n");

  return VisitDecl(file_scope_asm_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitFriendDecl(clang::FriendDecl *friend_decl,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitFriendDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FriendDecl");

  clang::NamedDecl *named_decl = friend_decl->getFriendDecl();
  clang::TypeSourceInfo *type_source_info = friend_decl->getFriendType();

  assert(named_decl == NULL xor
         type_source_info == NULL); // I think it is and only one: let see!

  if (named_decl != NULL) {
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "friend_decl", Traverse(named_decl)));
  }

  if (type_source_info != NULL) {
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "friend_type", Traverse(type_source_info->getType().getTypePtr())));
  }

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(friend_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitFriendTemplateDecl(
    clang::FriendTemplateDecl *friend_template_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitFriendTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FriendTemplateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(friend_template_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitImportDecl(clang::ImportDecl *import_decl,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitImportDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ImportDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDecl(import_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitNamedDecl(clang::NamedDecl *named_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitNamedDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("NamedDecl");
  node_desc.attributes.push_back(std::pair<std::string, std::string>(
      "name", named_decl->getNameAsString()));

  // DQ (11/27/2020): I want to print out this warning, but not too much.

  switch (named_decl->getVisibility()) {
  case clang::HiddenVisibility:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("visibility", "hidden"));
    break;
  case clang::ProtectedVisibility:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("visibility", "protected"));
    break;
  case clang::DefaultVisibility:
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("visibility", "default"));
    break;
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "(underlying_decl", Traverse(named_decl->getUnderlyingDecl())));

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitDecl(named_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitLabelDecl(clang::LabelDecl *label_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitLabelDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("LabelDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitNamedDecl(label_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitNamespaceAliasDecl(
    clang::NamespaceAliasDecl *namespace_alias_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitNamespaceAliasDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("NamespaceAliasDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitNamedDecl(namespace_alias_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitNamespaceDecl(
    clang::NamespaceDecl *namespace_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitNamespaceDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("NamespaceDecl");

  // In LLVM 20, getOriginalNamespace() was removed, use getFirstDecl() instead
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "original_namespace", Traverse(namespace_decl->getFirstDecl())));

  // DQ (11/28/2020): this function no longer exists in Clang 10.
  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("next_namespace",
  // Traverse(namespace_decl->getNextNamespace())));

  clang::DeclContext::decl_iterator it;
  unsigned cnt = 0;
  for (it = namespace_decl->decls_begin(); it != namespace_decl->decls_end();
       it++) {
    std::ostringstream oss;
    oss << "DeclContext::decls[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitNamedDecl(namespace_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTemplateDecl(clang::TemplateDecl *template_decl,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TemplateDecl");

  clang::TemplateParameterList *template_parameters =
      template_decl->getTemplateParameters();
  assert(template_parameters != NULL);

  clang::TemplateParameterList::iterator it;
  unsigned cnt = 0;
  for (it = template_parameters->begin(); it != template_parameters->end();
       it++) {
    std::ostringstream oss;
    oss << "template_parameter[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "templated_decl", Traverse(template_decl->getTemplatedDecl())));

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(template_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitBuiltinTemplateDecl(
    clang::BuiltinTemplateDecl *builtin_template_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitBuiltinTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BuiltinTemplateDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTemplateDecl(builtin_template_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitConceptDecl(clang::ConceptDecl *concept_decl,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitConceptDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ConceptDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTemplateDecl(concept_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitRedeclarableTemplateDecl(
    clang::RedeclarableTemplateDecl *redeclarable_template_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitRedeclarableTemplateDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("RedeclarableTemplateDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTemplateDecl(redeclarable_template_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitClassTemplateDecl(
    clang::ClassTemplateDecl *class_template_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitClassTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ClassTemplateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitRedeclarableTemplateDecl(class_template_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitFunctionTemplateDecl(
    clang::FunctionTemplateDecl *function_template_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitFunctionTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FunctionTemplateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitRedeclarableTemplateDecl(function_template_decl, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitTypeAliasTemplateDecl(
    clang::TypeAliasTemplateDecl *type_alias_template_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTypeAliasTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TypeAliasTemplateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitRedeclarableTemplateDecl(type_alias_template_decl, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitVarTemplateDecl(
    clang::VarTemplateDecl *var_template_decl, NodeDescriptor &node_desc) {

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitVarTemplateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("VarTemplateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitRedeclarableTemplateDecl(var_template_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTemplateTemplateParmDecl(
    clang::TemplateTemplateParmDecl *template_template_parm_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTemplateTemplateParmDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TemplateTemplateParmDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitTemplateDecl(template_template_parm_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypeDecl(clang::TypeDecl *type_decl,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTypeDecl" << std::endl;
#endif

  bool res = true;

  // Code copied from Tristan's dot file generator.
  node_desc.kind_hierarchy.push_back("TypeDecl");
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "type_for_decl", Traverse(type_decl->getTypeForDecl())));

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(type_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTagDecl(clang::TagDecl *tag_decl,
                                        NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTagDecl" << std::endl;
#endif

  bool res = true;

  node_desc.kind_hierarchy.push_back("TagDecl");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "canonical_decl", Traverse(tag_decl->getCanonicalDecl())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "(TagDecl) definition", Traverse(tag_decl->getDefinition())));

  node_desc.attributes.push_back(
      std::pair<std::string, std::string>("kind", tag_decl->getKindName()));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "typedef_name_for_anon_decl",
      Traverse(tag_decl->getTypedefNameForAnonDecl())));

  // TODO NestedNameSpecifier * getQualifier () const

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTagDecl: casting to DeclContext"
            << std::endl;
#endif
  clang::DeclContext::decl_iterator it;
  unsigned cnt = 0;
  for (it = tag_decl->decls_begin(); it != tag_decl->decls_end(); it++) {
#if DEBUG_VISIT_DECL
    std::cerr << "ClangToSageTranslator::VisitTagDecl: visit decl #" << cnt
              << " " << *it << std::endl;
#endif
    std::ostringstream oss;
    oss << "DeclContext::decls[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

#if DEBUG_VISIT_DECL
  std::cerr << "ClangToSageTranslator::VisitTagDecl:" << cnt << " decls visited"
            << std::endl;
#endif
  return VisitTypeDecl(tag_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitRecordDecl(clang::RecordDecl *record_decl,
                                           NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitRecordDecl" << std::endl;
#endif

  // FIXME May have to check the symbol table first, because of out-of-order
  // traversal of C++ classes (Could be done in CxxRecord class...)

  bool res = true;

  node_desc.kind_hierarchy.push_back("RecordDecl");

  clang::RecordDecl::field_iterator it;
  unsigned cnt = 0;
  for (it = record_decl->field_begin(); it != record_decl->field_end(); it++) {
    std::ostringstream oss;
    oss << "field[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }
  node_desc.attributes.push_back(std::pair<std::string, std::string>(
      "field_empty", record_decl->field_empty() ? "True" : "False"));

  return VisitTagDecl(record_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXRecordDecl(
    clang::CXXRecordDecl *cxx_record_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCXXRecordDecl" << std::endl;
#endif
  bool res = VisitRecordDecl(cxx_record_decl, node_desc);

  node_desc.kind_hierarchy.push_back("CXXRecordDecl");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "(CXXRecordecl) definition", Traverse(cxx_record_decl->getDefinition())));

  // Pei-Hung: skipping the remaining processes when there is no definition
  // found.  This should avoid issues when retriving bases, vbases, decls, ctors
  // and friends.
  if (!cxx_record_decl->hasDefinition())
    return res;

  clang::CXXRecordDecl::base_class_iterator it_base;
  unsigned cnt = 0;
  for (it_base = cxx_record_decl->bases_begin();
       it_base != cxx_record_decl->bases_end(); it_base++) {
    std::ostringstream oss;
    oss << "base_type[" << cnt++ << "]";
    switch (it_base->getAccessSpecifier()) {
    case clang::AS_public:
      oss << " (public)";
      break;
    case clang::AS_protected:
      oss << " (protected)";
      break;
    case clang::AS_private:
      oss << " (private)";
      break;
    case clang::AS_none:
      break;
    }
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        oss.str(), Traverse(it_base->getType().getTypePtr())));
  }

  clang::CXXRecordDecl::base_class_iterator it_vbase;
  cnt = 0;
  for (it_vbase = cxx_record_decl->vbases_begin();
       it_vbase != cxx_record_decl->vbases_end(); it_vbase++) {
    std::ostringstream oss;
    oss << "virtual_base_type[" << cnt++ << "]";
    switch (it_base->getAccessSpecifier()) {
    case clang::AS_public:
      oss << " (public)";
      break;
    case clang::AS_protected:
      oss << " (protected)";
      break;
    case clang::AS_private:
      oss << " (private)";
      break;
    case clang::AS_none:
      break;
    }
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        oss.str(), Traverse(it_vbase->getType().getTypePtr())));
  }

  clang::CXXRecordDecl::method_iterator it_method;
  cnt = 0;
  for (it_method = cxx_record_decl->method_begin();
       it_method != cxx_record_decl->method_end(); it_method++) {
    std::ostringstream oss;
    oss << "method[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it_method)));
  }

  clang::CXXRecordDecl::ctor_iterator it_ctor;
  cnt = 0;
  for (it_ctor = cxx_record_decl->ctor_begin();
       it_ctor != cxx_record_decl->ctor_end(); it_ctor++) {
    std::ostringstream oss;
    oss << "constructor[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it_ctor)));
  }

  clang::CXXRecordDecl::friend_iterator it_friend;
  cnt = 0;
  for (it_friend = cxx_record_decl->friend_begin();
       it_friend != cxx_record_decl->friend_end(); it_friend++) {
    std::ostringstream oss;
    oss << "friend[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it_friend)));
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "destructor", Traverse(cxx_record_decl->getDestructor())));

  return res;
}

bool ClangToDotTranslator::VisitClassTemplateSpecializationDecl(
    clang::ClassTemplateSpecializationDecl *class_tpl_spec_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitClassTemplateSpecializationDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ClassTemplateSpecializationDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitCXXRecordDecl(class_tpl_spec_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitClassTemplatePartialSpecializationDecl(
    clang::ClassTemplatePartialSpecializationDecl *class_tpl_part_spec_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr
      << "ClangToDotTranslator::VisitClassTemplatePartialSpecializationDecl"
      << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ClassTemplatePartialSpecializationDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitClassTemplateSpecializationDecl(class_tpl_part_spec_decl,
                                              node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitEnumDecl(clang::EnumDecl *enum_decl,
                                         NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitEnumDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("EnumDecl");

  node_desc.kind_hierarchy.push_back("EnumDecl");

  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("previous_declaration",
  // Traverse(enum_decl->getPreviousDeclaration())));
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "previous_declaration", Traverse(enum_decl->getPreviousDecl())));

  clang::EnumDecl::enumerator_iterator it;
  unsigned cnt = 0;
  for (it = enum_decl->enumerator_begin(); it != enum_decl->enumerator_end();
       it++) {
    std::ostringstream oss;
    oss << "enumerator[" << cnt++ << "]";
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "promotion_type", Traverse(enum_decl->getPromotionType().getTypePtr())));

  return VisitTagDecl(enum_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTemplateTypeParmDecl(
    clang::TemplateTypeParmDecl *template_type_parm_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTemplateTypeParmDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TemplateTypeParmDecl");

  if (template_type_parm_decl->hasDefaultArgument())
    node_desc.successors.push_back(
        // In LLVM 20, getDefaultArgument() returns TemplateArgumentLoc, need to
        // extract Type*
        std::pair<std::string, std::string>(
            "default_argument",
            Traverse(template_type_parm_decl->getDefaultArgument()
                         .getArgument()
                         .getAsType()
                         .getTypePtr())));

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitTypeDecl(template_type_parm_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypedefNameDecl(
    clang::TypedefNameDecl *typedef_name_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTypedefNameDecl" << std::endl;
#endif
  bool res = true;

  // Code copied from Tristan's dot file generator.
  node_desc.kind_hierarchy.push_back("TypedefNameDecl");
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "underlying_type",
      Traverse(typedef_name_decl->getUnderlyingType().getTypePtr())));

  // node_desc.kind_hierarchy.push_back("TypedefNameDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeDecl(typedef_name_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypedefDecl(clang::TypedefDecl *typedef_decl,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTypedefDecl" << std::endl;
#endif
  bool res = true;

  // Code copied from Tristan's dot file generator.
  node_desc.kind_hierarchy.push_back("TypedefDecl");

  return VisitTypedefNameDecl(typedef_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTypeAliasDecl(
    clang::TypeAliasDecl *type_alias_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTypeAliasDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("TypeAliasDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitTypedefNameDecl(type_alias_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitUnresolvedUsingTypenameDecl(
    clang::UnresolvedUsingTypenameDecl *unresolved_using_type_name_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitUnresolvedUsingTypenameDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UnresolvedUsingTypenameDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitTypeDecl(unresolved_using_type_name_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitUsingDecl(clang::UsingDecl *using_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitUsingDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UsingDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(using_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitUsingDirectiveDecl(
    clang::UsingDirectiveDecl *using_directive_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitUsingDirectiveDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UsingDirectiveDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(using_directive_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitUsingPackDecl(
    clang::UsingPackDecl *using_pack_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitUsingPackDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UsingPackDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(using_pack_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitUsingShadowDecl(
    clang::UsingShadowDecl *using_shadow_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitUsingShadowDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UsingShadowDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(using_shadow_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitConstructorUsingShadowDecl(
    clang::ConstructorUsingShadowDecl *constructor_using_shadow_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitConstructorUsingShadowDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ConstructorUsingShadowDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(constructor_using_shadow_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitValueDecl(clang::ValueDecl *value_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitValueDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ValueDecl");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "type", Traverse(value_decl->getType().getTypePtr())));

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitNamedDecl(value_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitBindingDecl(clang::BindingDecl *binding_decl,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitBindingDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("BindingDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitValueDecl(binding_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitDeclaratorDecl(
    clang::DeclaratorDecl *declarator_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitDeclaratorDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DeclaratorDecl");

  ROSE_ASSERT(FAIL_FIXME == 0); // FIXME

  return VisitValueDecl(declarator_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitFieldDecl(clang::FieldDecl *field_decl,
                                          NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitFieldDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("FieldDecl");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "in_class_initializer", Traverse(field_decl->getInClassInitializer())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "bit_width", Traverse(field_decl->getBitWidth())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "parent", Traverse(field_decl->getParent())));

  return VisitDeclaratorDecl(field_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitFunctionDecl(clang::FunctionDecl *function_decl,
                                             NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitFunctionDecl" << std::endl;
#endif
  bool res = true;

  // FIXME: There is something weird here when try to Traverse a function
  // reference in a recursive function (when first Traverse is not complete)
  //        It seems that it tries to instantiate the decl inside the
  //        function... It may be faster to recode from scratch...
  //   If I am not wrong this have been fixed....

  node_desc.kind_hierarchy.push_back("FunctionDecl");

  // DQ (11/27/2020): Trying to update the code from Clang 3.x to Clang 10.
  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("previous_declaration",
  // Traverse(function_decl->getPreviousDeclaration())));
  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("previous_declaration",
  // Traverse(function_decl->getPreviousDeclImpl())));
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "previous_declaration", Traverse(function_decl->getCanonicalDecl())));

  // DQ (11/27/2020): Trying to update the code from Clang 3.x to Clang 10.
  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("result_type",
  // Traverse(function_decl->getResultType().getTypePtr())));
  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "result_type", Traverse(function_decl->getReturnType().getTypePtr())));

  for (unsigned i = 0; i < function_decl->getNumParams(); i++) {
    std::ostringstream oss;
    oss << "parameter[" << i << "]";
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        oss.str(), Traverse(function_decl->getParamDecl(i))));
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "body", Traverse(function_decl->getBody())));

  return VisitDeclaratorDecl(function_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDeductionGuideDecl(
    clang::CXXDeductionGuideDecl *cxx_deduction_guide_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCXXDeductionGuideDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDeductionGuideDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitFunctionDecl(cxx_deduction_guide_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXMethodDecl(
    clang::CXXMethodDecl *cxx_method_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCXXMethodDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXMethodDecl");

  //     ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitFunctionDecl(cxx_method_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXConstructorDecl(
    clang::CXXConstructorDecl *cxx_constructor_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCXXConstructorDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXConstructorDecl");

  // ...

  clang::CXXConstructorDecl::init_iterator it;
  unsigned cnt = 0;
  for (it = cxx_constructor_decl->init_begin();
       it != cxx_constructor_decl->init_end(); it++) {
    std::ostringstream oss;
    oss << "init[" << cnt++ << "]";
    //      node_desc.successors.push_back(std::pair<std::string,
    //      std::string>(oss.str(), Traverse(*it)));
    if ((*it)->isMemberInitializer()) {
      clang::FieldDecl *field_decl = (*it)->getMember();
      node_desc.successors.push_back(std::pair<std::string, std::string>(
          oss.str(), Traverse((*it)->getInit())));
      node_desc.attributes.push_back(std::pair<std::string, std::string>(
          oss.str(), field_decl->getNameAsString()));
    }
  }
  if (cxx_constructor_decl->isDefaultConstructor())
    node_desc.attributes.push_back(
        std::pair<std::string, std::string>("is_default_constructor", "true"));

  //    ROSE_ASSERT(FAIL_TODO == 0); // TODO

  res = VisitCXXMethodDecl(cxx_constructor_decl, node_desc);

  return res;
}

bool ClangToDotTranslator::VisitCXXConversionDecl(
    clang::CXXConversionDecl *cxx_conversion_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCXXConversionDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXConversionDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitCXXMethodDecl(cxx_conversion_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitCXXDestructorDecl(
    clang::CXXDestructorDecl *cxx_destructor_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitCXXDestructorDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("CXXDestructorDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitCXXMethodDecl(cxx_destructor_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitMSPropertyDecl(
    clang::MSPropertyDecl *ms_property_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitMSPropertyDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("MSPropertyDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDeclaratorDecl(ms_property_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitNonTypeTemplateParmDecl(
    clang::NonTypeTemplateParmDecl *non_type_template_param_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitNonTypeTemplateParmDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("NonTypeTemplateParmDecl");

  if (non_type_template_param_decl->hasDefaultArgument())
    // In LLVM 20, getDefaultArgument() returns TemplateArgumentLoc, use
    // getSourceExpression() to get Expr*
    node_desc.successors.push_back(std::pair<std::string, std::string>(
        "default_argument",
        Traverse(non_type_template_param_decl->getDefaultArgument()
                     .getSourceExpression())));

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDeclaratorDecl(non_type_template_param_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitVarDecl(clang::VarDecl *var_decl,
                                        NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitVarDecl" << std::endl;
#endif
  bool res = true;

  // Create the SAGE node: SgVariableDeclaration

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "acting_definition", Traverse(var_decl->getActingDefinition())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "(VarDecl) definition", Traverse(var_decl->getDefinition())));

  // DQ (11/28/2020): I think this is no longer available in Clang 10.
  // node_desc.successors.push_back(std::pair<std::string,
  // std::string>("out_of_line_definition",
  // Traverse(var_decl->getOutOfLineDefinition())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "init", Traverse(var_decl->getInit())));

  node_desc.kind_hierarchy.push_back("VarDecl");

  return VisitDeclaratorDecl(var_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitDecompositionDecl(
    clang::DecompositionDecl *decomposition_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitDecompositionDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("DecompositionDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarDecl(decomposition_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitImplicitParamDecl(
    clang::ImplicitParamDecl *implicit_param_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitImplicitParamDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("ImplicitParamDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarDecl(implicit_param_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPCaptureExprDecl(
    clang::OMPCapturedExprDecl *omp_capture_expr_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitOMPCaptureExprDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPCaptureExprDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarDecl(omp_capture_expr_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitParmVarDecl(clang::ParmVarDecl *param_var_decl,
                                            NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitParmVarDecl" << std::endl;
#endif
  bool res = true;

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "original_type",
      Traverse(param_var_decl->getOriginalType().getTypePtr())));

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "default_arg", Traverse(param_var_decl->getDefaultArg())));

  node_desc.kind_hierarchy.push_back("ParmVarDecl");

  return VisitDeclaratorDecl(param_var_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitVarTemplateSpecializationDecl(
    clang::VarTemplateSpecializationDecl *var_template_specialization_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitVarTemplateSpecializationDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("VarTemplateSpecializationDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDeclaratorDecl(var_template_specialization_decl, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitVarTemplatePartialSpecializationDecl(
    clang::VarTemplatePartialSpecializationDecl
        *var_template_partial_specialization_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitVarTemplatePartialSpecializationDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("VarTemplatePartialSpecializationDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitVarTemplateSpecializationDecl(
             var_template_partial_specialization_decl, node_desc) &&
         res;
}

bool ClangToDotTranslator::VisitEnumConstantDecl(
    clang::EnumConstantDecl *enum_constant_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitEnumConstantDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("EnumConstantDecl");

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "init_expr", Traverse(enum_constant_decl->getInitExpr())));

  return VisitValueDecl(enum_constant_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitIndirectFieldDecl(
    clang::IndirectFieldDecl *indirect_field_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitIndirectFieldDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("IndirectFieldDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(indirect_field_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPDeclareMapperDecl(
    clang::OMPDeclareMapperDecl *omp_declare_mapper_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitOMPDeclareMapperDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPDeclareMapperDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(omp_declare_mapper_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPDeclareReductionDecl(
    clang::OMPDeclareReductionDecl *omp_declare_reduction_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitOMPDeclareReductionDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPDeclareReductionDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(omp_declare_reduction_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitUnresolvedUsingValueDecl(
    clang::UnresolvedUsingValueDecl *unresolved_using_value_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitUnresolvedUsingValueDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("UnresolvedUsingValueDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitValueDecl(unresolved_using_value_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPAllocateDecl(
    clang::OMPAllocateDecl *omp_allocate_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitOMPAllocateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPAllocateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(omp_allocate_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPRequiresDecl(
    clang::OMPRequiresDecl *omp_requires_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitOMPRequiresDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPRequiresDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(omp_requires_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitOMPThreadPrivateDecl(
    clang::OMPThreadPrivateDecl *omp_thread_private_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitOMPThreadPrivateDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("OMPThreadPrivateDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(omp_thread_private_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitPragmaCommentDecl(
    clang::PragmaCommentDecl *pragma_comment_decl, NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitPragmaCommentDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("PragmaCommentDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(pragma_comment_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitPragmaDetectMismatchDecl(
    clang::PragmaDetectMismatchDecl *pragma_detect_mismatch_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitPragmaDetectMismatchDecl"
            << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("PragmaDetectMismatchDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(pragma_detect_mismatch_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitStaticAssertDecl(
    clang::StaticAssertDecl *pragma_static_assert_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitStaticAssertDecl" << std::endl;
#endif
  bool res = true;

  node_desc.kind_hierarchy.push_back("StaticAssertDecl");

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitDecl(pragma_static_assert_decl, node_desc) && res;
}

bool ClangToDotTranslator::VisitTranslationUnitDecl(
    clang::TranslationUnitDecl *translation_unit_decl,
    NodeDescriptor &node_desc) {
#if DEBUG_VISIT_DECL
  std::cerr << "ClangToDotTranslator::VisitTranslationUnitDecl" << std::endl;
#endif

  bool res = true;

  // Code copied from Tristan's dot file generator.
  node_desc.kind_hierarchy.push_back("TranslationUnitDecl");

  clang::DeclContext::decl_iterator it;
  unsigned cnt = 0;
  for (it = translation_unit_decl->decls_begin();
       it != translation_unit_decl->decls_end(); it++) {
    std::ostringstream oss;
    oss << "DeclContext::decls[" << cnt++ << "]";
#ifdef SHORT_CUT_BUILTIN
    if (cnt < 6)
      continue;
#endif
    node_desc.successors.push_back(
        std::pair<std::string, std::string>(oss.str(), Traverse(*it)));
  }

  node_desc.successors.push_back(std::pair<std::string, std::string>(
      "anonymous_namespace",
      Traverse(translation_unit_decl->getAnonymousNamespace())));

  // Traverse the class hierarchy
  return VisitDecl(translation_unit_decl, node_desc) && res;
}
