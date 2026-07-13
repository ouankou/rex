void rex_omp_clause_copy_identity(int *shared) {
#pragma omp parallel firstprivate(shared)
  {
#pragma omp parallel firstprivate(shared)
    {
      *shared += 1;
    }

    {
      int *shared = 0;
#pragma omp parallel firstprivate(shared)
      {
        if (shared)
          *shared += 2;
      }
    }
  }
}
