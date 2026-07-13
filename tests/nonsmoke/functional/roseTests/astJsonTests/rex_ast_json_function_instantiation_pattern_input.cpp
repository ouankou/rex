#include <rex_ast_json_function_instantiation_pattern_system.hpp>

#include <string>

template <typename T> struct RexAstJsonInstantiationOwner {
  struct Iterator {
    Iterator() {}
  };

  friend RexAstJsonInstantiationOwner
  operator+(RexAstJsonInstantiationOwner left,
            const RexAstJsonInstantiationOwner &) {
    return left;
  }
};

int main() {
  RexAstJsonInstantiationOwner<int> left;
  RexAstJsonInstantiationOwner<int> right;
  RexAstJsonInstantiationOwner<int> sum = left + right;
  RexAstJsonInstantiationOwner<int>::Iterator iterator;
  std::string systemTemplateBody("typed-pattern");
  return sizeof(sum) == 0 || sizeof(iterator) == 0 ||
         systemTemplateBody.empty() ||
         rexAstJsonSystemFunctionLocalInstantiation<int>() == 0;
}
