typedef class RexInlineClass {
public:
  explicit RexInlineClass(int input) : value(input) {}
  int value;
} RexInlineClassAlias;

typedef struct RexInlineStruct {
  int value;
} *RexInlineStructPointer;

int main() {
  RexInlineClassAlias object(7);
  RexInlineStruct value = {object.value};
  RexInlineStructPointer pointer = &value;
  return pointer->value != 7;
}
