program image_control
  use iso_fortran_env, only: team_type, lock_type
  type(team_type) :: team
  type(lock_type) :: lock[*]
  integer :: status
  character(len=64) :: message
  logical :: acquired

  go to 90287
90287 form team(1, team, new_index=1, stat=status, errmsg=message)
  sync images(1)
  sync images(*)
  sync images(status+1, stat=status, errmsg=message)
  sync team(team, stat=status, errmsg=message)
  lock(lock, acquired_lock=acquired, stat=status, errmsg=message)
  unlock(lock, stat=status, errmsg=message)
end program image_control
