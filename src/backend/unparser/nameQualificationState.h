#ifndef REX_NAME_QUALIFICATION_STATE_H
#define REX_NAME_QUALIFICATION_STATE_H

// Indexes that a name-qualification traversal reads and writes as it walks a
// file.  Each index writes to its own container and can read through to a
// completed base container: a qualification session freezes one file's
// end-of-walk indexes as the base of every later traversal over that file, so
// each traversal sees exactly the completed walk plus its own additions and
// the base is never modified.

#include "Cxx_Grammar.h"

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

class NameQualificationNodeSet {
public:
  explicit NameQualificationNodeSet(SgUnorderedNodeSet &own,
                                    const SgUnorderedNodeSet *base = nullptr)
      : own_(own), base_(base) {}
  NameQualificationNodeSet(const NameQualificationNodeSet &) = delete;
  NameQualificationNodeSet &
  operator=(const NameQualificationNodeSet &) = delete;

  bool contains(SgNode *node) const {
    return own_.find(node) != own_.end() ||
           (base_ != nullptr && base_->find(node) != base_->end());
  }
  void insert(SgNode *node) {
    if (base_ == nullptr || base_->find(node) == base_->end()) {
      own_.insert(node);
    }
  }

private:
  SgUnorderedNodeSet &own_;
  const SgUnorderedNodeSet *base_;
};

// Qualifier strings keyed by reference node.  Erasing a key that only the base
// holds hides it from this index.
class NameQualificationNodeStrings {
public:
  explicit NameQualificationNodeStrings(
      SgUnorderedMapNodeToString &own,
      const SgUnorderedMapNodeToString *base = nullptr)
      : own_(own), base_(base) {}
  NameQualificationNodeStrings(const NameQualificationNodeStrings &) = delete;
  NameQualificationNodeStrings &
  operator=(const NameQualificationNodeStrings &) = delete;

  //! The qualifier recorded for node, or null.
  const std::string *find(SgNode *node) const {
    auto own = own_.find(node);
    if (own != own_.end()) {
      return &own->second;
    }
    if (base_ == nullptr || erased_.find(node) != erased_.end()) {
      return nullptr;
    }
    auto inherited = base_->find(node);
    return inherited != base_->end() ? &inherited->second : nullptr;
  }
  //! Record value unless node already has a qualifier.  Returns the
  //! qualifier now recorded and whether this call recorded it.
  std::pair<const std::string *, bool> emplace(SgNode *node,
                                               const std::string &value) {
    if (const std::string *existing = find(node)) {
      return {existing, false};
    }
    erased_.erase(node);
    return {&own_.emplace(node, value).first->second, true};
  }
  void erase(SgNode *node) {
    own_.erase(node);
    if (base_ != nullptr && base_->find(node) != base_->end()) {
      erased_.insert(node);
    }
  }

private:
  SgUnorderedMapNodeToString &own_;
  const SgUnorderedMapNodeToString *base_;
  std::unordered_set<SgNode *> erased_;
};

// Qualifier strings keyed by (reference node, emission statement), for
// references that shared syntax makes visible from several statements.
class NameQualificationUseSiteStrings {
public:
  using Storage = std::map<SgNode *, SgUnorderedMapNodeToString>;

  explicit NameQualificationUseSiteStrings(Storage &own,
                                           const Storage *base = nullptr)
      : own_(own), base_(base) {}
  NameQualificationUseSiteStrings(const NameQualificationUseSiteStrings &) =
      delete;
  NameQualificationUseSiteStrings &
  operator=(const NameQualificationUseSiteStrings &) = delete;

  const std::string *find(SgNode *node, SgStatement *useSite) const {
    if (const std::string *own = findIn(own_, node, useSite)) {
      return own;
    }
    return base_ != nullptr ? findIn(*base_, node, useSite) : nullptr;
  }
  std::pair<const std::string *, bool>
  emplace(SgNode *node, SgStatement *useSite, const std::string &value) {
    if (const std::string *existing = find(node, useSite)) {
      return {existing, false};
    }
    return {&own_[node].emplace(useSite, value).first->second, true};
  }

private:
  static const std::string *findIn(const Storage &storage, SgNode *node,
                                   SgStatement *useSite) {
    auto uses = storage.find(node);
    if (uses == storage.end()) {
      return nullptr;
    }
    auto qualifier = uses->second.find(useSite);
    return qualifier != uses->second.end() ? &qualifier->second : nullptr;
  }

  Storage &own_;
  const Storage *base_;
};

// Template evaluations already completed for a key.
template <typename Key> class NameQualificationEvaluationSet {
public:
  explicit NameQualificationEvaluationSet(const std::set<Key> *base = nullptr)
      : base_(base) {}
  NameQualificationEvaluationSet(const NameQualificationEvaluationSet &) =
      delete;
  NameQualificationEvaluationSet &
  operator=(const NameQualificationEvaluationSet &) = delete;

  bool contains(const Key &key) const {
    return own_.find(key) != own_.end() ||
           (base_ != nullptr && base_->find(key) != base_->end());
  }
  void insert(const Key &key) {
    if (base_ == nullptr || base_->find(key) == base_->end()) {
      own_.insert(key);
    }
  }
  //! Read through to base, the completed set of an earlier traversal.
  void inherit(const std::set<Key> *base) {
    ROSE_ASSERT(base != nullptr && base_ == nullptr && own_.empty());
    base_ = base;
  }
  //! The keys this set recorded, which a frozen analysis takes over.
  std::set<Key> &own() { return own_; }

private:
  std::set<Key> own_;
  const std::set<Key> *base_;
};

#endif
