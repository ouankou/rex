! S08C-cant-assert-keyword-nonnull.f90

! Legacy OFP regression used an invalid keyword + alternate return call.
! Flang requires positional arguments for alternate returns, so keep a valid
! alternate-return call to exercise unparse.

program p
    ! The following call is incorrect because F90 requires that
    ! positional arguments come before keyword arguments.
    ! However, OFP accepts it and ROSE fails an assertion on it.
    call g(0, *100)
100 continue
end program
