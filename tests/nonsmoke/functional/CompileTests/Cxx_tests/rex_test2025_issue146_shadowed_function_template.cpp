struct GlobalTag {
  char payload[1];
};

struct LocalTag {
  char payload[2];
};

template <typename T>
GlobalTag make_tag(T);

namespace N {
template <typename T>
LocalTag make_tag(T);

GlobalTag (*global_ptr)(int) = &::make_tag<int>;
LocalTag (*local_ptr)(int) = &make_tag<int>;
} // namespace N

int main() {
  return 0;
}
