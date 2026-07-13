#include "rose.h"

#include <string>
#include <vector>

namespace {
SgProject *parseProject(const char *program, const char *source) {
  const std::vector<std::string> arguments = {
      program, "-rose:verbose", "0", "-rose:skipfinalCompileStep", "-std=c++20",
      "-c",    source};
  return frontend(arguments);
}

SgVarRefExp *findReference(SgProject *project) {
  ASSERT_not_null(project);
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    if (reference != nullptr && reference->get_symbol() != nullptr &&
        reference->get_symbol()->get_name() == "rex_context_value") {
      return reference;
    }
  }
  return nullptr;
}

SgInitializedName *findInitializedName(SgProject *project,
                                       const std::string &name) {
  ASSERT_not_null(project);
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName != nullptr &&
        initializedName->get_name().getString() == name) {
      if (result != nullptr) {
        return nullptr;
      }
      result = initializedName;
    }
  }
  return result;
}

bool checkContextualBuiltinType(SgSourceFile *sourceFile,
                                SgInitializedName *initializedName) {
  ASSERT_not_null(sourceFile);
  ASSERT_not_null(initializedName);
  SgDeclarationStatement *declaration = initializedName->get_declptr();
  SgScopeStatement *scope = initializedName->get_scope();
  SgType *type = initializedName->get_type();
  if (declaration == nullptr || scope == nullptr || type == nullptr) {
    return false;
  }

  SgUnparse_Info info;
  info.set_current_source_file(sourceFile);
  info.set_current_scope(scope);
  info.set_template_argument_qualification_context(declaration);
  info.set_reference_node_for_qualification(initializedName);
  info.set_language(SgFile::e_Cxx_language);
  const std::string output = type->unparseToString(&info);
  if (output != "int") {
    std::cerr << "contextual builtin initialized-name="
              << initializedName->get_name().getString()
              << " type=" << type->class_name() << " output='" << output << "'"
              << std::endl;
    return false;
  }
  return true;
}

bool checkCanonicalInput(SgNode *node, const std::string &expected) {
  ASSERT_not_null(node);
  SgUnparse_Info info;
  const std::string actual = node->unparseToString(&info);
  const bool valid = actual == expected &&
                     info.get_language() == SgFile::e_Cxx_language &&
                     info.get_current_source_file() == nullptr &&
                     info.get_current_scope() == nullptr;
  if (!valid) {
    std::cerr << "canonical input " << node->class_name() << " expected='"
              << expected << "' actual='" << actual << "' language="
              << SgFile::get_outputLanguageOptionName(info.get_language())
              << " file=" << info.get_current_source_file()
              << " scope=" << info.get_current_scope() << std::endl;
  }
  return valid;
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    return 1;
  }

  SgProject *project = parseProject(argv[0], argv[argc - 1]);
  if (project == nullptr || project->numberOfFiles() != 1) {
    return 2;
  }
  SgSourceFile *sourceFile = isSgSourceFile(project->get_fileList().front());
  SgVarRefExp *reference = findReference(project);
  SgInitializedName *parameter =
      findInitializedName(project, "rex_context_parameter");
  if (sourceFile == nullptr || reference == nullptr || parameter == nullptr ||
      sourceFile->get_outputLanguage() != SgFile::e_Cxx_language) {
    return 3;
  }

  const std::string mode = argc == 3 ? argv[1] : std::string();
  if (mode == "--detached-context-sensitive") {
    SgVarRefExp *detached =
        SageBuilder::buildVarRefExp(reference->get_symbol());
    ASSERT_not_null(detached);
    ASSERT_require(detached->get_parent() == nullptr);
    (void)detached->unparseToString();
    return 0;
  }
  if (mode == "--conflicting-source-candidates") {
    SgSourceFile conflictingFile;
    conflictingFile.set_file_info(
        new Sg_File_Info("rex_unparser_conflicting_context.cpp", 1, 1));
    conflictingFile.set_sourceFileNameWithPath(
        "rex_unparser_conflicting_context.cpp");
    conflictingFile.set_sourceFileNameWithoutPath(
        "rex_unparser_conflicting_context.cpp");
    conflictingFile.set_Cxx_only(true);
    conflictingFile.set_outputLanguage(SgFile::e_Cxx_language);

    SgUnparse_Info info;
    info.set_current_source_file(&conflictingFile);
    info.set_language(SgFile::e_Cxx_language);
    (void)reference->unparseToString(&info);
    return 0;
  }
  if (mode == "--detached-null-absence") {
    (void)SageBuilder::buildNullExpression_nfi(
        SgNullExpression::e_null_expression_syntactic_absence)
        ->unparseToString();
    return 0;
  }
  if (mode == "--detached-template-argument") {
    SgTemplateArgumentPtrList arguments;
    SgTemplateArgument *argument =
        Rose::Builder::Templates::buildTemplateArgument(
            SageBuilder::buildVarRefExp(reference->get_symbol()));
    argument->set_explicitlySpecified(true);
    arguments.push_back(argument);
    (void)globalUnparseToString(&arguments, nullptr);
    return 0;
  }
  if (!mode.empty()) {
    return 4;
  }

  SgStatement *emissionStatement =
      SageInterface::getEnclosingStatement(reference);
  SgScopeStatement *scope = SageInterface::getEnclosingScope(reference);
  if (emissionStatement == nullptr || scope == nullptr) {
    return 5;
  }
  SgUnparse_Info contextualInfo;
  contextualInfo.set_current_source_file(sourceFile);
  contextualInfo.set_template_argument_qualification_context(emissionStatement);
  contextualInfo.set_reference_node_for_qualification(reference);
  contextualInfo.set_current_scope(scope);
  contextualInfo.set_language(SgFile::e_Cxx_language);
  if (reference->unparseToString(&contextualInfo) != "rex_context_value") {
    return 6;
  }
  if (!checkContextualBuiltinType(sourceFile, parameter)) {
    return 7;
  }

  SgIntVal *canonicalInteger = SageBuilder::buildIntVal(17);
  canonicalInteger->set_valueString("0x11");
  canonicalInteger->set_literal_spelling_form(
      SgValueExp::e_literal_canonical_generated);
  if (!checkCanonicalInput(SageBuilder::buildIntType(), "int") ||
      !checkCanonicalInput(canonicalInteger, "0x11") ||
      !checkCanonicalInput(SageBuilder::buildStringVal("rex  literal"),
                           "\"rex  literal\"")) {
    return 8;
  }

  SgOmpSourceExpression *sourceSpelling =
      new SgOmpSourceExpression("rex_source(1)");
  SageInterface::setSourcePositionForTransformation(sourceSpelling);
  if (!checkCanonicalInput(sourceSpelling, "rex_source(1)")) {
    return 9;
  }

  SgIntVal *expanded = SageBuilder::buildIntVal_nfi(1, "1");
  SgMacroExpansionExp *macro =
      new SgMacroExpansionExp("REX_MACRO(1)", expanded);
  expanded->set_parent(macro);
  SageInterface::setSourcePositionForTransformation(macro);
  if (!checkCanonicalInput(macro, "REX_MACRO(1)")) {
    return 10;
  }

  SgTemplateArgumentPtrList emptyArguments;
  SgTemplateParameterPtrList emptyParameters;
  if (globalUnparseToString(&emptyArguments, nullptr) != "<>" ||
      !globalUnparseToString(&emptyParameters, nullptr).empty()) {
    return 11;
  }
  SgTemplateArgument *integerArgument =
      Rose::Builder::Templates::buildTemplateArgument(23);
  integerArgument->set_explicitlySpecified(true);
  SgValueExp *integerArgumentValue =
      isSgValueExp(integerArgument->get_expression());
  ASSERT_not_null(integerArgumentValue);
  SageInterface::setSourcePositionForTransformation(integerArgumentValue);
  integerArgumentValue->set_literal_spelling_form(
      SgValueExp::e_literal_canonical_generated);
  SgTemplateArgumentPtrList canonicalArguments{integerArgument};
  if (globalUnparseToString(&canonicalArguments, nullptr) != "<23>") {
    return 12;
  }
  return 0;
}
