#ifndef FINITEDIFFERENCING_H
#define FINITEDIFFERENCING_H

#include "rosedll.h"

class SgNode;

//! Do a simple form of finite differencing on all functions contained within
//! root (which should be a project, file, or function definition).
void ROSE_DLL_API simpleIndexFiniteDifferencing(SgNode *root);

#endif // FINITEDIFFERENCING_H
