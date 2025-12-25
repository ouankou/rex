// Header file to support attaching preprocessingInfo objects
// to AST nodes

#ifndef _ATTACH_PREPROCESSING_INFO_H_
#define _ATTACH_PREPROCESSING_INFO_H_

#include "attachPreprocessingInfoTraversal.h"


// DQ (4/5/2006): Andreas has removed this code!

// void printOutComments ( SgLocatedNode* locatedNode );

// DQ (1/4/2021): Adding support for comments and CPP directives and tokens to use new_filename.
// This function is defined in preproc.C and used to collect the attributes
// extern ROSEAttributesList *getPreprocessorDirectives(const char *fileName);
// extern ROSEAttributesList *getPreprocessorDirectives(const char *fileName);
// ROSEAttributesList *getPreprocessorDirectives(const char *fileName);
// ROSEAttributesList *getPreprocessorDirectives( std::string fileName );
// ROSEAttributesList *getPreprocessorDirectives( std::string fileName, LexTokenStreamTypePointer & input_token_stream_pointer );
// ROSEAttributesList *getPreprocessorDirectives( std::string fileName );
ROSEAttributesList *getPreprocessorDirectives( std::string fileName, std::string new_filename = "");


// DQ (12/3/2020): We sometimes want to read a file twice, and gather the comments 
// and CPP directives twice, but the second time the file is read it is read so that 
// it can build a file with a different name. So we need to specify the name of the
// file that we want the comments and CPP directives to eventually be attached to 
// and not the one from which they were take.  This technique is used to support
// building a second file to be a dynamic library within the codeSegregation tool.
// void attachPreprocessingInfo(SgSourceFile *sageFile);
void attachPreprocessingInfo(SgSourceFile *sageFile, const std::string & new_filename = "");

#endif

// EOF
