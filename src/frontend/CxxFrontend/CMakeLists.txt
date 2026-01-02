# REX uses Clang/LLVM frontend exclusively (legacy frontend removed)
if(ENABLE-C)
  # Add Clang frontend subdirectory
  add_subdirectory(Clang)
endif()
