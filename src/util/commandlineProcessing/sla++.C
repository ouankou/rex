#include "rose_config.h"

/*
String list assignment (sla) functions to process assignment instructions and
flags. Copyright (C) 1998,1999 Brian T. N. Gunney

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.

Brian T. N. Gunney, brlynn@minn.net
*/

#include <iostream>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

#include "sla.h"

static int debug = 0;
void sla_set_debug(int d) {
  debug = d;
  printf("SLA debug is set to %d\n", debug);
}

SLA_FCN_BEGIN(sla_str, std::string) {

#ifdef SLA_DEBUG
  if (debug >= 16)
    std::cout << "Entered sla." << '\n'
              << "  argc:  " << argv.size() << '\n'
              << "  flind: " << flind << '\n'
              << "  assop: " << assop << '\n'
              << "  pname: " << pname << '\n'
              << "  value:   " << value << '\n'
              << "  argd:  " << argd << std::endl;
#endif
  int nvalue = 0;
  regex_t flind_re, pname_re, assop_re, sup_assop_re;
  regmatch_t pm;
  int errf;
  int prefer_detached_over_absent = 0;
  unsigned int assopmatchlength;
  const char *attachedargbegin;
  int assop_ends_str;

#ifdef SLA_DEBUG
  if (debug >= 16)
    std::cout << "Forming search string." << std::endl;
#endif

  std::string flind_exp = "^" + flind;
#ifdef SLA_DEBUG
  if (debug >= 2)
    std::cout << "flind expression: '" << flind_exp << "'" << std::endl;
#endif
  errf = regcomp(&flind_re, flind_exp.c_str(), REG_EXTENDED);
  if (errf != 0) {
    char buf[256];
    (void)regerror(errf, &flind_re, buf, sizeof(buf));
    std::cout << buf << std::endl;
    abort(); /* report error */
  }

  std::string pname_exp =
      "^" + pname.substr((!pname.empty() && pname[0] == '*'));
#ifdef SLA_DEBUG
  if (debug >= 2)
    std::cout << "pname expression: '" << pname_exp << "'" << std::endl;
#endif
  errf = regcomp(&pname_re, pname_exp.c_str(), REG_EXTENDED);
  if (errf != 0) {
    char buf[256];
    (void)regerror(errf, &pname_re, buf, sizeof(buf));
    std::cout << buf << std::endl;
    abort(); /* report error */
  }

  std::string assop_exp = "^" + assop;
  if (assop_exp[assop_exp.size() - 1] == '^') {
    prefer_detached_over_absent = 1;
    assop_exp.resize(assop_exp.size() - 1);
  }
#ifdef SLA_DEBUG
  if (debug >= 2)
    std::cout << "assop expression: '" << assop_exp << "'" << std::endl;
#endif
  errf = regcomp(&assop_re, assop_exp.c_str(), REG_EXTENDED);
  if (errf != 0) {
    char buf[256];
    (void)regerror(errf, &assop_re, buf, sizeof(buf));
    std::cout << buf << std::endl;
    abort(); /* report error */
  }

  std::string sup_assop_exp = assop_exp + 'x';
#ifdef SLA_DEBUG
  if (debug >= 2)
    std::cout << "sup_assop expression: '" << sup_assop_exp << "'" << std::endl;
#endif
  errf = regcomp(&sup_assop_re, sup_assop_exp.c_str(), REG_EXTENDED);
  if (errf != 0) {
    char buf[256];
    (void)regerror(errf, &sup_assop_re, buf, sizeof(buf));
    std::cout << buf << std::endl;
    abort(); /* report error */
  }

  /* Delete found assignment instructions or not. */
  /* argd  > 0 -> delete.
     argd == 0 -> chose automatically.
     argd  < 0 -> don't delete.
  */
  if (argd == 0) { /* Determine automatically. */
    if (assop == "$")
      argd = 1; /* Delete any found. */
    else
      argd = value != NULL ? 1 : -1; /* Delete as assigned to value. */
  }
#ifdef SLA_DEBUG
  if (debug >= 16) {
    std::cout << "Variable argd is " << argd << "\n";
  }
  if (debug >= 4) {
    std::cout << "Found assignment instructions will "
              << (argd > 0 ? "" : "NOT ") << "be deleted\n";
  }
#endif

  for (size_t i = 0; i < argv.size();) {

#ifdef SLA_DEBUG
    if (debug >= 4)
      std::cout << "trying: argv[" << i << "] = '" << argv[i] << "'"
                << std::endl;
#endif

    int offset;

    offset = 0;

    pm.rm_so = pm.rm_eo = -1;
    errf = regexec(&flind_re, argv[i].c_str() + offset, (size_t)1, &pm, 0);
    if (errf == REG_NOMATCH) {
      ++i;
      continue;
    }
    if (errf == REG_ESPACE) { /* error */
      std::cerr << "**** Out of memory in regexec: " << __LINE__ << " of "
                << __FILE__;
      nvalue = -1;
      break;
    }
    offset += pm.rm_eo;

    pm.rm_so = pm.rm_eo = -1;
    errf = regexec(&pname_re, argv[i].c_str() + offset, (size_t)1, &pm, 0);
    if (errf == REG_NOMATCH) {
      ++i;
      continue;
    }
    if (errf == REG_ESPACE) { /* error */
      std::cerr << "**** Out of memory in regexec: " << __LINE__ << " of "
                << __FILE__;
      nvalue = -1;
      break;
    }
    offset += pm.rm_eo;

    pm.rm_so = pm.rm_eo = -1;
    errf = regexec(&assop_re, argv[i].c_str() + offset, (size_t)1, &pm, 0);
    if (errf == REG_NOMATCH) {
      ++i;
      continue;
    }
    if (errf == REG_ESPACE) { /* error */
      std::cerr << "**** Out of memory in regexec: " << __LINE__ << " of "
                << __FILE__;
      nvalue = -1;
      break;
    }
    assopmatchlength = pm.rm_eo - pm.rm_so;
#ifdef SLA_DEBUG
    if (debug >= 4)
      std::cout << "assop match length is " << assopmatchlength << std::endl;
#endif
    attachedargbegin = argv[i].c_str() + offset + pm.rm_eo;

    /*** did assop match and end-of-string? ***/
    std::string tstr = argv[i].substr(offset, assopmatchlength) + 'x';
    pm.rm_so = pm.rm_eo = -1;
    errf = regexec(&sup_assop_re, tstr.c_str(), (size_t)1, &pm, 0);
    if (errf == REG_ESPACE) { /* error */
      std::cerr << "**** Out of memory in regexec: " << __LINE__ << " of "
                << __FILE__;
      nvalue = -1;
      break;
    }
    assop_ends_str = (errf == REG_NOMATCH);
    const bool detached_value =
        assop_ends_str && (assopmatchlength || prefer_detached_over_absent);
    if (detached_value && i + 1 >= argv.size()) {
      std::cerr << "Missing value for option: " << argv[i] << std::endl;
      abort();
    }
#ifdef SLA_DEBUG
    if (debug >= 4)
      std::cout << "assop ends str is " << assop_ends_str << std::endl;
#endif
#ifdef SLA_DEBUG
    if (debug >= 4)
      std::cout << "matched with " << argv[i] << std::endl;
#endif

#ifdef SLA_DEBUG
    if (debug >= 4) {
      if (!assop_ends_str) {
        std::cout << "using ATtached value '" << attachedargbegin << "'"
                  << std::endl;
      } else if (detached_value) {
        std::cout << "using DEtached value '" << argv[i + 1] << "'"
                  << std::endl;
      } else {
        std::cout << "using no value" << std::endl;
      }
    }
#endif

    /* The appropriate assignment instructions has been found.
       Take appropriate assignment and delete actions.
    */

    /*** Assignment actions. ***/
    if (value) {
      if (assop_ends_str) { /* either detached value or no value */
        if (detached_value) {
          value[(!pname.empty() && pname[0] == '*') ? nvalue : 0] = argv[i + 1];
        }
      } else { /* attached value (even if assop is "") */
        value[(!pname.empty() && pname[0] == '*') ? nvalue : 0] =
            attachedargbegin;
      }
    }

    /*** Delete actions ***/
    size_t advance = 1;
    if (assop_ends_str) { /* either detached value or no value */
      if (detached_value) {
        if ((argd > 0) || ((argd == 0) && value)) {
          /* Delete argv[i] and argv[i+1] from list. */
#ifdef SLA_DEBUG
          if (debug >= 4)
            std::cout << "Deleting " << i << ':' << argv[i] << " and " << i + 1
                      << ':' << argv[i + 1] << " from argv" << std::endl;
#endif
          argv.erase(argv.begin() + i, argv.begin() + i + 2);
          advance = 0;
        } else
          advance = 2;
      } else { /* no value */
        if (argd >= 0) {
          /* Delete argv[i] from list. */
#ifdef SLA_DEBUG
          if (debug >= 4)
            std::cout << "Deleting " << i << ':' << argv[i] << " from argv"
                      << std::endl;
#endif
          argv.erase(argv.begin() + i);
          advance = 0;
        }
      }
    } else { /* attached value */
      if (value && argd >= 0) {
        /* Delete argv[i] from list. */
#ifdef SLA_DEBUG
        if (debug >= 4)
          std::cout << "Deleting " << i << ':' << argv[i] << " from argv"
                    << std::endl;
#endif
        argv.erase(argv.begin() + i);
        advance = 0;
      }
    }

    nvalue++; /* Count number of value found. */
    i += advance;

#ifdef SLA_DEBUG
    if (debug >= 8)
      for (size_t j = 1; j < argv.size(); j++)
        std::cout << "remaining:argv[" << j << "]=" << argv[j] << std::endl;
#endif
  }

  regfree(&flind_re);
  regfree(&pname_re);
  regfree(&assop_re);
  regfree(&sup_assop_re);

  return nvalue;
}

int sla_none(std::vector<std::string> &argv, const std::string &flind,
             const std::string &assop, const std::string &pname, int argd) {
  return sla_str(argv, flind, assop, pname, static_cast<std::string *>(nullptr),
                 argd);
}

// clang-format off
SLA_FCN_BEGIN(sla_float, float) {
  SLA_CNV((float)atof)
}

SLA_FCN_BEGIN(sla_double, double) {
  SLA_CNV(atof)
}

SLA_FCN_BEGIN(sla_int, int) {
  SLA_CNV(atoi)
}

SLA_FCN_BEGIN(sla_short, short) {
  SLA_CNV((short)atoi)
}

SLA_FCN_BEGIN(sla_long, long) {
  SLA_CNV((long)atoi)
}

SLA_FCN_BEGIN(sla_uint, unsigned int) {
  SLA_CNV((unsigned int)atoi)
}

SLA_FCN_BEGIN(sla_ushort, unsigned short) {
  SLA_CNV((unsigned short)atoi)
}

SLA_FCN_BEGIN(sla_ulong, unsigned long) {
  SLA_CNV((unsigned long)atoi)
}

SLA_FCN_BEGIN(sla_char, char) {
  SLA_CNV(*)
}
