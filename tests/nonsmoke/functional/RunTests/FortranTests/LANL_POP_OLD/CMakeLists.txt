
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; LANL_POP_OLD tests require bash")
endif()

add_test(
  NAME RunTests_LANL_POP_OLD
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${BASH_EXECUTABLE} -c
    "set -e
     rm -rf poptest
     tar xzf ${CMAKE_CURRENT_SOURCE_DIR}/poptest.tgz -C ${CMAKE_CURRENT_BINARY_DIR}
     cd poptest
     if [ -n \"$$NUM_PROCESS\" ]
     then
       HUDSON_NUM_JOBS=$$NUM_PROCESS
     elif [ -n \"$$CTEST_PARALLEL_LEVEL\" ]
     then
       HUDSON_NUM_JOBS=$$CTEST_PARALLEL_LEVEL
     else
       HUDSON_NUM_JOBS=1
     fi
     export HUDSON_NUM_JOBS
     export HUDSON_TRANSLATOR=$<TARGET_FILE:testTranslator>
     export HUDSON_DIFF=\"$(command -v diff)\"
     export HUDSON_MAKE=\"${CMAKE_MAKE_PROGRAM}\"
     export HUDSON_SED=\"$(command -v sed)\"
     export HUDSON_FC=\"${BACKEND_FORTRAN_COMPILER}\"
     export HUDSON_LD=\"${BACKEND_FORTRAN_COMPILER}\"
     export HUDSON_CC=\"${CMAKE_C_COMPILER}\"
     export HUDSON_CXX=\"${CMAKE_CXX_COMPILER}\"
     export HUDSON_Cp=\"$(command -v cp)\"
     export HUDSON_Cpp=\"$(command -v cpp) -P\"
     export HUDSON_AWK=\"$(command -v awk)\"
     export HUDSON_LDLIBS=\"-lcurl\"
     ./test.sh"
)
set_tests_properties(RunTests_LANL_POP_OLD PROPERTIES LABELS RunTests RESOURCE_LOCK lanl_pop_old)
