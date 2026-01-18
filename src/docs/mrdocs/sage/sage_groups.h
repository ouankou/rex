// -*- c++ -*-

#ifndef ROSE_DOCS_SAGE_GROUPS_H
#define ROSE_DOCS_SAGE_GROUPS_H

/** @brief Sage III Intermediate Representation
 *
 * Abstract Syntax Tree (AST) intermediate representation (IR).
 *
 * These classes were reconstructed to be consistent with the Sage II
 * implementation. With Dennis Gannon's permission, this work is called Sage
 * III.
 *
 * See @ref rose_frontend.
 */
struct SageClasses {};

/** @brief Sage III Type Classes
 *
 * Collection of classes representing all types in the C++ grammar.
 *
 * See @ref SageClasses.
 */
struct SageType {};

/** @brief Sage III Statement Classes
 *
 * Collection of classes representing all statements in the C++ grammar.
 *
 * See @ref SageClasses.
 */
struct SageStatement {};

/** @brief Sage III Expression Classes
 *
 * Collection of classes representing all expressions in the C++ grammar.
 *
 * See @ref SageClasses.
 */
struct SageExpression {};

/** @brief Sage III Symbol Classes
 *
 * Collection of classes representing all symbols in the C++ grammar.
 *
 * See @ref SageClasses.
 */
struct SageSymbol {};

#endif
