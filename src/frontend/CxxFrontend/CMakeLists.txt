# There are three ways to get the EDG library:
#   If C/C++ analysis is not configured in ROSE, we can use a mostly empty dummy library.
#   If using Clang frontend instead of EDG, use the dummy library.
#   If we have EDG source code ("EDG" subdirectory) we can compile it.
#   Otherwise we can download a precompiled version from the ROSE team.
if(enable-c)
  if(enable-clang-frontend)
    # Using Clang frontend, need dummy EDG stubs
    add_library(EDG STATIC edg-dummy.C)
    # Add Clang frontend subdirectory
    add_subdirectory(Clang)
  elseif(have_EDG_source)
    add_subdirectory(EDG)
  endif()
else()
  add_library(EDG STATIC edg-dummy.C)
endif()
