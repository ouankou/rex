#include "sage3basic.h"

using namespace std;
using namespace Rose;

string SageInterface::declarationPositionString(
    const SgDeclarationStatement *declaration) {
  ROSE_ASSERT(declaration != NULL);
  Sg_File_Info *fileInfo = declaration->get_file_info();
  ROSE_ASSERT(fileInfo != NULL);

  int file_id = fileInfo->get_file_id();
  int line_number = fileInfo->get_line();
  int column_number = fileInfo->get_col();
  ROSE_ASSERT(line_number >= 0);
  ROSE_ASSERT(column_number >= 0);

  // Keep generated identifiers valid when source file IDs are negative.
  string fileIdString;
  if (file_id < 0) {
    fileIdString =
        "N" + StringUtility::numberToString(-static_cast<long long>(file_id));
  } else {
    fileIdString = StringUtility::numberToString(file_id);
  }

  return "_F" + fileIdString + "_L" +
         StringUtility::numberToString(line_number) + "_C" +
         StringUtility::numberToString(column_number);
}

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
      const SgNamedType *namedType = isSgNamedType(node);
      ROSE_ASSERT(namedType != NULL);
      SgDeclarationStatement *declaration = namedType->get_declaration();
      ROSE_ASSERT(declaration != NULL);
      key = generateUniqueName(declaration, true);
      additionalSuffix = "__namedType";
      break;
    }
    case V_SgTemplateType: {
      const SgTemplateType *templateType = isSgTemplateType(node);
      ROSE_ASSERT(templateType != NULL);

      key = "__template_type_";
      additionalSuffix =
          additionalSuffix + StringUtility::numberToString(templateType);
      break;
    }

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

      additionalSuffix = "__type";

      if (key.empty() == true) {
        printf(
            "Generated empty string from type->get_mangled() type = %p = %s \n",
            type, type->class_name().c_str());
        ROSE_ABORT();
      }
      break;
    }
    }
  }

  //--------------------- SgStatement--------------------------------------
  const SgStatement *statement = isSgStatement(node);
  if (statement != NULL) {
    switch (statement->variantT()) {
    case V_SgFunctionTypeTable: {
      // An interning table has its own identity independent of lexical
      // files and scopes, including when it is a process root.
      key = "__function_type_table_" + StringUtility::numberToString(node);
      additionalSuffix = "__function_type_table";
      break;
    }

    case V_SgTemplateClassDefinition:
    case V_SgClassDefinition:
    case V_SgTemplateInstantiationDefn: {
      const SgClassDefinition *classDefinition = isSgClassDefinition(statement);
      ROSE_ASSERT(classDefinition != NULL);
      SgClassDeclaration *classDeclaration = classDefinition->get_declaration();
      ROSE_ASSERT(classDeclaration != NULL);
      key = classDeclaration->get_mangled_name();

      if (isSgTemplateInstantiationDefn(classDefinition) != NULL)
        additionalSuffix = "__class_template_instantiation_definition";
      else
        additionalSuffix = "__class_definition";

      if (key.empty() == true) {
        key = generateUniqueName(classDeclaration, true);
        ROSE_ASSERT(key.empty() == false);
      }

      break;
    }

    case V_SgTemplateFunctionDefinition:
    case V_SgFunctionDefinition: {
      const SgFunctionDefinition *functionDefinition =
          isSgFunctionDefinition(statement);
      ROSE_ASSERT(functionDefinition != NULL);
      SgFunctionDeclaration *functionDeclaration =
          functionDefinition->get_declaration();
      ROSE_ASSERT(functionDeclaration != NULL);
      key = functionDeclaration->get_mangled_name();
      additionalSuffix = "__function_definition";

      if (key.empty() == true) {
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

    case V_SgTemplateFunctionDeclaration:
    case V_SgTemplateMemberFunctionDeclaration:

    case V_SgFunctionDeclaration:
    case V_SgMemberFunctionDeclaration:
    case V_SgTemplateInstantiationFunctionDecl:
    case V_SgTemplateInstantiationMemberFunctionDecl: {
      const SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(statement);
      key = functionDeclaration->get_mangled_name();

      if (functionDeclaration->get_definition() != NULL) {
        // Keep a definition distinct from a forward declaration.
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
      additionalSuffix = "__function_parameter_list";

      ROSE_ASSERT(key.empty() == false);
      break;
    }

    case V_SgTemplateVariableDeclaration:

    case V_SgVariableDeclaration: {
      additionalSuffix = "__variable";

      const SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(statement);
      key = variableDeclaration->get_mangled_name();
      // Distinguish an extern declaration without linkage.
      if ((variableDeclaration->get_declarationModifier()
               .get_storageModifier()
               .isExtern() == true) &&
          (variableDeclaration->get_linkage().empty())) {
        additionalSuffix += "__extern_declaration ";
      }

      // Include the declared names to distinguish declarations in unnamed
      // scopes.
      string variableNames;
      SgInitializedNamePtrList::const_iterator p =
          variableDeclaration->get_variables().begin();
      while (p != variableDeclaration->get_variables().end()) {
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
      const SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(statement);
      key = enumDeclaration->get_mangled_name();

      if (enumDeclaration->get_isUnNamed()) {
        key +=
            "__uniqueValue_" + declarationPositionString(enumDeclaration) + "_";
      }

      if (ignoreDifferenceBetweenDefiningAndNondefiningDeclarations == true) {
        additionalSuffix = "__enum_defining_declaration";
      } else {
        if (enumDeclaration == enumDeclaration->get_definingDeclaration()) {
          additionalSuffix = "__enum_defining_declaration";
        } else {
          additionalSuffix = "__enum_nondefining_declaration";
        }
      }

      if (key.empty() == true) {
        fprintf(stderr,
                "REX_NAME_INVARIANT[enum-mangled-name]: declaration=%p "
                "name='%s' has no semantic mangled name\n",
                static_cast<const void *>(enumDeclaration),
                enumDeclaration->get_name().str());
        ROSE_ABORT();
      }
      break;
    }

    case V_SgTemplateDeclaration: {
      const SgTemplateDeclaration *declaration =
          isSgTemplateDeclaration(statement);

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
      }

      additionalSuffix = "__template_declaration";

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

    case V_SgTemplateClassDeclaration:

    case V_SgClassDeclaration:
    case V_SgTemplateInstantiationDecl: {
      const SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(statement);
      key = classDeclaration->get_mangled_name();

      if (ignoreDifferenceBetweenDefiningAndNondefiningDeclarations == true) {
        additionalSuffix = "__class_nondefining_declaration";
      } else {
        if (classDeclaration == classDeclaration->get_definingDeclaration()) {
          additionalSuffix = "__class_defining_declaration";
        } else {
          additionalSuffix = "__class_nondefining_declaration";
        }
      }

      const SgTemplateInstantiationDecl *templateInstantiationDeclaration =
          isSgTemplateInstantiationDecl(statement);
      if (templateInstantiationDeclaration != NULL) {
        additionalSuffix += "__template_instantiation";
      }
      if (classDeclaration->get_isUnNamed()) {
        key += "__uniqueValue_" + declarationPositionString(classDeclaration) +
               "_";
      }

      ROSE_ASSERT(key.empty() == false);

      // Distinguish classes with the same name and different members.
      string memberNames = "_class_members_";
      if (classDeclaration->get_definition() != NULL) {
        SgDeclarationStatementPtrList::const_iterator p =
            classDeclaration->get_definition()->get_members().begin();
        while (p != classDeclaration->get_definition()->get_members().end()) {
          memberNames += string("_member_name_") + SageInterface::get_name(*p);

          p++;
        }
      }

      additionalSuffix += memberNames;

      break;
    }

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

    case V_SgDefaultOptionStmt:

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

      if (isFriend)
        key += "__friend_";
      key += accessString;

      // Include the source location to distinguish declarations across files.
      const Sg_File_Info *fileInfo = declarationStatement->get_file_info();
      ROSE_ASSERT(fileInfo != NULL);
      key += fileInfo->get_filenameString() + "_" +
             StringUtility::numberToString(fileInfo->get_line());
    }
  }

  //--------------------- SgSymbol--------------------------------------
  const SgSymbol *symbol = isSgSymbol(node);
  if (symbol != NULL) {
    switch (symbol->variantT()) {
    case V_SgVariableSymbol: {
      const SgVariableSymbol *valiableSymbol = isSgVariableSymbol(symbol);
      SgInitializedName *initializedName = valiableSymbol->get_declaration();
      key = generateUniqueName(initializedName, false);
      additionalSuffix = "__variable_symbol";
      break;
    }

    case V_SgTemplateVariableSymbol: {
      const SgTemplateVariableSymbol *valiableSymbol =
          isSgTemplateVariableSymbol(symbol);
      SgInitializedName *initializedName = valiableSymbol->get_declaration();
      key = generateUniqueName(initializedName, false);
      additionalSuffix = "__template_variable_symbol";
      break;
    }

    case V_SgClassSymbol: {
      const SgClassSymbol *classSymbol = isSgClassSymbol(symbol);
      SgDeclarationStatement *declaration = classSymbol->get_declaration();
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
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__enum_symbol";
      break;
    }

    case V_SgNamespaceSymbol: {
      const SgNamespaceSymbol *namespaceSymbol = isSgNamespaceSymbol(symbol);
      ROSE_ASSERT(namespaceSymbol != NULL);
      const SgDeclarationStatement *declaration =
          isSgDeclarationStatement(namespaceSymbol->get_symbol_basis());
      ROSE_ASSERT(declaration != NULL);
      key = generateUniqueName(declaration, false);
      additionalSuffix = "__namespace_symbol";
      break;
    }

    case V_SgEnumFieldSymbol: {
      const SgEnumFieldSymbol *enumFieldSymbol = isSgEnumFieldSymbol(symbol);
      SgInitializedName *declaration = enumFieldSymbol->get_declaration();
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

    case V_SgLabelSymbol: {
      const SgLabelSymbol *labelSymbol = isSgLabelSymbol(symbol);
      key = labelSymbol->get_name();
      additionalSuffix = "__label_symbol";
      break;
    }

    case V_SgNamelistSymbol: {
      key = symbol->get_mangled_name().getString();
      additionalSuffix = "__namelist_symbol";
      break;
    }
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

    default: {
      printf(
          "Error: default reached in generateUniqueName() symbol = %p = %s \n",
          symbol, symbol->class_name().c_str());
      ROSE_ABORT();
    }
    }
  }

  //-------------------------------SgExpression-----------------------------------
  // Expressions have node identity, not a shared declaration identity.
  const SgExpression *expression = isSgExpression(node);
  if (expression != NULL) {
    key = "__expression_";
    key += StringUtility::numberToString(node);
  }

  //-------------------------------SgInitializedName-----------------------------------
  const SgInitializedName *initializedName = isSgInitializedName(node);
  if (initializedName != NULL) {
    key = initializedName->get_mangled_name();

    // Include the type to distinguish variables in unnamed scopes.
    string type = initializedName->get_type()->get_mangled();
    key = key + type;

    additionalSuffix = "__initialized_name";

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

    // Parameters of defining and nondefining functions have distinct owners.
    SgFunctionParameterList *functionParameterList =
        isSgFunctionParameterList(node->get_parent());
    if (functionParameterList != NULL) {
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

    // A class static variable can have declarations in two scopes.
    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(node->get_parent());
    if (variableDeclaration != NULL) {
      key +=
          "_in_scope_" + variableDeclaration->get_scope()->get_mangled_name();
    }
  }

  //-------------------------------SgSupport-----------------------------------
  const SgSupport *support = isSgSupport(node);
  if (support != NULL) {
    switch (support->variantT()) {
    case V_SgProject: {
      key = "__project";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgSourceFile: {
      key = "__sourceFile_file_id_";

      const SgFile *file = isSgFile(node);
      int fileId = file->get_file_info()->get_file_id();
      key = key + StringUtility::numberToString(fileId);
      break;
    }

    case V_SgTypeTable: {
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

    case V_SgModifierNodes: {
      key = "__modifier_nodes_" + StringUtility::numberToString(node);
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
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgFunctionParameterTypeList: {
      key = "__function_parameter_type_list_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgFunctionTypeArgument: {
      key = "__function_type_argument_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgTemplateArgument: {
      const SgTemplateArgument *templateArgument = isSgTemplateArgument(node);
      key = "__template_argument_";
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
        SgExpression *subExpression = templateArgument->get_expression();
        ROSE_ASSERT(subExpression != nullptr);
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
        break;
      }

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

    case V_SgBaseClassModifier: {
      key = "__base_class_modifier_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgBaseClass: {
      const SgBaseClass *baseClass = isSgBaseClass(node);
      key = "__base_class_";
      ROSE_ASSERT(baseClass->get_base_class() != NULL);
      key = key + generateUniqueName(baseClass->get_base_class(), false);
      break;
    }

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
      key = key + pragma->get_name();
      break;
    }

    case V_SgFileList: {
      key = "__file_list_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgDirectoryList: {
      key = "__directory_list_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgUnparse_Info: {
      key = "__unparse_info_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgTemplateParameter: {
      key = "__template_parameter_";
      key = key + StringUtility::numberToString(node);
      break;
    }

    case V_SgIncludeFile: {
      key = "__include_file_";
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

  if (key.empty() == false) {
    key += additionalSuffix;
  }

  return key;
}
