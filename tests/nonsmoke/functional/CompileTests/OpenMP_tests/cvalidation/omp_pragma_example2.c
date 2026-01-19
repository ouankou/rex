
int i,m;
void
foo ()
{

//#pragma omp for
for (i = 0; i < 10; i++)
   m++;

// If we uncomment this statement then we get proper blocking
// m++;
 
#pragma omp for
for (i = 1; i < 10; i++)
  m++;

}
