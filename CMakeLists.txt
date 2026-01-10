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

# Helper function to detect and set compiler with preference order:
# 1. Use environment variable if set
# 2. Try preferred version (e.g., clang-20)
# 3. Try fallback versions (e.g., clang, flang-new)
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
        # Try fallback versions
        find_program(_COMPILER_FALLBACK NAMES ${FALLBACK_NAMES})
        if(_COMPILER_FALLBACK)
          set(${CMAKE_VAR} ${_COMPILER_FALLBACK} PARENT_SCOPE)
          message(STATUS "Auto-detected ${LANG_NAME} compiler: ${_COMPILER_FALLBACK}")
        endif()
      endif()
      unset(_COMPILER_PREFERRED CACHE)
      unset(_COMPILER_FALLBACK CACHE)
    endif()
  endif()
endfunction()

# Detect C and C++ compilers
detect_compiler(C CMAKE_C_COMPILER CC "C" "clang-20" "clang")
detect_compiler(CXX CMAKE_CXX_COMPILER CXX "C++" "clang++-20" "clang++")

set(_C_COMPILER_DISPLAY "${CMAKE_C_COMPILER}")
if(NOT _C_COMPILER_DISPLAY)
  set(_C_COMPILER_DISPLAY "<unspecified>")
endif()
set(_CXX_COMPILER_DISPLAY "${CMAKE_CXX_COMPILER}")
if(NOT _CXX_COMPILER_DISPLAY)
  set(_CXX_COMPILER_DISPLAY "<unspecified>")
endif()
message(STATUS "Compiler selection: Using ${_C_COMPILER_DISPLAY} and ${_CXX_COMPILER_DISPLAY}")

#-----------------------------------------------------------------------------------------------------------------------
# Read ROSE version from ROSE_VERSION file before calling project()
# This is needed so that the project version can be set correctly, which affects library versioning
#-----------------------------------------------------------------------------------------------------------------------
set(ROSE_SCM_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ROSE_VERSION")
if(NOT EXISTS "${ROSE_SCM_VERSION_FILE}")
  message(FATAL_ERROR "ROSE version file not found: ${ROSE_SCM_VERSION_FILE}")
endif()
file(STRINGS ${ROSE_SCM_VERSION_FILE} ROSE_PACKAGE_VERSION LIMIT_COUNT 1)

project(ROSE
  VERSION ${ROSE_PACKAGE_VERSION}
  LANGUAGES C CXX
)

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


########################################################################################################################
# ROSE version information
########################################################################################################################

# The ROSE version string was already read from ROSE_VERSION file before the project() command.
# The numerical version is encoded in SCM_DATE as ABBCCCDDDD.
set(ROSE_SCM_DATE_FILE "${PROJECT_SOURCE_DIR}/config/SCM_DATE")
if(NOT EXISTS "${ROSE_SCM_DATE_FILE}")
  message(FATAL_ERROR "ROSE SCM Date file not found: ${ROSE_SCM_DATE_FILE}")
endif()

file(STRINGS ${ROSE_SCM_VERSION_FILE} ROSE_SCM_VERSION_ID LIMIT_COUNT 1)

# Parse version into components for package configuration
# Note: PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, etc. are also set by project() command
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)" _version_match "${ROSE_PACKAGE_VERSION}")
if(NOT _version_match)
  message(FATAL_ERROR "Invalid ROSE version string: ${ROSE_PACKAGE_VERSION}")
endif()
set(ROSE_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(ROSE_VERSION_MINOR "${CMAKE_MATCH_2}")
set(ROSE_VERSION_PATCH "${CMAKE_MATCH_3}")
set(ROSE_VERSION_BUILD "${CMAKE_MATCH_4}")

# Compute the encoded numeric version and enforce consistency with SCM_DATE.
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
if(ENABLE-C)
  set(ROSE_BUILD_CXX_LANGUAGE_SUPPORT 1)
  set(ROSE_BUILD_C_LANGUAGE_SUPPORT 1)
endif()

# CUDA
option(ENABLE-CUDA "Enable CUDA analysis" OFF) # OFF because lack of CUDA is not handled properly
if(ENABLE-CUDA)
  find_package(CUDA REQUIRED)
  set(ROSE_BUILD_CUDA_LANGUAGE_SUPPORT 1)
endif()

# Fortran (disabled by default to avoid requiring a Fortran toolchain for basic builds)
option(ENABLE-FORTRAN "Enable Fortran analysis." OFF)

# JVM-based Open Fortran Parser (OFP) dependency (legacy frontend)
option(ENABLE-FORTRAN-OFP "Enable JVM-based Open Fortran Parser (OFP) support." ON)

# Experimental Flang frontend (f18)
option(ENABLE-FORTRAN-FLANG "Enable experimental Flang frontend (f18) support." OFF)
set(FLANG_ROOT "" CACHE PATH "Path to Flang installation (lib/, include/).")

# Legacy tests still reference enable-fortran (lowercase).
set(enable-fortran "${ENABLE-FORTRAN}")

if(ENABLE-FORTRAN AND NOT (ENABLE-FORTRAN-OFP OR ENABLE-FORTRAN-FLANG))
  message(FATAL_ERROR "Fortran analysis requires at least one frontend: OFP or Flang. Set -DENABLE-FORTRAN-OFP=ON or -DENABLE-FORTRAN-FLANG=ON, or disable Fortran.")
endif()

if(ENABLE-FORTRAN AND ENABLE-FORTRAN-OFP)
  message(STATUS "Looking for JVM (Fortran OFP dependency) ...")
  # The FindJava script is different than all our others--it doesn't consult JAVA_ROOT. Therefore, the only
  # value of JAVA_ROOT that we can support is the empty string.
  if("${JAVA_ROOT}" STREQUAL "")
  elseif("${JAVA_ROOT}" STREQUAL "no")
    message(FATAL_ERROR "Fortran OFP cannot be enabled when JAVA_ROOT is disabled (-DJAVA_ROOT=FALSE).")
  else()
    message(FATAL_ERROR "Java location cannot be explicit (-DJAVA_ROOT=...)")
  endif()

  find_package(Java REQUIRED)
  set(_rose_java_home "")
  if(NOT "$ENV{JAVA_HOME}" STREQUAL "")
    set(_rose_java_home "$ENV{JAVA_HOME}")
  endif()
  if(_rose_java_home STREQUAL "" AND Java_JAVA_EXECUTABLE)
    get_filename_component(_rose_java_real "${Java_JAVA_EXECUTABLE}" REALPATH)
    get_filename_component(_rose_java_bin "${_rose_java_real}" DIRECTORY)
    get_filename_component(_rose_java_home "${_rose_java_bin}" DIRECTORY)
  endif()
  if(_rose_java_home STREQUAL "" AND EXISTS "/usr/lib/jvm/default-java")
    set(_rose_java_home "/usr/lib/jvm/default-java")
  endif()
  if(_rose_java_home)
    set(ENV{JAVA_HOME} "${_rose_java_home}")
  endif()

  find_package(JNI REQUIRED)
  if(_rose_java_home)
    set(_rose_jvm_candidate "")
    set(_rose_jvm_name "libjvm${CMAKE_SHARED_LIBRARY_SUFFIX}")
    foreach(_rose_jvm_dir
      "${_rose_java_home}/lib/server"
      "${_rose_java_home}/jre/lib/server")
      if(EXISTS "${_rose_jvm_dir}/${_rose_jvm_name}")
        set(_rose_jvm_candidate "${_rose_jvm_dir}/${_rose_jvm_name}")
        break()
      endif()
    endforeach()
    if(_rose_jvm_candidate)
      set(_rose_jvm_is_stale FALSE)
      if(JAVA_JVM_LIBRARY AND EXISTS "${JAVA_JVM_LIBRARY}")
        foreach(_rose_prefix IN ITEMS "${CMAKE_INSTALL_PREFIX}" "${ROSE_TOP_BINARY_DIR}")
          if(_rose_prefix)
            string(FIND "${JAVA_JVM_LIBRARY}" "${_rose_prefix}" _rose_prefix_index)
            if(_rose_prefix_index EQUAL 0)
              set(_rose_jvm_is_stale TRUE)
            endif()
          endif()
        endforeach()
      endif()
      if(NOT JAVA_JVM_LIBRARY OR _rose_jvm_is_stale)
        set(JAVA_JVM_LIBRARY "${_rose_jvm_candidate}" CACHE FILEPATH "Path to Java JVM library" FORCE)
      endif()
      unset(_rose_prefix)
      unset(_rose_prefix_index)
      unset(_rose_jvm_is_stale)
    endif()
  endif()
  if(NOT JAVA_JVM_LIBRARY OR NOT EXISTS "${JAVA_JVM_LIBRARY}")
    message(FATAL_ERROR "Fortran OFP requires libjvm; set JAVA_HOME to a JDK with libjvm.so.")
  endif()

  set(USE_ROSE_OPEN_FORTRAN_PARSER_SUPPORT 1)
  set(OFP_JAVA_RUNTIME "${Java_JAVA_EXECUTABLE}")

  # Ensure CTest inherits a usable JAVA_HOME and libjvm path even when shell
  # init scripts are not sourced (e.g., Codex sandboxes).
  if(JAVA_JVM_LIBRARY)
    get_filename_component(_rose_jvm_dir "${JAVA_JVM_LIBRARY}" DIRECTORY)
    if(_rose_jvm_dir)
      set(ROSE_JVM_LIBRARY_DIR "${_rose_jvm_dir}")
    endif()
    set(_rose_ctest_env "${CMAKE_BINARY_DIR}/CTestFortranOFPEnv.cmake")
    set(_rose_ctest_env_content "")
    if(_rose_java_home)
      string(APPEND _rose_ctest_env_content
        "if(\"\$ENV{JAVA_HOME}\" STREQUAL \"\")\n"
        "  set(ENV{JAVA_HOME} \"${_rose_java_home}\")\n"
        "endif()\n")
    endif()
    string(APPEND _rose_ctest_env_content
      "if(\"\$ENV{LD_LIBRARY_PATH}\" STREQUAL \"\")\n"
      "  set(ENV{LD_LIBRARY_PATH} \"${_rose_jvm_dir}\")\n"
      "elseif(NOT \"\$ENV{LD_LIBRARY_PATH}\" MATCHES \"(^|:)${_rose_jvm_dir}(:|$)\")\n"
      "  set(ENV{LD_LIBRARY_PATH} \"${_rose_jvm_dir}:\$ENV{LD_LIBRARY_PATH}\")\n"
      "endif()\n")
    file(WRITE "${_rose_ctest_env}" "${_rose_ctest_env_content}")
    set_property(DIRECTORY PROPERTY TEST_INCLUDE_FILE "${_rose_ctest_env}")
  endif()
endif()

if(ENABLE-FORTRAN AND ENABLE-FORTRAN-FLANG)
  if(NOT LLVM_FOUND)
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
  list(REMOVE_DUPLICATES FLANG_INCLUDE_DIRS)

  set(_flang_lib_names FortranCommon FortranParser FortranSemantics
    FortranEvaluate FortranLower FortranRuntime FortranDecimal)
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

  add_compile_definitions(ROSE_EXPERIMENTAL_FLANG_ROSE_CONNECTION)
endif()

# OpenCL
option(ENABLE-OPENCL "Enable OpenCL analysis" ON)
if(ENABLE-OPENCL)
  find_package(OpenCL)
  set(ROSE_BUILD_OPENCL_LANGUAGE_SUPPORT 1)
  find_path(with-opencl-inc NAMES cl.h DOC "For OpenCL runtime library")
  find_library(with-opencl-lib OpenCL DOC "OpenCL library for runtime examples")
endif()

# Fortran
if(ENABLE-FORTRAN)
  # Detect Fortran compiler using the same helper function
  detect_compiler(Fortran CMAKE_Fortran_COMPILER FC "Fortran" "flang-20" "flang-new;flang")
  set(ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT 1)
  enable_language(Fortran)
  set(BACKEND_FORTRAN_COMPILER "${CMAKE_Fortran_COMPILER}")
  get_filename_component(BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH "${BACKEND_FORTRAN_COMPILER}" NAME)
  string(REGEX MATCH "[a-zA-Z0-9/.+-]+" BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH "${BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH}")
  set(_fortran_is_gnu 0)
  set(_fortran_is_llvm_flang 0)
  if("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "GNU" OR BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH MATCHES "gfortran")
    set(_fortran_is_gnu 1)
  elseif("${CMAKE_Fortran_COMPILER_ID}" STREQUAL "LLVMFlang" OR "${CMAKE_Fortran_COMPILER_ID}" STREQUAL "Flang" OR BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH MATCHES "flang")
    set(_fortran_is_llvm_flang 1)
  endif()
  if(_fortran_is_gnu)
    set(USE_GFORTRAN_IN_ROSE 1)
    set(BACKEND_FORTRAN_IS_GNU_COMPILER 1)
    add_compile_definitions(BACKEND_FORTRAN_IS_GNU_COMPILER=1)
    add_compile_definitions(BACKEND_FORTRAN_IS_LLVM_FLANG=0)
    message(STATUS "Using GNU Fortran backend: ${BACKEND_FORTRAN_COMPILER}")
  elseif(_fortran_is_llvm_flang)
    set(USE_GFORTRAN_IN_ROSE 0)
    set(BACKEND_FORTRAN_IS_GNU_COMPILER 0)
    add_compile_definitions(BACKEND_FORTRAN_IS_GNU_COMPILER=0)
    add_compile_definitions(BACKEND_FORTRAN_IS_LLVM_FLANG=1)
    message(STATUS "Using LLVM Flang backend: ${BACKEND_FORTRAN_COMPILER}")
  else()
    set(USE_GFORTRAN_IN_ROSE 0)
    set(BACKEND_FORTRAN_IS_GNU_COMPILER 0)
    add_compile_definitions(BACKEND_FORTRAN_IS_GNU_COMPILER=0)
    add_compile_definitions(BACKEND_FORTRAN_IS_LLVM_FLANG=0)
    message(STATUS "Using non-GNU Fortran backend: ${BACKEND_FORTRAN_COMPILER}")
  endif()
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

# Honor LLVM_HOME if provided for consistent LLVM/Clang discovery.
if(DEFINED ENV{LLVM_HOME})
  set(LLVM_ROOT "$ENV{LLVM_HOME}")
  set(Clang_ROOT "$ENV{LLVM_HOME}")
endif()

# Find LLVM/Clang (required for C/C++ support)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

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

# LLVM/Clang headers are added to the specific targets that need them.
# Avoid forcing LLVM include paths globally to prevent conflicts with
# the system C++ ABI headers when building third-party code (e.g., gtest).
# Prefer linking against shared LLVM library when available to avoid duplicate globals
# Try two methods to detect if shared LLVM library exists:
# Method 1: Check if "LLVM" target is in LLVM_AVAILABLE_LIBS (works on many systems)
# Method 2: Set LLVM_LINK_LLVM_DYLIB=ON and check result of llvm_map_components_to_libnames

# Method 1: Direct check for "LLVM" in available libraries list
if("LLVM" IN_LIST LLVM_AVAILABLE_LIBS)
  # Use shared LLVM library to avoid duplicate global objects and double-free errors
  set(LLVM_LIBS LLVM)
  message(STATUS "Using shared LLVM library (detected via LLVM_AVAILABLE_LIBS): ${LLVM_LIBS}")
else()
  # Method 2: Try using LLVM_LINK_LLVM_DYLIB with llvm_map_components_to_libnames
  set(LLVM_LINK_LLVM_DYLIB ON)
  llvm_map_components_to_libnames(LLVM_LIBS
    support core irreader
    option mc mcparser
    binaryformat bitreader bitwriter
    profiledata
    target targetparser
    frontendopenmp)

  # Check if we got the shared library (will be "LLVM" or "LLVM-<version>")
  # or static components (will be list like "LLVMSupport;LLVMCore;...")
  if(LLVM_LIBS MATCHES "^LLVM(-[0-9.]+)?$")
    message(STATUS "Using shared LLVM library (detected via llvm_map_components_to_libnames): ${LLVM_LIBS}")
  else()
    # No shared library available - using static components
    # Note: This may cause double-free errors at program exit with Clang frontend
    message(STATUS "Using static LLVM component libraries: ${LLVM_LIBS}")
    message(WARNING "Static LLVM linking may cause double-free errors with Clang frontend. "
                    "Recommend building LLVM with -DLLVM_BUILD_LLVM_DYLIB=ON for shared library support.")
  endif()
endif()

# Clang libraries needed
set(CLANG_LIBS
  clangFrontendTool
  clangStaticAnalyzerFrontend
  clangStaticAnalyzerCheckers
  clangStaticAnalyzerCore
  clangIndex
  clangFrontend
  clangCodeGen
  clangARCMigrate
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

if(CLANG_VERSION_MAJOR GREATER_EQUAL 15)
  list(APPEND CLANG_LIBS clangSupport)
endif()
if(CLANG_VERSION_MAJOR GREATER_EQUAL 16)
  list(APPEND CLANG_LIBS clangCrossTU clangASTMatchers)
endif()
if(CLANG_VERSION_MAJOR GREATER_EQUAL 18)
  list(APPEND CLANG_LIBS clangAPINotes clangTooling)
endif()

endif() # ENABLE-C

########################################################################################################################
# System features
########################################################################################################################

# A set of common features including endian, stdio.h, printf, size of long int, etc.
set(CMAKE_REQUIRED_QUIET ${QUIET})
include(ConfigureChecks)
set(CMAKE_REQUIRED_QUIET FALSE)

find_package(Perl REQUIRED)

# Optional Cereal serialization support (header-only)
set(CEREAL_ROOT "" CACHE PATH "Cereal installation directory (empty to auto-detect, 'no' to disable)")
include(FindCereal)
find_cereal()
if(CEREAL_FOUND)
  set(ROSE_HAVE_CEREAL 1)
else()
  unset(ROSE_HAVE_CEREAL)
endif()

########################################################################################################################
# Miscellaneous user-selectable features
########################################################################################################################

option(ENABLE-EXAMPLE-TRANSLATORS-DIRECTORY "Enable compilation and testing of exampleTranslators directory" OFF)
option(ENABLE-TESTS-DIRECTORY "Enable compilation and testing of the ROSE/tests directory" ON)
set(_rex_enable_testing_default "${ENABLE-TESTS-DIRECTORY}")
option(REX_ENABLE_TESTING "Enable REX tests (alias for ENABLE-TESTS-DIRECTORY)" ${_rex_enable_testing_default})
set(ENABLE-TESTS-DIRECTORY "${REX_ENABLE_TESTING}" CACHE BOOL "Enable compilation and testing of the ROSE/tests directory" FORCE)
if(ENABLE-TESTS-DIRECTORY)
  enable_testing()
  add_test(
    NAME rex_no_autotools
    COMMAND ${CMAKE_COMMAND}
      -DREX_SOURCE_DIR=${CMAKE_SOURCE_DIR}
      -DREX_BUILD_DIR=${CMAKE_BINARY_DIR}
      -P ${CMAKE_SOURCE_DIR}/cmake/check_no_autotools.cmake)
endif()
option(REX_ENABLE_DISABLED_TESTS "Enable tests marked disabled in CTest" OFF)
option(ENABLE-TUTORIAL-DIRECTORY "Enable compilation and testing of the ROSE/tutorial directory" OFF)

option(ENABLE-ADVANCED-WARNINGS "Support for an advanced uniform warning level for ROSE development" OFF)
if(ENABLE-ADVANCED-WARNINGS)
  message(WARNING "Using an advanced uniform warning level for ROSE development")
  set(ROSE_USE_UNIFORM_ADVANCED_WARNINGS_SUPPORT 1)
endif()

option(ENABLE-SANITIZER "Support address sanitizer and related sanitizer tools for ROSE development" OFF)
if(ENABLE-SANITIZER)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(_rose_sanitizer_flags "-fsanitize=address" "-fsanitize-recover=address")
    add_compile_options(
      $<$<COMPILE_LANGUAGE:C>:${_rose_sanitizer_flags}>
      $<$<COMPILE_LANGUAGE:CXX>:${_rose_sanitizer_flags}>)
    add_link_options(${_rose_sanitizer_flags})
    set(ROSE_USE_SANITIZER 1)
  else()
    message(WARNING "ENABLE-SANITIZER requested but compiler does not support sanitizer flags")
  endif()
endif()

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

option(ENABLE-MICROSOFT-EXTENSIONS "Enable internal support in ROSE for GNU language extensions" OFF)
if(ENABLE-MICROSOFT-EXTENSIONS)
  set(ROSE_SUPPORT_MICROSOFT_EXTENSIONS TRUE)
endif()

option(ENABLE-SMALLER-GENERATED-FILES "Build smaller generated Sage III IR files" OFF)
set(enable-smaller-generated-files ${ENABLE-SMALLER-GENERATED-FILES})

option(WITH-LIBHARU "Build bundled libharu PDF support" ON)
set(WITH-VALGRIND "" CACHE PATH "Valgrind installation prefix (empty to disable)")
set(WITH-VALGRIND-BIN "" CACHE PATH "Valgrind bin directory")
set(WITH-VALGRIND-INCLUDE "" CACHE PATH "Valgrind include directory")
set(WITH-VALGRIND-LIB "" CACHE PATH "Valgrind library directory")
set(WITH-GOMP-OMP-RUNTIME-LIBRARY "" CACHE PATH "GCC GOMP OpenMP runtime library directory")
set(WITH-OMNI-OMP-RUNTIME-SUPPORT "" CACHE PATH "Omni OpenMP runtime library directory")
set(WITH-LLVM-OMP-RUNTIME-LIBRARY "" CACHE PATH "LLVM OpenMP runtime library directory")

if(ENABLE-FORTRAN AND ENABLE-FORTRAN-OFP)
  set(enable-ofp-version "20200819-JDK8" CACHE STRING "version number for OFP")
  set(ROSE_OFP_VERSION_NUMBER "${enable-ofp-version}")
  set(ENV{CLASSPATH} "${CMAKE_SOURCE_DIR}/src/3rdPartyLibraries/antlr-jars/antlr-3.5.2-complete.jar:${CMAKE_SOURCE_DIR}/src/3rdPartyLibraries/fortran-parser/OpenFortranParser-${enable-ofp-version}.jar")
endif()

# To use Bison, set BISON_ROOT to the Bison installation prefix. To avoid Bison, use BISON_ROOT=no
include(FindBison)
find_bison()

# Zlib is required by libharu. To find it in a special place, set ZLIB_ROOT to its installation prefix.
include(FindZlib)
if(WITH-LIBHARU)
  find_zlib()
  set(ROSE_WITH_LIBHARU 1)
else()
  set(ZLIB_FOUND FALSE)
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
  WITH-GOMP-OMP-RUNTIME-LIBRARY
  WITH-OMNI-OMP-RUNTIME-SUPPORT
  WITH-LLVM-OMP-RUNTIME-LIBRARY)
foreach(_rose_var IN LISTS _rose_with_vars)
  if("${${_rose_var}}" STREQUAL "no" OR "${${_rose_var}}" STREQUAL "OFF")
    set(${_rose_var} "")
  endif()
endforeach()
unset(_rose_with_vars)
unset(_rose_var)

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

if(VALGRIND_BIN_PATH)
  find_program(VALGRIND_BINARY valgrind PATHS "${VALGRIND_BIN_PATH}" NO_DEFAULT_PATH)
  if(NOT VALGRIND_BINARY)
    message(FATAL_ERROR "Valgrind binary is missing, can't configure with Valgrind")
  endif()
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
endif()
if(VALGRIND_INCLUDE_PATH AND NOT EXISTS "${VALGRIND_INCLUDE_PATH}/valgrind/valgrind.h")
  message(FATAL_ERROR "valgrind.h is missing, can't compile with Valgrind")
endif()

if(WITH-GOMP-OMP-RUNTIME-LIBRARY)
  set(USE_ROSE_GOMP_OPENMP_LIBRARY 1)
  set(GCC_GOMP_OPENMP_LIB_PATH "${WITH-GOMP-OMP-RUNTIME-LIBRARY}")
  set(ROSE_INSTALLATION_PATH "${CMAKE_INSTALL_PREFIX}")
endif()
if(WITH-OMNI-OMP-RUNTIME-SUPPORT)
  set(USE_ROSE_OMNI_OPENMP_SUPPORT 1)
  set(OMNI_OPENMP_LIB_PATH "${WITH-OMNI-OMP-RUNTIME-SUPPORT}")
  set(ROSE_INSTALLATION_PATH "${CMAKE_INSTALL_PREFIX}")
endif()
if(WITH-LLVM-OMP-RUNTIME-LIBRARY)
  set(LLVM_OPENMP_LIB_PATH "${WITH-LLVM-OMP-RUNTIME-LIBRARY}")
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

find_library(M_LIB m REQUIRED)
find_library(RT_LIB rt)

# This is essential to find the right include path from either build or installation tree for a translator
if(HAVE_DLFCN_H)
  if(HAVE_DLADDR)
    set(use_rose_in_build_tree_var TRUE)
    # this following line won't work since it only set the environment variable for cmake's session not for ctest
    # session. Still no good way to set it within cmake fortunately,
    set($ENV{ROSE_IN_BUILD_TREE} ${ROSE_TOP_BINARY_DIR})
  endif()
else()
  set(use_rose_in_build_tree_var, FALSE)
endif()

set(LIBHARU_SOURCE_DIR ${ROSE_TOP_SRC_DIR}/src/3rdPartyLibraries/libharu-200910170404)
set(LIBHARU_BINARY_DIR ${ROSE_TOP_BINARY_DIR}/src/3rdPartyLibraries/libharu-200910170404)

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

# Check compilers and version numbers. The module is located in src/cmake/modules.
if(ENABLE-C)
  include(roseChooseBackendCompiler)
  include(roseGenerateBackendCompilerSpecificHeaders)
  install(DIRECTORY ${CMAKE_BINARY_DIR}/include-staging/ DESTINATION ${CLANG_BUILTIN_HEADERS_INSTALL_DIR})
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

set(ROSE_INCLUDES
  ${ROSE_TOP_BINARY_DIR}
  ${ROSE_TOP_BINARY_DIR}/src/util
  ${ROSE_TOP_BINARY_DIR}/src/frontend/SageIII/
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
  ${ROSE_TOP_SRC_DIR}/src/frontend/OpenFortranParser_SAGE_Connection
  ${ROSE_TOP_SRC_DIR}/src/frontend
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/formatSupport
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/languageIndependenceSupport
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/CxxCodeGeneration
  ${ROSE_TOP_SRC_DIR}/src/backend/unparser/FortranCodeGeneration
  ${ROSE_TOP_SRC_DIR}/src/util
  ${ROSE_TOP_SRC_DIR}/src/util/support
  ${ROSE_TOP_SRC_DIR}/src/util/graphs
  ${ROSE_TOP_SRC_DIR}/src/util/StringUtility
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

if(WITH-LIBHARU)
  list(APPEND ROSE_INCLUDES ${ROSE_TOP_SRC_DIR}/src/3rdPartyLibraries/libharu-2.1.0/include)
endif()

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
add_subdirectory(exampleTranslators)
add_subdirectory(scripts)

# Copy CTestCustom.cmake into the binary directory.  This file tells CTest to ignore some output that it otherwise
# considers compilation errors.
configure_file(cmake/CTestCustom.cmake.in ${CMAKE_BINARY_DIR}/CTestCustom.cmake @ONLY)

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
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This project now targets Linux only; non-Linux builds are unsupported.")
endif()

# Linux-only header discovery (GNU/Clang toolchains)
if(ENABLE-C)
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
  set(C_INCLUDE_STRING "${C_INCLUDE_STRING}}")
  set(CXX_INCLUDE_STRING "${CXX_INCLUDE_STRING}}")
else()
  set(C_INCLUDE_STRING {\"none\"})
  set(CXX_INCLUDE_STRING {\"none\"})
endif()


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

add_subdirectory(tools)
if(ENABLE-TESTS-DIRECTORY)
  add_subdirectory(tests)
endif()
if(ENABLE-TUTORIAL-DIRECTORY)
  add_subdirectory(tutorial)
endif()

########################################################################################################################
# Install RMC configuration file if present
########################################################################################################################
# When ROSE dependencies are managed with "rmc", a file named ".spock" is created at the top of the build tree.
# Install this file as "lib/rmc/rose" if RMC is active.
if(DEFINED ENV{SPOCK_EMPLOYED})
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
if(NOT UNIX)
  # This has to remain the last subdirectory so that all the targets are installed before bundle.cmake runs.
  add_subdirectory(cmake)

  # This include file defines how CPack should create our installers.
  include(cmake/ROSECPack.cmake)
endif()
