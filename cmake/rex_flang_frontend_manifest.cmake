# Required installed artifacts for the LLVM 23 Flang parsing, semantic analysis
# and expression-evaluation frontend. FIR/MLIR lowering is outside this inventory.
# Shared by normal configuration and the build script's CMake preflight.
set(REX_FLANG_FRONTEND_LIBRARIES
  FortranSemantics
  FortranEvaluate
  FortranParser
  FortranDecimal
  FortranSupport
)

set(REX_FLANG_FRONTEND_HEADERS
  flang/Evaluate/fold.h
  flang/Evaluate/tools.h
  flang/Parser/dump-parse-tree.h
  flang/Parser/instrumented-parser.h
  flang/Parser/message.h
  flang/Parser/parse-tree-visitor.h
  flang/Parser/parse-tree.h
  flang/Parser/parsing.h
  flang/Parser/provenance.h
  flang/Parser/unparse.h
  flang/Semantics/scope.h
  flang/Semantics/semantics.h
  flang/Semantics/symbol.h
  flang/Semantics/tools.h
  flang/Support/Fortran-features.h
  flang/Support/LangOptions.h
  flang/Support/OpenMP-features.h
  flang/Support/default-kinds.h
)

# Files the frontend requires in the Flang driver's target-specific intrinsic
# module directory, <resource-dir>/finclude/flang/<triple>.
set(REX_FLANG_INTRINSIC_FILES
  __fortran_builtins.mod
  iso_c_binding.mod
  omp_lib.h
  omp_lib.mod
)
