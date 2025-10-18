# CMakeLists.txt for Clang Frontend
# This is the experimental Clang-based frontend for C/C++ analysis as an alternative to EDG

if(enable-clang-frontend)
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

  # Set C++17 standard (required by LLVM 20)
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

  # Add LLVM includes as SYSTEM to avoid cxxabi.h conflicts with libstdc++
  # This ensures GCC's cxxabi.h is found before LLVM's version
  target_include_directories(libroseClangFrontend SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
  )

  # Add LLVM definitions
  target_compile_definitions(libroseClangFrontend PRIVATE
    ${LLVM_DEFINITIONS})

  # Compiler flags
  target_compile_options(libroseClangFrontend PRIVATE
    -w  # Suppress warnings during initial development
  )

  # Install headers
  install(FILES ${CLANG_FRONTEND_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR})

  # Install builtin headers
  install(FILES
    clang-builtin-c.h
    clang-builtin-cpp.hpp
    clang-builtin-cuda.hpp
    clang-builtin-opencl.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/clang)

  message(STATUS "Clang frontend configured successfully")

else()
  message(STATUS "Clang frontend NOT enabled (use -Denable-clang-frontend=ON)")
endif()
