#pragma omp parallel num_threads(2)
#pragma omp atomic write seq_cst
#pragma omp atomic compare acquire fail(seq_cst)
