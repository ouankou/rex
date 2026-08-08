program rex_fortran_openmp_exact_expression_spelling
  implicit none
  integer :: MiXeD_Thread_Count
  integer :: MiXeD_Private_Value
  integer :: MiXeD_Shared_Value
  integer :: NuM_TaSkS
  integer :: CoPyPrIvAtE
  integer :: loop_index

  MiXeD_Thread_Count = 1
  MiXeD_Shared_Value = 7
  NuM_TaSkS = 1
!$OMP PARALLEL NUM_THREADS(MiXeD_Thread_Count) PRIVATE(MiXeD_Private_Value) SHARED(MiXeD_Shared_Value)
  MiXeD_Private_Value = MiXeD_Shared_Value
!$OMP END PARALLEL

!$OMP TASKLOOP NUM_TASKS(NuM_TaSkS)
  do loop_index = 1, 1
    MiXeD_Shared_Value = loop_index
  end do
!$OMP END TASKLOOP

!$OMP PARALLEL PRIVATE(CoPyPrIvAtE)
!$OMP SINGLE
  CoPyPrIvAtE = MiXeD_Shared_Value
!$OMP END SINGLE COPYPRIVATE(CoPyPrIvAtE)
!$OMP END PARALLEL
end program rex_fortran_openmp_exact_expression_spelling
