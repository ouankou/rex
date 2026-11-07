// Keep this first: the builder header must not depend on the Sage AST or a PCH.
#include "sage-build.h"

int main() {
  Rose::builder::FlangDirectiveGroup group;
  group.family = Rose::builder::FlangDirectiveGroup::Family::openmp_extension;
  group.producer =
      Rose::builder::FlangDirectiveGroup::Producer::rex_openmp_parser;
  group.selected_output_text = "ompx 4_'!'";
  group.selected_output_tokens.push_back({"ompx ", std::nullopt});
  group.selected_output_tokens.push_back({"4_'!'", 2U});

  const auto copy = group;
  if (copy.family != group.family || copy.producer != group.producer ||
      copy.selected_output_tokens.size() != 2 ||
      copy.selected_output_tokens.front().quoted_literal_offset.has_value() ||
      copy.selected_output_tokens.back().quoted_literal_offset != 2U)
    return 1;

  std::string spelling;
  for (const auto &token : copy.selected_output_tokens)
    spelling += token.spelling;
  return spelling == copy.selected_output_text ? 0 : 1;
}
