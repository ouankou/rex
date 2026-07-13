#include <cstdio>

namespace {
constexpr int unroll_factor = 2 + 2;
enum TransformSize : unsigned { tile_size = 1u << 1 };
} // namespace

long long typed_transform_constants(long long *values) {
  long long result = 0;

#pragma omp unroll partial(unroll_factor)
  for (long long i = 0; i < 16; i += 2)
    result += values[i];

#pragma omp tile sizes(static_cast<int>(tile_size))
  for (long long i = 14; i >= 0; i -= 2)
    values[i] += i;

  {
    constexpr unsigned unroll_factor = 2u;
#pragma omp unroll partial(static_cast<int>(unroll_factor))
    for (int i = 0; i < 8; ++i) {
      const long long visible_after_lowering = values[i] + i;
      result += visible_after_lowering;
    }
  }

#pragma omp unroll partial(-(-2))
  for (int i = 7; i >= 0; --i)
    result += values[i];

  return result;
}

int main() {
  long long values[16];
  for (int i = 0; i < 16; ++i)
    values[i] = i * 13 + 5;
  const long long result = typed_transform_constants(values);
  long long checksum = result;
  for (long long value : values)
    checksum += value;
  std::printf("%lld %lld\n", result, checksum);
  return 0;
}
