# 1 "../../../trunk/libgfortran/generated/_abs_c4.F90"
# 1 "/usr/lib/gfortran/"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "../../../trunk/libgfortran/generated/_abs_c4.F90"
!   Comment here

# 1 "./config.h" 1

# 37 "../../../trunk/libgfortran/generated/_abs_c4.F90" 2

# 1 "./kinds.inc" 1
# 38 "../../../trunk/libgfortran/generated/_abs_c4.F90" 2

# 1 "./c99_protos.inc" 1
# 39 "../../../trunk/libgfortran/generated/_abs_c4.F90" 2

elemental function abs_c4 (parm)
   complex (kind=4), intent (in) :: parm
   real (kind=4) :: abs_c4

   abs_c4 = abs (parm)
end function

! { dg-do compile }
! { dg-options "-fpreprocessed -g3" }
