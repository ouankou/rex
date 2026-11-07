#define REX_SCOPE_THREADS 1
#define REX_SCOPE_OPAQUE_PAYLOAD rex_selected_opaque("space payload")
!$ompx REX_SCOPE_OPAQUE_PAYLOAD
!dir$ fixed
!$omp parallel num_threads(REX_SCOPE_THREADS) shared(total)
!$omp atomic update
      total = total + 1
