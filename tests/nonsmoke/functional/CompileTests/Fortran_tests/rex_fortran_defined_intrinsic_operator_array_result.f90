module rex_fortran_defined_intrinsic_operator_array_result_m
  implicit none

  interface operator(.or.)
    module procedure add_elements
  end interface

  interface operator(//)
    module procedure add_elements
  end interface

  interface operator(*)
    module procedure matrix_times_vector
  end interface

contains

  elemental integer function add_elements(left, right) result(value)
    integer, intent(in) :: left
    integer, intent(in) :: right

    value = left + right
  end function add_elements

  function matrix_times_vector(matrix, vector) result(value)
    integer, intent(in) :: matrix(2, 2)
    integer, intent(in) :: vector(2)
    integer :: value(2)

    value = matmul(matrix, vector)
  end function matrix_times_vector

end module rex_fortran_defined_intrinsic_operator_array_result_m

program rex_fortran_defined_intrinsic_operator_array_result
  use rex_fortran_defined_intrinsic_operator_array_result_m
  implicit none

  integer :: matrix(2, 2)
  integer :: vector(2)

  matrix = reshape([1, 2, 3, 4], shape(matrix))
  vector = [5, 6]

  if (any((matrix .or. matrix) /= matrix + matrix)) error stop
  if (any((matrix // matrix) /= matrix + matrix)) error stop
  if (any((matrix * vector) /= matmul(matrix, vector))) error stop
end program rex_fortran_defined_intrinsic_operator_array_result
