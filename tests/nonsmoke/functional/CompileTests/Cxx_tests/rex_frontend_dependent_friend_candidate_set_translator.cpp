#include "astJson/sageAstJsonPrivate.h"
#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_set>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  size_t unresolved_friend_count = 0;
  const Rose_STL_Container<SgNode *> declarations =
      NodeQuery::querySubTree(project, V_SgTemplateInstantiationFunctionDecl);
  for (SgNode *node : declarations) {
    SgTemplateInstantiationFunctionDecl *declaration =
        isSgTemplateInstantiationFunctionDecl(node);
    ROSE_ASSERT(declaration != nullptr);

    const SgDeclarationStatementPtrList &candidates =
        declaration->get_dependentTemplateCandidates();
    if (candidates.empty()) {
      ROSE_ASSERT(declaration->get_templateDeclaration() != nullptr);
      ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() ==
                  declaration->get_templateDeclaration());
      ROSE_ASSERT(declaration->get_templateDeclaration()
                      ->get_firstNondefiningDeclaration() ==
                  declaration->get_templateDeclaration());
      continue;
    }

    ++unresolved_friend_count;
    ROSE_ASSERT(declaration->get_templateName() == "select");
    ROSE_ASSERT(declaration->get_declarationModifier().isFriend());
    ROSE_ASSERT(declaration->get_templateDeclaration() == nullptr);
    ROSE_ASSERT(declaration->get_specializedTemplateDeclaration() == nullptr);
    ROSE_ASSERT(candidates.size() == 2);

    std::unordered_set<SgTemplateFunctionDeclaration *> exact_candidates;
    std::unordered_set<SgFunctionType *> exact_types;
    for (SgDeclarationStatement *candidate : candidates) {
      SgTemplateFunctionDeclaration *template_declaration =
          isSgTemplateFunctionDeclaration(candidate);
      ROSE_ASSERT(template_declaration != nullptr);
      ROSE_ASSERT(template_declaration->get_name() ==
                  declaration->get_templateName());
      ROSE_ASSERT(template_declaration->get_scope() ==
                  declaration->get_scope());
      ROSE_ASSERT(template_declaration->get_firstNondefiningDeclaration() ==
                  template_declaration);
      ROSE_ASSERT(template_declaration->get_symbol_from_symbol_table() !=
                  nullptr);
      ROSE_ASSERT(exact_candidates.insert(template_declaration).second);
      ROSE_ASSERT(exact_types.insert(template_declaration->get_type()).second);
    }
  }

  ROSE_ASSERT(unresolved_friend_count == 1);
  using namespace Rose::AstJson;
  SgSourceFile *file = isSgSourceFile(project->get_fileList().front());
  ROSE_ASSERT(file != nullptr);
  AstFileRecord ast =
      parseAstFileJson(buildJson(file, Checkpoint::PreOmpConstruction, file),
                       checkpointName(Checkpoint::PreOmpConstruction));
  for (const NodeRecord &record : ast.nodes) {
    if (record.kind != "SgTemplateInstantiationFunctionDecl" ||
        edgesFor(record, "dependentTemplateCandidates").empty())
      continue;
    const auto candidate_edges =
        edgesFor(record, "dependentTemplateCandidates");
    const auto functions = SgTemplateInstantiationFunctionDecl::numberOfNodes();
    const auto files = SgSourceFile::numberOfNodes();
    const auto parameters = SgFunctionParameterList::numberOfNodes();
    for (int defect = 0; defect != 4; ++defect) {
      AstFileRecord malformed = ast;
      NodeRecord &invalid = *std::find_if(
          malformed.nodes.begin(), malformed.nodes.end(),
          [&](const NodeRecord &node) { return node.id == record.id; });
      invalid.edges.erase(
          std::remove_if(invalid.edges.begin(), invalid.edges.end(),
                         [](const EdgeRecord &edge) {
                           return edge.field == "dependentTemplateCandidates";
                         }),
          invalid.edges.end());
      if (defect == 1) {
        invalid.edges.push_back(candidate_edges.front());
      } else if (defect == 2) {
        invalid.edges.push_back(candidate_edges.front());
        EdgeRecord duplicate = candidate_edges.front();
        duplicate.index = 1;
        invalid.edges.push_back(duplicate);
      } else if (defect == 3) {
        invalid.edges.insert(invalid.edges.end(), candidate_edges.begin(),
                             candidate_edges.end());
        invalid.edges.push_back(
            {"templateDeclaration", candidate_edges.front().target, 0});
      }
      bool rejected = false;
      try {
        (void)reconstructSourceFile(malformed, file, nullptr);
      } catch (const std::runtime_error &error) {
        const std::string diagnostic = error.what();
        rejected =
            diagnostic.find("one exact primary") != std::string::npos ||
            diagnostic.find("candidate has no exact canonical identity") !=
                std::string::npos;
      }
      ROSE_ASSERT(rejected);
      ROSE_ASSERT(SgTemplateInstantiationFunctionDecl::numberOfNodes() ==
                  functions);
      ROSE_ASSERT(SgSourceFile::numberOfNodes() == files);
      ROSE_ASSERT(SgFunctionParameterList::numberOfNodes() == parameters);
    }
  }
  return backend(project);
}
