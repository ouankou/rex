#include "rose.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Outliner.hh"
#include "RoseAst.h"

namespace {

[[noreturn]] void fail(const std::string &reason) {
  std::cerr << "REX_TEST_ERROR[outliner-local-type-template-contract]: "
            << reason << "\n";
  std::exit(1);
}

SgType *removeReference(SgType *type) {
  if (SgReferenceType *reference = isSgReferenceType(type)) {
    return reference->get_base_type();
  }
  if (SgRvalueReferenceType *reference = isSgRvalueReferenceType(type)) {
    return reference->get_base_type();
  }
  return type;
}

} // namespace

int main(int argc, char *argv[]) {
  std::vector<std::string> arguments(argv, argv + argc);
  Outliner::commandLineProcessing(arguments);
  SgProject *project = frontend(arguments);
  if (project == nullptr) {
    fail("frontend returned a null project");
  }
  AstTests::runAllTests(project);

  SgType *capturedType = nullptr;
  std::set<SgFunctionDeclaration *> originalDefinitions;
  RoseAst originalAst(project);
  for (RoseAst::iterator node = originalAst.begin(); node != originalAst.end();
       ++node) {
    if (SgFunctionDeclaration *function = isSgFunctionDeclaration(*node)) {
      if (function->get_definition() != nullptr) {
        originalDefinitions.insert(function);
      }
    }
    SgInitializedName *name = isSgInitializedName(*node);
    if (name != nullptr && name->get_name() == "local_value") {
      if (capturedType != nullptr && capturedType != name->get_type()) {
        fail("captured declaration has more than one semantic type");
      }
      capturedType = name->get_type();
    }
  }
  SgNamedType *capturedNamedType =
      capturedType != nullptr ? isSgNamedType(capturedType->findBaseType())
                              : nullptr;
  if (capturedNamedType == nullptr ||
      SageInterface::getEnclosingFunctionDeclaration(
          capturedNamedType->get_declaration()) == nullptr) {
    fail("specimen did not produce one function-local captured named type");
  }

  Outliner::outlineAll(project);
  AstTests::runAllTests(project);

  SgTemplateFunctionDeclaration *outlinedDefinition = nullptr;
  SgNonrealRefExp *outlinedCallReference = nullptr;
  RoseAst outlinedAst(project);
  for (RoseAst::iterator node = outlinedAst.begin(); node != outlinedAst.end();
       ++node) {
    SgTemplateFunctionDeclaration *function =
        isSgTemplateFunctionDeclaration(*node);
    if (function != nullptr && function->get_definition() != nullptr &&
        originalDefinitions.count(function) == 0) {
      if (outlinedDefinition != nullptr && outlinedDefinition != function) {
        fail("more than one outlined template definition was generated");
      }
      outlinedDefinition = function;
    }

    SgNonrealRefExp *reference = isSgNonrealRefExp(*node);
    if (reference != nullptr &&
        reference->get_resolved_function_declaration() != nullptr &&
        reference->get_resolved_function_declaration()
                ->get_name()
                .getString()
                .find("OUT__") == 0) {
      if (outlinedCallReference != nullptr &&
          outlinedCallReference != reference) {
        fail("more than one outlined template call reference was generated");
      }
      outlinedCallReference = reference;
    }
  }

  const SgTemplateParameterPtrList *parameters =
      outlinedDefinition != nullptr
          ? &outlinedDefinition->get_templateParameters()
          : nullptr;
  SgTemplateParameter *parameter =
      parameters != nullptr && parameters->size() == 1 ? parameters->front()
                                                       : nullptr;
  SgTemplateType *parameterType =
      parameter != nullptr ? isSgTemplateType(parameter->get_type()) : nullptr;
  if (outlinedDefinition == nullptr || parameterType == nullptr ||
      parameterType->get_template_parameter() != parameter) {
    fail("outlined definition has no exact generated local-type parameter");
  }

  SgInitializedName *unpackedLocal = nullptr;
  RoseAst bodyAst(outlinedDefinition->get_definition()->get_body());
  for (RoseAst::iterator node = bodyAst.begin(); node != bodyAst.end();
       ++node) {
    SgInitializedName *name = isSgInitializedName(*node);
    if (name != nullptr && name->get_name() == "local_value") {
      unpackedLocal = name;
      break;
    }
  }
  if (unpackedLocal == nullptr ||
      removeReference(unpackedLocal->get_type()) != parameterType) {
    fail("outlined unpack declaration does not use the generated type");
  }

  const SgTemplateArgumentPtrList *argumentsAtCall =
      outlinedCallReference != nullptr
          ? &outlinedCallReference->get_templateArguments()
          : nullptr;
  SgTemplateArgument *actual =
      argumentsAtCall != nullptr && argumentsAtCall->size() == 1
          ? argumentsAtCall->front()
          : nullptr;
  if (outlinedCallReference == nullptr ||
      !outlinedCallReference->get_explicit_template_argument_list() ||
      actual == nullptr ||
      actual->get_argumentType() != SgTemplateArgument::type_argument ||
      actual->get_type() != capturedType) {
    fail("outlined call has no exact function-local explicit type argument");
  }

  return backend(project);
}
