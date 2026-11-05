      program rex_fortran_openmp_empty_region_fixed
      implicit none
      integer value
      value = 7
c REX_EMPTY_BEFORE
c$omp parallel
c$omp end parallel
c$omp parallel ! REX_EMPTY_BEGIN_INLINE
c REX_EMPTY_INSIDE_FIRST

c REX_EMPTY_INSIDE_LAST
c$omp end parallel ! REX_EMPTY_END_INLINE
c REX_EMPTY_AFTER
      if (value .ne. 7) stop 1
      print *, value
      end
