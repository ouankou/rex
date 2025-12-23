// ROSE is a tool for building preprocessors, this file is an example preprocessor built with ROSE.
// rose.C: Example (default) ROSE Preprocessor: used for testing ROSE infrastructure
#include "rose.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iomanip>

#include "AstTests.h"

#include <algorithm>

#include "sageCommonSourceHeader.h"

using namespace Rose;

extern an_il_header il_header;

int
main ( int argc, char* argv[] )
   {
  // Main Function for default example ROSE Preprocessor
  // This is an example of a preprocessor that can be built with ROSE
  // This example can be used to test the ROSE infrastructure

#if 1
     list<string> l = CommandlineProcessing::generateArgListFromArgcArgv (argc,argv);
     printf ("Preprocessor (before): argv = \n%s \n",StringUtility::listToString(l).c_str());

     printf ("argc = %d \n",argc);
     l = CommandlineProcessing::generateArgListFromArgcArgv (argc,argv);
     printf ("l.size() = %d \n",l.size());
     printf ("Preprocessor (after): argv = \n%s \n",StringUtility::listToString(l).c_str());

  // printf ("Exiting in main! \n");
  // ROSE_ASSERT(1 == 2);
#endif

#if 0
     if ( CommandlineProcessing::isOption(argc,argv,"-rose:","(h|help)",true) ||
          CommandlineProcessing::isOption(argc,argv,"-", "(h|help)",true) ||
          CommandlineProcessing::isOption(argc,argv,"--","(h|help)",true) )
        {
          printf ("\nROSE (pre-release alpha version: %s) \n",VERSION);
          Rose::usage();
          exit(0);
        }

     l = CommandlineProcessing::generateArgListFromArgcArgv (argc,argv);
     printf ("Preprocessor (after): argv = \n%s \n",StringUtility::listToString(l).c_str());

     printf ("Exiting after initial command line processing \n");
     ROSE_ABORT();
#endif

#if 0
     string stringParameter;
     if ( CommandlineProcessing::isOptionWithParameter(argc,argv,"-rose:","(o|output)",stringParameter,true) )
        {
          printf ("-rose:output %s \n",stringParameter.c_str());
       // Make our own copy of the filename string
          int length = stringParameter.length();
          char* p_unparse_output_filename = (char*) new char[length+1];
          ROSE_ASSERT (p_unparse_output_filename != NULL);
          stringParameter.copy(p_unparse_output_filename,length,0);
          p_unparse_output_filename[length] = '\0';
          printf ("p_unparse_output_filename = %s \n",p_unparse_output_filename);
        }
       else
        {
          printf ("-rose:output not set! \n");
        }
#endif



     SgProject* project = frontend(argc,argv);
     ROSE_ASSERT (project != NULL);

  // DQ (2/6/2004): These tests fail in Coco for test2004_14.C
  // AstTests::runAllTests(const_cast<SgProject*>(project));

  // printf ("Generate the pdf output of the SAGE III AST \n");
  // generatePDF ( project );

     printf ("Generate the DOT output of the SAGE III AST \n");
     generateDOT ( *project );

     return backend(project);

  // alternative form
  // return backend(frontend(argc,argv));
   }










