/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "sage3basic.h"

#include "OpenACCIR.h"

#include <string>
#include <utility>

OpenACCDirective *parseOpenACC(std::string) {
  ROSE_ABORT();
  return nullptr;
}

bool checkOpenACCIR(OpenACCDirective *) { return false; }

SgStatement *
convertOpenACCDirective(std::pair<SgPragmaDeclaration *, OpenACCDirective *>) {
  ROSE_ABORT();
  return nullptr;
}
