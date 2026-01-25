! Legacy OFP regression used an invalid keyword + alternate return call.
! Flang requires positional arguments for alternate returns, so keep a valid
! alternate-return call to exercise unparse.

! The subroutine definition for reference below is commented out because
! it causes front end to fail another assertion before showing this bug.
subroutine g(k, *, *)
   integer :: k
   return 2
end subroutine

program p
    ! The following call is incorrect because F90 requires that
    ! positional arguments come before keyword arguments.
    ! However, OFP accepts it and ROSE fails an assertion on it.
    call g(0, *100, *200)
100 continue
200 continue
end program
