add_executable(identityTranslator identityTranslator.cpp)
target_link_libraries(identityTranslator ROSE_DLL)
  install(TARGETS identityTranslator DESTINATION bin)

if(TARGET rose_clang_libstdcxx_headers)
  target_link_libraries(identityTranslator PRIVATE rose_clang_libstdcxx_headers)
endif()
