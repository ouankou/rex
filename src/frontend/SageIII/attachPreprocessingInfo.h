#ifndef _ATTACH_PREPROCESSING_INFO_H_
#define _ATTACH_PREPROCESSING_INFO_H_

#include "attachPreprocessingInfoTraversal.h"

ROSEAttributesList *getPreprocessorDirectives(std::string fileName,
                                              std::string new_filename = "");

void attachPreprocessingInfo(SgSourceFile *sageFile,
                             const std::string &new_filename = "",
                             bool attach_to_ast = true);

#endif
