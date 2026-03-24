// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "markForOutputInCodeGeneration.h"

#include "markTemplateInstantiationsForOutput.h"

#include "rose_config.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

namespace {
bool is_frontend_suppressed_template_instantiation(
    SgDeclarationStatement *decl) {
  if (decl == NULL || decl->get_file_info() == NULL) {
    return false;
  }

  const bool is_template_instantiation =
      isSgTemplateInstantiationDecl(decl) != NULL ||
      isSgTemplateInstantiationFunctionDecl(decl) != NULL ||
      isSgTemplateInstantiationMemberFunctionDecl(decl) != NULL;
  if (!is_template_instantiation) {
    return false;
  }

  Sg_File_Info *file_info = decl->get_file_info();
  return file_info->isCompilerGenerated() == true &&
         file_info->isOutputInCodeGeneration() == false;
}
} // namespace

set<SgDeclarationStatement *>
MarkTemplateInstantiationsForOutput::BuildSetOfRequiredTemplateDeclarations(
    SgNode *node, SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);

  // This simplifies how the traversal is called!
  MarkTemplateInstantiationsForOutputSupport declarationFixupTraversal(file);

  // This inherited attribute is used for all traversals (within the iterative
  // approach we define)
  MarkTemplateInstantiationsForOutputSupportInheritedAttribute
      inheritedAttribute;

  // An alternative is to save the list of required declaration and then zero
  // out the list in the declarationFixupTraversal traversal and then re-execute
  // the traversal on each declaration that is compiler generated (i.e. not from
  // the current file).  We define an iterative process where we always search
  // the new declarations on the assembled list and merge the lists until
  // finally no list is generated (then we stop).

  // List of lists of declarations, each list is obtained from an iteration of
  // the prelink process, multiple iterations are required because each new
  // declaration found to be required may cascade into other declarations being
  // required.  All of these could generate required instantiations and trigger
  // the output of template instantiations.
  vector<list<SgDeclarationStatement *>> listOfListsOfDeclarations;

  // First call to traverse is a traversal of the whole AST
  declarationFixupTraversal.traverse(node, inheritedAttribute);

  list<SgDeclarationStatement *> accumulatedList =
      declarationFixupTraversal.listOfTemplateDeclarationsToOutput;

  // create a shorter name for the list where we accumulare required
  // declarations
  list<SgDeclarationStatement *> &currentList =
      declarationFixupTraversal.listOfTemplateDeclarationsToOutput;

  int prelinkIterationCounter = 0;
  // while (
  // declarationFixupTraversal.listOfTemplateDeclarationsToOutput.empty() ==
  // false )
  while (currentList.empty() == false) {
    // For each identified template instantiation, traverse it to identify if it
    // includes an instantiation that would ???

    // DQ (3/31/2013): This might not be required if we use the accumulatedList
    // to build the setOfRequiredDeclarations (function return value). append
    // the newest list to the back of the list of lists of required declarations
    listOfListsOfDeclarations.push_back(currentList);

    list<SgDeclarationStatement *>::iterator i =
        listOfListsOfDeclarations[prelinkIterationCounter].begin();
    while (i != listOfListsOfDeclarations[prelinkIterationCounter].end()) {
      // Iterate thorugh the list a look for compiler generated declarations

      ROSE_ASSERT((*i)->get_file_info() != NULL);
      if ((*i)->get_file_info()->isCompilerGenerated() == true) {
        // Look into this subtrees for any required declarations (which would
        // not have been caught last iteration!
        declarationFixupTraversal.traverse(*i, inheritedAttribute);
      }

      i++;
    }

    // DQ (3/31/2013): Added debugging code.
    if (listOfListsOfDeclarations.size() > 2) {
      printf("Exiting as a test: listOfListsOfDeclarations.size() > 2 "
             "prelinkIterationCounter = %u \n",
             prelinkIterationCounter);
      ROSE_ABORT();
    }

    // DQ (3/31/2013): This is some sort of set operation (union into
    // accumulatedList) and removing the insersection from the currentList. Note
    // sure if there is a simpler expression for this.

    // DQ (3/31/2013): Remove any previously handled declarations (list
    // elements).
    list<SgDeclarationStatement *>::iterator e = accumulatedList.begin();
    while (e != accumulatedList.end()) {
      if (find(currentList.begin(), currentList.end(), *e) !=
          currentList.end()) {
        currentList.remove(*e);
      }

      e++;
    }

    // Add any new elements from currentList into the accumulatedList (so that
    // we can use the accumulatedList to remove elements from the currentList at
    // the end of the loop).
    list<SgDeclarationStatement *>::iterator l = currentList.begin();
    while (l != currentList.end()) {
      // Check is this is a previously seen declaration.
      if (find(accumulatedList.begin(), accumulatedList.end(), *l) ==
          accumulatedList.end()) {
        accumulatedList.push_back(*l);
      }
      l++;
    }

    prelinkIterationCounter++;
  }

  // Convert the vector of lists to a set!
  set<SgDeclarationStatement *> setOfRequiredDeclarations;

  // DQ (3/31/2013): Use this simpler implementation.
  list<SgDeclarationStatement *>::iterator c = accumulatedList.begin();
  while (c != accumulatedList.end()) {
    setOfRequiredDeclarations.insert(*c);
    c++;
  }

  return setOfRequiredDeclarations;
}

void MarkTemplateInstantiationsForOutput::
    ProcessMemberFunctionTemplateDeclarations(
        set<SgDeclarationStatement *> setOfRequiredDeclarations,
        SgSourceFile *input_file) {
  ROSE_ASSERT(input_file != NULL);

  for (set<SgDeclarationStatement *>::iterator i =
           setOfRequiredDeclarations.begin();
       i != setOfRequiredDeclarations.end(); i++) {
    // Iterate through the functions recorded as required for compilation of
    // this file!

    // ROSE_ASSERT((*i)->get_definingDeclaration() != NULL);
    // ROSE_ASSERT((*i)->get_firstNondefiningDeclaration() != NULL);

    SgDeclarationStatement *definingDeclaration =
        (*i)->get_definingDeclaration();
    SgDeclarationStatement *firstNondefiningDeclaration =
        (*i)->get_firstNondefiningDeclaration();

    bool isDefiningDeclaration =
        definingDeclaration == NULL ? false : (*i == definingDeclaration);
    bool isfirstNondefiningDeclaration =
        firstNondefiningDeclaration == NULL
            ? false
            : (*i == firstNondefiningDeclaration);
    // first debug the member functions!
    // We only have to worry about member functions (since that is what this
    // function is handling).
    SgTemplateInstantiationMemberFunctionDecl *memberFunctionInstantiation =
        isSgTemplateInstantiationMemberFunctionDecl(*i);
    if (memberFunctionInstantiation != NULL) {
      // At least one of these should be true!
      ROSE_ASSERT(isfirstNondefiningDeclaration == true ||
                  isDefiningDeclaration == true);

      // If the template declaration is in the current file then we need not
      // output the instantiation (skip marking instantiation for output!)
      SgDeclarationStatement *templateDeclaration =
          memberFunctionInstantiation->get_templateDeclaration();

      // DQ (1/21/2013): This is a problem for specialized templates containing
      // declarations (e.g. member functions) that are not in the associated
      // template class.  These member function instantiations, don't always
      // have an associated member function template declaration.  So we can't
      // inforce this. ROSE_ASSERT(templateDeclaration != NULL);
      if (templateDeclaration != NULL) {
        // BEGIN: Indentation issue in this source code.

        // string currentFilename =
        // SageInterface::getEnclosingFileNode(templateDeclaration)->getFileName();
        SgSourceFile *file =
            SageInterface::getEnclosingSourceFile(memberFunctionInstantiation);
        ROSE_ASSERT(file != NULL);
        string currentFilename = (file != NULL) ? file->getFileName() : "";
        string filenameOfTemplateDeclaration =
            templateDeclaration->get_file_info()->get_filename();

        ROSE_ASSERT(templateDeclaration->get_scope() != NULL);
        SgTemplateInstantiationDefn
            *memberFunctionScopeTemplateInstantiationDefinition =
                isSgTemplateInstantiationDefn(templateDeclaration->get_scope());
        if (memberFunctionScopeTemplateInstantiationDefinition != NULL) {
          // This is a SgTemplateInstantiationMemberFunctionDecl in a templated
          // class declaration (so it might be a templated member function or a
          // non-templated member function).
          SgTemplateInstantiationDecl
              *memberFunctionScopeTemplateInstantiationDeclaration =
                  isSgTemplateInstantiationDecl(
                      memberFunctionScopeTemplateInstantiationDefinition
                          ->get_declaration());
          ROSE_ASSERT(memberFunctionScopeTemplateInstantiationDeclaration !=
                      NULL);
          SgDeclarationStatement *memberFunctionScopeTemplateDeclaration =
              memberFunctionScopeTemplateInstantiationDeclaration
                  ->get_templateDeclaration();
          ROSE_ASSERT(memberFunctionScopeTemplateDeclaration != NULL);
          filenameOfTemplateDeclaration =
              memberFunctionScopeTemplateDeclaration->get_file_info()
                  ->get_filename();
        }

        // We only want to fixup template details in the current file, since we
        // only unparse the current file.
        if (filenameOfTemplateDeclaration == currentFilename) {
          // This template declaration is in the current file so let the
          // vendor compiler instantiate it, there are a few rules here:
          //    1) if it is a specialization then we should output it
          //    (since it is used), or 2) if it is defined in the class
          //    and the class is a templated class then
          //       legacy frontend will not list the source for the
          //       member function in the class template declaration
          //       (independent of the setting og TEMPLATES_IN_IL within
          //       legacy frontend.
          bool isSpecialization =
              memberFunctionInstantiation->isSpecialization();

          // DQ (5/2/2012): Although I included that case for handling
          // "isDefinedInClass" below, I think it should always be false for
          // this handling of specializations. bool isDefinedInClass =
          // memberFunctionInstantiation->isDefinedInClass(); bool
          // isDefinedInClass = false;
          bool isDefinedInClass =
              memberFunctionInstantiation->isDefinedInClass();
          SgDeclarationStatement *templateDeclaration =
              memberFunctionInstantiation->get_templateDeclaration();
          // bool templateDeclarationIsDeclaredInClass =
          // (templateDeclaration->get_parent() ==
          // templateDeclaration->get_scope());
          SgNode *parentOfTemplateDeclaration =
              templateDeclaration->get_parent();
          // Later this test will have to be for
          // "isTemplateDefinition(parentOfTemplateDeclaration));" Make this a
          // little more general since a member function might appear in a
          // non-templated class. bool templateDeclarationIsDeclaredInClass =
          // (isSgTemplateInstantiationDefn(parentOfTemplateDeclaration) !=
          // NULL);
          bool templateDeclarationIsDeclaredInClass =
              (isSgClassDefinition(parentOfTemplateDeclaration) != NULL);

          // DQ (5/2/2012): We have to check if it is defined in the class since
          // then the template string (still used for unparsing) will not have
          // the function definitions. Check if this is a specialization in
          // which case we have to put it out! if ( (isSpecialization == true)
          // || (isDefinedInClass == true) ) if ( isSpecialization == true )
          if ((isSpecialization == true) || (isDefinedInClass == true)) {
            // I assume this is a definition if we are marking it for output!
            // ROSE_ASSERT(memberFunctionInstantiation->get_definition() !=
            // NULL); Mark this for output later when we generate code!
            markForOutputInCodeGeneration(memberFunctionInstantiation);
          } else {
            if (templateDeclarationIsDeclaredInClass == true) {
              // If it is not a specialization it might have been
              // that the template declaration appeared in the
              // class in which case legacy frontend has deleted
              // the defining template declaration and we only have
              // the opportunity to output the generated template
              // instantiation (not a specialization, but a simple
              // instantiation of the member function).  In this
              // case we have to mark the defining template
              // instantiation for output and if the instantiation
              // of the class is not output we have to output a
              // member function prototype for the instantiated
              // member function (since it will be output at the
              // end of the file (as an inlined function)).  I'm
              // not clear if it is an issue that as an inlined
              // function it is used (referenced) before it is
              // defined (but it seems to work just fine, at least
              // with some older versions of the g++ compiler).

              // DQ (8/8/2012): Output this declaration only if it is part of
              // the class being output. DQ (8/26/2005): Suppress prototypes of
              // constuctors (see test2005_147.C), not clear why these can't be
              // output! bool processMemberFunction = true;
              SgClassDefinition *parentClassDefinition =
                  isSgClassDefinition(memberFunctionInstantiation->get_scope());
              ROSE_ASSERT(parentClassDefinition != NULL);
              SgClassDeclaration *parentClassDeclaration =
                  parentClassDefinition->get_declaration();
              ROSE_ASSERT(parentClassDeclaration != NULL);

              bool processMemberFunction =
                  (parentClassDeclaration->get_file_info()
                       ->isOutputInCodeGeneration() == true);

              // DQ (8/27/2005): skipping constructors appears to be required
              // for both g++ 3.3.x and 3.4.x special handling for non-defining
              // constructor declarations
              if (isDefiningDeclaration == false &&
                  memberFunctionInstantiation->get_specialFunctionModifier()
                          .isConstructor() == true) {
                // printf ("Warning: Skipping output of constructor prototypes
                // since their specialization is a problem (bug) in some
                // versions of g++ (I think 3.4.x) \n");
                processMemberFunction = false;
              }

              // printf ("processMemberFunction = %s \n",processMemberFunction ?
              // "true" : "false");
              if (processMemberFunction == true) {
                // This is not a constructor prototype so it is OK to output the
                // prototype Mark this for output later when we generate code!
                // This marks the defining and non-defining declarations for
                // output (which for the case of a constructor we fixup below).
                markForOutputInCodeGeneration(memberFunctionInstantiation);

                // DQ (8/28/2005): It is a bug in g++ if we output the forward
                // declaration of a member function specialization (for either a
                // template or non-template member function).
                if (isDefiningDeclaration == true &&
                    memberFunctionInstantiation->get_specialFunctionModifier()
                            .isConstructor() == true) {
                }

                // DQ (8/26/2005): It still might be that the non-defining
                // declaration is not in the global scope so that marking it for
                // output is still insufficient.

                // test if the declaration appears in global scope

                // DQ (10/11/2007): Note that even if the defining declaration
                // of the template specialization appears in global scope (which
                // is OK) g++ 3.4.x and 4.x place additional constraints on
                // where the prototypes can be placed; so we cannot just put
                // them into global scope after the class declaration.  If the
                // class declaration appears in a namespace, then it must go
                // into the name space (and after the class declaration).

                SgGlobal *globalScope = file->get_globalScope();
                ROSE_ASSERT(globalScope != NULL);

                // printf ("#####################  globalScope = %p
                // \n",globalScope);

                SgDeclarationStatementPtrList &declarationList =
                    globalScope->get_declarations();
                SgDeclarationStatementPtrList::iterator location =
                    find(declarationList.begin(), declarationList.end(), *i);
                // printf ("declarationList.begin() = %p  declarationList.end()
                // = %p \n",*declarationList.begin(),*declarationList.end());
                // printf ("location in enumeration of declarations in global
                // scope = %p \n",*location);
                SgTemplateInstantiationMemberFunctionDecl
                    *nondefiningMemberFunctionInstantiation =
                        isSgTemplateInstantiationMemberFunctionDecl(
                            memberFunctionInstantiation
                                ->get_firstNondefiningDeclaration());
                SgDeclarationStatementPtrList::iterator
                    locationOfNondefiningDeclaration =
                        find(declarationList.begin(), declarationList.end(),
                             nondefiningMemberFunctionInstantiation);

                ROSE_ASSERT(
                    nondefiningMemberFunctionInstantiation
                        ->get_templateDeclaration() ==
                    memberFunctionInstantiation->get_templateDeclaration());
                // printf
                // ("memberFunctionInstantiation->get_templateDeclaration() = %p
                // \n",memberFunctionInstantiation->get_templateDeclaration());

                // DQ (11/5/2007): Check for existance of both the defining and
                // non-defining, since then we want to move the non-defining
                // declaration (the TRUE case). But if the non-defining
                // declaration does not exist then we want to build it (the
                // FALSE case). if (location != declarationList.end())
                if (location != declarationList.end() &&
                    (locationOfNondefiningDeclaration !=
                     declarationList.end())) {
                  // Found the declaration in global scope
                  // DQ (10/11/2007): Added support to move prototypes of
                  // template specializations into the correct namespace
                  // (required by g++ version later than 3.3.x) DQ (11/4/2007):
                  // If there is a defining declaration then wait until we
                  // process that declaration, if there is not one then move the
                  // trigger this transformation on the basis of the
                  // non-defining declaration (since the defiing declaration
                  // might not exist). The point is to only do this relocation
                  // once (however, I thinkwe may have to also do it for the
                  // defining declaration as well, not just for the non-defining
                  // declaration. In this case we would have to do the
                  // relocation each time we see the declaration (defining or
                  // non-defiing).  This initial implementatio has only handle
                  // the non-defining declaration. Older comment: Wait until we
                  // see the defining declaration and then move the nondefining
                  // declaration if it was declared in a class defined in a
                  // namespace. printf ("Check if there is a defining
                  // declaration
                  // memberFunctionInstantiation->get_definingDeclaration() = %p
                  // \n",memberFunctionInstantiation->get_definingDeclaration());
                  // if (isDefiningDeclaration == true)
                  if (isDefiningDeclaration == true ||
                      memberFunctionInstantiation->get_definingDeclaration() ==
                          NULL) {
                    // Refactored some code to the SageInterface
                    SgNamespaceDefinitionStatement *classNamespaceScope =
                        SageInterface::enclosingNamespaceScope(
                            memberFunctionInstantiation);
                    // This will invalidate any outstanding iterator defined on
                    // declarationList
                    SgTemplateInstantiationMemberFunctionDecl
                        *nondefiningMemberFunctionInstantiation =
                            isSgTemplateInstantiationMemberFunctionDecl(
                                memberFunctionInstantiation
                                    ->get_firstNondefiningDeclaration());
                    SgDeclarationStatementPtrList::iterator
                        locationOfNondefiningDeclaration =
                            find(declarationList.begin(), declarationList.end(),
                                 nondefiningMemberFunctionInstantiation);
                    // ROSE_ASSERT(locationOfNondefiningDeclaration !=
                    // declarationList.end());
                    if ((classNamespaceScope != NULL) &&
                        (locationOfNondefiningDeclaration !=
                         declarationList.end()))
                    // if ( (classNamespaceScope != NULL ) ) // &&
                    // (locationOfNondefiningDeclaration !=
                    // declarationList.end()) )
                    {
                      // We have to move the prototype of the specialized
                      // template to the namespace (after the declaration)
                      declarationList.erase(locationOfNondefiningDeclaration);

                      // Refactored some code to the SageInterface
                      // DQ (11/4/2007): Get the associated declaration, if it
                      // is part of a template instantiation then get the
                      // template declaration
                      SgDeclarationStatement *parentDeclaration =
                          SageInterface::getNonInstantiatonDeclarationForClass(
                              memberFunctionInstantiation);
                      bool inFront = false;
                      classNamespaceScope->insert_statement(
                          parentDeclaration,
                          nondefiningMemberFunctionInstantiation, inFront);
                    } else {
                    }
                  } else {
                  }
                } else {
                  // Could not find the declaration in global scope, so add it
                  // after the class declaration.

                  ROSE_ASSERT(memberFunctionInstantiation->get_class_scope() !=
                              NULL);

                  SgDeclarationStatement *parentDeclaration =
                      memberFunctionInstantiation
                          ->get_associatedClassDeclaration();
                  ROSE_ASSERT(parentDeclaration != NULL);

                  //                                      SgClassDeclaration*
                  //                                      parentClassDeclaration
                  //                                      =
                  //                                      isSgClassDeclaration(parentDeclaration);
                  //                                      SgNonrealDecl*
                  //                                      parentNonrealDecl =
                  //                                      isSgNonrealDecl(parentDeclaration);

                  // DQ (11/4/2007): This looks for a forward declaration of
                  // matching name exists in the specificed scope (starting as
                  // "parentClassDeclaration").
                  bool foundExistingPrototype =
                      SageInterface::isPrototypeInScope(
                          globalScope, memberFunctionInstantiation,
                          parentDeclaration);

                  // DQ (11/4/2007): If not found in global scope then check if
                  // it has already been moved to a namespace.
                  SgNamespaceDefinitionStatement *classNamespaceScope =
                      SageInterface::enclosingNamespaceScope(
                          memberFunctionInstantiation);
                  if (foundExistingPrototype == false) {
                    // Find the correct namespace
                    // SgNamespaceDefinitionStatement* classNamespaceScope =
                    // SageInterface::enclosingNamespaceScope(memberFunctionInstantiation);

                    if (classNamespaceScope != NULL) {
                      SgTemplateInstantiationMemberFunctionDecl
                          *nondefiningMemberFunctionInstantiation =
                              isSgTemplateInstantiationMemberFunctionDecl(
                                  memberFunctionInstantiation
                                      ->get_firstNondefiningDeclaration());
                      ROSE_ASSERT(nondefiningMemberFunctionInstantiation !=
                                  NULL);

                      foundExistingPrototype =
                          SageInterface::isPrototypeInScope(
                              classNamespaceScope, memberFunctionInstantiation,
                              nondefiningMemberFunctionInstantiation);
                    }
                  }
                  // DQ (11/3/2007): Use the new test result to avoid
                  // redundantly adding prototypes.
                  if (foundExistingPrototype == false) {
                    // DQ (11/3/2007): Only build the new member function
                    // prototype declaration if we are going to insert it into
                    // place.
                    SgDeclarationStatementPtrList::iterator parentLocation =
                        find(declarationList.begin(), declarationList.end(),
                             parentDeclaration);
                    if (parentLocation != declarationList.end()) {
                      // Use the version of the function located in the
                      // SageInterface
                      SgTemplateInstantiationMemberFunctionDecl
                          *nondefiningMemberFunctionInstantiation =
                              isSgTemplateInstantiationMemberFunctionDecl(
                                  memberFunctionInstantiation
                                      ->get_firstNondefiningDeclaration());
                      // SgTemplateInstantiationMemberFunctionDecl*
                      // copyOfMemberFunction =
                      // SageInterface::buildForwardFunctionDeclaration(memberFunctionInstantiation);
                      ROSE_ASSERT(nondefiningMemberFunctionInstantiation !=
                                  NULL);
                      SgTemplateInstantiationMemberFunctionDecl
                          *copyOfMemberFunction =
                              SageInterface::buildForwardFunctionDeclaration(
                                  nondefiningMemberFunctionInstantiation);

                      // DQ (8/28/2005): Mark it as extern "C++" since it might
                      // be placed in an extern "C" portion of the source code!
                      // This happends in swig generated code
                      // (polyhedralcmiswig.cc in KULL).
                      copyOfMemberFunction->get_declarationModifier()
                          .get_storageModifier()
                          .setExtern();
                      copyOfMemberFunction->set_linkage("C++");
                      // This is important to copytest2007_64.C
                      bool inFront = false;
                      // SgTemplateInstantiationMemberFunctionDecl*
                      // nondefiningMemberFunctionInstantiation =
                      // isSgTemplateInstantiationMemberFunctionDecl(memberFunctionInstantiation->get_firstNondefiningDeclaration());

                      // I think we can assert this!
                      // ROSE_ASSERT(memberFunctionInstantiation ==
                      // nondefiningMemberFunctionInstantiation);

                      // SgDeclarationStatementPtrList::iterator
                      // locationOfNondefiningDeclaration =
                      // find(declarationList.begin(),declarationList.end(),nondefiningMemberFunctionInstantiation);
                      // if ( (classNamespaceScope != NULL ) &&
                      // (locationOfNondefiningDeclaration !=
                      // declarationList.end()) )
                      if ((classNamespaceScope != NULL)) {
                        // We have to move the prototype of the specialized
                        // template to the namespace (after the declaration)
                        SgDeclarationStatement *parentDeclaration =
                            SageInterface::
                                getNonInstantiatonDeclarationForClass(
                                    memberFunctionInstantiation);
                        bool inFront = false;
                        classNamespaceScope->insert_statement(
                            parentDeclaration, copyOfMemberFunction, inFront);

                        ROSE_ASSERT(copyOfMemberFunction->get_parent() ==
                                    classNamespaceScope);
                      } else {
                        // printf ("classNamespaceScope == NULL ||
                        // (locationOfNondefiningDeclaration ==
                        // declarationList.end()): Insert prototype into
                        // globalScope \n");
                        globalScope->insert_statement(
                            parentDeclaration, copyOfMemberFunction, inFront);

                        ROSE_ASSERT(copyOfMemberFunction->get_parent() ==
                                    globalScope);
                      }
                      // DQ (10/12/2007): Reset the parent to reflect the
                      // structural change.
                      // ROSE_ASSERT(copyOfMemberFunction->get_parent() ==
                      // globalScope);
                    } else {
                      // parent class not found in global scope!
                    }
                  } else {
                  }
                  // end of case "Could not find the declaration in global
                  // scope, so add it after the class declaration."
                }
              } else {
              }
            } else {
            }
            // end of else case for "if ( isSpecialization == true )"
          }
        } else {
          // Since the template declaration is in another file it will be seen
          // when we the vendor compiler processes the current file (using the
          // same includes) and so we don't have to worry about the output of
          // this instantiated template.
        }

        // END: Indentation issue in this source code.
      } else {
        // DQ (1/21/2013): This case is part of supporting template instnations
        // than don't have an associated template declaration.
      }

    } // if (memberFunctionInstantiation != NULL)
  } // for loop over set of needed declarations
}

void MarkTemplateInstantiationsForOutput::ProcessFunctionTemplateDeclarations(
    set<SgDeclarationStatement *> setOfRequiredDeclarations,
    SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);

  for (set<SgDeclarationStatement *>::iterator i =
           setOfRequiredDeclarations.begin();
       i != setOfRequiredDeclarations.end(); i++) {
    // Iterate through the function recorded as required for compilation of this
    // file!

    // ROSE_ASSERT((*i)->get_definingDeclaration() != NULL);
    // ROSE_ASSERT((*i)->get_firstNondefiningDeclaration() != NULL);

    SgDeclarationStatement *definingDeclaration =
        (*i)->get_definingDeclaration();
    SgDeclarationStatement *firstNondefiningDeclaration =
        (*i)->get_firstNondefiningDeclaration();

    bool isDefiningDeclaration =
        definingDeclaration == NULL ? false : (*i == definingDeclaration);
    bool isfirstNondefiningDeclaration =
        firstNondefiningDeclaration == NULL
            ? false
            : (*i == firstNondefiningDeclaration);
    // first debug the member functions!
    SgTemplateInstantiationFunctionDecl *functionInstantiation =
        isSgTemplateInstantiationFunctionDecl(*i);
    if (functionInstantiation != NULL) {
      // At least one of these should be true!
      ROSE_ASSERT(isfirstNondefiningDeclaration == true ||
                  isDefiningDeclaration == true);

      // If the template declaration is in the current file then we need not
      // output the instantiation (skip marking instantiation for output!)
      SgDeclarationStatement *templateDeclaration =
          functionInstantiation->get_templateDeclaration();

      // DQ (11/3/2012): Changed this assertion to a conditional.
      // Looking at the code for where the template declaration is set, it seems
      // reasonable that it might not be found since p->assoc_template == NULL.
      // However it seems that there are other ways to find it.  This is written
      // about in a note in
      // setTemplateOrTemplateInstantiationFunctionGeneration().
      // ROSE_ASSERT(templateDeclaration != NULL);
      if (templateDeclaration != NULL) {
        string currentFilename = (file != NULL) ? file->getFileName() : "";
        string filenameOfTemplateDeclaration =
            templateDeclaration->get_file_info()->get_filename();
        if (filenameOfTemplateDeclaration == currentFilename) {
          bool isSpecialization = functionInstantiation->isSpecialization();
          if (isSpecialization == true) {
            markForOutputInCodeGeneration(functionInstantiation);
          }
        }
      }
    }
  }
}

void MarkTemplateInstantiationsForOutput::ProcessClassTemplateDeclarations(
    set<SgDeclarationStatement *> setOfRequiredDeclarations,
    SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);

  for (set<SgDeclarationStatement *>::iterator i =
           setOfRequiredDeclarations.begin();
       i != setOfRequiredDeclarations.end(); i++) {
    // Iterate through the function recorded as required for compilation of this
    // file!

    // ROSE_ASSERT((*i)->get_definingDeclaration() != NULL);
    // ROSE_ASSERT((*i)->get_firstNondefiningDeclaration() != NULL);

    SgDeclarationStatement *definingDeclaration =
        (*i)->get_definingDeclaration();
    SgDeclarationStatement *firstNondefiningDeclaration =
        (*i)->get_firstNondefiningDeclaration();

    bool isDefiningDeclaration =
        definingDeclaration == NULL ? false : (*i == definingDeclaration);
    bool isfirstNondefiningDeclaration =
        firstNondefiningDeclaration == NULL
            ? false
            : (*i == firstNondefiningDeclaration);
    // first debug the member functions!

    SgTemplateInstantiationDecl *classInstantiation =
        isSgTemplateInstantiationDecl(*i);
    if (classInstantiation != NULL) {
      // At least one of these should be true!
      ROSE_ASSERT(isfirstNondefiningDeclaration == true ||
                  isDefiningDeclaration == true);

      // If the template declaration is in the current file then we need not
      // output the instantiation (skip marking instantiation for output!)
      SgDeclarationStatement *templateDeclaration =
          classInstantiation->get_templateDeclaration();
      ROSE_ASSERT(templateDeclaration != NULL);
      string currentFilename = (file != NULL) ? file->getFileName() : "";
      string filenameOfTemplateDeclaration =
          templateDeclaration->get_file_info()->get_filename();
      if (filenameOfTemplateDeclaration == currentFilename) {
        bool isSpecialization = classInstantiation->isSpecialization();
        if (isSpecialization == true) {
          markForOutputInCodeGeneration(classInstantiation);
        }
      }
    }
  }
}

void markTemplateInstantiationsForOutput(SgNode *node) {
  // This function marks template instantiations for output within code
  // generation. By isolating the control over the output of template
  // instantiations we simplify the design of the unparser (code generator).
  // Note also that while template specializations appear as template
  // declaration in their syntax, they are functionally just explicit template
  // instantiations.

  // This function has multiple phases:
  //    1) Locate all template instantiations that are referenced in the source
  //    file. 2) Iterate over the list of instantiatied member functions
  //          a) if it is a specialization then mark it for output
  //          b) if the associated template definition appears in the source
  //          file then
  //                1. if it is defined in the class then mark the instantiation
  //                for output
  //                        legacy frontend does not include the definition of
  //                        member function in the string representing the
  //                        template definition.
  //                2. if it is not defined in the class then don't output the
  //                instantiation.
  //              FOR G++ 3.3.x
  //                3. Build a prototype for the member function and insert it
  //                into the correct
  //                   scope (global scope should work).
  //              FOR G++ 3.4.x and likely 4.x (also works for 3.3.x)
  //                4. Move the definition to appear after the class
  //                instantiation's definition
  //    3) Iterate over the template function instantiations
  //          a) if it is a specialization then mark it for output
  //          b) if the associated template definition appears in the source
  //          file then
  //             do NOT mark it for output.
  //    4) Iterate over the template class instantiations
  //          a) if it is a specialization then mark it for output
  //          b) if the associated template definition appears in the source
  //          file then
  //             do NOT mark it for output.
  //          c) if a member function (or friend function) of the class template
  //          instantiation
  //             is marked for output but the class template instantiation is
  //             not then copy the member function's declaration and insert it
  //             after the class template instantiation.

  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer("Mark template instantiations for output:");

  // DQ (8/2/2005): Added better handling of AST fragments where template
  // handling is not required! DQ (7/29/2005): Added support with Qing for AST
  // framents that occure in the ASTInterface classes.
  SgSourceFile *file = NULL;
  SgProject *project = NULL;
  // bool buildImplicitTemplates = false;

  ROSE_ASSERT(node != NULL);
  file = SageInterface::getEnclosingSourceFile(node, false);
  project = isSgProject(node);
  // buildImplicitTemplates = (file != NULL) &&
  // (file->get_no_implicit_templates() == false);

  // printf ("buildImplicitTemplates = %s \n",buildImplicitTemplates ? "true" :
  // "false"); if (buildImplicitTemplates == true)

  // printf ("markTemplateInstantiationsForOutput using file = %p \n",file);

  // This fixup is only possible if we have access to the entire AST (including
  // SgFile)
  if (file != NULL) {
    // *************************************************************
    // Collect template instantiations that are used and MIGHT be output
    // *************************************************************

    // Build the lists of declarations. This requires multiple passes
    // to include function included by functions previously included
    // So we generate one list for each pass, this helps the debugging.
    // since we only look for declarations that are both defined in the
    // current file and used in the current source file we only record
    // those templates that are required.
    set<SgDeclarationStatement *> setOfRequiredDeclarations =
        MarkTemplateInstantiationsForOutput::
            BuildSetOfRequiredTemplateDeclarations(node, file);

    // ***************************************************************************************
    // Iterate over member function template instantiations and figure out which
    // ones to output
    // ***************************************************************************************
    MarkTemplateInstantiationsForOutput::
        ProcessMemberFunctionTemplateDeclarations(setOfRequiredDeclarations,
                                                  file);

    // ********************************************************************************
    // Iterate over function template instantiations and figure out which ones
    // to output
    // ********************************************************************************

    MarkTemplateInstantiationsForOutput::ProcessFunctionTemplateDeclarations(
        setOfRequiredDeclarations, file);

    // *****************************************************************************
    // Iterate over class template instantiations and figure out which ones to
    // output
    // *****************************************************************************

    MarkTemplateInstantiationsForOutput::ProcessClassTemplateDeclarations(
        setOfRequiredDeclarations, file);

  } // end of if (file != NULL)
  else {
    if (project != NULL) {
      // GB (9/4/2009): Added this case for handling SgProject nodes. We do
      // this simply by iterating over the list of files in the project and
      // calling this function recursively. This is only one level of
      // recursion since files are not nested.
      SgFilePtrList &files = project->get_fileList();
      SgFilePtrList::iterator fIterator;
      for (fIterator = files.begin(); fIterator != files.end(); ++fIterator) {
        SgFile *file = *fIterator;
        ROSE_ASSERT(file != NULL);
        markTemplateInstantiationsForOutput(file);
      }
    }
  }
}

MarkTemplateInstantiationsForOutputSupportInheritedAttribute::
    MarkTemplateInstantiationsForOutputSupportInheritedAttribute() {
  insideDeclarationToOutput = false;
}

MarkTemplateInstantiationsForOutputSupportSynthesizedAttribute::
    MarkTemplateInstantiationsForOutputSupportSynthesizedAttribute() {}

MarkTemplateInstantiationsForOutputSupport::
    MarkTemplateInstantiationsForOutputSupport(SgSourceFile *inputFile) {
  // Save the file in the traversal class so that we can access the backend
  // specific template instatiation control flags easily.
  ROSE_ASSERT(inputFile != NULL);
  currentFile = inputFile;
}

void MarkTemplateInstantiationsForOutputSupport::saveDeclaration(
    SgDeclarationStatement *declaration) {
  // Put the declaration's first non-defining declaration in the
  // list with it's defining declaration, if it exists.

  SgDeclarationStatement *firstNondefiningDeclaration =
      declaration->get_firstNondefiningDeclaration();
  SgDeclarationStatement *definingDeclaration =
      declaration->get_definingDeclaration();
  // DQ (8/30/2005): This need not always exist either
  // ROSE_ASSERT(firstNondefiningDeclaration != NULL);

  // DQ (6/22/2005): The defining declaration does not always exist (e.g.
  // "~int();") ROSE_ASSERT(definingDeclaration != NULL);

  // DQ (8/30/2005): This should at leas be true!
  ROSE_ASSERT(firstNondefiningDeclaration != NULL ||
              definingDeclaration != NULL);

  if (firstNondefiningDeclaration != NULL) {
    if (is_frontend_suppressed_template_instantiation(
            firstNondefiningDeclaration)) {
      firstNondefiningDeclaration = NULL;
    }
  }

  if (firstNondefiningDeclaration != NULL) {
    // DQ (3/31/2013): Only add each declaration once!
    // listOfTemplateDeclarationsToOutput.push_back(firstNondefiningDeclaration);
    if (find(listOfTemplateDeclarationsToOutput.begin(),
             listOfTemplateDeclarationsToOutput.end(),
             firstNondefiningDeclaration) ==
        listOfTemplateDeclarationsToOutput.end()) {
      listOfTemplateDeclarationsToOutput.push_back(firstNondefiningDeclaration);
    }
    ROSE_ASSERT(find(listOfTemplateDeclarationsToOutput.begin(),
                     listOfTemplateDeclarationsToOutput.end(),
                     firstNondefiningDeclaration) !=
                listOfTemplateDeclarationsToOutput.end());
  }

  // The defining declaration does not always exist! (e.g. "extern struct
  // _IO_FILE_plus _IO_2_1_stdin_;")
  if (definingDeclaration != NULL) {
    if (is_frontend_suppressed_template_instantiation(definingDeclaration)) {
      definingDeclaration = NULL;
    }
  }

  if (definingDeclaration != NULL) {
    // DQ (3/31/2013): Only add each declaration once!
    // listOfTemplateDeclarationsToOutput.push_back(definingDeclaration);
    if (find(listOfTemplateDeclarationsToOutput.begin(),
             listOfTemplateDeclarationsToOutput.end(),
             definingDeclaration) == listOfTemplateDeclarationsToOutput.end()) {
      listOfTemplateDeclarationsToOutput.push_back(definingDeclaration);
    }
    ROSE_ASSERT(find(listOfTemplateDeclarationsToOutput.begin(),
                     listOfTemplateDeclarationsToOutput.end(),
                     definingDeclaration) !=
                listOfTemplateDeclarationsToOutput.end());
  }
}

MarkTemplateInstantiationsForOutputSupportInheritedAttribute
MarkTemplateInstantiationsForOutputSupport::evaluateInheritedAttribute(
    SgNode *node, MarkTemplateInstantiationsForOutputSupportInheritedAttribute
                      inheritedAttribute) {
  MarkTemplateInstantiationsForOutputSupportInheritedAttribute returnAttribute =
      inheritedAttribute;

  // Mark this explicitly as false to turn off effect of SgGlobal turning it on
  returnAttribute.insideDeclarationToOutput = false;

  Sg_File_Info *fileInfo = node->get_file_info();
  if (fileInfo != NULL) {
    // If this is marked for output then record this in the inherited attribute
    // to be returned
    if (fileInfo->isCompilerGeneratedNodeToBeUnparsed() == true) {
      // printf ("Skipping compiler generated IR nodes to be unparsed = %s = %s
      // \n",node->sage_class_name(),SageInterface::get_name(node).c_str());
      // returnAttribute.insideDeclarationToOutput = true;
      // printf ("Found compiler generated IR node to be unparsed = %s
      // \n",node->sage_class_name());
    } else {
      // Maybe SgGlobal should return false for hasPositionInSource()?
      if (fileInfo->hasPositionInSource() == true) {
        // This node has a position is some source code so we can check if it is
        // part of the current file!
        if ((fileInfo->isSameFile(currentFile) == true) &&
            (isSgGlobal(node) == NULL)) {
          // This is a node from the current file!
          returnAttribute.insideDeclarationToOutput = true;
        }
      }
    }
  }

  return returnAttribute;
}

MarkTemplateInstantiationsForOutputSupportSynthesizedAttribute
MarkTemplateInstantiationsForOutputSupport::evaluateSynthesizedAttribute(
    SgNode *node,
    MarkTemplateInstantiationsForOutputSupportInheritedAttribute
        inheritedAttribute,
    SubTreeSynthesizedAttributes /*synthesizedAttributeList*/) {
  MarkTemplateInstantiationsForOutputSupportSynthesizedAttribute
      returnAttribute;

  // ROSE_ASSERT(inheritedAttribute.insideDeclarationToOutput == true);
  if (inheritedAttribute.insideDeclarationToOutput == true) {

    switch (node->variantT()) {
    case V_SgMemberFunctionRefExp: {
      SgMemberFunctionRefExp *memberFunctionRefExp =
          isSgMemberFunctionRefExp(node);
      // Mark the file info object as being compiler generated instead of part
      // of a transformation. If it were part of a transformation the the
      // unparser would be forced to output the associated code.
      SgMemberFunctionSymbol *memberFunctionSymbol =
          memberFunctionRefExp->get_symbol();
      ROSE_ASSERT(memberFunctionSymbol != NULL);
      SgMemberFunctionDeclaration *inputMemberFunctionDeclaration =
          memberFunctionSymbol->get_declaration();
      ROSE_ASSERT(inputMemberFunctionDeclaration != NULL);

      // printf ("inputMemberFunctionDeclaration = %p = %s symbol = %p
      // \n",inputMemberFunctionDeclaration,inputMemberFunctionDeclaration->get_name().str(),memberFunctionSymbol);
      if (isSgTemplateInstantiationMemberFunctionDecl(
              inputMemberFunctionDeclaration) != NULL)
        saveDeclaration(inputMemberFunctionDeclaration);
      break;
    }

    case V_SgFunctionRefExp: {
      SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(node);
      // Mark the file info object as being compiler generated instead of part
      // of a transformation. If it were part of a transformation the the
      // unparser would be forced to output the associated code.
      SgFunctionSymbol *functionSymbol = functionRefExp->get_symbol();
      ROSE_ASSERT(functionSymbol != NULL);
      SgFunctionDeclaration *inputFunctionDeclaration =
          functionSymbol->get_declaration();
      ROSE_ASSERT(inputFunctionDeclaration != NULL);

      // printf ("inputFunctionDeclaration = %p = %s
      // \n",inputFunctionDeclaration,inputFunctionDeclaration->get_name().str());
      if (isSgTemplateInstantiationFunctionDecl(inputFunctionDeclaration) !=
          NULL)
        saveDeclaration(inputFunctionDeclaration);
      break;
    }

      // References to named types (typedefs, enums, and classes can be hidden
      // in variable declarations, but we want to look specifically at the
      // SgInitializedName objects (the variables) and look at there types
      // individually since pointers or references to types might not count as a
      // usage that triggers a template instantiation (or output of the
      // instantiation in the code generation phase).
    case V_SgInitializedName: {
      SgInitializedName *initializedName = isSgInitializedName(node);

      SgType *type = initializedName->get_type();
      SgType *stripedType = type->stripType();
      SgNamedType *namedType = isSgNamedType(stripedType);
      if (namedType != NULL) {
        // printf ("Found a named type \n");
        SgClassType *classType = isSgClassType(type);
        if (classType != NULL) {
          // printf ("Found a named type, save the constructor for the declared
          // class  \n");
          SgDeclarationStatement *declaration = classType->get_declaration();
          ROSE_ASSERT(declaration != NULL);
          SgClassDeclaration *classDeclaration =
              isSgClassDeclaration(declaration);
          ROSE_ASSERT(classDeclaration != NULL);

          // Record that the class was used (we don't check if it was used
          // through a pointer or reference or more directly).  Only when it is
          // used directly does it really require that we provide the forward
          // declaration and the definition of the specialization.
          if (isSgTemplateInstantiationDecl(classDeclaration) != NULL) {
            saveDeclaration(classDeclaration);

            // The constructor is available through the initializer (if it
            // exists, I think) ROSE_ASSERT(initializedName->get_initptr() !=
            // NULL);
            SgInitializer *initializer = initializedName->get_initptr();
            if (initializer != NULL) {
              SgMemberFunctionDeclaration *constructor = NULL;
              switch (initializer->variantT()) {
              case V_SgConstructorInitializer: {
                // printf ("SgConstructorInitializer has been found! \n");
                SgConstructorInitializer *constructorInitializer =
                    isSgConstructorInitializer(initializer);
                constructor = constructorInitializer->get_declaration();

                // DQ (8/13/2005):
                // KULL/src/utilities/Snapshot.cc
                // demonstates this problem.
                // This need not be a valid
                // point since the constructor
                // might not explicitly be
                // defined in the class or the
                // SgConstructorInitializer
                // may be used in a way such
                // that legacy frontend does
                // not resolve the member
                // function or even the class
                // (if only the arugments are
                // relavant).
                // ROSE_ASSERT(constructor !=
                // NULL);
                break;
              }

              default: {
                // These are the SgAggregateInitializer and SgAssignInitializer
                // cases

                // printf ("Error: default in switch! \n");
                // ROSE_ABORT();
              }
              }

              if (constructor != NULL) {
                // Note that the constructor can be a member function (not a
                // template).
#if PRINT_DEVELOPER_WARNINGS
                if (isSgTemplateInstantiationMemberFunctionDecl(constructor) ==
                    NULL) {
                  printf("constructor is NOT a template (%s) \n",
                         constructor->sage_class_name());
                }
#endif
                // ROSE_ASSERT(isSgTemplateInstantiationMemberFunctionDecl(constructor)
                // != NULL);
                ROSE_ASSERT(isSgMemberFunctionDeclaration(constructor) != NULL);
                saveDeclaration(constructor);
              }
            }

            // Save the class's destructor since it will be called implicitly
            // (and MUST be declared if so)
            SgMemberFunctionDeclaration *destructor =
                SageInterface::getDefaultDestructor(classDeclaration);
            if (destructor != NULL) {
              // DQ (3/2/2019): We are trying to refine what is a template
              // instantiation and what is a normal function. I think the
              // example that is an issue here is in facat a normal function
              // (see Cxx11_tests/test2014_04.C).
              // ROSE_ASSERT(isSgTemplateInstantiationMemberFunctionDecl(destructor)
              // != NULL);

              saveDeclaration(destructor);
            }
          }
        } else {
          // This might be a typedef (not implemented), or a enum not required
          // to be supported printf ("Not a SgClassType: namedType = %p = %s
          // \n",namedType,namedType->sage_class_name());
          ROSE_ASSERT(isSgTypedefType(namedType) == NULL);
        }
      }
      break;
    }
    default: {
      // Nothing to do here!
    }
    }
  }

  return returnAttribute;
}
