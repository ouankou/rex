class XSValue // : public XMemory
   {
     public:

         ~XSValue();

         struct XSValue_Data {
           // Using an un-named scope was a bug.
           union {
             bool f_bool;
             double f_double;
           } fValue;

         } fData;
   };

   XSValue::~XSValue() {
     // Unparsed as:   (this) -> fData . XSValue_Data::fValue .
     // XSValue_Data::f_double;
     fData.fValue.f_double;
   }
