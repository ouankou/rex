// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "astPostProcessing.h"

#include "resetParentPointers.h"

#include "rose_config.h"
// tps (01/14/2009): Had to define this locally as it is not part of sage3 but
// rose.h

#define DEBUG_PARENT_INITIALIZATION 0

// DQ (9/24/2007): This is Gergo's fix for the AST islands that were previously
// not traversed in the AST.
#define FIXED_ISLAND_TRAVERSAL

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

// [DQ]
// Declaration matching static member data
// list<string> ResetParentPointers::modifiedNodeInformationList;

// [DQ]
void ResetParentPointers::traceBackToRoot(SgNode *node) {
  // DQ (9/24/2007): Put this back since it is insifnicant to the performance.
  // DQ (9/24/2007): Comment out to check the performance, this test should
  // really be done in the AST consistancy tests. Trace the current node back as
  // far as possible (should be able to reach SgGlobal)
  SgNode *parentNode = node;

  // printf ("Starting at parentNode->sage_class_name() = %s
  // \n",parentNode->sage_class_name());
  int counter = 0;
  while ((parentNode != nullptr) && (parentNode->get_parent() != nullptr)) {
    parentNode = parentNode->get_parent();
    // printf ("     parentNode->sage_class_name() = %s
    // \n",parentNode->sage_class_name());
    if (counter > 1000) {
      // There is likely an error so limit path lengths back to the AST root to
      // this arbitrary distance
      printf("Error: ResetParentPointers::traceBackToRoot path to root length "
             "(1000) exceeded \n");
      ROSE_ASSERT(parentNode != nullptr);
      SgNode *grandParentNode = parentNode->get_parent();
      ROSE_ASSERT(grandParentNode != nullptr);
      printf("   starting node           = %p = %s \n", node,
             node->sage_class_name());
      printf("   (current node)          = %p = %s \n", parentNode,
             parentNode->sage_class_name());
      printf("   (current node's parent) = %p = %s \n", grandParentNode,
             grandParentNode->sage_class_name());

      SgNode *greatGrandParentNode = grandParentNode->get_parent();
      ROSE_ASSERT(greatGrandParentNode != nullptr);
      printf("   (parent node's parent)  = %p = %s \n", greatGrandParentNode,
             greatGrandParentNode->sage_class_name());

      SgNode *greatGreatGrandParentNode = greatGrandParentNode->get_parent();
      ROSE_ASSERT(greatGreatGrandParentNode != nullptr);
      printf("   (parent node's parent 2)  = %p = %s \n",
             greatGreatGrandParentNode,
             greatGreatGrandParentNode->sage_class_name());

      if (node->get_file_info() != nullptr)
        node->get_file_info()->display("node");
      if (parentNode->get_file_info() != nullptr)
        parentNode->get_file_info()->display("parentNode");
      if (grandParentNode->get_file_info() != nullptr)
        grandParentNode->get_file_info()->display("parentNode->get_parent()");

      ROSE_ABORT();
    }

    counter++;
  }

  // DQ (2/26/2004): Fixed in Sage to pass SgParent to SgFile constructor so
  // that SgProject
  //                 could be know earily in the construction of the SgFile to
  //                 support template instantiation and evaluation of SgProject
  //                 commandLine.
  // Check to see if we made it back to the root (current root is SgFile, later
  // it will be SgProject). It is also OK to stop at a node for which
  // get_parent() returns nullptr (SgType and SgSymbol nodes).

  // DQ (5/13/2004): The rot can sometimes be a SgFile node (as in the case of
  // the rewrite mechanism) if ( !( (isSgFile(parentNode) != nullptr) ||
  // (isSgProject(parentNode) != nullptr) ) ) Appling DeMorgan's Rule to the
  // previous statement we get the simpler form
  if ((isSgFile(parentNode) == nullptr) &&
      (isSgProject(parentNode) == nullptr)) {
    // DQ (10/21/2004): This is relaxed to allow setting of parent pointers from
    // manually constructed code!
#if STRICT_ERROR_CHECKING
    // Need to make this optional in some way so that this output is not
    // disturbing to users (but useful for debugging)!
    printf("Errors: could not trace back to SgFile node, path taken: \n");
    printf("Starting at AST node->sage_class_name() = %s \n",
           node->sage_class_name());
    SgLocatedNode *locatedNode = isSgLocatedNode(node);
    if (locatedNode != nullptr) {
      printf("initial node in failing path to root = %p = %s \n", locatedNode,
             locatedNode->sage_class_name());
      locatedNode->get_file_info()->display("problem AST node");
    }
    SgNode *parentNode = node;
    while (parentNode->get_parent() != nullptr) {
      parentNode = parentNode->get_parent();
      printf("     ParentNode->sage_class_name() = %s \n",
             parentNode->sage_class_name());
      SgLocatedNode *locatedNode = isSgLocatedNode(parentNode);
      if (locatedNode != nullptr) {
        printf("initial node in failing path to root = %p = %s \n", locatedNode,
               locatedNode->sage_class_name());
        locatedNode->get_file_info()->display("problem AST node");
      }
    }
#else
    // This is not an issue when AST post processing is done within construction
    // of AST fragements (e.g. loop processing) printf ("Relaxed Error Checking:
    // Commented out debugging output in
    // ResetParentPointers::traceBackToRoot(%s) traced to parent = %s \n",
    //      node->sage_class_name(),parentNode->sage_class_name());
#endif
    // DQ (10/21/2004): This is relaxed to allow setting of parent pointers from
    // manually constructed code! ROSE_ABORT();
  }
}

void ResetParentPointers::resetParentPointersInDeclaration(
    SgDeclarationStatement *declaration, SgNode *inputParent) {
  // This function makes the value in the parent consistent across the defining
  // and non-defining declarations

  // DQ (5/21/2006): Set the Sg_File_Info so that they can be traced (later we
  // might want to remove the parent pointer since it is not really required,
  // except that it is helpful for debugging). This allows declaration in
  // islands to be set!
  // declaration->get_startOfConstruct()->set_parent(declaration);
  // declaration->get_endOfConstruct()->set_parent(declaration);
  Sg_File_Info *fileInfoStart = declaration->get_startOfConstruct();
  if (fileInfoStart != nullptr) {
    if (fileInfoStart->get_parent() == nullptr) {
      fileInfoStart->set_parent(declaration);
    }
  }
  Sg_File_Info *fileInfoEnd = declaration->get_endOfConstruct();
  if (fileInfoEnd != nullptr) {
    if (fileInfoEnd->get_parent() == nullptr) {
      fileInfoEnd->set_parent(declaration);
    }
  }

  // DQ (10/12/2004): Refactored common code for setting parents in defining and
  // non-defining declarations
  ROSE_ASSERT(declaration != nullptr);
  SgDeclarationStatement *nondefiningDeclaration =
      declaration->get_firstNondefiningDeclaration();
  SgDeclarationStatement *definingDeclaration =
      declaration->get_definingDeclaration();

  SgNode *parent = nullptr;
  if (definingDeclaration != nullptr)
    parent = definingDeclaration->get_parent();
  if (parent == nullptr && nondefiningDeclaration != nullptr)
    parent = nondefiningDeclaration->get_parent();

  ROSE_ASSERT(definingDeclaration != nullptr ||
              nondefiningDeclaration != nullptr);

  if (definingDeclaration != nullptr || nondefiningDeclaration != nullptr) {
    if (parent == nullptr) {
      // DQ (10/12/2004): If neither the defining nor non-defining declaration
      // has its parent set then this is likely the first time the parent is
      // being set for any associated declaration (for the current object
      // declared).  In this case set the parent to be the parent passed in as a
      // parameter to this function.

      printf(
          "In ResetParentPointers::resetParentPointersInDeclaration(): using "
          "the inputParent = %p = %s = %s as a parent for declaration \n",
          inputParent, inputParent->class_name().c_str(),
          SageInterface::get_name(inputParent).c_str());
      parent = inputParent;
    }

    ROSE_ASSERT(parent != nullptr);
#if DEBUG_PARENT_INITIALIZATION || 0
    printf("parent of declaration = %p = %s = %s (defining or non-defining) is "
           "%p = %s \n",
           declaration, declaration->class_name().c_str(),
           SageInterface::get_name(declaration).c_str(), parent,
           parent->class_name().c_str());
#endif
    // DQ (10/9/2004): Avoid resetting any parents that are set to scope
    // statements (these are likely already correct, either a forward
    // declaration or a normal defining class declaration).
    if (definingDeclaration != nullptr &&
        definingDeclaration->get_parent() != nullptr) {
      // SgNode* currentParent = definingDeclaration->get_parent();
      // printf ("AST Fixup warning: definingDeclaration already has parent = %p
      // = %s \n",currentParent,currentParent->class_name().c_str());
    }

    // DQ (5/21/2006): Set the parent of the defining declaration (can be a non
    // SgScopeStatement) if (definingDeclaration != nullptr &&
    // isSgScopeStatement(definingDeclaration->get_parent()) == nullptr)
    if (definingDeclaration != nullptr &&
        definingDeclaration->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
      printf("AST Fixup: Setting parent of definingDeclaration = %p = %s (to "
             "parent = %p = %s) \n",
             definingDeclaration, definingDeclaration->class_name().c_str(),
             parent, parent->class_name().c_str());
#endif
      SgNode *defnDeclParent = definingDeclaration->get_parent();
      if (defnDeclParent != nullptr) {
        printf("Existing parent is already set to = %p = %s \n", defnDeclParent,
               defnDeclParent->class_name().c_str());
      }
      ROSE_ASSERT(defnDeclParent == nullptr);
      definingDeclaration->set_parent(parent);
    }
    ROSE_ASSERT(definingDeclaration == nullptr ||
                definingDeclaration->get_parent() != nullptr);

    if (nondefiningDeclaration != nullptr &&
        nondefiningDeclaration->get_parent() != nullptr) {
      // SgNode* currentParent = nondefiningDeclaration->get_parent();
      // printf ("AST Fixup warning: nondefiningDeclaration already has parent =
      // %p = %s \n",currentParent,currentParent->class_name().c_str());
    }
    // DQ (5/21/2006): Set the parent of the defining declaration (can be a non
    // SgScopeStatement) if (nondefiningDeclaration != nullptr &&
    // isSgScopeStatement(nondefiningDeclaration->get_parent()) == nullptr)
    if (nondefiningDeclaration != nullptr &&
        nondefiningDeclaration->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
      printf("AST Fixup: Setting parent of nondefiningDeclaration = %p = %s "
             "(to parent = %p = %s) \n",
             nondefiningDeclaration,
             nondefiningDeclaration->class_name().c_str(), parent,
             parent->class_name().c_str());
#endif
      if (nondefiningDeclaration->get_parent() != nullptr) {
#if DEBUG_PARENT_INITIALIZATION
        printf("Existing parent is already set to = %p = %s \n",
               nondefiningDeclaration->get_parent(),
               nondefiningDeclaration->get_parent()->class_name().c_str());
        nondefiningDeclaration->get_file_info()->display(
            "Called from reset parent: nondefiningDeclaration");
        isSgLocatedNode(nondefiningDeclaration->get_parent())
            ->get_file_info()
            ->display("Called from reset parent: parent");
#endif
      }

      // DQ (5/21/2006): We can uncomment this now (reasserted constraint)
      ROSE_ASSERT(nondefiningDeclaration->get_parent() == nullptr);
      nondefiningDeclaration->set_parent(parent);
    }
    ROSE_ASSERT(nondefiningDeclaration == nullptr ||
                nondefiningDeclaration->get_parent() != nullptr);
  }

  // DQ (10/12/2004): Check and see if this is always true!
  ROSE_ASSERT(declaration->get_parent() != nullptr);

  // Now  set the current parent (if not already set) to the same parent pointer
  if (declaration->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
    printf("AST Fixup: Setting parent of declaration = %p = %s (to parent = %p "
           "= %s) \n",
           declaration, declaration->class_name().c_str(), parent,
           parent->class_name().c_str());
#endif
    declaration->set_parent(parent);
  }
  ROSE_ASSERT(declaration->get_parent() != nullptr);

  // DQ (10/17/2004): Added assertions
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  if (classDeclaration != nullptr) {
    // DQ (10/22/2005): Changed semantics to make forward declaration have a
    // null pointer! DQ (older comment) Note that all classDeclarations have a
    // valid pointer to their definition independent of if they are defining or
    // non-defining declarations, this is different from functions which only
    // have a valid pointer to their definition if they are a defining
    // declaration.  I like that this removed opportunities for nullptr pointers
    // in the AST, but it can make it confusing for users who might only check
    // for a definition and assume it is a defining declaration if the
    // definition is found. ROSE_ASSERT(classDeclaration->get_definition() !=
    // nullptr);

    // DQ (10/22/2005): This is now conditional on having a valid definition
    if (classDeclaration->get_definition() != nullptr) {
      // DQ (10/17/2004): We have to set the definition uniformally so that even
      // definitions that would not be traversed (such as those associated with
      // hidden declarations (e.g. in typedefs or variable declarations) will
      // get their parent set).  It is reset by the normal mechanism to a value
      // consistant with the traversal if it is not set correctly.
      if (classDeclaration->get_definition()->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
        printf("Setting the parent of the class definition, since it has not "
               "yet been set. \n");
#endif
        classDeclaration->get_definition()->set_parent(classDeclaration);
      }
      ROSE_ASSERT(classDeclaration->get_definition()->get_parent() != nullptr);
    }
  }
}

void ResetParentPointers::resetParentPointersInType(SgType *typeNode,
                                                    SgNode *previousNode) {
  // DQ (10/7/2004): Refactored common code for traversing islands within the
  // AST which require a nested traversal

  // This strips off and pointer types or reference types array types and
  // modifier types that might hide the base type printf ("Initial value of
  // typeNode = %p = %s \n",typeNode,typeNode->sage_class_name());
  typeNode = typeNode->findBaseType();
  // printf ("after call to findBaseType(): typeNode = %p = %s
  // \n",typeNode,typeNode->sage_class_name());

  switch (typeNode->variantT()) {
  case V_SgClassType: {
    // Find any code (inside of a SgClassDefinition) that is buried within a
    // SgClassType. Examples of such code is: "struct X { int x; } Xvar;" or
    // "typedef struct X { int x; } Xtype;".
    SgClassType *classType = isSgClassType(typeNode);
    SgDeclarationStatement *declarationStatement = classType->get_declaration();
    ROSE_ASSERT(declarationStatement != nullptr);
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(declarationStatement);
    ROSE_ASSERT(classDeclaration != nullptr);
    // DQ (10/22/2005): The definition of a forward declaration is now nullptr
    // ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
    if (classDeclaration->get_definition() != nullptr) {
      SgClassDeclaration *definingClassDeclaration =
          classDeclaration->get_definition()->get_declaration();
      ROSE_ASSERT(definingClassDeclaration != nullptr);
      if (definingClassDeclaration->isForward() == false) {
        SgNode *existingParent = definingClassDeclaration->get_parent();
        if (existingParent != nullptr) {
          // printf ("This defining class declaration has been set previously so
          // reset to existing parent! \n");
          resetParentPointers(definingClassDeclaration, existingParent);
        } else {
          // printf ("This defining class declaration has not been set
          // previously (set to typedefDeclaration) \n");
          resetParentPointers(definingClassDeclaration, previousNode);
        }
      }
    }

    if (declarationStatement->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
      printf("AST Fixup: in declarationStatement = %p = %s parent unset, set "
             "parent = %p = %s \n",
             declarationStatement, declarationStatement->class_name().c_str(),
             previousNode, previousNode->class_name().c_str());
#endif
      declarationStatement->set_parent(previousNode);
    }
    ROSE_ASSERT(declarationStatement->get_parent() != nullptr);

    // DQ (10/22/2005): The definition of a forward declaration is now nullptr
    // ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
    if (classDeclaration->get_definition() != nullptr) {
      // DQ (10/17/2004): Added assertions
      ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
      SgClassDefinition *classDefinition = classDeclaration->get_definition();

      // Since the defintion is shared only set it if it is not already set!
      if (classDefinition->get_parent() == nullptr) {
// DQ (11/28/2009): fatal error C1017: invalid integer constant expression
#if PRINT_SIDE_EFFECT_WARNINGS || DEBUG_PARENT_INITIALIZATION
        printf(
            "Note: It would be better to set the parent of the class "
            "definition in the legacy frontend/Sage connection (I think) \n");
#endif
        if (classDeclaration->get_definingDeclaration() != nullptr)
          classDefinition->set_parent(
              classDeclaration->get_definingDeclaration());
        else
          classDefinition->set_parent(classDeclaration);
      }
      ROSE_ASSERT(classDeclaration->get_definition()->get_parent() != nullptr);
    }
    break;
  }

  case V_SgEnumType: {
    // Find any code (inside of a SgEnumDeclaration) that is buried within a
    // SgEnumType. Examples of such code is: "enum X { value = 0; } Xvar;" or
    // "typedef enum X { value = 0; } Xtype;". This is not a significant island
    // of untraversed code except that the parent pointers are not set and we
    // want to set them all!
    SgEnumType *enumType = isSgEnumType(typeNode);
    SgDeclarationStatement *declarationStatement = enumType->get_declaration();
    ROSE_ASSERT(declarationStatement != nullptr);
    SgEnumDeclaration *enumDeclaration =
        isSgEnumDeclaration(declarationStatement);
    ROSE_ASSERT(enumDeclaration != nullptr);

    // printf ("In ResetParentPointers::resetParentPointersInType(): found enum
    // declaration \n"); enumDeclaration->get_file_info()->display("found enum
    // declaration"); DQ (5/31/2006): Added to handle enum fields and there
    // parents
    resetParentPointers(enumDeclaration, previousNode);
    ROSE_ASSERT(enumDeclaration->get_parent() != nullptr);

    break;
  }

  case V_SgTypedefType: {
    // DQ (11/14/2004): Not certain that we require this case, but Kull
    // triggered the default case for a SgTypedefType so I have implemented it
    // as part of testing.

    // Find any code (inside of a SgTypedefDeclaration) that is buried within a
    // SgTypedefType. Examples of such code is: "typedef X Xtype;" where X is a
    // typedef type with a declaration. This is not a significant island of
    // untraversed code except that the parent pointers are not set and we want
    // to set them all!
    SgTypedefType *typedefType = isSgTypedefType(typeNode);
    SgDeclarationStatement *declarationStatement =
        typedefType->get_declaration();
    ROSE_ASSERT(declarationStatement != nullptr);
    SgTypedefDeclaration *typedefDeclaration =
        isSgTypedefDeclaration(declarationStatement);
    ROSE_ASSERT(typedefDeclaration != nullptr);

    // DQ (11/14/2004): If this is set to SgGlobal then reset it
    // if ( (typedefDeclaration->get_parent() == nullptr) ||
    // (isSgGlobal(typedefDeclaration->get_parent()) != nullptr) )
    if (typedefDeclaration->get_parent() == nullptr) {
      resetParentPointers(typedefDeclaration, previousNode);
    }
    ROSE_ASSERT(declarationStatement->get_parent() != nullptr);
    break;
  }

  default: {
    printf("Default reached in "
           "ResetParentPointers::resetParentPointersInType(%s) \n",
           typeNode->sage_class_name());
    ROSE_ABORT();
  }
  }
}

void ResetParentPointers::resetParentPointersInTemplateArgumentList(
    const SgTemplateArgumentPtrList &templateArgListPtr) {
  // DQ (10/15/2004): It would be helpful if this were a part of the standard
  // traversal, but it is not and it might be that the SgTemplateArgumentPtrList
  // would have to be made a new IR node to allow it to be traversed. for now we
  // will set the parents of any declarations hidden in any types explicitly (so
  // that the resetTemplateName() function will work (which requires the parent
  // pointers)).  The reason why the parent pointers are required to be set for
  // template arguments is that the template arguments can require qualified
  // names and the name qualification requires that the parents pointers be
  // traversed to trace back through the scopes and collect the names of all the
  // scopes.  This is handled in the get_scope() function which is called by the
  // get_qualified_name() function.

  // ROSE_ASSERT(templateArgListPtr != nullptr);
  // printf ("### In resetParentPointersInTemplateArgumentList():
  // templateArgListPtr->size() = %" PRIuPTR " ###
  // \n",templateArgListPtr->size());
  SgTemplateArgumentPtrList::const_iterator i = templateArgListPtr.begin();
  while (i != templateArgListPtr.end()) {
    // printf ("### In resetParentPointersInTemplateArgumentList():
    // templateArgList element *i = %s \n",(*i)->sage_class_name());
    switch ((*i)->get_argumentType()) {
    case SgTemplateArgument::argument_undefined: {
      printf("Error: SgTemplateArgument::argument_undefined not allowed \n");
      ROSE_ABORT();
    }

    case SgTemplateArgument::type_argument: {
      ROSE_ASSERT((*i)->get_type() != nullptr);
      SgType *argumentType = (*i)->get_type();
      // printf ("SgTemplateArgument::type_argument: argumentType = %p = %s
      // \n",argumentType,argumentType->sage_class_name());

      SgNamedType *namedType = isSgNamedType(argumentType);
      // printf ("### In resetParentPointersInTemplateArgumentList(): namedType
      // = %p \n",namedType);
      if (namedType != nullptr) {
        SgDeclarationStatement *declaration = namedType->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        if (declaration->get_parent() == nullptr) {
          // It should be possible to find an existing parent since defining
          // declarations can't appear within template arguments.  At least I
          // hope not!
          SgNode *existingParent = nullptr;
          // Note that the defining declaration is not required to exist
          // ("typedef struct X Y; X* xptr;" for example) printf
          // ("declaration->get_definingDeclaration() = %p
          // \n",declaration->get_definingDeclaration());
          if (declaration->get_definingDeclaration() != nullptr) {
            // Hopefully this is set by now, but likely it is not required to be
            existingParent =
                declaration->get_definingDeclaration()->get_parent();
          }
          // If still not found then look at the firstNondefiningDeclaration
          // printf ("existingParent = %p \n",existingParent);
          if (existingParent == nullptr) {
            ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() !=
                        nullptr);
            existingParent =
                declaration->get_firstNondefiningDeclaration()->get_parent();
          }

          SgTypedefType *typedefType = isSgTypedefType(namedType);
          // printf ("typedefType = %p \n",typedefType);
          if ((existingParent == nullptr) && (typedefType != nullptr)) {
            SgSymbol *symbol = typedefType->get_parent_scope();
            ROSE_ASSERT(symbol != nullptr);
            switch (symbol->variantT()) {
            case V_SgClassSymbol: {
              // printf ("In case V_SgClassSymbol: symbol = %p = %s
              // \n",symbol,symbol->sage_class_name());
              SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
              ROSE_ASSERT(classSymbol != nullptr);
              SgDeclarationStatement *declaration =
                  classSymbol->get_declaration();
              ROSE_ASSERT(declaration != nullptr);
              SgClassDeclaration *classDeclaration =
                  isSgClassDeclaration(declaration);
              ROSE_ASSERT(classDeclaration != nullptr);
              existingParent = classDeclaration->get_definition();
              ROSE_ASSERT(existingParent != nullptr);
              break;
            }

            default: {
              printf("Error: default reached symbol = %p = %s \n", symbol,
                     symbol->sage_class_name());
              ROSE_ABORT();
            }
            }
            ROSE_ASSERT(existingParent != nullptr);
          }

          if (existingParent == nullptr) {
            printf("namedType   = %p = %s \n", namedType,
                   namedType->class_name().c_str());
            printf("declaration = %p = %s \n", declaration,
                   declaration->class_name().c_str());
          }

          // DQ (3/27/2012): I think we wqant to allow declarations built to
          // support types and hidden behind types to have nullptr parents.
          // ROSE_ASSERT(existingParent != nullptr);
          if (existingParent != nullptr) {
#if DEBUG_PARENT_INITIALIZATION
            printf("Setting parent of %p = %s to %p = %s \n", declaration,
                   declaration->class_name().c_str(), existingParent,
                   existingParent->class_name().c_str());
#endif
            declaration->set_parent(existingParent);

            // DQ (10/17/2004): Added assertions
            SgClassDeclaration *classDeclaration =
                isSgClassDeclaration(declaration);
            if (classDeclaration != nullptr) {
              // DQ (1/30/2013): Commented out assertion that appears to be only
              // an issue for ROSE compiling ROSE (part of testing).
              // ROSE_ASSERT(classDeclaration->get_definition() != nullptr);
              // ROSE_ASSERT(classDeclaration->get_definition()->get_parent() !=
              // nullptr);
              if (classDeclaration->get_definition() == nullptr) {
// DQ (9/12/2014): Added more control over output of messages for release
// versions of ROSE.
#if PRINT_DEVELOPER_WARNINGS
                printf("WARNING: In "
                       "resetParentPointersInTemplateArgumentList(): commented "
                       "out to compile ROSE using ROSE: assertion failing for: "
                       "classDeclaration->get_definition() != nullptr \n");
                printf("--- classDeclaration = %p = %s = %s \n",
                       classDeclaration, classDeclaration->class_name().c_str(),
                       classDeclaration->get_name().str());
                // classDeclaration->get_file_info()->display("assertion failing
                // for: classDeclaration->get_definition() != nullptr: debug");
#endif
              } else {
                ROSE_ASSERT(classDeclaration->get_definition()->get_parent() !=
                            nullptr);
              }
            }
          } else {
            printf("WARNING: In new legacy frontend 4.x "
                   "support I want to allow some "
                   "paraents to be nullptr. \n");
          }
        }

        // DQ (10/13/2004): If this is a template declaration then we might have
        // to reset its name
        SgClassType *classType = isSgClassType(namedType);
        if (classType != nullptr) {
          SgClassDeclaration *classDeclaration =
              isSgClassDeclaration(declaration);
          ROSE_ASSERT(classDeclaration != nullptr);
          SgTemplateInstantiationDecl *templateClassDeclaration =
              isSgTemplateInstantiationDecl(classDeclaration);
          if (templateClassDeclaration != nullptr) {
            // printf ("Found a template instantiation declaration  (call
            // resetParentPointersInTemplateArgumentList) ... \n");
            resetParentPointersInTemplateArgumentList(
                templateClassDeclaration->get_templateArguments());
          }
        }
      }
      break;
    }

    case SgTemplateArgument::nontype_argument: {
      // These can be boolean or integer values, for example.

      // DQ (8/13/2013): Added support for nontype template arguments to be
      // either an expression (SgExpression) or a variable declaration
      // (SgInitializedName) ROSE_ASSERT((*i)->get_expression() != nullptr);
      if ((*i)->get_expression() != nullptr) {
        ROSE_ASSERT((*i)->get_initializedName() == nullptr);
        SgExpression *argumentExpression = (*i)->get_expression();
        if (argumentExpression->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
          printf("Setting parent in SgTemplateArgument::nontype_argument = %p "
                 "= %s \n",
                 argumentExpression, argumentExpression->class_name().c_str());
#endif
          argumentExpression->set_parent(*i);
        }
      } else {
        ROSE_ASSERT((*i)->get_initializedName() != nullptr);
        SgInitializedName *argumentInitializedName =
            (*i)->get_initializedName();

        if (argumentInitializedName->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
          printf("Setting parent in SgTemplateArgument::nontype_argument = %p "
                 "= %s \n",
                 argumentInitializedName,
                 argumentInitializedName->class_name().c_str());
#endif
          argumentInitializedName->set_parent(*i);
        }
      }

      // printf ("Error: SgTemplateArgument::nontype_argument not implemented
      // \n"); ROSE_ABORT();

      break;
    }

    case SgTemplateArgument::template_template_argument: {
      // DQ (8/24/2006): We don't want to reset the parent of a reference
      // to a shared SgTemplateDeclaration. So there is nothing to do here.

      // printf ("Error: resetParentPointersInTemplateArgumentList()
      // SgTemplateArgument::template_template_argument case not implemented
      // \n"); ROSE_ABORT();
      break;
    }

      // DQ (2/10/2014): Added this case to avoid compiler warning (I think
      // there is nothing to do here).
    case SgTemplateArgument::start_of_pack_expansion_argument: {
      // printf ("Error: resetParentPointersInTemplateArgumentList()
      // SgTemplateArgument::start_of_pack_expansion_argument case not
      // implemented \n"); ROSE_ABORT();
      break;
    }
    }

    // Increment to next template argument
    i++;
  }

  // printf ("### Leaving resetParentPointersInTemplateArgumentList():
  // templateArgListPtr->size() = %" PRIuPTR " ###
  // \n",templateArgListPtr->size());
}

namespace {
SgClassDeclaration *canonicalClassSymbolDeclaration(SgClassDeclaration *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  if (SgClassDeclaration *first =
          isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
    return first;
  }

  return decl;
}

void repairSymbolTableParents(SgScopeStatement *scope) {
  if (scope == nullptr)
    return;

  SgSymbolTable *table = scope->get_symbol_table();
  if (table == nullptr)
    return;

  if (table->get_parent() == nullptr)
    table->set_parent(scope);

  SgSymbolTable::BaseHashType *entries = table->get_table();
  if (entries == nullptr)
    return;

  for (SgSymbolTable::BaseHashType::iterator it = entries->begin();
       it != entries->end();
       /**/) {
    SgSymbol *symbol = it->second;
    if (symbol == nullptr) {
      it = entries->erase(it);
      continue;
    }

    if (symbol->get_parent() == nullptr)
      symbol->set_parent(table);

    ++it;
  }

  // Also ensure declarations in this scope own a symbol in this scope's table.
  if (isSgGlobal(scope) || isSgNamespaceDefinitionStatement(scope) ||
      isSgClassDefinition(scope) || isSgTemplateClassDefinition(scope) ||
      isSgTemplateInstantiationDefn(scope)) {
    auto class_decl_matches = [](SgClassDeclaration *lhs,
                                 SgClassDeclaration *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs == rhs) {
        return true;
      }
      SgClassDeclaration *lhs_first =
          isSgClassDeclaration(lhs->get_firstNondefiningDeclaration());
      if (lhs_first == nullptr) {
        lhs_first = lhs;
      }
      SgClassDeclaration *rhs_first =
          isSgClassDeclaration(rhs->get_firstNondefiningDeclaration());
      if (rhs_first == nullptr) {
        rhs_first = rhs;
      }
      return lhs_first == rhs_first;
    };
    auto scope_has_class_symbol = [&](SgClassDeclaration *decl) -> bool {
      if (scope == nullptr || decl == nullptr) {
        return false;
      }
      SgName name = decl->get_name();
      if (name.getString().empty()) {
        return false;
      }
      if (SgTemplateClassDeclaration *template_decl =
              isSgTemplateClassDeclaration(decl)) {
        if (SgTemplateClassSymbol *sym =
                scope->lookup_template_class_symbol(name, nullptr, nullptr)) {
          return isSgTemplateClassDeclaration(sym->get_declaration()) ==
                 template_decl;
        }
      }
      if (SgClassSymbol *sym = scope->lookup_class_symbol(name)) {
        return class_decl_matches(sym->get_declaration(), decl);
      }
      return false;
    };
    auto scope_has_enum_symbol = [&](SgEnumDeclaration *decl) -> bool {
      if (scope == nullptr || decl == nullptr) {
        return false;
      }
      SgName name = decl->get_name();
      if (name.getString().empty()) {
        return false;
      }
      if (SgEnumSymbol *sym = scope->lookup_enum_symbol(name)) {
        return sym->get_declaration() == decl;
      }
      return false;
    };
    auto class_symbol_belongs_to_scope = [&](SgClassDeclaration *decl) -> bool {
      if (scope == nullptr || decl == nullptr) {
        return false;
      }
      if (decl->get_scope() != scope) {
        return false;
      }

      SgNamedType *named_type = isSgNamedType(decl->get_type());
      if (named_type == nullptr) {
        return true;
      }

      SgDeclarationStatement *type_decl = named_type->get_declaration();
      if (type_decl == nullptr || type_decl->get_scope() == nullptr) {
        return true;
      }

      return type_decl->get_scope() == scope;
    };
    SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
    for (SgDeclarationStatement *decl : decls) {
      if (decl == nullptr)
        continue;

      if (SgTemplateInstantiationDecl *tid =
              isSgTemplateInstantiationDecl(decl)) {
        SgClassDeclaration *symbol_decl = canonicalClassSymbolDeclaration(tid);
        ROSE_ASSERT(symbol_decl != nullptr);
        if (symbol_decl->get_name().getString().empty())
          continue;
        if (!class_symbol_belongs_to_scope(symbol_decl))
          continue;
        if (scope_has_class_symbol(symbol_decl))
          continue;
        SgClassSymbol *sym = new SgClassSymbol(symbol_decl);
        if (table != nullptr) {
          table->insert(symbol_decl->get_name(), sym);
          sym->set_parent(table);
        }
        continue;
      }

      if (SgClassDeclaration *cd = isSgClassDeclaration(decl)) {
        SgClassDeclaration *symbol_decl = canonicalClassSymbolDeclaration(cd);
        ROSE_ASSERT(symbol_decl != nullptr);
        if (symbol_decl->get_name().getString().empty())
          continue;
        if (!class_symbol_belongs_to_scope(symbol_decl))
          continue;
        if (scope_has_class_symbol(symbol_decl))
          continue;
        if (SgTemplateClassDeclaration *tcd =
                isSgTemplateClassDeclaration(symbol_decl)) {
          SgTemplateClassSymbol *sym = new SgTemplateClassSymbol(tcd);
          if (table != nullptr) {
            table->insert(tcd->get_name(), sym);
            sym->set_parent(table);
          }
          continue;
        }

        SgClassSymbol *sym = new SgClassSymbol(symbol_decl);
        if (table != nullptr) {
          table->insert(symbol_decl->get_name(), sym);
          sym->set_parent(table);
        }
        continue;
      }

      if (SgEnumDeclaration *ed = isSgEnumDeclaration(decl)) {
        if (ed->get_name().getString().empty())
          continue;
        if (scope_has_enum_symbol(ed))
          continue;
        SgEnumSymbol *sym = new SgEnumSymbol(ed);
        if (table != nullptr) {
          table->insert(ed->get_name(), sym);
          sym->set_parent(table);
        }
        continue;
      }

      if (SgFunctionDeclaration *fd = isSgFunctionDeclaration(decl)) {
        if (fd->get_name().getString().empty())
          continue;
        if (fd->search_for_symbol_from_symbol_table() != nullptr)
          continue;
        SgScopeStatement *decl_scope = fd->get_scope();
        if (decl_scope != nullptr && decl_scope != scope)
          continue;
        SgType *ftype = fd->get_type();
        if (table != nullptr &&
            table->find_function(fd->get_name(), ftype) != nullptr)
          continue;

        SgSymbol *sym = nullptr;
        if (SgTemplateMemberFunctionDeclaration *tmpl_mem =
                isSgTemplateMemberFunctionDeclaration(fd)) {
          sym = new SgTemplateMemberFunctionSymbol(tmpl_mem);
        } else if (SgTemplateFunctionDeclaration *tmpl_func =
                       isSgTemplateFunctionDeclaration(fd)) {
          sym = new SgTemplateFunctionSymbol(tmpl_func);
        } else if (SgMemberFunctionDeclaration *mem =
                       isSgMemberFunctionDeclaration(fd)) {
          sym = new SgMemberFunctionSymbol(mem);
        } else {
          sym = new SgFunctionSymbol(fd);
        }
        if (table != nullptr && sym != nullptr) {
          table->insert(sym->get_name(), sym);
          sym->set_parent(table);
        }
        continue;
      }
    }
  }
}
} // namespace

ResetParentPointersInheritedAttribute
ResetParentPointers::evaluateInheritedAttribute(
    SgNode *node, ResetParentPointersInheritedAttribute inheritedAttribute) {
  ROSE_ASSERT(node != nullptr);
  // cerr << "reset parent for node " << node->unparseToString();

  if (SgScopeStatement *scope = isSgScopeStatement(node)) {
    repairSymbolTableParents(scope);
  }

  // Fix the parent pointer in the subtree (can't fix root node parent pointer
  // so this shoud be called from above any node that requires an update (safe
  // nodes would be the SgProject node) fix only made if parent == nullptr)

  // DQ (5/10/2006): Set the Sg_File_Info so that they can be traced (later we
  // might want to remove the parent pointer since it is not really required,
  // except that it is helpful for debugging).
  // node->get_startOfConstruct()->set_parent(declaration);
  // node->get_endOfConstruct()->set_parent(declaration);
  // Sg_File_Info* fileInfoStart = node->get_startOfConstruct();
  Sg_File_Info *fileInfoStart = node->get_file_info();
  if (fileInfoStart != nullptr) {
    if (fileInfoStart->get_parent() == nullptr) {
      fileInfoStart->set_parent(node);
    }
  }
  Sg_File_Info *fileInfoEnd = node->get_endOfConstruct();
  if (fileInfoEnd != nullptr) {
    if (fileInfoEnd->get_parent() == nullptr) {
      fileInfoEnd->set_parent(node);
    }
  }

  // Handle the end of construct
  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  if (locatedNode != nullptr) {
    Sg_File_Info *localFileInfoEndOfConstruct = node->get_endOfConstruct();
    if (localFileInfoEndOfConstruct != nullptr) {
      if (localFileInfoEndOfConstruct->get_parent() == nullptr) {
        localFileInfoEndOfConstruct->set_parent(node);
      } else {
        // printf ("Error: parent of localFileInfo is already set \n");
        // localFileInfoEndOfConstruct->display("parent of localFileInfo is
        // already set");
      }
    }
  }

  SgInitializedName *initializedName = isSgInitializedName(node);
  if (initializedName != nullptr) {
    // printf ("ResetParentPointers: initializedName = %p \n",initializedName);
    SgStorageModifier *modifier = &(initializedName->get_storageModifier());
    if (modifier != nullptr) {
      if (modifier->get_parent() == nullptr) {
        modifier->set_parent(initializedName);
      }
    }
  }

  if (SgConstructorInitializer *ctorInit = isSgConstructorInitializer(node)) {
    // Some constructor initializer nodes originate from compiler-generated
    // declarations in system headers and may not have an associated
    // declaration. Mark them explicitly so consistency checks do not expect a
    // class or declaration pointer.
    if (ctorInit->get_declaration() == nullptr &&
        ctorInit->get_class_decl() == nullptr) {
      ctorInit->set_associated_class_unknown(true);
    }
  }

  // #ifdef REMOVE_SET_PARENT_FUNCTION

  // Note: SgType and SgSymbol nodes are not fixed (currently) (SgType nodes
  //       and SgSymbol nodes are shared))
  // Set all nodes except SgSymbol and SgType nodes (even if they have been set
  // previously set)
  if (dynamic_cast<SgType *>(node) == nullptr &&
      dynamic_cast<SgSymbol *>(node) == nullptr) {
    // Handle the part of the tree that is not hidden in the islands
    if (inheritedAttribute.parentNode != nullptr) {
      // set the parent on the current node to the one saved in the inherited
      // attribute (for this traversal)
      ROSE_ASSERT(node != nullptr);
      if (node->get_parent() != inheritedAttribute.parentNode) {
        // DQ (8/1/2019): Output information where the node->get_parent() !=
        // inheritedAttribute.parentNode
        // ROSE_ASSERT(inheritedAttribute.parentNode != nullptr);
        // ROSE_ASSERT(node->get_parent() != nullptr);
      }

      // Set the parent node to the parent saved in the inherited attribute

      // DQ (11/1/2005): Only reset parents that are already nullptr
#if DEBUG_PARENT_INITIALIZATION
      printf("AST Fixup: Setting parent of node = %p = %s (to parent = %p = "
             "%s) \n",
             node, node->class_name().c_str(), inheritedAttribute.parentNode,
             inheritedAttribute.parentNode->class_name().c_str());
#endif

      // DQ (6/2/2006): This can lead to the parent of a SgClassDeclaration for
      // a type in a function parameter to be set to the SgFunctionParameterList
      // (by mistake). node->set_parent(inheritedAttribute.parentNode);
      if (node->get_parent() == nullptr) {
        node->set_parent(inheritedAttribute.parentNode);
      }
    } else {
      // printf ("On node->sage_class_name() = %s inheritedAttribute.parentNode
      // == nullptr (not set) \n",
      //      node->sage_class_name());
      if (!dynamic_cast<SgProject *>(node) && !dynamic_cast<SgFile *>(node)) {
        // DQ (10/21/2004): This is relaxed to allow setting of parent pointers
        // from manually constructed code!
#if STRICT_ERROR_CHECKING
        // Only SgProject and SgFile can be root nodes after legacy
        // frontend->SAGE translation
        printf("Warning: only SgProject and SgFile can be root nodes "
               "after legacy frontend->SAGE translation \n");
        ROSE_ABORT();
#endif
      }
    }

    // Test chain of parents back to the root node of the AST (test on non
    // SgType and non SgSymbol nodes)
    traceBackToRoot(node);

    // Handle possible islands in the AST (generally typedef statements where
    // the types need to be traversed)
    switch (node->variantT()) {
    case V_SgTemplateInstantiationDecl: {
      SgTemplateInstantiationDecl *templateInstantiation =
          isSgTemplateInstantiationDecl(node);
      ROSE_ASSERT(templateInstantiation != nullptr);
      ROSE_ASSERT(inheritedAttribute.parentNode != nullptr);
      resetParentPointersInDeclaration(templateInstantiation,
                                       inheritedAttribute.parentNode);

      // TV (05/29/2018): possible if it is non-real
      // ROSE_ASSERT(templateInstantiation->get_templateDeclaration() !=
      // nullptr);

      // DQ (10/15/2004): Now we have to reset the parents of any declarations
      // appearing in the template argument list!  Unless we should define the
      // traversal to traverse that list! Could there be cycles introduced this
      // way???  For now maybe we should just visit them explicitly (and check
      // with Markus).
      // ROSE_ASSERT(templateInstantiation->get_templateArguments() != nullptr);
      resetParentPointersInTemplateArgumentList(
          templateInstantiation->get_templateArguments());
      break;
    }

      // DQ (8/18/2005): Added case of template member function so that we could
      // set the SgTemplateDeclaration
    case V_SgTemplateInstantiationMemberFunctionDecl: {
      SgTemplateInstantiationMemberFunctionDecl *templateInstantiation =
          isSgTemplateInstantiationMemberFunctionDecl(node);
      ROSE_ASSERT(templateInstantiation != nullptr);
      ROSE_ASSERT(inheritedAttribute.parentNode != nullptr);

      // this is likely redundant
      resetParentPointersInDeclaration(templateInstantiation,
                                       inheritedAttribute.parentNode);

      SgDeclarationStatement *templateDeclaration =
          templateInstantiation->get_templateDeclaration();
      if (templateDeclaration == nullptr) {
      }
      // DQ (5/3/2012): commented out for the new legacy
      // frontend 4.3 support. ROSE_ASSERT(templateDeclaration !=
      // nullptr);

      // DQ (10/15/2004): Now we have to reset the parents of any declarations
      // appearing in the template argument list!  Unless we should define the
      // traversal to traverse that list! Could there be cycles introduced this
      // way???  For now maybe we should just visit them explicitly (and check
      // with Markus).
      // ROSE_ASSERT(templateInstantiation->get_templateArguments() != nullptr);
      resetParentPointersInTemplateArgumentList(
          templateInstantiation->get_templateArguments());
      break;
    }

      // DQ (10/9/2004): We would like to set all declarations to have parents
      // based on their defining or first non-defining declarations.
    case V_SgClassDeclaration: {
      // Set any of the defining or nondefining declaration parent pointers
      // these will be used to set parents of other class declarations
      // referenced within types.
      SgClassDeclaration *classDeclaration = isSgClassDeclaration(node);
      ROSE_ASSERT(classDeclaration != nullptr);
      ROSE_ASSERT(inheritedAttribute.parentNode != nullptr);
      resetParentPointersInDeclaration(classDeclaration,
                                       inheritedAttribute.parentNode);
      break;
    }

    case V_SgVarRefExp: {
      // DQ (5/1/2005): some SgInitializedName are not traversed and so there
      // parents are not set! Note that hidden in the SgVarRefExp is a
      // SgVariableSymbol with a SgInitializedName which will not have its
      // parent set since we don't traverse the SgVariableSymbol within the
      // SgVarRefExp to find the hidden SgInitializedName object.  Why?  Well,
      // in general symbols are shared and we can't guarentee uniqueness of the
      // visit of many symbols. The SgInitializedName is in the SgVariableSymbol
      // and so it is never visited!

      // Other IR nodes where the containing symbols which are not traversed
      // include:
      //      SgClassNameRefExp (contains a SgClassSymbol)
      //      SgFunctionRefExp (contains a SgFunctionSymbol)
      //      MemberFunctionRefExp (contains a SgMemberFunctionSymbol)
      //      ThisExp (contains a SgClassSymbol)
      // But only the SgVariableSymbol's declaration is a SgInitializedName, so
      // no other IR nodes must be specially handled (I think).

      SgVarRefExp *variableRefExpression = isSgVarRefExp(node);
      ROSE_ASSERT(variableRefExpression != nullptr);

      SgVariableSymbol *variableSymbol = variableRefExpression->get_symbol();

      if (variableSymbol == nullptr) {
        printf(
            "WARNING: variableSymbol == nullptr: variableRefExpression = %p \n",
            variableRefExpression);
      }
      ROSE_ASSERT(variableSymbol != nullptr);

      // DQ (1/1/2014): I think we may have to allow this for cases such as that
      // in test2014_01.c But I would prefer to have a sysmbol always built so
      // that ROSE had a consistant representation. Initially we want to allow
      // this so that we can get the graph of the AST so that I can understand
      // the problem better.
      if (variableSymbol != nullptr) {
        // This is bit confusing since what is returned is the SgInitializedName
        // and NOT a declaration!
        SgInitializedName *initializedName = variableSymbol->get_declaration();

        // DQ (2/6/2020): Added debugging information.
        if (initializedName == nullptr) {
          fprintf(stderr,
                  "In resetParentPointers.C: case V_SgVarRefExp: "
                  "variableSymbol = %p = %s \n",
                  variableSymbol, variableSymbol->class_name().c_str());
          bool found_symbol_name = false;
          if (SgProject *project = SageInterface::getProject(node)) {
            Rose_STL_Container<SgNode *> scopes =
                NodeQuery::querySubTree(project, V_SgScopeStatement);
            for (SgNode *scope_node : scopes) {
              SgScopeStatement *scope = isSgScopeStatement(scope_node);
              if (scope == nullptr) {
                continue;
              }
              SgSymbolTable *symtab = scope->get_symbol_table();
              if (symtab == nullptr) {
                continue;
              }
              rose_hash_multimap *table = symtab->get_table();
              if (table == nullptr) {
                continue;
              }
              for (auto it = table->begin(); it != table->end(); ++it) {
                if (it->second == variableSymbol) {
                  std::string symbol_name = it->first.getString();
                  fprintf(stderr,
                          "In resetParentPointers.C: case V_SgVarRefExp: "
                          "symbol table name = %s \n",
                          symbol_name.c_str());
                  found_symbol_name = true;
                  break;
                }
              }
              if (found_symbol_name) {
                break;
              }
            }
          }
          if (!found_symbol_name) {
            fprintf(stderr, "In resetParentPointers.C: case V_SgVarRefExp: "
                            "symbol name not found in symbol tables\n");
          }
          fprintf(stderr,
                  "In resetParentPointers.C: case V_SgVarRefExp: "
                  "variableRefExpression = %p = %s \n",
                  variableRefExpression,
                  variableRefExpression->class_name().c_str());
          if (SgFunctionDeclaration *enclosing =
                  SageInterface::getEnclosingFunctionDeclaration(
                      variableRefExpression)) {
            fprintf(stderr,
                    "In resetParentPointers.C: case V_SgVarRefExp: "
                    "enclosing function = %s \n",
                    enclosing->get_name().str());
          }
          for (SgNode *parent = variableRefExpression->get_parent();
               parent != nullptr; parent = parent->get_parent()) {
            if (SgStatement *stmt = isSgStatement(parent)) {
              fprintf(stderr,
                      "In resetParentPointers.C: case V_SgVarRefExp: "
                      "enclosing statement = %p = %s \n",
                      stmt, stmt->class_name().c_str());
              if (Sg_File_Info *stmt_info = stmt->get_file_info()) {
                fprintf(stderr,
                        "In resetParentPointers.C: case V_SgVarRefExp: "
                        "statement location line=%d col=%d file=%s \n",
                        stmt_info->get_line(), stmt_info->get_col(),
                        stmt_info->get_filenameString().c_str());
              }
              break;
            }
          }
          if (Sg_File_Info *info = variableRefExpression->get_file_info()) {
            fprintf(stderr,
                    "In resetParentPointers.C: case V_SgVarRefExp: "
                    "location line=%d col=%d file=%s \n",
                    info->get_line(), info->get_col(),
                    info->get_filenameString().c_str());
          }
          fflush(stderr);
        }
        ROSE_ASSERT(initializedName != nullptr);

        // printf ("AST fixup: In a SgVarRefExp found a SgInitializedName = %p
        // \n",initializedName);

        if (initializedName->get_parent() == nullptr) {
          // Set the parent to be the SgVarRefExp (since setting it to the
          // symbol would not productive, because symbols can be shared!)
#if DEBUG_PARENT_INITIALIZATION
          printf("Setting parent of %p = %s to %p = %s \n", initializedName,
                 initializedName->class_name().c_str(), variableRefExpression,
                 variableRefExpression->class_name().c_str());
#endif
          initializedName->set_parent(variableRefExpression);
        }
        ROSE_ASSERT(initializedName->get_parent() != nullptr);
      }

      break;
    }

    case V_SgInitializedName: {
      // Find the types within the function declaration and set the parents of
      // any declarations that are contained in the types.
      SgInitializedName *initializedName = isSgInitializedName(node);
      ROSE_ASSERT(initializedName != nullptr);
      // DQ (9/6/2005): Set the parents of SgInitializedName objects
      // This problem shows up in the loop processor test codes,
      // not clear if it is a real problem or not!
      if (initializedName->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
        printf("Warning Resetting the parent (previously nullptr) of a "
               "SgInitializedName object! \n");
#endif
        initializedName->set_parent(inheritedAttribute.parentNode);
      }
      // ROSE_ASSERT(initializedName->get_parent() != nullptr);

      SgType *type = initializedName->get_type();
      if (type != nullptr) {
        type = type->findBaseType();
        SgClassType *classType = isSgClassType(type);
        if (classType != nullptr) {
          SgDeclarationStatement *declaration = classType->get_declaration();
          ROSE_ASSERT(declaration != nullptr);
          SgClassDeclaration *classDeclaration =
              isSgClassDeclaration(declaration);
          ROSE_ASSERT(classDeclaration != nullptr);
          resetParentPointers(classDeclaration, inheritedAttribute.parentNode);
        }
      }

      SgInitializedName *previousInitializedName =
          initializedName->get_prev_decl_item();
      if (previousInitializedName != nullptr) {
        // This can sometimes have a null parent (test2005_67.C) (for non-static
        // member)
        if (previousInitializedName->get_parent() == nullptr) {
          printf("Warning (previousInitializedName->get_parent() == nullptr): "
                 "initializedName = %p previousInitializedName = %p get_name() "
                 "= %s \n",
                 initializedName, previousInitializedName,
                 previousInitializedName->get_name().str());
          ROSE_ASSERT(previousInitializedName->get_scope() != nullptr);
          printf("--- previousInitializedName->get_scope() = %p = %s \n",
                 previousInitializedName->get_scope(),
                 previousInitializedName->get_scope()->class_name().c_str());
        }
        // DQ (2/12/2011): Commented out to support generation of graph to debug
        // test2011_08.C, this test codes demonstrates that the
        // SgInitializedName build first might only be to support a symbol and
        // not have a proper parent.
        // ROSE_ASSERT(previousInitializedName->get_parent() != nullptr);
        // DQ (6/5/2011): Commented out as part of name qualification testing...
        if (previousInitializedName->get_prev_decl_item() != nullptr)
          ROSE_ASSERT(previousInitializedName->get_parent() != nullptr);
      }

#if STRICT_ERROR_CHECKING
      SgDeclarationStatement *declarationStatement =
          initializedName->get_declaration();
      ROSE_ASSERT(declarationStatement != nullptr);
      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declarationStatement);
      // ROSE_ASSERT(classDeclaration != nullptr);
      if (classDeclaration != nullptr &&
          classDeclaration->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
        printf("Setting parent of class declaration = %p found in "
               "SgInitializedName \n",
               classDeclaration);
#endif
        ROSE_ASSERT(classDeclaration->get_firstNondefiningDeclaration() !=
                    nullptr);
        ROSE_ASSERT(
            classDeclaration->get_firstNondefiningDeclaration()->get_parent() !=
            nullptr);
        SgNode *existingParent =
            classDeclaration->get_firstNondefiningDeclaration()->get_parent();
        ROSE_ASSERT(existingParent != nullptr);
        classDeclaration->set_parent(existingParent);
        ROSE_ASSERT(classDeclaration->get_parent() != nullptr);
      }
#endif
      break;
    }

      // DQ (10/9/2004): function declarations have types that can have
      // declarations so find the types and set the parents of all the
      // declarations.
    case V_SgFunctionDeclaration: {
      // Find the types within the function declaration and set the parents of
      // any declarations that are contained in the types.
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(node);
      ROSE_ASSERT(functionDeclaration != nullptr);
      SgType *returnType = functionDeclaration->get_orig_return_type();
#if STRICT_ERROR_CHECKING
      ROSE_ASSERT(returnType != nullptr);
#endif
      if (returnType != nullptr) {
        returnType = returnType->findBaseType();
        SgClassType *classType = isSgClassType(returnType);
        if (classType != nullptr) {
          SgDeclarationStatement *declaration = classType->get_declaration();
          ROSE_ASSERT(declaration != nullptr);
          SgClassDeclaration *classDeclaration =
              isSgClassDeclaration(declaration);
          ROSE_ASSERT(classDeclaration != nullptr);
          if (classDeclaration->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
            printf("Setting parent of class declaration found in return type "
                   "of function \n");
#endif
            classDeclaration->set_parent(functionDeclaration);
          }
          // DQ (10/17/2004): Modified this to not
          // enforce assertion that defining declaration
          // existed. Implemented the same fix for the
          // firstNondefiningDeclaration as well.

          // ROSE_ASSERT(classDeclaration->get_definingDeclaration()
          // != nullptr);
          if (classDeclaration->get_definingDeclaration() != nullptr) {
            // DQ (1/30/2013): Commented out assertion
            // that appears to be only an issue for ROSE
            // compiling ROSE (part of testing).
            if (classDeclaration->get_definingDeclaration()->get_parent() ==
                nullptr) {
// DQ (9/12/2014): Added more control over output of messages for release
// versions of ROSE.
#if PRINT_DEVELOPER_WARNINGS
              printf("WARNING: In resetParentPointersInTemplateArgumentList(): "
                     "commented out to compile ROSE using ROSE: assertion "
                     "failing for: "
                     "classDeclaration->get_definingDeclaration()->get_parent()"
                     " != nullptr \n");
              printf("--- classDeclaration = %p = %s = %s \n", classDeclaration,
                     classDeclaration->class_name().c_str(),
                     classDeclaration->get_name().str());
              // classDeclaration->get_file_info()->display("assertion failing
              // for: classDeclaration->get_definingDeclaration()->get_parent()
              // != nullptr: debug");
#endif
            }
            // ROSE_ASSERT(classDeclaration->get_definingDeclaration()->get_parent()
            // != nullptr);
          }

          // ROSE_ASSERT(classDeclaration->get_firstNondefiningDeclaration() !=
          // nullptr);
          if (classDeclaration->get_firstNondefiningDeclaration() != nullptr) {
            ROSE_ASSERT(classDeclaration->get_firstNondefiningDeclaration()
                            ->get_parent() != nullptr);
          }
        }
      }
      break;
    }

    case V_SgTypedefDeclaration: {
      // printf ("Found a SgTypedefDeclaration = %p looking for islands of
      // untraversed AST ... \n",node);
      SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(node);
      ROSE_ASSERT(typedefDeclaration != nullptr);

      // typedefDeclaration->get_file_info()->display("case
      // V_SgTypedefDeclaration: typedefDeclaration");

#ifndef FIXED_ISLAND_TRAVERSAL
      bool islandFound =
          typedefDeclaration->get_typedefBaseTypeContainsDefiningDeclaration();
      // printf ("islandFound = %s \n",(islandFound == true) ? "true" :
      // "false");
      if (islandFound == true) {
        // We only want to traverse the base type (since all "*" and "&" are
        // associated with the variables in this variable list, e.g. list of
        // SgInitializedName objects)

        // DQ (5/21/2006): Set the parent of the declaration in the typedef if
        // it exists (it should if "islandFound == true").
        ROSE_ASSERT(typedefDeclaration->get_declaration() != nullptr);
        // DQ (5/21/2006): Not true for enum declaration in typedef.
        // ROSE_ASSERT(typedefDeclaration->get_declaration()->get_parent() !=
        // nullptr);
        if (typedefDeclaration->get_declaration()->get_parent() !=
            typedefDeclaration) {
          // printf ("Reset the parent of embedded declaration in typedef to the
          // typedef \n");
          typedefDeclaration->get_declaration()->set_parent(typedefDeclaration);
        }
        // DQ (5/21/2006): reset the parent points in the island represented by
        // the declaration
        resetParentPointersInDeclaration(typedefDeclaration->get_declaration(),
                                         typedefDeclaration);

        SgType *baseType = typedefDeclaration->get_base_type();
        ROSE_ASSERT(baseType != nullptr);
        resetParentPointersInType(baseType, typedefDeclaration);
      }
#endif

      // DQ (10/12/2007): A forward enum will not have its parent set, see
      // test2007_92.C.  E.g. "typedef enum zero number;" so we need to test
      // this independent of the island problem (which is now fixed).
      // #ifndef FIXED_ISLAND_TRAVERSAL
      // DQ (10/14/2004): Even if this is not an island it might be a typedef of
      // a template which needs its name to be reset.  If it is a typedef that
      // is later used then it would be reset where the type of the variable
      // declaration would be seen, but if it is not used then we will only see
      // it here and thus we have to find and reset the name of the template
      // declaration.
      SgType *baseType = typedefDeclaration->get_base_type();
      ROSE_ASSERT(baseType != nullptr);
      // printf ("baseType = %p = %s \n",baseType,baseType->sage_class_name());
      SgNamedType *namedType = isSgNamedType(baseType);

      // DQ (10/16/2004): Need to set this since typedef types are used in cases
      // where they must be provided with qualified names and so there parents
      // must be setup properly!
      if (namedType != nullptr) {
        SgDeclarationStatement *declaration = namedType->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        if (declaration->get_parent() == nullptr) {
          // DQ (10/16/2004): reset pointers locared within declarations held
          // within types printf ("$$$ Reset pointers locared within
          // declarations held within types $$$ \n");
          resetParentPointersInType(namedType, typedefDeclaration);
          // printf ("$$$ DONE with reset pointers locared within declarations
          // held within types $$$ \n");
        }
        ROSE_ASSERT(declaration->get_parent() != nullptr);
      }
      // #endif
      break;
    }

    case V_SgVariableDeclaration: {
      // printf ("Found a SgVariableDeclaration = %p looking for islands of
      // untraversed AST ... \n",node);
      SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(node);
      ROSE_ASSERT(variableDeclaration != nullptr);

#ifndef FIXED_ISLAND_TRAVERSAL
      bool islandFound =
          variableDeclaration
              ->get_variableDeclarationContainsBaseTypeDefiningDeclaration();
      printf("variableDeclaration                                    = %p \n",
             variableDeclaration);
      printf("variableDeclaration->get_definingDeclaration()         = %p \n",
             variableDeclaration->get_definingDeclaration());
      printf("variableDeclaration->get_firstNondefiningDeclaration() = %p \n",
             variableDeclaration->get_firstNondefiningDeclaration());
      variableDeclaration->get_startOfConstruct()->display(
          "setup parents within SgVariableDeclaration");

      bool isSameAsDefiningDeclaration =
          (variableDeclaration ==
           variableDeclaration->get_definingDeclaration());
      printf("isSameAsDefiningDeclaration = %s \n",
             isSameAsDefiningDeclaration ? "true" : "false");
      bool isSameAsFirstNondefiningDeclaration =
          (variableDeclaration ==
           variableDeclaration->get_firstNondefiningDeclaration());
      printf("isSameAsFirstNondefiningDeclaration = %s \n",
             isSameAsFirstNondefiningDeclaration ? "true" : "false");
      printf("islandFound = %s \n", (islandFound == true) ? "true" : "false");
      if (islandFound == true) {
        // We only want to traverse the base type (since all "*" and "&" are
        // associated with the variables in this variable list, e.g. list of
        // SgInitializedName objects)
        SgInitializedNamePtrList &variableList =
            variableDeclaration->get_variables();
        ROSE_ASSERT(variableList.size() > 0);
        SgInitializedName *firstVariable = *(variableList.begin());
        ROSE_ASSERT(firstVariable != nullptr);
        // DQ (5/21/2006): Set the parent of the declaration in the typedef if
        // it exists (it should if "islandFound == true").
        // SgDeclarationStatement* declaration = nullptr;
        SgType *variableType = firstVariable->get_typeptr();
        ROSE_ASSERT(variableType != nullptr);
        SgNamedType *namedType = isSgNamedType(variableType);

        // DQ (6/21/2006): Handle case of indirection in SgNamedType variables,
        // for example SgArrayType (e.g. "struct { int x; } ArrayVar [100];").
        if (namedType == nullptr) {
          SgType *baseType = variableType->stripType();
          ROSE_ASSERT(baseType != nullptr);
          namedType = isSgNamedType(baseType);
          ROSE_ASSERT(namedType != nullptr);
        }

        ROSE_ASSERT(namedType != nullptr);
        ROSE_ASSERT(namedType->get_declaration() != nullptr);
        // DQ (5/21/2006): Not true for enum declaration in typedef.
        // ROSE_ASSERT(namedType->get_declaration()->get_parent() != nullptr);
        if (namedType->get_declaration()->get_parent() != variableDeclaration) {
          // printf ("Reset the parent of embedded declaration in variable
          // declaration to the SgVariableDeclaration \n");
          namedType->get_declaration()->set_parent(variableDeclaration);
          ROSE_ASSERT(namedType->get_declaration()->get_definingDeclaration() !=
                      nullptr);
          namedType->get_declaration()->get_definingDeclaration()->set_parent(
              variableDeclaration);
          if (namedType->get_declaration()->get_firstNondefiningDeclaration() !=
              nullptr)
            namedType->get_declaration()
                ->get_firstNondefiningDeclaration()
                ->set_parent(variableDeclaration);
        }
        // DQ (5/21/2006): reset the parent points in the island represented by
        // the declaration
        resetParentPointersInDeclaration(namedType->get_declaration(),
                                         variableDeclaration);
        SgType *baseType = firstVariable->get_typeptr();
        ROSE_ASSERT(baseType != nullptr);
        resetParentPointersInType(baseType, variableDeclaration);
      }
#endif
      break;
    }

      // DQ (9/24/2005): This need to be traversed since the declaration is not
      // traversed as part of the traversal (though it could be) DQ (4/16/2005):
      // Added support for explicit template instantation directives
    case V_SgTemplateInstantiationDirectiveStatement: {
      // At the moment we don't traverse the decalaration hidden in a
      // SgTemplateInstantiationDirectiveStatement printf ("Found a
      // SgTemplateInstantiationDirectiveStatement = %p ... \n",node);
      SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(node);
      ROSE_ASSERT(directive != nullptr);
      SgDeclarationStatement *declaration = directive->get_declaration();
      ROSE_ASSERT(declaration != nullptr);
      if (declaration->get_parent() == nullptr) {
#if DEBUG_PARENT_INITIALIZATION
        printf("Setting parent of %p = %s to %p = %s \n", declaration,
               declaration->class_name().c_str(), directive,
               directive->class_name().c_str());
#endif
        declaration->set_parent(directive);
      } else {
        // DQ (2/9/2014): This was an error, but it only shows up in the use of
        // the GNU 4.6 header files (and it is not clear that it should be an
        // error).  So output a message as we debug this issue.
        if (declaration->get_parent() != directive) {
        }

        // DQ (3/15/2006): Why is it an error to have this be a valid pointer?
        // The parent should be the directive, I think.
        // ROSE_ASSERT(declaration->get_parent() == directive);
      }
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          isSgMemberFunctionDeclaration(declaration);
      if (memberFunctionDeclaration != nullptr) {
        SgCtorInitializerList *ctors =
            memberFunctionDeclaration->get_CtorInitializerList();
        ROSE_ASSERT(ctors != nullptr);
        ROSE_ASSERT(ctors->get_parent() != nullptr);
      }
      break;
    }

    default: {
      // Only trap out typedefs and variable declarations
      break;
    }
    }
  } else {
    if (isSgType(node) != nullptr) {
      // Types are shared through the global type tables, so they must not pick
      // up an arbitrary tree parent from this traversal fallback.
    } else if (SgSymbol *symbol = isSgSymbol(node)) {
      if (inheritedAttribute.parentNode != nullptr &&
          symbol->get_parent() == nullptr &&
          isSgSymbolTable(inheritedAttribute.parentNode) != nullptr) {
        symbol->set_parent(inheritedAttribute.parentNode);
      }
    } else {
      printf("Found an unsupported node while resetting parents \n");
      printf("$$$$$ In evaluateInheritedAttribute() \n");
      printf("   --- astNode->class_name() = %s \n",
             node->class_name().c_str());
      ROSE_ABORT();
    }
  }

  // I/O useful for debugging
  // if ( SgProject::get_verbose() >= DIAGNOSTICS_VERBOSE_LEVEL )
  if (SgProject::get_verbose() > mlogLevel) {
    if (node->get_parent() != nullptr) {
      // Test if we have a parent that we would expect to have (given the
      // traversal that we are using)
      SgNode *suggestedNode = inheritedAttribute.parentNode;
      SgNode *parentNode = node->get_parent();
      if (inheritedAttribute.parentNode != parentNode) {
        // Interesting node
        string currentNodeString = node->class_name();
        string expectedNodeString =
            (suggestedNode) ? suggestedNode->class_name() : "nullptr POINTER";
        string recordedNodeString =
            (parentNode) ? parentNode->class_name() : "nullptr POINTER";
        printf("Note: On %s node expected parent (%s) didn't match recorded "
               "parent (%s) \n",
               currentNodeString.c_str(), expectedNodeString.c_str(),
               recordedNodeString.c_str());
      }
    }
  }

  // Always set the parent node point in the inherited attribute
  inheritedAttribute.parentNode = node;

  return inheritedAttribute;
}

// DQ (9/25/2004): build another function that has a better interface and really
// works with any SgNode
void resetParentPointers(SgNode *node, SgNode *parent) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  // TimingPerformance timer ("(reset parent pointers) time (sec) = ");

  // printf ("Resetting the parent pointers ... (starting at node = %s)
  // \n",node->sage_class_name());
  ResetParentPointersInheritedAttribute inheritedAttribute;

  inheritedAttribute.parentNode = parent;

  ResetParentPointers setParentPointerTraversal;
  setParentPointerTraversal.traverse(node, inheritedAttribute);
}

void ResetParentPointersOfClassAndNamespaceDeclarations::visit(SgNode *node) {
  // DQ (11/1/2005): Reset parent pointers of and data members
  // or namespace members to the class or namespace.

  ROSE_ASSERT(node != nullptr);

  switch (node->variantT()) {
  case V_SgClassDefinition:
  case V_SgTemplateInstantiationDefn: {
    SgClassDefinition *classDefinition = isSgClassDefinition(node);
    ROSE_ASSERT(classDefinition != nullptr);
    SgDeclarationStatementPtrList &memberList = classDefinition->get_members();
    SgDeclarationStatementPtrList::iterator i = memberList.begin();
    while (i != memberList.end()) {
      if ((*i)->get_parent() != classDefinition) {
        // Then we need to reset the parent!
#if DEBUG_PARENT_INITIALIZATION
        printf("Resetting parent of data member of SgClassDefinition or "
               "SgTemplateInstantiationDefn \n");
#endif
        (*i)->set_parent(classDefinition);
      }
      i++;
    }
    break;
  }

  case V_SgNamespaceDefinitionStatement: {
    SgNamespaceDefinitionStatement *namespaceDefinition =
        isSgNamespaceDefinitionStatement(node);
    ROSE_ASSERT(namespaceDefinition != nullptr);
    SgDeclarationStatementPtrList &declarationList =
        namespaceDefinition->get_declarations();
    SgDeclarationStatementPtrList::iterator i = declarationList.begin();
    while (i != declarationList.end()) {
      if ((*i)->get_parent() != namespaceDefinition) {
        // Then we need to reset the parent!
#if DEBUG_PARENT_INITIALIZATION
        printf("Resetting parent of data member of "
               "SgNamespaceDefinitionStatement \n");
#endif
        (*i)->set_parent(namespaceDefinition);
      }
      i++;
    }
    break;
  }

  case V_SgGlobal: {
    SgGlobal *globalScope = isSgGlobal(node);
    ROSE_ASSERT(globalScope != nullptr);
    SgDeclarationStatementPtrList &declarationList =
        globalScope->get_declarations();
    SgDeclarationStatementPtrList::iterator i = declarationList.begin();
    while (i != declarationList.end()) {
      if ((*i)->get_parent() != globalScope) {
        // Then we need to reset the parent!
#if DEBUG_PARENT_INITIALIZATION
        printf("Resetting parent of data member of SgGlobal \n");
#endif
        (*i)->set_parent(globalScope);
      }
      i++;
    }
    break;
  }

  case V_SgBasicBlock: {
    SgBasicBlock *blockScope = isSgBasicBlock(node);
    ROSE_ASSERT(blockScope != nullptr);
    SgStatementPtrList &statementList = blockScope->get_statements();
    SgStatementPtrList::iterator i = statementList.begin();
    while (i != statementList.end()) {
      if ((*i)->get_parent() != blockScope) {
        // Then we need to reset the parent!
#if DEBUG_PARENT_INITIALIZATION
        printf("Resetting parent of data member of SgBasicBlock \n");
#endif
        (*i)->set_parent(blockScope);
      }
      i++;
    }
    break;
  }

  default: {
    // Nothing else to do here!
  }
  }
}

// DQ (9/25/2004): build another function that has a better interface and really
// works with any SgNode
void resetParentPointersOfClassOrNamespaceDeclarations(SgNode *node) {
  // DQ (11/1/2005): In a separate pass set the data members of any class or
  // namespace to have a parent pointing at the class definition or the
  // namespace.

  // printf ("Resetting the parent pointers ... (starting at node = %s)
  // \n",node->sage_class_name());
  ResetParentPointersOfClassAndNamespaceDeclarations setParentPointerTraversal;
  setParentPointerTraversal.traverse(node, preorder);
}

void topLevelResetParentPointer(SgNode *node) {
  // Put this in a local scope so that the timing can be easily controled
  // this is required because the resetParentPointers() function is called
  // recursively.

  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance resetParentPointerTimer("Reset parent pointers:");

  // reset the parent pointers
  resetParentPointers(node, node->get_parent());

  // DQ (9/24/2007): This might not be required now that AST islands are fixed.
  // This IS required! reset the parent pointers in any class definitions,
  // namespace definitions or global scope
  resetParentPointersOfClassOrNamespaceDeclarations(node);
}

void resetFileInfoParentPointersInMemoryPool() {
  ResetFileInfoParentPointersInMemoryPool t;
  t.traverseMemoryPool();
}

void ResetFileInfoParentPointersInMemoryPool::visit(SgNode *node) {

  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  SgSupport *support = isSgSupport(node);

  // All types should have nullptr parent pointers (because types can be shared)
  if (locatedNode != nullptr) {
    if (locatedNode->get_startOfConstruct() == nullptr) {
      printf(
          "Error: locatedNode->get_startOfConstruct() == nullptr (locatedNode "
          "= %p = %s) \n",
          locatedNode, locatedNode->class_name().c_str());
      if (isSgFunctionParameterList(node) != nullptr) {
        // DQ (9/13/2011): Reported as possible nullptr value in static analysis
        // of ROSE code.
        SgNode *parent = locatedNode->get_parent();
        ROSE_ASSERT(parent != nullptr);

        printf("     This is a SgFunctionParameterList, so look at the parent "
               "= %p = %s \n",
               parent, parent->class_name().c_str());
      }

      if (locatedNode->get_parent() == nullptr) {
        printf("     locatedNode->get_parent() locatedNode = %p = %s \n",
               locatedNode, locatedNode->class_name().c_str());
      }
      // DQ (2/12/2012): Refactoring disagnostic support for detecting where we
      // are when something fails.
      SageInterface::whereAmI(locatedNode);
      // ROSE_ASSERT(locatedNode->get_file_info() != nullptr);
      // locatedNode->get_parent()->get_file_info()->display("Error:
      // locatedNode->get_startOfConstruct() == nullptr");
    }
    ROSE_ASSERT(locatedNode->get_startOfConstruct() != nullptr);

    if (locatedNode->get_startOfConstruct()->get_parent() == nullptr) {
      locatedNode->get_startOfConstruct()->set_parent(locatedNode);
    }
    ROSE_ASSERT(locatedNode->get_startOfConstruct()->get_parent() != nullptr);

    if (locatedNode->get_endOfConstruct() != nullptr) {
      if (locatedNode->get_endOfConstruct()->get_parent() == nullptr) {
        locatedNode->get_endOfConstruct()->set_parent(locatedNode);
      }
      ROSE_ASSERT(locatedNode->get_endOfConstruct()->get_parent() != nullptr);
    }
  }

  if (support != nullptr) {
    switch (support->variantT()) {
      // These are the only SgSupport IR nodes that have a Sg_File_Info object
      // pointer.
    case V_SgRenamePair:
    case V_SgPragma: {
      if (support->get_file_info() == nullptr)
        printf("support node = %p = %s \n", support,
               support->class_name().c_str());

      ROSE_ASSERT(support->get_file_info() != nullptr);
      if (support->get_file_info()->get_parent() == nullptr) {
        support->get_file_info()->set_parent(support);
      }
      ROSE_ASSERT(support->get_file_info()->get_parent() != nullptr);
      break;
    }
      // case V_SgFile:
    case V_SgSourceFile:
    case V_SgUnknownFile: {
      ROSE_ASSERT(support->get_file_info() != nullptr);
      ROSE_ASSERT(support->get_file_info()->get_parent() != nullptr);
      break;
    }

    default: {
      // All other SgSupport should call teh SgNode virtual base class function
      // and return nullptr
      if (support->get_file_info() != nullptr) {
        printf("Error: support->get_file_info() != nullptr for support = %p = "
               "%s \n",
               support, support->class_name().c_str());
      }
      ROSE_ASSERT(support->get_file_info() == nullptr);
      break;
    }
    }
  }
}

// DQ (8/23/2012): Modified to take a SgNode so that we could compute the global
// scope for use in setting parents of template instantiations that have not be
// placed into the AST but exist in the memory pool. This is called directly
// from void postProcessingSupport (SgNode* node). void
// resetParentPointersInMemoryPool()
void resetParentPointersInMemoryPool(SgNode *node) {
  // There are two traversals here, these could be combined since they both
  // operate on the memory pool.

  TimingPerformance timer("Reset parent pointers in memory pool:");

  ROSE_ASSERT(node != nullptr);

  SgGlobal *globalScope = nullptr;
  SgProject *project = isSgProject(node);
  if (project != nullptr) {
    SgFile *file = (*project)[0];

    SgSourceFile *sourceFile = isSgSourceFile(file);

    // ROSE_ASSERT(sourceFile != nullptr);
    if (sourceFile != nullptr) {
      globalScope = sourceFile->get_globalScope();
    }

    ROSE_ASSERT(globalScope != nullptr);
  } else {
    SgSourceFile *sourceFile = isSgSourceFile(node);
    if (sourceFile != nullptr) {
      globalScope = sourceFile->get_globalScope();
    } else {
      // DQ (8/5/2019): It is a bit of a problem that we have to allow this to
      // work when not using a SgProject or SgSourceFile, but this is required
      // for the inlining tests.  Note that these inlined tests will not have
      // the resetFileInfoParentPointersInMemoryPool() be called.

      // DQ (8/2/2019): This function is only meaningful to call with the
      // SgProject node, and will do nothing otherwise. This may be the source
      // of the problem in using the SageBuilder::buildFile() API in testing
      // test2019_501.C). If so then this function should be fixed to allow a
      // SgFile to be used alternatively. printf ("In
      // resetParentPointersInMemoryPool(): This function can't be called using
      // anything but the SgProject: node = %p = %s
      // \n",node,node->class_name().c_str()); ROSE_ABORT();

      // DQ (8/6/2019): Alternatively, let's use the existing parent pointers to
      // serach for the associated global scope.
      bool includingSelf = true;
      globalScope =
          SageInterface::getEnclosingNode<SgGlobal>(node, includingSelf);

      if (globalScope == nullptr) {
        // DQ (8/6/2019): Make it an error to not have found an associated
        // global scope from the input node.
        printf("Error: In resetParentPointersInMemoryPool(): Could not locate "
               "global scope in search upward through the AST from this node = "
               "%p = %s \n",
               node, node->class_name().c_str());
      } else {
      }
      ROSE_ASSERT(globalScope != nullptr);
    }
  }

  // ROSE_ASSERT(globalScope != nullptr);

  // DQ (10/9/2012): Make this conditional upon having found a valid
  // SgGlobal.
  if (globalScope != nullptr) {
    ResetParentPointersInMemoryPool t(globalScope);

    ROSE_ASSERT(t.globalScope != nullptr);

    t.traverseMemoryPool();

    // JJW: Moved this down because it requires that some non-Sg_File_Info
    // parent pointers have been set
    // Reset parents of any remaining unset Sg_File_Info object first
    resetFileInfoParentPointersInMemoryPool();
  } else {
    // DQ (8/2/2019): This function is only meaningful to call with the
    // SgProject node, and will do nothing otherwise. printf ("In
    // resetParentPointersInMemoryPool(): This function is not doing
    // anything when called using: node = %p = %s
    // \n",node,node->class_name().c_str()); ROSE_ABORT();
  }
}

void ResetParentPointersInMemoryPool::visit(SgNode *node) {

  // I built a pointer to global scope so that we could use it for this case.
  ROSE_ASSERT(globalScope != nullptr);

  SgType *type = isSgType(node);
  SgSymbol *symbol = isSgSymbol(node);
  SgLocatedNode *locatedNode = isSgLocatedNode(node);
  SgSupport *support = isSgSupport(node);

  // All types should have nullptr parent pointers (because types can be shared)
  if (type != nullptr) {
    // Note that the SgNode::get_parent() function is forced to return nullptr
    // for the case of a SgType IR node ROSE_ASSERT(type->get_parent() ==
    // nullptr);
  }

  // Symbols can be shared within a single file but are not yet shared across
  // files in the AST merge
  if (symbol != nullptr) {
    // If the parent pointer is stale, clear it so we can repair below.
    if (SgSymbolTable *parent_table = isSgSymbolTable(symbol->get_parent())) {
      if (parent_table->exists(symbol) == false) {
        symbol->set_parent(nullptr);
      }
    }
    // Should point to the associated SgSymbolTable
    if (symbol->get_parent() == nullptr) {
      // printf ("In ResetParentPointersInMemoryPool::visit(): symbol = %p = %s
      // get_parent() == nullptr \n",symbol,symbol->class_name().c_str());

      auto ensure_symbol_parent = [](SgSymbol *sym, SgScopeStatement *scope) {
        ROSE_ASSERT(sym != nullptr);
        ROSE_ASSERT(scope != nullptr);
        ROSE_ASSERT(scope->get_symbol_table() != nullptr);
        if (scope->symbol_exists(sym) == false) {
          scope->insert_symbol(sym->get_name(), sym);
        } else if (sym->get_parent() != scope->get_symbol_table()) {
          sym->set_parent(scope->get_symbol_table());
        }
      };

      switch (symbol->variantT()) {
      case V_SgFunctionSymbol: {
        SgFunctionSymbol *tempSymbol = isSgFunctionSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgFunctionDeclaration *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();
        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgMemberFunctionSymbol: {
        SgMemberFunctionSymbol *tempSymbol = isSgMemberFunctionSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgFunctionDeclaration *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();
        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgVariableSymbol: {
        SgVariableSymbol *tempSymbol = isSgVariableSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgInitializedName *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();

        // DQ (6/24/2006): There should be a test that detects this scope
        // problem!
        if (scope == nullptr) {
          printf("Looking for the scope in the SgVariableSymbol through the "
                 "definition (declaration = %p = %s = %s) \n",
                 declaration, declaration->class_name().c_str(),
                 SageInterface::get_name(declaration).c_str());
          ROSE_ASSERT(declaration->get_definition() != nullptr);

          SgDeclarationStatement *declarationStatement =
              declaration->get_definition();
          ROSE_ASSERT(declarationStatement != nullptr);
          ROSE_ASSERT(declarationStatement->get_scope() != nullptr);
          printf("Looking for the scope in the SgVariableSymbol: "
                 "declarationStatement = %p = %s \n",
                 declarationStatement,
                 declarationStatement->class_name().c_str());
          scope = declarationStatement->get_scope();
          ROSE_ASSERT(scope != nullptr);
        }

        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgTemplateSymbol: {
        SgTemplateSymbol *tempSymbol = isSgTemplateSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgTemplateDeclaration *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();
        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgTypedefSymbol: {
        SgTypedefSymbol *tempSymbol = isSgTypedefSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgTypedefDeclaration *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();
        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgLabelSymbol: {
        SgLabelSymbol *tempSymbol = isSgLabelSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgLabelStatement *declaration = tempSymbol->get_declaration();

        // DQ (12/9/2007): Added support for fortran labels
        // ROSE_ASSERT(declaration != nullptr);
        if (declaration != nullptr) {
          SgScopeStatement *scope = declaration->get_scope();
          ROSE_ASSERT(scope != nullptr);
          ensure_symbol_parent(symbol, scope);
        } else {
          printf("Support for testing fortran lables might be incomplete! \n");
          SgStatement *fortranStatement = tempSymbol->get_fortran_statement();
          ROSE_ASSERT(fortranStatement != nullptr);
        }

        break;
      }

      case V_SgClassSymbol: {
        SgClassSymbol *tempSymbol = isSgClassSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgClassDeclaration *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();
        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgEnumSymbol: {
        SgEnumSymbol *tempSymbol = isSgEnumSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgEnumDeclaration *declaration = tempSymbol->get_declaration();
        ROSE_ASSERT(declaration != nullptr);
        SgScopeStatement *scope = declaration->get_scope();
        ROSE_ASSERT(scope != nullptr);
        ensure_symbol_parent(symbol, scope);
        break;
      }

      case V_SgFunctionTypeSymbol: {
        SgFunctionTypeSymbol *tempSymbol = isSgFunctionTypeSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);
        SgType *type = tempSymbol->get_type();
        SgSymbolTable *table = nullptr;
        if (isSgFunctionType(type) != nullptr ||
            isSgMemberFunctionType(type) != nullptr) {
          SgFunctionTypeTable *funcTable =
              SgNode::get_globalFunctionTypeTable();
          ROSE_ASSERT(funcTable != nullptr);
          table = funcTable->get_function_type_table();
        } else {
          SgTypeTable *typeTable = SgNode::get_globalTypeTable();
          ROSE_ASSERT(typeTable != nullptr);
          table = typeTable->get_type_table();
        }
        ROSE_ASSERT(table != nullptr);
        if (table->exists(tempSymbol) == false) {
          table->insert(tempSymbol->get_name(), tempSymbol);
        }
        if (tempSymbol->get_parent() != table) {
          tempSymbol->set_parent(table);
        }
        break;
      }

        // DQ (2/28/2015): Added support for SgAliasSymbol case.
      case V_SgAliasSymbol: {
        SgAliasSymbol *tempSymbol = isSgAliasSymbol(symbol);
        ROSE_ASSERT(tempSymbol != nullptr);

        // DQ (2/28/2015): I think this is not possible to fix here, so we need
        // to report the error and exit.
        printf("ERROR: parent for SgAliasSymbol not set (can't be fixed up "
               "here) \n");
        ROSE_ABORT();
      }

      default: {
        printf("Error: default reached in switch(symbol->variantT()) symbol = "
               "%p = %s \n",
               symbol, symbol->class_name().c_str());
        ROSE_ABORT();
      }
      }
    }
    ROSE_ASSERT(symbol->get_parent() != nullptr);
  }
  // Skip SgExpression object for now!
  locatedNode = isSgStatement(locatedNode);

  // SgStatement and SgExpression IR nodes should always have a valid parent
  // (except for the SgProject)
  if (locatedNode != nullptr) {
    switch (locatedNode->variantT()) {
    case V_SgClassDeclaration:
    case V_SgTemplateInstantiationDecl: {
      // At this point the AST traversal has been used to set the parents and we
      // can use information from defining and non-defining declaration to set
      // parents of extrainious non-defining declarations accessible only from
      // the memory pool.  We only reset nullptr pointers.
      SgClassDeclaration *declaration = isSgClassDeclaration(locatedNode);
      if (declaration != nullptr && declaration->get_parent() == nullptr) {
        SgDeclarationStatement *definingDeclaration =
            declaration->get_definingDeclaration();
        SgDeclarationStatement *nondefiningDeclaration =
            declaration->get_firstNondefiningDeclaration();
        SgNode *parentOfRelatedDeclaration = nullptr;
        if (definingDeclaration != nullptr) {
          parentOfRelatedDeclaration = definingDeclaration->get_parent();
        }
        if (parentOfRelatedDeclaration == nullptr &&
            nondefiningDeclaration != nullptr) {
          parentOfRelatedDeclaration = nondefiningDeclaration->get_parent();
        }

        if (parentOfRelatedDeclaration != nullptr) {
          declaration->set_parent(parentOfRelatedDeclaration);
        }
        if (declaration->get_parent() == nullptr) {
          // DQ (3/6/2017): Converted to use message logging.
          // MLOG_WARN_C("astPostProcessing", "#####
          // ResetParentPointersInMemoryPool::visit(declaration = %p = %s)
          // declaration->get_parent() == nullptr
          // \n",node,node->class_name().c_str()); DQ (8/23/2012): For remaining
          // template instantiationsthat only have a non-definng declaration,
          // set the parent to the global scope (since they don't appear to be
          // connected to anything else).
          SgTemplateInstantiationDecl *templateInstantiation =
              isSgTemplateInstantiationDecl(nondefiningDeclaration);
          if (templateInstantiation != nullptr) {
            printf("WARNING: This is a case of a template class instantiation "
                   "that does not appear in the AST but exists in the memory "
                   "pool as part of the new refined disambiguation of template "
                   "instantations using template arguments. \n");

            // I built a pointer to global scope so that we could use it for
            // this case.
            ROSE_ASSERT(globalScope != nullptr);
            templateInstantiation->set_parent(globalScope);
          }
        }
        // DQ (6/10/2007): Test for null parents before the call to
        // resetTemplateNames() DQ (6/22/2006): Commented out temporarily for
        // debugging use of glob.h

        // DQ (8/3/2019): This assertion is a problem (failing C language test)
        // for test2012_47.c (and about 27 other C language tests). DQ
        // (8/2/2019): reintroduce this asseretion.
        // ROSE_ASSERT(declaration->get_parent() != nullptr);
      }

      // DQ (6/22/2006): Commented out temporarily for debugging use of glob.h
      // ROSE_ASSERT(locatedNode->get_parent() != nullptr);
      break;
    }

    case V_SgFunctionDeclaration:
    case V_SgMemberFunctionDeclaration: {
      SgNode *parent = locatedNode->get_parent();
      if (parent == nullptr) {
        SgFunctionDeclaration *functionDeclaration =
            isSgFunctionDeclaration(locatedNode);
        ROSE_ASSERT(functionDeclaration != nullptr);
        SgDeclarationStatement *definingDeclaration =
            functionDeclaration->get_definingDeclaration();
        SgDeclarationStatement *nondefiningDeclaration =
            functionDeclaration->get_firstNondefiningDeclaration();

        if (nondefiningDeclaration == nullptr) {
          SgNode *parentNode = functionDeclaration->get_parent();
          // DQ (9/13/2011): Reported as possible nullptr value in static
          // analysis of ROSE code.
          ROSE_ASSERT(parentNode != nullptr);

          printf("Error: nondefiningDeclaration == nullptr for "
                 "functionDeclaration = %p = %s \n",
                 functionDeclaration,
                 SageInterface::get_name(functionDeclaration).c_str());
          printf("   definingDeclaration = %p \n", definingDeclaration);
          printf("   functionDeclaration->get_parent() = %p = %s \n",
                 parentNode, parentNode->class_name().c_str());
        }
        ROSE_ASSERT(nondefiningDeclaration != nullptr);
#if PRINT_DEVELOPER_WARNINGS
        printf("Warning from ResetParentPointersInMemoryPool::visit(): parent "
               "== nullptr for function name = %s definingDeclaration = %p "
               "nondefiningDeclaration = %p parent = %p \n",
               functionDeclaration->get_name().str(), definingDeclaration,
               nondefiningDeclaration, nondefiningDeclaration->get_parent());
#endif
        // ROSE_ASSERT(nondefiningDeclaration->get_parent() != nullptr);
        if (definingDeclaration != nullptr) {
          // DQ (11/25/2020): Adding debugging support.
          if (definingDeclaration->get_parent() == nullptr) {
            printf("Error: definingDeclaration->get_parent() == nullptr: "
                   "definingDeclaration = %p = %s \n",
                   definingDeclaration,
                   definingDeclaration->class_name().c_str());
            printf(" --- definingDeclaration name = %s \n",
                   SageInterface::get_name(definingDeclaration).c_str());
          }
          ROSE_ASSERT(definingDeclaration->get_parent() != nullptr);

          // Make the parent the same for both the defining and nondefining
          // declarations
          if (nondefiningDeclaration->get_parent() == nullptr) {
            // This happens in the case where a member
            // function is used before it is declared (a
            // case where the parent was not set in the
            // legacy frontend/SageIII translation).
#if PRINT_DEVELOPER_WARNINGS
            printf(
                "Setting the nondefiningDeclaration->get_parent() == nullptr "
                "using definingDeclaration->get_parent() = %p \n",
                definingDeclaration->get_parent());
#endif
            ROSE_ASSERT(definingDeclaration->get_parent() != nullptr);
            nondefiningDeclaration->set_parent(
                definingDeclaration->get_parent());
          }
          ROSE_ASSERT(nondefiningDeclaration->get_parent() != nullptr);
        }
      }

      if (locatedNode->get_parent() == nullptr) {
        SageInterface::dumpInfo(
            locatedNode, "ResetParentPointersInMemoryPool::visit() error: "
                         "found a func dec without defining declaration and "
                         "its non-defining declaration has no scope info. ");
        ROSE_ASSERT(locatedNode->get_parent() != nullptr);
      }
      break;
    }
    case V_SgInitializedName:
    default: {
      break;
    }
    }
  }

  // Some SgSupport IR nodes have a valid parent
  if (support != nullptr) {
    // DQ (6/26/2006): Set the parent pointer to a type to collect them for
    // visualization.
    Sg_File_Info *fileInfo = isSg_File_Info(support);
    if (fileInfo != nullptr && fileInfo->get_parent() == nullptr) {
      // This is a detached Sg_File_Info object (else it would have been set
      // properly in the ResetFileInfoParentPointersInMemoryPool traversal).
      // by default we set it to the get_globalFunctionTypeTable() (somewhat
      // arbitrarily this helps me figure out what Sg_File_Info objects are
      // built redundantly).
      // support->set_parent(SgTypeShort::createType()get_globalFunctionTypeTable());

      // DQ (12/23/2006): Sg_File_Info objects are used in non-SgNode objects to
      // record source position information, when this is done we add extra
      // flags to the classification so that we can expect when the parent
      // pointer will be nullptr.  In these cases the parent pointer can't be
      // used to point to the object using the Sg_File_Info object since it is
      // not dirived from SgNode.  This is a diesn issue and may be addressed
      // differently in the future.  We want to skip setting the parent in this
      // case and avoid considering it to be an error.
      if (fileInfo->isCommentOrDirective() == false &&
          fileInfo->isToken() == false) {
#if PRINT_DEVELOPER_WARNINGS
        printf("ResetParentPointersInMemoryPool::visit(): Valid fileInfo = %p "
               "has parent == nullptr \n",
               fileInfo);
        // fileInfo->display("ResetParentPointersInMemoryPool::visit():
        // fileInfo->get_parent() == nullptr");
#endif
      }

      // printf ("Make this an error now to have a Sg_File_Info object with a
      // nullptr parent \n"); ROSE_ABORT();
    }
  }
}
