template<class K, class T>
class Map 
   {
     public:
          typedef K Key;
          typedef T Value;

          class Node {};

          template<class Derived, class Value>
          class BidirectionalIterator
             {
               public:
                    BidirectionalIterator();
             };

             class NodeIterator
                 : public BidirectionalIterator<NodeIterator, Node> {
               typedef BidirectionalIterator<NodeIterator, Node> Super;

             public:
               NodeIterator();
             };
   };
