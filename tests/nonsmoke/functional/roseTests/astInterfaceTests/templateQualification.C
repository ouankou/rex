#include "rose.h"

#include <algorithm>

#include <cctype>

#include <cstdlib>

#include <fstream>

#include <iterator>

#include <map>

#include <string>

namespace {
std::string makeOutputName(const std::string &input_path) {
  std::string::size_type pos = input_path.find_last_of("/\\");
  std::string base =
      (pos == std::string::npos) ? input_path : input_path.substr(pos + 1);
  return "rose_" + base;
}

std::string locateOutputFile(const std::string &output_name) {
  if (const char *output_dir = std::getenv("ROSE_TEST_OUTPUT_DIR");
      output_dir != nullptr && output_dir[0] != '\0') {
    std::string resolved = std::string(output_dir) + "/" + output_name;
    std::ifstream resolved_stream(resolved.c_str(), std::ios::in);
    if (resolved_stream.good()) {
      return resolved;
    }
  }

  std::ifstream local_stream(output_name.c_str(), std::ios::in);
  if (local_stream.good())
    return output_name;

  return output_name;
}

std::string readFile(const std::string &path) {
  std::ifstream in(path.c_str(), std::ios::in);
  ROSE_ASSERT(in.good());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void requireParameterListParents(SgProject *project) {
  ROSE_ASSERT(project != NULL);

  Rose::MemoryPoolTraversalFilter prev_filter =
      Rose::getMemoryPoolTraversalFilter();
  Rose::setMemoryPoolTraversalFilter(NULL);

  std::map<SgFunctionParameterList *, SgFunctionDeclaration *> owners;
  VariantVector fn_variants(V_SgFunctionDeclaration);
  NodeQuerySynthesizedAttributeType functions =
      NodeQuery::queryMemoryPool(fn_variants);
  for (SgNode *n : functions) {
    if (SgFunctionDeclaration *fn = isSgFunctionDeclaration(n)) {
      if (SgFunctionParameterList *plist = fn->get_parameterList()) {
        owners.emplace(plist, fn);
      }
    }
  }

  VariantVector param_variants(V_SgFunctionParameterList);
  NodeQuerySynthesizedAttributeType params =
      NodeQuery::queryMemoryPool(param_variants);
  for (SgNode *n : params) {
    SgFunctionParameterList *plist = isSgFunctionParameterList(n);
    if (plist == NULL)
      continue;
    if (plist->get_parent() != NULL)
      continue;

    SgLocatedNode *loc = isSgLocatedNode(plist);
    Sg_File_Info *fi = loc != NULL ? loc->get_file_info() : NULL;
    std::cerr << "FATAL: parameter list without parent: " << plist;
    if (fi != NULL) {
      std::cerr << " at " << fi->get_filenameString() << ":" << fi->get_line();
    }
    std::cerr << std::endl;

    auto it = owners.find(plist);
    if (it != owners.end() && it->second != NULL) {
      std::cerr << "  owner candidate: " << it->second->class_name() << " "
                << it->second->get_qualified_name().str() << std::endl;
    }

    Rose::setMemoryPoolTraversalFilter(prev_filter);
    ROSE_ABORT();
  }

  Rose::setMemoryPoolTraversalFilter(prev_filter);
}
} // namespace

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != NULL);

  requireParameterListParents(project);

  const SgFilePtrList &files = project->get_fileList();
  ROSE_ASSERT(files.empty() == false);
  std::string input_path = files.front()->get_sourceFileNameWithPath();

  int backend_status = backend(project);
  ROSE_ASSERT(backend_status == 0);

  std::string output_path = locateOutputFile(makeOutputName(input_path));
  std::string output = readFile(output_path);
  std::string normalized = output;
  normalized.erase(
      std::remove_if(normalized.begin(), normalized.end(),
                     [](unsigned char c) { return std::isspace(c); }),
      normalized.end());

  ROSE_ASSERT(normalized.find("template<intN>") != std::string::npos);
  ROSE_ASSERT(normalized.find("Array<int,10>") != std::string::npos);
  ROSE_ASSERT(normalized.find("template<0>") == std::string::npos);
  ROSE_ASSERT(normalized.find("::std::vector") == std::string::npos);
  ROSE_ASSERT(normalized.find("<::std::tuple") == std::string::npos);

  return 0;
}
