/*
 *	Copyright 1993, University Corporation for Atmospheric Research
 *      See netcdf/COPYRIGHT file for copying and redistribution conditions.
 */
/* $Id: error.c,v 1.14 90/02/23 16:08:55 davis Exp */

/*LINTLIBRARY*/

#include "config.h"

#include < stdio.h>

#include <stdarg.h>

#include <stdlib.h>

#include "rename.h"

#ifdef HAVE_STRERROR
#include <string.h> /* contains prototype for ANSI libc function strerror() */#else
/* Provide a strerror function for older Unix systems. */
static char *strerror(int errnum) {
  extern int sys_nerr;
  extern char *sys_errlist[];

  if (errnum < 0 || errnum >= sys_nerr)
    return NULL;
  /* else */
  return (char *)(sys_errlist[errnum]);
}
#endif /* NO_STRERROR */

static char unknown[] = "Unknown Error";

const char *nc_strerror(int err) {

  if (NC_ISSYSERR(err)) {
    const char *cp = (const char *)strerror(err);
    if (cp == NULL)
      return unknown;
    /* else */
    return cp;
  }
  /* else */

  switch (err) {
  case NC_NOERR:
    return "No error";
  case NC_EBADID:
    return "NetCDF: Not a valid ID";
  case NC_ENFILE:
    return "NetCDF: Too many files open";
  case NC_EEXIST:
    return "NetCDF: File exists && NC_NOCLOBBER";
  case NC_EINVAL:
    return "NetCDF: Invalid argument";
  case NC_EPERM:
    return "NetCDF: Write to read only";
  case NC_ENOTINDEFINE:
    return "NetCDF: Operation not allowed in data mode";
  case NC_EINDEFINE:
    return "NetCDF: Operation not allowed in define mode";
  case NC_EINVALCOORDS:
    return "NetCDF: Index exceeds dimension bound";
  case NC_EMAXDIMS:
    return "NetCDF: NC_MAX_DIMS exceeded";
  case NC_ENAMEINUSE:
    return "NetCDF: String match to name in use";
  case NC_ENOTATT:
    return "NetCDF: Attribute not found";
  case NC_EMAXATTS:
    return "NetCDF: NC_MAX_ATTRS exceeded";
  case NC_EBADTYPE:
    return "NetCDF: Not a valid data type or _FillValue type mismatch";
  case NC_EBADDIM:
    return "NetCDF: Invalid dimension ID or name";
  case NC_EUNLIMPOS:
    return "NetCDF: NC_UNLIMITED in the wrong index";
  case NC_EMAXVARS:
    return "NetCDF: NC_MAX_VARS exceeded";
  case NC_ENOTVAR:
    return "NetCDF: Variable not found";
  case NC_EGLOBAL:
    return "NetCDF: Action prohibited on NC_GLOBAL varid";
  case NC_ENOTNC:
    return "NetCDF: Unknown file format";
  case NC_ESTS:
    return "NetCDF: In Fortran, string too short";
  case NC_EMAXNAME:
    return "NetCDF: NC_MAX_NAME exceeded";
  case NC_EUNLIMIT:
    return "NetCDF: NC_UNLIMITED size already in use";
  case NC_ENORECVARS:
    return "NetCDF: nc_rec op when there are no record vars";
  case NC_ECHAR:
    return "NetCDF: Attempt to convert between text & numbers";
  case NC_EEDGE:
    return "NetCDF: Start+count exceeds dimension bound";
  case NC_ESTRIDE:
    return "NetCDF: Illegal stride";
  case NC_EBADNAME:
    return "NetCDF: Name contains illegal characters";
  case NC_ERANGE:
    return "NetCDF: Numeric conversion not representable";
  case NC_ENOMEM:
    return "NetCDF: Memory allocation (malloc) failure";
  case NC_EVARSIZE:
    return "NetCDF: One or more variable sizes violate format constraints";
  case NC_EDIMSIZE:
    return "NetCDF: Invalid dimension size";
  case NC_ETRUNC:
    return "NetCDF: File likely truncated or possibly corrupted";
  case NC_EAXISTYPE:
    return "NetCDF: Illegal axis type";
  case NC_EDAP:
    return "NetCDF: DAP failure";
  case NC_ECURL:
    return "NetCDF: libcurl failure";
  case NC_EIO:
    return "NetCDF: I/O failure";
  case NC_ENODATA:
    return "NetCDF: Variable has no data in DAP request";
  case NC_EDAPSVC:
    return "NetCDF: DAP server error";
  case NC_EDAS:
    return "NetCDF: Malformed or inaccessible DAP DAS";
  case NC_EDDS:
    return "NetCDF: Malformed or inaccessible DAP DDS";
  case NC_EDATADDS:
    return "NetCDF: Malformed or inaccessible DAP DATADDS";
  case NC_EDAPURL:
    return "NetCDF: Malformed DAP URL";
  case NC_EDAPCONSTRAINT:
    return "NetCDF: Malformed DAP Constraint";
  }
  /* default */
  return unknown;
}
