#include "FileUtility.h"

// Other ROSE includes
#include "FileUtility.h"

#include "Rose/StringUtility.h"

#include "IntegerOps.h"

#include "mlog.h"

#include "rose_config.h"

// Other includes
#include <cstring>

#include <errno.h> // DQ (9/29/2006): This is required for 64-bit g++ 3.4.4 compiler.
#include <fstream>

#include <inttypes.h>

#include <iostream>

#include "rose_config.h"

#include <sstream>

#include <dirent.h> /* readdir(), etc.                    */
#include <libgen.h> /* basename(), dirame()               */
#include <sys/param.h> // AS added to support the function getAbsolutePathFromRelativePath
#include <sys/stat.h> /* stat(), etc.                       */
#include <unistd.h>   /* getcwd(), etc.                     */

namespace Rose {
namespace StringUtility {

// [Robb Matzke 2016-05-06] deprecated
std::list<std::string> findfile(std::string patternString,
                                std::string pathString) {
  std::list<std::string> patternMatches;

  DIR *dir;             /* pointer to the scanned directory. */
  struct dirent *entry; /* pointer to one directory entry.   */
  // struct stat dir_stat; /* used by stat().                   */

  /* open the directory for reading */
  dir = opendir(pathString.c_str());
  if (!dir) {
    std::cerr << "Cannot read directory:" << pathString << std::endl;
    perror("");
    return patternMatches;
  }

  /* scan the directory, traversing each sub-directory, and */
  /* matching the pattern for each file name.               */
  while ((entry = readdir(dir))) {
    /* check if the pattern matchs. */
    /* MS: 11/22/2015: note that d_name is an array of char
     * and testing it as pointer always gives true;
     * removed this kind of testing code
     */
    std::string entryName = entry->d_name;
    if (entryName.find(patternString) != std::string::npos) {
      patternMatches.push_back(pathString + "/" + entryName);
    }
  }
  return patternMatches;
}

std::vector<std::string> readWordsInFile(std::string filename) {
  std::vector<std::string> variantsToUse;
  std::fstream file_op(filename.c_str());
  if (file_op.fail()) {
    std::cout << "error: could not find file \"" << filename
              << "\" which is meant to include the styles to enforce with "
              << "the name checker." << std::endl;
    exit(1); // abort program
  }

  std::string current_word;

  while (file_op >> current_word) {
    // First word denotes what the regular expression should operate
    // upon. Second word denotes the regular expression
    variantsToUse.push_back(current_word);
  }

  return variantsToUse;
}

//
// Rama: 12/06/06
// We need a function getAbsolutePathFromRelativePath that takes any filename
// and returns the absolute file name  (with path) AS suggested we use realpath
// that comes with stdlib. However, for a translator like ours, we need to have
// two versions of the file: silent and non-silent ones, depending on whether we
// need to check and print an error or not. That is done by the boolean
// parameter printErrorIfAny that is set to true by the caller The dafault
// version -- enforced by setting printErrorIfAny to false -- is the silent one.
//
// Also, look at the code added in function
// void SgFile::setupSourceFilename in file .../src/ROSETTA/Grammar/Support.code
//

std::string getAbsolutePathFromRelativePath(const std::string &relativePath,
                                            bool printErrorIfAny) {
  std::string returnString;
  char resolved_path[MAXPATHLEN];
  resolved_path[0] = '\0';

  // DQ (9/3/2006): Note that "realpath()"
  // can return an error if it processes a file or directory that does not
  // exist.  This is a problem for include paths that are specified on the
  // commandline and which don't exist; most compilers silently ignore these and
  // we have to at least ignore them.
  //      string resultingPath="";
  // tps (01/08/2010) : This implementation was incorrect as it mixed char* and
  // string. Fixed it.
  char *rp = realpath(relativePath.c_str(), resolved_path);
  std::string resultingPath = "";
  if (rp != NULL)
    resultingPath = std::string(rp);

  // printf("resultingPath == %s    printErrorIfAny == %d
  // \n",resultingPath.c_str(),printErrorIfAny);
  // If there was an error then resultingPath is NULL, else it points to
  // resolved_path.
  if (resultingPath.empty() == true) //== NULL )
  {
    // DQ (9/4/2006): SgProject is not available within this code since it is
    // used to compile ROSETTA before the IR nodes are defined!  So we should
    // just comment it out. DQ (9/4/2006): Only output such warnings if verbose
    // is set to some value greater than zero.
    if (printErrorIfAny == true) {
      // if (SgProject::get_verbose() > 0)
      //{
      // Output the kind of error that occured ...
      // Ask DAN and add checks for 64 bit machines here
      // extern int errno;
      // Output the kind of error that occured ...
      // This very low-level, internal, utility function should not be
      // complaining on standard output. [Matzke 2017-04-21]
      // printf ("relativePath = %s errno = %d resolved_path is undefined
      // \n",relativePath.c_str(),errno); printf ("     error = %s
      // \n",strerror(errno));

      // In case of error return the original relativePath
      // printf ("Error: getAbsolutePathFromRelativePath incured an error in use
      // of realpath() and is returning the input relativePath. \n");
      //}
    }
    // printf("returnString0 == %s    relativePath == %s   resolved_path == %s
    // \n",returnString.c_str(),relativePath.c_str(),resolved_path);
    returnString = relativePath;
  } else {
    // "realpath()" worked so return the corrected absolute path.
    // printf("returnString1 == %s    relativePath == %s   resolved_path == %s
    // \n",returnString.c_str(),relativePath.c_str(),resolved_path);
    returnString = resolved_path;
  }

  // printf("returnString3 == %s    relativePath == %s   resolved_path == %s
  // \n",returnString.c_str(),relativePath.c_str(),resolved_path);

  ROSE_ASSERT(returnString.empty() == false);

  return returnString;
}

void writeFile(const std::string &outputString,
               const std::string &fileNameString,
               const std::string &directoryName) {
  std::string outputFileName = directoryName + fileNameString;

  std::ofstream outputFile(outputFileName.c_str());
  ROSE_ASSERT(outputFile.good() == true);

  // Select an output stream for the program tree display (cout or
  // <filename>.C.roseShow) Macro OUTPUT_SHOWFILE_TO_FILE is defined in the
  // transformation_1.h header file ostream & outputStream = (OUTPUT_TO_FILE ?
  // ((ostream&) outputFile) : ((ostream&) cout));
  ROSE_ASSERT(outputFile.good() == true);

  outputFile << outputString;
  ROSE_ASSERT(outputFile.good() == true);

  outputFile.close();
}

std::string readFile(const std::string &fileName) {
  // Reads entire text file and places contents into a single string

  // BP : 10/23/2001, rather than allocate fixed large blocks of memory (350K *
  // sizeof(char) !!) allocate what is required. the code below is a slightly
  // modified version of what I found at:
  // http://www.cplusplus.com/ref/iostream/istream/read.html

  char *buffer = NULL;

  std::ifstream inputFile;
  inputFile.open(fileName.c_str(), std::ios::binary);
  if (inputFile.good() != true) {
    printf("ERROR: File not found -- %s \n", fileName.c_str());
    // ROSE_ABORT();
    std::string s("ERROR: File not found -- ");
    s += fileName;
    throw s;
  }

  ROSE_ASSERT(inputFile.good() == true);

  // get length of file:
  inputFile.seekg(0, std::ios::end);
  std::streamoff length = inputFile.tellg();
  inputFile.seekg(0, std::ios::beg);

  // allocate memory:
  buffer = new char[length + 1];
  ROSE_ASSERT(buffer != NULL);

  // read data as a block:
  inputFile.read(buffer, (int)length);
  buffer[length] = '\0';
  inputFile.close();

  // DQ: (10/21/02) Sunjeev reported the following assertion as failing on
  // his machine at UCSD (works for us, but i have made it more general)
  // ROSE_ASSERT(strlen(buffer) <= length);
  // MS: (12/11/02) added the strict test again
  ROSE_ASSERT(strlen(buffer) == (unsigned)length);

  std::string returnString = buffer;

  return returnString;
}

// Rama: 12/06/06
// Replaced the functionality by a call to basename
std::string stripPathFromFileName(const std::string &fileNameWithPath) {

  // DQ (9/6/2008): It seems that the problem might have been the
  // stripFileSuffixFromFileName(), so this is put back!

  // DQ (9/6/2008): This version does not work on paths that have "." in them
  // (basename() function does not work well).
  //    Example of input that fails:
  //    ROSE/ROSE_CompileTree/svn-LINUX-64bit-4.2.2/tutorial/inputCode_binaryAST_1
  // returns: svn-LINUX-64bit-4.2

  std::string returnString;
  char c_version[PATH_MAX];
  ROSE_ASSERT(fileNameWithPath.size() + 1 < PATH_MAX);
  strcpy(c_version, fileNameWithPath.c_str());
  returnString = basename(c_version);

  return returnString;
}

std::string stripFileSuffixFromFileName(const std::string &fileNameWithSuffix) {
  // Make it safe to input a filename without a suffix (return the filename)

  std::string returnString;

  // Handle the case of files where the filename does not have a suffix
  size_t lastSlashPos = fileNameWithSuffix.rfind('/');
  size_t lastDotPos = fileNameWithSuffix.rfind('.');

  if (lastSlashPos != std::string::npos && lastDotPos < lastSlashPos)
    returnString = fileNameWithSuffix;
  else
    returnString = fileNameWithSuffix.substr(0, lastDotPos);

  return returnString;
}

// Rama: I am not sure if this mechanism can deal with files ending with .  Like
// "test."  I am not clear about the purpose of the function too. So, not
// modifying it.
std::string fileNameSuffix(const std::string &fileNameWithSuffix) {
  // Make it safe to input a filename without a suffix (return the filename)
  std::string::size_type positionOfDot = fileNameWithSuffix.rfind('.');
  std::string returnString = fileNameWithSuffix;

  // allow input to not have an extension
  if (positionOfDot != std::string::npos) {
    // Advance past the "."
    positionOfDot++;

    returnString = fileNameWithSuffix.substr(positionOfDot);
  }

  return returnString;
}

// DQ (3/15/2005): New, simpler and better implementation suggested function
// from Tom, thanks Tom!
std::string getPathFromFileName(const std::string &fileNameWithPath) {
  char c_version[PATH_MAX];
  ROSE_ASSERT(fileNameWithPath.size() + 1 < PATH_MAX);
  strcpy(c_version, fileNameWithPath.c_str());
  std::string returnString = dirname(c_version);
  // dirname returns a "." if fileNameWithPath does not contain "/"'s
  // I am not sure why this function was written and so, preserve the
  // functionality using empty return string in such cases.

  if (returnString == ".")
    returnString = "";
  return returnString;
}

FileWithLineNumbers readFileWithPos(const std::string &fileName) {
  // Reads entire text file and places contents into a single string

  // BP : 10/23/2001, rather than allocate fixed large blocks of memory (350K *
  // sizeof(char) !!) allocate what is required. the code below is a slightly
  // modified version of what I found at:
  // http://www.cplusplus.com/ref/iostream/istream/read.html

  unsigned int line = 1;
  char *buffer = NULL;

  std::string fullFileName = getAbsolutePathFromRelativePath(fileName);

  // printf("Opening file : %s\n",fullFileName.c_str());

  std::ifstream inputFile;

  inputFile.open(fileName.c_str(), std::ios::binary);

  if (inputFile.good() != true) {
    printf("ERROR: File not found -- %s \n", fileName.c_str());
    // ROSE_ABORT();
    std::string s("ERROR: File not found -- ");
    s += fileName;
    throw s;
  }

  ROSE_ASSERT(inputFile.good() == true);

  // get length of file:
  inputFile.seekg(0, std::ios::end);
  std::streamoff length = inputFile.tellg();
  inputFile.seekg(0, std::ios::beg);

  // allocate memory:
  buffer = new char[length + 1];
  ROSE_ASSERT(buffer != NULL);

  // read data as a block:
  inputFile.read(buffer, (int)length);
  buffer[length] = '\0';
  inputFile.close();

  // DQ: (10/21/02) Sunjeev reported the following assertion as failing on
  // his machine at UCSD (works for us, but i have made it more general)
  // ROSE_ASSERT(strlen(buffer) <= length);
  // MS: (12/11/02) added the strict test again
  ROSE_ASSERT(strlen(buffer) == (unsigned)length);

  std::string returnString = buffer;
  delete[] buffer;

  FileWithLineNumbers result;
  for (std::string::size_type pos = 0; pos != std::string::npos;) {
    std::string::size_type lastPos = pos;
    pos = returnString.find('\n', lastPos);
    result.push_back(StringWithLineNumber(
        returnString.substr(lastPos, pos - lastPos), fullFileName, line));
    // cerr << "Added line '" << returnString.substr(lastPos, pos - lastPos) <<
    // "' at line " << line << endl;
    ++line;
    ++pos; // Skip newline
    if (pos == returnString.size())
      break;
  }

  result.push_back(StringWithLineNumber("", "", 1));
  return result;
}

std::string StringWithLineNumber::toString() const {
  std::ostringstream os;

  // DQ (1/21/2010): Added support for skipping these when in makes it easer to
  // debug ROSETTA generated files.
#ifdef SKIP_HASH_LINE_NUMBER_DECLARATIONS_IN_GENERATED_FILES
  // os << str << std::endl;
  os << "/* #line " << line << " \""
     << (filename.empty() ? "" : getAbsolutePathFromRelativePath(filename))
     << "\" */\n"
     << str << std::endl;
#else
  os << "#line " << line << " \""
     << (filename.empty() ? "" : getAbsolutePathFromRelativePath(filename))
     << "\"\n"
     << str << std::endl;
#endif

  return os.str();
}

std::string toString(const FileWithLineNumbers &strings,
                     const std::string &filename,
                     int physicalLine /* Line number in output file */) {
  std::string result;
  unsigned int lastLineNumber = 1;
  std::string lastFile = "";
  bool inPhysicalFile = true; // Not in a specifically named file
  bool needLineDirective = false;
  for (unsigned int i = 0; i < strings.size(); ++i) {
    // Determine if a #line directive is needed, if the last iteration did not
    // force one to be added
    bool newInPhysicalFile = (strings[i].filename == "");
    if (inPhysicalFile != newInPhysicalFile)
      needLineDirective = true;
    if (!inPhysicalFile &&
        (strings[i].filename != lastFile || strings[i].line != lastLineNumber))
      needLineDirective = true;

    if (strings[i].str == "" && i + 1 == strings.size()) { // Special case
      needLineDirective = false;
    }

    // Print out the #line directive (if needed) and the actual line
    if (needLineDirective) {
#ifdef SKIP_HASH_LINE_NUMBER_DECLARATIONS_IN_GENERATED_FILES
      /* Nothing to do here! */
      if (strings[i].filename == "") { // Reset to actual input file
        // The "1" is the increment because number is the line number of the
        // NEXT line after the #line directive
        result += "/* #line " + numberToString(physicalLine + 1) + " \"" +
                  filename + "\" */\n";
      } else {
        result += "/* #line " + numberToString(strings[i].line) + " \"" +
                  strings[i].filename + "\" */\n";
      }
#else
      if (strings[i].filename == "") { // Reset to actual input file
        // The "1" is the increment because number is the line number of the
        // NEXT line after the #line directive
        result += "#line " + numberToString(physicalLine + 1) + " \"" +
                  filename + "\"\n";
      } else {
        result += "#line " + numberToString(strings[i].line) + " \"" +
                  strings[i].filename + "\"\n";
      }
#endif
      // These are only updated when a #line directive is actually printed,
      // largely because of the blank line exception above (so if a blank line
      // starts a new file, the #line directive needs to be emitted on the
      // first non-blank line)
      lastLineNumber = strings[i].line + 1;
      lastFile = strings[i].filename;
    } else {
      ++lastLineNumber;
    }
    result += strings[i].str + "\n";

    bool printedLineDirective = needLineDirective;

    // Determine if a #line directive is needed for the next iteration
    needLineDirective = false;
    inPhysicalFile = newInPhysicalFile;
    if (strings[i].str.find('\n') != std::string::npos && !inPhysicalFile) {
      needLineDirective =
          true; // Ensure that the next line has a #line directive
    }

    // Update the physical line counter based on the number of lines output
    if (printedLineDirective)
      ++physicalLine; // For #line directive
    for (size_t pos = strings[i].str.find('\n'); pos != std::string::npos;
         pos = strings[i].str.find('\n', pos + 1)) {
      ++physicalLine; // Increment for \n in string
    }
    ++physicalLine; // Increment for \n added at end of line
  }
  return result;
}

// BP : 10/25/2001, a non recursive version that allocs memory only once
std::string copyEdit(const std::string &inputString,
                     const std::string &oldToken, const std::string &newToken) {
  std::string returnString;
  std::string::size_type oldTokenSize = oldToken.size();

  std::string::size_type position = 0;
  std::string::size_type lastPosition = 0;
  while (true) {
    position = inputString.find(oldToken, position);
    if (position == std::string::npos) {
      returnString += inputString.substr(lastPosition);
      break;
    } else {
      returnString += inputString.substr(lastPosition, position - lastPosition);
      returnString += newToken;
      position = lastPosition = position + oldTokenSize;
    }
  }

  return returnString;
}

FileWithLineNumbers copyEdit(const FileWithLineNumbers &inputString,
                             const std::string &oldToken,
                             const std::string &newToken) {
  FileWithLineNumbers result = inputString;
  for (unsigned int i = 0; i < result.size(); ++i)
    result[i].str = copyEdit(result[i].str, oldToken, newToken);
  return result;
}

FileWithLineNumbers copyEdit(const FileWithLineNumbers &inputString,
                             const std::string &oldToken,
                             const FileWithLineNumbers &newToken) {
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
                                         result[i].filename +
                                             " after subst for " + oldToken,
                                         result[i].line));
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
