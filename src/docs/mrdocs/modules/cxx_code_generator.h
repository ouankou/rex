// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_CXX_CODE_GENERATOR_H
#define ROSE_DOCS_MODULES_CXX_CODE_GENERATOR_H

/** @brief Backend Cxx Code Generator
 *
 * Generates C++ source code from the AST.
 *
 * Note: There are no user level functions within the backend. The only three related functions are available from the
 * SgNode interface (@ref SgNode::unparseToString) and the SgFile and SgProject interfaces (@ref SgFile::unparse and
 * @ref SgProject::unparse, respectively).
 *
 * See @ref rose_backend.
 */
struct CxxCodeGenerator {};

#endif
