#include "rose.h"

#include "FileHelper.h"
#include "Clang/clang-decl-attachment-session.hpp"

#include <filesystem>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
class TemporaryTree {
public:
  explicit TemporaryTree(std::filesystem::path path) : path_(std::move(path)) {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    error.clear();
    std::filesystem::create_directories(path_, error);
    if (error) {
      path_.clear();
    }
  }

  ~TemporaryTree() {
    if (!path_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

bool writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream output(path,
                       std::ios::out | std::ios::binary | std::ios::trunc);
  output.write(contents.data(), contents.size());
  output.close();
  return !output.fail();
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::in | std::ios::binary);
  std::ostringstream contents;
  contents << input.rdbuf();
  return input.bad() ? std::string() : contents.str();
}

class StructuralFingerprint : public AstSimpleProcessing {
public:
  void visit(SgNode *node) override {
    std::ostringstream entry;
    entry << node << ':' << node->variantT() << ':' << node->get_parent() << ':'
          << node->get_isModified() << ':'
          << node->get_containsTransformation();
    if (Sg_File_Info *info = node->get_file_info()) {
      entry << ':' << info << ':' << info->get_file_id() << ':'
            << info->get_physical_file_id() << ':' << info->get_line() << ':'
            << info->get_col() << ':' << info->isTransformation() << ':'
            << info->isCompilerGenerated() << ':'
            << info->isOutputInCodeGeneration();
    }
    if (AstAttributeMechanism *attributes = node->get_attributeMechanism()) {
      const auto names = attributes->getAttributeIdentifiers();
      for (const std::string &name : names) {
        entry << ":attribute=" << name;
      }
    }
    entries.push_back(entry.str());
  }

  std::vector<std::string> entries;
};

std::vector<std::string> fingerprint(SgProject *project) {
  StructuralFingerprint traversal;
  traversal.traverse(project, preorder);
  return traversal.entries;
}

bool verifyDeclAttachmentSessionsAreIsolated() {
  using DeclarationList = SgDeclarationStatementPtrList;
  auto sourceInfo = [](int line) {
    Sg_File_Info *info =
        new Sg_File_Info("rex_decl_attachment_session.cpp", line, 1);
    info->setOutputInCodeGeneration();
    return info;
  };

  SgGlobal *owner = new SgGlobal();
  SgEmptyDeclaration *first = new SgEmptyDeclaration(
      sourceInfo(10), SgEmptyDeclaration::e_empty_declaration_source_semicolon);
  SgEmptyDeclaration *second = new SgEmptyDeclaration(
      sourceInfo(10), SgEmptyDeclaration::e_empty_declaration_source_semicolon);
  SgEmptyDeclaration *stableTail = new SgEmptyDeclaration(
      sourceInfo(20), SgEmptyDeclaration::e_empty_declaration_source_semicolon);

  alignas(DeclarationList) unsigned char listStorage[sizeof(DeclarationList)];
  DeclarationList *declarations = new (listStorage) DeclarationList();
  declarations->push_back(first);
  declarations->push_back(stableTail);

  bool firstSessionIsExact = false;
  {
    DeclAttachmentSession firstSession;
    const std::vector<SgDeclarationStatement *> *sameLine =
        firstSession.lookupDeclarationsOnSameSourceLine(
            declarations, first->get_startOfConstruct());
    firstSessionIsExact =
        firstSession.contains(declarations, owner, first) &&
        firstSession.contains(declarations, owner, stableTail) &&
        !firstSession.contains(declarations, owner, second) &&
        sameLine != nullptr && sameLine->size() == 1 &&
        sameLine->front() == first;
  }
  declarations->~DeclarationList();
  if (!firstSessionIsExact) {
    return false;
  }

  // Reuse the exact list address, owner, size, and tail in a second frontend
  // session while changing an interior declaration. A process-global cache
  // cannot distinguish this from the prior project and returns stale members.
  declarations = new (listStorage) DeclarationList();
  declarations->push_back(second);
  declarations->push_back(stableTail);
  bool secondSessionIsExact = false;
  {
    DeclAttachmentSession secondSession;
    const std::vector<SgDeclarationStatement *> *sameLine =
        secondSession.lookupDeclarationsOnSameSourceLine(
            declarations, second->get_startOfConstruct());
    secondSessionIsExact =
        !secondSession.contains(declarations, owner, first) &&
        secondSession.contains(declarations, owner, second) &&
        secondSession.contains(declarations, owner, stableTail) &&
        sameLine != nullptr && sameLine->size() == 1 &&
        sameLine->front() == second;
  }
  declarations->~DeclarationList();
  return secondSessionIsExact;
}

bool verifyExactDeclAttachmentMutations() {
  auto sourceInfo = [](int line) {
    Sg_File_Info *info =
        new Sg_File_Info("rex_decl_attachment_exact.cpp", line, 1);
    info->setOutputInCodeGeneration();
    return info;
  };

  DeclAttachmentSession session;
  SgGlobal *owner = new SgGlobal();
  SgEmptyDeclaration *existing = new SgEmptyDeclaration(
      sourceInfo(10), SgEmptyDeclaration::e_empty_declaration_source_semicolon);
  SgEmptyDeclaration *replacement = new SgEmptyDeclaration(
      sourceInfo(20), SgEmptyDeclaration::e_empty_declaration_source_semicolon);
  existing->set_parent(owner);
  owner->get_declarations().push_back(existing);
  existing->set_scope(owner);

  if (!session.containsExactly(&owner->get_declarations(), owner, existing,
                               "contract-positive-membership")) {
    return false;
  }
  session.eraseExactly(&owner->get_declarations(), owner, existing,
                       "contract-positive-detach");
  if (!owner->get_declarations().empty() || existing->get_parent() != nullptr) {
    return false;
  }

  owner->get_declarations().push_back(existing);
  session.recordInsertion(&owner->get_declarations(), existing);
  existing->set_parent(owner);
  session.replaceExactly(&owner->get_declarations(), owner, existing,
                         replacement, "contract-positive-replacement");
  return owner->get_declarations().size() == 1 &&
         owner->get_declarations().front() == replacement &&
         existing->get_parent() == nullptr &&
         replacement->get_parent() == owner &&
         replacement->get_declarationScope() == nullptr &&
         session.containsExactly(&owner->get_declarations(), owner, replacement,
                                 "contract-positive-result");
}

SgProject *buildProject(const std::filesystem::path &source,
                        const std::filesystem::path &output) {
  const std::vector<std::string> arguments = {"rex_unparser_project_isolation",
                                              "-rose:verbose",
                                              "0",
                                              "-rose:skipfinalCompileStep",
                                              "-rose:unparse_tokens",
                                              "-rose:output",
                                              output.string(),
                                              "-std=c++20",
                                              "-c",
                                              source.string()};
  return frontend(arguments);
}

bool replaceFirstIntegerLiteral(SgProject *project, int replacement) {
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgIntVal)) {
    SgIntVal *oldValue = isSgIntVal(node);
    if (oldValue == nullptr) {
      continue;
    }
    SgIntVal *newValue = SageBuilder::buildIntVal(replacement);
    if (newValue == nullptr) {
      return false;
    }
    SageInterface::replaceExpression(oldValue, newValue, false);
    return true;
  }
  return false;
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    const std::string mode = argv[1];
    DeclAttachmentSession session;
    SgDeclarationStatementPtrList declarations;
    SgGlobal *owner = new SgGlobal();
    Sg_File_Info *location =
        new Sg_File_Info("rex_decl_attachment_contract.cpp", 1, 1);
    SgEmptyDeclaration *declaration = new SgEmptyDeclaration(
        location, SgEmptyDeclaration::e_empty_declaration_source_semicolon);

    if (mode == "--null-list") {
      session.contains(nullptr, owner, declaration);
    } else if (mode == "--null-owner") {
      session.contains(&declarations, nullptr, declaration);
    } else if (mode == "--null-declaration") {
      session.contains(&declarations, owner, nullptr);
    } else if (mode == "--null-membership-entry") {
      declarations.push_back(nullptr);
      session.contains(&declarations, owner, declaration);
    } else if (mode == "--null-source-line-entry") {
      declarations.push_back(nullptr);
      session.lookupDeclarationsOnSameSourceLine(&declarations, location);
    } else if (mode == "--null-insertion") {
      session.recordInsertion(&declarations, nullptr);
    } else if (mode == "--null-erasure") {
      session.recordErasure(&declarations, nullptr, false);
    } else if (mode == "--null-existing-replacement") {
      session.recordReplacement(&declarations, nullptr, declaration);
    } else if (mode == "--null-new-replacement") {
      session.recordReplacement(&declarations, declaration, nullptr);
    } else if (mode == "--null-source-line-list") {
      session.lookupDeclarationsOnSameSourceLine(nullptr, location);
    } else if (mode == "--null-source-line-location") {
      session.lookupDeclarationsOnSameSourceLine(&declarations, nullptr);
    } else if (mode == "--null-invalidation-list") {
      session.invalidate(nullptr);
    } else if (mode == "--duplicate-child-membership") {
      declaration->set_parent(owner);
      declarations.push_back(declaration);
      declarations.push_back(declaration);
      session.containsExactly(&declarations, owner, declaration,
                              "contract-duplicate");
    } else if (mode == "--parent-list-disagreement") {
      SgGlobal *otherOwner = new SgGlobal();
      declaration->set_parent(otherOwner);
      declarations.push_back(declaration);
      session.containsExactly(&declarations, owner, declaration,
                              "contract-parent-list");
    } else if (mode == "--simultaneous-cross-owner-membership") {
      SgGlobal *otherOwner = new SgGlobal();
      declaration->set_parent(owner);
      declarations.push_back(declaration);
      otherOwner->get_declarations().push_back(declaration);
      session.containsExactly(&otherOwner->get_declarations(), otherOwner,
                              declaration, "contract-cross-owner");
    } else if (mode == "--replacement-already-present") {
      SgEmptyDeclaration *replacement = new SgEmptyDeclaration(
          new Sg_File_Info("rex_decl_attachment_contract.cpp", 2, 1),
          SgEmptyDeclaration::e_empty_declaration_source_semicolon);
      declaration->set_parent(owner);
      replacement->set_parent(owner);
      declarations.push_back(declaration);
      declarations.push_back(replacement);
      session.replaceExactly(&declarations, owner, declaration, replacement,
                             "contract-replacement-present");
    } else if (mode == "--replacement-cross-owned") {
      SgGlobal *otherOwner = new SgGlobal();
      SgEmptyDeclaration *replacement = new SgEmptyDeclaration(
          new Sg_File_Info("rex_decl_attachment_contract.cpp", 2, 1),
          SgEmptyDeclaration::e_empty_declaration_source_semicolon);
      declaration->set_parent(owner);
      replacement->set_parent(otherOwner);
      declarations.push_back(declaration);
      otherOwner->get_declarations().push_back(replacement);
      session.replaceExactly(&declarations, owner, declaration, replacement,
                             "contract-replacement-cross-owned");
    } else if (mode == "--detach-missing-child") {
      session.eraseExactly(&declarations, owner, declaration,
                           "contract-detach-missing");
    } else {
      return 17;
    }
    return 0;
  }
  if (argc != 1) {
    return 18;
  }

  const std::filesystem::path root =
      std::filesystem::current_path() /
      ("rex_unparser_project_isolation_" + std::to_string(getpid()));
  TemporaryTree temporary(root);
  if (temporary.path().empty()) {
    return 1;
  }

  const std::filesystem::path firstSource = root / "rex_unparser_first.cpp";
  const std::filesystem::path secondSource = root / "rex_unparser_second.cpp";
  const std::filesystem::path firstOutput =
      root / "rose_rex_unparser_first.cpp";
  const std::filesystem::path secondOutput =
      root / "rose_rex_unparser_second.cpp";
  if (!writeFile(firstSource, "namespace rex_first { struct item {}; }\n"
                              "int rex_first_value = 1;\n") ||
      !writeFile(secondSource, "namespace rex_second { struct item {}; }\n"
                               "rex_second::item rex_second_value;\n")) {
    return 2;
  }

  // Path identity is filesystem state, not process-global unparser state. A
  // symlink retargeted between two live projects must be normalized again.
  const std::filesystem::path firstPathTarget = root / "path_target_first.h";
  const std::filesystem::path secondPathTarget = root / "path_target_second.h";
  const std::filesystem::path pathAlias = root / "path_alias.h";
  if (!writeFile(firstPathTarget, "#define REX_PATH_TARGET 1\n") ||
      !writeFile(secondPathTarget, "#define REX_PATH_TARGET 2\n")) {
    return 3;
  }
  std::error_code pathError;
  std::filesystem::create_symlink(firstPathTarget, pathAlias, pathError);
  if (pathError) {
    return 4;
  }
  const std::string firstNormalizedPath =
      FileHelper::normalizePathIfPossible(pathAlias.string());
  std::filesystem::remove(pathAlias, pathError);
  if (pathError) {
    return 5;
  }
  std::filesystem::create_symlink(secondPathTarget, pathAlias, pathError);
  if (pathError) {
    return 6;
  }
  const std::string secondNormalizedPath =
      FileHelper::normalizePathIfPossible(pathAlias.string());
  if (firstNormalizedPath == secondNormalizedPath) {
    return 7;
  }

  // Keep both ASTs alive before either is emitted. Qualification and unparser
  // state must be tied to the selected invocation, not the latest project.
  SgProject *firstProject = buildProject(firstSource, firstOutput);
  if (firstProject == nullptr) {
    return 8;
  }

  // An unreachable declaration in the process-wide allocation pool does not
  // belong to any AST. Building another project must not silently attach it to
  // that project's global scope.
  SgTemplateArgumentPtrList detachedArguments;
  SgTemplateInstantiationDecl *detachedDeclaration =
      new SgTemplateInstantiationDecl(
          SgName("rex_detached<int>"), SgClassDeclaration::e_class, nullptr,
          nullptr, nullptr, detachedArguments, SgTemplateArgumentPtrList());
  SageInterface::setSourcePosition(detachedDeclaration);
  detachedDeclaration->set_templateName(SgName("rex_detached"));
  detachedDeclaration->set_firstNondefiningDeclaration(detachedDeclaration);
  detachedDeclaration->set_definingDeclaration(nullptr);
  if (detachedDeclaration->get_parent() != nullptr) {
    return 9;
  }

  SgProject *secondProject = buildProject(secondSource, secondOutput);
  if (firstProject == nullptr || secondProject == nullptr ||
      firstProject == secondProject ||
      detachedDeclaration->get_parent() != nullptr) {
    return 10;
  }
  if (!SageInterface::collectModifiedLocatedNodes(firstProject).empty() ||
      !SageInterface::collectModifiedLocatedNodes(secondProject).empty()) {
    return 11;
  }
  if (!replaceFirstIntegerLiteral(firstProject, 7)) {
    return 12;
  }

  const std::vector<std::string> firstFingerprint = fingerprint(firstProject);
  firstProject->unparse();
  const std::string firstText = readFile(firstOutput);
  if (firstText.find("rex_first_value") == std::string::npos ||
      firstText.find('7') == std::string::npos ||
      firstText.find("rex_second_value") != std::string::npos ||
      fingerprint(firstProject) != firstFingerprint) {
    return 13;
  }

  const std::vector<std::string> secondFingerprint = fingerprint(secondProject);
  secondProject->unparse();
  const std::string secondText = readFile(secondOutput);
  if (secondText.find("rex_second_value") == std::string::npos ||
      secondText.find("rex_first_value") != std::string::npos ||
      fingerprint(secondProject) != secondFingerprint) {
    return 14;
  }

  firstProject->unparse();
  if (readFile(firstOutput) != firstText ||
      fingerprint(firstProject) != firstFingerprint) {
    return 15;
  }

  if (!verifyDeclAttachmentSessionsAreIsolated()) {
    return 16;
  }
  if (!verifyExactDeclAttachmentMutations()) {
    return 19;
  }

  return 0;
}
