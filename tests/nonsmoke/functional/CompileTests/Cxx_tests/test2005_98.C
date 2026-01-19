// This test code demonstrates an error with the gneration of code from the STL map container.

#include<map>

using namespace std;

void foo()
   {
  // Try to use a map to draw out error in compiling stl_map.h (error in processing Kull)
     map<int,float> xmap;

#if ( (__GNUC__ == 3) && (__GNUC_MINOR__ < 4) )
  // force instatiation of one of these: _Rb_tree_iterator
  // map<int,float>::iterator xmapIterator;
     _Rb_tree_iterator<int,int&,int*> xmapTreeIterator;
#else
  #warning "Case not tested for version 3.4 and higher."
#endif

     map<int, float>::iterator i = xmap.find(2);

     if (i == xmap.end())
        {
          i++;
        }
       else
        {
          i++;
        }

     xmap.erase(i);
     xmap.erase(i,i);

  // this generates an error!
     xmap.erase(1);
   }
