/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_GETTEXT_H
#define TEST2018_34_SUPPORT_GETTEXT_H

#include <locale.h>

#define _(String) (String)
#define N_(String) (String)
#define textdomain(Domain) ((void)0)
#define bindtextdomain(Package, Directory) ((void)0)
#define ngettext(String, StringPlural, N) ((N) == 1 ? (String) : (StringPlural))

#endif
