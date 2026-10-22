#pragma once

#include "rosedefs.h"

#include "Cxx_Grammar.h"

#include <map>
#include <optional>
#include <stack>

#include <string>

enum SimdType { Nothing, Addr3, ArmAddr3, Intel_AVX512, Arm_SVE2 };

enum OpType {
  None,
  Load,
  Broadcast,
  BroadcastZero,
  Gather,
  ExplicitGather,
  Scatter,
  Store,
  Add,
  Sub,
  Mul,
  Div
};

struct OmpSimdCompiler {

public:
  OmpSimdCompiler();
  ~OmpSimdCompiler();
  OmpSimdCompiler(const OmpSimdCompiler &) = delete;
  OmpSimdCompiler &operator=(const OmpSimdCompiler &) = delete;

  // Functions
  void omp_simd_pass1();
  void omp_simd_pass2();

  void omp_simd_build_3addr(SgExpression *rval, SgType *type);
  void omp_simd_build_math(VariantT op_type, SgType *type);
  void omp_simd_build_scalar_assign(SgExpression *node, SgType *type);
  void omp_simd_build_ptr_assign(SgExpression *pntr_exp, SgType *type);
  SgPntrArrRefExp *omp_simd_convert_ptr(SgExpression *pntr_exp);

  unsigned int omp_simd_get_length() const;

  // Helper functions
  SgBasicBlock *releaseBlock();
  void finishScalarLowering();
  Rose_STL_Container<SgNode *> *getIR();
  void setTarget(SgOmpSimdStatement *target);
  void setForLoop(SgForStatement *for_loop);
  void setInductionSymbol(SgVariableSymbol *symbol);
  bool isGeneratedTemporary(SgVariableSymbol *symbol) const;
  bool isPartialTemporary(SgVariableSymbol *symbol) const;
  bool isProductReduction(SgVariableSymbol *symbol) const;

private:
  enum class TemporaryKind { Vector, Pointer, Partial };
  struct Reduction {
    SgVariableSymbol *source;
    SgVariableSymbol *partial;
    SgOmpClause::omp_reduction_identifier_enum operation;
  };
  void prepareReductions();
  SgVariableSymbol *buildTemporary(SgType *type, TemporaryKind kind,
                                   SgInitializer *initializer = nullptr);
  bool isTemporary(SgVariableSymbol *symbol, TemporaryKind kind) const;

  SgBasicBlock *new_block = nullptr;
  SgBasicBlock *new_block_owner = nullptr;
  SgOmpSimdStatement *target = nullptr;
  SgForStatement *for_loop = nullptr;
  SgVariableSymbol *induction_symbol = nullptr;
  Rose_STL_Container<SgNode *> *ir_block = nullptr;
  bool block_released = false;
  std::optional<unsigned int> simdlen_;
  std::optional<unsigned int> safelen_;
  unsigned int next_name_id_ = 0;
  std::map<SgInitializedName *, Reduction> reduction_map_;
  std::map<SgInitializedName *, TemporaryKind> temporary_kinds_;

  std::stack<SgVariableSymbol *> value_stack_;
};

//
// Final translation functions
//

extern SimdType simd_arch;

void omp_simd_write_intel(SgOmpSimdStatement *target, SgForStatement *for_loop,
                          Rose_STL_Container<SgNode *> *ir_block,
                          unsigned int simd_length,
                          const OmpSimdCompiler &compiler);
void omp_simd_write_arm(SgOmpSimdStatement *target, SgForStatement *for_loop,
                        Rose_STL_Container<SgNode *> *ir_block,
                        const OmpSimdCompiler &compiler);
