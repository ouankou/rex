program rex_flang_real_literal_kinds
  integer, parameter :: rk = 8
  real(kind=2) :: half = 0.5_2
  real(kind=3) :: bfloat = 0.5_3
  real(kind=4) :: small = 1.25
  real(kind=8) :: dexp = 1.25D2
  real(kind=8) :: named = 0.125_rk
  real(kind=10) :: wide = 2.5_10
  real(kind=8) :: negative_zero = -0.0_8
  real(kind=8) :: mixed = 1.0_4 + 2.0_8
end program
