include_guard(GLOBAL)

# Resolve package-owned shared libraries to paths so installed REX targets do
# not require consumers to import LLVM's entire compiler target graph. CMake's
# LOCATION_<CONFIG> property applies the package's configuration mappings.
function(rex_imported_shared_library target output)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "Required package target '${target}' is missing")
  endif()
  get_target_property(_imported "${target}" IMPORTED)
  get_target_property(_type "${target}" TYPE)
  if(NOT _imported OR NOT _type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR
      "Required package target '${target}' must be an imported shared library; got '${_type}'")
  endif()

  if(CMAKE_CONFIGURATION_TYPES)
    set(_configurations ${CMAKE_CONFIGURATION_TYPES})
  elseif(CMAKE_BUILD_TYPE)
    set(_configurations "${CMAKE_BUILD_TYPE}")
  else()
    set(_configurations NOCONFIG)
  endif()
  set(_libraries "")
  foreach(_configuration IN LISTS _configurations)
    string(TOUPPER "${_configuration}" _upper_configuration)
    get_target_property(_location "${target}" "LOCATION_${_upper_configuration}")
    if(NOT _location OR NOT IS_ABSOLUTE "${_location}" OR
       NOT EXISTS "${_location}" OR IS_DIRECTORY "${_location}")
      message(FATAL_ERROR
        "Required package target '${target}' has no existing absolute shared-library location for '${_configuration}': '${_location}'")
    endif()
    if(CMAKE_CONFIGURATION_TYPES)
      list(APPEND _libraries "$<$<CONFIG:${_configuration}>:${_location}>")
    else()
      list(APPEND _libraries "${_location}")
    endif()
  endforeach()
  set("${output}" "${_libraries}" PARENT_SCOPE)
endfunction()
