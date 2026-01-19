
// The first parameter must be size_t (but size_t is unsigned int, so this works)
inline void* operator new(unsigned int, void* __p)  { return __p; }

class MotionItem{
};

void foo()
   {
     void* x = 0L;

  // This is a more conventional call to the new operator
     MotionItem* item1 = new MotionItem();

  // This is a call to new with "placement" arguments
     MotionItem* item2 = new (x) MotionItem();

  // Allocates 100 int and returns a pointer to an int (SgTypeInt)
     int *array1 = new int(100);

  // Allocates an array 100 ints and returns a pointer to an array of int (SgArrayType)
     int *array2 = new int[100];

   };
