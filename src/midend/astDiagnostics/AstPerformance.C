// One include file to include them all!
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"
// #include "HiddenList.h"

#include <cstdlib>

#include <fstream>

// file locking support
#include <errno.h>

#include <fcntl.h>

#include <stdio.h>

#include <sys/resource.h>

#include <sys/time.h>

#include <unistd.h>

#include "AstStatistics.h"

// DQ (12/8/2006): Linux memory usage mechanism (no longer used, implemented
// internally (below)). #include<memoryUsage.h>

// DQ (7/16/2025): Added counters for a suspected performance issue in ROSE.
// Specifically when processing large projects we spend a significant amount of
// time in FixupAstSymbolTablesToSupportAliasedSymbols().
size_t AstPerformance::
    numberOfCallsToInjectSymbolsFromReferencedScopeIntoCurrentScope = 0;
size_t AstPerformance::numberOfSymbolsCopiedIntoAliasSymbols = 0;
size_t AstPerformance::numberOfUsingDirectivesProcessingAliasSymbols = 0;
size_t AstPerformance::numberOfUsingBaseClassesProcessingAliasSymbols = 0;
size_t AstPerformance::isSubset_numberOfCalls = 0;
size_t AstPerformance::isSubset_numberOf_a_vector_size = 0;
size_t AstPerformance::isSubset_numberOf_b_set_size = 0;
size_t AstPerformance::isSubset_a_vector_size_max = 0;
size_t AstPerformance::isSubset_b_set_size_max = 0;

// DQ (7/19/2025): Adding more debugging...
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_range_size_max =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfBaseClass = 0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_symbolExistsInBaseScope =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_symbolExistsInBaseScope_SgVariableSymbol =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_calledFromUsingDirective =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_alreadyExistsAndIsInterestingCase =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_range_count =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_false_addingNewSgAliasSymbol =
        0;
size_t AstPerformance::
    injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_addingCausalNode =
        0;

// DQ (8/14/2025): Adding support to count the number of statements traversed in
// the name qualification when using traverseInputFile(). It should be only the
// statements in the source file, but it appears to include statements marked as
// compilerGenerated.
size_t AstPerformance::
    numberOfStatementsProcessedInNameQualificationUsingTraverseInputFile = 0;
size_t AstPerformance::numberOfCallsToHashOperator = 0;

using namespace std;

// DQ (8/29/2007): Part of initial support for more portable timers (suggested
// by Matt Sottile at LANL) JJW (5/21/2008): Changed back to clock() for
// portability

bool ROSE_MemoryUsage::getStatmInfo() {
  FILE *file = fopen("/proc/self/statm", "r");
  ;
  if (!file) {
    // printf("Unable to access system file /proc/self/statm to get memory usage
    // data\n");
    return false;
  }
  int numEntries = fscanf(file, "%d %d %d %d %d %d %d", &memory_pages,
                          &resident_pages, &shared_pages, &code_pages,
                          &stack_pages, &library_pages, &dirty_pages);
  if (numEntries != 7)
    return false;
  fclose(file);
  return true;
}

int ROSE_MemoryUsage::getAvailableMemoryPages() const { return memory_pages; }

int ROSE_MemoryUsage::getNumberOfResidentPages() const {
  return resident_pages;
}

int ROSE_MemoryUsage::getNumberOfSharedPages() const { return shared_pages; }

int ROSE_MemoryUsage::getNumberOfCodePages() const { return code_pages; }

int ROSE_MemoryUsage::getNumberOfStackPages() const { return stack_pages; }

int ROSE_MemoryUsage::getNumberOfLibraryPages() const { return library_pages; }

int ROSE_MemoryUsage::getNumberOfDirtyPages() const { return dirty_pages; }

int ROSE_MemoryUsage::getNumberOfCodePlusLibraryPages() const {
  return library_pages + code_pages;
}

long ROSE_MemoryUsage::getMemoryUsageKilobytes() const {
  return ((long)getNumberOfResidentPages() * (long)getPageSizeBytes()) /
         (long)1024;
}

int ROSE_MemoryUsage::getPageSizeBytes() const { return getpagesize(); }

double ROSE_MemoryUsage::getPageSizeMegabytes() const {
  return getPageSizeBytes() / (1024.0 * 1024.0);
}

double ROSE_MemoryUsage::getAvailableMemoryMegabytes() const {
  return getAvailableMemoryPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfResidentMegabytes() const {
  return getNumberOfResidentPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfSharedMegabytes() const {
  return getNumberOfSharedPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfCodeMegabytes() const {
  return getNumberOfCodePages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfStackMegabytes() const {
  return getNumberOfStackPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfLibraryMegabytes() const {
  return getNumberOfLibraryPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfDirtyMegabytes() const {
  return getNumberOfDirtyPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getMemoryUsageMegabytes() const {
  return getNumberOfResidentPages() * getPageSizeMegabytes();
}

double ROSE_MemoryUsage::getNumberOfCodePlusLibraryMegabytes() const {
  return getNumberOfCodePlusLibraryPages() * getPageSizeMegabytes();
}

// static data defined in the AstPerformance class
std::vector<ProcessingPhase *> AstPerformance::data;

// Support fo hierarchy of performance monitors
std::list<AstPerformance *> AstPerformance::performanceStack;

// static SgProject IR node require for report generation to a file
SgProject *AstPerformance::project = NULL;

// DQ (10/28/2020): Adding control over output of performance report (static
// data member).
bool AstPerformance::outputCompilationPerformance = false;

// MS (11/9/2020) : tracing support
std::ofstream *TracingPerformance::trace_stream = NULL;
double TracingPerformance::trace_zero_time = -1;
bool TracingPerformance::first_event = true;
bool TracingPerformance::trace_durations = true;

AstPerformance::AstPerformance(std::string s, bool outputReport)
    : label(s), outputReportInDestructor(outputReport) {
  static bool cleanup_registered = false;
  if (!cleanup_registered) {
    std::atexit(&AstPerformance::cleanup);
    cleanup_registered = true;
  }

  ProcessingPhase *parentData = NULL;
  // check the stack for an existing performance monitor (it will be come the
  // parent) TOO1 (4/11/2013): TODO: -rose:keep_going tends to segfault here, so
  // for now we
  //                         will simply skip AstPerformance processing.
  if (project != NULL && project->get_keep_going() == true) {
    project = NULL;
  }

  // DQ (10/28/2020): Reset this to be controlled via the command line (static
  // data member). outputReportInDestructor = outputCompilationPerformance;

  if (project != NULL && performanceStack.size() > 0) {
    std::list<AstPerformance *>::iterator i = performanceStack.begin();
    parentData = (*i)->localData;
    assert(parentData != NULL);
    localData = new ProcessingPhase(label, 0.0, parentData);
  } else {
    assert(parentData == NULL);
    localData = new ProcessingPhase(label, 0.0, parentData);

    // If this performance monitor does not have a parent then add it to the
    // static list
    data.push_back(localData);
  }

  assert(localData != NULL);

  // I would have hoped that the correct virtual function would have been
  // called, but this didn't work! double resolution = performanceResolution();
  // printf ("resolution = %f \n",resolution);
  // localData->set_resolution(resolution);

  // Put this performance monitor onto the stack
  performanceStack.push_front(this);
}

AstPerformance::~AstPerformance() {
  // printf ("Inside of AstPerformance destructor ... project = %p
  // outputReportInDestructor = %s \n",project,outputReportInDestructor ? "true"
  // : "false");

  // DQ (7/21/2010): Call this here before we get too far into the derived class
  // constructor. localData->set_memory_usage((double)
  // (localData->memoryUsage.getMemoryUsageMegabytes()));

  // Remove this performance monitor from the stack
  performanceStack.pop_front();

  // DQ (9/6/2006): This will reset the time; to a nearly zero value!
  // DQ (9/1/2006): Need to stop the timer and record the elapsed time.
  // localData->stopTiming(timer);

  // DQ (10/28/2020): Reset this to be controlled via the command line (static
  // data member). outputReportInDestructor = outputCompilationPerformance;

  // if (project != NULL)
  if (project != NULL && outputCompilationPerformance == true) {
    generateReportToFile(project);
  } else {
  }

  // printf ("Leaving AstPerformance destructor ... \n");
}

void AstPerformance::cleanup() {
  for (ProcessingPhase *phase : data) {
    delete phase;
  }
  data.clear();
}

ProcessingPhase::ProcessingPhase()
    : name("default name"), performance(-1.0), resolution(-1.0), memoryUsage(),
      internalMemoryUsageData(0) {}

// Forward declaration
// extern int RAMUST::getMem(int);

ProcessingPhase::ProcessingPhase(const std::string &s, double p,
                                 ProcessingPhase *parent)
    : name(s), performance(p), resolution(-1.0), memoryUsage(),
      internalMemoryUsageData(0) {
  // DQ (7/21/2010): Set in the destructor instead of the constructor.
  // internalMemoryUsageData = ROSE_MemoryUsage::getMemoryUsageKilobytes();
  // internalMemoryUsageData = memoryUsage.getMemoryUsageMegabytes();

  if (parent != NULL)
    parent->childList.push_back(this);
}

ProcessingPhase::~ProcessingPhase() {
  for (ProcessingPhase *child : childList) {
    delete child;
  }
  childList.clear();

  // DQ (7/21/2010): This is too late of a stage to set this value!
  // internalMemoryUsageData = memoryUsage.getMemoryUsageMegabytes();
}

// static
double time_stamp() {
  // DQ (2/20/2013): This is the suggested best portable way to compute elapsed
  // wall clock time (from Liao).
  struct timeval t;
  double time;

  gettimeofday(&t, NULL);

  time = (double)(t.tv_sec + (1.0e-6 * t.tv_usec));
  // printf ("In AstPerformance: time_stamp(): time = %f \n",time);

  return time;
}

double ProcessingPhase::getCurrentDelta(const RoseTimeType &timer) {
  // DQ (2/20/2013): Change method for getting time, since clock() only is
  // reporting CPU time and this did not correctly compute the time required in
  // the backend compilation step of the system() call. return double(clock() -
  // timer) / CLOCKS_PER_SEC; return (time_stamp() - timer);
  double value = (time_stamp() - timer);
  if (value < 0.0) {
    printf(
        "Warning: ProcessingPhase::getCurrentDelta(): returning value = %f \n",
        value);
  }
  return value;
}

int AstPerformance::getLock() {
  int fd;

  // printf ("Build the lock file \n");
  // generate a lock
  if (SgProject::get_verbose() >= 1)
    printf("Acquiring a lock: rose_performance_report_lockfile.lock \n");

  // DQ (8/24/2008): Setup counters to detect when file locks are in place (this
  // was a problem this morning)
  unsigned long counter = 0;
  const unsigned long userTolerance = 10;

  // DQ (1/24/2017): Robb suggests adding 0666 as a required 3rd function
  // argument (fails on ubuntu machines). while ( (fd =
  // open("rose_performance_report_lockfile.lock", O_WRONLY | O_CREAT | O_EXCL))
  // == -1 )
  while ((fd = open("rose_performance_report_lockfile.lock",
                    O_WRONLY | O_CREAT | O_EXCL, 0666)) == -1) {
    // Skip the message output if this is the first try!
    if (counter > 0)
      printf("Waiting for lock! counter = %lu userTolerance = %lu \n", counter,
             userTolerance);

    sleep(1);
    counter++;

    // DQ (8/24/2008): If after waiting a short while and the lock is still
    // there, then report the issue.
    if (counter > userTolerance) {
      printf("Waiting for file lock (run \"make clean\" to remove lock files, "
             "if problem persists)... \n");

      // Reset the counter to prevent it from over flowing on nightly tests,
      // though that might take a long time :-).
      counter = 1;
    }
  }

  if (fd == -1) {
    perror("error in opening lock file: rose_performance_report_lockfile.lock");
    // exit(1);
  }

  return fd;
}

void AstPerformance::releaseLock(int fd) {
  close(fd);

  if (SgProject::get_verbose() >= 1)
    printf("Removing the lock file \n");

  remove("rose_performance_report_lockfile.lock");
}

void ProcessingPhase::outputReport(int n) {
  // This function does the formatting for the performance data for each
  // performance topic catagory (and child catagories).

  // Indent child data
  for (int i = 0; i < n; i++)
    printf(" ");

  // DQ (6/30/2013): Modified formatting of performance data to be more clear
  // (and generally prettier). printf ("%s time = %4.3f (sec) memory usage %5.3f
  // (megabytes) \n",name.c_str(),performance,internalMemoryUsageData);
  printf("%s ", name.c_str());

  // Make formatting less senative to the lengths of performance catagory names.
  // int whitespaceSize = 150 - name.length();
  int whitespaceSize = 150 - (name.length() + n);
  if (whitespaceSize < 0)
    whitespaceSize = 5;
  for (int i = 0; i < whitespaceSize; i++)
    printf("-");

  // Output the rest of the string with timing and memory usage info.
  printf(" time = %8.3f (sec) memory usage %9.3f (megabytes) \n", performance,
         internalMemoryUsageData);

  // printf ("Children: childList = %" PRIuPTR " \n",childList.size());
  std::vector<ProcessingPhase *>::iterator i = childList.begin();
  while (i != childList.end()) {
    (*i)->outputReport(n + 5);
    i++;
  }
}

void AstPerformance::generateReport() {
  AstPerformance("", false).generateReportFromObject();
}

void AstPerformance::generateReportFromObject() const {
  // output any performance data saved by different phases during the
  // compilation

  // DQ (6/9/2010): Change the return type to size_t to support larger number of
  // IR nodes using values that overflow signed values of int. Note this is only
  // an error for ROSE compiling ROSE. Declaration of global functions
  // (generated by ROSETTA)
  extern size_t numberOfNodes();

  // DQ (10/28/2020): Control output of reporting using static bool data member
  // outputCompilationPerformance.
  if (outputCompilationPerformance == false) {
    return;
  }

  // DQ (10/21/2020): Adding IR node usage statistics reporting.
  AstNodeStatistics::IRnodeUsageStatistics();

  printf("\n\n");
  std::vector<ProcessingPhase *>::iterator i = data.begin();
  if (i != data.end()) {
    size_t numberOf_IR_Nodes = numberOfNodes();
    size_t numberOfKiloBytesUsed = memoryUsage() / (1 << 10);

    // DQ (12/8/2006): Using new Linux memory usage.
    // DQ (9/6/2006): Computed using getrusage();
    // long int memoryComputedFromSystem = 0;
    // unsigned long memoryComputedFromSystem = (*i)->get_memory_usage();
    double memoryComputedFromSystem = (*i)->get_memory_usage();

    ROSE_MemoryUsage currentUsage;
    if (currentUsage.informationValid()) {
      printf("General System Data: \n");
      printf("     timer resolution (sec)   = %f \n", (*i)->get_resolution());
      printf("     page size (bytes)        = %7d (megabytes) = %8.3f \n",
             currentUsage.getPageSizeBytes(),
             currentUsage.getPageSizeMegabytes());
      printf("     available memory (pages) = %7d (megabytes) = %8.3f \n",
             currentUsage.getAvailableMemoryPages(),
             currentUsage.getAvailableMemoryMegabytes());
      printf("     resident memory (pages)  = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfResidentPages(),
             currentUsage.getNumberOfResidentMegabytes());
      printf("     shared pages             = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfSharedPages(),
             currentUsage.getNumberOfSharedMegabytes());
      printf("     code size (pages)        = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfCodePages(),
             currentUsage.getNumberOfCodeMegabytes());
      printf("     stack size (pages)       = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfStackPages(),
             currentUsage.getNumberOfStackMegabytes());
      printf("     library size (pages)     = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfLibraryPages(),
             currentUsage.getNumberOfLibraryMegabytes());
      printf("     dirty pages              = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfDirtyPages(),
             currentUsage.getNumberOfDirtyMegabytes());
      printf("     executable code pages    = %7d (megabytes) = %8.3f \n",
             currentUsage.getNumberOfCodePlusLibraryPages(),
             currentUsage.getNumberOfCodePlusLibraryMegabytes());

      // printf ("Performance Report (resolution = %f, number of IR nodes = %d,
      // memory used (calculated for AST) = %d Kilobytes, memory used (actual) =
      // %ld Kilobytes ): \n",
      //      (*i)->get_resolution(),numberOf_IR_Nodes,numberOfKiloBytesUsed,memoryComputedFromSystem);
      printf("Performance Report (timer resolution = %f, number of IR nodes = "
             "%" PRIuPTR
             ", memory used (calculated from AST memory pool) = %" PRIuPTR
             " Kilobytes, memory used (actual) = %8.3f Megabytes ): \n",
             (*i)->get_resolution(), numberOf_IR_Nodes, numberOfKiloBytesUsed,
             memoryComputedFromSystem);
    } else {
      printf("Memory usage information from system is not available.\n");
    }
  }

  while (i != data.end()) {
    (*i)->outputReport(5);
    i++;
  }

  // DQ (7/16/2025): Added output of performance counters associated with what
  // might be a performance problem in ROSE. DQ (7/16/2025): Added counters for
  // a suspected performance issue in ROSE. Specifically when processing large
  // projects we spend a significant amount of time in
  // FixupAstSymbolTablesToSupportAliasedSymbols().
  printf("AstPerformance::"
         "numberOfCallsToInjectSymbolsFromReferencedScopeIntoCurrentScope = "
         "%zu \n",
         numberOfCallsToInjectSymbolsFromReferencedScopeIntoCurrentScope);
  printf("AstPerformance::numberOfSymbolsCopiedIntoAliasSymbols                "
         "           = %zu \n",
         numberOfSymbolsCopiedIntoAliasSymbols);
  printf("AstPerformance::numberOfUsingDirectivesProcessingAliasSymbols        "
         "           = %zu \n",
         numberOfUsingDirectivesProcessingAliasSymbols);
  printf("AstPerformance::numberOfUsingBaseClassesProcessingAliasSymbols       "
         "           = %zu \n",
         numberOfUsingBaseClassesProcessingAliasSymbols);

  printf("AstPerformance::isSubset_numberOfCalls                               "
         "           = %zu \n",
         isSubset_numberOfCalls);
  printf("AstPerformance::isSubset_numberOf_a_vector_size                      "
         "           = %zu \n",
         isSubset_numberOf_a_vector_size);
  printf("AstPerformance::isSubset_numberOf_b_set_size                         "
         "           = %zu \n",
         isSubset_numberOf_b_set_size);
  printf("AstPerformance::isSubset_a_vector_size_max                           "
         "           = %zu \n",
         isSubset_a_vector_size_max);
  printf("AstPerformance::isSubset_b_set_size_max                              "
         "           = %zu \n",
         isSubset_b_set_size_max);

  // DQ (7/19/2025): Adding performance debugging support.
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "alreadyExists_true_range_size_max                      = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_range_size_max);
  printf("AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
         "numberOfBaseClass                                      = %zu \n",
         injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfBaseClass);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "numberOfTimes_symbolExistsInBaseScope                  = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_symbolExistsInBaseScope);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "numberOfTimes_symbolExistsInBaseScope_SgVariableSymbol = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_symbolExistsInBaseScope_SgVariableSymbol);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "numberOfTimes_calledFromUsingDirective                 = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_calledFromUsingDirective);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "numberOfTimes_alreadyExistsAndIsInterestingCase        = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_alreadyExistsAndIsInterestingCase);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "alreadyExists_true_range_count                         = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_range_count);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "alreadyExists_false_addingNewSgAliasSymbol             = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_false_addingNewSgAliasSymbol);
  printf(
      "AstPerformance::injectSymbolsFromReferencedScopeIntoCurrentScope_"
      "alreadyExists_true_addingCausalNode                    = %zu \n",
      injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_addingCausalNode);

  // DQ (8/14/2025): Adding support to count the number of statements traversed
  // in the name qualification when using traverseInputFile(). It should be only
  // the statements in the source file, but it appears to include statements
  // marked as compilerGenerated.
  printf("AstPerformance::"
         "numberOfStatementsProcessedInNameQualificationUsingTraverseInputFile "
         "                                   = %zu \n",
         numberOfStatementsProcessedInNameQualificationUsingTraverseInputFile);
  printf("AstPerformance::numberOfCallsToHashOperator                          "
         "                                                   = %zu \n",
         numberOfCallsToHashOperator);

  printf("\n\n");
}

void ProcessingPhase::outputReportToFile(std::ofstream &datafile) {
  // For CSV formatted files we have to escape all ',' characters.
  // So we have to identify any fields that contain ',' and handle them as
  // special cases.
  string csv_field = name;

  // DQ (8/30/2006): Just quote those csv fields that contain a ',' character.
  string commaSeparator = ",";
  if (csv_field.find(commaSeparator) != string::npos) {
    csv_field = "\"" + csv_field + "\"";
  }

  datafile << ", " << csv_field << ", " << performance;

  // printf ("name = %s performance = %f \n",csv_field.c_str(),performance);

  // printf ("Children: childList = %" PRIuPTR " \n",childList.size());
  std::vector<ProcessingPhase *>::iterator i = childList.begin();
  while (i != childList.end()) {
    (*i)->outputReportToFile(datafile);
    i++;
  }
}

void AstPerformance::set_project(SgProject *projectParameter) {
  project = projectParameter;
  ROSE_ASSERT(project != NULL);
}

void AstPerformance::generateReportToFile(SgProject *project) const {
  // output CVS file with performance information data saved by different phases
  // during the compilation

  // DQ (6/9/2010): Change the return type to size_t to support larger number of
  // IR nodes using values that overflow signed values of int. Note this is only
  // an error for ROSE compiling ROSE. Declaration of global functions
  // (generated by ROSETTA)
  extern size_t numberOfNodes();
  extern size_t memoryUsage();

  set_project(project);

  ROSE_ASSERT(project != NULL);
  string output_filename = project->get_compilationPerformanceFile();

  // Ignore generation of a report if no source file was specified!
  if (project->numberOfFiles() == 0 || output_filename.empty() == true) {
    if (SgProject::get_verbose() >= 2) {
      // printf ("No source file specified for compilation, performance report
      // output skipped  project->numberOfFiles() = %d
      // \n",project->numberOfFiles());
      if (project->numberOfFiles() == 0) {
        printf("Note: No source file specified for output of performance data "
               "(CVS file). \n");
      } else {
        printf("Note: No Compilation Performance File specified for output of "
               "performance data (use -rose:compilationPerformanceFile "
               "<filename>) \n");
      }
    }
    return;
  }

  string source_file = project->get_file(0).get_sourceFileNameWithPath();

  ofstream datafile(output_filename.c_str(), ios::out | ios::app);

  if (datafile.good() == false) {
    printf("File failed to open \n");
    ROSE_ABORT();
  }

  // datafile << "This is a test!" << std::endl;

  // printf ("Get the lock ... \n");

  // generate a lock
  int fd = getLock();
  ROSE_ASSERT(fd > 0);
  // printf ("Got the lock ... \n");

  // Put the data for each ProcessingPhase out to a CSV formatted file
  // output the data
  datafile << "filename," << source_file << ", number of AST nodes, "
           << numberOfNodes() << ", memory, " << memoryUsage() << " ";

  // printf ("Output the data to the file ... \n");
  std::vector<ProcessingPhase *>::iterator i = data.begin();
  while (i != data.end()) {
    (*i)->outputReportToFile(datafile);
    i++;
  }

  datafile << endl;

  // printf ("Done: Output the data to the file ... (calling flush) \n");

  datafile.flush();

  // printf ("Done with file flush() ... \n");

  // release the lock
  // printf ("Releasing the file lock \n");
  releaseLock(fd);

  // printf ("File output complete \n");
  datafile.close();
}

double AstPerformance::performanceResolution() {
  // printf ("Inside of AstPerformance::performanceResolution() \n");
  return -1.0; // default value
}

TimingPerformance::TimingPerformance(std::string s, bool outputReport)
    // Save the label explaining what the performance number means
    : AstPerformance(s, outputReport), TracingPerformance() {
  // DQ (2/20/2013): We want to uniformally used the new mechanism to compute
  // the elapsed time.
  timer = time_stamp();

  if (checkTracing()) {
    emitTraceBoundaryEvent(label, timer, true);
  }
}

TraceOnlyPerformance::TraceOnlyPerformance(std::string s, bool /*outputReport*/)
    : TracingPerformance() {
  label = s;
  timer = time_stamp();
  if (checkTracing()) {
    emitTraceBoundaryEvent(label, timer, true);
  }
}

void TraceOnlyPerformance::endTimer() {
  if (checkTracing()) {
    double duration = ProcessingPhase::getCurrentDelta(timer);
    emitTraceDurationEvent(label, timer, duration);
    emitTraceBoundaryEvent(label, timer + duration, false);
  }
}

TraceOnlyPerformance::~TraceOnlyPerformance() { endTimer(); }

// MS (11/9/2020): emit tracing events in JSON format
bool TracingPerformance::checkTracing() {
  static bool trace_disabled = false;

  // if we've decided to disable tracing, immediately return false
  if (trace_disabled) {
    return false;
  }

  // if stream exists, then we know we're tracing
  if (trace_stream != NULL) {
    return true;
  }

  // stream is null, so see if we need to open it
  const char *trace_env = std::getenv("ROSE_TRACEFILE");
  if (trace_env != NULL) {
    // check if tracing boundaries
    if (std::getenv("ROSE_TRACEBOUNDARIES") != NULL) {
      trace_durations = false;
    }

    // use environment variable as base trace filename
    // TODO: this is not safe if multiple programs are tracing at once.
    //       need to acquire lock, determine unused filename, allocate
    //       it, and release lock.
    trace_stream = new std::ofstream(trace_env, std::ios::out);

    // valid stream = tracing
    if (trace_stream != NULL) {
      return true;
    }

    // something bad happened opening the stream.  warn and return false.
    std::cerr << "checkTracing(): unable to open file \"" << trace_env
              << "\".  Tracing disabled." << std::endl;

    // set static flag to avoid future checks
    trace_disabled = true;

    return false;
  }

  // no stream + no env = not tracing
  return false;
}

// MS (11/9/2020): emit tracing events in JSON format
void TracingPerformance::emitTraceDurationEvent(std::string label, double t,
                                                double dur) {
  // sanity check - should never enter here with a null stream
  ROSE_ASSERT(trace_stream != NULL);

  // if not tracing durations, return
  if (!trace_durations) {
    return;
  }

  // if we have not set the earliest time observed, set it to be the zero time.
  if (trace_zero_time < 0) {
    trace_zero_time = t;
  }

  // make all events relative to the first observed event
  double ts = t - trace_zero_time;

  // scale factor: assumes times are in microseconds
  static double scalefactor = 1.0e6;

  // check if first event.  if not, add ", \n" to separate from
  // previous.  this avoids the annoying trailing comma problem
  // for the last event.
  if (first_event) {
    *trace_stream << "{ \"traceEvents\": [" << std::endl;
    first_event = false;

    // stash a lambda that closes off the JSON list at exit.
    atexit([] { *trace_stream << std::endl << "] }" << std::endl; });
  } else {
    *trace_stream << ", " << std::endl;
  }

  *trace_stream << setprecision(18) << "{"
                << "\"name\": \"" << label << "\", "
                << "\"ph\": \"X\", "
                << "\"ts\": " << ts * scalefactor << ", "
                << "\"dur\": " << dur * scalefactor << ", "
                << "\"pid\": 1, "
                << "\"tid\": 1, "
                << "\"args\": {} }";
}

void TracingPerformance::emitTraceBoundaryEvent(std::string label, double t,
                                                bool isStart) {
  // sanity check - should never enter here with a null stream
  ROSE_ASSERT(trace_stream != NULL);

  // if tracing durations, don't trace boundary events
  if (trace_durations) {
    return;
  }

  // if we have not set the earliest time observed, set it to be the zero time.
  if (trace_zero_time < 0) {
    trace_zero_time = t;
  }

  // make all events relative to the first observed event
  double ts = t - trace_zero_time;

  // scale factor: assumes times are in microseconds
  static double scalefactor = 1.0e6;

  // check if first event.  if not, add ", \n" to separate from
  // previous.  this avoids the annoying trailing comma problem
  // for the last event.
  if (first_event) {
    *trace_stream << "{ \"traceEvents\": [" << std::endl;
    first_event = false;

    // stash a lambda that closes off the JSON list at exit.
    atexit([] { *trace_stream << std::endl << "] }" << std::endl; });
  } else {
    *trace_stream << ", " << std::endl;
  }

  std::string typeLabel = isStart ? "\"B\"" : "\"E\"";

  *trace_stream << setprecision(18) << "{"
                << "\"name\": \"" << label << "\", "
                << "\"ph\": " << typeLabel << ", "
                << "\"ts\": " << ts * scalefactor << ", "
                << "\"pid\": 1, "
                << "\"tid\": 1, "
                << "\"args\": {} }";
}

// DQ (6/30/2013): Refactored this function to be something that can be called
// from the destructor and also in the scope of the outer most scope timer
// before report generation (so we can compute total elapsed time).
void TimingPerformance::endTimer() {
  // DQ (9/1/2006): Refactor the code to stop the timing so that we can call it
  // in the destructor and the report generation (both trigger the stopping of
  // all timers).
  assert(localData != NULL);

  // MS (11/9/2020): tracing support
  if (checkTracing()) {
    double duration = ProcessingPhase::getCurrentDelta(timer);
    emitTraceDurationEvent(label, timer, duration);
    emitTraceBoundaryEvent(label, timer + duration, false);
  }

  double p = ProcessingPhase::getCurrentDelta(timer);
  if (p < 0.0) // Liao, 2/18/2009, avoid future bug
  {
#ifdef ROSE_UBUNTU_OS_VENDOR
    // DQ (4/24/2011): This failed only on Ubuntu (but only once) so not clear
    // if there is a real problem here or not. Making it a warning for now only
    // on Ubuntu systems while we try to address the robustness of Hudson
    // testing.
    printf(
        "WARNING: value returned from ProcessingPhase::getCurrentDelta(timer) "
        "is negative in ~TimingPerformance() (value = %6.10f) \n",
        p);
#else
    // DQ (4/24/2011): Make this an more normal assertion and output the value
    // that is a problem. cerr << "Error: AstPerformance.C
    // TimingPerformance::~TimingPerformance() set negative performance value!"
    // << endl;

    // DQ (6/24/2013): This fails also on the 4.2 compiler.
    // DQ (3/3/2013): Make this a warning for the gnu 4.4 compiler.
    printf(
        "WARNING: value returned from ProcessingPhase::getCurrentDelta(timer) "
        "is negative in ~TimingPerformance() (value = %6.10f) \n",
        p);

    // DQ (6/24/2013): This fails on rare ocassions, I don't think it is
    // important enough to cause us to fail tests (appears to be a OS issue).
    // ROSE_ABORT();
#endif
  }
  localData->set_performance(ProcessingPhase::getCurrentDelta(timer));
  localData->set_resolution(performanceResolution());

  // DQ (7/21/2010): Set this here to record the useage of memory in the
  // interval being evaluated. internalMemoryUsageData =
  // memoryUsage.getMemoryUsageMegabytes(); localData->internalMemoryUsageData =
  // get_memory_usage() - localData->internalMemoryUsageData;
  // localData->set_memory_usage( (double)(get_memory_usage()));
  // localData->internalMemoryUsageData = memoryUsage.getMemoryUsageMegabytes();
  ROSE_MemoryUsage memoryUsage;
  localData->set_memory_usage(memoryUsage.getMemoryUsageMegabytes());
}

TimingPerformance::~TimingPerformance() {
  // DQ (6/30/2013): Refactored this function to be something that can just call
  // the new endTimer() function.
  endTimer();
}

double TimingPerformance::performanceResolution() {
  // This may not be the correct resolution of the clock
  double resolution = 1.0 / (double)CLOCKS_PER_SEC;
  return resolution;
}

void AstPerformance::reportAccumulatedTime(const string &s,
                                           const double &accumulatedTime,
                                           const double &numberFunctionCalls) {
  printf("     Accumulated time for %s = %f number of calls = %ld \n",
         s.c_str(), accumulatedTime, (long)numberFunctionCalls);
}

void AstPerformance::startTimer(RoseTimeType &time) {
  // DQ (2/20/2013): We want to uniformally used the new mechanism to compute
  // the elapsed time.
  time = time_stamp();
}

void AstPerformance::accumulateTime(RoseTimeType &startTime,
                                    double &accumulatedTime,
                                    double &numberFunctionCalls) {
  accumulatedTime += ProcessingPhase::getCurrentDelta(startTime);
  numberFunctionCalls += 1.0;
}
