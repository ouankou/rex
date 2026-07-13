#include <iostream>

using namespace std;

template <class T> void emit_values(const T *values, int count) {
  for (int index = 0; index < count; ++index) {
    cout << values[index] << ' ';
  }
  cout << endl;
}

int main() {
  const int values[] = {1, 2, 3};
  emit_values(values, 3);
}
