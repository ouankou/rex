#include "sage3basic.h"

#include "fixupDeclarationScope.h"

namespace {
// Attempt to recover a scope from the parent chain when the explicit scope is
// missing.
SgScopeStatement *infer_scope_from_parent(SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  SgNode *parent = decl->get_parent();
  while (parent != nullptr) {
    if (SgScopeStatement *scope = isSgScopeStatement(parent)) {
      return scope;
    }
    parent = parent->get_parent();
  }

  return nullptr;
}
} // namespace

void fixupAstDeclarationScope(SgNode * /*node*/) {
  // This function was designed to fixup what I thought were inconsistancies in
  // how the defining and some non-defining declarations associated with friend
  // declarations had their scope set.  I now know this this was not a problem,
  // but it is helpful to enforce the consistancy.  It might also be useful to
  // process declarations with scopes set to namespace definitions, so that the
  // namespace definition can be normalized to be consistant across all of the
  // different re-entrant namespace definitions.  This is possible within the
  // new namespace support in ROSE.

  TimingPerformance timer("Fixup declaration scopes:");

  // This simplifies how the traversal is called!
  FixupAstDeclarationScope astFixupTraversal;

  // DQ (1/29/2007): This traversal now uses the memory pool (so that we will
  // visit declaration hidden in types (e.g. SgClassType)
  // SgClassType::traverseMemoryPoolNodes(v);
  astFixupTraversal.traverseMemoryPool();

  // Now process the map of sets of declarations.
  std::map<SgDeclarationStatement *, std::set<SgDeclarationStatement *> *>
      &mapOfSets = astFixupTraversal.mapOfSets;

  std::map<SgDeclarationStatement *,
           std::set<SgDeclarationStatement *> *>::iterator i =
      mapOfSets.begin();
  while (i != mapOfSets.end()) {
    SgDeclarationStatement *firstNondefiningDeclaration = i->first;

    // DQ (3/2/2015): Added assertion.
    ASSERT_not_null(firstNondefiningDeclaration);

    // DQ (3/2/2015): Added assertion.
    ASSERT_not_null(
        firstNondefiningDeclaration->get_firstNondefiningDeclaration());

    // DQ (3/2/2015): Make this assertion a warning: fails in outlining example
    // seq7a_test2006_78.C. ROSE_ASSERT(firstNondefiningDeclaration ==
    // firstNondefiningDeclaration->get_firstNondefiningDeclaration());
    if (firstNondefiningDeclaration !=
        firstNondefiningDeclaration->get_firstNondefiningDeclaration()) {
      printf(
          "WARNING: In fixupAstDeclarationScope(): firstNondefiningDeclaration "
          "!= firstNondefiningDeclaration->get_firstNondefiningDeclaration() "
          "\n");
      printf("   --- firstNondefiningDeclaration = %p = %s \n",
             firstNondefiningDeclaration,
             firstNondefiningDeclaration->class_name().c_str());
      printf("   --- "
             "firstNondefiningDeclaration->get_firstNondefiningDeclaration() = "
             "%p = %s \n",
             firstNondefiningDeclaration->get_firstNondefiningDeclaration(),
             firstNondefiningDeclaration->get_firstNondefiningDeclaration()
                 ->class_name()
                 .c_str());
    }

    // At this point scopes should have been set by the builders/frontends;
    // inference is a last resort.
    SgScopeStatement *correctScope = firstNondefiningDeclaration->get_scope();

    if (correctScope == nullptr) {
      correctScope = infer_scope_from_parent(firstNondefiningDeclaration);
      if (correctScope != nullptr) {
        firstNondefiningDeclaration->set_scope(correctScope);
      }
    }

    // DQ (11/24/2020): Debugging code segregation tool.
    if (correctScope == nullptr) {
      printf("Error: In fixupAstDeclarationScope(): correctScope == NULL: "
             "firstNondefiningDeclaration = %p = %s name = %s \n",
             firstNondefiningDeclaration,
             firstNondefiningDeclaration->class_name().c_str(),
             SageInterface::get_name(firstNondefiningDeclaration).c_str());
      printf(" --- firstNondefiningDeclaration->hasExplicitScope() = %s \n",
             firstNondefiningDeclaration->hasExplicitScope() ? "true"
                                                             : "false");
      printf(" --- firstNondefiningDeclaration->get_parent() = %p \n",
             firstNondefiningDeclaration->get_parent());
      if (firstNondefiningDeclaration->get_parent() != nullptr) {
        printf(
            "--- non-null: firstNondefiningDeclaration->get_parent() = %s name "
            "= %s \n",
            firstNondefiningDeclaration->get_parent()->class_name().c_str(),
            SageInterface::get_name(firstNondefiningDeclaration->get_parent())
                .c_str());
      }
    }

    ASSERT_not_null(correctScope);

    std::set<SgDeclarationStatement *> *declarationSet = i->second;
    ASSERT_not_null(declarationSet);

    std::set<SgDeclarationStatement *>::iterator j = declarationSet->begin();
    while (j != declarationSet->end()) {
      SgScopeStatement *associatedScope = (*j)->get_scope();
      ASSERT_not_null(associatedScope);

      // DQ (6/11/2013): This is triggered by namespace definition scopes that
      // are different due to re-entrant namespace declarations.  We should
      // maybe fix this. TV (7/22/13): This is also triggered when for global
      // scope accross files.
      if (associatedScope != correctScope) {
        // DQ (1/30/2014): Cleaning up some output spew.
        if (SgProject::get_verbose() > 0) {
          MLOG_WARN_C("astPostProcessing",
                      "This is the wrong scope (declaration = %p = %s): "
                      "associatedScope = %p = %s correctScope = %p = %s \n",
                      *j, (*j)->class_name().c_str(), associatedScope,
                      associatedScope->class_name().c_str(), correctScope,
                      correctScope->class_name().c_str());
        }
      }

      j++;
    }

    i++;
  }

  for (auto &entry : mapOfSets) {
    delete entry.second;
  }
  mapOfSets.clear();
}

void FixupAstDeclarationScope::visit(SgNode *node) {
  // DQ (6/11/2013): This corrects where legacy frontend can set the scope of a
  // friend declaration to be different from the defining declaration. We need
  // it to be a rule in ROSE that the scope of the declarations are consistant
  // between defining and all non-defining declaration).

  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
  if (declaration != nullptr) {
    if (declaration->get_scope() == nullptr) {
      SgScopeStatement *inferred_scope = infer_scope_from_parent(declaration);
      if (inferred_scope != nullptr) {
        declaration->set_scope(inferred_scope);
        if (declaration->get_parent() == nullptr) {
          declaration->set_parent(inferred_scope);
        }
      } else {
        MLOG_WARN_C("astPostProcessing",
                    "Unable to infer scope for declaration %p (%s)",
                    declaration, declaration->class_name().c_str());
      }
    }

    // SgDeclarationStatement* definingDeclaration         =
    // declaration->get_definingDeclaration();
    SgDeclarationStatement *firstNondefiningDeclaration =
        declaration->get_firstNondefiningDeclaration();

    if (firstNondefiningDeclaration != nullptr &&
        firstNondefiningDeclaration->get_scope() == nullptr) {
      SgScopeStatement *inferred_scope =
          infer_scope_from_parent(firstNondefiningDeclaration);
      if (inferred_scope != nullptr) {
        firstNondefiningDeclaration->set_scope(inferred_scope);
        if (firstNondefiningDeclaration->get_parent() == nullptr) {
          firstNondefiningDeclaration->set_parent(inferred_scope);
        }
      } else {
        MLOG_WARN_C("astPostProcessing",
                    "Unable to infer scope for "
                    "firstNondefiningDeclaration %p (%s)",
                    firstNondefiningDeclaration,
                    firstNondefiningDeclaration->class_name().c_str());
      }
    }

    // Note that these declarations don't follow the same rules (namely the
    // get_firstNondefiningDeclaration() can be NULL).
    if (isSgFunctionParameterList(node) || isSgVariableDefinition(node) ||
        isSgNonrealDecl(node)) {
    } else {
      if (firstNondefiningDeclaration != nullptr) {
        if (mapOfSets.find(firstNondefiningDeclaration) == mapOfSets.end()) {
          std::set<SgDeclarationStatement *> *new_empty_set =
              new std::set<SgDeclarationStatement *>();
          ASSERT_not_null(new_empty_set);
          ASSERT_not_null(firstNondefiningDeclaration);

          mapOfSets.insert(std::pair<SgDeclarationStatement *,
                                     std::set<SgDeclarationStatement *> *>(
              firstNondefiningDeclaration, new_empty_set));
        }

        ASSERT_require(mapOfSets.find(firstNondefiningDeclaration) !=
                       mapOfSets.end());

        // DQ (3/2/2015): Added assertion.
        ASSERT_not_null(declaration);
        // DQ (3/2/2015): Added assertion.
        // ROSE_ASSERT(declaration ==
        // declaration->get_firstNondefiningDeclaration());

        // mapOfSets[firstNondefiningDeclaration]->insert(firstNondefiningDeclaration);
        mapOfSets[firstNondefiningDeclaration]->insert(declaration);
      }
    }
  }
}
