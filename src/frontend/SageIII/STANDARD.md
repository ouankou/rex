ROSE Handling of C/C++/Fortran standard/dialect
===============================================

## Mapping of -std= option

ROSE recognizes the following standard specification. If more than one specification are possible for one standard, the first one in the list is provided to the backend compiler by ROSE.

### Standards

#### C89

* -std=c89

#### C90

 * -std=c90
 * -std=iso9899:1990
 * -std=iso9899:199409 (amended version of C90, WARNING)

#### C99

 * -std=c99
 * -std=c9x
 * -std=iso9899:1999
 * -std=iso9899:199x

#### C11

 * -std=c11
 * -std=c1x
 * -std=iso9899:2011

#### C17

 * -std=c17
 * -std=c18
 * -std=iso9899:2017
 * -std=iso9899:2018

#### Cxx98

 * -std=c++98

#### Cxx03

 * -std=c++03

#### Cxx11

 * -std=c++11
 * -std=c++0x

#### Cxx14

 * -std=c++14
 * -std=c++1y

#### Cxx17

 * -std=c++17
 * -std=c++1z

#### Cxx20

 * -std=c++20
 * -std=c++2a

#### Fortran (ROSE dialect selection)

ROSE uses a dedicated option for Fortran dialect selection; these are not
passed through as backend `-std` flags:

 * -rose:fortran_std=f77
 * -rose:fortran_std=f90
 * -rose:fortran_std=f95
 * -rose:fortran_std=f2003
 * -rose:fortran_std=f2008
 * -rose:fortran_std=f2018

#### Fortran backend `-std`

Flang accepts only the following backend `-std` value:

 * -std=f2018

### GNU

In addition, ROSE recognizes the GNU specific version for C/C++ standard (such as -std=gnu90).
If seen, ROSE stores that information, generating the correct flag for the backend compiler.

### ROSE additions

ROSE also recognize:
 * -std=c
 * -std=gnu
 * -std=c++
 * -std=gnu++

Legacy C compatibility mapping:
 * `-std=c`  -> GNU89 compatibility mode (`-std=gnu89`)
 * `-std=gnu` -> GNU89 compatibility mode (`-std=gnu89`)

#### Proposed?

Does it make sense to support language extension using the same technique:
 * -std=caf for CoArray Fortran ?
 * -std=opencl
 * -std=cuda
