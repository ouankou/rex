/******************************************
 * Category: DFA
 * Liveness Analysis Declaration
 * created by tps in Nov 2008
 *****************************************/

#ifndef __LivenessAnalysisBUP_HXX_LOADED__
#define __LivenessAnalysisBUP_HXX_LOADED__
#include "AstProcessing.h"

#include "LivenessAnalysis.h"

#include <string>

class BottomUpTraversalLivenessIN
    : public AstBottomUpProcessing<std::vector<SgInitializedName *>> {
  LivenessAnalysis *live;

public:
  BottomUpTraversalLivenessIN(LivenessAnalysis *liveness) { live = liveness; }
  virtual ~BottomUpTraversalLivenessIN() {};

protected:
  virtual std::vector<SgInitializedName *>
  evaluateSynthesizedAttribute(SgNode *node,
                               SynthesizedAttributesList synAttributes) {
    std::vector<SgInitializedName *> current_in = (live->getIn(node));

    //    std::cout << ">>> visiting node : " << node->class_name() << "
    //    elements IN[]: " << current_in.size()
    //            <<"  merging with nodes: " << synAttributes.size() <<
    //            std::endl;
    std::vector<SgInitializedName *> newVec = defaultSynthesizedAttribute();

    for (auto &vec : synAttributes) {
      // merge vec and in
      // std::cout << "    >>> Merging vec and newVec    --- vec size :" <<
      // vec.size() << std::endl;
      if (!vec.empty()) {
        newVec = live->merge_no_dups(vec, newVec);
      }
    }
    newVec = live->merge_no_dups(newVec, current_in);

    if (isSgStatement(node)) {
      // replace the current in[SgNode] map
      live->setIn(node, newVec);
      // std::cout << "  --- replacing stmt : newVecElements : " <<
      // newVec.size() << std::endl;
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
  virtual ~BottomUpTraversalLivenessOUT() {};

protected:
  virtual std::vector<SgInitializedName *>
  evaluateSynthesizedAttribute(SgNode *node,
                               SynthesizedAttributesList synAttributes) {
    std::vector<SgInitializedName *> current_out = (live->getOut(node));

    //    std::cout << ">>> visiting node : " << node->class_name() << "
    //    elements OUT[]: " << current_out.size()
    //            <<"  merging with nodes: " << synAttributes.size() <<
    //            std::endl;
    std::vector<SgInitializedName *> newVec = defaultSynthesizedAttribute();

    for (auto &vec : synAttributes) {
      // merge vec and out
      // std::cout << "    >>> Merging vec and newVec    --- vec size :" <<
      // vec.size() << std::endl;
      if (!vec.empty()) {
        newVec = live->merge_no_dups(vec, newVec);
      }
    }
    newVec = live->merge_no_dups(newVec, current_out);

    if (isSgStatement(node)) {
      // replace the current out[SgNode] map
      live->setOut(node, newVec);
      // std::cout << "  --- replacing stmt : newVecElements : " <<
      // newVec.size() << std::endl;
    }
    return newVec;
  }
  virtual std::vector<SgInitializedName *> defaultSynthesizedAttribute() {
    return std::vector<SgInitializedName *>();
  }
};

#endif
