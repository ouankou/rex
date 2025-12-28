include_directories(. ${ROSE_INCLUDES})

add_executable(rosebud
  rosebud.C
  Ast.C
  CxxGenerator.C
  Generator.C
  NoneGenerator.C
  NoneSerializer.C
  RoseGenerator.C
  RosettaGenerator.C
  Serializer.C
  Utility.C)

add_executable(unitTest01
  rosebud.C
  Ast.C
  CxxGenerator.C
  Generator.C
  NoneGenerator.C
  NoneSerializer.C
  RoseGenerator.C
  RosettaGenerator.C
  Serializer.C
  Utility.C)

add_dependencies(rosebud util_sawyer)
add_dependencies(unitTest01 util_sawyer)

set(link_libs ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(rosebud ${link_libs} util_sawyer)
target_link_libraries(unitTest01 ${link_libs} util_sawyer)
