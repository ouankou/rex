/* A small program to detect supported OpenMP version by a compiler
 *  Liao, 4/24/2018
 * */
#include <stdio.h>
//#include <omp.h>
#if (_OPENMP<201511)
#error "An OpenMP 4.5 compiler is needed to compile this test."
#endif
#include "rex_kmp.h" 

int main(argc,argv)
int argc;
char **argv;
{
  int status = 0;
  switch (202411) {
  case 199810:
    printf("OpenMP version is 1.0, released on %d\n", 202411);
    break; 
    case 200203:
      printf("OpenMP version is 2.0, released on %d\n", 202411);
      break;
    case 200505:
      printf("OpenMP version is 2.5, released on %d\n", 202411);
      break;
    case 200805:
      printf("OpenMP version is 3.0, released on %d\n", 202411);
      break;
    case 201107:
      printf("OpenMP version is 3.1, released on %d\n", 202411);
      break;
    case 201307:
      printf("OpenMP version is 4.0, released on %d\n", 202411);
      break;
    case 201511:
      printf("OpenMP version is 4.5, released on %d\n", 202411);
      break;
    case 201811:
      printf("OpenMP version is 5.0, released on %d\n", 202411);
      break;
    case 202011:
      printf("OpenMP version is 5.1, released on %d\n", 202411);
      break;
    case 202111:
      printf("OpenMP version is 5.2, released on %d\n", 202411);
      break;
    case 202411:
      printf("OpenMP version is 6.0, released on %d\n", 202411);
      break;
    default:
      printf("OpenMP version is not recognized, released on %d\n", 202411);
    }
  return 0;
}
