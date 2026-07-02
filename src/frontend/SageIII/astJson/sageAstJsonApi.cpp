#include "sageAstJsonPrivate.h"

namespace Rose {
namespace AstJson {

const char *checkpointName(Checkpoint checkpoint) {
  switch (checkpoint) {
  case Checkpoint::PreOmpConstruction:
    return "pre-omp-construction";
  case Checkpoint::PostOmpConstruction:
    return "post-omp-construction";
  case Checkpoint::PostOmpLowering:
    return "post-omp-lowering";
  }
  ROSE_ABORT();
}

bool checkpointEnabled(const SgSourceFile *file, Checkpoint checkpoint) {
  return hasCheckpointArgument(file, checkpoint);
}

Options optionsFromCommandLine(const SgSourceFile *file) {
  Options options;
  options.outputDirectory = defaultOutputDirectory(file);
  return options;
}

SgSourceFile *roundTripSourceFile(SgSourceFile *file, Checkpoint checkpoint) {
  return roundTripSourceFile(file, checkpoint, optionsFromCommandLine(file));
}

SgSourceFile *roundTripSourceFile(SgSourceFile *file, Checkpoint checkpoint,
                                  const Options &options) {
  if (file == nullptr || !checkpointEnabled(file, checkpoint)) {
    return file;
  }

  const std::filesystem::path path = checkpointPath(file, checkpoint, options);
  const std::string original_json = buildJson(file, checkpoint, file);
  writeFile(path, original_json);

  const std::string parsed_json = readFile(path);
  AstFileRecord ast;
  try {
    ast = parseAstFileJson(parsed_json, checkpointName(checkpoint));
  } catch (const std::exception &e) {
    const std::filesystem::path read_error_path =
        path.parent_path() / (path.stem().string() + ".read-parse-error" +
                              path.extension().string());
    writeFile(read_error_path, parsed_json);
    throw std::runtime_error("AST JSON parse failed for checkpoint " +
                             std::string(checkpointName(checkpoint)) +
                             "; wrote parsed JSON to " +
                             read_error_path.string() + ": " + e.what());
  }

  clearGlobalQualificationState();
  SgSourceFile *copy = reconstructSourceFile(ast, file);
  replaceFileInProject(file, copy);

  if (options.compareCanonicalRoundTrip) {
    GlobalQualificationStateSnapshot qualification_state;
    const std::string copied_json = buildJson(copy, checkpoint, copy);
    AstFileRecord copied_ast;
    try {
      copied_ast = parseAstFileJson(copied_json, checkpointName(checkpoint));
    } catch (const std::exception &e) {
      const std::filesystem::path copied_parse_error_path =
          path.parent_path() /
          (path.stem().string() + ".reconstructed-parse-error" +
           path.extension().string());
      writeFile(copied_parse_error_path, copied_json);
      throw std::runtime_error(
          "AST JSON reconstructed parse failed for checkpoint " +
          std::string(checkpointName(checkpoint)) +
          "; wrote reconstructed JSON to " + copied_parse_error_path.string() +
          ": " + e.what());
    }
    const std::string original_signature = semanticSignature(ast);
    const std::string copied_signature = semanticSignature(copied_ast);
    if (original_signature != copied_signature) {
      const std::filesystem::path copied_path =
          path.parent_path() /
          (path.stem().string() + ".reconstructed" + path.extension().string());
      const std::filesystem::path signature_path =
          path.parent_path() / (path.stem().string() + ".signature.txt");
      const std::filesystem::path copied_signature_path =
          path.parent_path() /
          (path.stem().string() + ".reconstructed.signature.txt");
      writeFile(copied_path, copied_json);
      writeFile(signature_path, original_signature);
      writeFile(copied_signature_path, copied_signature);
      throw std::runtime_error("AST JSON round-trip mismatch for checkpoint " +
                               std::string(checkpointName(checkpoint)) +
                               "; wrote reconstructed JSON to " +
                               copied_path.string());
    }
  }

  return copy;
}

void writeProjectJson(SgProject *project, const std::string &path,
                      const Options &options) {
  ROSE_ASSERT(project != nullptr);
  (void)options;
  writeFile(path, buildJson(project, Checkpoint::PreOmpConstruction, nullptr));
}

void writeSourceFileJson(SgSourceFile *file, Checkpoint checkpoint,
                         const std::string &path, const Options &options) {
  ROSE_ASSERT(file != nullptr);
  (void)options;
  writeFile(path, buildJson(file, checkpoint, file));
}

} // namespace AstJson
} // namespace Rose
