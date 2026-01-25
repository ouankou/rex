module test2011_11_mpi_f08_types
  implicit none
  type :: t
    integer :: d
  end type t
end module test2011_11_mpi_f08_types

program test2011_11
  implicit none

  interface
    subroutine MPI_Startall(count, array_of_requests, ierror)
      use test2011_11_mpi_f08_types
      implicit none

      integer, intent(in) :: count
      type(t), intent(inout) :: array_of_requests(*)
      integer, optional, intent(out) :: ierror
    end subroutine
  end interface
end program test2011_11
