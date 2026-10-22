// Author: Dan Quinlan
#ifndef AST_PERFORMANCE_H
#define AST_PERFORMANCE_H

#include <list>

#include <string>

#include <vector>

#include "rosedll.h"

/*! \brief This is a mechanism for reporting the performance of processing of
   the AST, subtrees, and IR nodes.

    The use of these performance monitors is designed to be simple to use,
   support hierarchies of timers, and to generate simple reports.  Tests of the
   performance monitors are in the
   ROSE/tests/nonsmoke/functional/roseTests/astPerformanceTests directory.

    Possible uses:
    -#) timing performance,
    -#) memory performance,
    -#) ...

 */

typedef double RoseTimeType;

class ROSE_MemoryUsage {
  int memory_pages;
  int resident_pages;
  int shared_pages;
  int code_pages;
  int stack_pages;
  int library_pages;
  int dirty_pages;
  bool valid;

public:
  ROSE_MemoryUsage()
      : memory_pages(0), resident_pages(0), shared_pages(0), code_pages(0),
        stack_pages(0), library_pages(0), dirty_pages(0) {
    valid = getStatmInfo();
  }

  bool informationValid() const { return valid; }

private:
  //! Get memory usage information from /proc/statm, returning true (and filling
  //! in the parameter structure) if the information is available and false if
  //! it is not.
  bool getStatmInfo();

public:
  int getPageSizeBytes() const;
  int getAvailableMemoryPages() const;
  int getNumberOfResidentPages() const;
  int getNumberOfSharedPages() const;
  int getNumberOfCodePages() const;
  int getNumberOfStackPages() const;
  int getNumberOfLibraryPages() const;
  int getNumberOfDirtyPages() const;

  int getNumberOfCodePlusLibraryPages() const;

  double getPageSizeMegabytes() const;
  double getAvailableMemoryMegabytes() const;
  double getNumberOfResidentMegabytes() const;
  double getNumberOfSharedMegabytes() const;
  double getNumberOfCodeMegabytes() const;
  double getNumberOfStackMegabytes() const;
  double getNumberOfLibraryMegabytes() const;
  double getNumberOfDirtyMegabytes() const;

  double getNumberOfCodePlusLibraryMegabytes() const;
  double getMemoryUsageMegabytes() const;
};

class ProcessingPhase {
  // This class stores a string and a final number to record the performance.

protected:
  //! title of performance measurement (e.g. "total time", "memory use", etc.)
  std::string name;
  double performance;
  double resolution;
  //! Memory usage information -- the constructor of this member obtains
  //! the information if it can
  ROSE_MemoryUsage memoryUsage;

  double internalMemoryUsageData;
  // Permit a hierarchy of performance monitors
  std::vector<ProcessingPhase *> childList;

public:
  ProcessingPhase(const std::string &s, double p, ProcessingPhase *parent);

  virtual ~ProcessingPhase();

  void outputReport(int n);
  void outputReportToFile(std::ofstream &datafile);
  static double getCurrentDelta(const RoseTimeType &timer);

  void set_performance(const double &p) { performance = p; }
  double get_resolution() const { return resolution; }
  void set_resolution(const double &r) { resolution = r; }
  double get_memory_usage() const { return internalMemoryUsageData; }
  void set_memory_usage(const double &m) { internalMemoryUsageData = m; }
};

class SgProject;

class ROSE_DLL_API AstPerformance {
public:
  // DQ (9/1/2006): Note that we don't have the project yet when this is called
  // within "main()" to summarize the total time.
  AstPerformance(std::string s);
  virtual ~AstPerformance();

  // This is the evolving data (built locally so that parents in the hierarchy
  // can refer to it)
  ProcessingPhase *localData;

  static bool outputCompilationPerformance;

  std::string label;
  static SgProject *project;

  static void set_project(SgProject *projectParameter);

  void generateReportFromObject() const;
  void generateReportToFile(SgProject *project) const;
  static void generateReport();
  static void cleanup();

  // Support for file locking (so that the performance file is not
  // corrupted by parallel writes as used in parallel compiles with
  // the "-j#" make option).
  static int getLock();
  static void releaseLock(int fd);

protected:
  // Storage of all performance information about
  // processing phases saved here for later processing.
  static std::vector<ProcessingPhase *> data;

  // This allows any existing performance monitor to become
  // the parent of any child performance monitor
  static std::list<AstPerformance *> performanceStack;
};

// Base class for event tracing.  This class holds state related to
// the output stream and methods for emitting trace events.  It is
// separate from the TimingPerformance class so that we can have a
// TraceOnlyPerformance object that performs the same timing role, but
// does not record the timer in the performance stack.  This is useful
// for tracing frequently made calls without causing the report to get
// huge.
class ROSE_DLL_API TracingPerformance {
protected:
  static std::ofstream *trace_stream;
  static double trace_zero_time;
  static bool first_event;
  static bool trace_durations;

  // private tracing functions.  these are private since they are only
  // called from within the destructor
  void emitTraceDurationEvent(std::string label, double t, double dur);
  void emitTraceBoundaryEvent(std::string label, double t, bool isStart);
  bool checkTracing();

public:
  TracingPerformance() {}
};

// MS (11/27/2020) TraceOnlyPerformance has the same API as
// TimingPerformance, but does not derive from AstPerformance and thus
// does not interfere with the performance reporting facility.
class ROSE_DLL_API TraceOnlyPerformance : public TracingPerformance {
private:
  RoseTimeType timer;
  std::string label;

public:
  TraceOnlyPerformance(std::string s);
  virtual ~TraceOnlyPerformance();
  void endTimer();
  typedef RoseTimeType time_type;
};

class ROSE_DLL_API TimingPerformance : public AstPerformance,
                                       public TracingPerformance {
private:
  RoseTimeType timer;

  // Used for timing compilation within ROSE
public:
  TimingPerformance(std::string s);
  virtual ~TimingPerformance();

  // DQ (6/30/2013): Refactored this function to be something that can be called
  // from the destructor and also in the scope of the outer most scope timer
  // before report generation (so we can compute total elapsed time).
  void endTimer();

  static double performanceResolution();

  typedef RoseTimeType time_type; // For compatibility
};

#endif
