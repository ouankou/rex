int check_omp_critical(int* logFile);
int check_omp_atomic(int* logFile);

typedef int (*a_ptr_to_test_function)(int* logFile);

typedef struct
   {
     char *name;
     a_ptr_to_test_function pass;
     a_ptr_to_test_function fail;
   } testcall;

static testcall alltests[] =
   {
     {"start", check_omp_critical, check_omp_atomic},
     {"end", 0, 0}
   };
