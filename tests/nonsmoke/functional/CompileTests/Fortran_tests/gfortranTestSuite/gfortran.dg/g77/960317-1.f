c { dg-do compile }
* g77 bug in ASSIGN/SAVE combination.
C
      SUBROUTINE QUICK
      SAVE
C
      ASSIGN 101 TO JUMP ! { dg-warning "Deleted feature: ASSIGN" "" }
  101 Continue
C
      RETURN
      END
C End of test case.
