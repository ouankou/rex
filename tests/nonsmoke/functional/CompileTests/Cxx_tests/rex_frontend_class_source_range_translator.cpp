#include "rose.h"

#include <string>

namespace {
bool endsWith(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

void requireExactPhysicalRange(const SgLocatedNode *node,
                               const std::string &physicalSuffix, int startLine,
                               int endLine) {
  ROSE_ASSERT(node != nullptr);
  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  ROSE_ASSERT(start != nullptr);
  ROSE_ASSERT(end != nullptr);
  ROSE_ASSERT(!start->isCompilerGenerated());
  ROSE_ASSERT(!end->isCompilerGenerated());
  ROSE_ASSERT(!start->isSourcePositionUnavailableInFrontend());
  ROSE_ASSERT(!end->isSourcePositionUnavailableInFrontend());
  ROSE_ASSERT(start->get_physical_file_id() >= 0);
  ROSE_ASSERT(start->get_physical_file_id() == end->get_physical_file_id());
  ROSE_ASSERT(endsWith(start->get_physical_filename(), physicalSuffix));
  ROSE_ASSERT(endsWith(end->get_physical_filename(), physicalSuffix));
  ROSE_ASSERT(start->get_line() == startLine);
  ROSE_ASSERT(end->get_line() == endLine);
  ROSE_ASSERT(startLine < endLine || start->get_col() <= end->get_col());
}

void requireMatchingRange(const SgLocatedNode *lhs, const SgLocatedNode *rhs) {
  ROSE_ASSERT(lhs != nullptr);
  ROSE_ASSERT(rhs != nullptr);
  const Sg_File_Info *lhsStart = lhs->get_startOfConstruct();
  const Sg_File_Info *lhsEnd = lhs->get_endOfConstruct();
  const Sg_File_Info *rhsStart = rhs->get_startOfConstruct();
  const Sg_File_Info *rhsEnd = rhs->get_endOfConstruct();
  ROSE_ASSERT(lhsStart != nullptr);
  ROSE_ASSERT(lhsEnd != nullptr);
  ROSE_ASSERT(rhsStart != nullptr);
  ROSE_ASSERT(rhsEnd != nullptr);
  ROSE_ASSERT(lhsStart->get_physical_file_id() ==
              rhsStart->get_physical_file_id());
  ROSE_ASSERT(lhsEnd->get_physical_file_id() == rhsEnd->get_physical_file_id());
  ROSE_ASSERT(lhsStart->get_line() == rhsStart->get_line());
  ROSE_ASSERT(lhsStart->get_col() == rhsStart->get_col());
  ROSE_ASSERT(lhsEnd->get_line() == rhsEnd->get_line());
  ROSE_ASSERT(lhsEnd->get_col() == rhsEnd->get_col());
}

void requireStartColumn(const SgLocatedNode *node, int column) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_startOfConstruct() != nullptr);
  ROSE_ASSERT(node->get_startOfConstruct()->get_col() == column);
}

void requireEndColumn(const SgLocatedNode *node, int column) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_endOfConstruct() != nullptr);
  ROSE_ASSERT(node->get_endOfConstruct()->get_col() == column);
}

SgClassDeclaration *findDefiningClass(SgNode *root, const std::string &name) {
  SgClassDeclaration *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    if (declaration == nullptr || declaration->get_name().getString() != name ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr || result == declaration);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_definingDeclaration() == result);
  ROSE_ASSERT(result->get_definition()->get_declaration() == result);
  return result;
}

SgTemplateClassDeclaration *findDefiningTemplate(SgNode *root,
                                                 const std::string &name) {
  SgTemplateClassDeclaration *result = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(root, V_SgTemplateClassDeclaration)) {
    SgTemplateClassDeclaration *declaration =
        isSgTemplateClassDeclaration(node);
    if (declaration == nullptr || declaration->get_name().getString() != name ||
        declaration->get_definition() == nullptr) {
      continue;
    }
    ROSE_ASSERT(result == nullptr || result == declaration);
    result = declaration;
  }
  ROSE_ASSERT(result != nullptr);
  ROSE_ASSERT(result->get_definingDeclaration() == result);
  ROSE_ASSERT(result->get_definition()->get_declaration() == result);
  return result;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgClassDeclaration *header = findDefiningClass(project, "rex_range_header");
  requireExactPhysicalRange(header, "rex_frontend_class_source_range.hpp", 4,
                            6);
  requireStartColumn(header, 1);
  requireEndColumn(header, 1);
  requireMatchingRange(header, header->get_definition());

  SgClassDeclaration *headerForward =
      findDefiningClass(project, "rex_range_header_forward");
  requireExactPhysicalRange(headerForward,
                            "rex_frontend_class_source_range.hpp", 11, 13);
  requireMatchingRange(headerForward, headerForward->get_definition());
  SgClassDeclaration *headerForwardFirst =
      isSgClassDeclaration(headerForward->get_firstNondefiningDeclaration());
  ROSE_ASSERT(headerForwardFirst != nullptr);
  ROSE_ASSERT(headerForwardFirst != headerForward);
  ROSE_ASSERT(headerForwardFirst->get_parent() ==
              SageInterface::getGlobalScope(headerForwardFirst));
  ROSE_ASSERT(headerForwardFirst->get_scope() ==
              SageInterface::getGlobalScope(headerForwardFirst));
  ROSE_ASSERT(headerForwardFirst->get_isAutonomousDeclaration());
  ROSE_ASSERT(headerForwardFirst->get_type() == headerForward->get_type());
  requireExactPhysicalRange(headerForwardFirst,
                            "rex_frontend_class_source_range.hpp", 8, 8);

  SgClassDeclaration *forward = findDefiningClass(project, "rex_range_forward");
  requireExactPhysicalRange(forward, "rex_frontend_class_source_range.cpp", 4,
                            6);
  requireStartColumn(forward, 1);
  requireEndColumn(forward, 1);
  requireMatchingRange(forward, forward->get_definition());
  SgClassDeclaration *first =
      isSgClassDeclaration(forward->get_firstNondefiningDeclaration());
  ROSE_ASSERT(first != nullptr);
  ROSE_ASSERT(first != forward);
  requireExactPhysicalRange(first, "rex_frontend_class_source_range.cpp", 3, 3);
  requireStartColumn(first, 1);
  requireEndColumn(first, 24);

  SgClassDeclaration *macro = findDefiningClass(project, "rex_range_macro");
  requireExactPhysicalRange(macro, "rex_frontend_class_source_range.cpp", 12,
                            12);
  requireStartColumn(macro, 1);
  requireMatchingRange(macro, macro->get_definition());

  SgTemplateClassDeclaration *primary =
      findDefiningTemplate(project, "rex_range_template");
  requireExactPhysicalRange(primary, "rex_frontend_class_source_range.cpp", 14,
                            16);
  requireStartColumn(primary, 1);
  requireExactPhysicalRange(primary->get_definition(),
                            "rex_frontend_class_source_range.cpp", 14, 16);
  requireStartColumn(primary->get_definition(), 23);

  SgTemplateClassDeclaration *macroTemplate =
      findDefiningTemplate(project, "rex_range_macro_template");
  requireExactPhysicalRange(macroTemplate,
                            "rex_frontend_class_source_range.cpp", 22, 22);
  requireStartColumn(macroTemplate, 1);
  requireExactPhysicalRange(macroTemplate->get_definition(),
                            "rex_frontend_class_source_range.cpp", 22, 22);
  requireStartColumn(macroTemplate->get_definition(), 1);

  return 0;
}
