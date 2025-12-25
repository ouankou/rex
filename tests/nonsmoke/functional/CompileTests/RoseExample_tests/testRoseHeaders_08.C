
#include <boost/graph/adjacency_list.hpp>

class DummyBlock;

namespace ControlFlow {
class Graphs {
public:
  typedef boost::adjacency_list<
      boost::setS, /* edges of each vertex in std::list */
      boost::vecS, /* vertices in std::vector */
      boost::bidirectionalS,
      boost::property<boost::vertex_name_t, DummyBlock *>>
      Graph;
};
} // namespace ControlFlow

class AsmUnparser 
   {
     public:
       typedef ControlFlow::Graphs::Graph CFG;

       // DQ (7/3/2013): This code will compile fine with ROSE configured to be
       // a GNU 4.4.5 compiler if this line is commented out. Uncommented the
       // file will compile only if ROSE is configured to be a GNU 4.2.4
       // compiler. For both GNU g++ version 4.2.4 and version 4.4.5 the file
       // compiles just fine (with the line uncommented).
#if 1
         typedef boost::graph_traits<CFG>::vertex_descriptor CFG_Vertex;
#endif
};
