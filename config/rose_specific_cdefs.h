#ifndef	_ROSE_SPECIFIC_SYS_CDEFS_H
#define	_ROSE_SPECIFIC_SYS_CDEFS_H	1

/* This file is required to avoid compiling VLA
   specific code in Red Hat Linus 7.3
   Avoids problem error:
   "/usr/include/_G_config.h", line 50: error: type containing an unknown-size array is not allowed
      struct __gconv_info __cd; 
                          ^

   Change this path if your C compiler include files are not in the normal place.
   This should be the standard location for this file, but it will have to be 
   modified if it is in a different place.
 */

#ifdef __attribute_malloc__
   #undef __attribute_malloc__
#endif

/* CLANG FRONTEND: Unified cross-platform solution
 * Use a placeholder path to trigger automatic architecture detection.
 *
 * The create_system_headers build script will detect this placeholder doesn't exist
 * and automatically find the correct arch-specific cdefs.h for the build system:
 *   - x86_64:      /usr/include/x86_64-linux-gnu/sys/cdefs.h
 *   - aarch64:     /usr/include/aarch64-linux-gnu/sys/cdefs.h
 *   - loongarch64: /usr/include/loongarch64-linux-gnu/sys/cdefs.h
 *   - riscv64:     /usr/include/riscv64-linux-gnu/sys/cdefs.h
 *   - etc.
 *
 * This works on ALL architectures without per-platform #ifdef directives.
 * The script now excludes problematic newlib paths and prefers *-linux-gnu paths.
 */
#include "/ROSE_WILL_AUTO_DETECT_ARCH_SPECIFIC_PATH/sys/cdefs.h"

/* Define __flexarr to not require use of Variable Length Array (VLA) feature. */
#undef __flexarr
#define __flexarr [1]

/* endif for _ROSE_SPECIFIC_SYS_CDEFS_H */
#endif

