# CMakeLists.txt for Clang Frontend
# Clang/LLVM-based frontend for C and C++ language analysis.

message(STATUS "Configuring Clang frontend...")

# Clang frontend source files
set(CLANG_FRONTEND_SOURCES
  clang-frontend.cpp
  clang-frontend-decl.cpp
  clang-frontend-stmt.cpp
  clang-frontend-type.cpp
  clang-frontend-valgrind.cpp
  clang-to-rose-support.cpp
  clang-to-dot.cpp
  clang-to-dot-decl.cpp
  clang-to-dot-stmt.cpp
  clang-to-dot-type.cpp
  clang_paths.cpp
)

# Clang frontend headers
set(CLANG_FRONTEND_HEADERS
  clang-decl-attachment-session.hpp
  clang-expanded-token-order.hpp
  clang-frontend.hpp
  clang-frontend-private.hpp
  clang-to-dot.hpp
  clang-to-dot-private.hpp
  clang_graph.h
)

# Create OBJECT library for Clang frontend
add_library(libroseClangFrontend OBJECT ${CLANG_FRONTEND_SOURCES})

# Depend on ROSETTA-generated files (Cxx_Grammar.h etc.)
add_dependencies(libroseClangFrontend rosetta_generated)

# Set C++17 standard
set_target_properties(libroseClangFrontend PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Include directories
# Add ROSE includes and current source directory as regular includes
target_include_directories(libroseClangFrontend PRIVATE
  ${ROSE_INCLUDES}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add LLVM includes to ensure LLVM headers are found first
# when multiple LLVM versions are installed.
# Get LLVM include path from llvm-config if LLVM_INCLUDE_DIRS is not set
if(NOT LLVM_INCLUDE_DIRS)
  find_program(CLANG_LLVM_CONFIG NAMES llvm-config-22 llvm-config)
  if(CLANG_LLVM_CONFIG)
    execute_process(COMMAND ${CLANG_LLVM_CONFIG} --includedir
      OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()
message(STATUS "Clang frontend using LLVM includes: ${LLVM_INCLUDE_DIRS}")

# Ensure LLVM/Clang headers are preferred for this target only.
rose_force_system_include_dirs(libroseClangFrontend
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS})

if(REX_ENABLE_WASM)
  foreach(_rex_wasm_clang_contract_var
      REX_WASM_VIRTUAL_CLANG_C_DRIVER
      REX_WASM_VIRTUAL_CLANG_CXX_DRIVER
      REX_WASM_VIRTUAL_CLANG_RESOURCE_DIR)
    if(NOT DEFINED ${_rex_wasm_clang_contract_var}
       OR "${${_rex_wasm_clang_contract_var}}" STREQUAL "")
      message(FATAL_ERROR
        "REX WASM Clang frontend contract is missing ${_rex_wasm_clang_contract_var}")
    endif()
  endforeach()
  unset(_rex_wasm_clang_contract_var)

  target_compile_definitions(libroseClangFrontend PRIVATE
    REX_WASM_IN_PROCESS_CLANG=1
    "REX_WASM_CLANG_C_DRIVER=\"${REX_WASM_VIRTUAL_CLANG_C_DRIVER}\""
    "REX_WASM_CLANG_CXX_DRIVER=\"${REX_WASM_VIRTUAL_CLANG_CXX_DRIVER}\""
    "REX_WASM_CLANG_RESOURCE_DIR=\"${REX_WASM_VIRTUAL_CLANG_RESOURCE_DIR}\"")
endif()

# Add LLVM definitions
set(_rose_clang_llvm_definitions "")
foreach(_rose_llvm_definition IN LISTS LLVM_DEFINITIONS)
  separate_arguments(_rose_split_llvm_definitions UNIX_COMMAND
    "${_rose_llvm_definition}")
  list(APPEND _rose_clang_llvm_definitions ${_rose_split_llvm_definitions})
endforeach()
if(_rose_clang_llvm_definitions)
  target_compile_options(libroseClangFrontend PRIVATE
    ${_rose_clang_llvm_definitions})
endif()
unset(_rose_clang_llvm_definitions)
unset(_rose_llvm_definition)
unset(_rose_split_llvm_definitions)

rose_enable_private_pch(libroseClangFrontend
  "${ROSE_TOP_SRC_DIR}/cmake/pch/clang-frontend-pch.hpp")

set(CLANG_BUILTIN_HEADERS
  clang-builtin-c.h
  clang-builtin-cpp.hpp
  clang-builtin-cuda.hpp
  clang-builtin-opencl.h
  clang-builtin-openmp-compat.h
  irs.h
  irsctl.h
)
set(CLANG_OPENMP_COMPAT_HEADERS
  openmp-compat/omp.h
)

# Copy builtin headers to staging directory for build-tree usage
# (Install tree headers are handled by install() commands below)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include-staging/clang)
foreach(header IN LISTS CLANG_BUILTIN_HEADERS)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${header}
    ${CMAKE_BINARY_DIR}/include-staging/clang/${header}
    COPYONLY)
endforeach()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include-staging/openmp-compat)
foreach(header IN LISTS CLANG_OPENMP_COMPAT_HEADERS)
  get_filename_component(_header_name ${header} NAME)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${header}
    ${CMAKE_BINARY_DIR}/include-staging/openmp-compat/${_header_name}
    COPYONLY)
endforeach()
unset(_header_name)

# Install headers
install(FILES ${CLANG_FRONTEND_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR})

# Install builtin headers
install(FILES ${CLANG_BUILTIN_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/clang)

install(FILES ${CLANG_OPENMP_COMPAT_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR_FOR_INTRINSICS}/clang/openmp-compat)

message(STATUS "Clang frontend configured successfully")
