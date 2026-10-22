/******************************************
 * Category: DFA
 * Liveness Analysis Declaration
 * created by tps in Nov 2008
 *****************************************/

#ifndef __LivenessAnalysisBUP_HXX_LOADED__
#define __LivenessAnalysisBUP_HXX_LOADED__
#include "AstProcessing.h"

#include "LivenessAnalysis.h"

#include <vector>

class BottomUpTraversalLivenessIN
    : public AstBottomUpProcessing<std::vector<SgInitializedName *>> {
  LivenessAnalysis *live;

public:
  BottomUpTraversalLivenessIN(LivenessAnalysis *liveness) { live = liveness; }

protected:
  virtual std::vector<SgInitializedName *>
  evaluateSynthesizedAttribute(SgNode *node,
                               SynthesizedAttributesList synAttributes) {
    std::vector<SgInitializedName *> current_in = (live->getIn(node));

    std::vector<SgInitializedName *> newVec = defaultSynthesizedAttribute();

    for (auto &vec : synAttributes) {
      if (!vec.empty()) {
        newVec = live->merge_no_dups(vec, newVec);
      }
    }
    newVec = live->merge_no_dups(newVec, current_in);

    if (isSgStatement(node)) {
      live->setIn(node, newVec);
    }
    return newVec;
  }
  virtual std::vector<SgInitializedName *> defaultSynthesizedAttribute() {
    return std::vector<SgInitializedName *>();
  }
};

class BottomUpTraversalLivenessOUT
    : public AstBottomUpProcessing<std::vector<SgInitializedName *>> {
  LivenessAnalysis *live;

public:
  BottomUpTraversalLivenessOUT(LivenessAnalysis *liveness) { live = liveness; }

protected:
  virtual std::vector<SgInitializedName *>
  evaluateSynthesizedAttribute(SgNode *node,
                               SynthesizedAttributesList synAttributes) {
    std::vector<SgInitializedName *> current_out = (live->getOut(node));

    std::vector<SgInitializedName *> newVec = defaultSynthesizedAttribute();

    for (auto &vec : synAttributes) {
      if (!vec.empty()) {
        newVec = live->merge_no_dups(vec, newVec);
      }
    }
    newVec = live->merge_no_dups(newVec, current_out);

    if (isSgStatement(node)) {
      live->setOut(node, newVec);
    }
    return newVec;
  }
  virtual std::vector<SgInitializedName *> defaultSynthesizedAttribute() {
    return std::vector<SgInitializedName *>();
  }
};

#endif
