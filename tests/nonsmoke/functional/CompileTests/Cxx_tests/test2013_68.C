#if 0
// based on a rose legacy frontend pulled yesterday, I have a frontend version dependent problem.
// Am I misusing something in sage and just got lucky in the older rose?

// my code:
void rewriteFunctionCallExp(SgFunctionCallExp* fce)
   {
    if (fce) {
      SgName qname, decltypename, tagname;
      bool varargs = false;
      SgFunctionType* varptrfuntype = 0;
      // is there a better way to get qualified id from expr?
      SgFunctionDeclaration *dec = fce->getAssociatedFunctionDeclaration () ;
      if (dec) {
       // rest of it irrelevant
      }
   }

// is victim to an assert inside getAssociatedFunctionDeclaration
// which under legacy_frontend returns null as documented.

// rewriteFunctionCallExp <-- printed from my code

// printed from rose -->

// Error: There should be no other cases functionExp = 0x10a84bf0 = SgVarRefExp 
// mungeArglists: ../../../../legacy_frontend-rose/src/frontend/SageIII/sage_support/sage_support.C:5534: SgFunctionSymbol* SgFunctionCallExp::getAssociatedFunctionSymbol() const: Assertion `false' failed.

// Here's the inputs below.

// ::::::::::::::
// test13.c
// ::::::::::::::

// #include "test13.h"


// int (*g1)(char a, double b, void * c) = 0;


// int main() {

// 	int y = g1(2,1.0, 0); // apply args

// 	return y;
// }
// ::::::::::::::
// test13.h
// ::::::::::::::

// extern int (*g1)(char a, double b, void * c);

// ---
// rebuilding rose with -g enabled now...
// thanks,
// Ben

#endif


extern int (*g1)(char a, double b, void * c);

int (*g1)(char a, double b, void * c) = 0;


int main() {

	int y = g1(2,1.0, 0); // apply args

	return y;
}
