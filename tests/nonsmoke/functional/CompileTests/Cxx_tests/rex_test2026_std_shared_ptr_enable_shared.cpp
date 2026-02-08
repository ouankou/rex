#include <memory>

using LocalId = int;

namespace vendor {
namespace mesh {

class Patch {
public:
  explicit Patch(LocalId id) : id_(id) {}

  LocalId getLocalId() const { return id_; }

private:
  LocalId id_;
};

class PatchHandle : public std::enable_shared_from_this<PatchHandle> {
public:
  explicit PatchHandle(std::shared_ptr<Patch> patch)
      : patch_(std::move(patch)) {}

  std::shared_ptr<PatchHandle> getSelf() { return shared_from_this(); }

  const std::shared_ptr<Patch> &patch() const { return patch_; }

private:
  std::shared_ptr<Patch> patch_;
};

class PatchLevel {
public:
  class Iterator {
  public:
    explicit Iterator(std::shared_ptr<Patch> patch)
        : patch_(std::move(patch)) {}

    const std::shared_ptr<Patch> &operator->() const { return patch_; }

  private:
    std::shared_ptr<Patch> patch_;
  };

  explicit PatchLevel(LocalId id)
      : patch_(std::make_shared<Patch>(id)), iterator_(patch_) {}

  Iterator begin() const { return iterator_; }

private:
  std::shared_ptr<Patch> patch_;
  Iterator iterator_;
};

} // namespace mesh
} // namespace vendor

int useSharedOwnership() {
  vendor::mesh::PatchLevel level(42);
  auto iterator = level.begin();

  auto handle = std::make_shared<vendor::mesh::PatchHandle>(
      std::make_shared<vendor::mesh::Patch>(7));

  return iterator->getLocalId() + handle->getSelf()->patch()->getLocalId();
}
