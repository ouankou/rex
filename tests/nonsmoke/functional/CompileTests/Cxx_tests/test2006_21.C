

class SgNode;
class AstNodePtr
   {
     SgNode* repr;
     public:
          typedef SgNode PtrBaseClass;
          AstNodePtr( SgNode* n = 0)  {}
          AstNodePtr(  AstNodePtr& that) {}
          operator SgNode* () const { return repr; }
   };

class AstInterface
   {
     public:
          static AstNodePtr CreateFunctionCall( AstNodePtr& f);
          AstNodePtr CreateFunctionCall( );
   };

AstNodePtr  CodeGen( AstInterface &_fa)
   {
     AstNodePtr f;
     if (f != 0)
        {
          return _fa.CreateFunctionCall(f);
        }

  // return AstNodePtr(0);
   }

