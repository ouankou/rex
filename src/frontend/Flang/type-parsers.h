//===-- lib/Parser/type-parsers.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef FORTRAN_PARSER_TYPE_PARSERS_H_
#define FORTRAN_PARSER_TYPE_PARSERS_H_

#include <flang/Parser/parse-state.h>
#include <flang/Parser/parse-tree.h>

#include <optional>

namespace Fortran::parser {

// LLVM 22's program entry parses REX's provenance-preserving OpenMP projection
// directly, without replacing the original cooked stream owned by Parsing.
template <typename A> struct Parser {
  using resultType = A;
  constexpr Parser() {}
  constexpr Parser(const Parser &) = default;
  static std::optional<resultType> Parse(ParseState &);
};

constexpr Parser<Program> program;

} // namespace Fortran::parser
#endif // FORTRAN_PARSER_TYPE_PARSERS_H_
