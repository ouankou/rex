// Partial specializations should keep definitions and ordering intact.
template<class T, class U>
struct PairWrap {
    using first = T;
    using second = U;
};

template<class T>
struct PairWrap<T, T> {
    using same = T;
};

PairWrap<int, int> same_ints;
PairWrap<int, double> mixed_pair;

int main() {
    (void)same_ints;
    (void)mixed_pair;
    return 0;
}
