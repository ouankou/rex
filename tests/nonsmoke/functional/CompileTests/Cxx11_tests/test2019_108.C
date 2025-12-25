namespace N
   {
     template <class T> class X { };

     // Note: legacy frontend generated two nondefining declarations here, one
     // is compiler generated but not marked as compiler generated in the
     // typical way. It uses a flag (compiler_generated_forward_decl) in the
     // secondary_decl struct. It is however not a problem for ROSE to unparse
     // both.
     template<> class N::X<long>; 
   }

template<> class N::X<long> final { };
