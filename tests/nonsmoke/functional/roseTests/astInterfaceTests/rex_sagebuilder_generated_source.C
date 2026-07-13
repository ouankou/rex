#include "rose.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
  std::cerr << "generated-source contract failure: " << message << '\n';
  return 1;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 || std::string(argv[1]).empty()) {
    return fail("expected one output filename");
  }

  const std::filesystem::path firstOutput = argv[1];
  const auto siblingOutput = [&](const std::string &suffix) {
    return firstOutput.parent_path() / (firstOutput.stem().string() + suffix +
                                        firstOutput.extension().string());
  };
  const std::vector<std::filesystem::path> outputs = {
      firstOutput, siblingOutput("_second"), siblingOutput("_third")};
  const std::filesystem::path sourceLookingOptionValue =
      siblingOutput("_forced_include");

  std::vector<std::filesystem::path> legacyDummies;
  std::vector<std::filesystem::path> objects;
  std::vector<std::string> canonicalOutputs;
  std::set<std::string> distinctOutputIdentities;
  std::set<std::string> objectIdentities;
  for (const std::filesystem::path &output : outputs) {
    legacyDummies.push_back(output.parent_path() /
                            ("temp_dummy_file_" + output.filename().string()));
    objects.push_back(output.parent_path() / (output.stem().string() + ".o"));
    const std::string canonicalOutput =
        std::filesystem::weakly_canonical(std::filesystem::absolute(output))
            .string();
    canonicalOutputs.push_back(canonicalOutput);
    distinctOutputIdentities.insert(canonicalOutput);
    objectIdentities.insert(std::filesystem::absolute(objects.back()).string());
    std::filesystem::remove(output);
    std::filesystem::remove(legacyDummies.back());
    std::filesystem::remove(objects.back());
  }
  std::filesystem::remove(sourceLookingOptionValue);
  if (distinctOutputIdentities.size() != outputs.size() ||
      objectIdentities.size() != objects.size()) {
    return fail("test setup did not provide distinct output identities");
  }

  {
    std::ofstream forcedInclude(sourceLookingOptionValue);
    if (!forcedInclude) {
      return fail("cannot create the source-looking option value");
    }
    forcedInclude << "// Deliberately named like a source file.\n";
  }

  SgProject *project = new SgProject();
  if (project == nullptr) {
    return fail("cannot construct the existing project");
  }
  project->get_fileList().clear();
  project->set_compileOnly(true);
  project->get_originalCommandLineArgumentList() = {
      "cc", "-c", "-include", sourceLookingOptionValue.string()};

  std::vector<SgSourceFile *> sources;
  for (size_t index = 0; index < outputs.size(); ++index) {
    SgSourceFile *source =
        SageBuilder::buildGeneratedSourceFile(outputs[index].string(), project);
    if (source == nullptr || !source->get_isGeneratedSource()) {
      return fail("builder did not publish generated-source identity");
    }
    if (source->get_globalScope() == nullptr ||
        source->get_project() != project) {
      return fail("builder did not attach a generated translation unit");
    }
    if (source->get_sourceFileNameWithPath() != canonicalOutputs[index] ||
        source->get_file_info()->get_filenameString() !=
            canonicalOutputs[index] ||
        source->get_unparse_output_filename() != canonicalOutputs[index]) {
      return fail("builder did not publish one canonical output identity");
    }
    const std::vector<std::string> &ownedCommand =
        source->get_originalCommandLineArgumentList();
    const Rose_STL_Container<std::string> ownedSources =
        CommandlineProcessing::generateSourceFilenames(
            ownedCommand, project->get_binary_only());
    if (ownedSources.size() != 1 ||
        ownedSources.front() != canonicalOutputs[index]) {
      return fail("generated translation unit does not own its canonical "
                  "source command");
    }
    if (std::filesystem::exists(outputs[index]) ||
        std::filesystem::exists(legacyDummies[index])) {
      return fail("builder performed file I/O before unparsing");
    }

    const std::string declarationName =
        "generated_contract_" + std::to_string(index);
    SgVariableDeclaration *declaration = SageBuilder::buildVariableDeclaration(
        declarationName, SageBuilder::buildIntType(), nullptr,
        source->get_globalScope());
    SageInterface::appendStatement(declaration, source->get_globalScope());
    sources.push_back(source);
  }

  if (project->get_fileList().size() != sources.size()) {
    return fail("project does not own every generated translation unit");
  }
  for (size_t index = 0; index < sources.size(); ++index) {
    if (project->get_fileList()[index] != sources[index]) {
      return fail("project generated-source ownership is not exact");
    }
  }

  project->unparse();
  for (size_t index = 0; index < outputs.size(); ++index) {
    if (!std::filesystem::exists(outputs[index])) {
      return fail("unparser did not create every generated output");
    }
    if (std::filesystem::exists(legacyDummies[index])) {
      return fail("legacy dummy input was created");
    }

    std::ifstream input(outputs[index]);
    if (!input) {
      return fail("generated output cannot be opened");
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad()) {
      return fail("generated output cannot be read");
    }
    for (size_t declarationIndex = 0; declarationIndex < outputs.size();
         ++declarationIndex) {
      const std::string declarationName =
          "generated_contract_" + std::to_string(declarationIndex);
      const bool containsDeclaration =
          contents.str().find(declarationName) != std::string::npos;
      if (containsDeclaration != (declarationIndex == index)) {
        return fail("generated output has the wrong translation-unit content");
      }
    }
  }

  if (project->compileOutput() != 0) {
    return fail("backend did not compile every generated translation unit");
  }
  for (const std::filesystem::path &object : objects) {
    if (!std::filesystem::exists(object) ||
        std::filesystem::file_size(object) == 0) {
      return fail("backend did not create every distinct generated object");
    }
  }

  for (size_t index = 0; index < outputs.size(); ++index) {
    std::filesystem::remove(objects[index]);
    std::filesystem::remove(outputs[index]);
    std::filesystem::remove(legacyDummies[index]);
  }
  std::filesystem::remove(sourceLookingOptionValue);
  return 0;
}
