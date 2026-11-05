      program rex_fortran_openacc_empty_region_fixed
      implicit none
      integer value
      value = 7
c REX_EMPTY_BEFORE
c$acc parallel
c$acc end parallel
c$acc parallel ! REX_EMPTY_BEGIN_INLINE
c REX_EMPTY_INSIDE_FIRST

c REX_EMPTY_INSIDE_LAST
c$acc end parallel ! REX_EMPTY_END_INLINE
c REX_EMPTY_AFTER
      if (value .ne. 7) stop 1
      print *, value
      end
