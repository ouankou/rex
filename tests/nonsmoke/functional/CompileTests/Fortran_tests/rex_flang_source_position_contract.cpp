#include <rose.h>

#include "Rose/FileSystem.h"
#include "SageTreeBuilder.h"
#include "astJson/sageAstJson.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Rose::builder::SageTreeBuilder;
using Rose::builder::SourcePosition;

const SourcePosition kStart{"/tmp/rex_flang_source_position.f90", 2, 3, 0};
const SourcePosition kEnd{"/tmp/rex_flang_source_position.f90", 2, 8};

SgNullStatement *buildSourceNode() {
  SgNullStatement *node = SageBuilder::buildNullStatement_nfi();
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(node->get_startOfConstruct() == nullptr);
  ROSE_ASSERT(node->get_endOfConstruct() == nullptr);
  return node;
}

bool hasExactRange(const SgLocatedNode *node) {
  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  return start != nullptr && end != nullptr && start->get_parent() == node &&
         end->get_parent() == node &&
         start->get_raw_filename() == kStart.path &&
         end->get_raw_filename() == kEnd.path &&
         start->get_raw_line() == kStart.line &&
         start->get_raw_col() == kStart.column &&
         end->get_raw_line() == kEnd.line &&
         end->get_raw_col() == kEnd.column - 1 &&
         !start->isSourcePositionUnavailableInFrontend() &&
         !end->isSourcePositionUnavailableInFrontend();
}

bool hasExactExpressionRange(const SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  const Sg_File_Info *start = expression->get_startOfConstruct();
  const Sg_File_Info *end = expression->get_endOfConstruct();
  const Sg_File_Info *operatorPosition = expression->get_operatorPosition();
  return hasExactRange(expression) && operatorPosition != nullptr &&
         operatorPosition != start && operatorPosition != end &&
         expression->get_file_info() == operatorPosition &&
         operatorPosition->get_parent() == expression &&
         operatorPosition->get_raw_filename() == kStart.path &&
         operatorPosition->get_raw_line() == kStart.line &&
         operatorPosition->get_raw_col() == kStart.column;
}

void checkRecordedSourceIdentity() {
  namespace fs = std::filesystem;
  const auto directory = Rose::FileSystem::createTemporaryDirectory();
  const auto original = directory / "snapshot.f90";
  const auto replacement = directory / "replacement.f90";
  for (const auto &path : {original, replacement}) {
    std::ofstream output(path);
    output << "program snapshot\n  continue\nend\n";
    ROSE_ASSERT(output.good());
  }
  const SourcePosition start{original.string(), 2, 3};
  const SourcePosition end{original.string(), 2, 8};
  SgSourceFile source;
  source.set_file_info(new Sg_File_Info(start.path, 1, 1));
  source.set_sourceFileNameWithPath(start.path);
  source.set_sourceFileNameWithoutPath(original.filename().string());
  source.set_Fortran_only(true);
  SageTreeBuilder builder;
  builder.setSourceFile(&source);
  auto *before = buildSourceNode();
  builder.setSourcePosition(before, start, end);
  ROSE_ASSERT(fs::remove(original));
  fs::create_symlink(replacement.filename(), original);
  auto *after = buildSourceNode();
  builder.setSourcePosition(after, start, end);
  for (auto *node : {before, after}) {
    ROSE_ASSERT(node->get_startOfConstruct()->get_raw_filename() == start.path);
    ROSE_ASSERT(node->get_endOfConstruct()->get_raw_filename() == end.path);
    ROSE_ASSERT(node->get_file_info()->get_physical_file_id() ==
                source.get_file_info()->get_physical_file_id());
    SageInterface::deepDelete(node);
  }
  fs::remove_all(directory);
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "directive-source") {
    const std::string path = std::filesystem::canonical(argv[2]).string();
    const std::vector<std::string> arguments{
        argv[0], "-rose:skipfinalCompileStep", "-c", argv[2]};
    SgProject *project = frontend(arguments);
    const auto checkRange = [&](const auto *node) {
      const auto *start = node->get_startOfConstruct();
      const auto *end = node->get_endOfConstruct();
      ROSE_ASSERT(start != nullptr && end != nullptr);
      ROSE_ASSERT(start->get_parent() == node && end->get_parent() == node);
      ROSE_ASSERT(start->get_raw_filename() == path &&
                  end->get_raw_filename() == path);
      ROSE_ASSERT(start->get_physical_filename() == path &&
                  end->get_physical_filename() == path);
      ROSE_ASSERT(start->get_raw_line() == 6 && end->get_raw_line() == 6);
      ROSE_ASSERT(start->get_raw_col() == 1 && end->get_raw_col() == 12);
      ROSE_ASSERT(!start->isCompilerGenerated() && !end->isCompilerGenerated());
    };
    const auto checkProject = [&](SgProject *candidate) {
      const auto directives =
          NodeQuery::querySubTree(candidate, V_SgPragmaDeclaration);
      ROSE_ASSERT(directives.size() == 1);
      auto *directive = isSgPragmaDeclaration(directives.front());
      ROSE_ASSERT(directive->get_fortran_directive_family() ==
                  SgPragmaDeclaration::e_fortran_directive_compiler);
      ROSE_ASSERT(directive->get_pragma()->get_parent() == directive);
      ROSE_ASSERT(directive->get_pragma()->get_pragma() == "!dir$ unroll");
      checkRange(directive);
      checkRange(directive->get_pragma());
    };
    checkProject(project);
    SgTreeCopy copier;
    auto *copy = isSgProject(project->copy(copier));
    ROSE_ASSERT(copy != nullptr);
    checkProject(copy);
    SageInterface::deleteAST(copy);
    const auto directory = Rose::FileSystem::createTemporaryDirectory();
    Rose::AstJson::Options options;
    options.outputDirectory = directory.string();
    auto *source = isSgSourceFile(project->get_fileList().front());
    source = Rose::AstJson::roundTripSourceFile(
        source, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
    checkProject(project);
    const auto output = directory / "directive.F90";
    source->set_unparse_output_filename(output.string());
    source->unparse();
    ROSE_ASSERT(Rose::StringUtility::readFile(output.string())
                    .find("\n!dir$ unroll\n") != std::string::npos);
    SageInterface::deleteAST(project);
    std::filesystem::remove_all(directory);
    return 0;
  }
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }

  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionFrontendConstruction);
  SageTreeBuilder builder;
  const std::string mode = argv[1];

  if (mode == "valid-source") {
    checkRecordedSourceIdentity();
    SgNullStatement *node = buildSourceNode();
    builder.setSourcePosition(node, kStart, kEnd);
    return hasExactRange(node) ? 0 : 1;
  }
  if (mode == "valid-pending-source") {
    SgNullStatement *node = buildSourceNode();
    SageInterface::setSourcePosition(node);
    ROSE_ASSERT(node->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(node->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(
        node->get_startOfConstruct()->isSourcePositionUnavailableInFrontend());
    ROSE_ASSERT(
        node->get_endOfConstruct()->isSourcePositionUnavailableInFrontend());
    builder.setSourcePosition(node, kStart, kEnd);
    return hasExactRange(node) ? 0 : 1;
  }
  if (mode == "valid-source-expression") {
    SgExpression *expression = SageBuilder::buildIntVal_nfi(7);
    ROSE_ASSERT(expression != nullptr);
    ROSE_ASSERT(expression->get_startOfConstruct() == nullptr);
    ROSE_ASSERT(expression->get_endOfConstruct() == nullptr);
    ROSE_ASSERT(expression->get_operatorPosition() == nullptr);
    builder.setSourcePosition(expression, kStart, kEnd);
    return hasExactExpressionRange(expression) ? 0 : 1;
  }
  if (mode == "valid-pending-source-expression") {
    SgExpression *expression = SageBuilder::buildIntVal_nfi(7);
    ROSE_ASSERT(expression != nullptr);
    SageInterface::setSourcePosition(expression);
    ROSE_ASSERT(expression->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(expression->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(expression->get_operatorPosition() != nullptr);
    builder.setSourcePosition(expression, kStart, kEnd);
    return hasExactExpressionRange(expression) ? 0 : 1;
  }
  if (mode == "valid-source-attribute") {
    SgAttributeSpecificationStatement *statement =
        SageBuilder::buildAttributeSpecificationStatement_nfi(
            SgAttributeSpecificationStatement::e_parameterStatement);
    ROSE_ASSERT(statement != nullptr);
    ROSE_ASSERT(statement->get_startOfConstruct() == nullptr);
    ROSE_ASSERT(statement->get_endOfConstruct() == nullptr);
    SgExprListExp *parameters = statement->get_parameter_list();
    ROSE_ASSERT(parameters != nullptr);
    ROSE_ASSERT(parameters->get_parent() == statement);
    builder.setSourcePosition(statement, kStart, kEnd);
    ROSE_ASSERT(hasExactRange(statement));
    ROSE_ASSERT(statement->get_translation_unit_source_order() == 1);
    ROSE_ASSERT(parameters->get_startOfConstruct() != nullptr);
    ROSE_ASSERT(parameters->get_endOfConstruct() != nullptr);
    ROSE_ASSERT(parameters->get_startOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(parameters->get_endOfConstruct()->isCompilerGenerated());
    ROSE_ASSERT(!parameters->get_startOfConstruct()->isTransformation());
    ROSE_ASSERT(!parameters->get_endOfConstruct()->isTransformation());
    return 0;
  }
  if (mode == "valid-physical-output-owner") {
    SgSourceFile source;
    source.set_file_info(new Sg_File_Info(kStart.path, 1, 1));
    source.set_sourceFileNameWithPath(kStart.path);
    source.set_sourceFileNameWithoutPath("rex_flang_source_position.f90");
    source.set_Fortran_only(true);
    builder.setSourceFile(&source);

    SgNullStatement *primary = buildSourceNode();
    builder.setSourcePosition(primary, kStart, kEnd);
    const SourcePosition includeStart{"/tmp/rex_flang_include.inc", 4, 2};
    const SourcePosition includeEnd{"/tmp/rex_flang_include.inc", 4, 7};
    SgNullStatement *included = buildSourceNode();
    builder.setSourcePosition(included, includeStart, includeEnd);
    return primary->get_file_info()->get_physical_file_id() ==
                       source.get_file_info()->get_physical_file_id() &&
                   included->get_file_info()->get_physical_file_id() !=
                       source.get_file_info()->get_physical_file_id()
               ? 0
               : 1;
  }
  if (mode == "valid-generated-object") {
    SgSourceFile source;
    source.set_file_info(new Sg_File_Info(kStart.path, 1, 1));
    source.set_sourceFileNameWithPath(kStart.path);
    source.set_sourceFileNameWithoutPath("rex_flang_source_position.f90");
    source.set_Fortran_only(true);
    builder.setSourceFile(&source);

    SgVariableDeclaration *declaration = new SgVariableDeclaration(
        SgName("generated_object"), SageBuilder::buildIntType(), nullptr);
    ROSE_ASSERT(declaration != nullptr);
    SageInterface::setOneSourcePositionNull(declaration);
    builder.setGeneratedSourcePosition(
        declaration, kStart, kEnd,
        SageTreeBuilder::GeneratedSourceAnchorKind::
            use_associated_object_declaration);
    const Sg_File_Info *start = declaration->get_startOfConstruct();
    const Sg_File_Info *end = declaration->get_endOfConstruct();
    const bool valid =
        hasExactRange(declaration) && start->isCompilerGenerated() &&
        end->isCompilerGenerated() && start->isOutputInCodeGeneration() &&
        end->isOutputInCodeGeneration();
    builder.setTokens({Rose::builder::Token(
        Rose::builder::TokenKind::comment,
        Rose::builder::PreprocessingDirectiveKind::none, kStart.path, 3, 1, 3,
        21, "! rex_source_comment\n")});
    builder.attachRemainingComments(declaration);
    const auto *generatedComments = declaration->getAttachedPreprocessingInfo();
    ROSE_ASSERT(generatedComments == nullptr || generatedComments->empty());
    SgNullStatement *written = buildSourceNode();
    builder.setSourcePosition(written, kStart, kEnd);
    builder.attachRemainingComments(written);
    const auto *writtenComments = written->getAttachedPreprocessingInfo();
    ROSE_ASSERT(writtenComments != nullptr && writtenComments->size() == 1);
    ROSE_ASSERT(writtenComments->front()->getString() ==
                "! rex_source_comment\n");
    SageInterface::deepDelete(written);
    SageInterface::deepDelete(declaration);
    return valid ? 0 : 1;
  }
  if (mode == "missing-start") {
    builder.setSourcePosition(buildSourceNode(), SourcePosition{}, kEnd);
  } else if (mode == "partial-start") {
    builder.setSourcePosition(
        buildSourceNode(),
        SourcePosition{"/tmp/rex_flang_source_position.f90", 0, 3}, kEnd);
  } else if (mode == "different-files") {
    builder.setSourcePosition(buildSourceNode(), kStart,
                              SourcePosition{"/tmp/rex_flang_other.f90", 2, 8});
  } else if (mode == "reversed-range") {
    builder.setSourcePosition(buildSourceNode(), kEnd, kStart);
  } else if (mode == "preclassified-statement") {
    builder.setSourcePosition(SageBuilder::buildNullStatement(), kStart, kEnd);
  } else if (mode == "preclassified-expression-operator") {
    SgExpression *expression = SageBuilder::buildIntVal_nfi(7);
    ROSE_ASSERT(expression != nullptr);
    Sg_File_Info *operatorPosition =
        Sg_File_Info::generateDefaultFileInfoForTransformationNode();
    ROSE_ASSERT(operatorPosition != nullptr);
    expression->set_operatorPosition(operatorPosition);
    operatorPosition->set_parent(expression);
    builder.setSourcePosition(expression, kStart, kEnd);
  } else if (mode == "wrong-generated-kind") {
    builder.setGeneratedSourcePosition(
        buildSourceNode(), kStart, kEnd,
        SageTreeBuilder::GeneratedSourceAnchorKind::
            use_associated_object_declaration);
  } else if (mode == "missing-comment-owner" ||
             mode == "pending-comment-owner") {
    SgSourceFile source;
    source.set_file_info(new Sg_File_Info(kStart.path, 1, 1));
    builder.setSourceFile(&source);
    SgNullStatement *node = buildSourceNode();
    if (mode == "pending-comment-owner") {
      SageInterface::setSourcePosition(node);
    }
    builder.attachRemainingComments(node);
  } else if (mode == "missing-output-owner" ||
             mode == "ambiguous-output-owner") {
    SgSourceFile source;
    source.set_file_info(mode == "missing-output-owner"
                             ? Sg_File_Info::generateDefaultFileInfo()
                             : new Sg_File_Info(kStart.path, 1, 1));
    if (mode == "ambiguous-output-owner") {
      source.get_file_info()->setShared();
    }
    source.set_sourceFileNameWithPath(kStart.path);
    source.set_sourceFileNameWithoutPath("rex_flang_source_position.f90");
    source.set_Fortran_only(true);
    builder.setSourceFile(&source);
    builder.setSourcePosition(buildSourceNode(), kStart, kEnd);
  } else {
    std::cerr << "unknown mode: " << mode << '\n';
    return 2;
  }

  std::cerr << "source-position contract unexpectedly returned\n";
  return 1;
}
