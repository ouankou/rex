
include_directories(${CMAKE_SOURCE_DIR}/src/util ${CMAKE_SOURCE_DIR}/src/util/support)


########### next target ###############


########### install files ###############

install(FILES SymbolicMultiply.h SymbolicPlus.h BooleanOperators.h UnaryOperators.h
              SymbolicVal.h SymbolicExpr.h SymbolicSelect.h SymbolicBound.h
              DESTINATION ${INCLUDE_INSTALL_DIR})
