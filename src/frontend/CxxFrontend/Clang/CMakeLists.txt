# CMakeLists.txt for Clang Frontend
# Clang/LLVM-based frontend for C language analysis (experimental)

message(STATUS "Configuring Clang frontend...")

# Clang frontend source files
set(CLANG_FRONTEND_SOURCES
  clang-frontend.cpp
  clang-frontend-decl.cpp
  clang-frontend-stmt.cpp
  clang-frontend-type.cpp
  clang-to-rose-support.cpp
  clang-to-dot.cpp
  clang-to-dot-decl.cpp
  clang-to-dot-stmt.cpp
  clang-to-dot-type.cpp
  clang_paths.cpp
)

# Clang frontend headers
set(CLANG_FRONTEND_HEADERS
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
  find_program(CLANG_LLVM_CONFIG NAMES llvm-config-21 llvm-config)
  if(CLANG_LLVM_CONFIG)
    execute_process(COMMAND ${CLANG_LLVM_CONFIG} --includedir
      OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()
message(STATUS "Clang frontend using LLVM includes: ${LLVM_INCLUDE_DIRS}")

# Ensure LLVM/Clang headers are preferred for this target only.
target_include_directories(libroseClangFrontend SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

# Add LLVM definitions
target_compile_definitions(libroseClangFrontend PRIVATE
  ${LLVM_DEFINITIONS})

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
