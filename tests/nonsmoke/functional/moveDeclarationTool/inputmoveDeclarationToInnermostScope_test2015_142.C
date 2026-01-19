
#include "inputmoveDeclarationToInnermostScope_test2015_142.h"

int foobar()
{
   double abc;

   do {

      global_abc = abc;

   } while (true);

   return 42;
}
