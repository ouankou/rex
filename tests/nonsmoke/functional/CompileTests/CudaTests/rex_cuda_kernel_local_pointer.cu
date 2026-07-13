__global__ void rex_cuda_kernel_local_pointer(float *input) {
  float *local = input;
  (void)local;
}

int main() {
  rex_cuda_kernel_local_pointer<<<1, 1>>>((float *)0);
  return 0;
}
