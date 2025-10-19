# REX uses Clang/LLVM frontend exclusively (EDG removed)
if(enable-c)
  # Add Clang frontend subdirectory
  add_subdirectory(Clang)
endif()
