program rex_issue192_concat_continuation
  implicit none
  character(len=32) :: char_tmp, logdate, logtime, log_filename
  character(len=32) :: mix_steps, mix_step

  char_tmp = 'pop.out'
  logdate = '20000101'
  logtime = '010203'

  log_filename = trim(char_tmp)/&
                               &/'.'/&
                               &/logdate/&
                               &/'.'/&
                               &/logtime

  if (log_filename /= 'pop.out.20000101.010203') stop 1

  mix_steps = '2'
  mix_step = ' 3'
  mix_steps = trim(mix_steps)/&
                           &/',' /&
                                 &/ trim(mix_step)

  if (mix_steps /= '2, 3') stop 2
end program rex_issue192_concat_continuation
