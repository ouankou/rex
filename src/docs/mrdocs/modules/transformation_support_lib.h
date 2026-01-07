// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_TRANSFORMATION_SUPPORT_LIB_H
#define ROSE_DOCS_MODULES_TRANSFORMATION_SUPPORT_LIB_H

/** @brief Transformation Support Library
 *
 * General support for transformations.
 *
 * This library holds functions that are generally useful for a broad cross-section of transformations. Support includes:
 * - Recognition of C++ overloaded operators.
 * - Recognition of general transformation hints within applications.
 * - Construction of macros for placement into the AST.
 * - Extraction of names from AST nodes (or subtrees): function names, type names, etc.
 *
 * Authors: Quinlan.
 *
 * Internal: Functions that get name strings from AST nodes should be placed into Sage III within the appropriate AST node
 * interfaces.
 *
 * Internal: Consider whether this should be a subgroup of the AST rewrite mechanism.
 *
 * TODO: Fix up interfaces in Sage III using code from the TransformationSupport class.
 *
 * See @ref localNodeQueryLib.
 */
struct transformationSupportLib {};

#endif
