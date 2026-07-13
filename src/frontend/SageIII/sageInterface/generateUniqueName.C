#include "sage3basic.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

// This function is local to this file
string SageInterface::declarationPositionString(
    const SgDeclarationStatement *declaration) {
  // This function generates a unique string for a declaration
  // and is used in the generateUniqueName() function.

  ROSE_ASSERT(declaration != NULL);
  Sg_File_Info *fileInfo = declaration->get_file_info();
  ROSE_ASSERT(fileInfo != NULL);

  int file_id = fileInfo->get_file_id();
  int line_number = fileInfo->get_line();
  int column_number = fileInfo->get_col();

  // Liao, 11/9/2009
  // for translation generated SgNode, the file_id might be negative, like -3
  // But naively inserting "-3" into the return string will break the rule for
  // identifier in C/C++. In this case, we convert it to N3
  string fileIdString;
  if (file_id < 0) {
    fileIdString += "N";
    file_id = -1 * file_id;
  }
  fileIdString += StringUtility::numberToString(file_id);

  string returnString = "_F" + fileIdString + "_L" +
                        StringUtility::numberToString(line_number) + "_C" +
                        StringUtility::numberToString(column_number);
  // string returnString = "_F" + StringUtility::numberToString(file_id) + "_L"
  // + StringUtility::numberToString(line_number) + "_C" +
  // StringUtility::numberToString(column_number);

  // Liao, 11/9/2009
  // the returned string should not have '-', which will break the rule for
  // identifier in C/C++
  size_t pos1 = returnString.find("-");
  ROSE_ASSERT(pos1 == string::npos);

  return returnString;
}

// DQ (8/10/2010): Change to take node parameter as const.
string SageInterface::generateUniqueName(
    const SgNode *node,
    bool ignoreDifferenceBetweenDefiningAndNondefiningDeclarations) {
  // This function handles details in the differences between
  // the unique names that we require for declarations and where the
  // mangled name mechanism would map them to be the same.
  // Examples include:
  //    1) Function declarations (e.g. forward declarations and the defining
  //    declaration)
  //       with and without default parameters specified all have the same
  //       mangled name.
  //    2) Forward function declarations and the defining function declaration
  //    all have
  //       the same mangled name.
  //    3) Namespace declarations all mangle to the same namespace name but
  //    there can be
  //       many of them (since they are re-entrant) so these need a more precise
  //       definition of uniqueness.

  ROSE_ASSERT(node != NULL);

  string key;
  string additionalSuffix;

  //--------------------- SgType--------------------------------------
  const SgType *type = isSgType(node);
  if (type != NULL) {
    switch (type->variantT()) {
    case V_SgClassType:
    case V_SgEnumType:
    case V_SgTypedefType:
    case V_SgNamedType: {
      // Handle case of named types which should be shared within the
      // merged AST. These are multiply represented within the
      // generated AST from legacy frontend. Note that a fixup pass
      // on the AST (fixupTypes.[hC]) forces the same declaration
      // (defining or nondefining) to be used for all SgNamedType
      // objects referencing the same declaration.
      const SgNamedType *namedType = isSgNamedType(node);
      ROSE_ASSERT(namedType != NULL);
      SgDeclarationStatement *declaration = namedType->get_declaration();
      ROSE_ASSERT(declaration != NULL);
      key = generateUniqueName(declaration, true);
      additionalSuffix = "__namedType";
      break;
    }
      // DQ (11/27/2012): I think we need to make this unique so that it will
      // not be shared for now!
    case V_SgTemplateType: {
      const SgTemplateType *templateType = isSgTemplateType(node);
      ROSE_ASSERT(templateType != NULL);

      key = "__template_type_";
      additionalSuffix =
          additionalSuffix + StringUtility::numberToString(templateType);
      break;
    }

      // DQ (12/5/2012): This is a fix for mergeTest_06.C where it appears to
      // only fail for the GNU 4.4 compiler.  Likely specific to header compiler
      // specific header files used for only that (and likely later) version of
      // the compiler.
    case V_SgModifierType: {
      const SgModifierType *modifierType = isSgModifierType(node);
      ROSE_ASSERT(modifierType != NULL);

      key = "__modifier_type_";
      additionalSuffix =
          additionalSuffix + StringUtility::numberToString(modifierType);
      break;
    }

      // All other types
    default: {
      // Note difference in function name get_mangled_name() and get_mangled()
      key = type->get_mangled();

      // DQ (7/4/2010): Use the original setting which allowed types to be
      // shared. DQ (6/28/2010): I think we need to make this unique so that it
      // will not be shared for now! additionalSuffix = "__type";
      // additionalSuffix = "__type"+ StringUtility::numberToString(type);
      additionalSuffix = "__type";

      // printf ("default case of SgType: key = %s \n",key.c_str());

      if (key.empty() == true) {
        printf(
            "Generated empty string from type->get_mangled() type = %p = %s \n",
            type, type->class_name().c_str());
        ROSE_ABORT();
      }
      break;
    }
    }

    // DQ (6/25/2010): This should be less important now that we normalize the
    // SgTypedefSeq IR nodes to have the same lists. printf ("In
    // SageInterface::generateUniqueName(): after adding typedefs for type = %s
    // \n",key.c_str());
  }

  //--------------------- SgStatement--------------------------------------
  const SgStatement *statement = isSgStatement(node);
  if (statement != NULL) {
    // SgScopeStatement*       scopeStatement = isSgScopeStatement(node);
    // SgDeclarationStatement* declaration    = isSgDeclarationStatement(node);
    switch (statement->variantT()) {
    case V_SgFunctionTypeTable: {
      const SgFunctionTypeTable *symbolTable = isSgFunctionTypeTable(node);
      ROSE_ASSERT(symbolTable->get_parent() != NULL);
      key = generateUniqueName(symbolTable->get_parent(), false);
      additionalSuffix = "__function_type_table";
      break;
    }

      // DQ (11/27/2012): Added support for new template IR nodes.
    case V_SgTemplateClassDefinition:

    case V_SgClassDefinition:
    case V_SgTemplateInstantiationDefn: {
      const SgClassDefinition *classDefinition = isSgClassDefinition(statement);
      ROSE_ASSERT(classDefinition != NULL);
      // SgClassDeclaration* classDeclaration =
      // isSgClassDeclaration(classDefinition->get_parent());
      SgClassDeclaration *classDeclaration = classDefinition->get_declaration();
      ROSE_ASSERT(classDeclaration != NULL);
      key = classDeclaration->get_mangled_name();

      // DQ (6/22/2006): Added support for SgTemplateInstantiationDefn
      if (isSgTemplateInstantiationDefn(classDefinition) != NULL)
        additionalSuffix = "__class_template_instantiation_definition";
      else
        additionalSuffix = "__class_definition";

      // printf ("In case V_SgClassDefinition: key = %s \n",key.c_str());
      if (key.empty() == true) {
        // printf ("In case V_SgClassDefinition: empty key calling
        // generateUniqueName on declaration \n");
        key = generateUniqueName(classDeclaration, true);
        ROSE_ASSERT(key.empty() == false);
      }

      // printf ("case V_SgClassDefinition: scopeStatement = %p = %s
      // classDeclaration = %p = %s
      // \n",scopeStatement,scopeStatement->class_name().c_str(),classDeclaration,classDeclaration->class_name().c_str());
      // printf ("case V_SgClassDefinition: key = %s additionalSuffix = %s
      // \n",key.c_str(),additionalSuffix.c_str()); ROSE_ABORT();

      break;
    }

      // DQ (11/27/2012): Added support for new template IR nodes.
    case V_SgTemplateFunctionDefinition:

    case V_SgFunctionDefinition: {
      const SgFunctionDefinition *functionDefinition =
          isSgFunctionDefinition(statement);
      ROSE_ASSERT(functionDefinition != NULL);
      // SgFunctionDeclaration* functionDeclaration =
      // isSgFunctionDeclaration(functionDefinition->get_parent());
      SgFunctionDeclaration *functionDeclaration =
          functionDefinition->get_declaration();
      ROSE_ASSERT(functionDeclaration != NULL);
      key = functionDeclaration->get_mangled_name();
      additionalSuffix = "__function_definition";

      // printf ("In case V_SgFunctionDefinition: key = %s \n",key.c_str());
      if (key.empty() == true) {
        // printf ("In case V_SgFunctionDefinition: empty key calling
        // generateUniqueName on declaration \n");
        key = generateUniqueName(functionDeclaration, true);
        ROSE_ASSERT(key.empty() == false);
      }
      break;
    }

      // Generate a name for SgDeclarationScope
    case V_SgDeclarationScope: {
      key = "__declaration_scope_";

      // Make the key unique for each declaration scope!
      const SgDeclarationScope *declarationScope =
          isSgDeclarationScope(statement);
      key = key + StringUtility::numberToString(declarationScope);
      break;
    }

      // Generate a name for SgFunctionParameterScope
    case V_SgFunctionParameterScope: {
      key = "__function_parameter_scope_";

      // Keep function parameter scopes distinct even for synthesized
      // nondefining declarations.
      const SgFunctionParameterScope *parameterScope =
          isSgFunctionParameterScope(statement);
      key = key + StringUtility::numberToString(parameterScope);
      break;
    }

      // Declarations

      // Generate a name for SgGlobal
    case V_SgGlobal: {
      key = "__global_file_id_";

      // Make the key unique for each file!
      const SgGlobal *globalScope = isSgGlobal(statement);
      int fileId = globalScope->get_file_info()->get_file_id();
      key = key + StringUtility::numberToString(fileId);
      break;
    }

      // For now I would like to avoid sharing ASM statements so we make
      // them unique by using their pointer value in the generateed name.
    case V_SgAsmStmt: {
      key = "__asm_statement_";

      // Make the key unique for each asm statement declaration!
      key = key + StringUtility::numberToString(statement);
      break;
    }

      // DQ (11/27/2012): Added support for new template IR nodes.
    case V_SgTemplateFunctionDeclaration:
    case V_SgTemplateMemberFunctionDeclaration:

    case V_SgFunctionDeclaration:
    case V_SgMemberFunctionDeclaration:
    case V_SgTemplateInstantiationFunctionDecl:
    case V_SgTemplateInstantiationMemberFunctionDecl: {
      const SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(statement);
      key = functionDeclaration->get_mangled_name();

      // if ( functionDeclaration ==
      // functionDeclaration->get_definingDeclaration() )
      if (functionDeclaration->get_definition() != NULL) {
        // To avoid "int foo();" having the smae name as "int foo() {}" we need
        // to fixup the unique names. This may be the technique used to handle
        // function prototypes with defalt valued parameters and namespaces as
        // well.
        additionalSuffix = "__function_declaration_with_definition";
      } else {
        // Check for default parameters (function protypes with default
        // parameter should not be mixed with function prototypes that don't
        // have default arguments).
        bool defaultArgumentsSpecified = false;
        SgInitializedNamePtrList::const_iterator i =
            functionDeclaration->get_args().begin();
        while (i != functionDeclaration->get_args().end()) {
          SgInitializedName *functionParameter = *i;
          if (functionParameter->get_initptr() != NULL) {
            // The presence of an initializer implies a default argument has
            // been specified.
            defaultArgumentsSpecified = true;
          }

          i++;
        }

        // Make the unique string different if the function contains default
        // arguments
        if (defaultArgumentsSpecified == true) {
          additionalSuffix = "__default_function_parameters_specified";
        }
      }

      break;
    }

    case V_SgFunctionParameterList: {
      const SgFunctionParameterList *functionParameterList =
          isSgFunctionParameterList(statement);
      ROSE_ASSERT(functionParameterList != NULL);

      key = functionParameterList->get_mangled_name();
      // printf ("In case V_SgFunctionParameterList: key (mangled name) = %s
      // \n",key.c_str());

      additionalSuffix = "__function_parameter_list";

      // ROSE_ABORT();
      ROSE_ASSERT(key.empty() == false);
      break;
    }

      // DQ (11/28/2012): Added support for new template variable declaration IR
      // node.
    case V_SgTemplateVariableDeclaration:

    case V_SgVariableDeclaration: {
      additionalSuffix = "__variable";

      const SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(statement);
      key = variableDeclaration->get_mangled_name();
      // printf ("In case V_SgVariableDeclaration: key (mangled name) = %s
      // \n",key.c_str());

      // DQ (5/10/2007): Fixed linkage to be a std::string instead of char*
      // DQ (6/20/2006): Fixup to avoid "int x;" from being confused with
      // "extern int x;" if (
      // (variableDeclaration->get_declarationModifier().get_storageModifier().isExtern()
      // == true) && (variableDeclaration->get_linkage() == NULL) )
      if ((variableDeclaration->get_declarationModifier()
               .get_storageModifier()
               .isExtern() == true) &&
          (variableDeclaration->get_linkage().empty())) {
        additionalSuffix += "__extern_declaration ";
      }

      // DQ (6/1/2006): Mangled names are unique to a scope if the scope is
      // named

      // DQ (6/1/2006): Add the variables to the declaration (including types)
      // to avoid variable declarations from different unnamed scopes from
      // clashing.
      string variableNames;
      SgInitializedNamePtrList::const_iterator p =
          variableDeclaration->get_variables().begin();
      while (p != variableDeclaration->get_variables().end()) {
        // variableNames += "_variable_name_" + (*p)->get_mangled_name() +
        // (*p)->get_type()->get_mangled();
        variableNames += "_variable_name_" + (*p)->get_mangled_name();
        if ((*p)->get_initptr() != NULL) {
          // Initializer presence is semantic declaration state.  Never unparse
          // the initializer or use an address to manufacture identity here.
          variableNames += "__has_initializer";
        }
        p++;
      }

      key = key + variableNames;
      break;
    }

    case V_SgVariableDefinition: {
      const SgVariableDefinition *variableDefinition =
          isSgVariableDefinition(statement);
      key = variableDefinition->get_mangled_name();
      additionalSuffix = "__variable_definition";
      break;
    }

    case V_SgTypedefDeclaration: {
      const SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(statement);
      key = typedefDeclaration->get_mangled_name();
      additionalSuffix = "__typedef_declaration";
      break;
    }

    case V_SgTemplateTypedefDeclaration: {
      const SgTemplateTypedefDeclaration *decl =
          isSgTemplateTypedefDeclaration(statement);
      key = decl->get_mangled_name();
      additionalSuffix = "__template_typedef_declaration";
      break;
    }

    case V_SgTemplateInstantiationTypedefDeclaration: {
      const SgTemplateInstantiationTypedefDeclaration *decl =
          isSgTemplateInstantiationTypedefDeclaration(statement);
      key = decl->get_mangled_name();
      additionalSuffix = "__template_typedef_instantiation";
      break;
    }

    case V_SgEnumDeclaration: {
      // additionalSuffix = "__enum_declaration";

      const SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(statement);
      key = enumDeclaration->get_mangled_name();

      // DQ (2/21/2007): Added to support AST merge.
      if (enumDeclaration->get_name().getString().find_first_of(
              "__generatedName_") != string::npos) {
        key +=
            "__uniqueValue_" + declarationPositionString(enumDeclaration) + "_";

        // printf ("Found an un-named class = %p = %s key = %s
        // \n",enumDeclaration,enumDeclaration->get_name().str(),key.c_str());
        // ROSE_ABORT();
      }

      // printf ("case V_SgEnumDeclaration: key = %s \n",key.c_str());

      if (ignoreDifferenceBetweenDefiningAndNondefiningDeclarations == true) {
        // Not sure which one to use (but enums at least always have a defining
        // declaration)
        additionalSuffix = "__enum_defining_declaration";
      } else {
        if (enumDeclaration == enumDeclaration->get_definingDeclaration()) {
          additionalSuffix = "__enum_defining_declaration";
        } else {
          additionalSuffix = "__enum_nondefining_declaration";
        }
      }

      // DQ (7/10/2010): Found a case where this fails, need more information
      // about it. DQ (2/20/2007): I think that since we have reset the names of
      // un-named enum declarations this should be possible to assert now!
      // ROSE_ASSERT (key.empty() == false);

      if (key.empty() == true) {
        fprintf(stderr,
                "REX_NAME_INVARIANT[enum-mangled-name]: declaration=%p "
                "name='%s' has no semantic mangled name\n",
                static_cast<const void *>(enumDeclaration),
                enumDeclaration->get_name().str());
        ROSE_ABORT();
      }
      // DQ (7/23/2010): I think that we can assert this here!
      ROSE_ASSERT(key.empty() == false);
      break;
    }

      // DQ (3/28/2012): We have a new design for the legacy
      // frontend 4.x support and the IR design no longer derives
      // a SgTemplateClassDeclaration from a
      // SgTemplateDeclaration, so this code does not work. DQ
      // (6/11/2011): Added support for new template IR nodes.
      // case V_SgTemplateClassDeclaration:
    case V_SgTemplateDeclaration: {
      const SgTemplateDeclaration *declaration =
          isSgTemplateDeclaration(statement);

      // DQ (3/28/2012): Added assertion test.
      ROSE_ASSERT(declaration != NULL);

      key = declaration->get_mangled_name();
      if (key.empty()) {
        fprintf(stderr,
                "REX_NAME_INVARIANT[template-mangled-name]: declaration=%p "
                "name='%s' kind=%d has no overload-complete mangled name\n",
                static_cast<const void *>(declaration),
                declaration->get_name().str(),
                declaration->get_template_kind());
        ROSE_ABORT();
      } else {
        // Not clear if there might be other nested declarations in a
        // SgTemplateDeclaration that require similar handling! printf ("Must we
        // handle other sorts of SgTemplateDeclaration
        // declaration->get_template_kind() = %d
        // \n",declaration->get_template_kind());
      }

      additionalSuffix = "__template_declaration";

      // DQ (2/11/2007): distinquish between defining and nondefining
      // declarations.
      if (ignoreDifferenceBetweenDefiningAndNondefiningDeclarations == true) {
        // Make all the declarations the same as the defining declaration
        additionalSuffix += "__defining_declaration";
      } else {
        if (declaration == declaration->get_definingDeclaration()) {
          additionalSuffix += "__defining_declaration";
        } else {
          additionalSuffix += "__nondefining_declaration";
        }
      }

      SgTemplateDeclaration::template_type_enum template_kind =
          declaration->get_template_kind();
      switch (template_kind) {
      case SgTemplateDeclaration::e_template_none:
        additionalSuffix += "_none";
        break;
      case SgTemplateDeclaration::e_template_class:
        additionalSuffix += "_class";
        break;
      case SgTemplateDeclaration::e_template_m_class:
        additionalSuffix += "_member_class";
        break;
      case SgTemplateDeclaration::e_template_function:
        additionalSuffix += "_function";
        break;
      case SgTemplateDeclaration::e_template_m_function:
        additionalSuffix += "_member_function";
        break;
      case SgTemplateDeclaration::e_template_m_data:
        additionalSuffix += "_member_data";
        break;
      case SgTemplateDeclaration::e_template_variable:
        additionalSuffix += "_variable";
        break;

      default: {
        printf("Error: default reached \n");
        ROSE_ABORT();
      }
      }

      if (declaration->get_scope() == NULL ||
          declaration->get_name().is_null()) {
        fprintf(stderr,
                "REX_AST_INVARIANT[template-unique-name]: declaration=%p "
                "type=%s has no exact semantic scope or name\n",
                static_cast<const void *>(declaration),
                declaration->class_name().c_str());
        ROSE_ABORT();
      }

      additionalSuffix +=
          "_typed_template_identity_" +
          mangleTemplateToString(declaration->get_name().getString(),
                                 declaration->get_templateParameters(),
                                 declaration->get_scope());
      if (const SgExpression *requiresClause =
              declaration->get_requiresClause()) {
        additionalSuffix += "_requires_" + mangleExpression(requiresClause);
      }
      break;
    }

    case V_SgUsingDeclarationStatement: {
      const SgUsingDeclarationStatement *declaration =
          isSgUsingDeclarationStatement(statement);
      key = declaration->get_mangled_name();
      additionalSuffix = "__using_declaration";
      break;
    }

    case V_SgUsingDirectiveStatement: {
      const SgUsingDirectiveStatement *declaration =
          isSgUsingDirectiveStatement(statement);
      key = declaration->get_mangled_name();
      additionalSuffix = "__using_directive";
      break;
    }

      // DQ (11/28/2012): Added support for new template IR nodes.
    case V_SgTemplateClassDeclaration:

    case V_SgClassDeclaration:
    case V_SgTemplateInstantiationDecl: {
      const SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(statement);
      key = classDeclaration->get_mangled_name();

      // if (classDeclaration->get_definition() != NULL)
      // printf ("### classDeclaration = %p = %s = %s
      // \n",classDeclaration,classDeclaration->class_name().c_str(),classDeclaration->get_name().str());
      if (ignoreDifferenceBetweenDefiningAndNondefiningDeclarations == true) {
        // Not sure which one to use (but classes at least always have a
        // nondefining declaration)
        additionalSuffix = "__class_nondefining_declaration";
      } else {
        if (classDeclaration == classDeclaration->get_definingDeclaration()) {
          additionalSuffix = "__class_defining_declaration";
        } else {
          additionalSuffix = "__class_nondefining_declaration";
        }
      }

      // DQ (2/9/2007): Need to make sure the SgTemplateInstantiationDecl is
      // different from SgClassDeclaration to avoid sharing within AST merge.
      const SgTemplateInstantiationDecl *templateInstantiationDeclaration =
          isSgTemplateInstantiationDecl(statement);
      if (templateInstantiationDeclaration != NULL) {
        additionalSuffix += "__template_instantiation";
      }
      // DQ (2/21/2007): If we use the symbol tabel to generate names then as we
      // manipulate the symbol table the names will change which is a disaster
      // for the AST merge algorithm!  So use this technique to generate the
      // name that is stored in the declaration directly rather than generated
      // dynamically.

      // This code allows us to add a position counter to the un-named class
      // which prevents it from being merged with something else in the same
      // scoep which has a different symbol! if
      // (classDeclaration->get_name().is_null() == true)
      if (classDeclaration->get_name().getString().find_first_of(
              "__generatedName_") != string::npos) {
        // DQ (2/21/2007): Note that the classDeclaration may not match on in
        // the scope (e.g. "typedef struct { int x; } Y;" where the "struct {
        // int x; }" generates a SgClassSymbol in the symbol table but the
        // declaration is not explicit in the scope where the
        // SgTypedefDeclaration is declared.  So search the symbol table instead
        // of the scope. unsigned int uniqueScopeValue =
        // classDeclaration->get_scope()->generateUniqueStatementNumberForScope(classDeclaration);
        // key += "__uniqueScopeValue_" +
        // StringUtility::numberToString(uniqueScopeValue) + "_"; unsigned int
        // uniqueSymbolValue =
        // classDeclaration->get_scope()->get_symbol_table()->generateUniqueNumberForMatchingSymbol(classDeclaration);
        // key += "__uniqueSymbolValue_" +
        // StringUtility::numberToString(uniqueSymbolValue) + "_";
        key += "__uniqueValue_" + declarationPositionString(classDeclaration) +
               "_";

        // printf ("Found an un-named class = %p = %s key = %s
        // \n",classDeclaration,classDeclaration->get_name().str(),key.c_str());
        // ROSE_ABORT();
      } else {
        // Classes with normal names are not a problem.
      }

      // DQ (2/20/2007): I think that since we have reset the names of un-named
      // class declarations this should be possible to assert now!
      if (key.empty() == true) {
        // ROSE_ASSERT(classDeclaration->get_firstNondefiningDeclaration() !=
        // NULL); key = "__generatedName_" +
        // StringUtility::numberToString(classDeclaration->get_firstNondefiningDeclaration());
        // key = "__generatedName_" +
        // StringUtility::numberToString(classDeclaration);
        key = "__generatedName_";
      }
      ROSE_ASSERT(key.empty() == false);

      // DQ (6/23/2010): Added support to make classes with the same name
      // different when they are different classes containing different members.
      string memberNames = "_class_members_";
      if (classDeclaration->get_definition() != NULL) {
        SgDeclarationStatementPtrList::const_iterator p =
            classDeclaration->get_definition()->get_members().begin();
        while (p != classDeclaration->get_definition()->get_members().end()) {
          // DQ (2/22/2007): Added type to generated mangled name for each
          // variable (supports AST merge generation of name for un-named
          // classes) memberNames += SgName("_member_type_") +
          // (*p)->get_type()->get_mangled() + SgName("_member_name_") +
          // (*p)->get_mangled_name();
          memberNames += string("_member_name_") + SageInterface::get_name(*p);

          p++;
        }
      }

      additionalSuffix += memberNames;

      // need to check for use of extern in declaration!
      // additionalSuffix = "__class_declaration";
      break;
    }

      // There might be a longer list of special cases than this!
    case V_SgNamespaceAliasDeclarationStatement:
    case V_SgNamespaceDeclarationStatement: {
      const SgDeclarationStatement *declaration =
          isSgDeclarationStatement(statement);
      key = declaration->get_mangled_name();
      break;
    }

    case V_SgNamespaceDefinitionStatement: {
      const SgNamespaceDefinitionStatement *namespaceDefinition =
          isSgNamespaceDefinitionStatement(statement);
      key = namespaceDefinition->get_namespaceDeclaration()->get_mangled_name();
      break;
    }

      // DQ (2/10/2007): Added support for unique names from template
      // instatiation directives
    case V_SgTemplateInstantiationDirectiveStatement: {
      const SgTemplateInstantiationDirectiveStatement
          *templateInstantiationDirective =
              isSgTemplateInstantiationDirectiveStatement(statement);
      ROSE_ASSERT(templateInstantiationDirective->get_declaration() != NULL);
      key =
          templateInstantiationDirective->get_declaration()->get_mangled_name();
      additionalSuffix = "__template_instantiation_directive_declaration";
      break;
    }

    case V_SgNonrealDecl: {
      const SgNonrealDecl *nrdecl = isSgNonrealDecl(statement);
      key = nrdecl->get_mangled_name();
      break;
    }

      // DQ (5/6/2007): Added more cases
    case V_SgDefaultOptionStmt:

      // DQ (2/20/2007): Added more cases
    case V_SgSwitchStatement:
    case V_SgDoWhileStmt:
    case V_SgCaseOptionStmt:

    case V_SgCtorInitializerList:
    case V_SgTryStmt:
    case V_SgExprStatement:
    case V_SgCatchStatementSeq:
    case V_SgCatchOptionStmt:
    case V_SgNullStatement:
    case V_SgReturnStmt:
    case V_SgBreakStmt:
    case V_SgContinueStmt:
    case V_SgBasicBlock:
    case V_SgForStatement:
    case V_SgRangeBasedForStatement:
    case V_SgForInitStatement:
    case V_SgIfStmt:
    case V_SgWhileStmt:
    case V_SgLabelStatement: // RV 05/11/2007
    case V_SgGotoStatement: {
      // We don't want to generate a unique string for these statements
      break;
    }

    case V_SgPragmaDeclaration: {
      const SgPragmaDeclaration *pragmaDeclaration =
          isSgPragmaDeclaration(node);
      key = "__pragma_declaration_";
      ROSE_ASSERT(pragmaDeclaration->get_pragma() != NULL);
      key =
          key + generateUniqueName(
                    pragmaDeclaration->get_pragma(),
                    ignoreDifferenceBetweenDefiningAndNondefiningDeclarations);
      break;
    }

    case V_SgStaticAssertionDeclaration: {
      key = "__static_assert_declaration_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    default: {
      // ignore these cases
      printf("Default reached in case of SgStatement in "
             "generateUniqueName(node = %p = %s) \n",
             node, node->class_name().c_str());
      ROSE_ABORT();
    }
    }

    const SgDeclarationStatement *declarationStatement =
        isSgDeclarationStatement(statement);
    if (declarationStatement != NULL) {
      // Build a prefix to contain the access permissions (and friend specifier)
      bool isFriend =
          declarationStatement->get_declarationModifier().isFriend();
      SgAccessModifier::access_modifier_enum accessPermisions =
          declarationStatement->get_declarationModifier()
              .get_accessModifier()
              .get_modifier();
      string accessString;
      switch (accessPermisions) {
      case SgAccessModifier::e_unknown: {
        accessString = "__unknown_access_";
        break;
      }

      case SgAccessModifier::e_private: {
        accessString = "__private_access_";
        break;
      }

      case SgAccessModifier::e_protected: {
        accessString = "__protected_access_";
        break;
      }

      case SgAccessModifier::e_public: {
        accessString = "__public_access_";
        break;
      }

        // DQ (8/17/2020): Uncommented this code, now that e_default !=
        // e_public. This case is equal to SgAccessModifier::e_public (so it is
        // redundant to list it here)
      case SgAccessModifier::e_default: {
        accessString = "__default_access_";
        break;
      }

      case SgAccessModifier::e_not_applicable: {
        accessString = "__not_applicable_access_";
        break;
      }

      case SgAccessModifier::e_undefined: {
        accessString = "__undefined_access_";
        break;
      }

      case SgAccessModifier::e_last_modifier: {
        fprintf(stderr,
                "REX_AST_INVARIANT[unique-name-access]: declaration=%p has "
                "the invalid last-modifier sentinel\n",
                static_cast<const void *>(declarationStatement));
        ROSE_ABORT();
      }

      default: {
        printf("Error: default reached in SageInterface::generateUniqueName "
               "(declaration prefix) \n");
        ROSE_ABORT();
      }
      }

      if (isFriend == true) {
        string friendString = (isFriend == true) ? "__friend_" : "";

        key += friendString;

        // DQ (2/6/2007): Processing stream header file appears to demonstrate
        // that we do also need the accessString
        key += accessString;
      } else {
        key += accessString;
      }

      // DQ (7/16/2010): To simplify debugging add the line number to the
      // generated string. DQ (7/4/2010): Tests across separate files that are
      // actually different programs are a problem if we don't also include the
      // filename.  The same "class X" might not be able to be merged across
      // different programs. We would not have to include this if we were
      // merging files within a single program.
      string filename;
      string linenumber;
      if (declarationStatement->get_file_info() != NULL) {
        // ROSE_ASSERT(declarationStatement->get_file_info() != NULL);
        filename = declarationStatement->get_file_info()->get_filename();
        linenumber = StringUtility::numberToString(
            declarationStatement->get_file_info()->get_line());
      } else {
        filename = "unknown_file_and_pointer";
        linenumber = StringUtility::numberToString(declarationStatement);
      }
      // key += filename;
      key += filename + "_" + linenumber;
    }
  }

  //--------------------- SgSymbol--------------------------------------
  const SgSymbol *symbol = isSgSymbol(node);
  if (symbol != NULL) {
    switch (symbol->variantT()) {
    case V_SgVariableSymbol: {
      const SgVariableSymbol *valiableSymbol = isSgVariableSymbol(symbol);
      SgInitializedName *initializedName = valiableSymbol->get_declaration();
      // key = initializedName->get_mangled_name();
      key = generateUniqueName(initializedName, false);
      additionalSuffix = "__variable_symbol";
      break;
    }

    case V_SgTemplateVariableSymbol: {
      const SgTemplateVariableSymbol *valiableSymbol =
          isSgTemplateVariableSymbol(symbol);
      SgInitializedName *initializedName = valiableSymbol->get_declaration();
      // key = initializedName->get_mangled_name();
      key = generateUniqueName(initializedName, false);
      additionalSuffix = "__template_variable_symbol";
      break;
    }

    case V_SgClassSymbol: {
      const SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
      SgDeclarationStatement *declaration = classSymbol->get_declaration();
      // key = declaration->get_mangled_name();
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__class_symbol";
      break;
    }

    case V_SgTypedefSymbol: {
      const SgTypedefSymbol *typedefSymbol = isSgTypedefSymbol(symbol);
      SgDeclarationStatement *declaration = typedefSymbol->get_declaration();
      ROSE_ASSERT(declaration != NULL);
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__typedef_symbol";
      break;
    }

    case V_SgFunctionSymbol: {
      const SgFunctionSymbol *functionSymbol = isSgFunctionSymbol(symbol);
      SgDeclarationStatement *declaration = functionSymbol->get_declaration();
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__function_symbol";
      break;
    }

    case V_SgMemberFunctionSymbol: {
      const SgMemberFunctionSymbol *memberFunctionSymbol =
          isSgMemberFunctionSymbol(symbol);
      SgDeclarationStatement *declaration =
          memberFunctionSymbol->get_declaration();
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__member_function_symbol";
      break;
    }

    case V_SgEnumSymbol: {
      const SgEnumSymbol *enumSymbol = isSgEnumSymbol(symbol);
      SgDeclarationStatement *declaration = enumSymbol->get_declaration();
      // key = declaration->get_mangled_name();
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__enum_symbol";
      break;
    }

    case V_SgNamespaceSymbol: {
      const SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);
      ROSE_ASSERT(namespaceSymbol != NULL);
      SgDeclarationStatement *declaration = namespaceSymbol->get_declaration();
      // DQ (7/4/1020): Unclear why we can have a declaration == NULL, so allow
      // this to generate a unique name for now! Perhaps this is for a "using
      // namespace std" without "std defined" (which is allows in C++). Or it
      // coudle the for an un-named namespace (also allowed in C++).
      if (declaration == NULL) {
        key = "__namespace_with_null_declaration_" +
              StringUtility::numberToString(symbol);
      } else {
        key = generateUniqueName(declaration, false);
      }
      additionalSuffix = "__namespace_symbol";
      break;
    }

    case V_SgEnumFieldSymbol: {
      const SgEnumFieldSymbol *enumFieldSymbol = isSgEnumFieldSymbol(symbol);
      SgInitializedName *declaration = enumFieldSymbol->get_declaration();
      // key = declaration->get_mangled_name();
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__enum_field_symbol";
      break;
    }

    case V_SgTemplateSymbol: {
      const SgTemplateSymbol *templateSymbol = isSgTemplateSymbol(symbol);
      SgTemplateDeclaration *declaration = templateSymbol->get_declaration();
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__template_symbol";
      break;
    }

    case V_SgFunctionTypeSymbol: {
      const SgFunctionTypeSymbol *functionTypeSymbol =
          isSgFunctionTypeSymbol(symbol);
      key = functionTypeSymbol->get_name();
      additionalSuffix = "__function_type_symbol";
      break;
    }

      // RV: 05/11/2007
    case V_SgLabelSymbol: {
      const SgLabelSymbol *labelSymbol = isSgLabelSymbol(symbol);
      key = labelSymbol->get_name();
      additionalSuffix = "__label_symbol";
      break;
    }

      // DQ (7/4/2010): It might be that these should not be shared!
    case V_SgAliasSymbol: {
      const SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);
      key = aliasSymbol->get_name();
      additionalSuffix = "__alias_symbol";
      break;
    }

    case V_SgTemplateFunctionSymbol: {
      const SgTemplateFunctionSymbol *templateFunctionSymbol =
          isSgTemplateFunctionSymbol(symbol);
      key = templateFunctionSymbol->get_name();
      additionalSuffix = "__template_function_symbol";
      break;
    }

    case V_SgTemplateMemberFunctionSymbol: {
      const SgTemplateMemberFunctionSymbol *templateMemberFunctionSymbol =
          isSgTemplateMemberFunctionSymbol(symbol);
      key = templateMemberFunctionSymbol->get_name();
      additionalSuffix = "__template_member_function_symbol";
      break;
    }

    case V_SgTemplateClassSymbol: {
      const SgTemplateClassSymbol *templateClassSymbol =
          isSgTemplateClassSymbol(symbol);
      key = templateClassSymbol->get_name();
      additionalSuffix = "__template_class_symbol";
      break;
    }

    case V_SgNonrealSymbol: {
      const SgNonrealSymbol *nrSymbol = isSgNonrealSymbol(symbol);
      key = nrSymbol->get_name();
      additionalSuffix = "__nonreal_symbol";
      break;
    }

      // All other SgSymbols
    default: {
      printf(
          "Error: default reached in generateUniqueName() symbol = %p = %s \n",
          symbol, symbol->class_name().c_str());
      ROSE_ABORT();
    }
    }
  }

  //-------------------------------SgExpression-----------------------------------
  // Never share expressions within the merge process
  const SgExpression *expression = isSgExpression(node);
  if (expression != NULL) {
    // printf ("expression = %p = %s
    // \n",expression,expression->class_name().c_str());

    switch (expression->variantT()) {
      // All other SgSymbols
    default: {
      key = "__expression_";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }
    }
  }

  //-------------------------------SgInitializedName-----------------------------------
  // Liao 11/4/2010, moved from SgSupport to SgLocatedNodeSupport
  const SgInitializedName *i_name = isSgInitializedName(node);
  if (i_name) {
    const SgInitializedName *initializedName = isSgInitializedName(node);
    // Make the mangled name from a SgInitializedName unique (not finished yet).
    // This case will handle "extern A::x" vs. "namespace A { int x; }" which
    // I expect will gnerate the same unique name but which are clearly
    // different!

    // Check for use of extern keyword (I think this is enough!)
    key = initializedName->get_mangled_name();

    // DQ (6/1/2006): Add the type to avoid variable declarations from different
    // unnamed scopes from clashing.
    string type = initializedName->get_type()->get_mangled();
    key = key + type;

    additionalSuffix = "__initialized_name";

    // DQ (8/10/2010): Change to represent as const variable declaration.
    // DQ (6/20/2006): Fixup to avoid "int x;" from being confused with "extern
    // int x;"
    const SgStorageModifier &storage = initializedName->get_storageModifier();
    if (storage.isExtern()) {
      additionalSuffix += "__extern_initialized_name";
    }
    if (initializedName->get_parent() == NULL) {
      fprintf(stderr,
              "REX_NAME_INVARIANT[initialized-name-owner]: name=%p '%s' is "
              "detached\n",
              static_cast<const void *>(initializedName),
              initializedName->get_name().str());
      ROSE_ABORT();
    }

    // DQ (3/3/2007): If this is part of a function parameter list then we want
    // to record if it is a defining or non-defining function (symbols for
    // parameters of non-defining functions are not placed into the symbol table
    // and parameters of defining functions are placed into the defining
    // function's function scope's symbol table. The unique name we genetate can
    // effect the symbols so we want to avoid having symbols moved
    // inappropriately due to over sharing from parameter names that are not
    // sufficiently unique!
    SgFunctionParameterList *functionParameterList =
        isSgFunctionParameterList(node->get_parent());
    if (functionParameterList != NULL) {
      // This is a function parameter
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(functionParameterList->get_parent());
      ROSE_ASSERT(functionDeclaration != NULL);
      if (functionDeclaration ==
          functionDeclaration->get_definingDeclaration()) {
        key = key + "_parameter_of_defining_declaration";
      } else {
        key = key + "_parameter_of_nondefining_declaration";
      }
    }

    // DQ (3/7/2007): This could be a static variable in a class which will make
    // two appearences in the source code. The first will be in the class as a
    // declaration and the second will be outside the class (as a declaration
    // from the point of view of ROSE, but as a means of allocating space from
    // teh pointof view of C++).
    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(node->get_parent());
    if (variableDeclaration != NULL) {
      // To make this unique, append the mangled name of the scope of the
      // variableDeclaration
      key +=
          "_in_scope_" + variableDeclaration->get_scope()->get_mangled_name();
    }
  }

  //-------------------------------SgSupport-----------------------------------
  const SgSupport *support = isSgSupport(node);
  if (support != NULL) {
    // printf ("support = %p = %s \n",support,support->class_name().c_str());

    switch (support->variantT()) {
    case V_SgProject: {
      key = "__project";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // case V_SgFile:
    case V_SgSourceFile: {
      key = "__sourceFile_file_id_";

      // Make the key unique for each file!
      const SgFile *file = isSgFile(node);
      int fileId = file->get_file_info()->get_file_id();
      key = key + StringUtility::numberToString(fileId);
      break;
    }

      // DQ (7/24/2010): Added to support local and global type tables.
    case V_SgTypeTable: {
      // const SgTypeTable* symbolTable = isSgTypeTable(node);
      // ROSE_ASSERT(symbolTable->get_parent() != NULL);
      // key = generateUniqueName(symbolTable->get_parent(),false);
      key = "__type_table_" + StringUtility::numberToString(node);
      additionalSuffix = "__type_table";
      break;
    }

    case V_SgSymbolTable: {
      const SgSymbolTable *symbolTable = isSgSymbolTable(node);
      if (symbolTable->get_parent() == NULL) {
        fprintf(stderr,
                "REX_NAME_INVARIANT[symbol-table-owner]: table=%p is "
                "detached\n",
                static_cast<const void *>(symbolTable));
        ROSE_ABORT();
      }
      key = generateUniqueName(symbolTable->get_parent(), false);
      additionalSuffix = "__symbol_table";
      break;
    }

    case V_SgStorageModifier: {
      const SgStorageModifier *storageModifier = isSgStorageModifier(node);
      ROSE_ASSERT(storageModifier->get_parent() != NULL);
      key = generateUniqueName(storageModifier->get_parent(), false);
      additionalSuffix = "__storage_modifier";
      break;
    }

    case V_Sg_File_Info: {
      key = "__file_info_";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgFunctionParameterTypeList: {
      key = "__function_parameter_type_list_";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgFunctionTypeArgument: {
      key = "__function_type_argument_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgTypedefSeq: {
      key = "__typedef_sequence_";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgTemplateArgument: {
      const SgTemplateArgument *templateArgument = isSgTemplateArgument(node);
      key = "__template_argument_";
      // Make the key unique for each file info object!
      // key = key + StringUtility::numberToString(node);
      switch (templateArgument->get_argumentType()) {
      case SgTemplateArgument::argument_undefined: {
        printf("Error: SgTemplateArgument::argument_undefined reached in "
               "switch \n");
        ROSE_ABORT();
      }
      case SgTemplateArgument::type_argument: {
        key += templateArgument->get_type()->get_mangled().str();
        break;
      }
      case SgTemplateArgument::nontype_argument: {
        // printf ("Error: SgTemplateArgument::nontype_argument reached (not
        // implemented yet, currently generating pointer value into return
        // string) \n"); printf ("     templateArgument->get_expression() = %p =
        // %s \n",
        //      templateArgument->get_expression(),templateArgument->get_expression()->class_name().c_str());
        // printf ("     templateArgument->get_parent() = %p
        // \n",templateArgument->get_expression()->get_parent());

        // DQ (3/7/2007): Use the value in the generated name so that it can be
        // shared.
        SgExpression *subExpression = templateArgument->get_expression();

        // DQ (8/22/3013): A nontype template argument can contain a
        // SgInitializedName as a declaration of a variable instead of an
        // expression. ROSE_ASSERT(subExpression != NULL);
        if (subExpression != NULL) {
          SgValueExp *valueExpression = isSgValueExp(subExpression);
          if (valueExpression != NULL) {
            key += "__" + valueExpression->class_name() + "__" +
                   valueExpression->get_constant_folded_value_as_string();
          } else {
            fprintf(stderr,
                    "REX_NAME_INVARIANT[nontype-template-argument]: "
                    "argument=%p expression=%p type=%s lacks typed structural "
                    "identity\n",
                    static_cast<const void *>(templateArgument),
                    static_cast<void *>(subExpression),
                    subExpression->class_name().c_str());
            ROSE_ABORT();
          }
        } else {
          // DQ (8/22/2013): Added branch to support non-expression kinds.
          // SageInterface::generateUniqueName ( const SgNode* node, bool
          // ignoreDifferenceBetweenDefiningAndNondefiningDeclarations )
          ROSE_ASSERT(templateArgument->get_initializedName() != NULL);
          key += "__" +
                 SageInterface::generateUniqueName(
                     templateArgument->get_initializedName(),
                     ignoreDifferenceBetweenDefiningAndNondefiningDeclarations);
        }
        break;
      }

        // DQ (7/11/2010): In AST file I/O tests we demonstrate an example of
        // this case.
      case SgTemplateArgument::template_template_argument:
      case SgTemplateArgument::start_of_pack_expansion_argument: {
        fprintf(stderr,
                "REX_NAME_INVARIANT[template-argument-kind]: argument=%p "
                "kind=%d has no typed structural identity\n",
                static_cast<const void *>(templateArgument),
                templateArgument->get_argumentType());
        ROSE_ABORT();
      }

      default: {
        printf("Error: default reached \n");
        ROSE_ABORT();
      }
      }
      break;
    }

    case V_SgQualifiedName: {
      key = "__qualified_name_";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // DQ (1/19/2007): previously unhandled case ...
    case V_SgBaseClassModifier: {
      key = "__base_class_modifier_";
      // Make the key unique for each file info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // DQ (1/19/2007): previously unhandled case ...
    case V_SgBaseClass: {
      const SgBaseClass *baseClass = isSgBaseClass(node);
      key = "__base_class_";
      ROSE_ASSERT(baseClass->get_base_class() != NULL);
      key = key + generateUniqueName(baseClass->get_base_class(), false);
      break;
    }

      // TV (09/12/2018)
    case V_SgNonrealBaseClass: {
      const SgNonrealBaseClass *baseClass = isSgNonrealBaseClass(node);
      key = "__nonreal_base_class_";
      ROSE_ASSERT(baseClass->get_base_class_nonreal() != NULL);
      key =
          key + generateUniqueName(baseClass->get_base_class_nonreal(), false);
      break;
    }

    case V_SgPragma: {
      const SgPragma *pragma = isSgPragma(node);
      key = "__pragma_";
      // Make the key unique for each file info object!
      // key = key + StringUtility::numberToString(node);
      key = key + pragma->get_name();
      break;
    }

      // DQ (1/23/2010): previously unhandled case ...
    case V_SgFileList: {
      key = "__file_list_";
      // Make the key unique for each SgFileList object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // DQ (1/23/2010): previously unhandled case ...
    case V_SgDirectoryList: {
      key = "__directory_list_";
      // Make the key unique for each SgDirectoryList object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // DQ (6/23/2011): previously unhandled case ...
    case V_SgUnparse_Info: {
      key = "__unparse_info_";
      // Make the key unique for each SgUnparse_Info object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // DQ (11/27/2012): previously unhandled case ...this implementation makes
      // each IR node unique (we might not want that later).
    case V_SgTemplateParameter: {
      key = "__template_parameter_";
      // Make the key unique for each SgTemplateParameter object!
      key = key + StringUtility::numberToString(node);
      break;
    }

      // DQ (10/31/2018): previously unhandled case ...this implementation makes
      // each IR node unique (we might not want that later).
    case V_SgIncludeFile: {
      key = "__include_file_";
      // Make the key unique for each SgTemplateParameter object!
      key = key + StringUtility::numberToString(node);
      break;
    }

    default: {
      printf("Error: default reached in generateUniqueName() node = %p = %s \n",
             node, node->class_name().c_str());
      ROSE_ABORT();
    }
    }
  }

  // Add the suffix that makes this IR nodes key different as required
  if (key.empty() == false) {
    key += additionalSuffix;
  }

  // I would like to avoid putting empty strings into the mangled name map!
  // ROSE_ASSERT(key.empty() == false);
  if (key.empty() == true) {
  }

  // ROSE_ASSERT(key.empty() == false);

  return key;
}
