#ifndef ROSE_AST_QUERY
#define ROSE_AST_QUERY

#include "AstProcessing.h"

#include "rosedll.h"

#include "ROSE_ABORT.h"

#include <functional>

#include <type_traits>
#include <utility>
// Support for operations like (SgTypeInt | SgTypeFloat)
// note that non-terminals would be expanded into the associated terminals!
// So SgType would generate (SgTypeInt | SgTypeFloat | SgTypeDouble | ... |
// <last type>)

class ROSE_DLL_API VariantVector : public std::vector<VariantT> {
  // This class is derived from the STL vector class
public:
  VariantVector() {}
  VariantVector(const VariantT &X);
  VariantVector(const VariantVector &X);
  VariantVector operator=(const VariantVector &X);

  ROSE_DLL_API friend VariantVector operator+(VariantT lhs, VariantT rhs);
  ROSE_DLL_API friend VariantVector operator+(VariantT lhs,
                                              const VariantVector &rhs);
  ROSE_DLL_API friend VariantVector operator+(const VariantVector &lhs,
                                              VariantT rhs);
  ROSE_DLL_API friend VariantVector operator+(const VariantVector &lhs,
                                              const VariantVector &rhs);
};

// DQ (4/23/2006): g++ 4.1.0 requires friend functions to be declared outside
// the class as well!
ROSE_DLL_API VariantVector operator+(VariantT lhs, VariantT rhs);
ROSE_DLL_API VariantVector operator+(VariantT lhs, const VariantVector &rhs);
ROSE_DLL_API VariantVector operator+(const VariantVector &lhs, VariantT rhs);
ROSE_DLL_API VariantVector operator+(const VariantVector &lhs,
                                     const VariantVector &rhs);

namespace AstQueryNamespace {
namespace detail {
// Keep AST query helpers usable in C++14 compile tests even though REX itself
// is built as C++17.
template <typename...> struct make_void {
  using type = void;
};

template <typename... Ts> using void_t = typename make_void<Ts...>::type;

#if __cplusplus >= 201703L
template <typename F, typename... Args>
using invoke_result_t = typename std::invoke_result<F, Args...>::type;
#else
template <typename F, typename... Args>
using invoke_result_t = typename std::result_of<F(Args...)>::type;
#endif

template <typename, typename F, typename... Args>
struct is_invocable_impl : std::false_type {};

template <typename F, typename... Args>
struct is_invocable_impl<void_t<invoke_result_t<F, Args...>>, F, Args...>
    : std::true_type {};

template <typename F, typename... Args>
struct is_invocable : is_invocable_impl<void, F, Args...> {};

template <typename F, typename... Args>
auto invoke(F &&f, Args &&...args) -> typename std::enable_if<
    std::is_member_pointer<typename std::decay<F>::type>::value,
    invoke_result_t<F &&, Args &&...>>::type {
  return std::mem_fn(f)(std::forward<Args>(args)...);
}

template <typename F, typename... Args>
auto invoke(F &&f, Args &&...args) -> typename std::enable_if<
    !std::is_member_pointer<typename std::decay<F>::type>::value,
    invoke_result_t<F &&, Args &&...>>::type {
  return std::forward<F>(f)(std::forward<Args>(args)...);
}
} // namespace detail

enum QueryDepth {
  ChildrenOnly = 1,
  AllNodes = 2,
};

template <typename ResultType>
void Merge(std::vector<ResultType> &mergeWith,
           const std::vector<ResultType> &mergeTo) {
  mergeWith.insert(mergeWith.end(), mergeTo.begin(), mergeTo.end());
}

/** @brief Simplifies development of queries on the AST that return lists of AST
 * nodes.
 *
 * This class uses several member functions within its interface. Each member
 * function takes an AST node pointer (any subtree of the AST). It represents a
 * library of queries that return a list of SgNode pointers.
 *
 * See @ref nodeQueryLib.
 */
struct DefaultNodeFunctional {
  using result_type = Rose_STL_Container<SgNode *>;
  result_type operator()(SgNode *node) const {
    result_type returnType;
    returnType.push_back(node);
    return returnType;
  }
};

class AstQuery_DUMMY {};

template <typename AST_Query_Base = AstQuery_DUMMY,
          typename NodeFunctional = DefaultNodeFunctional>
class AstQuery : public AST_Query_Base
// public ROSE_VisitTraversal, public AstSimpleProcessing
{

  // Instantiate the functional which returns the list of nodes to be processed
  NodeFunctional *nodeFunc;

  static_assert(detail::is_invocable<NodeFunctional &, SgNode *>::value,
                "AstQuery requires a callable that accepts SgNode*");

  // When a node satisfies a functional it is added to
  // this list.
  typedef detail::invoke_result_t<NodeFunctional &, SgNode *>
      AstQueryReturnType;
  AstQueryReturnType listOfNodes{};

  void visitAndMerge(SgNode *node, std::false_type);
  void visitAndMerge(SgNode *node, std::true_type);

public:
  AstQuery(NodeFunctional *funct);

  virtual ~AstQuery() = default;

  // get the result from the query
  AstQueryReturnType get_listOfNodes();

  // visitor function will apply the functional to every node
  // and add the result to the listOfNodes
  void visit(SgNode *node);
};

template <typename AST_Query_Base, typename NodeFunctional>
AstQuery<AST_Query_Base, NodeFunctional>::AstQuery(NodeFunctional *funct)
    : nodeFunc(funct) {
  ROSE_ASSERT(nodeFunc != nullptr);
}

template <typename AST_Query_Base, typename NodeFunctional>
typename AstQuery<AST_Query_Base, NodeFunctional>::AstQueryReturnType
AstQuery<AST_Query_Base, NodeFunctional>::get_listOfNodes() {
  return listOfNodes;
};

template <typename AST_Query_Base, typename NodeFunctional>
void AstQuery<AST_Query_Base, NodeFunctional>::visitAndMerge(SgNode *node,
                                                             std::false_type) {
  AstQueryNamespace::Merge(listOfNodes, detail::invoke(*nodeFunc, node));
}

template <typename AST_Query_Base, typename NodeFunctional>
void AstQuery<AST_Query_Base, NodeFunctional>::visitAndMerge(SgNode *node,
                                                             std::true_type) {
  // A void pointer denotes a side-effect-only query with no results to merge.
  detail::invoke(*nodeFunc, node);
}

template <typename AST_Query_Base, typename NodeFunctional>
void AstQuery<AST_Query_Base, NodeFunctional>::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(nodeFunc != NULL);

  // This function assemble the elements of the input list (a list of lists) to
  // form the output (a single list)
  visitAndMerge(node, std::is_same<AstQueryReturnType, void *>());
}

template <typename NodeFunctional>
detail::invoke_result_t<NodeFunctional &, SgNode *> querySubTree(
    SgNode *node, NodeFunctional nodeFunc,
    AstQueryNamespace::QueryDepth defineQueryType = AstQueryNamespace::AllNodes,
    t_traverseOrder treeTraversalOrder = preorder) {
  static_assert(detail::is_invocable<NodeFunctional &, SgNode *>::value,
                "nodeFunc must be callable with SgNode*");
  ROSE_ASSERT(node != NULL);

  AstQuery<AstSimpleProcessing, NodeFunctional> astQuery(&nodeFunc);
  switch (defineQueryType) {
  case AstQueryNamespace::AllNodes: {
    astQuery.traverse(node, treeTraversalOrder);

    break;
  }
  case AstQueryNamespace::ChildrenOnly: {
    // visit only the nodes which is pointed to by this class
    typedef std::vector<SgNode *> DataMemberPointerType;

    DataMemberPointerType returnData = node->get_traversalSuccessorContainer();

    // A child of a node is the nodes it points to.
    for (DataMemberPointerType::iterator i = returnData.begin();
         i != returnData.end(); ++i) {
      // visit the node which is pointed to by this SgNode
      if (*i != NULL)
        astQuery.visit(*i);
    };

    break;
  }
  default:
    printf("default reached in switch (queryType) \n");
    ROSE_ABORT();
  }

  return astQuery.get_listOfNodes();
}

template <class _Arg, class _Result>
_Result querySubTree(SgNode *subTree, _Result (*__x)(SgNode *, _Arg),
                     _Arg x_arg,
                     AstQueryNamespace::QueryDepth defineQueryType =
                         AstQueryNamespace::AllNodes) {
  return querySubTree(subTree, std::bind(__x, std::placeholders::_1, x_arg),
                      defineQueryType);
}

template <class _Result>
_Result querySubTree(SgNode *subTree, _Result (*__x)(SgNode *),
                     AstQueryNamespace::QueryDepth defineQueryType =
                         AstQueryNamespace::AllNodes) {
  return querySubTree(
      subTree, [__x](SgNode *n) { return __x(n); }, defineQueryType);
}

template <class Iterator, class NodeFunctional>
detail::invoke_result_t<NodeFunctional &, SgNode *>
queryRange(Iterator begin, Iterator end, NodeFunctional nodeFunc) {
  static_assert(detail::is_invocable<NodeFunctional &, SgNode *>::value,
                "nodeFunc must be callable with SgNode*");

  AstQuery<AstQuery_DUMMY, NodeFunctional> astQuery(&nodeFunc);

  for (; begin != end; ++begin) {
    astQuery.visit(*begin);
  }

  return astQuery.get_listOfNodes();
}

template <class _Arg, class _Result>
_Result queryRange(typename _Result::iterator begin,
                   const typename _Result::iterator end,
                   _Result (*__x)(SgNode *, _Arg), _Arg x_arg) {
  return queryRange(begin, end, std::bind(__x, std::placeholders::_1, x_arg));
}

template <class _Result>
_Result queryRange(typename _Result::iterator begin,
                   typename _Result::iterator end, _Result (*__x)(SgNode *)) {
  return queryRange(begin, end, [__x](SgNode *n) { return __x(n); });
}


template <class FunctionalType>
void queryMemoryPool(AstQuery<ROSE_VisitTraversal, FunctionalType> &astQuery,
                     VariantVector *variantsToTraverse);

template <typename NodeFunctional>
detail::invoke_result_t<NodeFunctional &, SgNode *>
queryMemoryPool(NodeFunctional nodeFunc,
                VariantVector *targetVariantVector = NULL) {
  static_assert(detail::is_invocable<NodeFunctional &, SgNode *>::value,
                "nodeFunc must be callable with SgNode*");

  AstQuery<ROSE_VisitTraversal, NodeFunctional> astQuery(&nodeFunc);
  if (targetVariantVector == NULL) {
    // Query the whole memory pool
    astQuery.traverseMemoryPool();
  } else {
    queryMemoryPool<NodeFunctional>(astQuery, targetVariantVector);
  }; // end if-else

  return astQuery.get_listOfNodes();
}

template <class _Arg, class _Result>
_Result queryMemoryPool(_Result (*__x)(SgNode *, _Arg), _Arg x_arg,
                        VariantVector *targetVariantVector = NULL) {
  return queryMemoryPool(std::bind(__x, std::placeholders::_1, x_arg),
                         targetVariantVector);
}

template <class _Result>
_Result queryMemoryPool(_Result (*__x)(SgNode *),
                        VariantVector *targetVariantVector = NULL) {
  return queryMemoryPool([__x](SgNode *n) { return __x(n); },
                         targetVariantVector);
}

}; // namespace AstQueryNamespace

#include <AstQueryMemoryPool.h>
// endif for ROSE_NAME_QUERY
#endif
