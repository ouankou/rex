
#ifndef CALL_GRAPH_H
#define CALL_GRAPH_H

#include "GraphDotOutput.h"

#include "VirtualGraphCreate.h"

#include "nodeQuery.h"

#include "AstDiagnostics.h"

#include <functional>

#include <iostream>

#include <queue>

#include <sstream>

#include <string>

#include <unordered_map>

#include <utility>

#include <vector>

class FunctionData;

// DQ (1/31/2006): Changed name and made global function type symbol table a
// static data member. extern SgFunctionTypeTable Sgfunc_type_table; This header
// has to be here since it uses type SgFunctionDeclarationPtrList
#include "ClassHierarchyGraph.h"

// AS(090707) Added the CallTargetSet namespace to replace the
// CallGraphFunctionSolver class
namespace CallTargetSet {
/**
 * Return the source expression that denotes a callee after validating and
 * traversing only compiler-synthesized implicit callee conversions.  Explicit
 * casts and all non-conversion call surfaces remain in the returned syntax.
 */
ROSE_DLL_API SgExpression *
unwrapExactImplicitCalleeConversions(SgExpression *functionExpression);

/**
 * CallTargetSet::solveFunctionPointerCall
 *
 * \brief Finds all functions that match the function type of pointerDerefExp
 *
 * Resolving function pointer calls is hard, so the CallGraph generator doesn't
 * try very hard at it.  When asked to resolve a function pointer call, it
 *simply finds all functions that match that type in the memory pool a returns a
 *list of them.
 *
 * @param[in] pointerDerefExp : A function pointer dereference.
 * @return: A vector of all functionDeclarations that match the type of the
 *function dereferenced in pointerDerefExp
 **/
std::vector<SgFunctionDeclaration *>
solveFunctionPointerCall(SgPointerDerefExp *pointerDerefExp);

// returns the list of declarations of all functions that may get called via a
// member function pointer
std::vector<SgFunctionDeclaration *>
solveMemberFunctionPointerCall(SgExpression *, ClassHierarchyWrapper *);

/**
 * CallTargetSet::solveFunctionPointerCallsFunctional
 *
 * \brief Checks if the functionDeclaration (node) matches functionType
 *
 * This is a filter called by solveFunctionPointerCall. It checks that node is
 * a functiondeclaration (or template instantiation) of type functionType.
 * If it does, it is added to a functionList and returned.  So function list can
 * have at most 1 entry.
 *
 * @param[in] node : The node we are checking.  It must be an
 *SgFunctionDeclaration
 * @param[in] functionType : The function type being checked.
 * @return: If node matched functionType, it is added on functionList and
 *returned.  Otherwise functionList is empty.
 **/
Rose_STL_Container<SgFunctionDeclaration *>
solveFunctionPointerCallsFunctional(SgNode *node, SgFunctionType *functionType);

// returns the list of declarations of all functions that may get called via a
// member function (non/polymorphic) call
std::vector<SgFunctionDeclaration *>
solveMemberFunctionCall(SgClassType *, ClassHierarchyWrapper *,
                        SgMemberFunctionDeclaration *, bool,
                        bool includePureVirtualFunc = false);

//! Returns the list of all constructors that may get called via an
//! initialization.
std::vector<SgFunctionDeclaration *>
solveConstructorInitializer(SgConstructorInitializer *sgCtorInit);

// Populates functionList with Properties of all functions that may get called.
ROSE_DLL_API void getPropertiesForExpression(
    SgExpression *exp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &propList,
    bool includePureVirtualFunc = false);

//! Populates functionList with definitions of all functions that may get
//! called. This is basically a wrapper around getPropertiesForExpression that
//! extracts the SgFunctionDefinition from the Properties object. This returns
//! only callees that have definitions - to get all possible callees, use
//! getDeclarationsForExpression
void getDefinitionsForExpression(
    SgExpression *exp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDefinition *> &calleeList);

//! Populates functionList with declarations of all functions that may get
//! called. This is basically a wrapper around getPropertiesForExpression.
void getDeclarationsForExpression(
    SgExpression *exp, ClassHierarchyWrapper *classHierarchy,
    Rose_STL_Container<SgFunctionDeclaration *> &calleeList,
    bool includePureVirtualFunc = false);

// Gets a vector of SgExpressions that are associated with the current
// SgFunctionDefinition. This functionality is necessary for virtual,
// interprocedural control flow graphs. However, it is costly and should be used
// infrequently (or optimized!).
void getExpressionsForDefinition(SgFunctionDefinition *targetDef,
                                 ClassHierarchyWrapper *classHierarchy,
                                 Rose_STL_Container<SgExpression *> &exps);

// Gets the latest implementation of the member function from the ancestor
// hierarchy
SgFunctionDeclaration *getFirstVirtualFunctionDefinitionFromAncestors(
    SgClassType *crtClass,
    SgMemberFunctionDeclaration *memberFunctionDeclaration,
    ClassHierarchyWrapper *classHierarchy);

}; // namespace CallTargetSet

class ROSE_DLL_API FunctionData {
public:
  bool hasDefinition;

  bool isDefined();

  FunctionData(SgFunctionDeclaration *functionDeclaration, SgProject *project,
               ClassHierarchyWrapper *);

  //! All the callees of this function
  Rose_STL_Container<SgFunctionDeclaration *> functionList;

  SgFunctionDeclaration *functionDeclaration;

  Rose_STL_Container<SgMemberFunctionDeclaration *> *
  findPointsToVirtualFunctions(SgMemberFunctionDeclaration *);
  bool compareFunctionDeclarations(SgFunctionDeclaration *f1,
                                   SgFunctionDeclaration *f2);
};

//! A function object to be used as a predicate to filter out functions in a
//! call graph: it does not filter out anything.
struct dummyFilter {
  using result_type = bool;
  result_type
  operator()(SgFunctionDeclaration *node) const; // always return true
};

//! A function object to filter out builtin functions in a call graph (only
//! non-builtin functions will be considered)
// Liao, 6/17/2012
struct ROSE_DLL_API builtinFilter {
  using result_type = bool;
  result_type operator()(SgFunctionDeclaration *node) const;
};

class ROSE_DLL_API CallGraphBuilder {
public:
  CallGraphBuilder(SgProject *proj);
  //! Default builder filtering nothing in the call graph
  void buildCallGraph();
  //! Builder accepting user defined predicate to filter certain functions
  template <typename Predicate> void buildCallGraph(Predicate pred);
  //! Grab the call graph built
  SgIncidenceDirectedGraph *getGraph();
  // void classifyCallGraph();

  // We map each function to the corresponding graph node
  std::unordered_map<SgFunctionDeclaration *, SgGraphNode *> &
  getGraphNodesMapping() {
    return graphNodes;
  }
  void addGraphNodeMapping(SgFunctionDeclaration *fdecl,
                           SgGraphNode *graphNode);

  //! Retrieve the node matching a function declaration using
  //! firstNondefiningDeclaration (does not work across translation units)
  SgGraphNode *hasGraphNodeFor(SgFunctionDeclaration *fdecl) const;
  //! Retrieve the node matching a function declaration (using mangled name to
  //! resolve across translation units)
  SgGraphNode *getGraphNodeFor(SgFunctionDeclaration *fdecl) const;

private:
  SgProject *project;
  SgIncidenceDirectedGraph *graph;
  // We map each function to the corresponding graph node
  typedef std::unordered_map<SgFunctionDeclaration *, SgGraphNode *> GraphNodes;
  GraphNodes graphNodes;
  typedef std::vector<std::pair<SgFunctionDeclaration *, SgGraphNode *>>
      GraphNodesByMangledNameList;
  typedef std::unordered_map<std::string, GraphNodesByMangledNameList>
      GraphNodesByMangledName;
  GraphNodesByMangledName graphNodesByMangledName;
  void registerGraphNode(SgFunctionDeclaration *fdecl, SgGraphNode *graphNode);
  SgGraphNode *getGraphNodeForMangledName(SgFunctionDeclaration *fdecl,
                                          bool requireSourceIdentity) const;
  SgGraphNode *getGraphNodeForConstruction(SgFunctionDeclaration *fdecl) const;
  bool shouldMaterializeImplicitCallTarget(SgFunctionDeclaration *fdecl) const;
  bool shouldMaterializeResolvedCallTarget(SgFunctionDeclaration *fdecl) const;
  SgGraphNode *
  ensureGraphNodeForImplicitCallTarget(SgFunctionDeclaration *fdecl);
  SgGraphNode *
  ensureGraphNodeForResolvedCallTarget(SgFunctionDeclaration *fdecl);
  void materializeNonFunctionImplicitCallTargets(
      ClassHierarchyWrapper *classHierarchy);
};
//! Generate a dot graph named 'fileName' from a call graph
// TODO this function is    not defined? If so, need to be removed.
//  AstDOTGeneration::writeIncidenceGraphToDOTFile() is used instead in the
//  tutorial. Liao 6/17/2012
void GenerateDotGraph(SgIncidenceDirectedGraph *graph, std::string fileName);

struct GetOneFuncDeclarationPerFunction {
  using result_type = Rose_STL_Container<SgNode *>;
  result_type operator()(SgNode *node);
};

static inline SgFunctionDeclaration *
callGraphCanonicalDeclarationChain(SgFunctionDeclaration *fdecl) {
  if (SgFunctionDeclaration *defDecl =
          isSgFunctionDeclaration(fdecl->get_definingDeclaration())) {
    return defDecl;
  }

  if (SgFunctionDeclaration *firstNondef =
          isSgFunctionDeclaration(fdecl->get_firstNondefiningDeclaration())) {
    return firstNondef;
  }

  return fdecl;
}

static inline bool
callGraphDeclIsBlockScopePrototype(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL || fdecl->get_definition() != NULL ||
      fdecl->get_definingDeclaration() != NULL ||
      isSgMemberFunctionDeclaration(fdecl) != NULL ||
      isSgTemplateFunctionDeclaration(fdecl) != NULL ||
      isSgTemplateMemberFunctionDeclaration(fdecl) != NULL) {
    return false;
  }

  for (SgNode *cursor = fdecl->get_parent(); cursor != NULL;
       cursor = cursor->get_parent()) {
    if (isSgFunctionDefinition(cursor) != NULL) {
      return true;
    }
    if (isSgGlobal(cursor) != NULL ||
        isSgNamespaceDefinitionStatement(cursor) != NULL ||
        isSgClassDefinition(cursor) != NULL ||
        isSgTemplateClassDefinition(cursor) != NULL ||
        isSgTemplateInstantiationDefn(cursor) != NULL) {
      return false;
    }
  }

  return false;
}

static inline SgFunctionDeclaration *
callGraphLookupNamespaceFunctionForBlockPrototype(
    SgFunctionDeclaration *fdecl) {
  if (!callGraphDeclIsBlockScopePrototype(fdecl)) {
    return NULL;
  }

  for (SgNode *cursor = fdecl->get_parent(); cursor != NULL;
       cursor = cursor->get_parent()) {
    SgScopeStatement *scope = isSgScopeStatement(cursor);
    if (scope == NULL || (isSgGlobal(scope) == NULL &&
                          isSgNamespaceDefinitionStatement(scope) == NULL)) {
      continue;
    }

    SgFunctionSymbol *symbol = scope->lookup_nontemplate_function_symbol(
        fdecl->get_name(), fdecl->get_type(), NULL);
    if (symbol == NULL) {
      symbol = isSgFunctionSymbol(
          scope->lookup_function_symbol(fdecl->get_name(), fdecl->get_type()));
    }
    if (symbol == NULL) {
      continue;
    }

    SgFunctionDeclaration *decl =
        isSgFunctionDeclaration(symbol->get_declaration());
    if (decl != NULL && decl != fdecl) {
      return callGraphCanonicalDeclarationChain(decl);
    }
  }

  return NULL;
}

static inline SgFunctionDeclaration *
canonicalFunctionDeclForCallGraph(SgFunctionDeclaration *fdecl) {
  ROSE_ASSERT(fdecl != NULL);

  SgFunctionDeclaration *canonical = callGraphCanonicalDeclarationChain(fdecl);
  if (canonical != fdecl) {
    return canonical;
  }

  if (SgFunctionDeclaration *namespace_decl =
          callGraphLookupNamespaceFunctionForBlockPrototype(fdecl)) {
    return namespace_decl;
  }

  return fdecl;
}

static inline bool
callGraphDeclHasUsableFilename(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL || fdecl->get_file_info() == NULL) {
    return false;
  }

  Sg_File_Info *file_info = fdecl->get_file_info();
  return file_info->get_physical_file_id() >= 0 &&
         !file_info->get_physical_filename().empty();
}

using CallGraphExplicitInstantiationSourceIndex =
    std::unordered_map<SgFunctionDeclaration *, SgFunctionDeclaration *>;

static inline bool callGraphNodeBelongsToProject(const SgNode *node,
                                                 const SgProject *project) {
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(project != NULL);

  for (const SgNode *owner = node; owner != NULL; owner = owner->get_parent()) {
    if (owner == project) {
      return true;
    }
  }

  return false;
}

static inline CallGraphExplicitInstantiationSourceIndex
buildCallGraphExplicitInstantiationSourceIndex(SgProject *project) {
  ROSE_ASSERT(project != NULL);

  CallGraphExplicitInstantiationSourceIndex result;
  VariantVector functionVariants(V_SgFunctionDeclaration);
  for (SgNode *node : NodeQuery::queryMemoryPool(functionVariants)) {
    SgFunctionDeclaration *declaration = isSgFunctionDeclaration(node);
    SgTemplateInstantiationDirectiveStatement *directive =
        declaration != NULL ? isSgTemplateInstantiationDirectiveStatement(
                                  declaration->get_parent())
                            : NULL;
    if (directive == NULL ||
        !callGraphNodeBelongsToProject(directive, project)) {
      continue;
    }

    Sg_File_Info *directiveInfo = directive->get_file_info();
    const bool sourceDirective =
        directiveInfo != NULL && directiveInfo->get_line() > 0 &&
        directiveInfo->get_col() > 0 && !directiveInfo->isCompilerGenerated() &&
        !directiveInfo->isFrontendSpecific() &&
        !directiveInfo->isTransformation();
    if (!sourceDirective) {
      continue;
    }
    if (!callGraphDeclHasUsableFilename(declaration)) {
      std::cerr
          << "REX_CALLGRAPH_INVARIANT[explicit-instantiation-source]: source "
             "template-instantiation directive owns a declaration without "
             "exact source identity"
          << std::endl;
      ROSE_ABORT();
    }

    SgFunctionDeclaration *canonical =
        canonicalFunctionDeclForCallGraph(declaration);
    result.emplace(canonical, declaration);
  }

  return result;
}

static inline SgFunctionDeclaration *predicateFunctionDeclForCallGraph(
    SgFunctionDeclaration *fdecl,
    const CallGraphExplicitInstantiationSourceIndex
        &explicitInstantiationSources) {
  ROSE_ASSERT(fdecl != NULL);

  auto exactSourceDeclarationInFamily =
      [](SgFunctionDeclaration *declaration) -> SgFunctionDeclaration * {
    if (declaration == NULL) {
      return NULL;
    }
    SgFunctionDeclaration *canonical =
        canonicalFunctionDeclForCallGraph(declaration);
    if (callGraphDeclHasUsableFilename(canonical)) {
      return canonical;
    }
    if (SgFunctionDeclaration *firstNondef = isSgFunctionDeclaration(
            canonical->get_firstNondefiningDeclaration())) {
      if (callGraphDeclHasUsableFilename(firstNondef)) {
        return firstNondef;
      }
    }
    if (SgFunctionDeclaration *def =
            isSgFunctionDeclaration(canonical->get_definingDeclaration())) {
      if (callGraphDeclHasUsableFilename(def)) {
        return def;
      }
    }
    return NULL;
  };

  SgFunctionDeclaration *canonical = canonicalFunctionDeclForCallGraph(fdecl);
  if (SgFunctionDeclaration *source =
          exactSourceDeclarationInFamily(canonical)) {
    return source;
  }

  CallGraphExplicitInstantiationSourceIndex::const_iterator explicitSource =
      explicitInstantiationSources.find(canonical);
  if (explicitSource != explicitInstantiationSources.end()) {
    return explicitSource->second;
  }

  if (SgFunctionDeclaration *source = exactSourceDeclarationInFamily(
          canonical->get_templateInstantiationPattern())) {
    return source;
  }

  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(canonical)) {
    if (SgFunctionDeclaration *source = exactSourceDeclarationInFamily(
            instantiation->get_templateDeclaration())) {
      return source;
    }
  } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                 isSgTemplateInstantiationMemberFunctionDecl(canonical)) {
    if (SgFunctionDeclaration *source = exactSourceDeclarationInFamily(
            instantiation->get_templateDeclaration())) {
      return source;
    }
  }

  return canonical;
}

static inline bool
callGraphDeclHasDefinitionOrDefiningDeclaration(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return false;
  }

  if (fdecl->get_definition() != NULL) {
    return true;
  }

  SgFunctionDeclaration *def_decl =
      isSgFunctionDeclaration(fdecl->get_definingDeclaration());
  return def_decl != NULL && def_decl->get_definition() != NULL;
}

static inline bool
callGraphDeclOwnsExactSourceDefinition(SgFunctionDeclaration *fdecl) {
  if (fdecl == NULL) {
    return false;
  }

  SgFunctionDeclaration *defining =
      fdecl->get_definition() != NULL
          ? fdecl
          : isSgFunctionDeclaration(fdecl->get_definingDeclaration());
  if (defining == NULL || defining->get_definition() == NULL ||
      isSgAuxiliaryDeclarationList(defining->get_parent()) != NULL ||
      isSgTemplateInstantiationDirectiveStatement(defining->get_parent()) !=
          NULL) {
    return false;
  }

  Sg_File_Info *file_info = defining->get_file_info();
  return file_info != NULL && file_info->get_line() > 0 &&
         file_info->get_col() > 0 && !file_info->isCompilerGenerated() &&
         !file_info->isFrontendSpecific() && !file_info->isTransformation();
}

static inline bool callGraphInstantiatedMemberHasClassTemplateArguments(
    SgTemplateInstantiationMemberFunctionDecl *inst_member) {
  if (inst_member == NULL) {
    return false;
  }

  SgTemplateInstantiationDecl *associated_class = isSgTemplateInstantiationDecl(
      inst_member->get_associatedClassDeclaration());
  if (associated_class == NULL) {
    return false;
  }

  return !associated_class->get_templateArguments().empty();
}

ROSE_DLL_API bool
callGraphDeclHasIndependentCallableIdentity(SgFunctionDeclaration *fdecl);

template <typename Predicate>
void CallGraphBuilder::buildCallGraph(Predicate pred) {
  const CallGraphExplicitInstantiationSourceIndex explicitInstantiationSources =
      buildCallGraphExplicitInstantiationSourceIndex(project);

  // Adds additional constraints to the predicate. It makes no sense to analyze
  // non-instantiated templates.
  struct isSelected {
    Predicate &pred;
    const CallGraphExplicitInstantiationSourceIndex
        &explicitInstantiationSources;
    isSelected(Predicate &pred, const CallGraphExplicitInstantiationSourceIndex
                                    &explicitInstantiationSources)
        : pred(pred),
          explicitInstantiationSources(explicitInstantiationSources) {}
    bool operator()(SgNode *node) {
      SgFunctionDeclaration *f = isSgFunctionDeclaration(node);
      // TV (10/26/2018): FIXME ROSE-1487
      // assert(!f || f==f->get_firstNondefiningDeclaration()); // node
      // uniqueness test
      if (f == nullptr || f != canonicalFunctionDeclForCallGraph(f) ||
          isSgTemplateMemberFunctionDeclaration(f) ||
          isSgTemplateFunctionDeclaration(f)) {
        return false;
      }
      if (callGraphDeclHasIndependentCallableIdentity(
              f->get_templateInstantiationPattern())) {
        // A callable source pattern already has the graph identity for this
        // semantic product (notably a hidden friend instantiated with its
        // enclosing class). Instantiated members whose dependent source
        // pattern is not independently callable continue below and retain
        // their concrete specialization identity.
        return false;
      }
      Sg_File_Info *functionFileInfo = f->get_file_info();
      const bool semanticConstructor =
          f->get_specialFunctionModifier().isConstructor() &&
          functionFileInfo != nullptr &&
          functionFileInfo->isCompilerGenerated() &&
          functionFileInfo->isFrontendSpecific() &&
          !callGraphDeclOwnsExactSourceDefinition(f);
      if (semanticConstructor) {
        const auto origin = f->get_frontend_declaration_origin();
        if (origin ==
            SgFunctionDeclaration::e_frontend_declaration_unclassified) {
          fprintf(stderr,
                  "REX_CALLGRAPH_INVARIANT[function-declaration-origin]: "
                  "semantic constructor=%p/%s name=%s has no exact "
                  "producer-published explicit/implicit origin\n",
                  static_cast<void *>(f), f->class_name().c_str(),
                  f->get_qualified_name().getString().c_str());
          ROSE_ABORT();
        }
        if (origin == SgFunctionDeclaration::e_frontend_declaration_implicit) {
          // An implicit special member has no independent source declaration.
          // It can enter the graph only when a construction expression
          // resolves to it.
          return false;
        }
        if (origin != SgFunctionDeclaration::e_frontend_declaration_explicit) {
          ROSE_ABORT();
        }
        // An explicitly source-declared constructor instantiated into
        // semantic storage still owns an independent callable identity.  The
        // predicate below selects it through its exact source pattern.
      }

      SgFunctionDeclaration *pred_decl =
          predicateFunctionDeclForCallGraph(f, explicitInstantiationSources);

      SgFunctionDeclaration *def_decl =
          isSgFunctionDeclaration(pred_decl->get_definingDeclaration());
      bool has_definition = pred_decl->get_definition() != NULL;
      if (!has_definition && def_decl != NULL) {
        has_definition = def_decl->get_definition() != NULL;
      }

      if (SgTemplateInstantiationFunctionDecl *inst_func =
              isSgTemplateInstantiationFunctionDecl(f)) {
        if (inst_func->get_templateArguments().empty() &&
            inst_func->get_deducedTemplateArguments().empty() &&
            !has_definition) {
          return false;
        }
      } else if (SgTemplateInstantiationMemberFunctionDecl *inst_member =
                     isSgTemplateInstantiationMemberFunctionDecl(f)) {
        if (inst_member->get_templateArguments().empty() &&
            inst_member->get_deducedTemplateArguments().empty() &&
            !has_definition &&
            !callGraphInstantiatedMemberHasClassTemplateArguments(
                inst_member)) {
          return false;
        }
      }

      return pred(pred_decl);
    }
  };

  // Add nodes to the graph by querying the memory pool for function
  // declarations, mapping them to unique declarations that can be used as keys
  // in a map (using get_firstNondefiningDeclaration()), and filtering according
  // to the predicate.
  graph = new SgIncidenceDirectedGraph();
  std::vector<FunctionData> callGraphData;
  ClassHierarchyWrapper classHierarchy(project);
  graphNodes.clear();
  graphNodesByMangledName.clear();
  VariantVector vv(V_SgFunctionDeclaration);
  GetOneFuncDeclarationPerFunction defFunc;
  std::vector<SgNode *> fdecl_nodes = NodeQuery::queryMemoryPool(defFunc, &vv);
  std::set<SgFunctionDeclaration *> selectedDeclarations;
  isSelected select(pred, explicitInstantiationSources);
  for (SgNode *node : fdecl_nodes) {
    SgFunctionDeclaration *fdecl = isSgFunctionDeclaration(node);
    SgFunctionDeclaration *unique = canonicalFunctionDeclForCallGraph(fdecl);
    const bool selected = select(unique);
    if (selected && selectedDeclarations.insert(unique).second) {
      FunctionData fdata(
          unique, project,
          &classHierarchy); // computes functions called by unique
      callGraphData.push_back(fdata);
    }
  }

  // A template specialization whose body is instantiated into semantic-only
  // storage is not itself a source definition.  It becomes a graph vertex only
  // when a source call resolves to it.  Compute that reachability from the
  // complete candidate set before adding vertices so called specializations
  // retain their own outgoing edges while unused explicit/implicit
  // instantiations do not appear as invented source functions.
  std::set<SgFunctionDeclaration *> resolvedCallees;
  for (const FunctionData &function : callGraphData) {
    for (SgFunctionDeclaration *callee : function.functionList) {
      if (callee != NULL) {
        resolvedCallees.insert(canonicalFunctionDeclForCallGraph(callee));
      }
    }
  }
  VariantVector referenceVariants(V_SgTemplateFunctionRefExp);
  referenceVariants.push_back(V_SgTemplateMemberFunctionRefExp);
  referenceVariants.push_back(V_SgNonrealRefExp);
  for (SgNode *node : NodeQuery::queryMemoryPool(referenceVariants)) {
    SgFunctionDeclaration *resolved = NULL;
    if (SgTemplateFunctionRefExp *reference =
            isSgTemplateFunctionRefExp(node)) {
      resolved = reference->get_semantic_function_declaration();
    } else if (SgTemplateMemberFunctionRefExp *reference =
                   isSgTemplateMemberFunctionRefExp(node)) {
      resolved = reference->get_semantic_member_function_declaration();
    } else if (SgNonrealRefExp *reference = isSgNonrealRefExp(node)) {
      resolved = reference->get_resolved_function_declaration();
    }
    if (resolved != NULL) {
      resolvedCallees.insert(canonicalFunctionDeclForCallGraph(resolved));
    }
  }

  for (const FunctionData &function : callGraphData) {
    SgFunctionDeclaration *unique =
        canonicalFunctionDeclForCallGraph(function.functionDeclaration);
    bool semanticFunctionInstantiation = false;
    if (SgTemplateInstantiationFunctionDecl *instantiation =
            isSgTemplateInstantiationFunctionDecl(unique)) {
      semanticFunctionInstantiation =
          !instantiation->get_templateArguments().empty() ||
          !instantiation->get_deducedTemplateArguments().empty();
    } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                   isSgTemplateInstantiationMemberFunctionDecl(unique)) {
      semanticFunctionInstantiation =
          !instantiation->get_templateArguments().empty() ||
          !instantiation->get_deducedTemplateArguments().empty();
    }
    if (semanticFunctionInstantiation &&
        !callGraphDeclOwnsExactSourceDefinition(unique) &&
        resolvedCallees.count(unique) == 0) {
      continue;
    }
    if (getGraphNodeForConstruction(unique) == NULL) {
      std::string functionName = unique->get_qualified_name().getString();
      SgGraphNode *graphNode = new SgGraphNode(functionName);
      graphNode->set_SgNode(unique);
      registerGraphNode(unique, graphNode);
      graph->addNode(graphNode);
    }
  }

  materializeNonFunctionImplicitCallTargets(&classHierarchy);

  // Add edges to the graph
  for (FunctionData &currentFunction : callGraphData) {
    SgFunctionDeclaration *curFuncDecl = currentFunction.functionDeclaration;
    std::string curFuncName = curFuncDecl->get_qualified_name().getString();
    SgGraphNode *srcNode = hasGraphNodeFor(currentFunction.functionDeclaration);
    if (srcNode == NULL) {
      continue;
    }
    std::vector<SgFunctionDeclaration *> &callees =
        currentFunction.functionList;
    for (SgFunctionDeclaration *callee : callees) {
      SgFunctionDeclaration *callee_unique =
          canonicalFunctionDeclForCallGraph(callee);
      SgGraphNode *dstNode = getGraphNodeFor(
          callee_unique); // getGraphNode here, see function comment
      if (dstNode == NULL) {
        dstNode = ensureGraphNodeForImplicitCallTarget(callee_unique);
      }
      if (dstNode == NULL) {
        dstNode = ensureGraphNodeForResolvedCallTarget(callee_unique);
      }
      if (dstNode == NULL)
        continue;
      if (graph->checkIfDirectedGraphEdgeExists(srcNode, dstNode) == false)
        graph->addDirectedEdge(srcNode, dstNode);
    }
  }
}

// endif for CALL_GRAPH_H
#endif
