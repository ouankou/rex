/* This test uses a standard opaque enum declaration so modern Clang-based
   frontends can represent the typedef target without relying on a legacy
   extension.
*/

/*
when compiling the following code:
enum _cairo_clip_mode : int;
typedef _cairo_clip_mode cairo_clip_mode_t;

I get the following error:
lt-identityTranslator:
/home/andreas/REPOSITORY-SRC/ROSE/June-23a-Unsafe/NEW_ROSE/src/frontend/legacy_frontend/sage_gen_be.C:3983:
SgDeclarationStatement* sage_gen_tag_reference(a_type*, a_boolean,
SgDeclarationStatement*&,
    DataRequiredForComputationOfSourcePostionInformation*): Assertion
    theSymbol != __null failed.
*/

enum _cairo_clip_mode : int;
typedef _cairo_clip_mode cairo_clip_mode_t;
