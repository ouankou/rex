#include "roseInternal.h"

#include "sage3basic.h"

#include "AstStatistics.h"

#include <sstream>

using namespace std;

using namespace ROSE_Statistics;

// ************************************************************************
//                AstNodeTraversalStatistics member functions
// ************************************************************************

AstNodeTraversalStatistics::AstNodeTraversalStatistics() {
  numNodeTypes.resize(V_SgNumVariants);
}

AstNodeTraversalStatistics::~AstNodeTraversalStatistics() {}

string AstNodeTraversalStatistics::toString(SgNode *node) {
  string s;
  traverse(node, preorder);
  SgProject *n = dynamic_cast<SgProject *>(node);
  if (n != NULL) {
    AstNodeTraversalStatistics stat2;
    stat2.traverseInputFiles(n, preorder);
    s = stat2.cmpStatistics(*this);
  } else {
    s = singleStatistics();
  }
  return s;
}

void AstNodeTraversalStatistics::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(node->variantT() < V_SgNumVariants);
  numNodeTypes.at(node->variantT())++;
}

string AstNodeTraversalStatistics::singleStatistics() {
  ostringstream ss;
  for (size_t i = 0; i != numNodeTypes.size(); i++) {
    if (numNodeTypes.at(i) > 0) {
      if (SgProject::get_verbose() >= 2)
        ss << "AST Traversal Statistics: " << setw(8) << numNodeTypes[i] << ": "
           << getVariantName(VariantT(i)) << endl;
    }
  }
  return ss.str();
}

string
AstNodeTraversalStatistics::cmpStatistics(AstNodeTraversalStatistics &q) {
  ostringstream ss;
  const StatisticsContainerType &numNodeTypes2 = q.numNodeTypes;
  ElementType sum1 = 0, sum2 = 0;

  ss << "**********************************************************************"
        "**********************************************\n";
  ss << "AST Traversal Statistics (traversed in current file) : (total in AST) "
        ": (percent of total traversed in current file)\n";
  ss << "**********************************************************************"
        "**********************************************\n";

  for (unsigned int i = 0; i != numNodeTypes.size(); i++) {
    if (numNodeTypes.at(i) > 0 || numNodeTypes2.at(i) > 0) {
      ss << generateCMPStatisticsValueString(getVariantName(VariantT(i)),
                                             numNodeTypes[i], numNodeTypes2[i]);
    }
    sum1 += numNodeTypes[i];
    sum2 += numNodeTypes2[i];
  }

  ss << generateCMPStatisticsValueString("TOTAL", sum1, sum2);
  ss << "**********************************************************************"
        "**********************************************\n";

  return ss.str();
}

// if the higher values are input data % is < 100.
string AstNodeTraversalStatistics::generateCMPStatisticsValueString(
    string name, ElementType v1, ElementType v2) {
  ostringstream ss;
  ss << "AST Traversal Statistics:";
  ss << setw(6) << v1 << ":" << setw(6) << v2 << ":";
  if (v2 > 0) {
    ss.setf(ios::fixed | ios::showpoint);
    ss << " " << setprecision(1) << setw(5)
       << (static_cast<float>(v1) / static_cast<float>(v2)) * 100.0 << "%";
  } else {
    ss << "-N/A-";
  }
  ss << " " << name;
  ss << endl;

  return ss.str();
}

// ************************************************************************
//                    AstNodeMemoryPoolStatistics member functions
// ************************************************************************

AstNodeMemoryPoolStatistics::AstNodeMemoryPoolStatistics() {
  // Initialize the total amount of memory used so that we can report fractional
  // percentage of use per IR node.
  totalMemoryUsed = memoryUsage();
  printf("Total memory used = %zu \n", totalMemoryUsed);
  printf("numberOfNodes = %" PRIuPTR " \n", numberOfNodes());

  statement_nodes = 0;
  statement_memoryFootprint = 0;
  statement_percent = 0.0;

  declaration_nodes = 0;
  declaration_memoryFootprint = 0;
  declaration_percent = 0.0;

  support_nodes = 0;
  support_memoryFootprint = 0;
  support_percent = 0.0;

  expression_nodes = 0;
  expression_memoryFootprint = 0;
  expression_percent = 0.0;

  type_nodes = 0;
  type_memoryFootprint = 0;
  type_percent = 0.0;

  symbol_nodes = 0;
  symbol_memoryFootprint = 0;
  symbol_percent = 0.0;

  node_nodes = 0;
  node_memoryFootprint = 0;
  node_percent = 0.0;

  file_info_nodes = 0;
  file_info_memoryFootprint = 0;
  file_info_percent = 0.0;
}

AstNodeMemoryPoolStatistics::~AstNodeMemoryPoolStatistics() {}

#define IR_NODE_VISIT_CASE(X)                                                  \
  case V_##X: {                                                                \
    X *castNode = is##X(node);                                                 \
    ROSE_ASSERT(castNode != NULL);                                             \
    std::size_t numberOfNodes = castNode->numberOfNodes();                     \
    std::size_t memoryFootprint = castNode->memoryUsage();                     \
    double percent =                                                           \
        (((double)memoryFootprint) / ((double)totalMemoryUsed)) * 100.0;       \
    if (SgProject::get_verbose() >= 0)                                         \
      printf("AST Memory Pool Statistics: numberOfNodes = %9zu memory "        \
             "consumption = %10zu bytes (%6.3f percent of total) sizeof() = "  \
             "%4zu node = %s \n",                                              \
             numberOfNodes, memoryFootprint, percent, sizeof(*castNode),       \
             castNode->class_name().c_str());                                  \
    if (isSgStatement(node) != NULL) {                                         \
      statement_nodes += numberOfNodes;                                        \
      statement_memoryFootprint += memoryFootprint;                            \
      statement_percent += percent;                                            \
    }                                                                          \
    if (isSgDeclarationStatement(node) != NULL) {                              \
      declaration_nodes += numberOfNodes;                                      \
      declaration_memoryFootprint += memoryFootprint;                          \
      declaration_percent += percent;                                          \
    }                                                                          \
    if (isSgSupport(node) != NULL) {                                           \
      support_nodes += numberOfNodes;                                          \
      support_memoryFootprint += memoryFootprint;                              \
      support_percent += percent;                                              \
    }                                                                          \
    if (isSgExpression(node) != NULL) {                                        \
      expression_nodes += numberOfNodes;                                       \
      expression_memoryFootprint += memoryFootprint;                           \
      expression_percent += percent;                                           \
    }                                                                          \
    if (isSgType(node) != NULL) {                                              \
      type_nodes += numberOfNodes;                                             \
      type_memoryFootprint += memoryFootprint;                                 \
      type_percent += percent;                                                 \
    }                                                                          \
    if (isSgSymbol(node) != NULL) {                                            \
      symbol_nodes += numberOfNodes;                                           \
      symbol_memoryFootprint += memoryFootprint;                               \
      symbol_percent += percent;                                               \
    }                                                                          \
    if (isSgNode(node) != NULL) {                                              \
      node_nodes += numberOfNodes;                                             \
      node_memoryFootprint += memoryFootprint;                                 \
      node_percent += percent;                                                 \
    }                                                                          \
    if (isSg_File_Info(node) != NULL) {                                        \
      file_info_nodes += numberOfNodes;                                        \
      file_info_memoryFootprint += memoryFootprint;                            \
      file_info_percent += percent;                                            \
    }                                                                          \
    break;                                                                     \
  }

void AstNodeMemoryPoolStatistics::visit(SgNode *node) {
  // Visit one representative per pool and query that exact node type.
  switch (node->variantT()) {

    ROSE_AST_FOR_EACH_NODE(IR_NODE_VISIT_CASE)

  default: {
    std::cerr << "Invalid AST node in memory statistics: " << node->class_name()
              << std::endl;
    ROSE_ABORT();
  }
  }
}

// ************************************************************************
//                    AstNodeStatistics member functions
// ************************************************************************

string AstNodeStatistics::traversalStatistics(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  AstNodeTraversalStatistics stat;
  return stat.toString(node);
}

string AstNodeStatistics::IRnodeUsageStatistics() {
  string s = "\n\n";
  AstNodeMemoryPoolStatistics memoryPoolTraversal;
  memoryPoolTraversal.traverseRepresentativeIRnodes();

  // DQ (3/8/2021): Adding support for representing different classes of IR
  // nodes (e.g. statements, declarations, expressions, support, etc.).
  if (SgProject::get_verbose() >= 0) {
    printf("\n\nStatistics by class or IR node: \n");
    printf(
        "AST Memory Pool Statistics: numberOfNodes = %9zu memory consumption "
        "= %10zu bytes (%7.3f percent of total) sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.node_nodes,
        memoryPoolTraversal.node_memoryFootprint,
        memoryPoolTraversal.node_percent, sizeof(SgNode), "SgNode");

    printf(
        "\nAST Memory Pool Statistics: numberOfNodes = %9zu memory consumption "
        "= %10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.statement_nodes,
        memoryPoolTraversal.statement_memoryFootprint,
        memoryPoolTraversal.statement_percent, sizeof(SgStatement),
        "SgStatement");
    printf(
        "AST Memory Pool Statistics: numberOfNodes = %9zu memory consumption = "
        "%10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.declaration_nodes,
        memoryPoolTraversal.declaration_memoryFootprint,
        memoryPoolTraversal.declaration_percent, sizeof(SgDeclarationStatement),
        "SgDeclarationStatement");

    printf(
        "\nAST Memory Pool Statistics: numberOfNodes = %9zu memory consumption "
        "= %10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.support_nodes,
        memoryPoolTraversal.support_memoryFootprint,
        memoryPoolTraversal.support_percent, sizeof(SgSupport), "SgSupport");
    printf(
        "AST Memory Pool Statistics: numberOfNodes = %9zu memory consumption = "
        "%10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.file_info_nodes,
        memoryPoolTraversal.file_info_memoryFootprint,
        memoryPoolTraversal.file_info_percent, sizeof(Sg_File_Info),
        "Sg_File_Info");

    printf(
        "\nAST Memory Pool Statistics: numberOfNodes = %9zu memory consumption "
        "= %10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.expression_nodes,
        memoryPoolTraversal.expression_memoryFootprint,
        memoryPoolTraversal.expression_percent, sizeof(SgExpression),
        "SgExpression");

    printf(
        "\nAST Memory Pool Statistics: numberOfNodes = %9zu memory consumption "
        "= %10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.type_nodes,
        memoryPoolTraversal.type_memoryFootprint,
        memoryPoolTraversal.type_percent, sizeof(SgType), "SgType");

    printf(
        "\nAST Memory Pool Statistics: numberOfNodes = %9zu memory consumption "
        "= %10zu bytes (%6.3f percent of total)  sizeof() = %4zu node = %s \n",
        memoryPoolTraversal.symbol_nodes,
        memoryPoolTraversal.symbol_memoryFootprint,
        memoryPoolTraversal.symbol_percent, sizeof(SgSymbol), "SgSymbol");
  }

  return s;
}
