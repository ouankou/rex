
#ifndef ROSE_AST_H
#define ROSE_AST_H

/*************************************************************
 * Author   : Markus Schordan                                *
 *************************************************************/

#include "roseInternal.h"

#include <cstddef>

#include <stack>

class SgNode;

/** Interface for iterating over an AST.
 *
 *  The iterator follows the STL iterator pattern and is implemented as a
 * depth-first, pre-order traversal that maintains its own stack. The iterator
 * performs the exact same traversal as the traversal classes in ROSE (it is
 * using the same underlying information).
 *
 * @code
 *  #include <RoseAst.h>
 *  SgNode *node = .... // any subtree
 *
 *  RoseAst ast(node);
 *  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i)
 *      std::cout <<"We are here: " <<(*i)->class_name() <<"\n";
 * @endcode */
class RoseAst {
public:
  typedef SgNode elementType;
  typedef elementType *pointer;
  typedef elementType &reference;
  typedef size_t size_type;

  /** Defines the starting node for traversal.
   *
   *  The traversal is over a subtree rooted at the specified `root` node. All
   * operations are specific to this subtree, and the traversal does not iterate
   * above this `root.` */
  RoseAst(SgNode *astNode);

  /** AST iterator.
   *
   *  This is an STL-compliant forward iterator for traversing AST nodes in
   * depth-first pre-order. The @ref withNullValues function includes null
   * child pointers, which are skipped by default. Entire subtrees can be
   * excluded from traversal with the @ref skipChildrenOnForward function.
   *
   *  The iterator also provides a number of auxiliary functions for querying
   * some structural properties of the AST w.r.t. the position in the traversal
   * (e.g., @ref parent and @ref stack_size).
   *
   *  @note Comparison of iterators is also correct for null values. Only if two
   * iterators refer to the same (identical) null value, they are equal,
   * otherwise they are not. If they refer to different null values they are
   * different. Hence, different null values in the AST are treated like
   * different nodes. This is necessary to allow STL algorithms to work properly
   * on the AST. */
  class iterator {
  public:
    /** Default constructor.
     *
     *  Constructs an iterator that compares equal to the @ref end iterator. */
    iterator();

    /** Construct an iterator pointing to a particular AST node. */
    iterator(SgNode *x, bool withNullValues);

    /** Test iterator equality.
     *
     *  Two iterators are equal if they point to the same node or they're both
     * @ref end iterators. Two iterators that are pointing to null nodes are
     * equal if and only if they refer to the same (identical) null value. If
     * they refer to different null values they are different. Hence, different
     * null values in the AST are treated like different nodes. This is
     * necessary to allow STL algorithms to work.
     *
     *  Additionally, even if two iterators are pointing to the same AST node
     * they will be equal only if they're in the same
     *  @ref skipChildrenOnForward state. */
    bool operator==(const iterator &x) const;

    /** Test iterator inequality. */
    bool operator!=(const iterator &x) const;

    /** Dereference an iterator.
     *
     *  Dereference an iterator to obtain the AST node to which it points.
     * Dereferencing an end iterator will throw an `std::out_of_range`
     * exception.  If null AST pointers are being followed, the return value can
     * be a null node (see @ref withNullValues). */
    SgNode *operator*() const;

    /** Advance the iterator.
     *
     *  Advances the iterator to the next AST node in a depth-first pre-order
     * search.  If the @ref skipChildrenOnForward flag is set then all children
     * of the current node are skipped and the @ref skipChildrenOnForward flag
     * is cleared. Null child pointers are skipped by default.
     *
     * @{ */
    iterator &operator++();   // prefix
    iterator operator++(int); // postfix
    /** @} */

    /** Cause children to be skipped on the next advancement.
     *
     *  This function marks the iterator so that its next increment operator
     * will advance over the current node's children without visiting them. The
     * children are skipped only for the immediate next forward iteration (this
     * is useful when used in the iterator idiom). This function is specific to
     * tree iteration and allows to take tree structure into account although we
     * are traversing the tree nodes as a sequence. */
    void skipChildrenOnForward();

    /** Include null child pointers in subsequent traversal.
     *
     *  Call this on the root iterator before advancing it. By default, null
     * child pointers are skipped.
     *
     *  The function returns this iterator and can therefore be called together
     * with @ref begin, like this:
     *
     *  @code
     *  RoseAst::iterator i = ast.begin().withNullValues();
     *  @endcode
     *
     * @{ */
    iterator &withNullValues();
    /** @} */

    /** Parent AST node relative to the iteration.
     *
     *  Returns the parent relative to the current iterator position, which
     * might differ from the `get_parent` property of the current node. For
     * instance, @ref parent can return a non-null pointer when the current node
     * is null. */
    SgNode *parent() const;

    // internal
    bool is_past_the_end() const;
    /** Depth of traversal. */
    int stack_size() const;

  protected:
    bool _skipChildrenOnForward;
    bool _withNullValues;

  private:
    static const int ROOT_NODE_INDEX = -2;
    friend class RoseAst;
    typedef struct {
      SgNode *node;
      int index;
    } stack_element;
    std::stack<stack_element> _stack;
    SgNode *access_node_by_parent_and_index(SgNode *p, int index) const;
  };

  /** Iterator positioned at root of subtree.
   *
   *  The returned iterator holds a pointer to the root node of the subtree to
   * be traversed. The root node is specified in the constructor. */
  iterator begin();

  /** Iterator positioned at the end of the traversal.
   *
   *  The returned iterator is positioned one past the last node in the
   * depth-first pre-order traversal. */
  iterator end();

private:
  SgNode *_startNode;
};

#endif
