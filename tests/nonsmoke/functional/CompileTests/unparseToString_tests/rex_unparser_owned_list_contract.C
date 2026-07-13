#include "rose.h"

#include "unparser.h"

#include <sstream>
#include <string>

namespace {

constexpr const char *kFilename = "rex_unparser_owned_list_contract.cpp";

struct MacroDirectiveFixture {
  SgSourceFile *source_file = nullptr;
  SgGlobal *global = nullptr;
  SgFunctionDeclaration *declaration = nullptr;
  std::string filename;

  MacroDirectiveFixture() {
    source_file = SageBuilder::buildGeneratedSourceFile(kFilename);
    ROSE_ASSERT(source_file != nullptr);
    filename = kFilename;
    global = source_file->get_globalScope();
    ROSE_ASSERT(global != nullptr);

    SgFunctionParameterList *parameters =
        SageBuilder::buildFunctionParameterList();
    declaration = SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        SgName("rex_owned_list_function"), SageBuilder::buildVoidType(),
        parameters, global);

    Sg_File_Info *start = new Sg_File_Info(filename, 2, 1);
    start->setOutputInCodeGeneration();
    declaration->set_file_info(start);
    Sg_File_Info *end = new Sg_File_Info(filename, 2, 31);
    end->setOutputInCodeGeneration();
    declaration->set_endOfConstruct(end);
  }

  void setDirectiveList(ROSEAttributesList *list) {
    ROSEAttributesListContainer *container = new ROSEAttributesListContainer();
    container->addList(filename, list);
    source_file->set_preprocessorDirectivesAndCommentsList(container);
  }

  void unparse() {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, filename, options);
    unparser.currentFile = source_file;
    SgUnparse_Info info;
    info.set_current_source_file(source_file);
    unparser.get_name_qualification_context().recordName(
        declaration, declaration, {"", 0, false, false});
    unparser.get_name_qualification_context().recordType(
        declaration, declaration, {"", 0, false, false});
    unparser.u_exprStmt->unparseFuncDeclStmt(declaration, info);
  }
};

PreprocessingInfo *buildMacroDirective(const std::string &filename) {
  return new PreprocessingInfo(
      PreprocessingInfo::CpreprocessorDefineDeclaration,
      "#define rex_owned_list_function(...) __VA_ARGS__\n", filename, 1, 1, 1,
      PreprocessingInfo::before);
}

SgSourceFile *ownedStatementSourceFile() {
  static SgSourceFile *sourceFile = [] {
    SgSourceFile *file = SageBuilder::buildGeneratedSourceFile(kFilename);
    ROSE_ASSERT(file != nullptr);
    file->set_Cxx_only(true);
    file->set_outputLanguage(SgFile::e_Cxx_language);
    ROSE_ASSERT(file->get_project() != nullptr);
    ROSE_ASSERT(file->get_globalScope() != nullptr);
    return file;
  }();
  return sourceFile;
}

SgBasicBlock *ownedStatementScope() {
  static SgBasicBlock *scope = [] {
    SgGlobal *global = ownedStatementSourceFile()->get_globalScope();
    ROSE_ASSERT(global != nullptr);
    SgFunctionDeclaration *function =
        SageBuilder::buildDefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexical(),
            SgName("rex_owned_statement_scope"), SageBuilder::buildVoidType(),
            SageBuilder::buildFunctionParameterList(), global);
    ROSE_ASSERT(function != nullptr);
    SgFunctionDeclaration *prototype =
        isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(prototype != nullptr);
    ROSE_ASSERT(prototype != function);
    ROSE_ASSERT(function->get_definition() != nullptr);
    ROSE_ASSERT(function->get_definition()->get_body() != nullptr);
    return function->get_definition()->get_body();
  }();
  return scope;
}

SgNullStatement *buildOwnedNullStatement() {
  SgSourceFile *sourceFile = ownedStatementSourceFile();
  SgBasicBlock *scope = ownedStatementScope();
  SgNullStatement *statement = SageBuilder::buildNullStatement();
  SageInterface::appendStatement(statement, scope);
  Sg_File_Info *sourcePosition = sourceFile->get_file_info();
  ROSE_ASSERT(sourcePosition != nullptr);
  ROSE_ASSERT(sourcePosition->get_physical_file_id() >= 0);
  const std::string physicalFilename =
      sourcePosition->getFilenameFromID(sourcePosition->get_physical_file_id());
  Sg_File_Info *start = new Sg_File_Info(physicalFilename, 20, 1);
  start->setOutputInCodeGeneration();
  start->set_parent(statement);
  statement->set_file_info(start);
  Sg_File_Info *end = new Sg_File_Info(physicalFilename, 20, 1);
  end->setOutputInCodeGeneration();
  end->set_parent(statement);
  statement->set_endOfConstruct(end);
  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(statement) == sourceFile);
  return statement;
}

std::string unparseOwnedStatement(SgNullStatement *statement) {
  ROSE_ASSERT(statement != nullptr);
  SgSourceFile *sourceFile = ownedStatementSourceFile();
  SgUnparse_Info info;
  info.set_current_source_file(sourceFile);
  info.set_language(SgFile::e_Cxx_language);
  return statement->unparseToString(&info);
}

std::string unparseOwnedStatementAsFile(SgStatement *statement) {
  ROSE_ASSERT(statement != nullptr);
  SgSourceFile *sourceFile = ownedStatementSourceFile();
  std::ostringstream output;
  Unparser_Opt options;
  Unparser unparser(&output, kFilename, options);
  unparser.currentFile = sourceFile;
  SgUnparse_Info info;
  info.set_current_source_file(sourceFile);
  info.set_language(SgFile::e_Cxx_language);
  unparser.u_exprStmt->unparseStatement(statement, info);
  return output.str();
}

SgEmptyDeclaration *
buildOwnedEmptyDeclaration(SgEmptyDeclaration::empty_declaration_role_enum role,
                           bool output) {
  SgSourceFile *sourceFile = ownedStatementSourceFile();
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);
  Sg_File_Info *sourcePosition = sourceFile->get_file_info();
  ROSE_ASSERT(sourcePosition != nullptr);
  const int physicalFileId = sourcePosition->get_physical_file_id();
  ROSE_ASSERT(physicalFileId >= 0);
  const std::string physicalFilename =
      Sg_File_Info::getFilenameFromID(physicalFileId);

  SgEmptyDeclaration *declaration = new SgEmptyDeclaration(role);
  ROSE_ASSERT(declaration != nullptr);
  declaration->set_definingDeclaration(declaration);
  declaration->set_firstNondefiningDeclaration(declaration);

  Sg_File_Info *start = new Sg_File_Info(physicalFilename, 30, 1);
  Sg_File_Info *end = new Sg_File_Info(physicalFilename, 30, 1);
  for (Sg_File_Info *position : {start, end}) {
    position->set_parent(declaration);
    position->set_physical_file_id(physicalFileId);
    if (output) {
      position->setOutputInCodeGeneration();
    } else {
      position->unsetOutputInCodeGeneration();
    }
  }
  declaration->set_file_info(start);
  declaration->set_endOfConstruct(end);
  SageInterface::appendStatement(declaration, global);
  ROSE_ASSERT(declaration->get_parent() == global);
  ROSE_ASSERT(declaration->get_scope() == global);
  return declaration;
}

SgEmptyDeclaration *buildPendingZeroWidthReplacement() {
  SgSourceFile *sourceFile = ownedStatementSourceFile();
  Sg_File_Info *sourcePosition = sourceFile->get_file_info();
  ROSE_ASSERT(sourcePosition != nullptr);
  const int physicalFileId = sourcePosition->get_physical_file_id();
  ROSE_ASSERT(physicalFileId >= 0);

  SgEmptyDeclaration *replacement = SageBuilder::buildEmptyDeclaration(
      SgEmptyDeclaration::e_empty_declaration_zero_width_source_replacement);
  ROSE_ASSERT(replacement != nullptr && replacement->get_parent() == nullptr);
  for (Sg_File_Info *position :
       {replacement->get_file_info(), replacement->get_startOfConstruct(),
        replacement->get_endOfConstruct()}) {
    ROSE_ASSERT(position != nullptr && position->isTransformation() &&
                position->isOutputInCodeGeneration());
    position->set_physical_file_id(physicalFileId);
  }
  return replacement;
}

void attachOwnedPreprocessingRecord(SgEmptyDeclaration *anchor,
                                    bool generated) {
  ROSE_ASSERT(anchor != nullptr);
  Sg_File_Info *anchorInfo = anchor->get_file_info();
  ROSE_ASSERT(anchorInfo != nullptr);
  const int physicalFileId = anchorInfo->get_physical_file_id();
  const std::string filename = Sg_File_Info::getFilenameFromID(physicalFileId);
  PreprocessingInfo *record =
      new PreprocessingInfo(PreprocessingInfo::CpreprocessorIncludeDeclaration,
                            "#include <rex_empty_declaration_role.hpp>\n",
                            filename, 30, 1, 1, PreprocessingInfo::before);
  ROSE_ASSERT(record->get_file_info() != nullptr);
  record->get_file_info()->set_physical_file_id(physicalFileId);
  record->get_file_info()->setOutputInCodeGeneration();
  if (generated) {
    record->setAsTransformation();
  }
  anchor->attachPreprocessingInfo(
      record, PreprocessingInfo::before,
      SgLocatedNode::PreprocessingInfoInsertion::back);
}

int exercisePositiveContracts() {
  {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgBasicBlock *block = SageBuilder::buildBasicBlock();
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseBasicBlockStmt(block, info);
  }

  if (unparseOwnedStatement(buildOwnedNullStatement()).empty()) {
    return 10;
  }

  {
    const std::string spelling =
        unparseOwnedStatementAsFile(buildOwnedEmptyDeclaration(
            SgEmptyDeclaration::e_empty_declaration_source_semicolon, true));
    if (spelling.find(';') == std::string::npos) {
      return 11;
    }
  }

  {
    SgEmptyDeclaration *sourceSurface = buildOwnedEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_source_semicolon, true);
    SgNode *sourceOwner = sourceSurface->get_parent();
    SgEmptyDeclaration *replacement = buildPendingZeroWidthReplacement();
    SageInterface::replaceStatement(sourceSurface, replacement);
    ROSE_ASSERT(sourceSurface->get_parent() == nullptr);
    ROSE_ASSERT(replacement->get_parent() == sourceOwner);
    if (!unparseOwnedStatementAsFile(replacement).empty()) {
      return 12;
    }
  }

  {
    SgEmptyDeclaration *anchor = buildOwnedEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor, true);
    attachOwnedPreprocessingRecord(anchor, true);
    const std::string spelling = unparseOwnedStatementAsFile(anchor);
    const std::string directive = "#include <rex_empty_declaration_role.hpp>";
    if (spelling.find(directive) == std::string::npos ||
        spelling.find(';') != std::string::npos ||
        spelling.find(directive, spelling.find(directive) + 1) !=
            std::string::npos) {
      return 13;
    }
  }

  {
    SgNullStatement *statement = buildOwnedNullStatement();
    PreprocessingInfo *record =
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// owned preprocessing entry\n", kFilename, 1, 1,
                              1, PreprocessingInfo::before);
    statement->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    (void)unparseOwnedStatement(statement);
  }

  {
    SgNullStatement *source = buildOwnedNullStatement();
    SgNullStatement *destination = buildOwnedNullStatement();
    PreprocessingInfo *record =
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// transferred preprocessing entry\n", kFilename,
                              18, 1, 1, PreprocessingInfo::before);
    source->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    source->transferPreprocessingInfo(
        record, destination, PreprocessingInfo::after,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    ROSE_ASSERT(record->getAttachedOwner() == destination);
    ROSE_ASSERT(record->getOutputPlacement() ==
                PreprocessingInfo::attached_output_boundary);
    ROSE_ASSERT(source->getAttachedPreprocessingInfo() == nullptr);
    PreprocessingInfo *detached = destination->detachPreprocessingInfo(record);
    ROSE_ASSERT(detached == record && detached->getAttachedOwner() == nullptr);
    delete detached;
  }

  {
    SgNullStatement *source = buildOwnedNullStatement();
    SgNullStatement *destination = buildOwnedNullStatement();
    const int physicalFileId = source->get_file_info()->get_physical_file_id();
    auto buildRecord = [&](const char *text) {
      PreprocessingInfo *record =
          new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment, text,
                                kFilename, 18, 1, 1, PreprocessingInfo::before);
      ROSE_ASSERT(record->get_file_info() != nullptr);
      record->get_file_info()->set_physical_file_id(physicalFileId);
      record->get_file_info()->setOutputInCodeGeneration();
      return record;
    };
    PreprocessingInfo *first = buildRecord("// first relocated entry\n");
    PreprocessingInfo *second = buildRecord("// second relocated entry\n");
    PreprocessingInfo *existing =
        buildRecord("// existing destination entry\n");
    source->attachPreprocessingInfo(
        first, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    source->attachPreprocessingInfo(
        second, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    destination->attachPreprocessingInfo(
        existing, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);

    SageInterface::moveCommentsToNewStatement(source, {0, 1}, destination,
                                              false);
    ROSE_ASSERT(source->getAttachedPreprocessingInfo() == nullptr);
    AttachedPreprocessingInfoType *destinationRecords =
        destination->getAttachedPreprocessingInfo();
    ROSE_ASSERT(destinationRecords != nullptr &&
                destinationRecords->size() == 3);
    ROSE_ASSERT((*destinationRecords)[0] == first &&
                (*destinationRecords)[1] == second &&
                (*destinationRecords)[2] == existing);
    for (size_t index = 0; index < destinationRecords->size(); ++index) {
      PreprocessingInfo *record = (*destinationRecords)[index];
      ROSE_ASSERT(record->getAttachedOwner() == destination);
      ROSE_ASSERT(record->getRelativePosition() == PreprocessingInfo::before);
      ROSE_ASSERT(record->getOutputPlacement() ==
                  (index < 2 ? PreprocessingInfo::attached_output_boundary
                             : PreprocessingInfo::source_position));
      ROSE_ASSERT(record->get_file_info()->get_physical_file_id() ==
                  destination->get_file_info()->get_physical_file_id());
    }
  }

  {
    SgNullStatement *source = buildOwnedNullStatement();
    SgNullStatement *destination = buildOwnedNullStatement();
    PreprocessingInfo *record =
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// cloned preprocessing entry\n", kFilename, 18,
                              1, 1, PreprocessingInfo::before);
    source->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    destination->cloneAttachedPreprocessingInfoFrom(source);
    ROSE_ASSERT(destination->getAttachedPreprocessingInfo() != nullptr);
    ROSE_ASSERT(destination->getAttachedPreprocessingInfo()->size() == 1);
    PreprocessingInfo *copy =
        destination->getAttachedPreprocessingInfo()->front();
    ROSE_ASSERT(copy != record && copy->getAttachedOwner() == destination);
    ROSE_ASSERT(copy->getOutputPlacement() ==
                PreprocessingInfo::attached_output_boundary);
  }

  {
    MacroDirectiveFixture fixture;
    ROSEAttributesList *list = new ROSEAttributesList();
    list->getList().push_back(buildMacroDirective(fixture.filename));
    fixture.setDirectiveList(list);
    fixture.unparse();
  }

  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    return exercisePositiveContracts();
  }
  if (argc != 2) {
    return 2;
  }

  const std::string mode = argv[1];
  if (mode == "basic-block-null-statement") {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgBasicBlock *block = SageBuilder::buildBasicBlock();
    block->get_statements().push_back(nullptr);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseBasicBlockStmt(block, info);
    return 0;
  }
  if (mode == "basic-block-foreign-statement" ||
      mode == "basic-block-duplicate-statement") {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgBasicBlock *block = SageBuilder::buildBasicBlock();
    SgNullStatement *statement = SageBuilder::buildNullStatement();
    block->get_statements().push_back(statement);
    if (mode == "basic-block-foreign-statement") {
      statement->set_parent(new SgGlobal());
    } else {
      statement->set_parent(block);
      block->get_statements().push_back(statement);
    }
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseBasicBlockStmt(block, info);
    return 0;
  }
  if (mode == "global-null-declaration") {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(kFilename);
    ROSE_ASSERT(sourceFile != nullptr);
    SgGlobal *global = sourceFile->get_globalScope();
    ROSE_ASSERT(global != nullptr);
    global->get_declarations().push_back(nullptr);
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_language(SgFile::e_Cxx_language);
    unparser.currentFile = sourceFile;
    unparser.u_exprStmt->unparseGlobalStmt(global, info);
    return 0;
  }
  if (mode == "global-foreign-declaration" ||
      mode == "global-duplicate-declaration") {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgSourceFile *sourceFile = SageBuilder::buildGeneratedSourceFile(kFilename);
    ROSE_ASSERT(sourceFile != nullptr);
    SgGlobal *global = sourceFile->get_globalScope();
    ROSE_ASSERT(global != nullptr);
    SgEmptyDeclaration *declaration = new SgEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_source_semicolon);
    global->get_declarations().push_back(declaration);
    if (mode == "global-foreign-declaration") {
      declaration->set_parent(SageBuilder::buildBasicBlock());
    } else {
      declaration->set_parent(global);
      global->get_declarations().push_back(declaration);
    }
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_language(SgFile::e_Cxx_language);
    unparser.currentFile = sourceFile;
    unparser.u_exprStmt->unparseGlobalStmt(global, info);
    return 0;
  }
  if (mode == "empty-declaration-invalid-role") {
    (void)new SgEmptyDeclaration(
        static_cast<SgEmptyDeclaration::empty_declaration_role_enum>(999));
    return 0;
  }
  if (mode == "empty-declaration-source-non-output") {
    unparseOwnedStatementAsFile(buildOwnedEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_source_semicolon, false));
    return 0;
  }
  if (mode == "empty-declaration-anchor-no-record") {
    unparseOwnedStatementAsFile(buildOwnedEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor, true));
    return 0;
  }
  if (mode == "empty-declaration-anchor-non-output") {
    SgEmptyDeclaration *anchor = buildOwnedEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor, false);
    attachOwnedPreprocessingRecord(anchor, true);
    unparseOwnedStatementAsFile(anchor);
    return 0;
  }
  if (mode == "empty-declaration-anchor-record-not-generated") {
    SgEmptyDeclaration *anchor = buildOwnedEmptyDeclaration(
        SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor, true);
    attachOwnedPreprocessingRecord(anchor, false);
    unparseOwnedStatementAsFile(anchor);
    return 0;
  }
  if (mode == "macro-null-list") {
    MacroDirectiveFixture fixture;
    fixture.setDirectiveList(nullptr);
    fixture.unparse();
    return 0;
  }
  if (mode == "macro-null-entry") {
    MacroDirectiveFixture fixture;
    ROSEAttributesList *list = new ROSEAttributesList();
    list->getList().push_back(nullptr);
    fixture.setDirectiveList(list);
    fixture.unparse();
    return 0;
  }
  if (mode == "attached-preprocessing-null-entry") {
    SgNullStatement *statement = buildOwnedNullStatement();
    statement->getAttachedPreprocessingInfo() =
        new AttachedPreprocessingInfoType();
    statement->getAttachedPreprocessingInfo()->push_back(nullptr);
    (void)unparseOwnedStatement(statement);
    return 0;
  }
  if (mode == "attached-preprocessing-shared-record") {
    SgNullStatement *first = buildOwnedNullStatement();
    SgNullStatement *second = buildOwnedNullStatement();
    PreprocessingInfo *record =
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// shared preprocessing must be rejected\n",
                              kFilename, 19, 1, 1, PreprocessingInfo::before);
    first->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    second->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    return 0;
  }
  if (mode == "attached-preprocessing-position-mutation") {
    SgNullStatement *statement = buildOwnedNullStatement();
    PreprocessingInfo *record =
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// attached preprocessing position\n", kFilename,
                              19, 1, 1, PreprocessingInfo::before);
    statement->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    record->setRelativePosition(PreprocessingInfo::after);
    return 0;
  }
  if (mode == "attached-preprocessing-misowned-record") {
    SgNullStatement *statement = buildOwnedNullStatement();
    statement->getAttachedPreprocessingInfo() =
        new AttachedPreprocessingInfoType();
    statement->getAttachedPreprocessingInfo()->push_back(
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// ownerless preprocessing\n", kFilename, 19, 1,
                              1, PreprocessingInfo::before));
    (void)unparseOwnedStatement(statement);
    return 0;
  }
  if (mode == "attached-preprocessing-duplicate-receipt") {
    SgNullStatement *statement = buildOwnedNullStatement();
    const int physicalFileId =
        statement->get_file_info()->get_physical_file_id();
    ROSE_ASSERT(physicalFileId >= 0);
    const std::string physicalFilename =
        Sg_File_Info::getFilenameFromID(physicalFileId);
    PreprocessingInfo *record = new PreprocessingInfo(
        PreprocessingInfo::CplusplusStyleComment, "// one invocation receipt\n",
        physicalFilename, 19, 1, 1, PreprocessingInfo::before);
    ROSE_ASSERT(record->get_file_info() != nullptr);
    record->get_file_info()->set_physical_file_id(physicalFileId);
    record->get_file_info()->setOutputInCodeGeneration();
    statement->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    SgSourceFile *sourceFile = ownedStatementSourceFile();
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    unparser.currentFile = sourceFile;
    SgUnparse_Info info;
    info.set_current_source_file(sourceFile);
    info.set_language(SgFile::e_Cxx_language);
    unparser.u_exprStmt->unparseStatement(statement, info);
    unparser.u_exprStmt->unparseStatement(statement, info);
    return 0;
  }
  if (mode == "move-comments-duplicate-selection") {
    SgNullStatement *source = buildOwnedNullStatement();
    SgNullStatement *destination = buildOwnedNullStatement();
    PreprocessingInfo *record = new PreprocessingInfo(
        PreprocessingInfo::CplusplusStyleComment, "// selected exactly once\n",
        kFilename, 19, 1, 1, PreprocessingInfo::before);
    source->attachPreprocessingInfo(
        record, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    SageInterface::moveCommentsToNewStatement(source, {0, 0}, destination,
                                              false);
    return 0;
  }
  if (mode == "cxx-attached-preprocessing-null-entry") {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgBasicBlock *block = SageBuilder::buildBasicBlock();
    block->unsetTransformation();
    block->getAttachedPreprocessingInfo() = new AttachedPreprocessingInfoType();
    block->getAttachedPreprocessingInfo()->push_back(nullptr);
    new SgFunctionDefinition(block);
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseBasicBlockStmt(block, info);
    return 0;
  }
  if (mode == "lexical-non-output-statement") {
    SgNullStatement *statement = buildOwnedNullStatement();
    statement->get_file_info()->unsetOutputInCodeGeneration();
    unparseOwnedStatementAsFile(statement);
    return 0;
  }
  if (mode == "non-output-preprocessing") {
    SgNullStatement *statement = buildOwnedNullStatement();
    PreprocessingInfo *preprocessing =
        new PreprocessingInfo(PreprocessingInfo::CplusplusStyleComment,
                              "// hidden preprocessing must be rejected\n",
                              kFilename, 19, 1, 1, PreprocessingInfo::before);
    preprocessing->get_file_info()->unsetOutputInCodeGeneration();
    statement->attachPreprocessingInfo(
        preprocessing, PreprocessingInfo::before,
        SgLocatedNode::PreprocessingInfoInsertion::back);
    unparseOwnedStatementAsFile(statement);
    return 0;
  }
  if (mode == "orphan-class-definition") {
    std::ostringstream output;
    Unparser_Opt options;
    Unparser unparser(&output, kFilename, options);
    SgClassDefinition *definition = new SgClassDefinition();
    SgUnparse_Info info;
    unparser.u_exprStmt->unparseClassDefnStmt(definition, info);
    return 0;
  }

  return 3;
}
