program rex_flang_do_end_labels
  implicit none
  integer :: i, total, values(2)

  total = 0
  do i = 1, 2
    total = total + i
    goto 30
    error stop
30 end do

  do while (total < 5)
    total = total + 1
    goto 50
    error stop
50 end do

  do concurrent (i = 1:2)
    values(i) = i
70 end do

  if (total /= 5 .or. any(values /= [1, 2])) error stop
end program rex_flang_do_end_labels
