program intrinsic_use_duplicate_same_entity
  use, intrinsic :: iso_c_binding, only : c_int, c_int
  integer(c_int) :: value

  value = 7_c_int
end program intrinsic_use_duplicate_same_entity
