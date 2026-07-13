void rex_openacc_parse_failure(void) {
#pragma acc parallel unknown_clause
  {
  }
}
