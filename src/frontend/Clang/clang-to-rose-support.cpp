#include "clang-frontend-private.hpp"

#include "sage3basic.h"

#include <atomic>

SgAsmOp::asm_operand_modifier_enum
ClangToSageTranslator::get_sgAsmOperandModifier(std::string modifier) {
  unsigned result = SgAsmOp::e_unknown;

  for (char c : modifier) {
    switch (c) {
    case '=':
      result |= SgAsmOp::e_output;
      break;
    case '+':
      result |= SgAsmOp::e_modify;
      break;
    case '&':
      result |= SgAsmOp::e_earlyclobber;
      break;
    case '%':
      result |= SgAsmOp::e_commutative;
      break;
    case '*':
      result |= SgAsmOp::e_ignore_next;
      break;
    case '#':
      result |= SgAsmOp::e_ignore_till_comma;
      break;
    case '?':
      result |= SgAsmOp::e_poor_choice;
      break;
    case '!':
      result |= SgAsmOp::e_bad_choice;
      break;
    default:
      ROSE_ABORT();
    }
  }
  return static_cast<SgAsmOp::asm_operand_modifier_enum>(result);
}

std::string ClangToSageTranslator::anonymousDeclarationId(
    const clang::NamedDecl *declaration) {
  ROSE_ASSERT(declaration != nullptr);
  static std::atomic<uint64_t> next_id{0};
  auto [entry, inserted] = p_anonymous_declaration_ids.try_emplace(
      declaration->getCanonicalDecl(), 0);
  if (inserted)
    entry->second = next_id.fetch_add(1, std::memory_order_relaxed);
  return std::to_string(entry->second);
}
