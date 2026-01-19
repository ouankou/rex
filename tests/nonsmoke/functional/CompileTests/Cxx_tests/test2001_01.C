
// Unparser Bug

// ((Array_Domain_Type*)(this))->IndexBase [i] =
// Index_Array[i]->Array_Descriptor.Array_Domain.Base[i];
// ((Array_Domain_Type & )(*this).IndexBase)[i] = ((((*(this ->
// Index_Array)[i]).Array_Descriptor).Array_Domain).Base)[i];

// #include<stdio.h>

// const void* NULL = 0;

class A {
public:
  A();
};

A::A() {
  // The test code tests the unparsing of the
  // "this" pointer when it is used explicitly
  const A *pointer = this;
  // assert (pointer != NULL);
  // exit(1);
  // abort(1);
}

int main() {
  // Build object so that we can call the constructor
  A objectA1;
  A objectA2 = A();
  A objectA3 = objectA1;

  // printf ("Program Terminated Normally! \n");
  return 0;
}
