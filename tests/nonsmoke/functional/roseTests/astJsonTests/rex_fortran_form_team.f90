subroutine regroup(team, number, index, status, message)
  use, intrinsic :: iso_fortran_env, only: team_type
  implicit none
  type(team_type) :: team
  integer :: number, index, status
  character(*) :: message
  form team(number, team, new_index=index, stat=status, errmsg=message)
  form team(number, team)
end subroutine regroup
