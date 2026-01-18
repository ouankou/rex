#ifndef _ROSE_SPECIFIC_COMPLEX_H
#define _ROSE_SPECIFIC_COMPLEX_H 1

/*
Pei-Hung (03/08/2021) This file is no longer needed as legacy frontend can
handle the complex type. Some environments no longer expose /usr/include by
default.
*/

/* Note that /usr/include/complex.h defines _Complex_I as (__extension__ 1.0iF)
   and legacy frontend can not handle the "iF" literal suffix and reports an
   error. This ROSE specific solution allows us to define _Complex_I after it is
   set by /usr/include/complex.h and use a value that is legacy frontend
   specific.
 */

#include </usr/include/complex.h>
/* redefine _Complex_I to be what legacy frontend defines as __I__ */
#undef _Complex_I
#define _Complex_I __I__

/* endif for _ROSE_SPECIFIC_SYS_CDEFS_H */
#endif
