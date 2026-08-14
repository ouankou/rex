#include <string>

using namespace std;

class IncludeDirective {
private:
  string includedPath;
  bool isQuotedIncludeDirective;
  size_t targetStartPos;
  size_t targetLength;

public:
  IncludeDirective(const string &directiveText);
  const string &getIncludedPath() const;
  bool isQuotedInclude() const;
  size_t getTargetStartPos() const;
  size_t getTargetLength() const;
};
