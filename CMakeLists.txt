################################################################################################ -*- cmake -*- #########
# General warnings, instructions, and style guide.
########################################################################################################################

# 1. Please be careful about where to put your tests and variable settings. The order matters!!
#
# 2. CMake configuration files are neither completely case-sensitive nor completely case-insensitive. Therefore, to
#    avoid errors, assume everything is case sensitive. Prefer lower case for function names.
#
# 3. Do not use argument turds in "else()" and "endif()" functions. Those arguments are only comments and are entirely
#    redundant with the "if()" argument and easily become out of date and misleading since nobody actually reads them.
#
# 4. All messages should start with a capitalized word except in special circumstances where capitalization would be
#    incorrect (such as the name of a command).
#
# 5. Indentation is two spaces. Do not use ASCII horizontal tab characters for indentation or alignment.
#
# 6. As with standard mathematical notation, there should be no white space on either side of a function's
#    parentheses. This includes functions such as "if", "else", and "endif".
#
# 7. Prefer FALSE and TRUE as Boolean values since these are the names used in Mathematics and most other computer
#    languages. Avoid OFF, ON, NO, YES, 0, 1, and especially avoid mixing them.
#
# 8. If a CMakefile file needs to be conditionally enabled, do it in that CMake file rather than around the
#    add_subdirectory in the level above. This keeps all the logic for a directory in a single file rather than
#    split across two files. It is a bit unfortunate that CMake can't find the lower-level CMakeList files

#    on its own, so some of the logic is still necessarily in the level above.  There are exceptions to this rule,
#    and they're pretty obvious when they occur--as when a single if() protects a whole bunch of add_subdirectory.

########################################################################################################################
# Platform-independent settings
########################################################################################################################

cmake_minimum_required(VERSION 3.15)

set(LLVM_REQUIRED_MAJOR "22" CACHE STRING "Minimum LLVM/Clang major version required by REX")
if(NOT LLVM_REQUIRED_MAJOR MATCHES "^[0-9]+$")
  message(FATAL_ERROR "LLVM_REQUIRED_MAJOR must be a numeric major version, got '${LLVM_REQUIRED_MAJOR}'.")
endif()

option(REX_ENABLE_WASM "Build the pure static browser WebAssembly REX demo" FALSE)
set(REX_USING_EMSCRIPTEN_TOOLCHAIN FALSE)
if(REX_ENABLE_WASM OR CMAKE_TOOLCHAIN_FILE MATCHES "Emscripten[.]cmake$")
  set(REX_USING_EMSCRIPTEN_TOOLCHAIN TRUE)
endif()

#-----------------------------------------------------------------------------------------------------------------------
# Enable ccache if not already enabled by symlink masquerading and if no other compiler launchers are already defined
#-----------------------------------------------------------------------------------------------------------------------
find_program(CCACHE_EXECUTABLE ccache)
mark_as_advanced(CCACHE_EXECUTABLE)
if(CCACHE_EXECUTABLE)
  foreach(LANG C CXX)
    if(NOT DEFINED CMAKE_${LANG}_COMPILER_LAUNCHER AND NOT CMAKE_${LANG}_COMPILER MATCHES ".*/ccache")
      message(STATUS "Enabling ccache for ${LANG}")
      set(CMAKE_${LANG}_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE} CACHE STRING "")
    endif()
  endforeach()
endif()

#-----------------------------------------------------------------------------------------------------------------------
# Compiler selection
#-----------------------------------------------------------------------------------------------------------------------
# Prefer Clang/Clang++ when available, but allow any modern compiler.

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
    unset(_reference_dir)
  endif()
  set(${OUT_VAR} "${_sibling_compiler}" PARENT_SCOPE)
  unset(_sibling_compiler)
endfunction()

function(set_fuse_ld_flag VAR FUSE_LD_VALUE)
  set(_rose_linker_flags " ${${VAR}} ")
  string(REGEX REPLACE " +-fuse-ld=[^ ]+" "" _rose_linker_flags "${_rose_linker_flags}")
  string(STRIP "${_rose_linker_flags}" _rose_linker_flags)
  if(FUSE_LD_VALUE)
    string(APPEND _rose_linker_flags " -fuse-ld=${FUSE_LD_VALUE}")
    string(STRIP "${_rose_linker_flags}" _rose_linker_flags)
  endif()
  set(${VAR} "${_rose_linker_flags}" CACHE STRING "" FORCE)
  unset(_rose_linker_flags)
endfunction()

function(get_fuse_ld_value OUT_VAR LINKER)
  get_filename_component(_rose_linker_name "${LINKER}" NAME)
  if(_rose_linker_name MATCHES "^(ld[.])?lld(-[0-9]+([.][0-9]+)*)?$")
    set(_rose_fuse_ld_value "lld")
  elseif(_rose_linker_name MATCHES "^mold(-[0-9]+([.][0-9]+)*)?$")
    set(_rose_fuse_ld_value "mold")
  elseif(_rose_linker_name MATCHES "^(ld[.])?bfd(-[0-9]+([.][0-9]+)*)?$")
    set(_rose_fuse_ld_value "bfd")
  else()
    set(_rose_fuse_ld_value "")
  endif()
  set(${OUT_VAR} "${_rose_fuse_ld_value}" PARENT_SCOPE)
  unset(_rose_fuse_ld_value)
  unset(_rose_linker_name)
endfunction()

# Infer LLVM/Clang package roots from the selected compiler prefix when the
# user did not provide explicit package roots.
function(find_llvm_root_from_compiler OUT_VAR COMPILER)
  set(_llvm_root "")
  if(COMPILER AND EXISTS "${COMPILER}")
    get_filename_component(_compiler_bindir "${COMPILER}" DIRECTORY)
    get_filename_component(_compiler_prefix "${_compiler_bindir}/.." ABSOLUTE)
    if(EXISTS "${_compiler_prefix}/lib/cmake/llvm/LLVMConfig.cmake" AND
       EXISTS "${_compiler_prefix}/lib/cmake/clang/ClangConfig.cmake")
      set(_llvm_root "${_compiler_prefix}")
    endif()
    unset(_compiler_bindir)
    unset(_compiler_prefix)
  endif()
  set(${OUT_VAR} "${_llvm_root}" PARENT_SCOPE)
  unset(_llvm_root)
endfunction()

# Helper function to detect and set compiler with preference order:
# 1. Use environment variable if set
# 2. Try preferred version (e.g., clang-${LLVM_REQUIRED_MAJOR})
# 3. Try fallback versions (e.g., clang, flang)
# 4. Let CMake use system default
function(detect_compiler LANG CMAKE_VAR ENV_VAR LANG_NAME PREFERRED_NAMES FALLBACK_NAMES)
  if(NOT DEFINED ${CMAKE_VAR} OR "${${CMAKE_VAR}}" STREQUAL "")
    if(DEFINED ENV{${ENV_VAR}} AND NOT "$ENV{${ENV_VAR}}" STREQUAL "")
      set(${CMAKE_VAR} "$ENV{${ENV_VAR}}" PARENT_SCOPE)
      message(STATUS "Using ${LANG_NAME} compiler from ${ENV_VAR}: $ENV{${ENV_VAR}}")
    else()
      # Try preferred version first
      find_program(_COMPILER_PREFERRED NAMES ${PREFERRED_NAMES})
      if(_COMPILER_PREFERRED)
        set(${CMAKE_VAR} ${_COMPILER_PREFERRED} PARENT_SCOPE)
        message(STATUS "Auto-detected ${LANG_NAME} compiler: ${_COMPILER_PREFERRED}")
      else()
        # Try fallback versions when provided.
        if(NOT "${FALLBACK_NAMES}" STREQUAL "")
          find_program(_COMPILER_FALLBACK NAMES ${FALLBACK_NAMES})
          if(_COMPILER_FALLBACK)
            set(${CMAKE_VAR} ${_COMPILER_FALLBACK} PARENT_SCOPE)
            message(STATUS "Auto-detected ${LANG_NAME} compiler: ${_COMPILER_FALLBACK}")
          endif()
        endif()
      endif()
      unset(_COMPILER_PREFERRED CACHE)
      unset(_COMPILER_FALLBACK CACHE)
    endif()
  endif()
endfunction()

# Detect C and C++ compilers.  Emscripten's toolchain file initializes the
# compiler during project(), so do not preempt it with a host clang probe.
if(NOT REX_USING_EMSCRIPTEN_TOOLCHAIN)
  if((NOT DEFINED CMAKE_C_COMPILER OR "${CMAKE_C_COMPILER}" STREQUAL "") AND
     DEFINED CMAKE_CXX_COMPILER AND NOT "${CMAKE_CXX_COMPILER}" STREQUAL "")
    find_sibling_compiler(_C_COMPILER_FROM_CXX "${CMAKE_CXX_COMPILER}" "clang-${LLVM_REQUIRED_MAJOR}" "clang")
    if(_C_COMPILER_FROM_CXX)
      set(CMAKE_C_COMPILER "${_C_COMPILER_FROM_CXX}")
      message(STATUS "Auto-detected C compiler next to C++ compiler: ${_C_COMPILER_FROM_CXX}")
    endif()
    unset(_C_COMPILER_FROM_CXX)
  endif()
  detect_compiler(C CMAKE_C_COMPILER CC "C" "clang-${LLVM_REQUIRED_MAJOR}" "clang")

  if((NOT DEFINED CMAKE_CXX_COMPILER OR "${CMAKE_CXX_COMPILER}" STREQUAL "") AND
     DEFINED CMAKE_C_COMPILER AND NOT "${CMAKE_C_COMPILER}" STREQUAL "")
    find_sibling_compiler(_CXX_COMPILER_FROM_C "${CMAKE_C_COMPILER}" "clang++-${LLVM_REQUIRED_MAJOR}" "clang++")
    if(_CXX_COMPILER_FROM_C)
      set(CMAKE_CXX_COMPILER "${_CXX_COMPILER_FROM_C}")
      message(STATUS "Auto-detected C++ compiler next to C compiler: ${_CXX_COMPILER_FROM_C}")
    endif()
    unset(_CXX_COMPILER_FROM_C)
  endif()
  detect_compiler(CXX CMAKE_CXX_COMPILER CXX "C++" "clang++-${LLVM_REQUIRED_MAJOR}" "clang++")

  if((NOT DEFINED LLVM_ROOT OR "${LLVM_ROOT}" STREQUAL "") AND
     DEFINED CMAKE_C_COMPILER AND NOT "${CMAKE_C_COMPILER}" STREQUAL "")
    find_llvm_root_from_compiler(_ROSE_DETECTED_LLVM_ROOT "${CMAKE_C_COMPILER}")
    if(_ROSE_DETECTED_LLVM_ROOT)
      set(LLVM_ROOT "${_ROSE_DETECTED_LLVM_ROOT}")
      set(Clang_ROOT "${_ROSE_DETECTED_LLVM_ROOT}")
      message(STATUS "Auto-detected LLVM root from compiler: ${_ROSE_DETECTED_LLVM_ROOT}")
    endif()
    unset(_ROSE_DETECTED_LLVM_ROOT)
  endif()
else()
  message(STATUS "Compiler selection: deferring to the Emscripten toolchain")
endif()

set(_C_COMPILER_DISPLAY "${CMAKE_C_COMPILER}")
if(NOT _C_COMPILER_DISPLAY)
  set(_C_COMPILER_DISPLAY "<unspecified>")
endif()
set(_CXX_COMPILER_DISPLAY "${CMAKE_CXX_COMPILER}")
if(NOT _CXX_COMPILER_DISPLAY)
  set(_CXX_COMPILER_DISPLAY "<unspecified>")
endif()
message(STATUS "Compiler selection: Using ${_C_COMPILER_DISPLAY} and ${_CXX_COMPILER_DISPLAY}")

if(NOT DEFINED _ROSE_CONFIGURED_ONCE)
  set(_ROSE_FIRST_CONFIGURE TRUE)
else()
  set(_ROSE_FIRST_CONFIGURE FALSE)
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

set(_ROSE_CURRENT_CMAKE_LINKER "")
if(DEFINED CMAKE_LINKER)
  set(_ROSE_CURRENT_CMAKE_LINKER "${CMAKE_LINKER}")
endif()

if(_ROSE_FIRST_CONFIGURE)
  if(_ROSE_CURRENT_CMAKE_LINKER)
    set(_ROSE_USER_PROVIDED_CMAKE_LINKER TRUE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
  else()
    set(_ROSE_USER_PROVIDED_CMAKE_LINKER FALSE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
  endif()
elseif(NOT DEFINED _ROSE_USER_PROVIDED_CMAKE_LINKER)
  set(_ROSE_USER_PROVIDED_CMAKE_LINKER FALSE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
endif()

if(NOT _ROSE_FIRST_CONFIGURE)
  if(NOT _ROSE_CURRENT_CMAKE_LINKER)
    set(_ROSE_USER_PROVIDED_CMAKE_LINKER FALSE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
  elseif(DEFINED _ROSE_LAST_CMAKE_LINKER AND
         "${_ROSE_CURRENT_CMAKE_LINKER}" STREQUAL "${_ROSE_LAST_CMAKE_LINKER}")
    # Keep the cached user-provided state for an unchanged linker.
  elseif(DEFINED _ROSE_AUTO_CMAKE_LINKER AND "${_ROSE_CURRENT_CMAKE_LINKER}" STREQUAL "${_ROSE_AUTO_CMAKE_LINKER}")
    set(_ROSE_USER_PROVIDED_CMAKE_LINKER FALSE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
  elseif(NOT DEFINED _ROSE_LAST_CMAKE_LINKER)
    get_filename_component(_ROSE_CMAKE_LINKER_NAME "${_ROSE_CURRENT_CMAKE_LINKER}" NAME)
    if(NOT _ROSE_CMAKE_LINKER_NAME STREQUAL "ld")
      set(_ROSE_USER_PROVIDED_CMAKE_LINKER TRUE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
    else()
      set(_ROSE_USER_PROVIDED_CMAKE_LINKER FALSE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
    endif()
    unset(_ROSE_CMAKE_LINKER_NAME)
  else()
    set(_ROSE_USER_PROVIDED_CMAKE_LINKER TRUE CACHE INTERNAL "Whether CMAKE_LINKER was user-provided" FORCE)
  endif()
endif()

#-----------------------------------------------------------------------------------------------------------------------
# Read ROSE version from ROSE_VERSION file before calling project()
# This is needed so that the project version can be set correctly, which affects library versioning
#-----------------------------------------------------------------------------------------------------------------------
set(ROSE_SCM_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ROSE_VERSION")
if(NOT EXISTS "${ROSE_SCM_VERSION_FILE}")
  message(FATAL_ERROR "ROSE version file not found: ${ROSE_SCM_VERSION_FILE}")
endif()
file(STRINGS ${ROSE_SCM_VERSION_FILE} ROSE_PACKAGE_VERSION LIMIT_COUNT 1)
string(STRIP "${ROSE_PACKAGE_VERSION}" ROSE_PACKAGE_VERSION)

project(ROSE
  VERSION ${ROSE_PACKAGE_VERSION}
  LANGUAGES C CXX
)

if(REX_ENABLE_WASM AND NOT EMSCRIPTEN)
  message(FATAL_ERROR "REX_ENABLE_WASM requires configuring with emcmake/Emscripten.")
endif()

include(CheckLinkerFlag)

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

set(REX_LINKER "auto" CACHE STRING "Linker to use for REX builds: auto, lld, mold, bfd, or system")
if(REX_ENABLE_WASM)
  set(REX_LINKER "system" CACHE STRING "Linker to use for REX builds: auto, lld, mold, bfd, or system" FORCE)
endif()
set_property(CACHE REX_LINKER PROPERTY STRINGS auto lld mold bfd system)
if(NOT REX_LINKER MATCHES "^(auto|lld|mold|bfd|system)$")
  message(FATAL_ERROR "REX_LINKER must be one of auto, lld, mold, bfd, or system; got '${REX_LINKER}'.")
endif()

if(NOT REX_ENABLE_WASM AND NOT REX_LINKER STREQUAL "system")
  set(_rose_lld_linker_names "ld.lld-${LLVM_REQUIRED_MAJOR}" "lld-${LLVM_REQUIRED_MAJOR}" ld.lld lld)
  set(_rose_linker_names "")
  if(REX_LINKER STREQUAL "auto")
    list(APPEND _rose_linker_names ${_rose_lld_linker_names})
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(loongarch64|loong64)$")
      list(APPEND _rose_linker_names mold)
    endif()
    list(APPEND _rose_linker_names ld.bfd)
  elseif(REX_LINKER STREQUAL "lld")
    list(APPEND _rose_linker_names ${_rose_lld_linker_names})
  elseif(REX_LINKER STREQUAL "mold")
    list(APPEND _rose_linker_names mold)
  elseif(REX_LINKER STREQUAL "bfd")
    list(APPEND _rose_linker_names ld.bfd)
  endif()

  set(_rose_linker "")
  if(_ROSE_USER_PROVIDED_CMAKE_LINKER)
    set(_rose_linker "${_ROSE_CURRENT_CMAKE_LINKER}")
    get_fuse_ld_value(_rose_explicit_fuse_ld_value "${_rose_linker}")
    if(NOT REX_LINKER STREQUAL "auto" AND NOT _rose_explicit_fuse_ld_value)
      message(FATAL_ERROR
        "REX_LINKER=${REX_LINKER} conflicts with explicit CMAKE_LINKER='${_rose_linker}' "
        "because the explicit linker is not recognized as lld, mold, or bfd.")
    elseif(NOT REX_LINKER STREQUAL "auto" AND NOT _rose_explicit_fuse_ld_value STREQUAL REX_LINKER)
      message(FATAL_ERROR
        "REX_LINKER=${REX_LINKER} conflicts with explicit CMAKE_LINKER='${_rose_linker}' "
        "(detected as ${_rose_explicit_fuse_ld_value}).")
    endif()
    unset(_rose_explicit_fuse_ld_value)
  elseif(CMAKE_CXX_COMPILER)
    find_sibling_compiler(_rose_linker "${CMAKE_CXX_COMPILER}" ${_rose_linker_names})
  endif()
  if(NOT _rose_linker)
    string(TOUPPER "${REX_LINKER}" _rose_linker_cache_suffix)
    set(_rose_linker_cache_var "_ROSE_${_rose_linker_cache_suffix}_${LLVM_REQUIRED_MAJOR}_LINKER")
    find_program(${_rose_linker_cache_var} NAMES ${_rose_linker_names})
    mark_as_advanced(${_rose_linker_cache_var})
    set(_rose_linker "${${_rose_linker_cache_var}}")
    unset(_rose_linker_cache_suffix)
    unset(_rose_linker_cache_var)
  endif()

  if(_rose_linker)
    set(CMAKE_LINKER "${_rose_linker}" CACHE FILEPATH "Linker" FORCE)
    if(NOT _ROSE_USER_PROVIDED_CMAKE_LINKER)
      set(_ROSE_AUTO_CMAKE_LINKER "${_rose_linker}" CACHE INTERNAL "REX auto-selected linker")
    endif()
    get_fuse_ld_value(_rose_fuse_ld_value "${_rose_linker}")
    set_fuse_ld_flag(CMAKE_EXE_LINKER_FLAGS "${_rose_fuse_ld_value}")
    set_fuse_ld_flag(CMAKE_SHARED_LINKER_FLAGS "${_rose_fuse_ld_value}")
    set_fuse_ld_flag(CMAKE_MODULE_LINKER_FLAGS "${_rose_fuse_ld_value}")
    if(_rose_fuse_ld_value)
      message(STATUS "Using ${REX_LINKER} linker: ${_rose_linker} (driver flag: -fuse-ld=${_rose_fuse_ld_value})")
    else()
      message(STATUS "Using ${REX_LINKER} linker: ${_rose_linker} (no driver flag set)")
    endif()
  elseif(NOT REX_LINKER STREQUAL "auto")
    message(FATAL_ERROR "Requested REX_LINKER=${REX_LINKER}, but no matching linker was found.")
  endif()

  unset(_rose_fuse_ld_value)
  unset(_rose_linker)
  unset(_rose_linker_names)
  unset(_rose_lld_linker_names)
endif()
if(DEFINED CMAKE_LINKER)
  set(_ROSE_LAST_CMAKE_LINKER "${CMAKE_LINKER}" CACHE INTERNAL "Last configured CMAKE_LINKER" FORCE)
else()
  unset(_ROSE_LAST_CMAKE_LINKER CACHE)
endif()
set(_ROSE_CONFIGURED_ONCE TRUE CACHE INTERNAL "REX configure has completed at least once")
unset(_ROSE_CURRENT_CMAKE_LINKER)
unset(_ROSE_FIRST_CONFIGURE)

#-----------------------------------------------------------------------------------------------------------------------
# Adjust CMake's default optimization, debug, and assertion flags for different build types.
#
# "RelWithDebInfo" is intended to be optimized with debug info, but CMake adds -DNDEBUG by default which disables
# assertions. We want assertions enabled by default for this build type. If you don't want assertions then use
# "-DCMAKE_C_FLAGS=-DNDEBUG" and "-DCMAKE_CXX_FLAGS=-DNDEBUG". The optimization level defaults to "-O2", which
# is a compromise between full optimization and the ability to use most features of interactive debuggers.
#
# "Debug" is intended for developers to debug their libraries and tools, but CMake doesn't turn off optimizations. We
# add "-O0" so that all features of interactive debugging work properly, although this has a major negative effect on
# performance. We don't use "-Og" because this doesn't give the best debugging experience (too many local variables
# get optimized out and therefore one cannot inspect them in the debugger).
#-----------------------------------------------------------------------------------------------------------------------
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

# CMP0054 causes CMake to only interpret "if()" arguments as variables or keywords when unquoted. CMake 3.1 and above no
# longer implicitly dereference variables or interpret keywords in an if() command argument when it is a Quoted Argument
# or a Bracket Argument. The OLD behavior for this policy is to dereference variables and interpret keywords even if
# they are quoted or bracketed. The NEW behavior is to not dereference variables or interpret keywords that have been
# quoted or bracketed.
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# Clang installs libc++abi headers in its prefix; when linking against libstdc++
# prefer libstdc++ headers to avoid mixed-ABI conflicts (e.g., cxxabi.h).
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "stdc++" _rose_implicit_stdcxx_index)
  if(NOT _rose_implicit_stdcxx_index EQUAL -1)
    set(_rose_libstdcxx_include_dirs "")
    foreach(_rose_dir IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
      if(_rose_dir MATCHES "/c\\+\\+/")
        list(APPEND _rose_libstdcxx_include_dirs "${_rose_dir}")
      endif()
    endforeach()
    list(REMOVE_DUPLICATES _rose_libstdcxx_include_dirs)
    if(_rose_libstdcxx_include_dirs)
      set(_rose_libstdcxx_isystem_opts "")
      foreach(_rose_dir IN LISTS _rose_libstdcxx_include_dirs)
        list(APPEND _rose_libstdcxx_isystem_opts "-isystem${_rose_dir}")
      endforeach()
      add_library(rose_clang_libstdcxx_headers INTERFACE)
      target_compile_options(rose_clang_libstdcxx_headers INTERFACE
        ${_rose_libstdcxx_isystem_opts})
      install(TARGETS rose_clang_libstdcxx_headers EXPORT RoseTargets)
      unset(_rose_libstdcxx_isystem_opts)
    endif()
    unset(_rose_libstdcxx_include_dirs)
  endif()
  unset(_rose_implicit_stdcxx_index)
  unset(_rose_dir)
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

  set(_rose_system_include_dirs "")
  foreach(_rose_raw_dir IN LISTS ARGN)
    string(STRIP "${_rose_raw_dir}" _rose_raw_dir)
    if(_rose_raw_dir)
      separate_arguments(_rose_split_dirs UNIX_COMMAND "${_rose_raw_dir}")
      list(APPEND _rose_system_include_dirs ${_rose_split_dirs})
    endif()
  endforeach()
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
  unset(_rose_raw_dir)
  unset(_rose_split_dirs)
endfunction()

option(ROSE_ENABLE_PCH "Enable precompiled headers for in-tree REX targets" TRUE)
if(REX_ENABLE_WASM)
  set(ROSE_ENABLE_PCH FALSE CACHE BOOL "Enable precompiled headers for in-tree REX targets" FORCE)
endif()

function(rose_target_accepts_pch OUT_VAR target)
  set(_rose_accepts_pch TRUE)

  if(CMAKE_VERSION VERSION_LESS "3.19")
    get_target_property(_rose_target_source_dir "${target}" SOURCE_DIR)
    if(NOT _rose_target_source_dir OR
       NOT _rose_target_source_dir STREQUAL CMAKE_CURRENT_SOURCE_DIR)
      set(_rose_accepts_pch FALSE)
    endif()
  endif()

  set(${OUT_VAR} ${_rose_accepts_pch} PARENT_SCOPE)
endfunction()

function(rose_enable_private_pch target header)
  if(NOT ROSE_ENABLE_PCH)
    return()
  endif()
  if(NOT COMMAND target_precompile_headers)
    message(STATUS "Precompiled headers requested, but this CMake version does not support target_precompile_headers")
    return()
  endif()
  if(NOT TARGET "${target}")
    message(FATAL_ERROR
      "rose_enable_private_pch(${target} ${header}) requires an existing CMake target.")
  endif()
  rose_target_accepts_pch(_rose_accepts_pch "${target}")
  if(NOT _rose_accepts_pch)
    return()
  endif()

  target_precompile_headers(${target} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${header}>")
endfunction()

function(rose_enable_sage_pch target)
  rose_enable_private_pch(${target}
    "${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/sage3basic.h")
endfunction()

function(rose_reuse_pch target pch_target)
  if(NOT ROSE_ENABLE_PCH)
    return()
  endif()
  if(NOT COMMAND target_precompile_headers)
    return()
  endif()
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rose_reuse_pch(${target} ${pch_target}) requires an existing target '${target}'.")
  endif()
  if(NOT TARGET "${pch_target}")
    message(FATAL_ERROR "rose_reuse_pch(${target} ${pch_target}) requires an existing target '${pch_target}'.")
  endif()
  rose_target_accepts_pch(_rose_accepts_pch "${target}")
  if(NOT _rose_accepts_pch)
    return()
  endif()

  target_precompile_headers(${target} REUSE_FROM ${pch_target})
endfunction()

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
      if(_rose_target_links MATCHES "(^|;)ROSE_DLL($|;)" OR
         _rose_target_links MATCHES "(^|;)Rose::rose($|;)")
        rose_reuse_pch(${_rose_target} rosePublicPch)
      endif()
    endforeach()
  endforeach()

  unset(_rose_target_links)
  unset(_rose_target_type)
  unset(_rose_target)
  unset(_rose_targets)
  unset(_rose_directory)
  unset(_rose_directories)
endfunction()

# In CMake 3.12 and above the find_package(<PackageName>) command now searches prefixes specified by the
# <PackageName>_ROOT CMake variable and the <PackageName>_ROOT environment variable. Package roots are maintained as a
# stack so nested calls to all find_* commands inside find modules also search the roots as prefixes. This policy
# provides compatibility with projects that have not been updated to avoid using <PackageName>_ROOT variables for other
# purposes.  The OLD behavior for this policy is to ignore <PackageName>_ROOT variables. The NEW behavior for this
# policy is to use <PackageName>_ROOT variables.
#
# Behavior is set to "NEW" because ROSE matrix testing, Livermore's LC RZ/CZ resources, Spack, and RMC/Spock seldom
# install ROSE software dependencies in standard locations because they need to support the ability to install multiple
# versions and configurations of the dependencies.
if(POLICY_CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

# In CMake 3.27+ find_package() also searches upper-case <PACKAGENAME>_ROOT variables in addition to the
# properly-cased <PackageName>_ROOT variables. Enable this so LLVM_ROOT and CLANG_ROOT are honored.
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

# This controls whether the "cmake" command is verbose. It has nothing to do with the verbosity the resulting Makefiles.
option(VERBOSE "CMake should be verbose" FALSE)
if(VERBOSE)
  set(QUIET FALSE)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
else()
  set(QUIET TRUE)
  set(CMAKE_VERBOSE_MAKEFILE FALSE)
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

# FIXME: Why do we have to have a copy of some standard built-in modules inside rose?
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

########################################################################################################################
# ROSE version information
########################################################################################################################

# The ROSE version string was already read from ROSE_VERSION file before the project() command.
# REX accepts both the historical four-component ROSE encoding (A.B.C.D -> ABBCCCDDDD)
# and the newer upstream three-component encoding (A.B.C -> A*10000000 + B*100 + C).
set(ROSE_SCM_DATE_FILE "${PROJECT_SOURCE_DIR}/config/SCM_DATE")
if(NOT EXISTS "${ROSE_SCM_DATE_FILE}")
  message(FATAL_ERROR "ROSE SCM Date file not found: ${ROSE_SCM_DATE_FILE}")
endif()

file(STRINGS ${ROSE_SCM_VERSION_FILE} ROSE_SCM_VERSION_ID LIMIT_COUNT 1)

# Parse version into components for package configuration.
# Note: PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, etc. are also set by project() command.
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(\\.([0-9]+))?$" _version_match "${ROSE_PACKAGE_VERSION}")
if(NOT _version_match)
  message(FATAL_ERROR "Invalid ROSE version string: ${ROSE_PACKAGE_VERSION}")
endif()
set(ROSE_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(ROSE_VERSION_MINOR "${CMAKE_MATCH_2}")
set(ROSE_VERSION_PATCH "${CMAKE_MATCH_3}")
set(ROSE_VERSION_HAS_BUILD "${CMAKE_MATCH_4}")
set(ROSE_VERSION_BUILD "${CMAKE_MATCH_5}")
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
if(NOT "${ROSE_VERSION_HAS_BUILD}" STREQUAL "")
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

file(READ ${ROSE_SCM_DATE_FILE} ROSE_VERSION_FROM_SCM_DATE)
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

# Results
message(STATUS "The ROSE version integer is ${ROSE_VERSION_INT}")
message(STATUS "The ROSE version string is ${ROSE_PACKAGE_VERSION}")

# Paths to install header, executable, and libraries
set(INCLUDE_INSTALL_DIR "include/rose")
set(INCLUDE_INSTALL_DIR_FOR_INTRINSICS "include")
set(BIN_INSTALL_DIR     "bin")
set(LIB_INSTALL_DIR     "${ROSE_LIB_DIR_NAME}")
set(CLANG_BUILTIN_HEADERS_INSTALL_DIR "${INCLUDE_INSTALL_DIR_FOR_INTRINSICS}/clang")

# A new definition to tweak code for cmake
set(USE_CMAKE 1)

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

########################################################################################################################
# ROSETTA
########################################################################################################################

########################################################################################################################
# Compiler toolchain features
########################################################################################################################

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -DHAVE_CONFIG_H -fPIC ")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions -DHAVE_CONFIG_H -fPIC ")

  # Ensure C++11 ABI compatibility
  if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")
  endif()

  if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")
  endif()

# Set ROSE_SHLIBPATH_VAR. Linux-only uses LD_LIBRARY_PATH.
set(ROSE_SHLIBPATH_VAR "LD_LIBRARY_PATH")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.8)
       set(ROSE_USING_GCC_VERSION_LATER_4_8 1 CACHE INTERNAL "")
    endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
       set(ROSE_USING_GCC_VERSION_LATER_6_0 1 CACHE INTERNAL "")
    endif()
endif()
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.1)
       set(ROSE_USING_GCC_VERSION_LATER_6_1 1 CACHE INTERNAL "")
    endif()
endif()
# message("GNU 6.1 CHECK:" ${ROSE_USING_GCC_VERSION_LATER_6_1})

########################################################################################################################
# Analyzable languages supported by ROSE
########################################################################################################################
# C/C++
option(ENABLE-C "Enable C/C++ analysis" ON)
if(REX_ENABLE_WASM)
  set(ENABLE-C ON CACHE BOOL "Enable C/C++ analysis" FORCE)
endif()
if(ENABLE-C)
  set(ROSE_BUILD_CXX_LANGUAGE_SUPPORT 1)
  set(ROSE_BUILD_C_LANGUAGE_SUPPORT 1)
endif()

# Honor LLVM_HOME early so both C and Fortran feature detection resolve the
# same LLVM installation.
if(DEFINED ENV{LLVM_HOME})
  set(LLVM_ROOT "$ENV{LLVM_HOME}")
  set(Clang_ROOT "$ENV{LLVM_HOME}")
endif()

# CUDA
option(ENABLE-CUDA "Enable CUDA analysis" OFF) # OFF because lack of CUDA is not handled properly
if(REX_ENABLE_WASM)
  set(ENABLE-CUDA OFF CACHE BOOL "Enable CUDA analysis" FORCE)
endif()
if(ENABLE-CUDA)
  find_package(CUDA REQUIRED)
  set(ROSE_BUILD_CUDA_LANGUAGE_SUPPORT 1)
endif()

# Fortran (disabled by default to avoid requiring a Fortran toolchain for basic builds)
option(ENABLE-FORTRAN "Enable Fortran analysis." OFF)

# Flang frontend
option(ENABLE-FORTRAN-FLANG "Enable Flang frontend support." ON)
if(REX_ENABLE_WASM)
  set(ENABLE-FORTRAN OFF CACHE BOOL "Enable Fortran analysis." FORCE)
  set(ENABLE-FORTRAN-FLANG OFF CACHE BOOL "Enable Flang frontend support." FORCE)
endif()
set(FLANG_ROOT "" CACHE PATH "Path to Flang installation (lib/, include/).")

# Legacy tests still reference enable-fortran (lowercase).
set(enable-fortran "${ENABLE-FORTRAN}")

if(ENABLE-FORTRAN AND NOT ENABLE-FORTRAN-FLANG)
  message(FATAL_ERROR "Fortran analysis requires the Flang frontend. Set -DENABLE-FORTRAN-FLANG=ON, or disable Fortran (-DENABLE-FORTRAN=OFF).")
endif()

if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  # Import LLVM exactly once. ClangConfig imports the matching LLVMConfig, so
  # when C/C++ support is enabled we load Clang and reuse LLVM from there.
  if(ENABLE-C)
    if(NOT Clang_FOUND)
      find_package(Clang REQUIRED CONFIG)
    endif()
  elseif(NOT LLVM_FOUND)
    find_package(LLVM REQUIRED CONFIG)
  endif()

  set(_flang_search_dirs "")
  if(FLANG_ROOT)
    list(APPEND _flang_search_dirs "${FLANG_ROOT}/lib" "${FLANG_ROOT}/lib64")
  endif()
  if(LLVM_LIBRARY_DIRS)
    list(APPEND _flang_search_dirs ${LLVM_LIBRARY_DIRS})
  endif()
  set(FLANG_INCLUDE_DIRS "")
  if(FLANG_ROOT)
    list(APPEND FLANG_INCLUDE_DIRS "${FLANG_ROOT}/include")
  endif()
  if(LLVM_INCLUDE_DIRS)
    list(APPEND FLANG_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS})
  endif()
  if(NOT FLANG_INCLUDE_DIRS)
    find_program(ROSE_LLVM_CONFIG_EXECUTABLE NAMES llvm-config-22 llvm-config)
    if(ROSE_LLVM_CONFIG_EXECUTABLE)
      execute_process(
        COMMAND ${ROSE_LLVM_CONFIG_EXECUTABLE} --includedir
        OUTPUT_VARIABLE _rose_llvm_includedir
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_rose_llvm_includedir)
        set(FLANG_INCLUDE_DIRS "${_rose_llvm_includedir}")
      endif()
      unset(_rose_llvm_includedir)
    endif()
    unset(ROSE_LLVM_CONFIG_EXECUTABLE)
  endif()
  list(REMOVE_DUPLICATES FLANG_INCLUDE_DIRS)
  set(FLANG_INCLUDE_DIRS "${FLANG_INCLUDE_DIRS}" CACHE INTERNAL "Flang include dirs")
  set(FLANG_INTRINSIC_INCLUDE_DIR "")
  foreach(_flang_include_dir IN LISTS FLANG_INCLUDE_DIRS)
    if(EXISTS "${_flang_include_dir}/flang")
      set(FLANG_INTRINSIC_INCLUDE_DIR "${_flang_include_dir}/flang")
      break()
    endif()
  endforeach()
  if(NOT FLANG_INTRINSIC_INCLUDE_DIR)
    message(FATAL_ERROR
      "Flang intrinsic include directory not found under FLANG_INCLUDE_DIRS: "
      "${FLANG_INCLUDE_DIRS}")
  endif()
  set(FLANG_INTRINSIC_INCLUDE_DIR "${FLANG_INTRINSIC_INCLUDE_DIR}" CACHE INTERNAL
      "Flang intrinsic module/header include dir")

  if(NOT DEFINED LLVM_VERSION_MAJOR)
    if(LLVM_PACKAGE_VERSION)
      string(REPLACE "." ";" _rose_llvm_version_parts "${LLVM_PACKAGE_VERSION}")
      list(GET _rose_llvm_version_parts 0 LLVM_VERSION_MAJOR)
      unset(_rose_llvm_version_parts)
    endif()
  endif()

  if(DEFINED LLVM_VERSION_MAJOR AND LLVM_VERSION_MAJOR LESS LLVM_REQUIRED_MAJOR)
    message(FATAL_ERROR "REX requires LLVM ${LLVM_REQUIRED_MAJOR} or newer. Found LLVM ${LLVM_VERSION_MAJOR}.")
  endif()

  set(_flang_lib_names FortranParser FortranSemantics
    FortranEvaluate FortranLower FortranDecimal FortranSupport)
  set(_flang_optional_lib_names FortranCommon FortranRuntime)
  set(FLANG_LIBS "")
  set(_flang_missing "")
  foreach(_flang_lib ${_flang_lib_names})
    unset(_flang_lib_path CACHE)
    find_library(_flang_lib_path NAMES ${_flang_lib} PATHS ${_flang_search_dirs} NO_DEFAULT_PATH)
    if(NOT _flang_lib_path)
      find_library(_flang_lib_path NAMES ${_flang_lib})
    endif()
    if(_flang_lib_path)
      list(APPEND FLANG_LIBS ${_flang_lib_path})
    else()
      list(APPEND _flang_missing ${_flang_lib})
    endif()
  endforeach()
  if(_flang_missing)
    message(FATAL_ERROR "Flang libraries not found: ${_flang_missing}. Set -DFLANG_ROOT=... or disable ENABLE-FORTRAN-FLANG.")
  endif()

  foreach(_flang_lib ${_flang_optional_lib_names})
    unset(_flang_lib_path CACHE)
    find_library(_flang_lib_path NAMES ${_flang_lib} PATHS ${_flang_search_dirs} NO_DEFAULT_PATH)
    if(NOT _flang_lib_path)
      find_library(_flang_lib_path NAMES ${_flang_lib})
    endif()
    if(_flang_lib_path)
      list(APPEND FLANG_LIBS ${_flang_lib_path})
    endif()
  endforeach()

  if(LLVM_LIBRARY_DIRS)
    file(GLOB _rose_flang_rt_candidates
      "${LLVM_LIBRARY_DIRS}/clang/*/lib/*/libflang_rt.runtime*"
      "${LLVM_LIBRARY_DIRS}/clang/*/lib/libflang_rt.runtime*")
    if(_rose_flang_rt_candidates)
      list(APPEND FLANG_LIBS ${_rose_flang_rt_candidates})
    endif()
    unset(_rose_flang_rt_candidates)
  endif()

  include(TestBigEndian)
  test_big_endian(ROSE_FLANG_BIG_ENDIAN)
  if(ROSE_FLANG_BIG_ENDIAN)
    set(ROSE_FLANG_ENDIAN_DEFINES FLANG_BIG_ENDIAN=1)
  else()
    set(ROSE_FLANG_ENDIAN_DEFINES FLANG_LITTLE_ENDIAN=1)
  endif()

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

# OpenCL
option(ENABLE-OPENCL "Enable OpenCL analysis" ON)
if(REX_ENABLE_WASM)
  set(ENABLE-OPENCL OFF CACHE BOOL "Enable OpenCL analysis" FORCE)
endif()
if(ENABLE-OPENCL)
  find_package(OpenCL)
  set(ROSE_BUILD_OPENCL_LANGUAGE_SUPPORT 1)
  find_path(with-opencl-inc NAMES cl.h DOC "For OpenCL runtime library")
  find_library(with-opencl-lib OpenCL DOC "OpenCL library for runtime examples")
endif()

# Fortran
if(ENABLE-FORTRAN)
  # Detect Fortran compiler using the same helper function
  detect_compiler(Fortran CMAKE_Fortran_COMPILER FC "Fortran" "flang-${LLVM_REQUIRED_MAJOR}" "flang")
  set(ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT 1)
  enable_language(Fortran)
  set(BACKEND_FORTRAN_COMPILER "${CMAKE_Fortran_COMPILER}")
  get_filename_component(BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH "${BACKEND_FORTRAN_COMPILER}" NAME)
  string(REGEX MATCH "[a-zA-Z0-9/.+-]+" BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH "${BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH}")
  if(NOT "${CMAKE_Fortran_COMPILER_ID}" MATCHES "^(LLVMFlang|Flang)$")
    message(FATAL_ERROR "REX requires LLVM Flang (flang) as the Fortran backend. Found: ${CMAKE_Fortran_COMPILER_ID} (${BACKEND_FORTRAN_COMPILER}).")
  endif()
  message(STATUS "Using LLVM Flang backend: ${BACKEND_FORTRAN_COMPILER}")
endif()

# The C preprocessor is enabled automatically if C, C++, or Fortran is enabled
if(ENABLE-C OR ENABLE-FORTRAN)
  set(ENABLE-CPP ON)
  set(ROSE_BUILD_CPP_LANGUAGE_SUPPORT 1)
endif()

########################################################################################################################
########################################################################################################################
# Clang/LLVM frontend (required when C/C++ support is enabled)
########################################################################################################################

# REX uses Clang/LLVM frontend exclusively
# Only required when C/C++ analysis is enabled
if(ENABLE-C)

# Set ROSE_USE_CLANG_FRONTEND for backward compatibility with existing code
set(ROSE_USE_CLANG_FRONTEND 1)

# Find LLVM/Clang (required for C/C++ support)
if(NOT Clang_FOUND)
  find_package(Clang REQUIRED CONFIG)
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

# Determine clang major version for conditional library selection.
if(NOT DEFINED CLANG_VERSION_MAJOR OR CLANG_VERSION_MAJOR STREQUAL "0")
  if(LLVM_PACKAGE_VERSION)
    string(REPLACE "." ";" LLVM_VERSION_PARTS "${LLVM_PACKAGE_VERSION}")
    list(GET LLVM_VERSION_PARTS 0 CLANG_VERSION_MAJOR)
  else()
    execute_process(
      COMMAND "${CMAKE_SOURCE_DIR}/config/getClangMajorVersionNumber.sh"
      OUTPUT_VARIABLE CLANG_VERSION_MAJOR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()
if(NOT CLANG_VERSION_MAJOR)
  set(CLANG_VERSION_MAJOR 0)
endif()

if(CLANG_VERSION_MAJOR LESS LLVM_REQUIRED_MAJOR)
  message(FATAL_ERROR "REX requires LLVM/Clang ${LLVM_REQUIRED_MAJOR} or newer. Found Clang ${CLANG_VERSION_MAJOR}.")
endif()

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

set(_rose_shared_llvm_link_item "")
if(NOT REX_ENABLE_WASM AND TARGET LLVM)
  foreach(_rose_prop
      IMPORTED_LOCATION
      IMPORTED_LOCATION_RELEASE
      IMPORTED_LOCATION_RELWITHDEBINFO
      IMPORTED_LOCATION_MINSIZEREL
      IMPORTED_LOCATION_DEBUG)
    if(NOT _rose_shared_llvm_link_item)
      get_target_property(_rose_candidate LLVM ${_rose_prop})
      if(_rose_candidate AND _rose_candidate MATCHES "\.so(\.[0-9.]+)?$")
        set(_rose_shared_llvm_link_item "${_rose_candidate}")
      endif()
    endif()
  endforeach()
endif()

if(NOT REX_ENABLE_WASM AND NOT _rose_shared_llvm_link_item)
  set(_rose_saved_library_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
  if(DEFINED LLVM_VERSION_MAJOR)
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".so.${LLVM_VERSION_MAJOR}")
    if(DEFINED LLVM_VERSION_MINOR)
      list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES
        ".so.${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")
    endif()
  endif()

  set(_rose_llvm_candidates LLVM)
  if(DEFINED LLVM_VERSION_MAJOR AND NOT LLVM_VERSION_MAJOR STREQUAL "")
    list(APPEND _rose_llvm_candidates LLVM-${LLVM_VERSION_MAJOR})
  endif()
  if(LLVM_PACKAGE_VERSION)
    list(APPEND _rose_llvm_candidates LLVM-${LLVM_PACKAGE_VERSION})
  endif()

  find_library(_rose_shared_llvm_link_item
    NAMES ${_rose_llvm_candidates}
    PATHS ${LLVM_LIBRARY_DIRS}
    NO_DEFAULT_PATH)
  if(NOT _rose_shared_llvm_link_item)
    find_library(_rose_shared_llvm_link_item NAMES ${_rose_llvm_candidates})
  endif()

  set(CMAKE_FIND_LIBRARY_SUFFIXES "${_rose_saved_library_suffixes}")
  unset(_rose_saved_library_suffixes)
  unset(_rose_llvm_candidates)
endif()

if(NOT REX_ENABLE_WASM AND NOT _rose_shared_llvm_link_item)
  message(FATAL_ERROR
    "REX Clang frontend requires a shared LLVM library (libLLVM). "
    "Install/build LLVM with LLVM_BUILD_LLVM_DYLIB=ON and rerun CMake.")
endif()

if(REX_ENABLE_WASM)
  set(LLVM_LIBS "")
  message(STATUS "Using static LLVM/Clang component targets for REX WASM")
else()
  set(LLVM_LIBS "${_rose_shared_llvm_link_item}")
  message(STATUS "Using required shared LLVM library for Clang frontend: ${LLVM_LIBS}")
endif()
unset(_rose_candidate)
unset(_rose_prop)
unset(_rose_shared_llvm_link_item)

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
else()
  set(CLANG_LIBS
    clangFrontendTool
    clangStaticAnalyzerFrontend
    clangStaticAnalyzerCheckers
    clangStaticAnalyzerCore
    clangIndex
    clangFrontend
    clangCodeGen
    clangRewrite
    clangSerialization
    clangDriver
    clangParse
    clangSema
    clangAnalysis
    clangAST
    clangBasic
    clangEdit
    clangLex)

  list(APPEND CLANG_LIBS
    clangSupport
    clangCrossTU
    clangASTMatchers
    clangAPINotes
    clangTooling)
endif()

set(_rose_clang_cpp_candidates
  clang-cpp
  clang-cpp${CLANG_VERSION_MAJOR}
  clang-cpp${LLVM_PACKAGE_VERSION}
  clang-cpp21)
set(_rose_clang_cpp_search_dirs
  ${LLVM_LIBRARY_DIRS}
  ${CLANG_LIBRARY_DIRS})
if(LLVM_DIR)
  set(_rose_llvm_prefix "")
  if(EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
    get_filename_component(_rose_llvm_dir_name "${LLVM_DIR}" NAME)
    if(_rose_llvm_dir_name STREQUAL "llvm")
      get_filename_component(_rose_llvm_prefix "${LLVM_DIR}/../../.." ABSOLUTE)
    elseif(_rose_llvm_dir_name STREQUAL "cmake")
      get_filename_component(_rose_llvm_prefix "${LLVM_DIR}/.." ABSOLUTE)
    else()
      get_filename_component(_rose_llvm_prefix "${LLVM_DIR}/.." ABSOLUTE)
    endif()
  elseif(EXISTS "${LLVM_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
    set(_rose_llvm_prefix "${LLVM_DIR}")
  else()
    set(_rose_llvm_prefix "${LLVM_DIR}")
  endif()
  if(_rose_llvm_prefix)
    list(APPEND _rose_clang_cpp_search_dirs
      "${_rose_llvm_prefix}/lib"
      "${_rose_llvm_prefix}/lib64")
  endif()
endif()
if(CMAKE_LIBRARY_ARCHITECTURE)
  foreach(_rose_libdir IN LISTS _rose_clang_cpp_search_dirs)
    list(APPEND _rose_clang_cpp_search_dirs
      "${_rose_libdir}/${CMAKE_LIBRARY_ARCHITECTURE}")
  endforeach()
endif()
list(REMOVE_DUPLICATES _rose_clang_cpp_search_dirs)
set(_rose_saved_library_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
if(UNIX AND NOT APPLE)
  if(CLANG_VERSION_MAJOR)
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".so.${CLANG_VERSION_MAJOR}")
  endif()
  if(DEFINED LLVM_VERSION_MAJOR)
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".so.${LLVM_VERSION_MAJOR}")
    if(DEFINED LLVM_VERSION_MINOR)
      list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES
        ".so.${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")
    endif()
  endif()
endif()
if(NOT REX_ENABLE_WASM)
  find_library(CLANG_CPP_LIBRARY
    NAMES ${_rose_clang_cpp_candidates}
    PATHS ${_rose_clang_cpp_search_dirs}
    NO_DEFAULT_PATH)
  if(NOT CLANG_CPP_LIBRARY)
    find_library(CLANG_CPP_LIBRARY NAMES ${_rose_clang_cpp_candidates})
  endif()
endif()
set(CMAKE_FIND_LIBRARY_SUFFIXES "${_rose_saved_library_suffixes}")
if(CLANG_CPP_LIBRARY)
  # Prefer the unified shared clang-cpp over static component libs to avoid
  # ODR/runtime lifetime mismatches with the shared LLVM dylib.
  set(CLANG_LIBS ${CLANG_CPP_LIBRARY})
  if(ENABLE-SANITIZER)
    message(STATUS "Using shared libclang-cpp for sanitizer build: ${CLANG_CPP_LIBRARY}")
  else()
    message(STATUS "Using shared libclang-cpp: ${CLANG_CPP_LIBRARY}")
  endif()
elseif(ENABLE-SANITIZER)
  message(FATAL_ERROR "ENABLE-SANITIZER requires libclang-cpp to avoid ODR mismatches with static Clang libraries. Install libclang-cpp or disable ENABLE-SANITIZER.")
endif()
unset(_rose_clang_cpp_candidates)
unset(_rose_clang_cpp_search_dirs)
unset(_rose_llvm_dir_name)
unset(_rose_llvm_prefix)
unset(_rose_saved_library_suffixes)

endif() # ENABLE-C

########################################################################################################################
# System features
########################################################################################################################

# A set of common features including endian, stdio.h, printf, size of long int, etc.
set(CMAKE_REQUIRED_QUIET ${QUIET})
include(ConfigureChecks)
set(CMAKE_REQUIRED_QUIET FALSE)

find_package(Perl REQUIRED)

########################################################################################################################
# Miscellaneous user-selectable features
########################################################################################################################

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

option(ENABLE-ADVANCED-WARNINGS "Support for an advanced uniform warning level for ROSE development" OFF)
if(ENABLE-ADVANCED-WARNINGS)
  message(WARNING "Using an advanced uniform warning level for ROSE development")
  set(ROSE_USE_UNIFORM_ADVANCED_WARNINGS_SUPPORT 1)
endif()

option(REX_WARNINGS_AS_ERRORS "Treat C/C++ compiler warnings as errors" ON)
if(REX_WARNINGS_AS_ERRORS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
      $<$<COMPILE_LANGUAGE:C>:-Werror>
      $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
  else()
    message(WARNING "REX_WARNINGS_AS_ERRORS requested but compiler does not support warning-as-error flags")
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
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    list(REMOVE_DUPLICATES _rose_requested_sanitizers)
    list(JOIN _rose_requested_sanitizers "," _rose_sanitizer_list)
    set(_rose_sanitizer_link_flags "-fsanitize=${_rose_sanitizer_list}")
    set(_rose_sanitizer_compile_flags
        "${_rose_sanitizer_link_flags}"
        "-fno-omit-frame-pointer"
        "-fno-optimize-sibling-calls")
    include(CheckCSourceCompiles)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "${_rose_sanitizer_link_flags}")
    set(CMAKE_REQUIRED_LINK_OPTIONS "${_rose_sanitizer_link_flags}")
    check_c_source_compiles("int main(void) { return 0; }" ROSE_C_SUPPORTS_SANITIZERS)
    check_cxx_source_compiles("int main() { return 0; }" ROSE_CXX_SUPPORTS_SANITIZERS)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT ROSE_C_SUPPORTS_SANITIZERS OR NOT ROSE_CXX_SUPPORTS_SANITIZERS)
      message(FATAL_ERROR "Sanitizers requested but compiler does not support ${_rose_sanitizer_link_flags}")
    endif()
    foreach(_rose_sanitizer_flag IN LISTS _rose_sanitizer_compile_flags)
      add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:${_rose_sanitizer_flag}>
        $<$<COMPILE_LANGUAGE:CXX>:${_rose_sanitizer_flag}>)
    endforeach()
    add_link_options(${_rose_sanitizer_link_flags})
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
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_rose_lcov_flags "-g" "-fprofile-arcs" "-ftest-coverage")
    add_compile_options(
      $<$<COMPILE_LANGUAGE:C>:${_rose_lcov_flags}>
      $<$<COMPILE_LANGUAGE:CXX>:${_rose_lcov_flags}>)
    add_link_options("--coverage")
    set(ROSE_USE_LCOV 1)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(WARNING "ENABLE-LCOV requested with Clang; coverage flags are not configured for lcov")
  else()
    message(WARNING "ENABLE-LCOV requested but compiler does not support lcov flags")
  endif()
endif()

option(ENABLE-GNU-EXTENSIONS "Enable internal support in ROSE for GNU language extensions" OFF)
if(ENABLE-GNU-EXTENSIONS)
  set(ROSE_SUPPORT_GNU_EXTENSIONS TRUE)
endif()

option(ENABLE-SMALLER-GENERATED-FILES "Build smaller generated Sage III IR files" OFF)
set(enable-smaller-generated-files ${ENABLE-SMALLER-GENERATED-FILES})

set(ROSE_ALLOC_MEMSET "0" CACHE STRING
    "Memory pool protection: 0=none, 1=command-line (not implemented), 2=zero new/delete, 3=aggressive different values")
set_property(CACHE ROSE_ALLOC_MEMSET PROPERTY STRINGS 0 1 2 3)
if(NOT ROSE_ALLOC_MEMSET MATCHES "^[0-3]$")
  message(FATAL_ERROR "ROSE_ALLOC_MEMSET must be one of 0, 1, 2, or 3. Current value: ${ROSE_ALLOC_MEMSET}")
endif()

set(WITH-VALGRIND "" CACHE PATH "Valgrind installation prefix (empty to disable)")
set(WITH-VALGRIND-BIN "" CACHE PATH "Valgrind bin directory")
set(WITH-VALGRIND-INCLUDE "" CACHE PATH "Valgrind include directory")
set(WITH-VALGRIND-LIB "" CACHE PATH "Valgrind library directory")
set(WITH-LLVM-OMP-RUNTIME-LIBRARY "" CACHE PATH "LLVM OpenMP runtime library directory")

# To use Bison, set BISON_ROOT to the Bison installation prefix. To avoid Bison, use BISON_ROOT=no
include(FindBison)
find_bison()

# Zlib may be required by libharu. To find it in a special place, set ZLIB_ROOT to its installation prefix.
include(FindZlib)
find_zlib()
if(REX_ENABLE_WASM)
  set(ZLIB_FOUND FALSE)
  set(ZLIB_LIBRARY "")
  set(ZLIB_LIBRARIES "")
endif()

set(VALGRIND_INSTALL_PATH "")
set(VALGRIND_BIN_PATH "")
set(VALGRIND_INCLUDE_PATH "")
set(VALGRIND_LIBRARY_PATH "")
set(VALGRIND_BINARY "")

set(_rose_with_vars
  WITH-VALGRIND
  WITH-VALGRIND-BIN
  WITH-VALGRIND-INCLUDE
  WITH-VALGRIND-LIB
  WITH-LLVM-OMP-RUNTIME-LIBRARY)
foreach(_rose_var IN LISTS _rose_with_vars)
  if("${${_rose_var}}" STREQUAL "no" OR "${${_rose_var}}" STREQUAL "OFF")
    set(${_rose_var} "")
  endif()
endforeach()
unset(_rose_with_vars)
unset(_rose_var)

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
  set(VALGRIND_INSTALL_PATH "${WITH-VALGRIND}")
  set(VALGRIND_BIN_PATH "${WITH-VALGRIND}/bin")
  set(VALGRIND_INCLUDE_PATH "${WITH-VALGRIND}/include")
  set(VALGRIND_LIBRARY_PATH "${WITH-VALGRIND}/lib")
endif()
if(WITH-VALGRIND-BIN)
  set(VALGRIND_BIN_PATH "${WITH-VALGRIND-BIN}")
endif()
if(WITH-VALGRIND-INCLUDE)
  set(VALGRIND_INCLUDE_PATH "${WITH-VALGRIND-INCLUDE}")
endif()
if(WITH-VALGRIND-LIB)
  set(VALGRIND_LIBRARY_PATH "${WITH-VALGRIND-LIB}")
endif()

set(ROSE_VALGRIND_REQUESTED OFF)
if(WITH-VALGRIND OR WITH-VALGRIND-BIN OR WITH-VALGRIND-INCLUDE OR WITH-VALGRIND-LIB)
  set(ROSE_VALGRIND_REQUESTED ON)
endif()

if(ROSE_VALGRIND_REQUESTED)
  set(_rose_valgrind_candidate "${VALGRIND_BIN_PATH}/valgrind")
  if(VALGRIND_BIN_PATH AND EXISTS "${_rose_valgrind_candidate}")
    set(VALGRIND_BINARY "${_rose_valgrind_candidate}")
  elseif(VALGRIND_BIN_PATH)
    find_program(VALGRIND_BINARY valgrind PATHS "${VALGRIND_BIN_PATH}")
  else()
    find_program(VALGRIND_BINARY valgrind)
  endif()
  if(NOT VALGRIND_BINARY)
    message(FATAL_ERROR "Valgrind binary is missing, can't configure with Valgrind. Set WITH-VALGRIND or WITH-VALGRIND-BIN.")
  endif()
  unset(_rose_valgrind_candidate)
  execute_process(
    COMMAND "${VALGRIND_BINARY}" --version
    RESULT_VARIABLE _rose_valgrind_status
    OUTPUT_VARIABLE _rose_valgrind_version
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _rose_valgrind_status EQUAL 0)
    message(FATAL_ERROR "Could not execute your Valgrind binary")
  endif()
  if(_rose_valgrind_version)
    message(STATUS "Valgrind version: ${_rose_valgrind_version}")
  endif()
  set(ROSE_USE_VALGRIND 1)
else()
  unset(VALGRIND_BINARY)
  find_program(VALGRIND_BINARY valgrind
    PATHS ENV PATH /usr/bin /usr/local/bin
    NO_CMAKE_FIND_ROOT_PATH)
  if(VALGRIND_BINARY AND NOT VALGRIND_INCLUDE_PATH)
    unset(VALGRIND_INCLUDE_PATH)
    find_path(VALGRIND_INCLUDE_PATH valgrind/valgrind.h
      PATHS /usr/include /usr/local/include
      NO_DEFAULT_PATH)
  endif()
  if(VALGRIND_BINARY AND VALGRIND_INCLUDE_PATH)
    set(ROSE_USE_VALGRIND 1)
    message(STATUS "Auto-enabled Valgrind support: ${VALGRIND_BINARY}")
  endif()
endif()
if(ROSE_USE_VALGRIND AND NOT VALGRIND_INCLUDE_PATH)
  unset(VALGRIND_INCLUDE_PATH)
  find_path(VALGRIND_INCLUDE_PATH valgrind/valgrind.h
    PATHS /usr/include /usr/local/include
    NO_DEFAULT_PATH)
endif()
if(ROSE_USE_VALGRIND AND NOT VALGRIND_INCLUDE_PATH)
  message(FATAL_ERROR "Valgrind headers not found. Set WITH-VALGRIND or WITH-VALGRIND-INCLUDE.")
endif()
if(VALGRIND_INCLUDE_PATH AND NOT EXISTS "${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h")
  message(FATAL_ERROR "valgrind.h is missing, can't compile with Valgrind")
endif()

set(_rose_memcheck_valgrind "")
if(VALGRIND_BINARY)
  set(_rose_memcheck_valgrind "${VALGRIND_BINARY}")
endif()
if(NOT _rose_memcheck_valgrind)
  find_program(_rose_memcheck_valgrind valgrind
    PATHS ENV PATH /usr/bin /usr/local/bin
    NO_CMAKE_FIND_ROOT_PATH)
  if(_rose_memcheck_valgrind)
    message(STATUS "Found Valgrind for CTest memcheck: ${_rose_memcheck_valgrind}")
  endif()
endif()
if(NOT _rose_memcheck_valgrind AND UNIX)
  execute_process(
    COMMAND /bin/sh -c "command -v valgrind"
    RESULT_VARIABLE _rose_valgrind_status
    OUTPUT_VARIABLE _rose_memcheck_valgrind
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_rose_valgrind_status EQUAL 0 AND _rose_memcheck_valgrind)
    message(STATUS "Found Valgrind for CTest memcheck: ${_rose_memcheck_valgrind}")
  else()
    set(_rose_memcheck_valgrind "")
  endif()
  unset(_rose_valgrind_status)
endif()

set(ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE 1)
set(ROSE_CTEST_RTH_TIMEOUT_SCALE 1)
if(ROSE_USE_VALGRIND)
  set(ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE 4)
  set(ROSE_CTEST_RTH_TIMEOUT_SCALE 4)
  if(REX_EXTENDED_TEST_TIMEOUTS)
    set(ROSE_CTEST_RTH_TIMEOUT_SCALE 24)
  endif()
endif()

set(ROSE_CTEST_MEMCHECK_COMMAND "")
set(ROSE_CTEST_MEMCHECK_OPTIONS "")
set(ROSE_CTEST_MEMCHECK_SUPPRESSIONS "")
set(ROSE_CTEST_MEMCHECK_TYPE "")
if(_rose_memcheck_valgrind)
  set(ROSE_REAL_VALGRIND "${_rose_memcheck_valgrind}")
  set(ROSE_CTEST_MEMCHECK_RTH_TIMEOUT_SCALE ${ROSE_CTEST_RTH_TIMEOUT_SCALE})
  set(ROSE_CTEST_MEMCHECK_WRAPPER
      "${CMAKE_BINARY_DIR}/scripts/rex-ctest-valgrind-wrapper")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/scripts")
  configure_file(cmake/rex-ctest-valgrind-wrapper.in
                 "${ROSE_CTEST_MEMCHECK_WRAPPER}" @ONLY)
  execute_process(COMMAND chmod +x "${ROSE_CTEST_MEMCHECK_WRAPPER}")
  set(ROSE_CTEST_MEMCHECK_COMMAND "${ROSE_CTEST_MEMCHECK_WRAPPER}")
  set(ROSE_CTEST_MEMCHECK_TYPE "Valgrind")
  set(ROSE_CTEST_MEMCHECK_SUPPRESSIONS "${CMAKE_SOURCE_DIR}/scripts/rex-suppressions-for-valgrind")
  set(_rose_memcheck_toolchain_major "${LLVM_REQUIRED_MAJOR}")
  if(DEFINED CLANG_VERSION_MAJOR AND CLANG_VERSION_MAJOR AND NOT CLANG_VERSION_MAJOR STREQUAL "0")
    set(_rose_memcheck_toolchain_major "${CLANG_VERSION_MAJOR}")
  elseif(DEFINED LLVM_VERSION_MAJOR AND LLVM_VERSION_MAJOR)
    set(_rose_memcheck_toolchain_major "${LLVM_VERSION_MAJOR}")
  endif()
  set(_rose_memcheck_trace_children_skip
      "*/clang++-${_rose_memcheck_toolchain_major}"
      "*/clang-${_rose_memcheck_toolchain_major}"
      "*/flang-${_rose_memcheck_toolchain_major}"
      "*/flang"
      "*/clang++"
      "*/clang"
      "*/g++"
      "*/gcc"
      "*/c++"
      "*/cc"
      "*/grep"
      "*/cmp"
      "*/mktemp"
      "*/dash"
      "*/bash")

  set(_rose_memcheck_harness_tools
      egrep
      fgrep
      diff
      sed
      awk
      basename
      tr
      cut
      dirname
      sort
      uniq
      git
      head
      tail
      cmake
      ctest
      make
      ninja
      sh
      perl
      python
      python3
      tar
      gzip
      bzip2
      find
      xz
      zip
      unzip
      cat
      cp
      env
      ln
      mkdir
      mv
      pwd
      readlink
      realpath
      rm
      rmdir
      seq
      tee
      timeout
      touch
      wc
      xargs
      install)
  foreach(_rose_memcheck_harness_tool IN LISTS _rose_memcheck_harness_tools)
    list(APPEND _rose_memcheck_trace_children_skip "*/${_rose_memcheck_harness_tool}")
  endforeach()
  list(APPEND _rose_memcheck_trace_children_skip "*/scripts/policies/*")

  function(_rose_append_memcheck_child_skip _rose_compiler)
    if(NOT _rose_compiler)
      return()
    endif()
    get_filename_component(_rose_compiler_name "${_rose_compiler}" NAME)
    if(_rose_compiler_name)
      list(APPEND _rose_memcheck_trace_children_skip "*/${_rose_compiler_name}")
    endif()
    set(_rose_memcheck_trace_children_skip "${_rose_memcheck_trace_children_skip}" PARENT_SCOPE)
  endfunction()
  foreach(_rose_compiler IN ITEMS
      "${CMAKE_C_COMPILER}"
      "${CMAKE_CXX_COMPILER}"
      "${CMAKE_Fortran_COMPILER}"
      "${WITH-ALT-BACKEND-C-COMPILER}"
      "${WITH-ALT-BACKEND-CXX-COMPILER}"
      "${WITH-ALT-BACKEND-FORTRAN-COMPILER}")
    _rose_append_memcheck_child_skip("${_rose_compiler}")
  endforeach()
  list(REMOVE_DUPLICATES _rose_memcheck_trace_children_skip)
  list(JOIN _rose_memcheck_trace_children_skip "," _rose_memcheck_trace_children_skip_arg)
  set(ROSE_CTEST_MEMCHECK_OPTIONS "--leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite,indirect,possible --error-exitcode=1 --max-stackframe=2047456 --num-callers=32 --trace-children=yes --trace-children-skip=${_rose_memcheck_trace_children_skip_arg} --trace-children-skip-by-arg=*gtest_internal_run_death_test*")
  unset(_rose_compiler)
  unset(_rose_memcheck_harness_tool)
  unset(_rose_memcheck_harness_tools)
  unset(_rose_memcheck_trace_children_skip_arg)
  unset(_rose_memcheck_trace_children_skip)
  unset(_rose_memcheck_toolchain_major)
  if(NOT EXISTS "${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}")
    message(WARNING "Valgrind suppressions file not found: ${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}")
    set(ROSE_CTEST_MEMCHECK_SUPPRESSIONS "")
  endif()
endif()
unset(_rose_memcheck_valgrind)
set(CTEST_MEMORYCHECK_COMMAND "${ROSE_CTEST_MEMCHECK_COMMAND}")
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "${ROSE_CTEST_MEMCHECK_OPTIONS}")
set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE "${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}")
set(ROSE_CTEST_CUSTOM_ENVIRONMENT "")
if(ROSE_USE_VALGRIND)
  set(ROSE_CTEST_CUSTOM_ENVIRONMENT
      "set(CTEST_CUSTOM_ENVIRONMENT \"ROSE_TEST_TIMEOUT_SCALE=${ROSE_CTEST_RTH_TIMEOUT_SCALE}\")")
  # Memcheck can exceed CTest's default 1500-second timeout on parser-heavy tests.
  set(_rose_dashboard_timeout 5400)
  if(REX_EXTENDED_TEST_TIMEOUTS)
    set(_rose_dashboard_timeout 22200)
  endif()
  set(DART_TESTING_TIMEOUT ${_rose_dashboard_timeout} CACHE STRING
      "Maximum time in seconds for CTest dashboard tests" FORCE)
  unset(_rose_dashboard_timeout)
endif()
if(ROSE_CTEST_MEMCHECK_COMMAND)
  set(MEMORYCHECK_COMMAND "${ROSE_CTEST_MEMCHECK_COMMAND}" CACHE FILEPATH
      "Path to the memory checking command used by CTest" FORCE)
  set(MEMORYCHECK_TYPE "${ROSE_CTEST_MEMCHECK_TYPE}" CACHE STRING
      "Memory checker type used by CTest" FORCE)
  set(MEMORYCHECK_COMMAND_OPTIONS "${ROSE_CTEST_MEMCHECK_OPTIONS}" CACHE STRING
      "Command line options for the CTest memory checker" FORCE)
  set(MEMORYCHECK_SUPPRESSIONS_FILE "${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}" CACHE FILEPATH
      "Suppressions file used by the CTest memory checker" FORCE)
else()
  set(MEMORYCHECK_COMMAND "" CACHE FILEPATH
      "Path to the memory checking command used by CTest" FORCE)
  set(MEMORYCHECK_TYPE "" CACHE STRING
      "Memory checker type used by CTest" FORCE)
  set(MEMORYCHECK_COMMAND_OPTIONS "" CACHE STRING
      "Command line options for the CTest memory checker" FORCE)
  set(MEMORYCHECK_SUPPRESSIONS_FILE "" CACHE FILEPATH
      "Suppressions file used by the CTest memory checker" FORCE)
endif()
include(CTest)

set(ROSE_LLVM_OPENMP_RUNTIME_DIR "")
set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "")
set(ROSE_LLVM_OPENMP_INCLUDE_DIR "")
if(REX_ENABLE_WASM AND ENABLE-C)
  set(ROSE_LLVM_OPENMP_INCLUDE_DIR "/rex/lib/clang/${CLANG_VERSION_MAJOR}/include")
  set(LLVM_OPENMP_INCLUDE_PATH "${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
  set(LLVM_OPENMP_LIB_PATH "")
  set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${ROSE_LLVM_OPENMP_INCLUDE_DIR}" CACHE INTERNAL
    "LLVM OpenMP header directory used by REX WASM")
  set(ROSE_LLVM_OPENMP_RUNTIME_DIR "" CACHE INTERNAL
    "Directory containing the LLVM OpenMP runtime library used by REX")
  set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "" CACHE INTERNAL
    "LLVM OpenMP runtime library selected by REX")
elseif(ENABLE-C)
  set(_rose_llvm_openmp_driver "")
  if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(_rose_llvm_openmp_driver "${CMAKE_C_COMPILER}")
  endif()
  if(NOT _rose_llvm_openmp_driver AND LLVM_TOOLS_BINARY_DIR)
    find_program(_rose_llvm_openmp_driver
      NAMES clang-${CLANG_VERSION_MAJOR} clang
      HINTS "${LLVM_TOOLS_BINARY_DIR}")
  endif()
  if(NOT _rose_llvm_openmp_driver)
    find_program(_rose_llvm_openmp_driver
      NAMES clang-${CLANG_VERSION_MAJOR} clang)
  endif()
  if(NOT _rose_llvm_openmp_driver)
    message(FATAL_ERROR
      "Unable to locate a Clang executable to resolve LLVM OpenMP paths.")
  endif()
  execute_process(
    COMMAND "${_rose_llvm_openmp_driver}" -dumpversion
    OUTPUT_VARIABLE _rose_llvm_openmp_driver_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_rose_llvm_openmp_driver_version)
    string(REGEX MATCH "^[0-9]+" _rose_llvm_openmp_driver_major
      "${_rose_llvm_openmp_driver_version}")
    if(_rose_llvm_openmp_driver_major AND
       NOT _rose_llvm_openmp_driver_major STREQUAL "${CLANG_VERSION_MAJOR}")
      message(FATAL_ERROR
        "OpenMP path resolution driver ${_rose_llvm_openmp_driver} reports "
        "major version ${_rose_llvm_openmp_driver_major}, but REX is "
        "configured for LLVM/Clang ${CLANG_VERSION_MAJOR}.")
    endif()
  endif()

  execute_process(
    COMMAND "${_rose_llvm_openmp_driver}" --print-resource-dir
    OUTPUT_VARIABLE _rose_clang_resource_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  set(_rose_llvm_openmp_runtime_names
    libiomp5.so libomp.so libomp.so.5 libomp5.so libiomp5.a libomp.a)
  set(_rose_llvm_openmp_runtime_hints "")
  if(WITH-LLVM-OMP-RUNTIME-LIBRARY)
    file(TO_CMAKE_PATH "${WITH-LLVM-OMP-RUNTIME-LIBRARY}"
      ROSE_LLVM_OPENMP_RUNTIME_DIR)
    list(APPEND _rose_llvm_openmp_runtime_hints
      "${ROSE_LLVM_OPENMP_RUNTIME_DIR}")
  endif()
  foreach(_rose_omp_probe_name IN LISTS _rose_llvm_openmp_runtime_names)
    execute_process(
      COMMAND "${_rose_llvm_openmp_driver}" -print-file-name=${_rose_omp_probe_name}
      OUTPUT_VARIABLE _rose_omp_probe_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(_rose_omp_probe_path)
      file(TO_CMAKE_PATH "${_rose_omp_probe_path}" _rose_omp_probe_path_norm)
      if(IS_ABSOLUTE "${_rose_omp_probe_path_norm}" AND
         EXISTS "${_rose_omp_probe_path_norm}")
        get_filename_component(_rose_omp_probe_dir
          "${_rose_omp_probe_path_norm}" DIRECTORY)
        list(APPEND _rose_llvm_openmp_runtime_hints
          "${_rose_omp_probe_dir}")
        unset(_rose_omp_probe_dir)
      endif()
      unset(_rose_omp_probe_path_norm)
    endif()
    unset(_rose_omp_probe_path)
  endforeach()
  if(LLVM_LIBRARY_DIRS)
    list(APPEND _rose_llvm_openmp_runtime_hints ${LLVM_LIBRARY_DIRS})
  endif()
  get_filename_component(_rose_llvm_openmp_driver_dir
    "${_rose_llvm_openmp_driver}" DIRECTORY)
  if(_rose_llvm_openmp_driver_dir)
    list(APPEND _rose_llvm_openmp_runtime_hints
      "${_rose_llvm_openmp_driver_dir}/../lib"
      "${_rose_llvm_openmp_driver_dir}/../lib64")
  endif()
  foreach(_rose_implicit_libdir IN LISTS
    CMAKE_C_IMPLICIT_LINK_DIRECTORIES
    CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES
    CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES)
    if(_rose_implicit_libdir)
      list(APPEND _rose_llvm_openmp_runtime_hints "${_rose_implicit_libdir}")
    endif()
  endforeach()
  if(_rose_clang_resource_dir)
    get_filename_component(_rose_clang_lib_dir
      "${_rose_clang_resource_dir}" DIRECTORY)
    get_filename_component(_rose_clang_lib_dir
      "${_rose_clang_lib_dir}" DIRECTORY)
    list(APPEND _rose_llvm_openmp_runtime_hints "${_rose_clang_lib_dir}")
    unset(_rose_clang_lib_dir)
  endif()
  if(CMAKE_LIBRARY_ARCHITECTURE)
    foreach(_rose_libdir IN LISTS _rose_llvm_openmp_runtime_hints)
      list(APPEND _rose_llvm_openmp_runtime_hints
        "${_rose_libdir}/${CMAKE_LIBRARY_ARCHITECTURE}")
    endforeach()
  endif()
  list(REMOVE_DUPLICATES _rose_llvm_openmp_runtime_hints)

  set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "")
  set(_rose_mismatched_openmp_runtime_library "")
  set(_rose_mismatched_openmp_runtime_major "")
  foreach(_rose_omp_runtime_dir IN LISTS _rose_llvm_openmp_runtime_hints)
    foreach(_rose_omp_lib_name IN LISTS _rose_llvm_openmp_runtime_names)
      if(EXISTS "${_rose_omp_runtime_dir}/${_rose_omp_lib_name}")
        file(REAL_PATH "${_rose_omp_runtime_dir}/${_rose_omp_lib_name}"
          _rose_runtime_candidate)
        set(_rose_runtime_candidate_major "")
        if(CLANG_VERSION_MAJOR)
          string(REGEX MATCH "/llvm-([0-9]+)/" _rose_runtime_candidate_match
            "${_rose_runtime_candidate}")
          if(_rose_runtime_candidate_match)
            set(_rose_runtime_candidate_major "${CMAKE_MATCH_1}")
          endif()
        endif()
        if(_rose_runtime_candidate_major AND
           NOT _rose_runtime_candidate_major STREQUAL "${CLANG_VERSION_MAJOR}")
          if(NOT _rose_mismatched_openmp_runtime_library)
            set(_rose_mismatched_openmp_runtime_library
              "${_rose_runtime_candidate}")
            set(_rose_mismatched_openmp_runtime_major
              "${_rose_runtime_candidate_major}")
          endif()
          unset(_rose_runtime_candidate)
          unset(_rose_runtime_candidate_major)
          unset(_rose_runtime_candidate_match)
          continue()
        endif()
        set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "${_rose_runtime_candidate}")
        unset(_rose_runtime_candidate)
        unset(_rose_runtime_candidate_major)
        unset(_rose_runtime_candidate_match)
        break()
      endif()
    endforeach()
    if(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
      break()
    endif()
  endforeach()
  if(NOT ROSE_LLVM_OPENMP_RUNTIME_LIBRARY)
    if(_rose_mismatched_openmp_runtime_library)
      message(FATAL_ERROR
        "Found LLVM OpenMP runtime from llvm-${_rose_mismatched_openmp_runtime_major}: "
        "${_rose_mismatched_openmp_runtime_library}, but REX is configured for "
        "LLVM/Clang ${CLANG_VERSION_MAJOR}. Install the matching LLVM OpenMP "
        "runtime (e.g., libomp-${CLANG_VERSION_MAJOR}-dev), or set "
        "WITH-LLVM-OMP-RUNTIME-LIBRARY.")
    endif()
    if(_rose_llvm_openmp_runtime_hints)
      string(REPLACE ";" "\n  " _rose_openmp_hint_lines
        "${_rose_llvm_openmp_runtime_hints}")
    else()
      set(_rose_openmp_hint_lines "<none>")
    endif()
    message(FATAL_ERROR
      "LLVM OpenMP runtime library (libiomp5/libomp) not found for LLVM/Clang "
      "${CLANG_VERSION_MAJOR}. Searched:\n  ${_rose_openmp_hint_lines}\n"
      "Install libomp-${CLANG_VERSION_MAJOR}-dev (or equivalent), or set "
      "WITH-LLVM-OMP-RUNTIME-LIBRARY.")
  endif()

  unset(_rose_mismatched_openmp_runtime_library)
  unset(_rose_mismatched_openmp_runtime_major)
  get_filename_component(ROSE_LLVM_OPENMP_RUNTIME_DIR
    "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}" DIRECTORY)
  set(_rose_runtime_build_line "")
  if(CLANG_VERSION_MAJOR)
    set(_rose_runtime_llvm_major "")
    string(REGEX MATCH "/llvm-([0-9]+)/" _rose_runtime_llvm_match
      "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}")
    if(_rose_runtime_llvm_match)
      set(_rose_runtime_llvm_major "${CMAKE_MATCH_1}")
    endif()
    if(NOT _rose_runtime_llvm_major)
      find_program(_rose_strings_exec NAMES strings)
      if(_rose_strings_exec)
        execute_process(
          COMMAND "${_rose_strings_exec}" -a "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}"
          COMMAND grep -m1 "LLVM OMP build compiler: Clang "
          OUTPUT_VARIABLE _rose_runtime_build_line
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
        if(_rose_runtime_build_line)
          string(REGEX MATCH "Clang ([0-9]+)" _rose_runtime_build_match
            "${_rose_runtime_build_line}")
          if(_rose_runtime_build_match)
            set(_rose_runtime_llvm_major "${CMAKE_MATCH_1}")
          endif()
        endif()
      endif()
    endif()
    if(_rose_runtime_llvm_major AND
       NOT _rose_runtime_llvm_major STREQUAL "${CLANG_VERSION_MAJOR}")
      message(FATAL_ERROR
        "Resolved LLVM OpenMP runtime from llvm-${_rose_runtime_llvm_major}: "
        "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}, but REX is configured for "
        "LLVM/Clang ${CLANG_VERSION_MAJOR}. Install the matching LLVM OpenMP "
        "runtime or set WITH-LLVM-OMP-RUNTIME-LIBRARY.")
    endif()
  endif()
  set(WITH-LLVM-OMP-RUNTIME-LIBRARY "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" CACHE PATH
    "LLVM OpenMP runtime library directory" FORCE)

  if(_rose_clang_resource_dir AND EXISTS "${_rose_clang_resource_dir}/include/omp.h")
    set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${_rose_clang_resource_dir}/include")
  endif()
  if(NOT ROSE_LLVM_OPENMP_INCLUDE_DIR AND ROSE_LLVM_OPENMP_RUNTIME_DIR)
    get_filename_component(_rose_llvm_runtime_prefix
      "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/.." ABSOLUTE)
    if(EXISTS "${_rose_llvm_runtime_prefix}/include/omp.h")
      set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${_rose_llvm_runtime_prefix}/include")
    endif()
    if(NOT ROSE_LLVM_OPENMP_INCLUDE_DIR)
      file(GLOB _rose_runtime_omp_headers
        "${_rose_llvm_runtime_prefix}/lib/clang/*/include/omp.h")
      if(_rose_runtime_omp_headers)
        list(SORT _rose_runtime_omp_headers COMPARE NATURAL)
        list(GET _rose_runtime_omp_headers -1 _rose_runtime_omp_header)
        get_filename_component(ROSE_LLVM_OPENMP_INCLUDE_DIR
          "${_rose_runtime_omp_header}" DIRECTORY)
      endif()
      unset(_rose_runtime_omp_header)
      unset(_rose_runtime_omp_headers)
    endif()
    unset(_rose_llvm_runtime_prefix)
  endif()
  if(NOT ROSE_LLVM_OPENMP_INCLUDE_DIR)
    message(FATAL_ERROR
      "LLVM OpenMP header (omp.h) not found for ${_rose_llvm_openmp_driver}. "
      "Ensure LLVM's OpenMP headers are installed.")
  endif()

  file(REAL_PATH "${ROSE_LLVM_OPENMP_INCLUDE_DIR}" ROSE_LLVM_OPENMP_INCLUDE_DIR)
  if(_rose_clang_resource_dir)
    set(_rose_clang_resource_include_dir "${_rose_clang_resource_dir}/include")
    if(EXISTS "${_rose_clang_resource_include_dir}")
      file(REAL_PATH "${_rose_clang_resource_include_dir}" _rose_clang_resource_include_dir)
    else()
      unset(_rose_clang_resource_include_dir)
    endif()
  endif()

  # If the only available OpenMP header comes from a different Clang resource
  # tree than the active frontend, stage a local copy of omp.h and use that
  # directory instead. This avoids include_next collisions between different
  # Clang stdint.h versions.
  if(_rose_clang_resource_include_dir
     AND NOT ROSE_LLVM_OPENMP_INCLUDE_DIR STREQUAL _rose_clang_resource_include_dir
     AND ROSE_LLVM_OPENMP_INCLUDE_DIR MATCHES "/lib/clang/.+/include$")
    set(_rose_staged_openmp_include_dir "${CMAKE_BINARY_DIR}/include-staging/llvm-openmp")
    file(MAKE_DIRECTORY "${_rose_staged_openmp_include_dir}")
    configure_file(
      "${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h"
      "${_rose_staged_openmp_include_dir}/omp.h"
      COPYONLY)
    set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${_rose_staged_openmp_include_dir}")
    message(STATUS
      "Staging LLVM OpenMP header to avoid mixed Clang resource headers: "
      "${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
    unset(_rose_staged_openmp_include_dir)
  endif()

  set(LLVM_OPENMP_LIB_PATH "${ROSE_LLVM_OPENMP_RUNTIME_DIR}")
  set(LLVM_OPENMP_INCLUDE_PATH "${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
  set(ROSE_LLVM_OPENMP_RUNTIME_DIR "${ROSE_LLVM_OPENMP_RUNTIME_DIR}" CACHE INTERNAL
    "Directory containing the LLVM OpenMP runtime library used by REX")
  set(ROSE_LLVM_OPENMP_RUNTIME_LIBRARY "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}" CACHE INTERNAL
    "LLVM OpenMP runtime library selected by REX")
  set(ROSE_LLVM_OPENMP_INCLUDE_DIR "${ROSE_LLVM_OPENMP_INCLUDE_DIR}" CACHE INTERNAL
    "Directory containing the LLVM OpenMP header used by REX")
  if(_rose_runtime_build_line)
    message(STATUS "Using LLVM OpenMP runtime: ${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY} (${_rose_runtime_build_line})")
  else()
    message(STATUS "Using LLVM OpenMP runtime: ${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}")
  endif()
  message(STATUS "Using LLVM OpenMP header dir: ${ROSE_LLVM_OPENMP_INCLUDE_DIR}")
  unset(_rose_runtime_build_line)
  unset(_rose_runtime_build_match)
  unset(_rose_strings_exec)
  unset(_rose_runtime_llvm_major)
  unset(_rose_runtime_llvm_match)
  unset(_rose_clang_resource_include_dir)
  unset(_rose_clang_resource_dir)
  unset(_rose_llvm_openmp_driver_version)
  unset(_rose_llvm_openmp_driver_major)
  unset(_rose_llvm_openmp_driver)
  unset(_rose_llvm_openmp_driver_dir)
  unset(_rose_llvm_openmp_runtime_hints)
  unset(_rose_openmp_hint_lines)
  unset(_rose_omp_runtime_dir)
  unset(_rose_omp_lib_name)
  unset(_rose_runtime_candidate)
  unset(_rose_runtime_candidate_major)
  unset(_rose_runtime_candidate_match)
  unset(_rose_mismatched_openmp_runtime_library)
  unset(_rose_mismatched_openmp_runtime_major)
  unset(_rose_libdir)
endif()

if(WITH-LLVM-OMP-RUNTIME-LIBRARY AND NOT LLVM_OPENMP_LIB_PATH)
  set(LLVM_OPENMP_LIB_PATH "${WITH-LLVM-OMP-RUNTIME-LIBRARY}")
endif()

if(REX_ENABLE_WASM)
  set(REX_WASM_CLANG_RESOURCE_INPUT "" CACHE PATH
    "Clang resource directory containing an include/ subtree for the REX WASM package")
  set(REX_WASM_CLANG_HEADERS_INPUT "" CACHE PATH
    "Fallback Clang lib/Headers source directory for the REX WASM package")
  set(REX_WASM_OMP_INCLUDE_DIR "" CACHE PATH
    "Directory containing the LLVM omp.h header for the REX WASM package")
  option(REX_WASM_PACKAGE_SYSROOT
    "Package the Emscripten sysroot headers into the REX WASM virtual filesystem" FALSE)
  set(REX_WASM_SYSROOT "${EMSCRIPTEN_SYSROOT}" CACHE PATH
    "Optional Emscripten sysroot to package for the REX WASM browser frontend")

  if(NOT REX_WASM_CLANG_RESOURCE_INPUT AND LLVM_LIBRARY_DIRS)
    foreach(_rose_llvm_lib_dir IN LISTS LLVM_LIBRARY_DIRS)
      if(EXISTS "${_rose_llvm_lib_dir}/clang/${CLANG_VERSION_MAJOR}/include/stddef.h")
        set(REX_WASM_CLANG_RESOURCE_INPUT
          "${_rose_llvm_lib_dir}/clang/${CLANG_VERSION_MAJOR}" CACHE PATH
          "Clang resource directory containing an include/ subtree for the REX WASM package" FORCE)
        break()
      endif()
    endforeach()
    unset(_rose_llvm_lib_dir)
  endif()

  if(NOT REX_WASM_CLANG_HEADERS_INPUT AND LLVM_CMAKE_DIR)
    get_filename_component(_rose_llvm_source_root
      "${LLVM_CMAKE_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${_rose_llvm_source_root}/clang/lib/Headers/stddef.h")
      set(REX_WASM_CLANG_HEADERS_INPUT
        "${_rose_llvm_source_root}/clang/lib/Headers" CACHE PATH
        "Fallback Clang lib/Headers source directory for the REX WASM package" FORCE)
    endif()
    if(NOT REX_WASM_OMP_INCLUDE_DIR AND
       EXISTS "${_rose_llvm_source_root}/openmp/runtime/src/include/omp.h")
      set(REX_WASM_OMP_INCLUDE_DIR
        "${_rose_llvm_source_root}/openmp/runtime/src/include" CACHE PATH
        "Directory containing the LLVM omp.h header for the REX WASM package" FORCE)
    endif()
    unset(_rose_llvm_source_root)
  endif()

  if(NOT REX_WASM_OMP_INCLUDE_DIR)
    foreach(_rose_omp_include_candidate
        "${LLVM_LIBRARY_DIRS}/clang/${CLANG_VERSION_MAJOR}/include"
        "/usr/lib/llvm-${CLANG_VERSION_MAJOR}/lib/clang/${CLANG_VERSION_MAJOR}/include"
        "/usr/lib/llvm-${CLANG_VERSION_MAJOR}/lib/clang/${LLVM_PACKAGE_VERSION}/include")
      if(EXISTS "${_rose_omp_include_candidate}/omp.h")
        set(REX_WASM_OMP_INCLUDE_DIR "${_rose_omp_include_candidate}" CACHE PATH
          "Directory containing the LLVM omp.h header for the REX WASM package" FORCE)
        break()
      endif()
    endforeach()
    unset(_rose_omp_include_candidate)
  endif()

  if(NOT REX_WASM_CLANG_RESOURCE_INPUT AND NOT REX_WASM_CLANG_HEADERS_INPUT)
    message(FATAL_ERROR
      "REX_ENABLE_WASM requires Clang resource headers. Set "
      "REX_WASM_CLANG_RESOURCE_INPUT or REX_WASM_CLANG_HEADERS_INPUT.")
  endif()
  if(NOT REX_WASM_OMP_INCLUDE_DIR OR NOT EXISTS "${REX_WASM_OMP_INCLUDE_DIR}/omp.h")
    message(FATAL_ERROR
      "REX_ENABLE_WASM requires an LLVM omp.h header. Set REX_WASM_OMP_INCLUDE_DIR.")
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
  if(REX_WASM_CLANG_RESOURCE_INPUT)
    file(COPY "${REX_WASM_CLANG_RESOURCE_INPUT}/include/"
      DESTINATION "${REX_WASM_CLANG_RESOURCE_DIR}/include")
  else()
    file(COPY "${REX_WASM_CLANG_HEADERS_INPUT}/"
      DESTINATION "${REX_WASM_CLANG_RESOURCE_DIR}/include")
  endif()
  configure_file(
    "${REX_WASM_OMP_INCLUDE_DIR}/omp.h"
    "${REX_WASM_CLANG_RESOURCE_DIR}/include/omp.h"
    COPYONLY)
endif()

set(LIBHARU_ROOT "" CACHE PATH "Optional libharu installation prefix")
if(NOT REX_ENABLE_WASM)
  if(LIBHARU_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${LIBHARU_ROOT}")
  endif()

  # libharu is required for PDF generation (system-provided).
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)
    pkg_check_modules(HPDF QUIET libhpdf)
  endif()

  if(NOT HPDF_FOUND)
    find_path(HPDF_INCLUDE_DIRS NAMES hpdf.h
              HINTS ENV LIBHARU_ROOT ${LIBHARU_ROOT}
              PATH_SUFFIXES include)
    find_library(HPDF_LIBRARIES NAMES hpdf libhpdf
                 HINTS ENV LIBHARU_ROOT ${LIBHARU_ROOT}
                 PATH_SUFFIXES lib lib64)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(HPDF DEFAULT_MSG
                                      HPDF_LIBRARIES HPDF_INCLUDE_DIRS)
  endif()

  if(NOT HPDF_FOUND OR NOT HPDF_LIBRARIES OR NOT HPDF_INCLUDE_DIRS)
    message(FATAL_ERROR "libharu (hpdf) is required. Install the libharu development package and ensure hpdf.h and libhpdf are discoverable, or set the LIBHARU_ROOT environment variable.")
  endif()
else()
  set(HPDF_FOUND FALSE)
  set(HPDF_LIBRARIES "")
  set(HPDF_LIBRARY_DIRS "")
  set(HPDF_INCLUDE_DIRS "")
endif()

if(NOT HPDF_VERSION)
  set(HPDF_VERSION_HEADER "")
  foreach(dir IN LISTS HPDF_INCLUDE_DIRS)
    if(EXISTS "${dir}/hpdf_version.h")
      set(HPDF_VERSION_HEADER "${dir}/hpdf_version.h")
      break()
    endif()
  endforeach()
  if(HPDF_VERSION_HEADER)
    file(STRINGS "${HPDF_VERSION_HEADER}" _hpdf_version_line
         REGEX "^#define[ \t]+HPDF_VERSION_TEXT[ \t]+\"")
    if(_hpdf_version_line)
      string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" HPDF_VERSION
                           "${_hpdf_version_line}")
    endif()
  endif()
endif()

if(HPDF_VERSION)
  message(STATUS "Found libharu (hpdf): ${HPDF_LIBRARIES} (found version \"${HPDF_VERSION}\")")
else()
  message(STATUS "Found libharu (hpdf): ${HPDF_LIBRARIES}")
endif()

if(NOT HPDF_LIBRARY_DIRS)
  foreach(_hpdf_lib IN LISTS HPDF_LIBRARIES)
    if(IS_ABSOLUTE "${_hpdf_lib}")
      get_filename_component(_hpdf_dir "${_hpdf_lib}" DIRECTORY)
      if(_hpdf_dir)
        list(APPEND HPDF_LIBRARY_DIRS "${_hpdf_dir}")
      endif()
    endif()
  endforeach()
  if(HPDF_LIBRARY_DIRS)
    list(REMOVE_DUPLICATES HPDF_LIBRARY_DIRS)
  endif()
endif()

if(HPDF_FOUND)
  set(ROSE_WITH_LIBHARU 1)
else()
  set(ROSE_WITH_LIBHARU 0)
endif()

set(ASSERTION_BEHAVIOR "exit" CACHE STRING "Default behavior of failed assertions. Can be 'abort', 'exit', or 'throw'.")
if(ASSERTION_BEHAVIOR STREQUAL "abort")
  add_definitions("-DROSE_ASSERTION_BEHAVIOR=ROSE_ASSERTION_ABORT")
elseif(ASSERTION_BEHAVIOR STREQUAL "exit")
  add_definitions("-DROSE_ASSERTION_BEHAVIOR=ROSE_ASSERTION_EXIT")
elseif(ASSERTION_BEHAVIOR STREQUAL "throw")
  add_definitions("-DROSE_ASSERTION_BEHAVIOR=ROSE_ASSERTION_THROW")
else()
  message(FATAL_ERROR "ASSERTION_BEHAVIOR should be 'abort', 'exit', or 'throw'")
endif()

find_path(with-GraphViz_include graphviz.h DOC "Specify the prefix where GraphViz include files are installed")
find_path(with-GraphViz_libs GraphViz_libs DOC "Specify the prefix where GraphViz libraries are installed")
find_path(with-ltdl-include ltdl.h DOC "use the ltdl headers installed in DIR")
find_library(with-ltdl-lib ltdl DOC "Path to ltdl library")

find_path(with-wine "bin/wine" DOC "Specify the prefix where Wine is installed")
if(with-wine)
  set(ROSE_WINE_INCLUDES -I${with-wine}/include)
endif()

find_package(FLEX)
if(NOT FLEX_FOUND)
  message(FATAL_ERROR "Could not find flex command")
endif()

if(NOT REX_ENABLE_WASM)
  find_library(M_LIB m REQUIRED)
  find_library(RT_LIB rt)
endif()

########################################################################################################################
# Back-end compilers
########################################################################################################################

set(WITH-ALT-BACKEND-CXX-COMPILER "" CACHE STRING "Specify an alternative C++ back-end compiler")
if(WITH-ALT-BACKEND-CXX-COMPILER)
  if(EXISTS ${WITH-ALT-BACKEND-CXX-COMPILER})
    set(BACKEND_CXX_COMPILER ${WITH-ALT-BACKEND-CXX-COMPILER})
  else()
    message(WARNING "${WITH-ALT-BACKEND-CXX-COMPILER} does not exist; pecify an absolute path to the compiler")
  endif()
endif()

set(WITH-ALT-BACKEND-C-COMPILER "" CACHE STRING "Specify an alternative C back-end compiler")
if(WITH-ALT-BACKEND-C-COMPILER)
  if(EXISTS ${WITH-ALT-BACKEND-C-COMPILER})
    set(BACKEND_C_COMPILER ${WITH-ALT-BACKEND-C-COMPILER})
    message(STATUS "just set backend C compiler to ${BACKEND_C_COMPILER}")
  else()
    message(WARNING "${WITH-ALT-BACKEND-C-COMPILER} does not exist; specify an absolute path to the compiler")
  endif()
endif()

set(WITH-ALT-BACKEND-FORTRAN-COMPILER "" CACHE STRING "Specify an alternative fortran back-end compiler")
if(WITH-ALT-BACKEND-FORTRAN-COMPILER)
  if(EXISTS ${WITH-ALT-BACKEND-FORTRAN-COMPILER})
    set(BACKEND_FORTRAN_COMPILER ${WITH-ALT-BACKEND-FORTRAN-COMPILER})
  else()
    message(WARNING "${WITH-ALT-BACKEND-FORTRAN-COMPILER} does not exist; specify an absolute path to the compiler")
  endif()
endif()

# define a global variable to collect all common linked third-party libraries for rose (Linux-only)
if(REX_ENABLE_WASM)
  set(link_with_libraries "")
else()
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set(link_with_libraries Threads::Threads)
  if(M_LIB)
    list(APPEND link_with_libraries m)
  endif()
  if(RT_LIB)
    list(APPEND link_with_libraries rt)
  endif()
  if(HAVE_QUADMATH_COMPILE)
    list(APPEND link_with_libraries quadmath)
  endif()
endif()

# Check compilers and version numbers. The module is located in src/cmake/modules.
if(REX_ENABLE_WASM)
  set(BACKEND_C_COMPILER "${CMAKE_C_COMPILER}")
  set(BACKEND_C_COMPILER_NAME_WITHOUT_PATH "emcc")
  set(BACKEND_C_COMPILER_MAJOR_VERSION_NUMBER "${CMAKE_C_COMPILER_VERSION_MAJOR}")
  set(BACKEND_C_COMPILER_MINOR_VERSION_NUMBER "${CMAKE_C_COMPILER_VERSION_MINOR}")
  set(BACKEND_C_COMPILER_PATCH_LEVEL_NUMBER "${CMAKE_C_COMPILER_VERSION_PATCH}")
  set(BACKEND_CXX_COMPILER "${CMAKE_CXX_COMPILER}")
  set(BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH "em++")
  set(BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER "${CMAKE_CXX_COMPILER_VERSION_MAJOR}")
  set(BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER "${CMAKE_CXX_COMPILER_VERSION_MINOR}")
  set(BACKEND_CXX_COMPILER_PATCH_LEVEL_NUMBER "${CMAKE_CXX_COMPILER_VERSION_PATCH}")
  foreach(_rex_backend_version_var
      BACKEND_C_COMPILER_MAJOR_VERSION_NUMBER
      BACKEND_C_COMPILER_MINOR_VERSION_NUMBER
      BACKEND_C_COMPILER_PATCH_LEVEL_NUMBER
      BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER
      BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER
      BACKEND_CXX_COMPILER_PATCH_LEVEL_NUMBER)
    if("${${_rex_backend_version_var}}" STREQUAL "")
      set(${_rex_backend_version_var} "0")
    endif()
  endforeach()
  unset(_rex_backend_version_var)
  set(BACKEND_CXX_IS_CLANG_COMPILER 1)
elseif(ENABLE-C OR ENABLE-FORTRAN)
  include(roseChooseBackendCompiler)

  if(ENABLE-FORTRAN)
    if("${BACKEND_FORTRAN_COMPILER_MAJOR_VERSION_NUMBER}" STREQUAL "" OR
       "${BACKEND_FORTRAN_COMPILER_MINOR_VERSION_NUMBER}" STREQUAL "")
      message(FATAL_ERROR "Failed to determine backend Fortran compiler version metadata.")
    endif()
  endif()

  if(ENABLE-C)
  include(roseGenerateBackendCompilerSpecificHeaders)
  install(DIRECTORY ${CMAKE_BINARY_DIR}/include-staging/ DESTINATION ${CLANG_BUILTIN_HEADERS_INSTALL_DIR})
  endif()
else()
  # These are required to be defined even when C/C++ analysis is not enabled, but their values don't matter
  set(BACKEND_C_COMPILER_NAME_WITHOUT_PATH "no_compiler")
  set(BACKEND_C_COMPILER_MAJOR_VERSION_NUMBER "0")
  set(BACKEND_C_COMPILER_MINOR_VERSION_NUMBER "0")
  set(BACKEND_C_COMPILER_PATCH_LEVEL_NUMBER "0")
  set(BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH "no_compiler")
  set(BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER "0")
  set(BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER "0")
  set(BACKEND_CXX_COMPILER_PATCH_LEVEL_NUMBER "0")
endif()

########################################################################################################################
# Summary of results
########################################################################################################################

if(VERBOSE)
  # if you are building in-source, this is the same as CMAKE_SOURCE_DIR, otherwise
  # this is the top level directory of your build tree
  message(STATUS "CMAKE_BINARY_DIR:                   ${CMAKE_BINARY_DIR}")

  # if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR, otherwise this
  # is the directory where the compiled or generated files from the current CMakeLists.txt will go to
  message(STATUS "CMAKE_CURRENT_BINARY_DIR:           ${CMAKE_CURRENT_BINARY_DIR}")

  # this is the directory, from which cmake was started, i.e. the top level source directory
  message(STATUS "CMAKE_SOURCE_DIR:                   ${CMAKE_SOURCE_DIR}")

  # this is the directory where the currently processed CMakeLists.txt is located in
  message(STATUS "CMAKE_CURRENT_SOURCE_DIR:           ${CMAKE_CURRENT_SOURCE_DIR}")

  # contains the full path to the top level directory of your build tree
  message(STATUS "PROJECT_BINARY_DIR:                 ${PROJECT_BINARY_DIR}")

  # contains the full path to the root of your project source directory,
  # i.e. to the nearest directory where CMakeLists.txt contains the PROJECT() command
  message(STATUS "PROJECT_SOURCE_DIR:                 ${PROJECT_SOURCE_DIR}")

  # set this variable to specify a common place where CMake should put all executable files
  # (instead of CMAKE_CURRENT_BINARY_DIR)
  message(STATUS "EXECUTABLE_OUTPUT_PATH:             ${EXECUTABLE_OUTPUT_PATH}")

  # set this variable to specify a common place where CMake should put all libraries
  # (instead of CMAKE_CURRENT_BINARY_DIR)
  message(STATUS "LIBRARY_OUTPUT_PATH:                ${LIBRARY_OUTPUT_PATH}")

  # tell CMake to search first in directories listed in CMAKE_MODULE_PATH
  # when you use FIND_PACKAGE() or INCLUDE()
  message(STATUS "CMAKE_MODULE_PATH:                  ${CMAKE_MODULE_PATH}")

  # this is the complete path of the cmake which runs currently (e.g. /usr/local/bin/cmake)
  message(STATUS "CMAKE_COMMAND:                      ${CMAKE_COMMAND}")

  # this is the CMake installation directory
  message(STATUS "CMAKE_ROOT:                         ${CMAKE_ROOT}")

  # this is the filename including the complete path of the file where this variable is used.
  message(STATUS "CMAKE_CURRENT_LIST_FILE:            ${CMAKE_CURRENT_LIST_FILE}")

  # this is used when searching for include files e.g. using the FIND_PATH() command.
  message(STATUS "CMAKE_INCLUDE_PATH:                 ${CMAKE_INCLUDE_PATH}")

  # this is used when searching for libraries e.g. using the FIND_LIBRARY() command.
  message(STATUS "CMAKE_LIBRARY_PATH:                 ${CMAKE_LIBRARY_PATH}")

	  # the complete system name (e.g. "Linux-6.x.y")
	  message(STATUS "CMAKE_SYSTEM:                       ${CMAKE_SYSTEM}")

	  # the short system name (e.g. "Linux")
	  message(STATUS "CMAKE_SYSTEM_NAME:                  ${CMAKE_SYSTEM_NAME}")

  # only the version part of CMAKE_SYSTEM
  message(STATUS "CMAKE_SYSTEM_VERSION:               ${CMAKE_SYSTEM_VERSION}")

  # the processor name (e.g. "Intel(R) Pentium(R) M processor 2.00GHz")
  message(STATUS "CMAKE_SYSTEM_PROCESSOR:             ${CMAKE_SYSTEM_PROCESSOR}")

	  # is TRUE on UNIX-like OS's
	  message(STATUS "UNIX:                               ${UNIX}")

  # set this to true if you don't want to rebuild the object files if the rules have changed,
  # but not the actual source files or headers (e.g. if you changed the some compiler switches)
  message(STATUS "CMAKE_SKIP_RULE_DEPENDENCY:         ${CMAKE_SKIP_RULE_DEPENDENCY}")

  # since CMake 2.1 the install rule depends on all, i.e. everything will be built before installing.
  # If you don't like this, set this one to true.
  message(STATUS "CMAKE_SKIP_INSTALL_ALL_DEPENDENCY:  ${CMAKE_SKIP_INSTALL_ALL_DEPENDENCY}")

  # If set, runtime paths are not added when using shared libraries. Default it is set to OFF
  message(STATUS "CMAKE_SKIP_RPATH:                   ${CMAKE_SKIP_RPATH}")

  # set this to true if you are using makefiles and want to see the full compile and link
  # commands instead of only the shortened ones
  message(STATUS "CMAKE_VERBOSE_MAKEFILE:             ${CMAKE_VERBOSE_MAKEFILE}")

  # this will cause CMake to not put in the rules that re-run CMake. This might be useful if
  # you want to use the generated build files on another machine.
  message(STATUS "CMAKE_SUPPRESS_REGENERATION:        ${CMAKE_SUPPRESS_REGENERATION}")

  # A simple way to get switches to the compiler is to use ADD_DEFINITIONS().
  # But there are also two variables exactly for this purpose:
  #   CMAKE_C_FLAGS:   the compiler flags for compiling C sources, and
  #   CMAKE_CXX_FLAGS: the compiler flags for compiling C++ sources
  message(STATUS "CMAKE_C_FLAGS:                      ${CMAKE_C_FLAGS}")
  message(STATUS "CMAKE_CXX_FLAGS:                    ${CMAKE_CXX_FLAGS}")

  # Choose the type of build.  Example: SET(CMAKE_BUILD_TYPE Debug)
  message(STATUS "CMAKE_BUILD_TYPE:                   ${CMAKE_BUILD_TYPE}")

  # if this is set to ON, then all libraries are built as shared libraries by default.
  message(STATUS "BUILD_SHARED_LIBS:                  ${BUILD_SHARED_LIBS}")

  # the compiler used for C files
  message(STATUS "CMAKE_C_COMPILER:                   ${CMAKE_C_COMPILER}")

  # the compiler used for C++ files
  message(STATUS "CMAKE_CXX_COMPILER:                 ${CMAKE_CXX_COMPILER}")

  # if the compiler is a variant of gcc, this should be set to 1
  message(STATUS "CMAKE_COMPILER_IS_GNUCC:            ${CMAKE_COMPILER_IS_GNUCC}")

  # if the compiler is a variant of g++, this should be set to 1
  message(STATUS "CMAKE_COMPILER_IS_GNUCXX:           ${CMAKE_COMPILER_IS_GNUCXX}")

  # the tools for creating libraries
  message(STATUS "CMAKE_AR:                           ${CMAKE_AR}")
  message(STATUS "CMAKE_RANLIB:                       ${CMAKE_RANLIB}")
endif()

########################################################################################################################
# ROSE headers are scattered throughout the source tree.
########################################################################################################################

set(ROSE_BINARY_INCLUDES
  ${ROSE_TOP_BINARY_DIR}
  ${ROSE_TOP_BINARY_DIR}/src/util
  ${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/)

set(ROSE_SOURCE_INCLUDES
  ${ROSE_TOP_SRC_DIR}
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/sage_support
  ${ROSE_TOP_SRC_DIR}/src/ROSETTA/src/
  ${ROSE_TOP_SRC_DIR}/src
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/astFixup
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/astPostProcessing
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/includeDirectivesProcessing
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/sageInterface
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/ompparser/src
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/accparser/src
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/virtualCFG
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/astTokenStream
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/astHiddenTypeAndDeclarationLists
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
  ${ROSE_TOP_SRC_DIR}/src/midend/programAnalysis/OAWrap
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

set(ROSE_EXTERNAL_INCLUDES
  ${VALGRIND_INCLUDE_PATH}
  ${HPDF_INCLUDE_DIRS})

set(ROSE_INCLUDES ${ROSE_BINARY_INCLUDES})
foreach(include_dir IN LISTS ROSE_SOURCE_INCLUDES ROSE_EXTERNAL_INCLUDES)
  if(include_dir AND EXISTS "${include_dir}")
    list(APPEND ROSE_INCLUDES "${include_dir}")
  endif()
endforeach()
list(REMOVE_DUPLICATES ROSE_INCLUDES)

########################################################################################################################
# Set output directories
########################################################################################################################

# Set default output paths for libraries and directories.  This causes all the libraries to be built into a single lib
# directory, and all the executables to be built into a single bin directory.  If these variables are not set, then by
# default the targets will be littered throughout the binary directory, matching the structure of the source directory.
#
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set up RPATH so that our executables will be able to find their dependent
# libraries using relative paths.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  check_linker_flag(CXX "-Wl,--disable-new-dtags" ROSE_LINKER_SUPPORTS_DISABLE_NEW_DTAGS)
  if(ROSE_LINKER_SUPPORTS_DISABLE_NEW_DTAGS)
    # Use DT_RPATH instead of DT_RUNPATH so build-tree and install-tree binaries
    # keep preferring their sibling lib/ directory even when LD_LIBRARY_PATH is set.
    add_link_options("-Wl,--disable-new-dtags")
  else()
    message(WARNING "Linker does not support -Wl,--disable-new-dtags; LD_LIBRARY_PATH may override RPATH for ROSE binaries")
  endif()
endif()

if(NOT DEFINED CMAKE_INSTALL_RPATH)
  message(STATUS "Setting CMAKE_INSTALL_RPATH")
  set(origin "$ORIGIN")
  set(CMAKE_INSTALL_RPATH
    "${origin}/../lib"
    "${origin}"
  )
  if(ROSE_JVM_LIBRARY_DIR)
    list(APPEND CMAKE_INSTALL_RPATH
      "${ROSE_JVM_LIBRARY_DIR}"
    )
  endif()
  # Keep external library search paths without embedding build-tree paths in install RPATH.
  if(LLVM_LIBRARY_DIRS)
    list(APPEND CMAKE_INSTALL_RPATH ${LLVM_LIBRARY_DIRS})
  endif()
  if(CLANG_LIBRARY_DIRS)
    list(APPEND CMAKE_INSTALL_RPATH ${CLANG_LIBRARY_DIRS})
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH}")
  endif()
endif()

if(DEFINED CMAKE_INSTALL_RPATH)
  # Normalize to a list so we don't emit empty RPATH entries (trailing separators).
  string(REPLACE ":" ";" CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}")
  list(REMOVE_ITEM CMAKE_INSTALL_RPATH "")
  list(REMOVE_DUPLICATES CMAKE_INSTALL_RPATH)
endif()

if(NOT DEFINED CMAKE_BUILD_WITH_INSTALL_RPATH)
  message(STATUS "Setting CMAKE_BUILD_WITH_INSTALL_RPATH")
  # Keep build/install RPATHs consistent; $ORIGIN/../lib resolves in either tree.
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if(NOT DEFINED CMAKE_BUILD_RPATH)
  message(STATUS "Setting CMAKE_BUILD_RPATH")
  set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")
  if(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    list(APPEND CMAKE_BUILD_RPATH "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  endif()
  if(ROSE_JVM_LIBRARY_DIR)
    list(APPEND CMAKE_BUILD_RPATH "${ROSE_JVM_LIBRARY_DIR}")
  endif()
endif()

if(DEFINED CMAKE_BUILD_RPATH)
  # Normalize to a list so we don't emit empty RPATH entries (trailing separators).
  string(REPLACE ":" ";" CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH}")
  list(REMOVE_ITEM CMAKE_BUILD_RPATH "")
  list(REMOVE_DUPLICATES CMAKE_BUILD_RPATH)
endif()

if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
  message(STATUS "Setting CMAKE_INSTALL_RPATH_USE_LINKPATH")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

########################################################################################################################
# Recursion
########################################################################################################################

# Recursion in CMake is not automatic. We have to tell it where the other CMakeList.txt files are located rather than
# letting it find them automatically.

add_subdirectory(LicenseInformation)
add_subdirectory(config)
add_subdirectory(src)
if(NOT REX_ENABLE_WASM)
  add_subdirectory(exampleTranslators)
  add_subdirectory(scripts)
endif()

# Copy CTestCustom.cmake into the binary directory.  This file tells CTest to ignore some output that it otherwise
# considers compilation errors.
configure_file(cmake/CTestCustom.cmake.in ${CMAKE_BINARY_DIR}/CTestCustom.cmake @ONLY)
# Configure CTest settings (including memcheck) for the build tree.
configure_file(cmake/CTestConfig.cmake.in ${CMAKE_BINARY_DIR}/CTestConfig.cmake @ONLY)

# Create include-staging directory for compiler headers (used by multiple language frontends)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include-staging")
# Stage public headers expected under include/Rose in install tree.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include-staging/Rose")
file(COPY ${ROSE_TOP_SRC_DIR}/src/Rose/SourceLocation.h
     DESTINATION ${CMAKE_BINARY_DIR}/include-staging/Rose)

if(ENABLE-CUDA)
    execute_process(
      COMMAND mkdir -p "./include-staging/cuda_HEADERS")
    execute_process(
      COMMAND cp ${ROSE_TOP_SRC_DIR}/config/preinclude-cuda.h ./include-staging/cuda_HEADERS)
endif()

# Linux-only: fail fast on non-Linux platforms.
if(NOT REX_ENABLE_WASM AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This project now targets Linux only; non-Linux builds are unsupported.")
endif()

# Linux-only header discovery (GNU/Clang toolchains)
if(REX_ENABLE_WASM)
  set(C_INCLUDE_STRING "{\"emscripten\"")
  set(CXX_INCLUDE_STRING "{\"emscripten\"")
elseif(ENABLE-C)
  set(C_INCLUDE_STRING "{\"${BACKEND_C_COMPILER_NAME_WITHOUT_PATH}_HEADERS\"")

  set(CXX_INCLUDE_STRING "{\"${BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH}_HEADERS\"")
  find_program(shell sh)

  # Determine compiler vendor names for get_compiler_header_dirs script (Linux-only: GNU/Clang)
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    set(C_COMPILER_VENDOR "gnu")
  elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set(C_COMPILER_VENDOR "clang")
  elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "IntelLLVM")
    set(C_COMPILER_VENDOR "intel")
  else()
    message(FATAL_ERROR "Unsupported C compiler for Linux-only build (expected GNU, Clang, or Intel)")
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CXX_COMPILER_VENDOR "gnu")
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CXX_COMPILER_VENDOR "clang")
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "IntelLLVM")
    set(CXX_COMPILER_VENDOR "intel")
  else()
    message(FATAL_ERROR "Unsupported CXX compiler for Linux-only build (expected GNU, Clang, or Intel)")
  endif()

  # Process header paths for Linux (GNU/Clang/Intel)
  if(shell AND (CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "IntelLLVM"))
    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/dirincludes "${CMAKE_BINARY_DIR}/include-staging" "${BACKEND_C_COMPILER_NAME_WITHOUT_PATH}_HEADERS"
      OUTPUT_VARIABLE C_includeString)
    string(STRIP "${C_includeString}" C_includeString)

    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/dirincludes "${CMAKE_BINARY_DIR}/include-staging" "${BACKEND_CXX_COMPILER_NAME_WITHOUT_PATH}_HEADERS"
      OUTPUT_VARIABLE CXX_includeString)
    string(STRIP "${CXX_includeString}" CXX_includeString)

    set(CXX_INCLUDE_STRING "${CXX_INCLUDE_STRING} ${CXX_includeString}")

    # Use the actual backend compiler instead of hardcoded gcc/g++
    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/get_compiler_header_dirs ${BACKEND_C_COMPILER} c ${C_COMPILER_VENDOR}
      OUTPUT_VARIABLE C_backend_includes)

    execute_process(
      COMMAND ${shell} ${CMAKE_SOURCE_DIR}/config/get_compiler_header_dirs ${BACKEND_CXX_COMPILER} c++ ${CXX_COMPILER_VENDOR}
      OUTPUT_VARIABLE CXX_backend_includes)
    string(REPLACE "\n" ";" C_backend_includes "${C_backend_includes}")
    string(REPLACE "\n" ";" CXX_backend_includes "${CXX_backend_includes}")

    set(C_INCLUDE_STRING "${C_INCLUDE_STRING} ${C_includeString}")
    foreach(C_include_dir ${C_backend_includes})
      string(STRIP "${C_include_dir}" C_include_dir)
      set(C_INCLUDE_STRING "${C_INCLUDE_STRING}, \"${C_include_dir}\"")
    endforeach()
    set(C_INCLUDE_STRING "${C_INCLUDE_STRING}, \"/usr/include\"")

    foreach(CXX_include_dir ${CXX_backend_includes})
      string(STRIP "${CXX_include_dir}" CXX_include_dir)
      set(CXX_INCLUDE_STRING "${CXX_INCLUDE_STRING}, \"${CXX_include_dir}\"")
    endforeach()
    set(CXX_INCLUDE_STRING "${CXX_INCLUDE_STRING}, \"/usr/include\"")

    # ROSE stages compiler-specific headers under include-staging in the build tree and installs them under
    # <prefix>/include. Translators select include paths based on whether they run from the build or install tree,
    # so keep a build-tree symlink to mirror the install layout.
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/include-staging ${CMAKE_BINARY_DIR}/include)
endif()
install(FILES ${ROSE_TOP_SRC_DIR}/src/Rose/SourceLocation.h DESTINATION include/Rose)
else()
  set(C_INCLUDE_STRING "{\"none\"")
  set(CXX_INCLUDE_STRING "{\"none\"")
endif()
set(C_INCLUDE_STRING "${C_INCLUDE_STRING}}")
set(CXX_INCLUDE_STRING "${CXX_INCLUDE_STRING}}")

########################################################################################################################
# Private and public build-time configuration files
########################################################################################################################

# Private configuration file.
#
# The rose_config.h file contains CPP macros describing what features were detected during the configuration phase.
# This file should not be installed and should not be #include'd into user-level code because it pollutes the global
# name space.  This needs to be here at the end in order to catch all the variables defined above such as compiler
# names, versions, etc.
configure_file(${ROSE_TOP_SRC_DIR}/rose_config.h.in.cmake ${ROSE_TOP_BINARY_DIR}/rose_config.h)

# Public configuration file.
#
# The rosePublicConfig.h header is generated from rose_config.h by copying certain CPP macros from rose_config.h to
# rosePublicConfig.h and changing their names so as not to pollute the global name space.  See
# scripts/publicConfiguration.pl for details.

########################################################################################################################
# Ancillary ROSE components. These are things that are not part of the ROSE library.
########################################################################################################################

if(ROSE_ENABLE_PCH)
  add_library(rosePublicPch OBJECT cmake/pch/rose_public_pch.C)
  target_include_directories(rosePublicPch PRIVATE ${ROSE_INCLUDES})
  target_compile_definitions(rosePublicPch PRIVATE disc_union=union)
  target_link_libraries(rosePublicPch PRIVATE ROSE_DLL ${link_with_libraries})
  target_compile_features(rosePublicPch PRIVATE cxx_std_14)
  add_dependencies(rosePublicPch ROSE_DLL rosetta_generated generate_rosePublicConfig)
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

########################################################################################################################
# Install RMC configuration file if present
########################################################################################################################
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

########################################################################################################################
# Miscellaneous final actions
########################################################################################################################

# These actions don't work on Linux Debian-based systems during "make install" and appear to not be necessary anyway.
if(NOT REX_ENABLE_WASM AND NOT UNIX)
  # This has to remain the last subdirectory so that all the targets are installed before bundle.cmake runs.
  add_subdirectory(cmake)

  # This include file defines how CPack should create our installers.
  include(cmake/ROSECPack.cmake)
endif()
