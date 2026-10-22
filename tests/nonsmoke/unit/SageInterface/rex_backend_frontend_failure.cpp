#include "rose.h"

#include <memory>

int main() {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_backend_frontend_failure.cpp");
  std::unique_ptr<SgProject> project(file->get_project());
  // An analysis tool may retain a failed frontend project for diagnostics.
  // Every backend entry point must preserve its failure before unparsing or
  // trying to compile the nonexistent source file.
  project->set_frontendErrorCode(37);
  ROSE_ASSERT(backend(project.get()) == 37);
  ROSE_ASSERT(project->get_backendErrorCode() == 37);
  project->set_backendErrorCode(0);
  ROSE_ASSERT(backendCompilesUsingOriginalInputFile(project.get()) == 37);
  ROSE_ASSERT(project->get_backendErrorCode() == 37);
  project->set_backendErrorCode(0);
  ROSE_ASSERT(backendGeneratesSourceCodeButCompilesUsingOriginalInputFile(
                  project.get()) == 37);
  ROSE_ASSERT(project->get_backendErrorCode() == 37);
}
