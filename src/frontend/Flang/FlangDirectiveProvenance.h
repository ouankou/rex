#ifndef ROSE_FLANG_DIRECTIVE_PROVENANCE_H_
#define ROSE_FLANG_DIRECTIVE_PROVENANCE_H_

#include <cstddef>
#include <string>

namespace Rose {
namespace builder {
namespace detail {

[[noreturn]] void RejectMissingFlangDirectiveCharacterProvenance(
    std::size_t cooked_character_index, const std::string &expected_path);

[[noreturn]] void RejectCrossFileFlangDirectiveCharacterProvenance(
    std::size_t cooked_character_index, const std::string &expected_path,
    const std::string &actual_path);

} // namespace detail
} // namespace builder
} // namespace Rose

#endif // ROSE_FLANG_DIRECTIVE_PROVENANCE_H_
