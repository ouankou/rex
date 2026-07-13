      program requires_dynamic_allocators_type
      use omp_lib
      implicit none

      integer, allocatable :: x(:)
      integer :: i
      integer(omp_memspace_handle_kind) :: memspace = omp_default_mem_space
      type(omp_alloctrait) :: traits(1) = [omp_alloctrait(omp_atk_alignment, 64)]
      integer(omp_allocator_handle_kind) :: alloc

!$omp requires dynamic_allocators
      alloc = omp_init_allocator(memspace, 1, traits)
!$omp allocate(x) allocator(alloc)
      allocate(x(8))
      x = [(i, i = 1, 8)]
      print *, 'sum', sum(x)
      deallocate(x)
      call omp_destroy_allocator(alloc)
      end program requires_dynamic_allocators_type
