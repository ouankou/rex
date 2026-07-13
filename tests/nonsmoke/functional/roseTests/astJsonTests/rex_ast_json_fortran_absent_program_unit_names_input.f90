integer :: main_value
main_value = 1
end

block data
  integer :: block_value
  common /flags/ block_value
end block data
