#ifndef ROSE_FRONTEND_CLANG_INCLUDE_OPTION_H
#define ROSE_FRONTEND_CLANG_INCLUDE_OPTION_H

#include <cstddef>
#include <string>
#include <vector>

namespace Rose {
namespace Cmdline {

enum class IncludeOptionParseResult {
  NotIncludeOption,
  Parsed,
  MissingArgument
};

template <typename IndexType, typename CountType, typename NextArgumentProvider>
inline IncludeOptionParseResult
normalizeAndAppendIncludeOption(const std::string &current_arg,
                                IndexType &index, CountType arg_count,
                                NextArgumentProvider &&next_argument,
                                std::vector<std::string> &output_args) {
  if (current_arg == "-include") {
    ++index;
    if (index < arg_count) {
      output_args.push_back(current_arg);
      output_args.push_back(next_argument(index));
      return IncludeOptionParseResult::Parsed;
    }
    return IncludeOptionParseResult::MissingArgument;
  }

  if (current_arg.rfind("-include", 0) == 0) {
    if (current_arg.size() > 8 && current_arg[8] != '-') {
      output_args.push_back("-include");
      output_args.push_back(current_arg.substr(8));
      return IncludeOptionParseResult::Parsed;
    }
  }

  return IncludeOptionParseResult::NotIncludeOption;
}

} // namespace Cmdline
} // namespace Rose

#endif // ROSE_FRONTEND_CLANG_INCLUDE_OPTION_H
