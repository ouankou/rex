#include "rose.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace {
SgSourceFile *mainSourceFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile != nullptr && !sourceFile->get_isHeaderFile()) {
      return sourceFile;
    }
  }
  return nullptr;
}

bool isSemanticNonLexicalSubtree(SgNode *node) {
  std::set<SgNode *> visited;
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    ROSE_ASSERT(visited.insert(cursor).second);
    if (isSgAuxiliaryDeclarationList(cursor) != nullptr ||
        isSgDeclarationScope(cursor) != nullptr) {
      return true;
    }
  }
  return false;
}

bool samePhysicalFile(Sg_File_Info *lhs, Sg_File_Info *rhs) {
  return lhs != nullptr && rhs != nullptr && lhs->get_physical_file_id() >= 0 &&
         rhs->get_physical_file_id() >= 0 && lhs->isSameFile(*rhs);
}

bool locationLeq(Sg_File_Info *lhs, Sg_File_Info *rhs) {
  ROSE_ASSERT(samePhysicalFile(lhs, rhs));
  return lhs->get_line() < rhs->get_line() ||
         (lhs->get_line() == rhs->get_line() &&
          lhs->get_col() <= rhs->get_col());
}

bool containsMarker(const PreprocessingInfo *record, const char *marker) {
  return record != nullptr && marker != nullptr &&
         record->getString().find(marker) != std::string::npos;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = mainSourceFile(project);
  ROSE_ASSERT(sourceFile != nullptr);
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  const bool terminalDirectiveTranslationUnit =
      sourceFile->getFileName().find(
          "rex_frontend_preprocessing_terminal_directive.c") !=
      std::string::npos;
  if (terminalDirectiveTranslationUnit) {
    SgFunctionDeclaration *function = nullptr;
    for (SgDeclarationStatement *declaration : global->get_declarations()) {
      SgFunctionDeclaration *candidate = isSgFunctionDeclaration(declaration);
      if (candidate != nullptr &&
          candidate->get_name() == "rex_preprocessing_terminal_directive") {
        ROSE_ASSERT(function == nullptr);
        function = candidate;
      }
    }
    ROSE_ASSERT(function != nullptr);
    SgFunctionParameterList *parameters = function->get_parameterList();
    ROSE_ASSERT(parameters != nullptr);
    ROSE_ASSERT(parameters->get_parent() == function);
    ROSE_ASSERT(parameters->getAttachedPreprocessingInfo() == nullptr);

    size_t mainFileRecordCount = 0;
    size_t openingCount = 0;
    size_t closingCount = 0;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
      SgLocatedNode *located = isSgLocatedNode(node);
      ROSE_ASSERT(located != nullptr);
      AttachedPreprocessingInfoType *records =
          located->getAttachedPreprocessingInfo();
      if (records == nullptr) {
        continue;
      }
      for (PreprocessingInfo *record : *records) {
        ROSE_ASSERT(record != nullptr);
        ROSE_ASSERT(record->get_file_info() != nullptr);
        if (!samePhysicalFile(record->get_file_info(),
                              global->get_startOfConstruct())) {
          continue;
        }
        ++mainFileRecordCount;
        ROSE_ASSERT(located == function);
        if (record->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorIfDeclaration) {
          ++openingCount;
          ROSE_ASSERT(record->getRelativePosition() ==
                      PreprocessingInfo::before);
        } else if (record->getTypeOfDirective() ==
                   PreprocessingInfo::CpreprocessorEndifDeclaration) {
          ++closingCount;
          ROSE_ASSERT(record->getRelativePosition() ==
                      PreprocessingInfo::after);
          ROSE_ASSERT(record->getString().find("#endif") != std::string::npos);
        } else {
          ROSE_ABORT();
        }
      }
    }
    ROSE_ASSERT(mainFileRecordCount == 2);
    ROSE_ASSERT(openingCount == 1);
    ROSE_ASSERT(closingCount == 1);
    return 0;
  }

  const bool emptyTranslationUnit =
      sourceFile->getFileName().find(
          "rex_frontend_preprocessing_empty_translation_unit.c") !=
      std::string::npos;
  std::map<PreprocessingInfo *, SgLocatedNode *> owners;
  std::set<std::string> markers;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
    SgLocatedNode *located = isSgLocatedNode(node);
    ROSE_ASSERT(located != nullptr);
    AttachedPreprocessingInfoType *records =
        located->getAttachedPreprocessingInfo();
    if (records == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *records) {
      ROSE_ASSERT(record != nullptr);
      ROSE_ASSERT(record->get_file_info() != nullptr);
      ROSE_ASSERT(!isSemanticNonLexicalSubtree(located));
      if (!samePhysicalFile(record->get_file_info(),
                            global->get_startOfConstruct())) {
        continue;
      }
      ROSE_ASSERT(owners.emplace(record, located).second);

      if (emptyTranslationUnit) {
        ROSE_ASSERT(located == global);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
        Sg_File_Info *start = global->get_startOfConstruct();
        Sg_File_Info *end = global->get_endOfConstruct();
        ROSE_ASSERT(samePhysicalFile(start, end));
        ROSE_ASSERT(locationLeq(start, end));
        ROSE_ASSERT(locationLeq(start, record->get_file_info()));
        ROSE_ASSERT(locationLeq(record->get_file_info(), end));
      } else {
        ROSE_ASSERT(located != global);
        Sg_File_Info *start = located->get_startOfConstruct();
        Sg_File_Info *end = located->get_endOfConstruct();
        Sg_File_Info *position = record->get_file_info();
        ROSE_ASSERT(samePhysicalFile(start, end));
        ROSE_ASSERT(samePhysicalFile(start, position));
        ROSE_ASSERT(locationLeq(start, end));
        switch (record->getRelativePosition()) {
        case PreprocessingInfo::before:
          ROSE_ASSERT(locationLeq(position, start));
          break;
        case PreprocessingInfo::after:
        case PreprocessingInfo::after_syntax:
          ROSE_ASSERT(locationLeq(end, position));
          break;
        case PreprocessingInfo::inside:
          ROSE_ASSERT(locationLeq(start, position));
          ROSE_ASSERT(locationLeq(position, end));
          break;
        default:
          ROSE_ABORT();
        }
      }

      if (containsMarker(record, "REX_PREPROCESSING_LEADING")) {
        markers.insert("leading");
      }
      if (containsMarker(record, "REX_PREPROCESSING_BETWEEN")) {
        markers.insert("between");
      }
      if (containsMarker(record, "REX_PREPROCESSING_TRAILING")) {
        markers.insert("trailing");
      }
      if (containsMarker(record, "#include <stddef.h>")) {
        markers.insert("system-include");
      }
      if (containsMarker(record, "REX_PREPROCESSING_EMPTY_LEADING")) {
        markers.insert("empty");
      }
    }
  }

  ROSE_ASSERT(!owners.empty());
  if (emptyTranslationUnit) {
    ROSE_ASSERT(global->get_declarations().empty());
    ROSE_ASSERT(markers == std::set<std::string>{"empty"});
  } else {
    ROSE_ASSERT(markers ==
                (std::set<std::string>{"between", "leading", "system-include",
                                       "trailing"}));
    SgAuxiliaryDeclarationList *auxiliary =
        global->get_auxiliary_declarations();
    ROSE_ASSERT(auxiliary != nullptr);
    ROSE_ASSERT(auxiliary->get_parent() == global);
    ROSE_ASSERT(!auxiliary->get_declarations().empty());
    for (SgDeclarationStatement *declaration : auxiliary->get_declarations()) {
      ROSE_ASSERT(declaration != nullptr);
      ROSE_ASSERT(declaration->get_parent() == auxiliary);
      ROSE_ASSERT(declaration->getAttachedPreprocessingInfo() == nullptr);
    }
  }
  return 0;
}
