struct RexExactThisTypes {
  RexExactThisTypes *mutable_this() { return this; }

  const volatile RexExactThisTypes *cv_this() const volatile { return this; }

  RexExactThisTypes *lambda_this() {
    return [this]() { return this; }();
  }

  RexExactThisTypes *implicit_lambda_this() {
    return [=]() { return this; }();
  }
};

using RexTargetSizeType = decltype(sizeof(int));

RexTargetSizeType rex_size_expression(int value) { return sizeof value; }

RexTargetSizeType rex_size_type() { return sizeof(long double); }

RexTargetSizeType rex_align_type() { return alignof(long double); }

template <class T> using RexNewArrayAlias = T[4];

struct RexNewArrayOwner {
  template <class T, int N = 3> using Row = T[N];
};

void rex_new_array_result_types(int count) {
  using RexNewArray = int[3];
  int *scalar = new int(7);
  int *elements = new int[count];
  int (*rows)[3] = new int[count][3];
  int *inferred = new int[]{1, 2};
  int (*inferred_rows)[3] = new int[][3]{{1, 2, 3}, {4, 5, 6}};
  int *alias_elements = new RexNewArray;
  const int *qualified_alias_elements = new const RexNewArray{};
  int **pointer_elements = new int *[count];
  int (*parenthesized_rows)[3] = new (int[2][3]);
  int *template_alias_elements = new RexNewArrayAlias<int>;
  const int *qualified_template_alias_elements =
      new const RexNewArrayAlias<int>{};
  int (*template_alias_rows)[4] = new RexNewArrayAlias<int>[count];
  int *member_alias_elements = new RexNewArrayOwner::Row<int>;
  int (*member_alias_rows)[3] = new RexNewArrayOwner::Row<int>[count];
  int array_type_operand[2];
  int *decltype_elements = new decltype(array_type_operand);
  delete scalar;
  delete[] elements;
  delete[] rows;
  delete[] inferred;
  delete[] inferred_rows;
  delete[] alias_elements;
  delete[] qualified_alias_elements;
  delete[] pointer_elements;
  delete[] parenthesized_rows;
  delete[] template_alias_elements;
  delete[] qualified_template_alias_elements;
  delete[] template_alias_rows;
  delete[] member_alias_elements;
  delete[] member_alias_rows;
  delete[] decltype_elements;
}
