// Minimal template instantiation test
template<typename T, int N>
class Array {
    T data[N];
};

int main() {
    Array<double, 10> arr;
    return 0;
}
