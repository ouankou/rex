/**
 *  \file ASTtools.hh
 *
 *  \brief Higher-level AST manipulation support routines.
 *
 *  \author Chunhua Liao <liaoch@cs.uh.edu>, Richard Vuduc
 *  <richie@llnl.gov>
 *
 *  This implementation was extracted by Liao from his ROSE
 *  OpenMP_Translator project.
 */
#include "sage3basic.h"

#include <iostream>

#include <sstream>

#include "ROSE_FALLTHROUGH.h"

#include "ASTtools.hh"

// =====================================================================

using namespace std;

// =====================================================================

SgFunctionDeclaration *ASTtools::canonicalFunctionFamilyDeclaration(
    SgFunctionDeclaration *declaration) {
  if (declaration == NULL)
    return NULL;

  if (SgFunctionDeclaration *first = isSgFunctionDeclaration(
          declaration->get_firstNondefiningDeclaration()))
    return first;

  if (SgFunctionDeclaration *defining =
          isSgFunctionDeclaration(declaration->get_definingDeclaration()))
    return defining;

  return declaration;
}

bool ASTtools::sameFunctionFamily(SgFunctionDeclaration *lhs,
                                  SgFunctionDeclaration *rhs) {
  return lhs != NULL && rhs != NULL &&
         canonicalFunctionFamilyDeclaration(lhs) ==
             canonicalFunctionFamilyDeclaration(rhs);
}

SgFunctionDeclaration *ASTtools::functionOwningHiddenNamedType(SgType *type) {
  SgType *base = type != NULL ? type->findBaseType() : NULL;
  SgNamedType *named = isSgNamedType(base);
  SgDeclarationStatement *declaration =
      named != NULL ? named->get_declaration() : NULL;
  if (auto *use = isSgNonrealDecl(declaration)) {
    if (auto *target = use->get_templateDeclaration()) {
      ROSE_ASSERT(target != use);
      declaration = target;
    }
  }
  if (declaration == NULL)
    return NULL;

  SgFunctionDeclaration *function =
      SageInterface::getEnclosingFunctionDeclaration(declaration);
  if (function == NULL)
    return NULL;

  for (SgScopeStatement *scope = declaration->get_scope(); scope != NULL;
       scope = scope->get_scope()) {
    if (isSgGlobal(scope) != NULL ||
        isSgNamespaceDefinitionStatement(scope) != NULL)
      return NULL;
    if (sameFunctionFamily(
            SageInterface::getEnclosingFunctionDeclaration(scope, true),
            function))
      return function;
  }

  fprintf(stderr,
          "REX_OUTLINER_INVARIANT[local-type-owner]: named type=%p "
          "declaration=%p function=%p has no exact lexical path to its "
          "owning function\n",
          static_cast<void *>(base), static_cast<void *>(declaration),
          static_cast<void *>(function));
  ROSE_ABORT();
}

bool ASTtools::isConstObj(const SgType *type) {
  bool canFollowIndirection = true;
  for (;;) {
    ROSE_ASSERT(type != nullptr);
    if (const SgTypedefType *alias = isSgTypedefType(type)) {
      type = alias->get_base_type();
    } else if (const SgModifierType *modifier = isSgModifierType(type)) {
      if (modifier->get_typeModifier().get_constVolatileModifier().isConst())
        return true;
      type = modifier->get_base_type();
    } else if (const SgPointerType *pointer = isSgPointerType(type)) {
      if (!canFollowIndirection)
        return false;
      canFollowIndirection = false;
      type = pointer->get_base_type();
    } else if (const SgReferenceType *reference = isSgReferenceType(type)) {
      if (!canFollowIndirection)
        return false;
      canFollowIndirection = false;
      type = reference->get_base_type();
    } else if (const SgArrayType *array = isSgArrayType(type)) {
      type = array->get_base_type();
    } else {
      return false;
    }
  }
}

void ASTtools::publishTemplateParameterPackExpansion(
    const SgTemplateParameter *parameter, SgTemplateArgument *argument) {
  if (parameter == nullptr || argument == nullptr) {
    std::cerr << "REX_OUTLINER_INVARIANT[template-argument-pack-surface]: null "
                 "template parameter or argument"
              << std::endl;
    ROSE_ABORT();
  }
  if (argument->get_is_pack_element()) {
    std::cerr << "REX_OUTLINER_INVARIANT[template-argument-pack-surface]: "
                 "argument already has a pack-expansion owner"
              << std::endl;
    ROSE_ABORT();
  }
  if (!parameter->get_is_parameter_pack()) {
    return;
  }

  if (argument->get_argumentType() == SgTemplateArgument::type_argument) {
    SgType *semantic_type = argument->get_type();
    if (semantic_type == nullptr || semantic_type != parameter->get_type() ||
        argument->get_sourceSpelledType() != nullptr) {
      std::cerr
          << "REX_OUTLINER_INVARIANT[template-argument-pack-surface]: type "
             "pack argument does not reference its exact parameter type"
          << std::endl;
      ROSE_ABORT();
    }

    // The semantic argument keeps the exact template-parameter type.  Its
    // spelling surface is a distinct use and must not inherit the declaration
    // ellipsis, otherwise `T...` becomes `T......` when the argument expansion
    // is emitted below.
    if (SgTemplateType *parameter_type = isSgTemplateType(semantic_type)) {
      ROSE_ASSERT(parameter_type->get_tpl_args().empty());
      ROSE_ASSERT(!parameter_type->get_is_pack_substitution());
      SgTemplateType *reference_type =
          new SgTemplateType(parameter_type->get_name());
      reference_type->set_template_parameter_position(
          parameter_type->get_template_parameter_position());
      reference_type->set_template_parameter_depth(
          parameter_type->get_template_parameter_depth());
      const bool has_template_coordinates =
          parameter_type->get_template_parameter_depth() >= 0 &&
          parameter_type->get_template_parameter_position() >= 0;
      const std::optional<SgTemplateType::canonical_source_identity>
          source_identity = parameter_type->get_canonical_source_identity();
      if (has_template_coordinates != source_identity.has_value()) {
        std::cerr << "REX_OUTLINER_INVARIANT[template-argument-pack-identity]: "
                     "parameter type has mismatched coordinates and canonical "
                     "source identity"
                  << std::endl;
        ROSE_ABORT();
      }
      if (source_identity.has_value()) {
        reference_type->initialize_canonical_source_identity(*source_identity);
      }
      reference_type->set_template_parameter(
          parameter_type->get_template_parameter());
      argument->set_sourceSpelledType(reference_type);
    } else if (isSgNonrealType(semantic_type) == nullptr) {
      std::cerr
          << "REX_OUTLINER_INVARIANT[template-argument-pack-surface]: type "
             "pack has no exact template-parameter type identity"
          << std::endl;
      ROSE_ABORT();
    }
  }

  argument->set_is_pack_element(true);
}

// ========================================================================

const SgFunctionDefinition *ASTtools::findFirstFuncDef(const SgStatement *s) {
  if (s) {
    const SgScopeStatement *scope = s->get_scope();
    const SgFunctionDefinition *def = isSgFunctionDefinition(scope);
    if (def)
      return def;
    else
      return findFirstFuncDef(scope);
  }
  return 0; // Can't find?
}

// ========================================================================

bool ASTtools::isSwitchCond(const SgStatement *s) {
  if (s) {
    const SgSwitchStatement *s_switch = isSgSwitchStatement(s->get_parent());
    return s_switch && s_switch->get_item_selector() == s;
  }
  return false;
}

bool ASTtools::isIfCond(const SgStatement *s) {
  if (s) {
    const SgIfStmt *s_if = isSgIfStmt(s->get_parent());
    return s_if && s_if->get_conditional() == s;
  }
  return false;
}

bool ASTtools::isWhileCond(const SgStatement *s) {
  if (s) {
    const SgNode *s_par = s->get_parent();
    if (isSgWhileStmt(s_par)) {
      const SgWhileStmt *s_while = isSgWhileStmt(s_par);
      ROSE_ASSERT(s_while);
      return s_while && s_while->get_condition() == s;
    } else if (isSgDoWhileStmt(s_par)) {
      const SgDoWhileStmt *s_while = isSgDoWhileStmt(s_par);
      ROSE_ASSERT(s_while);
      return s_while && s_while->get_condition() == s;
    }
  }
  return false;
}

string ASTtools::toStringFileLoc(const SgLocatedNode *n) {
  stringstream s;
  if (n) {
    string sep(" ");
    switch (n->variantT()) {
    case V_SgVarRefExp: {
      const SgVarRefExp *ref = isSgVarRefExp(n);
      ROSE_ASSERT(ref);
      const SgVariableSymbol *sym = ref->get_symbol();
      ROSE_ASSERT(sym);
      s << "'" << sym->get_name().getString() << '\'';
    } break;
    case V_SgFunctionRefExp: {
      const SgFunctionRefExp *ref = isSgFunctionRefExp(n);
      ROSE_ASSERT(ref);
      const SgFunctionSymbol *sym = ref->get_symbol();
      ROSE_ASSERT(sym);
      s << "'" << sym->get_name().getString() << '\'';
    }
      ROSE_FALLTHROUGH;
    default:
      sep = "";
      break;
    }

    const Sg_File_Info *start = n->get_startOfConstruct();
    s << sep << (const void *)n << ":<" << n->class_name() << ">"
      << " [" << start->get_raw_filename() << ':' << start->get_raw_line();
    const Sg_File_Info *end = n->get_endOfConstruct();
    if (end && end->get_raw_line() > start->get_raw_line())
      s << "--" << end->get_raw_line();
    s << "]";
  }
  return s.str();
}

// eof
