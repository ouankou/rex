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
         rhs->get_physical_file_id() >= 0 && lhs->isSameFile(*rhs) &&
         lhs->get_physical_file_occurrence_id() ==
             rhs->get_physical_file_occurrence_id();
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

  const bool includeOccurrenceTranslationUnit =
      sourceFile->getFileName().find(
          "rex_frontend_preprocessing_include_occurrence.c") !=
      std::string::npos;
  if (includeOccurrenceTranslationUnit) {
    std::map<std::string, SgVariableDeclaration *> declarations;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
      SgInitializedName *name = isSgInitializedName(node);
      ROSE_ASSERT(name != nullptr);
      const std::string spelling = name->get_name().getString();
      if (spelling != "rex_preprocessing_first_occurrence" &&
          spelling != "rex_preprocessing_second_occurrence") {
        continue;
      }
      SgVariableDeclaration *declaration =
          isSgVariableDeclaration(name->get_parent());
      ROSE_ASSERT(declaration != nullptr);
      ROSE_ASSERT(declarations.emplace(spelling, declaration).second);
    }
    ROSE_ASSERT(declarations.size() == 2);

    std::set<int> recordOccurrences;
    std::set<SgVariableDeclaration *> owners;
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgLocatedNode)) {
      SgLocatedNode *located = isSgLocatedNode(node);
      ROSE_ASSERT(located != nullptr);
      AttachedPreprocessingInfoType *records =
          located->getAttachedPreprocessingInfo();
      if (records == nullptr) {
        continue;
      }
      for (PreprocessingInfo *record : *records) {
        if (!containsMarker(record, "REX_PREPROCESSING_INCLUDE_OCCURRENCE")) {
          continue;
        }
        SgVariableDeclaration *owner = isSgVariableDeclaration(located);
        ROSE_ASSERT(owner != nullptr);
        ROSE_ASSERT(
            owner == declarations.at("rex_preprocessing_first_occurrence") ||
            owner == declarations.at("rex_preprocessing_second_occurrence"));
        ROSE_ASSERT(record->getAttachedOwner() == owner);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
        ROSE_ASSERT(samePhysicalFile(record->get_file_info(),
                                     owner->get_startOfConstruct()));
        ROSE_ASSERT(owners.insert(owner).second);
        ROSE_ASSERT(
            recordOccurrences
                .insert(
                    record->get_file_info()->get_physical_file_occurrence_id())
                .second);
      }
    }
    ROSE_ASSERT(owners.size() == 2);
    ROSE_ASSERT(recordOccurrences.size() == 2);
    return 0;
  }

  const bool parameterBoundaryTranslationUnit =
      sourceFile->getFileName().find(
          "rex_frontend_preprocessing_parameter_boundary.c") !=
      std::string::npos;
  if (parameterBoundaryTranslationUnit) {
    SgFunctionDeclaration *function = nullptr;
    for (SgDeclarationStatement *declaration : global->get_declarations()) {
      SgFunctionDeclaration *candidate = isSgFunctionDeclaration(declaration);
      if (candidate != nullptr &&
          candidate->get_name() == "rex_preprocessing_parameter_directive") {
        ROSE_ASSERT(function == nullptr);
        function = candidate;
      }
    }
    ROSE_ASSERT(function != nullptr);
    SgFunctionParameterList *parameters = function->get_parameterList_syntax();
    if (parameters == nullptr) {
      parameters = function->get_parameterList();
    }
    ROSE_ASSERT(parameters != nullptr);
    ROSE_ASSERT(parameters->get_parent() == function);
    ROSE_ASSERT(parameters->get_args().size() == 1);
    SgInitializedName *parameter = parameters->get_args().front();
    ROSE_ASSERT(parameter != nullptr);
    ROSE_ASSERT(parameter->get_name() == "rex_preprocessing_parameter");
    ROSE_ASSERT(parameter->get_parent() == parameters);

    size_t openingCount = 0;
    size_t closingCount = 0;
    AttachedPreprocessingInfoType *records =
        parameter->getAttachedPreprocessingInfo();
    ROSE_ASSERT(records != nullptr);
    for (PreprocessingInfo *record : *records) {
      ROSE_ASSERT(record != nullptr);
      ROSE_ASSERT(record->getAttachedOwner() == parameter);
      ROSE_ASSERT(samePhysicalFile(record->get_file_info(),
                                   parameter->get_startOfConstruct()));
      if (record->getTypeOfDirective() ==
          PreprocessingInfo::CpreprocessorIfDeclaration) {
        ++openingCount;
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
      } else if (record->getTypeOfDirective() ==
                 PreprocessingInfo::CpreprocessorEndifDeclaration) {
        ++closingCount;
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::after);
      }
    }
    ROSE_ASSERT(openingCount == 1);
    ROSE_ASSERT(closingCount == 1);
    ROSE_ASSERT(parameters->getAttachedPreprocessingInfo() == nullptr);

    const auto requireConditionalParameter = [&](const char *functionName,
                                                 const char *parameterName,
                                                 size_t parameterIndex,
                                                 bool closesOnSameParameter) {
      SgFunctionDeclaration *conditionalFunction = nullptr;
      for (SgDeclarationStatement *declaration : global->get_declarations()) {
        SgFunctionDeclaration *candidate = isSgFunctionDeclaration(declaration);
        if (candidate != nullptr && candidate->get_name() == functionName) {
          ROSE_ASSERT(conditionalFunction == nullptr);
          conditionalFunction = candidate;
        }
      }
      ROSE_ASSERT(conditionalFunction != nullptr);
      SgFunctionParameterList *conditionalParameters =
          conditionalFunction->get_parameterList_syntax();
      if (conditionalParameters == nullptr) {
        conditionalParameters = conditionalFunction->get_parameterList();
      }
      ROSE_ASSERT(conditionalParameters != nullptr);
      ROSE_ASSERT(conditionalParameters->get_parent() == conditionalFunction);
      ROSE_ASSERT(conditionalParameters->get_args().size() == 2);
      SgInitializedName *conditionalParameter =
          conditionalParameters->get_args().at(parameterIndex);
      ROSE_ASSERT(conditionalParameter != nullptr);
      ROSE_ASSERT(conditionalParameter->get_name() == parameterName);
      ROSE_ASSERT(conditionalParameter->get_parent() == conditionalParameters);

      AttachedPreprocessingInfoType *conditionalRecords =
          conditionalParameter->getAttachedPreprocessingInfo();
      ROSE_ASSERT(conditionalRecords != nullptr);
      size_t conditionalOpeningCount = 0;
      size_t conditionalClosingCount = 0;
      for (PreprocessingInfo *record : *conditionalRecords) {
        ROSE_ASSERT(record != nullptr);
        ROSE_ASSERT(record->getAttachedOwner() == conditionalParameter);
        ROSE_ASSERT(
            samePhysicalFile(record->get_file_info(),
                             conditionalParameter->get_startOfConstruct()));
        if (record->getTypeOfDirective() ==
            PreprocessingInfo::CpreprocessorIfDeclaration) {
          ++conditionalOpeningCount;
          ROSE_ASSERT(record->getRelativePosition() ==
                      PreprocessingInfo::before);
        } else if (record->getTypeOfDirective() ==
                   PreprocessingInfo::CpreprocessorEndifDeclaration) {
          ++conditionalClosingCount;
          ROSE_ASSERT(record->getRelativePosition() ==
                      PreprocessingInfo::after);
        }
      }
      ROSE_ASSERT(conditionalOpeningCount == 1);
      ROSE_ASSERT(conditionalClosingCount == (closesOnSameParameter ? 1 : 0));
      return conditionalParameters;
    };

    requireConditionalParameter(
        "rex_preprocessing_conditional_trailing_parameter",
        "rex_preprocessing_trailing_parameter", 1, true);
    SgFunctionParameterList *leadingParameters = requireConditionalParameter(
        "rex_preprocessing_conditional_leading_parameter",
        "rex_preprocessing_leading_parameter", 0, false);
    SgInitializedName *requiredTrailingParameter =
        leadingParameters->get_args().at(1);
    ROSE_ASSERT(requiredTrailingParameter != nullptr);
    ROSE_ASSERT(requiredTrailingParameter->get_name() ==
                "rex_preprocessing_required_trailing_parameter");
    records = requiredTrailingParameter->getAttachedPreprocessingInfo();
    ROSE_ASSERT(records != nullptr);
    closingCount = 0;
    for (PreprocessingInfo *record : *records) {
      ROSE_ASSERT(record != nullptr);
      if (record->getTypeOfDirective() ==
          PreprocessingInfo::CpreprocessorEndifDeclaration) {
        ++closingCount;
        ROSE_ASSERT(record->getAttachedOwner() == requiredTrailingParameter);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
      }
    }
    ROSE_ASSERT(closingCount == 1);

    SgFunctionDeclaration *emptyFunction = nullptr;
    for (SgDeclarationStatement *declaration : global->get_declarations()) {
      SgFunctionDeclaration *candidate = isSgFunctionDeclaration(declaration);
      if (candidate != nullptr &&
          candidate->get_name() ==
              "rex_preprocessing_empty_parameter_directive") {
        ROSE_ASSERT(emptyFunction == nullptr);
        emptyFunction = candidate;
      }
    }
    ROSE_ASSERT(emptyFunction != nullptr);
    SgFunctionParameterList *emptyParameters =
        emptyFunction->get_parameterList_syntax();
    if (emptyParameters == nullptr) {
      emptyParameters = emptyFunction->get_parameterList();
    }
    ROSE_ASSERT(emptyParameters != nullptr);
    ROSE_ASSERT(emptyParameters->get_parent() == emptyFunction);
    ROSE_ASSERT(emptyParameters->get_args().empty());
    records = emptyParameters->getAttachedPreprocessingInfo();
    ROSE_ASSERT(records != nullptr);
    openingCount = 0;
    closingCount = 0;
    for (PreprocessingInfo *record : *records) {
      ROSE_ASSERT(record != nullptr);
      ROSE_ASSERT(record->getAttachedOwner() == emptyParameters);
      ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::inside);
      ROSE_ASSERT(samePhysicalFile(record->get_file_info(),
                                   emptyParameters->get_startOfConstruct()));
      if (record->getTypeOfDirective() ==
          PreprocessingInfo::CpreprocessorIfDeclaration) {
        ++openingCount;
      } else if (record->getTypeOfDirective() ==
                 PreprocessingInfo::CpreprocessorEndifDeclaration) {
        ++closingCount;
      }
    }
    ROSE_ASSERT(openingCount == 1);
    ROSE_ASSERT(closingCount == 1);
    return 0;
  }

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
  SgInitializedName *conditionalRequired = nullptr;
  SgInitializedName *conditionalOptional = nullptr;
  bool conditionalDeclaratorEnabled = false;
  if (!emptyTranslationUnit) {
    for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
      SgInitializedName *initializedName = isSgInitializedName(node);
      ROSE_ASSERT(initializedName != nullptr);
      if (initializedName->get_name() ==
          "rex_preprocessing_conditional_group_required") {
        ROSE_ASSERT(conditionalRequired == nullptr);
        conditionalRequired = initializedName;
      } else if (initializedName->get_name() ==
                 "rex_preprocessing_conditional_group_optional") {
        ROSE_ASSERT(conditionalOptional == nullptr);
        conditionalOptional = initializedName;
      }
    }
    ROSE_ASSERT(conditionalRequired != nullptr);
    conditionalDeclaratorEnabled = conditionalOptional != nullptr;
    SgVariableDeclaration *conditionalMember = isSgVariableDeclaration(
        (conditionalDeclaratorEnabled ? conditionalOptional
                                      : conditionalRequired)
            ->get_parent());
    ROSE_ASSERT(conditionalMember != nullptr);
    SgDeclarationGroupStatement *conditionalGroup =
        isSgDeclarationGroupStatement(conditionalMember->get_parent());
    ROSE_ASSERT(conditionalGroup != nullptr);
    conditionalGroup->validate();
    ROSE_ASSERT(conditionalGroup->get_declarations().size() ==
                (conditionalDeclaratorEnabled ? 2 : 1));
  }
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
      if (containsMarker(record, "REX_PREPROCESSING_SAME_LINE_COMMENT")) {
        markers.insert("same-line-comment");
        ROSE_ASSERT(isSgExprStatement(located) != nullptr);
        ROSE_ASSERT(record->getAttachedOwner() == located);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::after);
        ROSE_ASSERT(record->getOutputPlacement() ==
                    PreprocessingInfo::source_position);
        Sg_File_Info *ownerEnd = located->get_endOfConstruct();
        Sg_File_Info *commentStart = record->get_file_info();
        ROSE_ASSERT(samePhysicalFile(ownerEnd, commentStart));
        ROSE_ASSERT(ownerEnd->get_line() == commentStart->get_line());
        ROSE_ASSERT(ownerEnd->get_col() < commentStart->get_col());
      }
      if (containsMarker(record, "REX_PREPROCESSING_DECLARATOR_BOUNDARY")) {
        markers.insert("declarator-boundary");
        SgInitializedName *initializedName = isSgInitializedName(located);
        ROSE_ASSERT(initializedName != nullptr);
        ROSE_ASSERT(initializedName->get_name() ==
                    "rex_preprocessing_group_second");
        SgVariableDeclaration *declaration =
            isSgVariableDeclaration(initializedName->get_parent());
        ROSE_ASSERT(declaration != nullptr);
        SgDeclarationGroupStatement *group =
            isSgDeclarationGroupStatement(declaration->get_parent());
        ROSE_ASSERT(group != nullptr);
        group->validate();
        ROSE_ASSERT(group->get_declarations().size() == 2);
        ROSE_ASSERT(group->get_declarations().back() == declaration);
        ROSE_ASSERT(record->getAttachedOwner() == initializedName);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
        ROSE_ASSERT(record->getOutputPlacement() ==
                    PreprocessingInfo::source_position);
        Sg_File_Info *boundary = initializedName->get_startOfConstruct();
        Sg_File_Info *comment = record->get_file_info();
        ROSE_ASSERT(samePhysicalFile(boundary, comment));
        ROSE_ASSERT(locationLeq(comment, boundary));
        ROSE_ASSERT(comment->get_line() < boundary->get_line());
      }
      if (containsMarker(record,
                         "REX_PREPROCESSING_CONDITIONAL_DECLARATOR_OPEN")) {
        markers.insert("conditional-declarator-open");
        SgInitializedName *initializedName = isSgInitializedName(located);
        ROSE_ASSERT(initializedName != nullptr);
        ROSE_ASSERT(initializedName == (conditionalDeclaratorEnabled
                                            ? conditionalOptional
                                            : conditionalRequired));
        ROSE_ASSERT(record->getAttachedOwner() == initializedName);
        ROSE_ASSERT(record->getRelativePosition() ==
                    (conditionalDeclaratorEnabled ? PreprocessingInfo::before
                                                  : PreprocessingInfo::after));
        ROSE_ASSERT(record->getTypeOfDirective() ==
                    PreprocessingInfo::CpreprocessorIfDeclaration);
      }
      if (containsMarker(record,
                         "REX_PREPROCESSING_CONDITIONAL_DECLARATOR_CLOSE")) {
        markers.insert("conditional-declarator-close");
        SgInitializedName *initializedName = isSgInitializedName(located);
        ROSE_ASSERT(initializedName != nullptr);
        ROSE_ASSERT(initializedName == (conditionalDeclaratorEnabled
                                            ? conditionalOptional
                                            : conditionalRequired));
        ROSE_ASSERT(record->getAttachedOwner() == initializedName);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::after);
        ROSE_ASSERT(record->getTypeOfDirective() ==
                    PreprocessingInfo::CpreprocessorEndifDeclaration);
      }
      if (record->getTypeOfDirective() == PreprocessingInfo::CSkippedToken &&
          containsMarker(record,
                         "rex_preprocessing_conditional_group_optional")) {
        markers.insert("conditional-declarator-skipped");
        ROSE_ASSERT(!conditionalDeclaratorEnabled);
        ROSE_ASSERT(located == conditionalRequired);
        ROSE_ASSERT(record->getAttachedOwner() == conditionalRequired);
        ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::after);
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
    std::set<std::string> expectedMarkers{"between",
                                          "conditional-declarator-close",
                                          "conditional-declarator-open",
                                          "declarator-boundary",
                                          "leading",
                                          "same-line-comment",
                                          "system-include",
                                          "trailing"};
    if (!conditionalDeclaratorEnabled) {
      expectedMarkers.insert("conditional-declarator-skipped");
    }
    ROSE_ASSERT(markers == expectedMarkers);
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
