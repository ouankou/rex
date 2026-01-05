include_directories(${ROSE_INCLUDES})

add_executable(translator translator.C)
target_link_libraries(translator ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_run_test_conf ${CMAKE_CURRENT_SOURCE_DIR}/runTest.conf)

set(_specimen_c_and_cxx_names
  suffix_01.c
  quote+SQUOTE+.c
  quote+DQUOTE+.c
  dollar+DOLLAR+.c
  star+STAR+.c
  hash+HASH+.c
  space+SPACE+.c
  backslash+BSLASH+.c
  .+SLASH+x.c
  .+SLASH++SLASH+x.c
  .+SLASH+.+SLASH+x.c
  .+SLASH+.+SLASH++SLASH+.+SLASH++SLASH++SLASH+x.c
  aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.c
  +DOT++SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+.+SLASH+x.c
)

set(_specimen_only_cxx_names
  suffix_02.c++
  suffix_05.cc
  suffix_07.cp
  suffix_09.cpp
  suffix_11.cxx
  suffix_03.C
)

foreach(specimen IN LISTS _specimen_c_and_cxx_names)
  add_test(
    NAME compiler_options_file_names_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      SPECIMEN=${specimen}
      ${_run_test_conf}
      ${specimen}.passed
  )
endforeach()

foreach(specimen IN LISTS _specimen_only_cxx_names)
  add_test(
    NAME compiler_options_file_names_cxx_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CC=
      CXX=${CMAKE_CXX_COMPILER}
      SPECIMEN=${specimen}
      ${_run_test_conf}
      ${specimen}.passed
  )
endforeach()
