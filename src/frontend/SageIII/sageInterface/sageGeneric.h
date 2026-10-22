#ifndef _SAGEGENERIC_H

#define _SAGEGENERIC_H 1

#include <string>
#include <type_traits>
#include <utility>

#define SG_UNEXPECTED_NODE(X) (sg::unexpected_node(X, __FILE__, __LINE__))
/// This namespace contains template functions that operate on the ROSE AST
namespace sg {
[[noreturn]]
void report_error(std::string desc, const char *file = nullptr, size_t ln = 0);

[[noreturn]]
void unexpected_node(const SgNode &n, const char *file = nullptr,
                     size_t ln = 0);

template <class _ReturnType> struct DispatchHandler {
  typedef _ReturnType ReturnType;
  typedef DispatchHandler<ReturnType> Base;

  DispatchHandler() : res() {}

  explicit DispatchHandler(const ReturnType &defaultval) : res(defaultval) {}

  operator ReturnType() const { return res; }

protected:
  ReturnType res;
};

#define GEN_VISIT(X)                                                           \
  void visit(X *n) { rv.handle(*n); }

template <class RoseVisitor>
struct VisitDispatcher : ROSE_VisitorPatternDefaultBase {
  // rvalue ctor
  VisitDispatcher(RoseVisitor &&rosevisitor, std::false_type)
      : rv(std::move(rosevisitor)) {}

  // lvalue ctor
  VisitDispatcher(const RoseVisitor &rosevisitor, std::true_type)
      : rv(rosevisitor) {}

  ROSE_AST_FOR_EACH_NODE(GEN_VISIT)

  RoseVisitor rv;
};

#undef GEN_VISIT

template <class RoseVisitor>
inline typename std::remove_const<
    typename std::remove_reference<RoseVisitor>::type>::type
_dispatch(RoseVisitor &&rv, SgNode *n) {
  typedef typename std::remove_reference<RoseVisitor>::type RoseVisitorNoref;
  typedef typename std::remove_const<RoseVisitorNoref>::type RoseHandler;

  ROSE_ASSERT(n);

  VisitDispatcher<RoseHandler> vis(std::forward<RoseVisitor>(rv),
                                   std::is_lvalue_reference<RoseVisitor>());

  n->accept(static_cast<ROSE_VisitorPattern &>(vis));
  return std::move(vis).rv;
}

template <class RoseVisitor>
inline typename std::remove_const<
    typename std::remove_reference<RoseVisitor>::type>::type
dispatch(RoseVisitor &&rv, SgNode *n) {
  return _dispatch(std::forward<RoseVisitor>(rv), n);
}

template <class RoseVisitor>
inline typename std::remove_const<
    typename std::remove_reference<RoseVisitor>::type>::type
dispatch(RoseVisitor &&rv, const SgNode *n) {
  return _dispatch(std::forward<RoseVisitor>(rv), const_cast<SgNode *>(n));
}

template <class SageNode> struct DefaultHandler {
  void handle(SageNode &) {}
};

template <class AncestorNode, class QualSgNode>
struct AncestorTypeFinder : DefaultHandler<const SgProject> {
  typedef DefaultHandler<const SgProject> Base;
  typedef std::pair<AncestorNode *, QualSgNode *> Pair;

  Pair res;

  AncestorTypeFinder() : Base(), res(NULL, NULL) {}

  // handling of const SgProject is outsourced to DefaultHandler
  // thus, AncestorNode = const SgProject does not cause conflicts
  using Base::handle;

  void handle(QualSgNode &n) { res.second = n.get_parent(); }
  void handle(AncestorNode &n) { res.first = &n; }

  operator Pair() const { return res; }
};

template <class AncestorNode, class QualSgNode>
AncestorNode *_ancestor(QualSgNode &n) {
  typedef AncestorTypeFinder<AncestorNode, QualSgNode> AncestorFinder;

  typename AncestorFinder::Pair res(NULL, n.get_parent());

  while (res.second != NULL) {
    res = (typename AncestorFinder::Pair)sg::dispatch(AncestorFinder(),
                                                      res.second);
  }

  return res.first;
}

template <class AncestorNode> AncestorNode *ancestor(SgNode *n) {
  if (n == NULL)
    return NULL;

  return _ancestor<AncestorNode>(*n);
}

/// \overload
template <class AncestorNode> const AncestorNode *ancestor(const SgNode *n) {
  if (n == NULL)
    return NULL;

  return _ancestor<const AncestorNode>(*n);
}

/// \overload
template <class AncestorNode> AncestorNode &ancestor(SgNode &n) {
  AncestorNode *res = _ancestor<AncestorNode>(n);

  ROSE_ASSERT(res);
  return *res;
}

/// \overload
template <class AncestorNode> const AncestorNode &ancestor(const SgNode &n) {
  const AncestorNode *res = _ancestor<const AncestorNode>(n);

  ROSE_ASSERT(res);
  return *res;
}

} // namespace sg
#endif /* _SAGEGENERIC_H */
