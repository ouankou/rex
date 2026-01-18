#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#ifndef NcCheckFunction
#define NcCheckFunction

namespace netCDF {
/*!
  Function checks error code and if necessary throws an exception.
  \param retCode Integer value returned by %netCDF C-routines.
  \param file    The name of the file from which this call originates.
  \param line    The line number in the file from which this call originates.
*/
void ncCheck(int retCode, char *file, int line);

}; // namespace netCDF

#endif
