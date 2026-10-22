#ifndef ROSE_MAX_SOURCE_EXTENTS_H
#define ROSE_MAX_SOURCE_EXTENTS_H

#include "AstProcessing.h"

// Find the source extent of a subtree for token mapping.

namespace MaxSourceExtents {

class InheritedAttribute {
public:
  SgSourceFile *sourceFile;

  explicit InheritedAttribute(SgSourceFile *input_sourceFile);
};

class SynthesizedAttribute {
public:
  SgNode *node;

  int start_line;
  int start_column;
  int end_line;
  int end_column;

  SynthesizedAttribute();
  SynthesizedAttribute(SgNode *n, int start_line, int start_column,
                       int end_line, int end_column);
};

class SourceExtentsTraversal
    : public SgTopDownBottomUpProcessing<InheritedAttribute,
                                         SynthesizedAttribute> {
public:
  InheritedAttribute
  evaluateInheritedAttribute(SgNode *n, InheritedAttribute inheritedAttribute);

  SynthesizedAttribute
  evaluateSynthesizedAttribute(SgNode *n, InheritedAttribute inheritedAttribute,
                               SubTreeSynthesizedAttributes childAttributes);
};

void computeMaxSourceExtents(SgSourceFile *sourceFile, SgNode *n,
                             int &start_line, int &start_column, int &end_line,
                             int &end_column);

} // namespace MaxSourceExtents

#endif
