# CMake has already identified the enabled languages. Publish that metadata for
# generated configuration headers instead of rediscovering compilers or parsing
# their human-readable banners through shell pipelines.
function(rex_cpp_string_literal value output)
  string(REPLACE "\\" "\\\\" value "${value}")
  string(REPLACE "\"" "\\\"" value "${value}")
  # Public configuration headers also have C consumers, which process trigraphs.
  string(REPLACE "?" "\\?" value "${value}")
  string(REPLACE "\n" "\\n" value "${value}")
  string(REPLACE "\r" "\\r" value "${value}")
  set(${output} "\"${value}\"" PARENT_SCOPE)
endfunction()

function(rose_backend_compiler_metadata language backend_language)
  set(_compiler "${CMAKE_${language}_COMPILER}")
  set(_version "${CMAKE_${language}_COMPILER_VERSION}")
  if(NOT _compiler OR NOT CMAKE_${language}_COMPILER_LOADED OR
     NOT CMAKE_${language}_COMPILER_ID)
    message(FATAL_ERROR
      "Backend ${language} metadata requires an enabled, identified CMake compiler")
  endif()

  # An independent BACKEND_* override cannot inherit the identity/version of
  # CMake's selected compiler. Configure that compiler through CMAKE_* instead.
  if(BACKEND_${backend_language}_COMPILER)
    get_filename_component(_backend_compiler
      "${BACKEND_${backend_language}_COMPILER}" REALPATH)
    get_filename_component(_configured_compiler "${_compiler}" REALPATH)
    if(NOT _backend_compiler STREQUAL _configured_compiler)
      message(FATAL_ERROR
        "BACKEND_${backend_language}_COMPILER must identify the configured "
        "CMAKE_${language}_COMPILER '${_compiler}'; configure the compiler "
        "through CMAKE_${language}_COMPILER instead")
    endif()
  endif()

  if(NOT _version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(\\.[0-9]+)?$")
    message(FATAL_ERROR
      "CMake did not provide a complete numeric ${language} compiler version: '${_version}'")
  endif()
  set(BACKEND_${backend_language}_COMPILER_MAJOR_VERSION_NUMBER
    "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(BACKEND_${backend_language}_COMPILER_MINOR_VERSION_NUMBER
    "${CMAKE_MATCH_2}" PARENT_SCOPE)
  get_filename_component(_name "${_compiler}" NAME)
  set(BACKEND_${backend_language}_COMPILER "${_compiler}" PARENT_SCOPE)
  set(BACKEND_${backend_language}_COMPILER_NAME_WITHOUT_PATH
    "${_name}" PARENT_SCOPE)
  separate_arguments(_command NATIVE_COMMAND "${CMAKE_${language}_COMPILER_ARG1}")
  list(PREPEND _command "${_compiler}")
  set(BACKEND_${backend_language}_COMPILER_COMMAND "${_command}" PARENT_SCOPE)
endfunction()

# A mixed-language link needs the runtimes that its driver does not add itself.
# Use CMake's compiler detection results; library names are toolchain metadata.
function(rex_backend_runtime_options language driver output)
  set(_options)
  foreach(_directory IN LISTS CMAKE_${language}_IMPLICIT_LINK_DIRECTORIES)
    if(NOT _directory IN_LIST CMAKE_${driver}_IMPLICIT_LINK_DIRECTORIES)
      list(APPEND _options "-L${_directory}")
    endif()
  endforeach()
  foreach(_directory IN LISTS CMAKE_${language}_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES)
    if(NOT _directory IN_LIST CMAKE_${driver}_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES)
      list(APPEND _options "-F${_directory}")
    endif()
  endforeach()
  foreach(_library IN LISTS CMAKE_${language}_IMPLICIT_LINK_LIBRARIES)
    if(NOT _library IN_LIST CMAKE_${driver}_IMPLICIT_LINK_LIBRARIES)
      if(IS_ABSOLUTE "${_library}" OR _library MATCHES "^-")
        list(APPEND _options "${_library}")
      else()
        list(APPEND _options "-l${_library}")
      endif()
    endif()
  endforeach()
  set(${output} "${_options}" PARENT_SCOPE)
endfunction()

rose_backend_compiler_metadata(C C)
rose_backend_compiler_metadata(CXX CXX)
if(ENABLE-FORTRAN)
  rose_backend_compiler_metadata(Fortran FORTRAN)
endif()

foreach(_path_field IN ITEMS
    CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_Fortran_COMPILER
    ROSE_TOP_BINARY_DIR ROSE_TOP_SRC_DIR
    ROSE_LLVM_OPENMP_RUNTIME_LIBRARY ROSE_CLANG_CUDA_RUNTIME_INCLUDE_DIR
    LLVM_OPENMP_INCLUDE_PATH)
  rex_cpp_string_literal("${${_path_field}}" REX_${_path_field}_LITERAL)
endforeach()
unset(_path_field)

set(BACKEND_CXX_IS_CLANG_COMPILER 0)
set(BACKEND_CXX_IS_GNU_COMPILER 0)
set(BACKEND_CXX_IS_INTEL_COMPILER 0)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(BACKEND_CXX_IS_CLANG_COMPILER 1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(BACKEND_CXX_IS_GNU_COMPILER 1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel" OR
       CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
  set(BACKEND_CXX_IS_INTEL_COMPILER 1)
endif()
