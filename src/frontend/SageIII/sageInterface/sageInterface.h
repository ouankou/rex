#ifndef ROSE_SAGE_INTERFACE
#define ROSE_SAGE_INTERFACE

#include "sage3basic.hhh"

#include <stdint.h>

#include <functional>

#include <optional>

#include <utility>

#include <variant>

#include "nodeQuery.h" //for querySubTree
#include <iostream>

#include "rosePublicConfig.h"

#include <sstream>

#include <string>

#include <vector>

SgFile *determineFileType(std::vector<std::string> argv, int &nextErrorCode,
                          SgProject *project);

#include <set>

#ifndef ROSE_USE_INTERNAL_FRONTEND_DEVELOPMENT
#include "ClassHierarchyGraph.h"

#include "LivenessAnalysis.h"
#endif

#include "ompSupport.h"
//! A global function for getting the string associated with an enum (which is
//! defined in global scope)
ROSE_DLL_API std::string getVariantName(VariantT v);

// DQ (12/9/2004): Qing, Rich and Dan have decided to start this namespace
// within ROSE This namespace is specific to interface functions that operate on
// the Sage III AST. The name was chosen so as not to conflict with other
// classes within ROSE. This will become the future home of many interface
// functions which operate on the AST and which are generally useful to users.
// As a namespace multiple files can be used to represent the compete interface
// and different developers may contribute interface functions easily.

// Constructor handling: (We have sageBuilder.h now for this purpose, Liao
// 2/1/2008)
//     We could add simpler layers of support for construction of IR nodes by
// hiding many details in "makeSg***()" functions. Such functions would
// return pointers to the associated Sg*** objects and would be able to hide
// many IR specific details, including:
//      memory handling
//      optional parameter settings not often required
//      use of Sg_File_Info objects (and setting them as transformations)
//
// namespace AST_Interface  (this name is taken already by some of Qing's work
// :-)

//! An alias for Sg_File_Info::generateDefaultFileInfoForTransformationNode()
#define TRANS_FILE Sg_File_Info::generateDefaultFileInfoForTransformationNode()

/** Functions that are useful when operating on the AST.
 *
 *  The Sage III IR design attempts to be minimalist. Thus additional
 * functionality is intended to be presented using separate higher level
 * interfaces which work with the IR.  This namespace collects functions that
 * operate on the IR and support numerous types of operations that are common to
 * general analysis and transformation of the AST. */
namespace SageInterface {
// Liao 6/22/2016: keep records of loop init-stmt normalization, later help undo
// it to support autoPar.
struct Transformation_Record {
  struct ForLoopInitNormalizationRecord {
    SgVariableDeclaration *originalDeclaration = nullptr;
    SgVariableDeclaration *normalizedDeclaration = nullptr;
    SgVariableSymbol *originalSymbol = nullptr;
    SgVariableSymbol *normalizedSymbol = nullptr;
    SgScopeStatement *normalizedScope = nullptr;
  };

  // a lookup table to check if a for loop has been normalized for its c99-style
  // init-stmt
  std::map<SgForStatement *, bool> forLoopInitNormalizationTable;
  // Exact declaration, symbol, and publication identities needed to reverse
  // the normalization transaction.
  std::map<SgForStatement *, ForLoopInitNormalizationRecord>
      forLoopInitNormalizationRecord;
};

ROSE_DLL_API extern Transformation_Record trans_records;

// DQ (4/3/2014): Added general AST support separate from the AST.

// Container and API for analysis information that is outside of the AST and as
// a result prevents frequent modification of the IR.
class DeclarationSets {
  // DQ (4/3/2014): This stores all associated declarations as a map of sets.
  // the key to the map is the first nondefining declaration and the elements of
  // the set are all of the associated declarations (including the defining
  // declaration).

private:
  //! Map of first-nondefining declaration to all other associated declarations.
  std::map<SgDeclarationStatement *, std::set<SgDeclarationStatement *> *>
      declarationMap;

public:
  ~DeclarationSets();
  void addDeclaration(SgDeclarationStatement *decl);
  const std::set<SgDeclarationStatement *> *
  getDeclarations(SgDeclarationStatement *decl);

  std::map<SgDeclarationStatement *, std::set<SgDeclarationStatement *> *> &
  getDeclarationMap();

  bool isLocatedInDefiningScope(SgDeclarationStatement *decl);
};

// DQ (4/3/2014): This constructs a data structure that holds analysis
// information about the AST that is separate from the AST.  This is intended to
// be a general mechanism to support analysis information without constantly
// modifying the IR.
DeclarationSets *buildDeclarationSets(SgNode *);

//! An internal counter for generating unique SgName
ROSE_DLL_API extern int gensym_counter;

//! Function to add "C" style comment to statement.
void addMessageStatement(SgStatement *stmt, std::string message);

//! A persistent attribute to represent a unique name for an expression
class UniqueNameAttribute : public AstAttribute {
private:
  std::string name;

public:
  UniqueNameAttribute(std::string n = "") { name = n; };
  AstAttribute::OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
  void set_name(std::string n) { name = n; };
  std::string get_name() { return name; };
};

ROSE_DLL_API void setTemplateParameterKeyword(
    SgTemplateParameter *param,
    SgTemplateParameter::template_parameter_keyword_enum kw);
ROSE_DLL_API SgTemplateParameter::template_parameter_keyword_enum
getTemplateParameterKeyword(SgTemplateParameter *param);
ROSE_DLL_API void
setAbbreviatedFunctionTemplateParameter(SgTemplateParameter *param,
                                        bool is_abbreviated_placeholder = true);
ROSE_DLL_API bool
isAbbreviatedFunctionTemplateParameter(SgTemplateParameter *param);

//------------------------------------------------------------------------
//@{
/*! @name Symbol tables
  \brief  utility functions for symbol tables
*/

// DQ (8/5/2020): the "using namespace" directive will not hide existing
// visability of symbols in resolving visability. So we need to test if a symbol
// is visible exclusing matching alises due to using direectives before we can
// decide to persue name space qualification. This is best demonstrated by
// Cxx_tests/test2020_18.C, test2020_19.C, test2020_20.C, and test2020_21.C.
ROSE_DLL_API SgSymbol *lookupSymbolInParentScopesIgnoringAliasSymbols(
    const SgName &name, SgScopeStatement *currentScope = NULL,
    SgTemplateParameterPtrList *templateParameterList = NULL,
    SgTemplateArgumentPtrList *templateArgumentList = NULL);

// DQ (8/21/2013): Modified to make newest function parameters be default
// arguments. DQ (8/16/2013): For now we want to remove the use of default
// parameters and add the support for template parameters and template
// arguments.
//! Find a symbol in current and ancestor scopes for a given variable name,
//! starting from top of ScopeStack if currentscope is not given or NULL.
// SgSymbol *lookupSymbolInParentScopes (const SgName & name, SgScopeStatement
// *currentScope=NULL); SgSymbol *lookupSymbolInParentScopes (const SgName &
// name, SgScopeStatement *currentScope, SgTemplateParameterPtrList*
// templateParameterList, SgTemplateArgumentPtrList* templateArgumentList);
ROSE_DLL_API SgSymbol *lookupSymbolInParentScopes(
    const SgName &name, SgScopeStatement *currentScope = NULL,
    SgTemplateParameterPtrList *templateParameterList = NULL,
    SgTemplateArgumentPtrList *templateArgumentList = NULL);

//! Parent-scope lookup for one name-qualification invocation. The caller must
//! supply the exact symbol kind and its invocation-owned alias visibility set.
ROSE_DLL_API SgSymbol *lookupSymbolInParentScopesForNameQualification(
    const SgName &name, SgScopeStatement *currentScope,
    VariantT requestedSymbolKind, const SgType *type,
    SgTemplateParameterPtrList *templateParameterList,
    SgTemplateArgumentPtrList *templateArgumentList,
    const SgUnorderedNodeSet &visibleAliasCausalNodes);

//! Parent-scope lookup used only by AST snippet-copy fixup. Recursive
//! base-class search is explicit to this call and never changes global state.
ROSE_DLL_API SgSymbol *lookupSymbolInParentScopesForAstCopyFixup(
    const SgName &name, SgScopeStatement *currentScope,
    VariantT requestedSymbolKind, const SgType *type = NULL,
    SgTemplateParameterPtrList *templateParameterList = NULL,
    SgTemplateArgumentPtrList *templateArgumentList = NULL);

// Liao 1/22/2008, used for get symbols for generating variable reference nodes
// ! Find a variable symbol in current and ancestor scopes for a given name
ROSE_DLL_API SgVariableSymbol *
lookupVariableSymbolInParentScopes(const SgName &name,
                                   SgScopeStatement *currentScope = NULL);

// DQ (11/24/2007): Functions moved from the Fortran support so that they could
// be called from within astPostProcessing.
//! look up the first matched function symbol in parent scopes given only a
//! function name, starting from top of ScopeStack if currentscope is not given
//! or NULL
ROSE_DLL_API SgFunctionSymbol *
lookupFunctionSymbolInParentScopes(const SgName &functionName,
                                   SgScopeStatement *currentScope = NULL);

// Liao, 1/24/2008, find exact match for a function
//! look up function symbol in parent scopes given both name and function type,
//! starting from top of ScopeStack if currentscope is not given or NULL
ROSE_DLL_API SgFunctionSymbol *
lookupFunctionSymbolInParentScopes(const SgName &functionName, const SgType *t,
                                   SgScopeStatement *currentScope = NULL);

ROSE_DLL_API SgFunctionSymbol *lookupTemplateFunctionSymbolInParentScopes(
    const SgName &functionName, SgFunctionType *ftype,
    SgTemplateParameterPtrList *tplparams,
    SgScopeStatement *currentScope = NULL);
ROSE_DLL_API SgFunctionSymbol *lookupTemplateMemberFunctionSymbolInParentScopes(
    const SgName &functionName, SgFunctionType *ftype,
    SgTemplateParameterPtrList *tplparams,
    SgScopeStatement *currentScope = NULL);

ROSE_DLL_API SgTemplateVariableSymbol *
lookupTemplateVariableSymbolInParentScopes(
    const SgName &name, SgTemplateParameterPtrList *tplparams,
    SgTemplateArgumentPtrList *tplargs, SgScopeStatement *currentScope = NULL);

// DQ (8/21/2013): Modified to make newest function parameters be default
// arguments. DQ (8/16/2013): For now we want to remove the use of default
// parameters and add the support for template parameters and template
// arguments. DQ (5/7/2011): Added support for SgClassSymbol (used in name
// qualification support). SgClassSymbol*     lookupClassSymbolInParentScopes
// (const SgName & name, SgScopeStatement *currentScope = NULL);
ROSE_DLL_API SgClassSymbol *lookupClassSymbolInParentScopes(
    const SgName &name, SgScopeStatement *currentScope = NULL,
    SgTemplateArgumentPtrList *templateArgumentList = NULL);
ROSE_DLL_API SgTypedefSymbol *
lookupTypedefSymbolInParentScopes(const SgName &name,
                                  SgScopeStatement *currentScope = NULL);

ROSE_DLL_API SgNonrealSymbol *lookupNonrealSymbolInParentScopes(
    const SgName &name, SgScopeStatement *currentScope = NULL,
    SgTemplateParameterPtrList *templateParameterList = NULL,
    SgTemplateArgumentPtrList *templateArgumentList = NULL);

// DQ (8/21/2013): Modified to make some of the newest function parameters be
// default arguments. DQ (8/13/2013): I am not sure if we want this functions in
// place of lookupTemplateSymbolInParentScopes.
ROSE_DLL_API SgTemplateClassSymbol *lookupTemplateClassSymbolInParentScopes(
    const SgName &name, SgTemplateParameterPtrList *templateParameterList,
    SgTemplateArgumentPtrList *templateArgumentList,
    SgScopeStatement *cscope = NULL);

ROSE_DLL_API SgEnumSymbol *
lookupEnumSymbolInParentScopes(const SgName &name,
                               SgScopeStatement *currentScope = NULL);
ROSE_DLL_API SgNamespaceSymbol *
lookupNamespaceSymbolInParentScopes(const SgName &name,
                                    SgScopeStatement *currentScope = NULL);

// SgClassSymbol* lookupClassSymbolInParentScopes (const SgName &  name,
// SgScopeStatement *cscope);

/*! \brief set_name of symbol in symbol table.

    This function extracts the symbol from the relavant symbol table,
    changes the name (at the declaration) and reinserts it into the
    symbol table.

    \internal  I think this is what this function does, I need to double check.
 */
// DQ (12/9/2004): Moved this function (by Alin Jula) from being a member of
// SgInitializedName to this location where it can be a part of the interface
// for the Sage III AST.
ROSE_DLL_API int set_name(SgInitializedName *initializedNameNode,
                          SgName new_name);

/*! \brief Output function type symbols in global function type symbol table.
 */
void outputGlobalFunctionTypeSymbolTable();

// DQ (6/27/2005):
/*! \brief Output the local symbol tables.

    \implementation Each symbol table is output with the file infor where it is
   located in the source code.
 */
ROSE_DLL_API void outputLocalSymbolTables(SgNode *node);

class OutputLocalSymbolTables : public AstSimpleProcessing {
public:
  void visit(SgNode *node);
};
/*! \brief Regenerate the symbol table.

   \implementation current symbol table must be NULL pointer before calling this
   function (for safety, but is this a good idea?)
 */
// DQ (9/28/2005):
void rebuildSymbolTable(SgScopeStatement *scope);
//! Detach every symbol through the symbol table's exact ownership API.
void detachAllSymbolsFromScope(SgScopeStatement *scope);
//! Validate that every symbol has one exact owning symbol-table entry.
void validateSymbolOwnership(SgNode *root);

/*! \brief Clear those variable symbols with unknown type (together with
 * initialized names) which are also not referenced by any variable references
 * or declarations under root. If root is NULL, all symbols with unknown type
 * will be deleted.
 */
// DQ (3/1/2009):
//! All the symbol table references in the copied AST need to be reset after
//! rebuilding the copied scope's symbol table.
void fixupReferencesToSymbols(const SgScopeStatement *this_scope,
                              SgScopeStatement *copy_scope, SgCopyHelp &help);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Stringify
  \brief Generate a useful string (name) to describe a SgNode
*/
/*! \brief Generate a useful name to describe the SgNode

    \internal default names are used for SgNode objects that can not be
   associated with a name.
 */
// DQ (9/21/2005): General function for extracting the name of declarations
// (when they have names)
std::string get_name(const SgNode *node);

/*! \brief Generate a useful name to describe the declaration

    \internal default names are used for declarations that can not be associated
   with a name.
 */
// DQ (6/13/2005): General function for extracting the name of declarations
// (when they have names)
std::string get_name(const SgStatement *stmt);

/*! \brief Generate a useful name to describe the expression

    \internal default names are used for expressions that can not be associated
   with a name.
 */
std::string get_name(const SgExpression *expr);

/*! \brief Generate a useful name to describe the declaration

    \internal default names are used for declarations that can not be associated
   with a name.
 */
// DQ (6/13/2005): General function for extracting the name of declarations
// (when they have names)
std::string get_name(const SgDeclarationStatement *declaration);

/*! \brief Generate a useful name to describe the scope

    \internal default names are used for scope that cannot be associated with a
   name.
 */
// DQ (6/13/2005): General function for extracting the name of declarations
// (when they have names)
std::string get_name(const SgScopeStatement *scope);

/*! \brief Generate a useful name to describe the SgSymbol

    \internal default names are used for SgSymbol objects that cannot be
   associated with a name.
 */
// DQ (2/11/2007): Added this function to make debugging support more complete
// (useful for symbol table debugging support).
std::string get_name(const SgSymbol *symbol);

/*! \brief Generate a useful name to describe the SgType

    \internal default names are used for SgType objects that cannot be
   associated with a name.
 */
std::string get_name(const SgType *type);

/*! \brief Generate a useful name to describe the SgSupport IR node
 */
std::string get_name(const SgSupport *node);

/*! \brief Generate a useful name to describe the SgLocatedNodeSupport IR node
 */
std::string get_name(const SgLocatedNodeSupport *node);

/*! \brief Generate a useful name to describe the
 * SgC_PreprocessorDirectiveStatement IR node
 */
std::string get_name(const SgC_PreprocessorDirectiveStatement *directive);

/*! \brief Generate a useful name to describe the SgToken IR node
 */
std::string get_name(const SgToken *token);

/*! \brief Returns the type introduced by a declaration.
 */
// PP (11/22/2021): General function for extracting the type of declarations
// (when they declare types)
SgType *getDeclaredType(const SgDeclarationStatement *declaration);

// DQ (3/20/2016): Added to refactor some of the DSL infrastructure support.
/*! \brief Generate a useful name to support construction of identifiers from
   declarations.

    This function permits names to be generated that will be unique across
   translation units (a specific requirement different from the context of the
   get_name() functions above).

    \internal This supports only a restricted set of declarations presently.
 */
std::string
generateUniqueNameForUseAsIdentifier(SgDeclarationStatement *declaration);
std::string generateUniqueNameForUseAsIdentifier_support(
    SgDeclarationStatement *declaration);

/*! \brief Global map of name collisions to support
 * generateUniqueNameForUseAsIdentifier() function.
 */
extern std::map<std::string, int> local_name_collision_map;
extern std::map<std::string, SgNode *> local_name_to_node_map;
extern std::map<SgNode *, std::string> local_node_to_name_map;

/*! \brief Traversal to set the global map of names to node and node to
 * names.collisions to support generateUniqueNameForUseAsIdentifier() function.
 */
void computeUniqueNameForUseAsIdentifier(SgNode *astNode);

/*! \brief Reset map variables used to support
 * generateUniqueNameForUseAsIdentifier() function.
 */
void reset_name_collision_map();

//@}

//------------------------------------------------------------------------
//@{
/*! @name Class utilities
  \brief
*/
/*! \brief Get the default destructor from the class declaration
 */
// DQ (6/21/2005): Get the default destructor from the class declaration
ROSE_DLL_API SgMemberFunctionDeclaration *
getDefaultDestructor(SgClassDeclaration *classDeclaration);

/*! \brief Get the default constructor from the class declaration
 */
// DQ (6/22/2005): Get the default constructor from the class declaration
ROSE_DLL_API SgMemberFunctionDeclaration *
getDefaultConstructor(SgClassDeclaration *classDeclaration);
/*! \brief Return true if template definition is in the class, false if outside
 * of class.
 */
// DQ (8/27/2005):
ROSE_DLL_API bool templateDefinitionIsInClass(
    SgTemplateInstantiationMemberFunctionDecl *memberFunctionDeclaration);

/*! \brief Generate a non-defining (forward) declaration from a defining
   function declaration.

   \internal should put into sageBuilder ?
 */
// DQ (9/17/2005):
ROSE_DLL_API SgTemplateInstantiationMemberFunctionDecl *
buildForwardFunctionDeclaration(
    SgTemplateInstantiationMemberFunctionDecl *memberFunctionInstantiation);

//! Check if a SgNode is a declaration for a structure
ROSE_DLL_API bool isStructDeclaration(SgNode *node);
//! Check if a SgNode is a declaration for a union
ROSE_DLL_API bool isUnionDeclaration(SgNode *node);

// DQ (11/9/2020): Added function to support adding a default constructor
// definition to a class if it does not have a default constructor, but has any
// other constructor that would prevend a compiler generated default constructor
// from being generated by the compiler. Note the physical_file_id is so that it
// can be marked to be unparsed when header file unparsing is active.
ROSE_DLL_API bool addDefaultConstructorIfRequired(
    SgClassType *classType,
    int physical_file_id = Sg_File_Info::TRANSFORMATION_FILE_ID);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Misc.
  \brief Not sure the classifications right now
*/

//! Recursively print current and parent nodes. used within gdb to probe the
//! context of a node.
void recursivePrintCurrentAndParent(SgNode *n);

//! Save AST into a pdf file. Start from a node to find its enclosing file node.
//! The entire file's AST will be saved into a pdf.
void saveToPDF(SgNode *node, std::string filename);
void saveToPDF(SgNode *node); // enable calling from gdb

//! Pretty print AST horizontally, output to std output.
void printAST(SgNode *node);

//! Pretty print AST horizontally, output to a specified file, a simpiler
//! interface than printAST2TextFile()
void printAST(SgNode *node, const char *filename);

//! Pretty print AST horizontally, output to a specified text file. If printType
//! is set to false, don't print out type info.
void printAST2TextFile(SgNode *node, const char *filename,
                       bool printType = true);

//! Pretty print AST horizontally, output to a specified text file. If printType
//! is set to false, don't print out types info.
void printAST2TextFile(SgNode *node, std::string filename,
                       bool printType = true);

// DQ (2/12/2012): Added some diagnostic support.
//! Diagnostic function for tracing back through the parent list to understand
//! at runtime where in the AST a failure happened.
void whereAmI(SgNode *node);

//! Extract a SgPragmaDeclaration's leading keyword . For example "#pragma omp
//! parallel" has a keyword of "omp".
std::string extractPragmaKeyword(const SgPragmaDeclaration *);

//! Check if a node is SgOmp*Statement
ROSE_DLL_API bool isOmpStatement(SgNode *);
/*! \brief Return true if function is overloaded.
 */
// DQ (8/27/2005):
bool isOverloaded(SgFunctionDeclaration *functionDeclaration);
//! Return true if expr refers to an overloaded operator-> or operator->*.
bool isOverloadedArrowOperator(SgExpression *expr);
//! Return true if symbol refers to an overloaded operator-> or operator->*.
bool isOverloadedArrowOperator(const SgFunctionSymbol *func_symbol);
//! Return true if expr is part of an overloaded operator-> call chain.
bool isOverloadedArrowOperatorChain(SgExpression *expr);

// DQ (2/14/2012): Added support function used for variable declarations in
// conditionals.
//! Support function used for variable declarations in conditionals
void initializeIfStmt(SgIfStmt *ifstmt, SgStatement *conditional,
                      SgStatement *true_body, SgStatement *false_body);

//! Support function used for variable declarations in conditionals
void initializeSwitchStatement(SgSwitchStatement *switchStatement,
                               SgStatement *item_selector, SgStatement *body);

//! Support function used for variable declarations in conditionals
void initializeWhileStatement(SgWhileStmt *whileStatement,
                              SgStatement *condition, SgStatement *body);

//! Generate unique names for expressions and attach the names as persistent
//! attributes ("UniqueNameAttribute")
void annotateExpressionsWithUniqueNames(SgProject *project);

//! Check if a SgNode is a main() function declaration
ROSE_DLL_API bool isMain(const SgNode *node);
// DQ (6/22/2005):
/*! \brief Generate unique name from C and C++ constructs. The name may contain
   space.

    This is support for the AST merge, but is generally useful as a more general
   mechanism than name mangling which is more closely ties to the generation of
   names to support link-time function name resolution.  This is more general
   than common name mangling in that it resolves more relevant differences
    between C and C++ declarations. (e.g. the type within the declaration:
   "struct { int:8; } foo;").

   \implementation current work does not support expressions.

*/
std::string generateUniqueName(
    const SgNode *node,
    bool ignoreDifferenceBetweenDefiningAndNondefiningDeclarations);

/** Generate a name like __temp#__ that is unique in the current scope and any
 * parent and children scopes. # is a unique integer counter.
 * @param baseName the word to be included in the variable names. */
std::string generateUniqueVariableName(SgScopeStatement *scope,
                                       std::string baseName = "temp");

// DQ (8/10/2010): Added const to first parameter.
// DQ (3/10/2007):
//! Generate a unique string from the source file position information
std::string
declarationPositionString(const SgDeclarationStatement *declaration);

// DQ (1/20/2007):
//! Added mechanism to generate project name from list of file names
ROSE_DLL_API std::string generateProjectName(const SgProject *project,
                                             bool supressSuffix = false);

//! Given a SgExpression that represents a named function (or bound member
//! function), return the mentioned function
SgFunctionDeclaration *getDeclarationOfNamedFunction(SgExpression *func);

//! Get the mask expression from the header of a SgForAllStatement
SgExpression *forallMaskExpression(SgForAllStatement *stmt);

//! Find all SgPntrArrRefExp under astNode, then add SgVarRefExp (if any) of
//! SgPntrArrRefExp's dim_info into NodeList_t
void addVarRefExpFromArrayDimInfo(SgNode *astNode,
                                  Rose_STL_Container<SgNode *> &NodeList_t);

// DQ (10/6/2006): Added support for faster mangled name generation (caching
// avoids recomputation).
/*! \brief Support for faster mangled name generation (caching avoids
   recomputation).

 */
#ifndef SWIG
// DQ (3/10/2013): This appears to be a problem for the SWIG interface
// (undefined reference at link-time).
void clearMangledNameCache(SgGlobal *globalScope);
void resetMangledNameCache(SgGlobal *globalScope);
#endif

std::string getMangledNameFromCache(SgNode *astNode);
std::string addMangledNameToCache(SgNode *astNode,
                                  const std::string &mangledName);

SgDeclarationStatement *getNonInstantiatonDeclarationForClass(
    SgTemplateInstantiationMemberFunctionDecl *memberFunctionInstantiation);

//! a better version for
//! SgVariableDeclaration::set_baseTypeDefininingDeclaration(), handling all
//! side effects automatically Used to have a struct declaration embedded into a
//! variable declaration
void setBaseTypeDefiningDeclaration(SgVariableDeclaration *var_decl,
                                    SgDeclarationStatement *base_decl);

// DQ (10/14/2006): This function tests the AST to see if for a non-defining
// declaration, the bool declarationPreceedsDefinition ( SgClassDeclaration*
// classNonDefiningDeclaration, SgClassDeclaration* classDefiningDeclaration );
//! Check if a defining declaration comes before of after the non-defining
//! declaration.
bool declarationPreceedsDefinition(
    SgDeclarationStatement *nonDefiningDeclaration,
    SgDeclarationStatement *definingDeclaration);

// DQ (10/19/2006): Function calls have interesting context dependent rules to
// determine if they are output with a global qualifier or not.  Were this is
// true we have to avoid global qualifiers, since the function's scope has not
// been defined.  This is an example of where qualification of function names in
// function calls are context dependent; an interesting example of where the C++
// language is not friendly to source-to-source processing :-).
bool functionCallExpressionPreceedsDeclarationWhichAssociatesScope(
    SgFunctionCallExp *functionCall);

/*! \brief Compute the intersection set for two ASTs.

    This is part of a test done by the copy function to compute those IR nodes
   in the copy that still reference the original AST.
 */
ROSE_DLL_API std::vector<SgNode *>
astIntersection(SgNode *original, SgNode *copy, SgCopyHelp *help = NULL);

//! Deep copy an arbitrary subtree
ROSE_DLL_API SgNode *deepCopyNode(const SgNode *subtree);

//! Deep copy a frontend-owned semantic subtree while preserving its exact
//! source-provenance classification.  This is distinct from deepCopyNode(),
//! which creates a new transformation output surface.
ROSE_DLL_API SgNode *deepCopySemanticSubtree(const SgNode *subtree);

//! Deep copy an arbitrary subtree and return the copy transaction's exact
//! original-to-copy identity map.  Consumers must use this map instead of
//! reconstructing correspondence from traversal order or spelling.
ROSE_DLL_API SgNode *
deepCopyNodeWithIdentityMap(const SgNode *subtree,
                            SgCopyHelp::copiedNodeMapType &identityMap);

//! A template function for deep copying a subtree. It is also  used to create
//! deepcopy functions with specialized parameter and return types. e.g
//! SgExpression* copyExpression(SgExpression* e);
template <typename NodeType> NodeType *deepCopy(const NodeType *subtree) {
  return dynamic_cast<NodeType *>(deepCopyNode(subtree));
}

//! Typed frontend-semantic counterpart to deepCopy().
template <typename NodeType>
NodeType *deepCopySemantic(const NodeType *subtree) {
  return dynamic_cast<NodeType *>(deepCopySemanticSubtree(subtree));
}

//! Deep copy an expression
ROSE_DLL_API SgExpression *copyExpression(SgExpression *e);

//! Deep copy a statement
ROSE_DLL_API SgStatement *copyStatement(SgStatement *s);

// from VarSym.cc in src/midend/astOutlining/src/ASTtools
//! Get the variable symbol for the first initialized name of a declaration
//! stmt.
ROSE_DLL_API SgVariableSymbol *getFirstVarSym(SgVariableDeclaration *decl);

//! Get the first initialized name of a declaration statement
ROSE_DLL_API SgInitializedName *
getFirstInitializedName(SgVariableDeclaration *decl);

//! A special purpose statement removal function, originally from
//! inlinerSupport.h, Need Jeremiah's attention to refine it. Please don't use
//! it for now.
ROSE_DLL_API void myRemoveStatement(SgStatement *stmt);

//! Check if a bool or int constant expression evaluates to be a true value
ROSE_DLL_API bool isConstantTrue(SgExpression *e);

//! Check if a bool or int constant expression evaluates to be a false value
ROSE_DLL_API bool isConstantFalse(SgExpression *e);

ROSE_DLL_API bool isCallToParticularFunction(SgFunctionDeclaration *decl,
                                             SgExpression *e);
ROSE_DLL_API bool isCallToParticularFunction(const std::string &qualifiedName,
                                             size_t arity, SgExpression *e);

//! Check if a declaration has a "static' modifier
bool ROSE_DLL_API isStatic(SgDeclarationStatement *stmt);

//! Set a declaration as static
ROSE_DLL_API void setStatic(SgDeclarationStatement *stmt);

//! Check if a declaration has an "extern" modifier
ROSE_DLL_API bool isExtern(SgDeclarationStatement *stmt);

//! Set a declaration as extern
ROSE_DLL_API void setExtern(SgDeclarationStatement *stmt);

//! True if an SgInitializedName is "mutable' (has storage modifier set)
bool ROSE_DLL_API isMutable(SgInitializedName *name);

//! Get a vector of input parameters from the function parameter list
std::vector<SgInitializedName *>
getInParameters(const SgInitializedNamePtrList &params);

//! Get a vector of output parameters from the function parameter list
std::vector<SgInitializedName *>
getOutParameters(const SgInitializedNamePtrList &params);

//! Interface for creating a statement whose computation writes its answer into
//! a given variable.
class StatementGenerator {
public:
  virtual ~StatementGenerator() {};
  virtual SgStatement *generate(SgExpression *where_to_write_answer) = 0;
  virtual void finalizeGeneratedStatement(SgStatement *) {}
};

//! Check if a SgNode _s is an assignment statement (any of =,+=,-=,&=,/=, ^=,
//! etc)
//!
//! Return the left hand, right hand expressions and if the left hand variable
//! is also being read
bool isAssignmentStatement(SgNode *_s, SgExpression **lhs = NULL,
                           SgExpression **rhs = NULL, bool *readlhs = NULL);

//! Variable references can be introduced by SgVarRef, SgPntrArrRefExp,
//! SgInitializedName, SgMemberFunctionRef etc. For Dot and Arrow Expressions,
//! their lhs is used to obtain SgInitializedName (coarse grain) by default.
//! Otherwise, fine-grain rhs is used.
ROSE_DLL_API SgInitializedName *
convertRefToInitializedName(SgNode *current, bool coarseGrain = true);

//! Validate and return the exact initialized name denoted by a resolved
//! variable-template nonreal reference.  This is a hard AST contract: the
//! synthetic source-spelling node, the real specialization, its source
//! template, its symbol, and both copies of the written arguments must agree.
ROSE_DLL_API SgInitializedName *
requireResolvedVariableTemplateReference(const SgNonrealRefExp *reference,
                                         const char *context);

//! Validate and return the exact callable denoted by a resolved function-
//! template nonreal reference.  This is a hard AST contract: the synthetic
//! source-spelling node, the real callable declaration, its canonical symbol,
//! its function type, and both copies of the written arguments must agree.
ROSE_DLL_API SgFunctionDeclaration *
requireResolvedFunctionTemplateReference(const SgNonrealRefExp *reference,
                                         const char *context);

//! Return the canonical source variable template reached from a specialization
//! after validating the complete, acyclic specialized-template chain.
ROSE_DLL_API SgTemplateVariableDeclaration *
requireCanonicalVariableTemplatePrimary(
    SgTemplateVariableDeclaration *specialization, const char *context);

//! Obtain the first queryed statement at line of a source file
ROSE_DLL_API SgStatement *getFirstStatementAtLine(SgSourceFile *sourceFile,
                                                  int line);

//! Obtain all the queryed statement at line of a source file
ROSE_DLL_API void getAllStatementsAtLine(SgSourceFile *sourceFile, int line,
                                         SgStatementPtrList &returnList);

//! Dump information about a SgNode for debugging
ROSE_DLL_API void dumpInfo(SgNode *node, std::string desc = "");

//! Reorder a list of declaration statements based on their appearance order in
//! source files
ROSE_DLL_API std::vector<SgDeclarationStatement *>
sortSgNodeListBasedOnAppearanceOrderInSource(
    const std::vector<SgDeclarationStatement *> &nodevec);

// DQ (4/13/2013): We need these to support the unparing of operators defined by
// operator syntax or member function names.
//! Is an overloaded operator a prefix operator (e.g. address operator X *
//! operator&(), dereference operator X & operator*(), unary plus operator X &
//! operator+(), etc.
// bool isPrefixOperator( const SgMemberFunctionRefExp* memberFunctionRefExp );
bool isPrefixOperator(SgExpression *exp);

//! Check for proper names of possible prefix operators (used in
//! isPrefixOperator()).
bool isPrefixOperatorName(const SgName &functionName);

//! Is an overloaded operator a postfix operator. (e.g. ).
bool isPostfixOperator(SgExpression *exp);

//! Is an overloaded operator an index operator (also referred to as call or
//! subscript operators). (e.g. X & operator()() or X & operator[]()).
bool isIndexOperator(SgExpression *exp);

// DQ (1/10/2014): Adding more general support for token based unparsing.
//! Used to support token unparsing (when the output the trailing token
//! sequence).
SgStatement *lastStatementOfScopeWithTokenInfo(
    SgScopeStatement *scope,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
        &tokenStreamSequenceMap);

// DQ (8/12/2020): Check the access permissions of all defining and nodefining
// declarations.
void checkAccessPermissions(SgNode *);

// DQ (8/14/2020): Check the symbol tables for specific scopes (debugging
// support).
void checkSymbolTables(SgNode *);

// Mark one node or a subtree as a transformation output surface.  A typed,
// compiler-synthesized implicit conversion remains a semantic wrapper while
// its operand is marked normally.  Mixed typed/provenance roles are rejected.
void markSubtreeToBeUnparsed(SgNode *root, int physical_file_id);
void markNodeToBeUnparsed(SgNode *node, int physical_file_id);

// DQ (7/12/2021): Debugging code to locate specific node marked as a
// transforamtion in the AST. Debugging the outliner.
bool findFirstSgCastExpMarkedAsTransformation(SgNode *n, const std::string &s);

//@}

//------------------------------------------------------------------------
//@{
/*! @name AST properties
  \brief version, language properties of current AST.
*/

// DQ (11/25/2020): Add support to set this as a specific language kind file
// (there is at least one language kind file processed by ROSE). The value of 0
// allows the old implementation to be tested, and the value of 1 allows the new
// optimized implementation to be tested. However to get all of the functions to
// be inlined, we have to recompile all of ROSE.
#define INLINE_OPTIMIZED_IS_LANGUAGE_KIND_FUNCTIONS 1

//  std::string version();  // utility_functions.h, version number
/*! Brief These traverse the memory pool of SgFile IR nodes and determine what
 * languages are in use!
 */
#if INLINE_OPTIMIZED_IS_LANGUAGE_KIND_FUNCTIONS
ROSE_DLL_API inline bool is_C_language() { return Rose::is_C_language; }
ROSE_DLL_API inline bool is_OpenMP_language() {
  return Rose::is_OpenMP_language;
}
ROSE_DLL_API inline bool is_C99_language() { return Rose::is_C99_language; }
ROSE_DLL_API inline bool is_Cxx_language() { return Rose::is_Cxx_language; }
ROSE_DLL_API inline bool is_Fortran_language() {
  return Rose::is_Fortran_language;
}
ROSE_DLL_API inline bool is_CAF_language() { return Rose::is_CAF_language; }
ROSE_DLL_API inline bool is_Cuda_language() { return Rose::is_Cuda_language; }
ROSE_DLL_API inline bool is_OpenCL_language() {
  return Rose::is_OpenCL_language;
}
#else
ROSE_DLL_API bool is_C_language();
ROSE_DLL_API bool is_OpenMP_language();
ROSE_DLL_API bool is_C99_language();
ROSE_DLL_API bool is_Cxx_language();
ROSE_DLL_API bool is_Fortran_language();
ROSE_DLL_API bool is_CAF_language();
ROSE_DLL_API bool is_Cuda_language();
ROSE_DLL_API bool is_OpenCL_language();
#endif

// CUDA translation units use the C++ object and expression semantics while
// retaining a distinct language identity for CUDA-specific frontend paths.
ROSE_DLL_API inline bool is_Cxx_family_language() {
  return is_Cxx_language() || is_Cuda_language();
}

ROSE_DLL_API bool is_mixed_C_and_Cxx_language();
ROSE_DLL_API bool is_mixed_Fortran_and_C_language();
ROSE_DLL_API bool is_mixed_Fortran_and_Cxx_language();
ROSE_DLL_API bool is_mixed_Fortran_and_C_and_Cxx_language();

ROSE_DLL_API bool is_language_case_insensitive();
ROSE_DLL_API bool language_may_contain_nondeclarations_in_scope();
ROSE_DLL_API void ensureCaseInsensitiveSymbolTable(SgScopeStatement *scope,
                                                   bool force_case_insensitive);
ROSE_DLL_API void transferSymbols(SgScopeStatement *from_scope,
                                  SgScopeStatement *to_scope,
                                  bool skip_label_symbols = true);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Scope
  \brief
*/

// DQ (10/5/2006): Added support for faster (non-quadratic) computation of
// unique labels for scopes in a function (as required for name mangling).
/*! \brief Assigns unique numbers to each SgScopeStatement of a function.

    This is used to provide unique names for variables and types defined is
    different nested scopes of a function (used in mangled name generation).
 */
void resetScopeNumbers(SgFunctionDefinition *functionDeclaration);

// DQ (10/5/2006): Added support for faster (non-quadratic) computation of
// unique labels for scopes in a function (as required for name mangling).
/*! \brief Clears the cache of scope,integer pairs for the input function.

    This is used to clear the cache of computed unique labels for scopes in a
   function. This function should be called after any transformation on a
   function that might effect the allocation of scopes and cause the existing
   unique numbers to be incorrect. This is part of support to provide unique
   names for variables and types defined is different nested scopes of a
   function (used in mangled name generation).
 */
void clearScopeNumbers(SgFunctionDefinition *functionDefinition);

//! Find the enclosing namespace of a declaration
SgNamespaceDefinitionStatement *
enclosingNamespaceScope(SgDeclarationStatement *declaration);
//  SgNamespaceDefinitionStatement * getEnclosingNamespaceScope (SgNode * node);

bool isPrototypeInScope(SgScopeStatement *scope,
                        SgFunctionDeclaration *functionDeclaration,
                        SgDeclarationStatement *startingAtDeclaration);

//! check if node1 is a strict ancestor of node 2. (a node is not considered its
//! own ancestor)
bool ROSE_DLL_API isAncestor(SgNode *node1, SgNode *node2);
//@}
//------------------------------------------------------------------------
//@{
/*! @name Preprocessing Information
  \brief #if-#else-#end, comments, #include, etc
*/

//! Dumps a located node's preprocessing information.
void dumpPreprocInfo(SgLocatedNode *locatedNode);

//! Insert  #include "filename" or #include <filename> (system header) onto the
//! global scope of a source file, add to be the last #include .. by default
//! among existing headers, Or as the first header. Recommended for use.
ROSE_DLL_API PreprocessingInfo *
insertHeader(SgSourceFile *source_file, const std::string &header_file_name,
             bool isSystemHeader, bool asLastHeader);

//! Insert  #include "filename" or #include <filename> (system header) onto the
//! global scope of a source file
ROSE_DLL_API PreprocessingInfo *
insertHeader(SgSourceFile *source_file, const std::string &header_file_name,
             bool isSystemHeader = false,
             PreprocessingInfo::RelativePositionType position =
                 PreprocessingInfo::before);

//! Insert  #include "filename" or #include <filename> (system header) into the
//! global scope containing the current scope, right after other #include XXX.
ROSE_DLL_API PreprocessingInfo *
insertHeader(const std::string &filename,
             PreprocessingInfo::RelativePositionType position,
             bool isSystemHeader, SgScopeStatement *scope);

//! Move preprocessing information of stmt_src to stmt_dst, Only move
//! preprocessing information from the specified source-relative position to a
//! specified target position, otherwise move all preprocessing information with
//! position information intact. The preprocessing information is appended to
//! the existing preprocessing information list of the target node by default.
//! Prepending is used if usePreprend is set to true. Optionally, the relative
//! position can be adjust after the moving using dst_position.
ROSE_DLL_API void
movePreprocessingInfo(SgStatement *stmt_src, SgStatement *stmt_dst,
                      PreprocessingInfo::RelativePositionType src_position =
                          PreprocessingInfo::undef,
                      PreprocessingInfo::RelativePositionType dst_position =
                          PreprocessingInfo::undef,
                      bool usePrepend = false);

//! Cut preprocessing information from a source node and save it into a buffer.
//! Used in combination of pastePreprocessingInfo(). The cut-paste operation is
//! equivalent to a split movePreprocessingInfo() operation and permits the
//! destination node to be unknown during the cut operation.
ROSE_DLL_API void
cutPreprocessingInfo(SgLocatedNode *src_node,
                     PreprocessingInfo::RelativePositionType pos,
                     AttachedPreprocessingInfoType &save_buf);

//! Paste preprocessing information from a buffer to a destination node. Used in
//! combination of cutPreprocessingInfo()
ROSE_DLL_API void
pastePreprocessingInfo(SgLocatedNode *dst_node,
                       PreprocessingInfo::RelativePositionType pos,
                       AttachedPreprocessingInfoType &saved_buf);

//@}

//! Publish a generated preprocessing record with one exact physical output
//! owner. The record remains typed as a transformation; its Sg_File_Info keeps
//! logical spelling provenance and stores physical output identity separately.
ROSE_DLL_API void publishGeneratedPreprocessingInfo(PreprocessingInfo *record,
                                                    SgLocatedNode *exactOwner);

//! Publish a generated trailing comment whose explicit attachment is after the
//! owner's syntax on the owner's current output line.
ROSE_DLL_API void publishGeneratedTrailingComment(PreprocessingInfo *record,
                                                  SgLocatedNode *exactOwner);

//! Relocate an already-published preprocessing record whose typed output
//! placement is the attached AST boundary. The record must identify priorOwner
//! exactly; detached, shared, source-position-owned, or differently owned
//! records are hard errors. Source/generated spelling provenance is immutable.
ROSE_DLL_API void
relocateAttachedPreprocessingInfoPhysicalOutputOwner(PreprocessingInfo *record,
                                                     SgLocatedNode *priorOwner,
                                                     SgLocatedNode *exactOwner);

//! Publish an existing preprocessing record at a new exact physical output
//! owner. Source-spelled records retain their logical spelling and semantic
//! classification while their physical output identity changes; generated
//! records are validated by publishGeneratedPreprocessingInfo().
ROSE_DLL_API void
publishPreprocessingInfoPhysicalOutputOwner(PreprocessingInfo *record,
                                            SgLocatedNode *exactOwner);

//! Build and attach a comment with an explicit lexical comment style.
ROSE_DLL_API PreprocessingInfo *
attachComment(SgLocatedNode *target, const std::string &content,
              PreprocessingInfo::DirectiveType commentStyle,
              PreprocessingInfo::RelativePositionType position =
                  PreprocessingInfo::before);

/* \brief move inner danglling #endif .. #if | #ifdef| #ifndef to be after lnode
    This is needed when we remove a target statement with internal statements.
    Some of the internal statements may have a dangling #endif  #if, #ifdef
   #ifndef. We need to move them to be attached to after position of lnode. Then
   we can safely remove or replace lnode (often a statement)
*/
ROSE_DLL_API int moveUpInnerDanglingIfEndifDirective(SgLocatedNode *lnode);

/* \brief scanning subtree from lnode, find and erase any NULL PreprocessingInfo
   pointers The unparser expects PreprocessingInfo pointers are not NULL. We may
   introduce NULL pointers after moving some preprocessing info. from one place
   to another.
*/

ROSE_DLL_API int eraseNullPreprocessingInfo(SgLocatedNode *lnode);

/*  \brief For each comment, we store its container, idx within the container,
         and depth level of the located node within AST from a selected root
*/
struct PreprocessingInfoData {
  AttachedPreprocessingInfoType *container; // the associated container
  int index; // idx of the comment within the container
  int depth; // starting from 0 : the root node of the selected root of the
             // sub-tree
};

/* \brief Recursively walk a subtree rooted at current node, extract
   PreprocessingInfo pointers to a list The list preserves the orginal order in
   which each preprocessing info shows up in the source code. This function is
   needed since naive walking of a subtree may generate out-of-order list of
   preprocessing info. We have to consider collecting the before, inside
   locations first, and the after location last using a recursion function.
*/

ROSE_DLL_API void preOrderCollectPreprocessingInfo(
    SgNode *current, std::vector<PreprocessingInfo *> &infoList, int depth);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Source File Position
  \brief set Sg_File_Info for a SgNode
*/

// ************************************************************************
//              Newer versions of now depricated functions
// ************************************************************************

// DQ (5/1/2012): This function queries the
// SageBuilder::SourcePositionClassification mode (stored in the SageBuilder
// interface) and used the specified mode to initialize the source position data
// (Sg_File_Info objects).  This function is the only function that should be
// called directly (though in a namespace we can't define permissions).
//! Set the source code positon for the current (input) node.
ROSE_DLL_API void setSourcePosition(SgNode *node);

// A better name might be "setSourcePositionForSubTree"
//! Set the source code positon for the subtree (including the root).
ROSE_DLL_API void setSourcePositionAtRootAndAllChildren(SgNode *root);

//! DQ (5/1/2012): New function with improved name.
void setSourcePositionAsTransformation(SgNode *node);

//! Ensure a located node has the file-info objects required before marking it
//! as a transformation/output subtree.
ROSE_DLL_API void
ensureLocatedNodeFileInfoForTransformation(SgLocatedNode *locatedNode);

// ************************************************************************

// ************************************************************************
//                  Older deprecated functions
// ************************************************************************
// Liao, 1/8/2007, set file info. for a whole subtree as transformation
// generated
//! Set current node's source position as transformation generated
ROSE_DLL_API void setOneSourcePositionForTransformation(SgNode *node);

//! Set current node's source position as NULL
ROSE_DLL_API void setOneSourcePositionNull(SgNode *node);

//! Recursively set source position info(Sg_File_Info) as transformation
//! generated
ROSE_DLL_API void setSourcePositionForTransformation(SgNode *root);

//! Set source position info(Sg_File_Info) as transformation generated for all
//! SgNodes in memory pool
//  ROSE_DLL_API void setSourcePositionForTransformation_memoryPool();

//! Check if a node is from a system header file
ROSE_DLL_API bool insideSystemHeader(SgLocatedNode *node);

// DQ (2/27/2021): Adding support to detect if a SgLocatedNode is located in a
// header file.
//! Check if a node is from a header file
ROSE_DLL_API bool insideHeader(SgLocatedNode *node);

//! Set the source position of SgLocatedNode to
//! Sg_File_Info::generateDefaultFileInfo(). These nodes WILL be unparsed. Not
//! for transformation usage.
// ROSE_DLL_API void setSourcePosition (SgLocatedNode * locatedNode);
// ************************************************************************

//@}

//------------------------------------------------------------------------
//@{
/*! @name Data types
  \brief
*/

//! Get the string representing the type name
ROSE_DLL_API std::string getTypeName(SgType *type);

//! Check if a type (or any nested type) is unknown/incomplete.
ROSE_DLL_API bool containsUnknownType(SgType *type);

//! Get the right bool type according to C or C++ language input
ROSE_DLL_API SgType *getBoolType(SgNode *n);

//! Check if a type is an integral type, only allowing signed/unsigned short,
//! int, long, long long.
////!
////! There is another similar function named SgType::isIntegerType(), which
/// allows additional types char, wchar, and bool to be treated as integer types
ROSE_DLL_API bool isStrictIntegerType(SgType *t);

//! Apply the C/C++ usual arithmetic conversions using the target ABI owned by
//! context. Invalid, non-arithmetic, or detached inputs are hard errors.
ROSE_DLL_API SgType *usualArithmeticConversionType(SgType *lhs, SgType *rhs,
                                                   const SgNode *context);
//! Get the data type of the first initialized name of a declaration statement
ROSE_DLL_API SgType *getFirstVarType(SgVariableDeclaration *decl);

//! Is a type default constructible?  This may not quite work properly.
ROSE_DLL_API bool isDefaultConstructible(SgType *type);

//! Is a type copy constructible?  This may not quite work properly.
ROSE_DLL_API bool isCopyConstructible(SgType *type);

//! Is a type assignable?  This may not quite work properly.
ROSE_DLL_API bool isAssignable(SgType *type);

#ifndef ROSE_USE_INTERNAL_FRONTEND_DEVELOPMENT
//! Check if a class type is a pure virtual class. True means that there is at
//! least one pure virtual function that has not been overridden. In the case of
//! an incomplete class type (forward declaration), this function returns false.
ROSE_DLL_API bool
isPureVirtualClass(SgType *type, const ClassHierarchyWrapper &classHierarchy);
#endif

//! Does a type have a trivial (built-in) destructor?
ROSE_DLL_API bool hasTrivialDestructor(SgType *t);

//! Is this type a non-constant reference type? (Handles typedefs correctly)
ROSE_DLL_API bool isNonconstReference(SgType *t);

//! Is this type a const or non-const reference type? (Handles typedefs
//! correctly)
ROSE_DLL_API bool isReferenceType(SgType *t);

//! Is this type a pointer type? (Handles typedefs correctly)
ROSE_DLL_API bool isPointerType(SgType *t);

//! Is this a pointer to a non-const type? Note that this function will return
//! true for const pointers pointing to non-const types. For example, (int*
//! const y) points to a modifiable int, so this function returns true.
//! Meanwhile, it returns false for (int const * x) and (int const * const x)
//! because these types point to a const int. Also, only the outer layer of
//! nested pointers is unwrapped. So the function returns true for (const int **
//! y), but returns false for const (int * const * x)
ROSE_DLL_API bool isPointerToNonConstType(SgType *type);

//! Is this a const type?
/* const char* p = "aa"; is not treated as having a const type. It is a pointer
to const char.
 * Similarly, neither for const int b[10]; or const int & c =10;
 * The standard says, "A compound type is not cv-qualified by the cv-qualifiers
(if any) of the types from which it is compounded. Any cv-qualifiers applied to
an array type affect the array element type, not the array type".
 */
ROSE_DLL_API bool isConstType(SgType *t);

//! Remove const (if present) from a type.  stripType() cannot do this because
//! it removes all modifiers.
SgType *removeConst(SgType *t);

//! Is this a volatile type?
ROSE_DLL_API bool isVolatileType(SgType *t);

//! Is this a restrict type?
ROSE_DLL_API bool isRestrictType(SgType *t);

//! Is this a scalar type?
/*! We define the following SgType as scalar types: char, short, int, long ,
 * void, Wchar, Float, double, long long, string, bool, complex, imaginary
 */
ROSE_DLL_API bool isScalarType(SgType *t);

//! Check if a type is an integral type, only allowing signed/unsigned short,
//! int, long, long long.
//!
//! There is another similar function named SgType::isIntegerType(), which
//! allows additional types char, wchar, and bool.
ROSE_DLL_API bool isStrictIntegerType(SgType *t);

//! Check if a type is a struct type (a special SgClassType in ROSE). Typedef
//! and modifier types are not stripped off. Only direct struct type is returned
//! as true.
ROSE_DLL_API bool isStructType(SgType *t);

//! Generate a mangled string for a given type based on Itanium C++ ABI
ROSE_DLL_API std::string mangleType(SgType *type);

//! Generate mangled scalar type names according to Itanium C++ ABI, the input
//! type should pass isScalarType() in ROSE
ROSE_DLL_API std::string mangleScalarType(SgType *type);

//! Generated mangled modifier types, include const, volatile,according to
//! Itanium C++ ABI.
ROSE_DLL_API std::string mangleModifierType(SgModifierType *type);

//! Calculate the number of elements of an array type: dim1* dim2*... , assume
//! element count is 1 for int a[].
ROSE_DLL_API size_t getArrayElementCount(SgArrayType *t);

//! Get the number of dimensions of an array type
ROSE_DLL_API int getDimensionCount(SgType *t);

//! Get the element type of an array. It recursively find the base type for
//! multi-dimension array types
ROSE_DLL_API SgType *getArrayElementType(SgType *t);

//! Get the element type of an array, pointer or string, or NULL if not
//! applicable. This function only check one level base type. No recursion.
ROSE_DLL_API SgType *getElementType(SgType *t);

/// \brief  returns the array dimensions in an array as defined for arrtype
/// \param  arrtype the type of a C/C++ array
/// \return an array that contains an expression indicating each dimension's
/// size.
///         OWNERSHIP of the expressions is TRANSFERED TO the CALLER (which
///         becomes responsible for freeing the expressions).
///         Note, the first entry of the array is a SgNullExpression, iff the
///         first array dimension was not specified.
/// @code
///         int x[] = { 1, 2, 3 };
/// @endcode
///         note, the expression does not have to be a constant
/// @code
///         int x[i*5];
/// @endcode
/// \post   return-value.empty() == false
/// \post   return-value[*] != NULL (no nullptr in the returned vector)
std::vector<SgExpression *> get_C_array_dimensions(const SgArrayType &arrtype);

/// \brief  returns the array dimensions in an array as defined for arrtype
/// \param  arrtype the type of a C/C++ array
/// \param  varref  a reference to an array variable (the variable of type
/// arrtype)
/// \return an array that contains an expression indicating each dimension's
/// size.
///         OWNERSHIP of the expressions is TRANSFERED TO the CALLER (which
///         becomes responsible for freeing the expressions).
///         If the first array dimension was not specified an expression
///         that indicates that size is generated.
/// @code
///         int x[][3] = { 1, 2, 3, 4, 5, 6 };
/// @endcode
///         the entry for the first dimension will be:
/// @code
///         // 3 ... size of 2nd dimension
///         sizeof(x) / (sizeof(int) * 3)
/// @endcode
/// \pre    arrtype is the array-type of varref
/// \post   return-value.empty() == false
/// \post   return-value[*] != NULL (no nullptr in the returned vector)
/// \post   !isSgNullExpression(return-value[*])
std::vector<SgExpression *> get_C_array_dimensions(const SgArrayType &arrtype,
                                                   const SgVarRefExp &varref);

/// \overload
/// \note     see get_C_array_dimensions for SgVarRefExp for details.
/// \todo     make initname const
std::vector<SgExpression *> get_C_array_dimensions(const SgArrayType &arrtype,
                                                   SgInitializedName &initname);

//! Check if an expression is an array access (SgPntrArrRefExp). If so, return
//! its name expression and subscripts if requested. Users can use
//! convertRefToInitializedName() to get the possible name. It does not check if
//! the expression is a top level SgPntrArrRefExp.
ROSE_DLL_API bool
isArrayReference(SgExpression *ref, SgExpression **arrayNameExp = NULL,
                 std::vector<SgExpression *> **subscripts = NULL);

//! Collect variable references in array types. The default
//! NodeQuery::querySubTree() will miss variables referenced in array type's
//! index list. e.g. double *buffer = new double[numItems] ;
ROSE_DLL_API int collectVariableReferencesInArrayTypes(
    SgLocatedNode *root, Rose_STL_Container<SgNode *> &currentVarRefList);

//! Lookup a named type based on its name, bottomup searching from a specified
//! scope. Note name collison might be allowed for c (not C++) between typedef
//! and enum/struct. Only the first matched named type will be returned in this
//! case. typedef is returned as it is, not the base type it actually refers to.
ROSE_DLL_API SgType *
lookupNamedTypeInParentScopes(const std::string &type_name,
                              SgScopeStatement *scope = NULL);

//! Return an existing named type or terminate if the current AST has no such
//! declaration. This never synthesizes a placeholder declaration.
ROSE_DLL_API SgType *
requireNamedTypeInParentScopes(const std::string &type_name,
                               SgScopeStatement *scope);

//! Returns true when a declarator type names the supplied exact source-owned
//! class or enum declaration. Externally named tag types are shared across a
//! project, so the type's canonical declaration family may belong to another
//! translation unit while the supplied declaration remains the sole owner of
//! its local source surface.
ROSE_DLL_API bool
isExactTagTypeIdentity(SgType *declaratorType,
                       SgDeclarationStatement *sourceOwnedTag);

// DQ (7/22/2014): Added support for comparing expression types in actual
// arguments with those expected from the formal function parameter types.
//! Get the type of the associated argument expression from the function type.
ROSE_DLL_API SgType *
getAssociatedTypeFromFunctionTypeList(SgExpression *actual_argument_expression);

//! Verify that 2 SgTemplateArgument are equivalent (same type, same expression,
//! or same template declaration)
ROSE_DLL_API bool templateArgumentEquivalence(SgTemplateArgument *arg1,
                                              SgTemplateArgument *arg2);

//! Verify that 2 SgTemplateArgumentPtrList are equivalent.
ROSE_DLL_API bool
templateArgumentListEquivalence(const SgTemplateArgumentPtrList &list1,
                                const SgTemplateArgumentPtrList &list2);

//! Verify that two template parameters describe the same parameter identity.
ROSE_DLL_API bool templateParameterEquivalence(SgTemplateParameter *parameter1,
                                               SgTemplateParameter *parameter2);

//! Verify that two template parameter lists describe the same signature.
ROSE_DLL_API bool
templateParameterListEquivalence(const SgTemplateParameterPtrList &list1,
                                 const SgTemplateParameterPtrList &list2);

//! Test for equivalence of types independent of access permissions (private or
//! protected modes for members of classes).
ROSE_DLL_API bool isEquivalentType(const SgType *lhs, const SgType *rhs);

//! Verify that a distinct C++ TypeLoc-owned source type resolves to its exact
//! canonical semantic type. This includes written template-id graphs whose
//! SgNonrealType identity is intentionally distinct from the resolved class or
//! alias-template instantiation.
ROSE_DLL_API bool cxxSourceTypeMatchesSemanticType(const SgType *source,
                                                   const SgType *semantic);

//! Verify that the exact type of a written C++ non-type template argument can
//! undergo the standard conversion represented by its canonical parameter
//! type.  This is deliberately narrower than general implicit conversion:
//! it covers top-level cv removal, qualification adjustment, array/function
//! decay, reference binding, and null pointer conversion without treating
//! unrelated but similarly spelled types as equivalent.
ROSE_DLL_API bool
cxxNonTypeTemplateArgumentTypeConversionIsExact(const SgType *source,
                                                const SgType *parameter);

//! Verify that a list of written C++ template arguments is the exact explicit
//! prefix of a resolved semantic argument list. The semantic list may contain
//! a defaulted suffix and may canonicalize source type spelling.
ROSE_DLL_API bool cxxSourceTemplateArgumentPrefixMatchesSemantic(
    const SgTemplateArgumentPtrList &source,
    const SgTemplateArgumentPtrList &semantic);

//! Verify that an exact Fortran source scalar type resolves to the canonical
//! semantic scalar type without discarding explicit KIND/LEN selectors.
ROSE_DLL_API bool fortranSourceTypeMatchesSemanticType(const SgType *source,
                                                       const SgType *semantic);

//! Verify that an exact Fortran source type resolves to a semantic expression
//! result type.  A nonconstant source CHARACTER LEN selector may match only
//! the explicit dynamic-result marker used by semantic expression types.
ROSE_DLL_API bool
fortranSourceTypeMatchesSemanticExpressionType(const SgType *source,
                                               const SgType *semantic);

//! Verify that a Fortran source-syntax function result has the same resolved
//! meaning as its canonical semantic result. Explicit KIND/LEN selectors use
//! typed folded metadata; only omitted selectors may match by intrinsic family.
ROSE_DLL_API bool fortranSourceFunctionResultMatchesSemanticResult(
    const SgFunctionType *source, const SgFunctionType *semantic);

//! Find the function type matching a function signature plus a given return
//! type
ROSE_DLL_API SgFunctionType *
findFunctionType(SgType *return_type, SgFunctionParameterTypeList *typeList);

//! Test if two types are equivalent SgFunctionType nodes. This is necessary for
//! template function types They may differ in one SgTemplateType pointer but
//! identical otherwise.
ROSE_DLL_API bool isEquivalentFunctionType(const SgFunctionType *lhs,
                                           const SgFunctionType *rhs);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Loop handling
  \brief
*/

// by Jeremiah
//! Add a step statement to the end of a loop body
//! Add a new label to the end of the loop, with the step statement after
//! it; then change all continue statements in the old loop body into
//! jumps to the label
//!
//! For example:
//! while (a < 5) {if (a < -3) continue;} (adding "a++" to end) becomes
//! while (a < 5) {if (a < -3) goto label; label: a++;}
ROSE_DLL_API void addStepToLoopBody(SgScopeStatement *loopStmt,
                                    SgStatement *step);

ROSE_DLL_API void moveForStatementIncrementIntoBody(SgForStatement *f);
ROSE_DLL_API void convertForToWhile(SgForStatement *f);
ROSE_DLL_API void convertAllForsToWhiles(SgNode *top);
//! Change continue statements in a given block of code to gotos to a label
ROSE_DLL_API void changeContinuesToGotos(SgStatement *stmt,
                                         SgLabelStatement *label);

//! Return the loop index variable for a for loop
ROSE_DLL_API SgInitializedName *getLoopIndexVariable(SgNode *loop);

//! Check if a SgInitializedName is used as a loop index within a AST subtree
//!  This function will use a bottom-up traverse starting from the subtree_root
//!  to find all enclosing loops and check if ivar is used as an index for
//!  either of them.
ROSE_DLL_API bool isLoopIndexVariable(SgInitializedName *ivar,
                                      SgNode *subtree_root);

//! Check if a for loop uses C99 style initialization statement with multiple
//! expressions like for (int i=0, j=0; ..) or for (i=0,j=0;...)
/*!
   for (int i=0, j=0; ..) is stored as two variable declarations under
   SgForInitStatement's init_stmt member for (i=0,j=0;...) is stored as a single
   expression statement, with comma expression (i=0,j=0).
*/
ROSE_DLL_API bool
hasMultipleInitStatmentsOrExpressions(SgForStatement *for_loop);

//! Routines to get and set the body of a loop
ROSE_DLL_API SgStatement *getLoopBody(SgScopeStatement *loop);

ROSE_DLL_API void setLoopBody(SgScopeStatement *loop, SgStatement *body);

//! Routines to get the condition of a loop. It recognize While-loop, For-loop,
//! and Do-While-loop
ROSE_DLL_API SgStatement *getLoopCondition(SgScopeStatement *loop);

//! Set the condition statement of a loop, including While-loop, For-loop, and
//! Do-While-loop.
ROSE_DLL_API void setLoopCondition(SgScopeStatement *loop, SgStatement *cond);

//! Check if a for-loop has a canonical form, return loop index, bounds, step,
//! and body if requested
//!
//! A canonical form is defined as : one initialization statement, a test
//! expression, and an increment expression , loop index variable should be of
//! an integer type.  IsInclusiveUpperBound is true when <= or >= is used for
//! loop condition
ROSE_DLL_API bool
isCanonicalForLoop(SgNode *loop, SgInitializedName **ivar = NULL,
                   SgExpression **lb = NULL, SgExpression **ub = NULL,
                   SgExpression **step = NULL, SgStatement **body = NULL,
                   bool *hasIncrementalIterationSpace = NULL,
                   bool *isInclusiveUpperBound = NULL);

class CheckedCanonicalLoopPlan;

//! Exact positive stride captured by a checked canonical-loop plan.
//!
//! The implicit ++/-- forms are a distinct value, never a null expression.
//! An explicit stride retains the exact validated expression edge and value.
class CheckedCanonicalLoopStride {
public:
  enum class Kind { implicit_unit, explicit_positive };

  CheckedCanonicalLoopStride() = delete;
  CheckedCanonicalLoopStride(const CheckedCanonicalLoopStride &) = default;
  CheckedCanonicalLoopStride(CheckedCanonicalLoopStride &&) = delete;
  CheckedCanonicalLoopStride &
  operator=(const CheckedCanonicalLoopStride &) = delete;
  CheckedCanonicalLoopStride &operator=(CheckedCanonicalLoopStride &&) = delete;

  Kind kind() const noexcept {
    return std::holds_alternative<ImplicitUnitTag>(storage_)
               ? Kind::implicit_unit
               : Kind::explicit_positive;
  }
  unsigned long long positiveValue() const noexcept { return positive_value_; }
  std::optional<std::reference_wrapper<SgExpression>>
  explicitExpression() const noexcept {
    if (const auto *expression =
            std::get_if<std::reference_wrapper<SgExpression>>(&storage_))
      return *expression;
    return std::nullopt;
  }

private:
  struct ImplicitUnitTag {};
  using Storage =
      std::variant<ImplicitUnitTag, std::reference_wrapper<SgExpression>>;

  explicit CheckedCanonicalLoopStride(ImplicitUnitTag) noexcept;
  CheckedCanonicalLoopStride(SgExpression &expression,
                             unsigned long long positive_value) noexcept;

  Storage storage_;
  unsigned long long positive_value_;

  friend class CheckedCanonicalLoopPlan;
};

//! Immutable, fully checked description of one transformable C/C++ loop.
//!
//! Construction validates exact reciprocal ownership for every header/body
//! edge, an acyclic structural owner chain, a side-effect-free and loop-
//! invariant bound/stride surface, a positive nonzero constant stride
//! magnitude, and the absence of induction-variable mutation or escape from
//! the body.  Every reference is a borrowed snapshot; transformation commits
//! revalidate the snapshot before changing the AST.  Only the checked factory
//! can construct a plan.
class CheckedCanonicalLoopPlan {
public:
  CheckedCanonicalLoopPlan() = delete;
  CheckedCanonicalLoopPlan(const CheckedCanonicalLoopPlan &) = default;
  CheckedCanonicalLoopPlan(CheckedCanonicalLoopPlan &&) = delete;
  CheckedCanonicalLoopPlan &
  operator=(const CheckedCanonicalLoopPlan &) = delete;
  CheckedCanonicalLoopPlan &operator=(CheckedCanonicalLoopPlan &&) = delete;

  SgForStatement &loop() const noexcept { return loop_.get(); }
  SgNode &structuralOwner() const noexcept { return structural_owner_.get(); }
  SgForInitStatement &forInit() const noexcept { return for_init_.get(); }
  SgStatement &initializer() const noexcept { return initializer_.get(); }
  SgStatement &testStatement() const noexcept { return test_statement_.get(); }
  SgExpression &testExpression() const noexcept {
    return test_expression_.get();
  }
  SgExpression &incrementExpression() const noexcept {
    return increment_expression_.get();
  }
  SgStatement &body() const noexcept { return body_.get(); }
  SgInitializedName &induction() const noexcept { return induction_.get(); }
  SgVariableSymbol &inductionSymbol() const noexcept {
    return induction_symbol_.get();
  }
  SgType &inductionType() const noexcept { return induction_type_.get(); }
  SgExpression &lowerBound() const noexcept { return lower_bound_.get(); }
  SgExpression &upperBound() const noexcept { return upper_bound_.get(); }
  const CheckedCanonicalLoopStride &stride() const noexcept { return stride_; }
  unsigned inductionWidth() const noexcept { return induction_width_; }
  bool inductionIsUnsigned() const noexcept { return induction_is_unsigned_; }
  bool isIncreasing() const noexcept { return increasing_; }
  bool hasInclusiveBound() const noexcept { return inclusive_; }
  bool initializerIsDeclaration() const noexcept {
    return initializer_is_declaration_;
  }

private:
  CheckedCanonicalLoopPlan(
      SgForStatement &loop, SgNode &structural_owner,
      SgForInitStatement &for_init, SgStatement &initializer,
      SgStatement &test_statement, SgExpression &test_expression,
      SgExpression &increment_expression, SgStatement &body,
      SgInitializedName &induction, SgVariableSymbol &induction_symbol,
      SgType &induction_type, SgExpression &lower_bound,
      SgExpression &upper_bound,
      std::optional<std::reference_wrapper<SgExpression>> explicit_stride,
      unsigned long long positive_stride_value, unsigned induction_width,
      bool induction_is_unsigned, bool increasing, bool inclusive,
      bool initializer_is_declaration) noexcept;

  std::reference_wrapper<SgForStatement> loop_;
  std::reference_wrapper<SgNode> structural_owner_;
  std::reference_wrapper<SgForInitStatement> for_init_;
  std::reference_wrapper<SgStatement> initializer_;
  std::reference_wrapper<SgStatement> test_statement_;
  std::reference_wrapper<SgExpression> test_expression_;
  std::reference_wrapper<SgExpression> increment_expression_;
  std::reference_wrapper<SgStatement> body_;
  std::reference_wrapper<SgInitializedName> induction_;
  std::reference_wrapper<SgVariableSymbol> induction_symbol_;
  std::reference_wrapper<SgType> induction_type_;
  std::reference_wrapper<SgExpression> lower_bound_;
  std::reference_wrapper<SgExpression> upper_bound_;
  CheckedCanonicalLoopStride stride_;
  unsigned induction_width_;
  bool induction_is_unsigned_;
  bool increasing_;
  bool inclusive_;
  bool initializer_is_declaration_;

  friend CheckedCanonicalLoopPlan
  requireCheckedCanonicalLoopPlan(SgForStatement *loop, const char *operation);
};

//! Build a checked loop plan without modifying or allocating into the AST.
ROSE_DLL_API CheckedCanonicalLoopPlan
requireCheckedCanonicalLoopPlan(SgForStatement *loop, const char *operation);

//! Return an exact constant trip count when both bounds are exact integer
//! constants in the induction type's domain and the terminal increment cannot
//! overflow or wrap.  Dynamic bounds, an unsafe terminal increment, and an
//! unrepresentable count return nullopt; a consumer requiring a constant count
//! must reject nullopt.
ROSE_DLL_API std::optional<unsigned long long>
exactCanonicalLoopTripCount(const CheckedCanonicalLoopPlan &plan);

class CheckedLoopUnrollPlan {
public:
  CheckedLoopUnrollPlan() = delete;
  CheckedLoopUnrollPlan(const CheckedLoopUnrollPlan &) = default;
  CheckedLoopUnrollPlan(CheckedLoopUnrollPlan &&) = delete;
  CheckedLoopUnrollPlan &operator=(const CheckedLoopUnrollPlan &) = delete;
  CheckedLoopUnrollPlan &operator=(CheckedLoopUnrollPlan &&) = delete;

  const CheckedCanonicalLoopPlan &loop() const noexcept { return loop_; }
  size_t factor() const noexcept { return factor_; }

private:
  CheckedLoopUnrollPlan(const CheckedCanonicalLoopPlan &loop,
                        size_t factor) noexcept
      : loop_(loop), factor_(factor) {}

  CheckedCanonicalLoopPlan loop_;
  size_t factor_;

  friend CheckedLoopUnrollPlan
  requireCheckedLoopUnrollPlan(SgForStatement *loop, size_t factor,
                               const char *operation);
};

//! Read-only unroll planning and one atomic commit.
ROSE_DLL_API CheckedLoopUnrollPlan
requireCheckedLoopUnrollPlan(SgForStatement *loop, size_t factor,
                             const char *operation = "loop-unrolling");
ROSE_DLL_API void commitLoopUnrolling(const CheckedLoopUnrollPlan &plan);

class CheckedLoopTilingPlan {
public:
  CheckedLoopTilingPlan() = delete;
  CheckedLoopTilingPlan(const CheckedLoopTilingPlan &) = default;
  CheckedLoopTilingPlan(CheckedLoopTilingPlan &&) = delete;
  CheckedLoopTilingPlan &operator=(const CheckedLoopTilingPlan &) = delete;
  CheckedLoopTilingPlan &operator=(CheckedLoopTilingPlan &&) = delete;

  SgForStatement &outerLoop() const noexcept { return outer_.get(); }
  const std::vector<CheckedCanonicalLoopPlan> &loops() const noexcept {
    return loops_;
  }
  const std::vector<size_t> &tileSizes() const noexcept { return tile_sizes_; }

private:
  CheckedLoopTilingPlan(SgForStatement &outer,
                        std::vector<CheckedCanonicalLoopPlan> loops,
                        std::vector<size_t> tile_sizes) noexcept
      : outer_(outer), loops_(std::move(loops)),
        tile_sizes_(std::move(tile_sizes)) {}

  std::reference_wrapper<SgForStatement> outer_;
  std::vector<CheckedCanonicalLoopPlan> loops_;
  std::vector<size_t> tile_sizes_;

  friend CheckedLoopTilingPlan
  requireCheckedLoopTilingPlan(SgForStatement *outer,
                               const std::vector<size_t> &tile_sizes,
                               const char *operation);
};

//! Validate a complete perfect nest and all tile sizes without mutation, then
//! commit the planned strip-mining transformations in place.
ROSE_DLL_API CheckedLoopTilingPlan requireCheckedLoopTilingPlan(
    SgForStatement *outer, const std::vector<size_t> &tile_sizes,
    const char *operation = "loop-tiling");
ROSE_DLL_API void commitLoopTiling(const CheckedLoopTilingPlan &plan);

enum class CanonicalFortranLoopDirection { increasing, decreasing, runtime };

//! Check if a Fortran Do loop has a complete canonical form: Do I=1, 10, 1.
//! A nonconstant step has runtime direction; it must never be guessed to be
//! increasing by consumers.
ROSE_DLL_API bool
isCanonicalDoLoop(SgFortranDo *loop, SgInitializedName **ivar /*=NULL*/,
                  SgExpression **lb /*=NULL*/, SgExpression **ub /*=NULL*/,
                  SgExpression **step /*=NULL*/, SgStatement **body /*=NULL*/,
                  CanonicalFortranLoopDirection *direction /*= NULL*/,
                  bool *isInclusiveUpperBound /*=NULL*/);

//! Set the lower bound of a loop header for (i=lb; ...)
ROSE_DLL_API void setLoopLowerBound(SgNode *loop, SgExpression *lb);

//! Set the upper bound of a loop header,regardless the condition expression
//! type.  for (i=lb; i op up, ...)
ROSE_DLL_API void setLoopUpperBound(SgNode *loop, SgExpression *ub);
ROSE_DLL_API void setCanonicalForLoopInclusiveComparison(SgForStatement *loop,
                                                         bool increasing);

//! Set the stride(step) of a loop 's incremental expression, regardless the
//! expression types (i+=s; i= i+s, etc)
ROSE_DLL_API void setLoopStride(SgNode *loop, SgExpression *stride);

//! Normalize loop init stmt by promoting the single variable declaration
//! statement outside of the for loop header's init statement, e.g. for (int
//! i=0;) becomes int i_x; for (i_x=0;..) and rewrite the loop with the new
//! index variable, if necessary
ROSE_DLL_API void normalizeForLoopInitDeclaration(SgForStatement *loop);
ROSE_DLL_API void retireForLoopInitNormalization(SgForStatement *loop);

//! Undo the normalization of for loop's C99 init declaration. Previous record
//! of normalization is used to ease the reverse transformation.
ROSE_DLL_API bool unnormalizeForLoopInitDeclaration(SgForStatement *loop);

/**
 * @brief Normalize the structure of `case` and `default` blocks within a switch
 * statement.
 *
 * This function examines the body of a given `SgSwitchStatement` and
 * restructures its `case` and `default` sections to ensure that the associated
 * statements are properly wrapped in basic blocks
 * (`SgBasicBlock`). This normalization is helpful for consistent transformation
 * and analysis of switch statements, especially in situations where multiple
 * statements follow a label or where no explicit block is present.
 *
 * The function performs the following actions:
 * - Iterates over all statements in the switch body.
 * - Identifies `SgCaseOptionStmt` and `SgDefaultOptionStmt`.
 * - If the labeled statement has no body and is immediately followed by a
 * single `SgBasicBlock`, no changes are made.
 * - Otherwise, all subsequent statements up to the next label are wrapped into
 * a new `SgBasicBlock` that is inserted immediately after the label.
 *
 * @param switchStmt Pointer to the switch statement (`SgSwitchStatement*`) to
 * be normalized. Must not be null. If the switch body is not a `SgBasicBlock`,
 * the function will return early.
 * @return true if normalization happens and AST has been changed.
 *         false if normalization does not happen and AST is intact.
 */
ROSE_DLL_API bool normalizeCaseAndDefaultBlocks(SgSwitchStatement *switchStmt);

//! Normalize a for loop. Malformed or non-canonicalizable loop headers are hard
//! errors detected by a read-only preflight before the AST is mutated.
//!
//! Translations are :
//!    For the init statement: for (int i=0;... ) becomes a declaration in the
//!           immediately enclosing lexical scope followed by for (i=0;..)
//!    The test's typed inclusive or exclusive comparison is preserved.
//!    For increment expression:
//!           i++ is normalized to i+=1 and
//!           i-- is normalized to i-=1.
//!           Canonical assignment increments are normalized to += or -= while
//!           preserving a positive stride magnitude.
ROSE_DLL_API void forLoopNormalization(SgForStatement *loop,
                                       bool foldConstant = true);

//! Validate a for loop's typed comparison without changing its inclusivity.
ROSE_DLL_API void normalizeForLoopTest(SgForStatement *loop);
ROSE_DLL_API void normalizeForLoopIncrement(SgForStatement *loop);

//! Normalize a Fortran Do loop. Make the default increment expression (1)
//! explicit
ROSE_DLL_API void doLoopNormalization(SgFortranDo *loop);

//! Unroll a target loop by grouping exact source iterations.  The checked
//! implementation never synthesizes range, endpoint, or stride products.
ROSE_DLL_API void loopUnrolling(SgForStatement *loop, size_t unrolling_factor);

//! Interchange/permutate a n-level perfectly-nested loop rooted at 'loop' using
//! a lexicographical order number within (0,depth!).
ROSE_DLL_API bool loopInterchange(SgForStatement *loop, size_t depth,
                                  size_t lexicoOrder);

//! Tile the n-level (starting from 1) loop of a perfectly nested loop nest
//! using tiling size s
ROSE_DLL_API void loopTiling(SgForStatement *loopNest, size_t targetLevel,
                             size_t tileSize);

//! Tile each leading level of one perfect loop nest in a single checked
//! transaction.  tileSizes[0] belongs to loopNest itself.
ROSE_DLL_API void loopTiling(SgForStatement *loopNest,
                             const std::vector<size_t> &tileSizes);

// Winnie Loop Collapsing
SgExprListExp *loopCollapsing(SgForStatement *target_loop,
                              size_t collapsing_factor,
                              SgStatement *setup_insertion_anchor = nullptr);

bool getForLoopInformations(SgForStatement *for_loop,
                            SgVariableSymbol *&iterator,
                            SgExpression *&lower_bound,
                            SgExpression *&upper_bound, SgExpression *&stride,
                            bool &has_incremental_iteration_space,
                            bool &has_inclusive_bound);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Topdown search
  \brief Top-down traversal from current node to find a node of a specified type
*/

//! Query a subtree to get all nodes of a given type, with an appropriate
//! downcast.
template <typename NodeType>
std::vector<NodeType *>
querySubTree(SgNode *top,
             VariantT variant = (VariantT)NodeType::static_variant) {

  Rose_STL_Container<SgNode *> nodes = NodeQuery::querySubTree(top, variant);
  std::vector<NodeType *> result(nodes.size(), NULL);
  int count = 0;

  for (Rose_STL_Container<SgNode *>::const_iterator i = nodes.begin();
       i != nodes.end(); ++i, ++count) {
    NodeType *node = dynamic_cast<NodeType *>(*i);
    ROSE_ASSERT(node);
    result[count] = node;
  }

  return result;
}
/*! \brief Returns STL vector of SgFile IR node pointers.

    Demonstrates use of restricted traversal over just SgFile IR nodes.
 */
std::vector<SgFile *> generateFileList();

/** Get the current SgProject IR Node.
 *
 *  The library should never have more than one project and it asserts such.  If
 * no project has been created yet then this function returns the null pointer.
 */
ROSE_DLL_API SgProject *getProject();

//! \return the project associated with a node
SgProject *getProject(const SgNode *node);

//! Query memory pools to grab SgNode of a specified type
template <typename NodeType>
static std::vector<NodeType *> getSgNodeListFromMemoryPool() {
  // This function uses a memory pool traversal specific to the SgFile IR nodes
  class MyTraversal : public ROSE_VisitTraversal {
  public:
    std::vector<NodeType *> resultlist;
    void visit(SgNode *node) {
      NodeType *result = dynamic_cast<NodeType *>(node);
      ROSE_ASSERT(result != NULL);
      if (result != NULL) {
        resultlist.push_back(result);
      }
    };
    virtual ~MyTraversal() {}
  };

  MyTraversal my_traversal;
  NodeType::traverseMemoryPoolNodes(my_traversal);
  return my_traversal.resultlist;
}

//! we have two serialize() functions, one for a single node, the other for a
//! list of pointers
void serialize(SgNode *node, std::string &prefix, bool hasRemaining,
               std::ostringstream &out, std::string &edgeLabel);

// A special node in the AST text dump
template <typename T>
void serialize_list(T &plist, std::string T_name, std::string &prefix,
                    bool hasRemaining, std::ostringstream &out,
                    std::string &edgeLabel) {
  out << prefix;
  out << (hasRemaining ? "|---" : "|___");

  //  out<<"+"<<edgeLabel<<"+>";
  out << " " << edgeLabel << " ->";
  // print address and type name
  // out<<"@"<<&plist<<" "<< typeid(T).name()<<" "; // mangled names are hard to
  // read
  out << "@" << &plist << " " << T_name << " ";

  out << std::endl;

  int last_non_null_child_idx = -1;
  for (int i = (int)(plist.size()) - 1; i >= 0; i--) {
    if (plist[i]) {
      last_non_null_child_idx = i;
      break;
    }
  }

  for (size_t i = 0; i < plist.size(); i++) {
    bool n_hasRemaining = false;
    if ((int)i < last_non_null_child_idx)
      n_hasRemaining = true;
    std::string suffix = hasRemaining ? "|   " : "    ";
    std::string n_prefix = prefix + suffix;
    std::string n_edge_label = "";
    if (plist[i])
      serialize(plist[i], n_prefix, n_hasRemaining, out, n_edge_label);
  }
}

/*! \brief top-down traversal from current node to find the main() function
 * declaration
 */
ROSE_DLL_API SgFunctionDeclaration *findMain(SgNode *currentNode);

//! Find the last declaration statement within a scope (if any). This is often
//! useful to decide where to insert another variable declaration statement.
//! Pragma declarations are not treated as a declaration by default in this
//! context.
SgStatement *findLastDeclarationStatement(SgScopeStatement *scope,
                                          bool includePragma = false);

// midend/programTransformation/partialRedundancyElimination/pre.h
//! Find referenced symbols within an expression
std::vector<SgVariableSymbol *> getSymbolsUsedInExpression(SgExpression *expr);

//! Find break statements inside a particular statement, stopping at nested
//! loops or switches
/*! loops or switch statements defines their own contexts for break
 statements.  The function will stop immediately if run on a loop or switch
 statement.  If fortranLabel is non-empty, breaks (EXITs) to that label within
 nested loops are included in the returned list.
*/
std::vector<SgBreakStmt *> findBreakStmts(SgStatement *code,
                                          const std::string &fortranLabel = "");

//! Find all continue statements inside a particular statement, stopping at
//! nested loops
/*! Nested loops define their own contexts for continue statements.  The
 function will stop immediately if run on a loop
 statement.  If fortranLabel is non-empty, continues (CYCLEs) to that label
 within nested loops are included in the returned list.
*/
std::vector<SgContinueStmt *>
findContinueStmts(SgStatement *code, const std::string &fortranLabel = "");
std::vector<SgGotoStatement *> findGotoStmts(SgStatement *scope,
                                             SgLabelStatement *l);
std::vector<SgStatement *> getSwitchCases(SgSwitchStatement *sw);

//! Collect all variable references in a subtree
void collectVarRefs(SgLocatedNode *root, std::vector<SgVarRefExp *> &result);

//! One variable-reference spelling and the exact statement that emits it.
class VariableReferenceUse {
public:
  VariableReferenceUse(SgVarRefExp *reference, SgStatement *statement)
      : reference_(reference), statement_(statement) {
    ASSERT_not_null(reference_);
    ASSERT_not_null(statement_);
  }

  SgVarRefExp *reference() const { return reference_; }
  SgStatement *statement() const { return statement_; }

private:
  SgVarRefExp *reference_;
  SgStatement *statement_;
};

//! Collect variable references together with their exact source use sites.
//!
//! Unlike parent-chain queries, this also preserves use sites for references
//! embedded in non-traversal type edges such as a new-expression array bound.
void collectVariableReferenceUses(SgLocatedNode *root,
                                  std::vector<VariableReferenceUse> &result);

//! Topdown traverse a subtree from root to find the first declaration given its
//! name, scope (optional, can be NULL), and defining or nondefining flag.
template <typename T>
T *findDeclarationStatement(SgNode *root, std::string name,
                            SgScopeStatement *scope, bool isDefining) {
  bool found = false;

  // Do we really want a NULL pointer to be acceptable input to this function?
  // Maybe we should have an assertion that it is non-null?
  if (!root)
    return NULL;

  T *decl = dynamic_cast<T *>(root);

  if (decl != NULL) {
    // CLANG FRONTEND FIX: search_for_symbol_from_symbol_table() can return NULL
    // for Clang-generated implicit/compiler-generated declarations
    SgSymbol *symbol = decl->search_for_symbol_from_symbol_table();

    if (symbol != NULL) {
      if (scope) {
        if ((decl->get_scope() == scope) && (symbol->get_name() == name)) {
          found = true;
        }
      } else // Liao 2/9/2010. We should allow NULL scope
      {
        if (symbol->get_name() == name) {
          found = true;
        }
      }
    }
  }

  if (found) {
    if (isDefining) {
      ROSE_ASSERT(decl->get_definingDeclaration() != NULL);
      return dynamic_cast<T *>(decl->get_definingDeclaration());
    } else {
      return decl;
    }
  }

  std::vector<SgNode *> children = root->get_traversalSuccessorContainer();

  // DQ (4/10/2016): Note that if we are searching for a function member that
  // has it's defining declaration defined outside of the class then it will not
  // be found in the child list.
  for (std::vector<SgNode *>::const_iterator i = children.begin();
       i != children.end(); ++i) {
    T *target = findDeclarationStatement<T>(*i, name, scope, isDefining);

    if (target) {
      return target;
    }
  }

  return NULL;
}
//! Topdown traverse a subtree from root to find the first function declaration
//! matching the given name, scope (optional, can be NULL), and defining or
//! nondefining flag. This is an instantiation of findDeclarationStatement<T>.
SgFunctionDeclaration *findFunctionDeclaration(SgNode *root, std::string name,
                                               SgScopeStatement *scope,
                                               bool isDefining);

//@}

//------------------------------------------------------------------------
//@{
/*! @name Bottom up search
  \brief Backwards traverse through the AST to find a node, findEnclosingXXX()
*/
// remember to put const to all arguments.

/** Find a node by type using upward traversal.
 *
 *  Traverse backward through a specified node's ancestors, starting with the
 * node's parent and progressing to more distant ancestors, to find the first
 * node matching the specified or derived type.  If `includingSelf` is true then
 * the starting node, `astNode,` is returned if its type matches, otherwise the
 * search starts at the parent of `astNode.`
 *
 *  For the purposes of this function, the parent (P) of an
 * SgDeclarationStatement node (N) is considered to be the first non-defining
 * declaration of N if N has both a defining declaration and a first
 * non-defining declaration and the defining declaration is different than the
 * first non-defining declaration.
 *
 *  If no ancestor of the requisite type of subtypes is found then this function
 * returns a null pointer.
 *
 *  If `astNode` is the null pointer, then the return value is a null pointer.
 * That is, if there is no node, then there cannot be an enclosing node of the
 * specified type. */
template <typename NodeType>
NodeType *getEnclosingNode(const SgNode *astNode,
                           const bool includingSelf = false) {
#define DEBUG_GET_ENCLOSING_NODE 0

  // DQ (12/31/2019): This version does not detect a cycle that Robb's version
  // detects in processing Cxx11_tests/test2016_23.C. This will have to be
  // investigated separately from the issue I am working on currently.

  // DQ (10/20/2012): This is the older version of this implementation.  Until I
  // am sure that the newer version (below) is what we want to use I will
  // resolve this conflict by keeping the previous version in place.

  if (nullptr == astNode) {
    return nullptr;
  }

  if ((includingSelf) && (dynamic_cast<const NodeType *>(astNode))) {
    return const_cast<NodeType *>(dynamic_cast<const NodeType *>(astNode));
  }

  // DQ (3/5/2012): Check for reference to self...
  ROSE_ASSERT(astNode->get_parent() != astNode);

  SgNode *parent = astNode->get_parent();

  // DQ (3/5/2012): Check for loops that will cause infinite loops.
  SgNode *previouslySeenParent = parent;
  bool foundCycle = false;
  int counter = 0;

#if DEBUG_GET_ENCLOSING_NODE
  printf("In getEnclosingNode(): previouslySeenParent = %p = %s \n",
         previouslySeenParent, previouslySeenParent->class_name().c_str());
#endif

  while ((foundCycle == false) && (parent != nullptr) &&
         (!dynamic_cast<const NodeType *>(parent))) {
    ROSE_ASSERT(parent->get_parent() != parent);

#if DEBUG_GET_ENCLOSING_NODE
    printf(" --- parent = %p = %s \n", parent, parent->class_name().c_str());
    printf(" --- --- parent->get_parent() = %p = %s \n", parent->get_parent(),
           parent->get_parent()->class_name().c_str());
#endif

    // DQ (1/8/2020): ROSE-82 (on RZ) This limit needs to be larger and
    // increasing it to 500 was enough for a specific code with a long chain of
    // if-then-else nesting, So to make this sufficent for more general code we
    // have increased the lomit to 100,000.  Note that 50 was not enough for
    // real code, but was enough for our regression tests. DQ (12/30/2019): This
    // is added to support detection of infinite loops over parent pointers. if
    // (counter >= 500)
    if (counter >= 100000) {
      printf("Exiting: In getEnclosingNode(): loop limit exceeded: counter = "
             "%d \n",
             counter);
      ROSE_ABORT();
    }
    parent = parent->get_parent();

    // DQ (3/5/2012): Check for loops that will cause infinite loops.
    // ROSE_ASSERT(parent != previouslySeenParent);
    if (parent == previouslySeenParent) {
      foundCycle = true;
    }
    counter++;
  }

#if DEBUG_GET_ENCLOSING_NODE
  printf("previouslySeenParent = %p = %s \n", previouslySeenParent,
         previouslySeenParent->class_name().c_str());
#endif

  parent = previouslySeenParent;

  SgDeclarationStatement *declarationStatement =
      isSgDeclarationStatement(parent);
  if (declarationStatement != nullptr) {
    SgDeclarationStatement *definingDeclaration =
        declarationStatement->get_definingDeclaration();
    SgDeclarationStatement *firstNondefiningDeclaration =
        declarationStatement->get_firstNondefiningDeclaration();

    if (definingDeclaration != nullptr &&
        declarationStatement != firstNondefiningDeclaration) {
      // DQ (10/19/2012): Use the defining declaration instead.
      // parent = firstNondefiningDeclaration;
      parent = definingDeclaration;
    }
  }

  // DQ (10/19/2012): This branch is just to document the cycle that was
  // previously detected, it is for debugging only. Thus it only makes
  // sense for it to be executed when "(foundCycle == true)". Since the
  // cycle is detected, but there is no assertion on the
  // cycle, we don't exit when a cycle is identified (which is the point
  // of the code below). Note also that I have fixed the code (above and
  // below) to only chase pointers through defining declarations (where
  // they exist), this is important since non-defining declarations can be
  // almost anywhere (and thus chasing them can make it appear that there
  // are cycles where there are none (I think); test2012_234.C
  // demonstrates an example of this. if (foundCycle == true)
  if (foundCycle == false) {

    while ((parent != nullptr) && (!dynamic_cast<const NodeType *>(parent))) {
      ROSE_ASSERT(parent->get_parent() != parent);
      SgDeclarationStatement *declarationStatement =
          isSgDeclarationStatement(parent);
      if (declarationStatement != nullptr) {
#if DEBUG_GET_ENCLOSING_NODE
        printf("Found a SgDeclarationStatement \n");
#endif
        SgDeclarationStatement *definingDeclaration =
            declarationStatement->get_definingDeclaration();
        SgDeclarationStatement *firstNondefiningDeclaration =
            declarationStatement->get_firstNondefiningDeclaration();
        if (definingDeclaration != nullptr &&
            declarationStatement != firstNondefiningDeclaration) {
          // DQ (10/19/2012): Use the defining declaration instead.
          // parent = firstNondefiningDeclaration;
          parent = definingDeclaration;
        }
      }

      parent = parent->get_parent();

      // DQ (3/5/2012): Check for loops that will cause infinite loops.
      ROSE_ASSERT(parent != previouslySeenParent);
    }
  }

  return const_cast<NodeType *>(dynamic_cast<const NodeType *>(parent));
}

//! Find enclosing source file node
ROSE_DLL_API SgSourceFile *
getEnclosingSourceFile(const SgNode *n, const bool includingSelf = false);

//! Return the exact target ABI type used by sizeof/alignof for the context's
//! one owning translation unit. Detached or untyped contexts are malformed.
ROSE_DLL_API SgType *requireTargetSizeType(const SgNode *context);

//! Get the closest scope from astNode. Return astNode if it is already a scope.
ROSE_DLL_API SgScopeStatement *getScope(const SgNode *astNode);

/** Return true only for a Fortran main program or BLOCK DATA program unit
 * whose source syntax has no name.  The function hard-fails when the public
 * declaration name and the explicit source-name metadata disagree. */
ROSE_DLL_API bool
isFortranProgramUnitWithoutSourceName(const SgFunctionDeclaration *decl);

/** Return whether a spelling is a valid Fortran source identifier. */
ROSE_DLL_API bool isValidFortranSourceIdentifier(const std::string &name);

/** Return the symbol-table key for a Fortran program unit.  Anonymous program
 * units receive a stable, source-position-derived internal key while their
 * public SgFunctionDeclaration::get_name() remains empty. */
ROSE_DLL_API SgName
getFortranProgramUnitSymbolTableKey(const SgFunctionDeclaration *decl);

/** Resolve a Fortran common-block designator in its lexical program-unit
 * scope.  Missing and ambiguous declarations are hard errors. */
ROSE_DLL_API SgCommonBlockObject *
lookupFortranCommonBlockObject(const SgName &useName, const SgNode *context);

/** Enforce the semantic and exact-source identity contract of a typed Fortran
 * common-block directive designator. */
ROSE_DLL_API void
validateFortranCommonBlockRef(const SgFortranCommonBlockRefExp *reference);

//! Get the enclosing scope from a node n
ROSE_DLL_API SgScopeStatement *
getEnclosingScope(SgNode *n, const bool includingSelf = false);

//! Traverse back through a node's parents to find the enclosing global scope
ROSE_DLL_API SgGlobal *getGlobalScope(const SgNode *astNode);

// DQ (12/7/2020): This is supporting the recognition of functions in header
// files from two different AST.
//! This is supporting the recognition of functions in header files from two
//! different ASTs
ROSE_DLL_API bool hasSameGlobalScope(SgStatement *statement_1,
                                     SgStatement *statement_2);

//! Find the function definition
ROSE_DLL_API SgFunctionDefinition *
getEnclosingProcedure(SgNode *n, const bool includingSelf = false);

ROSE_DLL_API SgFunctionDefinition *
getEnclosingFunctionDefinition(SgNode *astNode,
                               const bool includingSelf = false);

//! Find the closest enclosing statement, including the given node
ROSE_DLL_API SgStatement *getEnclosingStatement(SgNode *n);

//! Find the closest switch outside a given statement (normally used for case
//! and default statements)
ROSE_DLL_API SgSwitchStatement *findEnclosingSwitch(SgStatement *s);

//! Find enclosing OpenMP clause body statement from s. If s is already one,
//! return it directly.
ROSE_DLL_API SgOmpClauseBodyStatement *
findEnclosingOmpClauseBodyStatement(SgStatement *s);

//! Find the closest loop outside the given statement; if fortranLabel is not
//! empty, the Fortran label of the loop must be equal to it
ROSE_DLL_API SgScopeStatement *
findEnclosingLoop(SgStatement *s, const std::string &fortranLabel = "",
                  bool stopOnSwitches = false);

//! Find the enclosing function declaration, including its derived instances
//! like isSgProcedureHeaderStatement, isSgProgramHeaderStatement, and
//! isSgMemberFunctionDeclaration.
ROSE_DLL_API SgFunctionDeclaration *
getEnclosingFunctionDeclaration(SgNode *astNode,
                                const bool includingSelf = false);

// roseSupport/utility_functions.h
//! get the SgFile node from current node
ROSE_DLL_API SgFile *getEnclosingFileNode(SgNode *astNode);

//! Get the initializer containing an expression if it is within an initializer.
ROSE_DLL_API SgInitializer *getInitializerOfExpression(SgExpression *n);

//! Get the closest class definition enclosing the specified AST node,
ROSE_DLL_API SgClassDefinition *
getEnclosingClassDefinition(SgNode *astnode, const bool includingSelf = false);

//! Get the closest class declaration enclosing the specified AST node,
ROSE_DLL_API SgClassDeclaration *getEnclosingClassDeclaration(SgNode *astNode);

//! Get the closest module statement enclosing the specified AST node,
ROSE_DLL_API SgModuleStatement *
getEnclosingModuleStatement(SgNode *astNode, const bool includingSelf = false);

//! Get the enclosing TemplateDeclaration statement
ROSE_DLL_API SgDeclarationStatement *
getTemplateDeclaration(const SgNode *astNode);

//! Get the enclosing type of this associated node, not used other than in
//! ./src/backend/unparser/nameQualificationSupport.C
ROSE_DLL_API SgType *getAssociatedType(const SgNode *astNode);

// DQ (2/7/2019): Adding support for name qualification of variable references
// associated with SgPointerMemberType function parameters.
//! Get the enclosing SgExprListExp (used as part of function argument index
//! evaluation in subexpressions).
ROSE_DLL_API SgExprListExp *
getEnclosingExprListExp(SgNode *astNode, const bool includingSelf = false);

// DQ (2/7/2019): Need a function to return when an expression is in an
// expression subtree. This is part of index evaluation ofr expressions in
// function argument lists, but likely usefule elsewhere as well.
ROSE_DLL_API bool isInSubTree(SgExpression *subtree, SgExpression *exp);

// DQ (2/7/2019): Need a function to return the SgFunctionDeclaration from a
// SgFunctionCallExp.
ROSE_DLL_API SgFunctionDeclaration *
getFunctionDeclaration(SgFunctionCallExp *functionCallExp);

// DQ (2/17/2019): Generalizing this support for SgVarRefExp and
// SgMemberFunctionRefExp nodes. DQ (2/8/2019): Adding support for detecting
// when to use added name qualification for pointer-to-member expressions.
ROSE_DLL_API bool isDataMemberReference(SgVarRefExp *varRefExp);
ROSE_DLL_API bool
isAddressOfCurrentObjectDataMemberReference(SgVarRefExp *varRefExp);
// ROSE_DLL_API bool isAddressTaken(SgVarRefExp* varRefExp);
ROSE_DLL_API bool isAddressTaken(SgExpression *refExp);

// DQ (2/17/2019): Adding support for detecting when to use added name
// qualification for membr function references.
ROSE_DLL_API bool
isMemberFunctionMemberReference(SgMemberFunctionRefExp *memberFunctionRefExp);

// DQ (2/15/2019): Adding support for detecting which class a member reference
// is being made from. ROSE_DLL_API SgClassType*
// getClassTypeForDataMemberReference(SgVarRefExp* varRefExp); ROSE_DLL_API
// std::list<SgClassType*> getClassTypeChainForDataMemberReference(SgVarRefExp*
// varRefExp);
ROSE_DLL_API std::list<SgClassType *>
getClassTypeChainForMemberReference(SgExpression *refExp);

ROSE_DLL_API std::set<SgNode *> getFrontendSpecificNodes();

// DQ (2/17/2019): Display the shared nodes in the AST for debugging.
ROSE_DLL_API void outputSharedNodes(SgNode *node);

// DQ (10/31/2020): Added function to help debug edits to statements in scopes.
ROSE_DLL_API void displayScope(SgScopeStatement *scope);

// TODO
//@}

//------------------------------------------------------------------------
//@{
/*! @name AST Walk and Traversal
  \brief
*/
// Liao, 1/9/2008
/*!
      \brief return the first global scope under current project
*/
ROSE_DLL_API SgGlobal *getFirstGlobalScope(SgProject *project);

/*!
      \brief get the last statement within a scope, return NULL if it does not
   exit
*/
ROSE_DLL_API SgStatement *getLastStatement(SgScopeStatement *scope);

//! Get the first statement within a scope, return NULL if it does not exist.
//! Skip compiler-generated statement by default. Count transformation-generated
//! ones, but excluding those which are not to be outputted in unparsers.
ROSE_DLL_API SgStatement *
getFirstStatement(SgScopeStatement *scope,
                  bool includingCompilerGenerated = false);
//! Find the first defining function declaration statement in a scope
ROSE_DLL_API SgFunctionDeclaration *
findFirstDefiningFunctionDecl(SgScopeStatement *scope);

//! Get next statement within the same scope of current statement
ROSE_DLL_API SgStatement *getNextStatement(SgStatement *currentStmt);

//! Get previous statement of the current statement. It may return a previous
//! statement of a parent scope by default (climbOutScope is true), otherwise
//! only a previous statement of the same scope is returned.
ROSE_DLL_API SgStatement *getPreviousStatement(SgStatement *currentStmt,
                                               bool climbOutScope = true);

// DQ (11/15/2018): Adding support for traversals over the include file tree.
//! return path prefix for subtree of include files.
ROSE_DLL_API void listHeaderFiles(SgIncludeFile *includeFile);

// DQ (5/9/2021): Adding support for detection of statements in a scope that
// must be unparsed.
/*! \brief This function supports the token-based unparsing when used with
   unparsing of header files to know when the scope can be unparsed via it's
   token stream, even though a statement from a header file may contain a
   transformation. returns true if there is a statement in the scope that has to
   be unparsed (is from the same file as the scope). returns false if the scope
   is empty or contains only statements associated with one or more header
   files.
*/
ROSE_DLL_API bool scopeHasStatementsFromSameFile(SgScopeStatement *scope);

//@}

//------------------------------------------------------------------------
//@{
/*! @name AST Comparison
  \brief Compare AST nodes, subtree, etc
*/
//! Check if a SgIntVal node has a given value
ROSE_DLL_API bool isEqualToIntConst(SgExpression *e, int value);

//! Check if two function declarations refer to the same one. Two function
//! declarations are the same when they are a) identical, b) same name in C c)
//! same qualified named and mangled name in C++. A nondefining (prototype)
//! declaration and a defining declaration of a same function are treated as the
//! same.
/*!
 * There is a similar function bool
 * compareFunctionDeclarations(SgFunctionDeclaration *f1, SgFunctionDeclaration
 * *f2) from Classhierarchy.C
 */
ROSE_DLL_API bool isSameFunction(SgFunctionDeclaration *func1,
                                 SgFunctionDeclaration *func2);

//! Check if a statement is the last statement within its closed scope
ROSE_DLL_API bool isLastStatement(SgStatement *stmt);

//@}

//------------------------------------------------------------------------
//@{
/*! @name AST insert, removal, and replacement
  \brief Add, remove,and replace AST

  scope->append_statement(), exprListExp->append_expression() etc. are not
  enough to handle side effect of parent pointers, symbol tables, preprocessing
  info, defining/nondefining pointers etc.
*/

struct DeferredTransformation {
  // DQ (11/19/2020): We need to expand the use of this to cover deffered
  // transformations of common SageInterface transformations (e.g.
  // replaceStatement). So I needed to move this out of being specific to the
  // outliner and make it more generally data structure in the SageInterface.

  // DQ (11/15/2020): Need to add the concept of deffered transformation to
  // cover replaceStatement operations.

  // DQ (8/7/2019): Store data required to support defering the transformation
  // to insert the outlined function prototypes into class declaration (when
  // this is required to support the outlined function's access to protected or
  // private data members). This is part of an optimization to support the
  // optimization of header file unparsing (limiting the overhead of supporting
  // any header file to just focus on the few (typically one) header file that
  // would have to be unparsed.

  enum TransformationKind {
    // DQ (11/22/2020): Might need to also add
    // SageInterface::addDefaultConstructorIfRequired() and
    // SageStatement::insert_statment()
    // to support the processStatements.C transforamtions to pre-process the AST
    // (return expressions and variable initializations).
    e_error,
    e_default,
    e_outliner,
    e_replaceStatement,
    e_removeStatement,
    e_last
  };

  TransformationKind deferredTransformationKind;

  // DQ (12/12/2020): Adding a string label so that we can name the different
  // kinds of transformations. E.g. moving pattern matched function from header
  // file to dynamic library, vs. replacing function definitions in the dynamic
  // library file with function prototypes.
  std::string transformationLabel;

  // Remove sets statementToRemove, replace sets statementToRemove and
  // StatementToAdd.
  SgStatement *statementToRemove;
  SgStatement *statementToAdd;

  SgClassDefinition *class_definition;
  SgDeclarationStatement *target_class_member;
  SgDeclarationStatement *new_function_prototype;

  typedef std::set<SgClassDefinition *> ClassDefSet_t;
  ClassDefSet_t targetClasses;

  typedef std::vector<SgFunctionDeclaration *> FuncDeclList_t;
  FuncDeclList_t targetFriends;

  // DQ (2/28/2021): Adding support for outlining where it involves building up
  // pre-transformations. For example, in the code segregation, we build a
  // conditiona around the interval of statements that we are outlining. This
  // conditional is used to overwrite the first statement in the interval list.
  // Because we don't want to transform the AST until after the outlining, we
  // need so save the whole interval so that we, after the outlining, remove the
  // statements in the interval after that first statement.
  typedef std::vector<SgStatement *> IntervalType;
  IntervalType statementInterval;
  SgStatement *locationToOverwriteWithTransformation;
  SgStatement *transformationToOverwriteFirstStatementInInterval;
  SgBasicBlock *blockOfStatementsToOutline;

  // DQ (12/5/2019): Added ROSE_DLL_API prefix for shared-library visibility.
  ROSE_DLL_API DeferredTransformation();
  ROSE_DLL_API
  DeferredTransformation(SgClassDefinition *class_definition,
                         SgDeclarationStatement *target_class_member,
                         SgDeclarationStatement *new_function_prototype);
  ROSE_DLL_API
  DeferredTransformation(const DeferredTransformation &X); //! Copy constructor.
  ROSE_DLL_API ~DeferredTransformation(
      void); //! Shallow; does not delete fields.

  ROSE_DLL_API DeferredTransformation &
  operator=(const DeferredTransformation &X); //! operator=()

  static ROSE_DLL_API DeferredTransformation
  replaceStatement(SgStatement *oldStmt, SgStatement *newStmt,
                   bool movePreprocessinInfo = false);

  static ROSE_DLL_API std::string
  outputDeferredTransformationKind(const TransformationKind &kind);
  ROSE_DLL_API void display(std::string label) const;
};

// DQ (2/24/2009): Simple function to delete an AST subtree (used in outlining).
//! Function to delete AST subtree's nodes only, users must take care of any
//! dangling pointers, symbols or types that result.
enum class DeleteAstMode {
  kConservative,          // Preserve nodes referenced from outside the subtree.
  kRequireIsolated,       // Abort if any candidate has an external reference.
  kSkipExternalReferences // Assume subtree is isolated; skip global
                          // reference scan.
};

ROSE_DLL_API void deleteAST(SgNode *node);
ROSE_DLL_API void deleteAST(SgNode *node, DeleteAstMode mode);

//! Explicitly tear down an AST and release global caches and memory pools.
//! AST pointers are invalid after this call.
ROSE_DLL_API void tearDownAst(SgProject *project);

//! Returns true if AST teardown is enabled for this process.
ROSE_DLL_API bool isAstTeardownEnabled();

//! Register an AST teardown handler to run at process exit if cleanup was
//! not invoked.
ROSE_DLL_API void registerAstTeardownAtExit();

//! Record a project for teardown when running exit handlers.
ROSE_DLL_API void registerAstTeardownProject(SgProject *project);

// DQ (3/5/2022): Adding support to check AST for invalid poionters.
ROSE_DLL_API void checkSgNodePointers();

// DQ (2/25/2009): Added new function to support outliner.
//! Move statements in first block to the second block (preserves order and
//! rebuilds the symbol table).
ROSE_DLL_API void moveStatementsBetweenBlocks(SgBasicBlock *sourceBlock,
                                              SgBasicBlock *targetBlock);

//! Move statements between C++ namespace's definitions
ROSE_DLL_API void
moveStatementsBetweenBlocks(SgNamespaceDefinitionStatement *sourceBlock,
                            SgNamespaceDefinitionStatement *targetBlock);

//!  Check if a function declaration is a C++11 lambda function
ROSE_DLL_API bool isLambdaFunction(SgFunctionDeclaration *func);

//! check if a variable reference is this->a[i] inside of a lambda function
ROSE_DLL_API bool isLambdaCapturedVariable(SgVarRefExp *varRef);

//! Move a variable declaration to a new scope, handle symbol, special scopes
//! like For loop, etc.
ROSE_DLL_API void moveVariableDeclaration(SgVariableDeclaration *decl,
                                          SgScopeStatement *target_scope);

//! Clone one template parameter for a generated declaration. Template-template
//! declaration identities are copied independently, and every generated
//! located descendant is left detached until the destination declaration
//! publishes its exact physical output owner.
ROSE_DLL_API SgTemplateParameter *
cloneDetachedGeneratedTemplateParameter(const SgTemplateParameter *source,
                                        const char *context);

//! Publish or validate the exact output owner for generated located nodes
//! before a subtree crosses an attached AST mutation boundary. A physical
//! owner publishes its exact file and occurrence. A semantic owner instead
//! publishes detached transformation descendants as semantic-only frontend
//! structure and validates existing semantic descendants.
ROSE_DLL_API void publishGeneratedSubtreeOutputOwner(SgNode *generatedSubtree,
                                                     SgLocatedNode *exactOwner);

//! Return whether a declaration has exact semantic-auxiliary ownership.
//! A declaration whose parent is an auxiliary container must satisfy the
//! complete reciprocal scope/container/list contract; malformed partial
//! ownership is a hard error.
ROSE_DLL_API bool
hasExactSemanticAuxiliaryOwnership(const SgDeclarationStatement *declaration);

//! Relocate generated descendants of an already-published subtree through one
//! explicit physical-output file-and-occurrence transfer. Source-spelled
//! descendants retain their original physical provenance. Every generated
//! position must identify priorOwner exactly; this API never infers or repairs
//! an owner.
ROSE_DLL_API void
relocateGeneratedSubtreePhysicalOutputOwner(SgNode *generatedSubtree,
                                            SgLocatedNode *priorOwner,
                                            SgLocatedNode *exactOwner);

//! Classify one freshly constructed, non-expression frontend node as semantic
//! name/structure infrastructure. Any existing or partial source position is
//! a producer error.
ROSE_DLL_API void
setSemanticOnlyFrontendSourcePosition(SgLocatedNode *semanticNode);

//! Return whether one file-info record is exact semantic-only frontend
//! provenance owned by `semanticNode`. A function declaration can additionally
//! retain its typed physical source-file association, and an exactly
//! auxiliary-owned declaration or definition can retain its complete frontend
//! source coordinates, without either becoming a source-emitted declaration.
ROSE_DLL_API bool
hasExactSemanticFrontendSourcePosition(const SgNode *semanticNode,
                                       const Sg_File_Info *position);

//! Return whether every owned source position on a located node identifies
//! one exact compiler-generated, frontend-specific semantic-only surface.
ROSE_DLL_API bool
hasSemanticOnlyFrontendSourcePosition(const SgLocatedNode *semanticNode);

//! Return whether every owned source position on a located node identifies
//! one exact detached transformation surface awaiting an output owner.
ROSE_DLL_API bool
hasDetachedTransformationSourcePosition(const SgLocatedNode *generatedNode);

//! Promote one exact semantic-only frontend node that a transformation has
//! rewritten into an independently emitted lexical node. The caller supplies
//! the attached physical output owner explicitly; semantic descendants are
//! not reclassified implicitly.
ROSE_DLL_API void
promoteSemanticOnlyNodeToGeneratedOutput(SgLocatedNode *semanticNode,
                                         SgLocatedNode *exactOwner);

//! Begin the one exact bottom-up construction transaction for a detached
//! function-parameter scope. The scope is semantic name infrastructure and
//! must already carry exact semantic-only provenance. The physical output
//! owner and semantic lookup scope are independent, explicit roles; this is
//! required for a parameter scope nested in another detached declarative
//! region. No parent-chain inference is performed.
ROSE_DLL_API void beginDetachedFunctionParameterScopeConstruction(
    SgFunctionParameterScope *parameterScope,
    SgScopeStatement *exactPhysicalOutputOwner,
    SgScopeStatement *exactSemanticScope);

//! Attach a parameter scope to its final declaration and consume its exact
//! detached construction transaction. The scope remains semantic-only while
//! physically emitted descendants retain the transaction's explicit output
//! owner. Missing, stale, or mismatched transactions are hard errors.
ROSE_DLL_API void completeDetachedFunctionParameterScopeConstruction(
    SgFunctionDeclaration *declaration,
    SgFunctionParameterScope *parameterScope);

//! Attach a detached local-parameter scope to the requires-expression that
//! owns its declarative region and consume the same exact construction
//! transaction used while translating the local parameters.
ROSE_DLL_API void completeDetachedRequiresParameterScopeConstruction(
    SgRequiresExpr *requiresExpression,
    SgFunctionParameterScope *parameterScope);

ROSE_DLL_API void
beginDetachedForStatementConstruction(SgForStatement *statement,
                                      SgScopeStatement *exactOutputOwner);
ROSE_DLL_API void
completeDetachedForStatementConstruction(SgForStatement *statement);

//! Append a statement to the end of an explicit lexical scope, handling symbol
//! and defining/nondefining links.
ROSE_DLL_API void appendStatement(SgStatement *stmt, SgScopeStatement *scope);

//! Publish and validate the effective access of a declaration at its exact
//! structural position in a C++ class definition.  Fresh builder declarations
//! may arrive with an unclassified access modifier; source-backed declarations
//! must already agree with the surrounding access-label stream.
ROSE_DLL_API void
publishClassMemberAccessAtLexicalBoundary(SgDeclarationStatement *declaration,
                                          SgClassDefinition *definition);

//! Append a statement to the end of SgForInitStatement
ROSE_DLL_API void appendStatement(SgStatement *stmt,
                                  SgForInitStatement *for_init_stmt);

//! Append a list of statements to the end of an explicit lexical scope.
ROSE_DLL_API void appendStatementList(const std::vector<SgStatement *> &stmt,
                                      SgScopeStatement *scope);

//! Capture the primary source file's preprocessing directives in physical
//! source order.  The returned values are independent of later AST mutations.
ROSE_DLL_API std::vector<PreprocessingInfo>
collectCppDirectiveSnapshot(SgSourceFile *file);

// DQ (2/6/2009): Added function to support outlining into separate file.
//! Append a copy ('decl') of a function ('original_statement') into a
//! 'scope', include any referenced declarations required if the scope is
//! within a compiler generated file. All referenced declarations, including
//! those from headers, are inserted if excludeHeaderFiles is set to true
//! (the new file will not have any headers).
ROSE_DLL_API void appendStatementWithDependentDeclaration(
    SgDeclarationStatement *decl, SgGlobal *scope,
    SgStatement *original_statement,
    SgFunctionDeclaration *source_call_declaration, bool excludeHeaderFiles,
    const std::vector<PreprocessingInfo> &original_directives,
    SgSourceFile *original_source_file = NULL,
    int original_physical_file_id = -1);

//! Prepend a statement to the beginning of an explicit lexical scope, handling
//! side effects as appropriate.
ROSE_DLL_API void prependStatement(SgStatement *stmt, SgScopeStatement *scope);

//! Prepend a statement to the beginning of SgForInitStatement
ROSE_DLL_API void prependStatement(SgStatement *stmt,
                                   SgForInitStatement *for_init_stmt);

//! Prepend a list of statements to the beginning of an explicit lexical scope.
ROSE_DLL_API void prependStatementList(const std::vector<SgStatement *> &stmt,
                                       SgScopeStatement *scope);

//! Check if a scope statement has a simple children statement list
//! so insert additional statements under the scope is straightforward and
//! unambiguous . for example, SgBasicBlock has a simple statement list while
//! IfStmt does not.
ROSE_DLL_API bool hasSimpleChildrenList(SgScopeStatement *scope);

/**
 * Normalize an attached statement into the exact lexical anchor used by a
 * subsequent sibling insertion.
 *
 * Control-flow and OpenMP bodies that directly own @p targetStmt are
 * structurally normalized to an explicit basic block before this function
 * returns.  Conditions are represented by their enclosing control statement
 * because a condition has no sibling statement list.  Scope-sensitive
 * declarations must be built against the returned anchor's scope, not the
 * target's pre-normalization scope.
 */
ROSE_DLL_API SgStatement *
prepareStatementInsertionAnchor(SgStatement *targetStmt);

//! Insert a statement before or after the target statement within the
//! target's scope. Move around preprocessing info automatically
ROSE_DLL_API void insertStatement(SgStatement *targetStmt, SgStatement *newStmt,
                                  bool insertBefore = true,
                                  bool autoMovePreprocessingInfo = true);

//! Insert a list of statements before or after the target statement within
//! the
// target's scope
ROSE_DLL_API void
insertStatementList(SgStatement *targetStmt,
                    const std::vector<SgStatement *> &newStmts,
                    bool insertBefore = true);

//! Insert a statement before a target statement
ROSE_DLL_API void insertStatementBefore(SgStatement *targetStmt,
                                        SgStatement *newStmt,
                                        bool autoMovePreprocessingInfo = true);

//! Insert a list of statements before a target statement
ROSE_DLL_API void
insertStatementListBefore(SgStatement *targetStmt,
                          const std::vector<SgStatement *> &newStmts);

//! Insert a statement after a target statement, Move around preprocessing
//! info automatically by default
ROSE_DLL_API void insertStatementAfter(SgStatement *targetStmt,
                                       SgStatement *newStmt,
                                       bool autoMovePreprocessingInfo = true);

//! Insert a list of statements after a target statement
ROSE_DLL_API void
insertStatementListAfter(SgStatement *targetStmt,
                         const std::vector<SgStatement *> &newStmt);

//! Insert a statement after the last declaration within a scope. The
//! statement will be prepended to the scope if there is no declaration
//! statement found
ROSE_DLL_API void insertStatementAfterLastDeclaration(SgStatement *stmt,
                                                      SgScopeStatement *scope);

//! Insert a list of statements after the last declaration within a scope.
//! The statement will be prepended to the scope if there is no declaration
//! statement found
ROSE_DLL_API void
insertStatementAfterLastDeclaration(std::vector<SgStatement *> stmt_list,
                                    SgScopeStatement *scope);

//! Insert a statement before the first non-declaration statement in a scope.
//! If the scope has no non-declaration statements
//  then the statement is inserted at the end of the scope.
ROSE_DLL_API void
insertStatementBeforeFirstNonDeclaration(SgStatement *newStmt,
                                         SgScopeStatement *scope,
                                         bool movePreprocessingInfo = true);

//! Insert statements before the first non-declaration statement in a scope.
//! If the scope has no non-declaration statements
// then the new statements are inserted at the end of the scope.
ROSE_DLL_API void insertStatementListBeforeFirstNonDeclaration(
    const std::vector<SgStatement *> &newStmts, SgScopeStatement *scope);

// DQ (11/21/2018): We need to sometimes insert something after the last
// statement of the collection from rose_required_macros_and_functions.h.
ROSE_DLL_API SgStatement *lastFrontEndSpecificStatement(SgGlobal *globalScope);

ROSE_DLL_API bool isRemovableStatement(SgStatement *s);

//! Remove a statement from its attach point of the AST. Automatically keep
//! its associated preprocessing information at the original place after the
//! removal. The statement is still in memory and it is up to the users to
//! decide if the removed one will be inserted somewhere else or released
//! from memory (deleteAST()).
ROSE_DLL_API void removeStatement(SgStatement *stmt,
                                  bool autoRelocatePreprocessingInfo = true);

//! Deep delete a sub AST tree. It uses postorder traversal to delete each
//! child node. Users must take care of any dangling pointers, symbols or
//! types that result. This is identical to deleteAST()
ROSE_DLL_API void deepDelete(SgNode *root);

//! Replace a statement with another. Move preprocessing information from
//! oldStmt to newStmt if requested.
ROSE_DLL_API void replaceStatement(SgStatement *oldStmt, SgStatement *newStmt,
                                   bool movePreprocessinInfo = false);

//! Replace an anchor node with a specified pattern subtree with optional
//! SgVariantExpression. All SgVariantExpression in the pattern will be
//! replaced with copies of the anchor node.
ROSE_DLL_API SgNode *replaceWithPattern(SgNode *anchor, SgNode *new_pattern);

//! Replace all variable references to an old symbol in a scope to being
//! references to a new symbol.
// Essentially replace variable a with b.
ROSE_DLL_API void replaceVariableReferences(SgVariableSymbol *old_sym,
                                            SgVariableSymbol *new_sym,
                                            SgScopeStatement *scope);

//! Require exact physical/include ownership before mutating a statement in a
//! token-unparsed header. Ambiguous or unsupported ownership is a hard error.
ROSE_DLL_API void requireStatementCanBeTransformed(SgStatement *stmt);

/** Given an expression and an exact attached insertion anchor, atomically
 * inserts a temporary variable immediately before the anchor and publishes its
 * symbol. The returned variable-reference expression can be used instead of
 * the original expression. The optional SgAssignOp reevaluates the expression.
 * Reference types are represented by pointer temporaries.
 * @param expression Expression which will be replaced by a variable
 * @param insertionAnchor Attached direct child of the exact lexical scope that
 * will own the temporary declaration
 * @param reEvaluate an assignment op to reevaluate the expression. Leave
 * NULL if not needed
 * @return attached declaration of the temporary variable and a variable
 * reference expression to use instead of the original expression. */
std::pair<SgVariableDeclaration *, SgExpression *>
createTempVariableForExpression(SgExpression *expression,
                                SgStatement *insertionAnchor,
                                bool initializeInDeclaration,
                                SgAssignOp **reEvaluate = NULL);

/*  This function creates a temporary variable for a given expression in the
   given scope This is different from
   SageInterface::createTempVariableForExpression in that it does not try to
   be smart to create pointers to reference types and so on. The tempt is
   initialized to expression. The caller is responsible for setting the
   parent of SgVariableDeclaration since buildVariableDeclaration may not
   set_parent() when the scope stack is empty. See
   programTransformation/extractFunctionArgumentsNormalization/ExtractFunctionArguments.C
   for sample usage.
   @param expression Expression which will be replaced by a variable
   @param scope scope in which the temporary variable will be generated
*/

std::pair<SgVariableDeclaration *, SgExpression *>
createTempVariableAndReferenceForExpression(SgExpression *expression,
                                            SgScopeStatement *scope);

//! Append an argument to SgFunctionParameterList, transparently set
//! parent,scope, and symbols for arguments when possible
/*! We recommend to build SgFunctionParameterList before building a function
 declaration However, it is still allowed to append new arguments for
 existing function declarations.
 \todo function type , function symbol also need attention.
*/
ROSE_DLL_API SgVariableSymbol *appendArg(SgFunctionParameterList *,
                                         SgInitializedName *);
//! Prepend an argument to SgFunctionParameterList
ROSE_DLL_API SgVariableSymbol *prependArg(SgFunctionParameterList *,
                                          SgInitializedName *);

//! Append an expression to a SgExprListExp, set the parent pointer also
ROSE_DLL_API void appendExpression(SgExprListExp *, SgExpression *);

//! Append an expression list to a SgExprListExp, set the parent pointers
//! also
ROSE_DLL_API void appendExpressionList(SgExprListExp *,
                                       const std::vector<SgExpression *> &);

//! Set parameter list for a function declaration, considering existing
//! parameter list etc.
template <class actualFunction>
void setParameterList(actualFunction *func, SgFunctionParameterList *paralist) {

  // TODO consider the difference between C++ and Fortran
  // fixup the scope of arguments,no symbols for nondefining function
  // declaration's arguments

  // DQ (11/25/2011): templated function so that we can handle both
  // SgFunctionDeclaration and SgTemplateFunctionDeclaration (and their
  // associated member function derived classes).

  ROSE_ASSERT(func != NULL);
  ROSE_ASSERT(paralist != NULL);

  // Liao,2/5/2008  constructor of SgFunctionDeclaration will automatically
  // generate SgFunctionParameterList, so be cautious when set new paralist!!
  if (func->get_parameterList() != NULL) {
    if (func->get_parameterList() != paralist) {
      delete func->get_parameterList();
    }
  }

  func->set_parameterList(paralist);
  paralist->set_parent(func);

  {
    // DQ (5/15/2012): Need to set the declptr in each SgInitializedName IR
    // node. This is needed to support the AST Copy mechanism (at least). The
    // files: test2005_150.C, test2012_81.C and testcode2012_82.C demonstrate
    // this problem.
    SgInitializedNamePtrList &args = paralist->get_args();
    for (SgInitializedNamePtrList::iterator i = args.begin(); i != args.end();
         i++) {
      (*i)->set_declptr(func);
    }
  }
}

//! Publish an exactly constructed ctor-initializer list for a member function.
template <class actualMemberFunction>
void setCtorInitializerList(actualMemberFunction *func,
                            SgCtorInitializerList *ctorlist) {
  ROSE_ASSERT(func != NULL);
  ROSE_ASSERT(ctorlist != NULL);

  SgCtorInitializerList *previousCtorList = func->get_CtorInitializerList();
  if (previousCtorList != NULL && previousCtorList != ctorlist) {
    func->set_CtorInitializerList(ctorlist);
    SageInterface::deleteAST(previousCtorList);
  } else {
    func->set_CtorInitializerList(ctorlist);
  }
  ctorlist->set_parent(func);

  SgScopeStatement *classScope = func->get_scope();
  if (classScope == NULL || isSgClassDefinition(classScope) == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[constructor-initializer-publication]: "
            "member function=%p has no exact semantic class scope\n",
            static_cast<void *>(func));
    ROSE_ABORT();
  }

  SgInitializedNamePtrList &ctors = ctorlist->get_ctors();
  for (SgInitializedNamePtrList::iterator i = ctors.begin(); i != ctors.end();
       ++i) {
    SgInitializedName *ctor = *i;
    if (ctor == NULL || ctor->get_parent() != ctorlist ||
        ctor->get_scope() != classScope ||
        (ctor->get_declptr() != NULL && ctor->get_declptr() != func)) {
      fprintf(stderr,
              "REX_AST_INVARIANT[constructor-initializer-publication]: "
              "member function=%p list=%p entry=%p has no exact list parent, "
              "class scope, or declaration identity\n",
              static_cast<void *>(func), static_cast<void *>(ctorlist),
              static_cast<void *>(ctor));
      ROSE_ABORT();
    }
    ctor->set_declptr(func);
  }
}

//! Set a pragma of a pragma declaration. handle memory release for preexisting
//! pragma, and set parent pointer.
ROSE_DLL_API void setPragma(SgPragmaDeclaration *decl, SgPragma *pragma);

//! Replace an expression with another, used for variable reference substitution
//! and others. the old expression can be deleted (default case)  or kept.
ROSE_DLL_API void replaceExpression(SgExpression *oldExp, SgExpression *newExp,
                                    bool keepOldExp = false);

//! Replace a given expression with a list of statements produced by a generator
ROSE_DLL_API void
replaceExpressionWithStatement(SgExpression *from,
                               SageInterface::StatementGenerator *to);
//! Similar to replaceExpressionWithStatement, but with more restrictions.
//! Assumptions: from is not within the test of a loop or ifStmt,  not currently
//! traversing from or the statement it is in
ROSE_DLL_API void
replaceSubexpressionWithStatement(SgExpression *from,
                                  SageInterface::StatementGenerator *to);

//! Set operands for expressions with single operand, such as unary expressions.
//! handle file info, lvalue, pointer downcasting, parent pointer etc.
ROSE_DLL_API void setOperand(SgExpression *target, SgExpression *operand);

//! set left hand operand for binary expressions, transparently downcasting
//! target expressions when necessary
ROSE_DLL_API void setLhsOperand(SgExpression *target, SgExpression *lhs);

//! set left hand operand for binary expression
ROSE_DLL_API void setRhsOperand(SgExpression *target, SgExpression *rhs);

//! Delete exactly owned source-provenance trees before transforming their
//! semantic expression owners.
ROSE_DLL_API void removeAllOriginalExpressionTrees(SgNode *top);

// DQ (1/25/2010): Added support for directories
//! Move file to be generated in a subdirectory (will be generated by the
//! unparser).
ROSE_DLL_API void moveToSubdirectory(std::string directoryName, SgFile *file);

//! Supporting function to comment relocation in insertStatement() and
//! removeStatement().
ROSE_DLL_API SgStatement *findSurroundingStatementFromSameFile(
    SgStatement *targetStmt, bool &surroundingStatementPreceedsTargetStatement);

//! Relocate comments and CPP directives from one statement to another.
ROSE_DLL_API void
moveCommentsToNewStatement(SgStatement *sourceStatement,
                           const std::vector<int> &indexList,
                           SgStatement *destinationStatement,
                           bool destinationStatementPreceedsSourceStatement);

ROSE_DLL_API bool isTemplateInstantiationNode(SgNode *node);

// DQ (12/1/2015): Adding support for fixup internal data struuctures that have
// references to statements (e.g. macro expansions).
ROSE_DLL_API void
resetInternalMapsForTargetStatement(SgStatement *sourceStatement,
                                    bool statementWillBeDetached = false);

// Replace function-definition source surfaces throughout a subtree with the
// only declaration form that is legal at each lexical owner.
/*!\brief XXX
 * This function operates on the new file used to support outlined function
 * definitions. We use a copy of the file where the code will be outlined FROM,
 * so that if there are references to declarations in the outlined code we can
 * support the outpiled code with those references.  This approach has the added
 * advantage of also supporting the same include file tree as the original file
 * where the outlined code is being taken from.
 */
ROSE_DLL_API void replaceFunctionDefinitionsWithDeclarations(SgNode *node);

/** Replace an exact defining function declaration with a legal declaration
 * source surface.  Free functions and member definitions written in their
 * class are replaced by a newly built prototype.  A member definition written
 * outside its semantic class scope is replaced by SgEmptyDeclaration because
 * a namespace-scope qualified member prototype is ill-formed C++.  The
 * removed definition remains semantically owned by its scope's
 * SgAuxiliaryDeclarationList and its complete declaration family and symbol
 * are preserved.  Malformed or unsupported inputs are hard errors; this
 * function never returns null. */
ROSE_DLL_API SgDeclarationStatement *replaceFunctionDefinitionWithDeclaration(
    SgFunctionDeclaration *functionDefinition,
    bool movePreprocessingInfo = true);
ROSE_DLL_API std::vector<SgFunctionDeclaration *>
generateFunctionDefinitionsList(SgNode *node);

/** Build a nondefining declaration from an exact function definition whose
 * structural and semantic scopes permit a prototype at the same source
 * location.  In particular, out-of-class member definitions are rejected:
 * their existing in-class declaration is the only legal prototype.  Malformed
 * or unsupported inputs are hard errors; this function never returns null. */
ROSE_DLL_API SgFunctionDeclaration *
buildFunctionPrototype(SgFunctionDeclaration *functionDeclaration);

//@}
//------------------------------------------------------------------------
//@{
/*! @name AST repair, fix, and postprocessing.
  \brief Mostly used internally when some AST pieces are built without knowing
  their target scope/parent, especially during bottom-up construction of AST.
  The associated symbols, parent and scope  pointers cannot be set on
  construction then. A set of utility functions are provided to patch up scope,
  parent, symbol for them when the target scope/parent become know.
*/
//! Rebind local variable references after an AST subtree has been moved to a
//! different lexical scope.
/*!
 * This is an explicit transformation operation, not a frontend or
 * post-processing repair. Every reference must already have a real, typed
 * declaration. If a moved local reference cannot be resolved in its new
 * lexical context, the operation terminates with a hard error.
 */
ROSE_DLL_API void rebindVariableReferencesAfterMove(SgNode *root);

//! Patch up symbol, scope, and parent information when a
//! SgVariableDeclaration's scope is known.
/*!
It is possible to build a variable declaration without knowing its scope
information during bottom-up construction of AST, though top-down construction
is recommended in general. In this case, we have to patch up symbol table, scope
and parent information when the scope is known. This function is usually used
internally within appendStatment(), insertStatement().
*/
ROSE_DLL_API void fixVariableDeclaration(SgVariableDeclaration *varDecl,
                                         SgScopeStatement *scope);

//! Fix symbols, parent and scope pointers. Used internally within
//! appendStatment(), insertStatement() etc when a struct declaration was built
//! without knowing its target scope.
ROSE_DLL_API void fixStructDeclaration(SgClassDeclaration *structDecl,
                                       SgScopeStatement *scope);
//! Fix symbols, parent and scope pointers. Used internally within
//! appendStatment(), insertStatement() etc when a class declaration was built
//! without knowing its target scope.
ROSE_DLL_API void fixClassDeclaration(SgClassDeclaration *classDecl,
                                      SgScopeStatement *scope);

//! Fix symbols, parent and scope pointers. Used internally within
//! appendStatment(), insertStatement() etc when a namespace declaration was
//! built without knowing its target scope.
ROSE_DLL_API void
fixNamespaceDeclaration(SgNamespaceDeclarationStatement *structDecl,
                        SgScopeStatement *scope);

//! Fix symbol table for SgLabelStatement. Used Internally when the label is
//! built without knowing its target scope. Both parameters cannot be NULL.
ROSE_DLL_API void fixLabelStatement(SgLabelStatement *label_stmt,
                                    SgScopeStatement *scope);

//! Set a numerical label for a Fortran statement in its exact program-unit
//! label scope. SgLabelSymbol and SgLabelRefExp are created as needed.
ROSE_DLL_API void
setFortranNumericLabel(SgStatement *stmt, int label_value,
                       SgLabelSymbol::label_type_enum label_type,
                       SgScopeStatement *label_scope);

//! Suggest next usable (non-conflicting) numeric label value for a Fortran
//! function definition scope
ROSE_DLL_API int suggestNextNumericLabel(SgFunctionDefinition *func_def);

//! Fix the symbol table and set scope (only if scope in declaration is not
//! already set).
ROSE_DLL_API void fixFunctionDeclaration(SgFunctionDeclaration *stmt,
                                         SgScopeStatement *scope);

//! Fix the symbol table and set scope (only if scope in declaration is not
//! already set).
ROSE_DLL_API void fixTemplateDeclaration(SgTemplateDeclaration *stmt,
                                         SgScopeStatement *scope);

//! A wrapper containing fixes (fixVariableDeclaration(),fixStructDeclaration(),
//! fixLabelStatement(), etc) for all kinds statements. Should be used before
//! attaching the statement into AST.
ROSE_DLL_API void fixStatement(SgStatement *stmt, SgScopeStatement *scope);

// DQ (6/11/2015): This reports the statements that are marked as transformed
// (used to debug the token-based unparsing).
//! This collects the statements that are marked as transformed (useful in
//! debugging).
ROSE_DLL_API std::set<SgStatement *> collectTransformedStatements(SgNode *node);

//! This collects the statements that are marked as modified (a flag
//! automatically set by all set_* generated functions) (useful in debugging).
ROSE_DLL_API std::set<SgStatement *> collectModifiedStatements(SgNode *node);

//! This collects the SgLocatedNodes that are marked as modified (a flag
//! automatically set by all set_* generated functions) (useful in debugging).
ROSE_DLL_API std::set<SgLocatedNode *>
collectModifiedLocatedNodes(SgNode *node);

// DQ (6/5/2019): Use the previously constructed set (above) to reset the IR
// nodes to be marked as isModified.
//! Use the set of IR nodes and set the isModified flag in each IR node to true.
ROSE_DLL_API void
resetModifiedLocatedNodes(const std::set<SgLocatedNode *> &modifiedNodeSet);

// DQ (10/23/2018): Report nodes that are marked as modified.
ROSE_DLL_API void reportModifiedStatements(const std::string &label,
                                           SgNode *node);

// DQ (3/22/2019): Translate CPP directives from attached preprocessor
// information to CPP Directive Declaration IR nodes.
ROSE_DLL_API void translateToUseCppDeclarations(SgNode *n);

ROSE_DLL_API void translateScopeToUseCppDeclarations(SgScopeStatement *scope);

ROSE_DLL_API std::vector<SgC_PreprocessorDirectiveStatement *>
translateStatementToUseCppDeclarations(SgStatement *statement,
                                       SgScopeStatement *scope);
ROSE_DLL_API void printOutComments(SgLocatedNode *locatedNode);
ROSE_DLL_API bool
skipTranslateToUseCppDeclaration(PreprocessingInfo *currentPreprocessingInfo);

// DQ (12/2/2019): Debugging support.
ROSE_DLL_API void outputFileIds(SgNode *node);

//@}

//! Update defining and nondefining links due to a newly introduced function
//! declaration. Should be used after inserting the function into a scope.
/*! This function not only set the defining and nondefining links of the newly
 * introduced function declaration inside a scope, but also update other same
 * function declarations' links accordingly if there are any. Assumption: The
 * function has already inserted/appended/prepended into the scope before
 * calling this function.
 */
ROSE_DLL_API void updateDefiningNondefiningLinks(SgFunctionDeclaration *func,
                                                 SgScopeStatement *scope);

//------------------------------------------------------------------------
//@{
/*! @name Advanced AST transformations, analyses, and optimizations
  \brief Some complex but commonly used AST transformations.
  */

//! Collect all read and write references within stmt, which can be a function,
//! a scope statement, or a single statement. Note that a reference can be both
//! read and written, like i++
ROSE_DLL_API bool collectReadWriteRefs(SgStatement *stmt,
                                       std::vector<SgNode *> &readRefs,
                                       std::vector<SgNode *> &writeRefs,
                                       bool useCachedDefUse = false);

//! Collect unique variables which are read or written within a statement. Note
//! that a variable can be both read and written. The statement can be either of
//! a function, a scope, or a single line statement. For accesses to members of
//! aggregate data, we return the coarse grain aggregate mem obj by default.
ROSE_DLL_API bool collectReadWriteVariables(
    SgStatement *stmt, std::set<SgInitializedName *> &readVars,
    std::set<SgInitializedName *> &writeVars, bool coarseGrain = true);

//! Collect read only variables within a statement. The statement can be either
//! of a function, a scope, or a single line statement. For accesses to members
//! of aggregate data, we return the coarse grain aggregate mem obj by default.
ROSE_DLL_API void
collectReadOnlyVariables(SgStatement *stmt,
                         std::set<SgInitializedName *> &readOnlyVars,
                         bool coarseGrain = true);

//! Collect read only variable symbols within a statement. The statement can be
//! either of a function, a scope, or a single line statement. For accesses to
//! members of aggregate data, we return the coarse grain aggregate mem obj by
//! default.
ROSE_DLL_API void
collectReadOnlySymbols(SgStatement *stmt,
                       std::set<SgVariableSymbol *> &readOnlySymbols,
                       bool coarseGrain = true);

//! Check if a variable reference is used by its address: including &a
//! expression and foo(a) when type2 foo(Type& parameter) in C++
ROSE_DLL_API bool isUseByAddressVariableRef(SgVarRefExp *ref);

//! Collect variable references involving use by address: including &a
//! expression and foo(a) when type2 foo(Type& parameter) in C++
ROSE_DLL_API void
collectUseByAddressVariableRefs(const SgStatement *s,
                                std::set<SgVarRefExp *> &varSetB);

#ifndef ROSE_USE_INTERNAL_FRONTEND_DEVELOPMENT
//! Call liveness analysis on an entire project
ROSE_DLL_API LivenessAnalysis *call_liveness_analysis(SgProject *project,
                                                      bool debug = false);

//! get liveIn and liveOut variables for a for loop from liveness analysis
//! result liv.
ROSE_DLL_API void getLiveVariables(LivenessAnalysis *liv, SgForStatement *loop,
                                   std::set<SgInitializedName *> &liveIns,
                                   std::set<SgInitializedName *> &liveOuts);
#endif

//! Recognize and collect reduction variables and operations within a C/C++
//! loop, following OpenMP 3.0 specification for allowed reduction variable
//! types and operation types.
ROSE_DLL_API void ReductionRecognition(
    SgForStatement *loop,
    std::set<std::pair<SgInitializedName *, OmpSupport::omp_construct_enum>>
        &results);

//! Constant folding an AST subtree rooted at 'r' (replacing its children with
//! their constant values, if applicable). Please be advised that constant
//! folding on floating point computation may decrease the accuracy of floating
//! point computations!
/*! It is a wrapper function for ConstantFolding::constantFoldingOptimization().
 * Note that only r's children are replaced with their corresponding constant
 * values, not the input SgNode r itself. You have to call this upon an
 * expression's parent node if you want to fold the expression. */
ROSE_DLL_API void constantFolding(SgNode *r);

//! Instrument(Add a statement, often a function call) into a function right
//! before the return points, handle multiple return statements (with duplicated
//! statement s) and return expressions with side effects. Return the number of
//! statements inserted.
/*! Useful when adding a runtime library call to terminate the runtime system
 * right before the end of a program (e.g., OpenMP). Return with complex
 * expressions with side effects are rewritten using an additional assignment
 * statement.
 */
ROSE_DLL_API int instrumentEndOfFunction(SgFunctionDeclaration *func,
                                         SgStatement *s);

//! Remove jumps whose label is immediately after the jump.  Used to clean up
//! inlined code fragments.
ROSE_DLL_API void removeJumpsToNextStatement(SgNode *);

//! Remove labels which are not targets of any goto statements: its child
//! statement is also removed by default.
ROSE_DLL_API void removeUnusedLabels(SgNode *top, bool keepChild = false);

//! Find unused labels which are not targets of any goto statements
ROSE_DLL_API std::set<SgLabelStatement *> findUnusedLabels(SgNode *top);

//! Remove consecutive labels
ROSE_DLL_API void removeConsecutiveLabels(SgNode *top);

//! Merge a variable assignment statement into a matching variable declaration
//! statement. Callers should make sure the merge is semantically correct (by
//! not introducing compilation errors). This function simply does the merge
//! transformation, without eligibility check.
/*!
 *  e.g.  int i;  i=10;  becomes int i=10;  the original i=10 will be deleted
 * after the merge if success, return true, otherwise return false (e.g.
 * variable declaration does not match or already has an initializer) The
 * original assignment stmt will be removed by default This function is a bit
 * ambiguous about the merge direction, to be phased out.
 */
ROSE_DLL_API bool mergeDeclarationAndAssignment(SgVariableDeclaration *decl,
                                                SgExprStatement *assign_stmt);

//! Merge an assignment into its upstream declaration statement. Callers should
//! make sure the merge is semantically correct.
ROSE_DLL_API bool mergeAssignmentWithDeclaration(SgExprStatement *assign_stmt,
                                                 SgVariableDeclaration *decl);

//! Merge a declaration statement into a matching followed variable assignment.
//! Callers should make sure the merge is semantically correct (by not
//! introducing compilation errors). This function simply does the merge
//! transformation, without eligibility check.
/*!
 *  e.g.  int i;  i=10;  becomes int i=10;  the original int i; will be deleted
 * after the merge
 */
ROSE_DLL_API bool mergeDeclarationWithAssignment(SgVariableDeclaration *decl,
                                                 SgExprStatement *assign_stmt);

//! Split a variable declaration with an rhs assignment into two statements: a
//! declaration and an assignment.
/*! Return the generated assignment statement, if any
 *  e.g.  int i =10;  becomes int i; i=10;
 *  This can be seen as a normalization of declarations
 */
ROSE_DLL_API SgExprStatement *
splitVariableDeclaration(SgVariableDeclaration *decl);

//! Split declarations within a scope into declarations and assignment
//! statements, by default only top level declarations are considered. Return
//! the number of declarations split.
ROSE_DLL_API int splitVariableDeclaration(SgScopeStatement *scope,
                                          bool topLevelOnly = true);

//! Replace an expression with a temporary variable and an assignment statement
/*!
 Add a new temporary variable to contain the value of 'from'.
 Change reference to 'from' to use this new variable.
 Assumptions: (1)'from' is not within the test of a loop or 'if';
              (2)not currently traversing 'from' or the statement it is in.
 Return value: the new temp variable declaration's assign initializer containing
 the from expression.
 */
ROSE_DLL_API SgAssignInitializer *splitExpression(SgExpression *from,
                                                  std::string newName = "");

//! Split long expressions into blocks of statements
ROSE_DLL_API void splitExpressionIntoBasicBlock(SgExpression *expr);

//! Remove labeled goto statements
ROSE_DLL_API void removeLabeledGotos(SgNode *top);

//! If the given statement contains any break statements in its body, add a new
//! label below the statement and change the breaks into gotos to that new
//! label.
ROSE_DLL_API void changeBreakStatementsToGotos(SgStatement *loopOrSwitch);

//! Check if the body of a 'for' statement is a SgBasicBlock, create one if not.
ROSE_DLL_API SgBasicBlock *ensureBasicBlockAsBodyOfFor(SgForStatement *fs);

//! Check if the body of a 'while' statement is a SgBasicBlock, create one if
//! not.
ROSE_DLL_API SgBasicBlock *ensureBasicBlockAsBodyOfWhile(SgWhileStmt *ws);

//! Check if the body of a 'do .. while' statement is a SgBasicBlock, create one
//! if not.
ROSE_DLL_API SgBasicBlock *ensureBasicBlockAsBodyOfDoWhile(SgDoWhileStmt *ws);

//! Check if the body of a 'switch' statement is a SgBasicBlock, create one if
//! not.
ROSE_DLL_API SgBasicBlock *
ensureBasicBlockAsBodyOfSwitch(SgSwitchStatement *ws);

//! Check if the body of a 'case option' statement is a SgBasicBlock, create one
//! if not.
SgBasicBlock *ensureBasicBlockAsBodyOfCaseOption(SgCaseOptionStmt *cs);

//! Check if the body of a 'default option' statement is a SgBasicBlock, create
//! one if not.
SgBasicBlock *ensureBasicBlockAsBodyOfDefaultOption(SgDefaultOptionStmt *cs);

//! Check if the true body of a 'if' statement is a SgBasicBlock, create one if
//! not.
ROSE_DLL_API SgBasicBlock *ensureBasicBlockAsTrueBodyOfIf(SgIfStmt *ifs);

//! Check if the false body of a 'if' statement is a SgBasicBlock, create one if
//! not when the flag is true.
ROSE_DLL_API SgBasicBlock *
ensureBasicBlockAsFalseBodyOfIf(SgIfStmt *ifs, bool createEmptyBody = true);

//! Check if the body of a 'catch' statement is a SgBasicBlock, create one if
//! not.
ROSE_DLL_API SgBasicBlock *
ensureBasicBlockAsBodyOfCatch(SgCatchOptionStmt *cos);

//! Check if the body of a SgOmpBodyStatement is a SgBasicBlock, create one if
//! not
ROSE_DLL_API SgBasicBlock *
ensureBasicBlockAsBodyOfOmpBodyStmt(SgOmpBodyStatement *ompbodyStmt);

// DQ (1/18/2015): This is added to support better quality token-based
// unparsing.
//! Remove unused basic block IR nodes added as part of normalization.
ROSE_DLL_API void cleanupNontransformedBasicBlockNode();

// DQ (1/18/2015): This is added to support better quality token-based
// unparsing.
//! Record where normalization have been done so that we can preform
//! denormalizations as required for the token-based unparsing to generate
//! minimal diffs.
ROSE_DLL_API void recordNormalizations(SgStatement *s);

//! Convert all code within root matching the patern of (&left)->right, and
//! translate them into left.right.  Return the number of matches of the
//! pattern. Be default, only transformation generated nodes will be normalized.
ROSE_DLL_API int normalizeArrowExpWithAddressOfLeftOperand(
    SgNode *root, bool transformationGeneratedOnly = true);

//! Check if a statement is a (true or false) body of a container-like parent,
//! such as For, Do-while, switch, If, Catch, OmpBodyStmt, etc
bool isBodyStatement(SgStatement *s);

//! Fix up ifs, loops, while, switch, Catch, OmpBodyStatement, etc. to have
//! blocks as body components. It also adds an empty else body to if statements
//! that don't have them.
void changeAllBodiesToBlocks(SgNode *top, bool createEmptyBody = true);

// The same as changeAllBodiesToBlocks(SgNode* top). Phased out.
// void changeAllLoopBodiesToBlocks(SgNode* top);

//! Make a single statement body to be a basic block. Its parent is if, while,
//! catch, etc.
SgBasicBlock *makeSingleStatementBodyToBlock(SgStatement *singleStmt);

//! Get the constant value from a constant integer expression; abort on
//! everything else.  Note that signed long longs are converted to unsigned.
unsigned long long getIntegerConstantValue(SgValueExp *expr);

//! Get a statement's dependent declarations which declares the types used in
//! the statement. The returned vector of declaration statements are sorted
//! according to their appearance order in the original AST. Any reference to a
//! class or template class from a namespace will treated as a reference to the
//! enclosing namespace.
std::vector<SgDeclarationStatement *>
getDependentDeclarations(SgStatement *stmt);

//! Insert an expression (new_exp )before another expression (anchor_exp) has
//! possible side effects, without changing the original semantics. This is
//! achieved by using a comma operator: (new_exp, anchor_exp). The comma
//! operator is returned.
SgCommaOpExp *insertBeforeUsingCommaOp(SgExpression *new_exp,
                                       SgExpression *anchor_exp);

//! Insert an expression (new_exp ) after another expression (anchor_exp) has
//! possible side effects, without changing the original semantics. This is done
//! by using two comma operators:  type T1; ... ((T1 = anchor_exp, new_exp),T1)
//! )... , where T1 is a temp variable saving the possible side effect of
//! anchor_exp. The top level comma op exp is returned. The reference to T1 in
//! T1 = anchor_exp is saved in temp_ref.
SgCommaOpExp *insertAfterUsingCommaOp(SgExpression *new_exp,
                                      SgExpression *anchor_exp,
                                      SgStatement **temp_decl = NULL,
                                      SgVarRefExp **temp_ref = NULL);

/// \brief   moves the body of a function f to a new function f`;
///          f's body is replaced with code that forwards the call to f`.
/// \return  a pair indicating the statement containing the call of f`
///          and an initialized name refering to the temporary variable
///          holding the result of f`. In case f returns void
///          the initialized name is NULL.
/// \param   definingDeclaration the defining function declaration of f
/// \param   newName the name of function f`
/// \details f's new body becomes { f`(...); } and { int res = f`(...); return
/// res; }
///          for functions returning void and a value, respectively.
///          two function declarations are inserted in f's enclosing scope
/// @code
///          result_type f`(...);                       <--- (1)
///          result_type f (...) { forward call to f` }
///          result_type f`(...) { original code }      <--- (2)
/// @endcode
///          Calls to f are not updated, thus in the transformed code all
///          calls will continue calling f (this is also true for
///          recursive function calls from within the body of f`).
///          After the function has created the wrapper,
///          definingDeclaration becomes the wrapper function
///          The definition of f` is the next entry in the
///          statement list; the forward declaration of f` is the previous
///          entry in the statement list.
/// \pre     definingDeclaration must be a defining declaration of a
///          free standing function.
///          typeid(SgFunctionDeclaration) == typeid(definingDeclaration)
///          i.e., this function is NOT implemented for class member functions,
///          template functions, procedures, etc.
std::pair<SgStatement *, SgInitializedName *>
wrapFunction(SgFunctionDeclaration &definingDeclaration, SgName newName);

/// \overload
/// \tparam  NameGen functor that generates a new name based on the old name.
///          interface: SgName nameGen(const SgName&)
/// \param   nameGen name generator
/// \brief   see wrapFunction for details
template <class NameGen>
std::pair<SgStatement *, SgInitializedName *>
wrapFunction(SgFunctionDeclaration &definingDeclaration, NameGen nameGen) {
  return wrapFunction(definingDeclaration,
                      nameGen(definingDeclaration.get_name()));
}

/// \brief convenience function that returns the first initialized name in a
///        list of variable declarations.
SgInitializedName &getFirstVariable(SgVariableDeclaration &vardecl);

//@}

// DQ (6/7/2012): Unclear where this function should go...
bool hasTemplateSyntax(const SgName &name);

// DQ (1/23/2013): Added support for generated a set of source sequence entries.
std::set<unsigned int> collectSourceSequenceNumbers(SgNode *astNode);

//--------------------------------Type Traits (C++)---------------------------
bool HasNoThrowAssign(const SgType *const inputType);
bool HasNoThrowCopy(const SgType *const inputType);
bool HasNoThrowConstructor(const SgType *const inputType);
bool HasTrivialAssign(const SgType *const inputType);
bool HasTrivialCopy(const SgType *const inputType);
bool HasTrivialConstructor(const SgType *const inputType);
bool HasTrivialDestructor(const SgType *const inputType);
bool HasVirtualDestructor(const SgType *const inputType);
bool IsBaseOf(const SgType *const inputBaseType,
              const SgType *const inputDerivedType);
bool IsAbstract(const SgType *const inputType);
//! strip off typedef and modifer types, then check if a type is a class type,
//! excluding union type.
bool IsClass(const SgType *const inputType);
bool IsEmpty(const SgType *const inputType);
bool IsEnum(const SgType *const inputType);
bool IsPod(const SgType *const inputType);
bool IsPolymorphic(const SgType *const inputType);
bool IsStandardLayout(const SgType *const inputType);
bool IsLiteralType(const SgType *const inputType);
bool IsTrivial(const SgType *const inputType);
bool IsUnion(const SgType *const inputType);
SgType *UnderlyingType(SgType *type);

// DQ (3/2/2014): Added a new interface function (used in the snippet insertion
// support).
//   void supportForInitializedNameLists ( SgScopeStatement* scope,
//   SgInitializedNamePtrList & variableList );

// DQ (3/4/2014): Added support for testing two trees for equivalents using the
// AST iterators.
bool isStructurallyEquivalentAST(SgNode *tree1, SgNode *tree2);

// JP (10/14/24): Moved code to evaluate a const integer expression (like in
// array size definitions) to SageInterface
/*! The datastructure is used as the return type for
 * SageInterface::evaluateConstIntegerExpression(). One needs to always check
 * whether hasValue_ is true before accessing value_ */
struct const_int_expr_t {
  size_t value_;
  bool hasValue_;
};
/*! \brief The function tries to evaluate const integer expressions (such as are
 * used in array dimension sizes). It follows variable symbols, and requires
 * constness. */
struct const_int_expr_t evaluateConstIntegerExpression(SgExpression *expr);

// JP (9/17/14): Added function to test whether two SgType* are equivalent or
// not
bool checkTypesAreEqual(SgType *typeA, SgType *typeB);

void detectCycleInType(SgType *type, const std::string &from);

// DQ (7/14/2020): Debugging support.
void checkForInitializers(SgNode *node);

void clearSharedGlobalScopes(SgProject *project);

} // namespace SageInterface

#endif
