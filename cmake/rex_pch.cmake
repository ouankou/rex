function(rose_enable_private_pch target header)
  if(NOT ROSE_ENABLE_PCH)
    return()
  endif()
  if(NOT TARGET "${target}")
    message(FATAL_ERROR
      "rose_enable_private_pch(${target} ${header}) requires an existing CMake target.")
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
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rose_reuse_pch(${target} ${pch_target}) requires an existing target '${target}'.")
  endif()
  if(NOT TARGET "${pch_target}")
    message(FATAL_ERROR "rose_reuse_pch(${target} ${pch_target}) requires an existing target '${pch_target}'.")
  endif()

  # The reusable PCH is C++. CMake's REUSE_FROM applies to every language
  # in the consumer, so mark other compiled languages explicitly on their
  # source files. Query CMake's LANGUAGE metadata, including explicit overrides.
  get_target_property(_rose_source_dir "${target}" SOURCE_DIR)
  get_target_property(_rose_sources "${target}" SOURCES)
  foreach(_rose_source IN LISTS _rose_sources)
    get_filename_component(_rose_source_path "${_rose_source}" ABSOLUTE
      BASE_DIR "${_rose_source_dir}")
    get_source_file_property(_rose_language "${_rose_source_path}"
      TARGET_DIRECTORY "${target}" LANGUAGE)
    if(_rose_language AND NOT _rose_language STREQUAL "CXX")
      set_source_files_properties("${_rose_source_path}" TARGET_DIRECTORY "${target}"
        PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
    endif()
  endforeach()
  target_precompile_headers(${target} REUSE_FROM ${pch_target})
endfunction()
