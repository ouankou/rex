! illegal-forward-ref-type.f90 (legacy OFP regression)
! Flang requires derived types to be defined before use.

program p

  type :: t2
    real ::  x
  end type

  type(t2) :: y

end program
