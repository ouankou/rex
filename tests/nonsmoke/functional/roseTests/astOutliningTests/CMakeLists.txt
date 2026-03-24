include_directories(${ROSE_INCLUDES})

add_executable(outline outline.cc)
target_link_libraries(outline ROSE_DLL ${link_with_libraries})

add_executable(injectOutlinePragmas RandomStmtSelector.cc injectOutlinePragmas.cc)
target_link_libraries(injectOutlinePragmas ROSE_DLL ${link_with_libraries})

add_executable(outlineRandom RandomStmtSelector.cc outlineRandom.cc)
target_link_libraries(outlineRandom ROSE_DLL ${link_with_libraries})

add_executable(outlineSelection outlineSelection.cc)
target_link_libraries(outlineSelection ROSE_DLL ${link_with_libraries})

set(OUTLINE_C_TESTS
  array1.c
  arrayParameter.c
  arrayRef.c
  jacobi_rose_outline.c
  moreTest0.c
  moreTest1.c
  moreTest8.c
  print.c
  variableCloning.c
  variables_in_header.c
  zeroParameter.c
)

# Legacy C specimens that rely on GNU89 semantics.
set(OUTLINE_GNU89_C_TESTS
  jacobi_rose_outline.c
)

set(OUTLINE_CXX_TESTS
  array2.cpp
  moreTest2.cpp
  moreTest3.cpp
  moreTest4.cpp
  moreTest5.cpp
  moreTest6.cpp
  moreTest7.cpp
  charStar.cpp
  const-global.cpp
  polygon.cpp
  reference.cpp
  template_function1.cpp
  test_26_v2_2020.cpp
  test_66_2020.cpp
  triangle.cpp
)

set(OUTLINE_CXX2_TESTS
  print.cpp
  test_42_2020.cpp
  test_20_2019.cpp
  test_26_v1_2020.cpp
  charStar.cpp
  test_37_2020.cpp
  ft_cfftz.cpp
  test_61_2020.cpp
  test_119_2021.cpp
  test_120_2020.cpp
  test_121_2020.cpp
  test_52.cpp
)

set(OUTLINE_CXX4_TESTS
  ft_cfftz_omp.cpp
)

set(OUTLINE_CXX5_TESTS
  ft_cfftz_omp2.cpp
)

set(OUTLINE_CXX6_TESTS
  test_2023_3_21.cpp
)

set(OUTLINE_FORTRAN_TESTS
  array-variable.f
  array.f
  assignment.f
  assignmentmain.f
  assignmentmain2.f
  arithmeticIf.f
  blockdo.f90
  column72.f
  column80.f
  contains1.f90
  contains.f90
  continue.f
  dimension.f
  doloop.f
  doloop2.f
  doloop3.f90
  doloop4.f90
  doloop5.f90
  doloop_nested.f90
  format.f
  format2.f
  function.f
  function2.f
  hello.f
  implicit.f90
  integer.f
  intent.f90
  lengthlimit.f
  loan.f
  module.f90
  module1.f90
  module3.f90
  moduleParameter.f90
  moduleSubprogram.f90
  parameter.f
  parameter2.f
  power.f
  print.f
  read.f
  read2.f
  real8.f
  stop.f90
  stop1.f90
  stop2.f90
  subroutine1.f
  subroutine.f90
  ugoto.f
  useModule1.f90
  write.f
)

set(OUTLINE_OPENMP_C_TESTS
  jacobi.c
)

set(OUTLINE_EXTERNAL_CXX_TESTS
  test_CplusplusMacro_Cpp.C
  test2001_01.C
  test2001_02.C
  test2001_04.C
  test2001_05.C
  test2001_06.C
  test2001_08.C
  test2001_09.C
  test2001_10.C
  test2001_12.C
  test2001_13.C
  test2001_14.C
  test2001_16.C
  test2001_17.C
  test2001_18.C
  test2001_20.C
  test2001_21.C
  test2001_22.C
  test2001_24.C
  test2001_25.C
  test2001_26.C
  test2001_28.C
  test2001_29.C
  test2002_01.C
  test2003_02.C
  test2003_03.C
  test2003_04.C
  test2003_06.C
  test2003_08.C
  test2003_09.C
  test2003_11.C
  test2003_12.C
  test2003_13.C
  test2003_15.C
  test2003_16.C
  test2003_17.C
  test2003_19.C
  test2003_20.C
  test2003_21.C
  test2003_24.C
  test2003_25.C
  test2003_26.C
  test2003_28.C
  test2003_29.C
  test2003_30.C
  test2003_32.C
  test2003_35.C
  test2003_36.C
  test2003_38.C
  test2004_01.C
  test2004_02.C
  test2004_04.C
  test2004_05.C
  test2004_06.C
  test2004_08.C
  test2004_09.C
  test2004_10.C
  test2004_12.C
  test2004_13.C
  test2004_14.C
  test2004_16.C
  test2004_17.C
  test2004_18.C
  test2004_23.C
  test2004_24.C
  test2004_26.C
  test2004_28.C
  test2004_29.C
  test2004_31.C
  test2004_33.C
  test2004_34.C
  test2004_35.C
  test2004_37.C
  test2004_38.C
  test2004_39.C
  test2004_42.C
  test2004_43.C
  test2004_44.C
  test2004_47.C
  test2004_48.C
  test2004_49.C
  test2004_51.C
  test2004_52.C
  test2004_53.C
  test2004_55.C
  test2004_56.C
  test2004_58.C
  test2004_60.C
  test2004_61.C
  test2004_62.C
  test2004_64.C
  test2004_65.C
  test2004_67.C
  test2004_69.C
  test2004_70.C
  test2004_71.C
  test2004_73.C
  test2004_74.C
  test2004_75.C
  test2004_78.C
  test2004_79.C
  test2004_80.C
  test2004_82.C
  test2004_84.C
  test2004_85.C
  test2004_87.C
  test2004_88.C
  test2004_90.C
  test2004_93.C
  test2004_94.C
  test2004_95.C
  test2004_97.C
  test2004_98.C
  test2004_99.C
  test2004_101.C
  test2004_102.C
  test2004_103.C
  test2004_105.C
  test2004_106.C
  test2004_107.C
  test2004_109.C
  test2004_110.C
  test2004_111.C
  test2004_113.C
  test2004_114.C
  test2004_115.C
  test2004_117.C
  test2004_118.C
  test2004_119.C
  test2004_121.C
  test2004_122.C
  test2004_123.C
  test2004_125.C
  test2004_127.C
  test2004_128.C
  test2004_130.C
  test2004_131.C
  test2004_132.C
  test2004_134.C
  test2004_135.C
  test2004_136.C
  test2004_138.C
  test2004_139.C
  test2004_140.C
  test2004_142.C
  test2004_143.C
  test2004_144.C
  test2004_146.C
  test2004_147.C
  test2004_149.C
  test2004_151.C
  test2004_152.C
  test2004_153.C
  test2004_155.C
  test2004_156.C
  test2004_157.C
  test2004_159.C
  test2004_160.C
  test2004_161.C
  test2004_163.C
  test2004_164.C
  test2005_01.C
  test2005_04.C
  test2005_05.C
  test2005_06.C
  test2005_08.C
  test2005_09.C
  test2005_10.C
  test2005_14.C
  test2005_15.C
  test2005_16.C
  test2005_19.C
  test2005_20.C
  test2005_21.C
  test2005_23.C
  test2005_24.C
  test2005_25.C
  test2005_28.C
  test2005_29.C
  test2005_30.C
  test2005_32.C
  test2005_33.C
  test2005_34.C
  test2005_36.C
  test2005_37.C
  test2005_38.C
  test2005_40.C
  test2005_41.C
  test2005_42.C
  test2005_44.C
  test2005_45.C
  test2005_46.C
  test2005_48.C
  test2005_49.C
  test2005_50.C
  test2005_52.C
  test2005_53.C
  test2005_55.C
  test2005_58.C
  test2005_59.C
  test2005_60.C
  test2005_62.C
  test2005_63.C
  test2005_64.C
  test2005_67.C
  test2005_68.C
  test2005_70.C
  test2005_73.C
  test2005_74.C
  test2005_75a.C
  test2005_76.C
  test2005_77.C
  test2005_78.C
  test2005_80.C
  test2005_81.C
  test2005_82.C
  test2005_84.C
  test2005_86.C
  test2005_88.C
  test2005_90.C
  test2005_91.C
  test2005_92.C
  test2005_94.C
  test2005_95.C
  test2005_96.C
  test2005_98.C
  test2005_99.C
  test2005_100.C
  test2005_103.C
  test2005_104.C
  test2005_105.C
  test2005_107.C
  test2005_108.C
  test2005_109.C
  rex_test2026_issue213_va_start.cpp
  test2005_112.C
  test2005_113.C
  test2005_116.C
  test2005_117.C
  test2005_118.C
  test2005_120.C
  test2005_121.C
  test2005_122.C
  test2005_124.C
  test2005_125.C
  test2005_126.C
  test2005_128.C
  test2005_129.C
  test2005_130.C
  test2005_132.C
  test2005_133.C
  test2005_134.C
  test2005_136.C
  test2005_137.C
  test2005_138.C
  test2005_140.C
  test2005_141.C
  test2005_142.C
  test2005_144.C
  test2005_145.C
  test2005_146.C
  test2005_150.C
  test2005_152.C
  test2005_154.C
  test2005_156.C
  test2005_157.C
  test2005_158.C
  test2005_160.C
  test2005_161.C
  test2005_162.C
  test2005_164.C
  test2005_165.C
  test2005_166.C
  test2005_169.C
  test2005_170.C
  test2005_171.C
  test2005_175.C
  test2005_176.C
  test2005_177.C
  test2005_180.C
  test2005_181.C
  test2005_182.C
  test2005_184.C
  test2005_185.C
  test2005_187.C
  test2005_190.C
  test2005_191.C
  test2005_192.C
  test2005_194.C
  test2005_195.C
  test2005_196.C
  test2005_198.C
  test2005_202.C
  test2005_203.C
  test2005_207.C
  test2005_208.C
  test2005_209.C
  test2006_02.C
  test2006_03.C
  test2006_04.C
  test2006_06.C
  test2006_07.C
  test2006_08.C
  test2006_13.C
  test2006_14.C
  test2006_16.C
  test2006_18.C
  test2006_19.C
  test2006_20.C
  test2006_23.C
  test2006_25.C
  test2006_26.C
  test2006_34.C
  test2006_37.C
  test2006_38.C
  test2006_40.C
  test2006_41.C
  test2006_42.C
  test2006_47.C
  test2006_50.C
  test2006_52.C
  test2006_54.C
  test2006_55.C
  test2006_56.C
  test2006_58.C
  test2006_59.C
  test2006_61.C
  test2006_63.C
  test2006_66.C
  test2006_67.C
  test2006_69.C
  test2006_70.C
  test2006_71.C
  test2006_73.C
  test2006_74.C
  test2006_75.C
  test2006_77.C
  test2006_78.C
  test2006_79.C
  test2006_81.C
  test2006_82.C
  test2006_83.C
  test2006_85.C
  test2006_86.C
  test2006_87.C
  test2006_89.C
  test2006_90.C
  test2006_91.C
  test2006_93.C
  test2006_94.C
  test2006_95.C
  test2006_97.C
  test2006_98.C
  test2006_101.C
  test2006_103.C
  test2006_104.C
  test2006_105.C
  test2006_107.C
  test2006_108.C
  test2006_109.C
  test2006_112.C
  test2006_113.C
  test2006_115.C
  test2006_117.C
  test2006_118.C
  test2006_119.C
  test2006_122.C
  test2006_123.C
  test2006_124.C
  test2006_126.C
  test2006_129.C
  test2006_139.C
  test2006_141.C
  test2006_142.C
  test2006_143.C
  test2006_145.C
  test2006_146.C
  test2006_147.C
  test2006_149.C
  test2006_150.C
  test2006_151.C
  test2006_153.C
  test2006_154.C
  test2006_155.C
  test2006_158.C
  test2006_160.C
  test2006_161.C
  test2006_163.C
  test2006_164.C
  test2006_166.C
  test2006_183.C
  test2006_185.C
  test2007_01.C
  test2007_03.C
  test2007_04.C
  test2007_05.C
  test2007_07.C
  test2007_08.C
  test2007_09.C
  test2007_11.C
  test2007_12.C
  test2007_13.C
  test2007_15.C
  test2007_16.C
  test2007_17.C
  test2007_23.C
  test2007_24.C
  test2007_25.C
  test2007_28.C
  test2007_29.C
  test2007_38.C
  test2007_41.C
  test2007_43.C
  test2007_44.C
  test2007_47.C
  test2007_48.C
  test2007_49.C
  test2007_51.C
  test2007_52.C
  test2007_53.C
  test2007_57.C
  test2007_59.C
  test2007_62.C
  test2007_72.C
  test2007_75.C
  test2007_77.C
  test2007_81.C
  test2007_82.C
  test2007_83.C
  test2007_85.C
  test2007_90.C
  test2007_94.C
  test2007_102.C
  test2007_103.C
  test2007_128.C
  test2007_133.C
  test2007_135.C
  test2007_137.C
  test2007_142.C
  test2007_147.C
  test2007_150.C
  test2007_152.C
  test2007_154.C
  test2007_155.C
  test2007_158.C
  test2007_159.C
  test2007_160.C
  test2007_163.C
  test2007_164.C
  test2007_165.C
  test2007_167.C
  test2007_170.C
  test2007_171.C
  test2007_173.C
  test2008_04.C
  test_int_lexemes_donot_pass.C
  test_simple_int.C
  lexPhase2003_01.C
  math.C
  inputForLoopLocator.C
  X.C
  inputBug317.C
  test2004_76.C
  test2005_03.C
  test2005_11.C
  test2005_13.C
  test2005_72.C
  test2005_87.C
  test2005_147.C
  test2005_153.C
  test2005_174.C
  test2007_141.C
  test2007_162.C
  test2001_03.C
  test2004_116.C
  test2006_12.C
  test2005_188.C
  test2005_205.C
  test2007_95.C
  test2007_169.C
  test2006_44.C
  test2004_30.C
  test2006_99.C
  test2006_159.C
  test2005_56.C
  test2005_27.C
  test2005_179.C
  test2005_199.C
  test2006_165.C
)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME injectOutlinePragmas
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="inject outline pragmas [injectOutlinePragmas.passed]"
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:injectOutlinePragmas> -rose:num-outline 1 -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/moreTest8.c"
    ${_exit_status}
    injectOutlinePragmas.passed
)

add_test(
  NAME outlineRandom
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="outline random [outlineRandom.passed]"
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:outlineRandom> -rose:outline:random 1 -rose:verbose 2 -I${CMAKE_CURRENT_SOURCE_DIR} -c ${CMAKE_CURRENT_SOURCE_DIR}/moreTest8.c"
    ${_exit_status}
    outlineRandom.passed
)

set(_outline_external_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_outline_openmp_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests)

set(_outline_includes
  -I${_outline_external_dir}
  -I${CMAKE_CURRENT_SOURCE_DIR})

set(_outline_flags
  -rose:verbose 0
  -rose:outline:parameter_wrapper
  -rose:detect_dangling_pointers 1)

set(_outline_noswitch_args -rose:verbose 0)
set(_outline_fortran_fixed_form "-ffixed-form -ffixed-line-length=0")
set(_outline_fortran_free_form "-ffree-form")

set(_outline_dlopen_header_include
  -I${CMAKE_SOURCE_DIR}/src/midend/programTransformation/astOutlining)

string(JOIN " " _outline_includes_str ${_outline_includes})
string(JOIN " " _outline_flags_str ${_outline_flags})
string(JOIN " " _outline_noswitch_str ${_outline_noswitch_args})

# Classic outlining on local C tests
foreach(specimen IN LISTS OUTLINE_C_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  set(_target classic_${specimen}.passed)
  set(_specimen_lang_flags "")
  if(specimen IN_LIST OUTLINE_GNU89_C_TESTS)
    set(_specimen_lang_flags "-std=gnu89")
  endif()
  add_test(
    NAME outline_classic_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline classic ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_specimen_lang_flags} -rose:outline:enable_classic -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# Classic outlining on OpenMP specimen(s)
foreach(specimen IN LISTS OUTLINE_OPENMP_C_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  set(_target classic_${specimen}.passed)
  add_test(
    NAME outline_classic_openmp_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline classic ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> -I${_outline_openmp_dir} -rose:outline:select_omp_loop -rose:outline:enable_classic -c ${_outline_openmp_dir}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# In-place outlining on local C tests
foreach(specimen IN LISTS OUTLINE_C_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  set(_target inplace_${specimen}.passed)
  set(_specimen_lang_flags "")
  if(specimen IN_LIST OUTLINE_GNU89_C_TESTS)
    set(_specimen_lang_flags "-std=gnu89")
  endif()
  add_test(
    NAME outline_inplace_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline in-place ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_specimen_lang_flags} -rose:outline:temp_variable -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# Outline to a separate file (C + C++)
set(_outline_c_and_cxx_tests ${OUTLINE_C_TESTS} ${OUTLINE_CXX_TESTS})
foreach(specimen IN LISTS _outline_c_and_cxx_tests)
  string(REPLACE "." "_" _name ${specimen})
  set(_target tofile_${specimen}.passed)
  set(_specimen_lang_flags "")
  if(specimen IN_LIST OUTLINE_GNU89_C_TESTS)
    set(_specimen_lang_flags "-std=gnu89")
  endif()
  add_test(
    NAME outline_tofile_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline new-file ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_specimen_lang_flags} -rose:outline:new_file -rose:outline:temp_variable -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# Default outlining flags (C + C++)
foreach(specimen IN LISTS _outline_c_and_cxx_tests)
  string(REPLACE "." "_" _name ${specimen})
  set(_target default_${specimen}.passed)
  set(_specimen_lang_flags "")
  if(specimen IN_LIST OUTLINE_GNU89_C_TESTS)
    set(_specimen_lang_flags "-std=gnu89")
  endif()
  add_test(
    NAME outline_default_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline default ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_specimen_lang_flags} ${_outline_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# Fortran tests without outline-specific switches
if(enable-fortran)
  foreach(specimen IN LISTS OUTLINE_FORTRAN_TESTS)
    string(REPLACE "." "_" _name ${specimen})
    if(specimen MATCHES "\\.f$")
      set(_format_flags "${_outline_fortran_fixed_form}")
    else()
      set(_format_flags "${_outline_fortran_free_form}")
    endif()
    set(_target noswitch_${specimen}.passed)
    add_test(
      NAME outline_noswitch_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        TITLE="outline ${specimen} [${_target}]"
        USE_SUBDIR=yes
        CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_outline_noswitch_str} ${_format_flags} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
        ${_exit_status}
        ${_target}
    )
  endforeach()
endif()

# outlineSelection: seq7 tests on external C++ specimens
foreach(specimen IN LISTS OUTLINE_EXTERNAL_CXX_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  set(_target seq7a_${specimen}.passed)
  add_test(
    NAME outline_seq7a_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outlineSelection seq7 ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outlineSelection> ${_outline_includes_str} -rose:outline:seq 7 ${_outline_flags_str} -c ${_outline_external_dir}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

# outlineSelection: seq7 tests on local Fortran specimens
if(enable-fortran)
  foreach(specimen IN LISTS OUTLINE_FORTRAN_TESTS)
    string(REPLACE "." "_" _name ${specimen})
    if(specimen MATCHES "\\.f$")
      set(_format_flags "${_outline_fortran_fixed_form}")
    else()
      set(_format_flags "${_outline_fortran_free_form}")
    endif()
    set(_target seq7b_${specimen}.passed)
    add_test(
      NAME outline_seq7b_${_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${_rth}
        TITLE="outlineSelection seq7 ${specimen} [${_target}]"
        USE_SUBDIR=yes
        CMD="$<TARGET_FILE:outlineSelection> ${_outline_includes_str} -rose:verbose 0 -rose:outline:seq 7 ${_format_flags} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
        ${_exit_status}
        ${_target}
    )
  endforeach()
endif()

# Complex structural outlining test
set(_complexStruct_flags
  -rose:outline:new_file
  -rose:outline:temp_variable
  -rose:outline:exclude_headers
  -rose:outline:line 201)
string(JOIN " " _complexStruct_flags_str ${_complexStruct_flags})
add_test(
  NAME outline_complexStruct
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="outline complexStruct [complexStruct.passed]"
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_complexStruct_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/complexStruct.c"
    ${_exit_status}
    complexStruct.passed
)

# Dlopen-based outlining (C + C++)
set(_dlopen_flags
  ${_outline_dlopen_header_include}
  -rose:outline:use_dlopen
  -rose:outline:output_path .)
string(JOIN " " _dlopen_flags_str ${_dlopen_flags})
foreach(specimen IN LISTS _outline_c_and_cxx_tests)
  string(REPLACE "." "_" _name ${specimen})
  set(_target dlopen_${specimen}.passed)
  set(_specimen_lang_flags "")
  if(specimen IN_LIST OUTLINE_GNU89_C_TESTS)
    set(_specimen_lang_flags "-std=gnu89")
  endif()
  add_test(
    NAME outline_dlopen_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline dlopen ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_specimen_lang_flags} ${_dlopen_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

set(_dlopen2_flags
  ${_outline_dlopen_header_include}
  -rose:outline:use_dlopen
  -rose:outline:copy_orig_file
  -rose:outline:output_path .)
string(JOIN " " _dlopen2_flags_str ${_dlopen2_flags})
foreach(specimen IN LISTS _outline_c_and_cxx_tests)
  string(REPLACE "." "_" _name ${specimen})
  set(_target dlopen2_${specimen}.passed)
  set(_specimen_lang_flags "")
  if(specimen IN_LIST OUTLINE_GNU89_C_TESTS)
    set(_specimen_lang_flags "-std=gnu89")
  endif()
  add_test(
    NAME outline_dlopen2_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline dlopen2 ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_specimen_lang_flags} ${_dlopen2_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

add_test(
  NAME outline_test_inline_2022_6
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND bash -c
    "$<TARGET_FILE:outline> ${_outline_includes_str} ${_dlopen2_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/test_inline_1_2022.cpp && cp rose_test_inline_1_2022_lib.cpp rose_test_inline_2_2022_lib.cpp && ${CMAKE_CXX_COMPILER} -c -fPIC ${_outline_includes_str} rose_test_inline_1_2022_lib.cpp -o OUT_1_test_inline_2022_6.o && ${CMAKE_CXX_COMPILER} -c -fPIC ${_outline_includes_str} rose_test_inline_2_2022_lib.cpp -o OUT_2_test_inline_2022_6.o && ${CMAKE_CXX_COMPILER} -shared OUT_1_test_inline_2022_6.o OUT_2_test_inline_2022_6.o -o test_inline_2022_6.so"
)

set(_dlopen3_flags
  ${_outline_dlopen_header_include}
  -rose:outline:use_dlopen
  -rose:outline:copy_orig_file
  -rose:outline:exclude_headers
  -rose:unparseHeaderFiles
  -rose:unparseHeaderFilesRootFolder unparseHeaderRoot_test_20_2019)
string(JOIN " " _dlopen3_flags_str ${_dlopen3_flags})
foreach(specimen IN LISTS OUTLINE_CXX2_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  set(_target dlopen3_${specimen}.passed)
  add_test(
    NAME outline_dlopen3_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline dlopen3 ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_dlopen3_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

set(_dlopensimple_flags
  ${_outline_dlopen_header_include}
  -rose:outline:use_dlopen_simple
  -rose:outline:copy_orig_file
  -rose:outline:exclude_headers
  -rose:unparseHeaderFiles
  -rose:unparseHeaderFilesRootFolder unparseHeaderRoot_test_simple)
string(JOIN " " _dlopensimple_flags_str ${_dlopensimple_flags})
foreach(specimen IN LISTS OUTLINE_CXX2_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  set(_target dlopensimple_${specimen}.passed)
  add_test(
    NAME outline_dlopensimple_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="outline dlopensimple ${specimen} [${_target}]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:outline> ${_outline_includes_str} ${_dlopensimple_flags_str} -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}"
      ${_exit_status}
      ${_target}
  )
endforeach()

set(_dlopen4_flags
  -fpermissive
  ${_outline_dlopen_header_include}
  -rose:outline:select_omp_loop
  -rose:outline:use_dlopen
  -rose:outline:copy_orig_file
  -rose:unparseHeaderFilesRootFolder unparseHeaderRoot_dlopen4)
foreach(specimen IN LISTS OUTLINE_CXX4_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME outline_dlopen4_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:outline>
      ${_outline_includes}
      ${_dlopen4_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output dlopen4_${specimen}
  )
endforeach()

set(_dlopen5_flags
  -fpermissive
  ${_outline_dlopen_header_include}
  -rose:outline:select_omp_loop
  -rose:outline:use_dlopen_simple
  -rose:outline:copy_orig_file
  -rose:unparseHeaderFilesRootFolder unparseHeaderRoot_dlopen4)
foreach(specimen IN LISTS OUTLINE_CXX5_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME outline_dlopen5_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:outline>
      ${_outline_includes}
      ${_dlopen5_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output dlopen5_${specimen}
  )
endforeach()

set(_dlopen6_flags
  -fpermissive
  ${_outline_dlopen_header_include}
  -rose:outline:use_dlopen
  -rose:outline:copy_orig_file
  -rose:unparseHeaderFilesRootFolder unparseHeaderRoot_dlopen4)
foreach(specimen IN LISTS OUTLINE_CXX6_TESTS)
  string(REPLACE "." "_" _name ${specimen})
  add_test(
    NAME outline_dlopen6_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:outline>
      ${_outline_includes}
      ${_dlopen6_flags}
      -c ${CMAKE_CURRENT_SOURCE_DIR}/${specimen}
      -rose:output dlopen6_${specimen}
  )
endforeach()

add_test(
  NAME outline_test_class_ODR_violation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND bash -c
    "${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/test_class_ODR_violation_main.C && ${CMAKE_CXX_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/test_class_ODR_violation_outlined.C && ${CMAKE_CXX_COMPILER} test_class_ODR_violation_main.o test_class_ODR_violation_outlined.o -o test_class_ODR_violation"
)

