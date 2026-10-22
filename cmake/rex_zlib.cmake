# Native PDF output requires zlib. Resolve it before LLVM imports ZLIB::ZLIB so
# an explicit prefix governs both consumers, including after reconfiguration.
if("${ZLIB_ROOT}" STREQUAL "no")
  message(FATAL_ERROR "Zlib is required by libharu and cannot be disabled")
elseif(NOT "${ZLIB_ROOT}" STREQUAL "")
  unset(_rex_zlib_include CACHE)
  unset(_rex_zlib_library CACHE)
  find_path(_rex_zlib_include NAMES zlib.h
    PATHS "${ZLIB_ROOT}/include" NO_DEFAULT_PATH)
  find_library(_rex_zlib_library NAMES z
    PATHS "${ZLIB_ROOT}/lib" NO_DEFAULT_PATH)
  if(NOT _rex_zlib_include OR NOT _rex_zlib_library)
    message(FATAL_ERROR "Zlib requested at '${ZLIB_ROOT}' but not found")
  endif()
  set(ZLIB_INCLUDE_DIR "${_rex_zlib_include}" CACHE PATH
    "Zlib headers selected by ZLIB_ROOT" FORCE)
  set(ZLIB_LIBRARY "${_rex_zlib_library}" CACHE FILEPATH
    "Zlib library selected by ZLIB_ROOT" FORCE)
  unset(ZLIB_LIBRARY_RELEASE CACHE)
  unset(ZLIB_LIBRARY_DEBUG CACHE)
  unset(_rex_zlib_include CACHE)
  unset(_rex_zlib_library CACHE)
endif()
find_package(ZLIB REQUIRED)
