class foo
{
//members...
public:
    foo(){}
    foo(const foo& copy)=default; // copy constructor

    foo(const foo &&move); // move constructor

    ~foo() {} // destructor
};

