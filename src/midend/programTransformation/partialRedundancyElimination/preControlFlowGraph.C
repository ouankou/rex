#include "AstInterface_ROSE.h"

#include "CFG.h"

#include "preControlFlowGraph.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <memory>
#include <vector>

namespace legacy {

using namespace std;

class CfgConfig : public BuildCFGConfig<int> {
  PRE::ControlFlowGraph &graph;
  std::vector<std::unique_ptr<PRE::Vertex>> vertices;

public:
  CfgConfig(PRE::ControlFlowGraph &graph) : graph(graph) {}

  virtual PRE::Vertex *CreateNode() {
    PRE::Vertex vertex = graph.graph.add_vertex();
    graph.node_statements.push_back(vector<SgNode *>());
    vertices.push_back(std::make_unique<PRE::Vertex>(vertex));
    return vertices.back().get();
  }

  virtual void CreateEdge(PRE::Vertex *src, PRE::Vertex *tgt,
                          CFGConfig::EdgeType condval) {
    graph.graph.add_edge(*src, *tgt);
    graph.edge_type.push_back(condval);
    graph.edge_insertion_point.push_back(make_pair((SgNode *)0, false));
  }

  virtual void AddNodeStmt(PRE::Vertex *n, const AstNodePtr &s) {
    ROSE_ASSERT(s != NULL);
    graph.node_statements[*n].push_back(AstNodePtrImpl(s).get_ptr());
  }
};

void PRE::makeCfg(SgFunctionDefinition *n, PRE::ControlFlowGraph &graph) {
  CfgConfig cfgconfig(graph);
  AstInterfaceImpl astiImpl = AstInterfaceImpl(n);
  AstInterface asti(&astiImpl);
  ROSE_Analysis::BuildCFG(asti, AstNodePtrImpl(n), cfgconfig);
}

void PRE::addEdgeInsertionPoints(PRE::ControlFlowGraph &controlflow) {
  VertexIter i = controlflow.graph.vertices().begin(),
             iend = controlflow.graph.vertices().end();
  for (; i != iend; ++i) {
    OutEdgeIter out = controlflow.graph.out_edges(*i).begin(),
                outend = controlflow.graph.out_edges(*i).end();

    const vector<SgNode *> &stmts = controlflow.node_statements[*i];
    SgNode *last_stmt = stmts.empty() ? 0 : stmts[stmts.size() - 1];
    int out_edge_count = distance(out, outend);
    SgStatement *expr_parent = 0;

    for (vector<SgNode *>::const_iterator j = stmts.begin(); j != stmts.end();
         ++j) {
      ROSE_ASSERT((*j) != NULL);
      if (expr_parent)
        continue;
      if (isSgExpression(*j)) {
        expr_parent = SageInterface::getEnclosingStatement(*j);
        if (expr_parent == NULL) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[partial-redundancy-cfg-expression-"
                  "owner]: expression=%p/%s parent=%p has no enclosing "
                  "statement\n",
                  static_cast<void *>(*j), (*j)->class_name().c_str(),
                  static_cast<void *>((*j)->get_parent()));
          ROSE_ABORT();
        }
      } else {
        if (isSgExprStatement(*j)) {
          expr_parent = isSgStatement((*j)->get_parent());
          if (isSgBasicBlock(expr_parent) || isSgForInitStatement(expr_parent))
            expr_parent = 0;
        }
      }
    }

    if (!expr_parent) {
      expr_parent = isSgStatement(last_stmt);
    }
    if (out_edge_count == 1 && !isSgForStatement(expr_parent)) {
      // Inserting into an absent initializer replaces its null child. Keep
      // the owning list as the stable anchor for every planned insertion.
      if (auto *absence = isSgNullStatement(expr_parent)) {
        if (auto *initializer = isSgForInitStatement(absence->get_parent())) {
          ROSE_ASSERT(initializer->get_init_stmt().size() == 1 &&
                      initializer->get_init_stmt().front() == absence);
          expr_parent = initializer;
        }
      }
      controlflow.edge_insertion_point[*out] =
          make_pair(expr_parent, (isSgGotoStatement(expr_parent)
                                      ?
                                      /* Put insertion before goto statements
                                         but after other statement types */
                                      true
                                      : false));
    } else {
      if (out_edge_count == 1 && isSgForStatement(expr_parent)) {
        // Increment of for statement
        controlflow.edge_insertion_point[*out] =
            make_pair(isSgForStatement(expr_parent)->get_increment(),
                      false); // Put in after expression using operator,
      } else {
        if (out_edge_count >= 2) {
          for (; out != outend; ++out) {
            pair<SgStatement *, bool> insert_point;
            CFGConfig::EdgeType kind = controlflow.edge_type[*out];
          try_next_ancestor:
            ROSE_ASSERT(expr_parent != NULL);
            switch (expr_parent->variantT()) {
            case V_SgDoWhileStmt:
              if (kind == CFGConfig::COND_FALSE) {
                insert_point = make_pair(expr_parent, false);
              } else {
                fprintf(stderr,
                        "REX_AST_INVARIANT[partial-redundancy-do-while-edge]: "
                        "expected false CFG edge\n");
                ROSE_ABORT();
              }
              break;

            case V_SgIfStmt:
              if (kind == CFGConfig::COND_TRUE) {
                insert_point =
                    make_pair(isSgIfStmt(expr_parent)->get_true_body(), true);
              } else {
                if (isSgIfStmt(expr_parent)->get_false_body() == NULL) {
                  SgBasicBlock *bb = SageBuilder::buildBasicBlock();
                  SageInterface::publishGeneratedSubtreeOutputOwner(
                      bb, expr_parent);
                  isSgIfStmt(expr_parent)->set_false_body(bb);
                  bb->set_parent(expr_parent);
                }
                ROSE_ASSERT(isSgIfStmt(expr_parent)->get_false_body());
                insert_point =
                    make_pair(isSgIfStmt(expr_parent)->get_false_body(), true);
              }
              break;

            case V_SgWhileStmt:
              if (kind == CFGConfig::COND_TRUE) {
                insert_point =
                    make_pair(isSgWhileStmt(expr_parent)->get_body(), true);
              } else {
                insert_point = make_pair(expr_parent, false);
              }
              break;

            case V_SgSwitchStatement:
              fprintf(stderr,
                      "REX_AST_INVARIANT[partial-redundancy-switch-edge]: "
                      "switch insertion edge is unsupported\n");
              ROSE_ABORT();

            case V_SgForStatement:
              if (kind == CFGConfig::COND_TRUE) {
                insert_point = make_pair(
                    isSgForStatement(expr_parent)->get_loop_body(), true);
              } else {
                insert_point = make_pair(expr_parent, false);
              }
              break;
            case V_SgExprStatement:
            case V_SgGotoStatement:
            case V_SgLabelStatement:
            case V_SgBasicBlock: {
              expr_parent = isSgStatement(expr_parent->get_parent());
              goto try_next_ancestor;
            }
            default:
              cerr << "Unknown variant " << expr_parent->sage_class_name()
                   << endl;
              expr_parent->get_file_info()->display("Location in input code");
              ROSE_ABORT();
            }

            if (insert_point.first)
              controlflow.edge_insertion_point[*out] = insert_point;
          }
        }
      }
    }
  }

  EdgeIter ei = controlflow.graph.edges().begin(),
           eend = controlflow.graph.edges().end();
  for (; ei != eend; ++ei) {
    if (!controlflow.edge_insertion_point[*ei].first) {
      Vertex src = controlflow.graph.source(*ei);
      if (controlflow.graph.out_edges(src).size() == 1 &&
          !controlflow.node_statements[src].empty()) {
        const vector<SgNode *> &stmts = controlflow.node_statements[src];
        controlflow.edge_insertion_point[*ei] = make_pair(stmts.back(), false);
      }
    }
  }
}

} // namespace legacy
