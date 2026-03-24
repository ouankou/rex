add_test(
  NAME compiler_options_generate_source_file_names
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o; \
    ${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C -o test1.o; \
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C -o test1.o; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/doesFilesExist.sh ./rose_test1.C; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/filesNotExist.sh ./rose_test1.o.s; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o; \
    ${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/test1.C -o test1.o; \
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 test1.o; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/doesFilesExist.sh ./a.out; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/filesNotExist.sh ./rose_test1.o.s; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o; \
    ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 ${CMAKE_CURRENT_SOURCE_DIR}/test1.C; \
    sh ${CMAKE_CURRENT_SOURCE_DIR}/doesFilesExist.sh ./a.out ./rose_test1.C; \
    rm -f a.out rose_test1.C rose_test1.o.s test1.o"
)


