module rex_test2026_cuda_kernel
contains
  attributes(global) subroutine saxpy_kernel(a, x, y)
    implicit none
    integer :: a
    real, device :: x(:)
    real, device :: y(:)
  end subroutine saxpy_kernel
end module rex_test2026_cuda_kernel

program rex_test2026_cuda_kernel_launch_call
  use rex_test2026_cuda_kernel
  implicit none
  real, device :: x(10)
  real, device :: y(10)
  real, unified :: u(10)
  call saxpy_kernel<<<1,1>>>(1, x, y)
end program rex_test2026_cuda_kernel_launch_call
