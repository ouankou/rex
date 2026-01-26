// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_LANGUAGE_INDEPENDENT_CODE_GENERATOR_H
#define ROSE_DOCS_MODULES_LANGUAGE_INDEPENDENT_CODE_GENERATOR_H

/** @brief Backend Language Independent Code Generator
 *
 * Generates calls to the appropriate language to build source code from the
 * AST.
 *
 * Note: There are no user level functions within the backend. Related entry
 * points include `SgNode::unparseToString`, @ref unparseFile, and
 * @ref unparseProject.
 *
 * See @ref rose_backend.
 */
struct LanguageIndependentCodeGenerator {};

#endif
