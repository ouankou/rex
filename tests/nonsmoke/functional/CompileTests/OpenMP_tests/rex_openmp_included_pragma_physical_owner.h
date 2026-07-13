#ifndef REX_OPENMP_INCLUDED_PRAGMA_PHYSICAL_OWNER_H
#define REX_OPENMP_INCLUDED_PRAGMA_PHYSICAL_OWNER_H

static inline void rex_openmp_included_pragma_physical_owner_run(int *value) {
#pragma omp parallel
  {
#pragma omp atomic update
    (*value)++;
  }
}

#endif
