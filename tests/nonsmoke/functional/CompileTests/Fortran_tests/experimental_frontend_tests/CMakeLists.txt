set(_rose_flags -rose:verbose 0 -rose:experimental_fortran_frontend
  -I${CMAKE_CURRENT_SOURCE_DIR})
string(JOIN " " _rose_flags_str ${_rose_flags})
set(_experimental_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_experimental_output_dir})

set(_f90_tests
  tiny.f90
  R1101a.f90
  R1101b.f90
  R1101c.f90
  R1101d.f90
  R1101e.f90
  R1101f.f90
  R1101g.f90
  R1120a.f90
  R404_a.f90
  R521.f90
  R545.f90
  R611a.f90
  R719a.f90
  R832.f90
  R837.f90
  R854.f90
  R856.f90
  contiguous_attr.f90
  test2014_02.f90
)

set(_f2018_tests
  R507-F2015-N2007.f90
  R511-F2015-N2007.f90
  R1123-F2018.f90
  R1164-F2018.f90
  R1166-F2018.f90
  R1168-F2018.f90
  R1169-F2018.f90
  R1179-F2018.f90
  R1181-F2018.f90
)

set(_f90_working_on_tests
  issue-38.f90
  R404.f90
  R1001.f90
  R1109.f90
  R1111.f90
  R1120b.f90
  R1201.f90
  R1203.f90
  R1204.f90
  R1205.f90
  R1206.f90
  R1207.f90
  R1208.f90
  R1209.f90
  R1210.f90
  R1211.f90
  R1214.f90
  R1218.f90
  R1234.f90
  R1241.f90
  R524.f90
  R526.f90
  R533.f90
  R611.f90
  R620.f90
  R626.f90
  R640.f90
  R701.f90
  R704.f90
  R705.f90
  R706.f90
  R710.f90
  R713.f90
  R719.f90
  R720.f90
  R721.f90
  R743.f90
  R757.f90
  R804.f90
  R807.f90
  R831.f90
  R838.f90
  R850.f90
  R851.f90
  R852.f90
  R853.f90
  R902.f90
  R904.f90
  R905.f90
  R908.f90
  R909.f90
  R911.f90
  R922.f90
  R926.f90
  R927.f90
  R928.f90
  R929.f90
  R930.f90
)

set(_failing_tests
  pause_stmt.f77
  R432.f90
  R452.f90
  R816.f90
  R858.f08
  R859.f08
  R862.f08
)

set(_non_fortran_failing_tests
  R1116.fxx
  R1118.fxx
  R1220.fxx
  R1237.fxx
  R426.fxx
  R427.fxx
  R702.fxx
  R722.fxx
  R723.fxx
  R733.fxx
)

function(_experimental_sanitize_name input output_var)
  set(_name "${input}")
  string(REPLACE "/" "_" _name "${_name}")
  string(REPLACE "." "_" _name "${_name}")
  string(REPLACE "-" "_" _name "${_name}")
  set(${output_var} "${_name}" PARENT_SCOPE)
endfunction()

function(_experimental_add_translator_test test_prefix specimen)
  _experimental_sanitize_name(${specimen} _name)
  set(_test_name ${test_prefix}_${_name})
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${_experimental_output_dir}
    COMMAND bash -c
      "$<TARGET_FILE:testTranslator> ${_rose_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen} && test -s rose_${specimen}"
  )
  set(${test_prefix}_${specimen}_name ${_test_name} PARENT_SCOPE)
endfunction()

function(_experimental_add_frontend_test test_prefix specimen)
  _experimental_sanitize_name(${specimen} _name)
  add_test(
    NAME ${test_prefix}_${_name}
    WORKING_DIRECTORY ${_experimental_output_dir}
    COMMAND $<TARGET_FILE:testFrontEnd>
      ${_rose_flags}
      -rose:skip_syntax_check
      ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
  )
endfunction()

foreach(specimen IN LISTS _f90_tests)
  _experimental_add_translator_test(experimental_frontend_f90 ${specimen})
endforeach()

foreach(specimen IN LISTS _f2018_tests)
  _experimental_add_frontend_test(experimental_frontend_f2018 ${specimen})
endforeach()

foreach(specimen IN LISTS _f90_working_on_tests)
  _experimental_add_translator_test(experimental_frontend_working ${specimen})
endforeach()

foreach(specimen IN LISTS _failing_tests)
  _experimental_add_translator_test(experimental_frontend_failing ${specimen})
endforeach()

foreach(specimen IN LISTS _non_fortran_failing_tests)
  _experimental_add_translator_test(experimental_frontend_non_fortran ${specimen})
endforeach()
