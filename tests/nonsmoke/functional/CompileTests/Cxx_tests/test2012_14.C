// #include <iostream>
// using namespace std;

template<class T, class U, int I> 
struct X
{ void f() { 
// cout << "Primary template" << endl; 
} };

template<class T, int I> 
struct X<T, T*, I>
  { void f() { 
// cout << "Partial specialization 1" << endl;
  } };

int main() {
  // X<int, int, 10> a;
  X<int, int *, 5> b;
}
