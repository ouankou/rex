/* unparse_type_fort.C
 *
 * Code to unparse Sage/Fortran type nodes.
 *
 */
#include "sage3basic.h"

#include "unparser.h"

#include <set>

namespace {
bool exactGeneratedFortranTypeUse(SgUnparse_Info &info) {
  const SgInitializedName *name =
      isSgInitializedName(info.get_reference_node_for_qualification());
  const SgVariableDeclaration *declaration =
      name != nullptr ? isSgVariableDeclaration(name->get_parent()) : nullptr;
  auto is_exact_transformation_position = [](const Sg_File_Info *position,
                                             const SgNode *owner) {
    return position != nullptr && position->get_parent() == owner &&
           position->isTransformation() && !position->isCompilerGenerated() &&
           !position->isFrontendSpecific() &&
           !position->isSourcePositionUnavailableInFrontend() &&
           position->isOutputInCodeGeneration();
  };
  return name != nullptr && declaration != nullptr &&
         name->get_type() != nullptr &&
         name->get_fortran_source_type() == name->get_type() &&
         declaration->get_fortran_declaration_origin() ==
             SgVariableDeclaration::e_fortran_source_declaration &&
         is_exact_transformation_position(declaration->get_file_info(),
                                          declaration) &&
         is_exact_transformation_position(declaration->get_startOfConstruct(),
                                          declaration) &&
         is_exact_transformation_position(declaration->get_endOfConstruct(),
                                          declaration) &&
         is_exact_transformation_position(name->get_file_info(), name) &&
         is_exact_transformation_position(name->get_startOfConstruct(), name) &&
         is_exact_transformation_position(name->get_endOfConstruct(), name);
}

bool exactFortranAssumedTypeUse(SgUnparse_Info &info,
                                bool unlimitedPolymorphic) {
  SgNode *reference = info.get_reference_node_for_qualification();
  if (const SgInitializedName *name = isSgInitializedName(reference)) {
    return name->get_fortran_type_spec() ==
           (unlimitedPolymorphic
                ? SgInitializedName::e_fortran_type_spec_class_star
                : SgInitializedName::e_fortran_type_spec_type_star);
  }
  if (const SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(reference)) {
    return procedure->get_fortran_result_type_spec() ==
           (unlimitedPolymorphic ? SgProcedureHeaderStatement::
                                       e_fortran_result_type_spec_class_star
                                 : SgProcedureHeaderStatement::
                                       e_fortran_result_type_spec_type_star);
  }
  return false;
}

void requireFortranIntrinsicSourceType(SgType *type, SgUnparse_Info &info) {
  ASSERT_not_null(type);
  if (!type->get_fortran_source_syntax() &&
      !exactGeneratedFortranTypeUse(info)) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-intrinsic-source-type]: "
                 "semantic intrinsic type reached declaration emission\n";
    ROSE_ABORT();
  }
  if (const SgInitializedName *name =
          isSgInitializedName(info.get_reference_node_for_qualification())) {
    if (name->get_fortran_type_spec() !=
        SgInitializedName::e_fortran_type_spec_default) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-intrinsic-type-spec]: "
                   "intrinsic declaration has a non-intrinsic source "
                   "type-spec identity\n";
      ROSE_ABORT();
    }
  }
  if (const SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(
              info.get_reference_node_for_qualification())) {
    if (procedure->get_fortran_result_type_spec() !=
        SgProcedureHeaderStatement::e_fortran_result_type_spec_intrinsic) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[fortran-procedure-result-type-spec]: "
             "intrinsic procedure result has a non-intrinsic exact source "
             "type-spec identity\n";
      ROSE_ABORT();
    }
  }
}

SgSymbol *fortranSourceDerivedTypeSymbol(SgUnparse_Info &info) {
  SgNode *reference = info.get_reference_node_for_qualification();
  if (const SgInitializedName *name = isSgInitializedName(reference)) {
    return name->get_fortran_source_derived_type_symbol();
  }
  if (const SgProcedureHeaderStatement *procedure =
          isSgProcedureHeaderStatement(reference)) {
    return procedure->get_fortran_source_derived_type_symbol();
  }
  if (const SgAggregateInitializer *constructor =
          isSgAggregateInitializer(reference)) {
    return constructor->get_fortran_source_derived_type_symbol();
  }
  return nullptr;
}

SgName requireFortranSourceDerivedTypeName(SgClassType *classType,
                                           SgUnparse_Info &info) {
  ASSERT_not_null(classType);
  SgSymbol *sourceSymbol = fortranSourceDerivedTypeSymbol(info);
  if (sourceSymbol == nullptr && exactGeneratedFortranTypeUse(info)) {
    SgClassDeclaration *declaration =
        isSgClassDeclaration(classType->get_declaration());
    if (declaration == nullptr || declaration->get_name().is_null()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-derived-type-binding]: "
                   "generated source type has no exact canonical name\n";
      ROSE_ABORT();
    }
    return declaration->get_name();
  }
  if (sourceSymbol == nullptr || sourceSymbol->get_name().getString().empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-derived-type-binding]: "
                 "source derived type use has no exact visible symbol\n";
    ROSE_ABORT();
  }

  std::set<SgAliasSymbol *> aliases;
  SgSymbol *canonical = sourceSymbol;
  while (SgAliasSymbol *alias = isSgAliasSymbol(canonical)) {
    if (!aliases.insert(alias).second || alias->get_alias() == nullptr) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-derived-type-binding]: "
                   "source derived type binding contains a malformed alias "
                   "chain\n";
      ROSE_ABORT();
    }
    canonical = alias->get_alias();
  }
  SgClassSymbol *classSymbol = isSgClassSymbol(canonical);
  SgClassDeclaration *declaration =
      classSymbol != nullptr ? classSymbol->get_declaration() : nullptr;
  if (declaration == nullptr || declaration->get_type() != classType) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-derived-type-binding]: "
                 "source-visible symbol does not identify the exact semantic "
                 "class type\n";
    ROSE_ABORT();
  }
  return sourceSymbol->get_name();
}
} // namespace

//----------------------------------------------------------------------------
//  void UnparserFort::unparseType
//
//  General unparse function for types. Routes work to the appropriate
//  helper function.
//
//  NOTE: Unparses the type as a *declaration*. In Fortran, types only
//  appear in the declaration sections (not in casts, etc.)
//----------------------------------------------------------------------------

void UnparseFortran_type::unparseType(SgType *type, SgUnparse_Info &info,
                                      bool printAttrs) {
  ASSERT_not_null(type);

  switch (type->variantT()) {
  case V_SgTypeUnknown: {
    printf("Error: SgTypeUnknown should not be found in AST \n");
    ROSE_ABORT();
  }

  case V_SgTypeDefault: {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-default-type]: unresolved "
                 "SgTypeDefault reached Fortran declaration emission\n";
    ROSE_ABORT();
  }

  case V_SgTypeFortranAssumed: {
    if ((!type->get_fortran_source_syntax() ||
         !exactFortranAssumedTypeUse(info, false)) &&
        !exactGeneratedFortranTypeUse(info)) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-assumed-source-type]: "
                   "assumed type has no exact TYPE(*) declaration use\n";
      ROSE_ABORT();
    }
    curprint("TYPE(*)");
    break;
  }

  case V_SgTypeFortranUnlimitedPolymorphic: {
    if ((!type->get_fortran_source_syntax() ||
         !exactFortranAssumedTypeUse(info, true)) &&
        !exactGeneratedFortranTypeUse(info)) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-unlimited-polymorphic-source-"
                   "type]: unlimited polymorphic type has no exact CLASS(*) "
                   "declaration use\n";
      ROSE_ABORT();
    }
    curprint("CLASS(*)");
    break;
  }

  case V_SgTypeVoid: {
    const auto *initializedName =
        isSgInitializedName(info.get_reference_node_for_qualification());
    const auto *declaration =
        initializedName != nullptr
            ? isSgVariableDeclaration(initializedName->get_parent())
            : nullptr;
    if (declaration == nullptr || !declaration->get_declarationModifier()
                                       .get_storageModifier()
                                       .isExtern()) {
      std::cerr << "Error: SgTypeVoid is only valid for an EXTERNAL Fortran "
                   "procedure entity"
                << std::endl;
      ROSE_ABORT();
    }
    break;
  }

  case V_SgTypeString:
    unparseStringType(type, info, printAttrs);
    break;

    // scalar integral types
  case V_SgTypeChar:
    unparseBaseType(type, "CHARACTER", info);
    break;
  case V_SgTypeInt:
    unparseBaseType(type, "INTEGER", info);
    break;
  case V_SgTypeSignedInt:
    unparseBaseType(type, "INTEGER", info);
    break;
  case V_SgTypeUnsignedInt:
    if (!type->get_fortran_source_syntax()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-unsigned-source-type]: "
                   "semantic UNSIGNED type reached declaration emission "
                   "without exact source syntax identity"
                << std::endl;
      ROSE_ABORT();
    }
    unparseBaseType(type, "UNSIGNED", info);
    break;

    // scalar floating point types
  case V_SgTypeFloat:
    unparseBaseType(type, "REAL", info);
    break;
  case V_SgTypeDouble:
    unparseBaseType(type, "DOUBLE PRECISION", info);
    break;

    // scalar boolean type
  case V_SgTypeBool:
    unparseBaseType(type, "LOGICAL", info);
    break;

    // complex type
  case V_SgTypeComplex: {
    SgTypeComplex *complexType = isSgTypeComplex(type);
    ASSERT_not_null(complexType);
    if (isSgTypeDouble(complexType->get_base_type())) {
      unparseBaseType(type, "DOUBLE COMPLEX", info);
    } else if (isSgTypeFloat(complexType->get_base_type())) {
      unparseBaseType(type, "COMPLEX", info);
    } else {
      std::cerr << "Error: Fortran complex type has an unsupported base type"
                << std::endl;
      ROSE_ABORT();
    }
    break;
  }

    // Rice coarrays
  case V_SgTypeCAFTeam:
    unparseBaseType(type, "TEAM", info);
    break;

  case V_SgTypeCrayPointer:
    unparseBaseType(type, "POINTER", info);
    break;

    // array type
  case V_SgArrayType:
    unparseArrayType(type, info, printAttrs);
    break;

    // pointer and reference support
  case V_SgPointerType:
    unparsePointerType(type, info, printAttrs);
    break;
  case V_SgReferenceType:
    unparseReferenceType(type, info);
    break;

  case V_SgClassType:
    unparseClassType(type, info);
    break;

    // unparse kind and type parameters
  case V_SgModifierType:
    unparseModifierType(type, info);
    break;

  case V_SgFunctionType:
    unparseFunctionType(type, info);
    break;

  default: {
    printf(
        "UnparserFort::unparseType: Error: No handler for %s (variant: %d)\n",
        type->sage_class_name(), type->variantT());
    ROSE_ABORT();
  }
  }
}

//----------------------------------------------------------------------------
//  UnparserFort::<>
//----------------------------------------------------------------------------

void UnparseFortran_type::unparseTypeKind(SgType *type, SgUnparse_Info &info) {
  SgExpression *kindExpression = type->get_type_kind();
  if (kindExpression != nullptr) {
    if (type->get_hasTypeKindStar()) {
      curprint("*");
      unp->u_fortran_locatedNode->unparseExpression(kindExpression, info);
    } else {
      curprint("(kind=");
      unp->u_fortran_locatedNode->unparseExpression(kindExpression, info);
      curprint(")");
    }
  }
}

void UnparseFortran_type::unparseTypeLengthAndKind(
    SgType *type, SgExpression *lengthExpression, SgUnparse_Info &info) {
  SgExpression *kindExpression = type->get_type_kind();
  if (lengthExpression != nullptr || kindExpression != nullptr) {
    curprint("(");

    if (lengthExpression != nullptr) {
      curprint("len=");
      unp->u_fortran_locatedNode->unparseExpression(lengthExpression, info);

      // Check if there will be a kind paramter.
      if (kindExpression != NULL) {
        curprint(",");
      }
    }

    if (kindExpression != NULL) {
      curprint("kind=");
      unp->u_fortran_locatedNode->unparseExpression(kindExpression, info);
    }

    curprint(")");
  }
}

void UnparseFortran_type::unparseBaseType(SgType *type,
                                          const std::string &nameOfType,
                                          SgUnparse_Info &info) {
  if (isSgTypeChar(type) != nullptr || isSgTypeInt(type) != nullptr ||
      isSgTypeSignedInt(type) != nullptr ||
      isSgTypeUnsignedInt(type) != nullptr || isSgTypeFloat(type) != nullptr ||
      isSgTypeDouble(type) != nullptr || isSgTypeBool(type) != nullptr ||
      isSgTypeComplex(type) != nullptr) {
    requireFortranIntrinsicSourceType(type, info);
  }
  curprint(nameOfType);
  unparseTypeKind(type, info);
}

void UnparseFortran_type::unparseStringType(SgType *type, SgUnparse_Info &info,
                                            bool printAttrs) {
  SgTypeString *stringType{isSgTypeString(type)};
  ASSERT_not_null(stringType);
  if ((!stringType->get_fortran_source_syntax() &&
       !exactGeneratedFortranTypeUse(info)) ||
      stringType->get_fortran_dynamic_length_pending() ||
      stringType->get_fortran_dynamic_result_length()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-character-source-type]: "
                 "semantic or unresolved CHARACTER type reached declaration "
                 "emission\n";
    ROSE_ABORT();
  }
  requireFortranIntrinsicSourceType(type, info);
  curprint("CHARACTER");
  if (printAttrs) {
    unparseTypeLengthAndKind(stringType, stringType->get_lengthExpression(),
                             info);
  } else { // the length will be printed as part of the entity_decl.
    unparseTypeKind(type, info);
  }
}

void UnparseFortran_type::unparseArrayType(SgType *type, SgUnparse_Info &info,
                                           bool printDim) {
  // Examples:
  //   real, dimension(10, 10) :: A1, A2
  //   real, dimension(:) :: B1
  //   character(len=*) :: s1

  SgArrayType *array_type = isSgArrayType(type);
  ASSERT_not_null(array_type);
  ASSERT_not_null(array_type->get_base_type());
  ASSERT_not_null(array_type->get_dim_info());

  if (info.supressStrippedTypeName() == false) {
    // only output the name of the stripped type once
    SgType *stripType = array_type->stripType();
    unparseType(stripType, info);
    info.set_supressStrippedTypeName();
  }

  const SgInitializedName *initializedName =
      isSgInitializedName(info.get_reference_node_for_qualification());
  const bool emitCurrentShape =
      printDim && (array_type->get_isCoArray() || initializedName == nullptr ||
                   !fortranAttributeStatementOwnsArrayShape(initializedName));
  if (emitCurrentShape) {
    if (!array_type->get_isCoArray()) {
      ROSE_ASSERT(array_type->get_rank() >= 1);
    }
    curprint(array_type->get_isCoArray() ? ", CODIMENSION" : ", DIMENSION");

    ASSERT_not_null(unp);
    ASSERT_not_null(unp->u_fortran_locatedNode);

    if (array_type->get_isCoArray()) { // print codimension info
      curprint("[");

      // DQ (3/28/2017): Eliminate warning of overloaded virtual function in
      // base class (from Clang).
      // unp->u_fortran_locatedNode->unparseExprList(array_type->get_dim_info(),info,/*
      // do not output parens */ false);
      unp->u_fortran_locatedNode->unparseExprList(array_type->get_dim_info(),
                                                  info);

      curprint("]");
    } else // print dimension info
    {
      // DQ (3/28/2017): Eliminate warning of overloaded virtual function in
      // base class (from Clang).
      // unp->u_fortran_locatedNode->unparseExprList(array_type->get_dim_info(),info,/*
      // output parens */ true);
      curprint("(");
      unp->u_fortran_locatedNode->unparseExprList(array_type->get_dim_info(),
                                                  info);
      curprint(")");
    }
  }

  if (array_type->get_base_type()->containsInternalTypes() == true) {
    unparseType(array_type->get_base_type(), info, printDim);
  }
}

void UnparseFortran_type::unparsePointerType(SgType *type, SgUnparse_Info &info,
                                             bool printAttrs) {
  SgPointerType *pointer_type = isSgPointerType(type);
  ASSERT_not_null(pointer_type);

  unparseType(pointer_type->get_base_type(), info, printAttrs);
  curprint(type->get_isCoArray() ? ", COPOINTER" : ", POINTER");
}

void UnparseFortran_type::unparseReferenceType(SgType *type,
                                               SgUnparse_Info &info) {
  ASSERT_not_null(isSgReferenceType(type));
  static_cast<void>(info);
  std::cerr << "Error: SgReferenceType is not a valid Fortran declaration type"
            << std::endl;
  ROSE_ABORT();
}

void UnparseFortran_type::unparseClassType(SgType *type, SgUnparse_Info &info) {
  SgClassType *class_type = isSgClassType(type);
  ASSERT_not_null(class_type);

  if (info.isTypeSecondPart() == false) {
    const auto *initializedName =
        isSgInitializedName(info.get_reference_node_for_qualification());
    const auto *procedure = isSgProcedureHeaderStatement(
        info.get_reference_node_for_qualification());
    const auto *fortranConstructor =
        isSgAggregateInitializer(info.get_reference_node_for_qualification());
    const SgName sourceTypeName =
        requireFortranSourceDerivedTypeName(class_type, info);
    bool emitClass = false;
    if (fortranConstructor != nullptr) {
      const auto sourceForm = fortranConstructor->get_source_form();
      if ((sourceForm !=
               SgAggregateInitializer::e_aggregate_initializer_source_fortran &&
           sourceForm !=
               SgAggregateInitializer::
                   e_aggregate_initializer_source_fortran_structure) ||
          !fortranConstructor->get_fortran_has_source_explicit_type() ||
          fortranConstructor->get_fortran_source_explicit_type() !=
              class_type) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-constructor-type-spec]: "
                     "derived constructor has no exact source type-spec "
                     "ownership\n";
        ROSE_ABORT();
      }
      curprint(sourceTypeName.str());
      return;
    } else if (initializedName != nullptr) {
      const auto typeSpec = initializedName->get_fortran_type_spec();
      if (typeSpec == SgInitializedName::e_fortran_type_spec_class) {
        emitClass = true;
      } else if (typeSpec != SgInitializedName::e_fortran_type_spec_type &&
                 !(typeSpec == SgInitializedName::e_fortran_type_spec_default &&
                   exactGeneratedFortranTypeUse(info))) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-derived-type-spec]: "
                     "SgClassType declaration has no exact TYPE/CLASS source "
                     "identity\n";
        ROSE_ABORT();
      }
    } else if (procedure != nullptr) {
      const auto typeSpec = procedure->get_fortran_result_type_spec();
      if (typeSpec ==
          SgProcedureHeaderStatement::e_fortran_result_type_spec_class) {
        emitClass = true;
      } else if (typeSpec !=
                 SgProcedureHeaderStatement::e_fortran_result_type_spec_type) {
        std::cerr
            << "REX_UNPARSE_INVARIANT[fortran-procedure-result-type-spec]: "
               "typed procedure '"
            << procedure->get_name().getString()
            << "' has no exact TYPE/CLASS source identity" << std::endl;
        ROSE_ABORT();
      }
    } else {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-derived-type-use-site]: "
                   "SgClassType has no exact declaration or procedure-result "
                   "source identity\n";
      ROSE_ABORT();
    }
    if (emitClass) {
      curprint("CLASS(");
    } else {
      curprint("TYPE(");
    }
    curprint(sourceTypeName.str());
    curprint(")");
  }
}

void UnparseFortran_type::unparseModifierType(SgType *type,
                                              SgUnparse_Info &info) {
  SgModifierType *mod_type = isSgModifierType(type);
  ASSERT_not_null(mod_type);

  unparseType(mod_type->get_base_type(), info);

  SgExpression *kindExpression = mod_type->get_type_kind();
  if (kindExpression != nullptr) {
    curprint("(");
    if (kindExpression != NULL) {
      curprint("kind=");
      unp->u_fortran_locatedNode->unparseExpression(kindExpression, info);
    }
    curprint(")");
  }
}

void UnparseFortran_type::unparseFunctionType(SgType *type,
                                              SgUnparse_Info &info) {
  SgFunctionType *func_type = isSgFunctionType(type);
  ASSERT_not_null(func_type);

  curprint("PROCEDURE(");
  const auto *initializedName =
      isSgInitializedName(info.get_reference_node_for_qualification());
  if (initializedName != nullptr &&
      !initializedName->get_fortran_procedure_interface().is_null()) {
    curprint(initializedName->get_fortran_procedure_interface().str());
  } else if (func_type->get_return_type() != nullptr &&
             isSgTypeVoid(func_type->get_return_type()) == nullptr) {
    SgUnparse_Info returnTypeInfo(info);
    returnTypeInfo.set_reference_node_for_qualification(nullptr);
    unparseType(func_type->get_return_type(), returnTypeInfo, false);
  }
  curprint(")");
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

void UnparseFortran_type::curprint(const std::string &str) const {
  unp->emitFortranText(str);
}

bool UnparseFortran_type::isCharType(SgType *type) {
  switch (type->variantT()) {
  case V_SgTypeChar:
  case V_SgTypeSignedChar:
  case V_SgTypeUnsignedChar:
    return true;

  default:
    return false;
  }
}
