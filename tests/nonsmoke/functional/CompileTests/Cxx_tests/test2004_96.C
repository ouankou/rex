
// Test of a function pointer which returns a function pointer!
// typedef void *(*functionPointer)(void *);

// long form of declaration of pointer to function
// void *(*functionPointer_A)(void *);

// Since foo is not previously defined the "struct" is required!
// typedef struct foo *(*functionPointer_B)(void *);

// Put the secondary type declaration into the return type
// typedef struct foobar *(*functionPointer_C)(void *);
typedef struct foobar *(*foobarFunctionPointer)();

#if __cplusplus
foobar* foobarPointer = 0L;
#else
struct foobar* foobarPointer = 0L;
#endif

struct foobar {};
