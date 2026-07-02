#ifndef ROSE_SAGE_AST_JSON_H
#define ROSE_SAGE_AST_JSON_H

#include <string>

class SgProject;
class SgSourceFile;

namespace Rose {
namespace AstJson {

enum class Checkpoint {
  PreOmpConstruction,
  PostOmpConstruction,
  PostOmpLowering,
};

struct Options {
  std::string outputDirectory;
  bool compareCanonicalRoundTrip = true;
};

const char *checkpointName(Checkpoint checkpoint);

bool checkpointEnabled(const SgSourceFile *file, Checkpoint checkpoint);
Options optionsFromCommandLine(const SgSourceFile *file);

SgSourceFile *roundTripSourceFile(SgSourceFile *file, Checkpoint checkpoint);
SgSourceFile *roundTripSourceFile(SgSourceFile *file, Checkpoint checkpoint,
                                  const Options &options);

void writeProjectJson(SgProject *project, const std::string &path,
                      const Options &options);
void writeSourceFileJson(SgSourceFile *file, Checkpoint checkpoint,
                         const std::string &path, const Options &options);

} // namespace AstJson
} // namespace Rose

#endif
