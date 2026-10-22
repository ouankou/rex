#ifndef __ROSEDLL__
#define __ROSEDLL__

#if __GNUC__ >= 4 && !defined(USE_ROSE)
#define ROSE_DLL_API __attribute__((visibility("default")))
#define ROSE_UTIL_API __attribute__((visibility("default")))
#define ROSE_DLL_LOCAL __attribute__((visibility("hidden")))
#else
#define ROSE_DLL_API
#define ROSE_UTIL_API
#define ROSE_DLL_LOCAL
#endif

#endif
