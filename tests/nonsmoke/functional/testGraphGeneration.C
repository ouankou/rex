// Example ROSE Translator reads input program and tests AST and WholeAST graph generation.
#include "rose.h"

// Options to generate graphs with and without filering of
// IR nodes can be use to tailor the generated graph output.
// Example graph options are:
//      -rose:dotgraph:expressionFilter 1
//      -rose:dotgraph:fileInfoFilter 0 

int main( int argc, char * argv[] )
   {
     TimingPerformance timer ("main() execution time (sec) = ");

  // Generate the ROSE AST.
     SgProject* project = frontend(argc,argv);

  // AST consistency tests (optional for users, but this enforces more of our tests)
  // AstTests::runAllTests(project);

     AstDOTGeneration astdotgen;
  // SgProject & nonconstProject = (SgProject &) project;
     std::string filenamePostfix;

  // DQ (6/1/2019): Uncommented to debug multiple file support.
  // astdotgen.generateInputFiles(project,DOTGeneration<SgNode*>::TOPDOWNBOTTOMUP,filenamePostfix);

     SgFile* file = project->get_files()[0];
     ROSE_ASSERT(file != NULL);

  // DQ (6/1/2019): Commented out to debug multiple file support.
     astdotgen.generateWithinFile(
         file, DOTGeneration<SgNode *>::TOPDOWNBOTTOMUP, filenamePostfix);

     // Output an optional graph of the AST (the whole graph, of bounded
     // complexity, when active)
     const int MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH = 10000;
     generateAstGraph(project,MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH,"");

  // DQ (5/24/2021): Add the support to generate graph with header files.
     generateDOTforMultipleFile(*project, "with_header_files");

     // AST consistency tests (optional for users, but this enforces more of our
     // tests)
     AstTests::runAllTests(project);

     // regenerate the source code and call the vendor
     // compiler, only backend error code is reported.
#if BACKEND_FORTRAN_IS_LLVM_FLANG
     if (project != nullptr && project->get_Fortran_only()) {
       project->skipfinalCompileStep(true);
       project->set_compileOnly(true);
     }
#endif
     int exit_status = backend(project);

  // DQ (6/30/2013): Compute the elapsed time to this point.
     timer.endTimer();

  // Output any saved performance data (see ROSE/src/astDiagnostics/AstPerformance.h)
  // AstPerformance::generateReportToFile(project->get_file(0).get_sourceFileNameWithPath(),project->get_compilationPerformanceFile());
  // AstPerformance::generateReportToFile(project);
     timer.generateReportToFile(project);

  // DQ (12/12/2009): Allow output only when run in verbose mode to limit spew in testing.
     if (SgProject::get_verbose() > 0)
        {
          int memoryUsageSize = memoryUsage();
          printf ("Alternative output from memoryUsage() = %d \n",memoryUsageSize);

          printf ("Calling AstNodeStatistics::traversalStatistics(project) \n");
          std::cout << AstNodeStatistics::traversalStatistics(project);

          printf ("Calling AstNodeStatistics::IRnodeUsageStatistics \n");
          std::cout << AstNodeStatistics::IRnodeUsageStatistics();

       // DQ (2/17/2013): Added performance report (reports performance timings and memory usage for many different parts of ROSE).
          timer.generateReportFromObject();
        }

     return exit_status;
}
