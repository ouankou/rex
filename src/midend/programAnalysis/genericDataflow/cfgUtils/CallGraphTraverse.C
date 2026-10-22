#include "featureTests.h"
#ifdef ROSE_ENABLE_SOURCE_ANALYSIS

#include "CallGraphTraverse.h"

#include "cfgUtils.h"

#include <functional>
#include <set>
using namespace std;
using namespace Rose;

bool call_graph_traversal_includes_function_decl(SgFunctionDeclaration *decl) {
  if (decl == NULL) {
    return false;
  }

  const std::optional<bool> source_ownership =
      callGraphFunctionHasUserSourceOwnership(decl);
  Sg_File_Info *file_info = decl->get_file_info();
  if (source_ownership.value_or(file_info == NULL ||
                                !file_info->isCompilerGenerated())) {
    return true;
  }

  if (SgTemplateInstantiationFunctionDecl *tpl_func =
          isSgTemplateInstantiationFunctionDecl(decl)) {
    SgTemplateFunctionDeclaration *template_decl =
        tpl_func->get_templateDeclaration();
    if (template_decl == NULL) {
      return false;
    }
    Sg_File_Info *pattern_info = template_decl->get_file_info();
    return callGraphFunctionHasUserSourceOwnership(template_decl)
        .value_or(pattern_info != NULL && !pattern_info->isCompilerGenerated());
  }

  if (SgTemplateInstantiationMemberFunctionDecl *tpl_member =
          isSgTemplateInstantiationMemberFunctionDecl(decl)) {
    SgTemplateMemberFunctionDeclaration *template_decl =
        tpl_member->get_templateDeclaration();
    if (template_decl == NULL) {
      return false;
    }
    Sg_File_Info *pattern_info = template_decl->get_file_info();
    return callGraphFunctionHasUserSourceOwnership(template_decl)
        .value_or(pattern_info != NULL && !pattern_info->isCompilerGenerated());
  }

  return false;
}

/****************************
 ********* Function *********
 ****************************/
Function::Function() { decl = NULL; }

Function::Function(SgFunctionDeclaration *sample) {
  assert(!isSgTemplateFunctionDeclaration(sample));
  init(sample);
}

Function::Function(SgFunctionDefinition *sample) {
  init(sample->get_declaration());
}

Function::Function(SgFunctionCallExp *funcCall) {
  // The declaration edge is the typed semantic call target.  In particular,
  // a deduced SgTemplateFunctionRefExp deliberately retains the primary
  // template symbol for source spelling while its associated declaration is
  // the concrete specialization.  Consulting the spelling symbol first would
  // therefore turn a concrete call into a call to a non-callable template
  // pattern.
  if (SgFunctionDeclaration *associated_decl =
          funcCall->getAssociatedFunctionDeclaration()) {
    init(associated_decl);
    return;
  }

  // Some frontends remove all SgPointerDerefExp nodes from an expression like
  // this:
  //    void f() { (****f)(); }
  // but others leave them in the AST.  Therefore we need to handle the case
  // when the thing ultimately pointed to by the SgPointerDerefExp nodes is a
  // SgFunctionRefExp. The SgFunctionCallExp::getAssociatedFunctionSymbol() will
  // take care of this case and some others. [Robb Matzke 2012-12-28]
  SgFunctionSymbol *fsym = funcCall->getAssociatedFunctionSymbol();
  if (fsym != NULL) {
    init(fsym->get_declaration());
    return;
  }

  decl = NULL;
}

void Function::init(SgFunctionDeclaration *sample) {
  if (sample == nullptr || isSgTemplateFunctionDeclaration(sample) != nullptr) {
    fprintf(stderr,
            "REX_CALLGRAPH_INVARIANT[traversal-callable]: graph "
            "declaration=%p/%s name=%s is not one exact concrete callable\n",
            static_cast<void *>(sample),
            sample != nullptr ? sample->class_name().c_str() : "<null>",
            sample != nullptr ? sample->get_name().str() : "<null>");
    ROSE_ABORT();
  }
  // decl will be initialized to the defining declaration or the first
  // non-defining declaration if there is no definition
  decl = getCanonicalDecl(sample);
}

Function::Function(const Function &that) : decl(that.decl) {}

SgFunctionDeclaration *
Function::getCanonicalDecl(SgFunctionDeclaration *sampleDecl) {
  ROSE_ASSERT(sampleDecl != NULL);
  assert(!isSgTemplateFunctionDeclaration(sampleDecl));
  SgFunctionDeclaration *canonicalDecl = NULL;

  if (sampleDecl->get_definingDeclaration())
    canonicalDecl =
        isSgFunctionDeclaration(sampleDecl->get_definingDeclaration());
  else
    canonicalDecl =
        isSgFunctionDeclaration(sampleDecl->get_firstNondefiningDeclaration());
  ROSE_ASSERT(canonicalDecl);

  return canonicalDecl;
}

bool Function::eq(const Function &that) const { return decl == that.decl; }

bool Function::operator==(const Function &that) const { return eq(that); }

bool Function::operator!=(const Function &that) const {
  return !(*this == that);
}

bool Function::lessThan(const Function &that) const {
  // compare the declarations, since every Function object must have a unique
  // canonical declaration
  return std::less<SgFunctionDeclaration *>{}(decl, that.decl);
}
bool Function::operator<(const Function &that) const { return lessThan(that); }
bool Function::operator>(const Function &that) const {
  return !((*this) < that) && ((*this) != that);
}
bool Function::operator<=(const Function &that) const {
  return ((*this) < that) || ((*this) == that);
}
bool Function::operator>=(const Function &that) const {
  return !((*this) < that);
}

SgName Function::get_name() const {
  if (decl)
    return decl->get_name();
  else
    return "";
}

// returns this function's definition or NULL of it does not have one
SgFunctionDefinition *Function::get_definition() const {
  if (decl)
    return decl->get_definition();
  else
    return NULL;
}

// returns one of this function's declarations. it is guaranteed to be the same
// each time get_declaration is called
SgFunctionDeclaration *Function::get_declaration() const {
  assert(!isSgTemplateFunctionDeclaration(decl));
  return decl;
}

// returns the parameters of this function
SgInitializedNamePtrList Function::get_params() const {
  if (decl)
    return get_declaration()->get_args();
  else {
    SgInitializedNamePtrList list;
    return list;
  }
}

string Function::str(string indent) const {
  ostringstream oss;
  oss << get_name().getString() << "(";
  SgInitializedNamePtrList params = get_params();
  for (SgInitializedNamePtrList::iterator in = params.begin();
       in != params.end();) {
    oss << (*in)->get_name().getString();
    in++;
    if (in != params.end())
      oss << ", ";
  }
  oss << ")";
  return oss.str();
}

/******************************
 ********* CGFunction *********
 ******************************/

CGFunction::CGFunction(SgFunctionDeclaration *sample,
                       SgIncidenceDirectedGraph *graph)
    : Function(sample) {
  assert(!isSgTemplateFunctionDeclaration(sample));
  this->graph = graph;
  initCGNodes();
}

CGFunction::CGFunction(SgGraphNode *sample, SgIncidenceDirectedGraph *graph)
    : Function(isSgFunctionDeclaration(sample->get_SgNode())) {
  ROSE_ASSERT(isSgFunctionDeclaration(sample->get_SgNode()));
  assert(!isSgTemplateFunctionDeclaration(sample->get_SgNode()));

  this->graph = graph;
  initCGNodes();
}

CGFunction::CGFunction(const CGFunction &that) : Function(that) {
  this->graph = that.graph;

  // copy cgNodes from that to this
  for (set<SgGraphNode *>::iterator it = that.cgNodes.begin();
       it != that.cgNodes.end(); it++)
    cgNodes.insert(*it);
}

// initializes the cgNodes set
void CGFunction::initCGNodes() {
  set<SgGraphNode *> nodes = graph->computeNodeSet();
  for (set<SgGraphNode *>::iterator itn = nodes.begin(); itn != nodes.end();
       itn++) {
    SgNode *n = (*itn)->get_SgNode();
    ROSE_ASSERT(isSgFunctionDeclaration(n));
    assert(!isSgTemplateFunctionDeclaration(n));

    SgFunctionDeclaration *cfgDecl =
        getCanonicalDecl(isSgFunctionDeclaration(n));
    assert(!isSgTemplateFunctionDeclaration(cfgDecl));

    // if the given SgGraphNode refers to this function
    if (cfgDecl == decl)
      cgNodes.insert(*itn);
  }
}

bool CGFunction::operator==(const CGFunction &that) const {
  return Function::eq(that) && cgNodes == that.cgNodes;
}

bool CGFunction::operator!=(const CGFunction &that) const {
  return !(*this == that);
}

bool CGFunction::operator<(const CGFunction &that) const {
  return Function::lessThan(that);
}
bool CGFunction::operator>(const CGFunction &that) const {
  return !((*this) < that) && ((*this) != that);
}
bool CGFunction::operator<=(const CGFunction &that) const {
  return ((*this) < that) || ((*this) == that);
}
bool CGFunction::operator>=(const CGFunction &that) const {
  return !((*this) < that);
}

/*************************************
 ********* TraverseCallGraph *********
 *************************************/

TraverseCallGraph::TraverseCallGraph(SgIncidenceDirectedGraph *graph) {
  this->graph = graph;

  set<SgGraphNode *> nodes = graph->computeNodeSet();
  for (set<SgGraphNode *>::iterator itn = nodes.begin(); itn != nodes.end();
       itn++) {
    SgNode *n = (*itn)->get_SgNode();
    ROSE_ASSERT(isSgFunctionDeclaration(n));
    assert(!isSgTemplateFunctionDeclaration(n));
    CGFunction func(isSgFunctionDeclaration(n), graph);

    // Skip compiler-generated functions unless they are instantiations of
    // user-written templates.
    if (call_graph_traversal_includes_function_decl(
            isSgFunctionDeclaration(n))) {
      functions.insert(func);
    }
  }

  // Initially, every function has no known predecessor.
  for (set<CGFunction>::iterator it = functions.begin(); it != functions.end();
       it++) {
    noPred.insert(&*it);
  }

  // Remove functions reached by an incoming call edge.
  for (set<CGFunction>::iterator func = functions.begin();
       func != functions.end(); func++) {
    for (CGFunction::iterator it = (*func).successors(); it != (*func).end();
         it++) {
      const CGFunction *target = it.getTarget(functions);

      // if the target is compiler-generated, skip it
      if (target == NULL)
        continue;

      noPred.erase(target);
    }
  }
}

// returns a pointer to a CGFunction that matches the given declaration or NULL
// if no objects do the memory of the object persists for the entire lifetime of
// this TraverseCallGraph object
const CGFunction *TraverseCallGraph::getFunc(SgFunctionDeclaration *decl) {
  assert(!isSgTemplateFunctionDeclaration(decl));
  CGFunction func(decl, graph);

  set<CGFunction>::iterator findLoc = functions.find(func);
  if (findLoc == functions.end())
    return NULL;

  return &(*findLoc);
}

// returns a pointer to a CGFunction object that matches the given Function
// object the memory of the object persists for the entire lifetime of this
// TraverseCallGraph object
const CGFunction *TraverseCallGraph::getFunc(const Function &func) {
  return getFunc(func.get_declaration());
}

/************************************
 ***** TraverseCallGraphUnordered *****
 ************************************/

TraverseCallGraphUnordered::TraverseCallGraphUnordered(
    SgIncidenceDirectedGraph *graph)
    : TraverseCallGraph(graph) {}

void TraverseCallGraphUnordered::traverse() {
  for (set<CGFunction>::iterator f = functions.begin(); f != functions.end();
       f++) {
    visit(&(*f));
  }
}

TraverseCallGraphUnordered::~TraverseCallGraphUnordered() {}

/*************************************
 ***** TraverseCallGraphDataflow *****
 *************************************/
TraverseCallGraphDataflow::TraverseCallGraphDataflow(
    SgIncidenceDirectedGraph *graph)
    : TraverseCallGraph(graph) {}

void TraverseCallGraphDataflow::traverse() {
  // start the traversal from the nodes that are called from no other node
  for (set<CGFunction>::iterator it = functions.begin(); it != functions.end();
       it++) {
    assert(!isSgTemplateFunctionDeclaration(it->get_declaration()));
    assert(!isSgTemplateFunctionDefinition(it->get_declaration()));
    assert(!isSgTemplateMemberFunctionDeclaration(it->get_declaration()));
    remaining.push_back(&(*it));
  }

  // traverse functions for as long as visit keeps adding them to remaining
  while (remaining.size() > 0) {
    const CGFunction *func = remaining.front();
    remaining.pop_front();
    assert(!isSgTemplateFunctionDeclaration(func->get_declaration()));
    assert(!isSgTemplateFunctionDefinition(func->get_declaration()));
    assert(!isSgTemplateMemberFunctionDeclaration(func->get_declaration()));
    visit(func);
  }
}

// adds func to the back of the remaining list, if its not already there
void TraverseCallGraphDataflow::addToRemaining(const CGFunction *func) {
  for (list<const CGFunction *>::iterator it = remaining.begin();
       it != remaining.end(); it++)
    // if this function is already in remaining, don't bother inserting it
    if (*it == func)
      return;

  // insert func, if it was not found
  remaining.push_back(func);
}

TraverseCallGraphDataflow::~TraverseCallGraphDataflow() {}

#endif
