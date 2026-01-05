add_executable(pdfGenerator pdfGenerator.C)
add_dependencies(pdfGenerator ROSE_DLL)
target_link_libraries(pdfGenerator ROSE_DLL)
install(TARGETS pdfGenerator DESTINATION bin)

if(TARGET rose_clang_libstdcxx_headers)
  target_link_libraries(pdfGenerator PRIVATE rose_clang_libstdcxx_headers)
endif()
