#include <stdio.h>

/* extern "C" int openFortranParser_main(int argc, char **argv); */
int openFortranParser_main(int argc, char **argv);

int openFortranParser_main(int argc, char **argv)
  {
 // Fortran use in ROSE requires the OFP frontend and a Java runtime for OFP.
    fprintf (stderr, "ROSE is not configured with Fortran support; enable Fortran in CMake and ensure a Java runtime is available for OFP (compilation of input file is ignored)\n");

 // This is not considered to be an error, ROSE will return an error code of "0" to the operating system.
 // Even though this function will return an error code of "1" to the caller.
         return 1;
  }

int
experimental_openFortranParser_main(int argc, char **argv)
   {
  // This function does not need to call the JVM.
     int status = 0;

     printf ("ERROR: This function is defined in dummy_openFortranParser_main.C of the src/frontend/OpenFortranParser_SAGE_Connection directory (the experimental fortran frontend has not been enabled) \n");
     ROSE_ABORT();
   }
