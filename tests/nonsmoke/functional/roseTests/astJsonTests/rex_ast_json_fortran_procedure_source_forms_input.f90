module rex_ast_json_semantic_publication_module
  implicit none
contains
  function semantic_publication(values) result(text)
    integer, intent(in) :: values(1)
    character(len=semantic_publication_length(values) + lbound(values, 1) - 1) :: text

    text = 'abc'
  end function semantic_publication

  pure integer function semantic_publication_length(values)
    integer, intent(in) :: values(1)

    semantic_publication_length = values(1)
  end function semantic_publication_length
end module rex_ast_json_semantic_publication_module

program rex_ast_json_fortran_procedure_source_forms
  use rex_ast_json_semantic_publication_module
  implicit none
  integer, parameter :: source_kind = kind(0)
  integer, parameter :: source_len = 2
  integer :: i_twice
  external :: i_twice
  integer, external :: j_twice
  integer(kind=4), external :: k_twice
  integer(kind=source_kind + 0), external :: m_twice
  integer*4 :: star_integer
  real(kind=source_kind + 4) :: explicit_real
  real*8 :: star_real
  complex(kind=source_kind + 4) :: explicit_complex
  complex*16 :: star_complex
  logical(kind=source_kind) :: explicit_logical
  logical*4 :: star_logical
  character(kind=selected_char_kind('DEFAULT')) :: kind_only_char
  character(len=source_len + 1, kind=selected_char_kind('DEFAULT')) :: explicit_char
  character :: first*2, second*3
  character, pointer :: pointer_text(:)*3
  double precision :: fixed_real
  double complex :: fixed_complex
  integer(kind=source_kind), pointer :: pointer_values(:)
  integer :: dimensioned
  integer :: intrinsic_shadow_value
  integer :: semantic_publication_lengths(1)
  dimension dimensioned(2:5)

  type :: selector_box
    integer(kind=source_kind) :: member
    character :: left*2, right*3
    character, dimension(2) :: component_text*3
  end type selector_box
  type(selector_box), external :: make_selector_box

  type :: empty_box
  end type empty_box
  type(empty_box) :: empty_boxes(1)

  interface
    function unlimited_factory() result(value)
      class(*), allocatable :: value
    end function unlimited_factory
  end interface

  if (i_twice(3) /= 6) error stop 1
  if (j_twice(4) /= 8) error stop 2
  if (k_twice(5) /= 10) error stop 3
  if (m_twice(6) /= 12) error stop 4
  call intrinsic_shadow(intrinsic_shadow_value)
  semantic_publication_lengths = 3
  if (semantic_publication(semantic_publication_lengths) /= 'abc') error stop 5
  empty_boxes = [empty_box :: empty_box()]

contains

  subroutine intrinsic_shadow(value)
    integer, intent(out) :: value
    real :: abs
    intrinsic :: abs

    value = abs(-2)
  end subroutine intrinsic_shadow

  subroutine selector_dummies(assumed_text, runtime_length, dynamic_text, assumed_any, unlimited_any)
    character(len=*), intent(in) :: assumed_text
    integer, intent(in) :: runtime_length
    character(len=runtime_length), intent(in) :: dynamic_text
    type(*), intent(in) :: assumed_any
    class(*), intent(in) :: unlimited_any
    character(len=:), allocatable :: deferred_text

    deferred_text = trim(assumed_text)
  end subroutine selector_dummies

  type(selector_box) function copy_selector_box(input) result(output)
    type(selector_box), intent(in) :: input

    output = input
  end function copy_selector_box

  class(selector_box) function allocate_selector_box() result(output)
    allocatable :: output

    allocate(output)
    output%member = 1
  end function allocate_selector_box
end program rex_ast_json_fortran_procedure_source_forms

integer function i_twice(value)
  implicit none
  integer, intent(in) :: value

  i_twice = value * 2
end function i_twice

integer function j_twice(value)
  implicit none
  integer, intent(in) :: value

  j_twice = value * 2
end function j_twice

integer(kind=4) function k_twice(value)
  implicit none
  integer(kind=4), intent(in) :: value

  k_twice = value * 2
end function k_twice

integer(kind=kind(0)) function m_twice(value)
  implicit none
  integer(kind=kind(0)), intent(in) :: value

  m_twice = value * 2
end function m_twice
