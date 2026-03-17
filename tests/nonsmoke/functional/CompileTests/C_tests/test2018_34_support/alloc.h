/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_ALLOC_H
#define TEST2018_34_SUPPORT_ALLOC_H

#include <stdarg.h>
#include <stddef.h>

int easprintf(char **strp, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int evasprintf(char **strp, const char *fmt, va_list ap)
    __attribute__((format(printf, 2, 0)));
void efree(void *ptr);
void *ecalloc(size_t nmemb, size_t size);
void *emalloc(size_t size);
void *emalloc2(size_t nmemb, size_t size);
void *erealloc(void *ptr, size_t size);
void *erealloc3(void *ptr, size_t nmemb, size_t size);
void *erecalloc(void *ptr, size_t oldnmemb, size_t nmemb, size_t size);
char *estrdup(const char *str);
char *estrndup(const char *str, size_t len);

#endif
