// rose.h -- header file for the ROSE Optimizing Preprocessor

#ifndef ROSE_H
#define ROSE_H

/* rosePublicConfig.h has some of the same CPP symbol definitions as
 * rose_config.h, except the names have been changed so as to not pollute the
 * global name space.  All the names start with "ROSE_". */
#include "rosePublicConfig.h"

#include "sage3basic.hhh"

#include "rosedefs.h"

#ifdef ROSE_DOCGEN
#include "docs/Attributes.h"
#endif

#include "unparser.h"

#include "AstDOTGeneration.h"

#include "AstDiagnostics.h"

#include "AstPDFGeneration.h"

#include "AstProcessing.h"

#include "AstReverseProcessing.h"

#include <typeinfo>

#include "wholeAST_API.h"

#include "astGraph.h"

#include "AstAttributeDOT.h"

#include "inliner.h"

#include "constantFolding.h"

#include "cfgToDot.h"

#include "abiStuff.h"

#define TEMPLATE_IMPLEMENTATIONS
#include "AstDOTGenerationImpl.C"

#include "utility_functionsImpl.C"
#undef TEMPLATE_IMPLEMENTATIONS

/******************************************************************************************************************************
 *                            THIS CHECK SHOULD BE THE LAST THING IN THIS FILE!
 ******************************************************************************************************************************
 *
 * Make sure that configuration macros are not defined in user code. This test
 *is here because CPP symbols defining the presence or absence of certain
 *features detected at configure time (CMake's ConfigureChecks.cmake) pollute
 *the global name space. This makes it impossible for a user to include both
 *ROSE's configuration results in conjunction with the configuration results of
 *any other package.
 *
 * Public configuration results belong in cmake/rosePublicConfig.h.in.cmake,
 * using ROSE_ prefixes to avoid polluting user configuration macros.
 *
 * For legacy code that includes "rose_config.h" followed by "rose.h", simply
 *undefine CONFIG_ROSE between those two includes. It is safe to do this because
 *CONFIG_ROSE is not used for any other purpose.
 *
 * This check reports the error when a public header is included rather than
 *waiting until all of ROSE is compiled. Developers need to be a bit more
 *careful about including both the private (rose_config.h) and public
 *(rosePublicConfig.h) files in tandem.
 ******************************************************************************************************************************/
#ifdef CONFIG_ROSE
#error                                                                         \
    "rose_config.h included in public header by mistake. Use rosePublicConfig.h instead."
#endif

#endif /* !ROSE_H !*/
