#ifndef ROSE_INCLUDE_DIRECTIVE_H
#define ROSE_INCLUDE_DIRECTIVE_H

#include <cstddef>
#include <string>

class IncludeDirective {
private:
  std::string includedPath;
  bool isQuotedIncludeDirective;
  std::size_t targetStartPos;
  std::size_t targetLength;

public:
  IncludeDirective(const std::string &directiveText);
  const std::string &getIncludedPath() const;
  bool isQuotedInclude() const;
  std::size_t getTargetStartPos() const;
  std::size_t getTargetLength() const;
};

#endif
