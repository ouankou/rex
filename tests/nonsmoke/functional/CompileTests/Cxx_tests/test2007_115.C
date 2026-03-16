// Wide string literals must use adjacent-literal concatenation to span lines.

class nsString2 {
public:
  explicit nsString2(int length) {}
};

void foo() {
  const nsString2 myString(sizeof(L"A DOM String, Just For "
                                  L"You") /
                           sizeof(wchar_t));
}
