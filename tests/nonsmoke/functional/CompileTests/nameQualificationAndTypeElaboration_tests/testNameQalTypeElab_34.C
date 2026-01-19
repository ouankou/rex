// number #34

typedef enum enumType1{};

class Array
   {
     public:
       // Error: This becomes a SgEnumDeclaration in the AST
       // Is this a typedef which has no name?
       // Within this scope ::enumType1 is hidden (should be in legacy
       // frontend's list of hidden names)
       typedef enum enumType1 {};
       //        Array(enumType1 x);
       Array(::enumType1 x);
   };
