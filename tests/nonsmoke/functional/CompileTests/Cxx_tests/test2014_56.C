template<class T>
class Map 
   {
     public:
       template <class Derived> class BidirectionalIterator {
       public:
         BidirectionalIterator();
       };

       class NodeIterator : public BidirectionalIterator<NodeIterator> {
         typedef BidirectionalIterator<NodeIterator> Super;
       };
   };
