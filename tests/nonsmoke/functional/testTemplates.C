// Example ROSE Translator used for testing ROSE infrastructure
#include "rose.h"

#include "RoseAst.h"

void validateTemplateInstantiationOutputOwnership(SgProject *root) {
  RoseAst ast(root);
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    SgDeclarationStatement *declaration = nullptr;
    if (isSgTemplateInstantiationDecl(*i) != nullptr ||
        isSgTemplateInstantiationFunctionDecl(*i) != nullptr ||
        isSgTemplateInstantiationMemberFunctionDecl(*i) != nullptr ||
        isSgTemplateInstantiationTypedefDeclaration(*i) != nullptr) {
      declaration = isSgDeclarationStatement(*i);
    }
    if (declaration == nullptr || declaration->get_file_info() == nullptr) {
      continue;
    }
    if (SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(
                declaration->get_parent())) {
      SgScopeStatement *owner = isSgScopeStatement(directive->get_parent());
      const SgNodePtrList owner_successors =
          owner != nullptr ? owner->get_traversalSuccessorContainer()
                           : SgNodePtrList{};
      if (directive->get_declaration() != declaration || owner == nullptr ||
          directive->get_scope() != owner ||
          declaration->get_scope() != owner ||
          std::count(owner_successors.begin(), owner_successors.end(),
                     directive) != 1) {
        fprintf(stderr,
                "REX_TEST_INVARIANT[template-instantiation-owner]: "
                "declaration=%p type=%s name=%s has malformed explicit "
                "instantiation directive ownership\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                SageInterface::get_name(declaration).c_str());
        ROSE_ABORT();
      }
      continue;
    }
    SgDeclarationStatement::template_specialization_enum specialization =
        SgDeclarationStatement::e_unknown;
    if (SgTemplateInstantiationDecl *instantiation =
            isSgTemplateInstantiationDecl(declaration)) {
      specialization = instantiation->get_specialization();
    } else if (SgTemplateInstantiationFunctionDecl *instantiation =
                   isSgTemplateInstantiationFunctionDecl(declaration)) {
      specialization = instantiation->get_specialization();
    } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                   isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
      specialization = instantiation->get_specialization();
    }
    SgScopeStatement *lexical_owner =
        isSgScopeStatement(declaration->get_parent());
    const Sg_File_Info *file_info = declaration->get_file_info();
    std::size_t structural_edges = 0;
    if (lexical_owner != nullptr) {
      for (const auto &successor : lexical_owner->returnDataMemberPointers()) {
        if (successor.first == declaration) {
          ++structural_edges;
        }
      }
    }
    const bool exact_written_surface =
        lexical_owner != nullptr && file_info->get_line() > 0 &&
        file_info->get_physical_file_id() >= 0 &&
        !file_info->isCompilerGenerated() && !file_info->isFrontendSpecific() &&
        structural_edges == 1;
    if (exact_written_surface) {
      continue;
    }

    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(declaration->get_parent());
    SgScopeStatement *semantic_owner =
        auxiliary != nullptr ? isSgScopeStatement(auxiliary->get_parent())
                             : nullptr;
    const bool exact_semantic_owner =
        auxiliary != nullptr && semantic_owner != nullptr &&
        semantic_owner->get_auxiliary_declarations() == auxiliary &&
        declaration->get_scope() == semantic_owner &&
        std::count(auxiliary->get_declarations().begin(),
                   auxiliary->get_declarations().end(), declaration) == 1 &&
        !semantic_owner->statementExistsInScope(declaration) &&
        file_info->isCompilerGenerated() && file_info->isFrontendSpecific() &&
        !file_info->isTransformation();
    if (exact_semantic_owner) {
      continue;
    }

    fprintf(stderr,
            "REX_TEST_INVARIANT[template-instantiation-owner]: "
            "declaration=%p type=%s name=%s specialization=%d file=%s "
            "line=%d compiler-generated=%d frontend-specific=%d "
            "transformation=%d output=%d parent=%p parent-type=%s scope=%p "
            "scope-type=%s first=%p defining=%p definition=%p has neither an "
            "exact written surface nor an exact semantic auxiliary owner\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            SageInterface::get_name(declaration).c_str(),
            static_cast<int>(specialization),
            declaration->get_file_info()->get_filenameString().c_str(),
            declaration->get_file_info()->get_line(),
            declaration->get_file_info()->isCompilerGenerated(),
            declaration->get_file_info()->isFrontendSpecific(),
            declaration->get_file_info()->isTransformation(),
            declaration->get_file_info()->isOutputInCodeGeneration(),
            static_cast<void *>(declaration->get_parent()),
            declaration->get_parent() != nullptr
                ? declaration->get_parent()->class_name().c_str()
                : "<null>",
            static_cast<void *>(declaration->get_scope()),
            declaration->get_scope() != nullptr
                ? declaration->get_scope()->class_name().c_str()
                : "<null>",
            static_cast<void *>(declaration->get_firstNondefiningDeclaration()),
            static_cast<void *>(declaration->get_definingDeclaration()),
            static_cast<void *>(
                isSgClassDeclaration(declaration) != nullptr
                    ? isSgClassDeclaration(declaration)->get_definition()
                    : nullptr));
    ROSE_ABORT();
  }
}

int main(int argc, char *argv[]) {

  // Generate the ROSE AST.
  SgProject *project = frontend(argc, argv);

  // AST consistency tests (optional for users, but this enforces more of our
  // tests)
  AstTests::runAllTests(project);

  validateTemplateInstantiationOutputOwnership(project);

  // regenerate the source code and call the vendor
  // compiler, only backend error code is reported.
  return backend(project);
}
