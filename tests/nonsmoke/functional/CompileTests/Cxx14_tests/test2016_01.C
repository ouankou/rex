

#include <typeinfo>

#include <iostream>

int main(int argc, char *argv[]) {
  int a = 0;
  std::string test = "testing";

  auto my_lambda = [=](auto val) {
    std::cout << val << " is a " << typeid(decltype(val)).name() << std::endl;
  };

  my_lambda(1);
  my_lambda(a);
  my_lambda(test);
}
