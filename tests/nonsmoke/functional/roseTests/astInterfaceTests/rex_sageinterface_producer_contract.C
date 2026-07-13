#include "rose.h"

#include "RoseAst.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

struct GeneratedUnit {
  SgProject *project;
  SgSourceFile *source;
  SgGlobal *global;
};

struct FileInfoSnapshot {
  Sg_File_Info *position;
  unsigned int classification;
  std::string rawFilename;
  int rawLine;
  int rawColumn;
  int physicalFileId;
  int physicalLine;
  unsigned int sourceSequence;
};

GeneratedUnit makeGeneratedUnit(const std::string &stem,
                                const std::string &extension = ".cpp") {
  const std::string output =
      std::filesystem::absolute(stem + extension).lexically_normal().string();
  SgProject *project = new SgProject();
  ROSE_ASSERT(project != nullptr);
  project->get_fileList().clear();
  project->set_compileOnly(true);
  project->get_originalCommandLineArgumentList() = {"c++", "-c"};
  SgSourceFile *source = SageBuilder::buildGeneratedSourceFile(output, project);
  ROSE_ASSERT(source != nullptr && source->get_globalScope() != nullptr);
  return {project, source, source->get_globalScope()};
}

SgVariableDeclaration *makeVariable(const std::string &name, SgGlobal *global) {
  SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
      name, SageBuilder::buildIntType(), nullptr, global);
  ROSE_ASSERT(declaration != nullptr);
  return declaration;
}

PreprocessingInfo *makeRecord(PreprocessingInfo::DirectiveType directive =
                                  PreprocessingInfo::C_StyleComment,
                              PreprocessingInfo::RelativePositionType position =
                                  PreprocessingInfo::before) {
  PreprocessingInfo *record = new PreprocessingInfo(
      directive, "record", "transformation-generated", 0, 0, 0, position);
  ROSE_ASSERT(record != nullptr && record->get_file_info() != nullptr);
  // This record has a logical generation provenance but no physical output
  // owner until publishGeneratedPreprocessingInfo attaches it.
  record->get_file_info()->set_physical_file_id(Sg_File_Info::NULL_FILE_ID);
  return record;
}

std::vector<FileInfoSnapshot> captureFileInfoState(SgNode *root) {
  ROSE_ASSERT(root != nullptr);
  std::vector<FileInfoSnapshot> result;
  std::unordered_set<Sg_File_Info *> visited;
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgLocatedNode *located = isSgLocatedNode(*current);
    if (located == nullptr) {
      continue;
    }
    std::vector<Sg_File_Info *> positions = {located->get_file_info(),
                                             located->get_startOfConstruct(),
                                             located->get_endOfConstruct()};
    if (SgExpression *expression = isSgExpression(located)) {
      positions.push_back(expression->get_operatorPosition());
    }
    for (Sg_File_Info *position : positions) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->get_parent() == located);
      if (!visited.insert(position).second) {
        continue;
      }
      result.push_back({position, position->get_classificationBitField(),
                        position->get_raw_filename(), position->get_raw_line(),
                        position->get_raw_col(),
                        position->get_physical_file_id(),
                        position->get_physical_line(),
                        position->get_source_sequence_number()});
    }
  }
  ROSE_ASSERT(!result.empty());
  return result;
}

void requirePublishedPhysicalOwnerPreservesSemanticState(
    const std::vector<FileInfoSnapshot> &before, SgSourceFile *source) {
  ROSE_ASSERT(source != nullptr && source->get_file_info() != nullptr);
  const int physicalFileId = source->get_file_info()->get_physical_file_id();
  const std::string physicalFilename =
      source->get_file_info()->get_physical_filename();
  ROSE_ASSERT(physicalFileId >= 0 && !physicalFilename.empty());
  for (const FileInfoSnapshot &snapshot : before) {
    const bool generatedOrigin =
        (snapshot.classification & (Sg_File_Info::e_transformation |
                                    Sg_File_Info::e_compiler_generated)) != 0;
    const unsigned int expectedClassification =
        generatedOrigin ? snapshot.classification |
                              Sg_File_Info::e_output_in_code_generation
                        : snapshot.classification;
    ROSE_ASSERT(snapshot.position != nullptr);
    ROSE_ASSERT(snapshot.position->get_physical_file_id() == physicalFileId);
    ROSE_ASSERT(snapshot.position->get_physical_filename() == physicalFilename);
    ROSE_ASSERT(snapshot.position->get_classificationBitField() ==
                expectedClassification);
    ROSE_ASSERT(snapshot.position->get_raw_filename() == snapshot.rawFilename);
    ROSE_ASSERT(snapshot.position->get_raw_line() == snapshot.rawLine);
    ROSE_ASSERT(snapshot.position->get_raw_col() == snapshot.rawColumn);
    ROSE_ASSERT(snapshot.position->get_physical_line() ==
                snapshot.physicalLine);
    ROSE_ASSERT(snapshot.position->get_source_sequence_number() ==
                snapshot.sourceSequence);
  }
}

bool requireExactSemanticNonrealProvenance(SgLocatedNode *located) {
  SgNonrealDecl *declaration = isSgNonrealDecl(located);
  SgDeclarationScope *childScope = isSgDeclarationScope(located);
  SgDeclarationScopeList *scopeContainer = isSgDeclarationScopeList(located);
  if (scopeContainer != nullptr) {
    SgScopeStatement *lexicalOwner =
        isSgScopeStatement(scopeContainer->get_parent());
    if (lexicalOwner == nullptr ||
        lexicalOwner->get_auxiliary_declaration_scopes() != scopeContainer) {
      return false;
    }
    for (SgDeclarationScope *ownedScope : scopeContainer->get_scopes()) {
      ROSE_ASSERT(ownedScope != nullptr &&
                  ownedScope->get_parent() == scopeContainer);
      ROSE_ASSERT(std::count(scopeContainer->get_scopes().begin(),
                             scopeContainer->get_scopes().end(),
                             ownedScope) == 1);
    }
  } else if (declaration == nullptr && childScope != nullptr &&
             childScope->get_is_default_nonreal_scope()) {
    SgDeclarationScopeList *container =
        isSgDeclarationScopeList(childScope->get_parent());
    SgScopeStatement *lexicalOwner =
        container != nullptr ? isSgScopeStatement(container->get_parent())
                             : nullptr;
    ROSE_ASSERT(container != nullptr && lexicalOwner != nullptr);
    ROSE_ASSERT(lexicalOwner->get_auxiliary_declaration_scopes() == container);
    ROSE_ASSERT(std::count(container->get_scopes().begin(),
                           container->get_scopes().end(), childScope) == 1);
  } else if (declaration == nullptr && childScope != nullptr) {
    declaration = isSgNonrealDecl(childScope->get_parent());
    if (declaration == nullptr) {
      return false;
    }
    ROSE_ASSERT(SageBuilder::getNonrealDeclarationScope(declaration) ==
                childScope);
  } else if (declaration == nullptr) {
    return false;
  } else {
    ROSE_ASSERT(declaration->get_type() != nullptr);
    ROSE_ASSERT(declaration->get_type()->get_declaration() == declaration);
    ROSE_ASSERT(declaration->get_type()->get_parent() == declaration);
    childScope = SageBuilder::getNonrealDeclarationScope(declaration);
    ROSE_ASSERT(childScope != nullptr);
    ROSE_ASSERT(childScope->get_parent() == declaration);
  }

  for (Sg_File_Info *position :
       {located->get_file_info(), located->get_startOfConstruct(),
        located->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_parent() == located);
    ROSE_ASSERT(!position->isShared());
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  }
  return true;
}

void requireExactLocatedOwnership(SgNode *root, SgSourceFile *source) {
  ROSE_ASSERT(root != nullptr && source != nullptr);
  const int physicalFileId = source->get_file_info()->get_physical_file_id();
  ROSE_ASSERT(physicalFileId >= 0);
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgLocatedNode *located = isSgLocatedNode(*current);
    if (located == nullptr) {
      continue;
    }
    // Nonreal declarations and their child declaration scopes are transparent
    // semantic lookup identities.  They own no independent output surface and
    // must retain exact compiler-generated provenance even when the enclosing
    // source tree has a concrete physical owner.
    if (requireExactSemanticNonrealProvenance(located)) {
      continue;
    }
    std::vector<Sg_File_Info *> positions = {located->get_file_info(),
                                             located->get_startOfConstruct(),
                                             located->get_endOfConstruct()};
    if (SgExpression *expression = isSgExpression(located)) {
      positions.push_back(expression->get_operatorPosition());
    }
    for (Sg_File_Info *position : positions) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(!position->isShared());
      ROSE_ASSERT(position->get_physical_file_id() == physicalFileId);
      ROSE_ASSERT(position->get_physical_filename() ==
                  source->get_file_info()->get_physical_filename());
    }
  }
}

SgCastExp *makeSynthesizedImplicitConversion() {
  SgCastExp *cast = SageBuilder::buildCastExp(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType(),
      SgCastExp::e_implicit_cast, SgCastExp::e_semantic_conversion_NoOp,
      SgCastExp::e_value_category_prvalue, {});
  ROSE_ASSERT(cast != nullptr && cast->get_operand() != nullptr);
  for (Sg_File_Info *position :
       {cast->get_file_info(), cast->get_startOfConstruct(),
        cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
    ROSE_ASSERT(position != nullptr);
    position->unsetTransformation();
    position->setCompilerGenerated();
    position->setOutputInCodeGeneration();
    position->setImplicitCast();
  }
  return cast;
}

void classifyFrontendSemanticPlaceholder(SgLocatedNode *located) {
  ROSE_ASSERT(located != nullptr);
  std::vector<Sg_File_Info *> positions;
  auto addPosition = [&positions](Sg_File_Info *position) {
    ROSE_ASSERT(position != nullptr);
    if (std::find(positions.begin(), positions.end(), position) ==
        positions.end()) {
      positions.push_back(position);
    }
  };
  addPosition(located->get_file_info());
  addPosition(located->get_startOfConstruct());
  addPosition(located->get_endOfConstruct());
  if (SgExpression *expression = isSgExpression(located)) {
    addPosition(expression->get_operatorPosition());
  }
  for (Sg_File_Info *position : positions) {
    position->setCompilerGenerated();
    position->setFrontendSpecific();
    position->unsetTransformation();
    position->unsetSourcePositionUnavailableInFrontend();
    position->setOutputInCodeGeneration();
    position->set_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    position->set_physical_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    position->set_parent(located);
  }
}

void requireFrontendSemanticPlaceholder(SgLocatedNode *located) {
  ROSE_ASSERT(located != nullptr);
  std::vector<Sg_File_Info *> positions = {located->get_file_info(),
                                           located->get_startOfConstruct(),
                                           located->get_endOfConstruct()};
  if (SgExpression *expression = isSgExpression(located)) {
    positions.push_back(expression->get_operatorPosition());
  }
  for (Sg_File_Info *position : positions) {
    ROSE_ASSERT(position != nullptr && position->get_parent() == located);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isFrontendSpecific());
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
    ROSE_ASSERT(position->get_physical_file_id() ==
                Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  }
}

void exerciseHeaderAnchors(bool nonempty, bool tokenMode) {
  GeneratedUnit unit = makeGeneratedUnit(
      std::string("rex_sageinterface_header_") +
      (nonempty ? "nonempty_" : "empty_") + (tokenMode ? "token" : "ast"));
  unit.source->set_unparse_tokens(tokenMode);
  if (nonempty) {
    SageInterface::appendStatement(
        makeVariable("rex_header_owner", unit.global), unit.global);
  }

  PreprocessingInfo *middle = SageInterface::insertHeader(
      unit.source, "rex_middle.hpp", false, PreprocessingInfo::before);
  PreprocessingInfo *last = SageInterface::insertHeader(
      "rex_last.hpp", PreprocessingInfo::after, false, unit.global);
  PreprocessingInfo *first =
      SageInterface::insertHeader(unit.source, "rex_first.hpp", false, false);
  ROSE_ASSERT(first != nullptr && middle != nullptr && last != nullptr);

  std::vector<SgEmptyDeclaration *> anchors;
  for (SgDeclarationStatement *declaration : unit.global->get_declarations()) {
    SgEmptyDeclaration *anchor = isSgEmptyDeclaration(declaration);
    if (anchor == nullptr) {
      continue;
    }
    anchor->validate_lexical_role();
    if (anchor->get_lexical_role() ==
        SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor) {
      anchors.push_back(anchor);
    }
  }
  ROSE_ASSERT(anchors.size() == 3);
  ROSE_ASSERT(anchors[0]->getAttachedPreprocessingInfo()->front() == first);
  ROSE_ASSERT(anchors[1]->getAttachedPreprocessingInfo()->front() == middle);
  ROSE_ASSERT(anchors[2]->getAttachedPreprocessingInfo()->front() == last);
  for (SgEmptyDeclaration *anchor : anchors) {
    for (Sg_File_Info *position :
         {anchor->get_file_info(), anchor->get_startOfConstruct(),
          anchor->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr);
      ROSE_ASSERT(position->isTransformation());
      ROSE_ASSERT(!position->isCompilerGenerated());
      ROSE_ASSERT(position->isOutputInCodeGeneration());
      ROSE_ASSERT(position->get_physical_file_id() ==
                  unit.source->get_file_info()->get_physical_file_id());
      ROSE_ASSERT(position->get_physical_filename() ==
                  unit.source->get_file_info()->get_physical_filename());
    }
    ROSE_ASSERT(anchor->getAttachedPreprocessingInfo()->size() == 1);
    PreprocessingInfo *record = anchor->getAttachedPreprocessingInfo()->front();
    ROSE_ASSERT(record->isTransformation());
    ROSE_ASSERT(!record->get_file_info()->isTransformation());
    ROSE_ASSERT(record->get_file_info()->isOutputInCodeGeneration());
  }
  requireExactLocatedOwnership(unit.global, unit.source);
}

int exercisePositiveContracts() {
  GeneratedUnit unit = makeGeneratedUnit("rex_sageinterface_producer_positive");

  Sg_File_Info *detachedTransformation = new Sg_File_Info("NULL_FILE", 0, 0);
  ROSE_ASSERT(detachedTransformation != nullptr);
  detachedTransformation->setTransformation();
  ROSE_ASSERT(detachedTransformation->get_raw_physical_file_id() ==
              Sg_File_Info::NULL_FILE_ID);
  ROSE_ASSERT(detachedTransformation->get_physical_file_id() ==
              Sg_File_Info::NULL_FILE_ID);
  ROSE_ASSERT(detachedTransformation->get_physical_filename() == "NULL_FILE");
  Sg_File_Info *detachedCompilerGenerated = new Sg_File_Info("NULL_FILE", 0, 0);
  ROSE_ASSERT(detachedCompilerGenerated != nullptr);
  detachedCompilerGenerated->setCompilerGenerated();
  ROSE_ASSERT(detachedCompilerGenerated->get_raw_physical_file_id() ==
              Sg_File_Info::NULL_FILE_ID);
  ROSE_ASSERT(detachedCompilerGenerated->get_physical_file_id() ==
              Sg_File_Info::NULL_FILE_ID);
  ROSE_ASSERT(detachedCompilerGenerated->get_physical_filename() ==
              "NULL_FILE");

  const size_t initialDeclarationCount = unit.global->get_declarations().size();
  SageInterface::appendStatementList({}, unit.global);
  SageInterface::prependStatementList({}, unit.global);
  ROSE_ASSERT(unit.global->get_declarations().size() ==
              initialDeclarationCount);

  SgVariableDeclaration *temporaryAnchor =
      makeVariable("rex_temporary_anchor", unit.global);
  SageInterface::appendStatement(temporaryAnchor, unit.global);
  SgIntVal *temporaryInput = SageBuilder::buildIntVal(7);
  auto temporary = SageInterface::createTempVariableForExpression(
      temporaryInput, temporaryAnchor, true);
  ROSE_ASSERT(temporary.first != nullptr && temporary.second != nullptr);
  ROSE_ASSERT(temporary.first->get_parent() == unit.global);
  ROSE_ASSERT(temporary.first->get_variables().size() == 1);
  ROSE_ASSERT(temporary.first->get_variables().front()->get_scope() ==
              unit.global);
  SgVariableSymbol *temporarySymbol =
      isSgVariableSymbol(unit.global->find_symbol_from_declaration(
          temporary.first->get_variables().front()));
  ROSE_ASSERT(temporarySymbol != nullptr);
  ROSE_ASSERT(temporarySymbol->get_declaration() ==
              temporary.first->get_variables().front());
  auto temporaryPosition =
      std::find(unit.global->get_declarations().begin(),
                unit.global->get_declarations().end(), temporary.first);
  ROSE_ASSERT(temporaryPosition != unit.global->get_declarations().end());
  ROSE_ASSERT(std::next(temporaryPosition) !=
              unit.global->get_declarations().end());
  ROSE_ASSERT(*std::next(temporaryPosition) == temporaryAnchor);

  SgNonrealType *dependentType = SageBuilder::buildSemanticNonrealType(
      SgName("rex_explicit_nonreal_scope"), unit.global, nullptr, nullptr);
  SgNonrealDecl *dependentDeclaration =
      dependentType != nullptr
          ? isSgNonrealDecl(dependentType->get_declaration())
          : nullptr;
  ROSE_ASSERT(dependentDeclaration != nullptr);
  ROSE_ASSERT(dependentDeclaration->get_scope() != nullptr);
  ROSE_ASSERT(dependentDeclaration->get_parent() ==
              dependentDeclaration->get_scope());
  ROSE_ASSERT(dependentType->get_declaration() == dependentDeclaration);
  ROSE_ASSERT(dependentType->get_parent() == dependentDeclaration);
  SgDeclarationScope *dependentChildScope =
      SageBuilder::getNonrealDeclarationScope(dependentDeclaration);
  ROSE_ASSERT(dependentChildScope != nullptr);
  ROSE_ASSERT(dependentChildScope->get_parent() == dependentDeclaration);
  ROSE_ASSERT(requireExactSemanticNonrealProvenance(dependentDeclaration));
  ROSE_ASSERT(requireExactSemanticNonrealProvenance(dependentChildScope));
  SgSymbol *dependentSymbol =
      dependentDeclaration->get_scope()->find_symbol_from_declaration(
          dependentDeclaration);
  ROSE_ASSERT(dependentSymbol != nullptr);
  ROSE_ASSERT(dependentSymbol->get_symbol_basis() == dependentDeclaration);

  // A generated subtree's semantic origin and raw source metadata are
  // independent of its physical output owner.  Publishing must retain the
  // exact compiler-generated classification while assigning the complete
  // expression subtree one concrete physical output identity.
  SgCastExp *generatedCast = SageBuilder::buildCastExp(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType(),
      SgCastExp::e_C_style_cast);
  RoseAst generatedAst(generatedCast);
  for (RoseAst::iterator current = generatedAst.begin();
       current != generatedAst.end(); ++current) {
    SgLocatedNode *located = isSgLocatedNode(*current);
    if (located == nullptr) {
      continue;
    }
    std::vector<Sg_File_Info *> positions = {located->get_file_info(),
                                             located->get_startOfConstruct(),
                                             located->get_endOfConstruct()};
    if (SgExpression *expression = isSgExpression(located)) {
      positions.push_back(expression->get_operatorPosition());
    }
    for (Sg_File_Info *position : positions) {
      ROSE_ASSERT(position != nullptr);
      position->unsetTransformation();
      position->setCompilerGenerated();
    }
  }
  const std::vector<FileInfoSnapshot> generatedCastBefore =
      captureFileInfoState(generatedCast);
  SageInterface::publishGeneratedSubtreeOutputOwner(generatedCast, unit.global);
  requireExactLocatedOwnership(generatedCast, unit.source);
  requirePublishedPhysicalOwnerPreservesSemanticState(generatedCastBefore,
                                                      unit.source);
  for (const FileInfoSnapshot &snapshot : generatedCastBefore) {
    ROSE_ASSERT(snapshot.position->isCompilerGenerated());
    ROSE_ASSERT(!snapshot.position->isTransformation());
  }

  // Physical output ownership includes the lexical file occurrence, not just
  // its filename.  Generated nodes and attached preprocessing must acquire
  // that exact occurrence, and an explicit relocation may move them between
  // two occurrences of the same physical file.
  SgVariableDeclaration *firstOccurrenceOwner =
      makeVariable("rex_first_occurrence_owner", unit.global);
  SageInterface::appendStatement(firstOccurrenceOwner, unit.global);
  SgVariableDeclaration *secondOccurrenceOwner =
      makeVariable("rex_second_occurrence_owner", unit.global);
  SageInterface::appendStatement(secondOccurrenceOwner, unit.global);
  constexpr unsigned int firstOccurrence = 701;
  constexpr unsigned int secondOccurrence = 702;
  for (Sg_File_Info *position : {firstOccurrenceOwner->get_file_info(),
                                 firstOccurrenceOwner->get_startOfConstruct(),
                                 firstOccurrenceOwner->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    position->set_physical_file_occurrence_id(firstOccurrence);
  }
  for (Sg_File_Info *position : {secondOccurrenceOwner->get_file_info(),
                                 secondOccurrenceOwner->get_startOfConstruct(),
                                 secondOccurrenceOwner->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr);
    position->set_physical_file_occurrence_id(secondOccurrence);
  }

  SgCastExp *occurrenceCast = SageBuilder::buildCastExp(
      SageBuilder::buildIntVal(2), SageBuilder::buildIntType(),
      SgCastExp::e_C_style_cast);
  SageInterface::publishGeneratedSubtreeOutputOwner(occurrenceCast,
                                                    firstOccurrenceOwner);
  for (const FileInfoSnapshot &snapshot :
       captureFileInfoState(occurrenceCast)) {
    ROSE_ASSERT(snapshot.position->get_physical_file_occurrence_id() ==
                firstOccurrence);
  }
  SageInterface::relocateGeneratedSubtreePhysicalOutputOwner(
      occurrenceCast, firstOccurrenceOwner, secondOccurrenceOwner);
  for (const FileInfoSnapshot &snapshot :
       captureFileInfoState(occurrenceCast)) {
    ROSE_ASSERT(snapshot.position->get_physical_file_occurrence_id() ==
                secondOccurrence);
  }

  PreprocessingInfo *occurrenceRecord = makeRecord();
  SageInterface::publishGeneratedPreprocessingInfo(occurrenceRecord,
                                                   firstOccurrenceOwner);
  ROSE_ASSERT(
      occurrenceRecord->get_file_info()->get_physical_file_occurrence_id() ==
      firstOccurrence);
  SageInterface::relocateAttachedPreprocessingInfoPhysicalOutputOwner(
      occurrenceRecord, firstOccurrenceOwner, secondOccurrenceOwner);
  ROSE_ASSERT(
      occurrenceRecord->get_file_info()->get_physical_file_occurrence_id() ==
      secondOccurrence);

  // Frontend placeholders for absent for-header syntax are semantic edges,
  // not generated output surfaces.  Publishing an enclosing transformed loop
  // must leave their negative compiler-generated physical sentinel intact.
  SgNullStatement *absentInitializer = SageBuilder::buildNullStatement();
  SgForInitStatement *forInit =
      SageBuilder::buildForInitStatement(absentInitializer);
  SgNullStatement *absentCondition = SageBuilder::buildNullStatement();
  SgNullExpression *absentIncrement = SageBuilder::buildNullExpression(
      SgNullExpression::e_null_expression_syntactic_absence);
  SgNullStatement *loopBody = SageBuilder::buildNullStatement();
  SgForStatement *generatedLoop = SageBuilder::buildForStatement(
      forInit, absentCondition, absentIncrement, loopBody);
  ROSE_ASSERT(generatedLoop != nullptr);
  for (SgLocatedNode *placeholder :
       {static_cast<SgLocatedNode *>(forInit),
        static_cast<SgLocatedNode *>(absentInitializer),
        static_cast<SgLocatedNode *>(absentCondition),
        static_cast<SgLocatedNode *>(absentIncrement)}) {
    classifyFrontendSemanticPlaceholder(placeholder);
  }
  SageInterface::publishGeneratedSubtreeOutputOwner(generatedLoop, unit.global);
  for (SgLocatedNode *placeholder :
       {static_cast<SgLocatedNode *>(forInit),
        static_cast<SgLocatedNode *>(absentInitializer),
        static_cast<SgLocatedNode *>(absentCondition),
        static_cast<SgLocatedNode *>(absentIncrement)}) {
    requireFrontendSemanticPlaceholder(placeholder);
  }
  for (SgLocatedNode *written : {static_cast<SgLocatedNode *>(generatedLoop),
                                 static_cast<SgLocatedNode *>(loopBody)}) {
    ROSE_ASSERT(written->get_file_info()->get_physical_file_id() ==
                unit.source->get_file_info()->get_physical_file_id());
  }

  // Transformation origin is equally persistent.  In particular, publishing
  // an operator-bearing expression cannot turn copied transformation nodes
  // into apparent source nodes merely to make physical accessors report the
  // output file.
  SgExpression *generatedSum = SageBuilder::buildAddOp(
      SageBuilder::buildIntVal(2), SageBuilder::buildIntVal(3),
      SageBuilder::buildIntType());
  ROSE_ASSERT(generatedSum != nullptr);
  const std::vector<FileInfoSnapshot> generatedSumBefore =
      captureFileInfoState(generatedSum);
  for (const FileInfoSnapshot &snapshot : generatedSumBefore) {
    ROSE_ASSERT(snapshot.position->isTransformation());
  }
  SageInterface::publishGeneratedSubtreeOutputOwner(generatedSum, unit.global);
  requirePublishedPhysicalOwnerPreservesSemanticState(generatedSumBefore,
                                                      unit.source);
  for (const FileInfoSnapshot &snapshot : generatedSumBefore) {
    ROSE_ASSERT(snapshot.position->isTransformation());
  }

  // A compiler-synthesized implicit conversion is a transparent semantic
  // wrapper around its source operand, not a generated output surface.  An
  // enclosing statement mutation must preserve that typed provenance while
  // publishing independently generated descendants.
  SgCastExp *implicitConversion = makeSynthesizedImplicitConversion();
  const std::vector<FileInfoSnapshot> implicitConversionBefore =
      captureFileInfoState(implicitConversion);
  SageInterface::publishGeneratedSubtreeOutputOwner(implicitConversion,
                                                    unit.global);
  requirePublishedPhysicalOwnerPreservesSemanticState(implicitConversionBefore,
                                                      unit.source);
  for (Sg_File_Info *position : {implicitConversion->get_file_info(),
                                 implicitConversion->get_startOfConstruct(),
                                 implicitConversion->get_endOfConstruct(),
                                 implicitConversion->get_operatorPosition()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->isCompilerGenerated());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->isImplicitCast());
    ROSE_ASSERT(!position->isTransformation());
  }
  requireExactLocatedOwnership(implicitConversion->get_operand(), unit.source);

  // Marking a subtree for output must not turn its transparent implicit
  // conversion wrapper into written cast syntax.  The source operand remains
  // independently eligible for transformation output.
  const unsigned int implicitClassification =
      implicitConversion->get_file_info()->get_classificationBitField();
  SageInterface::markSubtreeToBeUnparsed(
      implicitConversion, unit.source->get_file_info()->get_physical_file_id());
  for (Sg_File_Info *position : {implicitConversion->get_file_info(),
                                 implicitConversion->get_startOfConstruct(),
                                 implicitConversion->get_endOfConstruct(),
                                 implicitConversion->get_operatorPosition()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->get_classificationBitField() ==
                implicitClassification);
    ROSE_ASSERT(!position->isTransformation());
    ROSE_ASSERT(position->isImplicitCast());
  }
  for (Sg_File_Info *position :
       {implicitConversion->get_operand()->get_file_info(),
        implicitConversion->get_operand()->get_startOfConstruct(),
        implicitConversion->get_operand()->get_endOfConstruct(),
        implicitConversion->get_operand()->get_operatorPosition()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->isTransformation());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
    ROSE_ASSERT(position->get_physical_file_id() ==
                unit.source->get_file_info()->get_physical_file_id());
  }

  SgCastExp *explicitConversion = SageBuilder::buildCastExp(
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType(),
      SgCastExp::e_C_style_cast);
  SageInterface::markSubtreeToBeUnparsed(
      explicitConversion, unit.source->get_file_info()->get_physical_file_id());
  for (Sg_File_Info *position : {explicitConversion->get_file_info(),
                                 explicitConversion->get_startOfConstruct(),
                                 explicitConversion->get_endOfConstruct(),
                                 explicitConversion->get_operatorPosition()}) {
    ROSE_ASSERT(position != nullptr);
    ROSE_ASSERT(position->isTransformation());
    ROSE_ASSERT(position->isOutputInCodeGeneration());
  }

  // A written cast from a function designator to a different callback type is
  // not itself FunctionToPointerDecay.  Preserve the mandatory exact decay as
  // a transparent inner conversion, then classify the requested pointer cast.
  SgFunctionDeclaration *callback =
      SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          SgName("rex_transformation_callback"), SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList(), unit.global);
  ROSE_ASSERT(callback != nullptr && callback->get_type() != nullptr &&
              callback->get_parent() == unit.global);
  SgFunctionRefExp *callbackReference =
      SageBuilder::buildFunctionRefExp(callback);
  ROSE_ASSERT(callbackReference != nullptr &&
              callbackReference->get_type() == callback->get_type());
  SgFunctionType *requestedCallbackType = SageBuilder::buildFunctionType(
      SageBuilder::buildVoidType(),
      SageBuilder::buildFunctionParameterTypeList(SageBuilder::buildIntType()));
  ROSE_ASSERT(requestedCallbackType != nullptr &&
              !SageInterface::isEquivalentType(callback->get_type(),
                                               requestedCallbackType));
  SgPointerType *requestedCallbackPointer =
      SageBuilder::buildPointerType(requestedCallbackType);
  SgCastExp *callbackCast = SageBuilder::buildCastExp(
      callbackReference, requestedCallbackPointer, SgCastExp::e_C_style_cast);
  ROSE_ASSERT(callbackCast != nullptr &&
              callbackCast->get_semantic_conversion_kind() ==
                  SgCastExp::e_semantic_conversion_BitCast);
  SgCastExp *callbackDecay = isSgCastExp(callbackCast->get_operand());
  ROSE_ASSERT(callbackDecay != nullptr &&
              callbackDecay->get_parent() == callbackCast &&
              callbackDecay->get_operand() == callbackReference &&
              callbackReference->get_parent() == callbackDecay);
  ROSE_ASSERT(callbackDecay->get_cast_type() == SgCastExp::e_implicit_cast &&
              callbackDecay->get_semantic_conversion_kind() ==
                  SgCastExp::e_semantic_conversion_FunctionToPointerDecay);
  SgPointerType *exactDecayType = isSgPointerType(callbackDecay->get_type());
  ROSE_ASSERT(exactDecayType != nullptr &&
              exactDecayType->get_base_type() == callbackReference->get_type());
  callbackDecay->validate_semantic_conversion();
  callbackCast->validate_semantic_conversion();

  // Publishing a statement in the global scope must not traverse and repair
  // pre-existing nested scopes.  Temporarily clear an unrelated scope's table
  // parent, verify it remains untouched, then restore the valid fixture state.
  SgNamespaceDeclarationStatement *unrelatedNamespace =
      SageBuilder::buildNamespaceDeclaration_nfi(
          SgName("rex_unrelated_symbol_scope"), false, unit.global,
          SageBuilder::e_namespace_declaration_canonical_generated_lexical,
          nullptr, nullptr, nullptr, std::nullopt);
  ROSE_ASSERT(unrelatedNamespace != nullptr);
  SgNamespaceDefinitionStatement *unrelatedScope =
      unrelatedNamespace->get_definition();
  ROSE_ASSERT(unrelatedScope != nullptr);
  SageInterface::publishGeneratedSubtreeOutputOwner(unrelatedNamespace,
                                                    unit.global);
  SgSymbolTable *unrelatedTable = unrelatedScope->get_symbol_table();
  ROSE_ASSERT(unrelatedTable != nullptr);
  const size_t unrelatedSymbolCount = unrelatedTable->size();
  unrelatedTable->set_parent(nullptr);
  SgVariableDeclaration *independentDeclaration =
      makeVariable("rex_independent_symbol_publication", unit.global);
  SageInterface::appendStatement(independentDeclaration, unit.global);
  ROSE_ASSERT(unrelatedTable->get_parent() == nullptr);
  ROSE_ASSERT(unrelatedTable->size() == unrelatedSymbolCount);
  unrelatedTable->set_parent(unrelatedScope);

  ROSE_ASSERT(independentDeclaration->get_file_info()->isTransformation());
  const std::vector<FileInfoSnapshot> messageOwnerBefore =
      captureFileInfoState(independentDeclaration);
  SageInterface::addMessageStatement(independentDeclaration,
                                     "/* REX GENERATED MESSAGE */");
  for (const FileInfoSnapshot &snapshot : messageOwnerBefore) {
    ROSE_ASSERT(snapshot.position->get_classificationBitField() ==
                snapshot.classification);
    ROSE_ASSERT(snapshot.position->get_raw_filename() == snapshot.rawFilename);
    ROSE_ASSERT(snapshot.position->get_raw_line() == snapshot.rawLine);
    ROSE_ASSERT(snapshot.position->get_raw_col() == snapshot.rawColumn);
    ROSE_ASSERT(snapshot.position->get_physical_file_id() ==
                snapshot.physicalFileId);
    ROSE_ASSERT(snapshot.position->get_physical_line() ==
                snapshot.physicalLine);
  }
  ROSE_ASSERT(independentDeclaration->getAttachedPreprocessingInfo() !=
              nullptr);
  ROSE_ASSERT(independentDeclaration->getAttachedPreprocessingInfo()->size() ==
              1);
  PreprocessingInfo *message =
      independentDeclaration->getAttachedPreprocessingInfo()->front();
  ROSE_ASSERT(message != nullptr && message->isTransformation());
  ROSE_ASSERT(message->get_file_info()->get_physical_file_id() ==
              independentDeclaration->get_file_info()->get_physical_file_id());

  SgVariableDeclaration *appended = makeVariable("rex_appended", unit.global);
  SageInterface::appendStatement(appended, unit.global);
  SgVariableDeclaration *prepended = makeVariable("rex_prepended", unit.global);
  SageInterface::prependStatement(prepended, unit.global);
  SgVariableDeclaration *inserted = makeVariable("rex_inserted", unit.global);
  SageInterface::insertStatement(appended, inserted, true, false);
  SgVariableDeclaration *replacement =
      makeVariable("rex_replacement", unit.global);
  SageInterface::replaceStatement(inserted, replacement);

  PreprocessingInfo *generatedRecord = makeRecord();
  Sg_File_Info *generatedRecordInfo = generatedRecord->get_file_info();
  generatedRecordInfo->setTransformation();
  const unsigned int generatedRecordClassification =
      generatedRecordInfo->get_classificationBitField();
  const std::string generatedRecordRawFilename =
      generatedRecordInfo->get_raw_filename();
  const int generatedRecordRawLine = generatedRecordInfo->get_raw_line();
  const int generatedRecordRawColumn = generatedRecordInfo->get_raw_col();
  const int generatedRecordPhysicalLine =
      generatedRecordInfo->get_physical_line();
  SageInterface::publishGeneratedPreprocessingInfo(generatedRecord,
                                                   replacement);
  ROSE_ASSERT(generatedRecord->isTransformation());
  ROSE_ASSERT(generatedRecordInfo->isTransformation());
  ROSE_ASSERT(generatedRecordInfo->get_classificationBitField() ==
              (generatedRecordClassification |
               Sg_File_Info::e_output_in_code_generation));
  ROSE_ASSERT(generatedRecordInfo->get_raw_filename() ==
              generatedRecordRawFilename);
  ROSE_ASSERT(generatedRecordInfo->get_raw_line() == generatedRecordRawLine);
  ROSE_ASSERT(generatedRecordInfo->get_raw_col() == generatedRecordRawColumn);
  ROSE_ASSERT(generatedRecordInfo->get_physical_line() ==
              generatedRecordPhysicalLine);
  ROSE_ASSERT(generatedRecordInfo->get_physical_file_id() ==
              replacement->get_file_info()->get_physical_file_id());
  ROSE_ASSERT(generatedRecordInfo->get_physical_filename() ==
              replacement->get_file_info()->get_physical_filename());

  PreprocessingInfo *comment = SageInterface::attachComment(
      replacement, "generated comment", PreprocessingInfo::C_StyleComment);
  PreprocessingInfo *definition =
      SageBuilder::buildCpreprocessorDefineDeclaration(
          replacement, "#define REX_GENERATED_VALUE 42\n");
  ROSE_ASSERT(comment != nullptr && definition != nullptr);
  ROSE_ASSERT(comment->isTransformation() && definition->isTransformation());

  SageInterface::movePreprocessingInfo(replacement, appended,
                                       PreprocessingInfo::before,
                                       PreprocessingInfo::after);
  AttachedPreprocessingInfoType savedRecords;
  SageInterface::cutPreprocessingInfo(appended, PreprocessingInfo::after,
                                      savedRecords);
  ROSE_ASSERT(savedRecords.size() == 2);
  SageInterface::pastePreprocessingInfo(prepended, PreprocessingInfo::before,
                                        savedRecords);
  ROSE_ASSERT(savedRecords.empty());
  ROSE_ASSERT(prepended->getAttachedPreprocessingInfo()->size() == 2);
  for (PreprocessingInfo *record : *prepended->getAttachedPreprocessingInfo()) {
    ROSE_ASSERT(record->isTransformation());
    ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
    ROSE_ASSERT(record->get_file_info()->get_physical_file_id() ==
                prepended->get_file_info()->get_physical_file_id());
  }

  requireExactLocatedOwnership(unit.global, unit.source);
  exerciseHeaderAnchors(false, false);
  exerciseHeaderAnchors(false, true);
  exerciseHeaderAnchors(true, false);
  exerciseHeaderAnchors(true, true);
  return 0;
}

int exerciseEmptyPreprocessingMoveToDetachedDestination() {
  GeneratedUnit unit =
      makeGeneratedUnit("rex_empty_preprocessing_move_detached_destination");
  SgVariableDeclaration *source =
      makeVariable("rex_empty_preprocessing_move_source", unit.global);
  SgNullStatement *detachedDestination = SageBuilder::buildNullStatement();
  ROSE_ASSERT(source != nullptr && detachedDestination != nullptr);
  SageInterface::appendStatement(source, unit.global);
  ROSE_ASSERT(source->get_parent() == unit.global);
  ROSE_ASSERT(source->getAttachedPreprocessingInfo() == nullptr);
  ROSE_ASSERT(detachedDestination->get_parent() == nullptr);
  ROSE_ASSERT(detachedDestination->getAttachedPreprocessingInfo() == nullptr);

  SageInterface::movePreprocessingInfo(source, detachedDestination,
                                       PreprocessingInfo::before,
                                       PreprocessingInfo::after, true);

  ROSE_ASSERT(source->get_parent() == unit.global);
  ROSE_ASSERT(source->getAttachedPreprocessingInfo() == nullptr);
  ROSE_ASSERT(detachedDestination->get_parent() == nullptr);
  ROSE_ASSERT(detachedDestination->getAttachedPreprocessingInfo() == nullptr);
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    return exercisePositiveContracts();
  }
  ROSE_ASSERT(argc == 2);
  const std::string mode = argv[1];

  if (mode == "--reject-detached-preprocessing-owner") {
    SageInterface::publishGeneratedPreprocessingInfo(
        makeRecord(), SageBuilder::buildNullStatement());
  }
  if (mode == "--check-empty-move-detached-destination") {
    return exerciseEmptyPreprocessingMoveToDetachedDestination();
  }
  if (mode == "--reject-detached-move-owner-with-record") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_detached_move_owner");
    SgVariableDeclaration *source =
        makeVariable("rex_detached_move_record_source", unit.global);
    SgNullStatement *detachedDestination = SageBuilder::buildNullStatement();
    ROSE_ASSERT(source != nullptr && detachedDestination != nullptr);
    SageInterface::appendStatement(source, unit.global);
    PreprocessingInfo *record = SageInterface::attachComment(
        source, "rex selected preprocessing record",
        PreprocessingInfo::C_StyleComment, PreprocessingInfo::before);
    ROSE_ASSERT(record != nullptr);
    ROSE_ASSERT(source->getAttachedPreprocessingInfo() != nullptr);
    ROSE_ASSERT(source->getAttachedPreprocessingInfo()->size() == 1);
    ROSE_ASSERT(detachedDestination->get_parent() == nullptr);
    SageInterface::movePreprocessingInfo(source, detachedDestination,
                                         PreprocessingInfo::before,
                                         PreprocessingInfo::after, true);
  }
  if (mode == "--fortran-generated-owner") {
    GeneratedUnit unit =
        makeGeneratedUnit("rex_sageinterface_generated_fortran_owner", ".f90");
    SgIntVal *kind = SageBuilder::buildIntVal_nfi("4");
    ROSE_ASSERT(kind != nullptr);
    SageBuilder::initializeSemanticExpressionSourceProvenance(kind);
    SgType *semanticType = SageBuilder::buildIntType(kind);
    ROSE_ASSERT(semanticType != nullptr);
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        "rex_fortran_generated_owner", semanticType, nullptr, unit.global);
    const std::vector<FileInfoSnapshot> before =
        captureFileInfoState(declaration);
    for (Sg_File_Info *position :
         {declaration->get_file_info(), declaration->get_startOfConstruct(),
          declaration->get_endOfConstruct()}) {
      ROSE_ASSERT(position != nullptr && position->isTransformation());
      ROSE_ASSERT(position->get_physical_file_id() < 0);
    }
    SageInterface::appendStatement(declaration, unit.global);
    requireExactLocatedOwnership(declaration, unit.source);
    requirePublishedPhysicalOwnerPreservesSemanticState(before, unit.source);
    for (Sg_File_Info *position :
         {declaration->get_file_info(), declaration->get_startOfConstruct(),
          declaration->get_endOfConstruct()}) {
      ROSE_ASSERT(position->isTransformation());
      ROSE_ASSERT(position->get_physical_file_id() ==
                  unit.source->get_file_info()->get_physical_file_id());
      ROSE_ASSERT(position->get_physical_filename() ==
                  unit.source->get_file_info()->get_physical_filename());
    }
    return 0;
  }
  if (mode == "--reject-unknown-preprocessing-kind") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_unknown_record");
    SageInterface::publishGeneratedPreprocessingInfo(
        makeRecord(PreprocessingInfo::CpreprocessorUnknownDeclaration),
        unit.global);
  }
  if (mode == "--reject-invalid-preprocessing-position") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_record_position");
    SageInterface::publishGeneratedPreprocessingInfo(
        makeRecord(PreprocessingInfo::C_StyleComment,
                   PreprocessingInfo::defaultValue),
        unit.global);
  }
  if (mode == "--reject-contradictory-preprocessing-physical-owner") {
    GeneratedUnit owner =
        makeGeneratedUnit("rex_preprocessing_physical_owner_primary");
    GeneratedUnit unrelated =
        makeGeneratedUnit("rex_preprocessing_physical_owner_contradiction");
    const int unrelatedId =
        unrelated.global->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(unrelatedId >= 0 &&
                unrelatedId !=
                    owner.global->get_file_info()->get_physical_file_id());
    PreprocessingInfo *record = makeRecord();
    record->get_file_info()->set_physical_file_id(unrelatedId);
    SageInterface::publishGeneratedPreprocessingInfo(record, owner.global);
  }
  if (mode == "--reject-impossible-stored-physical-id") {
    GeneratedUnit unit = makeGeneratedUnit("rex_impossible_physical_id");
    unit.global->get_file_info()->set_physical_file_id(
        Sg_File_Info::COPY_FILE_ID);
    (void)unit.global->get_file_info()->get_physical_file_id();
  }
  if (mode == "--reject-unowned-physical-id-query") {
    GeneratedUnit owner = makeGeneratedUnit("rex_physical_id_owner");
    GeneratedUnit unrelated = makeGeneratedUnit("rex_physical_id_unrelated");
    const int unrelatedId =
        unrelated.global->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(unrelatedId >= 0 &&
                unrelatedId !=
                    owner.global->get_file_info()->get_physical_file_id());
    (void)owner.global->get_file_info()->get_physical_file_id(unrelatedId);
  }
  if (mode == "--reject-unowned-logical-line-query") {
    GeneratedUnit owner = makeGeneratedUnit("rex_logical_line_owner");
    GeneratedUnit unrelated = makeGeneratedUnit("rex_logical_line_unrelated");
    const int unrelatedId =
        unrelated.global->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(unrelatedId >= 0 &&
                unrelatedId !=
                    owner.global->get_file_info()->get_physical_file_id());
    (void)owner.global->get_file_info()->get_line(unrelatedId);
  }
  if (mode == "--reject-unowned-physical-line-query") {
    GeneratedUnit owner = makeGeneratedUnit("rex_physical_line_owner");
    GeneratedUnit unrelated = makeGeneratedUnit("rex_physical_line_unrelated");
    const int unrelatedId =
        unrelated.global->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(unrelatedId >= 0 &&
                unrelatedId !=
                    owner.global->get_file_info()->get_physical_file_id());
    (void)owner.global->get_file_info()->get_physical_line(unrelatedId);
  }
  if (mode == "--reject-mixed-generated-source-origin") {
    GeneratedUnit unit = makeGeneratedUnit("rex_mixed_position_origin");
    SgCastExp *cast = SageBuilder::buildCastExp(SageBuilder::buildIntVal(1),
                                                SageBuilder::buildIntType(),
                                                SgCastExp::e_C_style_cast);
    ROSE_ASSERT(cast != nullptr && cast->get_endOfConstruct() != nullptr);
    const unsigned int generatedOriginMask =
        Sg_File_Info::e_transformation | Sg_File_Info::e_compiler_generated |
        Sg_File_Info::e_source_position_unavailable_in_frontend;
    cast->get_endOfConstruct()->set_classificationBitField(
        cast->get_endOfConstruct()->get_classificationBitField() &
        ~generatedOriginMask);
    SageInterface::publishGeneratedSubtreeOutputOwner(cast, unit.global);
  }
  if (mode == "--reject-contradictory-generated-physical-owners") {
    GeneratedUnit owner =
        makeGeneratedUnit("rex_generated_physical_owner_primary");
    GeneratedUnit unrelated =
        makeGeneratedUnit("rex_generated_physical_owner_contradiction");
    const int ownerId = owner.global->get_file_info()->get_physical_file_id();
    const int unrelatedId =
        unrelated.global->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(ownerId >= 0 && unrelatedId >= 0 && ownerId != unrelatedId);
    SgCastExp *cast = SageBuilder::buildCastExp(SageBuilder::buildIntVal(1),
                                                SageBuilder::buildIntType(),
                                                SgCastExp::e_C_style_cast);
    ROSE_ASSERT(cast != nullptr && cast->get_file_info() != nullptr &&
                cast->get_startOfConstruct() != nullptr &&
                cast->get_endOfConstruct() != nullptr &&
                cast->get_operatorPosition() != nullptr);
    cast->get_file_info()->set_physical_file_id(unrelatedId);
    cast->get_startOfConstruct()->set_physical_file_id(unrelatedId);
    cast->get_endOfConstruct()->set_physical_file_id(unrelatedId);
    cast->get_operatorPosition()->set_physical_file_id(unrelatedId);
    SageInterface::publishGeneratedSubtreeOutputOwner(cast, owner.global);
  }
  if (mode == "--reject-mixed-generated-physical-owners") {
    GeneratedUnit owner =
        makeGeneratedUnit("rex_generated_physical_mixed_primary");
    GeneratedUnit unrelated =
        makeGeneratedUnit("rex_generated_physical_mixed_contradiction");
    const int ownerId = owner.global->get_file_info()->get_physical_file_id();
    const int unrelatedId =
        unrelated.global->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(ownerId >= 0 && unrelatedId >= 0 && ownerId != unrelatedId);
    SgCastExp *cast = SageBuilder::buildCastExp(SageBuilder::buildIntVal(1),
                                                SageBuilder::buildIntType(),
                                                SgCastExp::e_C_style_cast);
    ROSE_ASSERT(cast != nullptr && cast->get_file_info() != nullptr &&
                cast->get_startOfConstruct() != nullptr &&
                cast->get_endOfConstruct() != nullptr &&
                cast->get_operatorPosition() != nullptr);
    cast->get_file_info()->set_physical_file_id(ownerId);
    cast->get_startOfConstruct()->set_physical_file_id(ownerId);
    cast->get_endOfConstruct()->set_physical_file_id(unrelatedId);
    cast->get_operatorPosition()->set_physical_file_id(unrelatedId);
    SageInterface::publishGeneratedSubtreeOutputOwner(cast, owner.global);
  }
  if (mode == "--reject-detached-message-owner") {
    SageInterface::addMessageStatement(SageBuilder::buildNullStatement(),
                                       "/* DETACHED MESSAGE */");
  }
  if (mode == "--reject-malformed-message-spelling") {
    GeneratedUnit unit = makeGeneratedUnit("rex_malformed_message_spelling");
    SgVariableDeclaration *owner =
        makeVariable("rex_message_owner", unit.global);
    SageInterface::appendStatement(owner, unit.global);
    SageInterface::addMessageStatement(
        owner, "/* first message */ code /* second message */");
  }
  if (mode == "--reject-null-append-scope") {
    SageInterface::appendStatement(SageBuilder::buildNullStatement(),
                                   static_cast<SgScopeStatement *>(nullptr));
  }
  if (mode == "--reject-null-prepend-scope") {
    SageInterface::prependStatement(SageBuilder::buildNullStatement(),
                                    static_cast<SgScopeStatement *>(nullptr));
  }
  if (mode == "--reject-null-append-list-scope") {
    SageInterface::appendStatementList({}, nullptr);
  }
  if (mode == "--reject-null-prepend-list-scope") {
    SageInterface::prependStatementList({}, nullptr);
  }
  if (mode == "--reject-null-temporary-variable-anchor") {
    (void)SageInterface::createTempVariableForExpression(
        SageBuilder::buildIntVal(1), static_cast<SgStatement *>(nullptr), true);
  }
  if (mode == "--reject-null-nonreal-scope") {
    SageBuilder::buildNonrealDecl(SgName("rex_missing_nonreal_scope"), nullptr);
  }
  if (mode == "--reject-null-nonreal-type-scope") {
    SageBuilder::buildSemanticNonrealType(
        SgName("rex_missing_nonreal_type_scope"),
        static_cast<SgScopeStatement *>(nullptr), nullptr, nullptr);
  }
  if (mode == "--reject-template-declaration-publication") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_generic_template");
    SgTemplateDeclaration *declaration =
        new SgTemplateDeclaration(SgName("rex_generic_template"));
    ROSE_ASSERT(declaration != nullptr);
    SageInterface::setOneSourcePositionForTransformation(declaration);
    SageInterface::appendStatement(declaration, unit.global);
  }
  if (mode == "--reject-missing-canonical-function-symbol") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_function_symbol");
    SgFunctionDeclaration *function =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_missing_function_symbol"), SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), unit.global);
    ROSE_ASSERT(function != nullptr);
    SgFunctionDeclaration *first =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr);
    SgFunctionSymbol *symbol =
        isSgFunctionSymbol(first->get_symbol_from_symbol_table());
    ROSE_ASSERT(symbol != nullptr);
    unit.global->remove_symbol(symbol);
    ROSE_ASSERT(!unit.global->symbol_exists(symbol));
    SageInterface::updateDefiningNondefiningLinks(function, unit.global);
  }
  if (mode == "--reject-malformed-implicit-conversion-role") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_implicit_conversion");
    SgCastExp *cast = makeSynthesizedImplicitConversion();
    cast->get_endOfConstruct()->unsetImplicitCast();
    SageInterface::publishGeneratedSubtreeOutputOwner(cast, unit.global);
  }
  if (mode == "--reject-misowned-frontend-for-placeholder") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_misowned_for_absence");
    SgNullStatement *placeholder = SageBuilder::buildNullStatement();
    SgForStatement *owner = SageBuilder::buildForStatement(
        SageBuilder::buildForInitStatement(SageBuilder::buildNullStatement()),
        SageBuilder::buildNullStatement(),
        SageBuilder::buildNullExpression(
            SgNullExpression::e_null_expression_syntactic_absence),
        placeholder);
    ROSE_ASSERT(owner != nullptr && placeholder->get_parent() == owner &&
                owner->get_loop_body() == placeholder);
    classifyFrontendSemanticPlaceholder(placeholder);
    SageInterface::publishGeneratedSubtreeOutputOwner(placeholder, unit.global);
  }
  if (mode == "--reject-malformed-marked-implicit-conversion-role") {
    SgCastExp *cast = makeSynthesizedImplicitConversion();
    cast->get_operatorPosition()->unsetImplicitCast();
    SageInterface::markSubtreeToBeUnparsed(cast, 0);
  }
  if (mode == "--reject-comment-style") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_comment_style");
    SageInterface::attachComment(
        unit.global, "not a pragma",
        PreprocessingInfo::CpreprocessorPragmaDeclaration);
  }
  if (mode == "--reject-spelled-comment") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_spelled_comment");
    SageInterface::attachComment(unit.global, "// already spelled",
                                 PreprocessingInfo::CplusplusStyleComment);
  }
  if (mode == "--reject-malformed-define") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_malformed_define");
    SageBuilder::buildCpreprocessorDefineDeclaration(unit.global, "#define");
  }
  if (mode == "--reject-detached-define-owner") {
    SageBuilder::buildCpreprocessorDefineDeclaration(
        SageBuilder::buildNullStatement(), "#define REX_DETACHED 1\n");
  }
  if (mode == "--reject-detached-header-owner") {
    SgGlobal *detached = new SgGlobal();
    SageInterface::insertHeader("rex_detached.hpp", PreprocessingInfo::before,
                                false, detached);
  }
  if (mode == "--reject-detached-statement-scope") {
    SgEmptyDeclaration *detached = SageBuilder::buildEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_source_semicolon);
    (void)detached->get_scope();
  }
  if (mode == "--reject-noncontiguous-header-anchor") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_header_order");
    SageInterface::appendStatement(makeVariable("rex_first", unit.global),
                                   unit.global);
    SgEmptyDeclaration *anchor = SageBuilder::buildEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor);
    SageInterface::publishGeneratedSubtreeOutputOwner(anchor, unit.global);
    anchor->set_parent(unit.global);
    anchor->set_scope(unit.global);
    unit.global->get_declarations().push_back(anchor);
    SageInterface::insertHeader(unit.source, "rex_order.hpp", false, true);
  }
  if (mode == "--reject-duplicate-append") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_duplicate_append");
    SgVariableDeclaration *declaration =
        makeVariable("rex_duplicate", unit.global);
    SageInterface::appendStatement(declaration, unit.global);
    SageInterface::appendStatement(declaration, unit.global);
  }
  if (mode == "--reject-nonautonomous-append") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_nonautonomous_append");
    SgClassDeclaration *declaration = new SgClassDeclaration(
        "rex_embedded", SgClassDeclaration::e_class, nullptr, nullptr);
    declaration->set_isAutonomousDeclaration(false);
    SageInterface::appendStatement(declaration, unit.global);
  }
  if (mode == "--reject-replace-self") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_replace_self");
    SgVariableDeclaration *declaration = makeVariable("rex_self", unit.global);
    SageInterface::appendStatement(declaration, unit.global);
    SageInterface::replaceStatement(declaration, declaration);
  }
  if (mode == "--reject-detached-append-owner") {
    SageInterface::appendStatement(SageBuilder::buildNullStatement(),
                                   SageBuilder::buildBasicBlock());
  }
  if (mode == "--reject-insert-preprocessing-merge") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_insert_merge");
    SgVariableDeclaration *target = makeVariable("rex_target", unit.global);
    SageInterface::appendStatement(target, unit.global);
    SageInterface::attachComment(target, "target comment",
                                 PreprocessingInfo::C_StyleComment);
    SgVariableDeclaration *inserted = makeVariable("rex_new", unit.global);
    PreprocessingInfo *record = makeRecord();
    record->setAsTransformation();
    inserted->addToAttachedPreprocessingInfo(record, PreprocessingInfo::before);
    SageInterface::insertStatement(target, inserted, true, true);
  }
  if (mode == "--reject-insert-preprocessing-basic-block") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_insert_block");
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_insert_block_owner"), SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), unit.global);
    ROSE_ASSERT(function != nullptr);
    SgBasicBlock *body = function->get_definition()->get_body();
    ROSE_ASSERT(body != nullptr);
    SgNullStatement *target = SageBuilder::buildNullStatement();
    SageInterface::appendStatement(target, body);
    SageInterface::attachComment(target, "target comment",
                                 PreprocessingInfo::C_StyleComment);
    SageInterface::insertStatement(target, SageBuilder::buildBasicBlock(), true,
                                   true);
  }
  if (mode == "--reject-paste-inside") {
    GeneratedUnit unit = makeGeneratedUnit("rex_reject_paste_inside");
    AttachedPreprocessingInfoType records{makeRecord()};
    SageInterface::pastePreprocessingInfo(unit.global,
                                          PreprocessingInfo::inside, records);
  }

  return 2;
}
