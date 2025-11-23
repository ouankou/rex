// Std template-template parameters and packs.
#include <map>
#include <vector>

template<template<class...> class C, class... Args>
struct UsesStdTT {
    C<Args...> value;
};

UsesStdTT<std::vector, int> vec_holder;
UsesStdTT<std::map, int, int> map_holder;

int main() {
    vec_holder.value.push_back(1);
    map_holder.value[1] = 2;
    return 0;
}
