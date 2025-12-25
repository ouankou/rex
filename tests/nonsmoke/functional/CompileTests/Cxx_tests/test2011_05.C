// This testcode demonstates the use of an expression that is normally hidden
// behind a constant and is accessble only in legacy frontend using
// RECORD_CONSTANT_EXPRESSIONS_IN_IL and in combination with recently completed
// work to support such source-to-source details (in legacy frontend for array
// types a constant expression is hidden is found in
// type->variant.array.bound_constant.

// This unparses as: "static double x[4UL];" which is not what we want.
static double x[sizeof(int)];

