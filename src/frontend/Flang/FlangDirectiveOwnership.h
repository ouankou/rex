#ifndef ROSE_FLANG_DIRECTIVE_OWNERSHIP_H
#define ROSE_FLANG_DIRECTIVE_OWNERSHIP_H

#include <flang/Parser/parse-tree.h>
#include <list>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace Rose::builder {

[[noreturn]] void RejectFlangOpenMPParseTreeNode(const char *boundary);

namespace detail {
template <typename T>
using Unqualified = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T> struct ClausePayloadValue {
  using type = T;
};
template <typename T>
struct ClausePayloadValue<std::optional<T>>
    : ClausePayloadValue<Unqualified<T>> {};
template <typename T, typename Allocator>
struct ClausePayloadValue<std::list<T, Allocator>>
    : ClausePayloadValue<Unqualified<T>> {};

template <typename T, typename Wrapper, typename = void>
struct IsFlangOpenMPClausePayload : std::false_type {};

template <typename T, typename Wrapper>
struct IsFlangOpenMPClausePayload<
    T, Wrapper, std::void_t<decltype(std::declval<Wrapper &>().v)>>
    : std::is_same<T, typename ClausePayloadValue<Unqualified<
                          decltype(std::declval<Wrapper &>().v)>>::type> {};

template <typename T, typename Variant>
struct IsFlangOpenMPClauseNode : std::false_type {};

// The generated union owns its empty tags, clause wrappers, and their immediate
// payloads. Do not follow arbitrary payload members into ordinary Fortran
// expressions. Shared language values are explicit stopping points below.
template <typename T, typename... Clauses>
struct IsFlangOpenMPClauseNode<T, std::variant<Clauses...>>
    : std::bool_constant<((std::is_same_v<T, Clauses> ||
                           IsFlangOpenMPClausePayload<T, Clauses>::value) ||
                          ...)> {};

template <typename T, typename = void>
struct WrapsFlangOpenMPDirective : std::false_type {};

template <typename T>
struct WrapsFlangOpenMPDirective<T,
                                 std::void_t<decltype(std::declval<T &>().v)>>
    : std::is_same<std::remove_cv_t<std::remove_reference_t<
                       decltype(std::declval<T &>().v)>>,
                   Fortran::parser::OmpDirectiveSpecification> {};

// Reject every legal entry to a native OpenMP construct before a generic
// walker can descend into its semantic payload or silently ignore a wrapper.
template <typename T>
struct IsFlangOpenMPParseTreeNode
    : std::bool_constant<
          std::is_base_of_v<Fortran::parser::OmpDirectiveSpecification, T> ||
          std::is_base_of_v<Fortran::parser::OmpBlockConstruct, T> ||
          std::is_base_of_v<Fortran::parser::OmpDirectiveName, T> ||
          IsFlangOpenMPClauseNode<
              T, decltype(std::declval<Fortran::parser::OmpClause &>()
                              .u)>::value ||
          WrapsFlangOpenMPDirective<T>::value ||
          std::is_same_v<T, Fortran::parser::OpenMPConstruct> ||
          std::is_same_v<T, Fortran::parser::OpenMPDeclarativeConstruct> ||
          std::is_same_v<T, Fortran::parser::OpenMPStandaloneConstruct> ||
          std::is_same_v<T, Fortran::parser::OpenMPSectionsConstruct> ||
          std::is_same_v<T, Fortran::parser::OmpSectionDirective> ||
          std::is_same_v<T, Fortran::parser::OmpUtilityDirective> ||
          std::is_same_v<T, Fortran::parser::OpenMPExecDirective> ||
          std::is_same_v<T, Fortran::parser::OpenMPInvalidDirective> ||
          std::is_same_v<T, Fortran::parser::OmpClause> ||
          std::is_same_v<T, Fortran::parser::OmpClauseList> ||
          std::is_same_v<T, Fortran::parser::OmpArgument> ||
          std::is_same_v<T, Fortran::parser::OmpArgumentList> ||
          std::is_same_v<T, Fortran::parser::OmpObject> ||
          std::is_same_v<T, Fortran::parser::OmpIteratorSpecifier> ||
          std::is_same_v<T, Fortran::parser::OmpMapperSpecifier> ||
          std::is_same_v<T, Fortran::parser::OmpReductionIdentifier>> {};

// Scalar expressions and identifiers can occur directly in generated OpenMP
// wrappers, but their node types belong to the base language and OpenACC too.
template <typename T>
struct IsFlangOpenMPParseTreeNode<Fortran::parser::Scalar<T>>
    : std::false_type {};
template <>
struct IsFlangOpenMPParseTreeNode<Fortran::parser::Expr> : std::false_type {};
template <>
struct IsFlangOpenMPParseTreeNode<Fortran::parser::Name> : std::false_type {};

template <typename T>
struct IsFlangOpenMPParseTreeNode<Fortran::parser::Statement<T>>
    : IsFlangOpenMPParseTreeNode<Unqualified<T>> {};
template <typename T>
struct IsFlangOpenMPParseTreeNode<Fortran::parser::UnlabeledStatement<T>>
    : IsFlangOpenMPParseTreeNode<Unqualified<T>> {};
template <typename T>
struct IsFlangOpenMPParseTreeNode<Fortran::common::Indirection<T>>
    : IsFlangOpenMPParseTreeNode<Unqualified<T>> {};
} // namespace detail

template <typename T>
void RequireNoFlangOpenMPParseTreeNode(const T &, const char *boundary) {
  using Node = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (detail::IsFlangOpenMPParseTreeNode<Node>::value)
    RejectFlangOpenMPParseTreeNode(boundary);
}

} // namespace Rose::builder

#endif
