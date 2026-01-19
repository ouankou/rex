
template <typename T>
class map
   {
public:
  // This case will work...but map is translated to the typedef base type.
  // typedef _Rb_tree_iterator<int>       iterator;
  typedef int iterator;
   };

class map_no_template
   {
public:
  // This case will work...but map is translated to the typedef base type.
  // typedef _Rb_tree_iterator<int>       iterator;
  typedef int iterator;
   };

class foo
   {
     public:
          void foobar()
             {
            // This will fail (seems it has to be a part of a template.
               map<int>::iterator it;

            // This will preserve the typedef type in the class map_no_template
               map_no_template::iterator it2;
             }
   };




