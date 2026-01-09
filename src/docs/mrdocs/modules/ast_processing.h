// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_AST_PROCESSING_H
#define ROSE_DOCS_MODULES_AST_PROCESSING_H

/** @brief AST Processing
 *
 * Mechanism for traversing the AST and computing attributes.
 *
 * This AST processing mechanism allows traversing the AST and computing inherited and synthesized attributes. The main
 * interfaces are:
 * - SimpleProcessing: pre- and postorder traversal with a single visit function.
 * - TopDownProcessing: computes inherited attributes using a preorder traversal.
 * - BottomUpProcessing: computes synthesized attributes using a postorder traversal.
 * - BottomUpTopDownProcessing: computes inherited and synthesized attributes.
 *
 * The classes have pure virtual functions; users should inherit and implement the required methods.
 *
 * Authors: Markus Schordan (preliminary versions developed by Quinlan and Kowarschik using a different algorithm for
 * combining attributes and a single interface).
 *
 * See @ref rose_midend.
 */
struct AstProcessingClasses {};

#endif
