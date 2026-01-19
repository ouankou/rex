namespace M_ {
	struct X {
		int i;
		X() : i(0) { }
	};
        } // namespace M_

namespace N_ {
	struct X {
		int i;
		X() : i(0) { }
	};
}
int N_::X::*pdn = &N_::X::i;

int M_::X::*pdm = &M_::X::i;
namespace N_ {
	int f_(int N_::X::*) { return 12; }
}
namespace M_ {
	int g_(int M_::X::*) { return 1013; }
        } // namespace M_

void foobar()
   {
     int n = f_(pdn);
  // ieq(n, 12);
     int m = g_(pdm);
  // ieq(m, 1013);
   }

