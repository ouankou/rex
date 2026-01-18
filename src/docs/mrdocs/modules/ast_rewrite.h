// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_AST_REWRITE_H
#define ROSE_DOCS_MODULES_AST_REWRITE_H

/** @brief AST Rewrite Mechanism
 *
 * The AST rewrite mechanism permits editing an application's AST, including
 * addition, deletion, or replacement of subtrees. Using the rewrite system as
 * part of a traversal is a common way to build a preprocessor. A transformation
 * is implemented by applying a specific change to the AST and then unparsing
 * the result.
 *
 * The primary user-level entry point is `AST_Rewrite::addSourceCodeString`,
 * which uses enum values defined in `AST_Rewrite` to specify insertion
 * locations.
 *
 * See @ref rose_midend.
 */
struct RewriteMechanism {};

/** @brief AST Rewrite nested classes
 *
 * Nested classes supporting the AST rewrite mechanism, including traversal and
 * attribute helpers.
 *
 * See @ref RewriteMechanism.
 */
struct RewriteNestedClasses {};

/** @brief AST Rewrite fragment identification traversal
 *
 * Nested classes required for the traversal that separates AST fragments built
 * from transformation strings.
 *
 * See @ref RewriteNestedClasses.
 */
struct RewriteFragmentIdentificationTraversal {};

#endif
