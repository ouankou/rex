# Native package discovery supplies BISON_TARGET and validates the executable.
# A requested prefix owns executable selection, including on reconfiguration.
if("${BISON_ROOT}" STREQUAL "no")
  message(FATAL_ERROR "Bison is required by the parsers and cannot be disabled")
elseif(NOT "${BISON_ROOT}" STREQUAL "")
  unset(_rex_bison_from_root CACHE)
  find_program(_rex_bison_from_root NAMES bison
    PATHS "${BISON_ROOT}/bin" NO_DEFAULT_PATH)
  if(NOT _rex_bison_from_root)
    message(FATAL_ERROR "Bison requested at '${BISON_ROOT}' but not found")
  endif()
  set(BISON_EXECUTABLE "${_rex_bison_from_root}" CACHE FILEPATH
    "Bison executable selected by BISON_ROOT" FORCE)
  unset(_rex_bison_from_root CACHE)
endif()
find_package(BISON REQUIRED)
