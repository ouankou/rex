#include <cuda_runtime.h>

__global__ __launch_bounds__(128) void rex_cuda_single() {}

__global__ __launch_bounds__(128, 2) void rex_cuda_pair() {}

__global__ __launch_bounds__(128, 2, 4) void rex_cuda_cluster() {}

template <int Threads>
__global__ __launch_bounds__(Threads, 2) void rex_cuda_dependent() {}
