
include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; LANL_POP_OLD tests require bash")
endif()

set(_lanl_pop_old_enabled TRUE)
if(NOT ENABLE-FORTRAN)
  set(_lanl_pop_old_enabled FALSE)
endif()

set(_gfortran_4_0 FALSE)
if(BACKEND_FORTRAN_COMPILER_NAME_WITHOUT_PATH MATCHES "gfortran"
   AND BACKEND_FORTRAN_COMPILER_MAJOR_VERSION_NUMBER STREQUAL "4"
   AND BACKEND_FORTRAN_COMPILER_MINOR_VERSION_NUMBER STREQUAL "0")
  set(_gfortran_4_0 TRUE)
endif()
if(_gfortran_4_0)
  set(_lanl_pop_old_enabled FALSE)
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
     export HUDSON_FFLAGS=\"-ffree-line-length-0\"
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
if(NOT _lanl_pop_old_enabled)
  set_tests_properties(RunTests_LANL_POP_OLD PROPERTIES DISABLED TRUE)
endif()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_FortranTests_LANL_POP_OLD_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=RunTests_LANL_POP_OLD
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_OLD_check-local PROPERTIES DEPENDS RunTests_LANL_POP_OLD )
