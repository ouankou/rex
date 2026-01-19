#include <vector>
typedef int    Index_t ; 

struct Domain
{
  public:
    // non-reference type
    Index_t  numNode()            { return m_numNode ; }

    void AllocateNodeElemIndexes()
    {
      Index_t numNode = this->numNode() ;
    }

  private:
    Index_t   m_numNode ;
} domain; 


