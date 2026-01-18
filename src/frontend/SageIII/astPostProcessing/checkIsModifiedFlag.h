#ifndef CHECK_ISMODIFIED_FLAG_H
#define CHECK_ISMODIFIED_FLAG_H

// DQ (4/16/2015): This functions have clearer sematics and a better
// implementation.
ROSE_DLL_API void reportNodesMarkedAsModified(SgNode *node);
ROSE_DLL_API void unsetNodesMarkedAsModified(SgNode *node);

// DQ (4/16/2015): Note that the semantics of this function is that it also
// resets the isModified flags.
bool checkIsModifiedFlag(SgNode *node);

// endif for CHECK_ISMODIFIED_FLAG_H
#endif
