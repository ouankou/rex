add_library(util_support OBJECT
  CommandOptions.C)

install(FILES
  DAG.h TreeImpl.h BitVectorRepr.h DirectedGraph.h
  IteratorTmpl.h PtrMap.h union_find.h CommandOptions.h
  DoublyLinkedList.h LatticeElemList.h PtrSet.h const.h FunctionObject.h
  Matrix.h SinglyLinkedList.h CountRefHandle.h ObserveObject.h
  FileHelper.h merge_support.h test_support.h
  DESTINATION ${INCLUDE_INSTALL_DIR})
