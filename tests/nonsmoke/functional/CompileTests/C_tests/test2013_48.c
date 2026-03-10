
// const int *sample_fmts_alt = (const int[]) { 2,3 };
// int *sample_fmts_alt = (int[]) { 2,3 };
enum { x = 9 };

// Valid compound literal initializer for pointer-to-int.
int *sample_fmts_alt = (int[]){x, 3};
