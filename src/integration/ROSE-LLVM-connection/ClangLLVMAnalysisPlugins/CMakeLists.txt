project(ClangLLVMAnalysisPlugin)
cmake_minimum_required(VERSION 3.12)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/cmake/modules"  ${CMAKE_MODULE_PATH})

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED)

set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 0)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${LLVM_CMAKE_DIR}"
  )

# import LLVM CMake functions
include(AddLLVM)

if( NOT LLVM_REQUIRES_RTTI )
  if( NOT LLVM_REQUIRES_EH )
    set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/RoseConnectionClangPlugin.exports)
  endif()
endif()

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_library(RoseConnectionClangPlugin MODULE RoseConnectionClangPlugin.cpp)


add_library(RoseConnectionLLVMPassPlugin MODULE RoseConnectionLLVMPassPlugin.cpp)
target_compile_features(RoseConnectionLLVMPassPlugin PRIVATE cxx_range_for cxx_auto_type)
target_link_libraries(RoseConnectionLLVMPassPlugin)
