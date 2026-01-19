
class A
   {
     public:
          int intValue;
          A(){ intValue = 42; };
   };

class B
   {
     public:
          A myBaseClass;
          B(){};
   };

/* Current error in unparsed code:
class B 
{
  private: 
  class A 
myBaseClass;
  public: inline B();
}
*/

   int main() {
     // Build object so that we can call the constructor
     B objectB;
     const B & objectBref = objectB;

     int x;

  // will be unparsed as:  
  // x = ((((class B &)(objectBref)).myBaseClass).intValue);
     x = ((B &)objectBref).myBaseClass.intValue;

     // printf ("Program Terminated Normally! \n");
     return 0;
   }
