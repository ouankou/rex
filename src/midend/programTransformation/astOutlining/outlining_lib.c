#include "outlining_lib.h"

#include <assert.h>

#include <stdarg.h>

#include <stdio.h>

#include <stdlib.h>

static void *functionLib = 0;

static funcPointerT findWithDlopen(const char *function_name,
                                   const char *lib_name) {
  functionLib = dlopen(lib_name, RTLD_LAZY);
  if (functionLib == NULL) {
    const char *error = dlerror();
    printf("Error: cannot open .so file named: %s with error code:%s\n",
           lib_name, error);
    exit(1);
  }
  // find the function
  dlerror();
  funcPointerT result = (funcPointerT)dlsym(functionLib, function_name);
  const char *error = dlerror();
  if (error) {
    printf("Error: cannot find function named:%s within a .so file named %s, "
           "error code:%s .\n",
           function_name, lib_name, error);
    exit(1);
  }
  return result;
}

funcPointerT findFunctionUsingDlopen(const char *function_name,
                                     const char *lib_name) {
  return findWithDlopen(function_name, lib_name);
}

int closeLibHandle() {
  if (functionLib == NULL) {
    fprintf(stderr, "outlined library has no open handle to close\n");
    abort();
  }
  if (dlclose(functionLib) != 0) {
    fprintf(stderr, "cannot close outlined library: %s\n", dlerror());
    abort();
  }
  functionLib = NULL;
  return 0;
}

bool rose_dynamic_library_exists(const char *filename) {
  // Function to check for existence of file.
  bool returnValue = false;

#ifdef VX_WORKS

#ifndef F_OK
#define F_OK 42

#endif
  int mode = 42;
  // if ( open( filename.c_str(), F_OK, mode ) != -1 )
  if (open(filename, F_OK, mode) != -1)
#else
  // if ( access( filename.c_str(), F_OK ) != -1 )
  if (access(filename, F_OK) != -1)
#endif
  {
    returnValue = true;
  } else {
    printf("Note: lib file=%s is unavailable! \n", filename);
  }

  return returnValue;
}

// a variable argument helper function, to make the call site as simple as
// possible int count : = 2 (fixed lib file name and function name) + parameter
// count
//
// second parameter: function name of the outlined function in a shared library
// third parameter: filename of the shared library
// the remaining parameters: pointers to arguments to be passed by reference
// into the function
void findAndCallFunctionUsingDlopen(int num, ...) {
  if (num < 2) {
    fprintf(stderr, "outlined call requires a function and library name\n");
    abort();
  }

  va_list arguments;
  const int param_count = num - 2;
  void **out_argv =
      param_count > 0 ? (void **)malloc(sizeof(void *) * param_count) : NULL;

  if (param_count > 0 && out_argv == NULL) {
    fprintf(stderr, "out of memory\n");
    abort();
  }

  va_start(arguments, num);

  const char *func_name = va_arg(arguments, const char *);
  const char *lib_name = va_arg(arguments, const char *);
  for (int index = 0; index < param_count; ++index) {
    out_argv[index] = va_arg(arguments, void *);
  }

  va_end(arguments);

  if (func_name == NULL || lib_name == NULL) {
    fprintf(stderr,
            "outlined call requires non-null function and library names\n");
    abort();
  }
  funcPointerT func_p = findFunctionUsingDlopen(func_name, lib_name);
  (*func_p)(out_argv);

  free(out_argv);
}
