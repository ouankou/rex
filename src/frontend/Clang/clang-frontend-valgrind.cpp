#include "sage3basic.h"

#include "clang-frontend-private.hpp"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>

#include <cstddef>
#include <unordered_map>

#if ROSE_USE_VALGRIND
#include <valgrind/memcheck.h>
#include <valgrind/valgrind.h>
#endif

void markClangAstStorageRangeDefinedForFrontend(const void *address,
                                                std::size_t size) {
#if ROSE_USE_VALGRIND
  if (!RUNNING_ON_VALGRIND || address == nullptr || size == 0) {
    return;
  }

  static thread_local std::unordered_map<const void *, std::size_t>
      marked_ranges;
  auto existing = marked_ranges.find(address);
  if (existing != marked_ranges.end()) {
    if (existing->second >= size) {
      return;
    }
    existing->second = size;
  } else {
    marked_ranges.emplace(address, size);
  }

  VALGRIND_MAKE_MEM_DEFINED(const_cast<void *>(address), size);
#else
  (void)address;
  (void)size;
#endif
}

namespace {

template <typename T> const T *markClangAstObjectDefined(const T *node) {
#if ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_MAKE_MEM_DEFINED(&node, sizeof(node));
    if (node != nullptr) {
      markClangAstStorageRangeDefinedForFrontend(node, sizeof(*node));
    }
  }
#endif
  return node;
}

template <typename T> const T &markClangValueDefined(const T &value) {
#if ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_MAKE_MEM_DEFINED(const_cast<T *>(&value), sizeof(value));
  }
#endif
  return value;
}

template <typename F>
auto readClangApiValueDefined(F &&read) -> decltype(read()) {
#if ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND) {
    VALGRIND_DISABLE_ERROR_REPORTING;
    auto value = read();
    VALGRIND_ENABLE_ERROR_REPORTING;
    markClangValueDefined(value);
    return value;
  }
#endif
  auto value = read();
  markClangValueDefined(value);
  return value;
}

void markClangDeclAttrsDefined(const clang::Decl *decl) {
#if ROSE_USE_VALGRIND
  decl = markClangAstObjectDefined(decl);
  if (!RUNNING_ON_VALGRIND || decl == nullptr) {
    return;
  }

  const bool has_attrs =
      readClangApiValueDefined([&]() { return decl->hasAttrs(); });
  if (!has_attrs) {
    return;
  }

  VALGRIND_DISABLE_ERROR_REPORTING;
  const clang::AttrVec &attrs = decl->getAttrs();
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_MAKE_MEM_DEFINED(const_cast<clang::AttrVec *>(&attrs),
                            sizeof(attrs));
  if (!attrs.empty()) {
    VALGRIND_MAKE_MEM_DEFINED(const_cast<clang::Attr **>(attrs.data()),
                              attrs.size() * sizeof(clang::Attr *));
  }
  for (const clang::Attr *attr : attrs) {
    markClangAstObjectDefined(attr);
  }
#else
  (void)decl;
#endif
}

} // namespace

bool clangDeclHasBuiltinAttrDefinedForFrontend(const clang::Decl *decl) {
  if (decl == nullptr) {
    return false;
  }

  markClangDeclAttrsDefined(decl);
  return readClangApiValueDefined(
      [&]() { return decl->hasAttr<clang::BuiltinAttr>(); });
}
