// -*- c++ -*-

#ifndef ROSE_DOCS_ROSE_HEAP_OBJECT_OWNERSHIP_H
#define ROSE_DOCS_ROSE_HEAP_OBJECT_OWNERSHIP_H

/** @brief Ownership of AST nodes and heap-allocated payloads
 *
 * REX distinguishes physical ownership from semantic references. A declaration
 * can reference a canonical type or symbol without owning its lifetime. Parent
 * links and declaration/type cycles therefore do not imply shared C++
 * ownership.
 *
 * ROSETTA member metadata declares deletion and copying policy. DEF_DELETE
 * members own their payloads. CLONE_PTR creates an independent ordinary C++
 * payload, while CLONE_TREE copies AST storage through the copy transaction.
 * COPY_DATA preserves a value or reference; it must not alias exclusive owned
 * storage between independently destructible objects. Shared canonical types
 * must retain their original declaration identity when a client copies an AST.
 *
 * AST nodes use memory pools. Deleting a node releases its declared owned
 * members; it does not establish that every node reachable through semantic
 * references can also be deleted. Subtree removal must account for symbols and
 * references from outside the subtree. SageInterface::deleteAST provides an
 * explicit DeleteAstMode for that boundary; kRequireIsolated rejects external
 * references. SageInterface::tearDownAst releases AST storage and global caches
 * and invalidates AST pointers. Do not use surviving pointers after teardown.
 *
 * Ordinary heap payloads should have an explicit owner, commonly represented
 * by std::unique_ptr. Use std::shared_ptr only for actual shared lifetime, with
 * std::weak_ptr for non-owning links that would otherwise create ownership
 * cycles. Multiple shared_ptr control blocks must never own the same raw
 * pointer. AST graph cycles are normal; ownership cycles require a separate
 * lifetime design.
 *
 * AstAttribute payloads follow their declared ownership policy. User analysis
 * attributes do not replace typed AST fields for compiler-required semantics.
 */
struct heap_object_ownership {};

#endif
