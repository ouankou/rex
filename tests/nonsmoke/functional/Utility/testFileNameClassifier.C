#include "Rose/StringUtility.h"

#include "rose.h"

#include <vector>

using namespace std;
using namespace Rose::StringUtility;

int main(int argc, char **argv) {
  // CH (2/2/2010): Skip this test since access denied
  return 1;

  FileNameClassification classification;

  string home;
  string sourceDir;

  // One would generally populate home here, but we fix the value
  // to make sure these tests are deterministic on across machines
  // homeDir(home);
  home = "/home/stutsman1/";
  sourceDir = home + "src/svn-rose/";

  string slashes = "/////";

  classification = classifyFileName("/usr/include/stdio.h", sourceDir);
  ROSE_ASSERT(classification.getLocation() == FILENAME_LOCATION_LIBRARY);
  ROSE_ASSERT(classification.getLibrary() == FILENAME_LIBRARY_C);
  ROSE_ASSERT(classification.getLibraryName() == "c");
  ROSE_ASSERT(classification.getDistanceFromSourceDirectory() == 6);

  classification = classifyFileName(home + "include/stdio.h", sourceDir);
  ROSE_ASSERT(classification.getLocation() == FILENAME_LOCATION_UNKNOWN);
  ROSE_ASSERT(classification.getLibrary() == FILENAME_LIBRARY_UNKNOWN);
  ROSE_ASSERT(classification.getDistanceFromSourceDirectory() == 3);

  // TODO leading slashes are legal UNIX paths, but its doubtful
  // that the compiler will generate such paths anyways so
  // perhaps this doesn't matter
  classification =
      classifyFileName(slashes + home + "include/stdio.h", sourceDir);
  ROSE_ASSERT(classification.getLocation() == FILENAME_LOCATION_UNKNOWN);
  ROSE_ASSERT(classification.getLibrary() == FILENAME_LIBRARY_UNKNOWN);
  ROSE_ASSERT(classification.getLibraryName() == "UNKNOWN");
  // TODO distance metric

  classification = classifyFileName("/usr/include/rose.h", sourceDir);
  ROSE_ASSERT(classification.getLocation() == FILENAME_LOCATION_LIBRARY);
  ROSE_ASSERT(classification.getLibrary() == FILENAME_LIBRARY_ROSE);
  ROSE_ASSERT(classification.getLibraryName() == "rose");
  ROSE_ASSERT(classification.getDistanceFromSourceDirectory() == 6);

  classification = classifyFileName("/usr/include/c++/3.4.3/string", sourceDir);
  ROSE_ASSERT(classification.getLocation() == FILENAME_LOCATION_LIBRARY);
  ROSE_ASSERT(classification.getLibrary() == FILENAME_LIBRARY_STDCXX);
  ROSE_ASSERT(classification.getLibraryName() == "stdc++");
  ROSE_ASSERT(classification.getDistanceFromSourceDirectory() == 8);

  // Test header filename cleanup routine

  ROSE_ASSERT(stripDotsFromHeaderFileName(". FileNameClassifier.h") ==
              "FileNameClassifier.h");
  ROSE_ASSERT(
      stripDotsFromHeaderFileName(".................. "
                                  "/usr/lib/gcc/i386-redhat-linux/3.4.6/../../"
                                  "../../include/c++/3.4.6/string") ==
      "/usr/lib/gcc/i386-redhat-linux/3.4.6/../../../../include/c++/3.4.6/"
      "string");
  ROSE_ASSERT(stripDotsFromHeaderFileName("") == "");
  ROSE_ASSERT(stripDotsFromHeaderFileName("test.h") == "test.h");
  ROSE_ASSERT(stripDotsFromHeaderFileName(" test.h") == "test.h");
  ROSE_ASSERT(stripDotsFromHeaderFileName("test... .h") == "test... .h");

  return 0;
}
