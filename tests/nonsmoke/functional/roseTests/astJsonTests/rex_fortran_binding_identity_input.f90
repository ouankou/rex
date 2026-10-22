module rex_binding_namelist
  implicit none
  integer :: rex_namelist_first, rex_namelist_second
  namelist /rex_parameters/ rex_namelist_first
  namelist /rex_parameters/ rex_namelist_second
end module

module rex_binding_namelist_bridge
  use rex_binding_namelist, only: rex_exported_parameters => rex_parameters
end module

module rex_binding_identity
  use, intrinsic :: iso_c_binding, only: c_int, c_int64_t, c_float
  use rex_binding_namelist_bridge, only: rex_local_parameters => rex_exported_parameters
  integer(c_int64_t) :: rex_target_kind8
contains
  subroutine rex_namelist_restore()
    character(128) :: buffer
    write(buffer, nml=rex_local_parameters)
    read(buffer, nml=rex_local_parameters)
  end subroutine

  function rex_bind_default(x) bind(c) result(y)
    integer(c_int), value :: x
    integer(c_int) :: y
    y = x + 1
  end function

  function rex_bind_named(x) bind(c, name="  rex_bound_" // "Name  ") result(y)
    integer(c_int), value :: x
    integer(c_int) :: y
    y = x + 2
  end function

  function rex_bind_empty(x) bind(c, name="") result(y)
    integer(c_int), value :: x
    integer(c_int) :: y
    y = x + 3
  end function

  function rex_bind_blanks(x) bind(c, name="   ") result(y)
    integer(c_int), value :: x
    integer(c_int) :: y
    y = x + 4
  end function

  function rex_bind_host(x) bind(c) result(y)
    integer(c_int), value :: x
    integer(c_int) :: y
    y = rex_bind_internal(x) + rex_bind_empty(x) + rex_bind_blanks(x)
  contains
    function rex_bind_internal(v) bind(c) result(w)
      integer(c_int), value :: v
      integer(c_int) :: w
      w = v + 5
    end function
  end function

  function rex_bind_reference(x) bind(c) result(y)
    integer(c_int) :: x
    integer(c_int) :: y
    y = x + 6
  end function

  subroutine rex_bind_descriptor(x) bind(c)
    real(c_float), intent(inout) :: x(:)
    x = x + 1
  end subroutine
end module
