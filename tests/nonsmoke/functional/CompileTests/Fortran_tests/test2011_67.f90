program test2011_67
  implicit none
  integer, parameter :: maxn = 100
  integer :: x_scalar, y_scalar, z_scalar
  integer :: n, i, j, k, l, itemp, jmax
  real :: temp
  real :: x(maxn)
  integer :: iy(maxn)
  external :: NEW

  ! Example of named labels used with EXIT statement.
  S1: DO
    IF (x_scalar > y_scalar) THEN
      z_scalar = x_scalar
      EXIT S1
    END IF
    CALL NEW(x_scalar)
  END DO

  N = 0
  LOOP1: DO I = 1, 10
    J = I
    LOOP2: DO K = 1, 5
      L = K
      N = N + 1
    END DO LOOP2
  END DO LOOP1

  jmax = n - 1
  outer: DO i = 1, n - 1
    temp = 1.e38
    inner: DO j = 1, jmax
      IF (x(j) .gt. x(j + 1)) cycle inner
      temp = x(j)
      x(j) = x(j + 1)
      x(j + 1) = temp
      itemp = iy(j)
      iy(j) = iy(j + 1)
      iy(j + 1) = itemp
    END DO inner
    IF (temp .eq. 1.e38) EXIT outer
    jmax = jmax - 1
  END DO outer
end program test2011_67
