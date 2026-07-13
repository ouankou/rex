template <typename Callable>
__global__ void rex_cuda_deduced_kernel(Callable callable) {
  callable();
}

int main() {
  rex_cuda_deduced_kernel<<<1, 1>>>([] __device__() {});
  return 0;
}
