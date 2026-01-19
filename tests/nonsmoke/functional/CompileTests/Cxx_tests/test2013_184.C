

namespace A
   {
     int a;
     namespace B {
     namespace C {
     // BUG: If we put namespace A, then when C is added to the global scope,
     // the "using namespace A::B::C;" will be unparsed as: "using namespace
     // B::C;" using namespace A;
     }
     } // namespace B

      using namespace B::C;
   }

using namespace A::B::C;
