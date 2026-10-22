program rex_flang_enum_end_label
  implicit none
  enum, bind(c)
    enumerator :: red = 1
30 end enum
  if (red /= 1) error stop
end program rex_flang_enum_end_label
