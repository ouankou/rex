

struct A;

struct A {  int field; } B;

#if __cplusplus
void foo(A *ptr) {  ptr->field; }
#else
void foo(struct A *ptr) {  ptr->field; }
#endif

