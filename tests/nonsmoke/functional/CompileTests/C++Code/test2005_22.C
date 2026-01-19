/*
Hi Dan,

You told me to send you a simple program showing that using
MiddleLevelRewrite::insert() is not as easy to use as it could be.

In this program I'm trying to use MiddleLevelRewrite::insert to insert a simple
"int z;" before the return statement of the function. The execution of the code
does produce an output file, but this file doesn't seem to be correct. (The file
is listed below.)


In addition I get an error message before the execution aborts:

myexe: Cxx_Grammar.C:1723: static char**
SgNode::buildCommandLineToSubstituteTransformationFile(int, char**,
std::basic_string<char, std::char_traits<char>, std::allocator<char> >):
Assertion `fileNameCounter == 1' failed. Abort

(and it's not easy to figure out how to fix this...)


I'm not convinced that I am using the MiddleLevelRewrite function correctly.
Maybe I should be using a AstTopDownBottomUpProcessing instead of
SgSimpleProcessing?

(BTW, I'm moving back to Rich's office after lunch. After all I am now allowed
to use that computer...)


Vera


Listing of files:
----------------------

Input file:
-------------
int main(){

 int y;
 y++;

 return 0;
}

Output file:
---------------
*/
