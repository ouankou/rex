// I think that this tests shuld fail, it does fail for C++, check if it fails for C++.

// Test cast of void* to int*
extern void *MY_all_alloc();

int main()
   {
  int *x = MY_all_alloc();
  return 0;
}
