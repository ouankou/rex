// ROSE-40 (Kull)

namespace namespace1 {
  class Class1 {
  public:
    virtual double func_and_local1();
  };

  double func_and_local1();
}

double namespace1::Class1::func_and_local1() 
   {
     double func_and_local1;

  // Original code:  func_and_local1 = namespace1::func_and_local1();
  // Generated code: func_and_local1 = func_and_local1();
     func_and_local1 = namespace1::func_and_local1();

     return func_and_local1;
   }
