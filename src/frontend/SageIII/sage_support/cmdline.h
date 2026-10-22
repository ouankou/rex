#ifndef ROSE_SAGESUPPORT_CMDLINE_H
#define ROSE_SAGESUPPORT_CMDLINE_H

/**
 * \file    cmdline.h
 * \author  Justin Too <too1@llnl.gov>
 * \date    April 4, 2012
 */

/*-----------------------------------------------------------------------------
 *  Dependencies
 *---------------------------------------------------------------------------*/
#include "sage_support.h"

namespace Rose {
namespace Cmdline {
/** Constants to be used with CommandlineProcessing::isOptionWithParameter
 *  to specify the removeOption argument.
 */
enum {
  KEEP_OPTION_IN_ARGV = 0, ///< Don't remove the CLI option from the input argv
  REMOVE_OPTION_FROM_ARGV = 1 ///< Remove the CLI option from the input argv
};

extern ROSE_DLL_API int verbose;

//! Convert `-I <path>` to `-I<path>`
std::vector<std::string>
NormalizeIncludePathOptions(std::vector<std::string> &argv);

namespace Unparser {
static const std::string option_prefix = "-rose:unparser:";

void StripRoseOptions(std::vector<std::string> &argv);

/** Process all Unparser-specific commandline options, i.e. -rose:unparser.
 */
void Process(SgProject *project, std::vector<std::string> &argv);

void ProcessClobberInputFile(SgProject *project,
                             std::vector<std::string> &argv);
} // namespace Unparser

namespace Fortran {
/** Process all Fortran commandline options.
 */
void Process(SgProject *project, std::vector<std::string> &argv);

// -rose:fortran
void ProcessFortranOnly(SgProject *project, std::vector<std::string> &argv);
} // namespace Fortran

namespace Gnu {
void Process(SgProject *project, std::vector<std::string> &argv);

// --param
void ProcessParam(SgProject *project, std::vector<std::string> &argv);
} // namespace Gnu
} // namespace Cmdline
} // namespace Rose
#endif // ROSE_SAGESUPPORT_CMDLINE_H
