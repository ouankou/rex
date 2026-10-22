# Fortran interface checker

`checkFortranInterfaces declarations.c procedures.f90` compares each externally
bound Fortran procedure with the C declarations bearing its semantic binding
name. C++ declarations participate when they have C language linkage. Internal
procedures and explicit empty BIND(C) names have no external binding to compare.

The checker consumes target layouts and argument conventions published by the
frontends. KIND aliases and equivalent constant expressions therefore give the
same result. It checks result types, argument counts, scalar representation,
and value/reference passing. Explicit and assumed-size contiguous arrays pass
their element address; C parameter bounds are not part of the ABI comparison.
Integer representation compatibility does not prove equal signed value ranges
or application-level array bounds.

Exit status is part of the interface:

- `0`: every external Fortran binding was verified against its C declarations.
- `1`: a required C declaration is missing or an interface is incompatible.
- `2`: no external Fortran binding was found, or an interface cannot be verified.

Record layouts, descriptor arguments, and procedure arguments are currently
unverified and produce an explicit diagnostic with status 2. They are never
reported as compatible. The C/C++ frontend supplies its selected target ABI;
the current Flang driver analyzes the native target. Differing target byte order
or pointer layout makes a pair incompatible.
