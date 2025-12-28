! { dg-do run }
! { dg-options "-std=legacy" }
!
! pr19478 read from /dev/null
! Thomas.Koenig@online.de
#define DEV_NULL "/dev/null"
      character*20 foo
      open(10,file=DEV_NULL)
      write(10,'(A)') "Hello"
      rewind(10)
      read(10,'(A)',end=100) foo
      call abort
 100  continue
      end
