// This code attempts to use every modifier in C++
#define RESTRICT restrict
#define EXPORT export

#define TEST_INLINING       0
#define TEST_CONST_MEMBERS  0
#define TEST_STATIC_MEMBERS 0

class classType
   {
public:
#if TEST_STATIC_MEMBERS
          static double publicStaticDoubleValue;
#endif
#if TEST_CONST_MEMBERS
          const double publicConstDoubleValue;
#endif

#if TEST_INLINING
          void noninlinePublicMemberFunctionWithDefinition () {};
          void noninlinePublicMemberFunctionWithoutDefinition ();
       // "inline" specification is optional here if it appears 
       // in the declaration where the function is defined
          inline void inlinePublicMemberFunctionWithoutDefinition ();
          void inlinePublicMemberFunctionWithoutDefinitionSpecifiedInDefnOnly ();
          inline void inlinePublicMemberFunctionWithoutDefinitionSpecifiedInDeclOnly ();
          inline void inlinePublicMemberFunction () {};
#endif

       // void publicMemberFunctionIntegerParameter ( register int i ) {};
       // classType ( register int* i ) {};
#if TEST_CONST_MEMBERS
          classType()
              // g++ does not require the initializers of const variables,
              // legacy frontend does
              : publicConstDoubleValue(42), protectedConstDoubleValue(43),
                privateConstDoubleValue(44) {};
#endif

#if TEST_CONST_MEMBERS
          explicit classType(register int integerValueParameter)
              // g++ does not require the initializers of const variables,
              // legacy frontend does
              : publicConstDoubleValue(1), protectedConstDoubleValue(2),
                privateConstDoubleValue(3) {};
#endif

        protected:
#if TEST_CONST_MEMBERS
          const double protectedConstDoubleValue;
#endif
#if TEST_STATIC_MEMBERS
          static double protectedStaticDoubleValue;
#endif

        private:
#if TEST_CONST_MEMBERS
          const double privateConstDoubleValue;
#endif
#if TEST_STATIC_MEMBERS
          static double privateStaticDoubleValue;
#endif
   };

#if TEST_INLINING
// No inline specifier was used in the declaration in the class so none appears here
void classType::noninlinePublicMemberFunctionWithoutDefinition () {}

// No inline specifier in the in class declaration (but it should appear in the unparsed code)
inline void classType::inlinePublicMemberFunctionWithoutDefinitionSpecifiedInDefnOnly () {}

// No inline specifier in the in class definition (but it should appear in the declaration of the unparsed code)
void classType::inlinePublicMemberFunctionWithoutDefinitionSpecifiedInDeclOnly () {}

// "inline" specification is optional here if it appears in the declaration in the class
inline void classType::inlinePublicMemberFunctionWithoutDefinition () {}
#endif

#if TEST_STATIC_MEMBERS
// intializers for const members (not required by g++)
double classType::publicStaticDoubleValue    = 0;
double classType::protectedStaticDoubleValue = 0;
double classType::privateStaticDoubleValue   = 0;
#endif
