// #if
// (!defined(SKIP_122Y24)&&!defined(SKIP12)&&!defined(ONLY))||defined(CASE_122Y24)
class X_
	{
public:
	int i;
	static int count;
     // X_(int ii) : i(ii) { ++count; }
     // X_(const X_ &x) { i = x.i; ++count; }
	};
int X_::count = 0;
X_ f_(X_ x);

// #include "final_defs.h"
int main(int argc, char *argv[]) {}
