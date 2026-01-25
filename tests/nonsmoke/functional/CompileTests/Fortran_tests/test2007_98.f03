! This appears to be a F2003 specific construct; ensure ROSE uses the
! F2003 dialect (-rose:fortran_std=f2003) and the .f03 filename suffix.
PROGRAM ASSOCIATE_EXAMPLE
     REAL :: X,A
     A = 0.4

   ! The associate construct is F2003 specific
     ASSOCIATE ( Z => A )
          X = Z
     END ASSOCIATE

END PROGRAM ASSOCIATE_EXAMPLE
