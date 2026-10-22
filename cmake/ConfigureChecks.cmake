include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckTypeSize)

# These checks have active AST, path-resolution, tutorial, and test consumers.
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
function(rex_check_dladdr)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_DL_LIBS})
  unset(HAVE_DLADDR CACHE)
  check_function_exists(dladdr HAVE_DLADDR)
endfunction()
rex_check_dladdr()

# Retain the size metadata exported through rosePublicConfig.h.
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
