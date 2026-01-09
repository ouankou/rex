// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_QUERY_LIB_H
#define ROSE_DOCS_MODULES_QUERY_LIB_H

/** @brief Query Library
 *
 * This library simplifies the development of useful queries upon the AST.
 *
 * The design of the Query Library is somewhat ad hoc; it highlights the importance of query abstractions for simplifying
 * many preprocessing operations, but leaves room for a cleaner design in the future.
 *
 * Metadata:
 * - Authors: Quinlan (Last checked in by $Author: dquinlan $)
 * - Version: $Id: QueryLib.docs,v 1.1 2004/07/07 10:26:28 dquinlan Exp $
 * - Date: Date last worked on $Date: 2004/07/07 10:26:28 $
 * - Bug: No known bugs.
 * - Warning: Documentation is incomplete.
 *
 * TODO:
 * - Finish documentation.
 * - Make the source code directory structure for Query lib match the documentation.
 * - Make the library more extensible (perhaps designed like the PDF and DOT libraries).
 *
 * Note: Large parts of documentation contained in ROSE/QueryLibs/QueryLib.docs.
 *
 * Note: This is text about the QueryLib file itself, not sure if we need it.
 */
struct queryLib {};

/** @brief Local Node Query Library
 *
 * This library simplifies the development of useful queries upon individual AST nodes.
 *
 * It provides reusable queries that are specific to certain node types and can later migrate into Sage III directly.
 *
 * Bug: No known bugs.
 * Warning: Documentation is incomplete.
 * TODO: Finish documentation.
 *
 * Note: Large parts of documentation contained in ROSE/QueryLibs/QueryLib.docs.
 *
 * See @ref queryLib.
 */
struct localNodeQueryLib {};

/** @brief Subtree Query Library
 *
 * This library simplifies the development of useful queries that require traversal of an AST subtree.
 *
 * It represents a family of queries that use AST processing and operate upon whole subtrees. Return values are most commonly
 * lists.
 *
 * Bug: Not finished yet.
 * Warning: Documentation is incomplete.
 * TODO: Finish documentation.
 *
 * Note: Large parts of documentation contained in ROSE/QueryLibs/QueryLib.docs.
 *
 * Internal: None of these functions should be expected to move into Sage III without careful consideration.
 *
 * See @ref queryLib.
 */
struct subtreeQueryLib {};

#endif
