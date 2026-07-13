#include "rose.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  bool reject_missing_use_site = false;
  std::vector<std::string> arguments;
  arguments.reserve(static_cast<size_t>(argc));
  for (int index = 0; index < argc; ++index) {
    if (std::string(argv[index]) == "--reject-missing-use-site") {
      reject_missing_use_site = true;
    } else {
      arguments.emplace_back(argv[index]);
    }
  }

  SgProject *project = frontend(arguments);
  ROSE_ASSERT(project != nullptr);

  SgTemplateTypedefDeclaration *alias = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgTemplateTypedefDeclaration)) {
    SgTemplateTypedefDeclaration *candidate =
        isSgTemplateTypedefDeclaration(node);
    if (candidate != nullptr && candidate->get_name() == "Alias") {
      ROSE_ASSERT(alias == nullptr);
      alias = candidate;
    }
  }
  ROSE_ASSERT(alias != nullptr);
  ROSE_ASSERT(alias->get_base_type() != nullptr);

  auto compact = [](std::string text) {
    text.erase(
        std::remove_if(text.begin(), text.end(),
                       [](unsigned char ch) { return std::isspace(ch); }),
        text.end());
    return text;
  };

  if (reject_missing_use_site) {
    (void)alias->get_base_type()->unparseToString();
    return 0;
  }

  SgSourceFile *source_file = SageInterface::getEnclosingSourceFile(alias);
  ROSE_ASSERT(source_file != nullptr);
  ROSE_ASSERT(alias->get_scope() != nullptr);

  SgUnparse_Info info;
  info.set_current_source_file(source_file);
  info.set_current_scope(alias->get_scope());
  info.set_template_argument_qualification_context(alias);
  info.set_reference_node_for_qualification(alias);
  info.set_language(SgFile::e_Cxx_language);
  info.set_SkipClassDefinition();
  info.set_SkipEnumDefinition();

  const std::string text =
      compact(alias->get_base_type()->unparseToString(&info));
  ROSE_ASSERT(text.find("ns::Later") != std::string::npos &&
              "Expected qualified nonreal type for dependent name");
  return 0;
}
