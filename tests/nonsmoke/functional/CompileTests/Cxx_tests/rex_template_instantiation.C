// Namespace-qualified instantiations, dependent template specializations, template-template args, and packs.
#include <array>
#include <chrono>
#include <ratio>
#include <tuple>
#include <vector>

template<class T>
struct WithRebind {
    template<class U>
    using rebind = std::vector<U>;
    using value_type = T;
};

template<class T>
struct UsesDependent {
    using rebound = typename T::template rebind<int>;
    rebound data;
};

template<template<class...> class C, class... Args>
struct UsePack {
    C<Args...> value;
};

int main() {
    std::array<int, 3> a = {1, 2, 3};
    std::chrono::duration<double, std::ratio<1, 1000>> ms(1.5);

    UsesDependent<WithRebind<float>> dep;
    dep.data.push_back(0);

    UsePack<std::tuple, int, double> pack;

    (void)a;
    (void)ms;
    (void)dep;
    (void)pack;
    return 0;
}
