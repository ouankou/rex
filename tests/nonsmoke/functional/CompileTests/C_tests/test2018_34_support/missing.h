/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_MISSING_H
#define TEST2018_34_SUPPORT_MISSING_H

#include <stddef.h>

#ifndef __printflike
#define __printflike(fmt_index, first_arg)                                     \
  __attribute__((format(printf, fmt_index, first_arg)))
#endif

const char *getprogname(void);
void setprogname(const char *name);

#endif
