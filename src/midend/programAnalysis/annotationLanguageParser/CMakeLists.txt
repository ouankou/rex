find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(
  AnnotationLanguageParserBison
  ${CMAKE_CURRENT_SOURCE_DIR}/language.y
  ${CMAKE_CURRENT_BINARY_DIR}/language-parser.cc
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/language.tab.h
  COMPILE_FLAGS "-pann")

FLEX_TARGET(
  AnnotationLanguageParserFlex
  ${CMAKE_CURRENT_SOURCE_DIR}/language.l
  ${CMAKE_CURRENT_BINARY_DIR}/language-lexer.cc)

ADD_FLEX_BISON_DEPENDENCY(AnnotationLanguageParserFlex AnnotationLanguageParserBison)

add_library(annotationLanguageParser STATIC
  ${BISON_AnnotationLanguageParserBison_OUTPUTS}
  ${FLEX_AnnotationLanguageParserFlex_OUTPUTS}
  annotations.cc
  annvariable.cc
  structure.cc
  enum_property.cc
  procedure.cc
  set_property.cc
  pointerrule.cc
  analyze.cc
  rule.cc
  broadway_expr.cc
  report.cc)

target_include_directories(annotationLanguageParser
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
  PRIVATE
    ${ROSE_INCLUDES})
