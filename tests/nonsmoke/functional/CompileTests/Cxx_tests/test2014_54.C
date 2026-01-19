#include <map>

template<class K, class T>
class Map {
public:
    typedef K Key;
    typedef T Value;

    typedef std::map<Key, Value> StlMap;

    class Node {};

    template<class Derived, class Value, class BaseIterator>
    class my_BidirectionalIterator: public std::iterator<std::bidirectional_iterator_tag, Value> {
    public:
        my_BidirectionalIterator();
    };

    class NodeIterator
        : public my_BidirectionalIterator<NodeIterator, Node,
                                          typename StlMap::iterator> {
      typedef my_BidirectionalIterator<NodeIterator, Node,
                                       typename StlMap::iterator>
          Super;

    public:
      NodeIterator();
    };
};

