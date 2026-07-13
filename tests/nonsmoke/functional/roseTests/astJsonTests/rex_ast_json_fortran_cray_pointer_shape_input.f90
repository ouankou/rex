program rex_ast_json_fortran_cray_pointer_shape
  implicit none
  pointer (shape_address, shaped_pointee(2:9))
  real :: shaped_pointee
  shaped_pointee(2) = 1.0
end program rex_ast_json_fortran_cray_pointer_shape
