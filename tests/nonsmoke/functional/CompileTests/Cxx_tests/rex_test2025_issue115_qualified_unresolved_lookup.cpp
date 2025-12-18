namespace ns {
void h(int);
void h(double);
} // namespace ns

template <typename T> void k(T t) { ns::h(t); }

int main() { k(1); }
