/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_FILEOPS_H
#define TEST2018_34_SUPPORT_FILEOPS_H

#include <stdio.h>
#include <sys/types.h>

struct timeval;

#define SUDO_LOCK 1
#define SUDO_TLOCK 2
#define SUDO_UNLOCK 4

bool lock_file(int fd, int flags);
int touch(int fd, char *path, struct timeval *tv);
char *sudo_parseln(FILE *fp);

#endif
