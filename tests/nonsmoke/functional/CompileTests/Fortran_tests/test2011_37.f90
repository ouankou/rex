program p
    real(4) :: donor_variability(5,6,7)
    real(4) :: temperature4(5,6,7)

    ! Alternate returns require positional arguments.
    call g(0, *100)
100 continue

    temperature4 = eoshift(donor_variability, SHIFT=1, BOUNDARY=zero, DIM=id)
end program
