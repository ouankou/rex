#ifndef ROSE_checkIsCompilerGeneratedFlag_H
#define ROSE_checkIsCompilerGeneratedFlag_H

/** Checks whether appropriate nodes of an AST are marked as compiler-generated.
 *
 *  Nodes originating from `rose_required_macros_and_functions.h` are
 * frontend-specific. This function marks frontend-specific nodes as
 * compiler-generated as well. */
size_t checkIsCompilerGeneratedFlag(SgNode *ast);

#endif
