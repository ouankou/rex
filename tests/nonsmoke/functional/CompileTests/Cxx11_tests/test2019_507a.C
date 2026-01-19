// #define B_FILE 1
// #include "defs.h"

/* _122Y24 expr a = f(a) requires temporary for the result of f(a) */
// #if
// (!defined(SKIP_122Y24)&&!defined(SKIP12)&&!defined(ONLY))||defined(CASE_122Y24)
/* B-file */
// Implements core 320 - 2006 [editorial]
class X_
	{
public:
	int i;
	static int count;
     // X_(int ii) : i(ii) { ++count; }
     // X_(const X_ &x) { i = x.i; ++count; }
};
