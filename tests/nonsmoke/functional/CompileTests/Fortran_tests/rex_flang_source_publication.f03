module rex_flang_source_publication
  implicit none
  private
  public :: exercise_source_publication

  integer :: fixed_length
  integer :: shaped
  integer :: storage(:)
  parameter (fixed_length = 8)
  dimension shaped(2)
  allocatable storage
  data shaped /1, 2/

contains

  subroutine exercise_source_publication(assumed_length, deferred_length)
    character(len=*), intent(in) :: assumed_length
    character(len=:), allocatable, intent(out) :: deferred_length
    character(len=8) :: literal_length
    real :: external_value
    external external_value

    literal_length = assumed_length
    allocate(character(len=len(assumed_length)) :: deferred_length)
    allocate(storage(2))
    deallocate(storage)
    deferred_length = literal_length
  end subroutine exercise_source_publication

end module rex_flang_source_publication
