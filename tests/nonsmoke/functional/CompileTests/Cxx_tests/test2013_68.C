

extern int (*g1)(char a, double b, void * c);

int (*g1)(char a, double b, void * c) = 0;


int main() {

	int y = g1(2,1.0, 0); // apply args

	return y;
}
