
#include "sage3basic.h"

#include "includeFileSupport.h"

using namespace std;

IncludeFileSupport::InheritedAttribute::InheritedAttribute() {
  path_prefix = "";
}

IncludeFileSupport::InheritedAttribute::InheritedAttribute(
    const IncludeFileSupport::InheritedAttribute &X) {
  path_prefix = X.path_prefix;
}

IncludeFileSupport::SynthesizedAttribute::SynthesizedAttribute() {
  path_prefix = "";
  include_file = NULL;
}

IncludeFileSupport::SynthesizedAttribute::SynthesizedAttribute(
    SgIncludeFile *input_include_file) {
  ASSERT_not_null(input_include_file);

  path_prefix = "";
  include_file = input_include_file;
}

IncludeFileSupport::SynthesizedAttribute::SynthesizedAttribute(
    const IncludeFileSupport::SynthesizedAttribute &X) {
  path_prefix = X.path_prefix;
  include_file = X.include_file;
}

IncludeFileSupport::InheritedAttribute
IncludeFileSupport::PrefixTraversal::evaluateInheritedAttribute(
    SgNode *node, IncludeFileSupport::InheritedAttribute inheritedAttribute) {
  // IncludeFileSupport::InheritedAttribute ih;
  IncludeFileSupport::InheritedAttribute ih(inheritedAttribute);

  SgIncludeFile *includeFile = isSgIncludeFile(node);
  if (includeFile != NULL) {

    // ROSE_ASSERT(includeFile->get_directory_prefix().is_null() == false);
    if (includeFile->get_directory_prefix() != ".") {
      // ih.path_prefix += includeFile->get_directory_prefix();
      ih.path_prefix =
          inheritedAttribute.path_prefix + includeFile->get_directory_prefix();
    }
  }

  return ih;
}

IncludeFileSupport::SynthesizedAttribute
IncludeFileSupport::PrefixTraversal::evaluateSynthesizedAttribute(
    SgNode *node, IncludeFileSupport::InheritedAttribute inheritedAttribute,
    SubTreeSynthesizedAttributes synthesizedAttributeList) {

  SgIncludeFile *includeFile = isSgIncludeFile(node);
  ASSERT_not_null(includeFile);

  SynthesizedAttribute syn_attribute(includeFile);

  if (includeFile != NULL) {
    // DQ (11/17/2018): the default setting for the
    // rose_required_macros_and_functions.h is true, since it is not inlcuded
    // in the children.
    bool unparseAllChildren = true;

    // Loop over the children.
    for (size_t i = 0; i < synthesizedAttributeList.size(); i++) {
      SgIncludeFile *child_include_file =
          synthesizedAttributeList[i].include_file;
      ASSERT_not_null(child_include_file);
      // if (child_include_file->get_will_be_unparsed() == false)
      if (child_include_file->get_will_be_unparsed() == false &&
          child_include_file->get_isPreinclude() == false) {
        // DQ (11/17/2018): If one of the children will not be unparsed, then we
        // need to provide a path to the original header file so that it can be
        // found at compile time.
        unparseAllChildren = false;
      }
    }
    if (unparseAllChildren == false) {
      for (size_t i = 0; i < synthesizedAttributeList.size(); i++) {
        SgIncludeFile *child_include_file =
            synthesizedAttributeList[i].include_file;
        if (child_include_file->get_will_be_unparsed() == false &&
            child_include_file->get_isPreinclude() == false) {
          // child_include_file->set_requires_explict_path_for_unparsed_headers(true);
          if (child_include_file->get_isSystemInclude() == false) {
            child_include_file->set_requires_explict_path_for_unparsed_headers(
                true);
          } else {
          }
        }
      }
    }

    // syn_attribute.path_prefix = "set_in_evaluateSynthesizedAttribute";
    syn_attribute.path_prefix = inheritedAttribute.path_prefix;

    for (size_t i = 0; i < synthesizedAttributeList.size(); i++) {
      SgIncludeFile *child_include_file =
          synthesizedAttributeList[i].include_file;
      ASSERT_not_null(child_include_file);
      if (child_include_file
              ->get_requires_explict_path_for_unparsed_headers() == true) {
        // Add a path to the added_include_path_set.
        // string path = includeFile->get_filename() + "___" +
        // inheritedAttribute.path_prefix; string path =
        // XXX::xxx(child_include_file->get_filename());
        string path =
            Rose::getPathFromFileName(child_include_file->get_filename());

        // if (syn_attribute.added_include_path_set.find(path) !=
        // syn_attribute.added_include_path_set.end())
        if (added_include_path_set.find(path) == added_include_path_set.end()) {
          // syn_attribute.added_include_path_set.insert(path);

          added_include_path_set.insert(path);
        }
      }
    }
  }

  return syn_attribute;
}

std::set<std::string>
IncludeFileSupport::headerFilePrefix(SgIncludeFile *includeFile) {
  InheritedAttribute ih;

  // Now buid the traveral object and call the traversal (preorder) on the
  // function definition.
  IncludeFileSupport::PrefixTraversal traversal;
  traversal.traverse(includeFile, ih);
  std::set<std::string> added_include_path_set =
      traversal.added_include_path_set;
  return added_include_path_set;
}
