void foo();

namespace internal {

int variable;

void foo() 
   {
     int extension = variable;
   }

}  // namespace internal

