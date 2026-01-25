! Bug report from Rice: 01-empty-string-constant.f90
! ROSE's unparser fails an assertion on an empty character string constant.
! It doesn't matter whether single or double quotes are used.

program p
  character(len=1) :: c1 = ""  ! empty literal; padded to length 1
  character(len=1) :: c2 = ''  ! same as above
end program
