#ifndef ROSE_FORTRAN_LINE_WRAP_SUPPORT_H
#define ROSE_FORTRAN_LINE_WRAP_SUPPORT_H

#include <cstddef>
#include <string>

namespace Rose {
namespace FortranLineWrapSupport {

bool startsWithCaseInsensitive(const std::string &text, size_t pos,
                               const char *token);
bool isCommentLine(const std::string &text);
bool isDirectiveChunk(const std::string &text, int used_cols);
int clampUsableColumnsToConfiguredWrap(int usable_cols, int configured_wrap);

} // namespace FortranLineWrapSupport
} // namespace Rose

#endif // ROSE_FORTRAN_LINE_WRAP_SUPPORT_H
