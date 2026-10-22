#include "sage3basic.h"

#include "newCallGraph.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

#include <cstring>

#include <errno.h>

#include <fcntl.h>

#include <stdio.h>

#include <sys/file.h>
#include <unistd.h>

using namespace std;
using namespace NewCallGraph;

CallGraphNodeInfo::CallGraphNodeInfo(SgFunctionDeclaration *x)
    : functionDeclaration(x) {
  ROSE_ASSERT(functionDeclaration != NULL);
}

CallGraphEdgeInfo::CallGraphEdgeInfo(CallGraphNodeInfo *from,
                                     CallGraphNodeInfo *to)
    : from(from), to(to) {
  ROSE_ASSERT(from != NULL);
  ROSE_ASSERT(to != NULL);
}

CallGraph::CallGraph() {}

CallGraph::~CallGraph() {
  for (const auto &edge : edgeMap) {
    delete edge.second;
  }
  edgeMap.clear();

  for (std::map<std::string, CallGraphNodeInfo *>::value_type &node : nodeMap) {
    delete node.second;
  }
  nodeMap.clear();
}

CallGraphFileStructure::CallGraphFileStructure()
    : isNode(false), isDefiningDeclaration(false) {
  fill(function_name, function_name + functionNameLength, 0);
  fill(function_call_name, function_call_name + functionNameLength, 0);
  fill(file_name, file_name + fileNameLength, 0);

  fill(directoryToUseInCompilingSourceFile,
       directoryToUseInCompilingSourceFile + directoryLength, 0);
  fill(commandlineToCompileSourceFile,
       commandlineToCompileSourceFile + commandlineLength, 0);
}

void CallGraph::visit(SgNode *astNode) {
  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(astNode);
  if (functionDeclaration != NULL) {
    string name = functionDeclaration->get_mangled_name().getString();

    const auto existing = nodeMap.find(name);
    if (existing == nodeMap.end()) {
      auto node = std::make_unique<CallGraphNodeInfo>(functionDeclaration);
      if (nodeMap.emplace(name, node.get()).second)
        node.release();
    } else if (functionDeclaration ==
               functionDeclaration->get_definingDeclaration()) {
      existing->second->functionDeclaration = functionDeclaration;
    }
  }

  SgFunctionRefExp *functionRefExpression = isSgFunctionRefExp(astNode);
  if (functionRefExpression != NULL) {
    SgFunctionSymbol *functionSymbol = functionRefExpression->get_symbol();
    ROSE_ASSERT(functionSymbol != NULL);

    // How do we handle a function call from global scope (e.g. constructor
    // initialization)?
    SgFunctionDeclaration *from_functionDeclaration =
        SageInterface::getEnclosingFunctionDeclaration(functionRefExpression);
    ROSE_ASSERT(from_functionDeclaration != NULL);

    SgFunctionDeclaration *to_functionDeclaration =
        functionSymbol->get_declaration();
    ROSE_ASSERT(to_functionDeclaration != NULL);

    string from_function_name =
        from_functionDeclaration->get_mangled_name().getString();
    string to_function_name =
        to_functionDeclaration->get_mangled_name().getString();

    const auto caller = nodeMap.find(from_function_name);
    ROSE_ASSERT(caller != nodeMap.end());
    const auto callee = nodeMap.find(to_function_name);
    CallGraphNodeInfo *to = nullptr;
    if (callee != nodeMap.end()) {
      to = callee->second;
    } else {
      auto node = std::make_unique<CallGraphNodeInfo>(to_functionDeclaration);
      to = node.get();
      ROSE_ASSERT(nodeMap.emplace(to_function_name, to).second);
      node.release();
    }
    auto edge = std::make_unique<CallGraphEdgeInfo>(caller->second, to);
    if (edgeMap
            .emplace(EdgeKey{from_function_name, to_function_name}, edge.get())
            .second)
      edge.release();
  }
}

int NewCallGraph::getLock() {
  const int fd = open("rose_callgraph_lockfile.lock",
                      O_WRONLY | O_CREAT | O_CLOEXEC, 0666);
  if (fd == -1) {
    perror("opening rose_callgraph_lockfile.lock");
    ROSE_ABORT();
  }
  int status;
  do {
    status = flock(fd, LOCK_EX);
  } while (status == -1 && errno == EINTR);
  ROSE_ASSERT(status == 0);
  return fd;
}

void NewCallGraph::releaseLock(int fd) {
  // Keep the inode stable so waiters and new writers lock the same file.
  const int closeStatus = close(fd);
  ROSE_ASSERT(closeStatus == 0);
}

vector<CallGraphFileStructure> *
NewCallGraph::readCallGraphFile(string binaryFilename) {
  if (!std::filesystem::exists(binaryFilename)) {
    return NULL;
  }
  ifstream binaryStream(binaryFilename.c_str(),
                        ifstream::ate | ifstream::binary);
  ROSE_ASSERT(binaryStream.good());
  const std::streamoff fileSize = binaryStream.tellg();
  ROSE_ASSERT(fileSize >= 0);
  size_t sizeOfEntry = sizeof(CallGraphFileStructure);
  size_t numberOfEntries = fileSize / sizeOfEntry;
  ROSE_ASSERT(numberOfEntries * sizeOfEntry == fileSize);
  vector<CallGraphFileStructure> *callGraphVector =
      new vector<CallGraphFileStructure>(numberOfEntries);
  binaryStream.seekg(0, ios::beg);
  if (fileSize != 0) {
    binaryStream.read(reinterpret_cast<char *>(callGraphVector->data()),
                      fileSize);
  }
  ROSE_ASSERT(binaryStream.good());
  // Inspect boolean representations without evaluating untrusted bool values.
  const auto validBoolean = [](const bool &value) {
    const bool falseValue = false;
    const bool trueValue = true;
    return std::memcmp(&value, &falseValue, sizeof(bool)) == 0 ||
           std::memcmp(&value, &trueValue, sizeof(bool)) == 0;
  };
  const auto terminated = [](const auto &value) {
    return std::memchr(value, '\0', sizeof(value)) != nullptr;
  };
  for (const CallGraphFileStructure &entry : *callGraphVector) {
    ROSE_ASSERT(validBoolean(entry.isNode));
    ROSE_ASSERT(validBoolean(entry.isDefiningDeclaration));
    ROSE_ASSERT(entry.isNode || !entry.isDefiningDeclaration);
    ROSE_ASSERT(terminated(entry.function_name));
    ROSE_ASSERT(terminated(entry.function_call_name));
    ROSE_ASSERT(terminated(entry.file_name));
    ROSE_ASSERT(terminated(entry.directoryToUseInCompilingSourceFile));
    ROSE_ASSERT(terminated(entry.commandlineToCompileSourceFile));
  }
  return callGraphVector;
}

void NewCallGraph::generateCallGraphFile(SgProject *project, CallGraph &cg) {
  ROSE_ASSERT(project != NULL);
  ROSE_ASSERT(project->numberOfFiles() > 0);

  const auto sourceFileName = [](SgFunctionDeclaration *declaration) {
    SgSourceFile *source = SageInterface::getEnclosingSourceFile(declaration);
    ASSERT_not_null(source);
    string filename = source->get_sourceFileNameWithPath();
    ROSE_ASSERT(filename.length() < fileNameLength);
    return filename;
  };
  for (const auto &node : cg.nodeMap) {
    ASSERT_not_null(node.second);
    sourceFileName(node.second->functionDeclaration);
  }

  struct FileLock {
    const int fd = NewCallGraph::getLock();
    ~FileLock() { NewCallGraph::releaseLock(fd); }
  } lock;
  ROSE_ASSERT(lock.fd >= 0);

  string binaryFilename = "binaryCallGraph";

  // Read the existing file (if it exists) of previously processed files and
  // their parts of the call graph.
  std::unique_ptr<vector<CallGraphFileStructure>> callGraphVector(
      readCallGraphFile(binaryFilename));

  ofstream binaryStream(binaryFilename.c_str(), ios::binary);

  if (binaryStream.good() == false) {
    printf("Binary file failed to open \n");
    ROSE_ABORT();
  }

  map<string, CallGraphNodeInfo *> &nodeMap = cg.nodeMap;
  auto &edgeMap = cg.edgeMap;
  std::set<std::string> storedNodes;
  std::set<CallGraph::EdgeKey> storedEdges;

  if (callGraphVector != NULL) {
    vector<CallGraphFileStructure>::iterator i = callGraphVector->begin();

    while (i != callGraphVector->end()) {
      if (i->isNode == true) {
        storedNodes.insert(i->function_name);
        const auto existing = nodeMap.find(i->function_name);
        if (existing != nodeMap.end()) {
          CallGraphNodeInfo *functionNode = existing->second;
          ROSE_ASSERT(functionNode != NULL);

          SgFunctionDeclaration *functionDeclaration =
              functionNode->functionDeclaration;
          ROSE_ASSERT(functionDeclaration != NULL);

          if (functionDeclaration ==
              functionDeclaration->get_definingDeclaration()) {
            const string source_file = sourceFileName(functionDeclaration);
            if (i->isDefiningDeclaration == false) {
              i->isDefiningDeclaration = true;
              std::memset(i->file_name, 0, sizeof(i->file_name));
              std::memcpy(i->file_name, source_file.c_str(),
                          source_file.size());
            } else {
              // Use the file name recorded from the first translation using to
              // see the function definition.
              printf(
                  "Note: This node (function_name = %s) was previously defined "
                  "in filename = %s but is also defined in source_file = %s \n",
                  i->function_name, i->file_name, source_file.c_str());
            }
          }
        }

        binaryStream.write(reinterpret_cast<const char *>(&*i),
                           sizeof(CallGraphFileStructure));
        SgName mangledName = i->function_name;

        ROSE_ASSERT(mangledName.get_length() < functionNameLength);

      } else {
        storedEdges.emplace(i->function_name, i->function_call_name);

        binaryStream.write(reinterpret_cast<const char *>(&*i),
                           sizeof(CallGraphFileStructure));
      }

      i++;
    }
  }

  map<string, CallGraphNodeInfo *>::iterator i_node = nodeMap.begin();
  while (i_node != nodeMap.end()) {
    SgFunctionDeclaration *functionDeclaration =
        i_node->second->functionDeclaration;
    SgName mangledName = functionDeclaration->get_mangled_name();

    ROSE_ASSERT(mangledName.get_length() < functionNameLength);

    CallGraphFileStructure function;

    if (storedNodes.count(i_node->first) == 0) {
      const string source_file = sourceFileName(functionDeclaration);
      function.isNode = true;
      if (functionDeclaration ==
          functionDeclaration->get_definingDeclaration()) {
        function.isDefiningDeclaration = true;
      } else {
        function.isDefiningDeclaration = false;
      }
      memcpy(function.function_name, mangledName.str(),
             mangledName.get_length());
      memcpy(function.file_name, source_file.c_str(), source_file.length());

      binaryStream.write(reinterpret_cast<const char *>(&function),
                         sizeof(CallGraphFileStructure));
    }

    i_node++;
  }

  auto i_edge = edgeMap.begin();
  while (i_edge != edgeMap.end()) {
    ROSE_ASSERT((*i_edge).second != NULL);
    ROSE_ASSERT((*i_edge).second->from != NULL);
    ROSE_ASSERT((*i_edge).second->from->functionDeclaration != NULL);
    ROSE_ASSERT((*i_edge).second->to != NULL);
    ROSE_ASSERT((*i_edge).second->to->functionDeclaration != NULL);

    SgName from_mangledName =
        (*i_edge).second->from->functionDeclaration->get_mangled_name();
    SgName to_mangledName =
        (*i_edge).second->to->functionDeclaration->get_mangled_name();
    ROSE_ASSERT(from_mangledName.get_length() < functionNameLength);
    ROSE_ASSERT(to_mangledName.get_length() < functionNameLength);
    CallGraphFileStructure functionCall;

    if (storedEdges.count(i_edge->first) == 0) {
      functionCall.isNode = false;

      // Always false for a call graph edge.
      functionCall.isDefiningDeclaration = false;

      memcpy(functionCall.function_name, from_mangledName.str(),
             from_mangledName.get_length());
      memcpy(functionCall.function_call_name, to_mangledName.str(),
             to_mangledName.get_length());

      binaryStream.write(reinterpret_cast<const char *>(&functionCall),
                         sizeof(CallGraphFileStructure));
    }

    i_edge++;
  }

  binaryStream.close();
  ROSE_ASSERT(binaryStream.good());
}

int NewCallGraph::buildCallGraph(SgProject *project) {
  int status = 0;

  ROSE_ASSERT(project != NULL);

  // Put analysis traversal here!
  CallGraph treeTraversal;
  treeTraversal.traverse(project, preorder);

  // Write the file using the collected data from this translation unit.
  generateCallGraphFile(project, treeTraversal);

  return status;
}
