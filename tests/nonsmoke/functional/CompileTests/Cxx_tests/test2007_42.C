// Test source positions for standard opaque enum declarations. Use an explicit
// underlying type so the specimen remains valid in modern C++ modes.

// This test code demonstrates the use of forward enum declarations and is
// designed so that an error in source position handling will cause a legitimate
// error in the final compilation of generated code.
enum numbers : int;
#include "test2007_42.h"

enum numbers : int;

// This is the definition;
enum numbers : int {};

// It appears that this redundant enum declaration is present in legacy
// frontend, built as an IR node in the translation from legacy frontend, but
// not output in the unparsed code in ROSE. This is not a crisis, since it is
// redundant and meaningless (as best I can tell).
enum numbers : int;
