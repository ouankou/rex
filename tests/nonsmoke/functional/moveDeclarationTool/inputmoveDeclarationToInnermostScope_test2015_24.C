#define HAVE_SAM

#ifdef HAVE_SAM

#include <vector>

namespace XXX {

class BndMapping 
   {
     public:
          BndMapping();
   };
}



namespace XXX {

class BlockMapping 
   {
     public:

          void reorganizeBoundaries();

          void addBoundary(const BndMapping& boundary) { m_boundaries.push_back(boundary); }

     private:
          std::vector<BndMapping> m_boundaries;
   };

}

namespace XXX 
{

}  // closing brace for namespace statement

#endif  // closing endif for HAVE_SAM guard



