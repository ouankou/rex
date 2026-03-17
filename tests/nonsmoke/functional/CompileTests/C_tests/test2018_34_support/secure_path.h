/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_SECURE_PATH_H
#define TEST2018_34_SUPPORT_SECURE_PATH_H

#include <sys/stat.h>
#include <sys/types.h>

#define SUDO_PATH_SECURE 0
#define SUDO_PATH_MISSING -1
#define SUDO_PATH_BAD_TYPE -2
#define SUDO_PATH_WRONG_OWNER -3
#define SUDO_PATH_WORLD_WRITABLE -4
#define SUDO_PATH_GROUP_WRITABLE -5

int sudo_secure_dir(const char *path, uid_t uid, gid_t gid, struct stat *sbp);
int sudo_secure_file(const char *path, uid_t uid, gid_t gid, struct stat *sbp);
int sudo_secure_path(const char *path, int type, uid_t uid, gid_t gid,
                     struct stat *sbp);

#endif
