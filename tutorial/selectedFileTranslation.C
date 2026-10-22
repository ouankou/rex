#include "rose.h"

using namespace std;

int
main ( int argc, char * argv[] )
   {
     vector<string> argvList(argv, argv + argc);

     SgProject* project = NULL;

     bool removeCommandOption = true;
     string pathToSkip;

  // The specified path string will be put into "skipPathString" and then the option will be removed
     if ( CommandlineProcessing::isOptionWithParameter(argvList,"-skipFile:","(path)",pathToSkip,removeCommandOption) )
        {
          printf ("Skip file containing path = %s \n",pathToSkip.c_str());

       // Builds SgProject with valid but empty SgFile objects, but with all command line processing
       // done. The frontend can then be invoked conditionally for each SgFile object.
          project = frontendShell(argvList);

       // Loop over all files in the SgProject (and call the frontend on a selected subset)
          SgFilePtrList::iterator fileIterator = project->get_fileList().begin();
          while (fileIterator != project->get_fileList().end())
             {
               string filename = (*fileIterator)->get_sourceFileNameWithPath();

            // Skip processing of files from a specific directory
            // If there is a match between the path and any part of the filename then skip processing this file
               if (filename.find(pathToSkip) != string::npos)
                  {
                    printf ("Skipping call to the frontend for this file (%s) \n",filename.c_str());
                  }
                 else
                  {
                    int frontendErrorCode = (*fileIterator)->callFrontEnd();
                    ROSE_ASSERT(frontendErrorCode == 0);
                  }

            // increment the file list iterator
               fileIterator++;
             }
        }
       else
        {
       // This call the frontend automatically for all source files on the command line
          project = frontend(argvList);
        }

  // Call the backend, where files were not processed the original file will be compiled
  // directly by the backend (vendor) compiler. The SgFile objects here would also be 
  // looped over and unparsed and or compiled separately.
     return backend(project);
}
