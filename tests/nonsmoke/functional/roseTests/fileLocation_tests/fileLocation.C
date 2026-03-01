// This code test the classification of files.
// Files are classified at either unknown, user,
// or system (system includes system libaries).

// The support for this work is in
// ROSE/src/Rose/StringUtility/FileNameClassifier.C

#include "rose.h"

#include <fcntl.h>

#include <limits.h>

#include <set>

#include <stdio.h>

#include <sys/stat.h>

#include <sys/types.h>

#include <time.h>

#include <unistd.h>

#include <libgen.h> /* basename(), dirame()               */

#include <cstdlib>
#include <filesystem>

using namespace std;
using namespace Rose;
using namespace Rose::StringUtility;

bool isLink(const string &name) {
  // First, check if this file exists
  if (!std::filesystem::exists(name)) {
    printf("The file \"%s\" does not exist!\n", name.c_str());
    return false;
  }

  // In oorder to evaluate if this is a link we can't just check the file
  // directly, since the file might be part of a directory that is linked.  So
  // we have to have to check each part of the whole absolute path to see if a
  // link was used. This detail make this function more complicated.

  struct stat info;

  string fileNameWithPath = name;
  char c_version[PATH_MAX];
  ROSE_ASSERT(fileNameWithPath.size() + 1 < PATH_MAX);

  // DQ (1/29/2010): Since this is a problem, escape the loop when we iterate
  // too long! A better fix is required, but this tests if this is the essential
  // problem.
  int count = 0;
  // while (fileNameWithPath != "/")
  while (fileNameWithPath != "/" && fileNameWithPath != ".") {
    strcpy(c_version, fileNameWithPath.c_str());

    // string directoryName = dirname(name.c_str());
    string directoryName = dirname(c_version);

    // printf ("directoryName = %s \n",directoryName.c_str());

    fileNameWithPath = directoryName;

    // DQ (1/29/2010): Since this is a problem, escape the loop when we iterate
    // too long! A better fix is required, but this tests if this is the
    // essential propblem.
    count++;
    if (count > 1000) {
      printf("ERROR: loop count in isLink exceeds %d \n", count);
      break;
    }
  }

  if (lstat(name.c_str(), &info) != 0) {
    // perror("lstat() error");
    printf("lstat() error for name = %s \n", name.c_str());
  } else {
  }

  // The minimum is to have a single link for a normal file.
  // bool isALink = info.st_nlink > 1;

  // DQ (1/30/2010): Ignore info.st_nlink, the only reliable way to check this
  // is to use the PPOSIX macro (S_ISLNK).
  bool isALink = S_ISLNK(info.st_mode);

  return isALink;
}

bool islinkOrPartOfLinkedDirectory(const string &fileName) {
  // In oorder to evaluate if this is a link we can't just check the file
  // directly, since the file might be part of a directory that is linked.  So
  // we have to have to check each part of the whole absolute path to see if a
  // link was used. This detail make this function more complicated.

  // struct stat info;

  string fileNameWithPath = fileName;
  char c_version[PATH_MAX];
  ROSE_ASSERT(fileNameWithPath.size() + 1 < PATH_MAX);

  bool directoryIsLink = false;
  bool fileIsLink = false;

  while (fileNameWithPath != "/" && directoryIsLink == false) {
    strcpy(c_version, fileNameWithPath.c_str());

    // I think that the interface to dirname() requires a char array.
    // string directoryName = dirname(name.c_str());
    string directoryName = dirname(c_version);

    fileNameWithPath = directoryName;

    directoryIsLink = isLink(directoryName);
  }

  if (directoryIsLink == false) {
    fileIsLink = isLink(fileName);
  } else {
    // If the directory is a link then the file is indirectly a linked file (for
    // our purposes in filename classification)
    fileIsLink = true;
  }

  return fileIsLink;
}

class visitorTraversal : public AstSimpleProcessing {
  string previousFilename;

  // CH (2/1/2010): The better way to avoid repeated filenames is to build a
  // filanames set
  std::set<string> previousFilenames;

public:
  virtual void visit(SgNode *n);
};

void display(const StringUtility::FileNameLocation &X,
             const string &label = "") {
  printf("In display(FileNameLocation): label = %s \n", label.c_str());
  string classification = "";
  switch (X) {
  case FILENAME_LOCATION_UNKNOWN:
    classification = "unknown";
    break;
  case FILENAME_LOCATION_USER:
    classification = "user";
    break;
  case FILENAME_LOCATION_LIBRARY:
    classification = "library";
    break;
  case FILENAME_LOCATION_NOT_EXIST:
    classification = "not exist";
    break;

  default: {
    printf("Error: undefined classification X = %d \n", X);
    ROSE_ASSERT(false);
  }
  }

  printf("file type classification = %s \n", classification.c_str());

  // return classification;
}

string getName(const StringUtility::FileNameLocation &X) {
  switch (X) {
  case FILENAME_LOCATION_UNKNOWN:
    return "unknown";
  case FILENAME_LOCATION_USER:
    return "user";
  case FILENAME_LOCATION_LIBRARY:
    return "library";
  case FILENAME_LOCATION_NOT_EXIST:
    return "not exist";

  default: {
    ROSE_ASSERT(false);
  }
  }
  return "unknown";
}

void display(const StringUtility::FileNameLibrary &X,
             const string &label = "") {
  printf("library classification = %s \n", X.c_str());

  // return classification;
}

void visitorTraversal::visit(SgNode *n) {
  SgStatement *statement = isSgStatement(n);
  if (statement != NULL) {
    string filename = statement->get_file_info()->get_filename();

    // CH (2/1/2010): Get the real filename (not a symlink)
    if (std::filesystem::exists(filename)) {
      char *resolved_path = realpath(filename.c_str(), NULL);
      if (resolved_path != NULL) {
        filename = resolved_path;
        free(resolved_path);
      }
    }

    // Skip the case of compiler generated Sg_File_Info objects.
    // if (previousFilename != filename && filename !=
    // "compilerGenerated")
    if (previousFilenames.count(filename) == 0 &&
        filename != "compilerGenerated") {

      FileNameClassification classification;
      // string sourceDir =
      // "/home/dquinlan/ROSE/roseCompileTree-g++4.2.2/developersScratchSpace/Dan/fileLocation_tests";

      // This causes the path edit distance to be: 4
      // string sourceDir = "/home/dquinlan/ROSE/svn-rose";
      // string sourceDir = "/home/hou1/rose";
      string sourceDir = "/";
      map<string, string> libs;
      libs["/home/hou1/opt/rose"] = "MyRose";
      // This causes the path edit distance to be: 0
      // string sourceDir = "/home/dquinlan/ROSE";

      classification = classifyFileName(filename, sourceDir, libs);

      FileNameLocation fileTypeClassification = classification.getLocation();
      FileNameLibrary libraryClassification = classification.getLibrary();
      // int pathEditDistance = classification.getDistanceFromSourceDirectory();

      if (fileTypeClassification == FILENAME_LOCATION_UNKNOWN) {
        cerr << "filename: " << filename << " is classified as UNKNOWN!!!"
             << endl;
        ROSE_ASSERT(false);
        // exit(0);
      } else {
        cout << "filename: " << filename << " "
             << getName(fileTypeClassification) << " " << libraryClassification
             << endl;
      }

      // DQ (1/30/2010): Skip the display of output (too much for testing).

      // Some of our tests explicitly build a link and this tests that it is
      // correctly identified as a link. printf
      // ("isLink(StringUtility::stripPathFromFileName(filename)) = %s
      // \n",isLink(StringUtility::stripPathFromFileName(filename)) ? "true" :
      // "false");

      // ROSE_ASSERT(isLink(filename) == false);
      // bool lk = isLink(filename);
      // printf ("isLink(filename) = %s \n",lk ? "true" : "false");

      // DQ (1/30/2010): Added this test.
      // ROSE_ASSERT(islinkOrPartOfLinkedDirectory(filename) == false);
      //  printf ("islinkOrPartOfLinkedDirectory(filename) = %s
      //  \n",islinkOrPartOfLinkedDirectory(filename) ? "true" : "false");
      previousFilenames.insert(filename);
    }

    // previousFilename = filename;
  }
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);

  visitorTraversal myvisitor;
  myvisitor.traverse(project, preorder);

  return backend(project);
}
