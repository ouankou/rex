template <typename T>
class MyClass {
public:
    friend void foo(MyClass<T> &m) {
        if (true) {
            return;
        }
        int x = 0;
    }
};

void test() {
    MyClass<int> m;
    foo(m);
}
