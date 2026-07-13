subroutine openacc_common_block_reference()
  integer :: flag_a, flag_b
  common /FlAgS/ flag_a, flag_b
!$acc data present(/fLaGs/)
  flag_a = flag_b
!$acc end data
end subroutine openacc_common_block_reference
