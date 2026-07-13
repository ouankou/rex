// REX pragma boundary prefix marker.

#pragma rex_global_marker

int rex_unparser_pragma_boundary_roundtrip(int value)
#pragma rex_function_body_marker
{
  int result = value;

#pragma rex_continued_marker first second
  result = result +
#pragma rex_expression_marker
           1;

  switch (result) {
#pragma rex_case_marker
  case 1:
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

  return result;
}
