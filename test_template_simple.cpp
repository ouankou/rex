// Self-contained template test without system headers
template<typename T, int N>
struct MyArray {
    T data[N];
    T& operator[](int i) { return data[i]; }
    int size() const { return N; }
};

int main() {
    MyArray<double, 1024> x;
    MyArray<int, 10> y;

    x[0] = 3.14;
    y[0] = 42;

    return x.size() + y.size();
}
