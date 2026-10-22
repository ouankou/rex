#include "sageAstJsonPrivate.h"

#include <string>

namespace {
SgMacroExpansionExp *makeMacro() {
  auto *value = SageBuilder::buildIntVal(7);
  auto *macro = new SgMacroExpansionExp("REX_VALUE(7)", value);
  SageInterface::setSourcePositionForTransformation(macro);
  ROSE_ASSERT(value->get_isModified() && value->isTransformation());
  ROSE_ASSERT(!macro->get_expansion_is_modified());
  return macro;
}

void check(SgMacroExpansionExp *macro, bool edited, int value) {
  ROSE_ASSERT(macro->get_expansion_is_modified() == edited);
  auto *literal = isSgIntVal(macro->get_expanded_expression_checked());
  ROSE_ASSERT(literal != nullptr && literal->get_value() == value);
  ROSE_ASSERT(macro->unparseToString() ==
              (edited ? std::to_string(value) : "REX_VALUE(7)"));
}

void roundTrip(bool edited) {
  using namespace Rose::AstJson;
  auto *macro = makeMacro();
  if (edited)
    isSgIntVal(macro->get_expanded_expression_checked())->set_value(8);
  check(macro, edited, edited ? 8 : 7);
  SgTreeCopy copier;
  auto *copy = isSgMacroExpansionExp(macro->copy(copier));
  check(copy, edited, edited ? 8 : 7);
  const auto record = parseJson(rawTypeOwnedExpressionRef(macro, {}));
  auto *restored = isSgMacroExpansionExp(expressionFromRef(record, {}));
  check(restored, edited, edited ? 8 : 7);
  for (auto *candidate : {copy, restored}) {
    isSgIntVal(candidate->get_expanded_expression_checked())->set_value(9);
    check(candidate, true, 9);
    check(macro, edited, edited ? 8 : 7);
  }
  for (const char *invalid : {"null", "0", "\"false\""}) {
    auto malformed = record;
    malformed.object.at("properties").object.at("expansion_is_modified") =
        parseJson(invalid);
    bool rejected = false;
    try {
      (void)expressionFromRef(malformed, {});
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    ROSE_ASSERT(rejected);
  }
  auto malformed = record;
  malformed.object.at("properties").object.erase("expansion_is_modified");
  bool rejected = false;
  try {
    (void)expressionFromRef(malformed, {});
  } catch (const std::runtime_error &error) {
    rejected = std::string(error.what()).find("expansion_is_modified") !=
               std::string::npos;
  }
  ROSE_ASSERT(rejected);
  SageInterface::deleteAST(copy);
  SageInterface::deleteAST(restored);
  SageInterface::deleteAST(macro);
}
} // namespace

int main() {
  roundTrip(false);
  roundTrip(true);
  auto *macro = makeMacro();
  auto *original = macro->get_expanded_expression_checked();
  auto *replacement = SageBuilder::buildIntVal(10);
  ROSE_ASSERT(macro->replace_expression(original, replacement) == 1);
  check(macro, true, 10);
  SageInterface::deleteAST(original);
  SageInterface::deleteAST(macro);
}
