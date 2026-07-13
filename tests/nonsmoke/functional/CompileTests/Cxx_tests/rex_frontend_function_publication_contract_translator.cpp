#include "nodeQuery.h"
#include "rose.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr const char *kRecursiveOwner = "rex_recursive_default_argument_owner";
constexpr const char *kExportedFunction = "rex_exported_context_function";

SgSourceFile *findMainFile(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    SgSourceFile *sourceFile = isSgSourceFile(file);
    if (sourceFile != nullptr && !sourceFile->get_isHeaderFile()) {
      return sourceFile;
    }
  }
  return nullptr;
}

SgFunctionDeclaration *canonicalFunction(SgFunctionDeclaration *declaration) {
  if (declaration == nullptr) {
    return nullptr;
  }
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  return canonical != nullptr ? canonical : declaration;
}

bool sameFunctionFamily(SgFunctionDeclaration *lhs,
                        SgFunctionDeclaration *rhs) {
  return lhs != nullptr && rhs != nullptr &&
         canonicalFunction(lhs) == canonicalFunction(rhs);
}

SgFunctionDeclaration *symbolFunctionDeclaration(SgSymbol *symbol) {
  if (SgTemplateMemberFunctionSymbol *templateMember =
          isSgTemplateMemberFunctionSymbol(symbol)) {
    return isSgFunctionDeclaration(templateMember->get_declaration());
  }
  if (SgTemplateFunctionSymbol *templateFunction =
          isSgTemplateFunctionSymbol(symbol)) {
    return isSgFunctionDeclaration(templateFunction->get_declaration());
  }
  if (SgMemberFunctionSymbol *member = isSgMemberFunctionSymbol(symbol)) {
    return member->get_declaration();
  }
  if (SgFunctionSymbol *function = isSgFunctionSymbol(symbol)) {
    return function->get_declaration();
  }
  return nullptr;
}

bool checkRecursiveDefaultArgumentPublication(SgProject *project) {
  SgMemberFunctionDeclaration *sourceConstructor = nullptr;
  SgInitializedName *defaultedParameter = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    SgMemberFunctionDeclaration *candidate =
        isSgMemberFunctionDeclaration(node);
    if (candidate == nullptr || candidate->get_name() != kRecursiveOwner ||
        candidate->get_parameterList() == nullptr ||
        candidate->get_parameterList()->get_args().size() != 1) {
      continue;
    }
    SgInitializedName *parameter =
        candidate->get_parameterList()->get_args().front();
    if (parameter == nullptr || parameter->get_initializer() == nullptr) {
      continue;
    }
    ROSE_ASSERT(sourceConstructor == nullptr);
    sourceConstructor = candidate;
    defaultedParameter = parameter;
  }
  if (sourceConstructor == nullptr) {
    return false;
  }

  ROSE_ASSERT(defaultedParameter != nullptr);
  ROSE_ASSERT(defaultedParameter->get_declptr() == sourceConstructor);
  ROSE_ASSERT(defaultedParameter->get_initializer()->get_parent() ==
              defaultedParameter);
  ROSE_ASSERT(isSgClassDefinition(sourceConstructor->get_scope()) != nullptr);
  ROSE_ASSERT(sourceConstructor->get_parent() ==
              sourceConstructor->get_scope());
  ROSE_ASSERT(canonicalFunction(sourceConstructor) == sourceConstructor);

  std::vector<SgNode *> initializers = NodeQuery::querySubTree(
      defaultedParameter->get_initializer(), V_SgConstructorInitializer);
  SgConstructorInitializer *recursiveInitializer = nullptr;
  for (SgNode *node : initializers) {
    SgConstructorInitializer *initializer = isSgConstructorInitializer(node);
    SgFunctionDeclaration *callee =
        initializer != nullptr ? initializer->get_declaration() : nullptr;
    if (callee != nullptr && callee->get_name() == kRecursiveOwner) {
      ROSE_ASSERT(recursiveInitializer == nullptr);
      recursiveInitializer = initializer;
    }
  }
  ROSE_ASSERT(recursiveInitializer != nullptr);

  SgFunctionDeclaration *callee = recursiveInitializer->get_declaration();
  ROSE_ASSERT(sameFunctionFamily(callee, sourceConstructor));
  ROSE_ASSERT(callee->get_scope() == sourceConstructor->get_scope());
  SgFunctionDeclaration *canonical = canonicalFunction(callee);
  ROSE_ASSERT(canonical == sourceConstructor);
  SgSymbol *symbol = canonical->get_symbol_from_symbol_table();
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(sameFunctionFamily(symbolFunctionDeclaration(symbol), canonical));
  return true;
}

bool checkExportContextScope(SgProject *project, SgSourceFile *sourceFile) {
  SgGlobal *global = sourceFile->get_globalScope();
  ROSE_ASSERT(global != nullptr);

  SgFunctionDeclaration *calledDeclaration = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgFunctionCallExp)) {
    SgFunctionCallExp *call = isSgFunctionCallExp(node);
    SgFunctionDeclaration *callee =
        call != nullptr ? call->getAssociatedFunctionDeclaration() : nullptr;
    if (callee != nullptr && callee->get_name() == kExportedFunction) {
      ROSE_ASSERT(calledDeclaration == nullptr);
      calledDeclaration = callee;
    }
  }
  if (calledDeclaration == nullptr) {
    return false;
  }

  SgFunctionDeclaration *canonical = canonicalFunction(calledDeclaration);
  ROSE_ASSERT(canonical != nullptr);
  ROSE_ASSERT(calledDeclaration->get_scope() == global);
  ROSE_ASSERT(canonical->get_scope() == global);

  SgNode *owner = canonical->get_parent();
  if (SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(owner)) {
    ROSE_ASSERT(auxiliary->get_parent() == global);
    ROSE_ASSERT(global->get_auxiliary_declarations() == auxiliary);
    ROSE_ASSERT(std::count(auxiliary->get_declarations().begin(),
                           auxiliary->get_declarations().end(),
                           canonical) == 1);
  } else {
    ROSE_ASSERT(owner == global);
  }

  SgSymbol *symbol = canonical->get_symbol_from_symbol_table();
  ROSE_ASSERT(symbol != nullptr);
  ROSE_ASSERT(sameFunctionFamily(symbolFunctionDeclaration(symbol), canonical));
  SgSymbolTable *symbolTable = isSgSymbolTable(symbol->get_parent());
  ROSE_ASSERT(symbolTable != nullptr);
  ROSE_ASSERT(symbolTable->get_parent() == global);
  return true;
}
} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(frontendExitStatus(project) == 0);

  SgSourceFile *sourceFile = findMainFile(project);
  ROSE_ASSERT(sourceFile != nullptr);

  const bool checkedRecursive =
      checkRecursiveDefaultArgumentPublication(project);
  const bool checkedExport = checkExportContextScope(project, sourceFile);
  ROSE_ASSERT(checkedRecursive != checkedExport);
  return 0;
}
