#ifndef ASTREVERSEPROCESSING_H
#define ASTREVERSEPROCESSING_H

#include "AstProcessing.h"

template <typename InheritedAttributeType>
class AstReversePrefixInhProcessing
    : public AstTopDownProcessing<InheritedAttributeType> {
public:
  AstReversePrefixInhProcessing();

protected:
  virtual void setNodeSuccessors(
      SgNode *node,
      typename AstReversePrefixInhProcessing<
          InheritedAttributeType>::SuccessorsContainer &succContainer);
};

template <class SynthesizedAttributeType>
class AstReversePrefixSynProcessing
    : public AstBottomUpProcessing<SynthesizedAttributeType> {
public:
  AstReversePrefixSynProcessing();

protected:
  virtual void setNodeSuccessors(
      SgNode *node,
      typename AstReversePrefixSynProcessing<
          SynthesizedAttributeType>::SuccessorsContainer &succContainer);
};

template <class InheritedAttributeType, class SynthesizedAttributeType>
class AstReversePrefixInhSynProcessing
    : public AstTopDownBottomUpProcessing<InheritedAttributeType,
                                          SynthesizedAttributeType> {
public:
  AstReversePrefixInhSynProcessing();

protected:
  virtual void setNodeSuccessors(
      SgNode *node,
      typename AstReversePrefixInhSynProcessing<
          InheritedAttributeType, SynthesizedAttributeType>::SuccessorsContainer
          &succContainer);
};

template <class InheritedAttributeType, class SynthesizedAttributeType>
class AstReverseBranchInhSynProcessing
    : public AstTopDownBottomUpProcessing<InheritedAttributeType,
                                          SynthesizedAttributeType> {
public:
  AstReverseBranchInhSynProcessing();

protected:
  virtual void setNodeSuccessors(
      SgNode *node,
      typename AstReverseBranchInhSynProcessing<
          InheritedAttributeType, SynthesizedAttributeType>::SuccessorsContainer
          &succContainer);
};

template <class InheritedAttributeType>
class AstReverseBranchInhProcessing
    : public AstTopDownProcessing<InheritedAttributeType> {
public:
  AstReverseBranchInhProcessing();

protected:
  virtual void setNodeSuccessors(
      SgNode *node,
      typename AstReverseBranchInhProcessing<
          InheritedAttributeType>::SuccessorsContainer &succContainer);
};

template <class SynthesizedAttributeType>
class AstReverseBranchSynProcessing
    : public AstBottomUpProcessing<SynthesizedAttributeType> {
public:
  AstReverseBranchSynProcessing();

protected:
  virtual void setNodeSuccessors(
      SgNode *node,
      typename AstReverseBranchSynProcessing<
          SynthesizedAttributeType>::SuccessorsContainer &succContainer);
};

template <class InheritedAttributeType>
AstReversePrefixInhProcessing<
    InheritedAttributeType>::AstReversePrefixInhProcessing() {
  AstTopDownProcessing<
      InheritedAttributeType>::set_useDefaultIndexBasedTraversal(false);
}

template <class InheritedAttributeType>
void AstReversePrefixInhProcessing<InheritedAttributeType>::setNodeSuccessors(
    SgNode *node,
    typename AstReversePrefixInhProcessing<
        InheritedAttributeType>::SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReversePrefixSuccessors(node, succContainer);
}

template <class SynthesizedAttributeType>
AstReversePrefixSynProcessing<
    SynthesizedAttributeType>::AstReversePrefixSynProcessing() {
  AstBottomUpProcessing<
      SynthesizedAttributeType>::set_useDefaultIndexBasedTraversal(false);
}

template <class SynthesizedAttributeType>
void AstReversePrefixSynProcessing<SynthesizedAttributeType>::setNodeSuccessors(
    SgNode *node,
    typename AstReversePrefixSynProcessing<
        SynthesizedAttributeType>::SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReversePrefixSuccessors(node, succContainer);
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
AstReversePrefixInhSynProcessing<
    InheritedAttributeType,
    SynthesizedAttributeType>::AstReversePrefixInhSynProcessing() {
  AstTopDownBottomUpProcessing<
      InheritedAttributeType,
      SynthesizedAttributeType>::set_useDefaultIndexBasedTraversal(false);
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void AstReversePrefixInhSynProcessing<InheritedAttributeType,
                                      SynthesizedAttributeType>::
    setNodeSuccessors(
        SgNode *node,
        typename AstReversePrefixInhSynProcessing<
            InheritedAttributeType,
            SynthesizedAttributeType>::SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReversePrefixSuccessors(node, succContainer);
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
AstReverseBranchInhSynProcessing<
    InheritedAttributeType,
    SynthesizedAttributeType>::AstReverseBranchInhSynProcessing() {
  AstTopDownBottomUpProcessing<
      InheritedAttributeType,
      SynthesizedAttributeType>::set_useDefaultIndexBasedTraversal(false);
}

template <class InheritedAttributeType, class SynthesizedAttributeType>
void AstReverseBranchInhSynProcessing<InheritedAttributeType,
                                      SynthesizedAttributeType>::
    setNodeSuccessors(
        SgNode *node,
        typename AstReverseBranchInhSynProcessing<
            InheritedAttributeType,
            SynthesizedAttributeType>::SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReverseBranchSuccessors(node, succContainer);
}

template <class InheritedAttributeType>
AstReverseBranchInhProcessing<
    InheritedAttributeType>::AstReverseBranchInhProcessing() {
  AstTopDownProcessing<
      InheritedAttributeType>::set_useDefaultIndexBasedTraversal(false);
}

template <class InheritedAttributeType>
void AstReverseBranchInhProcessing<InheritedAttributeType>::setNodeSuccessors(
    SgNode *node,
    typename AstReverseBranchInhProcessing<
        InheritedAttributeType>::SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReverseBranchSuccessors(node, succContainer);
}

template <class SynthesizedAttributeType>
AstReverseBranchSynProcessing<
    SynthesizedAttributeType>::AstReverseBranchSynProcessing() {
  AstBottomUpProcessing<
      SynthesizedAttributeType>::set_useDefaultIndexBasedTraversal(false);
}

template <class SynthesizedAttributeType>
void AstReverseBranchSynProcessing<SynthesizedAttributeType>::setNodeSuccessors(
    SgNode *node,
    typename AstReverseBranchSynProcessing<
        SynthesizedAttributeType>::SuccessorsContainer &succContainer) {
  AstSuccessorsSelectors::selectReverseBranchSuccessors(node, succContainer);
}

#include "AstReverseSimpleProcessing.h"

#endif
