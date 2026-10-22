add_library(libroseClangFrontend OBJECT
  clang-integer-value.cpp
  clang-frontend.cpp
  clang-frontend-decl.cpp
  clang-frontend-stmt.cpp
  clang-frontend-type.cpp
  clang-to-rose-support.cpp
  clang-to-dot.cpp
)

add_dependencies(libroseClangFrontend rosetta_generated)

target_include_directories(libroseClangFrontend PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# The parent configuration loads the required LLVM/Clang package exactly once.
# Another llvm-config search could select headers from a different installation.
if(NOT LLVM_INCLUDE_DIRS)
  message(FATAL_ERROR "The configured LLVM package has no include directories")
endif()

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

separate_arguments(_rose_clang_llvm_definitions UNIX_COMMAND "${LLVM_DEFINITIONS}")
if(_rose_clang_llvm_definitions)
  target_compile_options(libroseClangFrontend PRIVATE
    ${_rose_clang_llvm_definitions})
endif()
unset(_rose_clang_llvm_definitions)

rose_enable_private_pch(libroseClangFrontend
  "${ROSE_TOP_SRC_DIR}/cmake/pch/clang-frontend-pch.hpp")

set(CLANG_BUILTIN_HEADERS
  clang-builtin-c.h
  clang-builtin-cuda.hpp
  clang-builtin-openmp-compat.h
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

install(FILES
  clang-integer-value.hpp
  clang-decl-attachment-session.hpp
  clang-expanded-token-order.hpp
  clang-frontend.hpp
  clang-frontend-private.hpp
  clang-to-dot.hpp
  DESTINATION ${INCLUDE_INSTALL_DIR})

install(FILES ${CLANG_BUILTIN_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/clang)

install(FILES ${CLANG_OPENMP_COMPAT_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR_FOR_INTRINSICS}/clang/openmp-compat)
