#include "test2016_01.h"
// #include <stdio.h>

#define TEST_VEC_LEN 10

/* module load cudatoolkit/7.5 */
/* use gcc-4.9.3p */
/* nvcc -O2 --expt-extended-lambda -arch compute_35 -std=c++11 main.cu */

int foobar(int i);

int main(int argc, char *argv[])
{
   int *value ;

   cudaMallocManaged((void **)&value, sizeof(int) * TEST_VEC_LEN,
                     cudaMemAttachGlobal);
   // This is compilable by ROSE (but not relevant).
   forall(cuda_traversal(), TEST_VEC_LEN,
          [=](int i) __attribute__((my_device)) { value[i] = i; });

   cudaDeviceSynchronize() ;

   return 0 ;
}

