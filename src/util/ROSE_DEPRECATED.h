#ifndef ROSE_DEPRECATED_H
#define ROSE_DEPRECATED_H

#if defined(__clang__)
#define ROSE_DEPRECATED(WHY) __attribute__((deprecated(WHY)))
#elif defined(__GNUC__)
#define ROSE_DEPRECATED(WHY) __attribute__((deprecated))
#else
#define ROSE_DEPRECATED(WHY)
#endif

#undef ROSE_SUPPRESS_DEPRECATION_WARNINGS
#define ROSE_SUPPRESS_DEPRECATION_WARNINGS

#undef ROSE_DEPRECATED_FUNCTION
#define ROSE_DEPRECATED_FUNCTION

#undef ROSE_DEPRECATED_VARIABLE
#define ROSE_DEPRECATED_VARIABLE

#endif
