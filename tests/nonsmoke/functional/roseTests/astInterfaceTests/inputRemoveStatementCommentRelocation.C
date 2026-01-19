// DQ (9/16/2010): Added this test code to demonstrate problem with removing
// statments with comments or CPP directives attached.

// This header file declares a variable that will be used by the inserted code.
// By design, if the function it is attached to removes the #include then the
// variable declaration below it will fail to compile and will cause an error.
#include "inputRemoveStatementCommentRelocation_1.h"
// Use this function to test removal, if it cause removal of the #include then
// there will be an error (since variable_hidden_in_header_file will not be
// defined).
int removeThisFunctionToTestAttachedInfoBeforeStatement() { return 0; }
