


// #include <iostream>
struct super
   {
     virtual int operator [] (char*) = 0;
   };

struct dummy : public super
   {
     virtual int operator [] (char* lala) {return 0;}
   };

int main()
   {
  // dummy()["Kuh"];
     dummy::dummy()["Kuh"];
   }
