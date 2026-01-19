



void foo (void)
   {
     int i;
     double sum=0.0;

  // for(i=1;i<=10;i++) sum++;
  // while (i <= 10) { sum++; }
  // { int x; }
     while (i <= 10) { sum++; }

  // a statement between the for loop and pragma will help
  // sum++;

#pragma omp single
     sum++;
   }

