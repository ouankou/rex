#include "rose.h"

#include <algorithm>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);
  ROSE_ASSERT(project->numberOfFiles() == 1);

  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(sourceFile != nullptr);
  ROSE_ASSERT(!sourceFile->get_unparseHeaderFiles());
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgFunctionDeclaration *function = nullptr;
  for (SgDeclarationStatement *declaration : global->get_declarations()) {
    SgFunctionDeclaration *candidate = isSgFunctionDeclaration(declaration);
    if (candidate == nullptr ||
        candidate->get_name().getString() !=
            "rex_application_header_semantic_pragma_owner") {
      continue;
    }
    ROSE_ASSERT(function == nullptr);
    function = candidate;
  }
  ROSE_ASSERT(function != nullptr);
  ROSE_ASSERT(function->get_parent() == global);
  ROSE_ASSERT(function->get_scope() == global);
  ROSE_ASSERT(function->get_definition() != nullptr);
  ROSE_ASSERT(function->get_definingDeclaration() == function);
  ROSE_ASSERT(function->get_file_info() != nullptr);
  ROSE_ASSERT(function->get_file_info()->get_physical_filename().find(
                  "rex_frontend_application_header_semantic_pragma_owner."
                  "hpp") != std::string::npos);

  SgBasicBlock *body = function->get_definition()->get_body();
  ROSE_ASSERT(body != nullptr);
  SgPragmaDeclaration *outlinePragma = nullptr;
  for (SgStatement *statement : body->get_statements()) {
    SgPragmaDeclaration *pragma = isSgPragmaDeclaration(statement);
    if (pragma == nullptr) {
      continue;
    }
    ROSE_ASSERT(outlinePragma == nullptr);
    outlinePragma = pragma;
  }
  ROSE_ASSERT(outlinePragma != nullptr);
  ROSE_ASSERT(outlinePragma->get_parent() == body);
  ROSE_ASSERT(outlinePragma->get_scope() == body);
  ROSE_ASSERT(outlinePragma->get_pragma() != nullptr);
  ROSE_ASSERT(outlinePragma->get_pragma()->get_pragma() == "rose_outline");
  ROSE_ASSERT(outlinePragma->get_cxx_pragma_payload_kind() ==
              SgPragmaDeclaration::e_cxx_pragma_source_spelled);
  ROSE_ASSERT(outlinePragma->get_cxx_source_text() == "rose_outline");
  ROSE_ASSERT(outlinePragma->getAttachedPreprocessingInfo() == nullptr);

  SgStatement *target = SageInterface::getNextStatement(outlinePragma);
  ROSE_ASSERT(isSgExprStatement(target) != nullptr);
  ROSE_ASSERT(target->get_parent() == body);
  ROSE_ASSERT(std::find(body->get_statements().begin(),
                        body->get_statements().end(),
                        target) != body->get_statements().end());

  SgAuxiliaryDeclarationList *auxiliary = global->get_auxiliary_declarations();
  if (auxiliary != nullptr) {
    ROSE_ASSERT(std::find(auxiliary->get_declarations().begin(),
                          auxiliary->get_declarations().end(),
                          function) == auxiliary->get_declarations().end());
  }
  size_t physicalMarkerCount = 0;
  size_t lexicalMarkerCount = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(sourceFile, V_SgPragmaDeclaration)) {
    SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node);
    ROSE_ASSERT(pragma != nullptr && pragma->get_pragma() != nullptr);
    if (pragma->get_pragma()->get_pragma() == "rex_physical_marker") {
      ROSE_ASSERT(pragma->get_startOfConstruct()->get_line() == 50);
      ++physicalMarkerCount;
    }
    const std::string &text = pragma->get_pragma()->get_pragma();
    if (text == "rex_namespace_marker") {
      SgNamespaceDefinitionStatement *owner =
          isSgNamespaceDefinitionStatement(pragma->get_parent());
      ROSE_ASSERT(owner != nullptr && pragma->get_scope() == owner);
      ROSE_ASSERT(owner->get_namespaceDeclaration()->get_name() ==
                  "rex_pragma_inner");
      ++lexicalMarkerCount;
    } else if (text == "rex_record_marker") {
      SgClassDefinition *owner = isSgClassDefinition(pragma->get_parent());
      ROSE_ASSERT(owner != nullptr && pragma->get_scope() == owner);
      ROSE_ASSERT(owner->get_declaration()->get_name() == "RexPragmaOwner");
      ++lexicalMarkerCount;
    } else if (text == "rex_block_marker") {
      SgBasicBlock *owner = isSgBasicBlock(pragma->get_parent());
      ROSE_ASSERT(owner != nullptr && pragma->get_scope() == owner);
      SgFunctionDefinition *function =
          isSgFunctionDefinition(owner->get_parent());
      ROSE_ASSERT(function != nullptr && function->get_body() == owner);
      ROSE_ASSERT(function->get_declaration()->get_name() ==
                  "rex_empty_pragma_body");
      ++lexicalMarkerCount;
    }
  }
  ROSE_ASSERT(physicalMarkerCount == 2);
  ROSE_ASSERT(lexicalMarkerCount == 3);
  return 0;
}
