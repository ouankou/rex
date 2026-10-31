#include "rose.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Request = std::function<std::string()>;

SgProject *parseProject(const char *program, const char *source) {
  const std::vector<std::string> arguments = {
      program, "-rose:verbose", "0", "-rose:skipfinalCompileStep", "-std=c++17",
      "-c",    source};
  return frontend(arguments);
}

bool isSpecimenSource(SgLocatedNode *node, SgSourceFile *file) {
  Sg_File_Info *info = node->get_file_info();
  return info != nullptr && info->get_line() > 0 &&
         info->get_physical_file_id() >= 0 && !info->isCompilerGenerated() &&
         !info->isFrontendSpecific() &&
         !info->isSourcePositionUnavailableInFrontend() &&
         info->get_filenameString() == file->getFileName();
}

SgInitializedName *findInitializedName(SgSourceFile *file,
                                       const std::string &name) {
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    SgInitializedName *initializedName = isSgInitializedName(node);
    if (initializedName->get_name().getString() == name &&
        isSpecimenSource(initializedName, file)) {
      return initializedName;
    }
  }
  return nullptr;
}

// Statements, case labels, variable references and declared types, each
// unparsed on its own as fragment-heavy clients do.
std::vector<Request> collectRequests(SgSourceFile *file) {
  std::vector<Request> requests;
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgStatement)) {
    SgStatement *statement = isSgStatement(node);
    if (!isSpecimenSource(statement, file) ||
        !statement->get_file_info()->isOutputInCodeGeneration()) {
      continue;
    }
    if (isSgVariableDeclaration(statement) != nullptr ||
        isSgReturnStmt(statement) != nullptr ||
        isSgSwitchStatement(statement) != nullptr ||
        isSgFunctionDeclaration(statement) != nullptr) {
      requests.push_back([statement] { return statement->unparseToString(); });
    }
    if (SgCaseOptionStmt *option = isSgCaseOptionStmt(statement)) {
      SgExpression *key = option->get_key();
      requests.push_back([key] { return key->unparseToString(); });
    }
  }
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    if (isSpecimenSource(reference, file)) {
      requests.push_back([reference] { return reference->unparseToString(); });
    }
  }
  for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    if (!isSpecimenSource(name, file) || name->get_type() == nullptr ||
        name->get_scope() == nullptr) {
      continue;
    }
    requests.push_back([file, name] {
      SgUnparse_Info info;
      info.set_current_source_file(file);
      info.set_current_scope(name->get_scope());
      info.set_template_argument_qualification_context(
          SageInterface::getEnclosingStatement(name));
      info.set_reference_node_for_qualification(name);
      return name->get_type()->unparseToString(&info);
    });
  }
  return requests;
}

int fail(const std::string &message) {
  std::cerr << "rex_unparser_qualification_session_contract: " << message
            << std::endl;
  return 1;
}

int checkEquivalence(SgSourceFile *file) {
  const std::vector<Request> requests = collectRequests(file);
  if (requests.size() < 20) {
    return fail("specimen yields too few requests: " +
                std::to_string(requests.size()));
  }
  if (UnparseQualificationSession::analysisFor(file) != nullptr) {
    return fail("an analysis exists without an open session");
  }
  std::vector<std::string> perCall;
  for (const Request &request : requests) {
    perCall.push_back(request());
  }
  {
    UnparseQualificationSession session;
    const NameQualificationFileAnalysis *analysis =
        UnparseQualificationSession::analysisFor(file);
    if (analysis == nullptr ||
        UnparseQualificationSession::analysisFor(file) != analysis) {
      return fail("a session does not keep one analysis per file");
    }
    for (size_t index = 0; index < requests.size(); ++index) {
      const std::string output = requests[index]();
      if (output != perCall[index]) {
        return fail("request " + std::to_string(index) + " printed\n" + output +
                    "\nin a session but\n" + perCall[index] + "\nper call");
      }
    }
  }
  {
    // In reverse order, each type use must still see only the completed walk.
    UnparseQualificationSession session;
    for (size_t index = requests.size(); index-- > 0;) {
      if (requests[index]() != perCall[index]) {
        return fail("request " + std::to_string(index) +
                    " depends on earlier requests in a session");
      }
    }
  }
  if (UnparseQualificationSession::analysisFor(file) != nullptr) {
    return fail("a closed session left its analysis active");
  }
  return 0;
}

int checkNesting(SgSourceFile *file) {
  const std::vector<Request> requests = collectRequests(file);
  UnparseQualificationSession outer;
  const NameQualificationFileAnalysis *analysis =
      UnparseQualificationSession::analysisFor(file);
  {
    UnparseQualificationSession inner;
    if (UnparseQualificationSession::analysisFor(file) != analysis) {
      return fail("an inner session did not join the outer session");
    }
    (void)requests.front()();
  }
  if (UnparseQualificationSession::analysisFor(file) != analysis) {
    return fail("closing an inner session closed the outer session");
  }
  return 0;
}

int checkUntrackedInfo(SgSourceFile *file) {
  const std::vector<Request> requests = collectRequests(file);
  SgInitializedName *name = findInitializedName(file, "rex_value");
  if (name == nullptr) {
    return fail("specimen has no rex_value");
  }
  const uint64_t before = SgNode::get_globalAstModificationSequence();
  for (const Request &request : requests) {
    (void)request();
  }
  SgUnparse_Info info;
  info.set_current_source_file(file);
  info.set_current_scope(name->get_scope());
  info.set_reference_node_for_qualification(name);
  info.set_nestingLevel(2);
  if (SgNode::get_globalAstModificationSequence() != before) {
    return fail("unparsing advanced the AST modification sequence");
  }
  name->set_name("rex_renamed_value");
  if (SgNode::get_globalAstModificationSequence() == before) {
    return fail("an AST setter did not advance the modification sequence");
  }
  return 0;
}

int checkMutation(SgSourceFile *file) {
  const std::vector<Request> requests = collectRequests(file);
  SgInitializedName *name = findInitializedName(file, "rex_value");
  if (name == nullptr) {
    return fail("specimen has no rex_value");
  }
  UnparseQualificationSession session;
  (void)requests.front()();
  name->set_name("rex_renamed_value");
  (void)requests.front()();
  return fail("a mutation inside a session was not rejected");
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    return fail("usage: <equivalence|nesting|untracked-info|mutation> "
                "<source>");
  }
  const std::string mode = argv[1];
  SgProject *project = parseProject(argv[0], argv[2]);
  ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(project->get_fileList()[0]);
  ROSE_ASSERT(file != nullptr);
  if (mode == "equivalence") {
    return checkEquivalence(file);
  }
  if (mode == "nesting") {
    return checkNesting(file);
  }
  if (mode == "untracked-info") {
    return checkUntrackedInfo(file);
  }
  if (mode == "mutation") {
    return checkMutation(file);
  }
  return fail("unknown mode " + mode);
}
