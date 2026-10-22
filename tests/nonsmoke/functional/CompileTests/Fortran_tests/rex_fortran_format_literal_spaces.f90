program format_literal_spaces
  character(64) :: output
  character(2) :: records(2)
  character(1) :: lines(3)
  integer :: status
  write (output, 100) 7
100 format ('single   spaces', "double  spaces", i4, 03Hh h, 01hx, 1Hy)
  if (trim(output) /= 'single   spacesdouble  spaces   7h hxy') stop 1
  write (records, 200, iostat=status) 1, 2, 3, 4
200 format (2I1)
  if (status /= 0) stop 2
  if (any(records /= ['12', '34'])) stop 3
  write (output, 300) 1, 2, 3
300 format (*(I1, :, 'x  y'))
  if (trim(output) /= '1x  y2x  y3') stop 4
  write (output, 400) 1, 2, 3, 4
400 format (2(2I1))
  if (trim(output) /= '1234') stop 5
  write (lines, 500) 1, 2
500 format (I1, 2/, I1)
  if (any(lines /= ['1', ' ', '2'])) stop 6
600 format (DT, DT'', DT"tag"(-2,3), 2(I1), T2, TL1, TR1, 2X, -2P, F0.1, L0, E8.1E0, G0)
end program format_literal_spaces
