// This test code demonstates the use of forward declarations
// it is designed so that an error in how the source position
// will cause a lagitimate erro in the compilation.

// before first declaration
class Y;
// after first declaration

// before defining declaration
class Y {};
// after defining declaration
