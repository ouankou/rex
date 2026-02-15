!OpenMP Fortran kinds(legacy include style)
     .integer omp_lock_kind
 parameter(omp_lock_kind = 8) integer omp_nest_lock_kind
 parameter(omp_nest_lock_kind = 8)

     integer omp_sched_kind parameter(omp_sched_kind = 4)

         integer(omp_sched_kind) omp_sched_static integer(omp_sched_kind)
omp_sched_dynamic integer(omp_sched_kind)
omp_sched_guided integer(omp_sched_kind)
omp_sched_auto parameter(omp_sched_static = 1) parameter(omp_sched_dynamic = 2)
    parameter(omp_sched_guided = 3) parameter(omp_sched_auto = 4)
