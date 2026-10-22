// Support for building include graphs (graphs of include files)

#include "sage3basic.h"

#include "FileHelper.h"

#include "includeGraph.h"

using namespace std;
using namespace Rose;

std::ofstream IncludeFileGraph::file;

std::set<void *> IncludeFileGraph::graphNodeSet;

void IncludeFileGraph::insertGraphNode(void *node) {

  ROSE_ASSERT(node != NULL);

  graphNodeSet.insert(node);
}

void IncludeFileGraph::graph_include_files(SgSourceFile *sourceFile) {
  ROSE_ASSERT(sourceFile != NULL);

  if (graphNodeSet.find(sourceFile) == graphNodeSet.end()) {
    string filename = FileHelper::getFileName(sourceFile->getFileName());
    string name = string("file : ") + filename;

    file << "\"" << StringUtility::numberToString(sourceFile) << "\"["
         << "label=\"" << name << "\\n"
         << StringUtility::numberToString(sourceFile) << "\" color=\"green\" ];"
         << endl;

    insertGraphNode(sourceFile);
  } else {
    return;
  }

  SgIncludeFile *includeFile = sourceFile->get_associated_include_file();
  if (includeFile != NULL) {
    file << "\"" << StringUtility::numberToString(sourceFile) << "\" -> \""
         << StringUtility::numberToString(includeFile)
         << "\"[label=\"assoc-include"
         << "\" color=\"red\" labelfontcolor=\"blue4\" weight=1 len=2];"
         << endl;

    graph_include_files(includeFile);
  }
}

void IncludeFileGraph::graph_include_files(SgIncludeFile *includeFile) {
  ROSE_ASSERT(includeFile != NULL);

  if (graphNodeSet.find(includeFile) == graphNodeSet.end()) {
    // Use the full filename to make multiple included files more clear.
    string filename = includeFile->get_filename();
    string name = string("include-directive : ") + filename;

    name = name + "\\n name_as_written = " +
           includeFile->get_name_used_in_include_directive().str() + " ";
    name = name + "\\n directory_prefix = " +
           includeFile->get_directory_prefix().str() + " ";

    SgSourceFile *source_file = includeFile->get_source_file();
    if (source_file != NULL && source_file->get_isHeaderFile() == false) {
      name = name + "\\n applicationRootDirectory = " +
             includeFile->get_applicationRootDirectory().str() + "      ";
    }

    name = name + "\\n isSystemInclude = " +
           (includeFile->get_isSystemInclude() ? "true" : "false") + "      ";

    string fill_color = ",fillcolor=\"lightblue\" ";

    if (includeFile->get_isApplicationFile() == true) {
      fill_color = ",fillcolor=\"lightgreen\" ";
    }

    fill_color += ",style=filled";

    file << "\"" << StringUtility::numberToString(includeFile) << "\"["
         << "label=\"" << name << "\\n"
         << StringUtility::numberToString(includeFile) << "\" color=\"purple\""
         << fill_color << " ];" << endl;

    insertGraphNode(includeFile);
  } else {
    return;
  }

  SgSourceFile *sourceFile = includeFile->get_source_file();
  if (sourceFile != NULL) {
    file << "\"" << StringUtility::numberToString(includeFile) << "\" -> \""
         << StringUtility::numberToString(sourceFile) << "\"[label=\"file   "
         << "\" color=\"blue\" labelfontcolor=\"blue4\" weight=1 len=2];"
         << endl;

    // We should have already seen this file.
    graph_include_files(sourceFile);
  }

  SgIncludeFile *parent_include_file = includeFile->get_parent_include_file();
  if (parent_include_file != NULL) {
    file << "\"" << StringUtility::numberToString(includeFile) << "\" -> \""
         << StringUtility::numberToString(parent_include_file)
         << "\"[label=\"parent   "
         << "\" color=\"blue\" labelfontcolor=\"blue4\" weight=1 len=2];"
         << endl;

    // We should have already seen this file.
    graph_include_files(parent_include_file);
  }

  for (size_t i = 0; i < includeFile->get_include_file_list().size(); i++) {
    SgIncludeFile *nested_includeFile = includeFile->get_include_file_list()[i];
    ROSE_ASSERT(nested_includeFile != NULL);

    file << "\"" << StringUtility::numberToString(includeFile) << "\" -> \""
         << StringUtility::numberToString(nested_includeFile)
         << "\"[label=\"include"
         << "\" color=\"blue\" labelfontcolor=\"blue4\" weight=1 len=2];"
         << endl;

    graph_include_files(nested_includeFile);
  }
}

void IncludeFileGraph::graph_include_files(SgProject *project) {
  if (graphNodeSet.find(project) == graphNodeSet.end()) {
    string name = string("project : (generate name)");

    file << "\"" << StringUtility::numberToString(project) << "\"["
         << "label=\"" << name << "\\n"
         << StringUtility::numberToString(project) << "\" color=\"blue\" ];"
         << endl;

    insertGraphNode(project);
  } else {
    return;
  }

  SgFileList *fileList = project->get_fileList_ptr();

  for (size_t i = 0; i < fileList->get_listOfFiles().size(); ++i) {
    SgFile *tmp_file = fileList->get_listOfFiles()[i];
    ROSE_ASSERT(tmp_file != NULL);
    SgSourceFile *sourceFile = isSgSourceFile(tmp_file);
    ROSE_ASSERT(sourceFile != NULL);

    file << "\"" << StringUtility::numberToString(project) << "\" -> \""
         << StringUtility::numberToString(sourceFile) << "\"[label=\""
         << "source_file : " + StringUtility::numberToString(i)
         << "\" color=\"blue\" labelfontcolor=\"blue4\" weight=1 len=2];"
         << endl;

    graph_include_files(sourceFile);
  }
}

void generateGraphOfIncludeFiles(SgProject *project, std::string filename) {
  ROSE_ASSERT(project != NULL);

  if (filename.empty() == true) {
    filename = "include_file_graph";
  }

  string dot_header = filename;
  filename += ".dot";

  IncludeFileGraph::file.open(filename.c_str());
  if (!IncludeFileGraph::file) {
    fprintf(stderr, "REX_GRAPH_ERROR[include-file-open]: %s\n",
            filename.c_str());
    ROSE_ABORT();
  }

  IncludeFileGraph::file << "digraph \"" << dot_header << "\" {" << endl;

  IncludeFileGraph::graph_include_files(project);

  IncludeFileGraph::file << endl;
  IncludeFileGraph::file << "} " << endl;
  IncludeFileGraph::file.close();
  if (!IncludeFileGraph::file) {
    fprintf(stderr, "REX_GRAPH_ERROR[include-file-write]: %s\n",
            filename.c_str());
    ROSE_ABORT();
  }

  IncludeFileGraph::graphNodeSet.clear();
}

void generateGraphOfIncludeFiles(SgSourceFile *sourceFile,
                                 std::string filename) {
  ROSE_ASSERT(sourceFile != NULL);

  if (filename.empty() == true) {
    filename = "include_file_graph";
  }

  string dot_header = filename;
  filename += ".dot";

  IncludeFileGraph::file.open(filename.c_str());
  if (!IncludeFileGraph::file) {
    fprintf(stderr, "REX_GRAPH_ERROR[include-file-open]: %s\n",
            filename.c_str());
    ROSE_ABORT();
  }

  IncludeFileGraph::file << "digraph \"" << dot_header << "\" {" << endl;

  IncludeFileGraph::graph_include_files(sourceFile);

  IncludeFileGraph::file << endl;
  IncludeFileGraph::file << "} " << endl;
  IncludeFileGraph::file.close();
  if (!IncludeFileGraph::file) {
    fprintf(stderr, "REX_GRAPH_ERROR[include-file-write]: %s\n",
            filename.c_str());
    ROSE_ABORT();
  }

  IncludeFileGraph::graphNodeSet.clear();
}
