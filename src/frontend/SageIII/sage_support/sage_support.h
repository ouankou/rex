#ifndef ROSE_SAGESUPPORT_H
#define ROSE_SAGESUPPORT_H

/**
 * \file    sage_support.h
 * \author  Justin Too <too1@llnl.gov>
 * \date    April 4, 2012
 */

/*-----------------------------------------------------------------------------
 *  Dependencies
 *
 *  TODO:
 *  1. Move #includes into .C file if they are not needed here.
 *---------------------------------------------------------------------------*/
// sage3basic.h should not be included by librose header files, only by .C files
// per policy. [Robb P. Matzke 2015-01-07]

#include "astPostProcessing.h"

#include "rose_paths.h"

#include "omp_lowering.h"

#include "attachPreprocessingInfo.h"

#include "sageBuilder.h"

#include "IncludeDirective.h"

#include "IncludingPreprocessingInfosCollector.h"

#include <libgen.h>

#include <sys/wait.h>

// Needed for std::filesystem::exists(...)
#include <filesystem>

#include <stdio.h>

// Liao 10/8/2010, refactored OpenMP related code to ompAstConstruction.C
#include "ompAstConstruction.h"

namespace Rose {
namespace Frontend {
int Run(SgProject *project);
int RunSerial(SgProject *project);
} // namespace Frontend
} // namespace Rose

#endif // ROSE_SAGESUPPORT_H
