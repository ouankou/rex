// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_BACKEND_GENERATOR_H
#define ROSE_DOCS_MODULES_BACKEND_GENERATOR_H

/** @brief Backend C and C++ Code Generator
 *
 * Generates C or C++ source code from the AST.
 *
 * Generates C++ source code directly from the AST (older style code that predates the newer traversal mechanisms). Parts of
 * this code were borrowed from the original Sage II unparser. Internally, several options are available so that the generated
 * code can either reference the user's original source code or the generated code. The generated output can use `#line`
 * directives to optionally reference the original user's file, which helps debuggers map generated code back to the original
 * source.
 *
 * The primary user-facing entry point is @ref SgProject::unparse, which generates source code associated with the internal AST.
 * If the AST was transformed, those transformations appear as differences between generated code and the original input.
 *
 * Additional related functions are available from the SgNode interface (@ref SgNode::unparseToString) and the SgFile interface
 * (@ref SgFile::unparse).
 *
 * See @ref rose_backend.
 */
struct backendGenerator {};

#endif
