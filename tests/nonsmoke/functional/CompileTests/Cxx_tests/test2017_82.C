
//-------------- input code -----------------------------
 // cat define-directive.c
void foo(int a[10][10])
{
  int i;
  int j;

#define BIG 1
  int k = BIG ;
#define MY_SMP_PRIVATE i,j
#pragma omp parallel private (MY_SMP_PRIVATE)
#pragma omp for
  for (j =0; j< 10; j++)
    for (i =0; i< 10; i++)
      a[i][j] = 0;
}
