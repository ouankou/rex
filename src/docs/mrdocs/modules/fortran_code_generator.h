// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_FORTRAN_CODE_GENERATOR_H
#define ROSE_DOCS_MODULES_FORTRAN_CODE_GENERATOR_H

/** @brief Backend Fortran Code Generator
 *
 * Generates Fortran source code from the AST using a design similar to the C
 * and C++ code generator.
 *
 * Note: There are no user level functions within the backend. The only three
 * related functions are available from the SgNode interface (@ref
 * SgNode::unparseToString) and the SgFile and SgProject interfaces (@ref
 * SgFile::unparse and
 * @ref SgProject::unparse, respectively).
 *
 * Note: This code was developed in collaboration with Rice University (initial
 * pieces were developed by Nathan Tallent and Gina Goff).
 *
 * See @ref rose_backend.
 */
struct FortranCodeGenerator {};

#endif
