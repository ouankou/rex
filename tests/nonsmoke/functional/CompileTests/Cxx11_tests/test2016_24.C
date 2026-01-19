// Allocator traits -*- C++ -*-

// Copyright (C) 2011-2014 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file bits/alloc_traits.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{memory}
 */

#ifndef _ALLOC_TRAITS_H
#define _ALLOC_TRAITS_H 1

#if __cplusplus >= 201103L

#include <bits/memoryfwd.h>
// #include <bits/ptr_traits.h>

#include <ext/numeric_traits.h>

namespace std _GLIBCXX_VISIBILITY(default) {
_GLIBCXX_BEGIN_NAMESPACE_VERSION

template <typename _Alloc, typename _Tp> class __alloctr_rebind_helper {};

template <typename _Alloc, typename _Tp,
          bool = __alloctr_rebind_helper<_Alloc, _Tp>::__type::value>
struct __alloctr_rebind;

template <typename _Alloc, typename _Tp>
struct __alloctr_rebind<_Alloc, _Tp, true> {
  typedef typename _Alloc::template rebind<_Tp>::other __type;
};

template <template <typename, typename...> class _Alloc, typename _Tp,
          typename _Up, typename... _Args>
struct __alloctr_rebind<_Alloc<_Up, _Args...>, _Tp, false> {
  typedef _Alloc<_Tp, _Args...> __type;
};

/**
 * @brief  Uniform interface to all allocator types.
 * @ingroup allocators
 */
template <typename _Alloc> struct allocator_traits {
  /// The allocator type
  typedef _Alloc allocator_type;
  /// The allocated type
  typedef typename _Alloc::value_type value_type;

#define _GLIBCXX_ALLOC_TR_NESTED_TYPE(_NTYPE, _ALT)                            \
private:                                                                       \
  template <typename _Tp>                                                      \
  static typename _Tp::_NTYPE _S_##_NTYPE##_helper(_Tp *);                     \
  static _ALT _S_##_NTYPE##_helper(...);                                       \
  typedef decltype(_S_##_NTYPE##_helper((_Alloc *)0)) __##_NTYPE;              \
                                                                               \
public:

  _GLIBCXX_ALLOC_TR_NESTED_TYPE(pointer, value_type *)

  /**
   * @brief   The allocator's pointer type.
   *
   * @c Alloc::pointer if that type exists, otherwise @c value_type*
   */
  typedef __pointer pointer;

#undef _GLIBCXX_ALLOC_TR_NESTED_TYPE

  template <typename _Tp>
  using rebind_alloc = typename __alloctr_rebind<_Alloc, _Tp>::__type;
  template <typename _Tp>
  using rebind_traits = allocator_traits<rebind_alloc<_Tp>>;

private:
  template <typename _Alloc2> struct __allocate_helper {};

  template <typename _Alloc2>
  using __has_allocate = typename __allocate_helper<_Alloc2>::type;

  template <typename _Tp, typename... _Args> struct __construct_helper {};

  template <typename _Tp, typename... _Args>
  using __has_construct = typename __construct_helper<_Tp, _Args...>::type;

public:
};

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std _GLIBCXX_VISIBILITY(default)

#endif
#endif
