// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_FRONTEND_SAGE_INTERFACE_H
#define ROSE_DOCS_MODULES_FRONTEND_SAGE_INTERFACE_H

/** @brief ROSE Frontend Group
 *
 * Frontend interfaces and utilities for building and analyzing ASTs.
 */
struct ROSE_FrontEndGroup {};

/** @brief High level AST builders
 *
 * High-level SAGE III AST node and subtree builders.
 *
 * Building AST trees using raw SgNode constructors is tedious and error-prone, especially with symbol tables. This interface
 * provides AST node builders on top of constructors to handle symbol tables, scope edges, and parent relationships.
 *
 * See @ref ROSE_FrontEndGroup.
 */
struct frontendSageHighLevelInterface {};

#endif
