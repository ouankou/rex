#ifndef __ROSEDLL__
#define __ROSEDLL__

// When building a shared library with GNU-compatible compilers, use symbol
// visibility attributes to hide internal symbols by default.
#if __GNUC__ >= 4 && !defined(USE_ROSE)
    #define ROSE_DLL_HELPER_DLL_IMPORT __attribute__ ((visibility("default")))
    #define ROSE_DLL_HELPER_DLL_EXPORT __attribute__ ((visibility("default")))
    #define ROSE_DLL_HELPER_DLL_LOCAL  __attribute__ ((visibility("hidden")))
  #else
    #define ROSE_DLL_HELPER_DLL_IMPORT
    #define ROSE_DLL_HELPER_DLL_EXPORT
#define ROSE_DLL_HELPER_DLL_LOCAL
#endif

// ROSE_DLL_EXPORTS is only defined for cmake
  #ifdef ROSE_DLL_EXPORTS // defined if we are building the ROSE DLL (instead of using it)
    #define ROSE_DLL_API ROSE_DLL_HELPER_DLL_EXPORT
  #else
    #define ROSE_DLL_API ROSE_DLL_HELPER_DLL_IMPORT
  #endif // ROSE_DLL_DLL_EXPORTS
  #define ROSE_DLL_LOCAL ROSE_DLL_HELPER_DLL_LOCAL
  #ifdef ROSE_UTIL_EXPORTS
    #define ROSE_UTIL_API ROSE_DLL_HELPER_DLL_EXPORT
  #else
    #define ROSE_UTIL_API ROSE_DLL_HELPER_DLL_IMPORT
  #endif


// DQ (10/19/2010): Need to test if we can remove this.
// We should not reference CXX_IS_ROSE_ANALYSIS except in source code.
// tps : this is probably not needed anymore
// undef ROSE_ROSETTA_API if rose analyses itself.
// #if CXX_IS_ROSE_ANALYSIS
//  #undef ROSE_DLL_API
//  #define ROSE_DLL_API
//#endif 

#ifdef USE_ROSE
// #error "ROSE_DLL_API = "ROSE_DLL_API
// #define ROSE_DLL_API
#endif

#endif
