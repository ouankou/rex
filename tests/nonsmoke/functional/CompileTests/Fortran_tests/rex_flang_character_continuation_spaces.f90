program rex_character_continuation
  implicit none
  character(len=50) :: value
  value = "hello world &
&    &
&!"
  if (value /= "hello world     !") stop 1
  print '(I0)', 7
end program rex_character_continuation
