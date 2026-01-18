// Copyright 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Authors: wan@google.com (Zhanyong Wan)
//
// Low-level types and utilities for porting Google Test to various
// platforms.  They are subject to change without notice.  DO NOT USE
// THEM IN USER CODE.
//
// This file is fundamental to Google Test.  All other Google Test source
// files are expected to #include this.  Therefore, it cannot #include
// any other Google Test header.

#ifndef GTEST_INCLUDE_GTEST_INTERNAL_GTEST_PORT_H_
#define GTEST_INCLUDE_GTEST_INTERNAL_GTEST_PORT_H_

// The user can define the following macros in the build script to
// control Google Test's behavior.  If the user doesn't define a macro
// in this list, Google Test will define it.
//
//   GTEST_HAS_CLONE          - Define it to 1/0 to indicate that clone(2)
//                              is/isn't available.
//   GTEST_HAS_EXCEPTIONS     - Define it to 1/0 to indicate that exceptions
//                              are enabled.
//   GTEST_HAS_GLOBAL_STRING  - Define it to 1/0 to indicate that ::string
//                              is/isn't available (some systems define
//                              ::string, which is different to std::string).
//   GTEST_HAS_GLOBAL_WSTRING - Define it to 1/0 to indicate that ::string
//                              is/isn't available (some systems define
//                              ::wstring, which is different to std::wstring).
//   GTEST_HAS_POSIX_RE       - Define it to 1/0 to indicate that POSIX regular
//                              expressions are/aren't available.
//   GTEST_HAS_PTHREAD        - Define it to 1/0 to indicate that <pthread.h>
//                              is/isn't available.
//   GTEST_HAS_RTTI           - Define it to 1/0 to indicate that RTTI is/isn't
//                              enabled.
//   GTEST_HAS_STD_WSTRING    - Define it to 1/0 to indicate that
//                              std::wstring does/doesn't work (Google Test can
//                              be used where std::wstring is unavailable).
//   GTEST_HAS_TR1_TUPLE      - Define it to 1/0 to indicate tr1::tuple
//                              is/isn't available.
//   GTEST_HAS_SEH            - Define it to 1/0 to indicate whether the
//                              compiler supports Microsoft's "Structured
//                              Exception Handling".
//   GTEST_HAS_STREAM_REDIRECTION
//                            - Define it to 1/0 to indicate whether the
//                              platform supports I/O stream redirection using
//                              dup() and dup2().
//   GTEST_USE_OWN_TR1_TUPLE  - Define it to 1/0 to indicate whether Google
//                              Test's own tr1 tuple implementation should be
//                              used.  Unused when the user sets
//                              GTEST_HAS_TR1_TUPLE to 0.
//   GTEST_LANG_CXX11         - Define it to 1/0 to indicate that Google Test
//                              is building in C++11/C++98 mode.
//   GTEST_LINKED_AS_SHARED_LIBRARY
//                            - Define to 1 when compiling tests that use
//                              Google Test as a shared library.
//   GTEST_CREATE_SHARED_LIBRARY
//                            - Define to 1 when compiling Google Test itself
//                              as a shared library.

// This header defines the following utilities:
//
// Macros indicating the current platform (defined to 1 if compiled on
// the given platform; otherwise undefined):
//   GTEST_OS_LINUX    - Linux
//
// Note that it is possible that none of the GTEST_OS_* macros are defined.
//
// Macros indicating available Google Test features (defined to 1 if
// the corresponding feature is supported; otherwise undefined):
//   GTEST_HAS_COMBINE      - the Combine() function (for value-parameterized
//                            tests)
//   GTEST_HAS_DEATH_TEST   - death tests
//   GTEST_HAS_PARAM_TEST   - value-parameterized tests
//   GTEST_HAS_TYPED_TEST   - typed tests
//   GTEST_HAS_TYPED_TEST_P - type-parameterized tests
//   GTEST_USES_POSIX_RE    - enhanced POSIX regex is used. Do not confuse with
//                            GTEST_HAS_POSIX_RE (see above) which users can
//                            define themselves.
//   GTEST_USES_SIMPLE_RE   - our own simple regex is used;
//                            the above two are mutually exclusive.
//   GTEST_CAN_COMPARE_NULL - accepts untyped NULL in EXPECT_EQ().
//
// Macros for basic C++ coding:
//   GTEST_AMBIGUOUS_ELSE_BLOCKER_ - for disabling a gcc warning.
//   GTEST_ATTRIBUTE_UNUSED_  - declares that a class' instances or a
//                              variable don't have to be used.
//   GTEST_DISALLOW_ASSIGN_   - disables operator=.
//   GTEST_DISALLOW_COPY_AND_ASSIGN_ - disables copy ctor and operator=.
//   GTEST_MUST_USE_RESULT_   - declares that a function's result must be used.
//
// Synchronization:
//   Mutex, MutexLock, ThreadLocal, GetThreadCount()
//                  - synchronization primitives.
//   GTEST_IS_THREADSAFE - defined to 1 to indicate that the above
//                         synchronization primitives have real implementations
//                         and Google Test is thread-safe; or 0 otherwise.
//
// Template meta programming:
//   is_pointer     - as in TR1; needed on some older compilers only.
//   IteratorTraits - partial implementation of std::iterator_traits, which
//                    is not available in some standard libraries.
//
// Smart pointers:
//   scoped_ptr     - as in TR2.
//
// Regular expressions:
//   RE             - a simple regular expression class using the POSIX
//                    Extended Regular Expression syntax, or a reduced
//                    regular expression syntax when POSIX isn't available.
//
// Logging:
//   GTEST_LOG_()   - logs messages at the specified severity level.
//   LogToStderr()  - directs all log messages to stderr.
//   FlushInfoLog() - flushes informational log messages.
//
// Stdout and stderr capturing:
//   CaptureStdout()     - starts capturing stdout.
//   GetCapturedStdout() - stops capturing stdout and returns the captured
//                         string.
//   CaptureStderr()     - starts capturing stderr.
//   GetCapturedStderr() - stops capturing stderr and returns the captured
//                         string.
//
// Integer types:
//   TypeWithSize   - maps an integer to a int type.
//   Int32, UInt32, Int64, UInt64, TimeInMillis
//                  - integers of known sizes.
//   BiggestInt     - the biggest signed integer type.
//
// Command-line utilities:
//   GTEST_FLAG()       - references a flag.
//   GTEST_DECLARE_*()  - declares a flag.
//   GTEST_DEFINE_*()   - defines a flag.
//   GetInjectableArgvs() - returns the command line as a vector of strings.
//
// Environment variable utilities:
//   GetEnv()             - gets the value of an environment variable.
//   BoolFromGTestEnv()   - parses a bool environment variable.
//   Int32FromGTestEnv()  - parses an Int32 environment variable.
//   StringFromGTestEnv() - parses a string environment variable.

#include <ctype.h>  // for isspace, etc
#include <stddef.h> // for ptrdiff_t
#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <sys/stat.h>

#include <sys/types.h>

#include <iostream> // NOLINT
#include <sstream>  // NOLINT
#include <string>   // NOLINT

#define GTEST_DEV_EMAIL_ "googletestframework@@googlegroups.com"
#define GTEST_FLAG_PREFIX_ "gtest_"
#define GTEST_FLAG_PREFIX_DASH_ "gtest-"
#define GTEST_FLAG_PREFIX_UPPER_ "GTEST_"
#define GTEST_NAME_ "Google Test"
#define GTEST_PROJECT_URL_ "http://code.google.com/p/googletest/"

// Determines the version of gcc that is used to compile this.
#ifdef __GNUC__
// 40302 means version 4.3.2.
#define GTEST_GCC_VER_                                                         \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif // __GNUC__

// Determines the platform on which Google Test is compiled.
#if !defined(__linux__)
#error "REX Google Test support is Linux-only."
#endif
#define GTEST_OS_LINUX 1

#ifndef GTEST_LANG_CXX11
// gcc and clang define __GXX_EXPERIMENTAL_CXX0X__ when
// -std={c,gnu}++{0x,11} is passed.  The C++11 standard specifies a
// value for __cplusplus, and recent versions of clang, gcc, and
// probably other compilers set that too in C++11 mode.
#if __GXX_EXPERIMENTAL_CXX0X__ || __cplusplus >= 201103L
// Compiling in at least C++11 mode.
#define GTEST_LANG_CXX11 1
#else
#define GTEST_LANG_CXX11 0
#endif
#endif

// Brings in definitions for functions used in the testing::internal::posix
// namespace (read, write, close, chdir, isatty, stat).
#include <strings.h>

#include <unistd.h>

// Defines this to true iff Google Test can use POSIX regular expressions.
#ifndef GTEST_HAS_POSIX_RE
#define GTEST_HAS_POSIX_RE 1
#endif

#if GTEST_HAS_POSIX_RE

// On some platforms, <regex.h> needs someone to define size_t, and
// won't compile otherwise.  We can #include it here as we already
// included <stdlib.h>, which is guaranteed to define size_t through
// <stddef.h>.
#include <regex.h> // NOLINT

#define GTEST_USES_POSIX_RE 1

#endif // GTEST_HAS_POSIX_RE

#ifndef GTEST_HAS_EXCEPTIONS
// The user didn't tell us whether exceptions are enabled, so we need
// to figure it out.
#if defined(__EXCEPTIONS)
#define GTEST_HAS_EXCEPTIONS 1
#else
// For other compilers, assume exceptions are disabled unless overridden.
#define GTEST_HAS_EXCEPTIONS 0
#endif
#endif // GTEST_HAS_EXCEPTIONS

#if !defined(GTEST_HAS_STD_STRING)
// Even though we don't use this macro any longer, we keep it in case
// some clients still depend on it.
#define GTEST_HAS_STD_STRING 1
#elif !GTEST_HAS_STD_STRING
// The user told us that ::std::string isn't available.
#error "Google Test cannot be used where ::std::string isn't available."
#endif // !defined(GTEST_HAS_STD_STRING)

#ifndef GTEST_HAS_GLOBAL_STRING
// The user didn't tell us whether ::string is available, so we need
// to figure it out.

#define GTEST_HAS_GLOBAL_STRING 0

#endif // GTEST_HAS_GLOBAL_STRING

#ifndef GTEST_HAS_STD_WSTRING
// The user didn't tell us whether ::std::wstring is available, so we need
// to figure it out.
// TODO(wan@google.com): uses autoconf to detect whether ::std::wstring
//   is available.

#define GTEST_HAS_STD_WSTRING 1

#endif // GTEST_HAS_STD_WSTRING

#ifndef GTEST_HAS_GLOBAL_WSTRING
// The user didn't tell us whether ::wstring is available, so we need
// to figure it out.
#define GTEST_HAS_GLOBAL_WSTRING                                               \
  (GTEST_HAS_STD_WSTRING && GTEST_HAS_GLOBAL_STRING)
#endif // GTEST_HAS_GLOBAL_WSTRING

// Determines whether RTTI is available.
#ifndef GTEST_HAS_RTTI
// The user didn't tell us whether RTTI is enabled, so we need to
// figure it out.

// Starting with version 4.3.2, gcc defines __GXX_RTTI iff RTTI is enabled.
#if defined(__GNUC__) && (GTEST_GCC_VER_ >= 40302)

#ifdef __GXX_RTTI
#define GTEST_HAS_RTTI 1
#else
#define GTEST_HAS_RTTI 0
#endif // __GXX_RTTI

// Clang defines __GXX_RTTI starting with version 3.0, but its manual recommends
// using has_feature instead. has_feature(cxx_rtti) is supported since 2.7, the
// first version with C++ support.
#elif defined(__clang__)

#define GTEST_HAS_RTTI __has_feature(cxx_rtti)

#else

// For all other compilers, we assume RTTI is enabled.
#define GTEST_HAS_RTTI 1

#endif

#endif // GTEST_HAS_RTTI

// It's this header's responsibility to #include <typeinfo> when RTTI
// is enabled.
#if GTEST_HAS_RTTI
#include <typeinfo>
#endif

// Determines whether Google Test can use the pthreads library.
#ifndef GTEST_HAS_PTHREAD
// To disable threading support in Google Test, add -DGTEST_HAS_PTHREAD=0
// to your compiler flags.
#define GTEST_HAS_PTHREAD 1
#endif // GTEST_HAS_PTHREAD

#if GTEST_HAS_PTHREAD
// gtest-port.h guarantees to #include <pthread.h> when GTEST_HAS_PTHREAD is
// true.
#include <pthread.h> // NOLINT

// For timespec and nanosleep, used below.
#include <time.h> // NOLINT
#endif

// Determines whether Google Test can use tr1/tuple.  You can define
// this macro to 0 to prevent Google Test from using tuple (any
// feature depending on tuple with be disabled in this mode).
#ifndef GTEST_HAS_TR1_TUPLE
// The user didn't tell us not to do it, so we assume it's OK.
#define GTEST_HAS_TR1_TUPLE 1
#endif // GTEST_HAS_TR1_TUPLE

// Determines whether Google Test's own tr1 tuple implementation
// should be used.
#ifndef GTEST_USE_OWN_TR1_TUPLE
// The user didn't tell us, so we need to figure it out.

// We use our own TR1 tuple if we aren't sure the user has an
// implementation of it already.  libc++ only provides std::tuple in
// C++11 mode, and it can be used with some compilers that define __GNUC__.
#if (defined(__GNUC__) && !defined(__CUDACC__) && (GTEST_GCC_VER_ >= 40000) && \
     !defined(_LIBCPP_VERSION))
#define GTEST_ENV_HAS_TR1_TUPLE_ 1
#endif

// C++11 specifies that <tuple> provides std::tuple. Use that if gtest is used
// in C++11 mode and libstdc++ isn't very old.
#if GTEST_LANG_CXX11 && (!defined(__GLIBCXX__) || __GLIBCXX__ > 20110325)
#define GTEST_ENV_HAS_STD_TUPLE_ 1
#endif

#if GTEST_ENV_HAS_TR1_TUPLE_ || GTEST_ENV_HAS_STD_TUPLE_
#define GTEST_USE_OWN_TR1_TUPLE 0
#else
#define GTEST_USE_OWN_TR1_TUPLE 1
#endif

#endif // GTEST_USE_OWN_TR1_TUPLE

// To avoid conditional compilation everywhere, we make it
// gtest-port.h's responsibility to #include the header implementing
// tr1/tuple.
#if GTEST_HAS_TR1_TUPLE

#if GTEST_USE_OWN_TR1_TUPLE
#include "gtest/internal/gtest-tuple.h"
#elif GTEST_ENV_HAS_STD_TUPLE_
#include <tuple>
// C++11 puts its tuple into the ::std namespace rather than
// ::std::tr1.  gtest expects tuple to live in ::std::tr1, so put it there.
// This causes undefined behavior, but supported compilers react in
// the way we intend.
namespace std {
namespace tr1 {
using ::std::get;
using ::std::make_tuple;
using ::std::tuple;
using ::std::tuple_element;
using ::std::tuple_size;
} // namespace tr1
} // namespace std

#elif defined(__GNUC__) && (GTEST_GCC_VER_ >= 40000)
// GCC 4.0+ implements tr1/tuple in the <tr1/tuple> header.  This does
// not conform to the TR1 spec, which requires the header to be <tuple>.

#if !GTEST_HAS_RTTI && GTEST_GCC_VER_ < 40302
// Until version 4.3.2, gcc has a bug that causes <tr1/functional>,
// which is #included by <tr1/tuple>, to not compile when RTTI is
// disabled.  _TR1_FUNCTIONAL is the header guard for
// <tr1/functional>.  Hence the following #define is a hack to prevent
// <tr1/functional> from being included.
#define _TR1_FUNCTIONAL 1
#include <tr1/tuple>
#undef _TR1_FUNCTIONAL // Allows the user to #include
// <tr1/functional> if he chooses to.
#else
#include <tr1/tuple> // NOLINT
#endif               // !GTEST_HAS_RTTI && GTEST_GCC_VER_ < 40302

#else
// If the compiler is not GCC 4.0+, we assume the user is using a
// spec-conforming TR1 implementation.
#include <tuple> // NOLINT
#endif           // GTEST_USE_OWN_TR1_TUPLE

#endif // GTEST_HAS_TR1_TUPLE

// Determines whether clone(2) is supported.
// Usually it will only be available on Linux, excluding
// Linux on the Itanium architecture.
// Also see http://linux.die.net/man/2/clone.
#ifndef GTEST_HAS_CLONE
// The user didn't tell us, so we need to figure it out.

#if GTEST_OS_LINUX && !defined(__ia64__)
#define GTEST_HAS_CLONE 1
#else
#define GTEST_HAS_CLONE 0
#endif // GTEST_OS_LINUX && !defined(__ia64__)

#endif // GTEST_HAS_CLONE

// Determines whether to support stream redirection. This is used to test
// output correctness and to implement death tests.
#ifndef GTEST_HAS_STREAM_REDIRECTION
// By default, we assume that stream redirection is supported.
#define GTEST_HAS_STREAM_REDIRECTION 1
#endif // GTEST_HAS_STREAM_REDIRECTION

// Determines whether to support death tests.
#if GTEST_OS_LINUX
#define GTEST_HAS_DEATH_TEST 1
#include <vector> // NOLINT
#endif

// All compilers we care about are adequate for supporting
// value-parameterized tests.
#define GTEST_HAS_PARAM_TEST 1

// Determines whether to support type-driven tests.

// Typed tests need <typeinfo> and variadic macros.
#if defined(__GNUC__) || defined(__clang__)
#define GTEST_HAS_TYPED_TEST 1
#define GTEST_HAS_TYPED_TEST_P 1
#endif

// Determines whether to support Combine(). This only makes sense when
// value-parameterized tests are enabled.
#if GTEST_HAS_PARAM_TEST && GTEST_HAS_TR1_TUPLE
#define GTEST_HAS_COMBINE 1
#endif

// Determines whether the system compiler uses UTF-16 for encoding wide strings.
#define GTEST_WIDE_STRING_USES_UTF16_ 0

// Determines whether test results can be streamed to a socket.
#if GTEST_OS_LINUX
#define GTEST_CAN_STREAM_RESULTS_ 1
#endif

// Defines some utility macros.

// The GNU compiler emits a warning if nested "if" statements are followed by
// an "else" statement and braces are not used to explicitly disambiguate the
// "else" binding.  This leads to problems with code like:
//
//   if (gate)
//     ASSERT_*(condition) << "Some message";
//
// The "switch (0) case 0:" idiom is used to suppress this.
#ifdef __INTEL_COMPILER
#define GTEST_AMBIGUOUS_ELSE_BLOCKER_
#else
#define GTEST_AMBIGUOUS_ELSE_BLOCKER_                                          \
  switch (0)                                                                   \
  case 0:                                                                      \
  default: // NOLINT
#endif

// Use this annotation at the end of a struct/class definition to
// prevent the compiler from optimizing away instances that are never
// used.  This is useful when all interesting logic happens inside the
// c'tor and / or d'tor.  Example:
//
//   struct Foo {
//     Foo() { ... }
//   } GTEST_ATTRIBUTE_UNUSED_;
//
// Also use it after a variable or parameter declaration to tell the
// compiler the variable/parameter does not have to be used.
#if defined(__GNUC__) && !defined(COMPILER_ICC)
#define GTEST_ATTRIBUTE_UNUSED_ __attribute__((unused))
#else
#define GTEST_ATTRIBUTE_UNUSED_
#endif

// A macro to disallow operator=
// This should be used in the private: declarations for a class.
#define GTEST_DISALLOW_ASSIGN_(type) void operator=(type const &)

// A macro to disallow copy constructor and operator=
// This should be used in the private: declarations for a class.
#define GTEST_DISALLOW_COPY_AND_ASSIGN_(type)                                  \
  type(type const &);                                                          \
  GTEST_DISALLOW_ASSIGN_(type)

// Tell the compiler to warn about unused return values for functions declared
// with this macro.  The macro should be used on function declarations
// following the argument list:
//
//   Sprocket* AllocateSprocket() GTEST_MUST_USE_RESULT_;
#if defined(__GNUC__) && (GTEST_GCC_VER_ >= 30400) && !defined(COMPILER_ICC)
#define GTEST_MUST_USE_RESULT_ __attribute__((warn_unused_result))
#else
#define GTEST_MUST_USE_RESULT_
#endif // __GNUC__ && (GTEST_GCC_VER_ >= 30400) && !COMPILER_ICC

// Determine whether the compiler supports Structured Exception Handling.
#ifndef GTEST_HAS_SEH
// The user didn't tell us, so we need to figure it out.

// Assume no SEH.
#define GTEST_HAS_SEH 0

#endif // GTEST_HAS_SEH

#ifndef GTEST_API_
#define GTEST_API_
#endif

#ifdef __GNUC__
// Ask the compiler to never inline a given function.
#define GTEST_NO_INLINE_ __attribute__((noinline))
#else
#define GTEST_NO_INLINE_
#endif

// _LIBCPP_VERSION is defined by the libc++ library from the LLVM project.
#if defined(__GLIBCXX__) || defined(_LIBCPP_VERSION)
#define GTEST_HAS_CXXABI_H_ 1
#else
#define GTEST_HAS_CXXABI_H_ 0
#endif

namespace testing {

class Message;

namespace internal {

// A secret type that Google Test users don't know about.  It has no
// definition on purpose.  Therefore it's impossible to create a
// Secret object, which is what we want.
class Secret;

// The GTEST_COMPILE_ASSERT_ macro can be used to verify that a compile time
// expression is true. For example, you could use it to verify the
// size of a static array:
//
//   GTEST_COMPILE_ASSERT_(ARRAYSIZE(content_type_names) == CONTENT_NUM_TYPES,
//                         content_type_names_incorrect_size);
//
// or to make sure a struct is smaller than a certain size:
//
//   GTEST_COMPILE_ASSERT_(sizeof(foo) < 128, foo_too_large);
//
// The second argument to the macro is the name of the variable. If
// the expression is false, most compilers will issue a warning/error
// containing the name of the variable.

template <bool> struct CompileAssert {};

#define GTEST_COMPILE_ASSERT_(expr, msg)                                       \
  typedef ::testing::internal::CompileAssert<(static_cast<bool>(expr))>        \
      msg[static_cast<bool>(expr) ? 1 : -1] GTEST_ATTRIBUTE_UNUSED_

// Implementation details of GTEST_COMPILE_ASSERT_:
//
// - GTEST_COMPILE_ASSERT_ works by defining an array type that has -1
//   elements (and thus is invalid) when the expression is false.
//
// - The simpler definition
//
//    #define GTEST_COMPILE_ASSERT_(expr, msg) typedef char msg[(expr) ? 1 : -1]
//
//   does not work, as gcc supports variable-length arrays whose sizes
//   are determined at run-time (this is gcc's extension and not part
//   of the C++ standard).  As a result, gcc fails to reject the
//   following code with the simple definition:
//
//     int foo;
//     GTEST_COMPILE_ASSERT_(foo, msg); // not supposed to compile as foo is
//                                      // not a compile-time constant.
//
// - By using the type CompileAssert<(bool(expr))>, we ensures that
//   expr is a compile-time constant.  (Template arguments must be
//   determined at compile-time.)
//
// - The outter parentheses in CompileAssert<(bool(expr))> are necessary
//   to work around a bug in gcc 3.4.4 and 4.0.1.  If we had written
//
//     CompileAssert<bool(expr)>
//
//   instead, these compilers will refuse to compile
//
//     GTEST_COMPILE_ASSERT_(5 > 0, some_message);
//
//   (They seem to think the ">" in "5 > 0" marks the end of the
//   template argument list.)
//
// - The array size is (bool(expr) ? 1 : -1), instead of simply
//
//     ((expr) ? 1 : -1).
//
//   This is to avoid running into a bug in MS VC 7.1, which
//   causes ((0.0) ? 1 : -1) to incorrectly evaluate to 1.

// StaticAssertTypeEqHelper is used by StaticAssertTypeEq defined in gtest.h.
//
// This template is declared, but intentionally undefined.
template <typename T1, typename T2> struct StaticAssertTypeEqHelper;

template <typename T> struct StaticAssertTypeEqHelper<T, T> {};

#if GTEST_HAS_GLOBAL_STRING
typedef ::string string;
#else
typedef ::std::string string;
#endif // GTEST_HAS_GLOBAL_STRING

#if GTEST_HAS_GLOBAL_WSTRING
typedef ::wstring wstring;
#elif GTEST_HAS_STD_WSTRING
typedef ::std::wstring wstring;
#endif // GTEST_HAS_GLOBAL_WSTRING

// A helper for suppressing warnings on constant condition.  It just
// returns 'condition'.
GTEST_API_ bool IsTrue(bool condition);

// Defines scoped_ptr.

// This implementation of scoped_ptr is PARTIAL - it only contains
// enough stuff to satisfy Google Test's need.
template <typename T> class scoped_ptr {
public:
  typedef T element_type;

  explicit scoped_ptr(T *p = NULL) : ptr_(p) {}
  ~scoped_ptr() { reset(); }

  T &operator*() const { return *ptr_; }
  T *operator->() const { return ptr_; }
  T *get() const { return ptr_; }

  T *release() {
    T *const ptr = ptr_;
    ptr_ = NULL;
    return ptr;
  }

  void reset(T *p = NULL) {
    if (p != ptr_) {
      if (IsTrue(sizeof(T) > 0)) { // Makes sure T is a complete type.
        delete ptr_;
      }
      ptr_ = p;
    }
  }

private:
  T *ptr_;

  GTEST_DISALLOW_COPY_AND_ASSIGN_(scoped_ptr);
};

// Defines RE.

// A simple C++ wrapper for <regex.h>.  It uses the POSIX Extended
// Regular Expression syntax.
class GTEST_API_ RE {
public:
  // A copy constructor is required by the Standard to initialize object
  // references from r-values.
  RE(const RE &other) { Init(other.pattern()); }

  // Constructs an RE from a string.
  RE(const ::std::string &regex) { Init(regex.c_str()); } // NOLINT

#if GTEST_HAS_GLOBAL_STRING

  RE(const ::string &regex) { Init(regex.c_str()); } // NOLINT

#endif // GTEST_HAS_GLOBAL_STRING

  RE(const char *regex) { Init(regex); } // NOLINT
  ~RE();

  // Returns the string representation of the regex.
  const char *pattern() const { return pattern_; }

  // FullMatch(str, re) returns true iff regular expression re matches
  // the entire str.
  // PartialMatch(str, re) returns true iff regular expression re
  // matches a substring of str (including str itself).
  //
  // TODO(wan@google.com): make FullMatch() and PartialMatch() work
  // when str contains NUL characters.
  static bool FullMatch(const ::std::string &str, const RE &re) {
    return FullMatch(str.c_str(), re);
  }
  static bool PartialMatch(const ::std::string &str, const RE &re) {
    return PartialMatch(str.c_str(), re);
  }

#if GTEST_HAS_GLOBAL_STRING

  static bool FullMatch(const ::string &str, const RE &re) {
    return FullMatch(str.c_str(), re);
  }
  static bool PartialMatch(const ::string &str, const RE &re) {
    return PartialMatch(str.c_str(), re);
  }

#endif // GTEST_HAS_GLOBAL_STRING

  static bool FullMatch(const char *str, const RE &re);
  static bool PartialMatch(const char *str, const RE &re);

private:
  void Init(const char *regex);

  // We use a const char* instead of an std::string, as Google Test used to be
  // used where std::string is not available.  TODO(wan@google.com): change to
  // std::string.
  const char *pattern_;
  bool is_valid_;

#if GTEST_USES_POSIX_RE

  regex_t full_regex_;    // For FullMatch().
  regex_t partial_regex_; // For PartialMatch().

#else // GTEST_USES_SIMPLE_RE

  const char *full_pattern_; // For FullMatch();

#endif

  GTEST_DISALLOW_ASSIGN_(RE);
};

// Formats a source file path and a line number as they would appear
// in an error message from the compiler used to compile this code.
GTEST_API_ ::std::string FormatFileLocation(const char *file, int line);

// Formats a file location for compiler-independent XML output.
// Although this function is not platform dependent, we put it next to
// FormatFileLocation in order to contrast the two functions.
GTEST_API_ ::std::string FormatCompilerIndependentFileLocation(const char *file,
                                                               int line);

// Defines logging utilities:
//   GTEST_LOG_(severity) - logs messages at the specified severity level. The
//                          message itself is streamed into the macro.
//   LogToStderr()  - directs all log messages to stderr.
//   FlushInfoLog() - flushes informational log messages.

enum GTestLogSeverity { GTEST_INFO, GTEST_WARNING, GTEST_ERROR, GTEST_FATAL };

// Formats log entry severity, provides a stream object for streaming the
// log message, and terminates the message with a newline when going out of
// scope.
class GTEST_API_ GTestLog {
public:
  GTestLog(GTestLogSeverity severity, const char *file, int line);

  // Flushes the buffers and, if severity is GTEST_FATAL, aborts the program.
  ~GTestLog();

  ::std::ostream &GetStream() { return ::std::cerr; }

private:
  const GTestLogSeverity severity_;

  GTEST_DISALLOW_COPY_AND_ASSIGN_(GTestLog);
};

#define GTEST_LOG_(severity)                                                   \
  ::testing::internal::GTestLog(::testing::internal::GTEST_##severity,         \
                                __FILE__, __LINE__)                            \
      .GetStream()

inline void LogToStderr() {}
inline void FlushInfoLog() { fflush(NULL); }

// INTERNAL IMPLEMENTATION - DO NOT USE.
//
// GTEST_CHECK_ is an all-mode assert. It aborts the program if the condition
// is not satisfied.
//  Synopsys:
//    GTEST_CHECK_(boolean_condition);
//     or
//    GTEST_CHECK_(boolean_condition) << "Additional message";
//
//    This checks the condition and if the condition is not satisfied
//    it prints message about the condition violation, including the
//    condition itself, plus additional message streamed into it, if any,
//    and then it aborts the program. It aborts the program irrespective of
//    whether it is built in the debug mode or not.
#define GTEST_CHECK_(condition)                                                \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                                \
  if (::testing::internal::IsTrue(condition))                                  \
    ;                                                                          \
  else                                                                         \
    GTEST_LOG_(FATAL) << "Condition " #condition " failed. "

// An all-mode assert to verify that the given POSIX-style function
// call returns 0 (indicating success).  Known limitation: this
// doesn't expand to a balanced 'if' statement, so enclose the macro
// in {} if you need to use it as the only statement in an 'if'
// branch.
#define GTEST_CHECK_POSIX_SUCCESS_(posix_call)                                 \
  if (const int gtest_error = (posix_call))                                    \
  GTEST_LOG_(FATAL) << #posix_call << "failed with error " << gtest_error

// INTERNAL IMPLEMENTATION - DO NOT USE IN USER CODE.
//
// Use ImplicitCast_ as a safe version of static_cast for upcasting in
// the type hierarchy (e.g. casting a Foo* to a SuperclassOfFoo* or a
// const Foo*).  When you use ImplicitCast_, the compiler checks that
// the cast is safe.  Such explicit ImplicitCast_s are necessary in
// surprisingly many situations where C++ demands an exact type match
// instead of an argument type convertable to a target type.
//
// The syntax for using ImplicitCast_ is the same as for static_cast:
//
//   ImplicitCast_<ToType>(expr)
//
// ImplicitCast_ would have been part of the C++ standard library,
// but the proposal was submitted too late.  It will probably make
// its way into the language in the future.
//
// This relatively ugly name is intentional. It prevents clashes with
// similar functions users may have (e.g., implicit_cast). The internal
// namespace alone is not enough because the function can be found by ADL.
template <typename To> inline To ImplicitCast_(To x) { return x; }

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use ImplicitCast_<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.
//
// This relatively ugly name is intentional. It prevents clashes with
// similar functions users may have (e.g., down_cast). The internal
// namespace alone is not enough because the function can be found by ADL.
template <typename To, typename From> // use like this: DownCast_<T*>(foo);
inline To DownCast_(From *f) {        // so we only accept pointers
  // Ensures that To is a sub-type of From *.  This test is here only
  // for compile-time type checking, and has no overhead in an
  // optimized build at run-time, as it will be optimized away
  // completely.
  if (false) {
    const To to = NULL;
    ::testing::internal::ImplicitCast_<From *>(to);
  }

#if GTEST_HAS_RTTI
  // RTTI: debug mode only!
  GTEST_CHECK_(f == NULL || dynamic_cast<To>(f) != NULL);
#endif
  return static_cast<To>(f);
}

// Downcasts the pointer of type Base to Derived.
// Derived must be a subclass of Base. The parameter MUST
// point to a class of type Derived, not any subclass of it.
// When RTTI is available, the function performs a runtime
// check to enforce this.
template <class Derived, class Base>
Derived *CheckedDowncastToActualType(Base *base) {
#if GTEST_HAS_RTTI
  GTEST_CHECK_(typeid(*base) == typeid(Derived));
  return dynamic_cast<Derived *>(base); // NOLINT
#else
  return static_cast<Derived *>(base); // Poor man's downcast.
#endif
}

#if GTEST_HAS_STREAM_REDIRECTION

// Defines the stderr capturer:
//   CaptureStdout     - starts capturing stdout.
//   GetCapturedStdout - stops capturing stdout and returns the captured string.
//   CaptureStderr     - starts capturing stderr.
//   GetCapturedStderr - stops capturing stderr and returns the captured string.
//
GTEST_API_ void CaptureStdout();
GTEST_API_ std::string GetCapturedStdout();
GTEST_API_ void CaptureStderr();
GTEST_API_ std::string GetCapturedStderr();

#endif // GTEST_HAS_STREAM_REDIRECTION

#if GTEST_HAS_DEATH_TEST

const ::std::vector<testing::internal::string> &GetInjectableArgvs();
void SetInjectableArgvs(
    const ::std::vector<testing::internal::string> *new_argvs);

// A copy of all command line arguments.  Set by InitGoogleTest().
extern ::std::vector<testing::internal::string> g_argvs;

#endif // GTEST_HAS_DEATH_TEST

// Defines synchronization primitives.

#if GTEST_HAS_PTHREAD

// Sleeps for (roughly) n milli-seconds.  This function is only for
// testing Google Test's own constructs.  Don't use it in user tests,
// either directly or indirectly.
inline void SleepMilliseconds(int n) {
  const timespec time = {
      0,                 // 0 seconds.
      n * 1000L * 1000L, // And n ms.
  };
  nanosleep(&time, NULL);
}

// Allows a controller thread to pause execution of newly created
// threads until notified.  Instances of this class must be created
// and destroyed in the controller thread.
//
// This class is only for testing Google Test's own constructs. Do not
// use it in user tests, either directly or indirectly.
class Notification {
public:
  Notification() : notified_(false) {
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_init(&mutex_, NULL));
  }
  ~Notification() { pthread_mutex_destroy(&mutex_); }

  // Notifies all threads created with this notification to start. Must
  // be called from the controller thread.
  void Notify() {
    pthread_mutex_lock(&mutex_);
    notified_ = true;
    pthread_mutex_unlock(&mutex_);
  }

  // Blocks until the controller thread notifies. Must be called from a test
  // thread.
  void WaitForNotification() {
    for (;;) {
      pthread_mutex_lock(&mutex_);
      const bool notified = notified_;
      pthread_mutex_unlock(&mutex_);
      if (notified)
        break;
      SleepMilliseconds(10);
    }
  }

private:
  pthread_mutex_t mutex_;
  bool notified_;

  GTEST_DISALLOW_COPY_AND_ASSIGN_(Notification);
};

// As a C-function, ThreadFuncWithCLinkage cannot be templated itself.
// Consequently, it cannot select a correct instantiation of ThreadWithParam
// in order to call its Run(). Introducing ThreadWithParamBase as a
// non-templated base class for ThreadWithParam allows us to bypass this
// problem.
class ThreadWithParamBase {
public:
  virtual ~ThreadWithParamBase() {}
  virtual void Run() = 0;
};

// pthread_create() accepts a pointer to a function type with the C linkage.
// According to the Standard (7.5/1), function types with different linkages
// are different even if they are otherwise identical.  Some compilers treat
// them as different types.  Since class methods
// cannot be defined with C-linkage we need to define a free C-function to
// pass into pthread_create().
extern "C" inline void *ThreadFuncWithCLinkage(void *thread) {
  static_cast<ThreadWithParamBase *>(thread)->Run();
  return NULL;
}

// Helper class for testing Google Test's multi-threading constructs.
// To use it, write:
//
//   void ThreadFunc(int param) { /* Do things with param */ }
//   Notification thread_can_start;
//   ...
//   // The thread_can_start parameter is optional; you can supply NULL.
//   ThreadWithParam<int> thread(&ThreadFunc, 5, &thread_can_start);
//   thread_can_start.Notify();
//
// These classes are only for testing Google Test's own constructs. Do
// not use them in user tests, either directly or indirectly.
template <typename T> class ThreadWithParam : public ThreadWithParamBase {
public:
  typedef void (*UserThreadFunc)(T);

  ThreadWithParam(UserThreadFunc func, T param, Notification *thread_can_start)
      : func_(func), param_(param), thread_can_start_(thread_can_start),
        finished_(false) {
    ThreadWithParamBase *const base = this;
    // The thread can be created only after all fields except thread_
    // have been initialized.
    GTEST_CHECK_POSIX_SUCCESS_(
        pthread_create(&thread_, 0, &ThreadFuncWithCLinkage, base));
  }
  ~ThreadWithParam() { Join(); }

  void Join() {
    if (!finished_) {
      GTEST_CHECK_POSIX_SUCCESS_(pthread_join(thread_, 0));
      finished_ = true;
    }
  }

  virtual void Run() {
    if (thread_can_start_ != NULL)
      thread_can_start_->WaitForNotification();
    func_(param_);
  }

private:
  const UserThreadFunc func_; // User-supplied thread function.
  const T param_;             // User-supplied parameter to the thread function.
  // When non-NULL, used to block execution until the controller thread
  // notifies.
  Notification *const thread_can_start_;
  bool finished_;    // true iff we know that the thread function has finished.
  pthread_t thread_; // The native thread object.

  GTEST_DISALLOW_COPY_AND_ASSIGN_(ThreadWithParam);
};

// MutexBase and Mutex implement mutex on pthreads-based platforms. They
// are used in conjunction with class MutexLock:
//
//   Mutex mutex;
//   ...
//   MutexLock lock(&mutex);  // Acquires the mutex and releases it at the end
//                            // of the current scope.
//
// MutexBase implements behavior for both statically and dynamically
// allocated mutexes.  Do not use MutexBase directly.  Instead, write
// the following to define a static mutex:
//
//   GTEST_DEFINE_STATIC_MUTEX_(g_some_mutex);
//
// You can forward declare a static mutex like this:
//
//   GTEST_DECLARE_STATIC_MUTEX_(g_some_mutex);
//
// To create a dynamic mutex, just define an object of type Mutex.
class MutexBase {
public:
  // Acquires this mutex.
  void Lock() {
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_lock(&mutex_));
    owner_ = pthread_self();
    has_owner_ = true;
  }

  // Releases this mutex.
  void Unlock() {
    // Since the lock is being released the owner_ field should no longer be
    // considered valid. We don't protect writing to has_owner_ here, as it's
    // the caller's responsibility to ensure that the current thread holds the
    // mutex when this is called.
    has_owner_ = false;
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_unlock(&mutex_));
  }

  // Does nothing if the current thread holds the mutex. Otherwise, crashes
  // with high probability.
  void AssertHeld() const {
    GTEST_CHECK_(has_owner_ && pthread_equal(owner_, pthread_self()))
        << "The current thread is not holding the mutex @" << this;
  }

  // A static mutex may be used before main() is entered.  It may even
  // be used before the dynamic initialization stage.  Therefore we
  // must be able to initialize a static mutex object at link time.
  // This means MutexBase has to be a POD and its member variables
  // have to be public.
public:
  pthread_mutex_t mutex_; // The underlying pthread mutex.
  // has_owner_ indicates whether the owner_ field below contains a valid thread
  // ID and is therefore safe to inspect (e.g., to use in pthread_equal()). All
  // accesses to the owner_ field should be protected by a check of this field.
  // An alternative might be to memset() owner_ to all zeros, but there's no
  // guarantee that a zero'd pthread_t is necessarily invalid or even different
  // from pthread_self().
  bool has_owner_;
  pthread_t owner_; // The thread holding the mutex.
};

// Forward-declares a static mutex.
#define GTEST_DECLARE_STATIC_MUTEX_(mutex)                                     \
  extern ::testing::internal::MutexBase mutex

// Defines and statically (i.e. at link time) initializes a static mutex.
// The initialization list here does not explicitly initialize each field,
// instead relying on default initialization for the unspecified fields. In
// particular, the owner_ field (a pthread_t) is not explicitly initialized.
// This allows initialization to work whether pthread_t is a scalar or struct.
// The flag -Wmissing-field-initializers must not be specified for this to work.
#define GTEST_DEFINE_STATIC_MUTEX_(mutex)                                      \
  ::testing::internal::MutexBase mutex = {PTHREAD_MUTEX_INITIALIZER, false}

// The Mutex class can only be used for mutexes created at runtime. It
// shares its API with MutexBase otherwise.
class Mutex : public MutexBase {
public:
  Mutex() {
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_init(&mutex_, NULL));
    has_owner_ = false;
  }
  ~Mutex() { GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_destroy(&mutex_)); }

private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(Mutex);
};

// We cannot name this class MutexLock as the ctor declaration would
// conflict with a macro named MutexLock, which is defined on some
// platforms.  Hence the typedef trick below.
class GTestMutexLock {
public:
  explicit GTestMutexLock(MutexBase *mutex) : mutex_(mutex) { mutex_->Lock(); }

  ~GTestMutexLock() { mutex_->Unlock(); }

private:
  MutexBase *const mutex_;

  GTEST_DISALLOW_COPY_AND_ASSIGN_(GTestMutexLock);
};

typedef GTestMutexLock MutexLock;

// Helpers for ThreadLocal.

// pthread_key_create() requires DeleteThreadLocalValue() to have
// C-linkage.  Therefore it cannot be templatized to access
// ThreadLocal<T>.  Hence the need for class
// ThreadLocalValueHolderBase.
class ThreadLocalValueHolderBase {
public:
  virtual ~ThreadLocalValueHolderBase() {}
};

// Called by pthread to delete thread-local data stored by
// pthread_setspecific().
extern "C" inline void DeleteThreadLocalValue(void *value_holder) {
  delete static_cast<ThreadLocalValueHolderBase *>(value_holder);
}

// Implements thread-local storage on pthreads-based systems.
//
//   // Thread 1
//   ThreadLocal<int> tl(100);  // 100 is the default value for each thread.
//
//   // Thread 2
//   tl.set(150);  // Changes the value for thread 2 only.
//   EXPECT_EQ(150, tl.get());
//
//   // Thread 1
//   EXPECT_EQ(100, tl.get());  // In thread 1, tl has the original value.
//   tl.set(200);
//   EXPECT_EQ(200, tl.get());
//
// The template type argument T must have a public copy constructor.
// In addition, the default ThreadLocal constructor requires T to have
// a public default constructor.
//
// An object managed for a thread by a ThreadLocal instance is deleted
// when the thread exits.  Or, if the ThreadLocal instance dies in
// that thread, when the ThreadLocal dies.  It's the user's
// responsibility to ensure that all other threads using a ThreadLocal
// have exited when it dies, or the per-thread objects for those
// threads will not be deleted.
//
// Google Test only uses global ThreadLocal objects.  That means they
// will die after main() has returned.  Therefore, no per-thread
// object managed by Google Test will be leaked as long as all threads
// using Google Test have exited when main() returns.
template <typename T> class ThreadLocal {
public:
  ThreadLocal() : key_(CreateKey()), default_() {}
  explicit ThreadLocal(const T &value) : key_(CreateKey()), default_(value) {}

  ~ThreadLocal() {
    // Destroys the managed object for the current thread, if any.
    DeleteThreadLocalValue(pthread_getspecific(key_));

    // Releases resources associated with the key.  This will *not*
    // delete managed objects for other threads.
    GTEST_CHECK_POSIX_SUCCESS_(pthread_key_delete(key_));
  }

  T *pointer() { return GetOrCreateValue(); }
  const T *pointer() const { return GetOrCreateValue(); }
  const T &get() const { return *pointer(); }
  void set(const T &value) { *pointer() = value; }

private:
  // Holds a value of type T.
  class ValueHolder : public ThreadLocalValueHolderBase {
  public:
    explicit ValueHolder(const T &value) : value_(value) {}

    T *pointer() { return &value_; }

  private:
    T value_;
    GTEST_DISALLOW_COPY_AND_ASSIGN_(ValueHolder);
  };

  static pthread_key_t CreateKey() {
    pthread_key_t key;
    // When a thread exits, DeleteThreadLocalValue() will be called on
    // the object managed for that thread.
    GTEST_CHECK_POSIX_SUCCESS_(
        pthread_key_create(&key, &DeleteThreadLocalValue));
    return key;
  }

  T *GetOrCreateValue() const {
    ThreadLocalValueHolderBase *const holder =
        static_cast<ThreadLocalValueHolderBase *>(pthread_getspecific(key_));
    if (holder != NULL) {
      return CheckedDowncastToActualType<ValueHolder>(holder)->pointer();
    }

    ValueHolder *const new_holder = new ValueHolder(default_);
    ThreadLocalValueHolderBase *const holder_base = new_holder;
    GTEST_CHECK_POSIX_SUCCESS_(pthread_setspecific(key_, holder_base));
    return new_holder->pointer();
  }

  // A key pthreads uses for looking up per-thread values.
  const pthread_key_t key_;
  const T default_; // The default value for each thread.

  GTEST_DISALLOW_COPY_AND_ASSIGN_(ThreadLocal);
};

#define GTEST_IS_THREADSAFE 1

#else // GTEST_HAS_PTHREAD

// A dummy implementation of synchronization primitives (mutex, lock,
// and thread-local variable).  Necessary for compiling Google Test where
// mutex is not supported - using Google Test in multiple threads is not
// supported on such platforms.

class Mutex {
public:
  Mutex() {}
  void Lock() {}
  void Unlock() {}
  void AssertHeld() const {}
};

#define GTEST_DECLARE_STATIC_MUTEX_(mutex)                                     \
  extern ::testing::internal::Mutex mutex

#define GTEST_DEFINE_STATIC_MUTEX_(mutex) ::testing::internal::Mutex mutex

class GTestMutexLock {
public:
  explicit GTestMutexLock(Mutex *) {} // NOLINT
};

typedef GTestMutexLock MutexLock;

template <typename T> class ThreadLocal {
public:
  ThreadLocal() : value_() {}
  explicit ThreadLocal(const T &value) : value_(value) {}
  T *pointer() { return &value_; }
  const T *pointer() const { return &value_; }
  const T &get() const { return value_; }
  void set(const T &value) { value_ = value; }

private:
  T value_;
};

// The above synchronization primitives have dummy implementations.
// Therefore Google Test is not thread-safe.
#define GTEST_IS_THREADSAFE 0

#endif // GTEST_HAS_PTHREAD

// Returns the number of threads running in the process, or 0 to indicate that
// we cannot detect it.
GTEST_API_ size_t GetThreadCount();

// We can compare NULL in this configuration.
#define GTEST_CAN_COMPARE_NULL 1

template <bool bool_value> struct bool_constant {
  typedef bool_constant<bool_value> type;
  static const bool value = bool_value;
};
template <bool bool_value> const bool bool_constant<bool_value>::value;

typedef bool_constant<false> false_type;
typedef bool_constant<true> true_type;

template <typename T> struct is_pointer : public false_type {};

template <typename T> struct is_pointer<T *> : public true_type {};

template <typename Iterator> struct IteratorTraits {
  typedef typename Iterator::value_type value_type;
};

template <typename T> struct IteratorTraits<T *> {
  typedef T value_type;
};

template <typename T> struct IteratorTraits<const T *> {
  typedef T value_type;
};

#define GTEST_PATH_SEP_ "/"
#define GTEST_HAS_ALT_PATH_SEP_ 0
typedef long long BiggestInt; // NOLINT

// Utilities for char.

// isspace(int ch) and friends accept an unsigned char or EOF.  char
// may be signed, depending on the compiler (or compiler flags).
// Therefore we need to cast a char to unsigned char before calling
// isspace(), etc.

inline bool IsAlpha(char ch) {
  return isalpha(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsAlNum(char ch) {
  return isalnum(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsDigit(char ch) {
  return isdigit(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsLower(char ch) {
  return islower(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsSpace(char ch) {
  return isspace(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsUpper(char ch) {
  return isupper(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsXDigit(char ch) {
  return isxdigit(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsXDigit(wchar_t ch) {
  const unsigned char low_byte = static_cast<unsigned char>(ch);
  return ch == low_byte && isxdigit(low_byte) != 0;
}

inline char ToLower(char ch) {
  return static_cast<char>(tolower(static_cast<unsigned char>(ch)));
}
inline char ToUpper(char ch) {
  return static_cast<char>(toupper(static_cast<unsigned char>(ch)));
}

// The testing::internal::posix namespace holds wrappers for common
// POSIX functions. These wrappers hide platform differences. Since some
// compilers define these standard functions as macros, the wrapper cannot
// have the same name as the wrapped function.

namespace posix {

typedef struct stat StatStruct;

inline int FileNo(FILE *file) { return fileno(file); }
inline int IsATTY(int fd) { return isatty(fd); }
inline int Stat(const char *path, StatStruct *buf) { return stat(path, buf); }
inline int StrCaseCmp(const char *s1, const char *s2) {
  return strcasecmp(s1, s2);
}
inline char *StrDup(const char *src) { return strdup(src); }
inline int RmDir(const char *dir) { return rmdir(dir); }
inline bool IsDir(const StatStruct &st) { return S_ISDIR(st.st_mode); }

inline const char *StrNCpy(char *dest, const char *src, size_t n) {
  return strncpy(dest, src, n);
}

inline int ChDir(const char *dir) { return chdir(dir); }
inline FILE *FOpen(const char *path, const char *mode) {
  return fopen(path, mode);
}
inline FILE *FReopen(const char *path, const char *mode, FILE *stream) {
  return freopen(path, mode, stream);
}
inline FILE *FDOpen(int fd, const char *mode) { return fdopen(fd, mode); }
inline int FClose(FILE *fp) { return fclose(fp); }
inline int Read(int fd, void *buf, unsigned int count) {
  return static_cast<int>(read(fd, buf, count));
}
inline int Write(int fd, const void *buf, unsigned int count) {
  return static_cast<int>(write(fd, buf, count));
}
inline int Close(int fd) { return close(fd); }
inline const char *StrError(int errnum) { return strerror(errnum); }
inline const char *GetEnv(const char *name) { return getenv(name); }
inline void Abort() { abort(); }

} // namespace posix

#define GTEST_SNPRINTF_ snprintf

// The maximum number a BiggestInt can represent.  This definition
// works no matter BiggestInt is represented in one's complement or
// two's complement.
//
// We cannot rely on numeric_limits in STL, as __int64 and long long
// are not part of standard C++ and numeric_limits doesn't need to be
// defined for them.
const BiggestInt kMaxBiggestInt =
    ~(static_cast<BiggestInt>(1) << (8 * sizeof(BiggestInt) - 1));

// This template class serves as a compile-time function from size to
// type.  It maps a size in bytes to a primitive type with that
// size. e.g.
//
//   TypeWithSize<4>::UInt
//
// is typedef-ed to be unsigned int (unsigned integer made up of 4
// bytes).
//
// Such functionality should belong to STL, but I cannot find it
// there.
//
// Google Test uses this class in the implementation of floating-point
// comparison.
//
// For now it only handles UInt (unsigned int) as that's all Google Test
// needs.  Other types can be easily added in the future if need
// arises.
template <size_t size> class TypeWithSize {
public:
  // This prevents the user from using TypeWithSize<N> with incorrect
  // values of N.
  typedef void UInt;
};

// The specialization for size 4.
template <> class TypeWithSize<4> {
public:
  // unsigned int has size 4 on typical platforms.
  typedef int Int;
  typedef unsigned int UInt;
};

// The specialization for size 8.
template <> class TypeWithSize<8> {
public:
  typedef long long Int;           // NOLINT
  typedef unsigned long long UInt; // NOLINT
};

// Integer types of known sizes.
typedef TypeWithSize<4>::Int Int32;
typedef TypeWithSize<4>::UInt UInt32;
typedef TypeWithSize<8>::Int Int64;
typedef TypeWithSize<8>::UInt UInt64;
typedef TypeWithSize<8>::Int TimeInMillis; // Represents time in milliseconds.

// Utilities for command line flags and environment variables.

// Macro for referencing flags.
#define GTEST_FLAG(name) FLAGS_gtest_##name

// Macros for declaring flags.
#define GTEST_DECLARE_bool_(name) GTEST_API_ extern bool GTEST_FLAG(name)
#define GTEST_DECLARE_int32_(name)                                             \
  GTEST_API_ extern ::testing::internal::Int32 GTEST_FLAG(name)
#define GTEST_DECLARE_string_(name)                                            \
  GTEST_API_ extern ::std::string GTEST_FLAG(name)

// Macros for defining flags.
#define GTEST_DEFINE_bool_(name, default_val, doc)                             \
  GTEST_API_ bool GTEST_FLAG(name) = (default_val)
#define GTEST_DEFINE_int32_(name, default_val, doc)                            \
  GTEST_API_ ::testing::internal::Int32 GTEST_FLAG(name) = (default_val)
#define GTEST_DEFINE_string_(name, default_val, doc)                           \
  GTEST_API_ ::std::string GTEST_FLAG(name) = (default_val)

// Thread annotations
#define GTEST_EXCLUSIVE_LOCK_REQUIRED_(locks)
#define GTEST_LOCK_EXCLUDED_(locks)

// Parses 'str' for a 32-bit signed integer.  If successful, writes the result
// to *value and returns true; otherwise leaves *value unchanged and returns
// false.
// TODO(chandlerc): Find a better way to refactor flag and environment parsing
// out of both gtest-port.cc and gtest.cc to avoid exporting this utility
// function.
bool ParseInt32(const Message &src_text, const char *str, Int32 *value);

// Parses a bool/Int32/string from the environment variable
// corresponding to the given Google Test flag.
bool BoolFromGTestEnv(const char *flag, bool default_val);
GTEST_API_ Int32 Int32FromGTestEnv(const char *flag, Int32 default_val);
const char *StringFromGTestEnv(const char *flag, const char *default_val);

} // namespace internal
} // namespace testing

#endif // GTEST_INCLUDE_GTEST_INTERNAL_GTEST_PORT_H_
