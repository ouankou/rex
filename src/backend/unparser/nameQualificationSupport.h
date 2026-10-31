#include "AstProcessing.h"
#include "nameQualificationState.h"
#include "name_qualification_support.h"

#include <map>

#include <memory>

#include <set>

#include <string>

#include <tuple>

#include <unordered_map>

#include <vector>


std::string globalUnparseToString(const SgNode *node, SgUnparse_Info *info,
                                  NameQualificationContext *nameQualifications);
std::string globalUnparseToString(const SgTemplateArgumentPtrList *arguments,
                                  SgUnparse_Info *info,
                                  NameQualificationContext *nameQualifications);
std::string globalUnparseToString(const SgTemplateParameterPtrList *parameters,
                                  SgUnparse_Info *info,
                                  NameQualificationContext *nameQualifications);

class NameQualificationUsingDirectiveOrderCache;
class NameQualificationFileAnalysis;

bool templateClassMatchesPattern(const SgTemplateClassDeclaration *declaration,
                                 const SgTemplateClassDeclaration *pattern);

// A C tag written lexically inside a record belongs to the translation-unit
// tag namespace semantically. Validate that complete source/semantic split so
// all qualification and statement-emission phases consume one contract.
bool isExactCLexicalRecordTagSourceSurface(
    const SgDeclarationStatement *declaration,
    const SgScopeStatement *structuralScope);

class NameQualificationInheritedAttribute {
private:
  SgScopeStatement *currentScope;

  // DQ (4/19/2019): Added support to include current statement (required for
  // nested traversals of types to support name qualification for
  // SgPointerMemberType).
  SgStatement *currentStatement;
  SgNode *referenceNode;

public:
  NameQualificationInheritedAttribute();
  NameQualificationInheritedAttribute(
      const NameQualificationInheritedAttribute &X);

  // DQ (5/24/2013): Allow the current scope to be tracked from the traversal of
  // the AST instead of being computed at each IR node which is a problem for
  // template arguments. See test2013_187.C for an example of this.
  SgScopeStatement *get_currentScope();
  void set_currentScope(SgScopeStatement *scope);

  // DQ (4/19/2019): Added support to include current statement (required for
  // nested traversals of types to support name qualification for
  // SgPointerMemberType).
  SgStatement *get_currentStatement();
  void set_currentStatement(SgStatement *statement);
  SgNode *get_referenceNode();
  void set_referenceNode(SgNode *referenceNode);
};

void generateNameQualificationSupport(
    SgNode *node, SgUnorderedNodeSet &referencedNameSet,
    NameQualificationContext &nameQualifications,
    bool useInputFileTraversalOptimization = true,
    SgStatement *explicitEmissionStatement = nullptr,
    SgNode *explicitReferenceNode = nullptr,
    SgType *explicitSemanticTypeUse = nullptr,
    SgDeclarationStatement *explicitTypeDefinition = nullptr);

class NameQualificationSynthesizedAttribute {
public:
  SgNode *node;

public:
  NameQualificationSynthesizedAttribute();
  NameQualificationSynthesizedAttribute(SgNode *astNode);
  NameQualificationSynthesizedAttribute(
      const NameQualificationSynthesizedAttribute &X);
  NameQualificationSynthesizedAttribute &
  operator=(const NameQualificationSynthesizedAttribute &) = default;
};

class NameQualificationTraversal : public AstTopDownBottomUpProcessing<
                                       NameQualificationInheritedAttribute,
                                       NameQualificationSynthesizedAttribute> {
public:
  typedef SgUnorderedMapNodeToString NameQualificationMapType;

  typedef std::map<SgNode *, NameQualificationMapType>
      NameQualificationMapOfMapsType;

  typedef SgUnorderedNodeSet NameQualificationSetType;

private:
  // We might want to keep track of types that have been seen already.
  // Though, as a detail, all names in a class would be included as
  // having been seen as we enter the scope of the class (not as they
  // are traversed in the class).
  // * Keep a set of SgNodes that have been seen as declarations
  //   (referenced names don't count if they don't specify a scope (location)).
  // * So we need a function to gather all the name in a class.

  // Indexes shared by a traversal and every nested traversal it starts.
  struct SharedIndexes {
    SharedIndexes(NameQualificationSetType &referencedNames,
                  NameQualificationMapType &names,
                  NameQualificationMapType &types,
                  NameQualificationMapOfMapsType &typeUses,
                  const NameQualificationSetType *baseReferencedNames = nullptr,
                  const NameQualificationMapType *baseNames = nullptr,
                  const NameQualificationMapType *baseTypes = nullptr,
                  const NameQualificationMapOfMapsType *baseTypeUses = nullptr)
        : referencedNames(referencedNames, baseReferencedNames),
          names(names, baseNames), types(types, baseTypes),
          typeUses(typeUses, baseTypeUses) {}
    NameQualificationNodeSet referencedNames;
    NameQualificationNodeStrings names;
    NameQualificationNodeStrings types;
    NameQualificationUseSiteStrings typeUses;
  };
  std::shared_ptr<SharedIndexes> indexes;

  // Data
  // DQ (6/21/2011): since this is used recursively we can't have this be a new
  // set each time. Build the local set to use to record when declaration that
  // might required qualified references have been seen.
  NameQualificationNodeSet &referencedNameSet;

  // Traversal-local compatibility indexes used while the qualification
  // algorithm is running. Exact use-site records live in nameQualifications;
  // these maps are never process-global unparser state.
  NameQualificationNodeStrings &qualifiedNameMapForNames;
  NameQualificationNodeStrings &qualifiedNameMapForTypes;

  // Traversal-local index for qualification of nested type components.
  NameQualificationUseSiteStrings &qualifiedNameMapForMapsOfTypes;

  std::shared_ptr<NameQualificationUsingDirectiveOrderCache>
      usingDirectiveOrderCache;
  class ScopedVisibilityNodeSet {
  public:
    using const_iterator = SgUnorderedNodeSet::const_iterator;
    using iterator = SgUnorderedNodeSet::iterator;

    ScopedVisibilityNodeSet()
        : nodes_(std::make_shared<SgUnorderedNodeSet>()) {}
    // Share a committed set; this view's own inserts are rolled back.
    explicit ScopedVisibilityNodeSet(std::shared_ptr<SgUnorderedNodeSet> nodes)
        : nodes_(std::move(nodes)) {
      ROSE_ASSERT(nodes_ != nullptr);
    }
    ScopedVisibilityNodeSet(const ScopedVisibilityNodeSet &other)
        : nodes_(other.nodes_) {
      ROSE_ASSERT(nodes_ != nullptr);
    }
    ScopedVisibilityNodeSet &operator=(const ScopedVisibilityNodeSet &other) {
      if (this != &other) {
        rollback();
        nodes_ = other.nodes_;
        ROSE_ASSERT(nodes_ != nullptr);
      }
      return *this;
    }
    ~ScopedVisibilityNodeSet() { rollback(); }

    operator const SgUnorderedNodeSet &() const { return nodes(); }
    const_iterator find(SgNode *node) const { return nodes().find(node); }
    const_iterator end() const { return nodes().end(); }
    std::size_t size() const { return nodes().size(); }

    std::pair<iterator, bool> insert(SgNode *node) {
      ROSE_ASSERT(node != nullptr);
      std::pair<iterator, bool> inserted = nodes_->insert(node);
      if (inserted.second) {
        inserted_here_.push_back(node);
      }
      return inserted;
    }

    // Make this view's inserts permanent and return the shared set.
    std::shared_ptr<SgUnorderedNodeSet> commit() {
      inserted_here_.clear();
      return nodes_;
    }

  private:
    std::shared_ptr<SgUnorderedNodeSet> nodes_;
    std::vector<SgNode *> inserted_here_;

    const SgUnorderedNodeSet &nodes() const {
      ROSE_ASSERT(nodes_ != nullptr);
      return *nodes_;
    }

    void rollback() {
      if (nodes_ == nullptr) {
        ROSE_ASSERT(inserted_here_.empty());
        return;
      }
      for (auto node = inserted_here_.rbegin(); node != inserted_here_.rend();
           ++node) {
        if (nodes_->erase(*node) != 1) {
          fprintf(stderr,
                  "REX_NAME_QUALIFICATION_INVARIANT[alias-visibility-"
                  "transaction]: node=%p is absent during exact rollback\n",
                  static_cast<void *>(*node));
          ROSE_ABORT();
        }
      }
      inserted_here_.clear();
    }
  };

  ScopedVisibilityNodeSet visibleAliasCausalNodes;
  NameQualificationContext &nameQualifications;
  NameQualificationTraversal *sourcePublicationOwner = nullptr;
  SgStatement *sourcePublicationUseSite = nullptr;
  void requireSourcePublicationPrefix();

  SgStatement *
  qualificationUseSite(SgNode *node,
                       SgStatement *explicitUseSite = nullptr) const;
  void recordNameQualification(SgNode *node, const std::string &qualifier,
                               int length, bool global, bool typeElaboration,
                               SgStatement *explicitUseSite = nullptr);
  void recordTypeQualification(SgNode *node, const std::string &qualifier,
                               int length, bool global, bool typeElaboration,
                               SgStatement *explicitUseSite = nullptr);

  // DQ (1/24/2019): We need to accumulate the list of possible classes that are
  // private base classes so that additional name qualification can be added to
  // prevent the access of base classes that have been made private in nested
  // chass hierarchies.
  typedef std::map<SgClassDeclaration *, std::set<SgClassDeclaration *>>
      BaseClassSetMap;

  // DQ (1/24/2019): This is the list of private base classes.
  BaseClassSetMap privateBaseClassSets;

  // DQ (1/24/2019): From the set of private base classes we construct the set
  // of unaccessible base classes
  BaseClassSetMap inaccessibleClassSets;

  // DQ (7/23/2011): This supports nested calls where the scope of a subtrees
  // must have its scope explicitly specified. I think this only happens for the
  // index in the SgArrayType.
  SgScopeStatement *explictlySpecifiedCurrentScope;

  // DQ (4/19/2019): Added support to include current statement (required for
  // nested traversals of types to support name qualification for
  // SgPointerMemberType).
  SgStatement *explictlySpecifiedCurrentStatement;

  // The scope that emits the type whose spelling this nested traversal
  // analyzes.  It equals explictlySpecifiedCurrentScope except for an owned
  // class definition, whose members qualify relative to the class while the
  // definition itself is emitted by the enclosing type use.
  SgScopeStatement *explicitTypeEmissionScope;

  // The root of an explicitly contextualized nested traversal.  Expression
  // and type roots normally inherit the caller's emission statement, but a
  // statement expression below that root introduces real source statements
  // whose own scopes and statement identities must take precedence.
  SgNode *explicitTraversalRoot;
  // Available namespace aliases take priority during qualification.
  typedef std::map<SgDeclarationStatement *,
                   SgNamespaceAliasDeclarationStatement *>
      namespaceAliasMapType;
  namespaceAliasMapType namespaceAliasDeclarationMap;

  // DQ (8/14/2025): Adding optimization (default is false) to support name
  // qualification retricted to just the input source file (instead of the whole
  // translation unit).
  bool suppressNameQualificationAcrossWholeTranslationUnit;

  struct TemplateArgumentListEvaluationKey {
    const SgTemplateArgumentPtrList *templateArgumentList;
    SgScopeStatement *scope;
    SgStatement *positionStatement;

    bool operator<(const TemplateArgumentListEvaluationKey &rhs) const {
      return std::tie(templateArgumentList, scope, positionStatement) <
             std::tie(rhs.templateArgumentList, rhs.scope,
                      rhs.positionStatement);
    }
  };

  struct TemplateDeclarationEvaluationKey {
    SgDeclarationStatement *declaration;
    SgScopeStatement *scope;
    SgStatement *positionStatement;

    bool operator<(const TemplateDeclarationEvaluationKey &rhs) const {
      return std::tie(declaration, scope, positionStatement) <
             std::tie(rhs.declaration, rhs.scope, rhs.positionStatement);
    }
  };

  std::set<TemplateArgumentListEvaluationKey>
      activeTemplateArgumentListEvaluations;
  NameQualificationEvaluationSet<TemplateArgumentListEvaluationKey>
      completedTemplateArgumentListEvaluations;
  std::set<TemplateDeclarationEvaluationKey>
      activeTemplateDeclarationEvaluations;
  NameQualificationEvaluationSet<TemplateDeclarationEvaluationKey>
      completedTemplateDeclarationEvaluations;

  NameQualificationTraversal(
      std::shared_ptr<SharedIndexes> input_indexes,
      NameQualificationContext &input_nameQualifications);
  // Continue from a completed file walk.  Additions go to input_indexes and
  // input_nameQualifications, which extend the analysis without modifying it.
  NameQualificationTraversal(
      const NameQualificationFileAnalysis &analysis,
      std::shared_ptr<SharedIndexes> input_indexes,
      NameQualificationContext &input_nameQualifications);
  friend class NameQualificationFileAnalysis;

public:
  NameQualificationTraversal(
      NameQualificationMapType &input_qualifiedNameMapForNames,
      NameQualificationMapType &input_qualifiedNameMapForTypes,
      NameQualificationMapOfMapsType &input_qualifiedNameMapForMapsOfTypes,
      NameQualificationSetType &input_referencedNameSet,
      NameQualificationContext &input_nameQualifications);
  ~NameQualificationTraversal();

protected:
  // Source-order qualification must follow lexical emission edges. Semantic
  // auxiliary declarations and non-output physical-source groups remain
  // available to lookup through their scope and symbols, but are never
  // independent positions in the emitted source.
  void setNodeSuccessors(
      SgNode *node,
      AstSuccessorsSelectors::SuccessorsContainer &successors) override;

public:
  void prepareSourcePublicationPrefix(SgStatement *useSite);

  // A standalone type is a semantic edge, not a structural successor of the
  // walked source, so it is analyzed after the walk with the walk's state.
  void
  applyExplicitSemanticTypeUse(SgType *explicitSemanticTypeUse,
                               SgNode *explicitReferenceNode,
                               SgStatement *explicitEmissionStatement,
                               SgDeclarationStatement *explicitTypeDefinition);
  void setExplicitTraversalContext(SgScopeStatement *currentScope,
                                   SgStatement *currentStatement,
                                   SgNode *traversalRoot);

  // Nested type expressions need an explicit emission scope and statement.
  void generateNestedTraversalWithExplicitScope(
      SgNode *node, SgScopeStatement *currentScope,
      SgStatement *currentStatement, SgNode *referenceNode = NULL,
      SgScopeStatement *typeEmissionScope = nullptr);

  // Evaluates how much name qualification is required (typically 0 (no
  // qualification), but sometimes the depth of the nesting of scopes plus 1
  // (full qualification with global scoping operator)). int
  // nameQualificationDepth ( SgClassDefinition* classDefinition );
  int nameQualificationDepth(SgDeclarationStatement *declaration,
                             SgScopeStatement *currentScope,
                             SgStatement *positionStatement,
                             bool forceMoreNameQualification = false);
  int nameQualificationDepth(SgInitializedName *initializedName,
                             SgScopeStatement *currentScope,
                             SgStatement *positionStatement);
  int nameQualificationDepth(SgType *type, SgScopeStatement *currentScope,
                             SgStatement *positionStatement);

  int nameQualificationDepthOfParent(SgDeclarationStatement *declaration,
                                     SgScopeStatement *currentScope,
                                     SgStatement *positionStatement);
  int nameQualificationDepthForType(SgInitializedName *initializedName,
                                    SgType *qualificationType,
                                    SgScopeStatement *currentScope,
                                    SgStatement *positionStatement);

  // DQ (7/23/2011): Added support for array type index expressions.
  void processNameQualificationArrayType(SgArrayType *arrayType,
                                         SgScopeStatement *currentScope,
                                         SgStatement *positionStatement);
  void
  processNameQualificationForPossibleArrayType(SgType *possibleArrayType,
                                               SgScopeStatement *currentScope,
                                               SgStatement *positionStatement);

  SgDeclarationStatement *associatedDeclaration(SgScopeStatement *scope);
  SgDeclarationStatement *associatedDeclaration(SgType *type);

  void nameQualificationTypeSupport(SgType *type,
                                    SgScopeStatement *currentScope,
                                    SgInitializedName *initializedName);

  NameQualificationInheritedAttribute evaluateInheritedAttribute(
      SgNode *n,
      NameQualificationInheritedAttribute inheritedAttribute) override;

  NameQualificationSynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *n, NameQualificationInheritedAttribute inheritedAttribute,
      SynthesizedAttributesList synthesizedAttributeList) override;

  // Set the values in each reference to the name qualified language construct.
  void setNameQualification(SgVarRefExp *varRefExp,
                            SgVariableDeclaration *variableDeclaration,
                            int amountOfNameQualificationRequired);

  void setNameQualification(SgBaseClass *baseClass,
                            SgDeclarationStatement *sourceDeclaration,
                            SgStatement *useSiteStatement,
                            int amountOfNameQualificationRequired);
  void setNameQualification(SgUsingDeclarationStatement *usingDeclaration,
                            SgInitializedName *associatedInitializedName,
                            int amountOfNameQualificationRequired);
  void setNameQualification(SgUsingDeclarationStatement *usingDeclaration,
                            SgDeclarationStatement *declaration,
                            int amountOfNameQualificationRequired);
  void setNameQualification(SgUsingDirectiveStatement *usingDirective,
                            SgDeclarationStatement *declaration,
                            int amountOfNameQualificationRequired);
  void setNameQualification(SgFunctionRefExp *functionRefExp,
                            SgFunctionDeclaration *functionDeclaration,
                            int amountOfNameQualificationRequired);
  void setNameQualification(SgTemplateFunctionRefExp *functionRefExp,
                            SgFunctionDeclaration *functionDeclaration,
                            int amountOfNameQualificationRequired);
  void setFunctionReferenceNameQualification(
      SgExpression *functionRefExp, SgFunctionDeclaration *functionDeclaration,
      int amountOfNameQualificationRequired);
  void setNameQualification(SgMemberFunctionRefExp *functionRefExp,
                            SgMemberFunctionDeclaration *functionDeclaration,
                            int amountOfNameQualificationRequired);

  void setNameQualification(SgPseudoDestructorRefExp *psuedoDestructorRefExp,
                            SgDeclarationStatement *declarationStatement,
                            int amountOfNameQualificationRequired);

  void setNameQualification(SgConstructorInitializer *constructorInitializer,
                            SgDeclarationStatement *declaration,
                            int amountOfNameQualificationRequired,
                            SgStatement *useSiteStatement);

  void setNameQualification(SgAggregateInitializer *exp,
                            SgDeclarationStatement *typeDeclaration,
                            int amountOfNameQualificationRequired);

  void setNameQualificationOnType(SgInitializedName *initializedName,
                                  SgType *qualificationType,
                                  SgDeclarationStatement *declaration,
                                  int amountOfNameQualificationRequired,
                                  bool skipGlobalQualification,
                                  SgStatement *explicitUseSite);

  // DQ (12/17/2013): Added support for the name qualification of the
  // SgInitializedName object when used in the context of the preinitialization
  // list.
  void setNameQualificationOnName(SgInitializedName *initializedName,
                                  SgDeclarationStatement *declaration,
                                  int amountOfNameQualificationRequired,
                                  bool skipGlobalQualification);

  void setNameQualification(SgVariableDeclaration *variableDeclaration,
                            SgDeclarationStatement *declaration,
                            int amountOfNameQualificationRequired);

  void setNameQualificationOnBaseType(SgTypedefDeclaration *typedefDeclaration,
                                      SgDeclarationStatement *declaration,
                                      int amountOfNameQualificationRequired);
  void setNameQualification(SgTemplateArgument *templateArgument,
                            SgDeclarationStatement *declaration,
                            int amountOfNameQualificationRequired,
                            SgStatement *useSiteStatement);
  void setNameQualification(SgExpression *exp,
                            SgDeclarationStatement *typeDeclaration,
                            int amountOfNameQualificationRequired,
                            SgType *sourceReferencedType = nullptr);

  void setNameQualification(SgNonrealRefExp *exp,
                            SgDeclarationStatement *typeDeclaration, int);

  void setNameQualification(SgEnumVal *enumVal,
                            SgEnumDeclaration *enumDeclaration,
                            int amountOfNameQualificationRequired,
                            SgStatement *useSiteStatement);

  void setNameQualification(SgFunctionDeclaration *functionDeclaration,
                            int amountOfNameQualificationRequired);

  void setNameQualification(SgClassDeclaration *classDeclaration,
                            int amountOfNameQualificationRequired);

  void setNameQualification(
      SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration,
      SgDeclarationStatement *declaration,
      int amountOfNameQualificationRequired);

  void setNameQualification(SgEnumDeclaration *enumDeclaration,
                            int amountOfNameQualificationRequired);

  // This is a separate function just for setting the information specific to
  // the name qualification of return types. This information cannot be stored
  // in the SgFunctionType since that might be shared and referenced from
  // different locations.
  void
  setNameQualificationReturnType(SgFunctionDeclaration *functionDeclaration,
                                 SgDeclarationStatement *declaration,
                                 int amountOfNameQualificationRequired,
                                 SgStatement *useSiteStatement = nullptr);

  void setNameQualificationOnClassOf(SgPointerMemberType *pointerMemberType,
                                     SgDeclarationStatement *declaration,
                                     int amountOfNameQualificationRequired,
                                     SgStatement *useSiteStatement);
  void setNameQualificationOnBaseType(SgPointerMemberType *pointerMemberType,
                                      SgDeclarationStatement *declaration,
                                      int amountOfNameQualificationRequired,
                                      SgStatement *useSiteStatement);

  SgDeclarationStatement *getDeclarationAssociatedWithType(SgType *type);

  // Supporting function for different overloaded versions of the
  // setNameQualification() function.
  std::string
  setNameQualificationSupport(SgNode *referenceNode, SgScopeStatement *scope,
                              const int inputNameQualificationLength,
                              int &output_amountOfNameQualificationRequired,
                              bool &outputGlobalQualification,
                              bool &outputTypeEvaluation,
                              SgStatement *explicitUseSite = nullptr);

  // DQ (5/14/2011): type elaboration only works between non-types and types.
  // Different types must be distinquished using name qualification.
  bool requiresTypeElaboration(SgSymbol *symbol);

  // DQ (5/15/2011): Added support for template arguments and their recursive
  // handling.
  void evaluateNameQualificationForTemplateArgumentList(
      SgTemplateArgumentPtrList &templateArgumentList,
      SgScopeStatement *currentScope, SgStatement *positionStatement);

  void evaluateNameQualificationForNonrealDeclarationChain(
      SgNonrealDecl *declaration, SgScopeStatement *currentScope,
      SgStatement *positionStatement);

  void traverseType(SgType *type, SgNode *nodeReferenceToType,
                    SgScopeStatement *currentScope,
                    SgStatement *positionStatement);

  void traverseTemplatedFunction(SgFunctionRefExp *functionRefExp,
                                 SgNode *nodeReference,
                                 SgScopeStatement *currentScope,
                                 SgStatement *positionStatement);

  void traverseTemplatedMemberFunction(
      SgMemberFunctionRefExp *memberFunctionRefExp, SgNode *nodeReference,
      SgScopeStatement *currentScope, SgStatement *positionStatement);

  void
  evaluateTemplateInstantiationDeclaration(SgDeclarationStatement *declaration,
                                           SgScopeStatement *currentScope,
                                           SgStatement *positionStatement);

  // Every source declaration used for contextual qualification must have one
  // exact published identity that is visible before the use site.
  void validateDeclarationPublishedBeforeQualification(
      SgDeclarationStatement *declaration);

  // An explicit function-instantiation directive can name an exact primary
  // template retained from a system header.  That primary is available
  // through the typed instantiation-family edge even though it is deliberately
  // absent from the emitted declaration frontier.
  void validateFunctionDeclarationAvailableForQualification(
      SgFunctionDeclaration *declaration, SgScopeStatement *currentScope,
      SgStatement *currentStatement);

  // Type declarations are either exact source-order surfaces or exact
  // semantic dependencies.  Validate the corresponding disjoint contract
  // before recording contextual type qualification.
  void validateTypeDeclarationAvailableForQualification(
      SgDeclarationStatement *declaration, SgType *type,
      SgInitializedName *initializedName, SgScopeStatement *currentScope,
      SgStatement *currentStatement, int qualificationDepth);

  // DQ (3/31/2014): Adding support for global qualifiction.
  size_t depthOfGlobalNameQualification(SgDeclarationStatement *declaration);

  void set_suppressNameQualificationAcrossWholeTranslationUnit(bool value);
};

// The completed name-qualification walk of one source file, frozen so that
// later requests on the unchanged file reuse it.  Requests rooted at the file
// read its records directly.  A standalone type use continues from the frozen
// end-of-walk state in a traversal whose additions extend the caller's
// records and never the analysis.
class NameQualificationFileAnalysis {
public:
  explicit NameQualificationFileAnalysis(SgSourceFile *file);
  NameQualificationFileAnalysis(const NameQualificationFileAnalysis &) = delete;
  NameQualificationFileAnalysis &
  operator=(const NameQualificationFileAnalysis &) = delete;

  SgSourceFile *file() const { return file_; }
  const NameQualificationContext &records() const { return records_; }

  //! Analyze a standalone type use into records, which must extend
  //! records().
  void applyExplicitSemanticTypeUse(
      NameQualificationContext &records, SgType *explicitSemanticTypeUse,
      SgNode *explicitReferenceNode, SgStatement *explicitEmissionStatement,
      SgDeclarationStatement *explicitTypeDefinition) const;

private:
  friend class NameQualificationTraversal;

  SgSourceFile *file_;
  NameQualificationContext records_;
  NameQualificationTraversal::NameQualificationSetType referencedNames_;
  NameQualificationTraversal::NameQualificationMapType names_;
  NameQualificationTraversal::NameQualificationMapType types_;
  NameQualificationTraversal::NameQualificationMapOfMapsType typeUses_;
  std::set<NameQualificationTraversal::TemplateArgumentListEvaluationKey>
      completedTemplateArgumentListEvaluations_;
  std::set<NameQualificationTraversal::TemplateDeclarationEvaluationKey>
      completedTemplateDeclarationEvaluations_;
  std::shared_ptr<SgUnorderedNodeSet> visibleAliasCausalNodes_;
  NameQualificationTraversal::BaseClassSetMap privateBaseClassSets_;
  NameQualificationTraversal::BaseClassSetMap inaccessibleClassSets_;
  std::shared_ptr<NameQualificationUsingDirectiveOrderCache>
      usingDirectiveOrderCache_;
};

// Resolve the one project that a qualification request's root, emission
// statement and reference node belong to; disagreement is a hard error.
SgProject *
resolveNameQualificationProject(SgNode *node,
                                SgStatement *explicitEmissionStatement,
                                SgNode *explicitReferenceNode);
