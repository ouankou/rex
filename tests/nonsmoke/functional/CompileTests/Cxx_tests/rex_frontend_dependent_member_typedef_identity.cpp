struct RexDependentTypedefTarget {
  void accept(int);
};

template <class T> class RexDependentTypedefOwner {
public:
  typedef void (T::*RexDependentCallback)(int);

  RexDependentTypedefOwner(RexDependentCallback callback, T *object)
      : callback_(callback), object_(object) {}

  void invoke(int value) { (object_->*callback_)(value); }

private:
  RexDependentCallback callback_;
  T *object_;
};

int main() {
  RexDependentTypedefTarget target;
  RexDependentTypedefOwner<RexDependentTypedefTarget> owner(
      &RexDependentTypedefTarget::accept, &target);
  owner.invoke(1);
  return 0;
}
