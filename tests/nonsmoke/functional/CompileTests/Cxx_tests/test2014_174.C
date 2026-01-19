
class X 
   {
     public:
          virtual X* getParentNode() = 0;
   };

void foobar()
   {
  // Failing example from Xerces: for (class DOMNode *pB = pointB ->  getParentNode (), pA = pointA ->  getParentNode (); pB != pA; (pB = pB ->  getParentNode () , pA = pA ->  getParentNode ()))

  // X xyz;
     X* point_a = 0L;
     X* point_b = 0L;
     // for (X* a = 0L, *b = 0L; a != b; a++,b++)
     // for (X* a = point_a->getParentNode(), *b = point_b->getParentNode(); a
     // != b; a++,b++) DQ (8/30/2014): The handling of multiple variables
     // depends upon the source sequence numbering; and this is sensative to
     // white space in legacy frontend. So we need a better way to handle this.
     for (X* a = point_a->getParentNode(), 
            *b = point_b->getParentNode(); 
             a != b; 
             a = a->getParentNode() ,b = b->getParentNode())
        {
     }

  // Test using non-variable declarations.
     int i,j;
     for (i = 0, 
          j = 0;
          i != j;
          i++,j++)
        {
        }
   }

