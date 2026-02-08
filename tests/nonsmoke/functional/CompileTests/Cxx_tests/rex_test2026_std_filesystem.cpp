#include <filesystem>
#include <string>

namespace vendor {
namespace paths {

class Workspace {
public:
  static std::filesystem::path compose(const std::string &root,
                                       const std::string &leaf) {
    using std::filesystem::path;
    path base(root);
    path child(leaf);
    return base / child;
  }

  static std::filesystem::path normalize(const std::filesystem::path &input) {
    // Keep this local using-directive form to exercise qualification recovery.
    using namespace std::filesystem;
    path cleaned = input.lexically_normal();
    return cleaned;
  }
};

} // namespace paths
} // namespace vendor

std::filesystem::path buildConfigPath() {
  std::filesystem::path path = vendor::paths::Workspace::compose("tmp", "rex");
  return vendor::paths::Workspace::normalize(path / "config.json");
}
