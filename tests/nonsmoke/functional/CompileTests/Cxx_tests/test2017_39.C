namespace foospace 
   {
     template < typename T> int barfunction(T blah){return 17;}

     // Note that the name qualification here is not required (GNU compilers
     // will flag it as an error, while legacy frontend accepts it).
     template<> int foospace::barfunction<int>(int blah);
   }

namespace foospace 
   {
// This is allowed by legacy frontend, but not by GNU:
// --- error: explicit qualification in declaration of 'int
// foospace::barfunction(int)' NOTE: This is a SgTemplateInstantiationDirective
// extern template int foospace::barfunction<int>(int blah);
extern template int barfunction<int>(int blah);
   }

