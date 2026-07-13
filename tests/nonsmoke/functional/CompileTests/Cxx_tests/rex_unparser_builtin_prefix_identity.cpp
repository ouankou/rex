int __builtin_rex_user_function(int value) { return value + 1; }

int __builtin_offsetof_like_user_function(int value) { return value * 2; }

struct RexExpressionRoles {
  int member[2];
  operator int() const { return member[0]; }
};

int rex_expression_roles(RexExpressionRoles value, double source) {
  int converted = value;
  int explicitly_cast = (int)source;
  return converted + explicitly_cast +
         __builtin_offsetof(RexExpressionRoles, member[1]);
}

int main() {
  return __builtin_rex_user_function(4) == 5 &&
                 __builtin_offsetof_like_user_function(3) == 6
             ? 0
             : 1;
}
