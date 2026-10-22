#ifndef PRE_POST_TRANSFORMATION_H
#define PRE_POST_TRANSFORMATION_H

#include "LoopTransformInterface.h"

#include <charconv>

class PrePostTransformation : public TransformAstTree {
protected:
  virtual bool operator()(AstInterface &fa, const AstNodePtr &n,
                          AstNodePtr &result) = 0;
  static bool readOptionalPositiveSize(const std::vector<std::string> &argv,
                                       unsigned &index, const char *option,
                                       unsigned &size) {
    if (index == argv.size())
      return false;
    const std::string &argument = argv[index];
    if (argument.empty()) {
      std::cerr << "REX_LOOP_OPTION[positive-size]: " << option
                << " has an empty size\n";
      ROSE_ABORT();
    }
    const char *begin = argument.data();
    const char *end = begin + argument.size();
    if (begin != end && *begin == '+')
      ++begin;
    const bool numeric = begin != end && ((*begin >= '0' && *begin <= '9') ||
                                          (*begin == '-' && begin + 1 != end &&
                                           begin[1] >= '0' && begin[1] <= '9'));
    if (!numeric)
      return false;
    int parsedSize = 0;
    const auto parsed = std::from_chars(begin, end, parsedSize);
    if (parsed.ec != std::errc{} || parsed.ptr != end || parsedSize <= 0) {
      std::cerr << "REX_LOOP_OPTION[positive-size]: " << option
                << " has invalid size '" << argument << "'\n";
      ROSE_ABORT();
    }
    size = static_cast<unsigned>(parsedSize);
    ++index;
    return true;
  }

  static bool cmdline_find(const std::vector<std::string> &argv,
                           unsigned &index, const std::string &opt,
                           std::vector<std::string> *unknown_args) {
    for (; index < argv.size(); ++index) {
      if (argv[index] != opt) {
        if (unknown_args != 0)
          unknown_args->push_back(argv[index]);
      } else
        return true;
    }
    return false;
  }
  static void append_args(const std::vector<std::string> &argv, unsigned &index,
                          std::vector<std::string> &unknown_args) {
    for (; index < argv.size(); ++index)
      unknown_args.push_back(argv[index]);
  }

public:
  AstNodePtr operator()(const AstNodePtr &n,
                        AstInterface::TraversalVisitType t) {
    AstInterface &fa = LoopTransformInterface::getAstInterface();
    return TransformAstTraverse(fa, n, *this, t);
  }
};

#endif
