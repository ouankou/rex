/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_ERROR_H
#define TEST2018_34_SUPPORT_ERROR_H

#include <stdarg.h>

void warning(const char *fmt, ...);
void warningx(const char *fmt, ...);
void vwarning(const char *fmt, va_list ap);
void vwarningx(const char *fmt, va_list ap);

#endif
