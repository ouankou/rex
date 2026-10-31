#include "Rose/StringUtility/FileUtility.h"

#include "Rose/StringUtility.h"

#include "mlog.h"

#include "rose_config.h"

#include <cstdint>
#include <filesystem>

#include <fstream>

#include <iostream>
#include <limits>

#include <libgen.h>

namespace Rose {
namespace StringUtility {

std::string getAbsolutePathFromRelativePath(const std::string &relativePath) {
  ROSE_ASSERT(!relativePath.empty());
  ROSE_ASSERT(relativePath.find('\0') == std::string::npos);
  return std::filesystem::canonical(relativePath).string();
}

void writeFile(const std::string &outputString,
               const std::string &fileNameString,
               const std::string &directoryName) {
  std::string outputFileName = directoryName + fileNameString;

  std::ofstream outputFile(outputFileName.c_str());
  ROSE_ASSERT(outputFile.good() == true);

  outputFile << outputString;
  ROSE_ASSERT(outputFile.good() == true);

  outputFile.close();
  ROSE_ASSERT(outputFile.good() == true);
}

std::string readFile(const std::string &fileName) {
  std::ifstream inputFile(fileName, std::ios::binary | std::ios::ate);
  if (inputFile.good() != true) {
    printf("ERROR: File not found -- %s \n", fileName.c_str());
    std::string s("ERROR: File not found -- ");
    s += fileName;
    throw s;
  }

  const std::streamoff length = inputFile.tellg();
  ROSE_ASSERT(length >= 0);
  ROSE_ASSERT(length <= std::numeric_limits<std::streamsize>::max());
  std::string contents;
  ROSE_ASSERT(static_cast<std::uintmax_t>(length) <= contents.max_size());
  contents.resize(static_cast<std::string::size_type>(length));
  inputFile.seekg(0, std::ios::beg);
  ROSE_ASSERT(inputFile.good());
  inputFile.read(contents.data(), static_cast<std::streamsize>(length));
  ROSE_ASSERT(inputFile.good());
  ROSE_ASSERT(contents.find('\0') == std::string::npos);
  return contents;
}

std::string stripPathFromFileName(const std::string &fileNameWithPath) {
  ROSE_ASSERT(fileNameWithPath.find('\0') == std::string::npos);
  std::string path = fileNameWithPath;
  return basename(path.data());
}

std::string stripFileSuffixFromFileName(const std::string &fileNameWithSuffix) {
  const auto delimiter = fileNameWithSuffix.find_last_of("/.");
  if (delimiter == std::string::npos || fileNameWithSuffix[delimiter] != '.')
    return fileNameWithSuffix;
  return fileNameWithSuffix.substr(0, delimiter);
}

std::string fileNameSuffix(const std::string &fileNameWithSuffix) {
  const auto delimiter = fileNameWithSuffix.find_last_of("/.");
  if (delimiter == std::string::npos || fileNameWithSuffix[delimiter] != '.')
    return fileNameWithSuffix;
  return fileNameWithSuffix.substr(delimiter + 1);
}

std::string getPathFromFileName(const std::string &fileNameWithPath) {
  ROSE_ASSERT(fileNameWithPath.find('\0') == std::string::npos);
  std::string path = fileNameWithPath;
  std::string returnString = dirname(path.data());
  // dirname returns a "." if fileNameWithPath does not contain "/"'s

  if (returnString == ".")
    returnString = "";
  return returnString;
}

FileWithLineNumbers readFileWithPos(const std::string &fileName) {
  unsigned int line = 1;
  std::string fullFileName = getAbsolutePathFromRelativePath(fileName);
  const std::string contents = readFile(fileName);

  FileWithLineNumbers result;
  std::string::size_type pos = 0;
  do {
    const auto end = contents.find('\n', pos);
    result.push_back(StringWithLineNumber(contents.substr(pos, end - pos),
                                          fullFileName, line));
    ++line;
    if (end == std::string::npos)
      break;
    pos = end + 1;
  } while (pos < contents.size());

  result.push_back(StringWithLineNumber("", "", 1));
  return result;
}

namespace {
std::string lineDirective(const std::string &filename, std::int64_t line) {
  // EXT-CLANG-FILE-MACRO-CR: LLVM23 maps CR to LF in __FILE__, even with
  // escaped #line filenames. See https://github.com/ouankou/rex/issues/885.
  std::string directive =
      "#line " + numberToString(line) + " \"" + cEscape(filename) + "\"";
  directive = replaceAllCopy(directive, "?", "\\?");
#ifdef SKIP_HASH_LINE_NUMBER_DECLARATIONS_IN_GENERATED_FILES
  directive = replaceAllCopy(directive, "/*", "\\057*");
  directive = replaceAllCopy(directive, "*/", "*\\057");
  directive = "/* " + directive + " */";
#endif
  return directive + "\n";
}
} // namespace

std::string StringWithLineNumber::toString() const {
  return lineDirective(filename, line) + str + "\n";
}

std::string toString(const FileWithLineNumbers &strings,
                     const std::string &filename,
                     int physicalLine /* Line number in output file */) {
  std::string result;
  unsigned int lastLineNumber = 1;
  std::string lastFile = "";
  bool inPhysicalFile = true;
  bool needLineDirective = false;
  for (unsigned int i = 0; i < strings.size(); ++i) {
    bool newInPhysicalFile = (strings[i].filename == "");
    if (inPhysicalFile != newInPhysicalFile)
      needLineDirective = true;
    if (!inPhysicalFile &&
        (strings[i].filename != lastFile || strings[i].line != lastLineNumber))
      needLineDirective = true;

    if (strings[i].str == "" && i + 1 == strings.size()) {
      needLineDirective = false;
    }

    if (needLineDirective) {
      const auto &directiveFile =
          newInPhysicalFile ? filename : strings[i].filename;
      const std::int64_t directiveLine =
          newInPhysicalFile ? static_cast<std::int64_t>(physicalLine) + 1
                            : strings[i].line;
      result += lineDirective(directiveFile, directiveLine);
      lastLineNumber = strings[i].line + 1;
      lastFile = strings[i].filename;
    } else {
      ++lastLineNumber;
    }
    result += strings[i].str + "\n";

    bool printedLineDirective = needLineDirective;

    needLineDirective = false;
    inPhysicalFile = newInPhysicalFile;
    if (strings[i].str.find('\n') != std::string::npos && !inPhysicalFile) {
      needLineDirective = true;
    }

    if (printedLineDirective)
      ++physicalLine;
    for (size_t pos = strings[i].str.find('\n'); pos != std::string::npos;
         pos = strings[i].str.find('\n', pos + 1)) {
      ++physicalLine;
    }
    ++physicalLine;
  }
  return result;
}

std::string copyEdit(const std::string &inputString,
                     const std::string &oldToken, const std::string &newToken) {
  return replaceAllCopy(inputString, oldToken, newToken);
}

FileWithLineNumbers copyEdit(const FileWithLineNumbers &inputString,
                             const std::string &oldToken,
                             const std::string &newToken) {
  ROSE_ASSERT(!oldToken.empty());
  FileWithLineNumbers result = inputString;
  for (unsigned int i = 0; i < result.size(); ++i)
    result[i].str = copyEdit(result[i].str, oldToken, newToken);
  return result;
}

FileWithLineNumbers copyEdit(const FileWithLineNumbers &inputString,
                             const std::string &oldToken,
                             const FileWithLineNumbers &newToken) {
  ROSE_ASSERT(!oldToken.empty());
  FileWithLineNumbers result = inputString;
  for (unsigned int i = 0; i < result.size(); ++i) {
    std::string str = result[i].str;
    std::string::size_type pos = str.find(oldToken);
    if (pos != std::string::npos) {
      // Split the line into the before-substitution and after-substitution
      // regions
      result[i].str = str.substr(0, pos);
      result.insert(result.begin() + i + 1,
                    StringWithLineNumber(str.substr(pos + oldToken.size()),
                                         result[i].filename, result[i].line));
      // Do the insertion
      result.insert(result.begin() + i + 1, newToken.begin(), newToken.end());
      i += newToken.size(); // Rescan the after-substitution part of the old
                            // line, but not any of the new text
    }
  }
  return result;
}

} // namespace StringUtility
} // namespace Rose
