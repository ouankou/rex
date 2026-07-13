subroutine rex_fortran_openacc_semantic_positive(values, enabled)
  implicit none
  real, intent(inout) :: values(16)
  logical, intent(in) :: enabled
  integer :: index

!$acc parallel loop if(enabled)
  do index = 1, 16
    values(index) = values(index) + 1.0
  end do
end subroutine rex_fortran_openacc_semantic_positive
