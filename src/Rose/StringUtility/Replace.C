#include "Rose/StringUtility/Replace.h"
#include "mlog.h"

namespace Rose {
namespace StringUtility {

std::string replaceAllCopy(const std::string &input, const std::string &search,
                           const std::string &replacedWith) {
  ROSE_ASSERT(!search.empty());

  std::string result;
  result.reserve(input.size());

  std::size_t start_pos = 0;
  std::size_t found_pos;

  while ((found_pos = input.find(search, start_pos)) != std::string::npos) {
    result.append(input, start_pos, found_pos - start_pos);
    result.append(replacedWith);
    start_pos = found_pos + search.size();
  }

  result.append(input, start_pos, input.size() - start_pos);

  return result;
}

} // namespace StringUtility
} // namespace Rose
