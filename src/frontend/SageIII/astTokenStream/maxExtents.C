#include "maxExtents.h"

#include "sage3basic.h"

MaxSourceExtents::InheritedAttribute::InheritedAttribute(
    SgSourceFile *input_sourceFile)
    : sourceFile(input_sourceFile) {}

MaxSourceExtents::SynthesizedAttribute::SynthesizedAttribute() {
  node = nullptr;
}

MaxSourceExtents::SynthesizedAttribute::SynthesizedAttribute(
    SgNode *n, int input_start_line, int input_start_column, int input_end_line,
    int input_end_column) {
  ASSERT_not_null(n);

  start_line = input_start_line;
  start_column = input_start_column;
  end_line = input_end_line;
  end_column = input_end_column;
  node = n;
}

MaxSourceExtents::InheritedAttribute
MaxSourceExtents::SourceExtentsTraversal::evaluateInheritedAttribute(
    SgNode * /*n*/, InheritedAttribute inheritedAttribute) {
  return inheritedAttribute;
}

MaxSourceExtents::SynthesizedAttribute
MaxSourceExtents::SourceExtentsTraversal::evaluateSynthesizedAttribute(
    SgNode *n, InheritedAttribute inheritedAttribute,
    SubTreeSynthesizedAttributes childAttributes) {
  const int max_value = 9999999;

  SgLocatedNode *this_locatedNode = isSgLocatedNode(n);
  if (this_locatedNode == nullptr) {
    return SynthesizedAttribute(n, max_value, max_value, 0, 0);
  }
  bool isInSameFile = this_locatedNode->get_startOfConstruct()->isSameFile(
      inheritedAttribute.sourceFile);
  bool isCompilerGenerated = this_locatedNode->isCompilerGenerated();

  // Set these to the default values if this is a compiler generate node or a
  // node from a different file.
  int this_start_line =
      (isCompilerGenerated == false && isInSameFile == true)
          ? this_locatedNode->get_startOfConstruct()->get_line()
          : max_value;
  int this_start_column =
      (isCompilerGenerated == false && isInSameFile == true)
          ? this_locatedNode->get_startOfConstruct()->get_col()
          : max_value;
  int this_end_line = (isCompilerGenerated == false && isInSameFile == true)
                          ? this_locatedNode->get_endOfConstruct()->get_line()
                          : 0;
  int this_end_column = (isCompilerGenerated == false && isInSameFile == true)
                            ? this_locatedNode->get_endOfConstruct()->get_col()
                            : 0;

  int local_start_line = (this_start_line > 0) ? this_start_line : max_value;
  int local_start_column =
      (this_start_column > 0) ? this_start_column : max_value;
  int local_end_line = (this_end_line > 0) ? this_end_line : 0;
  int local_end_column = (this_end_column > 0) ? this_end_column : 0;

  for (const SynthesizedAttribute &child : childAttributes) {
    if (child.node == nullptr) {
      continue;
    }
    ASSERT_require(isSgLocatedNode(child.node));

    if (child.start_line < local_start_line) {
      local_start_line = child.start_line;
      local_start_column = child.start_column;
    } else if (child.start_line == local_start_line &&
               child.start_column < local_start_column) {
      local_start_column = child.start_column;
    }

    if (child.end_line > local_end_line) {
      local_end_line = child.end_line;
      local_end_column = child.end_column;
    } else if (child.end_line == local_end_line &&
               child.end_column > local_end_column) {
      local_end_column = child.end_column;
    }
  }

  return SynthesizedAttribute(n, local_start_line, local_start_column,
                              local_end_line, local_end_column);
}

void MaxSourceExtents::computeMaxSourceExtents(SgSourceFile *sourceFile,
                                               SgNode *n, int &start_line,
                                               int &start_column, int &end_line,
                                               int &end_column) {
  ASSERT_not_null(sourceFile);

  InheritedAttribute inheritedAttribute(sourceFile);
  SourceExtentsTraversal traversal;

  SynthesizedAttribute return_attribute =
      traversal.traverse(n, inheritedAttribute);

  start_line = return_attribute.start_line;
  start_column = return_attribute.start_column;
  end_line = return_attribute.end_line;
  end_column = return_attribute.end_column;
}
