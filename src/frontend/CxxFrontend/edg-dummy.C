// This is a dummy library for EDG for those cases when ROSE is not configured with C/C++ analysis support and therefore
// doesn't need a C/C++ parser.

#include <map>
#include <string>

// Forward declarations
class SgSourceFile;
class SgIncludeFile;

// EDG stub function
int edg_main(int argc, char** argv, SgSourceFile& sageFile) {
    // Stub implementation - does nothing
    return 0;
}

// EDG namespace with required global variables
namespace EDG_ROSE_Translation {
    std::map<std::string, SgIncludeFile*> edg_include_file_map;
    bool suppress_detection_of_transformations = false;
}
