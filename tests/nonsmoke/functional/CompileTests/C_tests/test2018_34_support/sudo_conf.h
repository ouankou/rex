/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_SUDO_CONF_H
#define TEST2018_34_SUPPORT_SUDO_CONF_H

#include "list.h"

#define GROUP_SOURCE_ADAPTIVE 0
#define GROUP_SOURCE_STATIC 1
#define GROUP_SOURCE_DYNAMIC 2

struct plugin_info {
  struct plugin_info *prev;
  struct plugin_info *next;
  const char *path;
  const char *symbol_name;
  char *const *options;
  int lineno;
};
TQ_DECLARE(plugin_info);

void sudo_conf_read(void);
const char *sudo_conf_askpass_path(void);
const char *sudo_conf_noexec_path(void);
const char *sudo_conf_debug_flags(void);
struct plugin_info_list *sudo_conf_plugins(void);
bool sudo_conf_disable_coredump(void);
int sudo_conf_group_source(void);
int sudo_conf_max_groups(void);

#endif
