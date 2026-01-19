struct T3
{
    int mem1;
    T3() { } // user-provided default constructor
};

void foobar(T3 x); // user-provided default constructor

int main() {

  foobar({});

  // T4 x({});
}
