// REX pragma boundary prefix marker.

#pragma rex_global_marker
#pragma ompx(
#pragma accx)
#pragma omp opaque(
#pragma acc opaque)

int rex_unparser_pragma_boundary_roundtrip(int value)
#pragma rex_function_body_marker
#pragma omp parallel
#pragma acc parallel
{
  int result =
#pragma rex_initializer_marker
      value
#pragma rex_initializer_end_marker
      ;

#pragma rex_continued_marker first second
  result = result
#pragma rex_expression_after_lhs_marker
           +
#pragma rex_expression_marker
           1;

  switch (
#pragma rex_selector_marker
      result
#pragma rex_selector_end_marker
  ) {
#pragma rex_case_marker
  case
#pragma rex_case_value_marker
      1:
#pragma rex_case_body_marker
    result += 2;
    break;

  case 2:
#pragma rex_braced_case_marker
  {
    result += 3;
    break;
  }

  default:
#pragma rex_default_block_marker
  {
    result += 4;
  }
  }

  return
#pragma rex_return_marker
      result;
}
