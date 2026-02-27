#pragma omp atomic
#pragma omp task shared(i) threadset(omp_pool)
#pragma omp task shared(j) threadset(omp_pool)
#pragma omp taskwait
#pragma omp task shared(i) threadset(omp_team)
#pragma omp task shared(j) threadset(omp_team)
#pragma omp taskwait
#pragma omp parallel
#pragma omp single
