#ifndef ROSE_AST_QUERY
#define ROSE_AST_QUERY

// tps (01/08/2010) Added sage3basic since this doesnt compile under gcc4.1.2
// #include "sage3basic.h"
// #include "sage3.h"

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
//        typedef class NodeQuery::VariantVector VariantVector;
#include "astQueryInheritedAttribute.h"

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

  static void printVariantVector(const VariantVector &X);
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

template <typename AstQuerySynthesizedAttributeType>
struct helpFunctionalOneParamater {
  using FunctorType =
      std::function<Rose_STL_Container<AstQuerySynthesizedAttributeType>(
          SgNode *)>;
  FunctorType functor_;

  explicit helpFunctionalOneParamater(FunctorType f) : functor_{f} {}

  FunctorType operator()(SgNode *node) { return functor_(node); }
};

struct helpF {
  using result_type = bool;
  result_type operator()(bool x, bool /*y*/) { return x; }
};

template <typename AstQuerySynthesizedAttributeType, typename ArgumentType>
struct helpFunctionalTwoParamaters {
  using FunctorType =
      std::function<Rose_STL_Container<AstQuerySynthesizedAttributeType>(
          SgNode *, ArgumentType)>;
  FunctorType functor_;

  explicit helpFunctionalTwoParamaters(FunctorType f) : functor_{f} {}

  FunctorType operator()(SgNode *node, ArgumentType arg) {
    return functor_(node, arg);
  }
};

enum QueryDepth {
  UnknownListElementTypeQueryDepth = 0,
  ChildrenOnly = 1,
  AllNodes = 2,
  // DQ (4/8/2004): Added support for extracting types from tranversed
  //                nodes (types are not generally traversed).
  ExtractTypes = 3,
  END_OF_NODE_TYPE_LIST_QUERY_DEPTH
};

// #include "variantVector.h"

// **********************************
// Prototypes for Variable Node Query
// **********************************

// forward declaration
class AstQueryInheritedAttributeType;

ROSE_DLL_API void Merge(Rose_STL_Container<SgNode *> &mergeWith,
                        Rose_STL_Container<SgNode *> mergeTo);
ROSE_DLL_API void Merge(Rose_STL_Container<SgFunctionDeclaration *> &mergeWith,
                        Rose_STL_Container<SgFunctionDeclaration *> mergeTo);
ROSE_DLL_API void Merge(Rose_STL_Container<int> &mergeWith,
                        Rose_STL_Container<int> mergeTo);
ROSE_DLL_API void Merge(Rose_STL_Container<std::string> &mergeWith,
                        Rose_STL_Container<std::string> mergeTo);

// DQ & AS (3/14/2007): Added to support us of astQuery with functions returning
// void* (not clear why void does not work, except that void is not really a
// return type but instead is a signal that there is no return type).  This is
// used in the generation of the list of return types (used for testing in AST
// Consistency tests).
ROSE_DLL_API void Merge(void *mergeWith, void *mergeTo);

template <typename ResultType>
void Merge(std::vector<ResultType> &mergeWith,
           std::vector<ResultType> &mergeTo) {
  mergeWith.push_back(mergeTo);
}

// typedef std::list<SgNode*> AstQuerySynthesizedAttributeType;

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

/*******************************************************
 * The class
 *    class AstQuery
 * traverses the memory pool and performs the action
 * specified in a functional on every node. The return
 *  value is a list of SgNode*.
 *****************************************************/
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
  AstQueryReturnType listOfNodes;

public:
  AstQuery();

  AstQuery(NodeFunctional *funct);

  virtual ~AstQuery();

  // set the node Functional
  void setPredicate(NodeFunctional *funct);

  // get the result from the query
  AstQueryReturnType get_listOfNodes();

  // clear the result list of the query. a new
  // query can then be restarted using the same object.
  void clear_listOfNodes();

  // visitor function will apply the functional to every node
  // and add the result to the listOfNodes
  void visit(SgNode *node);
};

template <typename AST_Query_Base, typename NodeFunctional>
AstQuery<AST_Query_Base, NodeFunctional>::AstQuery(NodeFunctional *funct) {
  nodeFunc = funct;
}

template <typename AST_Query_Base, typename NodeFunctional>
AstQuery<AST_Query_Base, NodeFunctional>::AstQuery() {
  nodeFunc = new DefaultNodeFunctional();
}

template <typename AST_Query_Base, typename NodeFunctional>
void AstQuery<AST_Query_Base, NodeFunctional>::setPredicate(
    NodeFunctional *funct) {
  nodeFunc = funct;
}

template <typename AST_Query_Base, typename NodeFunctional>
AstQuery<AST_Query_Base, NodeFunctional>::~AstQuery() {};

template <typename AST_Query_Base, typename NodeFunctional>
typename AstQuery<AST_Query_Base, NodeFunctional>::AstQueryReturnType
AstQuery<AST_Query_Base, NodeFunctional>::get_listOfNodes() {
  return listOfNodes;
};

template <typename AST_Query_Base, typename NodeFunctional>
void AstQuery<AST_Query_Base, NodeFunctional>::clear_listOfNodes() {
  listOfNodes.clear();
};

template <typename AST_Query_Base, typename NodeFunctional>
void AstQuery<AST_Query_Base, NodeFunctional>::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(nodeFunc != NULL);

#if DEBUG_NODEQUERY
  printf(
      "%%%%% TOP of evaluateSynthesizedAttribute (astNode->sage_class_name() = "
      "%s,synthesizedAttributeList.size() = %d) \n",
      astNode->sage_class_name(), synthesizedAttributeList.size());
#endif

  // This function assemble the elements of the input list (a list of lists) to
  // form the output (a single list)
  AstQueryNamespace::Merge(listOfNodes, detail::invoke(*nodeFunc, node));
}

/********************************************************************************
 * The function
 *      std::list<ListElement> querySubTree<ListElement>(SgNode* node,
 *Predicate& _pred) will query the subtree of the IR node in the first argument
 *for nodes satisfying the criteria specified in and returned by the predicate
 *in the second argument.
 ********************************************************************************/
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
  case AstQueryNamespace::ExtractTypes:
    printf("Sorry, ExtractTypes case not implemented for "
           "e_OneParameterFunctionClassification! \n");
    ROSE_ABORT();
    break;

  default:
    printf("default reached in switch (queryType) \n");
    ROSE_ABORT();
  }

  return astQuery.get_listOfNodes();
}

/********************************************************************************
 * The function
 *      _Result querySubTree ( SgNode * subTree,
 *                   _Result (*__x)(SgNode*,_Arg), _Arg x_arg,
 *                   AstQueryNamespace::QueryDepth defineQueryType =
 *AstQueryNamespace::AllNodes )
 ** will query the subtree of the IR node in the first argument for nodes
 *satisfying the criteria specified in and returned by the function pointer in
 *the second argument.
 ********************************************************************************/
template <class _Arg, class _Result>
_Result querySubTree(SgNode *subTree, _Result (*__x)(SgNode *, _Arg),
                     _Arg x_arg,
                     AstQueryNamespace::QueryDepth defineQueryType =
                         AstQueryNamespace::AllNodes) {
  return querySubTree(subTree, std::bind(__x, std::placeholders::_1, x_arg),
                      defineQueryType);
}

/********************************************************************************
 * The function
 *      _Result querySubTree ( SgNode * subTree,
 *                   _Result (*__x)(SgNode*),
 *                   AstQueryNamespace::QueryDepth defineQueryType =
 *AstQueryNamespace::AllNodes )
 ** will query the subtree of the IR node in the first argument for nodes
 *satisfying the criteria specified in and returned by the function pointer in
 *the second argument.
 ********************************************************************************/
template <class _Result>
_Result querySubTree(SgNode *subTree, _Result (*__x)(SgNode *),
                     AstQueryNamespace::QueryDepth defineQueryType =
                         AstQueryNamespace::AllNodes) {
  return querySubTree(
      subTree, [__x](SgNode *n) { return __x(n); }, defineQueryType);
}

/********************************************************************************
 * The function
 * _Result queryRange(typename _Result::const_iterator& begin, typename
 *_Result::const_iterator& end, Predicate _pred) will query the iterator
 *_Result::const_iterator from 'begin' to 'end' for IR nodes satisfying the
 *criteria specified in and returned by the predicate in the third argument.
 ********************************************************************************/
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

/********************************************************************************
 * The function
 * _Result queryRange(typename _Result::const_iterator& begin, typename
 *_Result::const_iterator& end, _Result (*__x)(SgNode*,_Arg), _Arg x_arg,
 *                   AstQueryNamespace::QueryDepth defineQueryType =
 *AstQueryNamespace::AllNodes ) will query the iterator _Result::const_iterator
 *from 'begin' to 'end' for IR nodes satisfying the criteria specified in and
 *returned by the function pointer in the third argument given the fourth
 *argument 'x_arg'.
 ********************************************************************************/
template <class _Arg, class _Result>
_Result queryRange(typename _Result::iterator begin,
                   const typename _Result::iterator end,
                   _Result (*__x)(SgNode *, _Arg), _Arg x_arg) {
  return queryRange(begin, end, std::bind(__x, std::placeholders::_1, x_arg));
}

/********************************************************************************
 * The function
 * _Result queryRange(typename _Result::const_iterator& begin, typename
 *_Result::const_iterator& end, _Result (*__x)(SgNode*),
 *                   AstQueryNamespace::QueryDepth defineQueryType =
 *AstQueryNamespace::AllNodes ) will query the iterator _Result::const_iterator
 *from 'begin' to 'end' for IR nodes satisfying the criteria specified in and
 *returned by the function pointer in the third argument.
 ********************************************************************************/
template <class _Result>
_Result queryRange(typename _Result::iterator begin,
                   typename _Result::iterator end, _Result (*__x)(SgNode *)) {
  return queryRange(begin, end, [__x](SgNode *n) { return __x(n); });
}

/****************************************************************************
 * The function
 *  void queryMemoryPool(ROSE_VisitTraversal& astQuery, VariantVector*
 *variantsToTraverse); traverses the parts of the memory pool which has
 *corresponding variants in VariantVector.
 ***************************************************************************/

template <class FunctionalType>
void queryMemoryPool(AstQuery<ROSE_VisitTraversal, FunctionalType> &astQuery,
                     VariantVector *variantsToTraverse);

/********************************************************************************
 * The function
 *      std::list<ListElement> querySubTree<ListElement>(SgNode* node,
 *Predicate& _pred) will query the subtree of the IR node in the first argument
 *for nodes satisfying the criteria specified in and returned by the predicate
 *in the second argument.
 ********************************************************************************/
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

/********************************************************************************
 * The function
 *      _Result querySubTree ( SgNode * subTree,
 *                   _Result (*__x)(SgNode*,_Arg), _Arg x_arg,
 *                   AstQueryNamespace::QueryDepth defineQueryType =
 *AstQueryNamespace::AllNodes ){
 ** will query the subtree of the IR node in the first argument for nodes
 *satisfying the criteria specified in and returned by the function pointer in
 *the second argument.
 ********************************************************************************/
template <class _Arg, class _Result>
_Result queryMemoryPool(_Result (*__x)(SgNode *, _Arg), _Arg x_arg,
                        VariantVector *targetVariantVector = NULL) {
  return queryMemoryPool(std::bind(__x, std::placeholders::_1, x_arg),
                         targetVariantVector);
}

/********************************************************************************
 * The function
 *      _Result querySubTree ( SgNode * subTree,
 *                   _Result (*__x)(SgNode*),
 *                   AstQueryNamespace::QueryDepth defineQueryType =
 *AstQueryNamespace::AllNodes ){
 ** will query the subtree of the IR node in the first argument for nodes
 *satisfying the criteria specified in and returned by the function pointer in
 *the second argument.
 ********************************************************************************/
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
