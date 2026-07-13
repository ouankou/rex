#include "RoseAst.h"
#include "rose.h"

#include <string>

namespace {

struct TagTypes {
  SgClassType *namedRecord = nullptr;
  SgEnumType *namedEnum = nullptr;
  SgClassType *anonymousRecord = nullptr;
  SgEnumType *anonymousEnum = nullptr;
};

SgNamedType *typedefBaseType(SgSourceFile *file, const std::string &name) {
  SgNamedType *result = nullptr;
  for (SgNode *node : RoseAst(file)) {
    SgTypedefDeclaration *declaration = isSgTypedefDeclaration(node);
    if (declaration == nullptr || declaration->get_name().getString() != name) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = isSgNamedType(declaration->get_base_type()->findBaseType());
    ROSE_ASSERT(result != nullptr);
    SgDeclarationStatement *definition =
        declaration->get_baseTypeDefiningDeclaration();
    ROSE_ASSERT(definition != nullptr &&
                declaration->get_declaration() == definition &&
                definition->get_parent() == declaration);
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

TagTypes collectTagTypes(SgSourceFile *file) {
  ROSE_ASSERT(file != nullptr);
  TagTypes result;
  for (SgNode *node : RoseAst(file)) {
    if (SgClassDeclaration *declaration = isSgClassDeclaration(node)) {
      if (declaration->get_name() == SgName("rex_c_named_record")) {
        SgClassType *type = isSgClassType(declaration->get_type());
        ROSE_ASSERT(type != nullptr);
        if (result.namedRecord == nullptr) {
          result.namedRecord = type;
        } else {
          ROSE_ASSERT(result.namedRecord == type);
        }
      }
    } else if (SgEnumDeclaration *declaration = isSgEnumDeclaration(node)) {
      if (declaration->get_name() == SgName("rex_c_named_enum")) {
        SgEnumType *type = isSgEnumType(declaration->get_type());
        ROSE_ASSERT(type != nullptr);
        if (result.namedEnum == nullptr) {
          result.namedEnum = type;
        } else {
          ROSE_ASSERT(result.namedEnum == type);
        }
      }
    }
  }

  result.anonymousRecord =
      isSgClassType(typedefBaseType(file, "rex_c_anonymous_record"));
  result.anonymousEnum =
      isSgEnumType(typedefBaseType(file, "rex_c_anonymous_enum"));
  ROSE_ASSERT(result.namedRecord != nullptr && result.namedEnum != nullptr &&
              result.anonymousRecord != nullptr &&
              result.anonymousEnum != nullptr);
  return result;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr && project->get_fileList().size() == 2);
  SgSourceFile *first = isSgSourceFile(project->get_fileList()[0]);
  SgSourceFile *second = isSgSourceFile(project->get_fileList()[1]);
  ROSE_ASSERT(first != nullptr && second != nullptr);

  const TagTypes firstTypes = collectTagTypes(first);
  const TagTypes secondTypes = collectTagTypes(second);

  // C tag identifiers and typedef names have no linkage.  Matching spellings
  // in distinct translation units can denote compatible types, but they are
  // not one project-wide AST type identity.
  ROSE_ASSERT(firstTypes.namedRecord != secondTypes.namedRecord);
  ROSE_ASSERT(firstTypes.namedEnum != secondTypes.namedEnum);
  ROSE_ASSERT(firstTypes.anonymousRecord != secondTypes.anonymousRecord);
  ROSE_ASSERT(firstTypes.anonymousEnum != secondTypes.anonymousEnum);

  return backend(project);
}
