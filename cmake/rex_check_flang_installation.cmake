cmake_minimum_required(VERSION 3.19)
include("${CMAKE_CURRENT_LIST_DIR}/rex_flang_frontend_manifest.cmake")
if(NOT IS_ABSOLUTE "${REX_FLANG_ROOT}")
  message(FATAL_ERROR "Flang root must be an absolute path: ${REX_FLANG_ROOT}")
endif()
foreach(library IN LISTS REX_FLANG_FRONTEND_LIBRARIES)
  set(found FALSE)
  foreach(path IN ITEMS
      "${REX_FLANG_ROOT}/lib/lib${library}.a"
      "${REX_FLANG_ROOT}/lib/lib${library}.so"
      "${REX_FLANG_ROOT}/lib64/lib${library}.a"
      "${REX_FLANG_ROOT}/lib64/lib${library}.so")
    if(EXISTS "${path}" AND NOT IS_DIRECTORY "${path}")
      set(found TRUE)
      break()
    endif()
  endforeach()
  if(NOT found)
    message(FATAL_ERROR
      "Flang root ${REX_FLANG_ROOT} is missing required frontend library lib${library}")
  endif()
endforeach()
foreach(header IN LISTS REX_FLANG_FRONTEND_HEADERS)
  set(path "${REX_FLANG_ROOT}/include/${header}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "Flang root ${REX_FLANG_ROOT} is missing required frontend header include/${header}")
  endif()
endforeach()
