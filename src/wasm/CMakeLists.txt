#******************************************************************************************************************#
# Copyright (c) 2018-2026, High Performance Computing Architecture and System
# research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
# and Lawrence Livermore National Security, LLC.
#
# SPDX-License-Identifier: (BSD-3-Clause)
#*****************************************************************************************************************#

add_executable(rex_wasm
  rex_wasm.cpp
  openacc_wasm_stubs.cpp
  ../frontend/SageIII/accparser/src/OpenACCIR.cpp
  ../frontend/SageIII/accparser/src/OpenACCIRToString.cpp)

target_link_libraries(rex_wasm PRIVATE ROSE_DLL)
target_compile_features(rex_wasm PRIVATE cxx_std_17)
target_include_directories(rex_wasm PRIVATE
  "${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/accparser/src")
target_compile_definitions(rex_wasm PRIVATE
  "REX_WASM_CLANG_RESOURCE_DIR=\"${REX_WASM_VIRTUAL_CLANG_RESOURCE_DIR}\"")

set_target_properties(rex_wasm PROPERTIES
  OUTPUT_NAME "rex_wasm"
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm")

set(REX_WASM_PRELOADS
  "--preload-file=${CMAKE_BINARY_DIR}/include-staging@/rex/include-staging")

if(NOT REX_WASM_CLANG_RESOURCE_DIR OR
   NOT IS_DIRECTORY "${REX_WASM_CLANG_RESOURCE_DIR}" OR
   IS_SYMLINK "${REX_WASM_CLANG_RESOURCE_DIR}" OR
   NOT EXISTS "${REX_WASM_CLANG_RESOURCE_DIR}/include/stddef.h" OR
   IS_DIRECTORY "${REX_WASM_CLANG_RESOURCE_DIR}/include/stddef.h" OR
   NOT EXISTS "${REX_WASM_CLANG_RESOURCE_DIR}/include/omp.h" OR
   IS_DIRECTORY "${REX_WASM_CLANG_RESOURCE_DIR}/include/omp.h")
  message(FATAL_ERROR
    "REX WASM requires the complete staged Clang resource directory: "
    "${REX_WASM_CLANG_RESOURCE_DIR}")
endif()
list(APPEND REX_WASM_PRELOADS
  "--preload-file=${REX_WASM_CLANG_RESOURCE_DIR}@${REX_WASM_VIRTUAL_CLANG_RESOURCE_DIR}")

if(REX_WASM_PACKAGE_SYSROOT)
  list(APPEND REX_WASM_PRELOADS
    "--preload-file=${REX_WASM_SYSROOT}/include@/emscripten/sysroot/include")
  target_compile_definitions(rex_wasm PRIVATE REX_WASM_HAS_SYSROOT=1)
endif()

target_compile_options(rex_wasm PRIVATE "-Oz")

target_link_options(rex_wasm PRIVATE
  "--bind"
  "-Oz"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createRexModule"
  "-sENVIRONMENT=web,worker,node"
  "-sINCOMING_MODULE_JS_API=locateFile,print,printErr"
  "-sASSERTIONS=0"
  "-sEXIT_RUNTIME=0"
  "-sFORCE_FILESYSTEM=1"
  ${REX_WASM_PRELOADS})
