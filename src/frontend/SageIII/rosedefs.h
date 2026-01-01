#ifndef __rosedefs
#define __rosedefs


#include "stdio.h"
#include <cassert>
#include <cstdio>
#include <list>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <sstream>

// DQ (10/21/2004): See comments in sage3basic.h; this must be consistent
// anywhere Cxx_Grammar.h is included.
#ifndef ALT_FIXUP_COPY
#define ALT_FIXUP_COPY 1
#endif

// DQ (9/24/2004): See comments in sage3basic.h; this must be consistent
// anywhere Cxx_Grammar.h is included.
#ifndef REMOVE_SET_PARENT_FUNCTION
#define REMOVE_SET_PARENT_FUNCTION
#endif

// Cxx_Grammar.h uses these macros in declarations (see sage3basic.h for the
// full policy).
#ifndef ROSE_DEPRECATED_FUNCTION
#define ROSE_DEPRECATED_FUNCTION /*deprecated*/
#endif

#ifndef ROSE_DEPRECATED_VARIABLE
#define ROSE_DEPRECATED_VARIABLE /*deprecated*/
#endif

// DQ (9/25/2007): Need to move this to here so that all of ROSE will see it.
#define Rose_STL_Container std::vector

// DQ (2/5/2010): include stdint.h always.
#include "stdint.h"
typedef uint64_t rose_addr_t; /* address and size (file and memory) */

// DQ (2/10/2010): Added assert.h (not clear where else it is included).
#include "assert.h"

#include "roseInternal.h"


//#include "rose_attributes_list.h"

// DQ (10/14/2010): We don't want to include this into our header file system
// since then users will see the defined macros in our autoconf generated 
// config.h (which we generate as rose_config.h to avoid filename conflicts).
// This fixes the problem that causes macro names to conflict (e.g. PACKAGE_BUGREPORT).
// #include "rose_config.h"

#ifndef ROSE_USE_INTERNAL_FRONTEND_DEVELOPMENT
#include "virtualCFG.h"

// DQ (10/29/2010): This must be included as a header file since the function
// declarations in SgAsmStatement require it in the generated Cxx_Grammar.h
// file.
#include "staticCFG.h"
#else

// DQ (11/12/2011): We need a declaration that can be used in Cxx_Grammar.h
class VirtualCFG
   {
     public:
          typedef int CFGNode;
          typedef int CFGEdge;
   };

class VirtualBinCFG
   {
     public:
          typedef int AuxiliaryInformation;
          typedef int CFGNode;
          typedef int CFGEdge;
   };

#endif

#endif
