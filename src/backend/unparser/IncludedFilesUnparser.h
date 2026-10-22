#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

class IncludedFilesUnparser : public AstSimpleProcessing {
private:
  SgProject *projectNode;
  std::string workingDirectory;
  std::string unparseRootPath;
  static const std::string defaultUnparseFolderName;

  // Keeps track of including paths that include files to unparse into files
  // that would not be unparsed. This is needed to determine where to store the
  // unparsed included files.

  // Keeps track of all #include directives that include a *not* unparsed file
  // into a *not* unparsed file.
  std::set<PreprocessingInfo *> notUnparsedPreprocessingInfos;

  // Keeps all paths to which files are unparsed. This is needed to avoid name
  // collisions.
  std::set<std::string> unparsePaths;

  // Keeps all paths that should be used with -I command line option for the
  // compiler. Note that the order of -I paths matters. We should avoid
  // situations when includes that have "up folder" jumps look outside the
  // created folder hierarchy. Therefore, the -I paths are sorted from the
  // deepest "up folder" hierarchy to the least deep.

  // Keeps scopes that have to be unparsed to produce the included file (i.e.
  // this scope contains the included file). But note that not all of the scopes
  // in this map might require unparsing.
  std::map<std::string, SgScopeStatement *> unparseScopesMap;
  std::map<std::string, std::map<unsigned int, std::set<SgScopeStatement *>>>
      unparseScopeCandidatesMap;
  std::map<std::string, std::map<unsigned int, std::set<SgScopeStatement *>>>
      preprocessingUnparseScopeCandidatesMap;
  std::map<std::string,
           std::map<unsigned int, std::map<SgScopeStatement *, SgNode *>>>
      unparseScopeCandidateOriginsMap;
  std::map<std::string,
           std::map<unsigned int, std::map<SgScopeStatement *, SgNode *>>>
      preprocessingUnparseScopeCandidateOriginsMap;
  std::map<std::string, unsigned int> unparseOccurrenceMap;

  std::set<std::string>
      newFilesToUnparse; // this is a temporary storage that needs to be
                         // accessible across several methods

  // All planning state belongs to one project/unparse invocation.  This used
  // to be process-global, which made a second project observe filenames and
  // dangling SgSourceFile pointers from the first project.
  std::set<std::string> filesWithMarkedTransformations;
  std::set<std::string> allFiles;
  std::set<std::string> filesToUnparse;
  std::set<std::string> filesWithUpdatedIncludePaths;
  std::list<std::pair<int, std::string>> includeCompilerPaths;
  std::map<std::string, std::string> unparseMap;
  std::map<std::string, std::set<std::string>> includingPathsMap;
  std::map<std::string, std::set<PreprocessingInfo *>>
      includingPreprocessingInfos;
  std::map<const PreprocessingInfo *, std::string> includeRewrites;
  std::set<std::string> filesToCopy;
  std::map<std::string, SgSourceFile *> unparseSourceFileMap;

  void prepareForNewIteration();
  void initializeFilesToUnparse();
  void collectAdditionalFilesToUnparse();
  bool isConflictingIncludePath(const std::string &includePath);
  void applyFunctionToIncludingPreprocessingInfos(
      const std::set<std::string> &includedFiles,
      void (IncludedFilesUnparser::*funPtr)(
          const std::string &includedFile,
          PreprocessingInfo *includingPreprocessingInfo));
  void collectNewFilesToUnparse(const std::string &includedFile,
                                PreprocessingInfo *includingPreprocessingInfo);
  void collectIncludingPathsFromUnaffectedFiles(
      const std::string &includedFile,
      PreprocessingInfo *includingPreprocessingInfo);
  void
  updatePreprocessingInfoPaths(const std::string &includedFile,
                               PreprocessingInfo *includingPreprocessingInfo);
  void collectNotUnparsedPreprocessingInfos(
      const std::string &includedFile,
      PreprocessingInfo *includingPreprocessingInfo);
  void populateUnparseMap();
  void collectIncludeCompilerPaths();
  void collectNotUnparsedFilesThatRequireUnparsingToAvoidFileNameCollisions();
  void addIncludeCompilerPath(int upFolderCount,
                              const std::string &includeCompilerPath);
  void addToUnparseScopesMap(const std::string &fileName, SgNode *startNode,
                             unsigned int physicalFileOccurrence,
                             bool preprocessingOnly);
  void validateUnparseScopeCandidates();

  void collectAdditionalListOfHeaderFilesToCopy();

protected:
  void visit(SgNode *astNode);

public:
  ~IncludedFilesUnparser();
  IncludedFilesUnparser(SgProject *projectNode);
  bool isInputFile(const std::string &absoluteFileName);

  void figureOutWhichFilesToUnparse();

  std::string getUnparseRootPath();
  std::map<std::string, std::string> getUnparseMap();
  std::map<std::string, SgScopeStatement *> getUnparseScopesMap();
  std::map<std::string, unsigned int> getUnparseOccurrenceMap();
  std::list<std::string> getIncludeCompilerOptions();

  std::map<std::string, SgSourceFile *> getUnparseSourceFileMap();

  std::set<std::string> getFilesToCopy();
  const std::set<std::string> &getFilesWithMarkedTransformations() const;
  const std::set<std::string> &getFilesWithUpdatedIncludePaths() const;
  const std::map<const PreprocessingInfo *, std::string> &
  getIncludeRewrites() const;
  std::string
  getCopiedFileOutputPath(const std::string &absoluteFileName) const;
};
