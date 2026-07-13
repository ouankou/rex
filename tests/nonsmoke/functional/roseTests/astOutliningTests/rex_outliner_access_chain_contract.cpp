class rex_outliner_access_owner {
  int hidden_value;

protected:
  int increment(int value) const;
  int increment_cv(int value) const volatile;

public:
  int evaluate() const {
    int result = hidden_value;
#pragma rose_outline
    for (int index = 0; index < 2; ++index) {
      result = increment(result);
    }
    return result;
  }

  int evaluate_cv() const volatile {
    int result = hidden_value;
#pragma rose_outline
    for (int index = 0; index < 2; ++index) {
      result = increment_cv(result);
    }
    return result;
  }
};

int rex_outliner_access_owner::increment(int value) const { return value + 1; }

int rex_outliner_access_owner::increment_cv(int value) const volatile {
  return value + 1;
}
