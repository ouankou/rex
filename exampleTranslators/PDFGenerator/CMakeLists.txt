add_executable(pdfGenerator pdfGenerator.C)
target_link_libraries(pdfGenerator PRIVATE ROSE_DLL)
install(TARGETS pdfGenerator DESTINATION bin)
