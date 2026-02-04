program rex_test2026_allocate_specs
  implicit none
  integer, allocatable :: a(:)
  integer :: stat
  integer :: stream
  logical :: pinned
  integer :: mold_source(5)
  allocate(a(5), mold=mold_source, stream=stream, pinned=pinned, stat=stat)
end program rex_test2026_allocate_specs
