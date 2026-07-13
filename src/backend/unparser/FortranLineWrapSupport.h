#ifndef ROSE_FORTRAN_LINE_WRAP_SUPPORT_H
#define ROSE_FORTRAN_LINE_WRAP_SUPPORT_H

#include <cstddef>
#include <string>
#include <vector>

namespace Rose {
namespace FortranLineWrapSupport {

bool isCommentLine(const std::string &text);
bool isFixedFormatCommentLine(const std::string &text);
std::vector<size_t> stringLiteralLexicalBoundaries(const std::string &literal,
                                                   char delimiter);
std::vector<std::string> wrapFreeFormatComment(const std::string &text,
                                               int first_line_used_columns,
                                               int usable_columns);
std::vector<std::string> wrapFixedFormatComment(const std::string &text,
                                                int first_line_used_columns,
                                                int usable_columns);

} // namespace FortranLineWrapSupport
} // namespace Rose

#endif // ROSE_FORTRAN_LINE_WRAP_SUPPORT_H
