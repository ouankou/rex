/*
Email from Andreas:

when compiling the attached code in ROSE I get the following error:
"/home/andreas/REPOSITORY-BUILD/gcc41/mozilla/js/src/test-copysign.c",
line 3: error:
         identifier "__builtin_copysign" is undefined
 __builtin_copysign(1.0, z);
 ^

Errors in legacy frontend Processing: (frontend_errorLevel > 3)
Aborted (core dumped)

*/

int main(){
    double z;
__builtin_copysign(1.0, z);
};



