// #include <rose.h>

#include "mlog.h"

#include "IncludeDirective.h"

#include <cctype>
#include <cstdio>
#include <vector>

namespace {
struct PhaseTwoSpelling {
  std::string logical;
  std::vector<size_t> rawOffsets;
};

std::string diagnosticDirectiveSpelling(const std::string &spelling) {
  std::string result;
  result.reserve(spelling.size());
  for (char character : spelling) {
    switch (character) {
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result += character;
      break;
    }
  }
  return result;
}

[[noreturn]] void rejectIncludeDirective(const std::string &reason,
                                         const std::string &directiveText) {
  const std::string diagnosticSpelling =
      diagnosticDirectiveSpelling(directiveText);
  std::fprintf(stderr,
               "REX_FRONTEND_INVARIANT[include-directive-spelling]: "
               "reason=%s spelling=%s\n",
               reason.c_str(), diagnosticSpelling.c_str());
  ROSE_ABORT();
}

bool isHorizontalWhitespace(char character) {
  return character == ' ' || character == '\t' || character == '\f' ||
         character == '\v';
}

PhaseTwoSpelling deletePhaseTwoLineSplices(const std::string &spelling) {
  PhaseTwoSpelling result;
  result.logical.reserve(spelling.size());
  result.rawOffsets.reserve(spelling.size());

  for (size_t raw = 0; raw < spelling.size();) {
    if (spelling[raw] == '\\' && raw + 1 < spelling.size()) {
      if (spelling[raw + 1] == '\n') {
        raw += 2;
        continue;
      }
      if (spelling[raw + 1] == '\r' && raw + 2 < spelling.size() &&
          spelling[raw + 2] == '\n') {
        raw += 3;
        continue;
      }
    }

    result.rawOffsets.push_back(raw);
    result.logical += spelling[raw];
    ++raw;
  }

  return result;
}

size_t rawOffsetForLogicalBoundary(const PhaseTwoSpelling &phaseTwo,
                                   const std::string &rawSpelling,
                                   size_t logicalOffset) {
  if (logicalOffset > phaseTwo.logical.size()) {
    rejectIncludeDirective("invalid-logical-offset", rawSpelling);
  }
  return logicalOffset == phaseTwo.logical.size()
             ? rawSpelling.size()
             : phaseTwo.rawOffsets[logicalOffset];
}

size_t skipIncludeDirectiveTrivia(const std::string &logical, size_t position,
                                  const std::string &rawSpelling) {
  while (position < logical.size()) {
    if (isHorizontalWhitespace(logical[position])) {
      ++position;
      continue;
    }
    if (logical.compare(position, 2, "/*") == 0) {
      const size_t commentEnd = logical.find("*/", position + 2);
      if (commentEnd == std::string::npos) {
        rejectIncludeDirective("unterminated-comment", rawSpelling);
      }
      position = commentEnd + 2;
      continue;
    }
    break;
  }
  return position;
}

void requireOnlyTrailingDirectiveTrivia(const std::string &logical,
                                        size_t position,
                                        const std::string &rawSpelling) {
  while (position < logical.size()) {
    if (isHorizontalWhitespace(logical[position])) {
      ++position;
      continue;
    }
    if (logical.compare(position, 2, "/*") == 0) {
      const size_t commentEnd = logical.find("*/", position + 2);
      if (commentEnd == std::string::npos) {
        rejectIncludeDirective("unterminated-comment", rawSpelling);
      }
      position = commentEnd + 2;
      continue;
    }
    if (logical.compare(position, 2, "//") == 0 || logical[position] == '\n' ||
        logical[position] == '\r') {
      return;
    }
    rejectIncludeDirective("tokens-after-header-name", rawSpelling);
  }
}
} // namespace

IncludeDirective::IncludeDirective(const string &directiveText) {
  isQuotedIncludeDirective = false;
  const PhaseTwoSpelling phaseTwo = deletePhaseTwoLineSplices(directiveText);
  const std::string &logical = phaseTwo.logical;
  size_t position = skipIncludeDirectiveTrivia(logical, 0, directiveText);
  if (position >= logical.size() || logical[position] != '#') {
    rejectIncludeDirective("missing-hash", directiveText);
  }
  position = skipIncludeDirectiveTrivia(logical, position + 1, directiveText);
  const size_t keywordStart = position;
  while (position < logical.size() &&
         (std::isalnum(static_cast<unsigned char>(logical[position])) ||
          logical[position] == '_')) {
    ++position;
  }
  const std::string keyword =
      logical.substr(keywordStart, position - keywordStart);
  if (keyword != "include" && keyword != "include_next") {
    rejectIncludeDirective("wrong-keyword", directiveText);
  }
  position = skipIncludeDirectiveTrivia(logical, position, directiveText);
  if (position >= logical.size()) {
    rejectIncludeDirective("missing-target", directiveText);
  }

  size_t endPos = string::npos;
  if (logical[position] == '"') {
    isQuotedIncludeDirective = true;
    targetStartPos =
        rawOffsetForLogicalBoundary(phaseTwo, directiveText, position);
    endPos = logical.find('"', position + 1);
    if (endPos == string::npos) {
      rejectIncludeDirective("unterminated-quoted-target", directiveText);
    }
    includedPath = logical.substr(position + 1, endPos - position - 1);
    requireOnlyTrailingDirectiveTrivia(logical, endPos + 1, directiveText);
  } else if (logical[position] == '<') {
    targetStartPos =
        rawOffsetForLogicalBoundary(phaseTwo, directiveText, position);
    endPos = logical.find('>', position + 1);
    if (endPos == string::npos) {
      rejectIncludeDirective("unterminated-angled-target", directiveText);
    }
    includedPath = logical.substr(position + 1, endPos - position - 1);
    requireOnlyTrailingDirectiveTrivia(logical, endPos + 1, directiveText);
  } else {
    targetStartPos =
        rawOffsetForLogicalBoundary(phaseTwo, directiveText, position);
    size_t scan = position;
    size_t lastTokenEnd = position;
    while (scan < logical.size() && logical[scan] != '\n' &&
           logical[scan] != '\r') {
      if (isHorizontalWhitespace(logical[scan])) {
        ++scan;
        continue;
      }
      if (logical.compare(scan, 2, "//") == 0) {
        break;
      }
      if (logical.compare(scan, 2, "/*") == 0) {
        const size_t commentEnd = logical.find("*/", scan + 2);
        if (commentEnd == string::npos) {
          rejectIncludeDirective("unterminated-comment", directiveText);
        }
        scan = commentEnd + 2;
        continue;
      }
      ++scan;
      lastTokenEnd = scan;
    }
    endPos = lastTokenEnd;
    includedPath = logical.substr(position, endPos - position);
  }

  if (includedPath.empty()) {
    rejectIncludeDirective("empty-target", directiveText);
  }
  const size_t logicalTargetEnd =
      isQuotedIncludeDirective || logical[position] == '<' ? endPos + 1
                                                           : endPos;
  const size_t rawTargetEnd =
      rawOffsetForLogicalBoundary(phaseTwo, directiveText, logicalTargetEnd);
  if (rawTargetEnd <= targetStartPos) {
    rejectIncludeDirective("invalid-target-range", directiveText);
  }
  targetLength = rawTargetEnd - targetStartPos;
}

const string &IncludeDirective::getIncludedPath() const { return includedPath; }

bool IncludeDirective::isQuotedInclude() const {
  return isQuotedIncludeDirective;
}

size_t IncludeDirective::getTargetStartPos() const { return targetStartPos; }

size_t IncludeDirective::getTargetLength() const { return targetLength; }
