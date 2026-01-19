template <class Tag>
struct keyword
   {
  // This is a static template variable.
     static keyword<Tag> instance;
   };

template <class Tag> keyword<Tag> keyword<Tag>::instance = {};

struct weight_map {};

// keyword<weight_map> _weight_map = keyword<weight_map> ::instance;

struct weight_map2 {};

// keyword<weight_map2> _weight_map2 = keyword<weight_map2> ::instance;

struct distance_map {};

// keyword<distance_map> _distance_map = keyword<distance_map> ::instance;

void foobar()
   {
     keyword<weight_map> ::instance;
     keyword<weight_map2> ::instance;
     keyword<distance_map> ::instance;
   }
