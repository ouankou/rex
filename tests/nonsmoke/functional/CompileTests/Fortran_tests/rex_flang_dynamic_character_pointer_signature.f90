program rex_flang_dynamic_character_pointer_signature
  implicit none
  character(4), dimension(2), target :: rex_storage
  character(4), dimension(:), pointer :: rex_pointer

  rex_storage = 'rex'
  rex_pointer => rex_make_pointer(rex_storage, 4)
  call rex_bind_pointer(rex_pointer, 4)

contains

  function rex_make_pointer(rex_source, rex_length) result(rex_result)
    character(*), dimension(:), target, intent(in) :: rex_source
    integer, intent(in) :: rex_length
    character(rex_length), dimension(:), pointer :: rex_result

    rex_result => rex_source
  end function rex_make_pointer

  subroutine rex_bind_pointer(rex_value, rex_length)
    integer, intent(in) :: rex_length
    character(rex_length), dimension(:), pointer, intent(inout) :: rex_value

    rex_value = 'rex'
  end subroutine rex_bind_pointer

end program rex_flang_dynamic_character_pointer_signature
