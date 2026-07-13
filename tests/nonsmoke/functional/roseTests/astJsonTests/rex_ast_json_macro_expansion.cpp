#include "rose.h"

#include <iostream>

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  Rose_STL_Container<SgNode *> macros =
      NodeQuery::querySubTree(project, V_SgMacroExpansionExp);
  if (macros.size() != 1) {
    std::cerr << "expected exactly one macro expansion expression, found "
              << macros.size() << "\n";
    return 1;
  }

  SgMacroExpansionExp *macro = isSgMacroExpansionExp(macros.front());
  SgExpression *expanded =
      macro != nullptr ? macro->get_expanded_expression() : nullptr;
  if (macro == nullptr ||
      macro->get_spelling() != "REX_AST_JSON_MACRO_CALL(41)" ||
      isSgFunctionCallExp(expanded) == nullptr ||
      expanded->get_parent() != macro || macro->get_type() == nullptr) {
    std::cerr << "macro expansion spelling or semantic edge did not survive "
                 "AST JSON reconstruction\n";
    return 1;
  }

  Rose_STL_Container<SgNode *> strings =
      NodeQuery::querySubTree(project, V_SgStringVal);
  SgStringVal *raw = nullptr;
  SgStringVal *unevaluated = nullptr;
  for (SgNode *node : strings) {
    SgStringVal *candidate = isSgStringVal(node);
    if (candidate != nullptr && candidate->get_isRawString()) {
      if (raw != nullptr) {
        std::cerr << "expected one raw string literal after AST JSON "
                     "reconstruction\n";
        return 1;
      }
      raw = candidate;
    }
    if (candidate != nullptr && candidate->get_cxx_unevaluated()) {
      if (unevaluated != nullptr) {
        std::cerr << "expected one unevaluated string literal after AST JSON "
                     "reconstruction\n";
        return 1;
      }
      unevaluated = candidate;
    }
  }
  if (raw == nullptr ||
      raw->get_literal_encoding() != SgStringVal::e_string_encoding_utf8 ||
      raw->get_raw_string_delimiter() != "json_tag" ||
      raw->get_raw_string_payload() != "alpha  beta\ngamma" ||
      raw->get_cxx_literal_spelling() !=
          "u8R\"json_tag(alpha  beta\ngamma)json_tag\"") {
    std::cerr << "typed raw-string surface did not survive AST JSON "
                 "reconstruction\n";
    return 1;
  }
  if (unevaluated == nullptr ||
      unevaluated->get_literal_encoding() !=
          SgStringVal::e_string_encoding_utf8 ||
      unevaluated->get_cxx_literal_spelling() != "u8\"ast json unevaluated\"") {
    std::cerr << "typed unevaluated string surface did not survive AST JSON "
                 "reconstruction\n";
    return 1;
  }

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
