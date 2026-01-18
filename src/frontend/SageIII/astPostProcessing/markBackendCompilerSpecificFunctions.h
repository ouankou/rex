#ifndef MARK_BACKEND_COMPILER_SPECIFIC_FUNCTIONS_AS_COMPILER_GENERATED_H
#define MARK_BACKEND_COMPILER_SPECIFIC_FUNCTIONS_AS_COMPILER_GENERATED_H

// DQ (3/5/2006):
// This file declares the ROSE support that marks backend (vendor compiler)
// specific declarations as compiler generated. These declarations are injected
// via the ROSE preinclude file `rose_required_macros_and_functions.h` and are
// specific to the selected backend compiler.

// DQ (3/5/2006):
/*! \brief Mark an backend specific functions as compiler generated.

    This function marks backend (vendor compiler) specific declarations as
   compiler generated. These declarations appear in the ROSE preinclude file
    `rose_required_macros_and_functions.h` and are specific to the selected
   backend compiler.
 */
void markBackendSpecificFunctionsAsCompilerGenerated(SgNode *node);

/*! \brief Supporting traversal to mark an backend specific functions as
   compiler generated.

    This class is a traversal specif to the lower level support of the
    markBackendSpecificFunctionsAsCompilerGenerated(SgNode*) function.
 */
// DQ (5/8/2006): Implement this using the memory pool traversal so that we will
// visit every IR node class MarkBackendSpecificFunctionsAsCompilerGenerated :
// public SgSimpleProcessing
class MarkBackendSpecificFunctionsAsCompilerGenerated
    : public ROSE_VisitTraversal {
public:
  std::string targetFileName;
  Sg_File_Info *targetFile;

  virtual ~MarkBackendSpecificFunctionsAsCompilerGenerated();
  MarkBackendSpecificFunctionsAsCompilerGenerated();

  //! Required traversal function
  void visit(SgNode *node);
};

// endif for MARK_BACKEND_COMPILER_SPECIFIC_FUNCTIONS_AS_COMPILER_GENERATED_H
#endif
