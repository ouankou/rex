add_library(rose_StringUtility OBJECT
  Convert.C
  Escape.C
  FileNameClassifier.C
  FileUtility.C
  NumberToString.C
  Predicate.C
  Replace.C
  SplitJoin.C
  StringToNumber.C)

add_dependencies(rose_StringUtility generate_rosePublicConfig)
target_compile_definitions(rose_StringUtility PRIVATE ROSE_UTIL_EXPORTS)

install(
  FILES Convert.h Escape.h FileUtility.h IntegerOps.h NumberToString.h Predicate.h Replace.h SplitJoin.h StringToNumber.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/Rose/StringUtility)
