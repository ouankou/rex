// See header file for documentation.

#include "fixupTemplateArguments.h"
#include "sage3basic.h"

// We need this so that BACKEND_COMPILERS will be known.
#include <rose_config.h>

using namespace std;

#define DEBUG_PRIVATE_TYPE 0
#define DEBUGGING_USING_RECURSIVE_DEPTH 0

// DQ (2/11/2017): This are tailored to provide less output for debugging larger
// problems.
#define DEBUG_VISIT_PRIVATE_TYPE 0
#define DEBUG_PRIVATE_TYPE_TRANSFORMATION 0

// bool contains_private_type (SgType* type);
// bool contains_private_type (SgTemplateArgument* templateArgument);

#if DEBUGGING_USING_RECURSIVE_DEPTH
// For debugging, keep track of the recursive depth.
static size_t global_depth = 0;
#endif

bool FixupTemplateArguments::contains_private_type(
    SgType *type, SgScopeStatement *targetScope) {
  // DQ (4/2/2018): Note that this function now addresses requirements of
  // supporting both private and protected types.

#if DEBUGGING_USING_RECURSIVE_DEPTH
  // For debugging, keep track of the recursive depth.
  static size_t depth = 0;

  printf("In contains_private_type(SgType*): depth = %zu \n", depth);
  ROSE_ASSERT(depth < 500);

  printf("In contains_private_type(SgType*): global_depth = %zu \n",
         global_depth);
  ROSE_ASSERT(global_depth < 55);
#endif

  // Note this is the recursive function.
  bool returnValue = false;

#if DEBUG_PRIVATE_TYPE || 0
  // DQ (1/7/2016): It is a problem to do this for some files (failing about 35
  // files in Cxx_tests). The issues appears to be in the unparsing of the
  // template arguments of the qualified names for the types. printf ("In
  // contains_private_type(SgType*): type = %p = %s = %s
  // \n",type,type->class_name().c_str(),type->unparseToString().c_str());
  printf("In contains_private_type(SgType*): type = %p = %s \n", type,
         type->class_name().c_str());
#endif

  SgTypedefType *typedefType = isSgTypedefType(type);
  if (typedefType != NULL) {
    // Get the associated declaration.
    SgTypedefDeclaration *typedefDeclaration =
        isSgTypedefDeclaration(typedefType->get_declaration());
    ROSE_ASSERT(typedefDeclaration != NULL);

    // DQ (4/2/2018): Fix this to address requirements of both private and
    // protected class members (see Cxx11_tests/test2018_71.C).
    bool isPrivate = typedefDeclaration->get_declarationModifier()
                         .get_accessModifier()
                         .isPrivate() ||
                     typedefDeclaration->get_declarationModifier()
                         .get_accessModifier()
                         .isProtected();
#if DEBUG_PRIVATE_TYPE || 0
    printf("typedefDeclaration isPrivate = %s \n",
           isPrivate ? "true" : "false");
#endif

    // First we need to know if this is a visable type.
    bool isVisable = false;
    // Test for the trivial case of matching scope (an even better test (below)
    // is be to make sure that the targetScope is nested in the typedef scope).
    if (typedefDeclaration->get_scope() == targetScope) {
      // ROSE_ABORT();
      // return false;
      isVisable = true;
    } else {
      // SgTypedefSymbol*   lookupTypedefSymbolInParentScopes  (const SgName &
      // name, SgScopeStatement *currentScope = NULL);
      SgTypedefSymbol *typedefSymbol =
          SageInterface::lookupTypedefSymbolInParentScopes(
              typedefDeclaration->get_name(), targetScope);
      if (typedefSymbol != NULL) {
        // ROSE_ABORT();
        // return false;
        isVisable = true;
      } else {
        // ROSE_ABORT();
      }
    }
    // If this is not private, then we are looking at what would be possbile
    // template arguments used in a possible name qualification. if (isPrivate
    // == false) if (isPrivate == false && isVisable == false)
    if (isVisable == false) {
      if (isPrivate == true) {
        return true;
      } else {

        // Get the scope and see if it is a template instantiation.
        SgScopeStatement *scope = typedefDeclaration->get_scope();
#if DEBUG_PRIVATE_TYPE || 0
        printf("++++++++++++++ Looking in parent scope for template arguments: "
               "scope = %p = %s \n",
               scope, scope->class_name().c_str());
#endif
        // Get the associated declaration.
        switch (scope->variantT()) {
        case V_SgTemplateInstantiationDefn: {
          SgTemplateInstantiationDefn *templateInstantiationDefinition =
              isSgTemplateInstantiationDefn(scope);
          ROSE_ASSERT(templateInstantiationDefinition != NULL);

          SgTemplateInstantiationDecl *templateInstantiationDeclaration =
              isSgTemplateInstantiationDecl(
                  templateInstantiationDefinition->get_declaration());
          ROSE_ASSERT(templateInstantiationDeclaration != NULL);

          SgTemplateArgumentPtrList &templateArgumentPtrList =
              templateInstantiationDeclaration->get_templateArguments();
          for (SgTemplateArgumentPtrList::iterator i =
                   templateArgumentPtrList.begin();
               i != templateArgumentPtrList.end(); i++) {
#if DEBUG_PRIVATE_TYPE
            printf("recursive call to contains_private_type(%p): name = %s = "
                   "%s \n",
                   *i, (*i)->class_name().c_str(),
                   (*i)->unparseToString().c_str());
#endif
#if DEBUGGING_USING_RECURSIVE_DEPTH
            global_depth++;
#endif

            bool isPrivateType = contains_private_type(*i, targetScope);

#if DEBUGGING_USING_RECURSIVE_DEPTH
            global_depth--;
#endif
            returnValue |= isPrivateType;
          }
          break;
        }

        default: {
#if DEBUG_PRIVATE_TYPE
          printf("Ignoring non-SgTemplateInstantiationDefn \n");
#endif
        }
        }
      }
    } else {
      // If it is visible then it need not be qualified and we don't care about
      // if it was private.
      ROSE_ASSERT(isVisable == true);

      // returnValue = true;
      returnValue = false;
    }
  } else {
#if DEBUG_PRIVATE_TYPE || 0
    printf("could be a wrapped type: type = %p = %s (not a template class "
           "instantiaton) \n",
           type, type->class_name().c_str());
    if (isSgModifierType(type) != NULL) {
      SgModifierType *modifierType = isSgModifierType(type);
      SgType *base_type = modifierType->get_base_type();
      printf("--- base_type = %p = %s \n", base_type,
             base_type->class_name().c_str());
      SgNamedType *namedType = isSgNamedType(base_type);
      if (namedType != NULL) {
        printf("--- base_type: name = %s \n", namedType->get_name().str());
      }
    }
#endif
    // If this is a default SgModifierType then unwrap it.

    // DQ (4/15/2019): With the new support for
    // SgType::STRIP_POINTER_MEMBER_TYPE, we want to use it here. Strip past
    // pointers and other wrapping modifiers (but not the typedef types, since
    // the whole point is to detect private instatances). type =
    // type->stripType(SgType::STRIP_MODIFIER_TYPE|SgType::STRIP_REFERENCE_TYPE|SgType::STRIP_RVALUE_REFERENCE_TYPE|SgType::STRIP_POINTER_TYPE|SgType::STRIP_ARRAY_TYPE);
    type = type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_POINTER_MEMBER_TYPE | SgType::STRIP_ARRAY_TYPE);

    ROSE_ASSERT(type != NULL);

    // Make sure this is not a simple template type (else we will have infinite
    // recursion). if (type != NULL && type->isIntegerType() == false &&
    // type->isFloatType() == false) if (type != NULL)
    SgTemplateType *templateType = isSgTemplateType(type);
    SgClassType *classType = isSgClassType(type);
    SgTypeVoid *voidType = isSgTypeVoid(type);
    SgRvalueReferenceType *rvalueReferenceType = isSgRvalueReferenceType(type);
    SgFunctionType *functionType = isSgFunctionType(type);
    SgDeclType *declType = isSgDeclType(type);

    // DQ (12/7/2016): An enum type needs to be handled since the declaration
    // might be private (but still debugging this for now).
    SgEnumType *enumType = isSgEnumType(type);

    // DQ (2/12/2017): Added specific type (causing infinite recursion for
    // CompileTests/RoseExample_tests/testRoseHeaders_03.C.
    SgTypeEllipse *typeEllipse = isSgTypeEllipse(type);
    SgTypeUnknown *typeUnknown = isSgTypeUnknown(type);
    SgTypeComplex *typeComplex = isSgTypeComplex(type);

    // DQ (2/16/2017): This is a case causeing many C codes to fail.
    SgTypeOfType *typeOfType = isSgTypeOfType(type);

    // TV (04/23/2018): deprecated SgTemplateType. Now using the notion of
    // non-real declaration (and associated declaration, symbol, and reference
    // expression)
    SgNonrealType *typeNonreal = isSgNonrealType(type);
    SgAutoType *typeAuto = isSgAutoType(type);
    SgTypeNullptr *typeNullptr = isSgTypeNullptr(type);

    if (type != NULL && typeNonreal == NULL && classType == NULL &&
        voidType == NULL && rvalueReferenceType == NULL &&
        functionType == NULL && declType == NULL && enumType == NULL &&
        typeEllipse == NULL && typeUnknown == NULL && typeComplex == NULL &&
        typeOfType == NULL && typeAuto == NULL && templateType == NULL &&
        typeNullptr == NULL) {
#if DEBUG_PRIVATE_TYPE || 0
      printf("found unwrapped type = %p = %s = %s (not a template class "
             "instantiaton) \n",
             type, type->class_name().c_str(), type->unparseToString().c_str());
#endif
      // if (type->isIntegerType() == false && type->isFloatType() == false)
      // if (type->isIntegerType() == false && type->isFloatType() == false)
      if (type->isIntegerType() == false && type->isFloatType() == false) {
#if DEBUG_PRIVATE_TYPE || 0
        printf("Making a recursive call to contains_private_type(type): not "
               "integer or float type: type = %p = %s  \n",
               type, type->class_name().c_str());
#endif
#if DEBUGGING_USING_RECURSIVE_DEPTH
        depth++;
        global_depth++;
#endif
        bool isPrivateType = contains_private_type(type, targetScope);

#if DEBUGGING_USING_RECURSIVE_DEPTH
        depth--;
        global_depth--;
#endif
        returnValue = isPrivateType;
      } else {
        // This can't be a private type.
#if DEBUG_PRIVATE_TYPE
        printf("This is an integer or float type (of some sort): type = %p = "
               "%s = %s \n",
               type, type->class_name().c_str(),
               type->unparseToString().c_str());
#endif
        returnValue = false;
      }
    } else {
      // This is where we need to resolve is any types that are associated with
      // declarations might be private (e.g. SgEnumType).

      // DQ (3/1/2019): I think we need to resolve if an unnamed type is being
      // used here (since they can't be template arguments).

      if (classType != NULL) {
        // Check if this is associated with a template class instantiation.
#if DEBUG_PRIVATE_TYPE
        SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(classType->get_declaration());
        ROSE_ASSERT(classDeclaration != NULL);
        printf("--------- classDeclaration = %p = %s = %s \n", classDeclaration,
               classDeclaration->class_name().c_str(),
               classDeclaration->get_name().str());
#endif
        SgTemplateInstantiationDecl *templateInstantiationDeclaration =
            isSgTemplateInstantiationDecl(classType->get_declaration());
        if (templateInstantiationDeclaration != NULL) {
#if DEBUGGING_USING_RECURSIVE_DEPTH
          global_depth++;
#endif
          returnValue = contains_private_type(
              templateInstantiationDeclaration->get_templateArguments(),
              targetScope);

#if DEBUGGING_USING_RECURSIVE_DEPTH
          global_depth--;
#endif
        }
      }
    }
  }

#if DEBUG_PRIVATE_TYPE || 0
  printf("Leaving contains_private_type(SgType*): type = %p = %s = %s "
         "returnValue = %s \n",
         type, type->class_name().c_str(), type->unparseToString().c_str(),
         returnValue ? "true" : "false");
#endif

  return returnValue;
}

bool FixupTemplateArguments::contains_private_type(
    SgTemplateArgumentPtrList &templateArgList, SgScopeStatement *targetScope) {
  bool returnValue = false;

  // ROSE_ASSERT(templateArgList.empty() == false);

  static std::set<SgTemplateArgumentPtrList> templateArgumentListSet;

  // DQ (2/15/2017): Make a copy of the vector and use that as a basis for
  // identifing if the list has been previously processed.
  // SgTemplateArgumentPtrList templateArgList = templateArgList;
  // ROSE_ASSERT(templateArgList.empty() == false);

  if (templateArgumentListSet.find(templateArgList) ==
      templateArgumentListSet.end()) {
    templateArgumentListSet.insert(templateArgList);
  } else {
    // #if DEBUGGING_USING_RECURSIVE_DEPTH

    // DQ (2/15/2017): Unclear if this is the correct return value, it might be
    // that we want to record the associated value from the first time the
    // argument list was processed and use that value. Then again, if the value
    // had already been substituted into the template argument then no further
    // processing is required.
    return false;
  }

  SgTemplateArgumentPtrList::const_iterator i = templateArgList.begin();
  while (returnValue == false && i != templateArgList.end()) {
    returnValue |= contains_private_type(*i, targetScope);
    i++;
  }

#if DEBUG_PRIVATE_TYPE || 0
  printf("Leaving contains_private_type(SgTemplateArgumentPtrList): "
         "templateArgumentListSet.size() = %zu returnValue = %s \n",
         templateArgumentListSet.size(), returnValue ? "true" : "false");
#endif

  return returnValue;
}

bool FixupTemplateArguments::contains_private_type(
    SgTemplateArgument *templateArgument, SgScopeStatement *targetScope) {
  // Note that within legacy frontend and ROSE the template arguments may be
  // shared so that we can support testing for equivalence.

  // static std::list<SgTemplateArgument*> templateArgumentList;
  // templateArgumentList.push_back(templateArgument);
  static std::set<SgTemplateArgument *> templateArgumentSet;

  if (templateArgumentSet.find(templateArgument) == templateArgumentSet.end()) {
    templateArgumentSet.insert(templateArgument);
  } else {
#if DEBUGGING_USING_RECURSIVE_DEPTH
    printf("@@@@@@@@@@@@@@@@@ Already been or being processed: "
           "templateArgument = %p = %s templateArgumentSet.size() = %zu \n",
           templateArgument, templateArgument->unparseToString().c_str(),
           templateArgumentSet.size());
#endif

    // DQ (2/15/2017): Unclear if this is the correct return value, it might be
    // that we want to record the associated value from the first time the
    // argument list was processed and use that value. Then again, if the value
    // had already been substituted into the template argument then no further
    // processing is required.
    return false;
  }

#if DEBUGGING_USING_RECURSIVE_DEPTH
  printf("--- added templateArgument = %p templateArgumentSet.size() = %zu \n",
         templateArgument, templateArgumentSet.size());
#endif

#if DEBUGGING_USING_RECURSIVE_DEPTH
  // For debugging, keep track of the recursive depth.
  static size_t depth = 0;

  printf("In contains_private_type(SgTemplateArgument*): depth = %zu \n",
         depth);
  ROSE_ASSERT(depth < 500);

  printf("In contains_private_type(SgTemplateArgument*): global_depth = %zu \n",
         global_depth);
  if (global_depth >= 50) {
    // output the list of SgTemplateArgument in the list
    printf("Error: too many elements in list: recursuion too deep \n");
    size_t counter = 0;
    for (std::set<SgTemplateArgument *>::iterator i =
             templateArgumentSet.begin();
         i != templateArgumentSet.end(); i++) {
      printf("--- templateArgumentSet[counter] = %p = %s \n", *i,
             templateArgument->unparseToString().c_str());
      counter++;
    }
  }
  ROSE_ASSERT(global_depth < 50);
#endif

  // Note this is the recursive function.
  bool returnValue = false;

#if DEBUG_PRIVATE_TYPE
  printf("In contains_private_type(SgTemplateArgument*): templateArgument = %p "
         "= %s = %s \n",
         templateArgument, templateArgument->class_name().c_str(),
         templateArgument->unparseToString().c_str());
#endif

  switch (templateArgument->get_argumentType()) {
  case SgTemplateArgument::type_argument: {
    ROSE_ASSERT(templateArgument->get_type() != NULL);

    SgType *templateArgumentType = templateArgument->get_type();
#if DEBUG_PRIVATE_TYPE
    printf("templateArgumentType = %p = %s \n", templateArgumentType,
           templateArgumentType->class_name().c_str());
    if (isSgModifierType(templateArgumentType) != NULL) {
      SgModifierType *modifierType = isSgModifierType(templateArgumentType);
      SgType *base_type = modifierType->get_base_type();
      printf("--- base_type = %p = %s \n", base_type,
             base_type->class_name().c_str());
      SgNamedType *namedType = isSgNamedType(base_type);
      if (namedType != NULL) {
        printf("--- base_type: name = %s \n", namedType->get_name().str());
      }
    }
#endif
#if DEBUGGING_USING_RECURSIVE_DEPTH
    depth++;
    global_depth++;
#endif

    // DQ (2/14/2017): We might want to generate a list of the private types
    // used so that we can check them against the scope of the declaration where
    // they occur. Note also that this does not address types that might appear
    // in name qualification.
    returnValue = contains_private_type(templateArgumentType, targetScope);

    // DQ (3/2/2019): Detect any class declaration that is un-named since it
    // can't be a tempalte argument either.
    SgNamedType *namedType = isSgNamedType(templateArgumentType);
    if (namedType != NULL) {
      SgDeclarationStatement *declarationStatement =
          namedType->get_declaration();
      ROSE_ASSERT(declarationStatement != NULL);

      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declarationStatement);
      if (classDeclaration != NULL) {
#if DEBUG_PRIVATE_TYPE
        printf("classDeclaration->get_isUnNamed() = %s \n",
               classDeclaration->get_isUnNamed() ? "true" : "false");
#endif
        if (classDeclaration->get_isUnNamed() == true) {
          returnValue = true;
          SgClassDeclaration *definingClassDeclaration = isSgClassDeclaration(
              declarationStatement->get_definingDeclaration());
          if (definingClassDeclaration != NULL) {
            ROSE_ASSERT(definingClassDeclaration->get_isUnNamed() == true);
          }
        }
      }
    }

#if DEBUG_PRIVATE_TYPE
    // DQ (3/1/2019): I think we need to resolve if an unnamed type is being
    // used here (since they can't be template arguments).
    printf("In contains_private_type(SgTemplateArgument*): "
           "templateArgumentType = %p = %s \n",
           templateArgumentType, templateArgumentType->class_name().c_str());
    if (namedType != NULL) {
      printf("--- namedType: name = %s \n", namedType->get_name().str());
      SgDeclarationStatement *declarationStatement =
          namedType->get_declaration();
      ROSE_ASSERT(declarationStatement != NULL);

      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declarationStatement);
      if (classDeclaration != NULL) {
        printf("--- classDeclaration = %p = %s name = %s \n", classDeclaration,
               classDeclaration->class_name().c_str(),
               classDeclaration->get_name().str());
        SgClassDeclaration *definingClassDeclaration = isSgClassDeclaration(
            declarationStatement->get_definingDeclaration());
        if (definingClassDeclaration != NULL) {
          printf("--- definingClassDeclaration = %p = %s name = %s \n",
                 definingClassDeclaration,
                 definingClassDeclaration->class_name().c_str(),
                 definingClassDeclaration->get_name().str());
        }
      }
    }
#endif

#if DEBUGGING_USING_RECURSIVE_DEPTH
    depth--;
    global_depth--;
#endif

#if DEBUG_PRIVATE_TYPE || 0
    printf("In contains_private_type(SgTemplateArgument*): case "
           "SgTemplateArgument::type_argument: DONE calling "
           "contains_private_type(templateArgumentType): returnValue = %s \n",
           returnValue ? "true" : "false");
#endif
    if (returnValue == true) {
      // Find an alternative typedef to use instead.

      // Note that this need not be a SgTypedefType (the lists are available in
      // every SgType).
      SgTypedefType *typedefType = isSgTypedefType(templateArgumentType);

      if (typedefType == NULL &&
          isSgModifierType(templateArgumentType) != NULL) {
        SgModifierType *modifierType = isSgModifierType(templateArgumentType);
        SgType *base_type = modifierType->get_base_type();
#if DEBUG_PRIVATE_TYPE || 0
        printf("******* Reset the typedefType to what was found in the "
               "modifier type as a base type = %p = %s \n",
               base_type, base_type->class_name().c_str());
#endif
        typedefType = isSgTypedefType(base_type);
      }

      SgTypedefSeq *typedef_table = NULL;
      if (typedefType != NULL) {
        // Check if this is a type from a typedef that is in the same scope as
        // the target declaration (variable declaration).
        SgTypedefDeclaration *typedefDeclaration =
            isSgTypedefDeclaration(typedefType->get_declaration());
        ROSE_ASSERT(typedefDeclaration != NULL);

        // Test for the matching scope (an even better test would be to make
        // sure that the targetScope is nested in the typedef scope).
        SgScopeStatement *typedefDeclarationScope =
            typedefDeclaration->get_scope();
        ROSE_ASSERT(targetScope != NULL);
        ROSE_ASSERT(typedefDeclarationScope != NULL);
#if DEBUG_PRIVATE_TYPE || 0
        printf("Looking at typedef typedefType = %p = %s = %s \n", typedefType,
               typedefType->class_name().c_str(),
               typedefType->unparseToString().c_str());
#endif

        // Consult the list of alreanative typedefs.
        typedef_table = typedefType->get_typedefs();
      } else {
        // DQ (3/2/2019): We need to select an alternative type to support a
        // non-un-named and non-private type as a template argument.
        typedef_table = templateArgumentType->get_typedefs();
      }

      // Consult the list of alternative typedefs.
      // SgTypedefSeq* typedef_table = typedefType->get_typedefs();
      ROSE_ASSERT(typedef_table != NULL);

      SgTypePtrList &typedefList = typedef_table->get_typedefs();

      bool foundNonPrivateTypeAlias = false;
      SgType *suitableTypeAlias = NULL;

      SgTypePtrList::iterator i = typedefList.begin();
      while (foundNonPrivateTypeAlias == false && i != typedefList.end()) {
        ROSE_ASSERT(*i != NULL);
#if DEBUG_PRIVATE_TYPE || 0
        printf("Looking for suitable type alias (#%d): *i = %p = %s = %s \n",
               counter, *i, (*i)->class_name().c_str(),
               (*i)->unparseToString().c_str());
#endif
#if DEBUGGING_USING_RECURSIVE_DEPTH
        global_depth++;
#endif
        bool isPrivateType = contains_private_type(*i, targetScope);

#if DEBUGGING_USING_RECURSIVE_DEPTH
        global_depth--;
#endif
        if (isPrivateType == false) {
          suitableTypeAlias = *i;
          foundNonPrivateTypeAlias = true;
        }

        // foundNonPrivateTypeAlias = !isPrivateType;

        i++;
      }
      if (foundNonPrivateTypeAlias == true) {
        ROSE_ASSERT(suitableTypeAlias != NULL);

        // TV (10/05/2018): (ROSE-1431) Traverse the chain of
        // all associated template arguments (coming from the
        // same legacy frontend template argument)
        SgTemplateArgument *templateArgument_it = templateArgument;
        while (templateArgument_it->get_previous_instance() != NULL) {
          templateArgument_it = templateArgument_it->get_previous_instance();
        }
        ROSE_ASSERT(templateArgument_it != NULL &&
                    templateArgument_it->get_previous_instance() == NULL);
        do {
          templateArgument_it->set_unparsable_type_alias(suitableTypeAlias);

          // DQ (1/9/2017): Also set the return result from get_type() so that
          // the name qualification will be handled correctly.
          templateArgument_it->set_type(suitableTypeAlias);

          templateArgument_it = templateArgument_it->get_next_instance();
        } while (templateArgument_it != NULL);

        ROSE_ASSERT(templateArgument_it == NULL);

        // #if DEBUG_PRIVATE_TYPE_TRANSFORMATION
      }
    }

    break;
  }

  default: {
#if DEBUG_PRIVATE_TYPE
    printf("Ignoring non-type template arguments \n");
#endif
  }
  }

#if DEBUG_PRIVATE_TYPE
  printf("Leaving contains_private_type(SgTemplateArgument*): templateArgument "
         "= %p = %s = %s \n",
         templateArgument, templateArgument->class_name().c_str(),
         templateArgument->unparseToString().c_str());
#endif

  // templateArgumentList.pop_back();
  templateArgumentSet.erase(templateArgument);

#if DEBUGGING_USING_RECURSIVE_DEPTH
  printf("--- pop templateArgument = %p templateArgumentSet.size() = %zu \n",
         templateArgument, templateArgumentSet.size());
#endif

  return returnValue;
}

// void FixupTemplateArguments::visit ( SgNode* node )
void FixupTemplateArguments::processTemplateArgument(
    SgTemplateArgument *templateArgument, SgScopeStatement *targetScope) {
  // ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(templateArgument != NULL);

#if DEBUGGING_USING_RECURSIVE_DEPTH
  // For debugging, keep track of the recursive depth.
  printf("In FixupTemplateArguments::visit: global_depth = %zu \n",
         global_depth);
  ROSE_ASSERT(global_depth < 50);
#endif

  // DQ (2/11/2017): Change this traversal to not use memory pools.
  // SgTemplateArgument* templateArgument = isSgTemplateArgument(node);
  ROSE_ASSERT(templateArgument != NULL);

#if DEBUGGING_USING_RECURSIVE_DEPTH
  global_depth++;
#endif

  bool result = contains_private_type(templateArgument, targetScope);

#if DEBUGGING_USING_RECURSIVE_DEPTH
  global_depth--;
#endif

  if (result == true) {
    // This type will be a problem to unparse, because it contains parts that
    // are private (or protected).
#if DEBUG_VISIT_PRIVATE_TYPE
    printf("\n\nWARNING: This template parameter can NOT be unparsed (contains "
           "references to private types): templateArgument = %p = %s \n",
           templateArgument, templateArgument->unparseToString().c_str());
    SgNode *parent = templateArgument->get_parent();
    SgDeclarationStatement *parentDeclaration =
        isSgDeclarationStatement(parent);
    if (parentDeclaration != NULL) {
      if (parentDeclaration->get_file_info() != NULL) {
      }
    } else {
      if (parent->get_file_info() != NULL) {
      }
    }
#endif
  } else {
    // This type is fine to unparse
#if DEBUG_PRIVATE_TYPE
    printf("Template parameter CAN be unparsed (no private types) \n\n");
#endif
  }
}

void FixupTemplateArguments::visit(SgNode *node) {
  ROSE_ASSERT(node != NULL);

  SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(node);
  if (variableDeclaration != NULL) {
    // Check the type of the variable declaration, and any template arguments if
    // it is a template type with template arguments. SgType* type =
    // variableDeclaration->get_type(); ROSE_ASSERT(type != NULL);
    SgInitializedName *initializedName =
        SageInterface::getFirstInitializedName(variableDeclaration);
    ROSE_ASSERT(initializedName != NULL);
    SgType *type = initializedName->get_type();
    ROSE_ASSERT(type != NULL);
    SgScopeStatement *targetScope = variableDeclaration->get_scope();
    ROSE_ASSERT(targetScope != NULL);

    // DQ (2/16/2017): Don't process code in template instantiations.
    SgTemplateInstantiationDefn *templateInstantiationDefn =
        isSgTemplateInstantiationDefn(targetScope);
    SgFunctionDeclaration *functionDeclaration =
        SageInterface::getEnclosingFunctionDeclaration(targetScope, true);
    SgTemplateInstantiationFunctionDecl *templateInstantiationFunctionDec =
        isSgTemplateInstantiationFunctionDecl(functionDeclaration);
    SgTemplateInstantiationMemberFunctionDecl
        *templateInstantiationMemberFunctionDec =
            isSgTemplateInstantiationMemberFunctionDecl(functionDeclaration);
    // if (templateInstantiationDefn == NULL)
    if (templateInstantiationDefn == NULL &&
        templateInstantiationFunctionDec == NULL &&
        templateInstantiationMemberFunctionDec == NULL) {
      // DQ (2/15/2017): When this is run, we cause transformations that cause
      // ROSE to have an infinte loop. Since this is a second (redundant)
      // invocaion, we likely should just not run this.  But it is not clear if
      // this truely fixes the problem that I am seeing.
      bool result = contains_private_type(type, targetScope);

      // DQ (3/25/2017): Added a trivial use to eliminate Clang warning about
      // the return value not being used. But it might be that we should not run
      // the function, however this is a complex subject from last month that I
      // don't wish to revisit at the moment while being focused om eliminating
      // warnings from Clang.
      ROSE_ASSERT(result == true || result == false);
    }
  }
}

// void fixupTemplateArguments()
void fixupTemplateArguments(SgNode *node) {
  // DQ (7/7/2005): Introduce tracking of performance of ROSE.
  TimingPerformance fixupTemplateArguments_timer(
      "Add reference to non-private template arguments (for unparsing):");

  // DQ (2/10/2017): The default should be for this to be on, but it needs to be
  // fixed to allow some of the later test codes to also pass. DQ (2/9/2017):
  // This is causing type names to be too long (over 400K), so it causes
  // tests/CompileTests/RoseExample_tests/testRoseHeaders_03.C to fails (along
  // with several other of the ROSE specific test codes in that directory (tests
  // 3,4,5,and 6)).

  // DQ (1/15/2017): Since this is a fix for GNU 4.9 and greater backend
  // compilers, and Intel and Clang compilers, we only want to test fixing it
  // there initially. Later we can apply the fix more uniformally. DQ
  // (11/27/2016): We only want to support calling this fixup where I am testing
  // it with the GNU 6.x compilers. #if
  // (BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER >= 6)
#if defined(BACKEND_CXX_IS_GNU_COMPILER) &&                                    \
    ((BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER == 4 &&                        \
      BACKEND_CXX_COMPILER_MINOR_VERSION_NUMBER == 9) ||                       \
     BACKEND_CXX_COMPILER_MAJOR_VERSION_NUMBER >= 5)
  FixupTemplateArguments t;
  // SgTemplateArgument::traverseMemoryPoolNodes(t);
  // t.traverse(node,preorder);
  t.traverse(node, preorder);
#else
  // DQ (1/15/2017): And apply this fix for all versions of Intel and Clang
  // backend compilers as well.
#if defined(BACKEND_CXX_IS_INTEL_COMPILER) ||                                  \
    defined(BACKEND_CXX_IS_CLANG_COMPILER)
  FixupTemplateArguments t;
  // SgTemplateArgument::traverseMemoryPoolNodes(t);
  t.traverse(node, preorder);
#endif
#endif
}
