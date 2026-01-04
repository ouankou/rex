add_executable(astCopyReplTest astCopyReplTest.C)
add_dependencies(dotGenerator ROSE_DLL)
target_link_libraries(astCopyReplTest ROSE_DLL)
install(TARGETS astCopyReplTest DESTINATION bin)

if(TARGET rose_clang_libstdcxx_headers)
  target_link_libraries(astCopyReplTest PRIVATE rose_clang_libstdcxx_headers)
endif()
