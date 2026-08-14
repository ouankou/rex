#include "sage3basic.h"

#include "clang-frontend-private.hpp"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#if ROSE_USE_VALGRIND
#include <valgrind/memcheck.h>
#include <valgrind/valgrind.h>
#endif

namespace {

#if ROSE_USE_VALGRIND
struct ClangFrontendValgrindPublicationSession {
  bool active = false;
  std::unordered_map<const void *, std::size_t> storage_ranges;
  std::array<std::unordered_set<const void *>,
             static_cast<std::size_t>(
                 ClangFrontendValgrindPublicationKind::Count)>
      objects;
};

ClangFrontendValgrindPublicationSession &
clangFrontendValgrindPublicationSession() {
  static thread_local ClangFrontendValgrindPublicationSession session;
  return session;
}

void requireActiveClangFrontendValgrindPublicationSession(
    const char *operation) {
  if (RUNNING_ON_VALGRIND != 0 &&
      !clangFrontendValgrindPublicationSession().active) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-valgrind-publication-session]: "
            "%s requires an active frontend session\n",
            operation);
    ROSE_ABORT();
  }
}
#endif

} // namespace

void beginClangFrontendValgrindPublicationSession() {
#if ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND == 0) {
    return;
  }

  ClangFrontendValgrindPublicationSession &session =
      clangFrontendValgrindPublicationSession();
  if (session.active) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-valgrind-publication-session]: "
            "cannot nest frontend sessions\n");
    ROSE_ABORT();
  }
  const bool retained_objects =
      std::any_of(session.objects.begin(), session.objects.end(),
                  [](const auto &objects) { return !objects.empty(); });
  if (!session.storage_ranges.empty() || retained_objects) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-valgrind-publication-session]: "
            "inactive frontend session retained publication state\n");
    ROSE_ABORT();
  }
  session.active = true;
#endif
}

void endClangFrontendValgrindPublicationSession() {
#if ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND == 0) {
    return;
  }

  ClangFrontendValgrindPublicationSession &session =
      clangFrontendValgrindPublicationSession();
  if (!session.active) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-valgrind-publication-session]: "
            "cannot end an inactive frontend session\n");
    ROSE_ABORT();
  }
  session.storage_ranges.clear();
  for (auto &objects : session.objects) {
    objects.clear();
  }
  session.active = false;
#endif
}

bool markClangFrontendValgrindPublicationOnce(
    ClangFrontendValgrindPublicationKind kind, const void *address) {
#if ROSE_USE_VALGRIND
  if (RUNNING_ON_VALGRIND == 0 || address == nullptr) {
    return true;
  }

  requireActiveClangFrontendValgrindPublicationSession("object publication");
  const std::size_t index = static_cast<std::size_t>(kind);
  if (index >=
      static_cast<std::size_t>(ClangFrontendValgrindPublicationKind::Count)) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[clang-valgrind-publication-session]: "
            "invalid publication kind=%zu\n",
            index);
    ROSE_ABORT();
  }
  return clangFrontendValgrindPublicationSession()
      .objects[index]
      .insert(address)
      .second;
#else
  (void)kind;
  (void)address;
  return true;
#endif
}

void markClangAstStorageRangeDefinedForFrontend(const void *address,
                                                std::size_t size) {
#if ROSE_USE_VALGRIND
  if (!RUNNING_ON_VALGRIND || address == nullptr || size == 0) {
    return;
  }

  requireActiveClangFrontendValgrindPublicationSession("storage publication");
  auto &marked_ranges =
      clangFrontendValgrindPublicationSession().storage_ranges;
  auto existing = marked_ranges.find(address);
  if (existing != marked_ranges.end()) {
    if (existing->second >= size) {
      return;
    }
    existing->second = size;
  } else {
    marked_ranges.emplace(address, size);
  }

  // Definedness is monotonic while one ASTContext is alive: Clang writes are
  // defined, lazy allocations have distinct bump-allocator addresses, and an
  // object is never freed and replaced within that context.  The explicit
  // frontend-session boundary clears every address before an ASTContext can be
  // destroyed and another allocator can reuse it.
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
