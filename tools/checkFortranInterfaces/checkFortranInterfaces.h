#ifndef CHECK_FORTRAN_INTERFACES_H_
#define CHECK_FORTRAN_INTERFACES_H_

class SgProject;

// Return 0 only when every externally bound Fortran interface is verified,
// 1 for an incompatible or missing C interface, and 2 for unsupported ABI data.
int checkFortranInterfaces(SgProject *project);

#endif
