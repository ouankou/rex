module rex_openmp_exact_semantic_module
  implicit none
  integer :: Imported_Count = 1
end module rex_openmp_exact_semantic_module

program rex_fortran_openmp_exact_semantic_handoff
  use rex_openmp_exact_semantic_module, only: Local_Count => Imported_Count
  implicit none
  integer :: ShAdOw
  integer :: common_value
  common /MiXeD_Block/ common_value
!$omp threadprivate(/MiXeD_Block/)

  ShAdOw = Local_Count
!$omp parallel num_threads(Local_Count) private(ShAdOw)
  ShAdOw = Local_Count
!$omp end parallel

  block
    integer :: ShAdOw
!$omp parallel private(ShAdOw)
    ShAdOw = Local_Count
!$omp end parallel
  end block

end program rex_fortran_openmp_exact_semantic_handoff

subroutine rex_openmp_exact_simd_target(ArG)
  implicit none
  integer, intent(in) :: ArG
!$omp declare simd(rex_openmp_exact_simd_target) uniform(ArG)
!$omp declare simd(rex_openmp_exact_simd_target) linear(ArG)
end subroutine rex_openmp_exact_simd_target

subroutine rex_openmp_exact_simd_adjacency(AdJ_ArG)
  implicit none
  integer, intent(in) :: AdJ_ArG
!$omp declare simd uniform(AdJ_ArG)
end subroutine rex_openmp_exact_simd_adjacency

subroutine rex_openmp_exact_variant_explicit_one(ExP_ArG)
  implicit none
  integer, intent(in) :: ExP_ArG
end subroutine rex_openmp_exact_variant_explicit_one

subroutine rex_openmp_exact_variant_explicit_two(ExP_ArG)
  implicit none
  integer, intent(in) :: ExP_ArG
end subroutine rex_openmp_exact_variant_explicit_two

subroutine rex_openmp_exact_variant_base(ExP_ArG)
  implicit none
  integer, intent(in) :: ExP_ArG
  external rex_openmp_exact_variant_explicit_one
  external rex_openmp_exact_variant_explicit_two
!$omp declare variant(rex_openmp_exact_variant_explicit_one) match(construct={parallel})
!$omp declare variant(rex_openmp_exact_variant_explicit_two) match(construct={teams})
end subroutine rex_openmp_exact_variant_base

subroutine rex_openmp_exact_variant_adjacency_impl(AdJ_ArG)
  implicit none
  integer, intent(in) :: AdJ_ArG
end subroutine rex_openmp_exact_variant_adjacency_impl

subroutine rex_openmp_exact_variant_adjacency_base(AdJ_ArG)
  implicit none
  integer, intent(in) :: AdJ_ArG
  external rex_openmp_exact_variant_adjacency_impl
!$omp declare variant(rex_openmp_exact_variant_adjacency_impl) match(construct={dispatch})
end subroutine rex_openmp_exact_variant_adjacency_base
