// REX-owned Fortran source whitespace policy.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ROSE_FORTRAN_SOURCE_WHITESPACE_H
#define ROSE_FORTRAN_SOURCE_WHITESPACE_H

namespace Rose::FortranLex {

// Source buffers and std::string::c_str() provide a terminating NUL byte.
// Return byte counts so UTF-8 blanks retain their exact physical extents.
inline int SpaceBytes(const char *p) {
  if (*p == ' ' || *p == '\r' || *p == '\xa0')
    return 1;
  if (p[0] == '\xc2' && p[1] == '\xa0')
    return 2;
  return 0;
}

inline int BlankBytes(const char *p) { return *p == '\t' ? 1 : SpaceBytes(p); }

} // namespace Rose::FortranLex

#endif
