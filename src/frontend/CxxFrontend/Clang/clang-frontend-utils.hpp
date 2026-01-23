#ifndef _CLANG_FRONTEND_UTILS_HPP_
#define _CLANG_FRONTEND_UTILS_HPP_

#include <algorithm>
#include <string>
#include <vector>

#include <clang/Basic/TargetOptions.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

static inline bool
isTargetFeatureSpecified(const std::vector<std::string> &features,
                         llvm::StringRef feature_name) {
  for (const auto &feature : features) {
    llvm::StringRef feature_ref(feature);
    if (feature_ref.size() != feature_name.size() + 1) {
      continue;
    }
    if (feature_ref.front() != '+' && feature_ref.front() != '-') {
      continue;
    }
    if (feature_ref.drop_front() == feature_name) {
      return true;
    }
  }
  return false;
}

static inline void
ensureX86BaselineTargetFeatures(clang::TargetOptions &target_opts) {
  if (target_opts.Triple.empty()) {
    target_opts.Triple = llvm::sys::getDefaultTargetTriple();
  }

  llvm::Triple triple(target_opts.Triple);
  if (triple.getArch() != llvm::Triple::x86 &&
      triple.getArch() != llvm::Triple::x86_64) {
    return;
  }

  auto &features = target_opts.Features;
  if (!isTargetFeatureSpecified(features, "mmx")) {
    features.push_back("+mmx");
  }
  if (!isTargetFeatureSpecified(features, "sse")) {
    features.push_back("+sse");
  }
  if (!isTargetFeatureSpecified(features, "sse2")) {
    features.push_back("+sse2");
  }
}

static inline void dropRelativeIncludeDirs(std::vector<std::string> &dirs) {
  dirs.erase(std::remove_if(dirs.begin(), dirs.end(),
                            [](const std::string &dir) {
                              return !dir.empty() && dir[0] != '/';
                            }),
             dirs.end());
}

#endif
