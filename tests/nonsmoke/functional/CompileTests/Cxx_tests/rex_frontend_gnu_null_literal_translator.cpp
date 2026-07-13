#include "rose.h"

#include <string>

namespace {

bool belongsToSpecimen(const SgValueExp *value) {
  const Sg_File_Info *source = value->get_startOfConstruct();
  return source != nullptr &&
         source->get_filenameString().find(
             "rex_frontend_gnu_null_literal.cpp") != std::string::npos;
}

class GnuNullVerifier : public AstSimpleProcessing {
public:
  size_t count = 0;

  void visit(SgNode *node) override {
    SgValueExp *value = isSgValueExp(node);
    if (value == nullptr || !belongsToSpecimen(value)) {
      return;
    }

    std::string spelling;
    if (SgIntVal *integer = isSgIntVal(value)) {
      spelling = integer->get_valueString();
      ROSE_ASSERT(isSgTypeInt(integer->get_type()) != nullptr);
    } else if (SgLongIntVal *integer = isSgLongIntVal(value)) {
      spelling = integer->get_valueString();
      ROSE_ASSERT(isSgTypeLong(integer->get_type()) != nullptr);
    } else if (SgLongLongIntVal *integer = isSgLongLongIntVal(value)) {
      spelling = integer->get_valueString();
      ROSE_ASSERT(isSgTypeLongLong(integer->get_type()) != nullptr);
    } else {
      return;
    }

    if (spelling != "__null") {
      return;
    }

    const Sg_File_Info *source = value->get_startOfConstruct();
    ROSE_ASSERT(source != nullptr);
    ROSE_ASSERT(!source->isTransformation());
    ROSE_ASSERT(!source->isCompilerGenerated());
    ROSE_ASSERT(value->unparseToString() == "__null");
    ++count;
  }
};

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  GnuNullVerifier verifier;
  verifier.traverse(project, preorder);
  ROSE_ASSERT(verifier.count == 2);

  return backend(project);
}
