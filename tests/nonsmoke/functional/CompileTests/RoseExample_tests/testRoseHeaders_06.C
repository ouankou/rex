/* rosePublicConfig.h has some of the same CPP symbol definitions as
 * rose_config.h, except the names have been changed so as to not pollute the
 * global name space.  All the names start with "ROSE_". */
#include "rosedefs.h"

#include "rosePublicConfig.h"

#include "sage3basic.hhh"
// DQ (4/21/2009): This header file contains the definitions of the IR nodes.
// tps : avoid detection" header files are scanned whether they include
// sage3basic.h for .h and .hh

// #include "sage3.h"

// DQ (3/22/2009): This is already included in sage3.h"
// #include "roseInternal.h"

// DQ (10/27/2003): Needed access to global function defined in unparser.h
// I think it makes sense to include all of the unparser into the interface
// of ROSE (don't know why this was left out previously).
#include "unparser.h"

// DQ:7/29/2002, MS:12/11/2002
// Place this at the end (since it is dependent upon ROSE classes.)
// added here to avoid placing it in each header file using the AstProcessingLib
#include "AstDOTGeneration.h"

#include "AstDiagnostics.h"

#include "AstPDFGeneration.h"

#include "AstProcessing.h"

#include "AstReverseProcessing.h"

#include <typeinfo>
// #include "AstStatistics.h"

#include "RoseAst.h"

#include "wholeAST_API.h"

// DQ (10/18/2003) Why is this commented out (what is it)
// #include "AgProcessing.h"

// Not sure that we want this here since it uses the rewrite system
// which has not defined yet (circular reference in the header files)
// #include "AstRestructure.h"

#include <rewrite.h>
// this is a temporary fix (will become obsolete)
#include "AstClearVisitFlags.h"

// DQ (5/26/2007): This is not depricated
// DQ (8/1/2005): Included Milind's AstMerge mechanism as standard part of ROSE.
// #include "AstMerge.h"

// DQ (2/22/2006): Added Andreas' work to graph the AST.
#include "astGraph.h"

// DQ (6/23/2006): Added Andreas's work to support custom DOT graphs using
// persistant attributes.
#include "AstAttributeDOT.h"

// DQ (3/11/2006): Jeremiah Willcock's inliner
#include "inliner.h"

// DQ (3/18/2006): Jeremiah Willcock's partial redundancy elimination (PRE)
#include "pre.h"
// DQ (4/8/2006): Constant folding of the AST (cleans out redundant
// constant expresion trees save in translation from the frontend).
// Required to be run before PRE!
#include "constantFolding.h"

// DQ (5/8/2007): Added Robert Preissl's support for hidden type and declartion
// lists.
#include "HiddenList.h"

#include "HiddenList_Intersection.h"

#include "HiddenList_Output.h"

// DQ (4/20/2009): Added support to optionally get more information out about
// new delete operators.
#define COMPILE_DEBUG_STATEMENTS 1

/******************************************************************************************************************************
 *                            THIS CHECK SHOULD BE THE LAST THING IN THIS FILE!
 ******************************************************************************************************************************
 *
 * Make sure that configure-time macros are not defined in user code. This test
 *is here because CPP symbols defining the presence or absence of certain
 *features detected at configure time (CMake's ConfigureChecks.cmake) pollute
 *the global name space. This makes it impossible for a user to include both
 *ROSE's configuration results in conjunction with the configuration results of
 *any other package.
 *
 * If a ROSE public header file truly needs to know a configuration result, then
 *modify scripts/publicConfiguration.pl to include the name of the symbol you
 *need (e.g., HAVE_PTHREAD_H). Then the configuration step will create a file
 *named "rosePublicConfig.h" with properly scoped CPP symbols (e.g.,
 * "ROSE_HAVE_PTHREAD_H).
 *
 * For legacy code that includes "rose_config.h" followed by "rose.h", simply
 *undefine CONFIG_ROSE between those two includes. It is safe to do this because
 *CONFIG_ROSE is not used for any other purpose.
 *
 * This test is here rather than in src/testRoseLib.C so that developers will
 *get this error sooner rather than having to wait until all of ROSE is
 *compiled.  However, it does mean that developers will need to be a bit more
 *careful about including both the private (rose_config.h) and public
 *(rosePublicConfig.h) files in tandem.
 ******************************************************************************************************************************/
#ifdef CONFIG_ROSE
#error                                                                         \
    "rose_config.h included in public header by mistake. Use rosePublicConfig.h instead."
#endif

// DQ (1/31/2013): #endif for this attempt to make this test code smaller.
#endif
