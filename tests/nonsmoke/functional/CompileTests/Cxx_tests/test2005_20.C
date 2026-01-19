/*
Hi Dan,

As you asked for on Tuesday, here is a small program (main.cpp) calling the
frontend twice to make two copies of the same sgproject.

Everything works fine when the input file ends in .c or .C (test with
testfile.c).

If the input file ends in .cpp (test with testfile.cpp) the the following error
message is displayed before aborting:

myexe: /home/hauge2/ROSE/src/frontend/legacy
frontend/legacy_frontend/src/cmd_line.c:2811: void proc_command_line(int,
char**): Assertion `fileNameCounter == 1' failed. Abort

Executing the code with the input file testfile.cpp demonstrates this error
message.

The error message gives the file and the approximate line number where an
if-test should be changed as to allow .cpp files.

main.cpp and the two testfiles are attached.

Vera

*/
