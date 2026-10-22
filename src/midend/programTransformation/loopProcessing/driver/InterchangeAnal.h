
#ifndef INTERCHANGE_ANAL
#define INTERCHANGE_ANAL

#include "LoopTransformOptions.h"

class CompSliceNest;
class CompSliceLocalityRegistry;
class ArrangeNestingOrder {
protected:
  virtual void SetNestingWeight(CompSliceLocalityRegistry *anal,
                                CompSliceNest &g, float *weightvec) = 0;
  void RearrangeSliceNest(CompSliceNest &g, float *weightvec);

public:
  virtual ~ArrangeNestingOrder() {}
  void operator()(CompSliceLocalityRegistry *anal, CompSliceNest &g);
  virtual LoopTransformOptions::OptType GetOptimizationType() = 0;
};

class ArrangeOrigNestingOrder : public ArrangeNestingOrder {
protected:
  virtual void SetNestingWeight(CompSliceLocalityRegistry *anal,
                                CompSliceNest &g, float *weightvec);

public:
  virtual LoopTransformOptions::OptType GetOptimizationType() {
    return LoopTransformOptions::NO_OPT;
  }
};

class ArrangeReuseOrder : public ArrangeNestingOrder {
protected:
  virtual void SetNestingWeight(CompSliceLocalityRegistry *anal,
                                CompSliceNest &g, float *weightvec);

public:
  virtual LoopTransformOptions::OptType GetOptimizationType() {
    return LoopTransformOptions::MULTI_LEVEL_OPT;
  }
};

#endif
