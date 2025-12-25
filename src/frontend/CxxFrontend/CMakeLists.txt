# REX uses Clang/LLVM frontend exclusively (legacy frontend removed)
if(enable-c)
  # Add Clang frontend subdirectory
  add_subdirectory(Clang)
endif()
