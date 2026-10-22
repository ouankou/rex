add_executable(astCopyReplTest astCopyReplTest.C)
target_link_libraries(astCopyReplTest PRIVATE ROSE_DLL)
install(TARGETS astCopyReplTest DESTINATION bin)
