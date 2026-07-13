#include <rose.h>

#include "SageTreeBuilder.h"

#include <iostream>
#include <string>

namespace {
using Rose::builder::SageTreeBuilder;
using Rose::builder::SourcePosition;

const SourcePosition kStart{"/tmp/rex_flang_source_position.f90", 2, 3};
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
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }

  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionFrontendConstruction);
  SageTreeBuilder builder(SageTreeBuilder::LanguageEnum::Fortran);
  const std::string mode = argv[1];

  if (mode == "valid-source") {
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
    return hasExactRange(declaration) && start->isCompilerGenerated() &&
                   end->isCompilerGenerated() &&
                   start->isOutputInCodeGeneration() &&
                   end->isOutputInCodeGeneration()
               ? 0
               : 1;
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
