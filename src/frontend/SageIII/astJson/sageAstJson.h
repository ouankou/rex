#ifndef ROSE_SAGE_AST_JSON_H
#define ROSE_SAGE_AST_JSON_H

#include <string>

class SgProject;
class SgNode;
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

// Return the deterministic typed serializer signature for one AST subtree.
// Unlike unparseToString(), this compares the node/property/edge/type/symbol
// contract directly and therefore cannot repair malformed state while
// determining equivalence.
std::string canonicalSubtreeSignature(SgNode *root);

} // namespace AstJson
} // namespace Rose

#endif
