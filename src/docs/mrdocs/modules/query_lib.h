// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_QUERY_LIB_H
#define ROSE_DOCS_MODULES_QUERY_LIB_H

/** @brief Query Library
 *
 * This library simplifies the development of useful queries upon the AST.
 *
 */
struct queryLib {};

/** @brief Local Node Query Library
 *
 * This library simplifies the development of useful queries upon individual AST
 * nodes.
 *
 * It provides reusable queries that are specific to certain node types.
 *
 * See @ref queryLib.
 */
struct localNodeQueryLib {};

/** @brief Subtree Query Library
 *
 * This library simplifies the development of useful queries that require
 * traversal of an AST subtree.
 *
 * It represents a family of queries that use AST processing and operate upon
 * whole subtrees. Return values are most commonly lists.
 *
 * See @ref queryLib.
 */
struct subtreeQueryLib {};

#endif
