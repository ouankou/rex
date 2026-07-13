
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#include <algorithm>
#include <unordered_set>

static bool
compoundLiteralContainsBaseTypeDefinition(SgAggregateInitializer *aggr_init,
                                          SgVariableDeclaration *&owner_decl) {
  ASSERT_not_null(aggr_init);
  owner_decl = nullptr;

  SgInitializedName *initialized_name =
      isSgInitializedName(aggr_init->get_parent());
  if (initialized_name == nullptr) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[compound-literal-owner]: aggregate "
                    "initializer has no exact initialized-name owner\n");
    ROSE_ABORT();
  }

  SgVariableDeclaration *variable_declaration =
      isSgVariableDeclaration(initialized_name->get_parent());
  if (variable_declaration == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[compound-literal-owner]: initialized "
            "name has no exact variable-declaration owner\n");
    ROSE_ABORT();
  }
  owner_decl = variable_declaration;

  // A compound literal is represented by one exact auxiliary declaration. Its
  // structural ownership and typed base-definition edge, not a late output
  // suppression flag or ancestry/name heuristic, determine whether the source
  // spelled an inline tag definition.
  Sg_File_Info *file_info = variable_declaration->get_file_info();
  SgAuxiliaryDeclarationList *auxiliary_owner =
      isSgAuxiliaryDeclarationList(variable_declaration->get_parent());
  SgScopeStatement *semantic_scope = variable_declaration->get_scope();
  if (file_info == nullptr || !file_info->isCompilerGenerated() ||
      auxiliary_owner == nullptr || semantic_scope == nullptr ||
      auxiliary_owner->get_parent() != semantic_scope ||
      initialized_name->get_scope() != semantic_scope ||
      initialized_name->get_initializer() != aggr_init) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[compound-literal-owner]: compound literal "
            "does not have one exact auxiliary compiler-generated "
            "declaration owner\n");
    ROSE_ABORT();
  }

  const bool contains_definition =
      variable_declaration->get_baseTypeDefiningDeclaration() != nullptr;
  return contains_definition;
}

static bool isIncludeDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
  return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
         type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
}

static size_t validateAggregateInitializerIncludeBoundaries(
    SgAggregateInitializer *aggregateInitializer,
    const SgExpressionPtrList &elements, bool hasExplicitBraces) {
  ASSERT_not_null(aggregateInitializer);
  Sg_File_Info *aggregateStart = aggregateInitializer->get_startOfConstruct();
  Sg_File_Info *aggregateEnd = aggregateInitializer->get_endOfConstruct();
  auto sourceLess = [](Sg_File_Info *lhs, Sg_File_Info *rhs) {
    return lhs->get_line() < rhs->get_line() ||
           (lhs->get_line() == rhs->get_line() &&
            lhs->get_col() < rhs->get_col());
  };
  auto validateSourcePosition = [&](PreprocessingInfo *info) {
    Sg_File_Info *includePosition =
        info != nullptr ? info->get_file_info() : nullptr;
    if (aggregateStart == nullptr || aggregateEnd == nullptr ||
        includePosition == nullptr || aggregateStart->get_line() <= 0 ||
        aggregateStart->get_col() <= 0 || aggregateEnd->get_line() <= 0 ||
        aggregateEnd->get_col() <= 0 || includePosition->get_line() <= 0 ||
        includePosition->get_col() <= 0 ||
        aggregateStart->get_physical_file_id() < 0 ||
        aggregateEnd->get_physical_file_id() < 0 ||
        includePosition->get_physical_file_id() < 0 ||
        !aggregateStart->isSameFile(*aggregateEnd) ||
        !aggregateStart->isSameFile(*includePosition) ||
        !sourceLess(aggregateStart, includePosition) ||
        !sourceLess(includePosition, aggregateEnd)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[initializer-include-boundary]: include "
              "is not strictly inside its aggregate source interval\n");
      ROSE_ABORT();
    }
  };

  std::unordered_set<PreprocessingInfo *> claimedIncludes;
  auto validateOwner = [&](SgLocatedNode *owner,
                           PreprocessingInfo::RelativePositionType position) {
    AttachedPreprocessingInfoType *attached =
        owner != nullptr ? owner->getAttachedPreprocessingInfo() : nullptr;
    if (attached == nullptr) {
      return;
    }
    for (PreprocessingInfo *info : *attached) {
      ASSERT_not_null(info);
      if (!isIncludeDirective(info)) {
        continue;
      }
      // An aggregate expression can itself be a direct element of an outer
      // aggregate.  A `before` include on that node is the outer aggregate's
      // typed boundary, not an `inside` boundary of this aggregate.  Likewise,
      // an `inside` include on a nested aggregate belongs to the nested
      // aggregate rather than to its parent's element boundary.  Select only
      // the exact relative-position role owned by this validation pass.
      if (info->getRelativePosition() != position) {
        continue;
      }
      if (!hasExplicitBraces || !claimedIncludes.insert(info).second) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[initializer-include-boundary]: "
                "owner=%p/%s relative=%d expected-relative=%d include=%s "
                "has no unique typed aggregate-interior boundary\n",
                static_cast<void *>(owner), owner->class_name().c_str(),
                static_cast<int>(info->getRelativePosition()),
                static_cast<int>(position), info->getString().c_str());
        ROSE_ABORT();
      }
      validateSourcePosition(info);
    }
  };

  validateOwner(aggregateInitializer, PreprocessingInfo::inside);
  SgExprListExp *initializerList = aggregateInitializer->get_initializers();
  ASSERT_not_null(initializerList);
  for (SgExpression *element : elements) {
    ASSERT_not_null(element);
    if (element->get_parent() != initializerList) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[aggregate-initializer-element-owner]: "
              "direct initializer element does not belong to the exact "
              "expression list\n");
      ROSE_ABORT();
    }
    validateOwner(element, PreprocessingInfo::before);
  }
  return claimedIncludes.size();
}

#define DEBUG__unparseAggrInit 0

void Unparse_ExprStmt::unparseAggrInit(SgExpression *expr,
                                       SgUnparse_Info &info) {
  SgAggregateInitializer *aggr_init = isSgAggregateInitializer(expr);
  ASSERT_not_null(aggr_init);

#if DEBUG__unparseAggrInit
  bool compiler_generated =
      aggr_init->get_startOfConstruct()->isCompilerGenerated();
  printf("Enter Unparse_ExprStmt::unparseAggrInit():\n");
  printf("  aggr_init = %p = %s\n", aggr_init, aggr_init->class_name().c_str());
  printf("    ->get_source_form() = %d\n",
         static_cast<int>(aggr_init->get_source_form()));
  printf("  compiler_generated = %s\n", compiler_generated ? "true" : "false");
#endif

  const SgAggregateInitializer::aggregate_initializer_source_form_enum
      source_form = aggr_init->get_source_form();
  bool need_explicit_braces = false;
  bool need_typed_brace_prefix = false;
  bool is_compound_literal = false;
  switch (source_form) {
  case SgAggregateInitializer::e_aggregate_initializer_source_braced:
    need_explicit_braces = true;
    break;
  case SgAggregateInitializer::e_aggregate_initializer_source_unbraced:
    break;
  case SgAggregateInitializer::e_aggregate_initializer_source_typed_braced:
    need_explicit_braces = true;
    need_typed_brace_prefix = true;
    break;
  case SgAggregateInitializer::e_aggregate_initializer_source_compound_literal:
    need_explicit_braces = true;
    is_compound_literal = true;
    break;
  case SgAggregateInitializer::e_aggregate_initializer_source_fortran:
  case SgAggregateInitializer::e_aggregate_initializer_source_fortran_structure:
  case SgAggregateInitializer::e_aggregate_initializer_source_unclassified:
  default:
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[aggregate-initializer-source-form]: "
            "C/C++ aggregate initializer has invalid source form=%d\n",
            static_cast<int>(source_form));
    ROSE_ABORT();
  }

  SgUnparse_Info newinfo2(info);
  newinfo2.set_inAggregateInitializer();

  if (is_compound_literal) {
    // This aggregate initializer is using a compound literal and so we need to
    // output the type. This looks like an explict cast, but is not a cast
    // internally in the language, just that this is how compound literals are
    // supposed to be handled.
    ASSERT_not_null(aggr_init->get_type());

#if DEBUG__unparseAggrInit
    printf("  aggr_init->get_type() = %p = %s \n", aggr_init->get_type(),
           aggr_init->get_type()->class_name().c_str());
#endif

    SgVariableDeclaration *compound_literal_var_decl = NULL;
    const bool contains_base_type_definition =
        compoundLiteralContainsBaseTypeDefinition(aggr_init,
                                                  compound_literal_var_decl);
#if DEBUG__unparseAggrInit
    printf("  contains_base_type_definition = %s \n",
           contains_base_type_definition ? "true" : "false");
#endif
    if (contains_base_type_definition) {
      newinfo2.unset_SkipClassDefinition();
      newinfo2.unset_SkipEnumDefinition();
      ASSERT_not_null(compound_literal_var_decl);
      newinfo2.set_declstatement_ptr(compound_literal_var_decl);
      SgDeclarationStatement *base_type_defn_decl =
          compound_literal_var_decl->get_baseTypeDefiningDeclaration();
      ASSERT_not_null(base_type_defn_decl);
      if (isSgClassDeclaration(base_type_defn_decl) == nullptr &&
          isSgEnumDeclaration(base_type_defn_decl) == nullptr) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[compound-literal-definition]: inline "
                "base-type definition is neither a class nor enum "
                "declaration\n");
        ROSE_ABORT();
      }
    } else {
      newinfo2.set_SkipClassDefinition();
      newinfo2.set_SkipEnumDefinition();
    }

#if DEBUG__unparseAggrInit
    printf("  SkipClassDefinition() = %s \n",
           newinfo2.SkipClassDefinition() ? "true" : "false");
    printf("  SkipEnumDefinition()  = %s \n",
           newinfo2.SkipEnumDefinition() ? "true" : "false");
#endif

    curprint("(");
    unp->u_type->outputType<SgAggregateInitializer>(
        aggr_init, aggr_init->get_type(), newinfo2);
    curprint(")");
  }

#if DEBUG__unparseAggrInit
  printf("  need_explicit_braces     = %s \n",
         need_explicit_braces ? "true" : "false");
  printf("  SkipEnumDefinition()     = %s \n",
         newinfo2.SkipEnumDefinition() ? "true" : "false");
  printf("  SkipClassDefinition()    = %s \n",
         newinfo2.SkipClassDefinition() ? "true" : "false");
  printf("  inAggregateInitializer() = %s \n",
         newinfo2.inAggregateInitializer() ? "true" : "false");
#endif

  SgExprListExp *initializers = aggr_init->get_initializers();
  if (initializers == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[aggregate-initializer-list]: initializer "
            "has no exact expression list\n");
    ROSE_ABORT();
  }
  const SgExpressionPtrList &list = initializers->get_expressions();
  if (need_typed_brace_prefix) {
    SgType *source_type = aggr_init->get_type();
    if (source_type == nullptr || isSgTypeUnknown(source_type) != nullptr ||
        isSgTypeDefault(source_type) != nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[aggregate-initializer-prefix-type]: "
              "typed braced initializer has no exact semantic type\n");
      ROSE_ABORT();
    }
    newinfo2.set_SkipClassSpecifier();
    unp->u_type->outputType<SgAggregateInitializer>(aggr_init, source_type,
                                                    newinfo2);
  }

  if (need_explicit_braces) {
    curprint("{");
  }

  // Validate element existence before any optional preprocessing-boundary
  // traversal.  A null child is a malformed aggregate AST in its own right;
  // letting the include validator discover it first obscures the producer
  // contract with an unrelated boundary diagnostic.
  for (size_t index = 0; index < list.size(); ++index) {
    if (list[index] == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[aggregate-initializer-element]: "
              "initializer element %zu is null\n",
              index);
      ROSE_ABORT();
    }
  }

  const size_t initializerIncludeCount =
      validateAggregateInitializerIncludeBoundaries(aggr_init, list,
                                                    need_explicit_braces);
  const SgUnsignedCharList &sourceElementRoles =
      aggr_init->get_source_element_roles();
  if ((initializerIncludeCount == 0 && !sourceElementRoles.empty()) ||
      (initializerIncludeCount != 0 &&
       sourceElementRoles.size() != list.size())) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[initializer-element-source-role]: "
            "initializer=%p includes=%zu elements=%zu roles=%zu does not "
            "publish one exact include-expansion ownership map\n",
            static_cast<void *>(aggr_init), initializerIncludeCount,
            list.size(), sourceElementRoles.size());
    ROSE_ABORT();
  }

#if DEBUG__unparseAggrInit
  printf("  list.size() = %zu \n", list.size());
#endif

  bool priorAstElementWithoutIncludeBoundary = false;
  for (size_t index = 0; index < list.size(); index++) {
    const unsigned char sourceElementRole =
        sourceElementRoles.empty()
            ? static_cast<unsigned char>(
                  SgAggregateInitializer::e_source_element_ast)
            : sourceElementRoles[index];
    const bool isAstElementRole =
        sourceElementRole == SgAggregateInitializer::e_source_element_ast ||
        sourceElementRole ==
            SgAggregateInitializer::e_source_element_ast_after_owner_separator;
    const bool isIncludeElementRole =
        sourceElementRole ==
            SgAggregateInitializer::e_source_element_include_expansion ||
        sourceElementRole ==
            SgAggregateInitializer::
                e_source_element_include_expansion_after_owner_separator;
    const bool hasOwnerSeparator =
        sourceElementRole == SgAggregateInitializer::
                                 e_source_element_ast_after_owner_separator ||
        sourceElementRole ==
            SgAggregateInitializer::
                e_source_element_include_expansion_after_owner_separator;
    if (!isAstElementRole && !isIncludeElementRole) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[initializer-element-source-role]: "
              "initializer=%p element=%zu has invalid role=%u\n",
              static_cast<void *>(aggr_init), index,
              static_cast<unsigned>(sourceElementRole));
      ROSE_ABORT();
    }
    if (hasOwnerSeparator && index == 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[initializer-element-source-role]: "
              "initializer=%p first element cannot follow an owner-file "
              "separator\n",
              static_cast<void *>(aggr_init));
      ROSE_ABORT();
    }
    if (sourceElementRole == SgAggregateInitializer::
                                 e_source_element_ast_after_owner_separator &&
        priorAstElementWithoutIncludeBoundary) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[initializer-element-source-role]: "
              "initializer=%p element=%zu publishes an owner-file boundary "
              "between two AST-owned elements\n",
              static_cast<void *>(aggr_init), index);
      ROSE_ABORT();
    }
    if (hasOwnerSeparator) {
      curprint(", ");
    }
    if (isIncludeElementRole) {
      SgExpression *element = list[index];
      size_t beforeIncludeCount = 0;
      if (AttachedPreprocessingInfoType *attached =
              element->getAttachedPreprocessingInfo()) {
        beforeIncludeCount = static_cast<size_t>(std::count_if(
            attached->begin(), attached->end(), [](PreprocessingInfo *info) {
              return isIncludeDirective(info) &&
                     info->getRelativePosition() == PreprocessingInfo::before;
            }));
      }
      const bool startsIncludeOwnedRun = index == 0 ||
                                         beforeIncludeCount != 0 ||
                                         priorAstElementWithoutIncludeBoundary;
      if (startsIncludeOwnedRun && beforeIncludeCount == 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[initializer-element-source-role]: "
                "initializer=%p include-owned run at element=%zu has no "
                "exact attached include boundary\n",
                static_cast<void *>(aggr_init), index);
        ROSE_ABORT();
      }
      unparseAttachedPreprocessingInfo(element, info,
                                       PreprocessingInfo::before);
      priorAstElementWithoutIncludeBoundary = false;
      continue;
    }
    if (priorAstElementWithoutIncludeBoundary && !hasOwnerSeparator) {
      curprint(", ");
    }
    SgExpression *element = list[index];
    if (element == nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[aggregate-initializer-element]: "
              "initializer element %zu is null\n",
              index);
      ROSE_ABORT();
    }
    SgType *etype = element->get_type();
    if (etype == nullptr || isSgTypeUnknown(etype) != nullptr ||
        isSgTypeDefault(etype) != nullptr) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[aggregate-initializer-element-type]: "
              "initializer element %zu has no exact semantic type\n",
              index);
      ROSE_ABORT();
    }

    SgConstructorInitializer *ctor_init = isSgConstructorInitializer(element);

    bool element_compiler_generated = element->isCompilerGenerated();
#if DEBUG__unparseAggrInit
    printf("  - list index = %zu \n", index);
    printf("    element = %p = %s\n", element, element->class_name().c_str());
    printf("      type = %p = %s\n", etype, etype->class_name().c_str());
    printf("      compiler_generated = %s\n",
           element_compiler_generated ? "true" : "false");
#endif
    if (ctor_init != nullptr && element_compiler_generated) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[aggregate-initializer-source-element]: "
              "compiler-generated constructor element %zu reached source "
              "aggregate emission\n",
              index);
      ROSE_ABORT();
    }

    SgUnparse_Info newinfo(info);
    newinfo.set_inAggregateInitializer();

#if DEBUG__unparseAggrInit
    printf("    SkipEnumDefinition()  = %s \n",
           newinfo.SkipEnumDefinition() ? "true" : "false");
    printf("    SkipClassDefinition() = %s \n",
           newinfo.SkipClassDefinition() ? "true" : "false");
#endif
    unparseExpression(element, newinfo);
    priorAstElementWithoutIncludeBoundary = true;
  }

  unparseAttachedPreprocessingInfo(aggr_init, info, PreprocessingInfo::inside);

  if (need_explicit_braces) {
    curprint("}");
  }

#if DEBUG__unparseAggrInit
  printf("Leaving Unparse_ExprStmt::unparseAggrInit() \n");
#endif
}
