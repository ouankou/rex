cmake_minimum_required(VERSION 3.19)

if(DEFINED _ROSE_CONFIGURED_ONCE)
  message(FATAL_ERROR
    "This build cache predates scoped REX linker options and may contain "
    "rewritten user linker flags. Configure a fresh build directory and "
    "supply your intended toolchain and linker flags again.")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are unsupported because REX generates and removes files "
    "inside its binary tree. Configure with a separate build directory.")
endif()

set(LLVM_REQUIRED_MAJOR "23" CACHE STRING "Exact LLVM/Clang major version required by REX")
if(NOT LLVM_REQUIRED_MAJOR STREQUAL "23")
  message(FATAL_ERROR
    "REX is pinned to LLVM/Clang major 23; LLVM_REQUIRED_MAJOR='${LLVM_REQUIRED_MAJOR}' is unsupported.")
endif()

option(REX_ENABLE_WASM "Build the pure static browser WebAssembly REX demo" FALSE)

include(cmake/rex_llvm_driver.cmake)

# Keep C and C++ compiler discovery within the same toolchain prefix whenever
# possible so CMake does not mix binaries from different LLVM installs.
function(find_sibling_compiler OUT_VAR REFERENCE_COMPILER)
  set(_sibling_compiler "")
  if(REFERENCE_COMPILER AND EXISTS "${REFERENCE_COMPILER}")
    get_filename_component(_reference_dir "${REFERENCE_COMPILER}" DIRECTORY)
    foreach(_candidate_name ${ARGN})
      if(EXISTS "${_reference_dir}/${_candidate_name}" AND
         NOT IS_DIRECTORY "${_reference_dir}/${_candidate_name}")
        set(_sibling_compiler "${_reference_dir}/${_candidate_name}")
        break()
      endif()
    endforeach()
  endif()
  set(${OUT_VAR} "${_sibling_compiler}" PARENT_SCOPE)
endfunction()

# Respect an explicit compiler or environment setting, then search the versioned
# name before its unversioned alias. Compiler identity/version checks follow
# language initialization, including when CMake selects its system default.
function(detect_compiler CMAKE_VAR ENV_VAR LANG_NAME PREFERRED_NAMES ALIAS_NAMES)
  if((NOT DEFINED ${CMAKE_VAR} OR "${${CMAKE_VAR}}" STREQUAL "") AND
     (NOT DEFINED ENV{${ENV_VAR}} OR "$ENV{${ENV_VAR}}" STREQUAL ""))
    find_program(_COMPILER NAMES ${PREFERRED_NAMES} ${ALIAS_NAMES})
    if(_COMPILER)
      set(${CMAKE_VAR} ${_COMPILER} PARENT_SCOPE)
      message(STATUS "Auto-detected ${LANG_NAME} compiler: ${_COMPILER}")
    endif()
    unset(_COMPILER CACHE)
  endif()
endfunction()

# Explicit toolchains select their compilers during project().
if(NOT REX_ENABLE_WASM AND NOT CMAKE_TOOLCHAIN_FILE)
  if((NOT DEFINED CMAKE_C_COMPILER OR "${CMAKE_C_COMPILER}" STREQUAL "") AND
     (NOT DEFINED ENV{CC} OR "$ENV{CC}" STREQUAL "") AND
     DEFINED CMAKE_CXX_COMPILER AND NOT "${CMAKE_CXX_COMPILER}" STREQUAL "")
    find_sibling_compiler(_C_COMPILER_FROM_CXX "${CMAKE_CXX_COMPILER}" "clang-${LLVM_REQUIRED_MAJOR}" "clang")
    if(_C_COMPILER_FROM_CXX)
      set(CMAKE_C_COMPILER "${_C_COMPILER_FROM_CXX}")
      message(STATUS "Auto-detected C compiler next to C++ compiler: ${_C_COMPILER_FROM_CXX}")
    endif()
    unset(_C_COMPILER_FROM_CXX)
  endif()
  detect_compiler(CMAKE_C_COMPILER CC "C" "clang-${LLVM_REQUIRED_MAJOR}" "clang")

  if((NOT DEFINED CMAKE_CXX_COMPILER OR "${CMAKE_CXX_COMPILER}" STREQUAL "") AND
     (NOT DEFINED ENV{CXX} OR "$ENV{CXX}" STREQUAL "") AND
     DEFINED CMAKE_C_COMPILER AND NOT "${CMAKE_C_COMPILER}" STREQUAL "")
    find_sibling_compiler(_CXX_COMPILER_FROM_C "${CMAKE_C_COMPILER}" "clang++-${LLVM_REQUIRED_MAJOR}" "clang++")
    if(_CXX_COMPILER_FROM_C)
      set(CMAKE_CXX_COMPILER "${_CXX_COMPILER_FROM_C}")
      message(STATUS "Auto-detected C++ compiler next to C compiler: ${_CXX_COMPILER_FROM_C}")
    endif()
    unset(_CXX_COMPILER_FROM_C)
  endif()
  detect_compiler(CMAKE_CXX_COMPILER CXX "C++" "clang++-${LLVM_REQUIRED_MAJOR}" "clang++")
else()
  message(STATUS "Compiler selection: deferring to the toolchain")
endif()

set(_ROSE_USER_PROVIDED_CMAKE_AR FALSE)
if(DEFINED ENV{AR} AND NOT "$ENV{AR}" STREQUAL "")
  set(_ROSE_USER_PROVIDED_CMAKE_AR TRUE)
elseif(DEFINED CMAKE_AR AND NOT "${CMAKE_AR}" STREQUAL "")
  if(NOT DEFINED _ROSE_AUTO_CMAKE_AR OR NOT "${CMAKE_AR}" STREQUAL "${_ROSE_AUTO_CMAKE_AR}")
    set(_ROSE_USER_PROVIDED_CMAKE_AR TRUE)
  endif()
endif()

set(_ROSE_USER_PROVIDED_CMAKE_RANLIB FALSE)
if(DEFINED ENV{RANLIB} AND NOT "$ENV{RANLIB}" STREQUAL "")
  set(_ROSE_USER_PROVIDED_CMAKE_RANLIB TRUE)
elseif(DEFINED CMAKE_RANLIB AND NOT "${CMAKE_RANLIB}" STREQUAL "")
  if(NOT DEFINED _ROSE_AUTO_CMAKE_RANLIB OR NOT "${CMAKE_RANLIB}" STREQUAL "${_ROSE_AUTO_CMAKE_RANLIB}")
    set(_ROSE_USER_PROVIDED_CMAKE_RANLIB TRUE)
  endif()
endif()

# Read the complete version before project() sets library versioning metadata.
set(ROSE_SCM_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ROSE_VERSION")
if(NOT EXISTS "${ROSE_SCM_VERSION_FILE}")
  message(FATAL_ERROR "ROSE version file not found: ${ROSE_SCM_VERSION_FILE}")
endif()
file(READ "${ROSE_SCM_VERSION_FILE}" ROSE_PACKAGE_VERSION)
string(STRIP "${ROSE_PACKAGE_VERSION}" ROSE_PACKAGE_VERSION)

project(ROSE
  VERSION "${ROSE_PACKAGE_VERSION}"
  LANGUAGES C CXX
)

message(STATUS "Compiler selection: Using ${CMAKE_C_COMPILER} and ${CMAKE_CXX_COMPILER}")

if(REX_ENABLE_WASM AND NOT EMSCRIPTEN)
  message(FATAL_ERROR "REX_ENABLE_WASM requires configuring with emcmake/Emscripten.")
endif()

if(CMAKE_CXX_COMPILER)
  find_sibling_compiler(_ROSE_LLVM_AR "${CMAKE_CXX_COMPILER}" "llvm-ar")
  if(_ROSE_LLVM_AR AND NOT _ROSE_USER_PROVIDED_CMAKE_AR)
    set(CMAKE_AR "${_ROSE_LLVM_AR}" CACHE FILEPATH "Archiver" FORCE)
    set(_ROSE_AUTO_CMAKE_AR "${_ROSE_LLVM_AR}" CACHE INTERNAL "REX auto-selected archiver")
  endif()
  unset(_ROSE_LLVM_AR)

  find_sibling_compiler(_ROSE_LLVM_RANLIB "${CMAKE_CXX_COMPILER}" "llvm-ranlib")
  if(_ROSE_LLVM_RANLIB AND NOT _ROSE_USER_PROVIDED_CMAKE_RANLIB)
    set(CMAKE_RANLIB "${_ROSE_LLVM_RANLIB}" CACHE FILEPATH "Archive index generator" FORCE)
    set(_ROSE_AUTO_CMAKE_RANLIB "${_ROSE_LLVM_RANLIB}" CACHE INTERNAL "REX auto-selected archive index generator")
  endif()
  unset(_ROSE_LLVM_RANLIB)
endif()
unset(_ROSE_USER_PROVIDED_CMAKE_AR)
unset(_ROSE_USER_PROVIDED_CMAKE_RANLIB)

# Keep assertions in RelWithDebInfo and disable optimization in Debug.
add_compile_options(
  "$<$<AND:$<CONFIG:RelWithDebInfo>,$<COMPILE_LANGUAGE:C,CXX>>:-UNDEBUG>"
  "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:-O0>")

# Clang installs libc++abi headers in its prefix; when linking against libstdc++
# prefer libstdc++ headers to avoid mixed-ABI conflicts (e.g., cxxabi.h).
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   "stdc++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
  set(_rose_libstdcxx_include_dirs "${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")
  list(FILTER _rose_libstdcxx_include_dirs INCLUDE REGEX "/c\\+\\+/")
  list(REMOVE_DUPLICATES _rose_libstdcxx_include_dirs)
  if(_rose_libstdcxx_include_dirs)
    list(TRANSFORM _rose_libstdcxx_include_dirs PREPEND "-isystem")
    add_library(rose_clang_libstdcxx_headers INTERFACE)
    target_compile_options(rose_clang_libstdcxx_headers INTERFACE
      ${_rose_libstdcxx_include_dirs})
    install(TARGETS rose_clang_libstdcxx_headers EXPORT RoseTargets)
  endif()
  unset(_rose_libstdcxx_include_dirs)
endif()

# Some packaged Clang toolchains report LLVM's installed headers as implicit
# include directories to CMake even when the C++ frontend does not actually
# search them. Force such paths through raw -isystem options for the targets
# that include LLVM/Clang/Flang headers so the generated build rules remain
# correct across toolchain packaging differences.
function(rose_force_system_include_dirs target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR
      "rose_force_system_include_dirs(${target} ...) requires an existing CMake target. "
      "Call it only after add_library() or add_executable() has created target '${target}'.")
  endif()

  set(_rose_system_include_dirs "${ARGN}")
  list(REMOVE_DUPLICATES _rose_system_include_dirs)

  set(_rose_system_include_opts "")
  foreach(_rose_dir IN LISTS _rose_system_include_dirs)
    if(_rose_dir)
      list(APPEND _rose_system_include_opts "-isystem${_rose_dir}")
    endif()
  endforeach()

  if(_rose_system_include_opts)
    target_compile_options(${target} PRIVATE ${_rose_system_include_opts})
  endif()
endfunction()

option(ROSE_ENABLE_PCH "Enable precompiled headers for in-tree REX targets" TRUE)
if(REX_ENABLE_WASM)
  set(ROSE_ENABLE_PCH FALSE CACHE BOOL "Enable precompiled headers for in-tree REX targets" FORCE)
endif()

include(cmake/rex_pch.cmake)

function(rose_collect_subdirectories output directory)
  set(_rose_directories "${directory}")
  get_property(_rose_children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(_rose_child IN LISTS _rose_children)
    rose_collect_subdirectories(_rose_grandchildren "${_rose_child}")
    list(APPEND _rose_directories ${_rose_grandchildren})
  endforeach()
  set(${output} ${_rose_directories} PARENT_SCOPE)
endfunction()

function(rose_apply_public_pch_to_in_tree_targets)
  if(NOT ROSE_ENABLE_PCH OR NOT TARGET rosePublicPch)
    return()
  endif()

  rose_collect_subdirectories(_rose_directories "${ROSE_TOP_SRC_DIR}")
  foreach(_rose_directory IN LISTS _rose_directories)
    get_property(_rose_targets DIRECTORY "${_rose_directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_rose_target IN LISTS _rose_targets)
      if(_rose_target STREQUAL "rosePublicPch")
        continue()
      endif()
      get_target_property(_rose_target_type "${_rose_target}" TYPE)
      if(NOT _rose_target_type STREQUAL "EXECUTABLE" AND
         NOT _rose_target_type STREQUAL "MODULE_LIBRARY")
        continue()
      endif()
      get_target_property(_rose_target_links "${_rose_target}" LINK_LIBRARIES)
      if("ROSE_DLL" IN_LIST _rose_target_links OR
         "Rose::rose" IN_LIST _rose_target_links)
        rose_reuse_pch(${_rose_target} rosePublicPch)
      endif()
    endforeach()
  endforeach()

endfunction()

# In CMake 3.27+ find_package() also searches upper-case <PACKAGENAME>_ROOT variables in addition to the
# properly-cased <PackageName>_ROOT variables. Enable this so LLVM_ROOT and CLANG_ROOT are honored.
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Distro LLVM packages often enable optional LibEdit/CURL probes in
# LLVMConfig.cmake. Disable these optional dependency lookups by default to
# avoid unnecessary configure noise for REX consumers. Users can override with:
#   -DCMAKE_DISABLE_FIND_PACKAGE_LibEdit=OFF
#   -DCMAKE_DISABLE_FIND_PACKAGE_CURL=OFF
if(NOT DEFINED CMAKE_DISABLE_FIND_PACKAGE_LibEdit)
  set(CMAKE_DISABLE_FIND_PACKAGE_LibEdit ON CACHE BOOL
    "Disable optional LibEdit package lookup during CMake configure")
endif()
if(NOT DEFINED CMAKE_DISABLE_FIND_PACKAGE_CURL)
  set(CMAKE_DISABLE_FIND_PACKAGE_CURL ON CACHE BOOL
    "Disable optional CURL package lookup during CMake configure")
endif()

option(BUILD_SHARED_LIBS "Build all libraries shared" FALSE)

option(use-lib64-paths "Should ROSE be installed in lib64 paths? (Some Linux distributes expect this)" OFF)
if(use-lib64-paths)
  set(ROSE_LIB_DIR_NAME "lib64")
else()
  set(ROSE_LIB_DIR_NAME "lib")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/cmake/modules"  ${CMAKE_MODULE_PATH})

# ROSE source and build (binary) directory hierarchies
set(ROSE_TOP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ROSE_TOP_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Keep OpenMP version in CMake as the single source of truth and use it
# to generate both C++ and Fortran OpenMP interfaces.
# OpenMP 6.0 corresponds to _OPENMP=202411.
set(ROSE_OPENMP_VERSION 202411 CACHE INTERNAL
  "OpenMP version used by ROSE-generated OpenMP interfaces")
message(STATUS "ROSE OpenMP version: ${ROSE_OPENMP_VERSION}")

# ROSE version information

# REX accepts both the historical four-component ROSE encoding (A.B.C.D -> ABBCCCDDDD)
# and the newer upstream three-component encoding (A.B.C -> A*10000000 + B*100 + C).
set(ROSE_SCM_DATE_FILE "${PROJECT_SOURCE_DIR}/config/SCM_DATE")
if(NOT EXISTS "${ROSE_SCM_DATE_FILE}")
  message(FATAL_ERROR "ROSE SCM Date file not found: ${ROSE_SCM_DATE_FILE}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${ROSE_SCM_VERSION_FILE}"
  "${ROSE_SCM_DATE_FILE}")

# project() validates the numeric version and publishes its components.
# REX's SCM encoding requires exactly three or four components.
if(PROJECT_VERSION_MINOR STREQUAL "" OR PROJECT_VERSION_PATCH STREQUAL "")
  message(FATAL_ERROR "Invalid ROSE version string: ${ROSE_PACKAGE_VERSION}")
endif()
set(ROSE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(ROSE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(ROSE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(ROSE_VERSION_BUILD "${PROJECT_VERSION_TWEAK}")
foreach(_rose_version_component ROSE_VERSION_MAJOR ROSE_VERSION_MINOR ROSE_VERSION_PATCH ROSE_VERSION_BUILD)
  if(NOT "${${_rose_version_component}}" STREQUAL "")
    string(REGEX REPLACE "^0+" "" ${_rose_version_component} "${${_rose_version_component}}")
    if("${${_rose_version_component}}" STREQUAL "")
      set(${_rose_version_component} 0)
    endif()
  endif()
endforeach()
unset(_rose_version_component)

# Compute the encoded numeric version and enforce consistency with SCM_DATE.
if(NOT PROJECT_VERSION_TWEAK STREQUAL "")
  if(ROSE_VERSION_MAJOR GREATER 9)
    message(FATAL_ERROR "ROSE version major component must be < 10: ${ROSE_VERSION_MAJOR}")
  endif()
  if(ROSE_VERSION_MINOR GREATER 99)
    message(FATAL_ERROR "ROSE version minor component must be < 100: ${ROSE_VERSION_MINOR}")
  endif()
  if(ROSE_VERSION_PATCH GREATER 999)
    message(FATAL_ERROR "ROSE version patch component must be < 1000: ${ROSE_VERSION_PATCH}")
  endif()
  if(ROSE_VERSION_BUILD GREATER 9999)
    message(FATAL_ERROR "ROSE version build component must be < 10000: ${ROSE_VERSION_BUILD}")
  endif()
  math(EXPR ROSE_VERSION_ENCODED
    "(((${ROSE_VERSION_MAJOR} * 100 + ${ROSE_VERSION_MINOR}) * 1000 + ${ROSE_VERSION_PATCH}) * 10000 + ${ROSE_VERSION_BUILD})")
else()
  set(ROSE_VERSION_BUILD 0)
  if(ROSE_VERSION_MINOR GREATER 99999)
    message(FATAL_ERROR "ROSE version minor component must be < 100000: ${ROSE_VERSION_MINOR}")
  endif()
  if(ROSE_VERSION_PATCH GREATER 99)
    message(FATAL_ERROR "ROSE version patch component must be < 100: ${ROSE_VERSION_PATCH}")
  endif()
  math(EXPR ROSE_VERSION_ENCODED
    "${ROSE_VERSION_MAJOR} * 10000000 + ${ROSE_VERSION_MINOR} * 100 + ${ROSE_VERSION_PATCH}")
endif()

file(READ "${ROSE_SCM_DATE_FILE}" ROSE_VERSION_FROM_SCM_DATE)
string(STRIP "${ROSE_VERSION_FROM_SCM_DATE}" ROSE_VERSION_FROM_SCM_DATE)
if(ROSE_VERSION_FROM_SCM_DATE STREQUAL "")
  message(FATAL_ERROR "ROSE SCM Date file is empty or contains only whitespace: ${ROSE_SCM_DATE_FILE}")
endif()
string(REGEX REPLACE "^0+" "" ROSE_VERSION_FROM_SCM_DATE "${ROSE_VERSION_FROM_SCM_DATE}")
if(ROSE_VERSION_FROM_SCM_DATE STREQUAL "")
  set(ROSE_VERSION_FROM_SCM_DATE "0")
endif()
if(NOT ROSE_VERSION_FROM_SCM_DATE STREQUAL ROSE_VERSION_ENCODED)
  message(FATAL_ERROR
    "ROSE SCM Date file ${ROSE_SCM_DATE_FILE} (${ROSE_VERSION_FROM_SCM_DATE}) does not match "
    "ROSE_VERSION (${ROSE_PACKAGE_VERSION}); expected ${ROSE_VERSION_ENCODED}")
endif()
set(ROSE_VERSION_INT "${ROSE_VERSION_FROM_SCM_DATE}")
set(ROSE_VERSION "${ROSE_VERSION_INT}UL")

message(STATUS "The ROSE version integer is ${ROSE_VERSION_INT}")
message(STATUS "The ROSE version string is ${ROSE_PACKAGE_VERSION}")

# Paths to install header, executable, and libraries
set(INCLUDE_INSTALL_DIR "include/rose")
set(INCLUDE_INSTALL_DIR_FOR_INTRINSICS "include")
set(BIN_INSTALL_DIR     "bin")
set(LIB_INSTALL_DIR     "${ROSE_LIB_DIR_NAME}")
set(CLANG_BUILTIN_HEADERS_INSTALL_DIR "${INCLUDE_INSTALL_DIR_FOR_INTRINSICS}/clang")

# Generate version file for package configuration
include(CMakePackageConfigHelpers)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cmake")
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/RoseConfigVersion.cmake
  VERSION ${ROSE_PACKAGE_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/RoseConfigVersion.cmake
  DESTINATION ${LIB_INSTALL_DIR}/cmake/Rose
)

# Compiler toolchain features

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -DHAVE_CONFIG_H -fPIC ")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions -DHAVE_CONFIG_H -fPIC ")

  # Ensure C++11 ABI compatibility
  if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")
  endif()

# Analyzable languages supported by ROSE
# C/C++
option(ENABLE-C "Enable C/C++ analysis" ON)
if(REX_ENABLE_WASM)
  set(ENABLE-C ON CACHE BOOL "Enable C/C++ analysis" FORCE)
endif()
if(ENABLE-C)
  set(ROSE_BUILD_CXX_LANGUAGE_SUPPORT 1)
  set(ROSE_BUILD_C_LANGUAGE_SUPPORT 1)
endif()

# CUDA
option(ENABLE-CUDA "Enable CUDA analysis" OFF)
if(REX_ENABLE_WASM)
  set(ENABLE-CUDA OFF CACHE BOOL "Enable CUDA analysis" FORCE)
endif()
if(ENABLE-CUDA)
  if(NOT ENABLE-C)
    message(FATAL_ERROR "ENABLE-CUDA requires ENABLE-C for the Clang frontend")
  endif()
  find_package(CUDAToolkit REQUIRED)

  # Clang's CUDA runtime wrapper includes headers supplied by the CUDA math
  # libraries. Permit an exact toolchain path for installations whose imported
  # CMake targets do not publish all of those headers (for example, a split
  # vendor SDK). Otherwise search only the include interface advertised by the
  # official CUDA::curand target; never guess a vendor-specific layout.
  set(ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR "" CACHE PATH
    "Directory containing curand_mtgp32_kernel.h for Clang's CUDA runtime wrapper")
  if(ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR)
    if(NOT IS_ABSOLUTE "${ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR}")
      message(FATAL_ERROR
        "ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR must be an absolute path: "
        "${ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR}")
    endif()
    if(NOT EXISTS
        "${ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR}/curand_mtgp32_kernel.h")
      message(FATAL_ERROR
        "ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR does not contain the header "
        "required by Clang's CUDA runtime wrapper: "
        "${ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR}/curand_mtgp32_kernel.h")
    endif()
  else()
    if(NOT TARGET CUDA::curand)
      message(FATAL_ERROR
        "CUDAToolkit did not provide the required CUDA::curand target")
    endif()
    get_target_property(_rose_cuda_curand_include_dirs
      CUDA::curand INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _rose_cuda_curand_include_dirs)
      message(FATAL_ERROR
        "CUDA::curand does not publish any interface include directories")
    endif()
    find_path(_rose_cuda_runtime_include_dir
      NAMES curand_mtgp32_kernel.h
      PATHS ${_rose_cuda_curand_include_dirs}
      NO_DEFAULT_PATH)
    if(NOT _rose_cuda_runtime_include_dir)
      message(FATAL_ERROR
        "CUDA::curand does not publish an interface include directory "
        "containing curand_mtgp32_kernel.h: "
        "${_rose_cuda_curand_include_dirs}")
    endif()
    set(ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR
      "${_rose_cuda_runtime_include_dir}")
    unset(_rose_cuda_runtime_include_dir CACHE)
    unset(_rose_cuda_curand_include_dirs)
  endif()

  message(STATUS
    "Using Clang CUDA runtime dependency headers: ${ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR}")
  set(ROSE_BUILD_CUDA_LANGUAGE_SUPPORT 1)
else()
  unset(ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR CACHE)
endif()

if(NOT REX_ENABLE_WASM)
  include(cmake/rex_zlib.cmake)
endif()

# Fortran (disabled by default to avoid requiring a Fortran toolchain for basic builds)
option(ENABLE-FORTRAN "Enable Fortran analysis." OFF)

if(REX_ENABLE_WASM)
  set(ENABLE-FORTRAN OFF CACHE BOOL "Enable Fortran analysis." FORCE)
endif()
set(FLANG_ROOT "" CACHE PATH "Path to Flang installation (lib/, include/).")

if(ENABLE-FORTRAN AND DEFINED ENABLE-FORTRAN-FLANG AND
   NOT ENABLE-FORTRAN-FLANG)
  message(FATAL_ERROR
    "Fortran analysis uses Flang; disable Fortran with -DENABLE-FORTRAN=OFF.")
endif()

rex_llvm_package_hints("${CMAKE_C_COMPILER}" _ROSE_LLVM_PACKAGE_HINTS)
if(ENABLE-C)
  find_package(Clang REQUIRED CONFIG HINTS ${_ROSE_LLVM_PACKAGE_HINTS})
elseif(ENABLE-FORTRAN)
  find_package(LLVM REQUIRED CONFIG HINTS ${_ROSE_LLVM_PACKAGE_HINTS})
endif()
unset(_ROSE_LLVM_PACKAGE_HINTS)
if(ENABLE-C OR ENABLE-FORTRAN)
  if(NOT DEFINED LLVM_VERSION_MAJOR OR
     NOT "${LLVM_VERSION_MAJOR}" STREQUAL "${LLVM_REQUIRED_MAJOR}")
    message(FATAL_ERROR
      "REX requires exactly LLVM ${LLVM_REQUIRED_MAJOR}. Found LLVM '${LLVM_VERSION_MAJOR}'.")
  endif()
endif()

if(ENABLE-FORTRAN)
  set(_flang_search_dirs "")
  set(_flang_include_candidates "")
  list(APPEND _flang_search_dirs ${LLVM_LIBRARY_DIRS})
  list(APPEND _flang_include_candidates ${LLVM_INCLUDE_DIRS})
  if(FLANG_ROOT)
    if(NOT IS_ABSOLUTE "${FLANG_ROOT}")
      message(FATAL_ERROR "FLANG_ROOT must be an absolute path: ${FLANG_ROOT}")
    endif()
    get_filename_component(_flang_root_real "${FLANG_ROOT}" REALPATH)
    set(_flang_root_matches_llvm FALSE)
    foreach(_llvm_library_dir IN LISTS LLVM_LIBRARY_DIRS)
      if(EXISTS "${_llvm_library_dir}")
        get_filename_component(_llvm_library_dir_real
          "${_llvm_library_dir}" REALPATH)
        get_filename_component(_llvm_prefix_from_lib
          "${_llvm_library_dir_real}/.." REALPATH)
        if(_flang_root_real STREQUAL _llvm_prefix_from_lib)
          set(_flang_root_matches_llvm TRUE)
        endif()
      endif()
    endforeach()
    if(NOT _flang_root_matches_llvm)
      message(FATAL_ERROR
        "FLANG_ROOT must be the same installation prefix as the loaded "
        "LLVMConfig.cmake. Got '${FLANG_ROOT}'; LLVM library directories are "
        "'${LLVM_LIBRARY_DIRS}'.")
    endif()
    unset(_flang_root_matches_llvm)
    unset(_flang_root_real)
    unset(_llvm_library_dir)
    unset(_llvm_library_dir_real)
    unset(_llvm_prefix_from_lib)
  endif()
  list(REMOVE_DUPLICATES _flang_search_dirs)
  if(NOT _flang_search_dirs)
    message(FATAL_ERROR
      "No Flang library search directories were provided by FLANG_ROOT or LLVM_LIBRARY_DIRS")
  endif()

  list(REMOVE_DUPLICATES _flang_include_candidates)
  if(NOT _flang_include_candidates)
    message(FATAL_ERROR
      "No Flang include directories were provided by FLANG_ROOT or LLVM_INCLUDE_DIRS")
  endif()

  include("${CMAKE_SOURCE_DIR}/cmake/rex_flang_frontend_manifest.cmake")
  if(NOT REX_FLANG_FRONTEND_LIBRARIES OR NOT REX_FLANG_FRONTEND_HEADERS OR
     NOT REX_FLANG_INTRINSIC_FILES)
    message(FATAL_ERROR
      "REX Flang frontend manifest must declare libraries, headers, and "
      "intrinsic module files")
  endif()
  set(FLANG_INCLUDE_DIRS "")
  foreach(_flang_include_candidate IN LISTS _flang_include_candidates)
    set(_flang_candidate_complete TRUE)
    foreach(_flang_required_header IN LISTS REX_FLANG_FRONTEND_HEADERS)
      if(NOT EXISTS
          "${_flang_include_candidate}/${_flang_required_header}" OR
         IS_DIRECTORY
          "${_flang_include_candidate}/${_flang_required_header}")
        set(_flang_candidate_complete FALSE)
        break()
      endif()
    endforeach()
    if(_flang_candidate_complete)
      set(FLANG_INCLUDE_DIRS "${_flang_include_candidate}")
      break()
    endif()
  endforeach()
  if(NOT FLANG_INCLUDE_DIRS)
    message(FATAL_ERROR
      "No single Flang include directory contains all required frontend "
      "headers: ${_flang_include_candidates}")
  endif()
  set(FLANG_INCLUDE_DIRS "${FLANG_INCLUDE_DIRS}" CACHE INTERNAL
      "Flang include dirs" FORCE)
  unset(_flang_candidate_complete)
  unset(_flang_include_candidate)
  unset(_flang_include_candidates)
  unset(_flang_required_header)

  # REX consumes Flang's parsing, semantic, and expression-evaluation layers.
  # Do not import or link the lowering layer, whose public interface requires
  # FIR/MLIR components that the REX frontend does not use.
  set(FLANG_LIBS "")
  set(_flang_evaluate_library_path "")
  set(_flang_missing "")
  foreach(_flang_lib IN LISTS REX_FLANG_FRONTEND_LIBRARIES)
    unset(_flang_lib_path)
    unset(_flang_lib_path CACHE)
    find_library(_flang_lib_path NAMES ${_flang_lib}
      PATHS ${_flang_search_dirs} NO_DEFAULT_PATH)
    if(_flang_lib_path)
      list(APPEND FLANG_LIBS "${_flang_lib_path}")
      if(_flang_lib STREQUAL "FortranEvaluate")
        set(_flang_evaluate_library_path "${_flang_lib_path}")
      endif()
    else()
      list(APPEND _flang_missing ${_flang_lib})
    endif()
  endforeach()
  if(_flang_missing)
    message(FATAL_ERROR
      "Required Flang libraries were not found in ${_flang_search_dirs}: ${_flang_missing}")
  endif()
  if(NOT _flang_evaluate_library_path)
    message(FATAL_ERROR
      "The required FortranEvaluate library has no resolved path")
  endif()
  if(NOT CMAKE_NM)
    message(FATAL_ERROR
      "Cannot inspect the selected FortranEvaluate library's system dependencies because CMake did not identify an nm tool")
  endif()
  # EXT-FLANG-FRONTEND-EXPORTS: docs/codebase-audit/external-exceptions.md.
  # Flang's exports require unrelated MLIR targets. Inspect the selected
  # Evaluate archive: its sinq reference identifies the configured libquadmath
  # dependency. Recheck this boundary when LLVM's package exports change.
  execute_process(
    COMMAND "${CMAKE_NM}" --undefined-only -j
      "${_flang_evaluate_library_path}"
    RESULT_VARIABLE _flang_nm_status
    OUTPUT_VARIABLE _flang_evaluate_undefined_symbols
    ERROR_VARIABLE _flang_nm_error)
  if(NOT _flang_nm_status EQUAL 0)
    message(FATAL_ERROR
      "Failed to inspect the selected FortranEvaluate library '${_flang_evaluate_library_path}' with '${CMAKE_NM}': ${_flang_nm_error}")
  endif()
  string(REPLACE "\n" ";" _flang_evaluate_undefined_symbols
    "${_flang_evaluate_undefined_symbols}")
  if("sinq" IN_LIST _flang_evaluate_undefined_symbols OR
     "_sinq" IN_LIST _flang_evaluate_undefined_symbols)
    if(NOT CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
      message(FATAL_ERROR
        "Cannot locate required Flang dependency libquadmath because the C++ compiler reported no implicit link directories")
    endif()
    unset(_flang_quadmath_path CACHE)
    find_library(_flang_quadmath_path
      NAMES quadmath
      PATHS ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}
      NO_DEFAULT_PATH)
    if(NOT _flang_quadmath_path)
      message(FATAL_ERROR
        "The selected FortranEvaluate library requires libquadmath, but it was not found in the C++ compiler's implicit link directories: "
        "${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}")
    endif()
    list(APPEND FLANG_LIBS "${_flang_quadmath_path}")
    message(STATUS
      "Using required Flang quadmath dependency: ${_flang_quadmath_path}")
    unset(_flang_quadmath_path CACHE)
  else()
    message(STATUS
      "The selected FortranEvaluate library does not require libquadmath")
  endif()
  unset(_flang_evaluate_library_path)
  unset(_flang_evaluate_undefined_symbols)
  unset(_flang_lib_path)
  unset(_flang_lib_path CACHE)
  unset(_flang_missing)
  unset(_flang_nm_error)
  unset(_flang_nm_status)
  unset(_flang_search_dirs)

  add_compile_definitions(ROSE_FLANG_FRONTEND)

  if(NOT ENABLE-C AND (NOT DEFINED LLVM_LIBS OR "${LLVM_LIBS}" STREQUAL ""))
    if("LLVM" IN_LIST LLVM_AVAILABLE_LIBS)
      set(LLVM_LIBS LLVM)
      message(STATUS "Using shared LLVM library (Fortran-only): ${LLVM_LIBS}")
    else()
      set(LLVM_LINK_LLVM_DYLIB ON)
      llvm_map_components_to_libnames(LLVM_LIBS
        support core irreader
        option mc mcparser
        binaryformat bitreader bitwriter
        profiledata
        target targetparser
        frontendopenmp)
      message(STATUS "Using LLVM component libraries (Fortran-only): ${LLVM_LIBS}")
    endif()
  endif()
endif()

# Fortran
if(ENABLE-FORTRAN)
  detect_compiler(CMAKE_Fortran_COMPILER FC "Fortran" "flang-${LLVM_REQUIRED_MAJOR}" "flang")
  set(ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT 1)
  enable_language(Fortran)
  set(BACKEND_FORTRAN_COMPILER "${CMAKE_Fortran_COMPILER}")
  if(NOT "${CMAKE_Fortran_COMPILER_ID}" MATCHES "^(LLVMFlang|Flang)$")
    message(FATAL_ERROR "REX requires LLVM Flang (flang) as the Fortran backend. Found: ${CMAKE_Fortran_COMPILER_ID} (${BACKEND_FORTRAN_COMPILER}).")
  endif()
  if(NOT CMAKE_Fortran_COMPILER_VERSION MATCHES "^${LLVM_REQUIRED_MAJOR}([.]|$)")
    message(FATAL_ERROR
      "REX requires exactly LLVM Flang ${LLVM_REQUIRED_MAJOR} as the Fortran backend. "
      "Found ${CMAKE_Fortran_COMPILER_VERSION} (${BACKEND_FORTRAN_COMPILER}).")
  endif()
  if(NOT LLVM_TOOLS_BINARY_DIR OR
     NOT IS_ABSOLUTE "${LLVM_TOOLS_BINARY_DIR}")
    message(FATAL_ERROR
      "LLVMConfig.cmake must publish an absolute LLVM_TOOLS_BINARY_DIR for the Flang toolchain contract.")
  endif()
  get_filename_component(_rose_flang_compiler_real
    "${BACKEND_FORTRAN_COMPILER}" REALPATH)
  get_filename_component(_rose_flang_compiler_dir
    "${_rose_flang_compiler_real}" DIRECTORY)
  get_filename_component(_rose_llvm_tools_real
    "${LLVM_TOOLS_BINARY_DIR}" REALPATH)
  if(NOT "${_rose_flang_compiler_dir}" STREQUAL
     "${_rose_llvm_tools_real}")
    message(FATAL_ERROR
      "The backend Flang compiler and LLVM/Flang frontend libraries must come from one installation. "
      "Got compiler '${_rose_flang_compiler_real}' and LLVM tools '${_rose_llvm_tools_real}'.")
  endif()
  unset(_rose_flang_compiler_dir)
  unset(_rose_flang_compiler_real)
  unset(_rose_llvm_tools_real)
  message(STATUS "Using LLVM Flang backend: ${BACKEND_FORTRAN_COMPILER}")

  # Flang installs omp_lib.h and its intrinsic modules in the target-specific
  # resource directory <resource-dir>/finclude/flang/<triple>, which its driver
  # passes through -fintrinsic-modules-path. Derive both components from the
  # selected backend driver so the frontend parses against the same modules.
  foreach(_rose_flang_query IN ITEMS resource-dir target-triple)
    execute_process(
      COMMAND "${BACKEND_FORTRAN_COMPILER}" "-print-${_rose_flang_query}"
      RESULT_VARIABLE _rose_flang_query_status
      OUTPUT_VARIABLE _rose_flang_query_output
      ERROR_VARIABLE _rose_flang_query_error
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _rose_flang_query_status EQUAL 0 OR
       NOT _rose_flang_query_output OR
       _rose_flang_query_output MATCHES "[\n]")
      message(FATAL_ERROR
        "The backend Flang compiler '${BACKEND_FORTRAN_COMPILER}' did not report "
        "exactly one -print-${_rose_flang_query} value: "
        "${_rose_flang_query_output}${_rose_flang_query_error}")
    endif()
    string(MAKE_C_IDENTIFIER "${_rose_flang_query}" _rose_flang_query_var)
    set(_rose_flang_${_rose_flang_query_var} "${_rose_flang_query_output}")
  endforeach()
  if(NOT IS_ABSOLUTE "${_rose_flang_resource_dir}")
    message(FATAL_ERROR
      "The backend Flang compiler reported a non-absolute resource directory: "
      "${_rose_flang_resource_dir}")
  endif()
  set(FLANG_INTRINSIC_INCLUDE_DIR
      "${_rose_flang_resource_dir}/finclude/flang/${_rose_flang_target_triple}")
  foreach(_rose_flang_intrinsic_file IN LISTS REX_FLANG_INTRINSIC_FILES)
    if(NOT EXISTS "${FLANG_INTRINSIC_INCLUDE_DIR}/${_rose_flang_intrinsic_file}" OR
       IS_DIRECTORY "${FLANG_INTRINSIC_INCLUDE_DIR}/${_rose_flang_intrinsic_file}")
      message(FATAL_ERROR
        "The Flang intrinsic module directory '${FLANG_INTRINSIC_INCLUDE_DIR}' "
        "is missing required file ${_rose_flang_intrinsic_file}. Install the "
        "complete flang-${LLVM_REQUIRED_MAJOR} package.")
    endif()
  endforeach()
  set(FLANG_INTRINSIC_INCLUDE_DIR "${FLANG_INTRINSIC_INCLUDE_DIR}" CACHE INTERNAL
      "Flang intrinsic module/header include dir" FORCE)
  message(STATUS "Using Flang intrinsic modules: ${FLANG_INTRINSIC_INCLUDE_DIR}")
  unset(_rose_flang_intrinsic_file)
  unset(_rose_flang_query)
  unset(_rose_flang_query_error)
  unset(_rose_flang_query_output)
  unset(_rose_flang_query_status)
  unset(_rose_flang_query_var)
  unset(_rose_flang_resource_dir)
  unset(_rose_flang_target_triple)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/rex_linker.cmake")
rex_configure_linker()

# The C preprocessor is enabled automatically if C, C++, or Fortran is enabled
if(ENABLE-C OR ENABLE-FORTRAN)
  set(ENABLE-CPP ON)
  set(ROSE_BUILD_CPP_LANGUAGE_SUPPORT 1)
endif()

# Clang/LLVM frontend (required when C/C++ support is enabled)

if(ENABLE-C)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

# ClangConfig loads an exact matching LLVMConfig.  Derive the frontend version
# only from that typed package contract; a cached/user-provided
# CLANG_VERSION_MAJOR or a PATH probe must never mask a mismatched package.
set(CLANG_VERSION_MAJOR "${LLVM_VERSION_MAJOR}")

# An explicit LLVM_DIR can otherwise override the exact hint embedded in
# ClangConfig.cmake and silently combine Clang and LLVM packages from distinct
# installations of the same version. Validate the directories containing the
# loaded package configs themselves: LLVM_CMAKE_DIR is an internal modules
# directory and points into the LLVM source tree for build-tree packages.
if(NOT Clang_DIR OR NOT IS_ABSOLUTE "${Clang_DIR}" OR
   NOT LLVM_DIR OR NOT IS_ABSOLUTE "${LLVM_DIR}" OR
   NOT EXISTS "${Clang_DIR}/ClangConfig.cmake" OR
   NOT EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
  message(FATAL_ERROR
    "ClangConfig.cmake and LLVMConfig.cmake must be loaded from absolute package directories. "
    "Got Clang '${Clang_DIR}' and LLVM '${LLVM_DIR}'.")
endif()
get_filename_component(_rose_clang_config_real
  "${Clang_DIR}/ClangConfig.cmake" REALPATH)
get_filename_component(_rose_llvm_config_real
  "${LLVM_DIR}/LLVMConfig.cmake" REALPATH)
get_filename_component(_rose_clang_package_dir
  "${_rose_clang_config_real}" DIRECTORY)
get_filename_component(_rose_llvm_package_dir
  "${_rose_llvm_config_real}" DIRECTORY)
get_filename_component(_rose_clang_package_root
  "${_rose_clang_package_dir}/../../.." REALPATH)
get_filename_component(_rose_llvm_package_root
  "${_rose_llvm_package_dir}/../../.." REALPATH)
if(NOT "${_rose_clang_package_root}" STREQUAL
   "${_rose_llvm_package_root}")
  message(FATAL_ERROR
    "Clang and LLVM package configs must come from one installation. "
    "Got Clang '${_rose_clang_package_root}' and LLVM '${_rose_llvm_package_root}'.")
endif()
set(ROSE_LLVM_PACKAGE_ROOT "${_rose_llvm_package_root}" CACHE INTERNAL
  "Canonical installation root containing the loaded LLVM and Clang packages" FORCE)
unset(_rose_clang_package_root)
unset(_rose_clang_package_dir)
unset(_rose_clang_config_real)
unset(_rose_llvm_package_root)
unset(_rose_llvm_package_dir)
unset(_rose_llvm_config_real)

# LLVM/Clang headers are added to the specific targets that need them.
# Avoid forcing LLVM include paths globally to prevent conflicts with
# the system C++ ABI headers when building third-party code (e.g., gtest).
#
# Clang frontend support requires shared LLVM to avoid duplicate global
# allocator instances and shutdown-time memory corruption from static linkage.
if(NOT REX_ENABLE_WASM AND NOT (UNIX AND NOT APPLE))
  message(FATAL_ERROR
    "REX Clang frontend currently supports Linux only.")
endif()

include(rex_imported_shared_library)
if(REX_ENABLE_WASM)
  set(LLVM_LIBS "")
  message(STATUS "Using static LLVM/Clang component targets for REX WASM")
else()
  rex_imported_shared_library(LLVM LLVM_LIBS)
  message(STATUS "Using required shared LLVM library for Clang frontend: ${LLVM_LIBS}")
endif()

# Clang libraries needed
if(REX_ENABLE_WASM)
  set(CLANG_LIBS
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
  foreach(_rose_wasm_clang_target IN LISTS CLANG_LIBS)
    if(NOT TARGET ${_rose_wasm_clang_target})
      message(FATAL_ERROR
        "The exact LLVM/Clang ${LLVM_REQUIRED_MAJOR} WASM package does not export required target "
        "${_rose_wasm_clang_target}")
    endif()
  endforeach()
  unset(_rose_wasm_clang_target)
else()
  rex_imported_shared_library(clang-cpp CLANG_LIBS)
  message(STATUS "Using required shared libclang-cpp: ${CLANG_LIBS}")
endif()

endif() # ENABLE-C

# System features

include(ConfigureChecks)

find_package(Perl REQUIRED)

# Miscellaneous user-selectable features

option(ENABLE-TESTS-DIRECTORY "Enable compilation and testing of the ROSE/tests directory" ON)
set(_rex_enable_testing_default "${ENABLE-TESTS-DIRECTORY}")
option(REX_ENABLE_TESTING "Enable REX tests (alias for ENABLE-TESTS-DIRECTORY)" ${_rex_enable_testing_default})
if(REX_ENABLE_WASM)
  set(REX_ENABLE_TESTING OFF CACHE BOOL "Enable REX tests (alias for ENABLE-TESTS-DIRECTORY)" FORCE)
  set(ENABLE-TESTS-DIRECTORY OFF CACHE BOOL "Enable compilation and testing of the ROSE/tests directory" FORCE)
endif()
set(ENABLE-TESTS-DIRECTORY "${REX_ENABLE_TESTING}" CACHE BOOL "Enable compilation and testing of the ROSE/tests directory" FORCE)
set(BUILD_TESTING "${ENABLE-TESTS-DIRECTORY}" CACHE BOOL "Build the testing tree." FORCE)
set(_rex_enable_example_translators_default "${ENABLE-TESTS-DIRECTORY}")
option(ENABLE-EXAMPLE-TRANSLATORS-DIRECTORY "Enable compilation and testing of exampleTranslators directory" ${_rex_enable_example_translators_default})
if(REX_ENABLE_WASM)
  set(ENABLE-EXAMPLE-TRANSLATORS-DIRECTORY OFF CACHE BOOL "Enable compilation and testing of exampleTranslators directory" FORCE)
endif()
if(ENABLE-TESTS-DIRECTORY AND NOT ENABLE-EXAMPLE-TRANSLATORS-DIRECTORY)
  message(FATAL_ERROR
    "ENABLE-TESTS-DIRECTORY=ON requires ENABLE-EXAMPLE-TRANSLATORS-DIRECTORY=ON because tests depend on translator executables. "
    "Reconfigure with -DENABLE-EXAMPLE-TRANSLATORS-DIRECTORY=ON or disable tests.")
endif()
if(ENABLE-TESTS-DIRECTORY)
  enable_testing()
  add_test(
    NAME rex_no_autotools
    COMMAND ${CMAKE_COMMAND}
      -DREX_SOURCE_DIR=${CMAKE_SOURCE_DIR}
      -DREX_BUILD_DIR=${CMAKE_BINARY_DIR}
      -P ${CMAKE_SOURCE_DIR}/cmake/check_no_autotools.cmake)
endif()
option(ENABLE-TUTORIAL-DIRECTORY "Enable compilation and testing of the ROSE/tutorial directory" OFF)

option(REX_WARNINGS_AS_ERRORS "Treat C/C++ compiler warnings as errors" ON)
if(REX_WARNINGS_AS_ERRORS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
     CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
      $<$<COMPILE_LANGUAGE:C>:-Werror>
      $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
  else()
    message(FATAL_ERROR "REX_WARNINGS_AS_ERRORS requested but compiler does not support warning-as-error flags")
  endif()
endif()

set(ROSE_SANITIZERS "" CACHE STRING "Semicolon-separated list of sanitizers to enable (address;leak;undefined)")
option(ENABLE-SANITIZER "Enable sanitizer instrumentation for ROSE development" OFF)
option(REX_EXTENDED_TEST_TIMEOUTS "Use extended timeouts for instrumented CTest configurations such as memcheck" OFF)
set(_rose_requested_sanitizers "")
if(ENABLE-SANITIZER)
  set(_rose_requested_sanitizers "${ROSE_SANITIZERS}")
  if(NOT _rose_requested_sanitizers)
    set(_rose_requested_sanitizers "address;leak")
    set(ROSE_SANITIZERS "${_rose_requested_sanitizers}" CACHE STRING
        "Semicolon-separated list of sanitizers to enable (address;leak;undefined)" FORCE)
  endif()
endif()
if(_rose_requested_sanitizers)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
     CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(REMOVE_DUPLICATES _rose_requested_sanitizers)
    list(JOIN _rose_requested_sanitizers "," _rose_sanitizer_list)
    set(_rose_sanitizer_link_flags "-fsanitize=${_rose_sanitizer_list}")
    set(_rose_sanitizer_compile_flags
        "${_rose_sanitizer_link_flags}"
        "-fno-omit-frame-pointer"
        "-fno-optimize-sibling-calls")
    function(_rex_check_sanitizer_support)
      include(CMakePushCheckState)
      include(CheckCSourceCompiles)
      include(CheckCXXSourceCompiles)
      set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
      cmake_push_check_state(RESET)
      set(CMAKE_REQUIRED_FLAGS "${_rose_sanitizer_link_flags}")
      set(CMAKE_REQUIRED_LINK_OPTIONS "${_rose_sanitizer_link_flags}")
      unset(ROSE_C_SUPPORTS_SANITIZERS CACHE)
      unset(ROSE_CXX_SUPPORTS_SANITIZERS CACHE)
      check_c_source_compiles("int main(void) { return 0; }" ROSE_C_SUPPORTS_SANITIZERS)
      check_cxx_source_compiles("int main() { return 0; }" ROSE_CXX_SUPPORTS_SANITIZERS)
      cmake_pop_check_state()
    endfunction()
    _rex_check_sanitizer_support()
    if(NOT ROSE_C_SUPPORTS_SANITIZERS OR NOT ROSE_CXX_SUPPORTS_SANITIZERS)
      message(FATAL_ERROR "Sanitizers requested but compiler does not support ${_rose_sanitizer_link_flags}")
    endif()
    foreach(_rose_sanitizer_flag IN LISTS _rose_sanitizer_compile_flags)
      add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:${_rose_sanitizer_flag}>
        $<$<COMPILE_LANGUAGE:CXX>:${_rose_sanitizer_flag}>)
    endforeach()
    add_link_options("$<$<LINK_LANGUAGE:C,CXX>:${_rose_sanitizer_link_flags}>")
    set(ROSE_USE_SANITIZER 1)
  else()
    message(FATAL_ERROR "Sanitizers requested but compiler does not support sanitizer flags")
  endif()
endif()
unset(_rose_requested_sanitizers)
unset(_rose_sanitizer_list)
unset(_rose_sanitizer_compile_flags)
unset(_rose_sanitizer_link_flags)

option(ENABLE-LCOV "Support lcov coverage instrumentation for ROSE development" OFF)
if(ENABLE-LCOV)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "ENABLE-LCOV requires a GNU C++ build compiler")
  endif()
  foreach(_rose_lcov_flag IN ITEMS -g -fprofile-arcs -ftest-coverage)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:${_rose_lcov_flag}>")
  endforeach()
  unset(_rose_lcov_flag)
  add_link_options("$<$<LINK_LANGUAGE:C,CXX>:--coverage>")
endif()

set(ROSE_ALLOC_MEMSET "0" CACHE STRING
    "Memory pool protection: 0=none, 2=zero new/delete, 3=distinct allocation/free fills")
set_property(CACHE ROSE_ALLOC_MEMSET PROPERTY STRINGS 0 2 3)
if(NOT ROSE_ALLOC_MEMSET MATCHES "^[023]$")
  message(FATAL_ERROR "ROSE_ALLOC_MEMSET must be one of 0, 2, or 3. Current value: ${ROSE_ALLOC_MEMSET}")
endif()

set(WITH-VALGRIND "" CACHE PATH "Valgrind installation prefix (empty to autodetect)")
set(WITH-VALGRIND-BIN "" CACHE PATH "Valgrind bin directory")
set(WITH-VALGRIND-INCLUDE "" CACHE PATH "Valgrind include directory")
set(WITH-LLVM-OMP-RUNTIME-LIBRARY "" CACHE PATH "LLVM OpenMP runtime library directory")

include(cmake/rex_bison.cmake)

set(VALGRIND_BIN_PATH "")
set(VALGRIND_INCLUDE_PATH "")

set(_rose_with_vars
  WITH-VALGRIND
  WITH-VALGRIND-BIN
  WITH-VALGRIND-INCLUDE
  WITH-LLVM-OMP-RUNTIME-LIBRARY)
foreach(_rose_var IN LISTS _rose_with_vars)
  if("${${_rose_var}}" STREQUAL "no" OR "${${_rose_var}}" STREQUAL "OFF")
    set(${_rose_var} "")
  endif()
endforeach()
unset(_rose_with_vars)
unset(_rose_var)

if(WITH-VALGRIND-LIB)
  message(FATAL_ERROR
    "WITH-VALGRIND-LIB is unsupported: REX uses Valgrind headers "
    "and its executable, not a library.")
endif()

if(DEFINED WITH-GOMP-OMP-RUNTIME-LIBRARY AND
   NOT "${WITH-GOMP-OMP-RUNTIME-LIBRARY}" STREQUAL "")
  message(FATAL_ERROR
    "WITH-GOMP-OMP-RUNTIME-LIBRARY is no longer supported. "
    "REX uses LLVM OpenMP runtime only.")
endif()

if(DEFINED WITH-OMNI-OMP-RUNTIME-SUPPORT AND
   NOT "${WITH-OMNI-OMP-RUNTIME-SUPPORT}" STREQUAL "")
  message(FATAL_ERROR
    "WITH-OMNI-OMP-RUNTIME-SUPPORT is no longer supported. "
    "REX uses LLVM OpenMP runtime only.")
endif()

if(WITH-VALGRIND)
  set(VALGRIND_BIN_PATH "${WITH-VALGRIND}/bin")
  set(VALGRIND_INCLUDE_PATH "${WITH-VALGRIND}/include")
endif()
if(WITH-VALGRIND-BIN)
  set(VALGRIND_BIN_PATH "${WITH-VALGRIND-BIN}")
endif()
if(WITH-VALGRIND-INCLUDE)
  set(VALGRIND_INCLUDE_PATH "${WITH-VALGRIND-INCLUDE}")
endif()
set(ROSE_VALGRIND_REQUESTED OFF)
if(WITH-VALGRIND OR WITH-VALGRIND-BIN OR WITH-VALGRIND-INCLUDE)
  set(ROSE_VALGRIND_REQUESTED ON)
endif()

if(VALGRIND_BIN_PATH)
  set(_rose_valgrind_candidate "${VALGRIND_BIN_PATH}/valgrind")
  if(NOT EXISTS "${_rose_valgrind_candidate}" OR
     IS_DIRECTORY "${_rose_valgrind_candidate}")
    message(FATAL_ERROR
      "The explicitly selected Valgrind directory has no binary: '${_rose_valgrind_candidate}'.")
  endif()
  set(VALGRIND_BINARY "${_rose_valgrind_candidate}")
  unset(_rose_valgrind_candidate)
else()
  unset(VALGRIND_BINARY)
  unset(VALGRIND_BINARY CACHE)
  find_program(VALGRIND_BINARY valgrind NO_CMAKE_FIND_ROOT_PATH)
endif()
if(ROSE_VALGRIND_REQUESTED AND NOT VALGRIND_BINARY)
  message(FATAL_ERROR "Valgrind binary is missing, can't configure with Valgrind. Set WITH-VALGRIND or WITH-VALGRIND-BIN.")
endif()
if(VALGRIND_BINARY)
  execute_process(
    COMMAND "${VALGRIND_BINARY}" --version
    RESULT_VARIABLE _rose_valgrind_status
    OUTPUT_VARIABLE _rose_valgrind_version
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _rose_valgrind_status EQUAL 0 OR
     NOT _rose_valgrind_version MATCHES "^valgrind-[0-9]+([.][0-9]+)+([.]GIT)?$")
    message(FATAL_ERROR
      "Selected Valgrind binary has no valid --version result: '${VALGRIND_BINARY}'.")
  endif()
  message(STATUS "Valgrind version: ${_rose_valgrind_version}")
endif()
if(VALGRIND_BINARY AND NOT VALGRIND_INCLUDE_PATH)
  unset(VALGRIND_INCLUDE_PATH CACHE)
  unset(VALGRIND_INCLUDE_PATH)
  find_path(VALGRIND_INCLUDE_PATH valgrind/valgrind.h
    NO_CMAKE_FIND_ROOT_PATH)
endif()
if(ROSE_VALGRIND_REQUESTED AND NOT VALGRIND_INCLUDE_PATH)
  message(FATAL_ERROR "Valgrind headers not found. Set WITH-VALGRIND or WITH-VALGRIND-INCLUDE.")
endif()
if(VALGRIND_INCLUDE_PATH AND
   (NOT EXISTS "${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h" OR
    IS_DIRECTORY "${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h"))
  message(FATAL_ERROR
    "Valgrind header must be a file: '${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h'")
endif()
set(ROSE_USE_VALGRIND 0)
if(VALGRIND_BINARY AND VALGRIND_INCLUDE_PATH)
  set(ROSE_USE_VALGRIND 1)
  if(NOT ROSE_VALGRIND_REQUESTED)
    message(STATUS "Auto-enabled Valgrind support: ${VALGRIND_BINARY}")
  endif()
endif()
include(cmake/rex_uninitialized_field_tests.cmake)


set(ROSE_LLVM_OPENMP_RUNTIME_DIR "")
set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "")
set(ROSE_LLVM_OPENMP_INCLUDE_DIR "")
set(ROSE_LLVM_CLANG_C_DRIVER "")
set(ROSE_LLVM_CLANG_CXX_DRIVER "")
if(REX_ENABLE_WASM AND ENABLE-C)
  set(ROSE_LLVM_OPENMP_INCLUDE_DIR "/rex/lib/clang/${CLANG_VERSION_MAJOR}/include")
  set(LLVM_OPENMP_INCLUDE_PATH "${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
  set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${ROSE_LLVM_OPENMP_INCLUDE_DIR}" CACHE INTERNAL
    "LLVM OpenMP header directory used by REX WASM")
  set(ROSE_LLVM_OPENMP_RUNTIME_DIR "" CACHE INTERNAL
    "Directory containing the LLVM OpenMP runtime library used by REX")
  set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "" CACHE INTERNAL
    "LLVM OpenMP runtime library selected by REX")
elseif(ENABLE-C)
  function(rex_configure_native_llvm_openmp)
    rex_require_llvm_driver(clang _rose_llvm_openmp_driver)
    file(REAL_PATH "${_rose_llvm_openmp_driver}" _rose_llvm_openmp_driver)
    set(ROSE_LLVM_CLANG_C_DRIVER "${_rose_llvm_openmp_driver}")
    set(ROSE_LLVM_CLANG_C_DRIVER
      "${ROSE_LLVM_CLANG_C_DRIVER}" CACHE INTERNAL
      "Exact Clang C driver owned by the loaded LLVM package" FORCE)
    rex_require_llvm_driver(clang++ _rose_llvm_clang_cxx_driver)

    rex_require_clang_resource_directory("${_rose_llvm_openmp_driver}"
      _rose_clang_resource_dir)

    # Backend drivers must own the frontend's resource headers; matching the
    # major version alone does not establish that they share an installation.
    function(_rose_validate_backend_clang_resource _rose_language)
      if(NOT CMAKE_${_rose_language}_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR
          "REX C/C++ final compilation requires Clang ${CLANG_VERSION_MAJOR} "
          "from the loaded LLVM/Clang installation. The configured "
          "${_rose_language} compiler is '${CMAKE_${_rose_language}_COMPILER}' "
          "with compiler ID '${CMAKE_${_rose_language}_COMPILER_ID}'.")
      endif()
      if(NOT CMAKE_${_rose_language}_COMPILER_VERSION MATCHES
         "^${CLANG_VERSION_MAJOR}([.][0-9]+)*$")
        message(FATAL_ERROR
          "REX C/C++ final compilation requires Clang ${CLANG_VERSION_MAJOR}. "
          "The configured ${_rose_language} compiler "
          "'${CMAKE_${_rose_language}_COMPILER}' reports version "
          "'${CMAKE_${_rose_language}_COMPILER_VERSION}'.")
      endif()
      rex_require_clang_resource_directory("${CMAKE_${_rose_language}_COMPILER}"
        _rose_backend_clang_resource_dir)
      if(NOT _rose_backend_clang_resource_dir STREQUAL
         _rose_clang_resource_dir)
        message(FATAL_ERROR
          "The configured ${_rose_language} backend compiler and loaded "
          "LLVM/Clang frontend package must own the same resource directory. "
          "Backend '${CMAKE_${_rose_language}_COMPILER}' owns "
          "'${_rose_backend_clang_resource_dir}', while the frontend package "
          "owns '${_rose_clang_resource_dir}'.")
      endif()
    endfunction()
    _rose_validate_backend_clang_resource(C)
    _rose_validate_backend_clang_resource(CXX)

    rex_require_clang_resource_directory("${_rose_llvm_clang_cxx_driver}"
      _rose_llvm_clang_cxx_resource_dir)
    if(NOT _rose_llvm_clang_cxx_resource_dir STREQUAL
       _rose_clang_resource_dir)
      message(FATAL_ERROR
        "The exact Clang C and C++ frontend drivers must publish one resource "
        "directory. Got C '${_rose_clang_resource_dir}' and C++ "
        "'${_rose_llvm_clang_cxx_resource_dir}'.")
    endif()
    set(ROSE_LLVM_CLANG_CXX_DRIVER "${_rose_llvm_clang_cxx_driver}")
    set(ROSE_LLVM_CLANG_CXX_DRIVER
      "${ROSE_LLVM_CLANG_CXX_DRIVER}" CACHE INTERNAL
      "Exact Clang C++ driver owned by the loaded LLVM package" FORCE)

    set(_rose_llvm_openmp_package_root "${ROSE_LLVM_PACKAGE_ROOT}")
    set(_rose_expected_resource_dirs
      "${_rose_llvm_openmp_package_root}/lib/clang/${CLANG_VERSION_MAJOR}"
      "${_rose_llvm_openmp_package_root}/lib64/clang/${CLANG_VERSION_MAJOR}")
    set(_rose_exact_resource_dir "")
    foreach(_rose_expected_resource_dir IN LISTS _rose_expected_resource_dirs)
      if(EXISTS "${_rose_expected_resource_dir}")
        file(REAL_PATH "${_rose_expected_resource_dir}"
          _rose_expected_resource_dir_real)
        if(_rose_clang_resource_dir STREQUAL
           _rose_expected_resource_dir_real)
          set(_rose_exact_resource_dir "${_rose_expected_resource_dir_real}")
          break()
        endif()
      endif()
    endforeach()
    if(NOT _rose_exact_resource_dir)
      string(REPLACE ";" "\n  " _rose_expected_resource_lines
        "${_rose_expected_resource_dirs}")
      message(FATAL_ERROR
        "Clang ${CLANG_VERSION_MAJOR} must report the resource directory owned by "
        "the loaded LLVM package root '${_rose_llvm_openmp_package_root}'. Got "
        "'${_rose_clang_resource_dir}'; expected one of:\n  "
        "${_rose_expected_resource_lines}")
    endif()
    set(ROSE_LLVM_OPENMP_INCLUDE_DIR
      "${_rose_exact_resource_dir}/include")
    if(NOT EXISTS "${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h")
      message(FATAL_ERROR
        "The exact Clang ${CLANG_VERSION_MAJOR} resource directory has no OpenMP "
        "header: '${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h'. Install the matching "
        "libomp-${CLANG_VERSION_MAJOR}-dev package.")
    endif()
    file(REAL_PATH "${ROSE_LLVM_OPENMP_INCLUDE_DIR}"
      ROSE_LLVM_OPENMP_INCLUDE_DIR)

    set(_rose_llvm_openmp_runtime_names
      libomp.so libomp.so.5 libomp5.so libiomp5.so libomp.a libiomp5.a)
    if(WITH-LLVM-OMP-RUNTIME-LIBRARY)
      if(NOT IS_ABSOLUTE "${WITH-LLVM-OMP-RUNTIME-LIBRARY}" OR
         NOT IS_DIRECTORY "${WITH-LLVM-OMP-RUNTIME-LIBRARY}")
        message(FATAL_ERROR
          "WITH-LLVM-OMP-RUNTIME-LIBRARY must name an existing absolute "
          "directory. Got '${WITH-LLVM-OMP-RUNTIME-LIBRARY}'.")
      endif()
      file(REAL_PATH "${WITH-LLVM-OMP-RUNTIME-LIBRARY}"
        _rose_explicit_openmp_runtime_dir)
      set(_rose_omp_runtime_dirs "${_rose_explicit_openmp_runtime_dir}")
    else()
      set(_rose_omp_runtime_dirs "${LLVM_LIBRARY_DIRS}")
      foreach(_rose_omp_runtime_dir IN LISTS _rose_omp_runtime_dirs)
        if(NOT IS_ABSOLUTE "${_rose_omp_runtime_dir}" OR
           NOT IS_DIRECTORY "${_rose_omp_runtime_dir}")
          message(FATAL_ERROR
            "LLVMConfig.cmake published an invalid LLVM library directory: "
            "'${_rose_omp_runtime_dir}'.")
        endif()
      endforeach()
    endif()
    foreach(_rose_omp_runtime_dir IN LISTS _rose_omp_runtime_dirs)
      foreach(_rose_omp_lib_name IN LISTS _rose_llvm_openmp_runtime_names)
        if(EXISTS "${_rose_omp_runtime_dir}/${_rose_omp_lib_name}")
          file(REAL_PATH
            "${_rose_omp_runtime_dir}/${_rose_omp_lib_name}"
            ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
          break()
        endif()
      endforeach()
      if(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
        break()
      endif()
    endforeach()
    if(WITH-LLVM-OMP-RUNTIME-LIBRARY)
      if(NOT ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
        message(FATAL_ERROR
          "The authoritative WITH-LLVM-OMP-RUNTIME-LIBRARY directory contains "
          "no LLVM OpenMP runtime: '${_rose_explicit_openmp_runtime_dir}'.")
      endif()
    elseif(NOT ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
      foreach(_rose_omp_probe_name IN LISTS _rose_llvm_openmp_runtime_names)
        execute_process(
          COMMAND "${_rose_llvm_openmp_driver}"
            -print-file-name=${_rose_omp_probe_name}
          OUTPUT_VARIABLE _rose_omp_probe_path
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_VARIABLE _rose_omp_probe_error
          RESULT_VARIABLE _rose_omp_probe_status)
        if(NOT _rose_omp_probe_status EQUAL 0 OR
           NOT _rose_omp_probe_path)
          message(FATAL_ERROR
            "Clang OpenMP runtime probe failed for '${_rose_omp_probe_name}': "
            "status '${_rose_omp_probe_status}', output "
            "'${_rose_omp_probe_path}', error '${_rose_omp_probe_error}'.")
        endif()
        file(TO_CMAKE_PATH "${_rose_omp_probe_path}"
          _rose_omp_probe_path_normalized)
        if(IS_ABSOLUTE "${_rose_omp_probe_path_normalized}" AND
           EXISTS "${_rose_omp_probe_path_normalized}")
          file(REAL_PATH "${_rose_omp_probe_path_normalized}"
            ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
          break()
        endif()
      endforeach()
      if(NOT ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
        message(FATAL_ERROR
          "The loaded LLVM package and its exact Clang driver provide no OpenMP "
          "runtime for LLVM/Clang ${CLANG_VERSION_MAJOR}. Install the matching "
          "libomp-${CLANG_VERSION_MAJOR}-dev package or set the authoritative "
          "WITH-LLVM-OMP-RUNTIME-LIBRARY directory.")
      endif()
    endif()

    get_filename_component(ROSE_LLVM_OPENMP_RUNTIME_DIR
      "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}" DIRECTORY)
    set(LLVM_OPENMP_INCLUDE_PATH "${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
    set(ROSE_LLVM_OPENMP_RUNTIME_DIR "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" CACHE INTERNAL
      "Directory containing the LLVM OpenMP runtime library used by REX")
    set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}" CACHE INTERNAL
      "LLVM OpenMP runtime library selected by REX")
    set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${ROSE_LLVM_OPENMP_INCLUDE_DIR}" CACHE INTERNAL
      "Directory containing the LLVM OpenMP header used by REX")
    message(STATUS "Using LLVM OpenMP runtime: ${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}")
    message(STATUS "Using LLVM OpenMP header dir: ${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
    foreach(output IN ITEMS
        ROSE_LLVM_CLANG_C_DRIVER ROSE_LLVM_CLANG_CXX_DRIVER
        ROSE_LLVM_OPENMP_RUNTIME_DIR ROSE_LLVM_OPENMP_RUNTIME_LIBRARY
        ROSE_LLVM_OPENMP_INCLUDE_DIR LLVM_OPENMP_INCLUDE_PATH)
      set(${output} "${${output}}" PARENT_SCOPE)
    endforeach()
  endfunction()
  rex_configure_native_llvm_openmp()
endif()

# Memory tracing consumes the final, package-validated driver paths.
include(cmake/rex_memcheck.cmake)

if(REX_ENABLE_WASM)
  set(REX_WASM_VIRTUAL_CLANG_C_DRIVER
    "/rex/bin/clang-${CLANG_VERSION_MAJOR}" CACHE INTERNAL
    "Exact virtual Clang C driver path used by the REX WASM frontend")
  set(REX_WASM_VIRTUAL_CLANG_CXX_DRIVER
    "/rex/bin/clang++-${CLANG_VERSION_MAJOR}" CACHE INTERNAL
    "Exact virtual Clang C++ driver path used by the REX WASM frontend")
  set(REX_WASM_VIRTUAL_CLANG_RESOURCE_DIR
    "/rex/lib/clang/${CLANG_VERSION_MAJOR}" CACHE INTERNAL
    "Exact virtual Clang resource directory used by the REX WASM frontend")

  set(REX_WASM_CLANG_RESOURCE_INPUT "" CACHE PATH
    "Exact configured Clang resource directory for the REX WASM package")
  set(REX_WASM_NATIVE_LLVM_ROOT "" CACHE PATH
    "Native LLVM installation supplying the OpenMP resource header")
  set(REX_WASM_OMP_INCLUDE_DIR "" CACHE PATH
    "Exact LLVM 23 resource include directory containing omp.h for the REX WASM package")
  option(REX_WASM_PACKAGE_SYSROOT
    "Package the Emscripten sysroot headers into the REX WASM virtual filesystem" FALSE)
  set(REX_WASM_SYSROOT "${EMSCRIPTEN_SYSROOT}" CACHE PATH
    "Optional Emscripten sysroot to package for the REX WASM browser frontend")

  if(DEFINED REX_WASM_CLANG_HEADERS_INPUT AND
     NOT "${REX_WASM_CLANG_HEADERS_INPUT}" STREQUAL "")
    message(FATAL_ERROR
      "REX_WASM_CLANG_HEADERS_INPUT was removed because source-tree header "
      "fallbacks produce an incomplete Clang resource directory. Configure "
      "REX_WASM_CLANG_RESOURCE_INPUT explicitly.")
  endif()
  if(NOT REX_WASM_CLANG_RESOURCE_INPUT)
    message(FATAL_ERROR
      "REX_ENABLE_WASM requires the exact staged LLVM ${LLVM_REQUIRED_MAJOR} "
      "Clang resource directory in REX_WASM_CLANG_RESOURCE_INPUT.")
  endif()
  if(NOT IS_ABSOLUTE "${REX_WASM_CLANG_RESOURCE_INPUT}")
    message(FATAL_ERROR
      "REX_WASM_CLANG_RESOURCE_INPUT must be absolute: "
      "${REX_WASM_CLANG_RESOURCE_INPUT}")
  endif()
  if(NOT EXISTS "${REX_WASM_CLANG_RESOURCE_INPUT}/include/stddef.h")
    message(FATAL_ERROR
      "REX_WASM_CLANG_RESOURCE_INPUT does not contain include/stddef.h: "
      "${REX_WASM_CLANG_RESOURCE_INPUT}")
  endif()

  get_filename_component(_rose_wasm_resource_input_real
    "${REX_WASM_CLANG_RESOURCE_INPUT}" REALPATH)
  set(_rose_wasm_resource_matches_llvm FALSE)
  set(_rose_wasm_expected_resource_dirs "")
  foreach(_rose_llvm_lib_dir IN LISTS LLVM_LIBRARY_DIRS)
    if(IS_ABSOLUTE "${_rose_llvm_lib_dir}")
      set(_rose_wasm_expected_resource_dir
        "${_rose_llvm_lib_dir}/clang/${LLVM_REQUIRED_MAJOR}")
      list(APPEND _rose_wasm_expected_resource_dirs
        "${_rose_wasm_expected_resource_dir}")
      if(EXISTS "${_rose_wasm_expected_resource_dir}")
        get_filename_component(_rose_wasm_expected_resource_real
          "${_rose_wasm_expected_resource_dir}" REALPATH)
        if(_rose_wasm_resource_input_real STREQUAL
           _rose_wasm_expected_resource_real)
          set(_rose_wasm_resource_matches_llvm TRUE)
        endif()
      endif()
    endif()
  endforeach()
  if(NOT _rose_wasm_resource_matches_llvm)
    message(FATAL_ERROR
      "REX_WASM_CLANG_RESOURCE_INPUT must be the exact LLVM ${LLVM_REQUIRED_MAJOR} "
      "resource directory from the loaded LLVMConfig.cmake. Got "
      "'${REX_WASM_CLANG_RESOURCE_INPUT}'; expected one of "
      "'${_rose_wasm_expected_resource_dirs}'.")
  endif()

  if(NOT REX_WASM_OMP_INCLUDE_DIR)
    message(FATAL_ERROR
      "REX_ENABLE_WASM requires an explicit LLVM ${LLVM_REQUIRED_MAJOR} "
      "resource include directory in REX_WASM_OMP_INCLUDE_DIR.")
  endif()
  if(NOT IS_ABSOLUTE "${REX_WASM_OMP_INCLUDE_DIR}" OR
     NOT EXISTS "${REX_WASM_OMP_INCLUDE_DIR}/omp.h")
    message(FATAL_ERROR
      "REX_WASM_OMP_INCLUDE_DIR must be an absolute directory containing "
      "omp.h: ${REX_WASM_OMP_INCLUDE_DIR}")
  endif()
  if(NOT REX_WASM_NATIVE_LLVM_ROOT OR
     NOT IS_ABSOLUTE "${REX_WASM_NATIVE_LLVM_ROOT}")
    message(FATAL_ERROR
      "REX_ENABLE_WASM requires the absolute native LLVM ${LLVM_REQUIRED_MAJOR} "
      "installation in REX_WASM_NATIVE_LLVM_ROOT.")
  endif()
  get_filename_component(_rose_wasm_native_llvm_root
    "${REX_WASM_NATIVE_LLVM_ROOT}" REALPATH)
  get_filename_component(_rose_wasm_native_omp_include
    "${_rose_wasm_native_llvm_root}/lib/clang/${LLVM_REQUIRED_MAJOR}/include"
    REALPATH)
  get_filename_component(_rose_wasm_omp_include_real
    "${REX_WASM_OMP_INCLUDE_DIR}" REALPATH)
  if(NOT "${_rose_wasm_native_omp_include}" STREQUAL
     "${_rose_wasm_omp_include_real}")
    message(FATAL_ERROR
      "REX_WASM_OMP_INCLUDE_DIR must be the exact resource include directory "
      "from REX_WASM_NATIVE_LLVM_ROOT. Got '${_rose_wasm_omp_include_real}'; "
      "expected '${_rose_wasm_native_omp_include}'.")
  endif()
  # omp.h is the OpenMP runtime API header of an LLVM release series; it is
  # identical across that series' patch releases, so it must come from the same
  # LLVM major as the WASM LLVM/Clang package.
  include(roseReadLlvmPackageVersion)
  rose_read_llvm_package_version("${_rose_wasm_native_llvm_root}"
    _rose_wasm_native_package_version)
  string(REGEX MATCH "^[0-9]+" _rose_wasm_native_major
    "${_rose_wasm_native_package_version}")
  if(NOT "${_rose_wasm_native_major}" STREQUAL "${LLVM_VERSION_MAJOR}")
    message(FATAL_ERROR
      "The native OpenMP header and the WASM LLVM/Clang package must come from "
      "the same LLVM major. Got native '${_rose_wasm_native_package_version}' "
      "and WASM '${LLVM_PACKAGE_VERSION}'.")
  endif()
  if(REX_WASM_PACKAGE_SYSROOT AND
     (NOT REX_WASM_SYSROOT OR NOT EXISTS "${REX_WASM_SYSROOT}/include"))
    message(FATAL_ERROR
      "REX_ENABLE_WASM requires an Emscripten sysroot with include/. Set REX_WASM_SYSROOT.")
  endif()

  set(REX_WASM_CLANG_RESOURCE_DIR
    "${CMAKE_BINARY_DIR}/wasm-resource/clang/${CLANG_VERSION_MAJOR}" CACHE INTERNAL
    "Staged Clang resource directory for the REX WASM package")
  file(REMOVE_RECURSE "${REX_WASM_CLANG_RESOURCE_DIR}")
  file(MAKE_DIRECTORY "${REX_WASM_CLANG_RESOURCE_DIR}/include")
  file(COPY "${REX_WASM_CLANG_RESOURCE_INPUT}/include/"
    DESTINATION "${REX_WASM_CLANG_RESOURCE_DIR}/include")
  configure_file(
    "${REX_WASM_OMP_INCLUDE_DIR}/omp.h"
    "${REX_WASM_CLANG_RESOURCE_DIR}/include/omp.h"
    COPYONLY)
  unset(_rose_llvm_lib_dir)
  unset(_rose_wasm_expected_resource_dir)
  unset(_rose_wasm_expected_resource_dirs)
  unset(_rose_wasm_expected_resource_real)
  unset(_rose_wasm_native_llvm_root)
  unset(_rose_wasm_native_omp_include)
  unset(_rose_wasm_native_major)
  unset(_rose_wasm_native_package_version)
  unset(_rose_wasm_omp_include_real)
  unset(_rose_wasm_resource_input_real)
  unset(_rose_wasm_resource_matches_llvm)
endif()

set(LIBHARU_ROOT "" CACHE PATH "Optional libharu installation prefix")
if(NOT REX_ENABLE_WASM)
  if(NOT LIBHARU_ROOT)
    set(LIBHARU_ROOT "$ENV{LIBHARU_ROOT}")
  endif()
  if(LIBHARU_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${LIBHARU_ROOT}")
  endif()

  # FindPkgConfig caches metadata and resolved paths across prefix changes.
  # Requery metadata and resolve its libraries in our own fresh searches.
  set(HPDF_FOUND FALSE)
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
    pkg_check_modules(HPDF QUIET libhpdf)
  endif()

  set(_hpdf_link_libraries "")
  if(HPDF_FOUND)
    foreach(_hpdf_name IN LISTS HPDF_LIBRARIES)
      unset(_hpdf_library CACHE)
      find_library(_hpdf_library NAMES ${_hpdf_name}
                   HINTS ${HPDF_LIBRARY_DIRS}
                   NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH REQUIRED)
      list(APPEND _hpdf_link_libraries "${_hpdf_library}")
    endforeach()
    unset(_hpdf_library CACHE)
  else()
    unset(HPDF_INCLUDE_DIRS)
    unset(HPDF_INCLUDE_DIRS CACHE)
    unset(HPDF_LIBRARIES)
    unset(HPDF_LIBRARIES CACHE)
    find_path(HPDF_INCLUDE_DIRS NAMES hpdf.h
              HINTS ENV LIBHARU_ROOT ${LIBHARU_ROOT}
              PATH_SUFFIXES include)
    find_library(HPDF_LIBRARIES NAMES hpdf libhpdf
                 HINTS ENV LIBHARU_ROOT ${LIBHARU_ROOT}
                 PATH_SUFFIXES lib lib64)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(HPDF DEFAULT_MSG
                                      HPDF_LIBRARIES HPDF_INCLUDE_DIRS)
    set(_hpdf_link_libraries "${HPDF_LIBRARIES}")
    get_filename_component(HPDF_LIBRARY_DIRS "${HPDF_LIBRARIES}" DIRECTORY)
    set(HPDF_CFLAGS_OTHER "")
    set(HPDF_LDFLAGS_OTHER "")
    set(HPDF_VERSION "")
  endif()

  if(NOT HPDF_FOUND OR NOT HPDF_LIBRARIES OR NOT HPDF_INCLUDE_DIRS)
    message(FATAL_ERROR "libharu (hpdf) is required. Install the libharu development package and ensure hpdf.h and libhpdf are discoverable, or set the LIBHARU_ROOT environment variable.")
  endif()

  add_library(REX::HPDF INTERFACE IMPORTED)
  target_include_directories(REX::HPDF INTERFACE ${HPDF_INCLUDE_DIRS})
  target_compile_options(REX::HPDF INTERFACE ${HPDF_CFLAGS_OTHER})
  target_link_libraries(REX::HPDF INTERFACE ${_hpdf_link_libraries})
  target_link_options(REX::HPDF INTERFACE ${HPDF_LDFLAGS_OTHER})
  unset(_hpdf_link_libraries)
else()
  set(HPDF_FOUND FALSE)
  set(HPDF_LIBRARIES "")
  set(HPDF_LIBRARY_DIRS "")
  set(HPDF_INCLUDE_DIRS "")
endif()

if(NOT REX_ENABLE_WASM)
  if(HPDF_VERSION)
    message(STATUS "Found libharu (hpdf): ${HPDF_LIBRARIES} (found version \"${HPDF_VERSION}\")")
  else()
    message(STATUS "Found libharu (hpdf): ${HPDF_LIBRARIES}")
  endif()
endif()

if(HPDF_FOUND)
  set(ROSE_WITH_LIBHARU 1)
else()
  set(ROSE_WITH_LIBHARU 0)
endif()

find_package(FLEX REQUIRED)

if(NOT REX_ENABLE_WASM)
  find_library(M_LIB m REQUIRED)
  find_library(RT_LIB rt)
endif()

# Back-end compilers

foreach(_removed_backend_compiler_option IN ITEMS
    WITH-ALT-BACKEND-C-COMPILER
    WITH-ALT-BACKEND-CXX-COMPILER
    WITH-ALT-BACKEND-FORTRAN-COMPILER)
  if(NOT "${${_removed_backend_compiler_option}}" STREQUAL "")
    message(FATAL_ERROR
      "${_removed_backend_compiler_option} was removed because selecting a "
      "different compiler after language initialization creates an incoherent "
      "toolchain. Select the exact compiler during the first configure with "
      "CMAKE_C_COMPILER, CMAKE_CXX_COMPILER, and CMAKE_Fortran_COMPILER.")
  endif()
endforeach()
unset(_removed_backend_compiler_option)

if(REX_ENABLE_WASM)
  set(link_with_libraries "")
else()
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set(link_with_libraries Threads::Threads "${M_LIB}")
  if(RT_LIB)
    list(APPEND link_with_libraries "${RT_LIB}")
  endif()
endif()

# C and C++ are enabled by project(), including Emscripten builds. Publish the
# actual CMake compiler metadata for every configuration; disabled analyses do
# not justify fabricated compiler versions.
include(roseChooseBackendCompiler)

set(REX_CXX_RUNTIME_LINK_OPTIONS "")
if(ENABLE-FORTRAN)
  rex_backend_runtime_options(CXX Fortran REX_CXX_RUNTIME_LINK_OPTIONS)
endif()
foreach(_argument_list IN ITEMS
    BACKEND_C_COMPILER_COMMAND BACKEND_CXX_COMPILER_COMMAND
    BACKEND_FORTRAN_COMPILER_COMMAND REX_CXX_RUNTIME_LINK_OPTIONS)
  set(${_argument_list}_CPP "")
  foreach(_option IN LISTS ${_argument_list})
    rex_cpp_string_literal("${_option}" _literal)
    string(APPEND ${_argument_list}_CPP "  ${_literal},\n")
  endforeach()
endforeach()
configure_file(cmake/rex-backend-link-options.h.in
  ${CMAKE_BINARY_DIR}/rex-backend-link-options.h @ONLY)
unset(REX_CXX_RUNTIME_LINK_OPTIONS)
unset(_argument_list)
unset(_option)
unset(_literal)

if(ENABLE-C AND NOT REX_ENABLE_WASM)
  install(DIRECTORY ${CMAKE_BINARY_DIR}/include-staging/
    DESTINATION ${CLANG_BUILTIN_HEADERS_INSTALL_DIR})
endif()

# ROSE headers are scattered throughout the source tree.

set(ROSE_INCLUDES
  ${ROSE_TOP_BINARY_DIR}
  ${ROSE_TOP_BINARY_DIR}/src/util
  ${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/
  ${ROSE_TOP_SRC_DIR}
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/sage_support
  ${ROSE_TOP_SRC_DIR}/src/ROSETTA/src/
  ${ROSE_TOP_SRC_DIR}/src
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/astPostProcessing
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/includeDirectivesProcessing
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/sageInterface
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/ompparser/src
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/accparser/src
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/virtualCFG
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/astTokenStream
  ${ROSE_TOP_SRC_DIR}/src/frontend/Flang
  ${ROSE_TOP_SRC_DIR}/src/frontend/Flang/builder
  ${ROSE_TOP_SRC_DIR}/src/frontend
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/formatSupport
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/languageIndependenceSupport
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/CxxCodeGeneration
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/FortranCodeGeneration
  ${ROSE_TOP_SRC_DIR}/src/util
  ${ROSE_TOP_SRC_DIR}/src/util/support
  ${ROSE_TOP_SRC_DIR}/src/util/graphs
  ${ROSE_TOP_SRC_DIR}/src/util/commandlineProcessing
  ${ROSE_TOP_SRC_DIR}/src/midend
  ${ROSE_TOP_SRC_DIR}/src/midend/abstractLayer
  ${ROSE_TOP_SRC_DIR}/src/midend/astDiagnostics
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/astInlining
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/astOutlining
  ${ROSE_TOP_SRC_DIR}/src/midend/astDump
  ${ROSE_TOP_SRC_DIR}/src/midend/astProcessing
  ${ROSE_TOP_SRC_DIR}/src/midend/astQuery
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil/annotation
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil/astInterface
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil/astSupport
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil/symbolicVal
  ${ROSE_TOP_SRC_DIR}/src/midend/astUtil/dependenceTracking
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/computation
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/depGraph
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/depInfo
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/driver
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/outsideInterface
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/prepostTransformation
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/loopProcessing/slicing
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/ompLowering
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/transformationTracking
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/CFG
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/CallGraphAnalysis
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/bitvectorDataflow
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/VirtualFunctionAnalysis
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/variableRenaming
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/defUseAnalysis
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/dominanceAnalysis
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/pointerAnal
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/staticInterproceduralSlicing
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/valuePropagation
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/partialRedundancyElimination
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/finiteDifferencing
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/functionCallNormalization
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/constantFolding
  ${ROSE_TOP_SRC_DIR}/src/midend/programTransformation/implicitCodeGeneration)

if(VALGRIND_INCLUDE_PATH)
  list(APPEND ROSE_INCLUDES ${VALGRIND_INCLUDE_PATH})
endif()
list(APPEND ROSE_INCLUDES ${HPDF_INCLUDE_DIRS})
list(REMOVE_DUPLICATES ROSE_INCLUDES)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set up RPATH so that our executables will be able to find their dependent
# libraries using relative paths.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Keep preferring sibling lib/ directories even with LD_LIBRARY_PATH set.
  # Every enabled driver must support this policy with its selected linker.
  rex_require_linker_option("-Wl,--disable-new-dtags")
endif()

rex_configure_rpath("${LIB_INSTALL_DIR}")

add_subdirectory(LicenseInformation)
add_subdirectory(src)
if(NOT REX_ENABLE_WASM)
  add_subdirectory(exampleTranslators)
  add_subdirectory(scripts)
endif()

configure_file(cmake/CTestCustom.cmake.in ${CMAKE_BINARY_DIR}/CTestCustom.cmake @ONLY)
# Configure CTest settings (including memcheck) for the build tree.
configure_file(cmake/CTestConfig.cmake.in ${CMAKE_BINARY_DIR}/CTestConfig.cmake @ONLY)

# Linux-only: fail fast on non-Linux platforms.
if(NOT REX_ENABLE_WASM AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This project now targets Linux only; non-Linux builds are unsupported.")
endif()

if(ENABLE-C)
  file(CREATE_LINK "${CMAKE_BINARY_DIR}/include-staging"
    "${CMAKE_BINARY_DIR}/include" SYMBOLIC)
endif()

# Generate configuration headers after all feature and compiler checks.
set(ROSE_HAVE_PTHREAD_H "${HAVE_PTHREAD_H}")
configure_file(${ROSE_TOP_SRC_DIR}/cmake/rosePublicConfig.h.in.cmake
  ${ROSE_TOP_BINARY_DIR}/rosePublicConfig.h @ONLY)
configure_file(${ROSE_TOP_SRC_DIR}/rose_config.h.in.cmake ${ROSE_TOP_BINARY_DIR}/rose_config.h)

if(ROSE_ENABLE_PCH)
  add_library(rosePublicPch OBJECT cmake/pch/rose_public_pch.C)
  target_include_directories(rosePublicPch PRIVATE ${ROSE_INCLUDES})
  target_link_libraries(rosePublicPch PRIVATE ROSE_DLL ${link_with_libraries})
  target_compile_features(rosePublicPch PRIVATE cxx_std_14)
  add_dependencies(rosePublicPch ROSE_DLL rosetta_generated)
  rose_enable_private_pch(rosePublicPch "${ROSE_TOP_SRC_DIR}/src/rose.h")
endif()

if(NOT REX_ENABLE_WASM)
  add_subdirectory(tools)
  if(ENABLE-TESTS-DIRECTORY)
    add_subdirectory(tests)
  endif()
endif()
if(NOT REX_ENABLE_WASM AND ENABLE-TUTORIAL-DIRECTORY)
  add_subdirectory(tutorial)
endif()
rose_apply_public_pch_to_in_tree_targets()

# Install RMC configuration file if present
# When ROSE dependencies are managed with "rmc", a file named ".spock" is created at the top of the build tree.
# Install this file as "lib/rmc/rose" if RMC is active.
if(NOT REX_ENABLE_WASM AND DEFINED ENV{SPOCK_EMPLOYED})
  if(EXISTS "${CMAKE_BINARY_DIR}/.spock")
    install(FILES ${CMAKE_BINARY_DIR}/.spock
      DESTINATION ${LIB_INSTALL_DIR}/rmc
      RENAME rose
    )
  endif()
endif()
