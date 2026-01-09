// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_PROGRAM_TRANSFORMATION_H
#define ROSE_DOCS_MODULES_PROGRAM_TRANSFORMATION_H

/** @brief ROSE Program Transformations
 *
 * Location of numerous program transformations that operate on the AST and may be called within the midend.
 *
 * TODO: Add constant folding since we currently unfold all folded expressions in the code generation phase. This would be
 * something that could be verified as well since the legacy frontend has also computed the constant folded result and we have
 * stored that explicitly as well as the expression tree from which it was folded. The proposed constant folding would also
 * work on AST fragments constructed explicitly in ROSE from lower level mechanisms.
 *
 * TODO: The ROSE source code might be easier to organize if we have an include directory just for the included files from the
 * transformations. Then use separate directories for the implementations (we want to always separate the implementation if
 * possible from the header file and place it in a separate file). This would make it easier to add transformations and make
 * the maintenance of the development tree a bit easier. As it is each new directory forces a new include path to be specified
 * and a new library to be generated (in $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs).
 *
 * TODO: Document the Outliner (required for the generated documentation).
 *
 * See @ref rose_midend.
 */
struct ROSE_ProgramTransformationGroup {};

#endif
