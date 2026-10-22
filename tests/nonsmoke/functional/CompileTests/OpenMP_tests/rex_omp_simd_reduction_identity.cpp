namespace left {
float total = 5;
float product = 2;
float difference = 11;
} // namespace left
namespace right {
float total = 7;
float product = 3;
} // namespace right

float global_total = 3;

void accumulate(float *values, float __part0, int count) {
#pragma omp simd reduction(+ : left::total, right::total, global_total)        \
    reduction(* : left::product, right::product)                               \
    reduction(- : left::difference)
  for (int i = 0; i < count; ++i) {
    global_total += values[i];
    global_total = global_total + values[i];
    left::total += values[i];
    left::total = left::total + values[i];
    right::total += values[i] * __part0;
    left::product *= values[i];
    left::product = left::product * values[i];
    left::difference -= values[i];
    right::product *= values[i] + 1.0f;
  }
}

int main() {
  float values[4] = {1, 2, 3, 4};
  accumulate(values, 10, 4);
  if (left::total != 25 || right::total != 107 || left::product != 1152 ||
      right::product != 360 || left::difference != 1 || global_total != 23)
    return 1;
  accumulate(values, 10, 4);
  accumulate(values, 10, 0);
  return left::total == 45 && right::total == 207 && left::product == 663552 &&
                 right::product == 43200 && left::difference == -9 &&
                 global_total == 43
             ? 0
             : 2;
}
