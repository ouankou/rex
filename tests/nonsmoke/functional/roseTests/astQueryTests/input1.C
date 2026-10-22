struct RexQueryStruct {
  int rex_struct_first, rex_struct_second;
};

union RexQueryUnion {
  int rex_union_first;
  double rex_union_second;
};

class Bar{
public:
  Bar(int x, char y){};
};

int a;

void foo()
   {
     float b;
     Bar x(1,'x');
   }

class hiddenClassObjectTag
   {
     long c;
   } hiddenClassObject1;

typedef class hiddenClassTypeTag
   {
     double d;
   } hiddenClassType;

hiddenClassType hiddenClassObject2;
