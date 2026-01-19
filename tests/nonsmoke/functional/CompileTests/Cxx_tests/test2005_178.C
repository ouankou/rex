/*
The loop body is already a SgBasicBlock
The loop body is already a SgBasicBlock
At end of prelink loop prelinkIterationCounter = 0
listOfTemplateDeclarationsToOutput.size() = 0
Inside of backend(SgProject*): SgProject::get_verbose() = 0
Inside of backend(SgProject*): project->numberOfFiles() = 1
sourceFilenames.size() = 1
rose_iteratorFail.C:5: warning: all member functions in class `Base' are
   private
rose_iteratorFail.C: In function `void
   visitWithAstNodePointersList(std::vector<Base*, std::allocator<Base*> >)':
rose_iteratorFail.C:25: error: parse error before `;' token

which results in a rose_iteratorBug.C which has one ';' too much, and that
is within the if-statement:
include <vector>
using namespace std;
*/

// This is the simpler case of the bug:
void foo()
   {
     if (int x = 7) 
        {
        }
   }
