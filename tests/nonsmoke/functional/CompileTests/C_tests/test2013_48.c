
// const int *sample_fmts_alt = (const int[]) { 2,3 };
// int *sample_fmts_alt = (int[]) { 2,3 };
const int x = 9;

// This is not a compound literal (it lacks a type specification) and includes a
// non-literal value.
int *sample_fmts_alt = {x, 3};
