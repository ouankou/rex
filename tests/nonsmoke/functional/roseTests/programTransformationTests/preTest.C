#include "rose.h"

#include "RoseAst.h"
#include "pre.h"

#include <vector>

#include <string>

using namespace std;

void requireSizeofComparison() {
  using namespace SageBuilder;
  SgType *sizeType = buildUnsignedLongType();
  SgSizeOfOp *first = buildSizeOfOp(buildIntType(), sizeType);
  SgSizeOfOp *same = buildSizeOfOp(buildIntType(), sizeType);
  SgSizeOfOp *different = buildSizeOfOp(buildDoubleType(), sizeType);
  SgSizeOfOp *value = buildSizeOfOp(buildIntVal(1), sizeType);
  SgSizeOfOp *sameValue = buildSizeOfOp(buildIntVal(1), sizeType);
  ROSE_ASSERT(expressionTreeEqual(first, same));
  ROSE_ASSERT(!expressionTreeEqual(first, different));
  ROSE_ASSERT(!expressionTreeEqual(first, value));
  ROSE_ASSERT(!expressionTreeEqual(value, first));
  ROSE_ASSERT(expressionTreeEqual(value, sameValue));
  for (SgSizeOfOp *expression : {first, same, different, value, sameValue})
    SageInterface::deleteAST(expression);
}

void requireCastComparison() {
  using namespace SageBuilder;
  auto cast = [](SgType *type) {
    return buildCastExp(buildDoubleVal(1.5), type, SgCastExp::e_C_style_cast,
                        SgCastExp::e_semantic_conversion_FloatingToIntegral,
                        SgCastExp::e_value_category_prvalue, {}, type);
  };
  SgCastExp *first = cast(buildIntType());
  SgCastExp *same = cast(buildIntType());
  SgCastExp *different = cast(buildLongType());
  ROSE_ASSERT(expressionTreeEqual(first, same));
  ROSE_ASSERT(!expressionTreeEqual(first, different));
  ROSE_ASSERT(!expressionTreeEqual(different, first));
  for (SgCastExp *expression : {first, same, different})
    SageInterface::deleteAST(expression);
}

void requireGeneratedPreprocessingOwnership(SgNode *root,
                                            const std::string &marker) {
  size_t matches = 0;
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgLocatedNode *owner = isSgLocatedNode(*current);
    if (owner == nullptr || owner->getAttachedPreprocessingInfo() == nullptr) {
      continue;
    }
    for (PreprocessingInfo *record : *owner->getAttachedPreprocessingInfo()) {
      if (record == nullptr ||
          record->getString().find(marker) == std::string::npos) {
        continue;
      }
      ++matches;
      Sg_File_Info *ownerInfo = owner->get_file_info();
      Sg_File_Info *recordInfo = record->get_file_info();
      if (SageInterface::getEnclosingSourceFile(owner, true) == nullptr ||
          ownerInfo == nullptr || recordInfo == nullptr ||
          ownerInfo->isShared() || recordInfo->isShared() ||
          ownerInfo->get_physical_file_id() < 0 ||
          recordInfo->get_physical_file_id() !=
              ownerInfo->get_physical_file_id() ||
          recordInfo->get_physical_filename() !=
              Sg_File_Info::getFilenameFromID(
                  ownerInfo->get_physical_file_id()) ||
          recordInfo->get_filenameString() != "Compiler-Generated in PRE" ||
          !record->isTransformation() || recordInfo->isTransformation() ||
          recordInfo->isCompilerGenerated() ||
          !recordInfo->isOutputInCodeGeneration()) {
        fprintf(stderr,
                "REX_TEST_ERROR: generated partial-redundancy preprocessing "
                "has no exact physical owner\n");
        ROSE_ABORT();
      }
    }
  }
  if (matches == 0) {
    fprintf(stderr,
            "REX_TEST_ERROR: partial redundancy elimination generated no "
            "owned preprocessing record\n");
    ROSE_ABORT();
  }
}

int main(int argc, char *argv[]) {
  vector<string> argvList(argv, argv + argc);
  SgProject *sageProject = frontend(argvList);
  requireSizeofComparison();
  requireCastComparison();
  AstTests::runAllTests(sageProject);

  legacy::PRE::partialRedundancyElimination(sageProject);
  requireGeneratedPreprocessingOwnership(sageProject,
                                         "Partial redundancy elimination:");

  AstTests::runAllTests(sageProject);

  sageProject->unparse();
  return 0;
}
