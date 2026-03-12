// ROSE-2505 (Tristan)

template <typename t_type2> class Class1 {
public:
  const t_type2 *func3() const { return &value_; }
  void func2(const t_type2 &value) { value_ = value; }
  void func1(const Class1<t_type2> &values);

private:
  t_type2 value_;
};

template <typename t_type2>
void Class1<t_type2>::func1(const Class1<t_type2> &values) {
  this->func2(*values.func3());
}

int exercise_test2020_80() {
  Class1<int> lhs;
  const Class1<int> rhs{};
  lhs.func1(rhs);
  return 0;
}
