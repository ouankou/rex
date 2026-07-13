/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "sage3basic.h"

#include "OpenACCParser.h"
#include "accAstConstruction.h"

#include <cstdio>

namespace openacc {

ParseResult parseDirective(std::string_view, ParseOptions) {
  std::fputs("REX_WASM_INVARIANT[openacc-parser]: OpenACC parsing is not "
             "available in the WebAssembly frontend\n",
             stderr);
  ROSE_ABORT();
}

} // namespace openacc

SgStatement *convertOpenACCDirective(SgPragmaDeclaration *,
                                     const openacc::Directive &) {
  std::fputs("REX_WASM_INVARIANT[openacc-ast]: OpenACC AST construction is "
             "not available in the WebAssembly frontend\n",
             stderr);
  ROSE_ABORT();
}

void validateOpenACCDirectiveForSage(const openacc::Directive &) {
  std::fputs("REX_WASM_INVARIANT[openacc-ast]: OpenACC AST validation is not "
             "available in the WebAssembly frontend\n",
             stderr);
  ROSE_ABORT();
}
