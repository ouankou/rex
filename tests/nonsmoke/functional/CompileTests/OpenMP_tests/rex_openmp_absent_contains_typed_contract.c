#pragma omp assumes absent(parallel, for, do, simd, target, teams, distribute, \
                               task, taskloop, sections, section, single,      \
                               master, masked, critical, barrier, taskwait,    \
                               taskgroup, atomic, flush, ordered, scan, scope, \
                               loop, workshare, cancel, metadirective)

#pragma omp assumes contains(metadirective, cancel, workshare, loop, scope,    \
                                 scan, ordered, flush, atomic, taskgroup,      \
                                 taskwait, barrier, critical, masked, master,  \
                                 single, section, sections, taskloop, task,    \
                                 distribute, teams, target, simd, do, for,     \
                                 parallel)

void rex_openmp_absent_contains_typed_contract(void) {}
