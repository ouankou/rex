namespace rex_lambda_return_target {
struct Result {};
} // namespace rex_lambda_return_target

namespace rex_lambda_return_use {
struct Result {};

int consume(rex_lambda_return_target::Result) { return 0; }

int exercise() {
  auto make_result = []() -> rex_lambda_return_target::Result { return {}; };
  return consume(make_result());
}
} // namespace rex_lambda_return_use

int main() { return rex_lambda_return_use::exercise(); }
