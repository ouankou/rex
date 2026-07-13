program rex_fortran_wait_spec_contract
  implicit none
  integer :: unit_number = 10
  integer :: request_id = 1
  integer :: status_code
  character(len=64) :: message

  wait(unit=unit_number, end=100, eor=200, err=300, id=request_id, &
       iomsg=message, iostat=status_code)
  stop

100 continue
  stop
200 continue
  stop
300 continue
end program rex_fortran_wait_spec_contract
