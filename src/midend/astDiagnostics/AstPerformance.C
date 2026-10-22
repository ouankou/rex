#include "sage3basic.h"
#include <Rose/StringUtility/Escape.h>

#include <cstdlib>

#include <fstream>

#include <errno.h>

#include <fcntl.h>

#include <stdio.h>

#include <sys/resource.h>

#include <sys/time.h>

#include <unistd.h>

#include "AstStatistics.h"

using namespace std;

bool ROSE_MemoryUsage::getStatmInfo() {
  FILE *file = fopen("/proc/self/statm", "r");
  if (!file) {
    return false;
  }
  int numEntries = fscanf(file, "%d %d %d %d %d %d %d", &memory_pages,
                          &resident_pages, &shared_pages, &code_pages,
                          &stack_pages, &library_pages, &dirty_pages);
  fclose(file);
  return numEntries == 7;
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

std::vector<ProcessingPhase *> AstPerformance::data;

std::list<AstPerformance *> AstPerformance::performanceStack;

SgProject *AstPerformance::project = NULL;

bool AstPerformance::outputCompilationPerformance = false;

std::ofstream *TracingPerformance::trace_stream = NULL;
double TracingPerformance::trace_zero_time = -1;
bool TracingPerformance::first_event = true;
bool TracingPerformance::trace_durations = true;

AstPerformance::AstPerformance(std::string s) : label(s) {
  static bool cleanup_registered = false;
  if (!cleanup_registered) {
    std::atexit(&AstPerformance::cleanup);
    cleanup_registered = true;
  }

  ProcessingPhase *parentData = NULL;

  if (project != NULL && performanceStack.size() > 0) {
    std::list<AstPerformance *>::iterator i = performanceStack.begin();
    parentData = (*i)->localData;
    ROSE_ASSERT(parentData != NULL);
    localData = new ProcessingPhase(label, 0.0, parentData);
  } else {
    localData = new ProcessingPhase(label, 0.0, parentData);

    data.push_back(localData);
  }

  performanceStack.push_front(this);
}

AstPerformance::~AstPerformance() {
  performanceStack.pop_front();

  if (project != NULL && outputCompilationPerformance == true) {
    generateReportToFile(project);
  }
}

void AstPerformance::cleanup() {
  for (ProcessingPhase *phase : data) {
    delete phase;
  }
  data.clear();
}

ProcessingPhase::ProcessingPhase(const std::string &s, double p,
                                 ProcessingPhase *parent)
    : name(s), performance(p), resolution(-1.0), memoryUsage(),
      internalMemoryUsageData(0) {
  if (parent != NULL)
    parent->childList.push_back(this);
}

ProcessingPhase::~ProcessingPhase() {
  for (ProcessingPhase *child : childList) {
    delete child;
  }
  childList.clear();
}

double time_stamp() {
  struct timeval t;
  double time;

  gettimeofday(&t, NULL);

  time = (double)(t.tv_sec + (1.0e-6 * t.tv_usec));

  return time;
}

double ProcessingPhase::getCurrentDelta(const RoseTimeType &timer) {
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

  if (SgProject::get_verbose() >= 1)
    printf("Acquiring a lock: rose_performance_report_lockfile.lock \n");

  unsigned long counter = 0;
  const unsigned long userTolerance = 10;

  while ((fd = open("rose_performance_report_lockfile.lock",
                    O_WRONLY | O_CREAT | O_EXCL, 0666)) == -1) {
    if (errno != EEXIST) {
      perror(
          "error in opening lock file: rose_performance_report_lockfile.lock");
      ROSE_ABORT();
    }
    if (counter > 0)
      printf("Waiting for lock! counter = %lu userTolerance = %lu \n", counter,
             userTolerance);

    sleep(1);
    counter++;

    if (counter > userTolerance) {
      printf("Still waiting for the performance report lock file.\n");

      counter = 1;
    }
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

  printf("%s ", name.c_str());

  // Make formatting less senative to the lengths of performance catagory names.
  int whitespaceSize = 150 - (name.length() + n);
  if (whitespaceSize < 0)
    whitespaceSize = 5;
  for (int i = 0; i < whitespaceSize; i++)
    printf("-");

  // Output the rest of the string with timing and memory usage info.
  printf(" time = %8.3f (sec) memory usage %9.3f (megabytes) \n", performance,
         internalMemoryUsageData);

  std::vector<ProcessingPhase *>::iterator i = childList.begin();
  while (i != childList.end()) {
    (*i)->outputReport(n + 5);
    i++;
  }
}

void AstPerformance::generateReport() {
  AstPerformance("").generateReportFromObject();
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

  // generate a lock
  int fd = getLock();
  ROSE_ASSERT(fd >= 0);

  // Put the data for each ProcessingPhase out to a CSV formatted file
  // output the data
  datafile << "filename," << source_file << ", number of AST nodes, "
           << numberOfNodes() << ", memory, " << memoryUsage() << " ";

  std::vector<ProcessingPhase *>::iterator i = data.begin();
  while (i != data.end()) {
    (*i)->outputReportToFile(datafile);
    i++;
  }

  datafile << endl;

  datafile.flush();

  // release the lock
  releaseLock(fd);

  datafile.close();
}

TimingPerformance::TimingPerformance(std::string s)
    // Save the label explaining what the performance number means
    : AstPerformance(s), TracingPerformance() {
  // DQ (2/20/2013): We want to uniformally used the new mechanism to compute
  // the elapsed time.
  timer = time_stamp();

  if (checkTracing()) {
    emitTraceBoundaryEvent(label, timer, true);
  }
}

TraceOnlyPerformance::TraceOnlyPerformance(std::string s)
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

bool TracingPerformance::checkTracing() {
  if (trace_stream != nullptr) {
    return true;
  }
  const char *trace_env = std::getenv("ROSE_TRACEFILE");
  if (trace_env == nullptr) {
    return false;
  }
  trace_durations = std::getenv("ROSE_TRACEBOUNDARIES") == nullptr;
  trace_stream = new std::ofstream(trace_env, std::ios::out);
  if (!*trace_stream) {
    std::cerr << "checkTracing(): unable to open file \"" << trace_env << "\"."
              << std::endl;
    ROSE_ABORT();
  }
  *trace_stream << "{ \"traceEvents\": [" << std::endl;
  ROSE_ASSERT(*trace_stream);
  ROSE_ASSERT(atexit([] {
                *trace_stream << std::endl << "] }" << std::endl;
                trace_stream->close();
                ROSE_ASSERT(*trace_stream);
                delete trace_stream;
                trace_stream = nullptr;
              }) == 0);
  return true;
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
    first_event = false;
  } else {
    *trace_stream << ", " << std::endl;
  }

  *trace_stream << setprecision(18) << "{"
                << "\"name\": \"" << Rose::StringUtility::jsonEscape(label)
                << "\", "
                << "\"ph\": \"X\", "
                << "\"ts\": " << ts * scalefactor << ", "
                << "\"dur\": " << dur * scalefactor << ", "
                << "\"pid\": 1, "
                << "\"tid\": 1, "
                << "\"args\": {} }";
  trace_stream->flush();
  ROSE_ASSERT(*trace_stream);
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
    first_event = false;
  } else {
    *trace_stream << ", " << std::endl;
  }

  std::string typeLabel = isStart ? "\"B\"" : "\"E\"";

  *trace_stream << setprecision(18) << "{"
                << "\"name\": \"" << Rose::StringUtility::jsonEscape(label)
                << "\", "
                << "\"ph\": " << typeLabel << ", "
                << "\"ts\": " << ts * scalefactor << ", "
                << "\"pid\": 1, "
                << "\"tid\": 1, "
                << "\"args\": {} }";
  trace_stream->flush();
  ROSE_ASSERT(*trace_stream);
}

// DQ (6/30/2013): Refactored this function to be something that can be called
// from the destructor and also in the scope of the outer most scope timer
// before report generation (so we can compute total elapsed time).
void TimingPerformance::endTimer() {
  ROSE_ASSERT(localData != NULL);

  const bool tracing = checkTracing();
  double p = ProcessingPhase::getCurrentDelta(timer);
  if (tracing) {
    emitTraceDurationEvent(label, timer, p);
    emitTraceBoundaryEvent(label, timer + p, false);
  }
  if (p < 0.0) // Liao, 2/18/2009, avoid future bug
  {
    printf(
        "WARNING: value returned from ProcessingPhase::getCurrentDelta(timer) "
        "is negative in ~TimingPerformance() (value = %6.10f) \n",
        p);
  }
  localData->set_performance(p);
  localData->set_resolution(performanceResolution());

  if (getenv("ROSE_TIMER_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_TIMER_TRACE %.6f %s\n", p, label.c_str());
    fflush(stderr);
  }

  ROSE_MemoryUsage memoryUsage;
  localData->set_memory_usage(memoryUsage.getMemoryUsageMegabytes());
}

TimingPerformance::~TimingPerformance() {
  // DQ (6/30/2013): Refactored this function to be something that can just call
  // the new endTimer() function.
  endTimer();
}

double TimingPerformance::performanceResolution() {
  return 1.0 / (double)CLOCKS_PER_SEC;
}
