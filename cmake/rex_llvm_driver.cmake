# Package roots and CMAKE_PREFIX_PATH retain CMake's precedence over these hints.
function(rex_llvm_package_hints compiler output)
  set(hints "$ENV{LLVM_HOME}")
  if(NOT REX_ENABLE_WASM AND NOT CMAKE_TOOLCHAIN_FILE AND
     compiler AND EXISTS "${compiler}")
    get_filename_component(directory "${compiler}" DIRECTORY)
    get_filename_component(prefix "${directory}/.." ABSOLUTE)
    if(EXISTS "${prefix}/lib/cmake/llvm/LLVMConfig.cmake" AND
       EXISTS "${prefix}/lib/cmake/clang/ClangConfig.cmake")
      list(APPEND hints "${prefix}")
    endif()
  endif()
  list(REMOVE_DUPLICATES hints)
  set(${output} "${hints}" PARENT_SCOPE)
endfunction()

# Resolve an executable owned by the selected LLVM package. Preserve its
# invocation spelling: clang++ may be a symlink whose name selects C++ mode.
function(rex_require_llvm_driver name output)
  if(NOT LLVM_TOOLS_BINARY_DIR OR
     NOT IS_ABSOLUTE "${LLVM_TOOLS_BINARY_DIR}")
    message(FATAL_ERROR
      "LLVMConfig.cmake must publish an absolute LLVM_TOOLS_BINARY_DIR.")
  endif()
  set(driver "")
  foreach(candidate IN ITEMS "${name}-${CLANG_VERSION_MAJOR}" "${name}")
    set(path "${LLVM_TOOLS_BINARY_DIR}/${candidate}")
    if(EXISTS "${path}")
      if(IS_DIRECTORY "${path}")
        message(FATAL_ERROR "LLVM driver path is a directory: '${path}'")
      endif()
      set(driver "${path}")
      break()
    endif()
  endforeach()
  if(NOT driver)
    message(FATAL_ERROR
      "The loaded LLVM package has no ${name} ${CLANG_VERSION_MAJOR} driver "
      "in '${LLVM_TOOLS_BINARY_DIR}'.")
  endif()
  file(REAL_PATH "${driver}" real_driver)
  file(REAL_PATH "${LLVM_TOOLS_BINARY_DIR}" tools_directory)
  get_filename_component(driver_directory "${real_driver}" DIRECTORY)
  if(NOT driver_directory STREQUAL tools_directory)
    message(FATAL_ERROR
      "LLVM driver '${driver}' resolves outside the loaded package tools "
      "directory '${tools_directory}': '${real_driver}'")
  endif()
  execute_process(COMMAND "${driver}" -dumpversion
    OUTPUT_VARIABLE version OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE error RESULT_VARIABLE status)
  if(NOT status STREQUAL "0" OR
     NOT version MATCHES "^${CLANG_VERSION_MAJOR}([.][0-9]+)*$")
    message(FATAL_ERROR
      "LLVM driver '${driver}' must report Clang major ${CLANG_VERSION_MAJOR}. "
      "Got status '${status}', version '${version}', error '${error}'.")
  endif()
  set(${output} "${driver}" PARENT_SCOPE)
endfunction()

function(rex_require_clang_resource_directory driver output)
  execute_process(COMMAND "${driver}" --print-resource-dir
    OUTPUT_VARIABLE directory OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE error RESULT_VARIABLE status)
  if(NOT status STREQUAL "0" OR
     NOT IS_ABSOLUTE "${directory}" OR NOT IS_DIRECTORY "${directory}")
    message(FATAL_ERROR
      "Clang resource directory resolution failed for '${driver}': "
      "status '${status}', output '${directory}', error '${error}'.")
  endif()
  file(REAL_PATH "${directory}" directory)
  set(${output} "${directory}" PARENT_SCOPE)
endfunction()
