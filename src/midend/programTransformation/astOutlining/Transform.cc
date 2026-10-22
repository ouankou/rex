/**
 *  \file Transform/Transform.cc
 *
 *  \brief Implements the outlining transformation.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "astPostProcessing.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>

#include <iostream>

#include <list>

#include <map>

#include <set>

#include <string>

#include "Rose/StringUtility/Convert.h"

#include "ASTtools.hh"

#include "Outliner.hh"

#include "RoseAst.h"

#include "IncludeDirective.h"

#include "PreprocessingInfo.hh"

// =====================================================================

using namespace std;
using namespace Rose;
using namespace SageBuilder;
using namespace SageInterface;
// =====================================================================

namespace {
void removeConsumedOutlineDirectives(SgSourceFile *generated_source);

bool isIncludeDirective(const PreprocessingInfo *info) {
  if (info == NULL) {
    return false;
  }
  const PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
  return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
         type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
}

void validateDirectIncludeSystemRole(
    const PreprocessingInfo *info,
    const std::map<std::string, bool> &direct_include_roles) {
  if (!isIncludeDirective(info)) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[excluded-source-include]: attempted to "
            "classify a non-include preprocessing record\n");
    ROSE_ABORT();
  }
  IncludeDirective directive(info->getString());
  const std::string spelling = directive.getIncludedPath();
  const std::map<std::string, bool>::const_iterator role =
      direct_include_roles.find(spelling);
  if (spelling.empty() || role == direct_include_roles.end()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[excluded-source-include]: directive=%s "
            "has no exact direct include-graph role\n",
            info->getString().c_str());
    ROSE_ABORT();
  }
}

using ResolvedIncludeOwnerCache =
    std::map<PreprocessingInfo *, std::set<std::string>>;

void eraseResolvedFrontendIncludeOwner(
    SgSourceFile *source_file,
    std::map<std::string, std::set<PreprocessingInfo *>> &resolved_directives,
    ResolvedIncludeOwnerCache &directive_owner_cache,
    PreprocessingInfo *directive, const char *mutation) {
  if (source_file == NULL || directive == NULL || mutation == NULL ||
      !isIncludeDirective(directive)) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[resolved-include-mutation]: "
            "mutation=%s source=%p directive=%p has incomplete identity\n",
            mutation != NULL ? mutation : "<null>",
            static_cast<void *>(source_file), static_cast<void *>(directive));
    ROSE_ABORT();
  }

  if (resolved_directives.empty() && !directive->isTransformation()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[resolved-include-mutation]: mutation=%s "
            "source=%s could not initialize include owner cache\n",
            mutation, source_file->getFileName().c_str());
    ROSE_ABORT();
  }

  size_t expected = 0;
  size_t erased = 0;
  if (!directive->isTransformation()) {
    ResolvedIncludeOwnerCache::const_iterator owner =
        directive_owner_cache.find(directive);
    if (owner == directive_owner_cache.end() || owner->second.empty()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[resolved-include-mutation]: "
              "mutation=%s source=%s directive=%s has no cached resolved "
              "owner\n",
              mutation, source_file->getFileName().c_str(),
              directive->getString().c_str());
      ROSE_ABORT();
    }

    expected = owner->second.size();
    for (const std::string &target_path : owner->second) {
      std::map<std::string, std::set<PreprocessingInfo *>>::iterator target_it =
          resolved_directives.find(target_path);
      if (target_it == resolved_directives.end()) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[resolved-include-mutation]: "
                "mutation=%s source=%s directive=%s missing target path "
                "cache=%s\n",
                mutation, source_file->getFileName().c_str(),
                directive->getString().c_str(), target_path.c_str());
        ROSE_ABORT();
      }
      erased += target_it->second.erase(directive);
      if (target_it->second.empty()) {
        resolved_directives.erase(target_it);
      }
    }
    directive_owner_cache.erase(owner);
  }

  if (erased != expected) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[resolved-include-mutation]: "
            "mutation=%s source=%s directive=%s transformation=%d erased=%zu "
            "expected=%zu\n",
            mutation, source_file->getFileName().c_str(),
            directive->getString().c_str(),
            directive->isTransformation() ? 1 : 0, erased, expected);
    ROSE_ABORT();
  }
}

ResolvedIncludeOwnerCache buildResolvedIncludeOwnerCache(
    const std::map<std::string, std::set<PreprocessingInfo *>> &directives) {
  ResolvedIncludeOwnerCache owner_cache;
  for (const auto &entry : directives) {
    if (entry.first.empty() || entry.second.empty()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[resolved-include-owner-cache]: "
              "target=%s has no exact path or directive owners\n",
              entry.first.c_str());
      ROSE_ABORT();
    }
    for (PreprocessingInfo *directive : entry.second) {
      if (directive == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[resolved-include-owner-cache]: "
                "target=%s contains a null directive owner\n",
                entry.first.c_str());
        ROSE_ABORT();
      }
      const std::pair<std::set<std::string>::iterator, bool> inserted =
          owner_cache[directive].insert(entry.first);
      if (!inserted.second) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[resolved-include-owner-cache]: "
                "directive=%s repeats target=%s\n",
                directive->getString().c_str(), entry.first.c_str());
        ROSE_ABORT();
      }
    }
  }
  return owner_cache;
}

void removeExcludedSourceIncludes(SgSourceFile *generated_source,
                                  SgSourceFile *input_source) {
  if (generated_source == NULL || generated_source->get_file_info() == NULL ||
      generated_source->get_file_info()->get_physical_file_id() < 0 ||
      input_source == NULL || input_source->get_file_info() == NULL ||
      input_source->get_file_info()->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[excluded-source-include]: generated "
            "or input source has no exact primary-file ownership\n");
    ROSE_ABORT();
  }

  const int input_physical_file_id =
      input_source->get_file_info()->get_physical_file_id();
  const int primary_physical_file_id =
      generated_source->get_file_info()->get_physical_file_id();
  if (primary_physical_file_id == input_physical_file_id) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[excluded-source-include]: generated and "
            "input source share physical identity=%d\n",
            primary_physical_file_id);
    ROSE_ABORT();
  }
  const std::map<std::string, bool> direct_include_roles =
      SageInterface::collectDirectIncludeSystemRoles(input_source);
  auto is_excluded_source_include =
      [primary_physical_file_id,
       &direct_include_roles](const PreprocessingInfo *info) {
        const bool primary_source_include =
            isIncludeDirective(info) && !info->isTransformation() &&
            info->get_file_info() != NULL &&
            info->get_file_info()->get_physical_file_id() ==
                primary_physical_file_id;
        if (primary_source_include) {
          validateDirectIncludeSystemRole(info, direct_include_roles);
        }
        return primary_source_include;
      };

  std::map<std::string, std::set<PreprocessingInfo *>> resolved_directives =
      generated_source->get_frontendResolvedIncludeDirectivesMap();
  ResolvedIncludeOwnerCache directive_owner_cache =
      buildResolvedIncludeOwnerCache(resolved_directives);

  RoseAst ast(generated_source);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgLocatedNode *located = isSgLocatedNode(*node);
    if (located == NULL) {
      continue;
    }
    AttachedPreprocessingInfoType *infos =
        located->getAttachedPreprocessingInfo();
    if (infos == NULL) {
      continue;
    }

    AttachedPreprocessingInfoType::iterator info = infos->begin();
    while (info != infos->end()) {
      if (*info == NULL || (*info)->get_file_info() == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[excluded-source-include]: node=%p "
                "has incomplete preprocessing ownership\n",
                static_cast<void *>(located));
        ROSE_ABORT();
      }
      if (is_excluded_source_include(*info)) {
        eraseResolvedFrontendIncludeOwner(generated_source, resolved_directives,
                                          directive_owner_cache, *info,
                                          "exclude-source-include");
        delete *info;
        info = infos->erase(info);
      } else {
        ++info;
      }
    }
  }

  generated_source->set_frontendResolvedIncludeDirectivesMap(
      resolved_directives);

  RoseAst verification(generated_source);
  for (RoseAst::iterator node = verification.begin();
       node != verification.end(); ++node) {
    SgLocatedNode *located = isSgLocatedNode(*node);
    AttachedPreprocessingInfoType *infos =
        located != NULL ? located->getAttachedPreprocessingInfo() : NULL;
    if (infos == NULL) {
      continue;
    }
    for (PreprocessingInfo *info : *infos) {
      if (info == NULL || info->get_file_info() == NULL) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[excluded-source-include]: node=%p "
                "has incomplete preprocessing ownership after purge\n",
                static_cast<void *>(located));
        ROSE_ABORT();
      }
      if (is_excluded_source_include(info)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[excluded-source-include]: node=%p "
                "retains a source include after header exclusion\n",
                static_cast<void *>(located));
        ROSE_ABORT();
      }
    }
  }
}
} // namespace

static void assertFunctionSymbolPresent(SgScopeStatement *scope,
                                        SgFunctionDeclaration *func) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(func != NULL);
  ROSE_ASSERT(Outliner::isValidOutliningScope(scope));

  if (scope->lookup_function_symbol(func->get_name()) != NULL)
    return;

  SgTemplateFunctionDeclaration *template_func =
      isSgTemplateFunctionDeclaration(func);
  if (template_func != NULL) {
    SgTemplateFunctionSymbol *template_sym =
        scope->lookup_template_function_symbol(
            template_func->get_name(), template_func->get_type(),
            &(template_func->get_templateParameters()));
    ROSE_ASSERT(template_sym != NULL);
    return;
  }

  if (SageInterface::is_Fortran_language()) {
    SgFunctionSymbol *symbol = new SgFunctionSymbol(func);
    scope->insert_symbol(func->get_name(), symbol);
    return;
  }

  ROSE_ASSERT(!"Missing function symbol for outlined function");
}

static bool isNonFortranGlobalArray(const SgInitializedName *name) {
  if (name == NULL || SageInterface::is_Fortran_language() ||
      isSgGlobal(name->get_scope()) == NULL) {
    return false;
  }

  return isSgArrayType(
             name->get_type()->stripType(SgType::STRIP_TYPEDEF_TYPE)) != NULL;
}

static std::string extractIncludeKey(const std::string &text) {
  std::string trimmed = Rose::StringUtility::trim(text);
  if (trimmed.empty())
    return trimmed;

  size_t start = trimmed.find_first_of("<\"");
  if (start != std::string::npos) {
    char end_char = (trimmed[start] == '<') ? '>' : '"';
    size_t end = trimmed.find(end_char, start + 1);
    if (end != std::string::npos) {
      return trimmed.substr(start + 1, end - start - 1);
    }
  }

  return trimmed;
}

static bool hasHeaderInCallSiteFile(SgScopeStatement *scope,
                                    const std::string &header) {
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(scope->get_file_info() != NULL);

  SgSourceFile *sourceFile = SageInterface::getEnclosingSourceFile(scope);
  ROSE_ASSERT(sourceFile != NULL);
  SgGlobal *globalScope = sourceFile->get_globalScope();
  ROSE_ASSERT(globalScope != NULL);

  std::vector<SgLocatedNode *> candidates;
  candidates.push_back(globalScope);
  SgDeclarationStatementPtrList &decls = globalScope->get_declarations();
  candidates.insert(candidates.end(), decls.begin(), decls.end());

  Sg_File_Info *sourcePosition = globalScope->get_file_info();
  if (sourcePosition == NULL || sourcePosition->isShared() ||
      sourcePosition->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[dlopen-dependency]: call-site source "
            "file has no exact physical owner\n");
    ROSE_ABORT();
  }
  const int callSitePhysicalId = sourcePosition->get_physical_file_id();
  for (SgLocatedNode *candidate : candidates) {
    if (candidate == NULL || candidate->get_file_info() == NULL)
      continue;

    if (candidate->get_file_info()->isShared() ||
        candidate->get_file_info()->get_physical_file_id() !=
            callSitePhysicalId)
      continue;

    AttachedPreprocessingInfoType *infos =
        candidate->getAttachedPreprocessingInfo();
    if (infos == NULL)
      continue;

    for (PreprocessingInfo *info : *infos) {
      if (info == NULL)
        continue;
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      if (type != PreprocessingInfo::CpreprocessorIncludeDeclaration &&
          type != PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
        continue;
      }
      if (extractIncludeKey(info->getString()) == header)
        return true;
    }
  }

  return false;
}

void Outliner::ensureDlopenSupportHeaderInCallSite(SgScopeStatement *scope) {
  ROSE_ASSERT(scope != NULL);
  if (Outliner::suppress_autotuning_header)
    return;

  if (!hasHeaderInCallSiteFile(scope, Outliner::AUTOTUNING_LIB_HEADER)) {
    SageInterface::insertHeader(Outliner::AUTOTUNING_LIB_HEADER,
                                PreprocessingInfo::after, false, scope);
  }
}

namespace {
enum class DlopenRuntimeFunction {
  find,
  findAndCall,
};

struct DlopenRuntimeSignature {
  SgName name;
  SgType *returnType;
  std::vector<SgType *> parameterTypes;
};

SgType *buildDlopenFunctionPointerType() {
  SgFunctionParameterTypeList *parameters =
      SageBuilder::buildFunctionParameterTypeList();
  parameters->append_argument(SageBuilder::buildPointerType(
      SageBuilder::buildPointerType(SageBuilder::buildVoidType())));
  return SageBuilder::buildPointerType(
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), parameters));
}

DlopenRuntimeSignature
buildDlopenRuntimeSignature(DlopenRuntimeFunction function) {
  if (function == DlopenRuntimeFunction::find) {
    SgType *constCharPointer = SageBuilder::buildPointerType(
        SageBuilder::buildConstType(SageBuilder::buildCharType()));
    return {SgName(Outliner::FIND_FUNCP_DLOPEN),
            buildDlopenFunctionPointerType(),
            {constCharPointer, constCharPointer}};
  }

  return {SgName(Outliner::FIND_AND_CALL_FUNCP_DLOPEN),
          SageBuilder::buildVoidType(),
          {SageBuilder::buildIntType(), SgTypeEllipse::createType()}};
}

void requireDlopenRuntimeSignature(SgFunctionDeclaration *declaration,
                                   const DlopenRuntimeSignature &expected) {
  SgFunctionType *functionType =
      declaration != NULL ? declaration->get_type() : NULL;
  SgFunctionParameterTypeList *parameters =
      functionType != NULL ? functionType->get_argument_list() : NULL;
  const SgTypePtrList *actualTypes =
      parameters != NULL ? &parameters->get_arguments() : NULL;
  if (functionType == NULL || actualTypes == NULL ||
      !SageInterface::isEquivalentType(functionType->get_return_type(),
                                       expected.returnType) ||
      actualTypes->size() != expected.parameterTypes.size()) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[dlopen-runtime-signature]: function=%s "
            "declaration=%p has no exact runtime signature\n",
            expected.name.getString().c_str(),
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }

  for (size_t i = 0; i < actualTypes->size(); ++i) {
    if ((*actualTypes)[i] == NULL || expected.parameterTypes[i] == NULL ||
        !SageInterface::isEquivalentType((*actualTypes)[i],
                                         expected.parameterTypes[i])) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[dlopen-runtime-signature]: "
              "function=%s parameter=%zu actual=%p/%s expected=%p/%s\n",
              expected.name.getString().c_str(), i,
              static_cast<void *>((*actualTypes)[i]),
              (*actualTypes)[i] != NULL
                  ? (*actualTypes)[i]->class_name().c_str()
                  : "<null>",
              static_cast<void *>(expected.parameterTypes[i]),
              expected.parameterTypes[i] != NULL
                  ? expected.parameterTypes[i]->class_name().c_str()
                  : "<null>");
      ROSE_ABORT();
    }
  }
}

SgFunctionSymbol *
requireDlopenRuntimeFunctionSymbol(SgScopeStatement *callSiteScope,
                                   DlopenRuntimeFunction function) {
  SgGlobal *global = SageInterface::getGlobalScope(callSiteScope);
  if (global == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[dlopen-runtime-declaration]: call-site "
            "scope=%p has no global scope\n",
            static_cast<void *>(callSiteScope));
    ROSE_ABORT();
  }

  DlopenRuntimeSignature expected = buildDlopenRuntimeSignature(function);
  SgFunctionSymbol *symbol = global->lookup_function_symbol(expected.name);
  if (symbol == NULL) {
    const SageBuilder::SourcePositionClassification savedMode =
        SageBuilder::getSourcePositionClassificationMode();
    SageBuilder::setSourcePositionClassificationMode(
        SageBuilder::e_sourcePositionTransformation);
    SgFunctionParameterList *parameters =
        SageBuilder::buildFunctionParameterList_nfi();
    for (SgType *parameterType : expected.parameterTypes) {
      SageInterface::appendArg(parameters,
                               SageBuilder::buildSemanticInitializedName(
                                   SgName(), parameterType, NULL));
    }
    SgFunctionDeclaration *declaration =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::sourceLexicalAtTop(
                global),
            expected.name, expected.returnType, parameters, global);
    if (SageBuilder::getSourcePositionClassificationMode() !=
        SageBuilder::e_sourcePositionTransformation) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[dlopen-runtime-declaration]: "
              "function=%s changed its generated-source construction "
              "transaction\n",
              expected.name.getString().c_str());
      ROSE_ABORT();
    }
    SageBuilder::setSourcePositionClassificationMode(savedMode);
    if (declaration == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[dlopen-runtime-declaration]: failed "
              "to publish exact source declaration for function=%s\n",
              expected.name.getString().c_str());
      ROSE_ABORT();
    }
    declaration->get_declarationModifier().get_storageModifier().setExtern();
    if (SageInterface::is_Cxx_language() || is_mixed_C_and_Cxx_language() ||
        is_mixed_Fortran_and_Cxx_language() ||
        is_mixed_Fortran_and_C_and_Cxx_language()) {
      declaration->set_linkage("C");
    }
    Sg_File_Info *source = declaration->get_file_info();
    Sg_File_Info *owner_source = global->get_file_info();
    if (declaration->get_parent() != global ||
        declaration->get_scope() != global ||
        !global->statementExistsInScope(declaration) || source == NULL ||
        owner_source == NULL || source->isShared() ||
        source->get_physical_file_id() < 0 ||
        source->get_physical_file_id() !=
            owner_source->get_physical_file_id() ||
        !source->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[dlopen-runtime-declaration]: "
              "function=%s declaration=%p has no exact generated source "
              "owner in global=%p\n",
              expected.name.getString().c_str(),
              static_cast<void *>(declaration), static_cast<void *>(global));
      ROSE_ABORT();
    }
    symbol =
        isSgFunctionSymbol(declaration->search_for_symbol_from_symbol_table());
  }

  SgFunctionDeclaration *declaration =
      symbol != NULL ? isSgFunctionDeclaration(symbol->get_declaration())
                     : NULL;
  if (symbol == NULL || declaration == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[dlopen-runtime-declaration]: "
            "function=%s has no exact global function symbol\n",
            expected.name.getString().c_str());
    ROSE_ABORT();
  }
  requireDlopenRuntimeSignature(declaration, expected);
  return symbol;
}
} // namespace

static SgBasicBlock *prepareBodySiblingInsertionScope(SgStatement *target) {
  ROSE_ASSERT(target != NULL);
  if (!SageInterface::isBodyStatement(target))
    return NULL;

  // Inserting a sibling beside a single-statement control-flow body first
  // creates a lexical block.  Establish that destination before building any
  // declaration or symbol for it; otherwise the declaration is initially
  // published in the control statement's semantic scope and then physically
  // moved into the new block by insertStatement.
  if (isSgBasicBlock(target) == NULL) {
    SgBasicBlock *block = SageInterface::makeSingleStatementBodyToBlock(target);
    if (block == NULL || target->get_parent() != block ||
        block->get_statements().size() != 1 ||
        block->get_statements().front() != target) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[insertion-scope]: target=%p type=%s "
              "did not produce one exact lexical block owner\n",
              static_cast<void *>(target), target->class_name().c_str());
      ROSE_ABORT();
    }
    return block;
  }

  SgStatement *owner = isSgStatement(target->get_parent());
  if (owner == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[insertion-scope]: block target=%p has no "
            "exact statement body owner\n",
            static_cast<void *>(target));
    ROSE_ABORT();
  }

  const bool if_true =
      isSgIfStmt(owner) != NULL && isSgIfStmt(owner)->get_true_body() == target;
  const bool if_false = isSgIfStmt(owner) != NULL &&
                        isSgIfStmt(owner)->get_false_body() == target;
  SgBasicBlock *shell = SageBuilder::buildBasicBlock(target);
  if (SgIfStmt *statement = isSgIfStmt(owner)) {
    if (if_true)
      statement->set_true_body(shell);
    else if (if_false)
      statement->set_false_body(shell);
    else
      ROSE_ABORT();
  } else if (SgWhileStmt *statement = isSgWhileStmt(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else if (SgDoWhileStmt *statement = isSgDoWhileStmt(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else if (SgForStatement *statement = isSgForStatement(owner)) {
    ROSE_ASSERT(statement->get_loop_body() == target);
    statement->set_loop_body(shell);
  } else if (SgSwitchStatement *statement = isSgSwitchStatement(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else if (SgCaseOptionStmt *statement = isSgCaseOptionStmt(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else if (SgDefaultOptionStmt *statement = isSgDefaultOptionStmt(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else if (SgCatchOptionStmt *statement = isSgCatchOptionStmt(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else if (SgOmpBodyStatement *statement = isSgOmpBodyStatement(owner)) {
    ROSE_ASSERT(statement->get_body() == target);
    statement->set_body(shell);
  } else {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[insertion-scope]: block target=%p has "
            "unsupported body owner=%p/%s\n",
            static_cast<void *>(target), static_cast<void *>(owner),
            owner->class_name().c_str());
    ROSE_ABORT();
  }
  shell->set_parent(owner);
  const std::vector<SgNode *> successors =
      owner->get_traversalSuccessorContainer();
  if (shell->get_parent() != owner || target->get_parent() != shell ||
      shell->get_statements().size() != 1 ||
      shell->get_statements().front() != target ||
      std::count(successors.begin(), successors.end(), shell) != 1) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[insertion-scope]: generated shell=%p did "
            "not publish one exact body ownership transaction\n",
            static_cast<void *>(shell));
    ROSE_ABORT();
  }
  SageInterface::rebindVariableReferencesAfterMove(shell);
  SageInterface::publishGeneratedSubtreeOutputOwner(shell, owner);
  return shell;
}

static SgScopeStatement *scopeForInsertedStatement(SgStatement *target) {
  ROSE_ASSERT(target != NULL);

  if (SgBasicBlock *body_scope = prepareBodySiblingInsertionScope(target))
    return body_scope;

  if (SgScopeStatement *parent_scope =
          isSgScopeStatement(target->get_parent())) {
    if (!SageInterface::hasSimpleChildrenList(parent_scope)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[insertion-scope]: target=%p type=%s "
              "has non-list parent scope=%p/%s\n",
              static_cast<void *>(target), target->class_name().c_str(),
              static_cast<void *>(parent_scope),
              parent_scope->class_name().c_str());
      ROSE_ABORT();
    }
    return parent_scope;
  }

  for (SgNode *current = target->get_parent(); current != NULL;
       current = current->get_parent()) {
    if (SgScopeStatement *scope = isSgScopeStatement(current)) {
      if (!SageInterface::hasSimpleChildrenList(scope)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[insertion-scope]: target=%p type=%s "
                "resolves to non-list scope=%p/%s\n",
                static_cast<void *>(target), target->class_name().c_str(),
                static_cast<void *>(scope), scope->class_name().c_str());
        ROSE_ABORT();
      }
      return scope;
    }
  }

  fprintf(stderr,
          "REX_OUTLINER_INVARIANT[insertion-scope]: target=%p type=%s has no "
          "exact lexical insertion scope\n",
          static_cast<void *>(target), target->class_name().c_str());
  ROSE_ABORT();
}

static void validateGeneratedVariableDeclaration(SgVariableDeclaration *decl,
                                                 SgScopeStatement *scope,
                                                 bool published) {
  ROSE_ASSERT(decl != NULL);
  ROSE_ASSERT(scope != NULL);

  SgNode *expectedParent = published ? static_cast<SgNode *>(scope) : NULL;
  SgScopeStatement *publishedScope = published ? decl->get_scope() : NULL;
  if ((published && publishedScope != scope) ||
      decl->get_parent() != expectedParent ||
      scope->get_symbol_table() == NULL ||
      scope->get_symbol_table()->get_parent() != scope) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[generated-variable-owner]: declaration=%p "
            "scope=%p parent=%p expected-parent=%p semantic-scope=%p\n",
            static_cast<void *>(decl), static_cast<void *>(scope),
            static_cast<void *>(decl->get_parent()),
            static_cast<void *>(expectedParent),
            static_cast<void *>(publishedScope));
    ROSE_ABORT();
  }

  for (SgInitializedName *name : decl->get_variables()) {
    if (name == NULL || name->get_parent() != decl ||
        name->get_scope() != scope) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-variable-name-owner]: "
              "declaration=%p name=%p parent=%p scope=%p expected-scope=%p\n",
              static_cast<void *>(decl), static_cast<void *>(name),
              static_cast<void *>(name != NULL ? name->get_parent() : NULL),
              static_cast<void *>(name != NULL ? name->get_scope() : NULL),
              static_cast<void *>(scope));
      ROSE_ABORT();
    }

    if (!name->get_name().is_null() && !name->get_name().getString().empty()) {
      SgVariableSymbol *symbol =
          scope->lookup_variable_symbol(name->get_name());
      if (symbol == NULL || symbol->get_declaration() != name ||
          symbol->get_parent() != scope->get_symbol_table()) {
        fprintf(
            stderr,
            "REX_OUTLINER_INVARIANT[generated-variable-symbol]: "
            "declaration=%p name='%s' symbol=%p symbol-parent=%p "
            "expected-table=%p\n",
            static_cast<void *>(decl), name->get_name().str(),
            static_cast<void *>(symbol),
            static_cast<void *>(symbol != NULL ? symbol->get_parent() : NULL),
            static_cast<void *>(scope->get_symbol_table()));
        ROSE_ABORT();
      }
    }
  }
}

static void
collectIncludeKeysFromInfo(const AttachedPreprocessingInfoType *infos,
                           std::set<std::string> &keys) {
  if (infos == NULL)
    return;

  for (AttachedPreprocessingInfoType::const_iterator it = infos->begin();
       it != infos->end(); ++it) {
    const PreprocessingInfo *info = *it;
    if (info == NULL)
      continue;

    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    if (type != PreprocessingInfo::CpreprocessorIncludeDeclaration &&
        type != PreprocessingInfo::CpreprocessorIncludeNextDeclaration) {
      continue;
    }

    std::string key = extractIncludeKey(info->getString());
    if (!key.empty())
      keys.insert(key);
  }
}

static void collectIncludeKeysFromStatement(SgStatement *stmt,
                                            std::set<std::string> &keys) {
  if (stmt == NULL)
    return;

  if (SgIncludeDirectiveStatement *include_stmt =
          isSgIncludeDirectiveStatement(stmt)) {
    std::string directive = include_stmt->get_directiveString();
    if (directive.empty()) {
      directive = include_stmt->get_name_used_in_include_directive();
    }
    std::string key = extractIncludeKey(directive);
    if (!key.empty())
      keys.insert(key);
  }

  collectIncludeKeysFromInfo(stmt->getAttachedPreprocessingInfo(), keys);
}

static void validateFriendClassDeclarations(SgProject *project) {
  if (project == NULL)
    return;

  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDeclaration)) {
    SgClassDeclaration *decl = isSgClassDeclaration(node);
    if (decl == NULL)
      continue;
    if (decl->get_declarationModifier().isFriend() == false)
      continue;

    SgClassDefinition *parent_def = isSgClassDefinition(decl->get_parent());
    if (parent_def == NULL)
      continue;

    if (!decl->isForward() || decl->get_definition() != nullptr ||
        decl->get_scope() == nullptr ||
        decl->get_firstNondefiningDeclaration() == nullptr) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[friend-class-declaration]: friend=%p "
              "name=%s lexical_owner=%p semantic_scope=%p must already be an "
              "exact nondefining declaration before outlining\n",
              static_cast<void *>(decl), decl->get_name().getString().c_str(),
              static_cast<void *>(parent_def),
              static_cast<void *>(decl->get_scope()));
      ROSE_ABORT();
    }
  }
}

static void dedupeIncludeDirectives(SgGlobal *glob_scope) {
  if (glob_scope == NULL || glob_scope->get_file_info() == NULL ||
      glob_scope->get_file_info()->isShared() ||
      glob_scope->get_file_info()->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[include-deduplication-owner]: output "
            "global scope has no exact physical file identity\n");
    ROSE_ABORT();
  }
  const int outputPhysicalFileId =
      glob_scope->get_file_info()->get_physical_file_id();
  SgSourceFile *sourceFile = SageInterface::getEnclosingSourceFile(glob_scope);
  if (sourceFile == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[include-deduplication-owner]: output "
            "global scope has no exact source-file owner\n");
    ROSE_ABORT();
  }
  std::map<std::string, std::set<PreprocessingInfo *>> resolved_directives =
      sourceFile->get_frontendResolvedIncludeDirectivesMap();
  ResolvedIncludeOwnerCache directive_owner_cache =
      buildResolvedIncludeOwnerCache(resolved_directives);

  std::set<std::string> seen;

  auto prune_info = [&](SgStatement *stmt) {
    if (stmt == NULL)
      return;
    AttachedPreprocessingInfoType *infos = stmt->getAttachedPreprocessingInfo();
    if (infos == NULL)
      return;

    AttachedPreprocessingInfoType filtered;
    AttachedPreprocessingInfoType removed;
    filtered.reserve(infos->size());
    for (AttachedPreprocessingInfoType::iterator it = infos->begin();
         it != infos->end(); ++it) {
      PreprocessingInfo *info = *it;
      if (info == NULL)
        continue;
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      bool is_include =
          (type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration);
      if (is_include) {
        Sg_File_Info *includeInfo = info->get_file_info();
        if (includeInfo == NULL || includeInfo->isShared() ||
            includeInfo->get_physical_file_id() < 0) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[include-deduplication-owner]: "
                  "directive=%s has no exact physical file identity\n",
                  info->getString().c_str());
          ROSE_ABORT();
        }
        if (includeInfo->get_physical_file_id() != outputPhysicalFileId) {
          filtered.push_back(info);
          continue;
        }
        std::string key = extractIncludeKey(info->getString());
        if (!key.empty() && seen.count(key) != 0) {
          removed.push_back(info);
          continue;
        }
        if (!key.empty())
          seen.insert(key);
      }
      filtered.push_back(info);
    }
    infos->swap(filtered);
    for (PreprocessingInfo *info : removed) {
      eraseResolvedFrontendIncludeOwner(sourceFile, resolved_directives,
                                        directive_owner_cache, info,
                                        "deduplicate-source-include");
      delete info;
    }
  };

  prune_info(glob_scope);

  SgDeclarationStatementPtrList &decls = glob_scope->get_declarations();
  for (SgDeclarationStatementPtrList::iterator it = decls.begin();
       it != decls.end();) {
    SgDeclarationStatement *decl = *it;
    if (SgIncludeDirectiveStatement *include_stmt =
            isSgIncludeDirectiveStatement(decl)) {
      Sg_File_Info *includeInfo = include_stmt->get_file_info();
      if (includeInfo == NULL || includeInfo->isShared() ||
          includeInfo->get_physical_file_id() < 0) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[include-deduplication-owner]: typed "
                "include statement has no exact physical file identity\n");
        ROSE_ABORT();
      }
      if (includeInfo->get_physical_file_id() != outputPhysicalFileId) {
        ++it;
        continue;
      }
      std::string key = extractIncludeKey(include_stmt->get_directiveString());
      if (key.empty()) {
        key = extractIncludeKey(
            include_stmt->get_name_used_in_include_directive());
      }
      if (!key.empty() && seen.count(key) != 0) {
        ++it;
        SageInterface::removeStatement(include_stmt);
        continue;
      }
      if (!key.empty())
        seen.insert(key);
    }

    prune_info(decl);
    ++it;
  }

  sourceFile->set_frontendResolvedIncludeDirectivesMap(resolved_directives);
}

// ! create a struct to contain data members for variables to be passed as
// parameters A wrapper struct for variables passed to the outlined function
// Each variable (e.g a) has two choices
//   1. store the value of a:  the same type representation in the struct
//   2. store the address of a:  pointer type of a
SgType *Outliner::captureValueType(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  SgType *unqualified =
      type->stripType(SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE);
  if (auto *reference = isSgReferenceType(unqualified))
    return reference->get_base_type();
  if (auto *reference = isSgRvalueReferenceType(unqualified))
    return reference->get_base_type();
  return type;
}

SgClassDeclaration *Outliner::generateParameterStructureDeclaration(
    SgBasicBlock *s, // the outlining target
    const std::string
        &func_name_str, // the name for the outlined function, we generate the
                        // name of struct based on this.
    const ASTtools::VarSymSet_t &syms, // variables to be passed as parameters
    ASTtools::VarSymSet_t &symsUsingAddress, // variables whose addresses are
                                             // stored into the struct
    SgScopeStatement *func_scope) // the scope of the outlined function, could
                                  // be different from s's global scope
{
  SgClassDeclaration *result = NULL;
  // no need to generate the declaration if no variables are to be passed
  if (syms.empty())
    return result;

  ROSE_ASSERT(s != NULL);
  ROSE_ASSERT(func_scope != NULL);
  // this declaration will later on be inserted right before the outlining
  // target calling the outlined function
  ROSE_ASSERT(isSgGlobal(func_scope) != NULL);
  string decl_name = func_name_str + "_data";

  SgNode *global_scoped_ancestor = getEnclosingFunctionDefinition(s, false);
  ROSE_ASSERT(global_scoped_ancestor != NULL);
  while (!isSgGlobal(global_scoped_ancestor->get_parent())) {
    global_scoped_ancestor = global_scoped_ancestor->get_parent();
    ROSE_ASSERT(global_scoped_ancestor != NULL);
  }
  ROSE_ASSERT(isSgStatement(global_scoped_ancestor));
  ROSE_ASSERT(global_scoped_ancestor->get_parent() == func_scope);
  result = buildStructDeclaration(
      declaration_ownership::transformationDetached(), decl_name, func_scope);
  insertStatementBefore(isSgStatement(global_scoped_ancestor), result);
  movePreprocessingInfo(isSgStatement(global_scoped_ancestor), result);

  // insert member variable declarations to it
  SgClassDefinition *def = result->get_definition();
  ROSE_ASSERT(def != NULL);
  SgScopeStatement *def_scope = isSgScopeStatement(def);
  ROSE_ASSERT(def_scope != NULL);
  for (ASTtools::VarSymSet_t::const_iterator i = syms.begin(); i != syms.end();
       ++i) {
    const SgInitializedName *i_name = (*i)->get_declaration();
    ROSE_ASSERT(i_name);
    const SgVariableSymbol *i_symbol =
        isSgVariableSymbol(i_name->get_symbol_from_symbol_table());
    ROSE_ASSERT(i_symbol != NULL);
    string member_name = i_name->get_name().str();
    SgType *member_type = i_name->get_type();
    if (symsUsingAddress.find(i_symbol) == symsUsingAddress.end())
      member_type = captureValueType(member_type);
    // use pointer type or its original type?
    SgType *non_typef_type = member_type->stripType(SgType::STRIP_TYPEDEF_TYPE);
    if (symsUsingAddress.find(i_symbol) != symsUsingAddress.end()) {
      member_name = member_name + "_p";

      // We use void* instead of type* to ease the handling of C++ class
      // pointers wrapped into the data structure.
      if (isSgArrayType(non_typef_type) != NULL) {
        member_type = buildPointerType(member_type);
      } else if (isSgArrayType(
                     non_typef_type->stripType(SgType::STRIP_POINTER_TYPE))) {
        member_type = buildPointerType(non_typef_type);
      } else { // Scalars, Pointers, Structures
        member_type = buildPointerType(buildVoidType());
      }
    }
    SgVariableDeclaration *member_decl =
        buildVariableDeclaration(member_name, member_type, NULL, def_scope);
    appendStatement(member_decl, def_scope);
  }

  return result;
}

//!  A helper function to decide if some variables need to be restored from
//!  their clones in the end of the outlined function
// This is needed to support variable cloning
// Input are:
//      All the variables
//      read-only variables
//      live-out variables
//
// The output is restoreVars, which is isWritten && isLiveOut --> !isRead &&
// isLiveOut
static void calculateVariableRestorationSet(
    const ASTtools::VarSymSet_t &syms,
    const std::set<SgInitializedName *> &readOnlyVars,
    const std::set<SgInitializedName *> &liveOutVars,
    std::set<SgInitializedName *> &restoreVars) {
  for (ASTtools::VarSymSet_t::const_reverse_iterator i = syms.rbegin();
       i != syms.rend(); ++i) {
    ROSE_ASSERT(*i != NULL);
    SgInitializedName *i_name = (*i)->get_declaration();
    // conservatively consider them as all live out if no liveness analysis is
    // enabled,
    bool isLiveOut = true;
    if (Outliner::enable_liveness)
      if (liveOutVars.find(i_name) == liveOutVars.end())
        isLiveOut = false;

    // generate restoring statements for written and liveOut variables:
    //  isWritten && isLiveOut --> !isRead && isLiveOut --> (findRead==NULL &&
    //  findLiveOut!=NULL)
    // must compare to the original init name (i_name), not the local copy
    // (local_var_init)
    if (readOnlyVars.find(i_name) == readOnlyVars.end() &&
        isLiveOut) // variables not in read-only set have to be restored
      restoreVars.insert(i_name);
  }

  if (Outliner::enable_debug) {
    cout << "Executing calculateVariableRestorationSet()....." << endl;
    cout << "Found " << restoreVars.size()
         << " symbols which must be restored in the end of the outlined "
            "function:";
    for (std::set<SgInitializedName *>::const_iterator iter =
             restoreVars.begin();
         iter != restoreVars.end(); iter++)
      cout << (*iter)->get_name().getString() << " ";
    cout << endl;
  }
}

//! A helper function to decide for the classic outlining, if a variable should
//! be passed using its original type (a) or its pointer type (&a)
// For simplicity, we assuming Pass-by-reference (using AddressOf()) =
// all_variables - read_only_variables So all variables which are written will
// use addressOf operation to be passed to the outlined function
// TODO: add more sophisticated logic, C++ reference, C array, Fortran variable
// etc.
static void calculateVariableUsingAddressOf(
    const ASTtools::VarSymSet_t &syms,
    const std::set<SgInitializedName *> readOnlyVars,
    ASTtools::VarSymSet_t &addressOfVarSyms) {
  for (ASTtools::VarSymSet_t::const_reverse_iterator i = syms.rbegin();
       i != syms.rend(); ++i) {
    // Basic information about the variable to be passed into the outlined
    // function Variable symbol name
    SgInitializedName *i_name = (*i)->get_declaration();
    if (readOnlyVars.find(i_name) ==
        readOnlyVars.end()) // not readonly ==> being written ==> use addressOf
                            // to be passed to the outlined function
      addressOfVarSyms.insert(*i);
  } // end for
}

/**
 * Major work of outlining is done here
 *  Preparations: variable collection
 *  Generate outlined function
 *  Replace outlining target with a function call
 *  Append dependent declarations,headers to new file if needed
 */
Outliner::Result
Outliner::outlineBlock(SgBasicBlock *s, const string &func_name_str,
                       const vector<PreprocessingInfo> &original_directives) {

  //---------step 1. Preparations-----------------------------------
  // new file, cut preprocessing information, collect variables
  SgSourceFile *original_source_file = SageInterface::getEnclosingSourceFile(s);
  ASSERT_not_null(original_source_file);
  SgProject *project = SageInterface::getProject(original_source_file);
  ASSERT_not_null(project);
  int original_physical_file_id = -1;
  if (s != NULL && s->get_file_info() != NULL) {
    original_physical_file_id = s->get_file_info()->get_physical_file_id();
  }
  if (original_physical_file_id < 0 && original_source_file != NULL &&
      original_source_file->get_file_info() != NULL) {
    original_physical_file_id =
        original_source_file->get_file_info()->get_physical_file_id();
  }

  // Generate a new source file for the outlined function, if requested
  SgSourceFile *new_file = NULL;
  if (Outliner::useNewFile) {
    if (copy_origFile) // single new file
      new_file = getLibSourceFile(s);
    else
      new_file = generateNewSourceFile(s, func_name_str);
    ROSE_ASSERT(new_file);
    if (use_dlopen) {
      // Both copied and output-only dlopen sources are compiled separately as
      // shared libraries. Unparse them with the project, but do not compile
      // or link their objects into the original program. Static outlining
      // continues to compile and link its generated source normally.
      new_file->set_skipfinalCompileStep(true);
    }
  }

  // Save some preprocessing information for later restoration.
  AttachedPreprocessingInfoType ppi_before, ppi_after;
  ASTtools::cutPreprocInfo(s, PreprocessingInfo::before, ppi_before);
  ASTtools::cutPreprocInfo(s, PreprocessingInfo::after, ppi_after);

  // Determine variables to be passed to outlined routine.
  // ----------------------------------------------------------
  // Also collect symbols which must use pointer dereferencing if replaced
  // during outlining
  ASTtools::VarSymSet_t syms, pdSyms;
  collectVars(s, s, syms);
  for (ASTtools::VarSymSet_t::iterator it = syms.begin(); it != syms.end();
       it++)
    ROSE_ASSERT(*it != NULL);

  // prepare necessary analysis to optimize the outlining
  //-----------------------------------------------------------------
  std::set<SgInitializedName *> readOnlyVars;
  std::set<SgInitializedName *> liveIns, liveOuts;
  // Collect read-only variables of the outlining target

  // Determine variables to be replaced by temp copy or pointer dereferencing.
  if (Outliner::temp_variable || Outliner::enable_classic ||
      Outliner::useStructureWrapper) {
    SageInterface::collectReadOnlyVariables(s, readOnlyVars);
    // Collect use by address plus non-assignable variables
    // They must be passed by reference if they need to be passed as parameters
    // TODO: this is not accurate: array variables are not assignable , but they
    // should not using pointer dereferencing
    ASTtools::collectPointerDereferencingVarSyms(s, pdSyms);

    // liveness analysis
    SgStatement *firstStmt = (s->get_statements())[0];
    if (isSgForStatement(firstStmt) && enable_liveness) {
      LivenessAnalysis *liv = SageInterface::call_liveness_analysis(project);
      SageInterface::getLiveVariables(liv, isSgForStatement(firstStmt), liveIns,
                                      liveOuts);
    }

    if (Outliner::enable_debug) {
      cout << "Transform.cc Outliner::outlineBlock() -----Found "
           << readOnlyVars.size() << " read only variables..:";
      for (std::set<SgInitializedName *>::const_iterator iter =
               readOnlyVars.begin();
           iter != readOnlyVars.end(); iter++)
        cout << " " << (*iter)->get_name().getString() << " ";
      cout << endl;

      cout << "Outliner::outlineBlock() -----Found " << pdSyms.size()
           << " varaibles to be replaced as pointer dereferencing variables..:";
      for (ASTtools::VarSymSet_t::const_iterator iter = pdSyms.begin();
           iter != pdSyms.end(); iter++)
        cout << " " << (*iter)->get_name().getString() << " ";
      cout << endl;

      cout << "Outliner::outlineBlock() -----Found " << liveOuts.size()
           << " live out variables..:";
      for (std::set<SgInitializedName *>::const_iterator iter =
               liveOuts.begin();
           iter != liveOuts.end(); iter++)
        cout << " " << (*iter)->get_name().getString() << " ";
      cout << endl;
    }
  }

  // Insert outlined function.
  // grab target scope first
  SgGlobal *glob_scope =
      const_cast<SgGlobal *>(SageInterface::getGlobalScope(s));
  SgScopeStatement *outlining_scope = glob_scope;
  if (!Outliner::useNewFile && SageInterface::is_Fortran_language()) {
    if (SgModuleStatement *module =
            SageInterface::getEnclosingModuleStatement(s)) {
      if (SgClassDefinition *module_def = module->get_definition()) {
        outlining_scope = module_def;
      }
    }
  }

  std::vector<SgDeclarationStatement *> dependent_declarations;
  if (Outliner::useNewFile) {
    dependent_declarations = SageInterface::collectDependentDeclarations(
        s, exclude_headers, original_physical_file_id);
  }

  //-------Step 2. Generate outlined
  // function------------------------------------
  // Generate a structure declaration if useStructureWrapper is set
  // A variable of the struct type will later used to wrap function parameters
  SgClassDeclaration *struct_decl = NULL;
  if (Outliner::useStructureWrapper) {
    struct_decl = generateParameterStructureDeclaration(s, func_name_str, syms,
                                                        pdSyms, glob_scope);
    ROSE_ASSERT(struct_decl != NULL);
  }

  // generate the function and its prototypes if necessary
  std::set<SgInitializedName *> restoreVars;
  calculateVariableRestorationSet(syms, readOnlyVars, liveOuts, restoreVars);

  if (Outliner::enable_classic) // merge readOnlyVars and pdSyms into pdSyms,
                                // only when no wrapper parameter is used &&
                                // enable_classic is on
  { // Liao 1/30/2013. I have to use this dirty trick to consolidate pdSyms and
    // readOnlyVars This is necessary to separate analysis from transformation
    // so the outliner's API functions can be more predictable.
    // TODO better handling later on for default case (no flags are turned on at
    // all)
    pdSyms.clear();
    calculateVariableUsingAddressOf(syms, readOnlyVars, pdSyms);
  }

  OutlinedLocalTypeTemplatePlan local_type_template_plan;
  OutlinedFunctionParameterBindings parameter_bindings;
  SgFunctionDeclaration *func = generateFunction(
      s, func_name_str, syms, pdSyms, restoreVars, struct_decl, outlining_scope,
      local_type_template_plan, parameter_bindings);

  ROSE_ASSERT(func != NULL);
  if (!Outliner::isFortranModuleDefinitionScope(outlining_scope)) {
    assertFunctionSymbolPresent(outlining_scope, func);
  }

  ROSE_ASSERT(s->get_statements().empty() == true);

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  //-----------Step 3. Insert the outlined function -------------
  SgFunctionDeclaration *source_call_declaration = NULL;
  DeferredTransformation headerFileTransformation = insert(
      func, outlining_scope, s, source_call_declaration); // Outliner::insert()
  assertFunctionSymbolPresent(outlining_scope, func);

  // Some comments and #include directives may be attached after the global
  // scope for an otherwise empty input file. We must move them to be attached
  // to the first prototype's before location. Otherwise, the #include directive
  // will show up in the end of the resulting file.
  SgStatement *firstStmt = getFirstStatement(glob_scope);
  SgFunctionDeclaration *first_func_decl = isSgFunctionDeclaration(firstStmt);
  if (first_func_decl != NULL) // we expect that the first statement is a
                               // prototype func of an outlined function.
  {
    std::set<std::string> include_keys;
    collectIncludeKeysFromStatement(glob_scope, include_keys);
    const SgDeclarationStatementPtrList &global_decls =
        glob_scope->get_declarations();
    for (SgDeclarationStatementPtrList::const_iterator it =
             global_decls.begin();
         it != global_decls.end(); ++it) {
      collectIncludeKeysFromStatement(*it, include_keys);
    }
    collectIncludeKeysFromStatement(first_func_decl, include_keys);

    AttachedPreprocessingInfoType save_buf;
    cutPreprocessingInfo(glob_scope, PreprocessingInfo::after, save_buf);
    if (!save_buf.empty()) {
      AttachedPreprocessingInfoType filtered;
      filtered.reserve(save_buf.size());
      for (AttachedPreprocessingInfoType::iterator i = save_buf.begin();
           i != save_buf.end(); ++i) {
        PreprocessingInfo *info = *i;
        if (info == NULL)
          continue;
        PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
        bool is_include =
            (type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
             type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration);
        if (is_include) {
          std::string key = extractIncludeKey(info->getString());
          if (!key.empty() && include_keys.count(key) != 0) {
            continue;
          }
          if (!key.empty())
            include_keys.insert(key);
        }
        filtered.push_back(info);
      }
      save_buf.swap(filtered);
    }

    if (!save_buf.empty()) {
      pastePreprocessingInfo(first_func_decl, PreprocessingInfo::before,
                             save_buf);
    }
  }

  assertFunctionSymbolPresent(glob_scope, func);
  //
  // Retest this...
  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  //-----------Step 4. Replace the outlining target with a function
  // call-------------

  // Prepare the parameter of the function call,
  // Generate packing statements, insert them into the beginning of the target s
  std::string wrapper_name;
  // two ways to pack parameters: an array of pointers v.s. A structure
  int sym_count = syms.size(); // using symbol count to decide if we need to
                               // pass parameters at all
  // We don't need to generate packing statements for parameters if using the
  // simple dlopen call convention.
  if (!use_dlopen_simple && (useParameterWrapper || useStructureWrapper)) {
    wrapper_name = generatePackingStatements(s, syms, pdSyms, struct_decl);
  }

  // Generate a call to the outlined function.
  SgScopeStatement *p_scope = scopeForInsertedStatement(s);
  ROSE_ASSERT(p_scope);

  // Retest this...
  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  SgStatement *func_call = NULL;
  SgVarRefExp *wrapper_exp = NULL;
  if (use_dlopen) {
    string lib_name;
    if (Outliner::copy_origFile) {
      string lib_file_base_name = StringUtility::stripFileSuffixFromFileName(
          StringUtility::stripPathFromFileName(generateLibSourceFileName(s)));
      lib_name = output_path + "/" + lib_file_base_name + ".so";
      if (MASTER_SHARED_LIB_NAME.size() == 0) {
        string lib_file_base_name = StringUtility::stripFileSuffixFromFileName(
            StringUtility::stripPathFromFileName(generateLibSourceFileName(s)));
        lib_name = output_path + "/" + lib_file_base_name + ".so";
      } else
        lib_name = output_path + "/" + MASTER_SHARED_LIB_NAME;
    } else
      lib_name = output_path + "/" + func_name_str + ".so";

    // Using the simple call convention: a single function call implementing all
    // logic of the call site:
    //    // hide function pointer declaration, done within Insert.cc : 1533 if
    //    (use_dlopen) ..
    //     check shared lib existence
    //     packing parameter addresses into array of pointers
    //     find the outlined function's function pointer
    //     call the outlined function
    if (use_dlopen_simple) {
      // findAndCallFunctionUsingDlopen (3, "OUT_1_ft_cfftz_omp_47",
      // "/tmp/rose_ft_cfftz_omp_lib.so", (void *)(&a));
      int parameter_count = syms.size();
      SgExprListExp *arg_list = buildExprListExp(
          buildIntVal(parameter_count + 2), buildStringVal(func_name_str),
          buildStringVal(lib_name));

      for (ASTtools::VarSymSet_t::iterator i = syms.begin(); i != syms.end();
           ++i) {
        SgVarRefExp *rhsvar = buildVarRefExp((*i)->get_declaration(), p_scope);
        arg_list->append_expression(buildCastExp(
            buildAddressOfOp(
                rhsvar,
                SageBuilder::buildAddressOfResultType(rhsvar->get_type()),
                SgAddressOfOp::e_address_operand_object),
            buildPointerType(buildVoidType()), SgCastExp::e_C_style_cast));
      }

      SgFunctionSymbol *runtimeFunction = requireDlopenRuntimeFunctionSymbol(
          p_scope, DlopenRuntimeFunction::findAndCall);
      func_call =
          buildExprStatement(buildFunctionCallExp(runtimeFunction, arg_list));
    } else {
      // if dlopen() is used, insert a lib call to find the function pointer
      // from a shared lib file e.g. OUT__2__8072__p =
      // findFunctionUsingDlopen("OUT__2__8072__", "OUT__2__8072__.so"); build
      // the return type of the lib call
      SgFunctionParameterTypeList *tlist = buildFunctionParameterTypeList();
      tlist->append_argument(
          buildPointerType(buildPointerType(buildVoidType())));

      SgFunctionType *outlinedFunctionType =
          buildFunctionType(buildVoidType(), tlist);
      // build the argument list
      // the new option copy_origFile will ask the outliner to generate
      // rose_input_lib.c/cxx and compile to a .so later
      // e.g.
      // OUT_1_test_26_2020_0p =
      // findFunctionUsingDlopen("OUT_1_test_26_2020_0","test_26_2020/rose_test_26_2020_lib.so");
      SgExprListExp *arg_list = buildExprListExp(buildStringVal(func_name_str),
                                                 buildStringVal(lib_name));
      SgFunctionSymbol *runtimeFunction = requireDlopenRuntimeFunctionSymbol(
          p_scope, DlopenRuntimeFunction::find);
      SgFunctionCallExp *dlopen_call =
          buildFunctionCallExp(runtimeFunction, arg_list);
      SgExprStatement *assign_stmt = buildAssignStatement(
          buildVarRefExp(func_name_str + "p", p_scope), dlopen_call);
      SageInterface::insertStatementBefore(s, assign_stmt);
      // Generate a function call using the func pointer
      // e.g. (*OUT__2__8888__p)(__out_argv2__1527__);
      SgExprListExp *exp_list_exp = SageBuilder::buildExprListExp();
      if (sym_count > 0) // passing wrapper parameter only if non-zero variables
                         // are used in the outlined function
      {
        SgVariableSymbol *wrapper_symbol =
            p_scope->lookup_variable_symbol(SgName(wrapper_name));
        if (wrapper_symbol == nullptr ||
            wrapper_symbol->get_parent() != p_scope->get_symbol_table() ||
            wrapper_symbol->get_declaration() == nullptr ||
            SageInterface::containsUnknownType(
                wrapper_symbol->get_declaration()->get_type())) {
          fprintf(stderr,
                  "REX_OUTLINER_INVARIANT[wrapper-call-symbol]: name=%s "
                  "scope=%p has no exact typed local variable symbol\n",
                  wrapper_name.c_str(), static_cast<void *>(p_scope));
          ROSE_ABORT();
        }
        wrapper_exp = buildVarRefExp(wrapper_symbol);
        appendExpression(exp_list_exp, wrapper_exp);
      } else
        appendExpression(exp_list_exp,
                         buildIntVal(0)); // NULL pointer as parameter
      SgVarRefExp *function_pointer =
          buildVarRefExp(func_name_str + "p", p_scope);
      func_call = buildFunctionCallStmt(
          buildPointerDerefExp(
              function_pointer,
              SageInterface::getElementType(function_pointer->get_type())),
          outlinedFunctionType->get_return_type(), exp_list_exp);
    }
  } else // regular function call for other cases
  {
    func_call = generateCall(source_call_declaration, syms, readOnlyVars,
                             wrapper_name, p_scope, local_type_template_plan);
  }

  ROSE_ASSERT(func_call != NULL);
  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  SageInterface::replaceStatement(s, func_call);
  if (use_dlopen) {
    ensureDlopenSupportHeaderInCallSite(p_scope);
  }

  ROSE_ASSERT(s != NULL);
  ROSE_ASSERT(s->get_statements().empty() == true);

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  // Restore preprocessing information.
  ASTtools::moveInsidePreprocInfo(s, func->get_definition()->get_body());
  ASTtools::pastePreprocInfoFront(ppi_before, func_call);
  ASTtools::pastePreprocInfoBack(ppi_after, func_call);

  SageInterface::rebindVariableReferencesAfterMove(p_scope);

  //-----------handle dependent declarations, headers if new file is
  // generated-------------
  if (new_file) {
    SageInterface::rebindVariableReferencesAfterMove(new_file);
  }

  if (wrapper_exp)
    ROSE_ASSERT(wrapper_exp->get_symbol()->get_declaration() != NULL);

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  ROSE_ASSERT(s != NULL);
  ROSE_ASSERT(s->get_statements().empty() == true);
  if (useNewFile == true) {
    ROSE_ASSERT(func->get_firstNondefiningDeclaration() != NULL);
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func) ==
                SageInterface::getEnclosingSourceFile(
                    func->get_firstNondefiningDeclaration()));
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func->get_scope()) ==
                SageInterface::getEnclosingSourceFile(
                    func->get_firstNondefiningDeclaration()));

    glob_scope = new_file->get_globalScope();
    SageInterface::appendStatementWithDependentDeclaration(
        func, glob_scope, source_call_declaration, exclude_headers,
        original_directives, dependent_declarations, original_source_file);
  }

  if (new_file != NULL) {
    // Dependency reconstruction can deep-copy a declaration containing the
    // consumed marker after getLibSourceFile() purged the copied
    // translation unit.  Enforce the producer postcondition again after all
    // generated declarations have been attached.
    removeConsumedOutlineDirectives(new_file);
  }

  // Run the AST fixup on the project to avoid per-file duplication.
  AstPostProcessing(project);
  validateFriendClassDeclarations(project);

  dedupeIncludeDirectives(glob_scope);

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());

  return Result(func, func_call, new_file, headerFileTransformation);
}

/**
 *  \brief Initializes packing statements for array types
 *  The function also skips typedef types to get the real type
 *
 *  \param lhs Left-hand side of the assignment
 *  \param rhs Right-hand side of the assignment
 *  \param target OpenMP pragme before which we have to place the packing
 * statments
 *
 *  Example:
 *    Input code:
 *        int a;
 *        int b[10];
 *        int c[10];
 *        int * d = ( int * ) malloc( sizeof( int ) * 10 );
 *        int i;
 *        #pragma omp parallel for firstprivate(c)
 *        {
 *            for(i=0;i<10;i++) {
 *                b[i] = a;
 *                c[i] = a;
 *                d[i] = a;
 *            }
 *        }
 *
 *    Outlined parameters struct:
 *        struct OUT__1__7768___data {
 *            void *a_p;
 *            int (*b_p)[10UL];
 *            int c[10UL];
 *            void *d_p;
 *        };
 *
 *    Packing statements:
 *        struct OUT__1__7768___data __out_argv;
 *        __out_argv.a_p = ((void *)(&a));                  -> shared scalar
 *        __out_argv.b_p = ((void *)(&b));                  -> shared static
 * array int __i0__; for (__i0__ = 0; __i0__ < 10UL; __i0__++)         ->
 * firstprivate array
 *            __out_argv.c[__i0__] = c[__i0__];
 *        __out_argv.d_p = ((void *)(&d));                  -> shared dynamic
 * array
 */
static SgStatement *build_array_packing_statement(SgExpression *lhs,
                                                  SgExpression *&rhs,
                                                  SgStatement *target) {
  SgScopeStatement *scope = scopeForInsertedStatement(target);
  ROSE_ASSERT(scope != NULL);

  // Loop initializer
  std::string loop_index_name =
      SageInterface::generateUniqueVariableName(scope, "i");
  SgVariableDeclaration *loop_index = buildVariableDeclaration(
      loop_index_name, buildIntType(), NULL /* initializer */, scope);
  validateGeneratedVariableDeclaration(loop_index, scope, false);
  SageInterface::insertStatementBefore(target, loop_index);
  validateGeneratedVariableDeclaration(loop_index, scope, true);
  SgStatement *loop_init = buildAssignStatement(
      buildVarRefExp(loop_index_name, scope), buildIntVal(0));

  // Get the real type of the LHS
  SgType *lhs_type = lhs->get_type()->stripType(SgType::STRIP_TYPEDEF_TYPE);
  ROSE_ASSERT(isSgArrayType(lhs_type));

  // Loop test
  SgStatement *loop_test = buildExprStatement(buildLessThanOp(
      buildVarRefExp(loop_index_name, scope),
      copyExpression(isSgArrayType(lhs_type)->get_index()),
      SageInterface::is_C_language() ? static_cast<SgType *>(buildIntType())
                                     : static_cast<SgType *>(buildBoolType())));

  // Loop increment
  SgExpression *loop_increment =
      buildPlusPlusOp(buildVarRefExp(loop_index_name, scope), buildIntType(),
                      SgUnaryOp::postfix);

  // Loop body
  SgExpression *assign_lhs =
      buildPntrArrRefExp(lhs, buildVarRefExp(loop_index_name, scope),
                         SageInterface::getElementType(lhs->get_type()));
  SgExpression *assign_rhs =
      buildPntrArrRefExp(rhs, buildVarRefExp(loop_index_name, scope),
                         SageInterface::getElementType(rhs->get_type()));
  SgStatement *loop_body = NULL;
  SgType *assign_lhs_type =
      assign_lhs->get_type()->stripType(SgType::STRIP_TYPEDEF_TYPE);
  if (isSgArrayType(assign_lhs_type)) {
    loop_body = build_array_packing_statement(assign_lhs, assign_rhs, target);
  } else {
    loop_body = buildAssignStatement(assign_lhs, assign_rhs);
  }

  // Loop satement
  return buildForStatement(loop_init, loop_test, loop_increment, loop_body);
}

static SgExpression *captureInitializer(SgExpression *source, SgType *type) {
  auto *array = isSgArrayType(type->stripType(SgType::STRIP_TYPEDEF_TYPE));
  if (array == nullptr)
    return source;
  const auto extent = evaluateConstIntegerExpression(array->get_index());
  ROSE_ASSERT(extent.hasValue_);
  auto *elements = buildExprListExp();
  for (size_t index = 0; index < extent.value_; ++index) {
    auto *element = buildPntrArrRefExp(copyExpression(source),
                                       buildUnsignedLongLongIntVal(index),
                                       array->get_base_type());
    appendExpression(elements,
                     captureInitializer(element, array->get_base_type()));
  }
  deleteAST(source);
  return buildAggregateInitializer(
      elements, type,
      SgAggregateInitializer::e_aggregate_initializer_source_braced);
}

std::string Outliner::generatePackingStatements(
    SgStatement *target, ASTtools::VarSymSet_t &syms,
    ASTtools::VarSymSet_t &pdsyms,
    SgClassDeclaration *struct_decl /* = NULL */) {

  int var_count = syms.size();
  int counter = 0;
  string wrapper_name = generateFuncArgName(target); //"__out_argv";
  // We still need to generate the declaration for the wrapper variable.
  if (var_count == 0)
    return wrapper_name;
  SgScopeStatement *cur_scope = scopeForInsertedStatement(target);
  ROSE_ASSERT(cur_scope != NULL);

  // void * __out_argv[count];
  SgType *my_type = NULL;

  if (useStructureWrapper) {
    ROSE_ASSERT(struct_decl != NULL);
    my_type = struct_decl->get_type();
  } else // default case for parameter wrapping is to use an array of pointers
  {
    SgType *pointer_type = buildPointerType(buildVoidType());
    my_type = buildArrayType(pointer_type, buildIntVal(var_count));
  }
  if (SageInterface::containsUnknownType(my_type)) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[wrapper-type]: target=%p has no exact "
            "typed wrapper variable\n",
            static_cast<void *>(target));
    ROSE_ABORT();
  }

  if (useStructureWrapper && SageInterface::is_Cxx_language() &&
      isSgOmpTaskStatement(target)) {
    auto *initializers = buildExprListExp();
    auto *definition =
        isSgClassDeclaration(struct_decl->get_definingDeclaration())
            ->get_definition();
    for (const SgVariableSymbol *symbol : syms) {
      SgExpression *value =
          buildVarRefExp(const_cast<SgVariableSymbol *>(symbol));
      std::string field = symbol->get_name().getString();
      if (pdsyms.count(symbol)) {
        field += "_p";
        value =
            buildAddressOfOp(value, buildAddressOfResultType(value->get_type()),
                             SgAddressOfOp::e_address_operand_object);
      }
      auto *member = definition->lookup_variable_symbol(field);
      ROSE_ASSERT(member != nullptr);
      appendExpression(initializers,
                       captureInitializer(value, member->get_type()));
    }
    auto *initializer = buildAggregateInitializer(
        initializers, my_type,
        SgAggregateInitializer::e_aggregate_initializer_source_braced);
    auto *allocation = buildNewExp(my_type, SgPointerType::createType(my_type),
                                   nullptr, SgNewExp::e_no_array_bound, nullptr,
                                   nullptr, initializer, false, nullptr, 1);
    auto *capture = buildVariableDeclaration(
        wrapper_name, allocation->get_type(),
        buildAssignInitializer(allocation, allocation->get_type()), cur_scope);
    validateGeneratedVariableDeclaration(capture, cur_scope, false);
    insertStatementBefore(target, capture);
    validateGeneratedVariableDeclaration(capture, cur_scope, true);
    return wrapper_name;
  }

  SgVariableDeclaration *out_argv =
      buildVariableDeclaration(wrapper_name, my_type, NULL, cur_scope);
  validateGeneratedVariableDeclaration(out_argv, cur_scope, false);
  SageInterface::insertStatementBefore(target, out_argv);
  validateGeneratedVariableDeclaration(out_argv, cur_scope, true);

  SgVariableSymbol *wrapper_symbol = getFirstVarSym(out_argv);
  ROSE_ASSERT(wrapper_symbol->get_parent() != NULL);
  for (ASTtools::VarSymSet_t::reverse_iterator i = syms.rbegin();
       i != syms.rend(); ++i) {
    SgExpression *lhs = NULL;
    SgExpression *rhs = NULL;
    SgStatement *assignment = NULL;
    if (useStructureWrapper) {
      // if use a struct to wrap parameters
      // two kinds of field: original type v.s. pointer type to the original
      // type
      //  __out_argv1__1527__.i = i;
      //  __out_argv1__1527__.sum_p = &sum;
      // Sara Royuela, Dec 12, 2012: There is a third type
      // When LHS is an array, we must copy each position.
      SgInitializedName *i_name = (*i)->get_declaration();
      SgVariableSymbol *i_symbol = const_cast<SgVariableSymbol *>(*i);
      string member_name = i_name->get_name().str();
      rhs = buildVarRefExp(i_symbol);
      if (pdsyms.find(i_symbol) != pdsyms.end()) // pointer type
      {
        member_name = member_name + "_p";
        rhs = buildAddressOfOp(
            rhs, SageBuilder::buildAddressOfResultType(rhs->get_type()),
            SgAddressOfOp::e_address_operand_object);
      }
      SgClassDefinition *class_def = isSgClassDefinition(
          isSgClassDeclaration(struct_decl->get_definingDeclaration())
              ->get_definition());
      ROSE_ASSERT(class_def != NULL);
      SgVarRefExp *member_ref = buildVarRefExp(member_name, class_def);
      lhs = buildDotExp(buildVarRefExp(out_argv), member_ref,
                        member_ref->get_type());

      SgType *lhs_type = lhs->get_type()->stripType(SgType::STRIP_TYPEDEF_TYPE);
      if (pdsyms.find(i_symbol) !=
          pdsyms.end()) // only pointer members with type void* need cast
      {
        if (isSgPointerType(lhs_type) != NULL)
          if (isSgTypeVoid(isSgPointerType(lhs_type)->get_base_type()) != NULL)
            rhs = buildCastExp(rhs, buildPointerType(buildVoidType()));
      }
      if (pdsyms.find(i_symbol) == pdsyms.end() &&
          isSgArrayType(lhs_type)) { // Copy each position of the array
        assignment = build_array_packing_statement(lhs, rhs, target);
      } else {
        assignment = buildAssignStatement(lhs, rhs);
      }
    } else
    // Default case: array of pointers, e.g.,  *(__out_argv +0)=(void*)(&var1);
    {
      SgVarRefExp *wrapper_ref = buildVarRefExp(wrapper_symbol);
      lhs = buildPntrArrRefExp(
          wrapper_ref, buildIntVal(counter),
          SageInterface::getElementType(wrapper_ref->get_type()));
      SgVarRefExp *rhsvar = buildVarRefExp((*i)->get_declaration(), cur_scope);
      SgExpression *value_to_pass =
          isNonFortranGlobalArray((*i)->get_declaration())
              ? isSgExpression(rhsvar)
              : buildAddressOfOp(
                    rhsvar,
                    SageBuilder::buildAddressOfResultType(rhsvar->get_type()),
                    SgAddressOfOp::e_address_operand_object);
      rhs = buildCastExp(value_to_pass, buildPointerType(buildVoidType()),
                         SgCastExp::e_C_style_cast);

      assignment = buildAssignStatement(lhs, rhs);
    }

    SageInterface::insertStatementBefore(target, assignment);
    counter++;
  }
  return wrapper_name;
}

SgSourceFile *Outliner::generateNewSourceFile(SgBasicBlock *s,
                                              const string &file_name) {
#ifdef __linux__
  if (enable_debug) {
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
  }
#endif

  SgSourceFile *new_file = NULL;
  SgProject *project = getEnclosingNode<SgProject>(s);
  ROSE_ASSERT(project != NULL);
  // s could be transformation generated, so use the root SgFile for file name
  SgFile *cur_file = getEnclosingNode<SgFile>(s);
  ROSE_ASSERT(cur_file != NULL);
  // grab the file suffix,
  //  TODO another way is to generate suffix according to source language type
  std::string orig_file_name = cur_file->get_file_info()->get_filenameString();
  std::string file_suffix = StringUtility::fileNameSuffix(orig_file_name);
  ROSE_ASSERT(file_suffix != "");
  std::string new_file_name = file_name + "." + file_suffix;
  if (!output_path
           .empty()) { // save the outlined function into a specified path
    new_file_name = StringUtility::stripPathFromFileName(new_file_name);
    new_file_name = output_path + "/" + new_file_name;
  }
  // remove pre-existing file with the same name
  remove(new_file_name.c_str());

  if (enable_debug)
    printf("In Outliner::generateNewSourceFile(): Calling "
           "buildGeneratedSourceFile(): "
           "new_file_name = %s \n",
           new_file_name.c_str());

  // This is an output-only translation unit, not a parsed input file.  The
  // dedicated generated-source API records that identity explicitly and does
  // not fabricate or parse a dummy input.
  new_file = buildGeneratedSourceFile(new_file_name, project);

  if (enable_debug)
    printf("DONE: In Outliner::generateNewSourceFile(): Calling "
           "buildGeneratedSourceFile(): "
           "new_file_name = %s \n",
           new_file_name.c_str());

  ROSE_ASSERT(new_file != NULL);
  return new_file;
}

namespace {
bool isStructurallyOwnedByGeneratedSource(const SgSourceFile *generated_source,
                                          const SgNode *node) {
  ROSE_ASSERT(generated_source != NULL);
  std::set<const SgNode *> visited;
  for (const SgNode *current = node; current != NULL;
       current = current->get_parent()) {
    if (current == generated_source) {
      return true;
    }
    if (!visited.insert(current).second) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-control-directive]: node=%p "
              "has a cycle in its structural parent chain\n",
              static_cast<const void *>(node));
      ROSE_ABORT();
    }
  }
  return false;
}

void removeConsumedOutlineDirectives(SgSourceFile *generated_source) {
  if (generated_source == NULL || generated_source->get_globalScope() == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[generated-control-directive]: generated "
            "source or its global scope is null\n");
    ROSE_ABORT();
  }

  Rose_STL_Container<SgNode *> nodes = NodeQuery::querySubTree(
      generated_source, V_SgPragmaDeclaration, AstQueryNamespace::AllNodes);
  for (SgNode *node : nodes) {
    SgPragmaDeclaration *directive = isSgPragmaDeclaration(node);
    if (directive == NULL || !Outliner::isOutlineDirective(directive) ||
        !isStructurallyOwnedByGeneratedSource(generated_source, directive)) {
      continue;
    }

    SgScopeStatement *owner = isSgScopeStatement(directive->get_parent());
    if (owner == NULL) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-control-directive]: "
              "directive=%p has no statement-list scope owner\n",
              static_cast<void *>(directive));
      ROSE_ABORT();
    }
    const std::vector<SgNode *> successors =
        owner->get_traversalSuccessorContainer();
    if (std::count(successors.begin(), successors.end(), directive) != 1) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-control-directive]: "
              "directive=%p owner=%p type=%s does not contain exactly one "
              "structural statement edge\n",
              static_cast<void *>(directive), static_cast<void *>(owner),
              owner->class_name().c_str());
      ROSE_ABORT();
    }

    SageInterface::removeStatement(directive);
    const std::vector<SgNode *> remaining_successors =
        owner->get_traversalSuccessorContainer();
    if (directive->get_parent() != NULL ||
        std::find(remaining_successors.begin(), remaining_successors.end(),
                  directive) != remaining_successors.end()) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-control-directive]: "
              "directive=%p remained structurally attached after removal\n",
              static_cast<void *>(directive));
      ROSE_ABORT();
    }
    SageInterface::deleteAST(directive);
  }

  nodes = NodeQuery::querySubTree(generated_source, V_SgPragmaDeclaration,
                                  AstQueryNamespace::AllNodes);
  for (SgNode *node : nodes) {
    SgPragmaDeclaration *directive = isSgPragmaDeclaration(node);
    if (directive != NULL && Outliner::isOutlineDirective(directive) &&
        isStructurallyOwnedByGeneratedSource(generated_source, directive)) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-control-directive]: generated "
              "source=%s retained outline directive=%p\n",
              generated_source->getFileName().c_str(),
              static_cast<void *>(directive));
      ROSE_ABORT();
    }
  }
}
} // namespace

/*!\brief the lib source file's name convention is rose_input_lib.[c|cxx].
 *
 * target is the input code block for outlining. It provides SgProject and input
 * file name info.
 *
 */
std::string Outliner::generateLibSourceFileName(SgBasicBlock *target) {
#ifdef __linux__
  if (enable_debug) {
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
  }
#endif
  std::string lib_file_name;

  // s could be transformation generated, so use the root SgFile for file name
  SgFile *input_file = getEnclosingNode<SgFile>(target);
  ROSE_ASSERT(input_file != NULL);
  // grab the file suffix,
  std::string input_file_name =
      input_file->get_file_info()->get_filenameString();
  std::string file_suffix = StringUtility::fileNameSuffix(input_file_name);
  std::string file_name = StringUtility::stripFileSuffixFromFileName(
      StringUtility::stripPathFromFileName(input_file_name));
  ROSE_ASSERT(file_suffix != "");
  lib_file_name = "rose_" + file_name + "_lib." + file_suffix;
  if (!output_path
           .empty()) { // save the outlined function into a specified path
    lib_file_name = StringUtility::stripPathFromFileName(lib_file_name);
    lib_file_name = output_path + "/" + lib_file_name;
  }

  if (enable_debug) {
    cout << "lib_file_name=" << lib_file_name << endl;
  }
  return lib_file_name;
}

/*!\brief Obtain the file handle to the separated source file storing outlined
 * functions. This file will be compiled to .so dynamically loadable library.
 * target is the input code block for outlining. It provides SgProject and input
 * file name info. the lib source file's name convention is
 * rose_input_lib.[c|cxx] by default. Or overrided by file_name.
 */
SgSourceFile *Outliner::getLibSourceFile(SgBasicBlock *target) {
  return getLibSourceFile(target, {});
}

SgSourceFile *Outliner::getLibSourceFile(
    SgBasicBlock *target,
    const std::vector<SgFunctionDeclaration *> &definitionsToMove) {
#ifdef __linux__
  if (enable_debug) {
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
  }
#endif
  SgSourceFile *new_file = NULL;
  SgProject *project = getEnclosingNode<SgProject>(target);
  ROSE_ASSERT(project != NULL);

  SgFile *input_file = getEnclosingNode<SgFile>(target);
  ROSE_ASSERT(input_file != NULL);
  SgSourceFile *input_source_file = isSgSourceFile(input_file);
  if (input_source_file == NULL) {
    fprintf(stderr, "REX_OUTLINER_INVARIANT[generated-include-tree]: outlining "
                    "target is not owned by a source file\n");
    ROSE_ABORT();
  }
  std::set<const SgFunctionDeclaration *> moving_originals;
  for (const SgFunctionDeclaration *declaration : definitionsToMove) {
    ROSE_ASSERT(declaration != nullptr &&
                declaration->get_definingDeclaration() == declaration &&
                declaration->get_definition() != nullptr &&
                SageInterface::getEnclosingSourceFile(declaration, true) ==
                    input_source_file);
    const bool unique = moving_originals.insert(declaration).second;
    ROSE_ASSERT(unique);
  }
  std::string new_file_name = generateLibSourceFileName(target);

  if (enable_debug)
    printf("Before strip path: new_file_name = %s \n", new_file_name.c_str());

  new_file_name = Rose::utility_stripPathFromFileName(new_file_name);

  if (enable_debug)
    printf("After strip path: new_file_name = %s \n", new_file_name.c_str());

  // Search if the lib file already exists.
  SgFilePtrList file_list = project->get_files();
  SgFilePtrList::iterator iter;
  for (iter = file_list.begin(); iter != file_list.end(); iter++) {
    SgFile *cur_file = *iter;
    SgSourceFile *sfile = isSgSourceFile(cur_file);

    if (sfile != NULL) {
      string cur_file_name = sfile->get_file_info()->get_filenameString();

      if (cur_file_name == new_file_name) {
        new_file = sfile;
        break;
      }
    }
  } // end for SgFile

  if (enable_debug)
    printf("In Outliner::getLibSourceFile(): new_file = %p \n", new_file);

  if (new_file == NULL) {
    SgCopyHelp::copiedNodeMapType identities;
    new_file = isSgSourceFile(
        SageInterface::deepCopySemanticSubtree(input_source_file, identities));
    ROSE_ASSERT(new_file != nullptr && new_file != input_source_file &&
                new_file->get_parent() == nullptr);
    std::set<SgFunctionDeclaration *> moving_copies;
    for (const SgFunctionDeclaration *declaration : moving_originals) {
      SgFunctionDeclaration *copy =
          isSgFunctionDeclaration(identities.at(declaration));
      ROSE_ASSERT(copy != nullptr && copy != declaration &&
                  copy->get_definingDeclaration() == copy &&
                  copy->get_definition() != nullptr);
      const bool unique = moving_copies.insert(copy).second;
      ROSE_ASSERT(unique);
    }
    identities.clear();
    SageBuilder::rebindCopiedSourceFilePhysicalIdentity(new_file,
                                                        new_file_name);
    new_file->set_unparse_output_filename(new_file_name);
    new_file->set_isCommandLineInputSource(false);
    SgFileList *files = project->get_fileList_ptr();
    ROSE_ASSERT(files != nullptr && files->get_parent() == project);
    new_file->set_parent(files);
    files->get_listOfFiles().push_back(new_file);

    removeConsumedOutlineDirectives(new_file);
    if (exclude_headers) {
      removeExcludedSourceIncludes(new_file, input_source_file);
      new_file->set_unparseHeaderFiles(false);
    }

    // Retain static/inline definitions and the exact pending transfers.
    // Other source definitions become prototypes at their lexical owner.
    std::vector<SgFunctionDeclaration *> functionList =
        generateFunctionDefinitionsList(new_file);
    if (new_file->get_file_info() == NULL ||
        new_file->get_file_info()->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_OUTLINER_INVARIANT[generated-declaration-ownership]: "
              "copied output has no exact physical-file ownership\n");
      ROSE_ABORT();
    }
    const int output_physical_file_id =
        new_file->get_file_info()->get_physical_file_id();

    std::vector<SgFunctionDeclaration *>::iterator i = functionList.begin();
    while (i != functionList.end()) {
      SgFunctionDeclaration *functionDeclaration = *i;
      ROSE_ASSERT(functionDeclaration != NULL);
      SgMemberFunctionDeclaration *member_function =
          isSgMemberFunctionDeclaration(functionDeclaration);
      SgClassDefinition *lexical_class =
          isSgClassDefinition(functionDeclaration->get_parent());
      if (member_function != NULL && lexical_class != NULL &&
          functionDeclaration->get_scope() != lexical_class) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-definition-owner]: in-class "
                "member=%s does not have its lexical class as semantic scope\n",
                functionDeclaration->get_name().str());
        ROSE_ABORT();
      }
      const bool is_inline_definition =
          functionDeclaration->get_functionModifier().isInline() ||
          lexical_class != NULL;

      const bool retain_definition =
          isStatic(functionDeclaration) || is_inline_definition ||
          moving_copies.count(functionDeclaration) != 0;
      Sg_File_Info *function_start =
          functionDeclaration->get_startOfConstruct();
      Sg_File_Info *function_end = functionDeclaration->get_endOfConstruct();
      const bool exact_function_range =
          function_start != NULL && function_end != NULL &&
          function_start->get_parent() == functionDeclaration &&
          function_end->get_parent() == functionDeclaration &&
          !function_start->isShared() && !function_end->isShared() &&
          function_start->get_physical_file_id() >= 0 &&
          function_start->get_physical_file_id() ==
              function_end->get_physical_file_id();
      const bool retained_header_source =
          exact_function_range &&
          function_start->get_physical_file_id() != output_physical_file_id &&
          !function_start->isTransformation() &&
          !function_start->isCompilerGenerated() &&
          (lexical_class == NULL ||
           (lexical_class->get_file_info() != NULL &&
            lexical_class->get_file_info()->get_physical_file_id() ==
                function_start->get_physical_file_id()));
      if (retain_definition &&
          (functionDeclaration->get_definition() == NULL ||
           functionDeclaration->get_parent() == NULL ||
           functionDeclaration->get_scope() == NULL || !exact_function_range ||
           (function_start->get_physical_file_id() != output_physical_file_id &&
            !retained_header_source) ||
           SageInterface::getEnclosingSourceFile(functionDeclaration, true) !=
               new_file)) {
        fprintf(stderr,
                "REX_OUTLINER_INVARIANT[copied-definition-owner]: "
                "retained function=%s has no exact definition, structural "
                "owner, semantic scope, source file, or physical file: "
                "definition=%p parent=%p/%s scope=%p/%s info=%p physical=%d "
                "expected-physical=%d source=%p expected-source=%p\n",
                functionDeclaration->get_name().str(),
                static_cast<void *>(functionDeclaration->get_definition()),
                static_cast<void *>(functionDeclaration->get_parent()),
                functionDeclaration->get_parent() != NULL
                    ? functionDeclaration->get_parent()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(functionDeclaration->get_scope()),
                functionDeclaration->get_scope() != NULL
                    ? functionDeclaration->get_scope()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(function_start),
                function_start != NULL ? function_start->get_physical_file_id()
                                       : -999,
                output_physical_file_id,
                static_cast<void *>(SageInterface::getEnclosingSourceFile(
                    functionDeclaration, true)),
                static_cast<void *>(new_file));
        ROSE_ABORT();
      }
      // Transform into prototype.
      if (!retain_definition) {
        if (isSgTemplateInstantiationFunctionDecl(functionDeclaration) !=
            NULL) {
          // Implicit free-function instantiations are semantic artifacts, not
          // definitions spelled in the copied translation unit.
          Sg_File_Info *function_file_info =
              functionDeclaration->get_file_info();
          if (function_file_info == NULL ||
              (!function_file_info->isCompilerGenerated() &&
               !function_file_info->isFrontendSpecific())) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[template-instantiation]: "
                    "function=%s is source-visible but cannot be converted "
                    "to a prototype\n",
                    functionDeclaration->get_name().str());
            ROSE_ABORT();
          }
        } else {
          SgDeclarationStatement *source_replacement =
              replaceFunctionDefinitionWithDeclaration(functionDeclaration);
          if (source_replacement->get_parent() == NULL ||
              source_replacement->get_file_info() == NULL ||
              source_replacement->get_file_info()->get_physical_file_id() !=
                  output_physical_file_id) {
            fprintf(stderr,
                    "REX_OUTLINER_INVARIANT[generated-declaration-ownership]: "
                    "function=%s replacement lost structural or physical "
                    "ownership\n",
                    functionDeclaration->get_name().str());
            ROSE_ABORT();
          }
        }
      }
      i++;
    }
  }

#ifdef __linux__
  if (enable_debug) {
    cout << "Exiting " << __PRETTY_FUNCTION__ << endl;
  }
#endif

  ROSE_ASSERT(new_file != NULL);
  SgIncludeFile *generated_include_root =
      new_file->get_associated_include_file();
  ROSE_ASSERT(!new_file->get_unparseHeaderFiles() ||
              generated_include_root != nullptr);
  if (generated_include_root != nullptr) {
    generated_include_root->validateIncludeOwnership();
    ROSE_ASSERT(generated_include_root->get_isRootSourceFile() &&
                generated_include_root->get_parent() == new_file &&
                generated_include_root->get_filename() == new_file_name);
  }
  return new_file;
}

// eof
