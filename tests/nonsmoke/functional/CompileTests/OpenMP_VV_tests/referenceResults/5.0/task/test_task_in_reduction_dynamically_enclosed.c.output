#pragma omp task in_reduction(+:sum)
#pragma omp taskwait
#pragma omp task in_reduction(+:sum)
#pragma omp taskwait
#pragma omp taskgroup task_reduction(+:sum)
#pragma omp taskloop reduction(+:sum)
