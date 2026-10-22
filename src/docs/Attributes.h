// -*- c++ -*-

#ifndef ROSE_DOCS_ATTRIBUTES_H
#define ROSE_DOCS_ATTRIBUTES_H

/** @brief User analysis attributes
 *
 *  AST nodes that support attributes expose the SgNode attribute API, backed by
 *  AstAttributeMechanism. Each container maps a user-chosen name to an
 *  AstAttribute subclass. These attributes can store analysis results between
 *  traversals. Traversal inherited and synthesized attributes have separate
 *  lifetimes governed by their traversal classes.
 *
 *  Compiler-required semantics belong in typed AST fields and node identities.
 *  An analysis attribute must not substitute for a missing declaration, type,
 *  scope, binding, or other required frontend information.
 *
 *  @section attribute_storage Storage and lookup
 *
 *  Derive an analysis value from AstAttribute, or wrap a value in such a
 *  subclass. Use a name specific to the analysis: names are unique only within
 *  one container, and the API does not prevent unrelated analyses from choosing
 *  the same name. getAttribute returns an AstAttribute pointer; the caller must
 *  check that its dynamic type is the expected analysis value. A missing name
 *  returns a null pointer on nodes that support the attribute interface.
 *
 *  AstAttributeMechanism::set inserts or replaces a value; a null value removes
 *  the entry. add inserts only when the name is absent, and replace requires an
 *  existing name. Both return whether their operation succeeded. The container
 *  disposes of rejected and replaced values according to their ownership
 * policy. getAttributeIdentifiers returns the names currently stored in the
 * container.
 *
 *  @section attribute_lifetime Ownership and copying
 *
 *  An attribute subclass must state its ownership policy. With
 *  AstAttribute::CONTAINER_OWNERSHIP, the container deletes the attribute when
 *  it is replaced, removed, or the container is destroyed. With
 *  AstAttribute::CUSTOM_OWNERSHIP, the attribute's designer manages its
 * lifetime; it must remain alive until removed from every container that refers
 * to it. The legacy NO_OWNERSHIP and UNKNOWN_OWNERSHIP policies do not reclaim
 * storage and report possible leaks. They are unsuitable for new analysis
 * values.
 *
 *  Copying a container invokes each value's virtual copy method. A value that
 *  returns null is omitted from the copy and produces a warning. A
 *  CONTAINER_OWNERSHIP value must return an independent allocation. An
 * attribute that deliberately shares storage must implement a matching
 * ownership policy. Implement attribute_class_name to identify the subclass in
 * diagnostics.
 *
 *  See AstAttribute::getOwnershipPolicy for the complete lifetime contract and
 *  @ref rose_midend for analysis interfaces.
 */
struct attributes {};

#endif
