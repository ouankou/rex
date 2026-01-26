// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_FORTRAN_CODE_GENERATOR_H
#define ROSE_DOCS_MODULES_FORTRAN_CODE_GENERATOR_H

/** @brief Backend Fortran Code Generator
 *
 * Generates Fortran source code from the AST using a design similar to the C
 * and C++ code generator.
 *
 * Note: There are no user level functions within the backend. Related entry
 * points include `SgNode::unparseToString`, @ref unparseFile, and
 * @ref unparseProject.
 *
 * Note: This code was developed in collaboration with Rice University (initial
 * pieces were developed by Nathan Tallent and Gina Goff).
 *
 * See @ref rose_backend.
 */
struct FortranCodeGenerator {};

#endif
