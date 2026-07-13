
#include <algorithm>

#include <limits>

#include <iostream>

#include <optional>

#include <unordered_map>

#include <vector>

#include "omp_lowering.h"

#include "omp_simd.h"

#include "sage3basic.h"

#include "sageBuilder.h"

using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;

namespace {

enum class IntelLaneWidthKind { I32OrFloat, Double };

enum class IntelGeneratedSymbolRole { Value, PartialReduction };

[[noreturn]] void failIntelSimdEmission(const char *contract, SgNode *node,
                                        const char *detail) {
  fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[%s]: node=%p/%s %s\n", contract,
          static_cast<void *>(node),
          node != nullptr ? node->class_name().c_str() : "<null>", detail);
  ROSE_ABORT();
}

class IntelSimdEmissionTransaction {
public:
  IntelSimdEmissionTransaction(unsigned int vector_width,
                               IntelLaneWidthKind lane_kind)
      : vector_width_(vector_width),
        loop_increment_(lane_kind == IntelLaneWidthKind::Double
                            ? vector_width / 2
                            : vector_width) {
    if ((vector_width_ != 4 && vector_width_ != 8 && vector_width_ != 16) ||
        loop_increment_ == 0) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-length]: width=%u "
              "cannot initialize an exact Intel emission transaction\n",
              vector_width_);
      ROSE_ABORT();
    }
  }

  unsigned int vectorWidth() const { return vector_width_; }
  unsigned int loopIncrement() const { return loop_increment_; }

  std::string nextBufferName() { return nextName("__buf", next_buffer_id_); }
  std::string nextMaskName() { return nextName("__mask", next_mask_id_); }
  std::string nextIndexName() { return nextName("__vindex", next_index_id_); }

  void planGeneratedSymbol(SgVariableSymbol *original_symbol,
                           IntelGeneratedSymbolRole role) {
    if (original_symbol == nullptr ||
        original_symbol->get_declaration() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: generated "
              "SIMD value has no exact original symbol identity\n");
      ROSE_ABORT();
    }
    const auto [position, inserted] = generated_symbols_.emplace(
        original_symbol, GeneratedSymbolRecord{role, nullptr});
    if (!inserted && position->second.role != role) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: original "
              "symbol=%p name=%s has conflicting generated roles\n",
              static_cast<void *>(original_symbol),
              original_symbol->get_name().getString().c_str());
      ROSE_ABORT();
    }
  }

  bool isPlannedPartial(SgVariableSymbol *original_symbol) const {
    const auto position = generated_symbols_.find(original_symbol);
    return position != generated_symbols_.end() &&
           position->second.role == IntelGeneratedSymbolRole::PartialReduction;
  }

  bool needsPartialOutput(SgVariableSymbol *original_symbol) const {
    const auto position = generated_symbols_.find(original_symbol);
    if (position == generated_symbols_.end() ||
        position->second.role != IntelGeneratedSymbolRole::PartialReduction) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: original "
              "symbol=%p is not one exact planned partial reduction\n",
              static_cast<void *>(original_symbol));
      ROSE_ABORT();
    }
    return position->second.output_symbol == nullptr;
  }

  std::string uniqueOutputName(SgVariableSymbol *original_symbol,
                               SgScopeStatement *output_scope) const {
    const auto position = generated_symbols_.find(original_symbol);
    if (position == generated_symbols_.end() ||
        position->second.output_symbol != nullptr || output_scope == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: original "
              "symbol=%p cannot request one exact fresh output name\n",
              static_cast<void *>(original_symbol));
      ROSE_ABORT();
    }
    const std::string base =
        position->second.role == IntelGeneratedSymbolRole::PartialReduction
            ? "rex_intel_simd_partial_"
            : "rex_intel_simd_value_";
    return SageInterface::generateUniqueVariableName(output_scope, base);
  }

  void bindGeneratedOutput(SgVariableSymbol *original_symbol,
                           SgVariableSymbol *output_symbol) {
    const auto position = generated_symbols_.find(original_symbol);
    if (position == generated_symbols_.end() || output_symbol == nullptr ||
        output_symbol == original_symbol ||
        output_symbol->get_declaration() == nullptr ||
        original_symbol->get_name() == output_symbol->get_name()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: original "
              "symbol=%p and output symbol=%p do not form one exact generated "
              "identity mapping\n",
              static_cast<void *>(original_symbol),
              static_cast<void *>(output_symbol));
      ROSE_ABORT();
    }
    for (const auto &[other_original, record] : generated_symbols_) {
      if (other_original != original_symbol &&
          record.output_symbol == output_symbol) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: output "
                "symbol=%p is shared by two generated original identities\n",
                static_cast<void *>(output_symbol));
        ROSE_ABORT();
      }
    }
    if (position->second.output_symbol != nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: original "
              "symbol=%p name=%s already owns an output identity\n",
              static_cast<void *>(original_symbol),
              original_symbol->get_name().getString().c_str());
      ROSE_ABORT();
    }
    position->second.output_symbol = output_symbol;
  }

  void retargetCopiedReference(SgVarRefExp *reference,
                               const char *contract) const {
    if (reference == nullptr || reference->get_symbol() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[%s]: copied operand contains a "
              "variable reference without exact identity\n",
              contract);
      ROSE_ABORT();
    }
    SgVariableSymbol *original_symbol = reference->get_symbol();
    const auto position = generated_symbols_.find(original_symbol);
    if (position == generated_symbols_.end())
      return;
    SgVariableSymbol *output_symbol = position->second.output_symbol;
    if (output_symbol == nullptr ||
        output_symbol->get_declaration() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[%s]: generated original symbol=%p "
              "name=%s has no exact output identity\n",
              contract, static_cast<void *>(original_symbol),
              original_symbol->get_name().getString().c_str());
      ROSE_ABORT();
    }
    reference->set_symbol(output_symbol);
    if (reference->get_symbol() != output_symbol) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[%s]: generated original symbol=%p "
              "name=%s could not bind its exact output identity\n",
              contract, static_cast<void *>(original_symbol),
              original_symbol->get_name().getString().c_str());
      ROSE_ABORT();
    }
  }

  void beginOutputRegion(SgOmpSimdStatement *target) {
    if (output_scope_ != nullptr || target == nullptr ||
        target->get_body() == nullptr ||
        target->get_body()->get_parent() != target) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: output region "
              "cannot be initialized exactly once\n");
      ROSE_ABORT();
    }
    SgStatement *body = target->get_body();
    target->set_body(nullptr);
    body->set_parent(nullptr);
    output_scope_ = SageBuilder::buildBasicBlock(body);
    if (output_scope_ == nullptr || body->get_parent() != output_scope_) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: could not wrap "
              "the associated region in one exact output scope\n");
      ROSE_ABORT();
    }
    target->set_body(output_scope_);
    output_scope_->set_parent(target);
    if (target->get_body() != output_scope_ ||
        output_scope_->get_parent() != target) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: output scope "
              "publication failed\n");
      ROSE_ABORT();
    }
    region_anchor_ = body;
    epilogue_tail_ = body;
  }

  SgBasicBlock *outputScope() const {
    if (output_scope_ == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: output scope "
              "was requested before publication\n");
      ROSE_ABORT();
    }
    return output_scope_;
  }

  void emitBeforeRegion(SgStatement *statement) {
    if (statement == nullptr || statement->get_parent() != nullptr ||
        region_anchor_ == nullptr ||
        region_anchor_->get_parent() != outputScope()) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: prelude "
                      "statement is not one exact detached output node\n");
      ROSE_ABORT();
    }
    SageInterface::insertStatementBefore(region_anchor_, statement);
    if (statement->get_parent() != outputScope()) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: prelude "
                      "statement publication failed\n");
      ROSE_ABORT();
    }
  }

  void emitAfterRegion(SgStatement *statement) {
    if (statement == nullptr || statement->get_parent() != nullptr ||
        epilogue_tail_ == nullptr ||
        epilogue_tail_->get_parent() != outputScope()) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: epilogue "
                      "statement is not one exact detached output node\n");
      ROSE_ABORT();
    }
    SageInterface::insertStatementAfter(epilogue_tail_, statement);
    if (statement->get_parent() != outputScope()) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: epilogue "
                      "statement publication failed\n");
      ROSE_ABORT();
    }
    epilogue_tail_ = statement;
  }

private:
  struct GeneratedSymbolRecord {
    IntelGeneratedSymbolRole role;
    SgVariableSymbol *output_symbol;
  };

  static std::string nextName(const char *prefix, unsigned int &next_id) {
    if (next_id == std::numeric_limits<unsigned int>::max()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-name]: %s counter "
              "overflowed\n",
              prefix);
      ROSE_ABORT();
    }
    std::string name = std::string(prefix) + std::to_string(next_id);
    ++next_id;
    return name;
  }

  const unsigned int vector_width_;
  const unsigned int loop_increment_;
  unsigned int next_buffer_id_ = 0;
  unsigned int next_mask_id_ = 0;
  unsigned int next_index_id_ = 0;
  std::unordered_map<SgVariableSymbol *, GeneratedSymbolRecord>
      generated_symbols_;
  SgBasicBlock *output_scope_ = nullptr;
  SgStatement *region_anchor_ = nullptr;
  SgStatement *epilogue_tail_ = nullptr;
};

SgExpression *
copyIntelIrOperand(const IntelSimdEmissionTransaction &transaction,
                   SgExpression *operand, const char *contract) {
  if (operand == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: cannot transfer a null SIMD IR "
            "operand\n",
            contract);
    ROSE_ABORT();
  }
  SgExpression *copy = SageInterface::copyExpression(operand);
  if (copy == nullptr || copy == operand || copy->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: operand=%p did not produce one "
            "exact detached output copy\n",
            contract, static_cast<void *>(operand));
    ROSE_ABORT();
  }
  for (SgVarRefExp *reference :
       SageInterface::querySubTree<SgVarRefExp>(copy, V_SgVarRefExp)) {
    transaction.retargetCopiedReference(reference, contract);
  }
  return copy;
}

SgVariableSymbol *
requireExactDeclaredVariableSymbol(SgVariableDeclaration *declaration,
                                   const char *contract) {
  if (declaration == nullptr || declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: output declaration=%p does not "
            "own one exact initialized name\n",
            contract, static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  SgInitializedName *name = declaration->get_variables().front();
  SgVariableSymbol *symbol =
      isSgVariableSymbol(name->get_symbol_from_symbol_table());
  if (symbol == nullptr || symbol->get_declaration() != name) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: output declaration=%p name=%s "
            "does not own one exact symbol identity\n",
            contract, static_cast<void *>(declaration),
            name->get_name().getString().c_str());
    ROSE_ABORT();
  }
  return symbol;
}

void planIntelGeneratedSymbols(IntelSimdEmissionTransaction &transaction,
                               const Rose_STL_Container<SgNode *> &ir_block) {
  for (SgNode *node : ir_block) {
    SgBinaryOp *operation = isSgBinaryOp(node);
    if (operation == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: IR node=%p is not "
              "one exact binary SIMD operation\n",
              static_cast<void *>(node));
      ROSE_ABORT();
    }
    if (node->variantT() != V_SgSIMDPartialStore)
      continue;
    SgVarRefExp *destination = isSgVarRefExp(operation->get_lhs_operand());
    if (destination == nullptr || destination->get_symbol() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: partial "
              "operation=%p has no exact generated destination identity\n",
              static_cast<void *>(operation));
      ROSE_ABORT();
    }
    transaction.planGeneratedSymbol(destination->get_symbol(),
                                    IntelGeneratedSymbolRole::PartialReduction);
  }

  for (SgNode *node : ir_block) {
    SgBinaryOp *operation = isSgBinaryOp(node);
    SgVarRefExp *destination = nullptr;
    IntelGeneratedSymbolRole role = IntelGeneratedSymbolRole::Value;
    bool produces_generated_value = false;
    switch (node->variantT()) {
    case V_SgSIMDLoad:
    case V_SgSIMDBroadcast:
    case V_SgSIMDGather:
    case V_SgSIMDExplicitGather:
      produces_generated_value = true;
      break;
    case V_SgSIMDPartialStore:
      continue;
    case V_SgSIMDAddOp:
    case V_SgSIMDSubOp:
    case V_SgSIMDMulOp:
    case V_SgSIMDDivOp:
      produces_generated_value = true;
      destination = isSgVarRefExp(operation->get_lhs_operand());
      if (destination != nullptr &&
          transaction.isPlannedPartial(destination->get_symbol()))
        role = IntelGeneratedSymbolRole::PartialReduction;
      break;
    case V_SgSIMDStore:
    case V_SgSIMDScatter:
    case V_SgSIMDScalarStore:
      continue;
    default:
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: unsupported IR "
              "operation kind=%s\n",
              node->sage_class_name());
      ROSE_ABORT();
    }
    if (!produces_generated_value)
      continue;
    if (destination == nullptr)
      destination = isSgVarRefExp(operation->get_lhs_operand());
    if (destination == nullptr || destination->get_symbol() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-identity]: generated "
              "operation=%p kind=%s has no exact destination identity\n",
              static_cast<void *>(operation), operation->sage_class_name());
      ROSE_ABORT();
    }
    transaction.planGeneratedSymbol(destination->get_symbol(), role);
  }
}

bool isStructurallyOwnedBy(const SgNode *node, const SgNode *owner) {
  std::vector<const SgNode *> visited;
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (std::find(visited.begin(), visited.end(), current) != visited.end()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: node=%p "
              "has a cyclic parent chain\n",
              static_cast<const void *>(node));
      ROSE_ABORT();
    }
    visited.push_back(current);
    if (current == owner)
      return true;
  }
  return false;
}

SgInitializedName *
requireExactTilingIncrement(SgVariableDeclaration *declaration,
                            SgOmpSimdStatement *region) {
  if (declaration == nullptr || declaration->get_variables().size() != 1 ||
      declaration->get_variables().front() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: declaration=%p "
            "does not own one exact initialized name\n",
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  SgInitializedName *name = declaration->get_variables().front();
  if (name->get_generated_variable_role() !=
          SgInitializedName::e_generated_loop_tiling_increment ||
      name->get_parent() != declaration ||
      name->get_declaration() != declaration ||
      !isStructurallyOwnedBy(declaration, region)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: declaration=%p "
            "has no exact generated increment role or region ownership\n",
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  return name;
}

void replaceExactTilingIncrementInitializer(SgInitializedName *name,
                                            int increment) {
  SgInitializer *old_initializer = name->get_initializer();
  if (old_initializer == nullptr || old_initializer->get_parent() != name) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: name=%p "
            "has no exact owned initializer to replace\n",
            static_cast<void *>(name));
    ROSE_ABORT();
  }
  const std::vector<SgNode *> successors =
      name->get_traversalSuccessorContainer();
  if (std::count(successors.begin(), successors.end(), old_initializer) != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: name=%p "
            "does not publish its initializer on one exact structural edge\n",
            static_cast<void *>(name));
    ROSE_ABORT();
  }

  SgAssignInitializer *replacement =
      buildAssignInitializer(buildIntVal(increment), name->get_type());
  if (replacement == nullptr || replacement->get_parent() != nullptr ||
      replacement->get_operand() == nullptr ||
      replacement->get_operand()->get_parent() != replacement) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: replacement "
            "initializer has no exact detached ownership\n");
    ROSE_ABORT();
  }

  name->set_initializer(nullptr);
  old_initializer->set_parent(nullptr);
  SageInterface::deleteAST(old_initializer);

  name->set_initializer(replacement);
  replacement->set_parent(name);
  if (name->get_initializer() != replacement ||
      replacement->get_parent() != name) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: name=%p "
            "failed exact replacement initializer publication\n",
            static_cast<void *>(name));
    ROSE_ABORT();
  }
}

void bindExactTilingIncrements(SgOmpSimdStatement *target, int increment) {
  if (target == nullptr || target->get_body() == nullptr ||
      target->get_body()->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: SIMD target has "
            "no exact associated transformed region\n");
    ROSE_ABORT();
  }

  std::vector<SgVariableDeclaration *> declarations =
      SageInterface::querySubTree<SgVariableDeclaration>(
          target, V_SgVariableDeclaration);
  std::vector<SgInitializedName *> increments;
  for (SgVariableDeclaration *declaration : declarations) {
    if (declaration == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: transformed "
              "region contains a null variable declaration\n");
      ROSE_ABORT();
    }
    size_t increment_roles = 0;
    for (SgInitializedName *name : declaration->get_variables()) {
      if (name == nullptr) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: declaration=%p "
                "contains a null initialized name\n",
                static_cast<void *>(declaration));
        ROSE_ABORT();
      }
      if (name->get_generated_variable_role() ==
          SgInitializedName::e_generated_loop_tiling_increment) {
        ++increment_roles;
      }
    }
    if (increment_roles == 0)
      continue;
    if (increment_roles != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: "
              "declaration=%p has %zu generated increment roles\n",
              static_cast<void *>(declaration), increment_roles);
      ROSE_ABORT();
    }
    SgInitializedName *name = requireExactTilingIncrement(declaration, target);
    if (std::find(increments.begin(), increments.end(), name) !=
        increments.end()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: name=%p "
              "is published more than once in the transformed region\n",
              static_cast<void *>(name));
      ROSE_ABORT();
    }
    increments.push_back(name);
  }

  std::vector<SgVarRefExp *> region_references =
      SageInterface::querySubTree<SgVarRefExp>(target, V_SgVarRefExp);
  SgNode *reference_root = getEnclosingFunctionDefinition(target);
  if (reference_root == nullptr)
    reference_root = target->get_scope();
  if (reference_root == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: SIMD target has "
            "no exact enclosing reference domain\n");
    ROSE_ABORT();
  }
  std::vector<SgVarRefExp *> all_references =
      SageInterface::querySubTree<SgVarRefExp>(reference_root, V_SgVarRefExp);

  for (SgVarRefExp *reference : region_references) {
    if (reference == nullptr || reference->get_symbol() == nullptr ||
        reference->get_symbol()->get_declaration() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: transformed "
              "region contains a variable reference without exact identity\n");
      ROSE_ABORT();
    }
    SgInitializedName *declaration = reference->get_symbol()->get_declaration();
    if (declaration->get_generated_variable_role() ==
            SgInitializedName::e_generated_loop_tiling_increment &&
        std::find(increments.begin(), increments.end(), declaration) ==
            increments.end()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: region "
              "references an increment declared outside its exact ownership "
              "boundary\n");
      ROSE_ABORT();
    }
  }

  for (SgInitializedName *name : increments) {
    const auto refers_to_name = [name](SgVarRefExp *reference) {
      return reference != nullptr && reference->get_symbol() != nullptr &&
             reference->get_symbol()->get_declaration() == name;
    };
    const size_t region_count = std::count_if(
        region_references.begin(), region_references.end(), refers_to_name);
    const size_t total_count = std::count_if(
        all_references.begin(), all_references.end(), refers_to_name);
    if (region_count == 0 || region_count != total_count) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-increment]: name=%p "
              "has region references=%zu and total references=%zu\n",
              static_cast<void *>(name), region_count, total_count);
      ROSE_ABORT();
    }
    replaceExactTilingIncrementInitializer(name, increment);
  }
}

IntelLaneWidthKind
requireExactIntelLaneWidth(const Rose_STL_Container<SgNode *> &ir_block,
                           unsigned int vector_width) {
  std::optional<IntelLaneWidthKind> lane_kind;
  for (SgNode *node : ir_block) {
    SgBinaryOp *operation = isSgBinaryOp(node);
    if (operation == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: node=%p is not one "
              "exact binary SIMD operation\n",
              static_cast<void *>(node));
      ROSE_ABORT();
    }
    switch (node->variantT()) {
    case V_SgSIMDLoad:
    case V_SgSIMDBroadcast:
    case V_SgSIMDGather:
    case V_SgSIMDExplicitGather:
    case V_SgSIMDStore:
    case V_SgSIMDScatter:
    case V_SgSIMDPartialStore:
    case V_SgSIMDScalarStore:
    case V_SgSIMDAddOp:
    case V_SgSIMDSubOp:
    case V_SgSIMDMulOp:
    case V_SgSIMDDivOp:
      break;
    default:
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: operation kind=%s "
              "is not supported by the exact Intel backend\n",
              node->sage_class_name());
      ROSE_ABORT();
    }

    SgExpression *lhs = operation->get_lhs_operand();
    SgExpression *rhs = operation->get_rhs_operand();
    if (lhs == nullptr || rhs == nullptr || lhs->get_parent() != operation ||
        rhs->get_parent() != operation) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: operation=%p does "
              "not own two exact operands\n",
              static_cast<void *>(operation));
      ROSE_ABORT();
    }

    SgType *type = lhs->get_type();
    if (type == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-width]: operation=%p has "
              "no exact scalar lane type\n",
              static_cast<void *>(operation));
      ROSE_ABORT();
    }
    type = type->stripType(SgType::STRIP_MODIFIER_TYPE |
                           SgType::STRIP_TYPEDEF_TYPE);
    IntelLaneWidthKind current_kind;
    if (isSgTypeDouble(type) != nullptr) {
      current_kind = IntelLaneWidthKind::Double;
    } else if (isSgTypeInt(type) != nullptr || isSgTypeFloat(type) != nullptr) {
      current_kind = IntelLaneWidthKind::I32OrFloat;
    } else {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-width]: operation=%p "
              "uses unsupported scalar lane type=%s\n",
              static_cast<void *>(operation), type->sage_class_name());
      ROSE_ABORT();
    }
    if (lane_kind.has_value() && *lane_kind != current_kind) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-width]: region mixes "
              "32-bit and 64-bit lane widths before emission\n");
      ROSE_ABORT();
    }
    lane_kind = current_kind;

    if (node->variantT() == V_SgSIMDScalarStore && vector_width == 4) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[intel-simd-width]: width=4 "
                      "reduction has no exact horizontal-store plan\n");
      ROSE_ABORT();
    }
  }
  if (!lane_kind.has_value()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-width]: empty region has "
            "no exact lane width\n");
    ROSE_ABORT();
  }
  return *lane_kind;
}

} // namespace

// If half_type is true, select the exact next-smaller vector width used by
// scalar stores.
SgType *intel_simd_type(const IntelSimdEmissionTransaction &transaction,
                        SgType *type, SgScopeStatement *new_block,
                        bool half_type = false) {
  unsigned int len = transaction.vectorWidth();
  if (half_type)
    len /= 2;

  if (len != 2 && len != 4 && len != 8 && len != 16) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-length]: internal "
            "width=%u has no exact Intel vector type\n",
            len);
    ROSE_ABORT();
  }

  // Model the intrinsic ABI as its exact GNU vector type instead of inventing
  // an opaque named type and hoping a later include makes that malformed
  // semantic node meaningful.  Intel's integer vector typedefs use signed
  // 64-bit elements; float and double vectors retain their scalar element
  // types.  The public intrinsic header is force-included by the frontend
  // transaction so calls have the matching external declarations.
  const unsigned int vector_bytes = len == 16 ? 64 : len == 8 ? 32 : 16;
  SgType *element_type = nullptr;
  unsigned int element_count = 0;
  switch (type->variantT()) {
  case V_SgTypeInt:
    element_type = buildLongLongType();
    element_count = vector_bytes / 8;
    break;
  case V_SgTypeFloat:
    element_type = buildFloatType();
    element_count = vector_bytes / 4;
    break;
  case V_SgTypeDouble:
    element_type = buildDoubleType();
    element_count = vector_bytes / 8;
    break;
  default:
    return type;
  }
  if (element_type == nullptr || element_count == 0)
    failIntelSimdEmission("intel-simd-type", type,
                          "has no exact intrinsic ABI vector representation");
  SgTypeModifier modifier;
  modifier.setVectorType();
  modifier.set_vector_size(element_count);
  SgModifierType *vector_type = buildModifierType(element_type, modifier);
  if (vector_type == nullptr ||
      !vector_type->get_typeModifier().isVectorType() ||
      vector_type->get_typeModifier().get_vector_size() != element_count)
    failIntelSimdEmission("intel-simd-type", vector_type,
                          "did not intern the exact intrinsic ABI vector type");
  return vector_type;
}

std::string intel_simd_func(const IntelSimdEmissionTransaction &transaction,
                            OpType op_type, SgType *type,
                            bool half_type = false) {
  unsigned int len = transaction.vectorWidth();
  if (half_type)
    len /= 2;

  if (len != 2 && len != 4 && len != 8 && len != 16) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-length]: internal "
            "width=%u has no exact Intel intrinsic family\n",
            len);
    ROSE_ABORT();
  }

  std::string instr = len == 16 ? "_mm512_" : len == 8 ? "_mm256_" : "_mm_";

  switch (op_type) {
  case Load:
    instr += "loadu_";
    break;
  case Broadcast:
    instr += "set1_";
    break;
  case BroadcastZero:
    instr += "setzero_";
    break;
  case Gather:
    instr += "mask_i32gather_";
    break;
  case ExplicitGather:
    instr += "i32gather_";
    break;

  case ScalarStore:
  case Store:
    instr += "storeu_";
    break;
  case Scatter:
    instr += "i32scatter_";
    break;

  case HAdd:
    instr += "hadd_";
    break;
  case Add:
    instr += "add_";
    break;
  case Sub:
    instr += "sub_";
    break;
  case Div:
    instr += "div_";
    break;

  case Mul: {
    if (type->variantT() == V_SgTypeInt)
      instr += "mullo_";
    else
      instr += "mul_";
  } break;

  case Extract: {
    switch (type->variantT()) {
    case V_SgTypeInt:
      instr += "extracti32x8_";
      break;
    case V_SgTypeFloat:
      instr += "extractf32x8_";
      break;
    case V_SgTypeDouble:
      instr += "extractf64x4_";
      break;
    default: {
    }
    }
  } break;

  default: {
  }
  }

  switch (type->variantT()) {
  case V_SgTypeInt: {
    if (op_type == BroadcastZero) {
      instr += len == 16 ? "si512" : len == 8 ? "si256" : "si128";
    } else if (op_type == Load || op_type == Store || op_type == ScalarStore) {
      if (len == 16)
        instr += "si512";
      else if (len == 8)
        instr += "si256";
      else
        instr += "si128";
    } else {
      instr += "epi32";
    }
  } break;

  case V_SgTypeFloat:
    instr += "ps";
    break;
  case V_SgTypeDouble:
    instr += "pd";
    break;

  default: {
  }
  }

  return instr;
}

//
// This is specific to the loop unrolling.
// If we find this specific sequence, we very likely have an index altered by
// the loopUnrolling from an OMP unroll clause. In that case, we need to adjust
// the base with the proper loop increment value.
//
void intel_normalize_offset(const IntelSimdEmissionTransaction &transaction,
                            SgPntrArrRefExp *array) {
  SgAddOp *add = isSgAddOp(array->get_rhs_operand());
  if (!add)
    return;

  SgMultiplyOp *mul = isSgMultiplyOp(add->get_rhs_operand());
  if (!mul) {
    SgAddOp *add2 = isSgAddOp(add->get_rhs_operand());
    if (!add2)
      return;

    mul = isSgMultiplyOp(add2->get_rhs_operand());
    if (!mul)
      return;
  }

  SgIntVal *inc = isSgIntVal(mul->get_lhs_operand());
  if (!inc)
    return;

  inc->set_value(transaction.loopIncrement());
}

// Generates a SIMD load statement for Intel
SgAssignInitializer *
intel_write_load(const IntelSimdEmissionTransaction &transaction,
                 SgBinaryOp *op, SgOmpSimdStatement *target,
                 SgBasicBlock *new_block) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgVarRefExp *va = static_cast<SgVarRefExp *>(lval);
  SgType *vector_type =
      intel_simd_type(transaction, va->get_type(), target->get_scope());
  SgPntrArrRefExp *array = static_cast<SgPntrArrRefExp *>(rval);

  intel_normalize_offset(transaction, array);

  // Build function call parameters
  SgExprListExp *parameters;

  if (va->get_type()->variantT() == V_SgTypeInt) {
    SgExpression *addressed_array = copyExpression(array);
    SgAddressOfOp *addr = buildAddressOfOp(
        addressed_array, buildPointerType(addressed_array->get_type()));
    SgPointerType *ptr_type = buildPointerType(vector_type);
    SgCastExp *cast = buildCastExp(addr, ptr_type);
    parameters = buildExprListExp(cast);
  } else {
    SgExpression *addressed_array = copyExpression(array);
    SgAddressOfOp *addr = buildAddressOfOp(
        addressed_array, buildPointerType(addressed_array->get_type()));
    parameters = buildExprListExp(addr);
  }

  // Build the function call
  std::string func_name = intel_simd_func(transaction, Load, va->get_type());

  SgExpression *ld = buildFunctionCallExp(func_name, vector_type, parameters,
                                          target->get_scope());
  return buildAssignInitializer(ld, vector_type);
}

// ==============================================================================================================
// Generates a SIMD broadcast statement on Intel architecture
//
SgAssignInitializer *
intel_write_broadcast(const IntelSimdEmissionTransaction &transaction,
                      SgBinaryOp *op, SgOmpSimdStatement *target,
                      SgBasicBlock *new_block) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgVarRefExp *v_dest = isSgVarRefExp(lval);
  if (v_dest == nullptr || rval == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-broadcast]: operation "
            "does not own an exact destination and scalar operand\n");
    ROSE_ABORT();
  }
  SgType *vector_type =
      intel_simd_type(transaction, v_dest->get_type(), target->get_scope());

  // Function call parameters
  SgExprListExp *parameters = buildExprListExp(
      copyIntelIrOperand(transaction, rval, "intel-simd-broadcast-ownership"));

  // Build the function call and place it above the for loop
  std::string func_name =
      intel_simd_func(transaction, Broadcast, v_dest->get_type());

  SgExpression *ld = buildFunctionCallExp(func_name, vector_type, parameters,
                                          target->get_scope());
  return buildAssignInitializer(ld, vector_type);
}

// ===========================================================================================================
// Generates a SIMD explicit gather statement
//
SgAssignInitializer *
intel_write_exp_gather(IntelSimdEmissionTransaction &transaction,
                       SgBinaryOp *op, SgOmpSimdStatement *target,
                       SgBasicBlock *new_block, SgForStatement *for_loop) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  // Get the loop condition of our outer for-loop to determine the stride
  SgStatement *for_cond = for_loop->get_test();
  SgExprStatement *for_cond2 = isSgExprStatement(for_cond);
  SgBinaryOp *cond = isSgBinaryOp(for_cond2->get_expression());
  SgExpression *stride = copyExpression(cond->get_rhs_operand());
  if (cond->variantT() == V_SgGreaterOrEqualOp ||
      cond->variantT() == V_SgLessOrEqualOp) {
    SgAddOp *add = buildAddOp(stride, buildIntVal(1), stride->get_type());
    stride = add;
  }

  // First, break down the pointer expression
  // TODO: This only works with 2D at the moment
  SgPntrArrRefExp *pntr1 = isSgPntrArrRefExp(rval);
  SgPntrArrRefExp *pntr2 = isSgPntrArrRefExp(pntr1->get_lhs_operand());
  SgVarRefExp *i_var = isSgVarRefExp(pntr2->get_rhs_operand());
  SgVarRefExp *j_var = isSgVarRefExp(pntr1->get_rhs_operand());
  SgType *vector_type =
      intel_simd_type(transaction, lval->get_type(), target->get_scope());

  // Second, create the index array
  std::string name = transaction.nextBufferName();
  SgIntVal *length = buildIntVal(transaction.vectorWidth());
  SgType *type = buildArrayType(buildIntType(), length);

  SgVariableDeclaration *vd =
      buildVariableDeclaration(name, type, NULL, new_block);
  appendStatement(vd, new_block);

  // Now, generate the for loop
  SgBasicBlock *block2 = buildBasicBlock();
  SgType *loop_index_type = buildIntType();
  SgVariableDeclaration *loop_inc_vd = buildVariableDeclaration(
      "__i", loop_index_type,
      buildAssignInitializer(buildIntVal(0), loop_index_type), new_block);
  SgLessThanOp *loop_cmp =
      buildLessThanOp(buildVarRefExp("__i"),
                      buildIntVal(transaction.vectorWidth()), cond->get_type());
  SgPlusAssignOp *loop_inc =
      buildPlusAssignOp(buildVarRefExp("__i"), buildIntVal(1), loop_index_type);
  SgForStatement *loop2 = buildForStatement(
      loop_inc_vd, buildExprStatement(loop_cmp), loop_inc, block2);
  appendStatement(loop2, new_block);

  // Generate the index generator
  // indexes[i] = (k + __i) * N + j;  // N = loop condition
  SgAddOp *add1 =
      buildAddOp(copyExpression(i_var), buildVarRefExp("__i", new_block),
                 i_var->get_type());
  SgPntrArrRefExp *index_pntr =
      buildPntrArrRefExp(buildVarRefExp(name, new_block),
                         buildVarRefExp("__i", new_block), loop_index_type);
  SgMultiplyOp *mul = buildMultiplyOp(add1, stride, loop_index_type);
  SgAddOp *add = buildAddOp(mul, copyExpression(j_var), loop_index_type);
  SgAssignOp *assign = buildAssignOp(index_pntr, add, loop_index_type);
  appendStatement(buildExprStatement(assign), block2);

  // Generate the load statement for the array indicies
  std::string vindex_name = transaction.nextIndexName();
  SgType *mask_type = intel_simd_type(transaction, buildIntType(), new_block);

  index_pntr = buildPntrArrRefExp(buildVarRefExp(name, new_block),
                                  buildIntVal(0), loop_index_type);
  SgAddressOfOp *addr =
      buildAddressOfOp(index_pntr, buildPointerType(loop_index_type));
  SgPointerType *ptr_type = buildPointerType(mask_type);
  SgCastExp *cast = buildCastExp(addr, ptr_type);

  std::string func_name =
      intel_simd_func(transaction, Load, index_pntr->get_type());
  SgExprListExp *parameters = buildExprListExp(cast);
  SgExpression *ld = buildFunctionCallExp(func_name, vector_type, parameters,
                                          target->get_scope());
  SgAssignInitializer *local_init = buildAssignInitializer(ld, mask_type);

  SgVariableDeclaration *mask_vd =
      buildVariableDeclaration(vindex_name, mask_type, local_init, new_block);
  appendStatement(mask_vd, new_block);

  // Now, generate the actual gather statement
  if (transaction.vectorWidth() == 8) {
    parameters = buildExprListExp(
        copyIntelIrOperand(transaction, pntr2->get_lhs_operand(),
                           "intel-simd-explicit-gather-ownership"),
        buildVarRefExp(vindex_name, new_block), buildIntVal(4));
  } else {
    parameters = buildExprListExp(
        buildVarRefExp(vindex_name, new_block),
        copyIntelIrOperand(transaction, pntr2->get_lhs_operand(),
                           "intel-simd-explicit-gather-ownership"),
        buildIntVal(4));
  }

  func_name = intel_simd_func(transaction, ExplicitGather, lval->get_type());
  ld = buildFunctionCallExp(func_name, vector_type, parameters,
                            target->get_scope());
  return buildAssignInitializer(ld, vector_type);

  // return nullptr;
}

// ===========================================================================================================
// Generates a SIMD gather statement
//
SgAssignInitializer *
intel_write_gather(IntelSimdEmissionTransaction &transaction, SgBinaryOp *op,
                   SgOmpSimdStatement *target, SgBasicBlock *new_block) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgVarRefExp *dest = static_cast<SgVarRefExp *>(lval);
  SgPntrArrRefExp *element = static_cast<SgPntrArrRefExp *>(rval);
  SgPntrArrRefExp *mask_pntr =
      static_cast<SgPntrArrRefExp *>(element->get_rhs_operand());

  // Load the array indexes first
  std::string vindex_name = transaction.nextIndexName();
  SgType *mask_type =
      intel_simd_type(transaction, mask_pntr->get_type(), target->get_scope());
  SgType *vector_type =
      intel_simd_type(transaction, dest->get_type(), target->get_scope());

  SgExpression *addressed_mask = copyExpression(mask_pntr);
  SgAddressOfOp *addr = buildAddressOfOp(
      addressed_mask, buildPointerType(addressed_mask->get_type()));
  SgPointerType *ptr_type = buildPointerType(mask_type);
  SgCastExp *cast = buildCastExp(addr, ptr_type);

  std::string func_name =
      intel_simd_func(transaction, Load, mask_pntr->get_type());
  SgExprListExp *parameters = buildExprListExp(cast);
  SgExpression *ld = buildFunctionCallExp(func_name, vector_type, parameters,
                                          target->get_scope());
  SgAssignInitializer *local_init = buildAssignInitializer(ld, mask_type);

  SgVariableDeclaration *mask_vd =
      buildVariableDeclaration(vindex_name, mask_type, local_init, new_block);
  appendStatement(mask_vd, new_block);

  // Generate the two mask statements
  std::string mask1 = transaction.nextMaskName();
  std::string mask2 = transaction.nextMaskName();
  std::string kmask = transaction.nextMaskName();

  SgType *kmask_type = transaction.vectorWidth() == 16
                           ? static_cast<SgType *>(buildUnsignedShortType())
                           : static_cast<SgType *>(buildUnsignedCharType());

  SgVariableDeclaration *mask1_vd = buildVariableDeclaration(
      mask1, kmask_type, NULL, transaction.outputScope());
  SgVariableDeclaration *mask2_vd = buildVariableDeclaration(
      mask2, kmask_type, NULL, transaction.outputScope());

  transaction.emitBeforeRegion(mask1_vd);
  transaction.emitBeforeRegion(mask2_vd);

  func_name = "_kxnor_mask16";
  if (transaction.vectorWidth() == 8)
    func_name = "_kxnor_mask8";

  parameters = buildExprListExp(buildVarRefExp(mask1, new_block),
                                buildVarRefExp(mask2, new_block));
  ld = buildFunctionCallExp(func_name, kmask_type, parameters,
                            target->get_scope());
  local_init = buildAssignInitializer(ld, kmask_type);

  SgVariableDeclaration *kmask_vd = buildVariableDeclaration(
      kmask, kmask_type, local_init, transaction.outputScope());
  transaction.emitBeforeRegion(kmask_vd);

  // Create the empty register
  std::string zero_name = transaction.nextBufferName();

  func_name = intel_simd_func(transaction, BroadcastZero, dest->get_type());
  ld = buildFunctionCallExp(func_name, vector_type, NULL, target->get_scope());
  local_init = buildAssignInitializer(ld, vector_type);

  SgVariableDeclaration *zero_vd = buildVariableDeclaration(
      zero_name, vector_type, local_init, transaction.outputScope());
  transaction.emitBeforeRegion(zero_vd);

  // Now for the gather statement
  SgVarRefExp *mask_ref = buildVarRefExp(vindex_name, new_block);
  SgExpression *base_ref = copyIntelIrOperand(
      transaction, element->get_lhs_operand(), "intel-simd-gather-ownership");
  SgVarRefExp *zero_ref = buildVarRefExp(zero_name, new_block);
  SgVarRefExp *kmask_ref = buildVarRefExp(kmask, new_block);

  int scale = 4;
  if (dest->get_type()->variantT() == V_SgTypeDouble) {
    scale = 8;

    // If we have a double, we also need to do an extraction of the mask
    SgType *extract_type;
    std::string extract_name = "";
    std::string vindex_name2 = vindex_name + "2";

    if (transaction.vectorWidth() == 16) {
      extract_type =
          intel_simd_type(transaction, buildIntType(), new_block, true);
      extract_name = "_mm512_extracti32x8_epi32";
    } else {
      extract_type =
          intel_simd_type(transaction, buildIntType(), new_block, true);
      extract_name = "_mm256_extractf128_si256";
    }

    parameters = buildExprListExp(mask_ref, buildIntVal(0));
    ld = buildFunctionCallExp(extract_name, extract_type, parameters,
                              target->get_scope());
    local_init = buildAssignInitializer(ld, extract_type);
    mask_vd = buildVariableDeclaration(vindex_name2, extract_type, local_init,
                                       new_block);
    appendStatement(mask_vd, new_block);

    mask_ref = buildVarRefExp(vindex_name2, new_block);
  }

  parameters = buildExprListExp(zero_ref, kmask_ref, mask_ref, base_ref,
                                buildIntVal(scale));

  func_name = intel_simd_func(transaction, Gather, dest->get_type());
  ld = buildFunctionCallExp(func_name, vector_type, parameters,
                            target->get_scope());
  return buildAssignInitializer(ld, vector_type);
}

// ==========================================================================================
// Generates a SIMD store statement
//
void intel_write_store(const IntelSimdEmissionTransaction &transaction,
                       SgBinaryOp *op, SgOmpSimdStatement *target,
                       SgBasicBlock *new_block) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgPntrArrRefExp *array = isSgPntrArrRefExp(lval);
  if (array)
    intel_normalize_offset(transaction, array);

  SgVarRefExp *v_src = static_cast<SgVarRefExp *>(rval);

  // Function call parameters
  SgExpression *addressed_lvalue = copyExpression(lval);
  SgAddressOfOp *addr = buildAddressOfOp(
      addressed_lvalue, buildPointerType(addressed_lvalue->get_type()));
  SgExprListExp *parameters =
      buildExprListExp(addr, copyIntelIrOperand(transaction, v_src,
                                                "intel-simd-store-ownership"));

  // Build the function call
  std::string func_name =
      intel_simd_func(transaction, Store, v_src->get_type());

  SgExprStatement *fc = buildFunctionCallStmt(func_name, buildVoidType(),
                                              parameters, target->get_scope());
  appendStatement(fc, new_block);
}

// ============================================================================================
/// Generates a SIMD scatter statement
//
void intel_write_scatter(IntelSimdEmissionTransaction &transaction,
                         SgBinaryOp *op, SgOmpSimdStatement *target,
                         SgBasicBlock *new_block) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgVarRefExp *dest = static_cast<SgVarRefExp *>(rval);
  SgPntrArrRefExp *element = static_cast<SgPntrArrRefExp *>(lval);
  SgPntrArrRefExp *mask_pntr =
      static_cast<SgPntrArrRefExp *>(element->get_rhs_operand());

  // Load the mask first
  std::string mask_name = transaction.nextMaskName();
  SgType *mask_type =
      intel_simd_type(transaction, mask_pntr->get_type(), target->get_scope());
  SgType *vector_type =
      intel_simd_type(transaction, dest->get_type(), target->get_scope());

  SgExpression *addressed_mask = copyExpression(mask_pntr);
  SgAddressOfOp *addr = buildAddressOfOp(
      addressed_mask, buildPointerType(addressed_mask->get_type()));
  SgPointerType *ptr_type = buildPointerType(mask_type);
  SgCastExp *cast = buildCastExp(addr, ptr_type);

  std::string func_name =
      intel_simd_func(transaction, Load, mask_pntr->get_type());
  SgExprListExp *parameters = buildExprListExp(cast);
  SgExpression *ld = buildFunctionCallExp(func_name, vector_type, parameters,
                                          target->get_scope());
  SgAssignInitializer *local_init = buildAssignInitializer(ld, mask_type);

  SgVariableDeclaration *mask_vd =
      buildVariableDeclaration(mask_name, mask_type, local_init, new_block);
  appendStatement(mask_vd, new_block);

  // Now for the scatter statement
  SgVarRefExp *mask_ref = buildVarRefExp(mask_name, new_block);
  SgExpression *base_ref = copyIntelIrOperand(
      transaction, element->get_lhs_operand(), "intel-simd-scatter-ownership");

  int scale = 4;
  if (dest->get_type()->variantT() == V_SgTypeDouble) {
    scale = 8;

    // If we have a double, we also need to do an extraction of the mask
    SgType *extract_type;
    std::string extract_name = "";
    std::string mask_name2 = mask_name + "2";

    if (transaction.vectorWidth() == 16) {
      extract_type =
          intel_simd_type(transaction, buildIntType(), new_block, true);
      extract_name = "_mm512_extracti32x8_epi32";
    } else {
      extract_type =
          intel_simd_type(transaction, buildIntType(), new_block, true);
      extract_name = "_mm256_extractf128_si256";
    }

    parameters = buildExprListExp(mask_ref, buildIntVal(0));
    ld = buildFunctionCallExp(extract_name, extract_type, parameters,
                              target->get_scope());
    local_init = buildAssignInitializer(ld, extract_type);
    mask_vd = buildVariableDeclaration(mask_name2, extract_type, local_init,
                                       new_block);
    appendStatement(mask_vd, new_block);

    mask_ref = buildVarRefExp(mask_name2, new_block);
  }

  func_name = intel_simd_func(transaction, Scatter, dest->get_type());
  parameters = buildExprListExp(
      base_ref, mask_ref,
      copyIntelIrOperand(transaction, dest, "intel-simd-scatter-ownership"),
      buildIntVal(scale));
  SgExprStatement *fc = buildFunctionCallStmt(func_name, buildVoidType(),
                                              parameters, target->get_scope());
  appendStatement(fc, new_block);
}

// ==================================================================================================
// Generates an Intel partial-store statement
//
void ensureIntelPartialBroadcast(IntelSimdEmissionTransaction &transaction,
                                 SgVarRefExp *var, SgOmpSimdStatement *target) {
  if (var == nullptr || var->get_symbol() == nullptr) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[intel-simd-reduction]: partial "
                    "broadcast has no exact variable identity\n");
    ROSE_ABORT();
  }
  SgVariableSymbol *original_symbol = var->get_symbol();
  if (!transaction.needsPartialOutput(original_symbol))
    return;
  const std::string name =
      transaction.uniqueOutputName(original_symbol, transaction.outputScope());

  SgType *vector_type =
      intel_simd_type(transaction, var->get_type(), target->get_scope());
  std::string func_name =
      intel_simd_func(transaction, BroadcastZero, var->get_type());
  SgExpression *zero = buildFunctionCallExp(func_name, vector_type, nullptr,
                                            target->get_scope());
  SgAssignInitializer *initializer = buildAssignInitializer(zero, vector_type);
  SgVariableDeclaration *declaration = buildVariableDeclaration(
      name, vector_type, initializer, transaction.outputScope());
  transaction.emitBeforeRegion(declaration);
  transaction.bindGeneratedOutput(
      original_symbol, requireExactDeclaredVariableSymbol(
                           declaration, "intel-simd-reduction-identity"));
}

SgAssignInitializer *
intel_write_partial_store(IntelSimdEmissionTransaction &transaction,
                          SgBinaryOp *op, SgOmpSimdStatement *target,
                          SgBasicBlock *new_block) {
  SgVarRefExp *var = isSgVarRefExp(op->get_lhs_operand());
  SgVarRefExp *src_var = isSgVarRefExp(op->get_rhs_operand());
  if (var == nullptr || var->get_symbol() == nullptr || src_var == nullptr ||
      src_var->get_symbol() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-reduction]: partial store "
            "does not own two exact variable operands\n");
    ROSE_ABORT();
  }

  ensureIntelPartialBroadcast(transaction, var, target);

  // Now set the local variable
  /*SgVarRefExp *varRef = buildVarRefExp(name, new_block);
  return buildAssignInitializer(varRef);*/
  return buildAssignInitializer(
      copyIntelIrOperand(transaction, src_var,
                         "intel-simd-partial-store-ownership"),
      intel_simd_type(transaction, var->get_type(), target->get_scope()));
}

// ============================================================================================
// Generates an Intel scalar-store statement
//
// Scalar store:
//
//__m256 __sub1 = _mm512_extractf32x8_ps(__vec6, 0);
//__m256 __sub2 = _mm512_extractf32x8_ps(__vec6, 1);
//__sub2 = _mm256_add_ps(__sub1, __sub2);
//__sub2 = _mm256_hadd_ps(__sub2,__sub2);
//__sub2 = _mm256_hadd_ps(__sub2,__sub2);
// float __buf0[8];
//_mm256_storeu_ps(&__buf0,__sub2);
// temp = __buf0[1] + __buf0[5];
//
void intel_write_scalar_store(IntelSimdEmissionTransaction &transaction,
                              SgBinaryOp *op, SgOmpSimdStatement *target) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgVarRefExp *scalar = static_cast<SgVarRefExp *>(lval);
  SgVarRefExp *vec = static_cast<SgVarRefExp *>(rval);
  std::vector<SgStatement *> to_insert;
  SgBasicBlock *epilogue_scope = transaction.outputScope();

  // Create the types
  SgType *vector_type = intel_simd_type(transaction, scalar->get_type(),
                                        target->get_scope(), true);

  SgAssignInitializer *local_init;
  std::string vec1 = transaction.nextBufferName();
  std::string vec2 = transaction.nextBufferName();

  // Extract
  if (transaction.vectorWidth() == 16) {
    std::string extract_name =
        intel_simd_func(transaction, Extract, vec->get_type());

    SgIntVal *val = buildIntVal(0);
    SgExprListExp *parameters = buildExprListExp(
        copyIntelIrOperand(transaction, vec,
                           "intel-simd-scalar-store-ownership"),
        val);
    SgExpression *fc1 = buildFunctionCallExp(extract_name, vector_type,
                                             parameters, target->get_scope());

    local_init = buildAssignInitializer(fc1, vector_type);
    SgVariableDeclaration *vd1 =
        buildVariableDeclaration(vec1, vector_type, local_init, epilogue_scope);
    to_insert.push_back(vd1);

    val = buildIntVal(1);
    parameters = buildExprListExp(
        copyIntelIrOperand(transaction, vec,
                           "intel-simd-scalar-store-ownership"),
        val);
    SgExpression *fc2 = buildFunctionCallExp(extract_name, vector_type,
                                             parameters, target->get_scope());

    local_init = buildAssignInitializer(fc2, vector_type);
    SgVariableDeclaration *vd2 =
        buildVariableDeclaration(vec2, vector_type, local_init, epilogue_scope);
    to_insert.push_back(vd2);

    // Add the two sub vectors
    parameters = buildExprListExp(buildVarRefExp(vec1, epilogue_scope),
                                  buildVarRefExp(vec2, epilogue_scope));
    std::string func_name =
        intel_simd_func(transaction, Add, vec->get_type(), true);
    SgExpression *fc3 = buildFunctionCallExp(func_name, vector_type, parameters,
                                             target->get_scope());
    SgExprStatement *expr =
        buildAssignStatement(buildVarRefExp(vec2, epilogue_scope), fc3);
    to_insert.push_back(expr);
  } else if (transaction.vectorWidth() == 8) {
    local_init = buildAssignInitializer(
        copyIntelIrOperand(transaction, vec,
                           "intel-simd-scalar-store-ownership"),
        vector_type);

    SgVariableDeclaration *vd2 =
        buildVariableDeclaration(vec2, vector_type, local_init, epilogue_scope);
    to_insert.push_back(vd2);
  }

  // Perform two horizontal adds
  std::string func_name =
      intel_simd_func(transaction, HAdd, vec->get_type(), true);
  auto build_horizontal_add = [&]() {
    SgExprListExp *parameters =
        buildExprListExp(buildVarRefExp(vec2, epilogue_scope),
                         buildVarRefExp(vec2, epilogue_scope));
    SgExpression *call = buildFunctionCallExp(func_name, vector_type,
                                              parameters, target->get_scope());
    return buildAssignStatement(buildVarRefExp(vec2, epilogue_scope), call);
  };
  SgExprStatement *expr = build_horizontal_add();
  to_insert.push_back(expr);

  if (vec->get_type()->variantT() != V_SgTypeDouble) {
    expr = build_horizontal_add();
    to_insert.push_back(expr);
  }

  // Create the buffer
  std::string name = transaction.nextBufferName();

  int buf_length = 8;
  int pos2 = 6;
  if (vec->get_type()->variantT() == V_SgTypeDouble) {
    buf_length = 4;
    pos2 = 2;
  }

  SgIntVal *length = buildIntVal(buf_length);
  SgType *type = buildArrayType(scalar->get_type(), length);

  SgVariableDeclaration *vd =
      buildVariableDeclaration(name, type, NULL, epilogue_scope);
  to_insert.push_back(vd);

  SgExprListExp *parameters;

  // Store
  if (vec->get_type()->variantT() == V_SgTypeInt) {
    SgVarRefExp *buffer = buildVarRefExp(name, epilogue_scope);
    SgAddressOfOp *addr =
        buildAddressOfOp(buffer, buildPointerType(buffer->get_type()));
    SgPointerType *ptr_type = buildPointerType(vector_type);
    SgCastExp *cast = buildCastExp(addr, ptr_type);
    parameters = buildExprListExp(cast, buildVarRefExp(vec2, epilogue_scope));
  } else {
    SgVarRefExp *buffer = buildVarRefExp(name, epilogue_scope);
    SgAddressOfOp *addr =
        buildAddressOfOp(buffer, buildPointerType(buffer->get_type()));
    parameters = buildExprListExp(addr, buildVarRefExp(vec2, epilogue_scope));
  }

  func_name = intel_simd_func(transaction, Store, vec->get_type(), true);
  SgExprStatement *fc = buildFunctionCallStmt(func_name, buildVoidType(),
                                              parameters, target->get_scope());
  to_insert.push_back(fc);

  // Scalar store
  // temp = __buf0[1] + __buf0[6];
  SgType *scalar_type = scalar->get_type();
  SgPntrArrRefExp *pntr1 = buildPntrArrRefExp(
      buildVarRefExp(name, epilogue_scope), buildIntVal(0), scalar_type);
  SgPntrArrRefExp *pntr2 = buildPntrArrRefExp(
      buildVarRefExp(name, epilogue_scope), buildIntVal(pos2), scalar_type);
  SgAddOp *add = buildAddOp(pntr1, pntr2, scalar_type);
  SgPlusAssignOp *assign =
      buildPlusAssignOp(copyExpression(scalar), add, scalar_type);

  expr = buildExprStatement(assign);
  to_insert.push_back(expr);

  // Now, add it all to the end of the loop
  for (SgStatement *statement : to_insert)
    transaction.emitAfterRegion(statement);
}

// ===========================================================================================================
// Writes an Intel SIMD math statement
//
SgAssignInitializer *
intel_write_math(const IntelSimdEmissionTransaction &transaction,
                 SgBinaryOp *op, SgOmpSimdStatement *target,
                 SgBasicBlock *new_block, VariantT math_type) {
  SgExpression *lval = op->get_lhs_operand();
  SgExpression *rval = op->get_rhs_operand();

  SgVarRefExp *va = static_cast<SgVarRefExp *>(lval);

  SgType *vector_type =
      intel_simd_type(transaction, va->get_type(), target->get_scope());
  SgExprListExp *parameters = isSgExprListExp(
      copyIntelIrOperand(transaction, rval, "intel-simd-math-ownership"));
  if (parameters == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-math]: right operand is "
            "not one exact expression list\n");
    ROSE_ABORT();
  }

  // Build the function call
  OpType x86Type = None;
  switch (math_type) {
  case V_SgSIMDAddOp:
    x86Type = Add;
    break;
  case V_SgSIMDSubOp:
    x86Type = Sub;
    break;
  case V_SgSIMDMulOp:
    x86Type = Mul;
    break;
  case V_SgSIMDDivOp:
    x86Type = Div;
    break;
  default: {
  }
  }

  std::string func_type = intel_simd_func(transaction, x86Type, va->get_type());
  SgExpression *ld = buildFunctionCallExp(func_type, vector_type, parameters,
                                          target->get_scope());

  if (transaction.isPlannedPartial(va->get_symbol())) {
    SgExprStatement *assign = buildAssignStatement(
        copyIntelIrOperand(transaction, va, "intel-simd-math-ownership"), ld);
    appendStatement(assign, new_block);
    return NULL;
  } else {
    return buildAssignInitializer(ld, vector_type);
  }
}

// =======================================================================================================================================
// Write the Intel intrinsics
void omp_simd_write_intel(SgOmpSimdStatement *target, SgForStatement *for_loop,
                          Rose_STL_Container<SgNode *> *ir_block,
                          unsigned int simd_length) {
  if (target == nullptr || for_loop == nullptr || ir_block == nullptr ||
      ir_block->empty() || target->get_body() == nullptr ||
      target->get_body()->get_parent() != target ||
      !isStructurallyOwnedBy(for_loop, target)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-region]: inputs do not "
            "identify one exact nonempty SIMD transformation region\n");
    ROSE_ABORT();
  }
  if (simd_length != 4 && simd_length != 8 && simd_length != 16) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-length]: width=%u is not "
            "one exact supported Intel SIMD width\n",
            simd_length);
    ROSE_ABORT();
  }
  IntelSimdEmissionTransaction transaction(
      simd_length, requireExactIntelLaneWidth(*ir_block, simd_length));
  planIntelGeneratedSymbols(transaction, *ir_block);
  transaction.beginOutputRegion(target);
  for (SgNode *node : *ir_block) {
    if (node->variantT() == V_SgSIMDPartialStore) {
      SgBinaryOp *partial = isSgBinaryOp(node);
      ensureIntelPartialBroadcast(
          transaction,
          partial != nullptr ? isSgVarRefExp(partial->get_lhs_operand())
                             : nullptr,
          target);
    }
  }

  // Setup the for loop
  SgBasicBlock *new_block = SageBuilder::buildBasicBlock();

  SgStatement *loop_body = getLoopBody(for_loop);
  replaceStatement(loop_body, new_block, true);

  // Translate the IR
  for (Rose_STL_Container<SgNode *>::iterator i = ir_block->begin();
       i != ir_block->end(); i++) {
    if (*i == nullptr || isSgBinaryOp(*i) == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: IR node=%p is not "
              "one exact binary SIMD operation\n",
              static_cast<void *>(*i));
      ROSE_ABORT();
    }

    SgBinaryOp *op = static_cast<SgBinaryOp *>(*i);
    SgExpression *lval = op->get_lhs_operand();
    SgExpression *rval = op->get_rhs_operand();

    SgAssignInitializer *init = NULL;

    switch ((*i)->variantT()) {
    case V_SgSIMDLoad: {
      init = intel_write_load(transaction, op, target, new_block);
    } break;

    case V_SgSIMDBroadcast: {
      init = intel_write_broadcast(transaction, op, target, new_block);
    } break;

    case V_SgSIMDGather: {
      init = intel_write_gather(transaction, op, target, new_block);
    } break;

    case V_SgSIMDExplicitGather: {
      init =
          intel_write_exp_gather(transaction, op, target, new_block, for_loop);
    } break;

    case V_SgSIMDStore: {
      intel_write_store(transaction, op, target, new_block);
    } break;

    case V_SgSIMDScatter: {
      intel_write_scatter(transaction, op, target, new_block);
    } break;

    case V_SgSIMDPartialStore: {
      init = intel_write_partial_store(transaction, op, target, new_block);
    } break;

    case V_SgSIMDScalarStore: {
      intel_write_scalar_store(transaction, op, target);
    } break;

    case V_SgSIMDAddOp:
    case V_SgSIMDSubOp:
    case V_SgSIMDMulOp:
    case V_SgSIMDDivOp: {
      init = intel_write_math(transaction, op, target, new_block,
                              (*i)->variantT());
    } break;

    default:
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: unsupported IR "
              "operation kind=%s\n",
              (*i)->sage_class_name());
      ROSE_ABORT();
    }

    // The variable declaration
    if ((*i)->variantT() !=
        V_SgSIMDScalarStore /*&& (*i)->variantT() != V_SgSIMDPartialStore*/) {
      if (isSgVarRefExp(lval)) {
        SgVarRefExp *var = static_cast<SgVarRefExp *>(lval);

        SgType *vector_type =
            intel_simd_type(transaction, var->get_type(), target->get_scope());
        if (!transaction.isPlannedPartial(var->get_symbol())) {
          if (init == nullptr) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: operation=%p "
                    "did not produce its required initializer\n",
                    static_cast<void *>(op));
            ROSE_ABORT();
          }
          SgScopeStatement *declaration_scope =
              (*i)->variantT() == V_SgSIMDBroadcast
                  ? static_cast<SgScopeStatement *>(transaction.outputScope())
                  : static_cast<SgScopeStatement *>(new_block);
          SgName name = transaction.uniqueOutputName(var->get_symbol(),
                                                     declaration_scope);
          SgVariableDeclaration *vd = buildVariableDeclaration(
              name, vector_type, init, declaration_scope);

          if ((*i)->variantT() == V_SgSIMDBroadcast) {
            transaction.emitBeforeRegion(vd);
          } else {
            appendStatement(vd, new_block);
          }
          transaction.bindGeneratedOutput(
              var->get_symbol(),
              requireExactDeclaredVariableSymbol(
                  vd, "intel-simd-generated-output-identity"));
        } else {
          if (init != nullptr) {
            SgExprStatement *expr = buildAssignStatement(
                copyIntelIrOperand(transaction, var,
                                   "intel-simd-partial-ownership"),
                init);
            appendStatement(expr, new_block);
          } else if ((*i)->variantT() != V_SgSIMDAddOp &&
                     (*i)->variantT() != V_SgSIMDSubOp &&
                     (*i)->variantT() != V_SgSIMDMulOp &&
                     (*i)->variantT() != V_SgSIMDDivOp) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[intel-simd-ir]: partial "
                    "operation=%p lost its required initializer\n",
                    static_cast<void *>(op));
            ROSE_ABORT();
          }
        }
      }
    }
  }

  bindExactTilingIncrements(target, transaction.loopIncrement());

  // Update the loop increment
  SgBinaryOp *inc = isSgBinaryOp(for_loop->get_increment());
  if (inc == nullptr || inc->get_rhs_operand() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[intel-simd-loop]: loop increment is "
            "not one exact binary expression\n");
    ROSE_ABORT();
  }
  SgExpression *original_increment = inc->get_rhs_operand();
  SgType *increment_type = original_increment->get_type();
  inc->set_rhs_operand(nullptr);
  original_increment->set_parent(nullptr);
  SgMultiplyOp *mul =
      buildMultiplyOp(original_increment,
                      buildIntVal(transaction.loopIncrement()), increment_type);
  inc->set_rhs_operand(mul);
  mul->set_parent(inc);
}
