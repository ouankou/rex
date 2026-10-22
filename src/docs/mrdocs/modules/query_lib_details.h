// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_QUERY_LIB_DETAILS_H
#define ROSE_DOCS_MODULES_QUERY_LIB_DETAILS_H

/** @brief Node Query Library
 *
 * This library simplifies the development of queries on the AST that return
 * lists of AST nodes (`SgNode*`).
 *
 * It represents a library of queries over subtrees and returns a list of AST
 * nodes.
 *
 * See @ref subtreeQueryLib.
 */
struct nodeQueryLib {};

/** @brief Name Query Library
 *
 * This library provides query helpers focused on name-based queries over AST
 * nodes.
 */
struct nameQueryLib {};

#endif
