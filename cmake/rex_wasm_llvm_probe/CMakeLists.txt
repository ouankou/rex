cmake_minimum_required(VERSION 3.20)
project(rex_wasm_llvm_probe LANGUAGES CXX)

if(NOT DEFINED REX_EXPECTED_LLVM_VERSION OR
   NOT REX_EXPECTED_LLVM_VERSION MATCHES "^22\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR
    "REX_EXPECTED_LLVM_VERSION must name the exact LLVM 22 source version")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
if(NOT LLVM_PACKAGE_VERSION VERSION_EQUAL REX_EXPECTED_LLVM_VERSION)
  message(FATAL_ERROR
    "LLVM package version '${LLVM_PACKAGE_VERSION}' does not match exact source version '${REX_EXPECTED_LLVM_VERSION}'")
endif()

set(_rex_required_clang_targets
  clangFrontend
  clangDriver
  clangParse
  clangSema
  clangSerialization
  clangAnalysis
  clangASTMatchers
  clangAST
  clangEdit
  clangLex
  clangBasic
  clangSupport)
foreach(_rex_clang_target IN LISTS _rex_required_clang_targets)
  if(NOT TARGET ${_rex_clang_target})
    message(FATAL_ERROR
      "LLVM/Clang package does not export required target ${_rex_clang_target}")
  endif()
endforeach()

add_executable(rex_wasm_llvm_probe rex_wasm_llvm_probe.cpp)
target_compile_features(rex_wasm_llvm_probe PRIVATE cxx_std_17)
target_include_directories(rex_wasm_llvm_probe PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS})
target_link_libraries(rex_wasm_llvm_probe PRIVATE
  ${_rex_required_clang_targets})
