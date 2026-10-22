// Author: Markus Schordan
#ifndef AST_STATISTICS_H
#define AST_STATISTICS_H

#include "AstProcessing.h"

#include <cstddef>
#include <iomanip>

#include <string>

namespace ROSE_Statistics {
class AstNodeTraversalStatistics : public SgSimpleProcessing {
public:
  typedef unsigned int ElementType;
  typedef std::vector<ElementType> StatisticsContainerType;

  AstNodeTraversalStatistics();
  virtual ~AstNodeTraversalStatistics();
  virtual std::string toString(SgNode *node);

protected:
  virtual void visit(SgNode *node);
  std::string singleStatistics();
  std::string cmpStatistics(AstNodeTraversalStatistics &q);
  std::string generateCMPStatisticsValueString(std::string name, ElementType v1,
                                               ElementType v2);
  StatisticsContainerType numNodeTypes;
};

class AstNodeMemoryPoolStatistics : public ROSE_VisitTraversal {
public:
  std::size_t totalMemoryUsed;

  std::size_t statement_nodes;
  std::size_t statement_memoryFootprint;
  double statement_percent;

  std::size_t declaration_nodes;
  std::size_t declaration_memoryFootprint;
  double declaration_percent;

  std::size_t support_nodes;
  std::size_t support_memoryFootprint;
  double support_percent;

  std::size_t expression_nodes;
  std::size_t expression_memoryFootprint;
  double expression_percent;

  std::size_t type_nodes;
  std::size_t type_memoryFootprint;
  double type_percent;

  std::size_t symbol_nodes;
  std::size_t symbol_memoryFootprint;
  double symbol_percent;

  std::size_t node_nodes;
  std::size_t node_memoryFootprint;
  double node_percent;

  std::size_t file_info_nodes;
  std::size_t file_info_memoryFootprint;
  double file_info_percent;

  AstNodeMemoryPoolStatistics();
  virtual ~AstNodeMemoryPoolStatistics();

protected:
  void visit(SgNode *node);
};

// end of "ROSE_Statistics" namespace
} // namespace ROSE_Statistics

/*! \brief Statistics about IR nodes used.

    This class contains a number of static functions which can be called (return
   strings) to report on the fequency, memory usage, etc. of IR nodes.

    \internal These functions are implemented using a range of traversals
   supported by ROSE for either traversing a specified subtree of the AST, the
   memory pools using to hold the AST, or traversals of representative IR nodes
   (visits one type or IR node only in the AST).
 */
class ROSE_DLL_API AstNodeStatistics {
public:
  //! This outputs the types and count of IR nodes used in a traversal of the
  //! input file (excluding header files)
  static std::string traversalStatistics(SgNode *node);

  //! This outputs the types, counts, and memory useage of IR nodes appearing in
  //! the memory pools (whole AST).
  static std::string IRnodeUsageStatistics();
};

#endif
