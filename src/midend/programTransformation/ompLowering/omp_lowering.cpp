// tps (01/14/2010) : Switching from rose.h to sage3.
#include "omp_lowering.h"

#include "Outliner.hh"

#include "RoseAst.h"

#include "rex_llvm.h"

#include "sage3basic.h"

#include "abiStuff.h"

#include "sageBuilder.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

using namespace std;
using namespace Rose;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

std::string OmpSupport::getKmpcRuntimeFunctionName(const std::string &abiName) {
  static const std::string prefix = "__kmpc_";
  if (abiName.rfind(prefix, 0) != 0 || abiName.size() == prefix.size()) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-runtime-name]: '" << abiName
              << "' is not one exact LLVM OpenMP runtime name\n";
    ROSE_ABORT();
  }
  if (!SageInterface::is_Fortran_language())
    return abiName;

  // A standard Fortran identifier must begin with a letter. The adapter owns
  // the ABI transition and exports this exact compiler-mangled source name.
  return "rex_" + abiName.substr(2);
}

SgFunctionRefExp *OmpSupport::buildFortranOutlinedFunctionRef(
    SgFunctionDeclaration *declaration) {
  ROSE_ASSERT(declaration != NULL);
  SgFunctionDeclaration *canonical =
      isSgFunctionDeclaration(declaration->get_firstNondefiningDeclaration());
  SgScopeStatement *scope = canonical != NULL ? canonical->get_scope() : NULL;
  SgSymbolTable *table = scope != NULL ? scope->get_symbol_table() : NULL;
  SgFunctionSymbol *symbol =
      scope != NULL && canonical != NULL
          ? isSgFunctionSymbol(scope->find_symbol_from_declaration(canonical))
          : NULL;
  if (canonical == NULL || scope == NULL || table == NULL || symbol == NULL ||
      symbol->get_declaration() != canonical || symbol->get_parent() != table ||
      !table->exists(symbol)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[fortran-outlined-reference]: "
            "outlined declaration=%p canonical=%p scope=%p table=%p "
            "symbol=%p has no exact semantic publication\n",
            static_cast<void *>(declaration), static_cast<void *>(canonical),
            static_cast<void *>(scope), static_cast<void *>(table),
            static_cast<void *>(symbol));
    ROSE_ABORT();
  }
  return SageBuilder::buildFortranFunctionRefExp(
      symbol, symbol,
      SgFunctionRefExp::e_fortran_source_visible_binding_semantic_publication);
}

static void insert_fortran_statement_in_specification_part(SgStatement *stmt,
                                                           SgBasicBlock *body);
static void
insert_fortran_declaration_into_procedure(SgVariableDeclaration *decl,
                                          SgScopeStatement *scope);

namespace {
std::map<const SgOmpClauseBodyStatement *, std::set<const SgInitializedName *>>
    implicit_target_map_variables;

SgAddressOfOp *buildExactAddressOfOp(SgExpression *operand) {
  SgType *operand_type = operand != nullptr ? operand->get_type() : nullptr;
  SgType *addressed_type = operand_type;
  if (SgReferenceType *reference = isSgReferenceType(addressed_type))
    addressed_type = reference->get_base_type();
  else if (SgRvalueReferenceType *reference =
               isSgRvalueReferenceType(addressed_type))
    addressed_type = reference->get_base_type();
  SgPointerType *result_type =
      addressed_type != nullptr ? SageBuilder::buildPointerType(addressed_type)
                                : nullptr;
  if (operand == nullptr || operand->get_parent() != nullptr ||
      operand_type == nullptr || addressed_type == nullptr ||
      result_type == nullptr ||
      result_type->get_base_type() != addressed_type) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[address-of-type]: operand=%p "
            "parent=%p type=%p result=%p base=%p does not identify one exact "
            "detached typed address operation\n",
            static_cast<void *>(operand),
            static_cast<void *>(operand != nullptr ? operand->get_parent()
                                                   : nullptr),
            static_cast<void *>(operand_type), static_cast<void *>(result_type),
            static_cast<void *>(result_type != nullptr
                                    ? result_type->get_base_type()
                                    : nullptr));
    ROSE_ABORT();
  }
  SgAddressOfOp *result = SageBuilder::buildAddressOfOp(operand, result_type);
  if (result == nullptr || result->get_operand_i() != operand ||
      operand->get_parent() != result || result->get_type() != result_type) {
    fprintf(
        stderr,
        "REX_OMP_LOWERING_INVARIANT[address-of-publication]: operand=%p "
        "result=%p owned-operand=%p parent=%p type=%p expected-type=%p "
        "did not publish one exact address operation\n",
        static_cast<void *>(operand), static_cast<void *>(result),
        static_cast<void *>(result != nullptr ? result->get_operand_i()
                                              : nullptr),
        static_cast<void *>(operand->get_parent()),
        static_cast<void *>(result != nullptr ? result->get_type() : nullptr),
        static_cast<void *>(result_type));
    ROSE_ABORT();
  }
  return result;
}

SgClassDeclaration *
getOrBuildRuntimeStructDeclaration(SgGlobal *globalScope,
                                   const SgName &runtimeTypeName) {
  if (globalScope == nullptr || runtimeTypeName.is_null()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-struct-input]: scope=%p "
            "name='%s' does not identify one exact runtime structure\n",
            static_cast<void *>(globalScope),
            runtimeTypeName.getString().c_str());
    ROSE_ABORT();
  }

  SgSymbolTable *symbols = globalScope->get_symbol_table();
  SgClassSymbol *symbol = symbols != nullptr
                              ? symbols->find_class(runtimeTypeName, nullptr)
                              : nullptr;
  if (symbol == nullptr) {
    (void)buildStructDeclaration(declaration_ownership::semanticAuxiliary(),
                                 runtimeTypeName, globalScope);
    symbol = symbols != nullptr ? symbols->find_class(runtimeTypeName, nullptr)
                                : nullptr;
  }

  SgClassDeclaration *canonical =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  SgClassDeclaration *defining =
      canonical != nullptr
          ? isSgClassDeclaration(canonical->get_definingDeclaration())
          : nullptr;
  SgClassDefinition *definition =
      defining != nullptr ? defining->get_definition() : nullptr;
  SgAuxiliaryDeclarationList *container =
      globalScope->get_auxiliary_declarations();
  const SgDeclarationStatementPtrList *auxiliaryDeclarations =
      container != nullptr ? &container->get_declarations() : nullptr;
  const size_t canonicalEdges =
      auxiliaryDeclarations != nullptr
          ? static_cast<size_t>(std::count(auxiliaryDeclarations->begin(),
                                           auxiliaryDeclarations->end(),
                                           canonical))
          : 0;
  const size_t definingEdges =
      auxiliaryDeclarations != nullptr
          ? static_cast<size_t>(std::count(auxiliaryDeclarations->begin(),
                                           auxiliaryDeclarations->end(),
                                           defining))
          : 0;
  SgClassType *type = canonical != nullptr ? canonical->get_type() : nullptr;
  if (symbol == nullptr || canonical == nullptr || defining == nullptr ||
      definition == nullptr || container == nullptr ||
      container->get_parent() != globalScope ||
      symbol->get_declaration() != canonical || symbols == nullptr ||
      symbols->find_class(runtimeTypeName, nullptr) != symbol ||
      canonical->get_name() != runtimeTypeName ||
      defining->get_name() != runtimeTypeName ||
      canonical->get_class_type() != SgClassDeclaration::e_struct ||
      defining->get_class_type() != SgClassDeclaration::e_struct ||
      canonical->get_scope() != globalScope ||
      defining->get_scope() != globalScope ||
      canonical->get_parent() != container ||
      defining->get_parent() != container || canonicalEdges != 1 ||
      definingEdges != 1 || globalScope->statementExistsInScope(canonical) ||
      globalScope->statementExistsInScope(defining) ||
      canonical->get_firstNondefiningDeclaration() != canonical ||
      canonical->get_definingDeclaration() != defining ||
      canonical->get_definition() != nullptr ||
      defining->get_firstNondefiningDeclaration() != canonical ||
      defining->get_definingDeclaration() != defining ||
      definition->get_declaration() != defining ||
      definition->get_parent() != defining || type == nullptr ||
      defining->get_type() != type || type->get_declaration() != canonical) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-struct-identity]: scope=%p "
            "name='%s' symbol=%p canonical=%p defining=%p definition=%p "
            "container=%p edges=%zu/%zu does not identify one exact "
            "auxiliary runtime structure\n",
            static_cast<void *>(globalScope),
            runtimeTypeName.getString().c_str(), static_cast<void *>(symbol),
            static_cast<void *>(canonical), static_cast<void *>(defining),
            static_cast<void *>(definition), static_cast<void *>(container),
            canonicalEdges, definingEdges);
    ROSE_ABORT();
  }
  return defining;
}

SgType *exactLogicalResultType() {
  if (SageInterface::is_C_language())
    return SageBuilder::buildIntType();
  return SageBuilder::buildBoolType();
}

void rebindTransformedVariableReference(SgVarRefExp *reference,
                                        SgVariableSymbol *expected_source,
                                        SgVariableSymbol *target,
                                        const char *contract) {
  SgInitializedName *target_name =
      target != nullptr ? target->get_declaration() : nullptr;
  SgScopeStatement *target_scope =
      target_name != nullptr ? target_name->get_scope() : nullptr;
  SgSymbolTable *target_table =
      target_scope != nullptr ? target_scope->get_symbol_table() : nullptr;
  if (reference == nullptr || expected_source == nullptr || target == nullptr ||
      contract == nullptr || reference->get_symbol() != expected_source ||
      target_name == nullptr || target_name->get_type() == nullptr ||
      target->get_symbol_basis() != target_name || target_scope == nullptr ||
      target_table == nullptr || target->get_parent() != target_table ||
      !target_table->exists(target) ||
      target_scope->find_symbol_from_declaration(target_name) != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[variable-reference-rebind]: "
            "contract=%s reference=%p current=%p expected=%p target=%p "
            "declaration=%p scope=%p table=%p has no exact rebind "
            "transaction\n",
            contract != nullptr ? contract : "<null>",
            static_cast<void *>(reference),
            static_cast<void *>(reference != nullptr ? reference->get_symbol()
                                                     : nullptr),
            static_cast<void *>(expected_source), static_cast<void *>(target),
            static_cast<void *>(target_name), static_cast<void *>(target_scope),
            static_cast<void *>(target_table));
    ROSE_ABORT();
  }

  std::vector<SgMacroExpansionExp *> invalidated_macro_surfaces;
  for (SgNode *owner = reference->get_parent(); owner != nullptr;
       owner = owner->get_parent()) {
    if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(owner)) {
      invalidated_macro_surfaces.push_back(macro);
    }
    if (isSgStatement(owner) != nullptr) {
      break;
    }
  }
  for (SgMacroExpansionExp *macro : invalidated_macro_surfaces) {
    SgExpression *expanded = macro->get_expanded_expression_checked();
    SgNode *macro_owner = macro->get_parent();
    size_t exact_edges = 0;
    for (SgNode *edge : macro->get_traversalSuccessorContainer()) {
      if (edge == expanded) {
        ++exact_edges;
      }
    }
    if (macro_owner == nullptr || expanded->get_parent() != macro ||
        exact_edges != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[variable-reference-rebind]: "
              "contract=%s macro=%p owner=%p expanded=%p edges=%zu has no "
              "exact source-surface retirement transaction\n",
              contract, static_cast<void *>(macro),
              static_cast<void *>(macro_owner), static_cast<void *>(expanded),
              exact_edges);
      ROSE_ABORT();
    }
    macro->set_expanded_expression(nullptr);
    expanded->set_parent(nullptr);
    SageInterface::replaceExpression(macro, expanded, true);
    if (macro->get_parent() != nullptr ||
        expanded->get_parent() != macro_owner) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[variable-reference-rebind]: "
              "contract=%s macro=%p was not retired in favor of semantic "
              "expression=%p under owner=%p\n",
              contract, static_cast<void *>(macro),
              static_cast<void *>(expanded), static_cast<void *>(macro_owner));
      ROSE_ABORT();
    }
    SageInterface::deepDelete(macro);
  }

  if (SgExpression *source = reference->get_originalExpressionTree()) {
    size_t exact_edges = 0;
    for (const std::pair<SgNode *, std::string> &edge :
         reference->returnDataMemberPointers()) {
      if (edge.second == "originalExpressionTree" && edge.first == source) {
        ++exact_edges;
      }
    }
    if (source == reference || source->get_parent() != reference ||
        exact_edges != 1 || source->get_originalExpressionTree() != nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[variable-reference-rebind]: "
              "contract=%s reference=%p source=%p edges=%zu has malformed "
              "source-spelling ownership\n",
              contract, static_cast<void *>(reference),
              static_cast<void *>(source), exact_edges);
      ROSE_ABORT();
    }
    reference->set_originalExpressionTree(nullptr);
    source->set_parent(nullptr);
    SageInterface::deepDelete(source);
  }

  reference->set_symbol(target);
  SageInterface::setOneSourcePositionForTransformation(reference);
  if (reference->get_symbol() != target ||
      reference->get_symbol()->get_declaration() != target_name ||
      reference->get_originalExpressionTree() != nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[variable-reference-rebind]: "
            "contract=%s reference=%p did not publish target=%p "
            "declaration=%p without stale source spelling\n",
            contract, static_cast<void *>(reference),
            static_cast<void *>(target), static_cast<void *>(target_name));
    ROSE_ABORT();
  }
}

SgTypeInt *buildKmpcInt32Type() {
  static_assert(sizeof(std::int32_t) == 4,
                "LLVM OpenMP runtime requires a 32-bit kmp_int32");
  return SageBuilder::buildIntType(SageBuilder::buildIntVal(4));
}

SgTypeInt *buildKmpcInt64Type() {
  static_assert(sizeof(std::int64_t) == 8,
                "LLVM OpenMP runtime requires a 64-bit kmp_int64");
  return SageBuilder::buildIntType(SageBuilder::buildIntVal(8));
}

void markStatementSubtreeForOutput(SgNode *root) {
  if (root == nullptr)
    return;

  Rose_STL_Container<SgNode *> statements =
      NodeQuery::querySubTree(root, V_SgStatement);
  for (SgNode *node : statements) {
    SgLocatedNode *located = isSgLocatedNode(node);
    if (located == nullptr)
      continue;

    if (Sg_File_Info *info = located->get_file_info())
      info->setOutputInCodeGeneration();
    if (Sg_File_Info *start = located->get_startOfConstruct())
      start->setOutputInCodeGeneration();
    if (Sg_File_Info *end = located->get_endOfConstruct())
      end->setOutputInCodeGeneration();
  }
}

SgVarRefExp *extractVarRefFromExpression(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }
  if (SgOmpMapItem *item = isSgOmpMapItem(expr)) {
    return extractVarRefFromExpression(item->get_expression());
  }
  if (SgVarRefExp *vref = isSgVarRefExp(expr)) {
    return vref;
  }
  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return extractVarRefFromExpression(aref->get_lhs_operand());
  }
  if (SgDotExp *dot = isSgDotExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(dot->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(dot->get_lhs_operand());
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(arrow->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(arrow->get_lhs_operand());
  }
  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return extractVarRefFromExpression(deref->get_operand());
  }
  if (SgAddressOfOp *addr = isSgAddressOfOp(expr)) {
    return extractVarRefFromExpression(addr->get_operand());
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    return extractVarRefFromExpression(cast->get_operand());
  }
  if (SgCommaOpExp *comma = isSgCommaOpExp(expr)) {
    if (SgVarRefExp *rhs =
            extractVarRefFromExpression(comma->get_rhs_operand())) {
      return rhs;
    }
    return extractVarRefFromExpression(comma->get_lhs_operand());
  }
  if (SgExprListExp *list = isSgExprListExp(expr)) {
    for (SgExpression *elem : list->get_expressions()) {
      if (SgVarRefExp *vref = extractVarRefFromExpression(elem)) {
        return vref;
      }
    }
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    return extractVarRefFromExpression(unary->get_operand());
  }
  return nullptr;
}

SgVariableSymbol *extractClauseVariableSymbol(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  if (SgOmpMapItem *item = isSgOmpMapItem(expr)) {
    return extractClauseVariableSymbol(item->get_expression());
  }

  if (SgVarRefExp *vref = isSgVarRefExp(expr)) {
    return isSgVariableSymbol(vref->get_symbol());
  }
  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return extractClauseVariableSymbol(aref->get_lhs_operand());
  }
  if (SgDotExp *dot = isSgDotExp(expr)) {
    if (SgVariableSymbol *lhs =
            extractClauseVariableSymbol(dot->get_lhs_operand())) {
      return lhs;
    }
    return extractClauseVariableSymbol(dot->get_rhs_operand());
  }
  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    if (SgVariableSymbol *lhs =
            extractClauseVariableSymbol(arrow->get_lhs_operand())) {
      return lhs;
    }
    return extractClauseVariableSymbol(arrow->get_rhs_operand());
  }
  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return extractClauseVariableSymbol(deref->get_operand());
  }
  if (SgAddressOfOp *addr = isSgAddressOfOp(expr)) {
    return extractClauseVariableSymbol(addr->get_operand());
  }
  if (SgCastExp *cast = isSgCastExp(expr)) {
    return extractClauseVariableSymbol(cast->get_operand());
  }
  if (SgCommaOpExp *comma = isSgCommaOpExp(expr)) {
    if (SgVariableSymbol *rhs =
            extractClauseVariableSymbol(comma->get_rhs_operand())) {
      return rhs;
    }
    return extractClauseVariableSymbol(comma->get_lhs_operand());
  }
  if (SgExprListExp *list = isSgExprListExp(expr)) {
    for (SgExpression *elem : list->get_expressions()) {
      if (SgVariableSymbol *sym = extractClauseVariableSymbol(elem)) {
        return sym;
      }
    }
  }
  if (SgUnaryOp *unary = isSgUnaryOp(expr)) {
    return extractClauseVariableSymbol(unary->get_operand());
  }
  return nullptr;
}

bool isSharedByDefaultInOrphanedConstruct(const SgInitializedName *init_var) {
  if (init_var == nullptr) {
    return false;
  }

  SgScopeStatement *var_scope = init_var->get_scope();
  if (isSgGlobal(var_scope) != nullptr ||
      isSgNamespaceDefinitionStatement(var_scope) != nullptr) {
    return true;
  }

  if (SgVariableDeclaration *var_decl =
          isSgVariableDeclaration(init_var->get_declaration())) {
    return isStatic(var_decl);
  }

  return false;
}

bool isOmpLibUseStatement(const SgStatement *stmt) {
  const SgUseStatement *use_stmt = isSgUseStatement(stmt);
  if (use_stmt == nullptr) {
    return false;
  }

  std::string module_name = use_stmt->get_name().getString();
  std::transform(module_name.begin(), module_name.end(), module_name.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return module_name == "omp_lib";
}

bool isOmpLibIncludeStatement(const SgStatement *stmt) {
  const SgFortranIncludeLine *include_stmt = isSgFortranIncludeLine(stmt);
  if (include_stmt == nullptr) {
    return false;
  }

  const std::filesystem::path include_path(include_stmt->get_filename());
  return include_path.filename() == "omp_lib.h";
}

SgBasicBlock *getEnclosingFortranProcedureBody(SgScopeStatement *scope) {
  ROSE_ASSERT(scope != nullptr);
  SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
  ROSE_ASSERT(func_def != nullptr);
  SgBasicBlock *body = func_def->get_body();
  ROSE_ASSERT(body != nullptr);
  return body;
}

const Sg_File_Info *getStatementStartInfo(const SgStatement *stmt) {
  ROSE_ASSERT(stmt != nullptr);
  if (const SgLocatedNode *located = isSgLocatedNode(stmt)) {
    if (const Sg_File_Info *info = located->get_startOfConstruct()) {
      return info;
    }
  }
  return stmt->get_file_info();
}

const Sg_File_Info *getStatementEndInfo(const SgStatement *stmt) {
  ROSE_ASSERT(stmt != nullptr);
  if (const SgLocatedNode *located = isSgLocatedNode(stmt)) {
    if (const Sg_File_Info *info = located->get_endOfConstruct()) {
      return info;
    }
  }
  return stmt->get_file_info();
}

bool sourcePositionLess(const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
  ROSE_ASSERT(lhs != nullptr);
  ROSE_ASSERT(rhs != nullptr);
  if (lhs->get_line() != rhs->get_line()) {
    return lhs->get_line() < rhs->get_line();
  }
  return lhs->get_col() < rhs->get_col();
}

bool sourcePositionAfter(const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
  ROSE_ASSERT(lhs != nullptr);
  ROSE_ASSERT(rhs != nullptr);
  if (lhs->get_line() != rhs->get_line()) {
    return lhs->get_line() > rhs->get_line();
  }
  return lhs->get_col() > rhs->get_col();
}

struct ExactLoweringRootOrder {
  size_t tree_index;
  std::vector<size_t> structural_path;
};

ExactLoweringRootOrder getExactLoweringRootOrder(SgNode *node,
                                                 SgSourceFile *primary_file,
                                                 SgSourceFile *outlined_file) {
  ROSE_ASSERT(node != nullptr);
  ROSE_ASSERT(primary_file != nullptr);
  std::vector<size_t> reversed_path;
  SgNode *current = node;
  while (current != primary_file && current != outlined_file) {
    SgNode *parent = current->get_parent();
    if (parent == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[root-owner]: node=%p kind=%s "
              "does not reach either lowering source file\n",
              static_cast<void *>(node), node->sage_class_name());
      ROSE_ABORT();
    }

    size_t index = 0;
    size_t matches = 0;
    SgNodePtrList siblings = parent->get_traversalSuccessorContainer();
    for (size_t i = 0; i < siblings.size(); ++i) {
      if (siblings[i] == current) {
        index = i;
        ++matches;
      }
    }
    if (matches != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[root-owner]: child=%p kind=%s "
              "has %zu owning traversal edges in parent=%p kind=%s\n",
              static_cast<void *>(current), current->sage_class_name(), matches,
              static_cast<void *>(parent), parent->sage_class_name());
      ROSE_ABORT();
    }
    reversed_path.push_back(index);
    current = parent;
  }

  std::reverse(reversed_path.begin(), reversed_path.end());
  return {current == primary_file ? 0u : 1u, std::move(reversed_path)};
}

void sortOpenMpRootsForLowering(Rose_STL_Container<SgNode *> &omp_nodes,
                                SgSourceFile *primary_file,
                                SgSourceFile *outlined_file) {
  if (outlined_file == primary_file) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[root-owner]: primary and "
                    "outlined lowering files are identical\n");
    ROSE_ABORT();
  }
  std::unordered_map<SgNode *, ExactLoweringRootOrder> orders;
  for (SgNode *node : omp_nodes) {
    orders.emplace(
        node, getExactLoweringRootOrder(node, primary_file, outlined_file));
  }

  std::sort(omp_nodes.begin(), omp_nodes.end(),
            [&orders](SgNode *lhs, SgNode *rhs) {
              const ExactLoweringRootOrder &lhs_order = orders.at(lhs);
              const ExactLoweringRootOrder &rhs_order = orders.at(rhs);
              if (lhs_order.tree_index != rhs_order.tree_index)
                return lhs_order.tree_index < rhs_order.tree_index;
              return lhs_order.structural_path < rhs_order.structural_path;
            });

  for (size_t i = 1; i < omp_nodes.size(); ++i) {
    const ExactLoweringRootOrder &previous = orders.at(omp_nodes[i - 1]);
    const ExactLoweringRootOrder &current = orders.at(omp_nodes[i]);
    if (previous.tree_index == current.tree_index &&
        previous.structural_path == current.structural_path) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[root-owner]: distinct roots=%p,%p "
              "share one exact structural path\n",
              static_cast<void *>(omp_nodes[i - 1]),
              static_cast<void *>(omp_nodes[i]));
      ROSE_ABORT();
    }
  }
}

SgStatement *findNextOriginalStatementInScope(SgStatement *target) {
  ROSE_ASSERT(target != nullptr);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != nullptr);

  bool saw_target = false;
  const SgStatementPtrList &stmts = scope->getStatementList();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgStatement *stmt = *it;
    if (stmt == nullptr) {
      continue;
    }
    if (!saw_target) {
      if (stmt == target) {
        saw_target = true;
      }
      continue;
    }

    const Sg_File_Info *stmt_start = getStatementStartInfo(stmt);
    if (stmt_start == nullptr || stmt_start->isTransformation()) {
      continue;
    }
    return stmt;
  }

  const Sg_File_Info *target_end = getStatementEndInfo(target);
  ROSE_ASSERT(target_end != nullptr);

  SgStatement *next_stmt = nullptr;
  const Sg_File_Info *next_info = nullptr;
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgStatement *stmt = *it;
    if (stmt == nullptr || stmt == target) {
      continue;
    }

    const Sg_File_Info *stmt_start = getStatementStartInfo(stmt);
    if (stmt_start == nullptr || stmt_start->isTransformation()) {
      continue;
    }
    if (stmt_start->get_filenameString() != target_end->get_filenameString()) {
      continue;
    }
    if (!sourcePositionAfter(stmt_start, target_end)) {
      continue;
    }

    if (next_info == nullptr || sourcePositionLess(stmt_start, next_info)) {
      next_stmt = stmt;
      next_info = stmt_start;
    }
  }

  return next_stmt;
}

void ensureFortranOmpAllocatorInterfaces(SgScopeStatement *scope) {
  SgBasicBlock *body = getEnclosingFortranProcedureBody(scope);
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgStatement *stmt = *it;
    if (stmt == nullptr) {
      continue;
    }
    if (isOmpLibUseStatement(stmt) || isOmpLibIncludeStatement(stmt)) {
      return;
    }
  }

  SgFortranIncludeLine *include_stmt = buildFortranIncludeLine("omp_lib.h");
  insert_fortran_statement_in_specification_part(include_stmt, body);
}

std::string
ompAllocatorModifierName(SgOmpClause::omp_allocator_modifier_enum modifier) {
  switch (modifier) {
  case SgOmpClause::e_omp_allocator_default_mem_alloc:
    return "omp_default_mem_alloc";
  case SgOmpClause::e_omp_allocator_large_cap_mem_alloc:
    return "omp_large_cap_mem_alloc";
  case SgOmpClause::e_omp_allocator_const_mem_alloc:
    return "omp_const_mem_alloc";
  case SgOmpClause::e_omp_allocator_high_bw_mem_alloc:
    return "omp_high_bw_mem_alloc";
  case SgOmpClause::e_omp_allocator_low_lat_mem_alloc:
    return "omp_low_lat_mem_alloc";
  case SgOmpClause::e_omp_allocator_cgroup_mem_alloc:
    return "omp_cgroup_mem_alloc";
  case SgOmpClause::e_omp_allocator_pteam_mem_alloc:
    return "omp_pteam_mem_alloc";
  case SgOmpClause::e_omp_allocator_thread_mem_alloc:
    return "omp_thread_mem_alloc";
  default:
    return "";
  }
}

SgExpression *
buildAllocatorArgumentExpression(const SgOmpAllocatorClause *clause,
                                 SgScopeStatement *scope) {
  ROSE_ASSERT(clause != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const SgOmpClause::omp_allocator_modifier_enum modifier =
      clause->get_modifier();
  if (modifier == SgOmpClause::e_omp_allocator_user_defined_modifier) {
    SgExpression *user_defined = clause->get_user_defined_modifier();
    ROSE_ASSERT(user_defined != nullptr);
    return copyExpression(user_defined);
  }

  const std::string allocator_name = ompAllocatorModifierName(modifier);
  if (!allocator_name.empty()) {
    return buildVarRefExp(allocator_name, scope);
  }

  MLOG_ERROR_CXX("ompLowering")
      << "Unsupported allocator modifier in OpenMP allocate lowering: "
      << static_cast<int>(modifier);
  ROSE_ABORT();
}

SgOmpAllocatorClause *
getAllocatorClauseOrAbort(SgOmpAllocateStatement *target) {
  ROSE_ASSERT(target != nullptr);

  SgOmpAllocatorClause *allocator_clause = nullptr;
  const SgOmpClausePtrList &clauses = target->get_clauses();
  for (SgOmpClausePtrList::const_iterator it = clauses.begin();
       it != clauses.end(); ++it) {
    SgOmpClause *clause = *it;
    if (clause == nullptr) {
      continue;
    }
    if (SgOmpAllocatorClause *current = isSgOmpAllocatorClause(clause)) {
      if (allocator_clause != nullptr) {
        MLOG_ERROR_CXX("ompLowering")
            << "OpenMP allocate lowering expects at most one allocator clause";
        ROSE_ABORT();
      }
      allocator_clause = current;
      continue;
    }

    MLOG_ERROR_CXX("ompLowering")
        << "Unsupported clause on OpenMP allocate statement in lowering: "
        << clause->sage_class_name();
    ROSE_ABORT();
  }

  if (allocator_clause == nullptr) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate lowering requires an explicit allocator clause";
    ROSE_ABORT();
  }

  return allocator_clause;
}

std::set<SgInitializedName *>
collectReferencedBaseObjects(const SgExpressionPtrList &expressions) {
  std::set<SgInitializedName *> result;
  for (SgExpressionPtrList::const_iterator it = expressions.begin();
       it != expressions.end(); ++it) {
    SgExpression *expr = *it;
    if (expr == nullptr) {
      continue;
    }
    SgInitializedName *name = SageInterface::convertRefToInitializedName(expr);
    if (name == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Unable to resolve allocate object from expression " << expr
          << " type=" << expr->class_name();
      ROSE_ABORT();
    }
    result.insert(name);
  }
  return result;
}

std::set<SgInitializedName *>
collectAllocateStatementBaseObjects(const SgAllocateStatement *stmt) {
  ROSE_ASSERT(stmt != nullptr);
  SgExprListExp *expr_list = stmt->get_expr_list();
  ROSE_ASSERT(expr_list != nullptr);

  SgExpressionPtrList allocate_objects;
  const SgExpressionPtrList &exprs = expr_list->get_expressions();
  for (SgExpressionPtrList::const_iterator it = exprs.begin();
       it != exprs.end(); ++it) {
    SgExpression *expr = *it;
    if (expr == nullptr || isSgTypeExpression(expr) != nullptr) {
      continue;
    }
    allocate_objects.push_back(expr);
  }

  return collectReferencedBaseObjects(allocate_objects);
}

bool requiresOnlyDynamicAllocators(const SgOmpRequiresStatement *stmt) {
  ROSE_ASSERT(stmt != nullptr);
  const SgOmpClausePtrList &clauses = stmt->get_clauses();
  if (clauses.empty()) {
    return false;
  }

  for (SgOmpClausePtrList::const_iterator it = clauses.begin();
       it != clauses.end(); ++it) {
    if (isSgOmpDynamicAllocatorsClause(*it) == nullptr) {
      return false;
    }
  }

  return true;
}

SgExpression *stripNoopCastsAndParens(SgExpression *expr) {
  SgExpression *result = expr;
  while (result != nullptr) {
    if (SgCastExp *cast = isSgCastExp(result)) {
      result = cast->get_operand();
      continue;
    }
    if (SgExprListExp *list = isSgExprListExp(result)) {
      if (list->get_expressions().size() == 1) {
        result = list->get_expressions().front();
        continue;
      }
    }
    break;
  }
  return result;
}

bool extractPointerDerefChain(SgExpression *expr, SgVarRefExp *&base_ref,
                              size_t &deref_depth) {
  base_ref = nullptr;
  deref_depth = 0;
  SgExpression *cursor = stripNoopCastsAndParens(expr);
  while (SgPointerDerefExp *deref = isSgPointerDerefExp(cursor)) {
    ++deref_depth;
    cursor = stripNoopCastsAndParens(deref->get_operand());
  }
  base_ref = isSgVarRefExp(cursor);
  return base_ref != nullptr && deref_depth > 0;
}

void normalizeScalarLocalDerefUses(
    SgBasicBlock *bb,
    const std::set<SgVariableSymbol *> &scalar_locals_from_pointer_symbols) {
  if (bb == nullptr || scalar_locals_from_pointer_symbols.empty()) {
    return;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    typedef Rose_STL_Container<SgNode *> NodeList_t;
    NodeList_t derefs = NodeQuery::querySubTree(bb, V_SgPointerDerefExp);
    for (NodeList_t::iterator i = derefs.begin(); i != derefs.end(); ++i) {
      SgPointerDerefExp *deref = isSgPointerDerefExp(*i);
      if (deref == nullptr || deref->get_parent() == nullptr) {
        continue;
      }
      SgExpression *operand = stripNoopCastsAndParens(deref->get_operand());
      SgVarRefExp *var_ref = isSgVarRefExp(operand);
      if (var_ref == nullptr || var_ref->get_symbol() == nullptr) {
        continue;
      }
      if (scalar_locals_from_pointer_symbols.count(var_ref->get_symbol()) ==
          0) {
        continue;
      }
      replaceExpression(deref, buildVarRefExp(var_ref->get_symbol()));
      changed = true;
    }
  }
}

SgType *stripTypeAliases(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }
  return type->stripType(SgType::STRIP_MODIFIER_TYPE |
                         SgType::STRIP_TYPEDEF_TYPE);
}

SgType *stripTypeAliasesAndReferences(SgType *type) {
  SgType *result = stripTypeAliases(type);
  while (SgReferenceType *ref_type = isSgReferenceType(result)) {
    result = stripTypeAliases(ref_type->get_base_type());
  }
  return result;
}

SgType *getExactCxxSourceType(const SgInitializedName *name,
                              const char *contract);
SgType *getOmpArraySectionSourceElementType(SgType *source_type,
                                            SgType *semantic_element_type,
                                            size_t dimension_count,
                                            const char *contract);

bool isPointerBackedType(SgType *type) {
  return isSgPointerType(stripTypeAliasesAndReferences(type)) != nullptr;
}

bool isDirectVarRefToSymbol(SgExpression *expr, SgVariableSymbol *sym) {
  if (expr == nullptr || sym == nullptr) {
    return false;
  }
  SgVarRefExp *ref = isSgVarRefExp(stripNoopCastsAndParens(expr));
  return ref != nullptr && ref->get_symbol() == sym;
}

SgExpression *getLValueChainRoot(SgExpression *expr) {
  SgExpression *current = expr;
  while (current != nullptr && current->get_parent() != nullptr) {
    SgNode *parent = current->get_parent();
    if (SgDotExp *dot = isSgDotExp(parent)) {
      if (dot->get_lhs_operand() == current) {
        current = dot;
        continue;
      }
    }
    if (SgArrowExp *arrow = isSgArrowExp(parent)) {
      if (arrow->get_lhs_operand() == current) {
        current = arrow;
        continue;
      }
    }
    if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(parent)) {
      if (aref->get_lhs_operand() == current) {
        current = aref;
        continue;
      }
    }
    break;
  }
  return current;
}

bool isWriteUseOfExpression(SgExpression *expr) {
  if (expr == nullptr || expr->get_parent() == nullptr) {
    return false;
  }

  SgNode *parent = expr->get_parent();
  if (SgAssignOp *assign = isSgAssignOp(parent)) {
    return assign->get_lhs_operand() == expr;
  }
  if (SgCompoundAssignOp *compound = isSgCompoundAssignOp(parent)) {
    return compound->get_lhs_operand() == expr;
  }
  if (SgPlusPlusOp *inc = isSgPlusPlusOp(parent)) {
    return inc->get_operand() == expr;
  }
  if (SgMinusMinusOp *dec = isSgMinusMinusOp(parent)) {
    return dec->get_operand() == expr;
  }
  return false;
}

bool isExpressionWrittenThroughChain(SgExpression *expr) {
  return isWriteUseOfExpression(getLValueChainRoot(expr));
}

bool isExpressionAddressTaken(SgExpression *expr) {
  if (expr == nullptr) {
    return false;
  }
  SgExpression *root = getLValueChainRoot(expr);
  if (root == nullptr || root->get_parent() == nullptr) {
    return false;
  }
  return isSgAddressOfOp(root->get_parent()) != nullptr;
}

SgExpression *getEnclosingReadOnlyAccessRoot(SgExpression *expr,
                                             SgVariableSymbol *base_sym) {
  SgExpression *cursor = stripNoopCastsAndParens(expr);
  while (cursor != nullptr && cursor->get_parent() != nullptr) {
    SgExpression *parent = isSgExpression(cursor->get_parent());
    if (parent == nullptr) {
      break;
    }

    if (base_sym != nullptr &&
        extractClauseVariableSymbol(parent) != base_sym) {
      break;
    }

    if (SgCastExp *cast = isSgCastExp(parent)) {
      if (cast->get_operand() == cursor) {
        cursor = cast;
        continue;
      }
    }

    if (SgPointerDerefExp *deref = isSgPointerDerefExp(parent)) {
      if (deref->get_operand() == cursor) {
        cursor = deref;
        continue;
      }
    }

    if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(parent)) {
      if (aref->get_lhs_operand() == cursor ||
          aref->get_rhs_operand() == cursor) {
        cursor = aref;
        continue;
      }
    }

    if (SgDotExp *dot = isSgDotExp(parent)) {
      if (dot->get_lhs_operand() == cursor ||
          dot->get_rhs_operand() == cursor) {
        cursor = dot;
        continue;
      }
    }

    if (SgArrowExp *arrow = isSgArrowExp(parent)) {
      if (arrow->get_lhs_operand() == cursor ||
          arrow->get_rhs_operand() == cursor) {
        cursor = arrow;
        continue;
      }
    }

    break;
  }
  return cursor;
}

bool mappedArrayUsesAreReadOnlyInScope(SgBasicBlock *body,
                                       SgVariableSymbol *base_sym) {
  if (body == nullptr || base_sym == nullptr ||
      !isPointerBackedType(base_sym->get_type())) {
    return false;
  }

  bool saw_supported_access = false;
  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = var_refs.begin();
       it != var_refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == nullptr || ref->get_symbol() != base_sym) {
      continue;
    }

    SgExpression *root = getEnclosingReadOnlyAccessRoot(ref, base_sym);
    if (root == nullptr || extractClauseVariableSymbol(root) != base_sym) {
      return false;
    }

    if (isSgPntrArrRefExp(root) == nullptr && isSgDotExp(root) == nullptr &&
        isSgArrowExp(root) == nullptr && isSgPointerDerefExp(root) == nullptr) {
      return false;
    }

    if (isExpressionWrittenThroughChain(root) ||
        isExpressionAddressTaken(root)) {
      return false;
    }

    SgNode *parent = root->get_parent();
    if (isSgAssignInitializer(parent) != nullptr) {
      return false;
    }
    if (SgExprListExp *args = isSgExprListExp(parent)) {
      if (isSgFunctionCallExp(args->get_parent()) != nullptr) {
        return false;
      }
    }

    saw_supported_access = true;
  }

  return saw_supported_access;
}

bool containsUnsupportedDirectGridStrideControlFlow(SgBasicBlock *body) {
  if (body == nullptr) {
    return true;
  }
  return !NodeQuery::querySubTree(body, V_SgBreakStmt).empty() ||
         !NodeQuery::querySubTree(body, V_SgContinueStmt).empty() ||
         !NodeQuery::querySubTree(body, V_SgGotoStatement).empty() ||
         !NodeQuery::querySubTree(body, V_SgReturnStmt).empty();
}

bool isScalarizableDirectGridStrideElementType(SgType *type) {
  SgType *stripped = stripTypeAliasesAndReferences(type);
  return stripped != nullptr && isSgClassType(stripped) == nullptr &&
         isSgArrayType(stripped) == nullptr &&
         isSgFunctionType(stripped) == nullptr &&
         isSgTypeVoid(stripped) == nullptr;
}

bool isAggregateDirectGridStrideElementType(SgType *type) {
  return isSgClassType(stripTypeAliasesAndReferences(type)) != nullptr;
}

bool symbolWrittenInsideScope(SgBasicBlock *body, SgVariableSymbol *sym);

bool isPointerToConstType(SgType *type) {
  SgPointerType *ptr_type =
      isSgPointerType(stripTypeAliasesAndReferences(type));
  if (ptr_type == nullptr) {
    return false;
  }

  SgType *base_type = ptr_type->get_base_type();
  return base_type != nullptr && SageInterface::isConstType(base_type);
}

SgInitializedName *
findMatchingEnclosingFunctionParameter(SgInitializedName *decl) {
  if (decl == nullptr) {
    return nullptr;
  }

  SgFunctionDeclaration *func = getEnclosingFunctionDeclaration(decl);
  if (func == nullptr || func->get_parameterList() == nullptr) {
    return nullptr;
  }

  const SgInitializedNamePtrList &params =
      func->get_parameterList()->get_args();
  for (SgInitializedNamePtrList::const_iterator it = params.begin();
       it != params.end(); ++it) {
    SgInitializedName *param = *it;
    if (param == nullptr || param == decl) {
      continue;
    }
    if (param->get_name() == decl->get_name()) {
      return param;
    }
  }

  return nullptr;
}

bool exprDerivesFromReadOnlyDevicePointer(
    SgExpression *expr, SgBasicBlock *kernel_body,
    std::set<SgVariableSymbol *> &visiting_syms);

bool symbolIsReadOnlyDevicePointer(
    SgVariableSymbol *sym, SgBasicBlock *kernel_body,
    std::set<SgVariableSymbol *> &visiting_syms) {
  if (sym == nullptr || kernel_body == nullptr ||
      !isPointerToConstType(sym->get_type())) {
    return false;
  }

  if (symbolWrittenInsideScope(kernel_body, sym)) {
    return false;
  }

  SgInitializedName *decl = sym->get_declaration();
  if (decl == nullptr) {
    return false;
  }

  if (isSgFunctionParameterList(decl->get_parent()) != nullptr) {
    return true;
  }

  if (SgInitializedName *param = findMatchingEnclosingFunctionParameter(decl)) {
    if (isPointerToConstType(param->get_type())) {
      return true;
    }
  }

  if (isSgVariableDeclaration(decl->get_parent()) != nullptr &&
      decl->get_initializer() == nullptr) {
    return true;
  }

  if (!visiting_syms.insert(sym).second) {
    return false;
  }

  bool derives_from_read_only_input = false;
  if (SgAssignInitializer *init =
          isSgAssignInitializer(decl->get_initializer())) {
    derives_from_read_only_input = exprDerivesFromReadOnlyDevicePointer(
        init->get_operand_i(), kernel_body, visiting_syms);
  }

  visiting_syms.erase(sym);
  return derives_from_read_only_input;
}

bool exprDerivesFromReadOnlyDevicePointer(
    SgExpression *expr, SgBasicBlock *kernel_body,
    std::set<SgVariableSymbol *> &visiting_syms) {
  expr = stripNoopCastsAndParens(expr);
  if (expr == nullptr) {
    return false;
  }

  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    return symbolIsReadOnlyDevicePointer(
        isSgVariableSymbol(var_ref->get_symbol()), kernel_body, visiting_syms);
  }

  if (SgAddressOfOp *addr = isSgAddressOfOp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(addr->get_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgPntrArrRefExp *aref = isSgPntrArrRefExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(aref->get_lhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgDotExp *dot = isSgDotExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(dot->get_lhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgArrowExp *arrow = isSgArrowExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(arrow->get_lhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgPointerDerefExp *deref = isSgPointerDerefExp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(deref->get_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgAddOp *add = isSgAddOp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(add->get_lhs_operand(),
                                                kernel_body, visiting_syms) ||
           exprDerivesFromReadOnlyDevicePointer(add->get_rhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgSubtractOp *sub = isSgSubtractOp(expr)) {
    return exprDerivesFromReadOnlyDevicePointer(sub->get_lhs_operand(),
                                                kernel_body, visiting_syms) ||
           exprDerivesFromReadOnlyDevicePointer(sub->get_rhs_operand(),
                                                kernel_body, visiting_syms);
  }

  if (SgConditionalExp *cond = isSgConditionalExp(expr)) {
    cond->validate();
    return exprDerivesFromReadOnlyDevicePointer(cond->get_true_value_exp(),
                                                kernel_body, visiting_syms) &&
           exprDerivesFromReadOnlyDevicePointer(cond->get_false_exp(),
                                                kernel_body, visiting_syms);
  }

  return false;
}

int computeAstDepth(SgNode *node) {
  int depth = 0;
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    ++depth;
  }
  return depth;
}

bool isNodeWithinSubtree(SgNode *root, SgNode *node) {
  if (root == nullptr || node == nullptr) {
    return false;
  }

  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    if (cursor == root) {
      return true;
    }
  }
  return false;
}

bool isReadOnlyDeviceLoadCandidate(SgExpression *expr,
                                   SgBasicBlock *kernel_body) {
  if (expr == nullptr || kernel_body == nullptr ||
      !isScalarizableDirectGridStrideElementType(expr->get_type()) ||
      isExpressionWrittenThroughChain(expr) || isExpressionAddressTaken(expr)) {
    return false;
  }

  SgVariableSymbol *base_sym = extractClauseVariableSymbol(expr);
  if (base_sym == nullptr) {
    return false;
  }

  std::set<SgVariableSymbol *> visiting_syms;
  return symbolIsReadOnlyDevicePointer(base_sym, kernel_body, visiting_syms);
}

SgExpression *buildReadOnlyDeviceLoadExpr(SgExpression *expr,
                                          SgScopeStatement *scope) {
  ROSE_ASSERT(expr != nullptr);
  ROSE_ASSERT(scope != nullptr);

  SgType *value_type = stripTypeAliasesAndReferences(expr->get_type());
  ROSE_ASSERT(value_type != nullptr);

  SgExpression *loaded_expression = copyExpression(expr);
  SgAddressOfOp *address = buildExactAddressOfOp(loaded_expression);
  return buildFunctionCallExp("__ldg", value_type, buildExprListExp(address),
                              scope);
}

void rewriteReadOnlyDeviceLoadsWithLdg(SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    return;
  }

  SgBasicBlock *outer_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  if (outer_body == nullptr) {
    return;
  }

  std::vector<SgExpression *> candidates;
  Rose_STL_Container<SgNode *> expr_nodes =
      NodeQuery::querySubTree(outer_body, V_SgExpression);
  for (Rose_STL_Container<SgNode *>::const_iterator it = expr_nodes.begin();
       it != expr_nodes.end(); ++it) {
    SgExpression *expr = isSgExpression(*it);
    if (expr == nullptr) {
      continue;
    }

    if (isSgPntrArrRefExp(expr) == nullptr && isSgDotExp(expr) == nullptr &&
        isSgArrowExp(expr) == nullptr && isSgPointerDerefExp(expr) == nullptr) {
      continue;
    }

    if (!isReadOnlyDeviceLoadCandidate(expr, outer_body)) {
      continue;
    }
    candidates.push_back(expr);
  }

  std::sort(candidates.begin(), candidates.end(),
            [](SgExpression *lhs, SgExpression *rhs) {
              return computeAstDepth(lhs) > computeAstDepth(rhs);
            });
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());

  for (std::vector<SgExpression *>::const_iterator it = candidates.begin();
       it != candidates.end(); ++it) {
    SgExpression *expr = *it;
    if (!isNodeWithinSubtree(outer_body, expr)) {
      continue;
    }

    SgScopeStatement *scope = getEnclosingScope(expr);
    if (scope == nullptr) {
      scope = outer_body;
    }

    replaceExpression(expr, buildReadOnlyDeviceLoadExpr(expr, scope));
  }
}

bool candidateCoversAllBaseUses(SgBasicBlock *body, SgVariableSymbol *base_sym,
                                const std::vector<SgPntrArrRefExp *> &refs) {
  if (body == nullptr || base_sym == nullptr) {
    return false;
  }

  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = var_refs.begin();
       it != var_refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == nullptr || ref->get_symbol() != base_sym) {
      continue;
    }
    bool covered = false;
    for (std::vector<SgPntrArrRefExp *>::const_iterator ref_it = refs.begin();
         ref_it != refs.end(); ++ref_it) {
      if (isAncestor(*ref_it, ref)) {
        covered = true;
        break;
      }
    }
    if (!covered) {
      return false;
    }
  }
  return true;
}

bool symbolWrittenInsideScope(SgBasicBlock *body, SgVariableSymbol *sym) {
  if (body == nullptr || sym == nullptr) {
    return false;
  }

  Rose_STL_Container<SgNode *> var_refs =
      NodeQuery::querySubTree(body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = var_refs.begin();
       it != var_refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == nullptr || ref->get_symbol() != sym) {
      continue;
    }
    if (isWriteUseOfExpression(ref)) {
      return true;
    }
  }
  return false;
}

bool isClosestEnclosingLoop(SgForStatement *loop, SgNode *node) {
  if (loop == nullptr || node == nullptr) {
    return false;
  }
  SgStatement *stmt = getEnclosingStatement(node);
  if (stmt == nullptr) {
    return false;
  }
  return findEnclosingLoop(stmt) == loop;
}

struct DirectGridStrideScalarCandidate {
  SgVariableSymbol *base_sym = nullptr;
  SgType *element_type = nullptr;
  std::vector<SgPntrArrRefExp *> refs;
  bool has_write = false;
  bool disallowed = false;
};

void scalarizeDirectGridStrideOuterIndexAccesses(
    SgForStatement *outer_loop, SgVariableSymbol *outer_index_sym) {
  if (outer_loop == nullptr || outer_index_sym == nullptr) {
    return;
  }

  SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  if (containsUnsupportedDirectGridStrideControlFlow(loop_body)) {
    return;
  }

  std::map<SgVariableSymbol *, DirectGridStrideScalarCandidate> candidates;
  Rose_STL_Container<SgNode *> refs =
      NodeQuery::querySubTree(loop_body, V_SgPntrArrRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = refs.begin();
       it != refs.end(); ++it) {
    SgPntrArrRefExp *aref = isSgPntrArrRefExp(*it);
    if (aref == nullptr ||
        !isDirectVarRefToSymbol(aref->get_rhs_operand(), outer_index_sym)) {
      continue;
    }

    SgVariableSymbol *base_sym =
        extractClauseVariableSymbol(aref->get_lhs_operand());
    if (base_sym == nullptr) {
      continue;
    }

    DirectGridStrideScalarCandidate &candidate = candidates[base_sym];
    candidate.base_sym = base_sym;
    if (candidate.element_type == nullptr) {
      candidate.element_type = stripTypeAliasesAndReferences(aref->get_type());
    }
    candidate.refs.push_back(aref);
    candidate.has_write =
        candidate.has_write || isExpressionWrittenThroughChain(aref);
    candidate.disallowed =
        candidate.disallowed || isExpressionAddressTaken(aref) ||
        !isScalarizableDirectGridStrideElementType(aref->get_type());
  }

  for (std::map<SgVariableSymbol *, DirectGridStrideScalarCandidate>::iterator
           it = candidates.begin();
       it != candidates.end(); ++it) {
    DirectGridStrideScalarCandidate &candidate = it->second;
    if (candidate.disallowed || candidate.refs.empty()) {
      continue;
    }
    if (candidate.refs.size() < 2 && !candidate.has_write) {
      continue;
    }
    if (!candidateCoversAllBaseUses(loop_body, candidate.base_sym,
                                    candidate.refs)) {
      continue;
    }

    std::string cache_name = generateUniqueVariableName(
        loop_body,
        "__rex_cached_" + candidate.base_sym->get_name().getString() + "_");
    SgType *cache_type = candidate.has_write
                             ? candidate.element_type
                             : buildConstType(candidate.element_type);
    SgExpression *init_expr = copyExpression(candidate.refs.front());
    SgVariableDeclaration *cache_decl = buildVariableDeclaration(
        cache_name, cache_type, buildAssignInitializer(init_expr, cache_type),
        loop_body);
    prependStatement(cache_decl, loop_body);
    SgVariableSymbol *cache_sym = getFirstVarSym(cache_decl);
    ROSE_ASSERT(cache_sym != nullptr);

    SgExpression *writeback_lhs = nullptr;
    if (candidate.has_write) {
      writeback_lhs = copyExpression(candidate.refs.front());
    }

    for (std::vector<SgPntrArrRefExp *>::reverse_iterator ref_it =
             candidate.refs.rbegin();
         ref_it != candidate.refs.rend(); ++ref_it) {
      replaceExpression(*ref_it, buildVarRefExp(cache_sym));
    }

    if (candidate.has_write && writeback_lhs != nullptr) {
      appendStatement(
          buildAssignStatement(writeback_lhs, buildVarRefExp(cache_sym)),
          loop_body);
    }
  }
}

struct InvariantAggregateRefKey {
  SgVariableSymbol *base_sym = nullptr;
  SgVariableSymbol *index_sym = nullptr;

  bool operator<(const InvariantAggregateRefKey &other) const {
    if (base_sym != other.base_sym) {
      return base_sym < other.base_sym;
    }
    return index_sym < other.index_sym;
  }
};

struct InvariantAggregateRefCandidate {
  SgVariableSymbol *base_sym = nullptr;
  SgVariableSymbol *index_sym = nullptr;
  SgType *element_type = nullptr;
  std::vector<SgPntrArrRefExp *> refs;
  bool disallowed = false;
};

struct InvariantFieldAccessKey {
  SgVariableSymbol *base_sym = nullptr;
  SgSymbol *field_sym = nullptr;

  bool operator<(const InvariantFieldAccessKey &other) const {
    if (base_sym != other.base_sym) {
      return base_sym < other.base_sym;
    }
    return field_sym < other.field_sym;
  }
};

struct InvariantFieldAccessCandidate {
  SgVariableSymbol *base_sym = nullptr;
  SgSymbol *field_sym = nullptr;
  SgType *cache_type = nullptr;
  std::vector<SgExpression *> refs;
  bool disallowed = false;
};

void hoistReadOnlyInvariantAggregateRefsBeforeLoop(SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    return;
  }

  SgBasicBlock *outer_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(outer_body, V_SgForStatement);
  for (Rose_STL_Container<SgNode *>::const_iterator loop_it = loops.begin();
       loop_it != loops.end(); ++loop_it) {
    SgForStatement *loop = isSgForStatement(*loop_it);
    if (loop == nullptr || loop == outer_loop) {
      continue;
    }

    SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(loop);
    SgBasicBlock *parent_block = isSgBasicBlock(loop->get_parent());
    if (loop_body == nullptr || parent_block == nullptr) {
      continue;
    }

    std::map<InvariantAggregateRefKey, InvariantAggregateRefCandidate>
        candidates;
    Rose_STL_Container<SgNode *> refs =
        NodeQuery::querySubTree(loop_body, V_SgPntrArrRefExp);
    for (Rose_STL_Container<SgNode *>::const_iterator ref_it = refs.begin();
         ref_it != refs.end(); ++ref_it) {
      SgPntrArrRefExp *aref = isSgPntrArrRefExp(*ref_it);
      if (aref == nullptr || !isClosestEnclosingLoop(loop, aref) ||
          !isAggregateDirectGridStrideElementType(aref->get_type())) {
        continue;
      }

      SgVariableSymbol *index_sym = nullptr;
      SgVarRefExp *index_ref =
          isSgVarRefExp(stripNoopCastsAndParens(aref->get_rhs_operand()));
      if (index_ref != nullptr) {
        index_sym = isSgVariableSymbol(index_ref->get_symbol());
      }
      SgVariableSymbol *base_sym =
          extractClauseVariableSymbol(aref->get_lhs_operand());
      if (base_sym == nullptr || index_sym == nullptr) {
        continue;
      }

      InvariantAggregateRefKey key;
      key.base_sym = base_sym;
      key.index_sym = index_sym;
      InvariantAggregateRefCandidate &candidate = candidates[key];
      candidate.base_sym = base_sym;
      candidate.index_sym = index_sym;
      if (candidate.element_type == nullptr) {
        candidate.element_type =
            stripTypeAliasesAndReferences(aref->get_type());
      }
      candidate.refs.push_back(aref);
      candidate.disallowed = candidate.disallowed ||
                             isExpressionAddressTaken(aref) ||
                             isExpressionWrittenThroughChain(aref);
    }

    for (std::map<InvariantAggregateRefKey,
                  InvariantAggregateRefCandidate>::iterator cand_it =
             candidates.begin();
         cand_it != candidates.end(); ++cand_it) {
      InvariantAggregateRefCandidate &candidate = cand_it->second;
      if (candidate.disallowed || candidate.refs.size() < 2 ||
          symbolWrittenInsideScope(loop_body, candidate.index_sym)) {
        continue;
      }

      std::string cache_name = generateUniqueVariableName(
          parent_block,
          "__rex_ref_" + candidate.base_sym->get_name().getString() + "_");
      SgType *ptr_type =
          buildPointerType(buildConstType(candidate.element_type));
      SgInitializedName *base_name = candidate.base_sym->get_declaration();
      SgType *source_base_type = getExactCxxSourceType(
          base_name, "invariant-aggregate-reference-source-type");
      SgType *source_element_type = getOmpArraySectionSourceElementType(
          source_base_type, candidate.element_type, 1,
          "invariant-aggregate-reference-source-type");
      SgType *source_ptr_type =
          buildPointerType(buildConstType(source_element_type));
      if (!SageInterface::isEquivalentType(ptr_type, source_ptr_type)) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[invariant-aggregate-"
                     "reference-source-type]: cached reference has "
                     "inequivalent semantic and source pointer types"
                  << std::endl;
        ROSE_ABORT();
      }
      SgExpression *cached_expression = copyExpression(candidate.refs.front());
      SgExpression *init_expr = buildExactAddressOfOp(cached_expression);
      SgVariableDeclaration *cache_decl = buildVariableDeclaration(
          cache_name, ptr_type, buildAssignInitializer(init_expr, ptr_type),
          parent_block);
      SgInitializedName *cache_name_decl =
          SageInterface::getFirstInitializedName(cache_decl);
      if (cache_name_decl == nullptr ||
          cache_name_decl->get_type() != ptr_type) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[invariant-aggregate-"
                     "reference-source-type]: cache declaration has no exact "
                     "semantic declarator"
                  << std::endl;
        ROSE_ABORT();
      }
      if (source_ptr_type != ptr_type) {
        cache_name_decl->set_cxx_source_type(source_ptr_type);
        if (cache_name_decl->get_cxx_source_type() != source_ptr_type) {
          ROSE_ABORT();
        }
      }
      insertStatementBefore(loop, cache_decl);
      SgVariableSymbol *cache_sym = getFirstVarSym(cache_decl);
      ROSE_ASSERT(cache_sym != nullptr);

      for (std::vector<SgPntrArrRefExp *>::reverse_iterator ref_it =
               candidate.refs.rbegin();
           ref_it != candidate.refs.rend(); ++ref_it) {
        SgPointerType *cache_pointer_type =
            isSgPointerType(stripTypeAliases(cache_sym->get_type()));
        ROSE_ASSERT(cache_pointer_type != nullptr);
        replaceExpression(
            *ref_it, buildPointerDerefExp(buildVarRefExp(cache_sym),
                                          cache_pointer_type->get_base_type()));
      }
    }
  }
}

void hoistReadOnlyInvariantFieldAccessesBeforeLoop(SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    return;
  }

  SgBasicBlock *outer_body = ensureBasicBlockAsBodyOfFor(outer_loop);
  Rose_STL_Container<SgNode *> loops =
      NodeQuery::querySubTree(outer_body, V_SgForStatement);
  for (Rose_STL_Container<SgNode *>::const_iterator loop_it = loops.begin();
       loop_it != loops.end(); ++loop_it) {
    SgForStatement *loop = isSgForStatement(*loop_it);
    if (loop == nullptr || loop == outer_loop) {
      continue;
    }

    SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(loop);
    SgBasicBlock *parent_block = isSgBasicBlock(loop->get_parent());
    if (loop_body == nullptr || parent_block == nullptr) {
      continue;
    }

    std::map<InvariantFieldAccessKey, InvariantFieldAccessCandidate> candidates;
    Rose_STL_Container<SgNode *> field_refs =
        NodeQuery::querySubTree(loop_body, V_SgDotExp);
    for (Rose_STL_Container<SgNode *>::const_iterator ref_it =
             field_refs.begin();
         ref_it != field_refs.end(); ++ref_it) {
      SgDotExp *dot = isSgDotExp(*ref_it);
      if (dot == nullptr || !isClosestEnclosingLoop(loop, dot)) {
        continue;
      }

      SgVarRefExp *base_ref = nullptr;
      size_t deref_depth = 0;
      if (!extractPointerDerefChain(dot->get_lhs_operand(), base_ref,
                                    deref_depth) ||
          base_ref == nullptr || deref_depth != 1) {
        continue;
      }

      SgSymbol *field_sym = nullptr;
      if (SgVarRefExp *field_ref =
              isSgVarRefExp(stripNoopCastsAndParens(dot->get_rhs_operand()))) {
        field_sym = field_ref->get_symbol();
      }
      if (field_sym == nullptr) {
        continue;
      }

      SgType *raw_field_type = stripTypeAliases(dot->get_type());
      if (raw_field_type == nullptr) {
        continue;
      }

      SgType *cache_type = nullptr;
      if (SgArrayType *array_type = isSgArrayType(raw_field_type)) {
        cache_type =
            buildPointerType(buildConstType(array_type->findBaseType()));
      } else {
        SgType *field_type = stripTypeAliasesAndReferences(raw_field_type);
        if (field_type == nullptr || isSgClassType(field_type) != nullptr ||
            isSgTypeVoid(field_type) != nullptr) {
          continue;
        }
        cache_type = buildConstType(field_type);
      }

      InvariantFieldAccessKey key;
      key.base_sym = isSgVariableSymbol(base_ref->get_symbol());
      key.field_sym = field_sym;
      InvariantFieldAccessCandidate &candidate = candidates[key];
      candidate.base_sym = key.base_sym;
      candidate.field_sym = field_sym;
      if (candidate.cache_type == nullptr) {
        candidate.cache_type = cache_type;
      }
      candidate.refs.push_back(dot);
      candidate.disallowed = candidate.disallowed ||
                             isExpressionAddressTaken(dot) ||
                             isExpressionWrittenThroughChain(dot);
    }

    for (std::map<InvariantFieldAccessKey,
                  InvariantFieldAccessCandidate>::iterator cand_it =
             candidates.begin();
         cand_it != candidates.end(); ++cand_it) {
      InvariantFieldAccessCandidate &candidate = cand_it->second;
      if (candidate.disallowed || candidate.refs.size() < 2 ||
          candidate.base_sym == nullptr || candidate.cache_type == nullptr ||
          symbolWrittenInsideScope(loop_body, candidate.base_sym)) {
        continue;
      }

      SgVariableSymbol *field_var_sym = isSgVariableSymbol(candidate.field_sym);
      const std::string field_name = field_var_sym != nullptr
                                         ? field_var_sym->get_name().getString()
                                         : std::string("field");
      std::string cache_name = generateUniqueVariableName(
          parent_block, "__rex_field_" + field_name + "_");
      SgVariableDeclaration *cache_decl = buildVariableDeclaration(
          cache_name, candidate.cache_type,
          buildAssignInitializer(copyExpression(candidate.refs.front()),
                                 candidate.cache_type),
          parent_block);
      insertStatementBefore(loop, cache_decl);
      SgVariableSymbol *cache_sym = getFirstVarSym(cache_decl);
      ROSE_ASSERT(cache_sym != nullptr);

      for (std::vector<SgExpression *>::reverse_iterator ref_it =
               candidate.refs.rbegin();
           ref_it != candidate.refs.rend(); ++ref_it) {
        replaceExpression(*ref_it, buildVarRefExp(cache_sym));
      }
    }
  }
}

bool is_32_bit_target(const SgNode *context) {
  SgProject *project = SageInterface::getProject(context);
  ROSE_ASSERT(project != nullptr);
  return project->get_mode_32_bit();
}

StructLayoutInfo get_target_layout_info(SgType *type, const SgNode *context) {
  ROSE_ASSERT(type != nullptr);

  if (is_32_bit_target(context)) {
    I386PrimitiveTypeLayoutGenerator primitive_generator(nullptr);
    NonpackedTypeLayoutGenerator layout_generator(&primitive_generator);
    return layout_generator.layoutType(type);
  }

  X86_64PrimitiveTypeLayoutGenerator primitive_generator(nullptr);
  NonpackedTypeLayoutGenerator layout_generator(&primitive_generator);
  return layout_generator.layoutType(type);
}

size_t get_target_type_size_bytes(SgType *type, const SgNode *context) {
  StructLayoutInfo layout = get_target_layout_info(type, context);
  ROSE_ASSERT(layout.size > 0);
  return layout.size;
}

bool use_kmpc_loop_64bit_runtime(SgType *loop_var_type, const SgNode *context) {
  return get_target_type_size_bytes(loop_var_type, context) > 4;
}

SgType *getRuntimeInt64Type(SgScopeStatement *scope) {
  if (scope == nullptr) {
    MLOG_ERROR_CXX("ompLowering")
        << "Runtime signed 64-bit type selection has no target context";
    ROSE_ABORT();
  }

  SgType *runtime_base_type = buildLongType();
  if (runtime_base_type == nullptr) {
    MLOG_ERROR_CXX("ompLowering")
        << "Target ABI did not provide the signed long builtin type";
    ROSE_ABORT();
  }
  if (get_target_type_size_bytes(runtime_base_type, scope) != 8) {
    runtime_base_type = buildLongLongType();
    if (runtime_base_type == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Target ABI did not provide the signed long long builtin type";
      ROSE_ABORT();
    }
  }
  if (get_target_type_size_bytes(runtime_base_type, scope) != 8) {
    MLOG_ERROR_CXX("ompLowering")
        << "Target ABI has neither an exact signed 64-bit long nor signed "
           "64-bit long long builtin type";
    ROSE_ABORT();
  }

  // The runtime ABI requires a signed 64-bit value, not a source spelling
  // named int64_t.  Publishing a synthetic typedef under that ordinary name
  // either hijacks a user declaration or makes lowering depend on a hidden
  // unparsed declaration.  Preserve the exact target builtin identity chosen
  // above; all generated declarations and casts therefore carry a complete
  // semantic type without introducing any lexical or auxiliary alias.
  return runtime_base_type;
}

const char *get_kmpc_for_static_init_name(bool use_64_runtime) {
  return use_64_runtime ? "__kmpc_for_static_init_8"
                        : "__kmpc_for_static_init_4";
}

const char *get_kmpc_dispatch_init_name(bool use_64_runtime) {
  return use_64_runtime ? "__kmpc_dispatch_init_8" : "__kmpc_dispatch_init_4";
}

const char *get_kmpc_dispatch_next_name(bool use_64_runtime) {
  return use_64_runtime ? "__kmpc_dispatch_next_8" : "__kmpc_dispatch_next_4";
}

SgType *resolvePointerBaseType(SgType *pointer_type, size_t deref_depth) {
  SgType *result = pointer_type;
  for (size_t i = 0; i < deref_depth; ++i) {
    result = stripTypeAliases(result);
    SgPointerType *ptr = isSgPointerType(result);
    if (ptr == nullptr) {
      return nullptr;
    }
    result = ptr->get_base_type();
  }
  return stripTypeAliases(result);
}

bool buildExpressionMatchingTypeFromActiveSymbol(
    SgVariableSymbol *active_symbol, SgType *expected_type,
    SgExpression *&value_expr) {
  ROSE_ASSERT(active_symbol != nullptr);
  ROSE_ASSERT(expected_type != nullptr);

  SgType *expected = stripTypeAliases(expected_type);
  ROSE_ASSERT(expected != nullptr);

  SgExpression *candidate = buildVarRefExp(active_symbol);
  SgType *current = stripTypeAliases(active_symbol->get_type());

  while (current != nullptr) {
    if (current == expected) {
      value_expr = candidate;
      return true;
    }

    if (SgReferenceType *ref_type = isSgReferenceType(current)) {
      current = stripTypeAliases(ref_type->get_base_type());
      continue;
    }

    SgPointerType *ptr_type = isSgPointerType(current);
    if (ptr_type == nullptr)
      break;

    candidate = buildPointerDerefExp(candidate, ptr_type->get_base_type());
    current = stripTypeAliases(ptr_type->get_base_type());
  }

  return false;
}

struct ResolvedMapperInfo {
  SgOmpDeclareMapperStatement *declaration = nullptr;
  std::string identifier_text;
  std::string formal_name;
  SgVariableSymbol *formal_symbol = nullptr;
  SgType *formal_type = nullptr;
};

enum class MapperUseKind { map_clause, to_clause, from_clause };

struct ResolvedMapItem {
  SgExpression *expression = nullptr;
  SgOmpClause::omp_map_operator_enum map_operator =
      SgOmpClause::e_omp_map_unknown;
  int runtime_flag_bits = 0;
  bool is_implicit_base_pointer = false;
  bool is_implicit_target_variable = false;
  bool use_literal_target_param = false;
  SgVariableSymbol *direct_variable_symbol = nullptr;
};

enum class ExpandedMapEntryKind { direct_item, dynamic_mapper_section };

struct OmpArraySectionDimension {
  SgSubscriptExpression *syntax = nullptr;
  SgExpression *lower = nullptr;
  SgExpression *length = nullptr;
};

struct ResolvedOmpArraySectionDimension {
  SgExpression *lower = nullptr;
  SgExpression *length = nullptr;
  SgExpression *declared_extent = nullptr;
  bool lower_omitted = false;
  bool length_omitted = false;
};

struct ExpandedMapEntry {
  ExpandedMapEntryKind kind = ExpandedMapEntryKind::direct_item;
  ResolvedMapItem direct_item;
  ResolvedMapperInfo resolved_mapper;
  SgExpression *section_base_expression = nullptr;
  std::vector<OmpArraySectionDimension> section_dimensions;
  MapperUseKind use_kind = MapperUseKind::map_clause;
  SgOmpClause::omp_map_operator_enum use_map_op =
      SgOmpClause::e_omp_map_unknown;
  int runtime_flag_bits = 0;
  SgStatement *anchor_stmt = nullptr;
};

SgVariableSymbol *getDirectResolvedMapItemVariableSymbol(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  SgVarRefExp *var_ref = isSgVarRefExp(stripNoopCastsAndParens(expr));
  if (var_ref == nullptr) {
    return nullptr;
  }

  return isSgVariableSymbol(var_ref->get_symbol());
}

SgExpression *buildLiteralTargetParamArgExpression(SgVariableSymbol *var_sym,
                                                   SgScopeStatement *scope) {
  ROSE_ASSERT(var_sym != NULL);
  ROSE_ASSERT(scope != NULL);

  SgType *type = stripTypeAliasesAndReferences(var_sym->get_type());
  ROSE_ASSERT(type != NULL);

  SgExpression *argument = buildVarRefExp(var_sym);
  SgAddressOfOp *address = buildExactAddressOfOp(argument);
  return buildFunctionCallExp(
      "rex_pack_literal_arg_bytes", buildPointerType(buildVoidType()),
      buildExprListExp(
          address,
          buildSizeOfOp(type, SageInterface::requireTargetSizeType(scope))),
      scope);
}

std::string trimMapperCopy(const std::string &value) {
  const std::string::size_type begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const std::string::size_type end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

[[noreturn]] void failMapperLoweringIdentity(const char *reason) {
  std::cerr << "REX_OMP_LOWERING_INVARIANT[mapper-identity]: " << reason
            << std::endl;
  ROSE_ABORT();
}

std::string getVarRefNameText(const SgVarRefExp *vref) {
  if (vref == nullptr) {
    failMapperLoweringIdentity("expected an exact SgVarRefExp");
  }
  if (vref->get_symbol() == nullptr) {
    failMapperLoweringIdentity("variable reference has no semantic symbol");
  }
  const std::string name = vref->get_symbol()->get_name().getString();
  if (name.empty()) {
    failMapperLoweringIdentity("variable symbol has an empty semantic name");
  }
  return name;
}

std::string normalizeMapperIdentifierString(const std::string &value) {
  std::string trimmed = trimMapperCopy(value);
  if (SageInterface::is_Fortran_language()) {
    std::transform(
        trimmed.begin(), trimmed.end(), trimmed.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  }
  return trimmed;
}

std::string getMapperIdentifierText(const SgExpression *expr) {
  if (expr == nullptr) {
    return "";
  }
  const SgOmpNameExpression *identifier = isSgOmpNameExpression(expr);
  if (identifier == nullptr) {
    failMapperLoweringIdentity(
        "mapper identifier is not an exact SgOmpNameExpression");
  }
  if (identifier->get_spelling().empty()) {
    failMapperLoweringIdentity("mapper identifier spelling is empty");
  }
  return normalizeMapperIdentifierString(identifier->get_spelling());
}

bool isDefaultDeclareMapperIdentifier(
    SgOmpClause::omp_declare_mapper_identifier_enum identifier) {
  switch (identifier) {
  case SgOmpClause::e_omp_declare_mapper_identifier_default:
    return true;
  case SgOmpClause::e_omp_declare_mapper_identifier_user:
    return false;
  case SgOmpClause::e_omp_declare_mapper_identifier_unspecified:
  default:
    failMapperLoweringIdentity(
        "declare mapper identifier kind is unspecified or invalid");
  }
}

std::string
getDeclareMapperFormalName(const SgOmpDeclareMapperStatement *mapper_stmt) {
  if (mapper_stmt == nullptr) {
    failMapperLoweringIdentity("declare mapper is null");
  }
  SgExpression *mapper_variable = mapper_stmt->get_mapper_variable();
  if (mapper_variable == nullptr) {
    failMapperLoweringIdentity("declare mapper has no formal variable");
  }
  const SgVarRefExp *vref = isSgVarRefExp(mapper_variable);
  if (vref == nullptr) {
    failMapperLoweringIdentity(
        "declare mapper formal variable is not an exact SgVarRefExp");
  }
  if (vref->get_parent() != mapper_stmt) {
    failMapperLoweringIdentity(
        "declare mapper does not structurally own its formal variable");
  }
  return getVarRefNameText(vref);
}

SgVariableSymbol *
getDeclareMapperFormalSymbol(const SgOmpDeclareMapperStatement *mapper_stmt) {
  if (mapper_stmt == nullptr) {
    failMapperLoweringIdentity("declare mapper is null");
  }
  SgVarRefExp *formal = isSgVarRefExp(mapper_stmt->get_mapper_variable());
  if (formal == nullptr || formal->get_parent() != mapper_stmt ||
      formal->get_symbol() == nullptr ||
      formal->get_symbol()->get_declaration() == nullptr) {
    failMapperLoweringIdentity(
        "declare mapper formal variable has no exact symbol identity");
  }
  return formal->get_symbol();
}

SgType *
getDeclareMapperFormalType(const SgOmpDeclareMapperStatement *mapper_stmt) {
  if (mapper_stmt == nullptr) {
    failMapperLoweringIdentity("declare mapper is null");
  }
  SgExpression *mapper_type = mapper_stmt->get_mapper_type();
  if (mapper_type == nullptr) {
    failMapperLoweringIdentity("declare mapper has no formal type");
  }
  SgTypeExpression *type_expr = isSgTypeExpression(mapper_type);
  if (type_expr == nullptr) {
    failMapperLoweringIdentity(
        "declare mapper formal type is not an exact SgTypeExpression");
  }
  if (type_expr->get_parent() != mapper_stmt) {
    failMapperLoweringIdentity(
        "declare mapper does not structurally own its formal type");
  }
  if (type_expr->get_represented_type() == nullptr ||
      isSgTypeUnknown(type_expr->get_represented_type()) != nullptr) {
    failMapperLoweringIdentity(
        "declare mapper formal type has no exact semantic type");
  }
  return type_expr->get_represented_type();
}

SgStatement *findDirectChildStatementInScope(SgStatement *anchor,
                                             SgScopeStatement *scope) {
  if (anchor == nullptr || scope == nullptr) {
    failMapperLoweringIdentity(
        "mapper lookup requires a nonnull anchor and lexical scope");
  }

  SgNode *cursor = anchor;
  while (cursor != nullptr && cursor->get_parent() != scope) {
    cursor = cursor->get_parent();
  }
  SgStatement *result = isSgStatement(cursor);
  if (result == nullptr) {
    failMapperLoweringIdentity(
        "mapper lookup anchor is not structurally owned by its lexical scope");
  }
  return result;
}

bool collectEffectiveArraySectionDimensions(
    SgExpression *expression,
    std::vector<OmpArraySectionDimension> &dimensions) {
  if (expression == nullptr) {
    return false;
  }

  if (SgOmpMapItem *item = isSgOmpMapItem(expression)) {
    if (item->get_expression() == nullptr ||
        item->get_expression()->get_parent() != item) {
      failMapperLoweringIdentity(
          "typed map item has no exactly owned locator expression");
    }
    return collectEffectiveArraySectionDimensions(item->get_expression(),
                                                  dimensions);
  }

  if (SgCastExp *cast_exp = isSgCastExp(expression)) {
    return collectEffectiveArraySectionDimensions(cast_exp->get_operand(),
                                                  dimensions);
  }

  if (SgUnaryOp *unary_op = isSgUnaryOp(expression)) {
    return collectEffectiveArraySectionDimensions(unary_op->get_operand(),
                                                  dimensions);
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(expression)) {
    if (!collectEffectiveArraySectionDimensions(array_ref->get_lhs_operand(),
                                                dimensions)) {
      return false;
    }

    if (SgSubscriptExpression *subscript =
            isSgSubscriptExpression(array_ref->get_rhs_operand())) {
      SgExpression *lower = subscript->get_lowerBound();
      SgExpression *length = subscript->get_upperBound();
      SgExpression *stride = subscript->get_stride();
      if (subscript->get_parent() != array_ref || lower == nullptr ||
          length == nullptr || stride == nullptr ||
          lower->get_parent() != subscript ||
          length->get_parent() != subscript ||
          stride->get_parent() != subscript) {
        failMapperLoweringIdentity(
            "array section has missing or incorrectly owned typed bounds");
      }
      if (isSgNullExpression(stride) == nullptr) {
        const SgIntVal *int_stride = isSgIntVal(stride);
        if (int_stride == nullptr || int_stride->get_value() != 1) {
          failMapperLoweringIdentity(
              "array section has an unsupported non-unit stride");
        }
      }
      dimensions.push_back({subscript, lower, length});
    }
    return true;
  }

  return true;
}

bool isArraySectionReference(SgExpression *expr) {
  std::vector<OmpArraySectionDimension> dims;
  return collectEffectiveArraySectionDimensions(expr, dims) && !dims.empty();
}

struct OmpMapItemLoweringRecord {
  SgOmpMapClause *clause = nullptr;
  SgOmpMapItem *item = nullptr;
  size_t clause_item_index = 0;
  size_t directive_item_index = 0;
  SgVariableSymbol *symbol = nullptr;
  SgOmpClause::omp_map_operator_enum operation = SgOmpClause::e_omp_map_unknown;
  std::vector<OmpArraySectionDimension> dimensions;
};

void appendOwnedMapItemLoweringRecords(
    SgOmpMapClause *clause, std::vector<OmpMapItemLoweringRecord> &records) {
  if (clause == nullptr || clause->get_variables() == nullptr ||
      clause->get_variables()->get_parent() != clause) {
    failMapperLoweringIdentity(
        "map clause has no exactly owned semantic variable list");
  }

  SgExprListExp *variables = clause->get_variables();
  const SgExpressionPtrList &expressions = variables->get_expressions();
  for (size_t item_index = 0; item_index < expressions.size(); ++item_index) {
    SgOmpMapItem *item = isSgOmpMapItem(expressions[item_index]);
    if (item == nullptr || item->get_parent() != variables ||
        item->get_expression() == nullptr ||
        item->get_expression()->get_parent() != item) {
      failMapperLoweringIdentity(
          "map clause contains a malformed or unowned typed map item");
    }

    OmpMapItemLoweringRecord record;
    record.clause = clause;
    record.item = item;
    record.clause_item_index = item_index;
    record.directive_item_index = records.size();
    record.symbol = extractClauseVariableSymbol(item);
    record.operation = clause->get_operation();
    if (record.symbol == nullptr) {
      failMapperLoweringIdentity(
          "map clause item has no exact semantic base symbol");
    }
    collectEffectiveArraySectionDimensions(item, record.dimensions);
    records.push_back(std::move(record));
  }
}

SgExpression *buildArraySectionBaseExpression(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  if (SgOmpMapItem *item = isSgOmpMapItem(expr)) {
    return buildArraySectionBaseExpression(item->get_expression());
  }

  if (SgCastExp *cast_exp = isSgCastExp(expr)) {
    return buildArraySectionBaseExpression(cast_exp->get_operand());
  }

  if (SgUnaryOp *unary_op = isSgUnaryOp(expr)) {
    return buildArraySectionBaseExpression(unary_op->get_operand());
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(expr)) {
    if (isArraySectionReference(array_ref)) {
      return buildArraySectionBaseExpression(array_ref->get_lhs_operand());
    }
  }

  return copyExpression(expr);
}

std::vector<SgExpression *>
getDeclaredArrayDimensionExtents(SgType *type, size_t syntax_dimension_count) {
  SgType *current = stripTypeAliasesAndReferences(type);
  std::vector<SgExpression *> result;
  while (result.size() < syntax_dimension_count) {
    SgPointerType *pointer_type = isSgPointerType(current);
    if (pointer_type == nullptr) {
      break;
    }
    result.push_back(nullptr);
    current = stripTypeAliasesAndReferences(pointer_type->get_base_type());
  }

  while (SgArrayType *array_type = isSgArrayType(current)) {
    result.push_back(array_type->get_index());
    current = stripTypeAliasesAndReferences(array_type->get_base_type());
  }
  return result;
}

SgType *getOmpArraySectionElementType(SgType *base_type, size_t dimension_count,
                                      const char *contract) {
  SgType *current = stripTypeAliasesAndReferences(base_type);
  bool traversed_pointer_dimension = false;
  for (size_t index = 0; index < dimension_count; ++index) {
    if (SgArrayType *array_type = isSgArrayType(current)) {
      current = stripTypeAliasesAndReferences(array_type->get_base_type());
      continue;
    }
    if (SgPointerType *pointer_type = isSgPointerType(current)) {
      if (traversed_pointer_dimension || index != 0) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
                  << "]: section crosses a noncontiguous pointer chain at "
                     "dimension "
                  << index << std::endl;
        ROSE_ABORT();
      }
      traversed_pointer_dimension = true;
      current = stripTypeAliasesAndReferences(pointer_type->get_base_type());
      continue;
    }
    std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
              << "]: section dimension " << index
              << " has no matching array or pointer type" << std::endl;
    ROSE_ABORT();
  }
  if (current == nullptr || isSgTypeVoid(current) != nullptr) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
              << "]: section has no sized semantic element type" << std::endl;
    ROSE_ABORT();
  }
  return current;
}

SgType *getExactCxxSourceType(const SgInitializedName *name,
                              const char *contract) {
  SgType *semantic_type = name != nullptr ? name->get_type() : nullptr;
  SgType *direct_source_type =
      name != nullptr ? name->get_cxx_source_type() : nullptr;
  if (name == nullptr || semantic_type == nullptr || contract == nullptr) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT["
              << (contract != nullptr ? contract : "<null>")
              << "]: initialized name has no exact semantic type context"
              << std::endl;
    ROSE_ABORT();
  }
  if (direct_source_type != nullptr) {
    if (direct_source_type == semantic_type ||
        !SageInterface::isEquivalentType(semantic_type, direct_source_type)) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
                << "]: initialized name " << name->get_name()
                << " has a non-distinct or inequivalent direct C++ source "
                   "type"
                << std::endl;
      ROSE_ABORT();
    }
    return direct_source_type;
  }

  SgFunctionParameterList *semantic_parameters =
      isSgFunctionParameterList(name->get_parent());
  SgFunctionDeclaration *function =
      semantic_parameters != nullptr
          ? isSgFunctionDeclaration(semantic_parameters->get_parent())
          : nullptr;
  if (function == nullptr ||
      function->get_parameterList() != semantic_parameters) {
    return semantic_type;
  }

  SgFunctionParameterList *source_parameters =
      function->get_parameterList_syntax();
  if (source_parameters == nullptr ||
      source_parameters == semantic_parameters) {
    return semantic_type;
  }

  const SgInitializedNamePtrList &semantic_args =
      semantic_parameters->get_args();
  const SgInitializedNamePtrList &source_args = source_parameters->get_args();
  const auto position =
      std::find(semantic_args.begin(), semantic_args.end(), name);
  const size_t index =
      position != semantic_args.end()
          ? static_cast<size_t>(std::distance(semantic_args.begin(), position))
          : semantic_args.size();
  SgInitializedName *source_name =
      index < source_args.size() ? source_args[index] : nullptr;
  SgType *source_type =
      source_name != nullptr ? source_name->get_type() : nullptr;
  if (position == semantic_args.end() ||
      semantic_args.size() != source_args.size() || source_name == nullptr ||
      source_name->get_parent() != source_parameters ||
      source_name->get_name() != name->get_name() || source_type == nullptr ||
      !SageInterface::isEquivalentType(semantic_type, source_type)) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract << "]: function "
              << function->get_name() << " parameter " << name->get_name()
              << " has no exact equivalent source-syntax pairing" << std::endl;
    ROSE_ABORT();
  }
  return source_type;
}

SgType *getOmpArraySectionSourceElementType(SgType *source_type,
                                            SgType *semantic_element_type,
                                            size_t dimension_count,
                                            const char *contract) {
  SgType *current = source_type;
  bool traversed_pointer_dimension = false;
  for (size_t index = 0; index < dimension_count; ++index) {
    // A typedef or qualifier can itself denote the array/pointer dimension.
    // Peel it only when it obstructs traversal.  Once every mapped dimension
    // has been consumed, the remaining alias is the exact source spelling of
    // the element and must be retained.
    while (current != nullptr && isSgArrayType(current) == nullptr &&
           isSgPointerType(current) == nullptr) {
      if (SgModifierType *modifier = isSgModifierType(current)) {
        current = modifier->get_base_type();
      } else if (SgTypedefType *typedef_type = isSgTypedefType(current)) {
        current = typedef_type->get_base_type();
      } else if (SgReferenceType *reference = isSgReferenceType(current)) {
        current = reference->get_base_type();
      } else {
        break;
      }
    }

    if (SgArrayType *array_type = isSgArrayType(current)) {
      current = array_type->get_base_type();
      continue;
    }
    if (SgPointerType *pointer_type = isSgPointerType(current)) {
      if (traversed_pointer_dimension || index != 0) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
                  << "]: source section crosses a noncontiguous pointer "
                     "chain at dimension "
                  << index << std::endl;
        ROSE_ABORT();
      }
      traversed_pointer_dimension = true;
      current = pointer_type->get_base_type();
      continue;
    }
    std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
              << "]: source section dimension " << index
              << " has no matching array or pointer type" << std::endl;
    ROSE_ABORT();
  }

  if (current == nullptr || semantic_element_type == nullptr ||
      isSgTypeVoid(stripTypeAliasesAndReferences(current)) != nullptr ||
      !SageInterface::isEquivalentType(current, semantic_element_type)) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
              << "]: source section has no exact equivalent, nameable "
                 "element type"
              << std::endl;
    ROSE_ABORT();
  }
  return current;
}

std::vector<ResolvedOmpArraySectionDimension> resolveOmpArraySectionDimensions(
    const std::vector<OmpArraySectionDimension> &syntax_dimensions,
    SgType *base_type, const char *contract) {
  const std::vector<SgExpression *> declared_extents =
      getDeclaredArrayDimensionExtents(base_type, syntax_dimensions.size());
  std::vector<ResolvedOmpArraySectionDimension> result;
  result.reserve(std::max(syntax_dimensions.size(), declared_extents.size()));

  for (size_t index = 0; index < syntax_dimensions.size(); ++index) {
    const OmpArraySectionDimension &syntax = syntax_dimensions[index];
    if (syntax.syntax == nullptr || syntax.lower == nullptr ||
        syntax.length == nullptr ||
        syntax.lower->get_parent() != syntax.syntax ||
        syntax.length->get_parent() != syntax.syntax) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
                << "]: section dimension " << index
                << " has no exact typed syntax ownership" << std::endl;
      ROSE_ABORT();
    }

    const bool omitted_lower = isSgNullExpression(syntax.lower) != nullptr;
    const bool omitted_length = isSgNullExpression(syntax.length) != nullptr;
    SgExpression *declared_extent =
        index < declared_extents.size() ? declared_extents[index] : nullptr;
    if (isSgNullExpression(declared_extent) != nullptr) {
      declared_extent = nullptr;
    }

    ResolvedOmpArraySectionDimension resolved;
    resolved.lower = syntax.lower;
    resolved.length = syntax.length;
    resolved.declared_extent = declared_extent;
    resolved.lower_omitted = omitted_lower;
    resolved.length_omitted = omitted_length;
    if (omitted_length && declared_extent == nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
                << "]: section dimension " << index
                << " omits its length without a declared array extent"
                << std::endl;
      ROSE_ABORT();
    }
    result.push_back(resolved);
  }

  for (size_t index = syntax_dimensions.size(); index < declared_extents.size();
       ++index) {
    SgExpression *declared_extent = declared_extents[index];
    if (declared_extent == nullptr ||
        isSgNullExpression(declared_extent) != nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[" << contract
                << "]: implicit trailing dimension " << index
                << " has no declared extent" << std::endl;
      ROSE_ABORT();
    }
    result.push_back({nullptr, nullptr, declared_extent, true, true});
  }
  return result;
}

SgExpression *buildOmpArraySectionLowerExpression(
    const ResolvedOmpArraySectionDimension &dimension) {
  if (dimension.lower_omitted) {
    return buildIntVal(0);
  }
  if (dimension.lower == nullptr) {
    failMapperLoweringIdentity(
        "resolved array section dimension has no lower bound");
  }
  return copyExpression(dimension.lower);
}

SgExpression *buildOmpArraySectionLengthExpression(
    const ResolvedOmpArraySectionDimension &dimension) {
  if (!dimension.length_omitted) {
    if (dimension.length == nullptr) {
      failMapperLoweringIdentity(
          "resolved array section dimension has no length");
    }
    return copyExpression(dimension.length);
  }
  if (dimension.declared_extent == nullptr) {
    failMapperLoweringIdentity(
        "resolved omitted section length has no declared extent");
  }
  if (dimension.lower_omitted) {
    return copyExpression(dimension.declared_extent);
  }
  SgExpression *extent = copyExpression(dimension.declared_extent);
  return buildSubtractOp(extent, buildOmpArraySectionLowerExpression(dimension),
                         extent->get_type());
}

SgExpression *buildOmpArraySectionStorageExtentExpression(
    const ResolvedOmpArraySectionDimension &dimension) {
  return dimension.declared_extent != nullptr
             ? copyExpression(dimension.declared_extent)
             : buildOmpArraySectionLengthExpression(dimension);
}

SgExpression *buildOmpArraySectionExtentExpression(
    const std::vector<ResolvedOmpArraySectionDimension> &dimensions) {
  SgExpression *extent = nullptr;
  for (const ResolvedOmpArraySectionDimension &dimension : dimensions) {
    SgExpression *length = buildOmpArraySectionLengthExpression(dimension);
    extent = extent == nullptr
                 ? length
                 : buildMultiplyOp(extent, length, extent->get_type());
  }
  return extent != nullptr ? extent : buildIntVal(1);
}

SgExpression *buildOmpArraySectionLinearOffsetExpression(
    const std::vector<ResolvedOmpArraySectionDimension> &dimensions) {
  SgExpression *offset = nullptr;
  for (size_t index = 0; index < dimensions.size(); ++index) {
    SgExpression *term = buildOmpArraySectionLowerExpression(dimensions[index]);
    for (size_t trailing = index + 1; trailing < dimensions.size();
         ++trailing) {
      term = buildMultiplyOp(
          term,
          buildOmpArraySectionStorageExtentExpression(dimensions[trailing]),
          term->get_type());
    }
    offset =
        offset == nullptr ? term : buildAddOp(offset, term, offset->get_type());
  }
  return offset != nullptr ? offset : buildIntVal(0);
}

bool ompArraySectionHasNonzeroOffset(
    const std::vector<ResolvedOmpArraySectionDimension> &dimensions) {
  for (const ResolvedOmpArraySectionDimension &dimension : dimensions) {
    if (dimension.lower_omitted) {
      continue;
    }
    const SgIntVal *integer = isSgIntVal(dimension.lower);
    if (integer == nullptr || integer->get_value() != 0) {
      return true;
    }
  }
  return false;
}

bool appendUniqueMapperCandidateType(std::vector<SgType *> &candidate_types,
                                     SgType *candidate_type) {
  if (candidate_type == nullptr) {
    return false;
  }

  for (SgType *existing : candidate_types) {
    if (existing == candidate_type ||
        SageInterface::isEquivalentType(existing, candidate_type)) {
      return false;
    }
  }
  candidate_types.push_back(candidate_type);
  return true;
}

SgExpression *requireEffectiveClauseItemExpression(const SgOmpClause *clause,
                                                   SgExpression *expr) {
  if (clause == nullptr || expr == nullptr) {
    failMapperLoweringIdentity(
        "effective clause item requires a clause and expression");
  }
  if (isSgOmpMapClause(clause) != nullptr) {
    SgOmpMapItem *item = isSgOmpMapItem(expr);
    if (item == nullptr || item->get_expression() == nullptr ||
        item->get_expression()->get_parent() != item) {
      failMapperLoweringIdentity(
          "map clause item is not an exactly owned SgOmpMapItem");
    }
    return item->get_expression();
  }
  if (isSgOmpMapItem(expr) != nullptr) {
    failMapperLoweringIdentity(
        "typed map item appears outside an OpenMP map clause");
  }
  return expr;
}

SgExpression *buildEffectiveClauseItemExpression(const SgOmpClause *clause,
                                                 SgExpression *expr) {
  return copyExpression(requireEffectiveClauseItemExpression(clause, expr));
}

std::vector<SgType *> getMapperCandidateTypes(SgExpression *expr) {
  std::vector<SgType *> candidate_types;
  if (expr == nullptr) {
    failMapperLoweringIdentity("mapped expression is null");
  }
  if (expr->get_type() == nullptr || isSgTypeUnknown(expr->get_type())) {
    failMapperLoweringIdentity("mapped expression has no exact semantic type");
  }

  appendUniqueMapperCandidateType(
      candidate_types, stripTypeAliasesAndReferences(expr->get_type()));

  std::vector<OmpArraySectionDimension> dims;
  if (collectEffectiveArraySectionDimensions(expr, dims) && !dims.empty()) {
    SgExpression *base_expr = buildArraySectionBaseExpression(expr);
    if (base_expr != nullptr) {
      SgType *base_type = stripTypeAliasesAndReferences(base_expr->get_type());
      if (SgPointerType *ptr_type = isSgPointerType(base_type)) {
        appendUniqueMapperCandidateType(
            candidate_types,
            stripTypeAliasesAndReferences(ptr_type->get_base_type()));
      } else if (SgArrayType *array_type = isSgArrayType(base_type)) {
        appendUniqueMapperCandidateType(
            candidate_types,
            stripTypeAliasesAndReferences(array_type->findBaseType()));
      }
    }
  }

  return candidate_types;
}

SgOmpClause::omp_map_operator_enum
normalizeMapperMapOperator(SgOmpClause::omp_map_operator_enum op) {
  switch (op) {
  case SgOmpClause::e_omp_map_present:
  case SgOmpClause::e_omp_map_self:
  case SgOmpClause::e_omp_map_unknown:
    return SgOmpClause::e_omp_map_tofrom;
  case SgOmpClause::e_omp_map_storage:
    return SgOmpClause::e_omp_map_alloc;
  default:
    return op;
  }
}

int runtimeFlagsFromMapClauseModifiers(const SgOmpMapClause *clause) {
  if (clause == nullptr) {
    return 0;
  }

  const SgOmpClause::omp_map_modifier_enum modifiers[] = {
      clause->get_modifier1(), clause->get_modifier2(),
      clause->get_modifier3()};
  int flags = 0;
  for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
    switch (modifier) {
    case SgOmpClause::e_omp_map_modifier_always:
      flags |= OMP_TGT_MAPTYPE_ALWAYS;
      break;
    case SgOmpClause::e_omp_map_modifier_close:
      flags |= OMP_TGT_MAPTYPE_CLOSE;
      break;
    case SgOmpClause::e_omp_map_modifier_present:
      flags |= OMP_TGT_MAPTYPE_PRESENT;
      break;
    default:
      break;
    }
  }
  return flags;
}

bool mapClauseHasModifier(const SgOmpMapClause *clause,
                          SgOmpClause::omp_map_modifier_enum modifier) {
  if (clause == nullptr) {
    return false;
  }

  return clause->get_modifier1() == modifier ||
         clause->get_modifier2() == modifier ||
         clause->get_modifier3() == modifier;
}

bool mapClauseUsesExplicitMapper(const SgOmpMapClause *clause) {
  return mapClauseHasModifier(clause, SgOmpClause::e_omp_map_modifier_mapper);
}

bool mapClauseUsesIteratorModifier(const SgOmpMapClause *clause) {
  return clause != nullptr &&
         (mapClauseHasModifier(clause,
                               SgOmpClause::e_omp_map_modifier_iterator) ||
          !clause->get_iterator_definitions().empty());
}

std::string getRequestedMapperIdentifier(const SgOmpMapClause *clause) {
  if (clause == nullptr) {
    failMapperLoweringIdentity("map clause is null");
  }
  const bool explicit_mapper = mapClauseUsesExplicitMapper(clause);
  if (explicit_mapper && clause->get_mapper_identifier() == nullptr) {
    failMapperLoweringIdentity(
        "explicit map mapper has no identifier expression");
  }
  if (!explicit_mapper && clause->get_mapper_identifier() != nullptr) {
    failMapperLoweringIdentity(
        "map clause owns a mapper identifier without a mapper modifier");
  }
  return getMapperIdentifierText(clause->get_mapper_identifier());
}

bool motionClauseUsesExplicitMapper(const SgOmpClause *motion_clause) {
  if (const SgOmpToClause *to_clause = isSgOmpToClause(motion_clause)) {
    return to_clause->get_kind() == SgOmpClause::e_omp_to_kind_mapper;
  }
  if (const SgOmpFromClause *from_clause = isSgOmpFromClause(motion_clause)) {
    return from_clause->get_kind() == SgOmpClause::e_omp_from_kind_mapper;
  }
  return false;
}

bool motionClauseUsesIterator(const SgOmpClause *motion_clause) {
  if (const SgOmpToClause *to_clause = isSgOmpToClause(motion_clause)) {
    return to_clause->get_kind() == SgOmpClause::e_omp_to_kind_iterator ||
           !to_clause->get_iterator_definitions().empty();
  }
  if (const SgOmpFromClause *from_clause = isSgOmpFromClause(motion_clause)) {
    return from_clause->get_kind() == SgOmpClause::e_omp_from_kind_iterator ||
           !from_clause->get_iterator_definitions().empty();
  }
  return false;
}

std::string getRequestedMapperIdentifier(const SgOmpClause *motion_clause) {
  if (const SgOmpToClause *to_clause = isSgOmpToClause(motion_clause)) {
    const bool explicit_mapper = motionClauseUsesExplicitMapper(to_clause);
    if (explicit_mapper && to_clause->get_mapper_identifier() == nullptr) {
      failMapperLoweringIdentity(
          "explicit to mapper has no identifier expression");
    }
    if (!explicit_mapper && to_clause->get_mapper_identifier() != nullptr) {
      failMapperLoweringIdentity(
          "to clause owns a mapper identifier without mapper kind");
    }
    return getMapperIdentifierText(to_clause->get_mapper_identifier());
  }
  if (const SgOmpFromClause *from_clause = isSgOmpFromClause(motion_clause)) {
    const bool explicit_mapper = motionClauseUsesExplicitMapper(from_clause);
    if (explicit_mapper && from_clause->get_mapper_identifier() == nullptr) {
      failMapperLoweringIdentity(
          "explicit from mapper has no identifier expression");
    }
    if (!explicit_mapper && from_clause->get_mapper_identifier() != nullptr) {
      failMapperLoweringIdentity(
          "from clause owns a mapper identifier without mapper kind");
    }
    return getMapperIdentifierText(from_clause->get_mapper_identifier());
  }
  failMapperLoweringIdentity("mapper motion clause is neither to nor from");
}

SgOmpClause::omp_map_operator_enum
decayMapperMapOperator(SgOmpClause::omp_map_operator_enum use_op,
                       SgOmpClause::omp_map_operator_enum mapper_item_op) {
  const SgOmpClause::omp_map_operator_enum normalized_use =
      normalizeMapperMapOperator(use_op);
  const SgOmpClause::omp_map_operator_enum normalized_item =
      normalizeMapperMapOperator(mapper_item_op);

  switch (normalized_use) {
  case SgOmpClause::e_omp_map_alloc:
    return normalized_item;
  case SgOmpClause::e_omp_map_to:
    switch (normalized_item) {
    case SgOmpClause::e_omp_map_alloc:
      return SgOmpClause::e_omp_map_alloc;
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
      return SgOmpClause::e_omp_map_to;
    case SgOmpClause::e_omp_map_release:
      return SgOmpClause::e_omp_map_release;
    case SgOmpClause::e_omp_map_delete:
      return SgOmpClause::e_omp_map_delete;
    default:
      break;
    }
    break;
  case SgOmpClause::e_omp_map_from:
    switch (normalized_item) {
    case SgOmpClause::e_omp_map_alloc:
      return SgOmpClause::e_omp_map_alloc;
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
      return SgOmpClause::e_omp_map_from;
    case SgOmpClause::e_omp_map_release:
      return SgOmpClause::e_omp_map_release;
    case SgOmpClause::e_omp_map_delete:
      return SgOmpClause::e_omp_map_delete;
    default:
      break;
    }
    break;
  case SgOmpClause::e_omp_map_tofrom:
    switch (normalized_item) {
    case SgOmpClause::e_omp_map_alloc:
      return SgOmpClause::e_omp_map_alloc;
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
      return SgOmpClause::e_omp_map_tofrom;
    case SgOmpClause::e_omp_map_release:
      return SgOmpClause::e_omp_map_release;
    case SgOmpClause::e_omp_map_delete:
      return SgOmpClause::e_omp_map_delete;
    default:
      break;
    }
    break;
  case SgOmpClause::e_omp_map_release:
    if (normalized_item == SgOmpClause::e_omp_map_delete) {
      return SgOmpClause::e_omp_map_delete;
    }
    return SgOmpClause::e_omp_map_release;
  case SgOmpClause::e_omp_map_delete:
    return SgOmpClause::e_omp_map_delete;
  default:
    break;
  }

  MLOG_ERROR_CXX("ompLowering")
      << "Unsupported mapper map-type decay for use operator "
      << static_cast<int>(use_op) << " and mapper operator "
      << static_cast<int>(mapper_item_op);
  ROSE_ABORT();
}

int buildRuntimeMapTypeFlags(SgOmpClause::omp_map_operator_enum op,
                             int runtime_flag_bits) {
  const SgOmpClause::omp_map_operator_enum normalized_op =
      normalizeMapperMapOperator(op);
  int flags = OMP_TGT_MAPTYPE_TARGET_PARAM | runtime_flag_bits;

  switch (normalized_op) {
  case SgOmpClause::e_omp_map_alloc:
  case SgOmpClause::e_omp_map_release:
    return flags;
  case SgOmpClause::e_omp_map_to:
    return flags | OMP_TGT_MAPTYPE_TO;
  case SgOmpClause::e_omp_map_from:
    return flags | OMP_TGT_MAPTYPE_FROM;
  case SgOmpClause::e_omp_map_tofrom:
    return flags | OMP_TGT_MAPTYPE_TO | OMP_TGT_MAPTYPE_FROM;
  case SgOmpClause::e_omp_map_delete:
    return flags | OMP_TGT_MAPTYPE_DELETE;
  default:
    break;
  }

  MLOG_ERROR_CXX("ompLowering")
      << "Unsupported mapper runtime map operator " << static_cast<int>(op);
  ROSE_ABORT();
}

bool isImplicitBasePointerMapItem(SgStatement *anchor_stmt,
                                  SgExpression *expr) {
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(anchor_stmt);
  if (target == nullptr || expr == nullptr) {
    return false;
  }

  if (!isPointerType(expr->get_type()) || isArraySectionReference(expr)) {
    return false;
  }

  SgVariableSymbol *base_symbol = extractClauseVariableSymbol(expr);
  if (base_symbol == nullptr) {
    return false;
  }

  return isImplicitTargetMapVariable(target, base_symbol);
}

SgVariableSymbol *resolveMapperMemberSymbol(SgExpression *lhs_expression,
                                            const std::string &member_name,
                                            bool is_arrow) {
  if (lhs_expression == nullptr || member_name.empty()) {
    return nullptr;
  }

  SgType *base_type = stripTypeAliasesAndReferences(lhs_expression->get_type());
  if (is_arrow) {
    SgPointerType *pointer_type = isSgPointerType(base_type);
    if (pointer_type == nullptr) {
      return nullptr;
    }
    base_type = stripTypeAliasesAndReferences(pointer_type->get_base_type());
  }

  SgClassType *class_type = isSgClassType(base_type);
  if (class_type == nullptr || class_type->get_declaration() == nullptr) {
    return nullptr;
  }

  SgClassDeclaration *class_decl =
      isSgClassDeclaration(class_type->get_declaration());
  SgClassDefinition *class_def = class_decl->get_definition();
  if (class_def == nullptr &&
      class_decl->get_definingDeclaration() != nullptr) {
    SgClassDeclaration *defining_decl =
        isSgClassDeclaration(class_decl->get_definingDeclaration());
    if (defining_decl != nullptr) {
      class_def = defining_decl->get_definition();
    }
  }
  if (class_def == nullptr) {
    return nullptr;
  }

  return class_def->lookup_variable_symbol(member_name);
}

void validateMaterializedMapperMemberAccesses(SgExpression *expr) {
  if (expr == nullptr) {
    failMapperLoweringIdentity(
        "materialized mapper expression is null during member validation");
  }

  std::vector<SgExpression *> accesses;
  Rose_STL_Container<SgNode *> dots = NodeQuery::querySubTree(expr, V_SgDotExp);
  for (SgNode *node : dots) {
    accesses.push_back(isSgExpression(node));
  }
  Rose_STL_Container<SgNode *> arrows =
      NodeQuery::querySubTree(expr, V_SgArrowExp);
  for (SgNode *node : arrows) {
    accesses.push_back(isSgExpression(node));
  }

  std::sort(accesses.begin(), accesses.end(),
            [](SgExpression *lhs, SgExpression *rhs) {
              return computeAstDepth(lhs) > computeAstDepth(rhs);
            });

  for (SgExpression *access_expr : accesses) {
    if (access_expr == nullptr) {
      failMapperLoweringIdentity(
          "materialized mapper member-access list contains null");
    }

    SgBinaryOp *access = isSgBinaryOp(access_expr);
    ROSE_ASSERT(access != nullptr);
    SgVarRefExp *rhs_ref =
        isSgVarRefExp(stripNoopCastsAndParens(access->get_rhs_operand()));
    if (rhs_ref == nullptr) {
      failMapperLoweringIdentity(
          "materialized mapper member access has no exact member reference");
    }

    const bool is_arrow = isSgArrowExp(access_expr) != nullptr;
    SgVariableSymbol *member_symbol = resolveMapperMemberSymbol(
        access->get_lhs_operand(), getVarRefNameText(rhs_ref), is_arrow);
    if (member_symbol == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Failed to resolve mapper member '" << getVarRefNameText(rhs_ref)
          << "' within typed expression " << access_expr->class_name() << "@"
          << static_cast<void *>(access_expr);
      ROSE_ABORT();
    }

    if (rhs_ref->get_symbol() != member_symbol) {
      MLOG_ERROR_CXX("ompLowering")
          << "Copied mapper member '" << getVarRefNameText(rhs_ref)
          << "' lost its exact semantic symbol identity in "
          << access_expr->class_name() << "@"
          << static_cast<void *>(access_expr);
      ROSE_ABORT();
    }
  }
}

bool isFormalMapperVarRef(SgExpression *expr,
                          const SgVariableSymbol *formal_symbol) {
  if (formal_symbol == nullptr) {
    failMapperLoweringIdentity("declare mapper formal symbol is null");
  }
  SgVarRefExp *vref = isSgVarRefExp(stripNoopCastsAndParens(expr));
  return vref != nullptr && vref->get_symbol() == formal_symbol;
}

SgExpression *materializeMapperExpression(SgExpression *template_expr,
                                          SgVariableSymbol *formal_symbol,
                                          SgExpression *actual_expr) {
  ROSE_ASSERT(template_expr != nullptr);
  ROSE_ASSERT(formal_symbol != nullptr);
  ROSE_ASSERT(actual_expr != nullptr);

  if (isFormalMapperVarRef(template_expr, formal_symbol)) {
    SgExpression *result = copyExpression(actual_expr);
    if (result == nullptr)
      failMapperLoweringIdentity(
          "failed to copy direct mapper actual expression");
    return result;
  }

  SgExpression *clone = copyExpression(template_expr);
  ROSE_ASSERT(clone != nullptr);

  Rose_STL_Container<SgNode *> refs =
      NodeQuery::querySubTree(clone, V_SgVarRefExp);
  std::vector<SgVarRefExp *> formal_refs;
  formal_refs.reserve(refs.size());
  for (SgNode *node : refs) {
    SgVarRefExp *vref = isSgVarRefExp(node);
    if (vref != nullptr && vref->get_symbol() == formal_symbol) {
      formal_refs.push_back(vref);
    }
  }
  std::sort(formal_refs.begin(), formal_refs.end(),
            [](SgVarRefExp *lhs, SgVarRefExp *rhs) {
              return computeAstDepth(lhs) > computeAstDepth(rhs);
            });

  for (SgVarRefExp *formal_ref : formal_refs) {
    if (formal_ref == nullptr) {
      failMapperLoweringIdentity(
          "materialized mapper reference list contains null");
    }
    if (formal_ref->get_parent() == nullptr) {
      failMapperLoweringIdentity(
          "materialized mapper formal reference has no structural owner");
    }
    SgExpression *actual_copy = copyExpression(actual_expr);
    if (actual_copy == nullptr) {
      failMapperLoweringIdentity(
          "failed to copy mapper actual expression for substitution");
    }
    replaceExpression(formal_ref, actual_copy);
  }

  validateMaterializedMapperMemberAccesses(clone);
  return clone;
}

bool isDirectMapperSelfItem(SgExpression *expr,
                            const SgVariableSymbol *formal_symbol) {
  return isFormalMapperVarRef(expr, formal_symbol);
}

ResolvedMapperInfo resolveVisibleMapperForExpression(
    SgExpression *mapped_expr, const std::string &requested_identifier,
    bool identifier_was_explicit, SgStatement *anchor_stmt) {
  ResolvedMapperInfo result;
  if (mapped_expr == nullptr || anchor_stmt == nullptr) {
    failMapperLoweringIdentity(
        "mapper resolution requires a mapped expression and anchor statement");
  }

  const std::string normalized_identifier =
      normalizeMapperIdentifierString(requested_identifier);
  const bool request_user_mapper = identifier_was_explicit &&
                                   !normalized_identifier.empty() &&
                                   normalized_identifier != "default";

  const std::vector<SgType *> candidate_types =
      getMapperCandidateTypes(mapped_expr);
  if (candidate_types.empty()) {
    failMapperLoweringIdentity(
        "mapped expression produced no exact candidate semantic type");
  }

  SgStatement *anchor = anchor_stmt;
  for (SgScopeStatement *scope = getEnclosingScope(anchor_stmt);
       scope != NULL;) {
    SgStatement *anchor_child = findDirectChildStatementInScope(anchor, scope);
    const SgStatementPtrList statements = scope->generateStatementList();
    size_t scope_match_count = 0;

    for (SgStatement *stmt : statements) {
      if (stmt == nullptr) {
        failMapperLoweringIdentity(
            "lexical scope statement list contains a null entry");
      }
      if (stmt == anchor_child) {
        break;
      }

      SgOmpDeclareMapperStatement *mapper_stmt =
          isSgOmpDeclareMapperStatement(stmt);
      if (mapper_stmt == nullptr) {
        continue;
      }

      const bool mapper_is_default =
          isDefaultDeclareMapperIdentifier(mapper_stmt->get_identifier());
      std::string mapper_identifier_text = "default";
      if (!mapper_is_default) {
        mapper_identifier_text =
            getMapperIdentifierText(mapper_stmt->get_user_defined_identifier());
      }

      if (request_user_mapper) {
        if (mapper_is_default ||
            mapper_identifier_text != normalized_identifier) {
          continue;
        }
      } else if (!mapper_is_default) {
        continue;
      }

      SgType *formal_type = stripTypeAliasesAndReferences(
          getDeclareMapperFormalType(mapper_stmt));
      ROSE_ASSERT(formal_type != nullptr);

      bool type_matches = false;
      for (SgType *candidate_type : candidate_types) {
        if (candidate_type != nullptr &&
            SageInterface::isEquivalentType(formal_type, candidate_type)) {
          type_matches = true;
          break;
        }
      }
      if (!type_matches) {
        continue;
      }

      ++scope_match_count;
      if (scope_match_count > 1) {
        MLOG_ERROR_CXX("ompLowering")
            << "Ambiguous declare mapper resolution for typed expression "
            << mapped_expr->class_name() << "@"
            << static_cast<void *>(mapped_expr) << " using identifier '"
            << (normalized_identifier.empty() ? "default"
                                              : normalized_identifier)
            << "' in scope " << scope->sage_class_name();
        ROSE_ABORT();
      }

      result.declaration = mapper_stmt;
      result.identifier_text = mapper_identifier_text;
      result.formal_name = getDeclareMapperFormalName(mapper_stmt);
      result.formal_symbol = getDeclareMapperFormalSymbol(mapper_stmt);
      result.formal_type = formal_type;
    }

    if (scope_match_count > 0) {
      return result;
    }

    anchor = isSgStatement(scope);
    SgScopeStatement *next_scope = scope->get_scope();
    if (next_scope == scope) {
      break;
    }
    scope = next_scope;
  }

  return result;
}

std::vector<const ResolvedMapItem *>
getOrderedResolvedMapItems(const std::vector<ResolvedMapItem> &items) {
  std::vector<const ResolvedMapItem *> ordered_items;
  ordered_items.reserve(items.size());
  for (const ResolvedMapItem &item : items) {
    ordered_items.push_back(&item);
  }
  std::stable_sort(ordered_items.begin(), ordered_items.end(),
                   [](const ResolvedMapItem *lhs, const ResolvedMapItem *rhs) {
                     return lhs->is_implicit_base_pointer &&
                            !rhs->is_implicit_base_pointer;
                   });
  return ordered_items;
}

struct MapArgumentExpressions {
  SgExpression *mapping_expression = nullptr;
  SgExpression *mapping_base_expression = nullptr;
  SgExpression *mapping_size_expression = nullptr;
  SgExpression *mapping_type_expression = nullptr;
};

MapArgumentExpressions
buildResolvedMapItemArgumentExpressions(const ResolvedMapItem &item,
                                        SgScopeStatement *scope) {
  ROSE_ASSERT(item.expression != nullptr);
  ROSE_ASSERT(scope != nullptr);

  std::vector<OmpArraySectionDimension> syntax_dimensions;
  collectEffectiveArraySectionDimensions(item.expression, syntax_dimensions);

  SgExpression *base_expression = nullptr;
  if (!syntax_dimensions.empty()) {
    base_expression = buildArraySectionBaseExpression(item.expression);
  } else if (isSgArrayType(stripTypeAliases(item.expression->get_type())) !=
             nullptr) {
    base_expression = copyExpression(item.expression);
  } else {
    base_expression = copyExpression(item.expression);
  }
  ROSE_ASSERT(base_expression != nullptr);
  const std::vector<ResolvedOmpArraySectionDimension> dimensions =
      resolveOmpArraySectionDimensions(syntax_dimensions,
                                       base_expression->get_type(),
                                       "resolved-map-item-section");

  MapArgumentExpressions result;
  result.mapping_base_expression = copyExpression(base_expression);
  int runtime_flag_bits = item.runtime_flag_bits;
  if (item.is_implicit_base_pointer) {
    runtime_flag_bits |= OMP_TGT_MAPTYPE_IMPLICIT;
  }

  const bool treat_as_array =
      !dimensions.empty() ||
      isSgArrayType(stripTypeAliases(base_expression->get_type())) != nullptr;
  const bool is_implicit_pointer_map =
      item.is_implicit_base_pointer && !treat_as_array &&
      isPointerType(item.expression->get_type());
  if (item.use_literal_target_param) {
    ROSE_ASSERT(item.direct_variable_symbol != nullptr);
    SgInitializedName *mapping_variable =
        item.direct_variable_symbol->get_declaration();
    ROSE_ASSERT(mapping_variable != nullptr);
    SgType *mapping_variable_type = mapping_variable->get_type();
    ROSE_ASSERT(mapping_variable_type != nullptr);

    result.mapping_expression = buildLiteralTargetParamArgExpression(
        item.direct_variable_symbol, scope);
    result.mapping_base_expression = copyExpression(result.mapping_expression);
    result.mapping_size_expression =
        buildCastExp(buildSizeOfOp(mapping_variable_type,
                                   SageInterface::requireTargetSizeType(scope)),
                     getRuntimeInt64Type(scope));
  } else if (treat_as_array) {
    SgType *element_type = getOmpArraySectionElementType(
        base_expression->get_type(), dimensions.size(),
        "resolved-map-item-section");

    SgExpression *extent_expression =
        buildOmpArraySectionExtentExpression(dimensions);
    if (!ompArraySectionHasNonzeroOffset(dimensions)) {
      result.mapping_expression = copyExpression(base_expression);
    } else {
      SgPointerType *element_pointer_type = buildPointerType(element_type);
      result.mapping_expression = buildAddOp(
          buildCastExp(copyExpression(base_expression), element_pointer_type),
          buildOmpArraySectionLinearOffsetExpression(dimensions),
          element_pointer_type);
    }
    result.mapping_size_expression = buildCastExp(
        buildMultiplyOp(
            buildSizeOfOp(element_type,
                          SageInterface::requireTargetSizeType(scope)),
            extent_expression, SageInterface::requireTargetSizeType(scope)),
        getRuntimeInt64Type(scope));
  } else if (is_implicit_pointer_map) {
    result.mapping_expression = copyExpression(item.expression);
    result.mapping_size_expression =
        buildCastExp(buildIntVal(0), getRuntimeInt64Type(scope));
  } else if (isPointerType(item.expression->get_type())) {
    result.mapping_expression = copyExpression(item.expression);
    result.mapping_size_expression =
        buildCastExp(buildSizeOfOp(item.expression->get_type(),
                                   SageInterface::requireTargetSizeType(scope)),
                     getRuntimeInt64Type(scope));
  } else {
    SgExpression *mapped_expression = copyExpression(item.expression);
    result.mapping_expression = buildExactAddressOfOp(mapped_expression);
    result.mapping_base_expression = copyExpression(result.mapping_expression);
    result.mapping_size_expression =
        buildCastExp(buildSizeOfOp(item.expression->get_type(),
                                   SageInterface::requireTargetSizeType(scope)),
                     getRuntimeInt64Type(scope));
  }

  if (item.use_literal_target_param) {
    int literal_flags = OMP_TGT_MAPTYPE_TARGET_PARAM | OMP_TGT_MAPTYPE_LITERAL;
    if (item.is_implicit_target_variable) {
      literal_flags |= OMP_TGT_MAPTYPE_IMPLICIT;
    }
    result.mapping_type_expression = buildIntVal(literal_flags);
  } else if (is_implicit_pointer_map) {
    result.mapping_type_expression =
        buildIntVal(OMP_TGT_MAPTYPE_TARGET_PARAM | runtime_flag_bits);
  } else {
    result.mapping_type_expression = buildIntVal(
        buildRuntimeMapTypeFlags(item.map_operator, runtime_flag_bits));
  }

  return result;
}

void appendResolvedMapItemArguments(const std::vector<ResolvedMapItem> &items,
                                    SgExprListExp *map_variable_list,
                                    SgExprListExp *map_variable_base_list,
                                    SgExprListExp *map_variable_size_list,
                                    SgExprListExp *map_variable_type_list,
                                    SgScopeStatement *scope) {
  ROSE_ASSERT(map_variable_list != nullptr);
  ROSE_ASSERT(map_variable_base_list != nullptr);
  ROSE_ASSERT(map_variable_size_list != nullptr);
  ROSE_ASSERT(map_variable_type_list != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const std::vector<const ResolvedMapItem *> ordered_items =
      getOrderedResolvedMapItems(items);
  for (const ResolvedMapItem *item_ptr : ordered_items) {
    ROSE_ASSERT(item_ptr != nullptr);
    MapArgumentExpressions expressions =
        buildResolvedMapItemArgumentExpressions(*item_ptr, scope);
    map_variable_list->append_expression(expressions.mapping_expression);
    map_variable_base_list->append_expression(
        expressions.mapping_base_expression);
    map_variable_size_list->append_expression(
        expressions.mapping_size_expression);
    map_variable_type_list->append_expression(
        expressions.mapping_type_expression);
  }
}

void collectExpandedMapEntriesForExpression(
    SgExpression *mapped_expr, const std::string &requested_identifier,
    bool identifier_was_explicit, MapperUseKind use_kind,
    SgOmpClause::omp_map_operator_enum use_map_op, int runtime_flag_bits,
    SgStatement *anchor_stmt, std::vector<ExpandedMapEntry> &items,
    std::vector<const SgOmpDeclareMapperStatement *> &active_mappers);

void collectExpandedMapEntriesUsingResolvedMapper(
    SgExpression *mapped_expr, const ResolvedMapperInfo &resolved_mapper,
    MapperUseKind use_kind, SgOmpClause::omp_map_operator_enum use_map_op,
    int runtime_flag_bits, SgStatement *anchor_stmt,
    std::vector<ExpandedMapEntry> &items,
    std::vector<const SgOmpDeclareMapperStatement *> &active_mappers) {
  ROSE_ASSERT(mapped_expr != nullptr);
  ROSE_ASSERT(anchor_stmt != nullptr);
  ROSE_ASSERT(resolved_mapper.declaration != nullptr);

  if (resolved_mapper.formal_name.empty() ||
      resolved_mapper.formal_symbol == nullptr) {
    MLOG_ERROR_CXX("ompLowering")
        << "Declare mapper is missing a formal variable for typed expression "
        << mapped_expr->class_name() << "@" << static_cast<void *>(mapped_expr);
    ROSE_ABORT();
  }

  if (std::find(active_mappers.begin(), active_mappers.end(),
                resolved_mapper.declaration) != active_mappers.end()) {
    MLOG_ERROR_CXX("ompLowering")
        << "Recursive declare mapper expansion detected for typed expression "
        << mapped_expr->class_name() << "@" << static_cast<void *>(mapped_expr);
    ROSE_ABORT();
  }

  active_mappers.push_back(resolved_mapper.declaration);
  const SgOmpClausePtrList &mapper_clauses =
      resolved_mapper.declaration->get_clauses();
  for (SgOmpClause *clause : mapper_clauses) {
    SgOmpMapClause *mapper_clause = isSgOmpMapClause(clause);
    if (mapper_clause == nullptr) {
      MLOG_ERROR_CXX("ompLowering")
          << "Unsupported non-map clause within declare mapper: "
          << clause->class_name();
      ROSE_ABORT();
    }

    const SgOmpClause::omp_map_operator_enum mapper_item_op =
        normalizeMapperMapOperator(mapper_clause->get_operation());
    const int combined_flag_bits =
        runtime_flag_bits | runtimeFlagsFromMapClauseModifiers(mapper_clause);
    const std::string nested_requested_identifier =
        getRequestedMapperIdentifier(mapper_clause);
    const bool nested_identifier_was_explicit =
        mapClauseUsesExplicitMapper(mapper_clause);
    if (mapClauseUsesIteratorModifier(mapper_clause)) {
      MLOG_ERROR_CXX("ompLowering")
          << "Iterator-based mapper expansion is not implemented for "
          << mapper_clause->class_name() << "@"
          << static_cast<void *>(mapper_clause);
      ROSE_ABORT();
    }
    const SgExpressionPtrList &mapper_items =
        [&]() -> const SgExpressionPtrList & {
      SgExprListExp *variables = mapper_clause->get_variables();
      if (variables == nullptr || variables->get_parent() != mapper_clause ||
          variables->get_expressions().empty()) {
        failMapperLoweringIdentity(
            "declare mapper map clause has no exact nonempty variable list");
      }
      return variables->get_expressions();
    }();
    for (SgExpression *mapper_item : mapper_items) {
      ROSE_ASSERT(mapper_item != nullptr);

      SgExpression *effective_mapper_item =
          requireEffectiveClauseItemExpression(mapper_clause, mapper_item);
      SgExpression *materialized_item = materializeMapperExpression(
          effective_mapper_item, resolved_mapper.formal_symbol, mapped_expr);
      const bool is_direct_self_item = isDirectMapperSelfItem(
          effective_mapper_item, resolved_mapper.formal_symbol);

      if (use_kind == MapperUseKind::map_clause) {
        const SgOmpClause::omp_map_operator_enum derived_op =
            decayMapperMapOperator(use_map_op, mapper_item_op);
        if (is_direct_self_item && !nested_identifier_was_explicit) {
          ResolvedMapItem direct_item;
          direct_item.expression = materialized_item;
          direct_item.map_operator = derived_op;
          direct_item.runtime_flag_bits = combined_flag_bits;
          direct_item.is_implicit_base_pointer =
              isImplicitBasePointerMapItem(anchor_stmt, materialized_item);
          direct_item.direct_variable_symbol =
              getDirectResolvedMapItemVariableSymbol(materialized_item);
          ExpandedMapEntry direct_entry;
          direct_entry.direct_item = direct_item;
          items.push_back(direct_entry);
        } else {
          collectExpandedMapEntriesForExpression(
              materialized_item, nested_requested_identifier,
              nested_identifier_was_explicit, MapperUseKind::map_clause,
              derived_op, combined_flag_bits, anchor_stmt, items,
              active_mappers);
        }
        continue;
      }

      const bool keep_for_to = mapper_item_op == SgOmpClause::e_omp_map_to ||
                               mapper_item_op == SgOmpClause::e_omp_map_tofrom;
      const bool keep_for_from =
          mapper_item_op == SgOmpClause::e_omp_map_from ||
          mapper_item_op == SgOmpClause::e_omp_map_tofrom;
      if ((use_kind == MapperUseKind::to_clause && !keep_for_to) ||
          (use_kind == MapperUseKind::from_clause && !keep_for_from)) {
        continue;
      }

      const SgOmpClause::omp_map_operator_enum update_op =
          use_kind == MapperUseKind::to_clause ? SgOmpClause::e_omp_map_to
                                               : SgOmpClause::e_omp_map_from;
      if (is_direct_self_item && !nested_identifier_was_explicit) {
        ResolvedMapItem direct_item;
        direct_item.expression = materialized_item;
        direct_item.map_operator = update_op;
        direct_item.runtime_flag_bits = combined_flag_bits;
        direct_item.is_implicit_base_pointer =
            isImplicitBasePointerMapItem(anchor_stmt, materialized_item);
        direct_item.direct_variable_symbol =
            getDirectResolvedMapItemVariableSymbol(materialized_item);
        ExpandedMapEntry direct_entry;
        direct_entry.direct_item = direct_item;
        items.push_back(direct_entry);
      } else {
        collectExpandedMapEntriesForExpression(
            materialized_item, nested_requested_identifier,
            nested_identifier_was_explicit, use_kind, update_op,
            combined_flag_bits, anchor_stmt, items, active_mappers);
      }
    }
  }
  active_mappers.pop_back();
}

void collectExpandedMapEntriesForExpression(
    SgExpression *mapped_expr, const std::string &requested_identifier,
    bool identifier_was_explicit, MapperUseKind use_kind,
    SgOmpClause::omp_map_operator_enum use_map_op, int runtime_flag_bits,
    SgStatement *anchor_stmt, std::vector<ExpandedMapEntry> &items,
    std::vector<const SgOmpDeclareMapperStatement *> &active_mappers) {
  ROSE_ASSERT(mapped_expr != nullptr);
  ROSE_ASSERT(anchor_stmt != nullptr);

  ResolvedMapperInfo resolved_mapper = resolveVisibleMapperForExpression(
      mapped_expr, requested_identifier, identifier_was_explicit, anchor_stmt);
  if (resolved_mapper.declaration == nullptr) {
    const std::string normalized_identifier =
        normalizeMapperIdentifierString(requested_identifier);
    const bool requires_user_mapper = identifier_was_explicit &&
                                      !normalized_identifier.empty() &&
                                      normalized_identifier != "default";
    if (requires_user_mapper) {
      MLOG_ERROR_CXX("ompLowering")
          << "Failed to resolve mapper '" << requested_identifier
          << "' for typed expression " << mapped_expr->class_name() << "@"
          << static_cast<void *>(mapped_expr);
      ROSE_ABORT();
    }

    ResolvedMapItem direct_item;
    direct_item.expression = mapped_expr;
    direct_item.map_operator = use_map_op;
    direct_item.runtime_flag_bits = runtime_flag_bits;
    direct_item.is_implicit_base_pointer =
        isImplicitBasePointerMapItem(anchor_stmt, mapped_expr);
    direct_item.direct_variable_symbol =
        getDirectResolvedMapItemVariableSymbol(mapped_expr);
    ExpandedMapEntry direct_entry;
    direct_entry.direct_item = direct_item;
    items.push_back(direct_entry);
    return;
  }

  if (isArraySectionReference(mapped_expr)) {
    ExpandedMapEntry dynamic_entry;
    dynamic_entry.kind = ExpandedMapEntryKind::dynamic_mapper_section;
    dynamic_entry.resolved_mapper = resolved_mapper;
    dynamic_entry.section_base_expression =
        buildArraySectionBaseExpression(mapped_expr);
    dynamic_entry.use_kind = use_kind;
    dynamic_entry.use_map_op = use_map_op;
    dynamic_entry.runtime_flag_bits = runtime_flag_bits;
    dynamic_entry.anchor_stmt = anchor_stmt;
    collectEffectiveArraySectionDimensions(mapped_expr,
                                           dynamic_entry.section_dimensions);
    if (dynamic_entry.section_base_expression == nullptr ||
        dynamic_entry.section_dimensions.empty()) {
      MLOG_ERROR_CXX("ompLowering")
          << "Failed to materialize mapper array-section expansion for "
          << mapped_expr->class_name() << "@"
          << static_cast<void *>(mapped_expr);
      ROSE_ABORT();
    }
    items.push_back(dynamic_entry);
    return;
  }

  collectExpandedMapEntriesUsingResolvedMapper(
      mapped_expr, resolved_mapper, use_kind, use_map_op, runtime_flag_bits,
      anchor_stmt, items, active_mappers);
}

bool hasDynamicExpandedMapEntries(const std::vector<ExpandedMapEntry> &items) {
  for (const ExpandedMapEntry &item : items) {
    if (item.kind == ExpandedMapEntryKind::dynamic_mapper_section) {
      return true;
    }
  }
  return false;
}

void collectDirectResolvedMapItems(const std::vector<ExpandedMapEntry> &items,
                                   std::vector<ResolvedMapItem> &direct_items) {
  for (const ExpandedMapEntry &item : items) {
    if (item.kind == ExpandedMapEntryKind::direct_item) {
      direct_items.push_back(item.direct_item);
    }
  }
}

std::vector<ExpandedMapEntry>
collectExpandedMapItemsForClause(SgStatement *anchor_stmt,
                                 const SgOmpMapClause *map_clause) {
  std::vector<ExpandedMapEntry> items;
  if (anchor_stmt == nullptr) {
    failMapperLoweringIdentity("map clause lowering has no anchor statement");
  }
  if (map_clause == nullptr) {
    failMapperLoweringIdentity("map clause lowering received a null clause");
  }
  if (map_clause->get_variables() == nullptr) {
    failMapperLoweringIdentity("map clause has no exact variable list");
  }

  if (mapClauseUsesIteratorModifier(map_clause)) {
    MLOG_ERROR_CXX("ompLowering")
        << "Iterator-based map clause lowering is not implemented for "
        << map_clause->class_name() << "@"
        << static_cast<const void *>(map_clause);
    ROSE_ABORT();
  }

  const std::string requested_identifier =
      getRequestedMapperIdentifier(map_clause);
  const bool identifier_was_explicit = mapClauseUsesExplicitMapper(map_clause);
  const int runtime_flag_bits = runtimeFlagsFromMapClauseModifiers(map_clause);
  std::vector<const SgOmpDeclareMapperStatement *> active_mappers;
  const SgExpressionPtrList &variables =
      map_clause->get_variables()->get_expressions();
  for (SgExpression *expr : variables) {
    if (expr == nullptr) {
      failMapperLoweringIdentity("map clause variable list contains null");
    }
    SgExpression *effective_expr =
        buildEffectiveClauseItemExpression(map_clause, expr);
    if (effective_expr == nullptr) {
      failMapperLoweringIdentity(
          "map clause item has no exact effective expression");
    }
    collectExpandedMapEntriesForExpression(
        effective_expr, requested_identifier, identifier_was_explicit,
        MapperUseKind::map_clause, map_clause->get_operation(),
        runtime_flag_bits, anchor_stmt, items, active_mappers);
  }
  return items;
}

std::vector<ExpandedMapEntry>
collectExpandedMotionItemsForClause(SgStatement *anchor_stmt,
                                    const SgOmpClause *motion_clause) {
  std::vector<ExpandedMapEntry> items;
  if (anchor_stmt == nullptr) {
    failMapperLoweringIdentity(
        "mapper motion clause lowering has no anchor statement");
  }
  if (motion_clause == nullptr) {
    failMapperLoweringIdentity("mapper motion clause is null");
  }

  if (motion_clause->variantT() != V_SgOmpToClause &&
      motion_clause->variantT() != V_SgOmpFromClause) {
    MLOG_ERROR_CXX("ompLowering")
        << "Unexpected motion clause in mapper expansion: "
        << motion_clause->sage_class_name();
    ROSE_ABORT();
  }

  const SgOmpVariablesClause *vars_clause =
      isSgOmpVariablesClause(motion_clause);
  if (vars_clause == nullptr) {
    failMapperLoweringIdentity(
        "mapper motion clause is not an exact variables clause");
  }
  if (vars_clause->get_variables() == nullptr) {
    failMapperLoweringIdentity(
        "mapper motion clause has no exact variable list");
  }

  if (motionClauseUsesIterator(motion_clause)) {
    MLOG_ERROR_CXX("ompLowering")
        << "Iterator-based target update lowering is not implemented for "
        << motion_clause->class_name() << "@"
        << static_cast<const void *>(motion_clause);
    ROSE_ABORT();
  }

  const std::string requested_identifier =
      getRequestedMapperIdentifier(motion_clause);
  const bool identifier_was_explicit =
      motionClauseUsesExplicitMapper(motion_clause);
  const MapperUseKind use_kind = motion_clause->variantT() == V_SgOmpToClause
                                     ? MapperUseKind::to_clause
                                     : MapperUseKind::from_clause;
  const SgOmpClause::omp_map_operator_enum use_map_op =
      use_kind == MapperUseKind::to_clause ? SgOmpClause::e_omp_map_to
                                           : SgOmpClause::e_omp_map_from;

  std::vector<const SgOmpDeclareMapperStatement *> active_mappers;
  const SgExpressionPtrList &variables =
      vars_clause->get_variables()->get_expressions();
  for (SgExpression *expr : variables) {
    if (expr == nullptr) {
      failMapperLoweringIdentity(
          "mapper motion clause variable list contains null");
    }
    SgExpression *effective_expr =
        buildEffectiveClauseItemExpression(motion_clause, expr);
    if (effective_expr == nullptr) {
      failMapperLoweringIdentity(
          "mapper motion clause item has no exact effective expression");
    }
    collectExpandedMapEntriesForExpression(
        effective_expr, requested_identifier, identifier_was_explicit, use_kind,
        use_map_op, 0, anchor_stmt, items, active_mappers);
  }
  return items;
}

bool rewritePointerBasedForIndex(SgForStatement *for_loop) {
  if (for_loop == nullptr || for_loop->get_for_init_stmt() == nullptr) {
    return false;
  }

  const SgStatementPtrList &inits =
      for_loop->get_for_init_stmt()->get_init_stmt();
  if (inits.size() != 1) {
    return false;
  }

  SgExprStatement *init_stmt = isSgExprStatement(inits[0]);
  if (init_stmt == nullptr) {
    return false;
  }

  SgAssignOp *assign = isSgAssignOp(init_stmt->get_expression());
  if (assign == nullptr) {
    return false;
  }

  SgVarRefExp *pointer_ref = nullptr;
  size_t index_deref_depth = 0;
  if (!extractPointerDerefChain(assign->get_lhs_operand(), pointer_ref,
                                index_deref_depth)) {
    return false;
  }

  if (pointer_ref == nullptr || pointer_ref->get_symbol() == nullptr) {
    return false;
  }

  SgVariableSymbol *pointer_sym = pointer_ref->get_symbol();
  SgType *index_type =
      resolvePointerBaseType(pointer_sym->get_type(), index_deref_depth);
  if (index_type == nullptr) {
    return false;
  }

  static unsigned long loop_index_counter = 0;
  ++loop_index_counter;
  const std::string local_name =
      "__target_loop_index_" +
      StringUtility::numberToString(loop_index_counter);

  SgScopeStatement *scope = for_loop->get_scope();
  ROSE_ASSERT(scope != nullptr);
  SgVariableDeclaration *index_decl =
      buildVariableDeclaration(local_name, index_type, nullptr, scope);
  insertStatementBefore(for_loop, index_decl);
  SgVariableSymbol *index_sym = getFirstVarSym(index_decl);
  ROSE_ASSERT(index_sym != nullptr);

  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t derefs = NodeQuery::querySubTree(for_loop, V_SgPointerDerefExp);
  for (NodeList_t::iterator i = derefs.begin(); i != derefs.end(); ++i) {
    SgPointerDerefExp *deref = isSgPointerDerefExp(*i);
    if (deref == nullptr) {
      continue;
    }
    if (deref->get_parent() == nullptr) {
      continue;
    }
    SgVarRefExp *candidate_base = nullptr;
    size_t candidate_depth = 0;
    if (!extractPointerDerefChain(deref, candidate_base, candidate_depth)) {
      continue;
    }
    if (candidate_base->get_symbol() != pointer_sym ||
        candidate_depth != index_deref_depth) {
      continue;
    }
    replaceExpression(deref, buildVarRefExp(index_sym));
  }

  return true;
}

void rewritePointerBasedForIndices(SgForStatement *for_loop) {
  if (for_loop == nullptr) {
    return;
  }
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t loops = NodeQuery::querySubTree(for_loop, V_SgForStatement);
  for (NodeList_t::iterator i = loops.begin(); i != loops.end(); ++i) {
    rewritePointerBasedForIndex(isSgForStatement(*i));
  }
}

bool isConditionalPreprocessingDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
    return true;
  default:
    break;
  }

  std::string text = info->getString();
  const std::string::size_type first_non_space =
      text.find_first_not_of(" \t\r\n");
  if (first_non_space != std::string::npos) {
    text = text.substr(first_non_space);
  }
  if (!text.empty() && text[0] == '#') {
    if (text.rfind("#if", 0) == 0 || text.rfind("#ifdef", 0) == 0 ||
        text.rfind("#ifndef", 0) == 0 || text.rfind("#elif", 0) == 0 ||
        text.rfind("#else", 0) == 0 || text.rfind("#endif", 0) == 0) {
      return true;
    }
  }

  return false;
}

static std::string trimCopy(const std::string &input) {
  const std::string::size_type first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return std::string();
  const std::string::size_type last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

static std::string toLowerCopy(const std::string &input) {
  std::string lowered(input);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return lowered;
}

static bool isOpenMPPragmaText(const std::string &raw_text) {
  const std::string text = toLowerCopy(trimCopy(raw_text));
  if (text.empty())
    return false;

  if (text == "omp")
    return true;

  if (text.rfind("omp ", 0) == 0 || text.rfind("omp\t", 0) == 0 ||
      text.rfind("omp\n", 0) == 0 || text.rfind("omp\r", 0) == 0) {
    return true;
  }

  return false;
}

static bool isOpenMPDirectivePreprocessingInfo(const PreprocessingInfo *info) {
  if (info == nullptr)
    return false;

  const std::string text = toLowerCopy(trimCopy(info->getString()));
  if (text.empty())
    return false;

  // C/C++ pragma forms.
  if (text.rfind("#pragma omp", 0) == 0 ||
      text.rfind("// #pragma omp", 0) == 0 ||
      text.rfind("/* #pragma omp", 0) == 0) {
    return true;
  }

  // Fortran directive sentinel forms (free/fixed form).
  if (text.rfind("!$omp", 0) == 0 || text.rfind("c$omp", 0) == 0 ||
      text.rfind("*$omp", 0) == 0) {
    return true;
  }

  return false;
}

static void removeOpenMPPragmaDeclarations(SgNode *root) {
  if (root == nullptr)
    return;

  Rose_STL_Container<SgNode *> pragmas =
      NodeQuery::querySubTree(root, V_SgPragmaDeclaration);
  std::vector<SgPragmaDeclaration *> to_remove;
  for (Rose_STL_Container<SgNode *>::const_iterator it = pragmas.begin();
       it != pragmas.end(); ++it) {
    SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(*it);
    if (pragma_decl == nullptr)
      continue;
    SgPragma *pragma = pragma_decl->get_pragma();
    if (pragma == nullptr)
      continue;
    if (!isOpenMPPragmaText(pragma->get_pragma()))
      continue;

    SgNode *auxiliary_member = pragma_decl;
    std::unordered_set<SgNode *> owner_chain;
    while (auxiliary_member != nullptr &&
           auxiliary_member->get_parent() != nullptr &&
           isSgAuxiliaryDeclarationList(auxiliary_member->get_parent()) ==
               nullptr) {
      if (!owner_chain.insert(auxiliary_member).second) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[pragma-retirement-owner]: pragma=%p has a "
                "cyclic structural owner chain\n",
                static_cast<void *>(pragma_decl));
        ROSE_ABORT();
      }
      auxiliary_member = auxiliary_member->get_parent();
    }
    if (auxiliary_member != nullptr) {
      SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(auxiliary_member->get_parent());
      if (auxiliary != nullptr) {
        SgDeclarationStatement *semantic_declaration =
            isSgDeclarationStatement(auxiliary_member);
        SgScopeStatement *semantic_scope =
            isSgScopeStatement(auxiliary->get_parent());
        if (semantic_declaration == nullptr || semantic_scope == nullptr ||
            semantic_scope->get_auxiliary_declarations() != auxiliary ||
            std::count(auxiliary->get_declarations().begin(),
                       auxiliary->get_declarations().end(),
                       semantic_declaration) != 1) {
          fprintf(stderr,
                  "REX_OMP_INVARIANT[pragma-retirement-owner]: pragma=%p "
                  "auxiliary=%p member=%p scope=%p has no exact semantic "
                  "non-output owner\n",
                  static_cast<void *>(pragma_decl),
                  static_cast<void *>(auxiliary),
                  static_cast<void *>(semantic_declaration),
                  static_cast<void *>(semantic_scope));
          ROSE_ABORT();
        }
        continue;
      }
    }

    if (pragma_decl->get_parent() == nullptr) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[pragma-retirement-owner]: lexical pragma=%p "
              "is detached\n",
              static_cast<void *>(pragma_decl));
      ROSE_ABORT();
    }
    if (isOpenMPPragmaText(pragma->get_pragma()))
      to_remove.push_back(pragma_decl);
  }

  for (std::vector<SgPragmaDeclaration *>::const_iterator it =
           to_remove.begin();
       it != to_remove.end(); ++it) {
    SageInterface::removeStatement(*it, true);
  }
}

static void removeOpenMPDirectivePreprocessingInfo(SgNode *root) {
  if (root == nullptr)
    return;

  std::unordered_set<PreprocessingInfo *> removed_infos;
  auto filter_attached =
      [&removed_infos](AttachedPreprocessingInfoType *attached) {
        if (attached == nullptr)
          return;
        for (AttachedPreprocessingInfoType::iterator it = attached->begin();
             it != attached->end();) {
          PreprocessingInfo *info = *it;
          if (isOpenMPDirectivePreprocessingInfo(info)) {
            removed_infos.insert(info);
            it = attached->erase(it);
          } else {
            ++it;
          }
        }
      };

  if (SgLocatedNode *located_root = isSgLocatedNode(root))
    filter_attached(located_root->getAttachedPreprocessingInfo());

  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    if (SgLocatedNode *located = isSgLocatedNode(*it))
      filter_attached(located->getAttachedPreprocessingInfo());
  }

  for (PreprocessingInfo *info : removed_infos) {
    delete info;
  }
}

static void
deletePreprocessingInfos(const std::unordered_set<PreprocessingInfo *> &infos) {
  for (PreprocessingInfo *info : infos) {
    delete info;
  }
}

void stripConditionalDirectivesFromList(AttachedPreprocessingInfoType &list) {
  AttachedPreprocessingInfoType filtered;
  std::unordered_set<PreprocessingInfo *> removed_infos;
  filtered.reserve(list.size());
  for (AttachedPreprocessingInfoType::const_iterator it = list.begin();
       it != list.end(); ++it) {
    PreprocessingInfo *info = *it;
    if (info == nullptr) {
      continue;
    }
    if (isConditionalPreprocessingDirective(info)) {
      removed_infos.insert(info);
      continue;
    }
    filtered.push_back(info);
  }
  list.swap(filtered);
  deletePreprocessingInfos(removed_infos);
}

void stripConditionalDirectivesFromNode(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }
  if (AttachedPreprocessingInfoType *attached =
          node->getAttachedPreprocessingInfo()) {
    AttachedPreprocessingInfoType filtered;
    std::unordered_set<PreprocessingInfo *> removed_infos;
    filtered.reserve(attached->size());
    for (AttachedPreprocessingInfoType::const_iterator it = attached->begin();
         it != attached->end(); ++it) {
      PreprocessingInfo *info = *it;
      if (info == nullptr) {
        continue;
      }
      if (isConditionalPreprocessingDirective(info)) {
        removed_infos.insert(info);
        continue;
      }
      filtered.push_back(info);
    }
    attached->swap(filtered);
    deletePreprocessingInfos(removed_infos);
  }
}

void rewriteCudaSiblingIncludeDirectives(
    AttachedPreprocessingInfoType *attached,
    const std::filesystem::path &source_dir) {
  if (attached == nullptr) {
    return;
  }

  for (AttachedPreprocessingInfoType::iterator it = attached->begin();
       it != attached->end(); ++it) {
    PreprocessingInfo *info = *it;
    if (info == nullptr ||
        info->getTypeOfDirective() !=
            PreprocessingInfo::CpreprocessorIncludeDeclaration) {
      continue;
    }

    const std::string include_text = info->getString();
    const std::size_t include_pos = include_text.find("#include");
    if (include_pos == std::string::npos) {
      continue;
    }

    const std::size_t quote_begin = include_text.find('"', include_pos);
    if (quote_begin == std::string::npos) {
      continue;
    }
    const std::size_t quote_end = include_text.find('"', quote_begin + 1);
    if (quote_end == std::string::npos || quote_end <= quote_begin + 1) {
      continue;
    }

    const std::string include_name =
        include_text.substr(quote_begin + 1, quote_end - quote_begin - 1);
    std::filesystem::path include_path(include_name);
    if (include_path.extension() != ".c") {
      continue;
    }

    std::filesystem::path cuda_path(include_path);
    cuda_path.replace_extension(".cu");

    std::error_code ec;
    if (!std::filesystem::exists(source_dir / cuda_path, ec) || ec) {
      continue;
    }

    info->setString(include_text.substr(0, quote_begin + 1) +
                    cuda_path.generic_string() +
                    include_text.substr(quote_end));
  }
}

void rewriteCudaSiblingIncludesInOutlinedFile(
    SgSourceFile *new_file, const std::filesystem::path &source_path) {
  if (new_file == nullptr) {
    return;
  }

  const std::filesystem::path source_dir =
      source_path.has_parent_path() ? source_path.parent_path()
                                    : std::filesystem::current_path();

  if (SgGlobal *global = new_file->get_globalScope()) {
    rewriteCudaSiblingIncludeDirectives(global->getAttachedPreprocessingInfo(),
                                        source_dir);
  }

  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(new_file, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    if (SgLocatedNode *located = isSgLocatedNode(*it)) {
      rewriteCudaSiblingIncludeDirectives(
          located->getAttachedPreprocessingInfo(), source_dir);
    }
  }
}

void stripConditionalDirectivesFromSubtree(SgNode *root) {
  if (root == nullptr) {
    return;
  }
  if (SgLocatedNode *located_root = isSgLocatedNode(root)) {
    stripConditionalDirectivesFromNode(located_root);
  }
  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    if (SgLocatedNode *located = isSgLocatedNode(*it)) {
      stripConditionalDirectivesFromNode(located);
    }
  }
}

bool isConditionalBeginDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const PreprocessingInfo::DirectiveType t = info->getTypeOfDirective();
  return t == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
         t == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
         t == PreprocessingInfo::CpreprocessorIfDeclaration;
}

bool isConditionalMiddleDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const PreprocessingInfo::DirectiveType t = info->getTypeOfDirective();
  return t == PreprocessingInfo::CpreprocessorElseDeclaration ||
         t == PreprocessingInfo::CpreprocessorElifDeclaration;
}

bool isConditionalEndDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  return info->getTypeOfDirective() ==
         PreprocessingInfo::CpreprocessorEndifDeclaration;
}

void removeUnbalancedConditionalDirectives(SgNode *root) {
  if (root == nullptr) {
    return;
  }

  std::vector<PreprocessingInfo *> ordered_infos;
  SageInterface::preOrderCollectPreprocessingInfo(root, ordered_infos, 0);

  struct ConditionalBlock {
    PreprocessingInfo *begin;
    std::vector<PreprocessingInfo *> middles;
  };

  std::vector<ConditionalBlock> stack;
  std::unordered_set<PreprocessingInfo *> to_remove;

  for (std::vector<PreprocessingInfo *>::const_iterator it =
           ordered_infos.begin();
       it != ordered_infos.end(); ++it) {
    PreprocessingInfo *info = *it;
    if (!isConditionalPreprocessingDirective(info)) {
      continue;
    }
    if (isConditionalBeginDirective(info)) {
      ConditionalBlock block;
      block.begin = info;
      stack.push_back(block);
      continue;
    }
    if (isConditionalMiddleDirective(info)) {
      if (stack.empty()) {
        to_remove.insert(info);
      } else {
        stack.back().middles.push_back(info);
      }
      continue;
    }
    if (isConditionalEndDirective(info)) {
      if (stack.empty()) {
        to_remove.insert(info);
      } else {
        stack.pop_back();
      }
    }
  }

  for (std::vector<ConditionalBlock>::const_iterator it = stack.begin();
       it != stack.end(); ++it) {
    to_remove.insert(it->begin);
    for (std::vector<PreprocessingInfo *>::const_iterator mit =
             it->middles.begin();
         mit != it->middles.end(); ++mit) {
      to_remove.insert(*mit);
    }
  }

  if (to_remove.empty()) {
    return;
  }

  std::unordered_set<PreprocessingInfo *> removed_infos;
  auto remove_from_attached =
      [&to_remove, &removed_infos](AttachedPreprocessingInfoType *attached) {
        if (attached == nullptr) {
          return;
        }
        for (AttachedPreprocessingInfoType::iterator it = attached->begin();
             it != attached->end();) {
          PreprocessingInfo *info = *it;
          if (to_remove.count(info) != 0) {
            if (info != nullptr) {
              removed_infos.insert(info);
            }
            it = attached->erase(it);
          } else {
            ++it;
          }
        }
      };

  if (SgLocatedNode *located_root = isSgLocatedNode(root)) {
    remove_from_attached(located_root->getAttachedPreprocessingInfo());
  }

  Rose_STL_Container<SgNode *> located_nodes =
      NodeQuery::querySubTree(root, V_SgLocatedNode);
  for (Rose_STL_Container<SgNode *>::const_iterator it = located_nodes.begin();
       it != located_nodes.end(); ++it) {
    SgLocatedNode *located = isSgLocatedNode(*it);
    if (located == nullptr) {
      continue;
    }
    remove_from_attached(located->getAttachedPreprocessingInfo());
  }

  deletePreprocessingInfos(removed_infos);
}

void prependGlobalDeclPreservingLeadingPreproc(SgStatement *decl,
                                               SgGlobal *global_scope) {
  ROSE_ASSERT(decl != nullptr);
  ROSE_ASSERT(global_scope != nullptr);

  SgDeclarationStatementPtrList &declarations =
      global_scope->get_declarations();
  SgEmptyDeclaration *last_preprocessing_anchor = nullptr;
  bool passed_preprocessing_anchors = false;
  for (SgDeclarationStatement *declaration : declarations) {
    SgEmptyDeclaration *anchor = isSgEmptyDeclaration(declaration);
    const bool is_preprocessing_anchor =
        anchor != nullptr &&
        anchor->get_lexical_role() ==
            SgEmptyDeclaration::e_empty_declaration_preprocessing_anchor;
    if (!is_preprocessing_anchor) {
      passed_preprocessing_anchors = true;
      continue;
    }
    anchor->validate_lexical_role();
    if (passed_preprocessing_anchors || anchor->get_parent() != global_scope ||
        anchor->get_scope() != global_scope ||
        std::count(declarations.begin(), declarations.end(), anchor) != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[global-preprocessing-anchor]: "
              "global=%p anchor=%p is non-leading or lacks one exact lexical "
              "owner\n",
              static_cast<void *>(global_scope), static_cast<void *>(anchor));
      ROSE_ABORT();
    }
    last_preprocessing_anchor = anchor;
  }

  if (last_preprocessing_anchor != nullptr) {
    SageInterface::insertStatementAfter(last_preprocessing_anchor, decl,
                                        /*autoMovePreprocessingInfo=*/false);
    return;
  }

  if (SgStatement *first_stmt = SageInterface::getFirstStatement(global_scope);
      first_stmt != nullptr) {
    SageInterface::insertStatementBefore(first_stmt, decl,
                                         /*autoMovePreprocessingInfo=*/true);
  } else {
    SageInterface::prependStatement(decl, global_scope);
  }
}

bool hasTargetOffloadConstructs(SgSourceFile *file) {
  ROSE_ASSERT(file != nullptr);

  static const VariantT target_variants[] = {
      V_SgOmpTargetStatement,
      V_SgOmpTargetTeamsStatement,
      V_SgOmpTargetParallelStatement,
      V_SgOmpTargetDataStatement,
      V_SgOmpTargetUpdateStatement,
      V_SgOmpTargetTeamsDistributeStatement,
      V_SgOmpTargetParallelForStatement,
      V_SgOmpTargetTeamsDistributeParallelForStatement,
  };

  for (VariantT variant : target_variants) {
    for (SgNode *node : NodeQuery::querySubTree(file, variant)) {
      if (!isOmpContextSelectorMetadataDirective(node)) {
        return true;
      }
    }
  }
  return false;
}

bool hasOpenMPRuntimeConstructs(SgSourceFile *file) {
  ROSE_ASSERT(file != nullptr);

  Rose_STL_Container<SgNode *> omp_nodes =
      NodeQuery::querySubTree(file, V_SgOmpExecStatement);
  omp_nodes.erase(std::remove_if(omp_nodes.begin(), omp_nodes.end(),
                                 [](const SgNode *node) {
                                   return isOmpContextSelectorMetadataDirective(
                                       node);
                                 }),
                  omp_nodes.end());
  omp_nodes = mergeSgNodeList(
      omp_nodes, NodeQuery::querySubTree(file, V_SgOmpThreadprivateStatement));

  return !omp_nodes.empty();
}
} // namespace

size_t get_host_pointer_size_bytes(const SgNode *context) {
  return is_32_bit_target(context) ? 4 : 8;
}

bool canUseLiteralTargetParam(const SgOmpClauseBodyStatement *target,
                              SgVariableSymbol *var_sym,
                              SgOmpClause::omp_map_operator_enum map_operator) {
  if (target == NULL || var_sym == NULL) {
    return false;
  }

  SgType *type = stripTypeAliasesAndReferences(var_sym->get_type());
  if (type == NULL || !SageInterface::isScalarType(type) ||
      isPointerType(type) || isSgTypeLongDouble(type) != NULL) {
    return false;
  }

  const bool is_implicit = isImplicitTargetMapVariable(target, var_sym);
  const SgOmpClause::omp_map_operator_enum normalized_op =
      normalizeMapperMapOperator(map_operator);
  const bool need_copy_from = normalized_op == SgOmpClause::e_omp_map_from ||
                              normalized_op == SgOmpClause::e_omp_map_tofrom;
  if (need_copy_from && !is_implicit) {
    return false;
  }

  return get_target_type_size_bytes(type, target) <=
         get_host_pointer_size_bytes(target);
}

static bool isLiteralTargetParamPackCall(const SgExpression *expr) {
  const SgFunctionCallExp *call = isSgFunctionCallExp(expr);
  if (call == NULL) {
    return false;
  }

  const SgFunctionRefExp *callee = isSgFunctionRefExp(call->get_function());
  if (callee == NULL || callee->get_symbol() == NULL) {
    return false;
  }

  return callee->get_symbol()->get_name().getString() ==
         "rex_pack_literal_arg_bytes";
}

static void materializeLiteralTargetArgExpressions(
    SgExprListExp *map_variable_list, SgExprListExp *map_variable_base_list,
    SgBasicBlock *outlined_driver_body, SgScopeStatement *scope) {
  ROSE_ASSERT(map_variable_list != NULL);
  ROSE_ASSERT(map_variable_base_list != NULL);
  ROSE_ASSERT(outlined_driver_body != NULL);
  ROSE_ASSERT(scope != NULL);

  SgExpressionPtrList &arg_exprs = map_variable_list->get_expressions();
  SgExpressionPtrList &base_exprs = map_variable_base_list->get_expressions();
  ROSE_ASSERT(arg_exprs.size() == base_exprs.size());

  int literal_arg_counter = 0;
  for (size_t i = 0; i < arg_exprs.size(); ++i) {
    SgExpression *packed_expr = nullptr;
    if (isLiteralTargetParamPackCall(arg_exprs[i])) {
      packed_expr = arg_exprs[i];
    } else if (isLiteralTargetParamPackCall(base_exprs[i])) {
      packed_expr = base_exprs[i];
    }

    if (packed_expr == nullptr) {
      continue;
    }

    const std::string packed_name =
        "__rex_packed_literal_arg_" + std::to_string(literal_arg_counter++);
    SgType *packed_type = buildPointerType(buildVoidType());
    SgVariableDeclaration *packed_decl = buildVariableDeclaration(
        packed_name, packed_type,
        buildAssignInitializer(copyExpression(packed_expr), packed_type),
        scope);
    outlined_driver_body->append_statement(packed_decl);
    SgVariableSymbol *packed_sym = getFirstVarSym(packed_decl);
    ROSE_ASSERT(packed_sym != NULL);

    arg_exprs[i] = buildVarRefExp(packed_sym);
    arg_exprs[i]->set_parent(map_variable_list);
    base_exprs[i] = buildVarRefExp(packed_sym);
    base_exprs[i]->set_parent(map_variable_base_list);
  }
}

static void
rebuildOutlinedFunctionSyntaxType(SgFunctionDeclaration *function,
                                  SgFunctionParameterList *syntax_parameters) {
  SgFunctionType *old_type =
      function != NULL ? function->get_type_syntax() : NULL;
  SgFunctionParameterTypeList *old_arguments =
      old_type != NULL ? old_type->get_argument_list() : NULL;
  if (function == NULL || syntax_parameters == NULL ||
      syntax_parameters == function->get_parameterList() ||
      syntax_parameters->get_parent() != function || old_type == NULL ||
      !function->get_type_syntax_is_available() ||
      old_type->get_parent() != function || old_arguments == NULL ||
      old_arguments->get_parent() != old_type) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[function-syntax-rebuild]: function=%p "
            "syntax-parameters=%p old-type=%p has no exact declaration-local "
            "syntax transaction\n",
            static_cast<void *>(function),
            static_cast<void *>(syntax_parameters),
            static_cast<void *>(old_type));
    ROSE_ABORT();
  }

  SgPartialFunctionType *partial = new SgPartialFunctionType(
      old_type->get_return_type(), old_type->get_has_ellipses());
  SgFunctionParameterTypeList *partial_arguments = partial->get_argument_list();
  if (partial_arguments == NULL || partial_arguments->get_parent() != partial) {
    ROSE_ABORT();
  }
  for (SgInitializedName *parameter : syntax_parameters->get_args()) {
    if (parameter == NULL || parameter->get_type() == NULL ||
        parameter->get_parent() != syntax_parameters) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[function-syntax-rebuild]: source "
              "parameter is null, untyped, or structurally detached\n");
      ROSE_ABORT();
    }
    partial_arguments->append_argument(parameter->get_type());
  }
  SgFunctionType *new_type = SgFunctionType::createType(partial);
  partial->set_argument_list(NULL);
  partial_arguments->set_parent(NULL);
  SageInterface::deleteAST(
      partial_arguments, SageInterface::DeleteAstMode::kSkipExternalReferences);
  delete partial;
  if (new_type == NULL || new_type == old_type ||
      new_type->get_argument_list() == NULL ||
      new_type->get_argument_list()->get_arguments().size() !=
          syntax_parameters->get_args().size()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[function-syntax-rebuild]: rebuilt "
            "syntax type has no exact parameter arity\n");
    ROSE_ABORT();
  }

  function->set_type_syntax(NULL);
  function->set_type_syntax_is_available(false);
  old_type->set_argument_list(NULL);
  old_type->set_parent(NULL);
  old_arguments->set_parent(NULL);
  SageInterface::deleteAST(
      old_arguments, SageInterface::DeleteAstMode::kSkipExternalReferences);
  delete old_type;

  new_type->set_parent(function);
  function->set_type_syntax(new_type);
  function->set_type_syntax_is_available(true);
  if (function->get_type_syntax() != new_type ||
      !function->get_type_syntax_is_available() ||
      new_type->get_parent() != function) {
    ROSE_ABORT();
  }
}

static void retireOutlinedFunctionSyntaxType(SgFunctionDeclaration *function) {
  if (function == NULL || function->get_type_syntax() == NULL) {
    return;
  }
  SgFunctionType *syntax_type = function->get_type_syntax();
  SgFunctionParameterTypeList *arguments = syntax_type->get_argument_list();
  SgFunctionParameterList *syntax_parameters =
      function->get_parameterList_syntax();
  if (!function->get_type_syntax_is_available() ||
      syntax_type->get_parent() != function || arguments == NULL ||
      arguments->get_parent() != syntax_type ||
      (syntax_parameters != NULL &&
       (syntax_parameters == function->get_parameterList() ||
        syntax_parameters->get_parent() != function ||
        syntax_parameters->get_args().size() !=
            arguments->get_arguments().size()))) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[function-syntax-retire]: function=%p "
            "syntax=%p arguments=%p parameters=%p has no exact "
            "declaration-local syntax transaction\n",
            static_cast<void *>(function), static_cast<void *>(syntax_type),
            static_cast<void *>(arguments),
            static_cast<void *>(syntax_parameters));
    ROSE_ABORT();
  }
  function->set_type_syntax(NULL);
  function->set_type_syntax_is_available(false);
  function->set_parameterList_syntax(NULL);
  syntax_type->set_argument_list(NULL);
  syntax_type->set_parent(NULL);
  arguments->set_parent(NULL);
  if (syntax_parameters != NULL)
    syntax_parameters->set_parent(NULL);
  SageInterface::deleteAST(
      arguments, SageInterface::DeleteAstMode::kSkipExternalReferences);
  delete syntax_type;
  if (syntax_parameters != NULL) {
    SageInterface::deleteAST(
        syntax_parameters,
        SageInterface::DeleteAstMode::kSkipExternalReferences);
  }
  if (function->get_type_syntax() != NULL ||
      function->get_type_syntax_is_available() ||
      function->get_parameterList_syntax() != NULL) {
    ROSE_ABORT();
  }
}

static void
lowerLiteralTargetKernelParameters(SgFunctionDeclaration *outlined_func,
                                   const ASTtools::VarSymSet_t &literal_syms) {
  ROSE_ASSERT(outlined_func != NULL);

  SgFunctionDefinition *func_def = outlined_func->get_definition();
  ROSE_ASSERT(func_def != NULL);
  SgBasicBlock *body = func_def->get_body();
  ROSE_ASSERT(body != NULL);

  // LLVM's __tgt_target_kernel ABI prepends a hidden kernel-launch-environment
  // parameter even for bare kernels. Add it explicitly so REX-generated CUDA
  // kernels match the runtime's argument layout.
  SgFunctionParameterList *params = outlined_func->get_parameterList();
  ROSE_ASSERT(params != NULL);
  SgFunctionParameterList *syntax_params =
      outlined_func->get_parameterList_syntax();
  SgFunctionType *syntax_type = outlined_func->get_type_syntax();
  SgFunctionParameterTypeList *syntax_argument_types =
      syntax_type != NULL ? syntax_type->get_argument_list() : NULL;
  if (syntax_params != NULL && syntax_params != params &&
      (syntax_params->get_parent() != outlined_func ||
       syntax_params->get_args().size() != params->get_args().size() ||
       syntax_type == NULL || !outlined_func->get_type_syntax_is_available() ||
       syntax_type->get_parent() != outlined_func ||
       syntax_argument_types == NULL ||
       syntax_argument_types->get_parent() != syntax_type ||
       syntax_argument_types->get_arguments().size() !=
           syntax_params->get_args().size())) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[literal-parameter-syntax]: outlined "
            "function has inconsistent semantic and source parameter "
            "surfaces\n");
    ROSE_ABORT();
  }
  SgFunctionDeclaration *nondef_decl =
      isSgFunctionDeclaration(outlined_func->get_firstNondefiningDeclaration());
  if (params->get_args().empty() ||
      params->get_args().front()->get_name().getString() !=
          "__rex_kernel_launch_env") {
    SgInitializedName *kernel_launch_env_param =
        SageBuilder::buildInitializedName("__rex_kernel_launch_env",
                                          buildPointerType(buildVoidType()));
    setOneSourcePositionForTransformation(kernel_launch_env_param);
    prependArg(params, kernel_launch_env_param);
    if (syntax_params != NULL && syntax_params != params) {
      SgInitializedName *syntax_kernel_launch_env =
          SageBuilder::buildInitializedName("__rex_kernel_launch_env",
                                            buildPointerType(buildVoidType()));
      setOneSourcePositionForTransformation(syntax_kernel_launch_env);
      prependArg(syntax_params, syntax_kernel_launch_env);
    }

    outlined_func->set_type(buildFunctionType(
        outlined_func->get_type()->get_return_type(),
        buildFunctionParameterTypeList(outlined_func->get_parameterList())));

    if (nondef_decl != NULL) {
      nondef_decl->set_type(outlined_func->get_type());
    }
  }

  if (literal_syms.empty()) {
    if (syntax_params != NULL && syntax_params != params)
      rebuildOutlinedFunctionSyntaxType(outlined_func, syntax_params);
    return;
  }

  std::set<std::string> literal_param_names;
  for (ASTtools::VarSymSet_t::const_iterator it = literal_syms.begin();
       it != literal_syms.end(); ++it) {
    const SgVariableSymbol *var_sym = *it;
    if (var_sym == NULL) {
      continue;
    }
    literal_param_names.insert(var_sym->get_name().getString());
  }

  std::vector<SgStatement *> original_body_stmts = body->get_statements();
  SgInitializedNamePtrList &param_args = params->get_args();
  for (SgInitializedNamePtrList::iterator it = param_args.begin();
       it != param_args.end(); ++it) {
    SgInitializedName *param = *it;
    if (param == NULL) {
      continue;
    }

    if (literal_param_names.find(param->get_name().getString()) ==
        literal_param_names.end()) {
      continue;
    }

    SgType *original_type = stripTypeAliasesAndReferences(param->get_type());
    ROSE_ASSERT(original_type != NULL);

    SgVariableSymbol *param_sym =
        isSgVariableSymbol(param->get_symbol_from_symbol_table());
    ROSE_ASSERT(param_sym != NULL);

    SgType *transport_type =
        get_host_pointer_size_bytes(body) <= 4
            ? static_cast<SgType *>(buildUnsignedIntType())
            : static_cast<SgType *>(buildUnsignedLongLongType());
    param->set_type(transport_type);
    if (syntax_params != NULL && syntax_params != params) {
      const size_t parameter_index =
          static_cast<size_t>(std::distance(params->get_args().begin(), it));
      if (parameter_index >= syntax_params->get_args().size() ||
          syntax_params->get_args()[parameter_index] == NULL ||
          syntax_params->get_args()[parameter_index]->get_name() !=
              param->get_name()) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[literal-parameter-syntax]: "
                "literal parameter '%s' has no exact source-surface pair\n",
                param->get_name().getString().c_str());
        ROSE_ABORT();
      }
      syntax_params->get_args()[parameter_index]->set_type(transport_type);
    }

    if (nondef_decl != NULL && nondef_decl != outlined_func) {
      SgInitializedNamePtrList &nondef_args =
          nondef_decl->get_parameterList()->get_args();
      for (SgInitializedNamePtrList::iterator nondef_it = nondef_args.begin();
           nondef_it != nondef_args.end(); ++nondef_it) {
        SgInitializedName *nondef_param = *nondef_it;
        if (nondef_param == NULL) {
          continue;
        }
        if (nondef_param->get_name() == param->get_name()) {
          nondef_param->set_type(transport_type);
          break;
        }
      }
    }

    outlined_func->set_type(buildFunctionType(
        outlined_func->get_type()->get_return_type(),
        buildFunctionParameterTypeList(outlined_func->get_parameterList())));
    if (nondef_decl != NULL) {
      nondef_decl->set_type(outlined_func->get_type());
    }

    std::string shadow_name = param->get_name().getString() + "__rex_value";
    SgVariableDeclaration *shadow_decl =
        buildVariableDeclaration(shadow_name, original_type, NULL, body);
    prependStatement(shadow_decl, body);
    SgVariableSymbol *shadow_sym = getFirstVarSym(shadow_decl);
    ROSE_ASSERT(shadow_sym != NULL);

    SgExpression *shadow_reference = buildVarRefExp(shadow_sym);
    SgExpression *parameter_reference = buildVarRefExp(param_sym);
    SgExprListExp *memcpy_args = buildExprListExp(
        buildExactAddressOfOp(shadow_reference),
        buildExactAddressOfOp(parameter_reference),
        buildSizeOfOp(original_type,
                      SageInterface::requireTargetSizeType(body)));
    SgExprStatement *memcpy_stmt = buildFunctionCallStmt(
        "__builtin_memcpy", buildPointerType(buildVoidType()), memcpy_args,
        body);
    insertStatementAfter(shadow_decl, memcpy_stmt);

    for (std::vector<SgStatement *>::const_iterator stmt_it =
             original_body_stmts.begin();
         stmt_it != original_body_stmts.end(); ++stmt_it) {
      SgStatement *stmt = *stmt_it;
      if (stmt == NULL) {
        continue;
      }
      Rose_STL_Container<SgNode *> refs =
          NodeQuery::querySubTree(stmt, V_SgVarRefExp);
      for (Rose_STL_Container<SgNode *>::const_iterator ref_it = refs.begin();
           ref_it != refs.end(); ++ref_it) {
        SgVarRefExp *ref = isSgVarRefExp(*ref_it);
        if (ref == NULL || ref->get_symbol() == NULL) {
          continue;
        }
        if (ref->get_symbol()->get_name() == param->get_name()) {
          rebindTransformedVariableReference(ref, ref->get_symbol(), shadow_sym,
                                             "literal-target-parameter-shadow");
        }
      }
    }
  }
  if (syntax_params != NULL && syntax_params != params)
    rebuildOutlinedFunctionSyntaxType(outlined_func, syntax_params);
}

static void recordTargetKernelLaunchBounds(SgFunctionDeclaration *outlined_func,
                                           SgExpression *launch_bounds) {
  if (outlined_func == NULL) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[cuda-launch-bounds]: outlined "
                    "function is null\n");
    ROSE_ABORT();
  }
  if (launch_bounds == NULL) {
    return;
  }
  if (outlined_func->get_cuda_launch_bounds_expression() != nullptr) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[cuda-launch-bounds]: outlined "
                    "function already owns launch bounds\n");
    ROSE_ABORT();
  }
  SgType *source_type = launch_bounds->get_type();
  if (launch_bounds->get_parent() != nullptr || source_type == nullptr ||
      isSgTypeUnknown(source_type) != nullptr ||
      isSgTypeDefault(source_type) != nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[cuda-launch-bounds]: num_threads has "
            "no exact detached, typed ownership payload\n");
    ROSE_ABORT();
  }
  outlined_func->set_cuda_launch_bounds_expression(launch_bounds);
  launch_bounds->set_parent(outlined_func);
  outlined_func->validate_cuda_launch_bounds_expression();
}

static int computeMaxNestedForDepth(SgNode *node) {
  if (node == NULL) {
    return 0;
  }

  int best_depth = 0;
  if (SgForStatement *for_stmt = isSgForStatement(node)) {
    best_depth = 1 + computeMaxNestedForDepth(for_stmt->get_loop_body());
  }

  SgNodePtrList children = node->get_traversalSuccessorContainer();
  for (SgNodePtrList::const_iterator child_it = children.begin();
       child_it != children.end(); ++child_it) {
    best_depth = std::max(best_depth, computeMaxNestedForDepth(*child_it));
  }

  return best_depth;
}

std::map<SgOmpExecStatement *, std::map<SgInitializedName *, SgExpression *> *>
    clause_variable_renaming_record;

namespace {

[[noreturn]] void failOutlinedClauseCopyIdentity(const char *role,
                                                 const SgNode *original,
                                                 const SgNode *mapped) {
  fprintf(stderr,
          "REX_OMP_LOWERING_INVARIANT[outlined-clause-copy-identity]: "
          "role=%s original=%p type=%s mapped=%p type=%s has no exact "
          "nonidentity typed copy\n",
          role, static_cast<const void *>(original),
          original != nullptr ? original->sage_class_name() : "<null>",
          static_cast<const void *>(mapped),
          mapped != nullptr ? mapped->sage_class_name() : "<null>");
  ROSE_ABORT();
}

SgNode *
requireExactMappedClauseNode(const SgCopyHelp::copiedNodeMapType &identityMap,
                             const SgNode *original, const char *role) {
  if (original == nullptr) {
    failOutlinedClauseCopyIdentity(role, nullptr, nullptr);
  }
  SgCopyHelp::copiedNodeMapType::const_iterator mapped =
      identityMap.find(original);
  if (mapped == identityMap.end() || mapped->second == nullptr ||
      mapped->second == original ||
      mapped->second->variantT() != original->variantT()) {
    failOutlinedClauseCopyIdentity(
        role, original, mapped != identityMap.end() ? mapped->second : nullptr);
  }
  return mapped->second;
}

[[noreturn]] void failOutlinedClauseRecord(const char *detail,
                                           const SgNode *directive,
                                           const SgNode *payload) {
  fprintf(stderr,
          "REX_OMP_LOWERING_INVARIANT[outlined-clause-copy-record]: "
          "directive=%p type=%s payload=%p type=%s %s\n",
          static_cast<const void *>(directive),
          directive != nullptr ? directive->sage_class_name() : "<null>",
          static_cast<const void *>(payload),
          payload != nullptr ? payload->sage_class_name() : "<null>", detail);
  ROSE_ABORT();
}

SgOmpClausePtrList exactDirectiveClauses(SgOmpExecStatement *directive) {
  if (SgOmpClauseBodyStatement *body = isSgOmpClauseBodyStatement(directive)) {
    return body->get_clauses();
  }
  if (SgOmpClauseStatement *statement = isSgOmpClauseStatement(directive)) {
    return statement->get_clauses();
  }
  failOutlinedClauseRecord("cannot own OpenMP clauses", directive, nullptr);
}

std::vector<SgExpression *>
exactClauseVariableExpressions(SgOmpExecStatement *directive,
                               SgInitializedName *name) {
  if (directive == nullptr || name == nullptr) {
    failOutlinedClauseRecord("has a null directive or clause variable",
                             directive, name);
  }

  std::vector<SgExpression *> expressions;
  for (SgOmpClause *clause : exactDirectiveClauses(directive)) {
    SgOmpVariablesClause *variablesClause = isSgOmpVariablesClause(clause);
    if (variablesClause == nullptr) {
      continue;
    }
    SgExprListExp *variables = variablesClause->get_variables();
    if (variables == nullptr) {
      failOutlinedClauseRecord("owns a variable clause without an exact list",
                               directive, clause);
    }
    for (SgExpression *variable : variables->get_expressions()) {
      SgVariableSymbol *symbol = extractClauseVariableSymbol(variable);
      if (symbol == nullptr || symbol->get_declaration() == nullptr) {
        failOutlinedClauseRecord(
            "owns an unresolved clause-variable expression", directive,
            variable);
      }
      if (symbol->get_declaration() == name) {
        expressions.push_back(variable);
      }
    }
  }
  return expressions;
}

size_t countExactClauseVariableOccurrences(SgOmpExecStatement *directive,
                                           SgInitializedName *name) {
  return exactClauseVariableExpressions(directive, name).size();
}

bool isWithinExactParentChain(const SgNode *node, const SgNode *root) {
  std::set<const SgNode *> visited;
  for (const SgNode *current = node; current != nullptr;
       current = current->get_parent()) {
    if (!visited.insert(current).second) {
      failOutlinedClauseRecord("has a cyclic parent chain", node, current);
    }
    if (current == root) {
      return true;
    }
  }
  return false;
}

SgVariableSymbol *
exactDetachedBackingSymbol(SgOmpExecStatement *directive,
                           SgExpression *expression,
                           size_t *dereferenceDepth = nullptr) {
  if (expression == nullptr || expression->get_parent() != nullptr) {
    failOutlinedClauseRecord("has no exact detached clause-renaming expression",
                             directive, expression);
  }
  SgPointerDerefExp *deref = isSgPointerDerefExp(expression);
  SgVarRefExp *reference = nullptr;
  size_t depth = 0;
  while (deref != nullptr) {
    SgExpression *operand = deref->get_operand();
    SgPointerType *pointer =
        operand != nullptr ? isSgPointerType(operand->get_type()) : nullptr;
    if (operand == nullptr || operand->get_parent() != deref ||
        pointer == nullptr || pointer->get_base_type() != deref->get_type()) {
      failOutlinedClauseRecord(
          "has a malformed exact pointer-dereference chain", directive,
          expression);
    }
    ++depth;
    if (SgPointerDerefExp *nested = isSgPointerDerefExp(operand)) {
      deref = nested;
      continue;
    }
    reference = isSgVarRefExp(operand);
    break;
  }
  SgVariableSymbol *symbol = reference != nullptr
                                 ? isSgVariableSymbol(reference->get_symbol())
                                 : nullptr;
  if (symbol == nullptr || symbol->get_declaration() == nullptr || depth == 0) {
    failOutlinedClauseRecord(
        "has no exact pointer-dereference backing-symbol identity", directive,
        expression);
  }
  if (dereferenceDepth != nullptr) {
    *dereferenceDepth = depth;
  }
  return symbol;
}

using ClauseVariableRenamingMap = std::map<SgInitializedName *, SgExpression *>;

struct ExactCopiedClauseVariable {
  SgInitializedName *name = nullptr;
  SgVariableSymbol *symbol = nullptr;
  std::vector<SgExpression *> expressions;
};

ExactCopiedClauseVariable requireExactCopiedClauseVariable(
    const SgCopyHelp::copiedNodeMapType &identityMap,
    SgOmpExecStatement *originalDirective, SgOmpExecStatement *copiedDirective,
    SgInitializedName *originalName) {
  const std::vector<SgExpression *> originalExpressions =
      exactClauseVariableExpressions(originalDirective, originalName);
  if (originalExpressions.empty()) {
    failOutlinedClauseCopyIdentity("clause-variable-expression", originalName,
                                   nullptr);
  }

  ExactCopiedClauseVariable result;
  std::set<SgExpression *> copiedExpressionSet;
  for (SgExpression *originalExpression : originalExpressions) {
    SgExpression *copiedExpression =
        isSgExpression(requireExactMappedClauseNode(
            identityMap, originalExpression, "clause-variable-expression"));
    SgVariableSymbol *copiedSymbol =
        copiedExpression != nullptr
            ? extractClauseVariableSymbol(copiedExpression)
            : nullptr;
    SgInitializedName *copiedName =
        copiedSymbol != nullptr ? copiedSymbol->get_declaration() : nullptr;
    if (copiedExpression == nullptr || copiedSymbol == nullptr ||
        copiedName == nullptr ||
        !isWithinExactParentChain(copiedExpression, copiedDirective) ||
        !copiedExpressionSet.insert(copiedExpression).second) {
      failOutlinedClauseCopyIdentity("clause-variable-expression",
                                     originalExpression, copiedExpression);
    }
    if (result.name == nullptr) {
      result.name = copiedName;
      result.symbol = copiedSymbol;
    } else if (result.name != copiedName || result.symbol != copiedSymbol) {
      failOutlinedClauseCopyIdentity("clause-variable-declaration",
                                     originalName, copiedName);
    }
    result.expressions.push_back(copiedExpression);
  }

  const std::vector<SgExpression *> copiedExpressions =
      exactClauseVariableExpressions(copiedDirective, result.name);
  if (copiedExpressions.size() != copiedExpressionSet.size() ||
      !std::all_of(copiedExpressions.begin(), copiedExpressions.end(),
                   [&copiedExpressionSet](SgExpression *expression) {
                     return copiedExpressionSet.count(expression) == 1;
                   })) {
    failOutlinedClauseCopyIdentity("clause-variable-membership", originalName,
                                   result.name);
  }

  const auto directlyMappedName = identityMap.find(originalName);
  if (directlyMappedName != identityMap.end()) {
    SgNode *mappedName = requireExactMappedClauseNode(
        identityMap, originalName, "clause-variable-declaration");
    if (mappedName != result.name) {
      failOutlinedClauseCopyIdentity("clause-variable-declaration",
                                     originalName, mappedName);
    }
  }
  return result;
}

struct PreparedClauseVariableEntry {
  SgInitializedName *originalName = nullptr;
  SgVariableSymbol *originalBackingSymbol = nullptr;
  SgExpression *originalExpression = nullptr;
  SgInitializedName *copiedNameBeforeRebind = nullptr;
  SgVariableSymbol *copiedSymbolBeforeRebind = nullptr;
  SgVariableSymbol *copiedBackingSymbol = nullptr;
  SgExpression *copiedExpression = nullptr;
  std::vector<SgExpression *> copiedClauseExpressions;
};

struct PreparedClauseVariableRenaming {
  SgOmpExecStatement *originalDirective = nullptr;
  SgOmpExecStatement *copiedDirective = nullptr;
  SgFunctionDeclaration *originalFunction = nullptr;
  SgFunctionDeclaration *copiedFunction = nullptr;
  ClauseVariableRenamingMap *originalMapping = nullptr;
  std::unique_ptr<ClauseVariableRenamingMap> copiedMapping;
  std::vector<PreparedClauseVariableEntry> entries;
};

std::vector<PreparedClauseVariableRenaming>
prepareClauseVariableRenamingRecords(
    SgFunctionDeclaration *originalFunction,
    SgFunctionDeclaration *copiedFunction,
    const SgCopyHelp::copiedNodeMapType &identityMap) {
  if (originalFunction == nullptr || copiedFunction == nullptr) {
    failOutlinedClauseRecord("requires two exact function identities",
                             originalFunction, copiedFunction);
  }

  std::vector<PreparedClauseVariableRenaming> prepared;
  std::set<SgOmpExecStatement *> copiedDirectives;
  for (const auto &record : clause_variable_renaming_record) {
    SgOmpExecStatement *originalDirective = record.first;
    if (originalDirective == nullptr || record.second == nullptr) {
      failOutlinedClauseRecord("has a null directive or variable mapping",
                               originalDirective, nullptr);
    }
    if (!isWithinExactParentChain(originalDirective, originalFunction)) {
      continue;
    }

    PreparedClauseVariableRenaming next;
    next.originalDirective = originalDirective;
    next.originalFunction = originalFunction;
    next.copiedFunction = copiedFunction;
    next.originalMapping = record.second;
    next.copiedMapping = std::make_unique<ClauseVariableRenamingMap>();
    std::set<SgInitializedName *> copiedNamesBeforeRebind;

    for (const auto &entry : *record.second) {
      SgInitializedName *originalName = entry.first;
      size_t dereferenceDepth = 0;
      SgVariableSymbol *originalBacking = exactDetachedBackingSymbol(
          originalDirective, entry.second, &dereferenceDepth);
      OmpSupport::ClauseVariableCopyIdentity copied =
          OmpSupport::requireExactClauseVariableCopyIdentity(
              identityMap, originalDirective, originalName, originalBacking);
      ExactCopiedClauseVariable copiedClause = requireExactCopiedClauseVariable(
          identityMap, originalDirective, copied.directive, originalName);
      if (copiedClause.name != copied.clauseVariable ||
          !copiedNamesBeforeRebind.insert(copiedClause.name).second) {
        failOutlinedClauseRecord(
            "maps multiple original variables onto one copied identity",
            copied.directive, copiedClause.name);
      }
      if (!isWithinExactParentChain(originalBacking, originalFunction) ||
          !isWithinExactParentChain(originalBacking->get_declaration(),
                                    originalFunction) ||
          !isWithinExactParentChain(copied.backingSymbol, copiedFunction) ||
          !isWithinExactParentChain(copied.backingSymbol->get_declaration(),
                                    copiedFunction)) {
        failOutlinedClauseRecord(
            "maps a backing symbol outside the outlined function copy",
            originalDirective, originalBacking);
      }
      if (next.copiedDirective == nullptr) {
        next.copiedDirective = copied.directive;
      } else if (next.copiedDirective != copied.directive) {
        failOutlinedClauseRecord(
            "maps one directive record onto multiple copied directives",
            originalDirective, copied.directive);
      }

      const size_t originalOccurrences =
          countExactClauseVariableOccurrences(originalDirective, originalName);
      const size_t copiedOccurrences = countExactClauseVariableOccurrences(
          copied.directive, copied.clauseVariable);
      if (originalOccurrences == 0 ||
          originalOccurrences != copiedOccurrences) {
        failOutlinedClauseRecord(
            "does not preserve exact clause-variable membership",
            originalDirective, originalName);
      }

      SgExpression *copiedExpression = buildVarRefExp(copied.backingSymbol);
      for (size_t depth = 0; depth < dereferenceDepth; ++depth) {
        SgPointerType *pointerType =
            isSgPointerType(copiedExpression->get_type());
        if (pointerType == nullptr || pointerType->get_base_type() == nullptr) {
          failOutlinedClauseRecord(
              "cannot reproduce its exact pointer-dereference depth",
              copied.directive, copied.backingSymbol);
        }
        copiedExpression = buildPointerDerefExp(copiedExpression,
                                                pointerType->get_base_type());
        isSgPointerDerefExp(copiedExpression)->set_need_paren(true);
      }
      if (copiedExpression == nullptr ||
          !SageInterface::isEquivalentType(copiedExpression->get_type(),
                                           entry.second->get_type())) {
        failOutlinedClauseRecord(
            "did not preserve the mapped clause value type", copied.directive,
            copiedExpression);
      }
      next.entries.push_back({originalName, originalBacking, entry.second,
                              copiedClause.name, copiedClause.symbol,
                              copied.backingSymbol, copiedExpression,
                              std::move(copiedClause.expressions)});
    }

    if (next.copiedDirective == nullptr || next.entries.empty() ||
        !isWithinExactParentChain(next.copiedDirective, copiedFunction) ||
        !copiedDirectives.insert(next.copiedDirective).second ||
        clause_variable_renaming_record.count(next.copiedDirective) != 0) {
      failOutlinedClauseRecord(
          "has no unique copied directive record in the copied function",
          originalDirective, next.copiedDirective);
    }
    prepared.push_back(std::move(next));
  }
  return prepared;
}

void commitClauseVariableRenamingRecords(
    std::vector<PreparedClauseVariableRenaming> &prepared) {
  for (const PreparedClauseVariableRenaming &record : prepared) {
    const auto current =
        clause_variable_renaming_record.find(record.originalDirective);
    if (current == clause_variable_renaming_record.end() ||
        current->second != record.originalMapping ||
        clause_variable_renaming_record.count(record.copiedDirective) != 0) {
      failOutlinedClauseRecord(
          "changed after exact copy planning and before commit",
          record.originalDirective, record.copiedDirective);
    }
    if (record.originalFunction == nullptr || record.copiedMapping == nullptr ||
        !record.copiedMapping->empty() || record.entries.empty() ||
        record.originalMapping == nullptr ||
        record.originalMapping->size() != record.entries.size()) {
      failOutlinedClauseRecord(
          "lost a prepared backing-symbol identity before commit",
          record.originalDirective, record.copiedDirective);
    }
    for (const PreparedClauseVariableEntry &entry : record.entries) {
      const auto originalEntry =
          record.originalMapping->find(entry.originalName);
      SgVariableSymbol *currentOriginalBacking = exactDetachedBackingSymbol(
          record.originalDirective, entry.originalExpression);
      SgVariableSymbol *actualBacking = exactDetachedBackingSymbol(
          record.copiedDirective, entry.copiedExpression);
      SgVariableSymbol *currentClauseSymbol = nullptr;
      SgInitializedName *currentClauseName = nullptr;
      std::set<SgExpression *> exactExpressions;
      for (SgExpression *copiedClauseExpression :
           entry.copiedClauseExpressions) {
        SgVariableSymbol *symbol =
            copiedClauseExpression != nullptr
                ? extractClauseVariableSymbol(copiedClauseExpression)
                : nullptr;
        SgInitializedName *name =
            symbol != nullptr ? symbol->get_declaration() : nullptr;
        if (copiedClauseExpression == nullptr || symbol == nullptr ||
            name == nullptr ||
            !isWithinExactParentChain(copiedClauseExpression,
                                      record.copiedDirective) ||
            !exactExpressions.insert(copiedClauseExpression).second ||
            (currentClauseSymbol != nullptr &&
             (currentClauseSymbol != symbol || currentClauseName != name))) {
          failOutlinedClauseRecord(
              "changed an exact copied clause-variable identity during "
              "rebinding",
              record.copiedDirective, copiedClauseExpression);
        }
        currentClauseSymbol = symbol;
        currentClauseName = name;
      }
      const std::vector<SgExpression *> currentExpressions =
          exactClauseVariableExpressions(record.copiedDirective,
                                         currentClauseName);
      if (originalEntry == record.originalMapping->end() ||
          originalEntry->second != entry.originalExpression ||
          currentOriginalBacking != entry.originalBackingSymbol ||
          entry.copiedNameBeforeRebind == nullptr ||
          entry.copiedSymbolBeforeRebind == nullptr ||
          !isWithinExactParentChain(currentOriginalBacking,
                                    record.originalFunction) ||
          !isWithinExactParentChain(currentOriginalBacking->get_declaration(),
                                    record.originalFunction) ||
          actualBacking != entry.copiedBackingSymbol ||
          currentClauseSymbol != entry.copiedBackingSymbol ||
          currentExpressions.size() != exactExpressions.size() ||
          !std::all_of(currentExpressions.begin(), currentExpressions.end(),
                       [&exactExpressions](SgExpression *expression) {
                         return exactExpressions.count(expression) == 1;
                       }) ||
          !isWithinExactParentChain(actualBacking, record.copiedFunction) ||
          !isWithinExactParentChain(actualBacking->get_declaration(),
                                    record.copiedFunction) ||
          !record.copiedMapping
               ->emplace(currentClauseName, entry.copiedExpression)
               .second) {
        failOutlinedClauseRecord(
            "changed its exact copied backing symbol before commit",
            record.copiedDirective, entry.copiedExpression);
      }
    }
  }

  for (PreparedClauseVariableRenaming &record : prepared) {
    for (const auto &entry : *record.originalMapping) {
      SgExpression *expression = entry.second;
      if (expression == nullptr || expression->get_parent() != nullptr) {
        failOutlinedClauseRecord(
            "cannot retire a non-detached original mapping expression",
            record.originalDirective, expression);
      }
      SageInterface::deleteAST(expression,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      if (SgNode::isLiveNode(expression)) {
        failOutlinedClauseRecord(
            "left an original mapping expression live after exact commit",
            record.originalDirective, expression);
      }
    }
    delete record.originalMapping;
    clause_variable_renaming_record.erase(record.originalDirective);
    clause_variable_renaming_record.emplace(record.copiedDirective,
                                            record.copiedMapping.release());
  }
}

} // namespace

OmpSupport::ClauseVariableCopyIdentity
OmpSupport::requireExactClauseVariableCopyIdentity(
    const SgCopyHelp::copiedNodeMapType &identityMap,
    SgOmpExecStatement *originalDirective,
    SgInitializedName *originalClauseVariable,
    SgVariableSymbol *originalBackingSymbol) {
  ClauseVariableCopyIdentity result{
      isSgOmpExecStatement(requireExactMappedClauseNode(
          identityMap, originalDirective, "directive")),
      nullptr,
      isSgVariableSymbol(requireExactMappedClauseNode(
          identityMap, originalBackingSymbol, "backing-symbol"))};
  if (result.directive == nullptr) {
    failOutlinedClauseCopyIdentity(
        "directive", originalDirective,
        requireExactMappedClauseNode(identityMap, originalDirective,
                                     "directive"));
  }
  ExactCopiedClauseVariable copiedClause = requireExactCopiedClauseVariable(
      identityMap, originalDirective, result.directive, originalClauseVariable);
  result.clauseVariable = copiedClause.name;
  if (result.backingSymbol == nullptr) {
    failOutlinedClauseCopyIdentity(
        "backing-symbol", originalBackingSymbol,
        requireExactMappedClauseNode(identityMap, originalBackingSymbol,
                                     "backing-symbol"));
  }

  SgInitializedName *originalBackingName =
      originalBackingSymbol != nullptr
          ? originalBackingSymbol->get_declaration()
          : nullptr;
  SgInitializedName *copiedBackingName =
      isSgInitializedName(requireExactMappedClauseNode(
          identityMap, originalBackingName, "backing-declaration"));
  if (copiedBackingName == nullptr ||
      result.backingSymbol->get_declaration() != copiedBackingName) {
    failOutlinedClauseCopyIdentity("backing-declaration", originalBackingName,
                                   copiedBackingName);
  }
  return result;
}

static void clearClauseVariableRenamingRecord() {
  // Validate the complete ownership graph before deleting any detached
  // expression, so malformed state cannot leave a partially-cleared record.
  for (const auto &record : clause_variable_renaming_record) {
    if (record.first == nullptr || record.second == nullptr) {
      failOutlinedClauseRecord("has a null directive or mapping during clear",
                               record.first, nullptr);
    }
    for (const auto &entry : *record.second) {
      if (entry.first == nullptr || entry.second == nullptr ||
          entry.second->get_parent() != nullptr) {
        failOutlinedClauseRecord(
            "has a null identity or non-detached expression during clear",
            record.first,
            entry.second != nullptr ? static_cast<SgNode *>(entry.second)
                                    : static_cast<SgNode *>(entry.first));
      }
    }
  }

  for (std::map<SgOmpExecStatement *,
                std::map<SgInitializedName *, SgExpression *> *>::iterator it =
           clause_variable_renaming_record.begin();
       it != clause_variable_renaming_record.end(); ++it) {
    std::map<SgInitializedName *, SgExpression *> *name_mapping = it->second;
    for (std::map<SgInitializedName *, SgExpression *>::iterator expr_it =
             name_mapping->begin();
         expr_it != name_mapping->end(); ++expr_it) {
      SgExpression *expr = expr_it->second;
      SageInterface::deleteAST(expr,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      if (SgNode::isLiveNode(expr)) {
        failOutlinedClauseRecord(
            "left a mapping expression live after exact clear", it->first,
            expr);
      }
    }
    delete name_mapping;
  }
  clause_variable_renaming_record.clear();
}

// Liao 1/23/2015
// when translating mapped variables using
// xomp_deviceDataEnvironmentPrepareVariable(), the original variable reference
// will be used as a parameter. However, later
// replaceVariablesWithPointerDereference () will find it and replace it with a
// device version reference, which is not desired. In order to avoid this, we
// keep track of these few references to the original Host CPU side variables
// and don't replace them later on. This may not be elegant, but let's get
// something working first.
static set<SgVarRefExp *> preservedHostVarRefs;

struct KmpcGlobalTidSourceContext {
  std::string enclosing_function_name;
  int source_physical_file_id;
  int source_line;
};

enum class KmpcSourceContextOrigin {
  source_directive,
  generated_directive,
};

static const std::string kKmpcSourceContextAttributeName =
    "rex:omp:kmpc-source-context";

class KmpcSourceContextAttribute : public AstAttribute {
public:
  KmpcSourceContextAttribute(const KmpcGlobalTidSourceContext &source,
                             KmpcSourceContextOrigin origin)
      : source_(source), origin_(origin), copied_(false) {}

  const KmpcGlobalTidSourceContext &source() const { return source_; }
  KmpcSourceContextOrigin origin() const { return origin_; }
  bool copied() const { return copied_; }

  AstAttribute *copy() const override {
    return new KmpcSourceContextAttribute(source_, origin_, true);
  }

  std::string attribute_class_name() const override {
    return "KmpcSourceContextAttribute";
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

private:
  KmpcSourceContextAttribute(const KmpcGlobalTidSourceContext &source,
                             KmpcSourceContextOrigin origin, bool copied)
      : source_(source), origin_(origin), copied_(copied) {}

  KmpcGlobalTidSourceContext source_;
  KmpcSourceContextOrigin origin_;
  bool copied_;
};

static KmpcGlobalTidSourceContext
require_kmpc_global_tid_source_context(SgNode *);

static void attach_kmpc_source_context(SgNode *directive,
                                       const KmpcGlobalTidSourceContext &source,
                                       KmpcSourceContextOrigin origin) {
  if (directive == nullptr || source.enclosing_function_name.empty() ||
      source.source_physical_file_id < 0 || source.source_line < 1 ||
      directive->attributeExists(kKmpcSourceContextAttributeName)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[kmpc-source-context]: invalid or "
            "duplicate runtime source context\n");
    ROSE_ABORT();
  }
  directive->addNewAttribute(kKmpcSourceContextAttributeName,
                             new KmpcSourceContextAttribute(source, origin));
}

static void preserve_kmpc_source_context(SgOmpExecStatement *directive) {
  if (directive == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[kmpc-source-context]: cannot preserve "
            "runtime source context for a null directive\n");
    ROSE_ABORT();
  }
  if (directive->attributeExists(kKmpcSourceContextAttributeName)) {
    require_kmpc_global_tid_source_context(directive);
    return;
  }
  attach_kmpc_source_context(directive,
                             require_kmpc_global_tid_source_context(directive),
                             KmpcSourceContextOrigin::source_directive);
}

static SgVariableDeclaration *
get_kmpc_global_tid(const KmpcGlobalTidSourceContext &, SgScopeStatement *,
                    SgStatement **);
static void insert_function_parameter(std::string, SgType *,
                                      SgFunctionDeclaration *, bool);
static void ensure_fortran_variable_declaration(SgBasicBlock *, const SgName &,
                                                SgType *);
static void insert_fortran_statement_in_specification_part(SgStatement *,
                                                           SgBasicBlock *);
static void insert_fortran_statement_in_specification_part(SgStatement *,
                                                           SgBasicBlock *);
static void insert_fortran_declaration_into_procedure(SgVariableDeclaration *,
                                                      SgScopeStatement *);
static void
materialize_fortran_outlined_function_result_declarations(SgBasicBlock *);
static void rebind_fortran_outlined_function_references(SgBasicBlock *);
static void normalize_fortran_external_subroutine_declarations(SgBasicBlock *);
static void normalize_fortran_if_statements(SgSourceFile *);
// move the outlined function to a separate file

static SgFunctionDeclaration *move_outlined_function(SgFunctionDeclaration *,
                                                     SgSourceFile *);
std::vector<SgFunctionDeclaration *> *outlined_function_list = NULL;
std::vector<SgDeclarationStatement *> *outlined_struct_list = NULL;

std::vector<SgFunctionDeclaration *> *target_outlined_function_list = NULL;
static std::map<SgFunctionDeclaration *, SgFunctionDeclaration *>
    target_outlined_source_function;
std::vector<SgDeclarationStatement *> *target_outlined_struct_list = NULL;
static void post_processing(SgSourceFile *);
static SgSourceFile *generate_outlined_function_file(SgFunctionDeclaration *,
                                                     std::string);
static void fix_storage_modifier(SgSourceFile *, SgGlobal *);
static unsigned int kmpc_global_tid_counter = 0;
static unsigned int kmpc_kernel_id_counter = 0;

static SgSourceFile *cpu_outlined_file = NULL;

#define ENABLE_XOMP                                                            \
  1 // Enable the middle layer (XOMP) of OpenMP runtime libraries
//! Generate a symbol set from an initialized name list,
// filter out struct/class typed names
static void convertAndFilter(const SgInitializedNamePtrList input,
                             ASTtools::VarSymSet_t &output) {
  for (SgInitializedNamePtrList::const_iterator iter = input.begin();
       iter != input.end(); iter++) {
    const SgInitializedName *iname = *iter;
    SgVariableSymbol *symbol =
        isSgVariableSymbol(iname->get_symbol_from_symbol_table());
    ROSE_ASSERT(symbol != NULL);
    if (!isSgClassType(symbol->get_type()))
      output.insert(symbol);
  }
}

namespace OmpSupport {
bool enable_accelerator = false; /* default is to not recognize and lowering
                                    OpenMP accelerator directives */
bool enable_debugging = false;   /* default is not to debug the process */

// A flag to control if device data environment runtime functions are used to
// automatically manage data as much as possible. instead of generating explicit
// data allocation, copy, free functions.
bool useDDE = true;

unsigned int nCounter = 0;

struct GpuOffloadLoweringContext {
  std::map<SgVariableSymbol *, int> per_block_reduction_map;
  std::vector<SgVariableDeclaration *> per_block_declarations;
  ASTtools::VarSymSet_t literal_target_param_syms;
  std::vector<SgVariableDeclaration *> semantic_device_placeholders;
};

class ExactVariableSymbolReferenceCollector : public ROSE_VisitTraversal {
public:
  explicit ExactVariableSymbolReferenceCollector(SgVariableSymbol *symbol)
      : symbol_(symbol) {}

  void visit(SgNode *node) override {
    SgVarRefExp *reference = isSgVarRefExp(node);
    if (reference != NULL && reference->get_symbol() == symbol_)
      references_.push_back(reference);
  }

  std::vector<SgVarRefExp *> collect() {
    traverseMemoryPool();
    return references_;
  }

private:
  SgVariableSymbol *symbol_;
  std::vector<SgVarRefExp *> references_;
};

static void
retireConsumedTargetDevicePlaceholders(GpuOffloadLoweringContext &offload_ctx) {
  std::unordered_set<SgVariableDeclaration *> seen;
  for (SgVariableDeclaration *declaration :
       offload_ctx.semantic_device_placeholders) {
    if (declaration == NULL || !SgNode::isLiveNode(declaration) ||
        !seen.insert(declaration).second) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[device-placeholder-retirement]: "
              "placeholder=%p is null, dead, or duplicated\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    SgAuxiliaryDeclarationList *owner =
        isSgAuxiliaryDeclarationList(declaration->get_parent());
    SgScopeStatement *scope =
        owner != NULL ? isSgScopeStatement(owner->get_parent()) : NULL;
    SgInitializedName *name = getFirstInitializedName(declaration);
    SgVariableSymbol *symbol =
        name != NULL && scope != NULL
            ? isSgVariableSymbol(scope->find_symbol_from_declaration(name))
            : NULL;
    if (owner == NULL || scope == NULL ||
        scope->get_auxiliary_declarations() != owner || name == NULL ||
        name->get_scope() != scope || symbol == NULL ||
        symbol->get_declaration() != name ||
        symbol->get_parent() != scope->get_symbol_table() ||
        !scope->get_symbol_table()->exists(symbol) ||
        std::count(owner->get_declarations().begin(),
                   owner->get_declarations().end(), declaration) != 1) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[device-placeholder-retirement]: "
              "placeholder=%p owner=%p scope=%p name=%p symbol=%p lost its "
              "exact auxiliary identity\n",
              static_cast<void *>(declaration), static_cast<void *>(owner),
              static_cast<void *>(scope), static_cast<void *>(name),
              static_cast<void *>(symbol));
      ROSE_ABORT();
    }

    const std::vector<SgVarRefExp *> references =
        ExactVariableSymbolReferenceCollector(symbol).collect();
    if (!references.empty()) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[device-placeholder-retirement]: "
              "placeholder=%p name=%s retains %zu references after kernel "
              "parameter remapping\n",
              static_cast<void *>(declaration), name->get_name().str(),
              references.size());
      ROSE_ABORT();
    }

    if (!SageBuilder::detachAuxiliaryDeclaration(scope, declaration) ||
        declaration->get_parent() != NULL) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[device-placeholder-retirement]: "
              "placeholder=%p did not complete its exact auxiliary "
              "detachment\n",
              static_cast<void *>(declaration));
      ROSE_ABORT();
    }
    scope->remove_symbol(symbol);
    if (symbol->get_parent() != NULL ||
        scope->get_symbol_table()->exists(symbol) ||
        (scope->get_auxiliary_declarations() != NULL &&
         std::find(
             scope->get_auxiliary_declarations()->get_declarations().begin(),
             scope->get_auxiliary_declarations()->get_declarations().end(),
             declaration) !=
             scope->get_auxiliary_declarations()->get_declarations().end())) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[device-placeholder-retirement]: "
              "placeholder=%p or symbol=%p remained published after exact "
              "detachment\n",
              static_cast<void *>(declaration), static_cast<void *>(symbol));
      ROSE_ABORT();
    }
    delete symbol;
    SageInterface::deleteAST(declaration,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
  offload_ctx.semantic_device_placeholders.clear();
}

static void retireConsumedOmpDirective(SgStatement *directive) {
  if (directive == NULL || !SgNode::isLiveNode(directive) ||
      directive->get_parent() != NULL) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[consumed-directive-retirement]: "
            "directive=%p parent=%p is not one live detached replacement\n",
            static_cast<void *>(directive),
            static_cast<void *>(directive != NULL ? directive->get_parent()
                                                  : NULL));
    ROSE_ABORT();
  }
  if (SgOmpExecStatement *exec = isSgOmpExecStatement(directive)) {
    SgStatement *semantic_parent = exec->get_omp_parent();
    if (semantic_parent != NULL) {
      SgOmpExecStatement *parent_exec = isSgOmpExecStatement(semantic_parent);
      if (parent_exec == NULL || !SgNode::isLiveNode(parent_exec) ||
          std::count(parent_exec->get_omp_children().begin(),
                     parent_exec->get_omp_children().end(), directive) != 1) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[consumed-directive-relationship]: "
                "directive=%p semantic-parent=%p has no exact reverse edge\n",
                static_cast<void *>(directive),
                static_cast<void *>(semantic_parent));
        ROSE_ABORT();
      }
      SgStatementPtrList &siblings = parent_exec->get_omp_children();
      siblings.erase(std::find(siblings.begin(), siblings.end(), directive));
    }
    for (SgStatement *semantic_child : exec->get_omp_children()) {
      SgOmpExecStatement *child_exec = isSgOmpExecStatement(semantic_child);
      if (child_exec == NULL || !SgNode::isLiveNode(child_exec) ||
          child_exec->get_omp_parent() != directive) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[consumed-directive-relationship]: "
                "directive=%p has malformed semantic child=%p\n",
                static_cast<void *>(directive),
                static_cast<void *>(semantic_child));
        ROSE_ABORT();
      }
      child_exec->set_omp_parent(NULL);
    }
    exec->get_omp_children().clear();
    exec->set_omp_parent(NULL);

    const auto mapping = clause_variable_renaming_record.find(exec);
    if (mapping != clause_variable_renaming_record.end()) {
      ClauseVariableRenamingMap *entries = mapping->second;
      if (entries == NULL || entries->empty()) {
        failOutlinedClauseRecord(
            "consumed directive has a null or empty variable mapping", exec,
            entries != NULL ? static_cast<SgNode *>(exec) : NULL);
      }
      for (const auto &entry : *entries) {
        SgExpression *expression = entry.second;
        (void)exactDetachedBackingSymbol(exec, expression);
        SageInterface::deleteAST(
            expression, SageInterface::DeleteAstMode::kRequireIsolated);
        if (SgNode::isLiveNode(expression)) {
          failOutlinedClauseRecord(
              "consumed directive left a detached mapping expression live",
              exec, expression);
        }
      }
      delete entries;
      clause_variable_renaming_record.erase(mapping);
    }
  }
  if (SgOmpClauseBodyStatement *target =
          isSgOmpClauseBodyStatement(directive)) {
    implicit_target_map_variables.erase(target);
  }
  for (SgNode *node : NodeQuery::querySubTree(directive, V_SgForStatement)) {
    SgForStatement *loop = isSgForStatement(node);
    ROSE_ASSERT(loop != nullptr);
    SageInterface::retireForLoopInitNormalization(loop);
  }
  SageInterface::deleteAST(directive,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  if (SgNode::isLiveNode(directive)) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[consumed-directive-retirement]: "
            "directive=%p remained live after exact subtree retirement\n",
            static_cast<void *>(directive));
    ROSE_ABORT();
  }
}

static void
detachTransferredOmpDirectiveBody(SgOmpClauseBodyStatement *directive,
                                  SgStatement *replacement) {
  SgScopeStatement *replacement_owner =
      replacement != NULL ? isSgScopeStatement(replacement->get_parent())
                          : NULL;
  if (directive == NULL || replacement == NULL ||
      directive->get_parent() != NULL || directive->get_body() != replacement ||
      replacement_owner == NULL ||
      static_cast<SgNode *>(replacement_owner) == directive ||
      !replacement_owner->statementExistsInScope(replacement)) {
    fprintf(
        stderr,
        "REX_OMP_INVARIANT[transferred-directive-body]: directive=%p "
        "parent=%p body=%p replacement=%p owner=%p has no exact "
        "replacement handoff\n",
        static_cast<void *>(directive),
        static_cast<void *>(directive != NULL ? directive->get_parent() : NULL),
        static_cast<void *>(directive != NULL ? directive->get_body() : NULL),
        static_cast<void *>(replacement),
        static_cast<void *>(replacement_owner));
    ROSE_ABORT();
  }

  directive->set_body(NULL);
  if (directive->get_body() != NULL ||
      replacement->get_parent() != replacement_owner ||
      !replacement_owner->statementExistsInScope(replacement)) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[transferred-directive-body]: directive=%p "
            "replacement=%p owner=%p failed its exact ownership transfer\n",
            static_cast<void *>(directive), static_cast<void *>(replacement),
            static_cast<void *>(replacement_owner));
    ROSE_ABORT();
  }
}

static void
recordOutlinedSourceFunctionAnchor(SgFunctionDeclaration *outlined_function,
                                   SgFunctionDeclaration *source_function) {
  SgGlobal *outlined_global =
      outlined_function != NULL ? getGlobalScope(outlined_function) : NULL;
  SgGlobal *source_global =
      source_function != NULL ? getGlobalScope(source_function) : NULL;
  if (outlined_function == NULL || source_function == NULL ||
      outlined_function == source_function || outlined_global == NULL ||
      source_global != outlined_global ||
      outlined_function->get_parent() != outlined_global ||
      !outlined_global->statementExistsInScope(outlined_function) ||
      !SgNode::isLiveNode(source_function) ||
      !target_outlined_source_function
           .emplace(outlined_function, source_function)
           .second) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[target-outlined-source-anchor]: outlined=%p "
            "source=%p outlined-global=%p source-global=%p has no unique "
            "source-function anchor\n",
            static_cast<void *>(outlined_function),
            static_cast<void *>(source_function),
            static_cast<void *>(outlined_global),
            static_cast<void *>(source_global));
    ROSE_ABORT();
  }
}

static void
recordTargetOutlinedFunction(SgFunctionDeclaration *outlined_function,
                             SgFunctionDeclaration *source_function) {
  if (target_outlined_function_list == NULL) {
    fprintf(stderr, "REX_OMP_INVARIANT[target-outlined-source-anchor]: target "
                    "outlined-function collection is not active\n");
    ROSE_ABORT();
  }
  recordOutlinedSourceFunctionAnchor(outlined_function, source_function);
  target_outlined_function_list->push_back(outlined_function);
}

static void
transOmpVariablesWithContext(SgStatement *ompStmt, SgBasicBlock *bb1,
                             SgExpression *orig_loop_upper = NULL,
                             bool isAcceleratorModel = false,
                             GpuOffloadLoweringContext *offload_ctx = NULL);

void markImplicitTargetMapVariable(SgOmpClauseBodyStatement *target,
                                   SgInitializedName *var) {
  if (target == NULL || var == NULL) {
    return;
  }
  implicit_target_map_variables[target].insert(var);
}

bool isImplicitTargetMapVariable(const SgOmpClauseBodyStatement *target,
                                 const SgSymbol *sym) {
  if (target == NULL || sym == NULL) {
    return false;
  }

  std::map<const SgOmpClauseBodyStatement *,
           std::set<const SgInitializedName *>>::const_iterator map_iter =
      implicit_target_map_variables.find(target);
  if (map_iter == implicit_target_map_variables.end()) {
    return false;
  }

  const SgVariableSymbol *var_sym =
      isSgVariableSymbol(const_cast<SgSymbol *>(sym));
  if (var_sym == NULL) {
    return false;
  }

  return map_iter->second.find(var_sym->get_declaration()) !=
         map_iter->second.end();
}

void clearImplicitTargetMapVariables() {
  implicit_target_map_variables.clear();
}
//------------------------------------
// Add include "xxxx.h" into source files, right before the first statement from
// users Lazy approach: assume all files will contain OpenMP runtime library
// calls
// TODO: (low priority) a better way is to only insert Headers when OpenMP is
// used. 2/1/2008, try to use MiddleLevelRewrite to parse the content of the
// header, which
//  should generate function symbols used for runtime function calls
//  But it is not stable!

//! This makeDataSharingExplicit() is added by Hongyi on July/23/2012.
//! Consider private, firstprivate, lastprivate, shared, reduction  is it
//! correct?@Leo
// TODO: consider the initialized name of variable in function call or
// definitions

/** Algorithm for patchUpSharedVariables edited by Hongyi Ma on August 7th 2012
 *   1. find all variables references in  parallel region
 *   2. find all variable declarations in this parallel region
 *   3. check whether these variables has been in private or shared clause
 * already
 *   4. if not, add them into shared clause
 */

//! function prototypes for  patch up shared variables

/*    Get name of varrefexp  */
string getName(SgNode *n) {
  string name;
  SgVarRefExp *var = isSgVarRefExp(n);
  if (var)
    name = var->get_symbol()->get_name().getString();

  return name;
}

/*    Remove duplicate list entries  */
void getUnique(Rose_STL_Container<SgNode *> &list) {
  Rose_STL_Container<SgNode *>::iterator start = list.begin();
  unsigned int size = list.size();
  unsigned int i, j;

  if (size > 1) {
    for (i = 0; i < size - 1; i++) {
      j = i + 1;
      while (j < size) {
        SgVarRefExp *iis = isSgVarRefExp(list.at(i));
        SgVarRefExp *jjs = isSgVarRefExp(list.at(j));

        SgInitializedName *is =
            isSgInitializedName(iis->get_symbol()->get_declaration());
        SgInitializedName *js =
            isSgInitializedName(jjs->get_symbol()->get_declaration());
        if (is == js) {
          list.erase(start + j);
          size--;
          continue;
        }

        j++;
      }
    }
  }
}
/* the end of getUnique name */

/* gather varaible references from remaining expressions */

void gatherReferences(const Rose_STL_Container<SgNode *> &expr,
                      Rose_STL_Container<SgNode *> &vars) {
  Rose_STL_Container<SgNode *>::const_iterator iter = expr.begin();

  while (iter != expr.end()) {

    Rose_STL_Container<SgNode *> tempList =
        NodeQuery::querySubTree(*iter, V_SgVarRefExp);

    Rose_STL_Container<SgNode *>::iterator ti = tempList.begin();
    while (ti != tempList.end()) {
      vars.push_back(*ti);
      ti++;
    }
    iter++;
  }
  /* then remove the duplicate variables */
  getUnique(vars);
}
/* the end of gatherReferences function*/

// Check if a variable is explicitly specified by clauses of
// omp_clause_body_stmt. Return e_unknown if not.
static omp_construct_enum
getExplicitDataSharingAttributeForClause(const SgOmpClause *clause) {
  if (clause == NULL) {
    return e_unknown;
  }

  switch (clause->variantT()) {
  case V_SgOmpPrivateClause:
    return e_private;
  case V_SgOmpSharedClause:
    return e_shared;
  case V_SgOmpReductionClause:
    return e_reduction;
  case V_SgOmpCopyinClause:
    return e_copyin;
  case V_SgOmpCopyprivateClause:
    return e_copyprivate;
  case V_SgOmpFirstprivateClause:
    return e_firstprivate;
  case V_SgOmpLastprivateClause:
    return e_lastprivate;
  case V_SgOmpMapClause:
    return e_map;
  default:
    return e_unknown;
  }
}

namespace {
const std::string kOmpClauseBodyAnalysisCacheAttributeName =
    "rose:omp_clause_body_analysis_cache";

class OmpClauseBodyAnalysisCache : public AstAttribute {
public:
  std::map<SgInitializedName *, omp_construct_enum>
      explicit_data_sharing_attributes;
  bool explicit_data_sharing_valid = false;
  uint64_t explicit_data_sharing_modification_sequence = 0;

  std::vector<SgInitializedName *> affected_for_loop_index_vars;
  bool affected_for_loop_index_vars_valid = false;
  uint64_t affected_for_loop_index_vars_modification_sequence = 0;

  std::map<SgInitializedName *, omp_construct_enum> data_sharing_attributes;
  uint64_t data_sharing_modification_sequence = 0;

  AstAttribute *copy() const override {
    return new OmpClauseBodyAnalysisCache();
  }

  std::string attribute_class_name() const override {
    return "OmpClauseBodyAnalysisCache";
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
};

OmpClauseBodyAnalysisCache &
getOmpClauseBodyAnalysisCache(SgOmpClauseBodyStatement *omp_clause_body_stmt) {
  ROSE_ASSERT(omp_clause_body_stmt != NULL);

  if (AstAttribute *attribute = omp_clause_body_stmt->getAttribute(
          kOmpClauseBodyAnalysisCacheAttributeName)) {
    OmpClauseBodyAnalysisCache *cache =
        dynamic_cast<OmpClauseBodyAnalysisCache *>(attribute);
    ROSE_ASSERT(cache != NULL);
    return *cache;
  }

  OmpClauseBodyAnalysisCache *cache = new OmpClauseBodyAnalysisCache();
  omp_clause_body_stmt->addNewAttribute(
      kOmpClauseBodyAnalysisCacheAttributeName, cache);
  return *cache;
}
} // namespace

static omp_construct_enum getExplicitDataSharingAttribute(
    SgInitializedName *iname, SgOmpClauseBodyStatement *omp_clause_body_stmt) {
  ROSE_ASSERT(iname != NULL);
  ROSE_ASSERT(omp_clause_body_stmt != NULL);

  OmpClauseBodyAnalysisCache &cache =
      getOmpClauseBodyAnalysisCache(omp_clause_body_stmt);
  const uint64_t current_ast_modification_sequence =
      SgNode::get_globalAstModificationSequence();
  if (!cache.explicit_data_sharing_valid ||
      cache.explicit_data_sharing_modification_sequence !=
          current_ast_modification_sequence) {
    cache.explicit_data_sharing_attributes.clear();

    for (SgOmpClause *clause : omp_clause_body_stmt->get_clauses()) {
      const omp_construct_enum attribute =
          getExplicitDataSharingAttributeForClause(clause);
      if (attribute == e_unknown) {
        continue;
      }

      SgOmpVariablesClause *vars_clause = isSgOmpVariablesClause(clause);
      if (vars_clause == NULL) {
        continue;
      }

      SgExprListExp *vars = vars_clause->get_variables();
      if (vars == NULL) {
        continue;
      }

      for (SgExpression *expr : vars->get_expressions()) {
        SgVariableSymbol *symbol = extractClauseVariableSymbol(expr);
        if (symbol == NULL) {
          continue;
        }

        cache.explicit_data_sharing_attributes.insert(
            std::make_pair(symbol->get_declaration(), attribute));
      }
    }

    cache.explicit_data_sharing_modification_sequence =
        current_ast_modification_sequence;
    cache.explicit_data_sharing_valid = true;
  }

  std::map<SgInitializedName *, omp_construct_enum>::const_iterator attr_iter =
      cache.explicit_data_sharing_attributes.find(iname);
  if (attr_iter == cache.explicit_data_sharing_attributes.end()) {
    return e_unknown;
  }

  return attr_iter->second;
}

static bool shouldInheritDataSharingAttributeFromParent(
    SgOmpClauseBodyStatement *omp_clause_body_stmt) {
  if (omp_clause_body_stmt == NULL) {
    return false;
  }

  return isSgOmpDoStatement(omp_clause_body_stmt) ||
         isSgOmpForStatement(omp_clause_body_stmt) ||
         isSgOmpForSimdStatement(omp_clause_body_stmt) ||
         isSgOmpSimdStatement(omp_clause_body_stmt) ||
         isSgOmpSingleStatement(omp_clause_body_stmt) ||
         isSgOmpSectionsStatement(omp_clause_body_stmt) ||
         isSgOmpSectionStatement(omp_clause_body_stmt) ||
         isSgOmpMasterStatement(omp_clause_body_stmt) ||
         isSgOmpOrderedStatement(omp_clause_body_stmt) ||
         isSgOmpCriticalStatement(omp_clause_body_stmt) ||
         isSgOmpAtomicStatement(omp_clause_body_stmt) ||
         isSgOmpTargetStatement(omp_clause_body_stmt) ||
         isSgOmpTargetDataStatement(omp_clause_body_stmt);
}

//! Check if a variable access is a shared access , assuming it is already
//! within an OpenMP region.
bool isSharedAccess(SgVarRefExp *varRef) {
  return (getDataSharingAttribute(varRef) == e_shared);
}

omp_construct_enum getDataSharingAttribute(SgVarRefExp *varRef) {
  ROSE_ASSERT(varRef != NULL);
  SgSymbol *s = varRef->get_symbol();
  return getDataSharingAttribute(s, varRef);
}

static int requirePositiveLoopCount(SgExpression *expr,
                                    const char *clause_name) {
  if (expr == NULL) {
    cerr << "REX_AST_INVARIANT[omp-loop-count]: " << clause_name
         << " clause has no loop-count expression" << endl;
    ROSE_ABORT();
  }

  const const_int_expr_t evaluated =
      SageInterface::evaluateConstIntegerExpression(expr);
  if (!evaluated.hasValue_) {
    cerr << "REX_AST_INVARIANT[omp-loop-count]: " << clause_name
         << " clause requires a constant integer expression, found "
         << expr->class_name() << endl;
    ROSE_ABORT();
  }
  if (evaluated.value_ == 0 ||
      evaluated.value_ > static_cast<size_t>(std::numeric_limits<int>::max())) {
    cerr << "REX_AST_INVARIANT[omp-loop-count]: " << clause_name
         << " clause loop count must be in [1, INT_MAX]" << endl;
    ROSE_ABORT();
  }
  return static_cast<int>(evaluated.value_);
}

// TODO: expose to header
// From collapse(Integer), find all affected for loops of a 'omp for' or 'omp
// simd' directive In this case, normalizing combined constructs like 'parallel
// for' is convenient, less directives to consider.
vector<SgForStatement *>
getAffectedForLoops(SgOmpClauseBodyStatement *forOrSimd) {
  vector<SgForStatement *> loops;
  ROSE_ASSERT(forOrSimd != NULL);
  int loop_count = 1; // by default, only one loop is affected.
  SgExpression *exp = getClauseExpression(forOrSimd, V_SgOmpCollapseClause);
  SgExpression *exp_ordered =
      getClauseExpression(forOrSimd, V_SgOmpOrderedClause);
  if (exp != NULL) {
    loop_count = requirePositiveLoopCount(exp, "collapse");
  } else if (exp_ordered != NULL) {
    // An ordered clause without a parameter has an explicit null-expression
    // placeholder. Any other non-value expression is malformed.
    if (isSgNullExpression(exp_ordered) != NULL)
      loop_count = 1;
    else
      loop_count = requirePositiveLoopCount(exp_ordered, "ordered");
  }
  // TODO: what if both ordered() and collapse() appear??

  // Now obtain all loops within forOrSimd, up to loop_count
  RoseAst ast(forOrSimd);
  for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
    if (loop_count == 0)
      break;
    if (SgForStatement *fs = isSgForStatement(*i)) {
      loops.push_back(fs);
      loop_count--;
    }
  }
  return loops;
}

// TODO: expose to header
vector<SgInitializedName *>
getAffectedForLoopIndexVars(SgOmpClauseBodyStatement *forOrSimd) {
  OmpClauseBodyAnalysisCache &cache = getOmpClauseBodyAnalysisCache(forOrSimd);
  const uint64_t current_ast_modification_sequence =
      SgNode::get_globalAstModificationSequence();
  if (!cache.affected_for_loop_index_vars_valid ||
      cache.affected_for_loop_index_vars_modification_sequence !=
          current_ast_modification_sequence) {
    cache.affected_for_loop_index_vars.clear();

    vector<SgForStatement *> loops = getAffectedForLoops(forOrSimd);
    for (size_t i = 0; i < loops.size(); i++) {
      cache.affected_for_loop_index_vars.push_back(
          getLoopIndexVariable(loops[i]));
    }

    cache.affected_for_loop_index_vars_modification_sequence =
        current_ast_modification_sequence;
    cache.affected_for_loop_index_vars_valid = true;
  }

  return cache.affected_for_loop_index_vars;
}

// TODO: expose to header
// Check if a variable is a loop index variable of a loop affected by OpenMP for
// or simd directives.
bool isAffectedForLoopIndexVariable(SgOmpClauseBodyStatement *forOrSimd,
                                    SgInitializedName *iname) {
  vector<SgInitializedName *> loopIndexVars =
      getAffectedForLoopIndexVars(forOrSimd);
  vector<SgInitializedName *>::iterator where =
      find(loopIndexVars.begin(), loopIndexVars.end(), iname);
  return (where != loopIndexVars.end());
}

static omp_construct_enum getDataSharingAttributeInClauseBody(
    SgSymbol *sym, SgInitializedName *iname,
    SgOmpClauseBodyStatement *omp_clause_body_stmt) {
  OmpClauseBodyAnalysisCache &cache =
      getOmpClauseBodyAnalysisCache(omp_clause_body_stmt);
  const uint64_t current_ast_modification_sequence =
      SgNode::get_globalAstModificationSequence();
  if (cache.data_sharing_modification_sequence !=
      current_ast_modification_sequence) {
    cache.data_sharing_attributes.clear();
    cache.data_sharing_modification_sequence =
        current_ast_modification_sequence;
  }

  std::map<SgInitializedName *, omp_construct_enum> &cached_attributes =
      cache.data_sharing_attributes;
  std::map<SgInitializedName *, omp_construct_enum>::const_iterator
      cached_iter = cached_attributes.find(iname);
  if (cached_iter != cached_attributes.end()) {
    return cached_iter->second;
  }

  omp_construct_enum rt_val = e_shared;

  omp_construct_enum temp_val =
      getExplicitDataSharingAttribute(iname, omp_clause_body_stmt);
  if (temp_val != e_unknown) {
    rt_val = temp_val;
    cached_attributes[iname] = rt_val;
    return rt_val;
  }

  SgVariableDeclaration *var_decl =
      isSgVariableDeclaration(iname->get_declaration());
  if (var_decl && isAncestor(omp_clause_body_stmt, var_decl)) {
    if (isStatic(var_decl))
      rt_val = e_shared;
    else
      rt_val = e_private;
    cached_attributes[iname] = rt_val;
    return rt_val;
  }

  if (isThreadprivate(sym)) {
    rt_val = e_threadprivate;
    cached_attributes[iname] = rt_val;
    return rt_val;
  }

  if (isAffectedForLoopIndexVariable(omp_clause_body_stmt, iname)) {
    if (isSgOmpForStatement(omp_clause_body_stmt)) {
      rt_val = e_private;
      cached_attributes[iname] = rt_val;
      return rt_val;
    } else if (SgOmpSimdStatement *simd_stmt =
                   isSgOmpSimdStatement(omp_clause_body_stmt)) {
      if (hasClause(simd_stmt, V_SgOmpCollapseClause)) {
        rt_val = e_lastprivate;
      } else {
        rt_val = e_linear;
      }
      cached_attributes[iname] = rt_val;
      return rt_val;
    }
  }

  if (SgOmpClauseBodyStatement *parent_clause_body_stmt =
          findEnclosingOmpClauseBodyStatement(
              getEnclosingStatement(omp_clause_body_stmt->get_parent()))) {
    if (shouldInheritDataSharingAttributeFromParent(omp_clause_body_stmt)) {
      rt_val = getDataSharingAttributeInClauseBody(sym, iname,
                                                   parent_clause_body_stmt);
      cached_attributes[iname] = rt_val;
      return rt_val;
    }
  }

  cached_attributes[iname] = rt_val;
  return rt_val;
}

//! Return the data sharing attribute type of a variable within a context node
//! (anchor_stmt indicates the start search location within AST) Possible values
//! include: e_shared, e_private,  e_firstprivate,  e_lastprivate,  e_reduction,
//! e_threadprivate, e_copyin, and e_copyprivate.
// The rules are defined in OpenMP 4.5 specification,  page 179,
//    2.15.1 Data-sharing Attribute Rules
omp_construct_enum getDataSharingAttribute(SgSymbol *sym, SgNode *anchor_node) {
  omp_construct_enum rt_val = e_shared; // shared by default for now
  // TODO: if default() is present, we have to change this.
  ROSE_ASSERT(sym != NULL);
  ROSE_ASSERT(anchor_node != NULL);
  SgStatement *anchor_stmt = getEnclosingStatement(anchor_node);
  ROSE_ASSERT(anchor_stmt != NULL);

  SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
  ROSE_ASSERT(var_sym != NULL);

  SgInitializedName *iname = isSgInitializedName(var_sym->get_declaration());
  // TODO: what to do with SgOmpWorkshareStatement ?  it is a
  // region/SgOmpBodyStatement, but it does not belong to OmpClauseBodyStatement

  // obtain the enclosing OpenMP clause body statement: SgOmpForStatement,
  // parallel, sections, single, target, target data, task, etc.
  // TODO: this may not be reliable:  region {stmtlist ;  loop; stmtlist; }
  SgOmpClauseBodyStatement *omp_clause_body_stmt =
      findEnclosingOmpClauseBodyStatement(anchor_stmt);

  if (omp_clause_body_stmt != NULL) {
    return getDataSharingAttributeInClauseBody(sym, iname,
                                               omp_clause_body_stmt);
  } // end of has an OpenMP enclosing clause body statement
  else // orphaned code segments
  {
    /*
        For the data race detection project, we choose to inline everything. So
      the implementation of orphaned segs is lower priority.
      //TODO: handle more cases as needed.
      Variables with static storage duration that are declared in called
      routines in the region are shared.

      File-scope or namespace-scope variables referenced in called routines in
      the region are shared unless they appear in a threadprivate directive.

       Objects with dynamic storage duration are shared.

       Static data members are shared unless they appear in a threadprivate
      directive.

       In C++, formal arguments of called routines in the region that are passed
      by reference have the same data-sharing attributes as the associated
      actual arguments.

       Other variables declared in called routines in the region are private.
    */
    if (isThreadprivate(sym)) {
      rt_val = e_threadprivate;
      return rt_val;
    }
    if (isSharedByDefaultInOrphanedConstruct(iname)) {
      rt_val = e_shared;
      return rt_val;
    } else {
      // find locally declared variables
      SgDeclarationStatement *var_decl = iname->get_declaration();
      SgFunctionDefinition *func_def =
          getEnclosingFunctionDefinition(anchor_stmt);
      ROSE_ASSERT(func_def != NULL);
      if (isAncestor(func_def, var_decl)) {
        rt_val = e_private;
        return rt_val;
      }
      // if it is within a main function, it should be private no matter what.
      // Single sequential region, not shared with others.
      if (isMain(func_def->get_declaration())) {
        rt_val = e_private;
        return rt_val;
      }
    }
  } // end of orphaned code segments

  return rt_val;
}

bool isThreadprivate(SgSymbol *sym) {
  bool rt_val = false;

  ROSE_ASSERT(sym != NULL);
  SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
  ROSE_ASSERT(var_sym != NULL);
  std::set<SgInitializedName *> var_set = collectThreadprivateVariables();
  SgInitializedName *iname = var_sym->get_declaration();
  ROSE_ASSERT(iname != NULL);

  if (var_set.find(iname) != var_set.end())
    rt_val = true;
  return rt_val;
}

//! Patch up all variables to make them explicit in data-sharing explicit
int patchUpSharedVariables(SgFile *file) {

  int result = 0; // record for the number of shared variables added

  ROSE_ASSERT(file != NULL);
  Rose_STL_Container<SgNode *> allParallelRegion =
      NodeQuery::querySubTree(file, V_SgOmpParallelStatement);
  Rose_STL_Container<SgNode *>::iterator allParallelRegionItr =
      allParallelRegion.begin();

  for (; allParallelRegionItr != allParallelRegion.end();
       allParallelRegionItr++) {
    if (isOmpContextSelectorMetadataDirective(*allParallelRegionItr)) {
      continue;
    }
    //! Gather all expressions statements
    Rose_STL_Container<SgNode *> expressions =
        NodeQuery::querySubTree(*allParallelRegionItr, V_SgExprStatement);
    //! Store all variable references
    // TODO: this may miss the constant variables referenced in data type
    // declaration. e.g. int a[length];
    Rose_STL_Container<SgNode *> allRef;
    gatherReferences(expressions, allRef);

    //! Find all local variable declarations in the parallel region
    Rose_STL_Container<SgNode *> localVariables =
        NodeQuery::querySubTree(*allParallelRegionItr, V_SgVariableDeclaration);

    //! Check variables are not local, not variables in clauses already
    Rose_STL_Container<SgNode *>::iterator allRefItr = allRef.begin();
    while (allRefItr != allRef.end()) {
      SgVarRefExp *item = isSgVarRefExp(*allRefItr);
      string varName = item->get_symbol()->get_name().getString();

      Rose_STL_Container<SgNode *>::iterator localVariablesItr =
          localVariables.begin();

      bool isLocal = false; // record whether this variable should be added into
                            // shared clause

      while (localVariablesItr != localVariables.end()) {
        SgInitializedNamePtrList vars =
            ((SgVariableDeclaration *)(*localVariablesItr))->get_variables();

        string localName = vars.at(0)->get_name().getString();
        if (varName == localName) {
          isLocal = true;
        }
        localVariablesItr++;
      }

      bool isInPrivate = false;
      SgInitializedName *reg =
          isSgInitializedName(item->get_symbol()->get_declaration());

      isInPrivate = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpPrivateClause);

      bool isInShared = false;

      isInShared = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpSharedClause);

      bool isInFirstprivate = false;

      isInFirstprivate = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpFirstprivateClause);

      bool isInReduction = false;

      isInReduction = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpReductionClause);

      const bool isInCopyin = isInClauseVariableList(
          reg, isSgOmpClauseBodyStatement(*allParallelRegionItr),
          V_SgOmpCopyinClause);
      const bool is_threadprivate = isThreadprivate(item->get_symbol());

      if (!isLocal && !isInShared && !isInPrivate && !isInFirstprivate &&
          !isInReduction && !isInCopyin && !is_threadprivate) {
        MLOG_DEBUG_CXX("ompLowering")
            << "add shared clause variable node=" << item
            << " type=" << item->class_name();
        addClauseVariable(reg,
                          isSgOmpClauseBodyStatement(*allParallelRegionItr),
                          V_SgOmpSharedClause);
        result++;
        MLOG_DEBUG_CXX("ompLowering") << "shared clause insertion succeeded";
      }
      allRefItr++;
    }

  } // end of all parallel region

  return result;
} // the end of patchUpSharedVariables()

//! make all data-sharing attribute explicit

int makeDataSharingExplicit(SgFile *file) {
  int result = 0; // to record the number of varbaile added
  ROSE_ASSERT(file != NULL);

  int p = patchUpPrivateVariables(file); // private variable first

  int f = patchUpFirstprivateVariables(file); // then firstprivate variable

  int s = patchUpSharedVariables(file); // consider shared variables

  // TODO:  patchUpDefaultVariables(file);

  result = p + f + s;
  return result;

} //! the end of makeDataSharingExplicit()

void insertRTLHeaders(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  if (!file->get_Fortran_only() &&
      (hasOpenMPRuntimeConstructs(file) || hasTargetOffloadConstructs(file))) {
    SageInterface::insertHeader(file, "rex_kmp.h",
                                /*isSystemHeader=*/false,
                                /*asLastHeader=*/true);
    file->set_processedToIncludeCppDirectivesAndComments(true);
  }
  if (enable_accelerator) {
    SageInterface::insertHeader(file, "xomp_cuda_lib_inlined.cu",
                                /*isSystemHeader=*/false,
                                /*asLastHeader=*/true);
    file->set_processedToIncludeCppDirectivesAndComments(true);
  }
}

void insertAcceleratorInit(SgSourceFile *sgfile) {
  bool hasMain = false;
  // find the main entry
  SgFunctionDefinition *mainDef = NULL;
  string mainName = "::main";
  ROSE_ASSERT(sgfile != NULL);

  SgFunctionDeclaration *mainDecl = findMain(sgfile);
  if (mainDecl != NULL) {
    // printf ("Found main function setting hasMain == true \n");
    mainDef = mainDecl->get_definition();
    hasMain = true;
  }

  // TODO declare pointers for threadprivate variables and global lock
  // addGlobalOmpDeclarations(ompfrontend, sgfile->get_globalScope(), hasMain );

  if (!hasMain)
    return;
  ROSE_ASSERT(mainDef != NULL); // Liao, at this point, we expect a defining
                                // declaration of main() is
  // look up symbol tables for symbols
  SgScopeStatement *currentscope = mainDef->get_body();
  SgBasicBlock *body = isSgBasicBlock(currentscope);
  ROSE_ASSERT(body != NULL);

  SgExprStatement *expStmt = buildFunctionCallStmt(
      SgName("rex_offload_init"), buildVoidType(), NULL, currentscope);
  setSourcePositionForTransformation(expStmt);
  // Insert before all user statements so one-time cubin registration is not
  // counted inside declaration initializers such as `long long time0 =
  // clock();`.
  prependStatement(expStmt, currentscope);

  // Do not auto-insert rex_offload_fini() at end of main. For standalone
  // processes the OS reclaims the registered image and device-side state on
  // exit, and forcing teardown into user-visible process lifetime adds a
  // measurable fixed cost to short-running GPU programs. Explicit teardown
  // remains available through rex_offload_fini() for callers that need it.

  return;
}

//! Replace references to oldVar within root with references to newVar
int replaceVariableReferences(SgNode *root, SgVariableSymbol *oldVar,
                              SgVariableSymbol *newVar) {
  ROSE_ASSERT(oldVar != NULL);
  ROSE_ASSERT(newVar != NULL);

  VariableSymbolMap_t varRemap;
  varRemap.insert(VariableSymbolMap_t::value_type(oldVar, newVar));
  return replaceVariableReferences(root, varRemap);
}

static bool shouldSkipOpenMPClauseVarRefRewrite(const SgVarRefExp *ref_orig) {
  return ref_orig != NULL && getEnclosingNode<SgOmpClause>(
                                 const_cast<SgVarRefExp *>(ref_orig)) != NULL;
}

static SgOmpClause *
requirePreservedOpenMPClauseVarRefRole(SgVarRefExp *reference) {
  if (reference == NULL || reference->get_symbol() == NULL) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[clause-reference-role]: reference=%p "
            "has no exact source symbol\n",
            static_cast<void *>(reference));
    ROSE_ABORT();
  }
  SgOmpClause *clause = getEnclosingNode<SgOmpClause>(reference);
  SgVariableSymbol *symbol = isSgVariableSymbol(reference->get_symbol());
  SgInitializedName *declaration =
      symbol != NULL ? symbol->get_declaration() : NULL;
  SgScopeStatement *scope =
      declaration != NULL ? declaration->get_scope() : NULL;
  SgSymbolTable *table = scope != NULL ? scope->get_symbol_table() : NULL;
  if (clause == NULL || symbol == NULL || declaration == NULL ||
      symbol->get_symbol_basis() != declaration || table == NULL ||
      symbol->get_parent() != table || !table->exists(symbol) ||
      scope->find_symbol_from_declaration(declaration) != symbol) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[clause-reference-role]: reference=%p "
            "symbol=%p declaration=%p scope=%p clause=%p has no exact "
            "preserved source identity\n",
            static_cast<void *>(reference), static_cast<void *>(symbol),
            static_cast<void *>(declaration), static_cast<void *>(scope),
            static_cast<void *>(clause));
    ROSE_ABORT();
  }

  std::unordered_set<SgNode *> visited;
  for (SgNode *child = reference; child != clause;) {
    SgNode *owner = child->get_parent();
    if (owner == NULL || !visited.insert(child).second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[clause-reference-role]: "
              "reference=%p has no acyclic exact clause owner chain\n",
              static_cast<void *>(reference));
      ROSE_ABORT();
    }
    const std::vector<SgNode *> successors =
        owner->get_traversalSuccessorContainer();
    if (std::count(successors.begin(), successors.end(), child) != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[clause-reference-role]: child=%p/"
              "%s owner=%p/%s is not one exact structural clause edge\n",
              static_cast<void *>(child), child->class_name().c_str(),
              static_cast<void *>(owner), owner->class_name().c_str());
      ROSE_ABORT();
    }
    child = owner;
  }
  return clause;
}

static void clearOpenMPClauseOriginalExpressionTrees(SgNode *root) {
  if (root == NULL) {
    return;
  }

  Rose_STL_Container<SgNode *> expr_nodes =
      NodeQuery::querySubTree(root, V_SgExpression);
  for (Rose_STL_Container<SgNode *>::const_iterator it = expr_nodes.begin();
       it != expr_nodes.end(); ++it) {
    SgExpression *expr = isSgExpression(*it);
    if (expr == NULL) {
      continue;
    }
    if (getEnclosingNode<SgOmpClause>(expr) == NULL) {
      continue;
    }
    if (SgExpression *source = expr->get_originalExpressionTree()) {
      if (source->get_parent() != expr) {
        fprintf(stderr,
                "REX_AST_INVARIANT[original-expression-provenance]: OpenMP "
                "lowering found a source expression without its exact owner\n");
        ROSE_ABORT();
      }
      expr->set_originalExpressionTree(NULL);
      source->set_parent(nullptr);
      SageInterface::deepDelete(source);
    }
  }
}

//! Replace variable references within root based on a map from old symbols to
//! new symbols
/* This function is mostly used by transOmpVariables() to handle private,
 * firstprivate, reduction, etc.
 *
 *
 */
int replaceVariableReferences(SgNode *root, VariableSymbolMap_t varRemap) {
  int result = 0;
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t refs = NodeQuery::querySubTree(root, V_SgVarRefExp);
  for (NodeList_t::iterator i = refs.begin(); i != refs.end(); ++i) {
    SgVarRefExp *ref_orig = isSgVarRefExp(*i);
    ROSE_ASSERT(ref_orig);
    if (shouldSkipOpenMPClauseVarRefRewrite(ref_orig)) {
      (void)requirePreservedOpenMPClauseVarRefRole(ref_orig);
      continue;
    }
    VariableSymbolMap_t::const_iterator iter =
        varRemap.find(ref_orig->get_symbol());
    if (iter != varRemap.end()) {
      SgVariableSymbol *newSym = iter->second;
      rebindTransformedVariableReference(
          ref_orig, ref_orig->get_symbol(), newSym,
          "openmp-private-variable-substitution");
      result++;
    }
  }
  return result;
}

int replaceVariablesWithPointerDereference(SgNode *root,
                                           ASTtools::VarSymSet_t vars) {
  int result = 0;
  typedef Rose_STL_Container<SgNode *> NodeList_t;
  NodeList_t refs = NodeQuery::querySubTree(root, V_SgVarRefExp);
  for (NodeList_t::iterator i = refs.begin(); i != refs.end(); ++i) {
    SgVarRefExp *ref_orig = isSgVarRefExp(*i);
    ROSE_ASSERT(ref_orig);
    if (shouldSkipOpenMPClauseVarRefRewrite(ref_orig)) {
      continue;
    }
    ASTtools::VarSymSet_t::const_iterator ii =
        vars.find(ref_orig->get_symbol());
    if (ii != vars.end()) {
      SgPointerType *pointer_type =
          isSgPointerType(stripTypeAliases(ref_orig->get_type()));
      ROSE_ASSERT(pointer_type != nullptr);
      SgExpression *ptr_ref = buildPointerDerefExp(
          copyExpression(ref_orig), pointer_type->get_base_type());
      ptr_ref->set_need_paren(true);
      SageInterface::replaceExpression(ref_orig, ptr_ref);
      result++;
    }
  }
  return result;
}

// The LLVM OpenMP loop runtime consumes an inclusive terminal induction value
// and a signed increment.  C and C++ canonical loops retain a positive stride
// magnitude in the AST and may use a strict comparison, so adapt that typed
// source contract before crossing the runtime ABI boundary.
static SgExpression *buildKmpcInclusiveUpperBound(SgExpression *source_upper,
                                                  bool is_incremental,
                                                  bool is_inclusive,
                                                  bool is_fortran) {
  if (source_upper == nullptr || source_upper->get_type() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-loop-upper]: source upper "
            "bound has no exact type\n");
    ROSE_ABORT();
  }
  if (is_fortran || is_inclusive)
    return copyExpression(source_upper);

  SgExpression *upper = copyExpression(source_upper);
  SgExpression *unit = buildIntVal(1);
  return is_incremental
             ? static_cast<SgExpression *>(
                   buildSubtractOp(upper, unit, source_upper->get_type()))
             : static_cast<SgExpression *>(
                   buildAddOp(upper, unit, source_upper->get_type()));
}

static SgExpression *buildKmpcSignedStride(SgExpression *source_stride,
                                           bool is_incremental,
                                           bool is_fortran) {
  if (source_stride == nullptr || source_stride->get_type() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-loop-stride]: source stride "
            "has no exact type\n");
    ROSE_ABORT();
  }
  SgExpression *stride = copyExpression(source_stride);
  if (is_fortran || is_incremental)
    return stride;
  return buildMinusOp(stride, source_stride->get_type());
}

static SgStatement *generateTargetReduceOnCPU(std::string orig_var,
                                              SgVariableSymbol *buffer_decl,
                                              SgVariableDeclaration *num_blocks,
                                              int r_operator) {
  SgType *index_type = buildIntType();
  SgVariableDeclaration *init_stmt = buildVariableDeclaration(
      "i", index_type, buildAssignInitializer(buildIntVal(0), index_type),
      num_blocks->get_scope());
  SgType *comparison_type = SageInterface::is_C_language()
                                ? static_cast<SgType *>(buildIntType())
                                : static_cast<SgType *>(buildBoolType());
  SgStatement *cond_stmt = buildExprStatement(buildLessThanOp(
      buildVarRefExp(init_stmt), buildVarRefExp(num_blocks), comparison_type));
  SgExpression *incr_exp = buildPlusPlusOp(buildVarRefExp(init_stmt),
                                           index_type, SgUnaryOp::postfix);
  SgStatement *loop_body = NULL;
  switch (r_operator) {
  case 6:   // SgOmpClause::e_omp_reduction_plus
  case 7: { // SgOmpClause::e_omp_reduction_minus
    SgPointerType *buffer_type =
        isSgPointerType(stripTypeAliases(buffer_decl->get_type()));
    ROSE_ASSERT(buffer_type != nullptr);
    SgExpression *buffer_element = buildPntrArrRefExp(
        buildVarRefExp(buffer_decl), buildVarRefExp(init_stmt),
        buffer_type->get_base_type());
    SgExpression *reduction_target = buildVarRefExp(orig_var);
    loop_body = buildExprStatement(buildPlusAssignOp(
        reduction_target, buffer_element, reduction_target->get_type()));
    break;
  }
  default:
    ROSE_ASSERT(0 && "Unsupported reduction operator is met.");
  }
  ROSE_ASSERT(loop_body != NULL);
  SgStatement *for_stmt =
      buildForStatement_nfi(init_stmt, cond_stmt, incr_exp, loop_body);

  return for_stmt;
}

//! check if an omp for/do loop use static schedule or not
// Static schedule include: default schedule, or schedule(static[,chunk_size])
bool useStaticSchedule(SgOmpClauseBodyStatement *omp_loop) {
  ROSE_ASSERT(omp_loop);
  bool result = false;
  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(omp_loop, V_SgOmpScheduleClause);
  if (clauses.size() == 0) {
    result = true; // default schedule is static
  } else {
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    if (s_clause->get_kind() == SgOmpClause::e_omp_schedule_kind_static)
      result = true;
  }
  return result;
}

// Chunk size  for dynamic and guided schedule should be 1 if not specified.
static SgExpression *createAdjustedChunkSize(SgExpression *orig_chunk_size) {
  SgExpression *result = NULL;
  if (orig_chunk_size)
    result = copyExpression(orig_chunk_size);
  else
    result = buildIntVal(1);
  ROSE_ASSERT(result != NULL);
  return result;
}
// Convert a schedule kind enum value to a small case string
string toString(SgOmpClause::omp_schedule_kind_enum s_kind) {
  string result;
  if (s_kind == SgOmpClause::e_omp_schedule_kind_static) {
    result = "static";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_dynamic) {
    result = "dynamic";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_guided) {
    result = "guided";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_runtime) {
    result = "runtime";
  } else if (s_kind == SgOmpClause::e_omp_schedule_kind_auto) {
    //      cerr<<"GOMP does not provide an implementation for
    //      schedule(auto)....."<<endl;
    result = "auto";
  } else {
    cerr << "Error: illegal or unhandled schedule kind:" << s_kind << endl;
    ROSE_ABORT();
  }
  return result;
}

//! Generate XOMP loop schedule init function's name
string
generateGOMPLoopInitFuncName(bool isOrdered,
                             SgOmpClause::omp_schedule_kind_enum s_kind) {
  // XOMP_loop_static_init()
  // XOMP_loop_ordered_static_init ()
  // XOMP_loop_dynamic_init ()
  // XOMP_loop_ordered_dynamic_init ()
  // .....
  string result;
  result = "XOMP_loop_";
  // Handled ordered
  if (isOrdered)
    result += "ordered_";
  result += toString(s_kind);
  result += "_init";
  return result;
}

//! Generate XOMP loop schedule start function's name
string
generateXOMPLoopStartFuncName(bool isOrdered,
                              SgOmpClause::omp_schedule_kind_enum s_kind) {
  // XOMP_loop_static_start ()
  // XOMP_loop_ordered_static_start ()
  // XOMP_loop_dynamic_start ()
  // XOMP_loop_ordered_dynamic_start ()
  // .....
  string result;
  result = "XOMP_loop_";
  // Handled ordered
  if (isOrdered)
    result += "ordered_";
  result += toString(s_kind);
  result += "_start";
  return result;
}

//! Generate XOMP loop schedule next function's name
string
generateXOMPLoopNextFuncName(bool isOrdered,
                             SgOmpClause::omp_schedule_kind_enum s_kind) {
  string result;
  // XOMP_loop_static_next()
  // XOMP_loop_ordered_static_next ()
  // XOMP_loop_dynamic_next ()
  // XOMP_loop_ordered_dynamic_next()
  // .....

  result = "XOMP_loop_";
  if (isOrdered)
    result += "ordered_";
  result += toString(s_kind);
  result += "_next";
  return result;
}

//! Fortran only action: insert include "libxompf.fh" into the function body
//! with calls to XOMP_loop_* functions
// This is necessary since XOMP_loop_* functions will be treated as returning
// REAL by implicit rules (starting with X) This function finds the function
// definition enclosing a start node, check if there is any existing include
// 'libxompf.fh' then insert one if there is none.
static void insert_libxompf_h(SgNode *startNode) {
  ROSE_ASSERT(startNode != NULL);
  // This function should not be used for other than Fortran
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  // we don't expect input node is a func def already
  ROSE_ASSERT(isSgFunctionDefinition(startNode) == NULL);

  SgBasicBlock *t_body = getEnclosingRegionOrFuncDefinition(startNode);
  ROSE_ASSERT(t_body != NULL);
  // Try to find an existing include 'libxompf.fh'
  // Assumptions:
  //   1. It only shows up at the top level, not within other SgBasicBlock
  //   2. The startNode is after the include line
  SgStatement *s_include = NULL; // existing include
  SgStatementPtrList stmt_list = t_body->get_statements();
  SgStatementPtrList::iterator iter;
  for (iter = stmt_list.begin(); iter != stmt_list.end(); iter++) {
    SgStatement *stmt = *iter;
    ROSE_ASSERT(stmt != NULL);
    SgFortranIncludeLine *f_inc = isSgFortranIncludeLine(stmt);
    if (f_inc) {
      string f_name =
          StringUtility::stripPathFromFileName(f_inc->get_filename());
      if (f_name == "libxompf.fh" || f_name == "libxompf.h") {
        s_include = f_inc;
        break;
      }
    }
  }
  if (s_include == NULL) {
    s_include = buildFortranIncludeLine("libxompf.fh");
    SgStatement *l_stmt = findLastDeclarationStatement(t_body);
    if (l_stmt)
      insertStatementAfter(l_stmt, s_include);
    else
      prependStatement(s_include, t_body);
  }
}
//! Translate an omp for loop with non-static scheduling clause or with ordered
//! clause ()
// bb1 is the basic block to insert the translated loop
// bb1 already has compiler-generated variable declarations for new loop control
// variables
/*
 * start, end, incremental, chunk_size, own_start, own_end
 XOMP_loop_static_init(int lower, int upper, int stride, int chunk_size);

 if (GOMP_loop_dynamic_start (orig_lower, orig_upper, adj_stride, orig_chunk,
&_p_lower, &_p_upper))
//  if (GOMP_loop_ordered_dynamic_start (S, E, INCR, CHUNK, &_p_lower,
&_p_upper))
{
do
{
for (_p_index = _p_lower; _p_index < _p_upper; _p_index += orig_stride)
set_data (_p_index, iam);
}
while (GOMP_loop_dynamic_next (&_p_lower, &_p_upper));
// while (GOMP_loop_ordered_dynamic_next (&_p_lower, &_p_upper));
}
GOMP_loop_end ();
//  GOMP_loop_end_nowait ();
//
// More explanation: -------------------------------------------
// Omni uses the following translation
_ompc_dynamic_sched_init(_p_loop_lower,_p_loop_upper,_p_loop_stride,5);
while(_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper)){
for (_p_loop_index = _p_loop_lower; (_p_loop_index) < _p_loop_upper;
_p_loop_index += _p_loop_stride) { k_3++;
}
}
// In order to merge two kinds of translations into one scheme.
// we split
while(_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper)){
for (_p_loop_index = _p_loop_lower; (_p_loop_index) < _p_loop_upper;
_p_loop_index += _p_loop_stride) { k_3++;
}
}

// to
if (_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper)){
do {
for (_p_loop_index = _p_loop_lower; (_p_loop_index) < _p_loop_upper;
_p_loop_index += _p_loop_stride) { k_3++;
}
} while (_ompc_dynamic_sched_next(&_p_loop_lower,&_p_loop_upper));
}
// and XOMP layer will compensate for the difference.
*/
static void transOmpLoop_others(
    SgOmpClauseBodyStatement *target, SgStatement *associated_loop,
    SgVariableDeclaration *index_decl, SgVariableDeclaration *lower_decl,
    SgVariableDeclaration *upper_decl, SgVariableDeclaration *stride_decl,
    SgVariableDeclaration *last_iter_decl, SgBasicBlock *bb1,
    const KmpcGlobalTidSourceContext &tid_context,
    SgExpression *runtime_upper_bound, bool source_is_incremental,
    SageInterface::CanonicalFortranLoopDirection source_fortran_direction) {
  ROSE_ASSERT(target != NULL);
  ROSE_ASSERT(index_decl != NULL);
  ROSE_ASSERT(lower_decl != NULL);
  ROSE_ASSERT(upper_decl != NULL);
  ROSE_ASSERT(bb1 != NULL);
  if (associated_loop == nullptr || target->get_body() != associated_loop ||
      associated_loop->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[omp-loop-detached-transaction]: "
            "detached directive=%p no longer owns associated loop=%p\n",
            static_cast<void *>(target), static_cast<void *>(associated_loop));
    ROSE_ABORT();
  }
  // The OpenMP syntax requires that the omp for pragma is immediately followed
  // by the for loop.
  SgForStatement *for_loop = isSgForStatement(associated_loop);
  SgFortranDo *do_loop = isSgFortranDo(associated_loop);
  SgStatement *loop =
      for_loop != NULL ? (SgStatement *)for_loop : (SgStatement *)do_loop;
  target->set_body(nullptr);
  associated_loop->set_parent(nullptr);

  SgExprListExp *parameters = NULL;
  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(tid_context, bb1, &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), bb1);
  if (SageInterface::is_Fortran_language())
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration, bb1);
  else
    appendStatement(kmpc_global_tid_declaration, bb1);
  if (kmpc_global_tid_init != NULL)
    appendStatement(kmpc_global_tid_init, bb1);
  SgExpression *source_location_info = buildIntVal(0);

  SgInitializedName *orig_index;
  SgExpression *orig_lower, *orig_upper, *orig_stride;
  bool isIncremental = true; // if the loop iteration space is incremental
  SageInterface::CanonicalFortranLoopDirection fortran_direction =
      SageInterface::CanonicalFortranLoopDirection::runtime;
  bool isInclusiveUpperBound = do_loop != nullptr;
  // grab the original loop 's controlling information
  bool is_canonical = false;
  if (for_loop)
    is_canonical = isCanonicalForLoop(for_loop, &orig_index, &orig_lower,
                                      &orig_upper, &orig_stride, NULL,
                                      &isIncremental, &isInclusiveUpperBound);
  else if (do_loop) {
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, &fortran_direction, NULL);
  } else {
    cerr << "error! transOmpLoop_others(). loop is neither for_loop nor "
            "do_loop. Aborting.."
         << endl;
    ROSE_ABORT();
  }
  ROSE_ASSERT(is_canonical == true);
  if (runtime_upper_bound == nullptr ||
      runtime_upper_bound->get_parent() != nullptr ||
      (for_loop != nullptr && isIncremental != source_is_incremental) ||
      (do_loop != nullptr && fortran_direction != source_fortran_direction)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-loop-contract]: detached "
            "upper=%p parent=%p direction=%d expected=%d is not exact\n",
            static_cast<void *>(runtime_upper_bound),
            static_cast<void *>(runtime_upper_bound != nullptr
                                    ? runtime_upper_bound->get_parent()
                                    : nullptr),
            isIncremental ? 1 : 0, source_is_incremental ? 1 : 0);
    ROSE_ABORT();
  }

  SgType *loop_control_type = getFirstVariable(*lower_decl).get_type();
  ROSE_ASSERT(loop_control_type != nullptr);
  SgType *logical_result_type = SageInterface::is_C_language()
                                    ? static_cast<SgType *>(buildIntType())
                                    : static_cast<SgType *>(buildBoolType());
  auto build_declaration_address = [](SgVariableDeclaration *declaration) {
    SgExpression *reference = buildVarRefExp(declaration);
    return buildExactAddressOfOp(reference);
  };

  const bool use_64_runtime = use_kmpc_loop_64bit_runtime(
      getFirstVariable(*lower_decl).get_type(), bb1);

  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(target, V_SgOmpScheduleClause);

  // the case of with the ordered schedule, but without any schedule policy
  // specified treat it as (static, 0) based on GCC's translation
  SgOmpClause::omp_schedule_kind_enum s_kind =
      SgOmpClause::e_omp_schedule_kind_static;
  SgExpression *orig_chunk_size = NULL;
  string func_init_name =
      getKmpcRuntimeFunctionName(get_kmpc_for_static_init_name(use_64_runtime));
  int32_t schedule_type = 0;
  bool hasOrder = false;
  if (hasClause(target, V_SgOmpOrderedClause))
    hasOrder = true;
  ROSE_ASSERT(hasOrder || clauses.size() != 0);
  // Most cases: with schedule(kind,chunk_size)
  if (clauses.size() != 0) {
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    s_kind = s_clause->get_kind();
    orig_chunk_size = s_clause->get_chunk_size();
    SgOmpClause::omp_schedule_modifier_enum schedule_modifier =
        s_clause->get_modifier();
    if ((hasOrder || s_kind == SgOmpClause::e_omp_schedule_kind_static) &&
        schedule_modifier !=
            SgOmpClause::e_omp_schedule_modifier_nonmonotonic) {
      schedule_type = kmp_sched_modifier_monotonic;
    } else {
      schedule_type = kmp_sched_modifier_nonmonotonic;
    };

    // chunk size is 1 for dynamic and guided schedule, if not specified.
    if (s_kind == SgOmpClause::e_omp_schedule_kind_dynamic ||
        s_kind == SgOmpClause::e_omp_schedule_kind_guided) {
      orig_chunk_size = createAdjustedChunkSize(orig_chunk_size);
      func_init_name = getKmpcRuntimeFunctionName(
          get_kmpc_dispatch_init_name(use_64_runtime));
      if (s_kind == SgOmpClause::e_omp_schedule_kind_dynamic) {
        schedule_type += kmp_sched_dynamic;
      } else {
        schedule_type += kmp_sched_guided;
      };
      parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildIntVal(schedule_type),
          buildVarRefExp(lower_decl), buildVarRefExp(upper_decl),
          buildVarRefExp(stride_decl), orig_chunk_size);

    } else if (s_kind == SgOmpClause::e_omp_schedule_kind_auto ||
               s_kind == SgOmpClause::e_omp_schedule_kind_runtime) {
      orig_chunk_size = buildIntVal(1);
      func_init_name = getKmpcRuntimeFunctionName(
          get_kmpc_dispatch_init_name(use_64_runtime));
      if (s_kind == SgOmpClause::e_omp_schedule_kind_auto) {
        schedule_type += kmp_sched_auto;
      } else {
        schedule_type += kmp_sched_runtime;
      };
      parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildIntVal(schedule_type),
          buildVarRefExp(lower_decl), buildVarRefExp(upper_decl),
          buildVarRefExp(stride_decl), orig_chunk_size);

    } else {
      if (orig_chunk_size == NULL)
        orig_chunk_size = buildIntVal(0);
      schedule_type += kmp_sched_static_chunk;
      SgExpression *e_last_iter = nullptr;
      SgExpression *e_lower = nullptr;
      SgExpression *e_upper = nullptr;
      SgExpression *e_stride = nullptr;
      if (do_loop != nullptr) {
        // Fortran arguments are pass-by-reference already.
        e_last_iter = buildVarRefExp(last_iter_decl);
        e_lower = buildVarRefExp(lower_decl);
        e_upper = buildVarRefExp(upper_decl);
        e_stride = buildVarRefExp(stride_decl);
      } else {
        e_last_iter = build_declaration_address(last_iter_decl);
        e_lower = build_declaration_address(lower_decl);
        e_upper = build_declaration_address(upper_decl);
        e_stride = build_declaration_address(stride_decl);
      }
      SgExpression *runtime_increment = do_loop != nullptr
                                            ? copyExpression(orig_stride)
                                            : buildVarRefExp(stride_decl);
      parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildIntVal(schedule_type),
          e_last_iter, e_lower, e_upper, e_stride, runtime_increment,
          copyExpression(orig_chunk_size));
    }
  } else
    orig_chunk_size = buildIntVal(0);

  // schedule(auto) does not have chunk size
  if (s_kind != SgOmpClause::e_omp_schedule_kind_auto &&
      s_kind != SgOmpClause::e_omp_schedule_kind_runtime)
    ROSE_ASSERT(orig_chunk_size != NULL);
  const bool use_dispatch_runtime =
      s_kind == SgOmpClause::e_omp_schedule_kind_dynamic ||
      s_kind == SgOmpClause::e_omp_schedule_kind_guided ||
      s_kind == SgOmpClause::e_omp_schedule_kind_auto ||
      s_kind == SgOmpClause::e_omp_schedule_kind_runtime;

  if (SageInterface::is_Fortran_language() && use_dispatch_runtime) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(bb1);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(
        func_def->get_body(),
        SgName(getKmpcRuntimeFunctionName(
            get_kmpc_dispatch_next_name(use_64_runtime))),
        buildKmpcInt32Type());
  }

  SgExprStatement *func_init_stmt =
      buildFunctionCallStmt(func_init_name, buildVoidType(), parameters, bb1);
  appendStatement(func_init_stmt, bb1);

  auto build_dispatch_next_expr = [&]() -> SgExpression * {
    SgExprListExp *dispatch_parameters = NULL;
    if (for_loop) {
      dispatch_parameters =
          buildExprListExp(copyExpression(source_location_info),
                           copyExpression(thread_global_tid),
                           build_declaration_address(last_iter_decl),
                           build_declaration_address(lower_decl),
                           build_declaration_address(upper_decl),
                           build_declaration_address(stride_decl));
    } else {
      dispatch_parameters = buildExprListExp(
          copyExpression(source_location_info),
          copyExpression(thread_global_tid), buildVarRefExp(last_iter_decl),
          buildVarRefExp(lower_decl), buildVarRefExp(upper_decl),
          buildVarRefExp(stride_decl));
    }
    return buildFunctionCallExp(
        getKmpcRuntimeFunctionName(get_kmpc_dispatch_next_name(use_64_runtime)),
        SageInterface::is_Fortran_language()
            ? static_cast<SgType *>(buildKmpcInt32Type())
            : static_cast<SgType *>(buildIntType()),
        dispatch_parameters, bb1);
  };

  auto build_static_chunk_continue_expr = [&]() -> SgExpression * {
    if (do_loop != nullptr) {
      SgExpression *increasing =
          buildAndOp(buildGreaterThanOp(copyExpression(orig_stride),
                                        buildIntVal(0), logical_result_type),
                     buildLessOrEqualOp(buildVarRefExp(lower_decl),
                                        copyExpression(runtime_upper_bound),
                                        logical_result_type),
                     logical_result_type);
      SgExpression *decreasing =
          buildAndOp(buildLessThanOp(copyExpression(orig_stride),
                                     buildIntVal(0), logical_result_type),
                     buildGreaterOrEqualOp(buildVarRefExp(lower_decl),
                                           copyExpression(runtime_upper_bound),
                                           logical_result_type),
                     logical_result_type);
      return buildOrOp(increasing, decreasing, logical_result_type);
    }
    return isIncremental
               ? static_cast<SgExpression *>(buildLessOrEqualOp(
                     buildVarRefExp(lower_decl),
                     copyExpression(runtime_upper_bound), logical_result_type))
               : static_cast<SgExpression *>(buildGreaterOrEqualOp(
                     buildVarRefExp(lower_decl),
                     copyExpression(runtime_upper_bound), logical_result_type));
  };

  auto build_upper_clamp_stmt = [&]() -> SgStatement * {
    SgExpression *if_condition = nullptr;
    if (do_loop != nullptr) {
      SgExpression *increasing =
          buildAndOp(buildGreaterThanOp(copyExpression(orig_stride),
                                        buildIntVal(0), logical_result_type),
                     buildGreaterThanOp(buildVarRefExp(upper_decl),
                                        copyExpression(runtime_upper_bound),
                                        logical_result_type),
                     logical_result_type);
      SgExpression *decreasing =
          buildAndOp(buildLessThanOp(copyExpression(orig_stride),
                                     buildIntVal(0), logical_result_type),
                     buildLessThanOp(buildVarRefExp(upper_decl),
                                     copyExpression(runtime_upper_bound),
                                     logical_result_type),
                     logical_result_type);
      if_condition = buildOrOp(increasing, decreasing, logical_result_type);
    } else {
      if_condition =
          isIncremental
              ? static_cast<SgExpression *>(buildGreaterThanOp(
                    buildVarRefExp(upper_decl),
                    copyExpression(runtime_upper_bound), logical_result_type))
              : static_cast<SgExpression *>(buildLessThanOp(
                    buildVarRefExp(upper_decl),
                    copyExpression(runtime_upper_bound), logical_result_type));
    }
    SgExprStatement *update_upper_bound_stmt = buildAssignStatement(
        buildVarRefExp(upper_decl), copyExpression(runtime_upper_bound));
    SgStatement *if_true_body = update_upper_bound_stmt;
    if (SageInterface::is_Fortran_language()) {
      // Assemble the generated block as one detached subtree. It acquires its
      // exact physical output owner only when the enclosing if statement is
      // appended to the already attached lowering block.
      if_true_body = buildBasicBlock(update_upper_bound_stmt);
    }
    return buildIfStmt(if_condition, if_true_body, NULL);
  };

  auto append_static_chunk_advance = [&](SgBasicBlock *scope) {
    if (SageInterface::is_Fortran_language()) {
      appendStatement(
          buildAssignStatement(buildVarRefExp(lower_decl),
                               buildAddOp(buildVarRefExp(lower_decl),
                                          buildVarRefExp(stride_decl),
                                          loop_control_type)),
          scope);
      appendStatement(
          buildAssignStatement(buildVarRefExp(upper_decl),
                               buildAddOp(buildVarRefExp(upper_decl),
                                          buildVarRefExp(stride_decl),
                                          loop_control_type)),
          scope);
      return;
    }

    appendStatement(buildExprStatement(buildPlusAssignOp(
                        buildVarRefExp(lower_decl), buildVarRefExp(stride_decl),
                        loop_control_type)),
                    scope);
    appendStatement(buildExprStatement(buildPlusAssignOp(
                        buildVarRefExp(upper_decl), buildVarRefExp(stride_decl),
                        loop_control_type)),
                    scope);
  };

  SgBasicBlock *true_body = buildBasicBlock();
  if (SageInterface::is_Fortran_language()) {
    SgExpression *entry_cond = NULL;
    if (use_dispatch_runtime) {
      entry_cond = buildEqualityOp(build_dispatch_next_expr(), buildIntVal(1),
                                   logical_result_type);
    } else {
      entry_cond = build_static_chunk_continue_expr();
    }
    SgIfStmt *if_stmt = buildIfStmt(entry_cond, true_body, NULL);
    appendStatement(if_stmt, bb1);
  } else {
    appendStatement(true_body, bb1);
  }

  // do {} while (__kmpc_dispatch_next_*(...)) or while (lower <= upper)
  if (for_loop) {
    SgExpression *func_next_exp = NULL;
    if (use_dispatch_runtime) {
      func_next_exp = build_dispatch_next_expr();
    } else {
      func_next_exp = build_static_chunk_continue_expr();
    }
    SgBasicBlock *do_body = buildBasicBlock();
    SgWhileStmt *while_do_stmt = buildWhileStmt(func_next_exp, do_body);
    appendStatement(while_do_stmt, true_body);

    appendStatement(build_upper_clamp_stmt(), do_body);

    // insert the loop into do-while
    appendStatement(loop, do_body);
    if (!use_dispatch_runtime) {
      append_static_chunk_advance(do_body);
      parameters =
          buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
      appendStatement(buildFunctionCallStmt(
                          getKmpcRuntimeFunctionName("__kmpc_for_static_fini"),
                          buildVoidType(), parameters, bb1),
                      bb1);
    };
  }
  // Liao 1/7/2011, Fortran does not support SgDoWhileStmt
  // We use the following control flow as an alternative:
  //   label  continue
  //          loop_here
  //          if (GOMP_loop_static_next (&_p_lower, &_p_upper))
  //             goto label
  else if (do_loop) {
    SgFunctionDefinition *funcDef = getEnclosingFunctionDefinition(bb1);
    ROSE_ASSERT(funcDef != NULL);
    // label  CONTINUE
    SgLabelStatement *label_stmt_1 =
        buildLabelStatement("", buildFortranContinueStmt());
    appendStatement(label_stmt_1, true_body);
    int l_val = suggestNextNumericLabel(funcDef);
    setFortranNumericLabel(label_stmt_1, l_val,
                           SgLabelSymbol::e_start_label_type, funcDef);
    appendStatement(build_upper_clamp_stmt(), true_body);
    // loop here
    appendStatement(loop, true_body);

    if (!use_dispatch_runtime)
      append_static_chunk_advance(true_body);

    // if () goto label
    SgExpression *func_next_exp = NULL;
    if (use_dispatch_runtime) {
      func_next_exp = buildEqualityOp(build_dispatch_next_expr(),
                                      buildIntVal(1), logical_result_type);
    } else {
      func_next_exp = build_static_chunk_continue_expr();
    }
    SgGotoStatement *gt_stmt =
        buildGotoStatement(label_stmt_1->get_numeric_label()->get_symbol());
    // Assemble both branches while detached.  appendStatement is an attached
    // publication transaction and therefore cannot use a not-yet-published
    // basic block as its physical output owner.
    SgIfStmt *if_stmt_2 =
        buildIfStmt(func_next_exp, buildBasicBlock(gt_stmt), buildBasicBlock());
    appendStatement(if_stmt_2, true_body);
    // assertion from unparser
    SgStatementPtrList &statementList =
        isSgBasicBlock(if_stmt_2->get_true_body())->get_statements();
    ROSE_ASSERT(statementList.size() == 1);

    if (!use_dispatch_runtime) {
      parameters =
          buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
      appendStatement(buildFunctionCallStmt(
                          getKmpcRuntimeFunctionName("__kmpc_for_static_fini"),
                          buildVoidType(), parameters, bb1),
                      bb1);
    }
  }

  // Rewrite loop control variables
  replaceVariableReferences(
      loop, isSgVariableSymbol(orig_index->get_symbol_from_symbol_table()),
      getFirstVarSym(index_decl));
  SageInterface::setLoopLowerBound(loop, buildVarRefExp(lower_decl));
  if (for_loop != nullptr)
    SageInterface::setCanonicalForLoopInclusiveComparison(for_loop,
                                                          isIncremental);
  SageInterface::setLoopUpperBound(loop, buildVarRefExp(upper_decl));
  transOmpVariables(target, bb1,
                    runtime_upper_bound); // This should happen before the
                                          // barrier is inserted.
  if (!hasClause(target, V_SgOmpNowaitClause)) {
    parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    appendStatement(
        buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_barrier"),
                              buildVoidType(), parameters, bb1),
        bb1);
  }
}

// Expected AST
// * OmpForStatement
// ** SgForStatement
// Algorithm:
// Loop normalization first  for stop condition expressions
//   <: for (i= 0;i <20; i++) --> for (i= 0;i <20; i+=1)  [0,20, +1] to pass to
//   runtime calls
//  <=: for (i= 0;i<=20; i++) --> for (i= 0;i <21; i+=1)
//   >: for (i=20;i >-1; i--) --> for (i=20;i >-1; i-=1) [20, -1, -1]
//  >=: for (i=20;i>= 0; i--) --> for (i=20;i >-1; i-=1)
// We have a SageInterface::forLoopNormalization() which does the opposite
// (normalizing a C loop to a Fortran style loop) < --> <= and > --> >=,
// GCC-GOMP use compiler-generated statements to schedule loop iterations using
// static schedule All other schedule policies use runtime calls instead. We
// translate static schedule here and non-static ones in transOmpLoop_others()
//
// Static schedule, including:
// 1. default (static even) case
// 2. schedule(static[, chunk_size]): == static even if chunk_size is not
// specified
// gomp does not provide a runtime call to calculate loop control values
// for the default (static even) scheduling
// compilers have to generate the statements to do this. I HATE THIS!!!
// the loop scheduling algorithm for the default case is
/*
// calculate loop iteration count from lower, upper and stride , no -1 if upper
is an inclusive bound int _p_iter_count = (stride + -1 + upper - lower )/stride;
// calculate a proper chunk size
// two cases: evenly divisible  20/5 =4
//   not evenly divisible 20/3= 6
// Initial candidate

int _p_num_threads = omp_get_num_threads ();
_p_chunk_size = _p_iter_count / _p_num_threads;
int _p_ck_temp = (_p_chunk_size * _p_num_threads) != _p_iter_count;
// increase the chunk size by 1 if not evenly divisible
_p_chunk_size = _p_ck_temp + _p_chunk_size;

// decide on the lower and upper bound for the current thread
int _p_thread_id = omp_get_thread_num ();
_p_lower = lower + _p_chunk_size * _p_thread_id * stride;a
// -1 if upper is an inclusive bound
_p_upper = _p_lower + _p_chunk_size * stride;

// adjust the upper bound
_p_upper = MIN_EXPR <_p_upper, upper>;
// _p_upper = _p_upper<upper? _p_upper: upper;
// Note: decremental iteration space needs some minor changes to the algorithm
above.
// stride should be negated
// MIN_EXP should be MAX_EXP
// upper bound adjustment should be +1 instead of -1
*/
void transOmpLoop(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpForStatement *target1 = isSgOmpForStatement(node);
  SgOmpDoStatement *target2 = isSgOmpDoStatement(node);

  SgOmpClauseBodyStatement *target =
      (target1 != NULL ? (SgOmpClauseBodyStatement *)target1
                       : (SgOmpClauseBodyStatement *)target2);
  ROSE_ASSERT(target != NULL);

  // Lowering replaces the directive before all runtime declarations are
  // materialized. Capture the immutable source identity while the directive
  // is still attached to its exact function; a detached node is not a valid
  // source for enclosing-scope or source-location queries.
  const KmpcGlobalTidSourceContext tid_context =
      require_kmpc_global_tid_source_context(target);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  SgStatement *loop = requireExactAssociatedLoop(
      target, body, AssociatedLoopPathContract::Worksharing, "omp-loop");
  SgForStatement *for_loop = isSgForStatement(loop);
  SgFortranDo *do_loop = isSgFortranDo(loop);

  if (for_loop != NULL) {
    // Outlined OpenMP regions can represent induction variables as pointer
    // dereferences (e.g., *i, *(*ip__)). Rewrite them to local scalar indices
    // before canonical normalization/analysis.
    rewritePointerBasedForIndices(for_loop);
  }

  SgExprListExp *parameters = NULL;
  SgExpression *source_location_info = buildIntVal(0);

  // Step 1. Loop normalization
  // we reuse the normalization from SageInterface, though it is different from
  // what gomp expects. the point is to have a consistent loop form. We can
  // adjust the difference later on.
  if (for_loop) {
    SageInterface::forLoopNormalization(for_loop);
  } else if (do_loop) {
    SageInterface::doLoopNormalization(do_loop);
  }

  SgInitializedName *orig_index = NULL;
  SgExpression *orig_lower = NULL;
  SgExpression *orig_upper = NULL;
  SgExpression *orig_stride = NULL;
  bool isIncremental = true; // if the loop iteration space is incremental
  SageInterface::CanonicalFortranLoopDirection fortran_direction =
      SageInterface::CanonicalFortranLoopDirection::runtime;
  bool isInclusiveUpperBound = do_loop != nullptr;
  // grab the original loop 's controlling information
  bool is_canonical = false;
  if (for_loop)
    is_canonical = isCanonicalForLoop(for_loop, &orig_index, &orig_lower,
                                      &orig_upper, &orig_stride, NULL,
                                      &isIncremental, &isInclusiveUpperBound);
  else if (do_loop)
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, &fortran_direction, NULL);
  if (!is_canonical) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[omp-loop]: loop=%p kind=%s is not "
            "exactly canonical after normalization\n",
            static_cast<void *>(loop), loop->sage_class_name());
    ROSE_ABORT();
  }
  SgExpression *runtime_upper_bound = buildKmpcInclusiveUpperBound(
      orig_upper, isIncremental, isInclusiveUpperBound, do_loop != nullptr);
  if (runtime_upper_bound == nullptr ||
      runtime_upper_bound->get_parent() != nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-loop-upper]: generated "
            "inclusive upper=%p is not detached\n",
            static_cast<void *>(runtime_upper_bound));
    ROSE_ABORT();
  }

  // step 2. Insert a basic block to replace OmpForStatement
  // This newly introduced scope is used to hold loop variables, private
  // variables ,etc
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();

  replaceStatement(target, bb1, true);

  // TODO handle preprocessing information
  //  Save some preprocessing information for later restoration.
  //   AttachedPreprocessingInfoType ppi_before, ppi_after;
  //   ASTtools::cutPreprocInfo (s, PreprocessingInfo::before, ppi_before);
  //   ASTtools::cutPreprocInfo (s, PreprocessingInfo::after, ppi_after);

  // Declare local loop control variables: _p_loop_index _p_loop_lower
  // _p_loop_upper , no change to the original stride
  SgType *loop_var_type = NULL;
  // Use 64-bit loop controls only when the target ABI requires it.
  if (for_loop) {
    bool use_64bit_loop_vars =
        use_kmpc_loop_64bit_runtime(buildLongType(), bb1);
    if (use_64bit_loop_vars)
      loop_var_type = buildLongType();
    else
      loop_var_type = buildIntType();
  } else if (do_loop) {
    loop_var_type = orig_index != NULL ? orig_index->get_type() : NULL;
    if (loop_var_type == NULL || (isSgTypeInt(loop_var_type) == NULL &&
                                  isSgTypeSignedInt(loop_var_type) == NULL)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-loop-control-type]: "
              "canonical DO induction variable=%p has no exact integer "
              "semantic type\n",
              static_cast<void *>(orig_index));
      ROSE_ABORT();
    }
  }
  SgVariableDeclaration *index_decl = NULL;
  SgVariableDeclaration *lower_decl = NULL;
  SgVariableDeclaration *upper_decl = NULL;
  SgVariableDeclaration *last_iter_decl = NULL;
  SgVariableDeclaration *stride_decl = NULL;

  if (SageInterface::is_Fortran_language()) { // special rules to insert
                                              // variable declarations in
                                              // Fortran
    // They have to be inserted to enclosing function body or enclosing parallel
    // region body and after existing declaration statement sequence, if any.
    nCounter++;
    index_decl = buildAndInsertDeclarationForOmp(
        "p_index_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    lower_decl = buildAndInsertDeclarationForOmp(
        "p_lower_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    upper_decl = buildAndInsertDeclarationForOmp(
        "p_upper_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    stride_decl = buildAndInsertDeclarationForOmp(
        "p_stride_" + StringUtility::numberToString(nCounter), loop_var_type,
        NULL, bb1);
    last_iter_decl = buildAndInsertDeclarationForOmp(
        "p_last_iter_" + StringUtility::numberToString(nCounter),
        buildKmpcInt32Type(), NULL, bb1);
  } else {
    index_decl = buildVariableDeclaration("__index_", loop_var_type, NULL, bb1);
    lower_decl = buildVariableDeclaration(
        "__lower_", loop_var_type,
        buildAssignInitializer(copyExpression(orig_lower), loop_var_type), bb1);
    upper_decl = buildVariableDeclaration(
        "__upper_", loop_var_type,
        buildAssignInitializer(copyExpression(runtime_upper_bound),
                               loop_var_type),
        bb1);
    stride_decl = buildVariableDeclaration(
        "__stride_", loop_var_type,
        buildAssignInitializer(
            buildKmpcSignedStride(orig_stride, isIncremental, false),
            loop_var_type),
        bb1);
    SgType *last_iter_type = buildIntType();
    last_iter_decl = buildVariableDeclaration(
        "__last_iter_", last_iter_type,
        buildAssignInitializer(buildIntVal(0), last_iter_type), bb1);

    appendStatement(index_decl, bb1);
    appendStatement(lower_decl, bb1);
    appendStatement(upper_decl, bb1);
    appendStatement(stride_decl, bb1);
    appendStatement(last_iter_decl, bb1);
  }

  if (SageInterface::is_Fortran_language()) {
    // LLVM runtime loop init expects input lower/upper/stride to be
    // initialized.
    appendStatement(buildAssignStatement(buildVarRefExp(lower_decl),
                                         copyExpression(orig_lower)),
                    bb1);
    appendStatement(buildAssignStatement(buildVarRefExp(upper_decl),
                                         copyExpression(runtime_upper_bound)),
                    bb1);
    appendStatement(buildAssignStatement(buildVarRefExp(stride_decl),
                                         buildKmpcSignedStride(
                                             orig_stride, isIncremental, true)),
                    bb1);
    appendStatement(
        buildAssignStatement(buildVarRefExp(last_iter_decl), buildIntVal(0)),
        bb1);
  }

  bool hasOrder = false;
  if (hasClause(target, V_SgOmpOrderedClause))
    hasOrder = true;

  // Grab or calculate chunk_size
  //    SgExpression* my_chunk_size = NULL;
  bool hasSpecifiedSize = false;
  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(target, V_SgOmpScheduleClause);
  if (clauses.size() != 0) {
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    // SgOmpClause::omp_schedule_kind_enum s_kind = s_clause->get_kind();
    //  ROSE_ASSERT(s_kind == SgOmpClause::e_omp_schedule_static);
    SgExpression *orig_chunk_size = s_clause->get_chunk_size();
    //  ROSE_ASSERT(orig_chunk_size->get_parent() != NULL);
    if (orig_chunk_size) {
      hasSpecifiedSize = true;
      // my_chunk_size = orig_chunk_size;
    }
  }

  const bool use_64_runtime = use_kmpc_loop_64bit_runtime(
      getFirstVariable(*lower_decl).get_type(), bb1);

  //  step 3. Translation for omp for
  if (!useStaticSchedule(target) || hasOrder || hasSpecifiedSize) {
    transOmpLoop_others(target, loop, index_decl, lower_decl, upper_decl,
                        stride_decl, last_iter_decl, bb1, tid_context,
                        runtime_upper_bound, isIncremental, fortran_direction);
  } else {
    SgStatement *kmpc_global_tid_init = NULL;
    SgVariableDeclaration *kmpc_global_tid_declaration =
        get_kmpc_global_tid(tid_context, bb1, &kmpc_global_tid_init);
    SgExpression *thread_global_tid = buildVarRefExp(
        getFirstVariable(*kmpc_global_tid_declaration).get_name(), bb1);
    if (SageInterface::is_Fortran_language())
      insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                                bb1);
    else
      appendStatement(kmpc_global_tid_declaration, bb1);
    if (kmpc_global_tid_init != NULL)
      appendStatement(kmpc_global_tid_init, bb1);

    // void XOMP_loop_default(int lower, int upper, int stride, long *n_lower,
    // long * n_upper)
    //  XOMP_loop_default (lower, upper, stride, &_p_lower, &_p_upper );
    //  lower:  copyExpression(orig_lower)
    //  upper: copyExpression(orig_upper)
    //  stride: copyExpression(orig_stride)
    //  n_lower: buildVarRefExp(lower_decl)
    //  n_upper: buildVarRefExp(upper_decl)
    SgExpression *e4 = NULL;
    SgExpression *e5 = NULL;
    if (for_loop) {
      e4 = buildExactAddressOfOp(buildVarRefExp(lower_decl));
      e5 = buildExactAddressOfOp(buildVarRefExp(upper_decl));
    } else if (do_loop) { // Fortran, pass-by-reference by default
      e4 = buildVarRefExp(lower_decl);
      e5 = buildVarRefExp(upper_decl);
    }
    ROSE_ASSERT(e4 && e5);
    // by default, LLVM uses 34 as the scheduling policy enum
    SgExpression *schedule_type = buildIntVal(kmp_sched_static_nochunk);
    SgExpression *e_last_iter =
        buildExactAddressOfOp(buildVarRefExp(last_iter_decl));
    SgExpression *e_stride = buildExactAddressOfOp(buildVarRefExp(stride_decl));
    if (do_loop) {
      // Fortran call arguments are already passed by reference.
      e_last_iter = buildVarRefExp(last_iter_decl);
      e_stride = buildVarRefExp(stride_decl);
    }
    SgExpression *runtime_increment = do_loop != nullptr
                                          ? copyExpression(orig_stride)
                                          : buildVarRefExp(stride_decl);
    parameters = buildExprListExp(source_location_info, thread_global_tid,
                                  schedule_type, e_last_iter, e4, e5, e_stride,
                                  runtime_increment, buildIntVal(1));
    SgStatement *call_stmt = buildFunctionCallStmt(
        getKmpcRuntimeFunctionName(
            get_kmpc_for_static_init_name(use_64_runtime)),
        buildVoidType(), parameters, bb1);
    appendStatement(call_stmt, bb1);

    // insert the upper bound checking
    SgType *logical_result_type = exactLogicalResultType();
    SgExpression *if_condition = nullptr;
    if (do_loop != nullptr) {
      SgExpression *increasing =
          buildAndOp(buildGreaterThanOp(copyExpression(orig_stride),
                                        buildIntVal(0), logical_result_type),
                     buildGreaterThanOp(buildVarRefExp(upper_decl),
                                        copyExpression(runtime_upper_bound),
                                        logical_result_type),
                     logical_result_type);
      SgExpression *decreasing =
          buildAndOp(buildLessThanOp(copyExpression(orig_stride),
                                     buildIntVal(0), logical_result_type),
                     buildLessThanOp(buildVarRefExp(upper_decl),
                                     copyExpression(runtime_upper_bound),
                                     logical_result_type),
                     logical_result_type);
      if_condition = buildOrOp(increasing, decreasing, logical_result_type);
    } else {
      if_condition =
          isIncremental
              ? static_cast<SgExpression *>(buildGreaterThanOp(
                    buildVarRefExp(upper_decl),
                    copyExpression(runtime_upper_bound), logical_result_type))
              : static_cast<SgExpression *>(buildLessThanOp(
                    buildVarRefExp(upper_decl),
                    copyExpression(runtime_upper_bound), logical_result_type));
    }
    SgExprStatement *update_upper_bound_stmt = buildAssignStatement(
        buildVarRefExp(upper_decl), copyExpression(runtime_upper_bound));
    SgStatement *if_true_body = update_upper_bound_stmt;
    if (SageInterface::is_Fortran_language()) {
      // Assemble the generated block as one detached subtree. It acquires its
      // exact physical output owner only when the enclosing if statement is
      // appended to the already attached lowering block.
      if_true_body = buildBasicBlock(update_upper_bound_stmt);
    }
    SgIfStmt *if_statement = buildIfStmt(if_condition, if_true_body, NULL);
    appendStatement(if_statement, bb1);

    // add loop here
    SgStatement *new_loop = deepCopy(loop);
    appendStatement(new_loop, bb1);
    // replace loop index with the new one
    replaceVariableReferences(
        new_loop,
        isSgVariableSymbol(orig_index->get_symbol_from_symbol_table()),
        getFirstVarSym(index_decl));
    // rewrite the lower and upper bounds
    SageInterface::setLoopLowerBound(new_loop, buildVarRefExp(lower_decl));
    if (SgForStatement *new_for_loop = isSgForStatement(new_loop))
      SageInterface::setCanonicalForLoopInclusiveComparison(new_for_loop,
                                                            isIncremental);
    SageInterface::setLoopUpperBound(new_loop, buildVarRefExp(upper_decl));

    transOmpVariables(target, bb1, runtime_upper_bound);
    SgExprListExp *fini_parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    appendStatement(buildFunctionCallStmt(
                        getKmpcRuntimeFunctionName("__kmpc_for_static_fini"),
                        buildVoidType(), fini_parameters, bb1),
                    bb1);
    // insert barrier if there is no nowait clause
    if (!hasClause(target, V_SgOmpNowaitClause)) {
      SgExprListExp *barrier_parameters =
          buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
      appendStatement(
          buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_barrier"),
                                buildVoidType(), barrier_parameters, bb1),
          bb1);
    }
  }

  SageInterface::deleteAST(runtime_upper_bound,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  if (SgNode::isLiveNode(runtime_upper_bound)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-loop-upper]: detached "
            "template=%p remained live after lowering\n",
            static_cast<void *>(runtime_upper_bound));
    ROSE_ABORT();
  }

  retireConsumedOmpDirective(target);

} // end trans omp for

//! Translate omp for or omp do loops affected by the "omp target" directive,
//! Liao 1/28/2013
/*

Example:
// for (i = 0; i < N; i++)
{ // top level block, prepare to be outlined.
// int i ; // = blockDim.x * blockIdx.x + threadIdx.x; // this CUDA declaration
can be inserted later i = getLoopIndexFromCUDAVariables(1);

if (i<SIZE)  // boundary checking to avoid invalid memory accesses
{
for (j = 0; j < M; j++)
for (k = 0; k < K; k++)
c[i][j]= c[i][j]+a[i][k]*b[k][j];
}
} // end of top level block

Algorithm:
 * check if it is a OmpTargetLoop
 * loop normalization
 * replace OmpForStatement with a block: bb1
 * declare int _dev_i within bb1;  replace for loop body’s loop index with
_dev_i;
 * build if stmt with correct condition
 * move loop body to if-stmt’s true body
 * remove for_loop
 */
void transOmpTargetLoop(SgNode *node) {
  // step 0: Sanity check
  ROSE_ASSERT(node != NULL);
  SgOmpForStatement *target1 = isSgOmpForStatement(node);
  SgOmpDoStatement *target2 = isSgOmpDoStatement(node);

  SgOmpClauseBodyStatement *target =
      (target1 != NULL ? (SgOmpClauseBodyStatement *)target1
                       : (SgOmpClauseBodyStatement *)target2);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  SgStatement *loop = requireExactAssociatedLoop(
      target, body, AssociatedLoopPathContract::Target, "target-loop");
  SgForStatement *for_loop = isSgForStatement(loop);
  SgFortranDo *do_loop = isSgFortranDo(loop);

  // make sure this is really a loop affected by "omp target"
  // bool is_target_loop = false;
  SgNode *parent = node->get_parent();
  ROSE_ASSERT(parent != NULL);
  if (isSgBasicBlock(
          parent)) // skip one possible BB between omp parallel and omp for.
    parent = parent->get_parent();
  SgNode *grand_parent = parent->get_parent();
  ROSE_ASSERT(grand_parent != NULL);
  SgOmpParallelStatement *parent_parallel = isSgOmpParallelStatement(parent);
  SgOmpTargetStatement *grand_target = isSgOmpTargetStatement(grand_parent);
  ROSE_ASSERT(parent_parallel != NULL);
  ROSE_ASSERT(grand_target != NULL);

  // Step 1. Loop normalization
  // For the init statement: for (int i=0;... ) becomes int i; for (i=0;..)
  // For test expression: i<x is normalized to i<= (x-1) and i>x is normalized
  // to i>= (x+1) For increment expression: i++ is normalized to i+=1 and i-- is
  // normalized to i+=-1 i-=s is normalized to i+= -s
  if (for_loop) {
    SageInterface::forLoopNormalization(for_loop);
  } else if (do_loop) {
    SageInterface::doLoopNormalization(do_loop);
  }

  SgInitializedName *orig_index = NULL;
  SgExpression *orig_lower = NULL;
  SgExpression *orig_upper = NULL;
  SgExpression *orig_stride = NULL;
  bool isIncremental = true; // if the loop iteration space is incremental
  // grab the original loop 's controlling information
  bool is_canonical = false;

  if (for_loop)
    is_canonical =
        isCanonicalForLoop(for_loop, &orig_index, &orig_lower, &orig_upper,
                           &orig_stride, NULL, &isIncremental);
  else if (do_loop)
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, nullptr, NULL);
  if (!is_canonical) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop]: loop=%p kind=%s is not "
            "exactly canonical after normalization\n",
            static_cast<void *>(loop), loop->sage_class_name());
    ROSE_ABORT();
  }

  // also make sure the loop body is a block
  // TODO: we consider peeling off 1 level loop control only, need to be
  // conditional on what the spec. can provide at pragma level
  // TODO: Fortran support later on
  ROSE_ASSERT(for_loop != NULL);
  SgBasicBlock *loop_body = ensureBasicBlockAsBodyOfFor(for_loop);

  // Step 2. Insert a basic block to replace SgOmpForStatement
  // This newly introduced scope is used to hold loop variables ,etc
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();
  replaceStatement(target, bb1, true);

  // Step 3. Using device thread id and replace reference of original loop index
  // with the thread index
  //  Declare device thread id variable
  // int i = blockDim.x * blockIdx.x + threadIdx.x;
  // SgAssignInitializer* init_idx =  buildAssignInitializer(
  //                                      buildAddOp( buildMultiplyOp
  //                                      (buildVarRefExp("blockDim.x"),
  //                                      buildVarRefExp("blockIdx.x")) ,
  //                                       buildVarRefExp("threadIdx.x", bb1)));
  // Better build of CUDA variables within a runtime library call so these
  // variables are hidden from the translation
  //   getLoopIndexFromCUDAVariables(1)
  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getLoopIndexFromCUDAVariables"),
                           buildIntType(), buildExprListExp(buildIntVal(1)),
                           bb1),
      buildIntType());

  SgVariableDeclaration *dev_i_decl =
      buildVariableDeclaration("_dev_i", buildIntType(), init_idx, bb1);
  prependStatement(dev_i_decl, bb1);
  SgVariableSymbol *dev_i_symbol = getFirstVarSym(dev_i_decl);
  ROSE_ASSERT(dev_i_symbol != NULL);

  // replace reference to loop index with reference to device i variable
  ROSE_ASSERT(orig_index != NULL);
  SgSymbol *orig_symbol = orig_index->get_symbol_from_symbol_table();
  ROSE_ASSERT(orig_symbol != NULL);

  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(loop_body, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    if (vRef->get_symbol() == orig_symbol)
      rebindTransformedVariableReference(vRef, vRef->get_symbol(), dev_i_symbol,
                                         "target-loop-device-index");
  }

  // Step 4. build the if () condition statement, move the loop body into the
  // true body Liao, 2/21/2013. We must be accurate about the range of
  // iterations or the computation may result in WRONG results!! A classic
  // example is the Jacobi iteration: in which the first and last iterations are
  // not executed to make sure elements have boundaries. After normalization, we
  // have inclusive lower and upper bounds of the input loop the condition of
  // if() should look like something: if (_dev_i >=0+1 &&_dev_i <= (n - 1) - 1)
  // {...}
  SgBasicBlock *true_body = buildBasicBlock();
  SgExprStatement *cond_stmt = NULL;
  if (isIncremental) {
    SgType *logical_result_type = exactLogicalResultType();
    SgExpression *lhs =
        buildGreaterOrEqualOp(buildVarRefExp(dev_i_symbol),
                              deepCopy(orig_lower), logical_result_type);
    SgExpression *rhs =
        buildLessOrEqualOp(buildVarRefExp(dev_i_symbol), deepCopy(orig_upper),
                           logical_result_type);
    cond_stmt = buildExprStatement(buildAndOp(lhs, rhs, logical_result_type));
  } else {
    cerr << "error. transOmpTargetLoop(): decremental case is not yet handled !"
         << endl;
    ROSE_ABORT();
  }
  SgIfStmt *if_stmt = buildIfStmt(cond_stmt, true_body, NULL);
  appendStatement(if_stmt, bb1);
  moveStatementsBetweenBlocks(loop_body, true_body);
  // Peel off the original loop
  removeStatement(for_loop);

  // handle private variables at this loop level, mostly loop index variables.
  // TODO: this is not very elegant since the outer most loop's loop variable is
  // still translated.
  GpuOffloadLoweringContext offload_ctx;
  transOmpVariablesWithContext(target, bb1, NULL, true, &offload_ctx);
}

//! Translate omp for or omp do loops affected by the "omp target" directive,
//! using a round robin-scheduler Liao 7/10/2014
/*  Algorithm

// original loop info. grab from the loop structure
int orig_start =0;
int orig_end = n-1; // inclusive upper bound
int orig_step = 1;
int orig_chunk_size = 1;// fixed at 1

// new lower and upper bound, to be filled out by the loop scheduler
int _dev_lower;
int _dev_upper;
int _dev_loop_chunk_size;
int _dev_loop_sched_index;
int _dev_loop_stride;

// CUDA thread count and ID for the 1-D block
int _dev_thread_num = getCUDABlockThreadCount(1);
int _dev_thread_id = getLoopIndexFromCUDAVariables(1);

//initialize scheduler
XOMP_static_sched_init (orig_start, orig_end, orig_step, orig_chunk_size,
_dev_thread_num, _dev_thread_id, \ & _dev_loop_chunk_size , &
_dev_loop_sched_index, & _dev_loop_stride);

while (XOMP_static_sched_next (&_dev_loop_sched_index, orig_end,
orig_step,_dev_loop_stride, _dev_loop_chunk_size, _dev_thread_num,
_dev_thread_id, & _dev_lower , & _dev_upper))
{
for (i= _dev_lower ; i <= _dev_upper; i ++ ) { // rewrite lower and upper bound
and step normalized to 1
// original loop body here
}
}
}

*/
void transOmpTargetLoop_RoundRobin(SgNode *node) {
  // step 0: Sanity check
  ROSE_ASSERT(node != NULL);
  SgOmpForStatement *target1 = isSgOmpForStatement(node);
  SgOmpDoStatement *target2 = isSgOmpDoStatement(node);

  // the target of the translation is a SgOmpForStatement
  SgOmpClauseBodyStatement *target =
      (target1 != NULL ? (SgOmpClauseBodyStatement *)target1
                       : (SgOmpClauseBodyStatement *)target2);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  SgStatement *loop = requireExactAssociatedLoop(
      target, body, AssociatedLoopPathContract::Target,
      "target-loop-round-robin");
  SgForStatement *for_loop = isSgForStatement(loop);
  SgFortranDo *do_loop = isSgFortranDo(loop);

  // make sure this is really a loop affected by "omp target"
  // bool is_target_loop = false;
  SgNode *parent = node->get_parent();
  ROSE_ASSERT(parent != NULL);
  if (isSgBasicBlock(
          parent)) // skip one possible BB between omp parallel and omp for.
    parent = parent->get_parent();
  SgNode *grand_parent = parent->get_parent();
  if (isSgBasicBlock(grand_parent)) // skip one possible BB between omp target
                                    // and omp parallel.
    grand_parent = grand_parent->get_parent();
  ROSE_ASSERT(grand_parent != NULL);
  SgOmpParallelStatement *parent_parallel = isSgOmpParallelStatement(parent);
  SgOmpTargetStatement *grand_target = isSgOmpTargetStatement(grand_parent);
  ROSE_ASSERT(parent_parallel != NULL);
  ROSE_ASSERT(grand_target != NULL);

  // Step 1. Loop normalization
  // For the init statement: for (int i=0;... ) becomes int i; for (i=0;..)
  // For test expression: i<x is normalized to i<= (x-1) and i>x is normalized
  // to i>= (x+1) For increment expression: i++ is normalized to i+=1 and i-- is
  // normalized to i+=-1 i-=s is normalized to i+= -s
  if (for_loop) {
    SageInterface::forLoopNormalization(for_loop);
  } else if (do_loop) {
    SageInterface::doLoopNormalization(do_loop);
  }

  SgInitializedName *orig_index = NULL;
  SgExpression *orig_lower = NULL;
  SgExpression *orig_upper = NULL;
  SgExpression *orig_stride = NULL;
  bool isIncremental = true; // if the loop iteration space is incremental
  // grab the original loop 's controlling information
  bool is_canonical = false;

  if (for_loop)
    is_canonical =
        isCanonicalForLoop(for_loop, &orig_index, &orig_lower, &orig_upper,
                           &orig_stride, NULL, &isIncremental);
  else if (do_loop)
    is_canonical =
        isCanonicalDoLoop(do_loop, &orig_index, &orig_lower, &orig_upper,
                          &orig_stride, NULL, nullptr, NULL);
  if (!is_canonical) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop-round-robin]: loop=%p "
            "kind=%s is not exactly canonical after normalization\n",
            static_cast<void *>(loop), loop->sage_class_name());
    ROSE_ABORT();
  }

  // TODO: Fortran support later on
  ROSE_ASSERT(for_loop != NULL);
  // SgBasicBlock* loop_body = ensureBasicBlockAsBodyOfFor (for_loop);

  // Step 2. Insert a basic block to replace SgOmpForStatement
  // This newly introduced scope is used to hold loop variables ,etc
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();
  replaceStatement(target, bb1, true);

  // Insert variables used by the two scheduler functions
  /* int _dev_lower;
     int _dev_upper;
     int _dev_loop_chunk_size;
     int _dev_loop_sched_index;
     int _dev_loop_stride;
  */
  SgVariableDeclaration *dev_lower_decl =
      buildVariableDeclaration("_dev_lower", buildIntType(), NULL, bb1);
  appendStatement(dev_lower_decl, bb1);
  SgVariableDeclaration *dev_upper_decl =
      buildVariableDeclaration("_dev_upper", buildIntType(), NULL, bb1);
  appendStatement(dev_upper_decl, bb1);
  SgVariableDeclaration *dev_loop_chunk_size_decl = buildVariableDeclaration(
      "_dev_loop_chunk_size", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_chunk_size_decl, bb1);
  SgVariableDeclaration *dev_loop_sched_index_decl = buildVariableDeclaration(
      "_dev_loop_sched_index", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_sched_index_decl, bb1);
  SgVariableDeclaration *dev_loop_stride_decl =
      buildVariableDeclaration("_dev_loop_stride", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_stride_decl, bb1);

  // Insert CUDA thread id and count declarations
  // int _dev_thread_num = getCUDABlockThreadCount(1);
  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getCUDABlockThreadCount"), buildIntType(),
                           buildExprListExp(buildIntVal(1)), bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_num_decl = buildVariableDeclaration(
      "_dev_thread_num", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_num_decl, bb1);
  SgVariableSymbol *dev_thread_num_symbol = getFirstVarSym(dev_thread_num_decl);
  ROSE_ASSERT(dev_thread_num_symbol != NULL);

  // int _dev_thread_id = getLoopIndexFromCUDAVariables(1);
  init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getLoopIndexFromCUDAVariables"),
                           buildIntType(), buildExprListExp(buildIntVal(1)),
                           bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_id_decl =
      buildVariableDeclaration("_dev_thread_id", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_id_decl, bb1);
  SgVariableSymbol *dev_thread_id_symbol = getFirstVarSym(dev_thread_id_decl);
  ROSE_ASSERT(dev_thread_id_symbol != NULL);

  // initialize scheduler
  // XOMP_static_sched_init (orig_start, orig_end, orig_step, orig_chunk_size,
  // _dev_thread_num, _dev_thread_id,
  //                       & _dev_loop_chunk_size , & _dev_loop_sched_index, &
  //                       _dev_loop_stride);
  SgExprListExp *parameters =
      buildExprListExp(copyExpression(orig_lower), copyExpression(orig_upper),
                       copyExpression(orig_stride), buildIntVal(1),
                       buildVarRefExp(dev_thread_num_symbol),
                       buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_chunk_size_decl))));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_sched_index_decl))));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_stride_decl))));
  SgStatement *call_stmt = buildFunctionCallStmt(
      "XOMP_static_sched_init", buildVoidType(), parameters, bb1);
  appendStatement(call_stmt, bb1);

  // function call exp as while (condition)
  // XOMP_static_sched_next (&_dev_loop_sched_index, orig_end,
  // orig_step,_dev_loop_stride, _dev_loop_chunk_size,
  //                       _dev_thread_num, _dev_thread_id, & _dev_lower , &
  //                       _dev_upper)
  parameters = buildExprListExp(
      buildExactAddressOfOp(
          buildVarRefExp(getFirstVarSym(dev_loop_sched_index_decl))),
      copyExpression(orig_upper), copyExpression(orig_stride),
      buildVarRefExp(getFirstVarSym(dev_loop_stride_decl)),
      buildVarRefExp(getFirstVarSym(dev_loop_chunk_size_decl)));
  appendExpression(parameters, buildVarRefExp(dev_thread_num_symbol));
  appendExpression(parameters, buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_lower_decl))));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_upper_decl))));
  SgExpression *func_call_exp = buildFunctionCallExp(
      "XOMP_static_sched_next", buildBoolType(), parameters, bb1);

  SgStatement *new_loop = deepCopy(for_loop);
  SgWhileStmt *w_stmt = buildWhileStmt(func_call_exp, new_loop);
  appendStatement(w_stmt, bb1);
  //  moveStatementsBetweenBlocks (loop_body,
  //  isSgBasicBlock(w_stmt->get_body()));

  // rewrite upper, lower bounds, TODO how about step? normalized to 1 already ?
  setLoopLowerBound(new_loop, buildVarRefExp(getFirstVarSym(dev_lower_decl)));
  setLoopUpperBound(new_loop, buildVarRefExp(getFirstVarSym(dev_upper_decl)));
  removeStatement(for_loop);

  // handle private variables at this loop level, mostly loop index variables.
  // TODO: this is not very elegant since the outer most loop's loop variable is
  // still translated.
  GpuOffloadLoweringContext offload_ctx;
  transOmpVariablesWithContext(target, bb1, NULL, true, &offload_ctx);
}

//! Check if an OpenMP statement has a clause of type vvt
Rose_STL_Container<SgOmpClause *> getClause(SgStatement *clause_stmt,
                                            const VariantVector &vvt) {
  ROSE_ASSERT(clause_stmt != NULL);
  SgOmpClausePtrList clauses;
  if (isSgOmpClauseBodyStatement(clause_stmt)) {
    clauses = (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses();
  } else if (isSgOmpClauseStatement(clause_stmt)) {
    clauses = (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  } else {
    ROSE_ABORT();
  };
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clauses, vvt);
  return p_clause;
}

//! Check if an OpenMP statement has a clause of type vt
Rose_STL_Container<SgOmpClause *> getClause(SgStatement *clause_stmt,
                                            const VariantT &vt) {
  return getClause(clause_stmt, VariantVector(vt));
}

//! Check if an OpenMP statement has a clause of type vt
bool hasClause(SgStatement *clause_stmt, const VariantT &vt) {
  ROSE_ASSERT(clause_stmt != NULL);
  SgOmpClausePtrList clauses;
  if (isSgOmpClauseBodyStatement(clause_stmt)) {
    clauses = (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses();
  } else if (isSgOmpClauseStatement(clause_stmt)) {
    clauses = (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  } else {
    ROSE_ABORT();
  };
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clauses, vt);
  return (p_clause.size() != 0);
}

//! A helper function to generate implicit or explicit task for either omp
//! parallel or omp task
//  Parameters:  SgNode* node: the OMP Parallel or OMP Parallel
//               std::string& wrapper_name: for C/C++, structure wrapper is used
//               to wrap all parameters. This is to return the struct name
//               ASTtools::VarSymSet_t& syms :  all variables to be passed
//               in/out the outlined function ASTtools::VarSymSet_t&pdSyms3 :
//               variables which must be passed by references, used to guide the
//               creation of struct wrapper: member using base type vs. using
//               pointer type.  The algorithm to generate this set is already
//               very conservative: after transOmpVariables() , the only exclude
//               firstprivate. In the context of OpenMP, it is equivalent to say
//               this is a set of variables which are to be passed by
//               references.
// Algorithms:
//    Set flags of the outliner to indicate desired behaviors: parameter
//    wrapping or not? translate OpenMP variables (first private, private,
//    reduction, etc) so the code to be outlined is already as simple as
//    possible (without OpenMP-specific semantics)
//
// It calls the ROSE AST outliner internally.
SgFunctionDeclaration *generateOutlinedTask(SgNode *node,
                                            std::string &wrapper_name,
                                            ASTtools::VarSymSet_t &syms,
                                            ASTtools::VarSymSet_t &pdSyms3,
                                            bool use_task_param,
                                            bool insert_runtime_ids) {
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);

  // must be either omp task or omp parallel
  SgOmpTaskStatement *target1 = isSgOmpTaskStatement(node);
  SgOmpParallelStatement *target2 = isSgOmpParallelStatement(node);
  ROSE_ASSERT(target1 != NULL || target2 != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // Outliner::preprocess() only accepts a subset of statement kinds.  Parallel
  // and task bodies can legally be a bare OpenMP directive (e.g., omp single),
  // so normalize to a basic block first and lower nested directives later.
  if (isSgBasicBlock(body) == NULL) {
    SgOmpBodyStatement *body_stmt = isSgOmpBodyStatement(target);
    ROSE_ASSERT(body_stmt != NULL);
    body = ensureBasicBlockAsBodyOfOmpBodyStmt(body_stmt);
  }
  SgFunctionDeclaration *result = NULL;
  // Initialize outliner
  Outliner::enable_classic = false; // we need use parameter wrapping, which is
                                    // not classic behavior of outlining
  // We pass one variable per parameter, at least for Fortran 77.
  // For both C/C++ and Fortran, we use the same method to pass parameters
  // separately instead of a struct or array wrapper.
  Outliner::useParameterWrapper = false;

  // TODO there should be some semantics check for the regions to be outlined
  // for example, multiple entries or exists are not allowed for OpenMP
  // This is however of low priority since most vendor compilers have this
  // already.
  SgBasicBlock *body_block = Outliner::preprocess(body);

  //---------------------------------------------------------------
  //  Key step: handling special variables BEFORE actual outlining is done!!
  // Variable handling is done after Outliner::preprocess() to ensure a basic
  // block for the body, but before calling the actual outlining This simplifies
  // the outlining since firstprivate, private variables are replaced
  // with their local copies before outliner is used
  transOmpVariables(target, body_block);

  // Normalize symbol links introduced by clause-variable rewrites before
  // collecting outlined captures.
  SageInterface::rebindVariableReferencesAfterMove(body_block);

  // variable sets for private, firstprivate, reduction, and pointer
  // dereferencing (pd)
  ASTtools::VarSymSet_t pSyms, fpSyms, reductionSyms, pdSyms;

  string func_name = Outliner::generateFuncName(target);

  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  //-----------------------------------------------------------------
  // Generic collection of variables to be passed as parameters of the outlined
  // functions semantically equivalent to shared variables in OpenMP
  Outliner::collectVars(body_block, syms);

  // Now decide on the parameter convention for all the parameters:
  // pass-by-value vs. pass-by-reference (pointer dereferencing)

  //     SageInterface::collectReadOnlyVariables(body_block,readOnlyVars);
  // We choose to be conservative about the variables needing pointer
  // dereferencing first AllParameters - readOnlyVars  - private -firstprivate
  // Union ASTtools::collectPointerDereferencingVarSyms(body_block, pdSyms)

  // Assume all parameters need to be passed by reference/pointers first
  std::copy(syms.begin(), syms.end(), std::inserter(pdSyms, pdSyms.begin()));

  // exclude firstprivate variables: they are read only in fact
  // TODO keep class typed variables!!!  even if they are firstprivate or
  // private!!
  SgInitializedNamePtrList fp_vars =
      collectClauseVariables(target, V_SgOmpFirstprivateClause);
  ASTtools::VarSymSet_t fp_syms, pdSyms2;
  convertAndFilter(fp_vars, fp_syms);
  set_difference(pdSyms.begin(), pdSyms.end(), fp_syms.begin(), fp_syms.end(),
                 std::inserter(pdSyms2, pdSyms2.begin()),
                 ASTtools::VarSymLess());
  //  ROSE_ASSERT (pdSyms.size() == pdSyms2.size());  this means the previous
  //  set_difference is neccesary !

  pdSyms3 = pdSyms2;

  // lastprivate and reduction variables cannot be excluded  since write access
  // to their shared copies

  // Sara Royuela 24/04/2012
  // When unpacking array variables in the outlined function, it is needed to
  // have access to the size of the array. When this size is a variable (or a
  // operation containing variables), this variable must be added to the
  // arguments of the outlined function. Example:
  //    Input snippet:                      Outlined function:
  //        int N = 1;                          static void OUT__1__5493__(void
  //        *__out_argv) { int a[N];                               int (*a)[N] =
  //        (int (*)[N])(((struct OUT__1__5493___data *)__out_argv) -> a_p);
  //        #pragma omp task shared(a)              ( *a)[0] = 1;
  //            a[0] = 1;                       }
  ASTtools::VarSymSet_t new_syms;
  for (ASTtools::VarSymSet_t::const_iterator i = syms.begin(); i != syms.end();
       ++i) {
    SgType *i_type = (*i)->get_declaration()->get_type();

    while (isSgArrayType(i_type)) {
      // Get most significant dimension
      SgExpression *index = ((SgArrayType *)i_type)->get_index();

      // Get the variables used to compute the dimension
      // FIXME We insert a new statement and delete it afterwards in order to
      // use "collectVars" function
      //       Think about implementing an specific function for expressions
      ASTtools::VarSymSet_t a_syms, a_pSyms;
      SgExprStatement *index_stmt = buildExprStatement(index);
      appendStatement(index_stmt, body_block);
      Outliner::collectVars(index_stmt, a_syms);
      SageInterface::removeStatement(index_stmt);
      for (ASTtools::VarSymSet_t::iterator j = a_syms.begin();
           j != a_syms.end(); ++j) {
        const SgVariableSymbol *s = *j;
        new_syms.insert(
            s); // If the symbol is not in the symbol list, it is added
      }

      // Advance over the type
      i_type = ((SgArrayType *)i_type)->get_base_type();
    }
  }

  for (ASTtools::VarSymSet_t::const_iterator i = new_syms.begin();
       i != new_syms.end(); ++i) {
    const SgVariableSymbol *s = *i;
    syms.insert(s);
  }

  // a data structure used to wrap parameters
  SgClassDeclaration *struct_decl = NULL;

  // Generate the outlined function
  /* Parameter list
       SgBasicBlock* s,  // block to be outlined
       const string& func_name_str, // function name
       const ASTtools::VarSymSet_t& syms, // parameter list for all variables to
    be passed around const ASTtools::VarSymSet_t& pdSyms, // variables must use
    pointer dereferencing (pass-by-reference) const ASTtools::VarSymSet_t&
    psyms, // private or dead variables (not live-in, not live-out)
       SgClassDeclaration* struct_decl,  // an optional wrapper structure for
    parameters Depending on the internal flag, unpacking/unwrapping statements
    are generated inside the outlined function to use wrapper parameters.
  */
  std::set<SgInitializedName *> restoreVars;
  Outliner::OutlinedLocalTypeTemplatePlan local_type_template_plan;
  result = Outliner::generateFunction(body_block, func_name, syms, pdSyms3,
                                      restoreVars, struct_decl, g_scope,
                                      local_type_template_plan);
  if (!local_type_template_plan.entries.empty()) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[outlined-local-type-call]: runtime outlined "
            "function requires explicit local type template arguments\n");
    ROSE_ABORT();
  }

  if (insert_runtime_ids) {
    SgPointerType *int_pointer_type = buildPointerType(SgTypeInt::createType());
    SgType *thread_id_type = SageInterface::is_Fortran_language()
                                 ? buildKmpcInt32Type()
                                 : static_cast<SgType *>(int_pointer_type);
    // insert the kmpc ids as the first two parameters
    if (use_task_param) {
      auto *taskType = requireNamedTypeInParentScopes("ptask", g_scope);
      insert_function_parameter("task", taskType, result, false);
    } else {
      insert_function_parameter(SageInterface::is_Fortran_language()
                                    ? "rex_bound_tid"
                                    : "__bound_tid",
                                thread_id_type, result, false);
    }

    insert_function_parameter(SageInterface::is_Fortran_language()
                                  ? "rex_global_tid"
                                  : "__global_tid",
                              thread_id_type, result, false);
  }

  // insert the forward declaration
  SgFunctionDeclaration *source_call_declaration = nullptr;
  Outliner::insert(result, g_scope, body_block, source_call_declaration);
  ROSE_ASSERT(source_call_declaration != nullptr);

  // Host C outlining moves the generated definition to a synthesized source
  // file immediately in transOmpParallel.  Record the lexical source function
  // at the producer while both declarations still have their exact common
  // translation-unit owner.  A nested outline produced inside an already
  // recorded target function is itself executable device output and must join
  // the same target move transaction; leaving only a source-file prototype
  // produces an unresolved device call after the enclosing kernel is copied.
  const bool nested_target_outline =
      enable_accelerator && target_outlined_function_list != NULL &&
      std::find(target_outlined_function_list->begin(),
                target_outlined_function_list->end(),
                enclosing_function) != target_outlined_function_list->end();
  if (nested_target_outline) {
    recordTargetOutlinedFunction(result, enclosing_function);
  } else if (!enable_accelerator && SageInterface::is_C_language()) {
    recordOutlinedSourceFunctionAnchor(result, enclosing_function);
  }

  // Generate packing statements
  // must pass target , not body_block to get the right scope in which the
  // declarations are inserted
  if (!SageInterface::is_Fortran_language())
    wrapper_name =
        Outliner::generatePackingStatements(target, syms, pdSyms3, struct_decl);
  ROSE_ASSERT(result != NULL);

  // 12/7/2010
  // For Fortran outlined subroutines,
  // add INCLUDE 'omp_lib.h' in case OpenMP runtime routines are called within
  // the outlined subroutines
  if (SageInterface::is_Fortran_language()) {
    SgBasicBlock *body = result->get_definition()->get_body();
    ROSE_ASSERT(body != NULL);
    normalize_fortran_external_subroutine_declarations(body);
    materialize_fortran_outlined_function_result_declarations(body);
    rebind_fortran_outlined_function_references(body);
    SgFortranIncludeLine *inc_line = buildFortranIncludeLine("omp_lib.h");
    prependStatement(inc_line, body);
    markStatementSubtreeForOutput(result);
  }
  return result;
}

/* GCC's libomp uses the following translation method:
 *
 *
#include "libgomp_g.h"


#include <omp.h>

#include <stdio.h>

//void main_omp_fn_0 (struct _omp_data_s_0* _omp_data_i);
void main_omp_fn_0 (void ** __out_argv);

int main (void)
{
int i;
//  struct _omp_data_s_0 _omp_data_o_1;

i = 0;
// wrap shared variables
//  _omp_data_o_1.i = i;
void *__out_argv1__5876__[1];
__out_argv1__5876__[0] = ((void *)(&i));

//GOMP_parallel_start (main_omp_fn_0, &_omp_data_o_1, 0);
GOMP_parallel_start (main_omp_fn_0, &__out_argv1__5876__, 0); // must use &
here!!!
//main_omp_fn_0 (&_omp_data_o_1);
//main_omp_fn_0 ((void *)__out_argv1__5876__); //best type match
main_omp_fn_0 (__out_argv1__5876__);
GOMP_parallel_end ();

// grab the changed value
//  i = _omp_data_o_1.i;
return 0;
}

//void main_omp_fn_0(void *__out_argvp)
void main_omp_fn_0(void **__out_argv)
//void OUT__1__5876__(void **__out_argv)
{
// void **__out_argv = (void **) __out_argvp;
int *i = (int *)(__out_argv[0]);
 *i = omp_get_thread_num();
  // No stdout noise from lowering.
 }
 */

void transOmpParallel(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpParallelStatement *target = isSgOmpParallelStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *cached_if_condition = NULL;
  if (hasClause(target, V_SgOmpIfClause)) {
    Rose_STL_Container<SgOmpClause *> if_clauses =
        getClause(target, V_SgOmpIfClause);
    ROSE_ASSERT(if_clauses.size() == 1);
    SgOmpIfClause *if_clause = isSgOmpIfClause(if_clauses[0]);
    ROSE_ASSERT(if_clause != NULL);
    ROSE_ASSERT(if_clause->get_expression() != NULL);
    cached_if_condition = copyExpression(if_clause->get_expression());
  }

  SgExpression *cached_num_threads = NULL;
  if (hasClause(target, V_SgOmpNumThreadsClause)) {
    Rose_STL_Container<SgOmpClause *> num_threads_clauses =
        getClause(target, V_SgOmpNumThreadsClause);
    ROSE_ASSERT(num_threads_clauses.size() == 1);
    SgOmpNumThreadsClause *num_threads_clause =
        isSgOmpNumThreadsClause(num_threads_clauses[0]);
    ROSE_ASSERT(num_threads_clause != NULL);
    ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
    cached_num_threads = copyExpression(num_threads_clause->get_expression());
  }

  // Liao 12/7/2010
  // For Fortran code, we have to insert EXTERNAL OUTLINED_FUNC into
  // the function body containing the parallel region
  SgFunctionDefinition *func_def = NULL;
  SgAttributeSpecificationStatement *outlined_external_declaration = NULL;
  if (SageInterface::is_Fortran_language()) {
    func_def = getEnclosingFunctionDefinition(target);
    ROSE_ASSERT(func_def != NULL);
  }
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // The AST retains only the active branch after preprocessing. Carrying
  // conditional directives through outlining can therefore split unmatched
  // #if/#endif fragments across host and outlined functions.
  stripConditionalDirectivesFromSubtree(body);
  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner
  AttachedPreprocessingInfoType save_buf1, save_buf2;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);
  stripConditionalDirectivesFromList(save_buf1);
  stripConditionalDirectivesFromList(save_buf2);

  // some #endif may be attached to the body, we should not move it with the
  // body into the outlined funcion!! cutPreprocessingInfo(body,
  // PreprocessingInfo::before, save_buf_body) ;

  //-----------------------------------------------------------------
  // step 1: generated an outlined function as the task
  std::string wrapper_name;
  ASTtools::VarSymSet_t syms; // store all variables in the outlined task ???
  ASTtools::VarSymSet_t
      pdSyms3; // store all variables which should be passed by references (pd
               // means pointer dereferencing)
  std::set<SgInitializedName *>
      readOnlyVars; // not used since OpenMP provides all variable controlling
                    // details already. side effect analysis is essentially not
                    // being used.
  SgFunctionDeclaration *outlined_func =
      generateOutlinedTask(node, wrapper_name, syms, pdSyms3);

  if (SageInterface::is_Fortran_language()) { // EXTERNAL outlined_function ,
                                              // otherwise the function name
                                              // will be interpreted as a
                                              // integer/real variable
    ROSE_ASSERT(func_def != NULL);
    SgBasicBlock *func_body = func_def->get_body();
    ROSE_ASSERT(func_body != NULL);
    SgAttributeSpecificationStatement *external_stmt1 =
        buildAttributeSpecificationStatement(
            SgAttributeSpecificationStatement::e_externalStatement);
    outlined_external_declaration = external_stmt1;
    SgFunctionRefExp *func_ref1 =
        buildFortranOutlinedFunctionRef(outlined_func);
    external_stmt1->get_parameter_list()->prepend_expression(func_ref1);
    func_ref1->set_parent(external_stmt1->get_parameter_list());
    // Publish through the typed Fortran specification-part transaction so an
    // INCLUDE-only host still places EXTERNAL after its source INCLUDE anchor
    // and before executable statements.
    insert_fortran_statement_in_specification_part(external_stmt1, func_body);
  }

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  //-----------------------------------------------------------------
  // step 2: generate call to the outlined function

  // Generate the parameter list for the call to the XOMP runtime function
  SgExprListExp *parameters = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration = NULL;
  SgExpression *thread_global_tid = NULL;

  // add __kmpc_fork_call (0, 2, OUT_func_xxx, &a, &sum);
  // or __kmpc_fork_call (0, 0, OUT_func_xxx); // if no variables need to be
  // passed
  SgExpression *source_location_info = buildIntVal(0);
  SgExpression *outlined_function_parameter_amount = buildIntVal(syms.size());
  SgExpression *outlined_function_argument =
      SageInterface::is_Fortran_language()
          ? static_cast<SgExpression *>(
                buildFortranOutlinedFunctionRef(outlined_func))
          : static_cast<SgExpression *>(buildFunctionRefExp(outlined_func));
  if (!SageInterface::is_Fortran_language()) {
    SgFunctionParameterTypeList *micro_parameters =
        new SgFunctionParameterTypeList();
    micro_parameters->append_argument(buildPointerType(buildIntType()));
    micro_parameters->append_argument(buildPointerType(buildIntType()));
    micro_parameters->append_argument(SgTypeEllipse::createType());
    SgType *micro_function_type =
        buildFunctionType(buildVoidType(), micro_parameters);
    outlined_function_argument = buildCastExp(
        outlined_function_argument, buildPointerType(micro_function_type),
        SgCastExp::e_C_style_cast);
  }
  parameters =
      buildExprListExp(source_location_info, outlined_function_parameter_amount,
                       outlined_function_argument);
  ASTtools::VarSymSet_t::iterator iter;
  for (iter = syms.begin(); iter != syms.end(); iter++) {
    const SgVariableSymbol *sb = *iter;
    SgExpression *actual_arg = NULL;
    if (SageInterface::is_Fortran_language()) {
      actual_arg = buildVarRefExp(const_cast<SgVariableSymbol *>(sb));
    } else {
      actual_arg = buildExactAddressOfOp(
          buildVarRefExp(const_cast<SgVariableSymbol *>(sb)));
    }
    ROSE_ASSERT(actual_arg != NULL);
    appendExpression(parameters, actual_arg);
  }
  ROSE_ASSERT(parameters != NULL);

  // extern void XOMP_parallel_start (void (*func) (void *), void *data,
  // unsigned ifClauseValue, unsigned numThreadsSpecified);
  // * func: pointer to a function which will be run in parallel
  // * data: pointer to a data segment which will be used as the arguments of
  // func
  // * ifClauseValue: set to if-clause-expression if if-clause exists, or
  // default is 1.
  // * numThreadsSpecified: set to the expression of num_threads clause if the
  // clause exists, or default is 0

  SgStatement *outlined_function_call =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_fork_call"),
                            buildVoidType(), parameters, p_scope);
  // the head of transformed code
  SgStatement *s1 = outlined_function_call;
  // the tail of transformed code
  SgStatement *s2 = s1;

  // if num_threads clause exists, we need to set up the omp number of threads
  // first. therefore, the head will be the function call of setting up
  // num_threads.
  SgExprStatement *set_num_threads_statement = NULL;
  SgExpression *omp_num_threads = cached_num_threads;
  if (omp_num_threads != NULL) {
    SgStatement *kmpc_global_tid_init = NULL;
    kmpc_global_tid_declaration =
        get_kmpc_global_tid(require_kmpc_global_tid_source_context(target),
                            p_scope, &kmpc_global_tid_init);
    thread_global_tid = buildVarRefExp(
        getFirstVariable(*kmpc_global_tid_declaration).get_name(), p_scope);
    if (SageInterface::is_Fortran_language()) {
      insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                                p_scope);
    } else {
      insertStatement(target, kmpc_global_tid_declaration);
      kmpc_global_tid_declaration->set_parent(target->get_parent());
    }
    if (kmpc_global_tid_init != NULL) {
      if (SageInterface::is_Fortran_language())
        insertStatement(target, kmpc_global_tid_init);
      else
        insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
    }
    parameters =
        buildExprListExp(buildIntVal(0), thread_global_tid, omp_num_threads);
    set_num_threads_statement = buildFunctionCallStmt(
        getKmpcRuntimeFunctionName("__kmpc_push_num_threads"), buildVoidType(),
        parameters, p_scope);
    // set up the head of transformed code to num_threads setter
    // the tail is still the outlined function call
    s1 = set_num_threads_statement;
  };

  // transform the if clause
  // the head of transformed code will be the if statement in this case
  SgExpression *if_condition = cached_if_condition;
  if (if_condition != NULL) {
    if (omp_num_threads == NULL) {
      SgStatement *kmpc_global_tid_init = NULL;
      kmpc_global_tid_declaration =
          get_kmpc_global_tid(require_kmpc_global_tid_source_context(target),
                              p_scope, &kmpc_global_tid_init);
      thread_global_tid = buildVarRefExp(
          getFirstVariable(*kmpc_global_tid_declaration).get_name(), p_scope);
      if (SageInterface::is_Fortran_language()) {
        insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                                  p_scope);
      } else {
        insertStatement(target, kmpc_global_tid_declaration);
        kmpc_global_tid_declaration->set_parent(target->get_parent());
      }
      if (kmpc_global_tid_init != NULL) {
        if (SageInterface::is_Fortran_language())
          insertStatement(target, kmpc_global_tid_init);
        else
          insertStatementAfter(kmpc_global_tid_declaration,
                               kmpc_global_tid_init);
      }
    };
    SgIfStmt *if_statement = buildIfStmt(if_condition, s1, NULL);
    SgExprStatement *else_stmt = NULL;
    SgBasicBlock *false_body = buildBasicBlock();
    if (SageInterface::is_Fortran_language()) {
      parameters =
          buildExprListExp(copyExpression(thread_global_tid), buildIntVal(0));
    } else {
      parameters = buildExprListExp(
          buildExactAddressOfOp(copyExpression(thread_global_tid)),
          buildIntVal(0));
    }
    ASTtools::VarSymSet_t::iterator iter;
    for (iter = syms.begin(); iter != syms.end(); iter++) {
      const SgVariableSymbol *sb = *iter;
      SgExpression *actual_arg = NULL;
      if (SageInterface::is_Fortran_language()) {
        actual_arg = buildVarRefExp(const_cast<SgVariableSymbol *>(sb));
      } else {
        actual_arg = buildExactAddressOfOp(
            buildVarRefExp(const_cast<SgVariableSymbol *>(sb)));
      }
      ROSE_ASSERT(actual_arg != NULL);
      appendExpression(parameters, actual_arg);
    }
    else_stmt = buildFunctionCallStmt(outlined_func->get_name(),
                                      buildVoidType(), parameters, p_scope);
    false_body->append_statement(else_stmt);
    if_statement->set_false_body(false_body);
    false_body->set_parent(if_statement);

    // the head and tail are both changed to the if statement because all the
    // other transformed code are included as children of if statement
    s1 = if_statement;
    s2 = s1;
  };

  SageInterface::replaceStatement(target, s1, true);

  // Keep preprocessing information
  // I have to use cut-paste instead of direct move since
  // the preprocessing information may be moved to a wrong place during
  // outlining while the destination node is unknown until the outlining is
  // done.
  pastePreprocessingInfo(s1, PreprocessingInfo::before, save_buf1);

  // we can only set up the relationship between these two statements now,
  // because ROSE requires that the targeting location must have the parent
  // info, which is not available until "pastePreprocessingInfo" right above.
  if (set_num_threads_statement != NULL) {
    SageInterface::insertStatementAfter(set_num_threads_statement,
                                        outlined_function_call);
  };

  SgExprListExp *parameters2 = buildExprListExp();
  if (!SageInterface::is_Fortran_language()) {
    string file_name = target->get_endOfConstruct()->get_filenameString();
    int line = target->get_endOfConstruct()->get_line();
    parameters2->append_expression(buildStringVal(file_name));
    parameters2->append_expression(buildIntVal(line));
  }

  pastePreprocessingInfo(s2, PreprocessingInfo::after, save_buf2);

  // Defensive cleanup: conditional directives are already resolved by the
  // frontend, and carrying stale #if/#endif fragments across outlining can
  // leave unbalanced directives in host output.
  stripConditionalDirectivesFromSubtree(s1);
  stripConditionalDirectivesFromSubtree(
      outlined_func->get_definition()->get_body());
  retireConsumedOmpDirective(target);
  if (SageInterface::is_Fortran_language()) {
    SgBasicBlock *func_body = func_def->get_body();
    if (outlined_external_declaration == NULL || func_body == NULL ||
        outlined_external_declaration->get_parent() != func_body ||
        !func_body->statementExistsInScope(outlined_external_declaration) ||
        outlined_external_declaration->get_parameter_list() == NULL ||
        outlined_external_declaration->get_parameter_list()
                ->get_expressions()
                .size() != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[outlined-external-declaration]: "
              "outlined procedure '%s' lost its exact host EXTERNAL "
              "declaration\n",
              outlined_func->get_name().getString().c_str());
      ROSE_ABORT();
    }
  }
  // Keep outlined procedures in the original file for Fortran and C++.
  // Fortran needs declaration-link consistency; C++ currently hits
  // qualification/ODR issues when outlined functions are moved to a synthesized
  // source file.
  if (!enable_accelerator && !SageInterface::is_Fortran_language() &&
      !SageInterface::is_Cxx_language()) {
    // Generate a new source file for the outlined function if necessary
    if (cpu_outlined_file == NULL) {
      cpu_outlined_file = generate_outlined_function_file(outlined_func, "");
    }
    SgSourceFile *outlined_owner =
        SageInterface::getEnclosingSourceFile(outlined_func, true);
    if (outlined_owner == NULL) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[cpu-outlined-function-owner]: function=%p "
              "has no exact source-file owner\n",
              static_cast<void *>(outlined_func));
      ROSE_ABORT();
    }
    if (outlined_owner == cpu_outlined_file) {
      SgGlobal *output_scope = cpu_outlined_file->get_globalScope();
      if (output_scope == NULL || outlined_func->get_parent() != output_scope ||
          outlined_func->get_scope() != output_scope ||
          !output_scope->statementExistsInScope(outlined_func) ||
          outlined_func->get_file_info() == NULL ||
          outlined_func->get_file_info()->get_physical_file_id() !=
              output_scope->get_file_info()->get_physical_file_id()) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[cpu-outlined-function-owner]: nested "
                "function=%p is not published exactly in its existing output "
                "translation unit\n",
                static_cast<void *>(outlined_func));
        ROSE_ABORT();
      }
    } else {
      // The cross-file move transaction validates and commits every
      // clause-variable identity from the deep-copy map before detaching the
      // original function. A nested outline already constructed in the output
      // file has no move transaction and is validated in place above.
      move_outlined_function(outlined_func, cpu_outlined_file);
    }
  }
}

//! Categorize exact ordered map items without collapsing repeated base symbols.
void categorizeMapClauseItems(
    const std::vector<OmpMapItemLoweringRecord> &records,
    std::set<SgSymbol *> &array_syms, // variable symbols which are array types
                                      // (explicit or as a pointer)
    std::set<SgSymbol *> &atom_syms) // variable symbols which are non-aggregate
                                     // types: scalar, pointer, etc
{
  for (const OmpMapItemLoweringRecord &record : records) {
    SgSymbol *sym = record.symbol;
    if (sym == nullptr || record.item == nullptr || record.clause == nullptr) {
      failMapperLoweringIdentity(
          "ordered map-item record lost its exact AST identity");
    }
    SgType *type = sym->get_type();
    // TODO handle complex types like structure, typedef, cast, etc. here
    if (isSgArrayType(type))
      array_syms.insert(sym);
    else if (isSgPointerType(type)) {
      if (!record.dimensions.empty())
        array_syms.insert(sym);
      else // otherwise a pointer pointing to non-array types
        atom_syms.insert(sym);
    } else if (isScalarType(type)) {
      atom_syms.insert(sym);
    } else if (isSgTypedefType(type)) {
      atom_syms.insert(sym);
    } else {
      cerr << "Error. transOmpMapVariables() of omp_lowering.cpp: unhandled "
              "map clause variable type:"
           << type->class_name() << endl;
      ROSE_ABORT();
    }
  }
  for (SgSymbol *symbol : array_syms) {
    atom_syms.erase(symbol);
  }
}

// Check if a variable is in the clause's variable list
bool isInClauseVariableList(SgOmpClause *cls, SgSymbol *var) {
  ROSE_ASSERT(cls && var);
  SgOmpVariablesClause *var_cls = isSgOmpVariablesClause(cls);
  ROSE_ASSERT(var_cls);
  SgExpressionPtrList refs =
      isSgOmpVariablesClause(var_cls)->get_variables()->get_expressions();

  std::vector<SgSymbol *> var_list;
  for (size_t j = 0; j < refs.size(); j++) {
    SgVariableSymbol *symbol = extractClauseVariableSymbol(refs[j]);
    if (symbol == nullptr) {
      continue;
    }
    var_list.push_back(symbol);
  }

  if (find(var_list.begin(), var_list.end(), var) != var_list.end())
    return true;
  else
    return false;
}

// ! Replace all references to original symbol with references to new symbol
// return the number of references being replaced.
// TODO: move to SageInterface
// static int replaceVariableReferences(SgNode* subtree, const SgVariableSymbol*
// origin_sym, SgVariableSymbol* new_sym )
static int replaceVariableReferences(
    SgNode *subtree,
    std::map<SgVariableSymbol *, SgVariableSymbol *> symbol_map) {
  int result = 0;
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(subtree, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    if (shouldSkipOpenMPClauseVarRefRewrite(vRef)) {
      continue;
    }
    // skip compiler generated references to the original variables which meant
    // to be kept.
    // TODO: maybe a better way is to match a pattern: if it is the first
    // parameter of xomp_deviceDataEnvironmentPrepareVariable()
    if (preservedHostVarRefs.find(vRef) != preservedHostVarRefs.end())
      continue;
    SgVariableSymbol *orig_sym = vRef->get_symbol();
    if (symbol_map[orig_sym] != NULL) {
      result++;
      rebindTransformedVariableReference(vRef, orig_sym, symbol_map[orig_sym],
                                         "accelerator-variable-substitution");
    }
  }
  return result;
}

// TODO: move to sageinterface, the current one has wrong reference type, and
// has undesired effect!!
//  grab the list of dimension sizes for an input array type, store them in the
//  vector container
static void getArrayDimensionSizes(const SgArrayType *array_type,
                                   std::vector<SgExpression *> &result) {
  ROSE_ASSERT(array_type != NULL);

  const SgType *cur_type = array_type;
  do {
    ROSE_ASSERT(isSgArrayType(cur_type) != NULL);
    SgExpression *index_exp = isSgArrayType(cur_type)->get_index();
    result.push_back(
        index_exp); // could be NULL, especially for the first dimension
    cur_type = isSgArrayType(cur_type)->get_base_type();
  } while (isSgArrayType(cur_type));
}

// TODO move to SageInterface
//  Liao 2/8/2013
//  rewrite array reference using multiple-dimension subscripts to a reference
//  using one-dimension subscripts e.g. a[i][j] is changed to a[i*col_size +j]
//       a [i][j][k]  is changed to a [(i*col_size + j)*K_size +k]
//  The parameter is the array reference expression to be changed
//  Note the array reference expression must be the top one since there will be
//  inner ones for a multi-dimensional array references in AST.
static void linearizeArrayAccess(SgPntrArrRefExp *top_array_ref) {
  // Sanity check
  //  TODO check language compatibility for C/C++ only: row major storage
  ROSE_ASSERT(top_array_ref != NULL);
  // ROSE_ASSERT (top_array_ref->get_lhs_operand_i() != NULL);
  ROSE_ASSERT(top_array_ref->get_parent() != NULL);
  ROSE_ASSERT(
      isSgPntrArrRefExp(top_array_ref->get_parent()) ==
      NULL); // top ==> must not be a child of a higher level array ref exp

  // must be a canonical array reference, not like (a+10)[10]
  SgExpression *arrayNameExp = NULL;
  std::vector<SgExpression *> *subscripts = new vector<SgExpression *>;
  bool is_array_ref =
      isArrayReference(top_array_ref, &arrayNameExp, &subscripts);
  ROSE_ASSERT(is_array_ref);
  SgInitializedName *i_name = convertRefToInitializedName(arrayNameExp);
  ROSE_ASSERT(i_name != NULL);
  SgType *var_type = i_name->get_type();
  SgArrayType *array_type = isSgArrayType(var_type);
  SgPointerType *pointer_type = isSgPointerType(var_type);
  // pointer type can also be used as pointer[i], which is represented as
  // SgPntrArrRefExp. In this case, we don't need to linearized it any more
  if (pointer_type != NULL)
    return;
  if (array_type == NULL) {
    cerr << "Error. linearizeArrayAccess() found unhandled variable type:"
         << var_type->class_name() << endl;
  }

  ROSE_ASSERT(array_type != NULL);

  std::vector<SgExpression *> dimensions;
  getArrayDimensionSizes(array_type, dimensions);

  ROSE_ASSERT((*subscripts).size() == dimensions.size());
  ROSE_ASSERT((*subscripts).size() >
              1); // we only accept 2-D or above for processing. Caller should
                  // check this in advance

  // left hand operand
  SgExpression *new_lhs = buildVarRefExp(i_name);
  SgExpression *new_rhs = deepCopy((*subscripts)[0]); // initialized to be i;

  // build rhs, like (i*col_size + j)*K_size +k
  for (size_t i = 1; i < dimensions.size();
       i++) // only repeat dimension count -1 times
  {
    SgType *index_type = new_rhs->get_type();
    ROSE_ASSERT(index_type != nullptr);
    new_rhs = buildAddOp(
        buildMultiplyOp(new_rhs, deepCopy(dimensions[i]), index_type),
        deepCopy((*subscripts)[i]), index_type);
  }

  // set new lhs and rhs for the top ref
  deepDelete(top_array_ref->get_lhs_operand_i());
  deepDelete(top_array_ref->get_rhs_operand_i());

  top_array_ref->set_lhs_operand_i(new_lhs);
  new_lhs->set_parent(top_array_ref);

  top_array_ref->set_rhs_operand_i(new_rhs);
  new_rhs->set_parent(top_array_ref);
}

// Find all top level array references within the body block,
// we do the following:
//   if it is within the set of arrays (array_syms) to be rewritten: arrays on
//   map() clause, if it is more than 1-D change it to be linearized subscript
//   access
static void
rewriteArraySubscripts(SgBasicBlock *body_block,
                       const std::set<SgSymbol *> mapped_array_syms) {
  std::vector<SgPntrArrRefExp *> candidate_refs; // store eligible references
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(body_block, V_SgPntrArrRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgPntrArrRefExp *vRef = isSgPntrArrRefExp((*i));
    ROSE_ASSERT(vRef != NULL);
    SgNode *parent = vRef->get_parent();
    // if it is top level ref?
    if (isSgPntrArrRefExp(parent)) // has a higher level array ref, skip it
      continue;
    // TODO: move this logic into a function in SageInterface
    //  If it is a canonical array reference we can handle?
    vector<SgExpression *> subscripts;
    vector<SgExpression *> *subscript_ptr = &subscripts;
    SgExpression *array_name_exp = NULL;
    isArrayReference(vRef, &array_name_exp, &subscript_ptr);
    SgInitializedName *a_name = convertRefToInitializedName(array_name_exp);
    if (a_name == NULL)
      continue;
    // if it is within the mapped array set?
    ROSE_ASSERT(a_name != NULL);
    SgSymbol *array_sym = a_name->get_symbol_from_symbol_table();
    ROSE_ASSERT(array_sym != NULL);

    if (mapped_array_syms.find(array_sym) != mapped_array_syms.end())
      candidate_refs.push_back(vRef);
  }

  // To be safe, we use reverse order iteration when changing them
  for (std::vector<SgPntrArrRefExp *>::reverse_iterator riter =
           candidate_refs.rbegin();
       riter != candidate_refs.rend(); riter++) {
    SgExpression *arrayNameExp = NULL;
    std::vector<SgExpression *> subscripts;
    std::vector<SgExpression *> *subscript_ptr = &subscripts;
    bool is_array_ref = isArrayReference(*riter, &arrayNameExp, &subscript_ptr);
    ROSE_ASSERT(is_array_ref);
    if (subscripts.size() > 1)
      linearizeArrayAccess(*riter);
  }
}

// Liao, 2/28/2013
// A helper function to collect variables used within a code portion
// To facilitate faster query into the variable collection, we use a map.
// TODO : move to SageInterface ?
std::map<SgVariableSymbol *, bool> collectVariableAppearance(SgNode *root) {
  std::map<SgVariableSymbol *, bool> result;
  ROSE_ASSERT(root != NULL);
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(root, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    SgVariableSymbol *sym = vRef->get_symbol();
    ROSE_ASSERT(sym != NULL);
    result[sym] = true;
  }
  return result;
}

// Preserve exact directive/clause/item order while classifying map operations.
void extractMapClauses(Rose_STL_Container<SgOmpClause *> map_clauses,
                       std::vector<OmpMapItemLoweringRecord> &item_records) {
  if (map_clauses.size() == 0)
    return; // stop if no map clauses at all

  for (Rose_STL_Container<SgOmpClause *>::const_iterator iter =
           map_clauses.begin();
       iter != map_clauses.end(); iter++) {
    SgOmpMapClause *m_cls = isSgOmpMapClause(*iter);
    ROSE_ASSERT(m_cls != NULL);
    appendOwnedMapItemLoweringRecords(m_cls, item_records);

    SgOmpClause::omp_map_operator_enum map_operator = m_cls->get_operation();
    switch (map_operator) {
    case SgOmpClause::e_omp_map_alloc:
    case SgOmpClause::e_omp_map_storage:
    case SgOmpClause::e_omp_map_release:
    case SgOmpClause::e_omp_map_delete:
    case SgOmpClause::e_omp_map_to:
    case SgOmpClause::e_omp_map_from:
    case SgOmpClause::e_omp_map_tofrom:
    case SgOmpClause::e_omp_map_present:
    case SgOmpClause::e_omp_map_self:
    case SgOmpClause::e_omp_map_unknown:
      break;
    default:
      cerr << "Error. transOmpMapVariables() from omp_lowering.cpp: found "
              "unacceptable map operator type:"
           << map_operator << endl;
      ROSE_ABORT();
    }
  } // end for
}

static int
generate_mapping_variable_type(const OmpMapItemLoweringRecord &record) {
  bool needCopyTo = false;
  bool needCopyFrom = false;
  switch (record.operation) {
  case SgOmpClause::e_omp_map_to:
    needCopyTo = true;
    break;
  case SgOmpClause::e_omp_map_from:
    needCopyFrom = true;
    break;
  case SgOmpClause::e_omp_map_tofrom:
  case SgOmpClause::e_omp_map_present:
  case SgOmpClause::e_omp_map_self:
  case SgOmpClause::e_omp_map_unknown:
    needCopyTo = true;
    needCopyFrom = true;
    break;
  case SgOmpClause::e_omp_map_alloc:
  case SgOmpClause::e_omp_map_storage:
  case SgOmpClause::e_omp_map_release:
  case SgOmpClause::e_omp_map_delete:
    break;
  default:
    failMapperLoweringIdentity("map item has an invalid mapping operation");
  }

  int type_value = OMP_TGT_MAPTYPE_TARGET_PARAM;

  if (needCopyTo) {
    type_value = type_value | OMP_TGT_MAPTYPE_TO;
  };

  if (needCopyFrom) {
    type_value = type_value | OMP_TGT_MAPTYPE_FROM;
  };

  return type_value;
}

// Translated a single mapped array variable, knowing the map clauses , where to
// insert, etc. Only generate memory allocation, deallocation, copy, functions,
// not the declaration since decl involves too many variable bookkeeping. This
// is intended to be called by a for loop going through all mapped array
// variables.
//  Essentially, we have to decide if we need to do the following steps for each
//  variable
//
//  Data handling: declaration, allocation, and copy
//    1. declared a pointer type to the device copy : pass by pointer type vs.
//    pass by value
//    2. allocate device copy using the dimension bound info: for array types
//    (pointers used for linearized arrays)
//    3. copy the data from CPU to the device (GPU) copy:
//
//    4. replace references to the CPU copies with references to the GPU copy
//    5. replace original multidimensional element indexing with linearized
//    address indexing (for 2-D and more dimension arrays)
//
//  Data handling: copy back, de-allocation
//    6. copy GPU_copy back to CPU variables
//    7. de-allocate the GPU variables
//
//   Step 1,2,3 and 6, 7 should generate statements before or after the
//   SgOmpTargetStatement Step 4 and 5 should change the body of the affected
//   SgOmpParallelStatement
// Revised Algorithm (version 3)    1/23/2015, optionally use device data
// environment (DDE) functions to manage data automatically. Instead of generate
// explicit data allocation, copy, free functions, using the following three DDE
// functions:
//   1. xomp_deviceDataEnvironmentEnter()
//   2. xomp_deviceDataEnvironmentPrepareVariable ()
//   3. xomp_deviceDataEnvironmentExit()
// This is necessary to have a consistent translation for mapped data showing up
// in both "target data" and "target" directives. These DDE functions internally
// will keep track of data allocated and try to reuse enclosing data
// environment.
static void generateMappedArrayMemoryHandling(
    const OmpMapItemLoweringRecord &record, SgExpression *device_expression,
    /*Where to insert generated function calls*/
    SgBasicBlock *insertion_scope, SgStatement *insertion_anchor_stmt,
    bool need_generate_data_stmt,
    std::vector<SgExpression *> *map_variable_list,
    std::vector<SgExpression *> *map_variable_base_list,
    std::vector<SgExpression *> *map_variable_size_list,
    std::vector<SgExpression *> *map_variable_type_list) {
  SgSymbol *sym = record.symbol;
  ROSE_ASSERT(sym != NULL);
  ROSE_ASSERT(device_expression !=
              NULL); // runtime now needs explicit device ID to work
  SgType *orig_type = sym->get_type();
  const std::vector<ResolvedOmpArraySectionDimension> effective_dimensions =
      resolveOmpArraySectionDimensions(record.dimensions, orig_type,
                                       "legacy-map-item-section");
  if (effective_dimensions.empty()) {
    failMapperLoweringIdentity(
        "mapped array item has neither a section nor declared dimensions");
  }

  // Step 1: declare a pointer type to array variables in map clauses, we
  // linearize all arrays to be a 1-D pointer
  //   Element_type * _dev_var;
  //   e.g.: double* _dev_array;
  // I believe that all array variables need allocations on GPUs, regardless
  // their map operations (alloc, to, from, or tofrom)

  SgType *element_type = getOmpArraySectionElementType(
      orig_type, effective_dimensions.size(), "legacy-map-item-section");
  string orig_name = (sym->get_name()).getString();
  string dev_var_name = "_dev_" + orig_name;
  SgExpression *mapping_array_size =
      buildOmpArraySectionExtentExpression(effective_dimensions);
  SgExpression *mapping_array_offset =
      buildOmpArraySectionLinearOffsetExpression(effective_dimensions);
  auto build_mapping_total_bytes = [&]() {
    SgType *size_type = SageInterface::requireTargetSizeType(insertion_scope);
    return buildMultiplyOp(buildSizeOfOp(element_type, size_type),
                           copyExpression(mapping_array_size), size_type);
  };
  const int mapping_variable_type_enum = generate_mapping_variable_type(record);
  if (need_generate_data_stmt) {
    const bool needCopyTo =
        (mapping_variable_type_enum & OMP_TGT_MAPTYPE_TO) != 0;
    const bool needCopyFrom =
        (mapping_variable_type_enum & OMP_TGT_MAPTYPE_FROM) != 0;

    if (useDDE) {
      // a single function call does all things transparently: reuse first, if
      // not then allocation, copy data e.g. float* _dev_u = (float*)
      // xomp_deviceDataEnvironmentPrepareVariable ((void*)u, _dev_u_size, true,
      // false);
      SgExpression *copyToExp = NULL;
      SgExpression *copyFromExp = NULL;
      if (needCopyTo)
        copyToExp = buildBoolValExp(1);
      else
        copyToExp = buildBoolValExp(0);

      if (needCopyFrom)
        copyFromExp = buildBoolValExp(1);
      else
        copyFromExp = buildBoolValExp(0);

      SgVarRefExp *host_var_ref = buildVarRefExp(isSgVariableSymbol(sym));
      preservedHostVarRefs.insert(host_var_ref);
      // cout<<"Debug: inserting var ref to be
      // preserved:"<<sym->get_name()<<"@"<<host_var_ref <<endl;
      //  should not be done here. Only one call for a whole device data
      //  environment Now insert xomp_deviceDataEnvironmentEnter() before
      //  xomp_deviceDataEnvironmentPrepareVariable()
      // SgExprStatement* dde_enter_stmt = buildFunctionCallStmt
      // (SgName("xomp_deviceDataEnvironmentEnter"), buildVoidType(), NULL,
      // insertion_scope);
      // insertStatementBefore (dde_prep_stmt, dde_enter_stmt);
    } else {
      // Step 2.5 generate memory allocation on GPUs
      // e.g.:  _dev_m1 = (double *)xomp_deviceMalloc (_dev_m1_size);
      SgExprStatement *mem_alloc_stmt = buildAssignStatement(
          buildVarRefExp(dev_var_name, insertion_scope),
          buildCastExp(buildFunctionCallExp(
                           SgName("xomp_deviceMalloc"),
                           buildPointerType(buildVoidType()),
                           buildExprListExp(build_mapping_total_bytes()),
                           insertion_scope),
                       buildPointerType(element_type)));
      insertStatementBefore(insertion_anchor_stmt, mem_alloc_stmt);

      // Step 3. copy the data from CPU to GPU
      // Only for variable in map(to:), or map(tofrom:)
      // e.g. xomp_memcpyHostToDevice ((void*)dev_m1, (const void*)a,
      // array_size);
      if (needCopyTo) {
        SgExprListExp *parameters = buildExprListExp(
            buildCastExp(buildVarRefExp(dev_var_name, insertion_scope),
                         buildPointerType(buildVoidType())),
            buildCastExp(buildVarRefExp(orig_name, insertion_scope),
                         buildPointerType(buildConstType(buildVoidType()))),
            build_mapping_total_bytes());
        SgExprStatement *mem_copy_to_stmt = buildFunctionCallStmt(
            SgName("xomp_memcpyHostToDevice"),
            buildPointerType(buildVoidType()), parameters, insertion_scope);
        // insertStatementBefore (insertion_anchor_stmt, mem_copy_to_stmt);
      }
    }

    if (useDDE) { // call xomp_deviceDataEnvironmentExit() and it will
                  // automatically copy back data and deallocate.
                  // SgExprStatement* dde_exit_stmt = buildFunctionCallStmt
                  // (SgName("xomp_deviceDataEnvironmentExit"), buildVoidType(),
                  // NULL, insertion_scope);
                  // appendStatement(dde_exit_stmt ,
                  // insertion_anchor_stmt->get_scope()); do nothing here or we
                  // will get multiple exit() for a single DDE.
    } else {      // or explicitly control copy back and deallocation
      // Step 6. copy back data from GPU to CPU, only for variable in
      // map(out:var_list) e.g. xomp_memcpyDeviceToHost ((void*)c, (const
      // void*)dev_m3, array_size); Note: insert this AFTER the target directive
      // stmt SgStatement* prev_stmt = target_parallel_stmt;
      if (needCopyFrom) {
        SgExprListExp *parameters = buildExprListExp(
            buildCastExp(buildVarRefExp(orig_name, insertion_scope),
                         buildPointerType(buildVoidType())),
            buildCastExp(buildVarRefExp(dev_var_name, insertion_scope),
                         buildPointerType(buildConstType(buildVoidType()))),
            build_mapping_total_bytes());
        SgExprStatement *mem_copy_back_stmt = buildFunctionCallStmt(
            SgName("xomp_memcpyDeviceToHost"),
            buildPointerType(buildVoidType()), parameters, insertion_scope);
        // appendStatement(mem_copy_back_stmt,
        // insertion_anchor_stmt->get_scope()); prev_stmt = mem_copy_back_stmt;
      }

      // Step 7, de-allocate GPU memory
      // e.g. xomp_freeDevice(dev_m1);
      // Note: insert this AFTER the target directive stmt or the copy back stmt
      SgExprStatement *mem_dealloc_stmt = buildFunctionCallStmt(
          SgName("xomp_freeDevice"), buildBoolType(),
          buildExprListExp(buildVarRefExp(dev_var_name, insertion_scope)),
          insertion_scope);
      appendStatement(mem_dealloc_stmt, insertion_anchor_stmt->get_scope());
    }
  }

  // check the type of current array symbol and calculate the desired data size
  SgType *mapped_pointer_type = buildPointerType(element_type);
  map_variable_list->push_back(
      buildAddOp(buildCastExp(buildVarRefExp(sym->get_name(), sym->get_scope()),
                              mapped_pointer_type),
                 mapping_array_offset, mapped_pointer_type));
  map_variable_base_list->push_back(
      buildVarRefExp(sym->get_name(), sym->get_scope()));
  SgType *size_type = SageInterface::requireTargetSizeType(insertion_scope);
  SgExpression *mapping_variable_total_size =
      buildCastExp(buildMultiplyOp(buildSizeOfOp(element_type, size_type),
                                   mapping_array_size, size_type),
                   buildLongLongType());
  map_variable_size_list->push_back(mapping_variable_total_size);

  SgExpression *mapping_variable_value =
      buildIntVal(mapping_variable_type_enum);
  map_variable_type_list->push_back(mapping_variable_value);
}

// trans OpenMP map variables
// return all generated or remaining variables to be passed to the outliner
// Liao, 2/4/2013
// Translate the map clause variables associated with "omp target parallel"
// We only support combined "target parallel" or "parallel" immediately
// following "target" So we handle outlining and data handling for two
// directives at the same time
// TODO: move to the header
// Input:
//
//  map(alloc|to|from|tofrom:var_list)
//  array variable in var_list should have dimension bounds information like
//  [0:N-1][0:K-1]
//
//  Essentially, we have to decide if we need to do the following steps for each
//  variable
//
//  Data handling: declaration, allocation, and copy
//    1. declared a pointer type to the device copy : pass by pointer type vs.
//    pass by value
//    2. allocate device copy using the dimension bound info: for array types
//    (pointers used for linearized arrays)
//    3. copy the data from CPU to the device (GPU) copy:
//
//    4. replace references to the CPU copies with references to the GPU copy
//    5. replace original multidimensional element indexing with linearized
//    address indexing (for 2-D and more dimension arrays)
//
//  Data handling: copy back, de-allocation
//    6. copy GPU_copy back to CPU variables
//    7. de-allocate the GPU variables
//
//   Step 1,2,3 and 6, 7 should generate statements before or after the
//   SgOmpTargetStatement Step 4 and 5 should change the body of the affected
//   SgOmpParallelStatement
//
//  Algorithm 1:
//   collect all variables in map clauses: they should be either scalar or
//   arrays with bound info. For each array variable,
//       we generate memory handling statements for them: declaration,
//       allocation, copy back-forth, de-allocation
//   For the use of array variable,
//       we replace the original references with references to new pointer typed
//       variables Linearize the access when 2-D or more dimensions are used.
//
//   Based on the mapped variables, we output the variables to be passed to the
//   outlined function to be generated later on
//         variables which will be passed by their original data types
//         variables which will be passed by their address of type: pointer type
//         pointing to their original data type
//
//  Revised Algorithm (version 2):  To translate "omp target" + "omp parallel
//  for" enclosed within "omp target data" region: New facts:
//        the map clauses are now associated with "omp target data" instead of
//        "omp target" Only a subset of all mapped variables at "omp target
//        data" level will be used within "omp target":
//           a single data region contains multiple "omp target" regions
//        When translating "omp target" + "omp parallel for", we don't need to
//        generate data handling statements
//            but we need to refer to the declarations for device variables.
//        Memory declaration, allocation, copy back-forth, de-allocation is
//        generated within the body of the "omp target data" region.
//            we can still try to generate them when translating "omp parallel
//            for" under "omp target", if not yet generated before.
//
// Revised Algorithm (V3): using Device Data Environment (DDE) runtime support
// to manage nested data regions
//       To simplify the handling, we assume
//         1. Both "target data"  and "target parallel for " should have map()
//         clauses
//         2. Using DDE, the translation is simplified as is identical for both
//         directive
ASTtools::VarSymSet_t transOmpMapVariables(
    SgStatement *node, SgExprListExp *map_variable_list,
    SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list,
    SgExprListExp *map_variable_type_list,
    GpuOffloadLoweringContext *offload_ctx = NULL,
    std::vector<ExpandedMapEntry> *dynamic_entries_out = NULL) {
  ASTtools::VarSymSet_t all_syms;
  ROSE_ASSERT(all_syms.size() == 0); // it should be empty

  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);

  // collect map clauses and their variables
  // ----------------------------------------------------------
  // Some notes for the relevant AST input:
  // we store a map clause for each variant/operator (alloc, to, from, and
  // tofrom), so there should be up to 4 SgOmpMapClause.
  //    SgOmpClause::omp_map_operator_enum
  // each map clause has
  //   an owned list of SgOmpMapItem expressions. Array-section bounds are
  //   structural children of each item's locator expression.

  Rose_STL_Container<SgOmpClause *> map_clauses =
      getClause(target, V_SgOmpMapClause);

  if (map_clauses.size() == 0)
    return all_syms; // stop if no map clauses at all

  std::vector<OmpMapItemLoweringRecord> map_item_records;

  // a map between original symbol and its device version : used for variable
  // replacement
  std::map<SgVariableSymbol *, SgVariableSymbol *> cpu_gpu_var_map;

  // store all variables showing up in any of the device clauses
  SgExpression *device_expression =
      getClauseExpression(target, VariantVector(V_SgOmpDeviceClause));

  extractMapClauses(map_clauses, map_item_records);
  std::set<SgSymbol *> array_syms; // store clause variable symbols which are
                                   // array types (explicit or as a pointer)
  std::set<SgSymbol *> atom_syms;  // store clause variable symbols which are
                                   // non-aggregate types: scalar, pointer, etc

  // categorize the variables:
  categorizeMapClauseItems(map_item_records, array_syms, atom_syms);

  // set the scope and anchor statement we will focus on based on the
  // availability of an enclosing target data region
  SgBasicBlock *insertion_scope = NULL; // the body
  SgStatement *insertion_anchor_stmt =
      NULL; // the single statement within the body

  // at this point, the body should already be normalized to be a BB
  SgBasicBlock *body_block = ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(body_block != NULL);

  SgStatement *target_child_stmt = NULL;
  // We cannot assert this since the body of "omp target data" may already be
  // expanded as part of a previous translation
  //    ROSE_ASSERT( (target_data_stmt_body->get_statements()).size() ==1);
  target_child_stmt = (body_block->get_statements())[0];

  insertion_scope = body_block;
  insertion_anchor_stmt = target_child_stmt;
  ROSE_ASSERT(insertion_scope != NULL);
  ROSE_ASSERT(insertion_anchor_stmt != NULL);

  // collect used variables in the insertion scope
  std::map<SgVariableSymbol *, bool> variable_map =
      collectVariableAppearance(insertion_scope);

  if (device_expression == NULL) {
    device_expression = buildIntVal(0);
  };

  std::vector<SgExpression *> side_effect_map_variable_list;
  std::vector<SgExpression *> side_effect_map_variable_base_list;
  std::vector<SgExpression *> side_effect_map_variable_size_list;
  std::vector<SgExpression *> side_effect_map_variable_type_list;
  // handle array variables showing up in the map clauses:
  for (std::set<SgSymbol *>::const_iterator iter = array_syms.begin();
       iter != array_syms.end(); iter++) {
    SgSymbol *sym = *iter;
    ROSE_ASSERT(sym != NULL);
    SgType *orig_type = sym->get_type();

    // Step 1: declare a pointer type to array variables in map clauses, we
    // linearize all arrays to be a 1-D pointer
    //   Element_type * _dev_var;
    //   e.g.: double* _dev_array;
    // I believe that all array variables need allocations on GPUs, regardless
    // their map operations (alloc, to, from, or tofrom)

    SgVariableSymbol *orig_sym = isSgVariableSymbol(sym);
    SgInitializedName *orig_name_decl =
        orig_sym != nullptr ? orig_sym->get_declaration() : nullptr;
    SgType *source_orig_type =
        getExactCxxSourceType(orig_name_decl, "device-placeholder-source-type");
    SgType *element_type = nullptr;
    SgType *source_element_type = nullptr;
    for (const OmpMapItemLoweringRecord &record : map_item_records) {
      if (record.symbol != sym) {
        continue;
      }
      const std::vector<ResolvedOmpArraySectionDimension> effective_dimensions =
          resolveOmpArraySectionDimensions(record.dimensions, orig_type,
                                           "device-placeholder-section");
      SgType *record_element_type = getOmpArraySectionElementType(
          orig_type, effective_dimensions.size(), "device-placeholder-section");
      SgType *record_source_element_type = getOmpArraySectionSourceElementType(
          source_orig_type, record_element_type, effective_dimensions.size(),
          "device-placeholder-source-section");
      if ((element_type != nullptr && !SageInterface::isEquivalentType(
                                          element_type, record_element_type)) ||
          (source_element_type != nullptr &&
           !SageInterface::isEquivalentType(source_element_type,
                                            record_source_element_type))) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[device-placeholder-section]: "
                     "mapped symbol "
                  << sym->get_name()
                  << " has incompatible element types across exact map items"
                  << std::endl;
        ROSE_ABORT();
      }
      element_type = record_element_type;
      source_element_type = record_source_element_type;
    }
    if (element_type == nullptr || source_element_type == nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[device-placeholder-section]: "
                   "mapped symbol "
                << sym->get_name() << " has no exact mapped element type"
                << std::endl;
      ROSE_ABORT();
    }
    string orig_name = (sym->get_name()).getString();
    string dev_var_name = "_dev_" + orig_name;

    const bool use_const_device_pointer =
        mappedArrayUsesAreReadOnlyInScope(insertion_scope, orig_sym);
    SgType *dev_var_type = buildPointerType(
        use_const_device_pointer ? buildConstType(element_type) : element_type);
    SgType *source_dev_var_type = buildPointerType(
        use_const_device_pointer ? buildConstType(source_element_type)
                                 : source_element_type);
    if (!SageInterface::isEquivalentType(dev_var_type, source_dev_var_type)) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[device-placeholder-source-"
                   "type]: mapped symbol "
                << sym->get_name()
                << " produced inequivalent semantic and source device types"
                << std::endl;
      ROSE_ABORT();
    }

    // The device pointer is a semantic placeholder consumed by the outlining
    // transaction.  It must participate in symbol lookup and copy/remapping,
    // but it is not a lexical declaration in either the host driver or the
    // outlined kernel.  Keep it under the scope's explicit non-output owner
    // until every rewritten reference has been remapped to a kernel parameter.
    SgVariableDeclaration *dev_var_decl =
        buildSemanticAuxiliaryVariableDeclaration(dev_var_name, dev_var_type,
                                                  NULL, insertion_scope);
    ROSE_ASSERT(dev_var_decl != NULL);
    SgInitializedName *dev_var_name_decl =
        SageInterface::getFirstInitializedName(dev_var_decl);
    if (dev_var_name_decl == nullptr ||
        dev_var_name_decl->get_type() != dev_var_type) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[device-placeholder-source-"
                   "type]: device placeholder has no exact semantic "
                   "declarator"
                << std::endl;
      ROSE_ABORT();
    }
    if (source_dev_var_type != dev_var_type) {
      dev_var_name_decl->set_cxx_source_type(source_dev_var_type);
      if (dev_var_name_decl->get_cxx_source_type() != source_dev_var_type) {
        ROSE_ABORT();
      }
    }
    if (!isSgOmpTargetDataStatement(node)) {
      if (offload_ctx == NULL) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[device-placeholder-transaction]: target "
                "placeholder=%p has no exact offload transaction\n",
                static_cast<void *>(dev_var_decl));
        ROSE_ABORT();
      }
      offload_ctx->semantic_device_placeholders.push_back(dev_var_decl);
    }

    ROSE_ASSERT(orig_sym != NULL);
    SgVariableSymbol *new_sym = getFirstVarSym(dev_var_decl);
    cpu_gpu_var_map[orig_sym] = new_sym; // store the mapping, this is always
                                         // needed to guide the outlining

    // Not all map variables from "omp target data" will be used within the
    // current parallel region We only need to find out the used one only.

    // linearized array pointers should be directly passed to the outliner later
    // on, without adding & operator in front of them we assume AST is
    // normalized and all target regions have explicit and correct map() clauses
    // Still some transformation like loop collapse will change the variables
    if (variable_map[orig_sym])
      all_syms.insert(new_sym);
  } // end for

  // Build mapping arguments once per exact map item in source order. Multiple
  // sections with the same base symbol intentionally remain distinct records.
  std::unordered_set<SgSymbol *> generated_map_data_symbols;
  for (const OmpMapItemLoweringRecord &record : map_item_records) {
    if (array_syms.find(record.symbol) == array_syms.end()) {
      continue;
    }
    const bool generate_data_statements =
        generated_map_data_symbols.insert(record.symbol).second;
    generateMappedArrayMemoryHandling(
        record, device_expression, insertion_scope, insertion_anchor_stmt,
        generate_data_statements, &side_effect_map_variable_list,
        &side_effect_map_variable_base_list,
        &side_effect_map_variable_size_list,
        &side_effect_map_variable_type_list);
  }

  // C/C++ mapping arguments are produced by the resolved-item path below.
  // Fortran lowering still needs the legacy array side effects emitted while
  // materializing mapped array handling above.
  if (SageInterface::is_Fortran_language()) {
    for (SgExpression *expr : side_effect_map_variable_list) {
      map_variable_list->append_expression(expr);
    }
    for (SgExpression *expr : side_effect_map_variable_base_list) {
      map_variable_base_list->append_expression(expr);
    }
    for (SgExpression *expr : side_effect_map_variable_size_list) {
      map_variable_size_list->append_expression(expr);
    }
    for (SgExpression *expr : side_effect_map_variable_type_list) {
      map_variable_type_list->append_expression(expr);
    }
  }

  // Step 5. TODO  replace indexing element access with address calculation
  // (only needed for 2/3 -D) We switch the order of 4 and 5 since we want to
  // rewrite the subscripts before the arrays are replaced
  rewriteArraySubscripts(insertion_scope, array_syms);

  // Step 4. replace references to old with new variables,
  // The omp target data region is still executed on the host. We don't need to
  // outline it or rename its variables. Thus, the original body should be
  // preserved.
  if (!isSgOmpTargetDataStatement(node))
    replaceVariableReferences(insertion_scope, cpu_gpu_var_map);

  // TODO handle scalar, separate or merged into previous loop ?

  // store remaining variables so outliner can readily use this information
  // for pointers to linearized arrays, they should passed by their original
  // form, not using & operator, regardless the map operator types
  // (to|from|alloc|tofrom) for a scalar, two cases: to vs. from | tofrom if in
  // only, pass by value is good if either from or tofrom: two possible
  // solutions: 1) we need to treat it as an array of size 1 or any other
  // choices. TODO!!
  //  we also have to replace the reference to scalar to the array element
  //  access: be cautious about using by value (a) vs. using by address  (&a)
  // 2) try to still pass by value, but copy the final value back to the CPU
  // version right now we assume they are not on from|tofrom, until we face a
  // real input applications with map(from:scalar_a) For all scalars, we
  // directly copy them into all_syms for now
  for (std::set<SgSymbol *>::iterator iter = atom_syms.begin();
       iter != atom_syms.end(); iter++) {
    SgVariableSymbol *var_sym = isSgVariableSymbol(*iter);
    if (variable_map[var_sym] ==
        true) // we should only collect map variables which show up in the
              // current parallel region
      all_syms.insert(var_sym);
  }

  std::vector<ExpandedMapEntry> expanded_entries;
  for (SgOmpClause *clause : map_clauses) {
    SgOmpMapClause *map_clause = isSgOmpMapClause(clause);
    if (map_clause == NULL)
      continue;
    std::vector<ExpandedMapEntry> clause_items =
        collectExpandedMapItemsForClause(target, map_clause);
    expanded_entries.insert(expanded_entries.end(), clause_items.begin(),
                            clause_items.end());
  }
  if (!isSgOmpTargetDataStatement(node) && offload_ctx != NULL) {
    for (ExpandedMapEntry &entry : expanded_entries) {
      if (entry.kind != ExpandedMapEntryKind::direct_item) {
        continue;
      }
      ResolvedMapItem &item = entry.direct_item;
      if (item.direct_variable_symbol == NULL ||
          variable_map[item.direct_variable_symbol] != true) {
        continue;
      }

      item.is_implicit_target_variable =
          isImplicitTargetMapVariable(target, item.direct_variable_symbol);
      if (!canUseLiteralTargetParam(target, item.direct_variable_symbol,
                                    item.map_operator)) {
        continue;
      }

      item.use_literal_target_param = true;
      offload_ctx->literal_target_param_syms.insert(
          item.direct_variable_symbol);
    }
  }
  const bool has_dynamic_entries =
      hasDynamicExpandedMapEntries(expanded_entries);
  if (dynamic_entries_out != NULL) {
    dynamic_entries_out->clear();
    if (has_dynamic_entries) {
      *dynamic_entries_out = expanded_entries;
    }
  }
  if (!has_dynamic_entries) {
    std::vector<ResolvedMapItem> resolved_items;
    collectDirectResolvedMapItems(expanded_entries, resolved_items);
    appendResolvedMapItemArguments(
        resolved_items, map_variable_list, map_variable_base_list,
        map_variable_size_list, map_variable_type_list, target->get_scope());
  }

  return all_syms;
} // end transOmpMapVariables() for omp target data's map clauses for now

// Collect mapping variables information in from/to clauses.
void collectOmpTargetUpdateInfo(
    SgStatement *target, SgExprListExp *map_variable_list,
    SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list,
    SgExprListExp *map_variable_type_list,
    std::vector<ExpandedMapEntry> *dynamic_entries_out = NULL) {
  ROSE_ASSERT(target != NULL);
  Rose_STL_Container<SgOmpClause *> from_clauses =
      getClause(target, V_SgOmpFromClause);
  Rose_STL_Container<SgOmpClause *> to_clauses =
      getClause(target, V_SgOmpToClause);

  std::vector<ExpandedMapEntry> expanded_entries;
  for (SgOmpClause *clause : from_clauses) {
    std::vector<ExpandedMapEntry> clause_items =
        collectExpandedMotionItemsForClause(target, clause);
    expanded_entries.insert(expanded_entries.end(), clause_items.begin(),
                            clause_items.end());
  }
  for (SgOmpClause *clause : to_clauses) {
    std::vector<ExpandedMapEntry> clause_items =
        collectExpandedMotionItemsForClause(target, clause);
    expanded_entries.insert(expanded_entries.end(), clause_items.begin(),
                            clause_items.end());
  }

  const bool has_dynamic_entries =
      hasDynamicExpandedMapEntries(expanded_entries);
  if (dynamic_entries_out != NULL) {
    dynamic_entries_out->clear();
    if (has_dynamic_entries) {
      *dynamic_entries_out = expanded_entries;
    }
  }
  if (!has_dynamic_entries) {
    std::vector<ResolvedMapItem> resolved_items;
    collectDirectResolvedMapItems(expanded_entries, resolved_items);
    appendResolvedMapItemArguments(
        resolved_items, map_variable_list, map_variable_base_list,
        map_variable_size_list, map_variable_type_list, target->get_scope());
  }
} // collectOmpTargetUpdateInfo()

struct RuntimeMapArgumentArrayDeclarations {
  SgVariableDeclaration *args_base_decl = nullptr;
  SgVariableDeclaration *args_decl = nullptr;
  SgVariableDeclaration *arg_sizes_decl = nullptr;
  SgVariableDeclaration *arg_types_decl = nullptr;
  SgVariableDeclaration *arg_number_decl = nullptr;
  bool uses_heap_storage = false;
};

size_t getMapArgumentListCount(SgExprListExp *map_variable_list,
                               SgExprListExp *map_variable_base_list,
                               SgExprListExp *map_variable_size_list,
                               SgExprListExp *map_variable_type_list) {
  if (map_variable_list == nullptr || map_variable_base_list == nullptr ||
      map_variable_size_list == nullptr || map_variable_type_list == nullptr) {
    return 0;
  }

  const size_t arg_count = map_variable_list->get_expressions().size();
  ROSE_ASSERT(map_variable_base_list->get_expressions().size() == arg_count);
  ROSE_ASSERT(map_variable_size_list->get_expressions().size() == arg_count);
  ROSE_ASSERT(map_variable_type_list->get_expressions().size() == arg_count);
  return arg_count;
}

SgExpression *
buildArraySectionElementIndexExpression(SgExpression *lower_bound,
                                        SgVariableSymbol *index_symbol,
                                        SgScopeStatement *scope) {
  ROSE_ASSERT(index_symbol != nullptr);

  SgExpression *index_expr = buildVarRefExp(index_symbol);
  if (lower_bound == nullptr || isSgNullExpression(lower_bound) != nullptr) {
    return index_expr;
  }

  SgType *lower_type = stripTypeAliasesAndReferences(lower_bound->get_type());
  if (lower_type != nullptr) {
    index_expr = buildCastExp(index_expr, buildLongLongType());
  }
  SgType *result_type = index_expr->get_type();
  ROSE_ASSERT(result_type != nullptr);
  return buildAddOp(copyExpression(lower_bound), index_expr, result_type);
}

SgExpression *buildArraySectionElementExpression(
    SgExpression *base_expression,
    const std::vector<ResolvedOmpArraySectionDimension> &dimensions,
    const std::vector<SgVariableSymbol *> &index_symbols,
    SgScopeStatement *scope) {
  ROSE_ASSERT(base_expression != nullptr);
  ROSE_ASSERT(dimensions.size() == index_symbols.size());

  SgExpression *result = copyExpression(base_expression);
  for (size_t i = 0; i < dimensions.size(); ++i) {
    SgType *indexed_type = stripTypeAliasesAndReferences(result->get_type());
    if (SgArrayType *array_type = isSgArrayType(indexed_type)) {
      indexed_type = array_type->get_base_type();
    } else if (SgPointerType *pointer_type = isSgPointerType(indexed_type)) {
      indexed_type = pointer_type->get_base_type();
    } else {
      MLOG_ERROR_CXX("ompLowering")
          << "array-section element base has no exact indexed type";
      ROSE_ABORT();
    }
    result = buildPntrArrRefExp(
        result,
        buildArraySectionElementIndexExpression(
            dimensions[i].lower_omitted ? nullptr : dimensions[i].lower,
            index_symbols[i], scope),
        indexed_type);
  }
  return result;
}

SgExpression *buildMallocArrayInitializer(SgType *element_type,
                                          SgExpression *element_count,
                                          SgScopeStatement *scope) {
  ROSE_ASSERT(element_type != nullptr);
  ROSE_ASSERT(element_count != nullptr);
  ROSE_ASSERT(scope != nullptr);

  SgType *size_type = SageInterface::requireTargetSizeType(scope);
  SgExpression *allocation_size =
      buildMultiplyOp(buildSizeOfOp(element_type, size_type),
                      copyExpression(element_count), size_type);
  return buildCastExp(
      buildFunctionCallExp(SgName("malloc"), buildPointerType(buildVoidType()),
                           buildExprListExp(allocation_size), scope),
      buildPointerType(element_type));
}

void appendMapArgumentArrayAssignment(SgBasicBlock *block,
                                      SgScopeStatement *scope,
                                      SgVariableDeclaration *target_decl,
                                      SgVariableDeclaration *index_decl,
                                      SgExpression *value_expr,
                                      SgType *element_type) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(target_decl != nullptr);
  ROSE_ASSERT(index_decl != nullptr);
  ROSE_ASSERT(value_expr != nullptr);
  ROSE_ASSERT(element_type != nullptr);

  block->append_statement(buildAssignStatement(
      buildPntrArrRefExp(buildVarRefExp(target_decl),
                         buildVarRefExp(index_decl), element_type),
      buildCastExp(copyExpression(value_expr), element_type)));
}

void appendRawMapArgumentListsToDynamicArrays(
    SgExprListExp *map_variable_list, SgExprListExp *map_variable_base_list,
    SgExprListExp *map_variable_size_list,
    SgExprListExp *map_variable_type_list, SgBasicBlock *block,
    SgScopeStatement *scope, SgVariableDeclaration *args_base_decl,
    SgVariableDeclaration *args_decl, SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl) {
  if (map_variable_list == nullptr || map_variable_base_list == nullptr ||
      map_variable_size_list == nullptr || map_variable_type_list == nullptr) {
    return;
  }

  const SgExpressionPtrList &args = map_variable_list->get_expressions();
  const SgExpressionPtrList &bases = map_variable_base_list->get_expressions();
  const SgExpressionPtrList &sizes = map_variable_size_list->get_expressions();
  const SgExpressionPtrList &types = map_variable_type_list->get_expressions();
  ROSE_ASSERT(args.size() == bases.size());
  ROSE_ASSERT(args.size() == sizes.size());
  ROSE_ASSERT(args.size() == types.size());

  SgType *void_ptr_type = buildPointerType(buildVoidType());
  SgType *int64_type = getRuntimeInt64Type(scope);
  for (size_t i = 0; i < args.size(); ++i) {
    appendMapArgumentArrayAssignment(block, scope, args_base_decl,
                                     arg_index_decl, bases[i], void_ptr_type);
    appendMapArgumentArrayAssignment(block, scope, args_decl, arg_index_decl,
                                     args[i], void_ptr_type);
    appendMapArgumentArrayAssignment(block, scope, arg_sizes_decl,
                                     arg_index_decl, sizes[i], int64_type);
    appendMapArgumentArrayAssignment(block, scope, arg_types_decl,
                                     arg_index_decl, types[i], int64_type);
    block->append_statement(buildExprStatement(buildPlusPlusOp(
        buildVarRefExp(arg_index_decl),
        getFirstVariable(*arg_index_decl).get_type(), SgUnaryOp::postfix)));
  }
}

enum class DynamicMapExpansionPass { count_only, populate };

void appendExpandedMapEntriesDynamicPass(
    const std::vector<ExpandedMapEntry> &entries, DynamicMapExpansionPass pass,
    SgBasicBlock *block, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl,
    SgVariableDeclaration *args_base_decl, SgVariableDeclaration *args_decl,
    SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl, size_t &loop_counter,
    size_t &literal_counter);

void appendExpandedMapEntryDynamicPass(
    const ExpandedMapEntry &entry, DynamicMapExpansionPass pass,
    SgBasicBlock *block, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl,
    SgVariableDeclaration *args_base_decl, SgVariableDeclaration *args_decl,
    SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl, size_t &loop_counter,
    size_t &literal_counter) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);
  ROSE_ASSERT(arg_number_decl != nullptr);

  if (entry.kind == ExpandedMapEntryKind::direct_item) {
    if (pass == DynamicMapExpansionPass::count_only) {
      block->append_statement(buildExprStatement(
          buildPlusAssignOp(buildVarRefExp(arg_number_decl), buildIntVal(1),
                            getFirstVariable(*arg_number_decl).get_type())));
      return;
    }

    ROSE_ASSERT(args_base_decl != nullptr);
    ROSE_ASSERT(args_decl != nullptr);
    ROSE_ASSERT(arg_sizes_decl != nullptr);
    ROSE_ASSERT(arg_types_decl != nullptr);
    ROSE_ASSERT(arg_index_decl != nullptr);

    MapArgumentExpressions expressions =
        buildResolvedMapItemArgumentExpressions(entry.direct_item, scope);
    if (isLiteralTargetParamPackCall(expressions.mapping_expression) ||
        isLiteralTargetParamPackCall(expressions.mapping_base_expression)) {
      const std::string packed_name =
          "__rex_packed_literal_arg_dyn_" + std::to_string(literal_counter++);
      SgType *packed_type = buildPointerType(buildVoidType());
      SgVariableDeclaration *packed_decl = buildVariableDeclaration(
          packed_name, packed_type,
          buildAssignInitializer(copyExpression(expressions.mapping_expression),
                                 packed_type),
          block);
      block->append_statement(packed_decl);
      SgVariableSymbol *packed_symbol = getFirstVarSym(packed_decl);
      ROSE_ASSERT(packed_symbol != nullptr);
      expressions.mapping_expression = buildVarRefExp(packed_symbol);
      expressions.mapping_base_expression = buildVarRefExp(packed_symbol);
    }

    SgType *void_ptr_type = buildPointerType(buildVoidType());
    SgType *int64_type = getRuntimeInt64Type(scope);
    appendMapArgumentArrayAssignment(
        block, scope, args_base_decl, arg_index_decl,
        expressions.mapping_base_expression, void_ptr_type);
    appendMapArgumentArrayAssignment(block, scope, args_decl, arg_index_decl,
                                     expressions.mapping_expression,
                                     void_ptr_type);
    appendMapArgumentArrayAssignment(
        block, scope, arg_sizes_decl, arg_index_decl,
        expressions.mapping_size_expression, int64_type);
    appendMapArgumentArrayAssignment(
        block, scope, arg_types_decl, arg_index_decl,
        expressions.mapping_type_expression, int64_type);
    block->append_statement(buildExprStatement(buildPlusPlusOp(
        buildVarRefExp(arg_index_decl),
        getFirstVariable(*arg_index_decl).get_type(), SgUnaryOp::postfix)));
    return;
  }

  ROSE_ASSERT(entry.kind == ExpandedMapEntryKind::dynamic_mapper_section);
  ROSE_ASSERT(entry.section_base_expression != nullptr);
  ROSE_ASSERT(!entry.section_dimensions.empty());
  ROSE_ASSERT(entry.resolved_mapper.declaration != nullptr);
  const std::vector<ResolvedOmpArraySectionDimension> section_dimensions =
      resolveOmpArraySectionDimensions(
          entry.section_dimensions, entry.section_base_expression->get_type(),
          "dynamic-mapper-section");
  if (section_dimensions.empty()) {
    failMapperLoweringIdentity(
        "dynamic mapper section has no resolved dimensions");
  }

  std::function<void(size_t, SgBasicBlock *, std::vector<SgVariableSymbol *> &)>
      build_loop_nest;
  build_loop_nest = [&](size_t dim_index, SgBasicBlock *current_block,
                        std::vector<SgVariableSymbol *> &index_symbols) {
    if (dim_index == section_dimensions.size()) {
      SgExpression *element_expr = buildArraySectionElementExpression(
          entry.section_base_expression, section_dimensions, index_symbols,
          scope);
      std::vector<ExpandedMapEntry> nested_entries;
      std::vector<const SgOmpDeclareMapperStatement *> active_mappers;
      collectExpandedMapEntriesUsingResolvedMapper(
          element_expr, entry.resolved_mapper, entry.use_kind, entry.use_map_op,
          entry.runtime_flag_bits, entry.anchor_stmt, nested_entries,
          active_mappers);
      appendExpandedMapEntriesDynamicPass(
          nested_entries, pass, current_block, scope, arg_number_decl,
          args_base_decl, args_decl, arg_sizes_decl, arg_types_decl,
          arg_index_decl, loop_counter, literal_counter);
      return;
    }

    const ResolvedOmpArraySectionDimension &dimension =
        section_dimensions[dim_index];
    SgExpression *length_expr = buildOmpArraySectionLengthExpression(dimension);

    const std::string index_name =
        "__rex_mapper_section_index_" + std::to_string(loop_counter++);
    SgType *index_type = buildLongLongType();
    SgType *index_cast_type = getRuntimeInt64Type(scope);
    SgForStatement *for_loop =
        new SgForStatement(static_cast<Sg_File_Info *>(nullptr));
    ROSE_ASSERT(for_loop != nullptr);
    SgVariableDeclaration *index_decl = buildVariableDeclaration(
        index_name, index_type,
        buildAssignInitializer(buildLongLongIntVal(0), index_type), for_loop);
    SgVariableSymbol *index_symbol = getFirstVarSym(index_decl);
    ROSE_ASSERT(index_symbol != nullptr);
    index_symbols.push_back(index_symbol);

    SgBasicBlock *loop_body = buildBasicBlock();
    build_loop_nest(dim_index + 1, loop_body, index_symbols);
    SgForInitStatement *for_init = buildForInitStatement();
    for_init->append_init_stmt(index_decl);
    index_decl->set_parent(for_init);
    buildForStatement_nfi(for_loop, for_init,
                          buildExprStatement(buildLessThanOp(
                              buildVarRefExp(index_symbol),
                              buildCastExp(length_expr, index_cast_type),
                              exactLogicalResultType())),
                          buildPlusPlusOp(buildVarRefExp(index_symbol),
                                          index_type, SgUnaryOp::postfix),
                          loop_body);
    setSourcePositionForTransformation(for_loop);
    current_block->append_statement(for_loop);
    index_symbols.pop_back();
  };

  std::vector<SgVariableSymbol *> index_symbols;
  build_loop_nest(0, block, index_symbols);
}

void appendExpandedMapEntriesDynamicPass(
    const std::vector<ExpandedMapEntry> &entries, DynamicMapExpansionPass pass,
    SgBasicBlock *block, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl,
    SgVariableDeclaration *args_base_decl, SgVariableDeclaration *args_decl,
    SgVariableDeclaration *arg_sizes_decl,
    SgVariableDeclaration *arg_types_decl,
    SgVariableDeclaration *arg_index_decl, size_t &loop_counter,
    size_t &literal_counter) {
  for (const ExpandedMapEntry &entry : entries) {
    appendExpandedMapEntryDynamicPass(
        entry, pass, block, scope, arg_number_decl, args_base_decl, args_decl,
        arg_sizes_decl, arg_types_decl, arg_index_decl, loop_counter,
        literal_counter);
  }
}

RuntimeMapArgumentArrayDeclarations buildDynamicRuntimeMapArgumentArrays(
    SgBasicBlock *block, SgScopeStatement *scope,
    SgExprListExp *prefix_map_variable_list,
    SgExprListExp *prefix_map_variable_base_list,
    SgExprListExp *prefix_map_variable_size_list,
    SgExprListExp *prefix_map_variable_type_list,
    const std::vector<ExpandedMapEntry> &dynamic_entries,
    SgExprListExp *suffix_map_variable_list = NULL,
    SgExprListExp *suffix_map_variable_base_list = NULL,
    SgExprListExp *suffix_map_variable_size_list = NULL,
    SgExprListExp *suffix_map_variable_type_list = NULL) {
  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const size_t prefix_count = getMapArgumentListCount(
      prefix_map_variable_list, prefix_map_variable_base_list,
      prefix_map_variable_size_list, prefix_map_variable_type_list);
  const size_t suffix_count = getMapArgumentListCount(
      suffix_map_variable_list, suffix_map_variable_base_list,
      suffix_map_variable_size_list, suffix_map_variable_type_list);

  RuntimeMapArgumentArrayDeclarations result;
  result.uses_heap_storage = true;
  SgType *arg_number_type = buildIntType();
  result.arg_number_decl = buildVariableDeclaration(
      "__arg_num", arg_number_type,
      buildAssignInitializer(
          buildIntVal(static_cast<int>(prefix_count + suffix_count)),
          arg_number_type),
      block);
  block->append_statement(result.arg_number_decl);

  size_t loop_counter = 0;
  size_t literal_counter = 0;
  appendExpandedMapEntriesDynamicPass(
      dynamic_entries, DynamicMapExpansionPass::count_only, block, scope,
      result.arg_number_decl, NULL, NULL, NULL, NULL, NULL, loop_counter,
      literal_counter);

  SgExpression *arg_count_expr = buildVarRefExp(result.arg_number_decl);
  SgType *void_ptr_type = buildPointerType(buildVoidType());
  SgType *void_ptr_ptr_type = buildPointerType(void_ptr_type);
  SgType *int64_type = getRuntimeInt64Type(scope);
  SgType *int64_ptr_type = buildPointerType(int64_type);

  result.args_base_decl = buildVariableDeclaration(
      "__args_base", void_ptr_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(void_ptr_type, arg_count_expr, scope),
          void_ptr_ptr_type),
      block);
  block->append_statement(result.args_base_decl);

  result.args_decl = buildVariableDeclaration(
      "__args", void_ptr_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(void_ptr_type, arg_count_expr, scope),
          void_ptr_ptr_type),
      block);
  block->append_statement(result.args_decl);

  result.arg_sizes_decl = buildVariableDeclaration(
      "__arg_sizes", int64_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(int64_type, arg_count_expr, scope),
          int64_ptr_type),
      block);
  block->append_statement(result.arg_sizes_decl);

  result.arg_types_decl = buildVariableDeclaration(
      "__arg_types", int64_ptr_type,
      buildAssignInitializer(
          buildMallocArrayInitializer(int64_type, arg_count_expr, scope),
          int64_ptr_type),
      block);
  block->append_statement(result.arg_types_decl);

  SgType *arg_index_type = buildIntType();
  SgVariableDeclaration *arg_index_decl = buildVariableDeclaration(
      "__arg_index", arg_index_type,
      buildAssignInitializer(buildIntVal(0), arg_index_type), block);
  block->append_statement(arg_index_decl);

  appendRawMapArgumentListsToDynamicArrays(
      prefix_map_variable_list, prefix_map_variable_base_list,
      prefix_map_variable_size_list, prefix_map_variable_type_list, block,
      scope, result.args_base_decl, result.args_decl, result.arg_sizes_decl,
      result.arg_types_decl, arg_index_decl);

  loop_counter = 0;
  literal_counter = 0;
  appendExpandedMapEntriesDynamicPass(
      dynamic_entries, DynamicMapExpansionPass::populate, block, scope,
      result.arg_number_decl, result.args_base_decl, result.args_decl,
      result.arg_sizes_decl, result.arg_types_decl, arg_index_decl,
      loop_counter, literal_counter);

  appendRawMapArgumentListsToDynamicArrays(
      suffix_map_variable_list, suffix_map_variable_base_list,
      suffix_map_variable_size_list, suffix_map_variable_type_list, block,
      scope, result.args_base_decl, result.args_decl, result.arg_sizes_decl,
      result.arg_types_decl, arg_index_decl);

  return result;
}

void appendDynamicRuntimeMapArgumentArrayCleanup(
    const RuntimeMapArgumentArrayDeclarations &arrays, SgBasicBlock *block,
    SgScopeStatement *scope) {
  if (!arrays.uses_heap_storage) {
    return;
  }

  ROSE_ASSERT(block != nullptr);
  ROSE_ASSERT(scope != nullptr);

  const SgVariableDeclaration *declarations[] = {
      arrays.arg_types_decl, arrays.arg_sizes_decl, arrays.args_decl,
      arrays.args_base_decl};
  for (const SgVariableDeclaration *decl : declarations) {
    ROSE_ASSERT(decl != nullptr);
    block->append_statement(buildFunctionCallStmt(
        "free", buildVoidType(),
        buildExprListExp(buildCastExp(
            buildVarRefExp(const_cast<SgVariableDeclaration *>(decl)),
            buildPointerType(buildVoidType()))),
        scope));
  }
}

static SgVariableDeclaration *buildTargetKernelArgsDeclaration(
    SgGlobal *global_scope, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl, SgVariableDeclaration *args_base,
    SgVariableDeclaration *args, SgVariableDeclaration *arg_sizes,
    SgVariableDeclaration *arg_types, SgVariableDeclaration *num_blocks_decl,
    SgVariableDeclaration *threads_per_block_decl, SgExpression *tripcount);

// Translate a parallel region under "omp target"
/*

 call customized outlining, the generateTask() for omp task or regular omp
 parallel is not compatible since we want to use the classic outlining support:
 each variable is passed as a separate parameter.

 We also use the revised generateFunc() to explicitly specify pass by original
 type vs. pass using pointer type

 */
void transOmpTargetSpmd(SgNode *node, SgExpression *omp_num_teams,
                        SgExpression *omp_num_threads,
                        SgExpression *kernel_launch_bounds) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // device expression
  SgExpression *device_expression = NULL;
  device_expression =
      getClauseExpression(target, VariantVector(V_SgOmpDeviceClause));
  // If not found, use the default ID 0
  if (device_expression == NULL)
    device_expression = buildIntVal(0);

  // Now we need to ensure that "omp target " has a basic block as its body
  // so we can insert declarations into an inner block, instead of colliding
  // declarations within the scope of "omp target" This is important since we
  // often have consecutive "omp target" regions within one big scope We cannot
  // just insert things into that big scope.
  SgBasicBlock *omp_target_stmt_body_block =
      ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(isSgBasicBlock(target->get_body()));

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner.
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  // 1/15/2009, Liao, also handle the last #endif, which is attached inside of
  // the target
  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  //-----------------------------------------------------------------
  // step 1: generated an outlined function and make it a CUDA function
  SgOmpClauseBodyStatement *target_parallel_stmt =
      isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target_parallel_stmt);
  // Prepare the outliner
  Outliner::enable_classic = true;
  //    Outliner::useParameterWrapper = false; //TODO: better handling of the
  //    dependence among flags
  SgBasicBlock *body_block = Outliner::preprocess(body);
  // translator OpenMP 3.0 and earlier variables.
  transOmpVariables(target, body_block);

  ASTtools::VarSymSet_t all_syms; // all generated or remaining variables to be
                                  // passed to the outliner
  // This addressOf_syms does not apply to CUDA kernel generation: since we
  // cannot use pass-by-reference for CUDA kernel. If we want to copy back
  // value, we have to use memory copy  since they are in two different memory
  // spaces.
  ASTtools::VarSymSet_t
      addressOf_syms; // generated or remaining variables should be passed by
                      // using their addresses

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  all_syms = transOmpMapVariables(
      target, map_variable_list, map_variable_base_list, map_variable_size_list,
      map_variable_type_list, &offload_ctx,
      &dynamic_map_entries); //, addressOf_syms);

  ASTtools::VarSymSet_t
      per_block_reduction_syms; // translation generated per block reduction
                                // symbols with name like _dev_per_block within
                                // the enclosed for loop

  // collect possible per block reduction variables introduced by
  // transOmpTargetLoop() we rely on the pattern of such variables:
  // _dev_per_block_* these variables are arrays already, we pass them by their
  // original types, not addressOf types
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(body_block, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    SgName var_name = vRef->get_symbol()->get_name();
    string var_name_str = var_name.getString();
    if (var_name_str.find("_dev_per_block_", 0) == 0) {
      all_syms.insert(vRef->get_symbol());
      per_block_reduction_syms.insert(vRef->get_symbol());
    }
  }

  string func_name = Outliner::generateFuncName(target);
  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  // pass all the parameters by reference
  for (std::set<const SgVariableSymbol *>::iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    if (!isPointerType((*iter)->get_type()) &&
        !isSgArrayType((*iter)->get_type()) &&
        offload_ctx.literal_target_param_syms.find(
            const_cast<SgVariableSymbol *>(*iter)) ==
            offload_ctx.literal_target_param_syms.end()) {
      addressOf_syms.insert(*iter);
    };
  };

  std::set<SgInitializedName *> restoreVars;
  Outliner::OutlinedLocalTypeTemplatePlan local_type_template_plan;
  SgFunctionDeclaration *result = Outliner::generateFunction(
      body_block, func_name + "kernel__", all_syms, addressOf_syms, restoreVars,
      NULL, g_scope, local_type_template_plan);
  if (!local_type_template_plan.entries.empty()) {
    fprintf(stderr, "REX_OMP_INVARIANT[target-local-type-call]: target kernel "
                    "requires explicit local type template arguments\n");
    ROSE_ABORT();
  }
  SgFunctionDeclaration *result_decl =
      isSgFunctionDeclaration(result->get_firstNondefiningDeclaration());
  ROSE_ASSERT(result_decl != NULL);
  lowerLiteralTargetKernelParameters(result,
                                     offload_ctx.literal_target_param_syms);
  retireConsumedTargetDevicePlaceholders(offload_ctx);
  recordTargetKernelLaunchBounds(result, kernel_launch_bounds);
  result_decl->get_functionModifier()
      .setCudaKernel(); // add __global__ modifier

  result->get_functionModifier().setCudaKernel();

  SgGlobal *glob_scope = getGlobalScope(target);
  if (glob_scope == NULL || glob_scope != g_scope ||
      result->get_parent() != glob_scope || result->get_scope() != glob_scope ||
      !glob_scope->statementExistsInScope(result)) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[target-kernel-publication]: function=%p "
            "parent=%p scope=%p target-global=%p construction-global=%p was "
            "not published exactly once by the outliner\n",
            static_cast<void *>(result),
            static_cast<void *>(result->get_parent()),
            static_cast<void *>(result->get_scope()),
            static_cast<void *>(glob_scope), static_cast<void *>(g_scope));
    ROSE_ABORT();
  }
  SgFunctionSymbol *func_symbol =
      glob_scope->lookup_function_symbol(result->get_name());
  ROSE_ASSERT(func_symbol != NULL);

  SgScopeStatement *p_scope =
      target->get_scope(); // the scope of "omp parallel" will be destroyed
                           // later, so we use scope of "omp target"
  ROSE_ASSERT(p_scope != NULL);

  // create the outlined driver for GPU offloading, which is empty at this point
  SgBasicBlock *outlined_driver_body = buildBasicBlock();

  // insert dim3 threadsPerBlock(xomp_get_maxThreadsPerBlock());
  // TODO: for 1-D mapping, int type is enough,  //TODO: a better interface
  // accepting expression as initializer!!
  SgType *launch_dimension_type = buildIntType();
  SgVariableDeclaration *threads_per_block_decl = buildVariableDeclaration(
      "_threads_per_block_", launch_dimension_type,
      buildAssignInitializer(omp_num_threads, launch_dimension_type), p_scope);
  outlined_driver_body->append_statement(threads_per_block_decl);

  SgVariableDeclaration *num_blocks_decl = buildVariableDeclaration(
      "_num_blocks_", launch_dimension_type,
      buildAssignInitializer(omp_num_teams, launch_dimension_type), p_scope);
  outlined_driver_body->append_statement(num_blocks_decl);

  // Now we have num_block declaration, we can insert the per block declaration
  // used for reduction variables
  SgExpression *shared_data = NULL; // shared data size expression for CUDA
                                    // kernel execution configuration
  for (std::vector<SgVariableDeclaration *>::iterator iter =
           offload_ctx.per_block_declarations.begin();
       iter != offload_ctx.per_block_declarations.end(); iter++) {
    SgVariableDeclaration *decl = *iter;
    insertStatementAfter(num_blocks_decl, decl);
    SgVariableSymbol *sym = getFirstVarSym(decl);
    SgPointerType *pointer_type = isSgPointerType(sym->get_type());
    ROSE_ASSERT(pointer_type != NULL);
    SgType *base_type = pointer_type->get_base_type();
    if (offload_ctx.per_block_declarations.size() > 1) {
      cerr << "Error. multiple reduction variables are not yet handled."
           << endl;
      ROSE_ABORT();
      // threadsPerBlock.x*sizeof(REAL)  //TODO: how to handle multiple shared
      // data blocks, each for a reduction variable??
    }
    SgType *size_type = SageInterface::requireTargetSizeType(g_scope);
    shared_data =
        buildMultiplyOp(buildVarRefExp(threads_per_block_decl),
                        buildSizeOfOp(base_type, size_type), size_type);
  }

  // func_symbol =
  // isSgFunctionSymbol(result->get_firstNondefiningDeclaration()->get_symbol_from_symbol_table
  // ());
  ROSE_ASSERT(func_symbol != NULL);
  // in the original function, we call the outlined driver and pass all the
  // required variables by reference prepare all the parameters for using LLVM
  // GPU offloading
  SgClassDeclaration *tgt_offload_entry = getOrBuildRuntimeStructDeclaration(
      getGlobalScope(target), "__tgt_offload_entry");

  kmpc_kernel_id_counter += 1;
  SgType *kernel_id_type = buildCharType();
  SgVariableDeclaration *outlined_kernel_id_decl = buildVariableDeclaration(
      func_name + "id__", kernel_id_type,
      buildAssignInitializer(buildIntVal(0), kernel_id_type), g_scope);

  // Use the OpenMP runtime's default device sentinel.
  SgType *device_id_type = buildLongLongType();
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", device_id_type,
      buildAssignInitializer(buildLongLongIntVal(-1), device_id_type), p_scope);
  outlined_driver_body->append_statement(device_id_decl);

  // define the entry point
  SgExprListExp *offload_entry_parameters = buildExprListExp(
      buildCastExp(
          buildExactAddressOfOp(buildVarRefExp(outlined_kernel_id_decl)),
          buildPointerType(buildVoidType())),
      buildStringVal(func_name + "kernel__"), buildIntVal(0), buildIntVal(0),
      buildIntVal(0));
  SgType *offload_entry_type = tgt_offload_entry->get_type();
  SgBracedInitializer *offload_entry_initilization =
      buildBracedInitializer(offload_entry_parameters, offload_entry_type);
  SgVariableDeclaration *offload_entry_decl = buildVariableDeclaration(
      func_name + "omp_offload_entry__", offload_entry_type,
      buildAssignInitializer(offload_entry_initilization, offload_entry_type),
      g_scope);
  offload_entry_decl->get_decl_item(SgName(func_name + "omp_offload_entry__"))
      ->set_gnu_attribute_section_name("omp_offloading_entries");

  prependGlobalDeclPreservingLeadingPreproc(offload_entry_decl, g_scope);
  prependGlobalDeclPreservingLeadingPreproc(outlined_kernel_id_decl, g_scope);

  SgType *host_pointer_type = buildPointerType(buildVoidType());
  SgVariableDeclaration *host_point_decl = buildVariableDeclaration(
      "__host_ptr", host_pointer_type,
      buildAssignInitializer(buildCastExp(buildExactAddressOfOp(buildVarRefExp(
                                              outlined_kernel_id_decl)),
                                          host_pointer_type),
                             host_pointer_type),
      p_scope);
  outlined_driver_body->append_statement(host_point_decl);

  SgVariableDeclaration *args_base_decl = nullptr;
  SgVariableDeclaration *args_decl = nullptr;
  SgVariableDeclaration *arg_sizes = nullptr;
  SgVariableDeclaration *arg_types = nullptr;
  SgVariableDeclaration *arg_number_decl = nullptr;
  RuntimeMapArgumentArrayDeclarations dynamic_arrays;
  if (!dynamic_map_entries.empty()) {
    dynamic_arrays = buildDynamicRuntimeMapArgumentArrays(
        outlined_driver_body, p_scope, map_variable_list,
        map_variable_base_list, map_variable_size_list, map_variable_type_list,
        dynamic_map_entries);
    args_base_decl = dynamic_arrays.args_base_decl;
    args_decl = dynamic_arrays.args_decl;
    arg_sizes = dynamic_arrays.arg_sizes_decl;
    arg_types = dynamic_arrays.arg_types_decl;
    arg_number_decl = dynamic_arrays.arg_number_decl;
  } else {
    materializeLiteralTargetArgExpressions(map_variable_list,
                                           map_variable_base_list,
                                           outlined_driver_body, p_scope);

    SgType *args_base_type = buildArrayType(buildPointerType(buildVoidType()));
    SgBracedInitializer *offloading_variables_base =
        buildBracedInitializer(map_variable_base_list, args_base_type);
    args_base_decl = buildVariableDeclaration(
        "__args_base", args_base_type,
        buildAssignInitializer(offloading_variables_base, args_base_type),
        p_scope);
    outlined_driver_body->append_statement(args_base_decl);

    SgType *args_type = buildArrayType(buildPointerType(buildVoidType()));
    SgBracedInitializer *offloading_variables =
        buildBracedInitializer(map_variable_list, args_type);
    args_decl = buildVariableDeclaration(
        "__args", args_type,
        buildAssignInitializer(offloading_variables, args_type), p_scope);
    outlined_driver_body->append_statement(args_decl);

    SgType *arg_sizes_type = buildArrayType(getRuntimeInt64Type(p_scope));
    SgBracedInitializer *map_variable_sizes =
        buildBracedInitializer(map_variable_size_list, arg_sizes_type);
    arg_sizes = buildVariableDeclaration(
        "__arg_sizes", arg_sizes_type,
        buildAssignInitializer(map_variable_sizes, arg_sizes_type), p_scope);
    outlined_driver_body->append_statement(arg_sizes);

    SgType *arg_types_type = buildArrayType(getRuntimeInt64Type(p_scope));
    SgBracedInitializer *map_variable_types =
        buildBracedInitializer(map_variable_type_list, arg_types_type);
    arg_types = buildVariableDeclaration(
        "__arg_types", arg_types_type,
        buildAssignInitializer(map_variable_types, arg_types_type), p_scope);
    outlined_driver_body->append_statement(arg_types);

    int kernel_arg_num = map_variable_base_list->get_expressions().size();
    SgType *arg_number_type = buildIntType();
    arg_number_decl = buildVariableDeclaration(
        "__arg_num", arg_number_type,
        buildAssignInitializer(buildIntVal(kernel_arg_num), arg_number_type),
        p_scope);
    outlined_driver_body->append_statement(arg_number_decl);
  }

  SgVariableDeclaration *kernel_args_decl = buildTargetKernelArgsDeclaration(
      g_scope, p_scope, arg_number_decl, args_base_decl, args_decl, arg_sizes,
      arg_types, num_blocks_decl, threads_per_block_decl, NULL);
  outlined_driver_body->append_statement(kernel_args_decl);

  // call __tgt_target_kernel to execute the CUDA kernel
  SgVariableSymbol *kernel_args_sym = getFirstVarSym(kernel_args_decl);
  ROSE_ASSERT(kernel_args_sym != NULL);
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(num_blocks_decl),
      buildVarRefExp(threads_per_block_decl), buildVarRefExp(host_point_decl),
      buildExactAddressOfOp(buildVarRefExp(kernel_args_sym)));
  string func_offloading_name = "__tgt_target_kernel";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildIntType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  outlined_driver_body->append_statement(func_offloading_stmt);

  appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays,
                                              outlined_driver_body, p_scope);

  SageInterface::fixStatement(outlined_driver_body, p_scope);
  //------------now remove omp parallel since everything within it has been
  // outlined to a function
  replaceStatement(target, outlined_driver_body, true);
  attachComment(threads_per_block_decl, "Launch CUDA kernel ...",
                PreprocessingInfo::C_StyleComment);

  // Restore preprocessing info attached to the original directive.
  // Outlining/statement replacement can otherwise drop conditional guards that
  // the Clang frontend preserves via attached PreprocessingInfo (e.g., skipped
  // #ifdef regions).
  if (!save_buf_inside.empty()) {
    for (PreprocessingInfo *info : save_buf_inside) {
      if (info != nullptr) {
        info->setRelativePosition(PreprocessingInfo::after);
      }
    }
    SageInterface::pastePreprocessingInfo(
        outlined_driver_body, PreprocessingInfo::after, save_buf_inside);
  }
  SageInterface::pastePreprocessingInfo(outlined_driver_body,
                                        PreprocessingInfo::before, save_buf1);
  SageInterface::pastePreprocessingInfo(outlined_driver_body,
                                        PreprocessingInfo::after, save_buf2);

  recordTargetOutlinedFunction(isSgFunctionDeclaration(result),
                               enclosing_function);
  retireConsumedOmpDirective(target);
}

static SgExpression *buildKernelArgNullPtrExpr() {
  return buildCastExp(buildIntVal(0),
                      buildPointerType(buildPointerType(buildVoidType())));
}

static SgExpression *buildKernelLaunchDimInitializer(SgExpression *x_dim_expr) {
  SgType *dimension_type = buildArrayType(buildIntType(), buildIntVal(3));
  return buildAggregateInitializer(
      buildExprListExp(copyExpression(x_dim_expr), buildIntVal(1),
                       buildIntVal(1)),
      dimension_type,
      SgAggregateInitializer::e_aggregate_initializer_source_braced);
}

static SgVariableDeclaration *buildTargetKernelArgsDeclaration(
    SgGlobal *global_scope, SgScopeStatement *scope,
    SgVariableDeclaration *arg_number_decl, SgVariableDeclaration *args_base,
    SgVariableDeclaration *args, SgVariableDeclaration *arg_sizes,
    SgVariableDeclaration *arg_types, SgVariableDeclaration *num_blocks_decl,
    SgVariableDeclaration *threads_per_block_decl, SgExpression *tripcount) {
  ROSE_ASSERT(global_scope != NULL);
  ROSE_ASSERT(scope != NULL);
  ROSE_ASSERT(arg_number_decl != NULL);
  ROSE_ASSERT(args_base != NULL);
  ROSE_ASSERT(args != NULL);
  ROSE_ASSERT(arg_sizes != NULL);
  ROSE_ASSERT(arg_types != NULL);
  ROSE_ASSERT(num_blocks_decl != NULL);
  ROSE_ASSERT(threads_per_block_decl != NULL);

  SgClassDeclaration *kernel_args_decl = getOrBuildRuntimeStructDeclaration(
      global_scope, "__tgt_kernel_arguments");
  ROSE_ASSERT(kernel_args_decl != NULL);

  SgType *int64_type = getRuntimeInt64Type(scope);
  SgExpression *tripcount_expr =
      tripcount != NULL ? buildCastExp(copyExpression(tripcount), int64_type)
                        : buildCastExp(buildLongLongIntVal(0), int64_type);

  std::vector<SgExpression *> kernel_args_exprs;
  kernel_args_exprs.push_back(buildIntVal(3));
  kernel_args_exprs.push_back(buildVarRefExp(arg_number_decl));
  kernel_args_exprs.push_back(buildVarRefExp(args_base));
  kernel_args_exprs.push_back(buildVarRefExp(args));
  kernel_args_exprs.push_back(buildVarRefExp(arg_sizes));
  kernel_args_exprs.push_back(buildVarRefExp(arg_types));
  kernel_args_exprs.push_back(buildKernelArgNullPtrExpr());
  kernel_args_exprs.push_back(buildKernelArgNullPtrExpr());
  kernel_args_exprs.push_back(tripcount_expr);
  kernel_args_exprs.push_back(buildLongLongIntVal(0));
  kernel_args_exprs.push_back(
      buildKernelLaunchDimInitializer(buildVarRefExp(num_blocks_decl)));
  kernel_args_exprs.push_back(
      buildKernelLaunchDimInitializer(buildVarRefExp(threads_per_block_decl)));
  kernel_args_exprs.push_back(buildIntVal(0));

  SgType *kernel_args_type = kernel_args_decl->get_type();
  SgBracedInitializer *kernel_args_init = buildBracedInitializer(
      buildExprListExp(kernel_args_exprs), kernel_args_type);

  return buildVariableDeclaration(
      "__kernel_args", kernel_args_type,
      buildAssignInitializer(kernel_args_init, kernel_args_type), scope);
}

struct TargetLoopLoweringInfo {
  SgInitializedName *orig_index = nullptr;
  SgExpression *orig_lower = nullptr;
  SgExpression *orig_upper = nullptr;
  SgExpression *orig_stride = nullptr;
  bool is_incremental = true;
  bool is_inclusive_bound = true;
};

static SgExpression *
buildTargetLoopTripCountExpr(const TargetLoopLoweringInfo &info) {
  ROSE_ASSERT(info.orig_index != nullptr);
  SgType *loop_control_type = info.orig_index->get_type();
  ROSE_ASSERT(loop_control_type != nullptr);
  SgExpression *distance = nullptr;
  if (info.is_incremental) {
    distance = buildSubtractOp(deepCopy(info.orig_upper),
                               deepCopy(info.orig_lower), loop_control_type);
  } else {
    distance = buildSubtractOp(deepCopy(info.orig_lower),
                               deepCopy(info.orig_upper), loop_control_type);
  }
  if (info.is_inclusive_bound) {
    distance = buildAddOp(distance, buildIntVal(1), loop_control_type);
  }
  return distance;
}

enum class CudaBuiltinDimensionVariable {
  block_dim,
  block_index,
  grid_dim,
  thread_index
};

static bool
hasExactCudaRuntimeDeclarationProvenance(const SgLocatedNode *node) {
  if (node == nullptr) {
    return false;
  }

  const Sg_File_Info *start = node->get_startOfConstruct();
  const Sg_File_Info *end = node->get_endOfConstruct();
  auto has_exact_runtime_position =
      [node](const Sg_File_Info *position) -> bool {
    return SageInterface::hasExactSemanticFrontendSourcePosition(node,
                                                                 position);
  };
  return node->get_file_info() == start && has_exact_runtime_position(start) &&
         has_exact_runtime_position(end);
}

static SgClassDeclaration *
requireExactCudaBuiltinDimensionType(SgGlobal *global_scope,
                                     SgClassSymbol *symbol) {
  static const SgName type_name("__rex_cuda_builtin_dimension_type");
  SgClassDeclaration *canonical =
      symbol != nullptr ? symbol->get_declaration() : nullptr;
  SgClassDeclaration *defining =
      canonical != nullptr
          ? isSgClassDeclaration(canonical->get_definingDeclaration())
          : nullptr;
  SgClassDefinition *definition =
      defining != nullptr ? defining->get_definition() : nullptr;
  SgAuxiliaryDeclarationList *owner =
      global_scope != nullptr ? global_scope->get_auxiliary_declarations()
                              : nullptr;
  const SgDeclarationStatementPtrList *auxiliary_declarations =
      owner != nullptr ? &owner->get_declarations() : nullptr;
  const size_t canonical_edges =
      auxiliary_declarations != nullptr
          ? static_cast<size_t>(std::count(auxiliary_declarations->begin(),
                                           auxiliary_declarations->end(),
                                           canonical))
          : 0;
  const size_t defining_edges =
      auxiliary_declarations != nullptr
          ? static_cast<size_t>(std::count(auxiliary_declarations->begin(),
                                           auxiliary_declarations->end(),
                                           defining))
          : 0;
  SgClassType *type = canonical != nullptr ? canonical->get_type() : nullptr;
  SgVariableSymbol *x_symbol =
      definition != nullptr ? definition->lookup_variable_symbol("x") : nullptr;
  SgInitializedName *x_name =
      x_symbol != nullptr ? x_symbol->get_declaration() : nullptr;
  SgVariableDeclaration *x_declaration =
      x_name != nullptr ? isSgVariableDeclaration(x_name->get_declaration())
                        : nullptr;
  const SgDeclarationStatementPtrList *members =
      definition != nullptr ? &definition->get_members() : nullptr;
  const size_t x_edges =
      members != nullptr ? static_cast<size_t>(std::count(
                               members->begin(), members->end(), x_declaration))
                         : 0;
  const SgAccessModifier *x_access =
      x_declaration != nullptr
          ? &x_declaration->get_declarationModifier().get_accessModifier()
          : nullptr;

  if (global_scope == nullptr || symbol == nullptr || canonical == nullptr ||
      defining == nullptr || definition == nullptr || owner == nullptr ||
      owner->get_parent() != global_scope ||
      global_scope->get_auxiliary_declarations() != owner ||
      global_scope->get_symbol_table()->find_class(type_name) != symbol ||
      global_scope->find_symbol_from_declaration(canonical) != symbol ||
      symbol->get_declaration() != canonical ||
      symbol->get_parent() != global_scope->get_symbol_table() ||
      !global_scope->get_symbol_table()->exists(symbol) ||
      canonical->get_name() != type_name || defining->get_name() != type_name ||
      !canonical->get_isUnNamed() || !defining->get_isUnNamed() ||
      canonical->get_class_type() != SgClassDeclaration::e_struct ||
      defining->get_class_type() != SgClassDeclaration::e_struct ||
      canonical->get_scope() != global_scope ||
      defining->get_scope() != global_scope ||
      canonical->get_parent() != owner || defining->get_parent() != owner ||
      canonical_edges != 1 || defining_edges != 1 ||
      global_scope->statementExistsInScope(canonical) ||
      global_scope->statementExistsInScope(defining) ||
      canonical->get_firstNondefiningDeclaration() != canonical ||
      canonical->get_definingDeclaration() != defining ||
      canonical->get_definition() != nullptr ||
      defining->get_firstNondefiningDeclaration() != canonical ||
      defining->get_definingDeclaration() != defining ||
      definition->get_declaration() != defining ||
      definition->get_parent() != defining || type == nullptr ||
      defining->get_type() != type || type->get_declaration() != canonical ||
      !hasExactCudaRuntimeDeclarationProvenance(canonical) ||
      !hasExactCudaRuntimeDeclarationProvenance(defining) ||
      !hasExactCudaRuntimeDeclarationProvenance(definition) ||
      members == nullptr || members->size() != 1 || x_edges != 1 ||
      x_symbol == nullptr || x_name == nullptr || x_declaration == nullptr ||
      x_symbol->get_declaration() != x_name ||
      x_symbol->get_parent() != definition->get_symbol_table() ||
      !definition->get_symbol_table()->exists(x_symbol) ||
      definition->find_symbol_from_declaration(x_name) != x_symbol ||
      x_name->get_name() != "x" || x_name->get_scope() != definition ||
      x_name->get_parent() != x_declaration ||
      x_name->get_declptr() != x_declaration ||
      x_declaration->get_parent() != definition ||
      x_declaration->get_variables().size() != 1 ||
      x_declaration->get_variables().front() != x_name ||
      x_name->get_type() != buildUnsignedIntType() || x_access == nullptr ||
      !x_access->isPublic() || x_access->get_is_explicit() ||
      !hasExactCudaRuntimeDeclarationProvenance(x_declaration) ||
      !hasExactCudaRuntimeDeclarationProvenance(x_name)) {
    fprintf(
        stderr,
        "REX_OMP_LOWERING_INVARIANT[cuda-builtin-dimension-type]: "
        "scope=%p symbol=%p canonical=%p defining=%p definition=%p owner=%p "
        "type=%p edges=%zu/%zu members=%zu x-symbol=%p x-name=%p "
        "x-declaration=%p x-edges=%zu does not identify one exact "
        "AST-owned CUDA runtime dimension type\n",
        static_cast<void *>(global_scope), static_cast<void *>(symbol),
        static_cast<void *>(canonical), static_cast<void *>(defining),
        static_cast<void *>(definition), static_cast<void *>(owner),
        static_cast<void *>(type), canonical_edges, defining_edges,
        members != nullptr ? members->size() : 0, static_cast<void *>(x_symbol),
        static_cast<void *>(x_name), static_cast<void *>(x_declaration),
        x_edges);
    ROSE_ABORT();
  }
  return defining;
}

static SgClassDefinition *getCudaBuiltinDimensionType(SgGlobal *global_scope) {
  ROSE_ASSERT(global_scope != nullptr);
  static const SgName type_name("__rex_cuda_builtin_dimension_type");

  SgClassSymbol *symbol =
      global_scope->get_symbol_table()->find_class(type_name);
  if (symbol != nullptr) {
    SgClassDeclaration *canonical = symbol->get_declaration();
    SgClassDeclaration *defining =
        canonical != nullptr
            ? isSgClassDeclaration(canonical->get_definingDeclaration())
            : nullptr;
    if (!hasExactCudaRuntimeDeclarationProvenance(canonical) ||
        !hasExactCudaRuntimeDeclarationProvenance(defining)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT"
              "[cuda-builtin-dimension-type-collision]: scope=%p reserved "
              "type='%s' symbol=%p canonical=%p defining=%p is not a "
              "compiler-generated CUDA runtime declaration\n",
              static_cast<void *>(global_scope), type_name.str(),
              static_cast<void *>(symbol), static_cast<void *>(canonical),
              static_cast<void *>(defining));
      ROSE_ABORT();
    }
  } else {
    SgClassDeclaration *defining = buildAnonymousStructDeclaration(
        declaration_ownership::semanticAuxiliary(), type_name, global_scope);
    ROSE_ASSERT(defining != nullptr);

    SgClassDefinition *definition = defining->get_definition();
    ROSE_ASSERT(definition != nullptr);
    SgVariableDeclaration *x_declaration = buildVariableDeclaration_nfi(
        "x", buildUnsignedIntType(), nullptr, definition);
    ROSE_ASSERT(x_declaration != nullptr);
    initializeSemanticVariableDeclarationSourceProvenance(x_declaration);
    SgAccessModifier &x_access =
        x_declaration->get_declarationModifier().get_accessModifier();
    x_access.setPublic();
    x_access.set_is_explicit(false);
    definition->append_member(x_declaration);
    symbol = global_scope->get_symbol_table()->find_class(type_name);
  }

  return requireExactCudaBuiltinDimensionType(global_scope, symbol)
      ->get_definition();
}

static const char *
getCudaBuiltinDimensionVariableName(CudaBuiltinDimensionVariable variable) {
  switch (variable) {
  case CudaBuiltinDimensionVariable::block_dim:
    return "blockDim";
  case CudaBuiltinDimensionVariable::block_index:
    return "blockIdx";
  case CudaBuiltinDimensionVariable::grid_dim:
    return "gridDim";
  case CudaBuiltinDimensionVariable::thread_index:
    return "threadIdx";
  }
  MLOG_ERROR_CXX("ompLowering") << "Unknown CUDA builtin dimension variable";
  ROSE_ABORT();
}

static SgVariableSymbol *
getCudaBuiltinDimensionVariableSymbol(CudaBuiltinDimensionVariable variable,
                                      SgGlobal *global_scope) {
  ROSE_ASSERT(global_scope != nullptr);
  SgClassDefinition *dimension_definition =
      getCudaBuiltinDimensionType(global_scope);
  SgClassDeclaration *dimension_declaration =
      dimension_definition->get_declaration();
  ROSE_ASSERT(dimension_declaration != nullptr);
  SgClassDeclaration *dimension_canonical = isSgClassDeclaration(
      dimension_declaration->get_firstNondefiningDeclaration());
  SgClassSymbol *dimension_symbol =
      dimension_canonical != nullptr
          ? isSgClassSymbol(
                global_scope->find_symbol_from_declaration(dimension_canonical))
          : nullptr;
  if (dimension_symbol == nullptr || dimension_canonical == nullptr ||
      dimension_symbol->get_declaration() != dimension_canonical ||
      dimension_symbol->get_type() == nullptr ||
      dimension_canonical->get_definingDeclaration() != dimension_declaration) {
    MLOG_ERROR_CXX("ompLowering")
        << "CUDA builtin dimension type has no exact canonical symbol";
    ROSE_ABORT();
  }

  const SgName variable_name(getCudaBuiltinDimensionVariableName(variable));
  SgVariableSymbol *variable_symbol =
      global_scope->get_symbol_table()->find_variable(variable_name);
  if (variable_symbol == nullptr) {
    SgVariableDeclaration *variable_declaration =
        buildSemanticAuxiliaryVariableDeclaration(
            variable_name, buildConstType(dimension_symbol->get_type()),
            nullptr, global_scope);
    ROSE_ASSERT(variable_declaration != nullptr);
    variable_symbol = getFirstVarSym(variable_declaration);
  }

  if (variable_symbol == nullptr ||
      variable_symbol->get_declaration() == nullptr) {
    MLOG_ERROR_CXX("ompLowering")
        << "CUDA builtin '" << variable_name.getString()
        << "' has no variable declaration";
    ROSE_ABORT();
  }

  SgInitializedName *variable_name_declaration =
      variable_symbol->get_declaration();
  SgVariableDeclaration *variable_declaration =
      isSgVariableDeclaration(variable_name_declaration->get_declaration());
  SgAuxiliaryDeclarationList *variable_owner =
      variable_declaration != nullptr
          ? isSgAuxiliaryDeclarationList(variable_declaration->get_parent())
          : nullptr;
  SgModifierType *const_type =
      isSgModifierType(variable_name_declaration->get_type());
  SgClassType *class_type = const_type != nullptr
                                ? isSgClassType(const_type->get_base_type())
                                : nullptr;
  const SgTypeModifier *type_modifier =
      const_type != nullptr ? &const_type->get_typeModifier() : nullptr;
  if (variable_declaration == nullptr || variable_owner == nullptr ||
      variable_owner->get_parent() != global_scope ||
      global_scope->get_auxiliary_declarations() != variable_owner ||
      std::count(variable_owner->get_declarations().begin(),
                 variable_owner->get_declarations().end(),
                 variable_declaration) != 1 ||
      global_scope->statementExistsInScope(variable_declaration) ||
      variable_declaration->get_variables().size() != 1 ||
      variable_declaration->get_variables().front() !=
          variable_name_declaration ||
      variable_name_declaration->get_parent() != variable_declaration ||
      variable_name_declaration->get_declptr() != variable_declaration ||
      variable_name_declaration->get_scope() != global_scope ||
      variable_symbol->get_symbol_basis() != variable_name_declaration ||
      variable_symbol->get_parent() != global_scope->get_symbol_table() ||
      !global_scope->get_symbol_table()->exists(variable_symbol) ||
      global_scope->find_symbol_from_declaration(variable_name_declaration) !=
          variable_symbol ||
      global_scope->get_symbol_table()->find_variable(variable_name) !=
          variable_symbol ||
      !hasExactCudaRuntimeDeclarationProvenance(variable_declaration) ||
      !hasExactCudaRuntimeDeclarationProvenance(variable_name_declaration) ||
      const_type == nullptr || type_modifier == nullptr ||
      !type_modifier->get_constVolatileModifier().isConst() ||
      type_modifier->get_constVolatileModifier().isVolatile() ||
      type_modifier->isRestrict() || class_type == nullptr ||
      class_type != dimension_symbol->get_type() ||
      class_type->get_declaration() == nullptr ||
      class_type->get_declaration()->get_definingDeclaration() !=
          dimension_declaration) {
    fprintf(
        stderr,
        "REX_OMP_LOWERING_INVARIANT[cuda-builtin-dimension-variable]: "
        "name=%s symbol=%p name-declaration=%p declaration=%p owner=%p "
        "raw-type=%p const-type=%p class-type=%p class-declaration=%p "
        "class-defining=%p expected-type=%p expected-defining=%p does not "
        "identify one exact AST-owned CUDA runtime variable\n",
        variable_name.getString().c_str(), static_cast<void *>(variable_symbol),
        static_cast<void *>(variable_name_declaration),
        static_cast<void *>(variable_declaration),
        static_cast<void *>(variable_owner),
        static_cast<void *>(variable_name_declaration->get_type()),
        static_cast<void *>(const_type), static_cast<void *>(class_type),
        static_cast<void *>(
            class_type != nullptr ? class_type->get_declaration() : nullptr),
        static_cast<void *>(
            class_type != nullptr && class_type->get_declaration() != nullptr
                ? class_type->get_declaration()->get_definingDeclaration()
                : nullptr),
        static_cast<void *>(dimension_symbol->get_type()),
        static_cast<void *>(dimension_declaration));
    ROSE_ABORT();
  }
  return variable_symbol;
}

static SgExpression *buildCudaDimXRef(CudaBuiltinDimensionVariable variable,
                                      SgScopeStatement *scope) {
  ROSE_ASSERT(scope != nullptr);
  SgGlobal *global_scope = getGlobalScope(scope);
  ROSE_ASSERT(global_scope != nullptr);
  SgVariableSymbol *variable_symbol =
      getCudaBuiltinDimensionVariableSymbol(variable, global_scope);
  SgClassDefinition *dimension_definition =
      getCudaBuiltinDimensionType(global_scope);

  SgVariableSymbol *x_symbol =
      dimension_definition->lookup_variable_symbol("x");
  ROSE_ASSERT(x_symbol != nullptr);
  ROSE_ASSERT(x_symbol->get_type() != nullptr);
  return buildDotExp(buildVarRefExp(variable_symbol), buildVarRefExp(x_symbol),
                     x_symbol->get_type());
}

static SgExpression *buildCudaGlobalThreadIdXExpr(SgScopeStatement *scope) {
  SgExpression *block_dimension =
      buildCudaDimXRef(CudaBuiltinDimensionVariable::block_dim, scope);
  SgType *result_type = block_dimension->get_type();
  ROSE_ASSERT(result_type != nullptr);
  return buildAddOp(
      buildMultiplyOp(
          block_dimension,
          buildCudaDimXRef(CudaBuiltinDimensionVariable::block_index, scope),
          result_type),
      buildCudaDimXRef(CudaBuiltinDimensionVariable::thread_index, scope),
      result_type);
}

static SgExpression *buildCudaGlobalThreadCountXExpr(SgScopeStatement *scope) {
  SgExpression *grid_dimension =
      buildCudaDimXRef(CudaBuiltinDimensionVariable::grid_dim, scope);
  SgType *result_type = grid_dimension->get_type();
  ROSE_ASSERT(result_type != nullptr);
  return buildMultiplyOp(
      grid_dimension,
      buildCudaDimXRef(CudaBuiltinDimensionVariable::block_dim, scope),
      result_type);
}

static TargetLoopLoweringInfo
analyzeTargetLoopForGpu(SgForStatement *for_loop) {
  ROSE_ASSERT(for_loop != NULL);

  // In target-offloading outlined kernels, loop indices can appear as pointer
  // dereferences (e.g., *ip__). Rewrite them to local scalar indices first so
  // canonical-loop analysis and normalization can proceed.
  rewritePointerBasedForIndices(for_loop);

  // For the init statement: for (int i=0;... ) becomes int i; for (i=0;..)
  // For test expression: i<x is normalized to i<= (x-1) and i>x is normalized
  // to i>= (x+1). For increment expression: i++ is normalized to i+=1 and
  // i-- is normalized to i+=-1.
  SageInterface::forLoopNormalization(for_loop);

  TargetLoopLoweringInfo info;
  bool is_canonical = isCanonicalForLoop(
      for_loop, &info.orig_index, &info.orig_lower, &info.orig_upper,
      &info.orig_stride, NULL, &info.is_incremental);
  ROSE_ASSERT(is_canonical == true);
  info.is_inclusive_bound = true;
  return info;
}

static TargetLoopLoweringInfo
analyzeTargetLoopForGpuReadOnly(SgForStatement *for_loop) {
  if (for_loop == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-gpu-loop-analysis]: loop is "
            "null\n");
    ROSE_ABORT();
  }

  TargetLoopLoweringInfo info;
  if (!SageInterface::isCanonicalForLoop(
          for_loop, &info.orig_index, &info.orig_lower, &info.orig_upper,
          &info.orig_stride, nullptr, &info.is_incremental,
          &info.is_inclusive_bound)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-gpu-loop-analysis]: loop=%p "
            "is not one exact canonical C/C++ loop\n",
            static_cast<void *>(for_loop));
    ROSE_ABORT();
  }
  return info;
}

static bool expressionDependsOnVarsDeclaredInside(SgExpression *expr,
                                                  SgNode *region_root) {
  if (expr == NULL || region_root == NULL) {
    return false;
  }

  Rose_STL_Container<SgNode *> refs =
      NodeQuery::querySubTree(expr, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::const_iterator it = refs.begin();
       it != refs.end(); ++it) {
    SgVarRefExp *ref = isSgVarRefExp(*it);
    if (ref == NULL || ref->get_symbol() == NULL) {
      continue;
    }
    SgInitializedName *decl = ref->get_symbol()->get_declaration();
    if (decl != NULL && isAncestor(region_root, decl)) {
      return true;
    }
  }
  return false;
}

static bool canUseDirectTargetLoopFastPath(const TargetLoopLoweringInfo &info) {
  return info.orig_index != nullptr && info.orig_lower != nullptr &&
         info.orig_upper != nullptr && info.orig_stride != nullptr;
}

static SgVariableDeclaration *
findHoistedTargetLoopIndexDeclaration(SgForStatement *for_loop,
                                      const TargetLoopLoweringInfo &info) {
  if (for_loop == nullptr || info.orig_index == nullptr) {
    return nullptr;
  }

  SgVariableDeclaration *index_decl =
      isSgVariableDeclaration(info.orig_index->get_declaration());
  if (index_decl == nullptr) {
    return nullptr;
  }

  // forLoopNormalization() and rewritePointerBasedForIndices() both hoist
  // the loop index declaration to the statement immediately preceding the
  // transformed loop. Only move that tightly-coupled declaration.
  if (SageInterface::getPreviousStatement(for_loop, false) != index_decl) {
    return nullptr;
  }

  const SgInitializedNamePtrList &decl_vars = index_decl->get_variables();
  if (decl_vars.size() != 1 || decl_vars.front() != info.orig_index) {
    return nullptr;
  }

  return index_decl;
}

static void
lowerTargetLoopDirectGridStride(SgForStatement *for_loop, SgBasicBlock *bb1,
                                const TargetLoopLoweringInfo &info) {
  ROSE_ASSERT(info.orig_index != nullptr);
  SgType *loop_control_type = info.orig_index->get_type();
  ROSE_ASSERT(loop_control_type != nullptr);
  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildCudaGlobalThreadCountXExpr(bb1), buildIntType());
  SgVariableDeclaration *dev_thread_num_decl = buildVariableDeclaration(
      "_dev_thread_num", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_num_decl, bb1);
  SgVariableSymbol *dev_thread_num_symbol = getFirstVarSym(dev_thread_num_decl);
  ROSE_ASSERT(dev_thread_num_symbol != NULL);

  init_idx =
      buildAssignInitializer(buildCudaGlobalThreadIdXExpr(bb1), buildIntType());
  SgVariableDeclaration *dev_thread_id_decl =
      buildVariableDeclaration("_dev_thread_id", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_id_decl, bb1);
  SgVariableSymbol *dev_thread_id_symbol = getFirstVarSym(dev_thread_id_decl);
  ROSE_ASSERT(dev_thread_id_symbol != NULL);

  setLoopLowerBound(
      for_loop,
      buildAddOp(deepCopy(info.orig_lower),
                 buildMultiplyOp(buildVarRefExp(dev_thread_id_symbol),
                                 deepCopy(info.orig_stride), loop_control_type),
                 loop_control_type));
  setLoopUpperBound(for_loop, deepCopy(info.orig_upper));
  setLoopStride(for_loop,
                buildMultiplyOp(buildVarRefExp(dev_thread_num_symbol),
                                deepCopy(info.orig_stride), loop_control_type));

  appendStatement(for_loop, bb1);

  SgInitializedName *outer_index = getLoopIndexVariable(for_loop);
  SgVariableSymbol *outer_index_sym =
      outer_index != nullptr
          ? isSgVariableSymbol(outer_index->get_symbol_from_symbol_table())
          : nullptr;
  scalarizeDirectGridStrideOuterIndexAccesses(for_loop, outer_index_sym);
  hoistReadOnlyInvariantAggregateRefsBeforeLoop(for_loop);
  hoistReadOnlyInvariantFieldAccessesBeforeLoop(for_loop);
  rewriteReadOnlyDeviceLoadsWithLdg(for_loop);
}

static void lowerTargetLoopRoundRobin(SgForStatement *for_loop,
                                      SgBasicBlock *bb1,
                                      const TargetLoopLoweringInfo &info) {
  SgVariableDeclaration *dev_lower_decl =
      buildVariableDeclaration("_dev_lower", buildIntType(), NULL, bb1);
  appendStatement(dev_lower_decl, bb1);
  SgVariableDeclaration *dev_upper_decl =
      buildVariableDeclaration("_dev_upper", buildIntType(), NULL, bb1);
  appendStatement(dev_upper_decl, bb1);
  SgVariableDeclaration *dev_loop_chunk_size_decl = buildVariableDeclaration(
      "_dev_loop_chunk_size", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_chunk_size_decl, bb1);
  SgVariableDeclaration *dev_loop_sched_index_decl = buildVariableDeclaration(
      "_dev_loop_sched_index", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_sched_index_decl, bb1);
  SgVariableDeclaration *dev_loop_stride_decl =
      buildVariableDeclaration("_dev_loop_stride", buildIntType(), NULL, bb1);
  appendStatement(dev_loop_stride_decl, bb1);

  SgAssignInitializer *init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getCUDABlockThreadCount"), buildIntType(),
                           buildExprListExp(buildIntVal(1)), bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_num_decl = buildVariableDeclaration(
      "_dev_thread_num", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_num_decl, bb1);
  SgVariableSymbol *dev_thread_num_symbol = getFirstVarSym(dev_thread_num_decl);
  ROSE_ASSERT(dev_thread_num_symbol != NULL);

  init_idx = buildAssignInitializer(
      buildFunctionCallExp(SgName("getLoopIndexFromCUDAVariables"),
                           buildIntType(), buildExprListExp(buildIntVal(1)),
                           bb1),
      buildIntType());
  SgVariableDeclaration *dev_thread_id_decl =
      buildVariableDeclaration("_dev_thread_id", buildIntType(), init_idx, bb1);
  appendStatement(dev_thread_id_decl, bb1);
  SgVariableSymbol *dev_thread_id_symbol = getFirstVarSym(dev_thread_id_decl);
  ROSE_ASSERT(dev_thread_id_symbol != NULL);

  SgExprListExp *parameters = buildExprListExp(
      copyExpression(info.orig_lower), copyExpression(info.orig_upper),
      copyExpression(info.orig_stride), buildIntVal(1),
      buildVarRefExp(dev_thread_num_symbol),
      buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_chunk_size_decl))));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_sched_index_decl))));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_loop_stride_decl))));
  SgStatement *call_stmt = buildFunctionCallStmt(
      "XOMP_static_sched_init", buildVoidType(), parameters, bb1);
  appendStatement(call_stmt, bb1);

  parameters = buildExprListExp(
      buildExactAddressOfOp(
          buildVarRefExp(getFirstVarSym(dev_loop_sched_index_decl))),
      copyExpression(info.orig_upper), copyExpression(info.orig_stride),
      buildVarRefExp(getFirstVarSym(dev_loop_stride_decl)),
      buildVarRefExp(getFirstVarSym(dev_loop_chunk_size_decl)));
  appendExpression(parameters, buildVarRefExp(dev_thread_num_symbol));
  appendExpression(parameters, buildVarRefExp(dev_thread_id_symbol));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_lower_decl))));
  appendExpression(parameters, buildExactAddressOfOp(buildVarRefExp(
                                   getFirstVarSym(dev_upper_decl))));
  SgExpression *func_call_exp = buildFunctionCallExp(
      "XOMP_static_sched_next", buildBoolType(), parameters, bb1);

  SgWhileStmt *w_stmt = buildWhileStmt(func_call_exp, for_loop);
  appendStatement(w_stmt, bb1);

  setLoopLowerBound(for_loop, buildVarRefExp(getFirstVarSym(dev_lower_decl)));
  setLoopUpperBound(for_loop, buildVarRefExp(getFirstVarSym(dev_upper_decl)));
}

static void
moveExactTargetLoopIndexDeclaration(SgVariableDeclaration *declaration,
                                    SgBasicBlock *target_scope) {
  if (declaration == NULL || target_scope == NULL ||
      declaration->get_variables().size() != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop-index-move]: "
            "declaration=%p target=%p is not one exact variable move\n",
            static_cast<void *>(declaration),
            static_cast<void *>(target_scope));
    ROSE_ABORT();
  }

  SgInitializedName *name = declaration->get_variables().front();
  SgScopeStatement *source_scope = name != NULL ? name->get_scope() : NULL;
  SgVariableSymbol *symbol =
      source_scope != NULL
          ? isSgVariableSymbol(source_scope->find_symbol_from_declaration(name))
          : NULL;
  if (name == NULL || source_scope == NULL || source_scope == target_scope ||
      declaration->get_parent() != source_scope ||
      !source_scope->statementExistsInScope(declaration) || symbol == NULL ||
      symbol->get_declaration() != name ||
      symbol->get_parent() != source_scope->get_symbol_table() ||
      !source_scope->get_symbol_table()->exists(symbol) ||
      target_scope->get_symbol_table()->exists(name->get_name())) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop-index-move]: "
            "declaration=%p source=%p target=%p symbol=%p has no isolated "
            "move identity\n",
            static_cast<void *>(declaration), static_cast<void *>(source_scope),
            static_cast<void *>(target_scope), static_cast<void *>(symbol));
    ROSE_ABORT();
  }

  SageInterface::removeStatement(declaration, false);
  if (declaration->get_parent() != NULL) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop-index-move]: "
            "declaration=%p remained structurally owned after detachment\n",
            static_cast<void *>(declaration));
    ROSE_ABORT();
  }
  source_scope->remove_symbol(symbol);
  if (source_scope->get_symbol_table()->exists(symbol)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop-index-move]: symbol=%p "
            "remained in source scope=%p\n",
            static_cast<void *>(symbol), static_cast<void *>(source_scope));
    ROSE_ABORT();
  }

  name->set_scope(target_scope);
  target_scope->insert_symbol(name->get_name(), symbol);
  appendStatement(declaration, target_scope);
  if (declaration->get_parent() != target_scope ||
      name->get_scope() != target_scope ||
      symbol->get_parent() != target_scope->get_symbol_table() ||
      !target_scope->get_symbol_table()->exists(symbol) ||
      target_scope->find_symbol_from_declaration(name) != symbol) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-loop-index-move]: "
            "declaration=%p symbol=%p was not published exactly in target=%p\n",
            static_cast<void *>(declaration), static_cast<void *>(symbol),
            static_cast<void *>(target_scope));
    ROSE_ABORT();
  }
}

// Transform the worksharing loop in a target spmd region
SgBasicBlock *transOmpTargetLoopBlock(SgNode *node,
                                      bool *used_direct_grid_stride,
                                      GpuOffloadLoweringContext *offload_ctx) {
  // step 0: Sanity check
  ROSE_ASSERT(node != NULL);
  ROSE_ASSERT(offload_ctx != NULL);
  (void)offload_ctx;
  SgForStatement *for_loop = isSgForStatement(node);
  ROSE_ASSERT(for_loop != NULL);

  TargetLoopLoweringInfo info = analyzeTargetLoopForGpu(for_loop);

  // TODO: Fortran support later on
  ROSE_ASSERT(for_loop != NULL);
  // SgBasicBlock* loop_body = ensureBasicBlockAsBodyOfFor (for_loop);

  // Step 2. Insert a basic block to replace SgOmpForStatement
  //  This newly introduced scope is used to hold loop variables ,etc
  SgVariableDeclaration *hoisted_index_decl =
      findHoistedTargetLoopIndexDeclaration(for_loop, info);
  SgBasicBlock *bb1 = SageBuilder::buildBasicBlock();
  replaceStatement(for_loop, bb1, true);
  if (hoisted_index_decl != NULL) {
    moveExactTargetLoopIndexDeclaration(hoisted_index_decl, bb1);
  }

  bool use_direct_grid_stride = canUseDirectTargetLoopFastPath(info);
  if (used_direct_grid_stride != NULL) {
    *used_direct_grid_stride = use_direct_grid_stride;
  }
  if (use_direct_grid_stride) {
    lowerTargetLoopDirectGridStride(for_loop, bb1, info);
  } else {
    lowerTargetLoopRoundRobin(for_loop, bb1, info);
  }

  // handle private variables at this loop level, mostly loop index variables.
  // TODO: this is not very elegant since the outer most loop's loop variable is
  // still translated.
  return bb1;
}

namespace {
size_t requireExactCollapseFactor(SgOmpClauseBodyStatement *target) {
  if (target == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-preflight]: target is null\n");
    ROSE_ABORT();
  }
  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(target, V_SgOmpCollapseClause);
  if (clauses.size() != 1 || clauses.front() == nullptr ||
      clauses.front()->get_parent() != getOmpClauseList(target)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-preflight]: expected one "
            "exactly owned collapse clause, found %zu\n",
            clauses.size());
    ROSE_ABORT();
  }
  SgOmpCollapseClause *collapse = isSgOmpCollapseClause(clauses.front());
  SgExpression *expression =
      collapse != nullptr ? collapse->get_expression() : nullptr;
  if (expression == nullptr || expression->get_parent() != collapse) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-preflight]: clause has no "
            "exact expression edge\n");
    ROSE_ABORT();
  }
  const std::vector<SgNode *> expression_edges =
      collapse->get_traversalSuccessorContainer();
  if (std::count(expression_edges.begin(), expression_edges.end(),
                 expression) != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-preflight]: expression is "
            "not published on one exact structural edge\n");
    ROSE_ABORT();
  }
  const size_t factor =
      static_cast<size_t>(requireExactPositiveIntegralConstant(
          expression, static_cast<unsigned long long>(INT_MAX),
          "collapse-factor"));
  return factor;
}

size_t requireExactCollapsePreflight(SgOmpClauseBodyStatement *target,
                                     SgForStatement *outer_loop) {
  if (outer_loop == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-preflight]: C/C++ loop is "
            "null\n");
    ROSE_ABORT();
  }
  const size_t factor = requireExactCollapseFactor(target);
  const size_t available_loops =
      SageInterface::querySubTree<SgForStatement>(outer_loop, V_SgForStatement)
          .size();
  if (factor > available_loops) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-preflight]: factor=%zu "
            "exceeds the %zu loops in the exact associated subtree\n",
            factor, available_loops);
    ROSE_ABORT();
  }
  std::vector<size_t> identity_tiles(factor, 1);
  (void)SageInterface::requireCheckedLoopTilingPlan(outer_loop, identity_tiles,
                                                    "omp-collapse-preflight");
  return factor;
}
} // namespace

// transformation for combined directive
// omp target parallel for
// omp target teams distribute parallel for
void transOmpTargetSpmdWorksharing(SgNode *node, SgExpression *omp_num_teams,
                                   SgExpression *omp_num_threads,
                                   SgExpression *kernel_launch_bounds,
                                   bool has_explicit_num_teams,
                                   bool has_explicit_num_threads) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // Validate the original source association and target-loop contract before
  // allocating a default clause expression, wrapping/collapsing the body, or
  // cutting preprocessing information.
  SgStatement *source_body = target->get_body();
  SgForStatement *source_associated_loop = requireExactAssociatedForLoop(
      target, source_body, AssociatedLoopPathContract::Target,
      "target-spmd-associated-loop-preflight");
  (void)SageInterface::requireCheckedCanonicalLoopPlan(
      source_associated_loop, "target-spmd-canonical-loop-preflight");
  (void)analyzeTargetLoopForGpuReadOnly(source_associated_loop);
  if (hasClause(target, V_SgOmpCollapseClause))
    (void)requireExactCollapsePreflight(target, source_associated_loop);

  // device expression
  SgExpression *device_expression = NULL;
  device_expression =
      getClauseExpression(target, VariantVector(V_SgOmpDeviceClause));
  // If not found, use the default ID 0
  if (device_expression == NULL)
    device_expression = buildIntVal(0);

  // Now we need to ensure that "omp target " has a basic block as its body
  // so we can insert declarations into an inner block, instead of colliding
  // declarations within the scope of "omp target" This is important since we
  // often have consecutive "omp target" regions within one big scope We cannot
  // just insert things into that big scope.
  SgBasicBlock *omp_target_stmt_body_block =
      ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(isSgBasicBlock(target->get_body()));

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  if (hasClause(target, V_SgOmpCollapseClause))
    transOmpCollapse(target);

  SgForStatement *associated_for_loop = requireExactAssociatedForLoop(
      target, target->get_body(), AssociatedLoopPathContract::Target,
      "target-spmd-associated-loop");

  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner.
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  // 1/15/2009, Liao, also handle the last #endif, which is attached inside of
  // the target
  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  //-----------------------------------------------------------------
  // step 1: generated an outlined function and make it a CUDA function
  SgOmpClauseBodyStatement *target_parallel_stmt =
      isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target_parallel_stmt);
  (void)has_explicit_num_teams;

  // Prepare the outliner
  Outliner::enable_classic = true;
  // Outliner::useParameterWrapper = false; //TODO: better handling of the
  // dependence among flags
  SgBasicBlock *body_block = Outliner::preprocess(body);

  // The combined directive only has one code block and should only process omp
  // variables once
  transOmpVariablesWithContext(target, body_block, NULL, true, &offload_ctx);

  SgExpression *host_loop_iter_count_expr = NULL;
  int direct_launch_thread_cap = 0;
  {
    TargetLoopLoweringInfo host_loop_info =
        analyzeTargetLoopForGpuReadOnly(associated_for_loop);
    if (canUseDirectTargetLoopFastPath(host_loop_info)) {
      host_loop_iter_count_expr = buildTargetLoopTripCountExpr(host_loop_info);
      if (expressionDependsOnVarsDeclaredInside(host_loop_iter_count_expr,
                                                body_block)) {
        host_loop_iter_count_expr = NULL;
      }

      if (!has_explicit_num_threads) {
        const int nested_loop_depth =
            computeMaxNestedForDepth(associated_for_loop->get_loop_body());
        if (nested_loop_depth >= 2) {
          direct_launch_thread_cap = 128;
        } else if (nested_loop_depth >= 1) {
          direct_launch_thread_cap = 256;
        }
      }
    }
  }

  ASTtools::VarSymSet_t all_syms; // all generated or remaining variables to be
                                  // passed to the outliner
  // This addressOf_syms does not apply to CUDA kernel generation: since we
  // cannot use pass-by-reference for CUDA kernel. If we want to copy back
  // value, we have to use memory copy  since they are in two different memory
  // spaces.
  ASTtools::VarSymSet_t
      addressOf_syms; // generated or remaining variables should be passed by
                      // using their addresses

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  all_syms = transOmpMapVariables(
      target, map_variable_list, map_variable_base_list, map_variable_size_list,
      map_variable_type_list, &offload_ctx,
      &dynamic_map_entries); //, addressOf_syms);
  /*
  for (std::set<const SgVariableSymbol*>::iterator iter = all_syms.begin(); iter
  != all_syms.end(); iter++) { std::cout << "SPMD worksharing variable: " <<
  (*iter)->get_name() << "...\n";
  };
  */

  ASTtools::VarSymSet_t
      per_block_reduction_syms; // translation generated per block reduction
                                // symbols with name like _dev_per_block within
                                // the enclosed for loop

  // collect possible per block reduction variables introduced by
  // transOmpTargetLoop() we rely on the pattern of such variables:
  // _dev_per_block_* these variables are arrays already, we pass them by their
  // original types, not addressOf types
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(body_block, V_SgVarRefExp);
  for (Rose_STL_Container<SgNode *>::iterator i = nodeList.begin();
       i != nodeList.end(); i++) {
    SgVarRefExp *vRef = isSgVarRefExp((*i));
    SgName var_name = vRef->get_symbol()->get_name();
    string var_name_str = var_name.getString();
    if (var_name_str.find("__reduction_buffer_", 0) == 0) {
      all_syms.insert(vRef->get_symbol());
      per_block_reduction_syms.insert(vRef->get_symbol());
    }
  }

  string func_name = Outliner::generateFuncName(target);
  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  // pass all the parameters by reference
  for (std::set<const SgVariableSymbol *>::iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    if (!isPointerType((*iter)->get_type()) &&
        !isSgArrayType((*iter)->get_type()) &&
        offload_ctx.literal_target_param_syms.find(
            const_cast<SgVariableSymbol *>(*iter)) ==
            offload_ctx.literal_target_param_syms.end()) {
      addressOf_syms.insert(*iter);
    };
  };

  std::set<SgInitializedName *> restoreVars;
  Outliner::OutlinedLocalTypeTemplatePlan local_type_template_plan;
  SgFunctionDeclaration *result = Outliner::generateFunction(
      body_block, func_name + "kernel__", all_syms, addressOf_syms, restoreVars,
      NULL, g_scope, local_type_template_plan);
  if (!local_type_template_plan.entries.empty()) {
    fprintf(stderr, "REX_OMP_INVARIANT[target-local-type-call]: target kernel "
                    "requires explicit local type template arguments\n");
    ROSE_ABORT();
  }
  SgFunctionDeclaration *result_decl =
      isSgFunctionDeclaration(result->get_firstNondefiningDeclaration());
  ROSE_ASSERT(result_decl != NULL);
  lowerLiteralTargetKernelParameters(result,
                                     offload_ctx.literal_target_param_syms);
  retireConsumedTargetDevicePlaceholders(offload_ctx);
  recordTargetKernelLaunchBounds(result, kernel_launch_bounds);
  result_decl->get_functionModifier()
      .setCudaKernel(); // add __global__ modifier

  result->get_functionModifier().setCudaKernel();

  SgGlobal *glob_scope = getGlobalScope(target);
  if (glob_scope == NULL || glob_scope != g_scope ||
      result->get_parent() != glob_scope || result->get_scope() != glob_scope ||
      !glob_scope->statementExistsInScope(result)) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[target-kernel-publication]: function=%p "
            "parent=%p scope=%p target-global=%p construction-global=%p was "
            "not published exactly once by the outliner\n",
            static_cast<void *>(result),
            static_cast<void *>(result->get_parent()),
            static_cast<void *>(result->get_scope()),
            static_cast<void *>(glob_scope), static_cast<void *>(g_scope));
    ROSE_ABORT();
  }
  SgFunctionSymbol *func_symbol =
      glob_scope->lookup_function_symbol(result->get_name());
  ROSE_ASSERT(func_symbol != NULL);

  SgScopeStatement *p_scope =
      omp_target_stmt_body_block; // the scope of "omp parallel" will be
                                  // destroyed later, so we use scope of "omp
                                  // target"
  ROSE_ASSERT(p_scope != NULL);

  // The exact associated loop identity is preserved when the outliner moves the
  // body into the generated function; never rediscover it by descendant search.
  if (!isAncestor(result, associated_for_loop)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[target-spmd-associated-loop]: "
            "outlined function=%p does not own exact loop=%p\n",
            static_cast<void *>(result),
            static_cast<void *>(associated_for_loop));
    ROSE_ABORT();
  }
  transOmpTargetLoopBlock(associated_for_loop, NULL, &offload_ctx);

  // create the outlined driver for GPU offloading, which is empty at this point
  SgBasicBlock *outlined_driver_body = omp_target_stmt_body_block;

  // Use the OpenMP runtime's default device sentinel.
  SgType *runtime_counter_type = buildLongLongType();
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", runtime_counter_type,
      buildAssignInitializer(buildLongLongIntVal(-1), runtime_counter_type),
      p_scope);
  outlined_driver_body->append_statement(device_id_decl);

  SgVariableDeclaration *threads_per_block_decl = NULL;
  SgVariableDeclaration *num_blocks_decl = NULL;
  SgVariableDeclaration *tripcount_decl = NULL;
  // insert dim3 threadsPerBlock(xomp_get_maxThreadsPerBlock());
  // TODO: for 1-D mapping, int type is enough.
  SgType *launch_dimension_type = buildIntType();
  threads_per_block_decl = buildVariableDeclaration(
      "_threads_per_block_", launch_dimension_type,
      buildAssignInitializer(omp_num_threads, launch_dimension_type), p_scope);
  outlined_driver_body->append_statement(threads_per_block_decl);

  // dim3 numBlocks (xomp_get_max1DBlock(VEC_LEN));
  num_blocks_decl = buildVariableDeclaration(
      "_num_blocks_", launch_dimension_type,
      buildAssignInitializer(omp_num_teams, launch_dimension_type), p_scope);
  outlined_driver_body->append_statement(num_blocks_decl);

  if (host_loop_iter_count_expr != NULL) {
    tripcount_decl = buildVariableDeclaration(
        "__rex_tripcount", runtime_counter_type,
        buildAssignInitializer(copyExpression(host_loop_iter_count_expr),
                               runtime_counter_type),
        p_scope);
    outlined_driver_body->append_statement(tripcount_decl);

    if (!has_explicit_num_threads) {
      SgBasicBlock *cap_launch_body = buildBasicBlock();

      SgBasicBlock *cap_threads_body = buildBasicBlock();
      SgVariableDeclaration *launch_granularity_decl = buildVariableDeclaration(
          "__rex_launch_granularity", runtime_counter_type,
          buildAssignInitializer(buildLongLongIntVal(32), runtime_counter_type),
          cap_threads_body);
      cap_threads_body->append_statement(launch_granularity_decl);

      SgBasicBlock *use_block_granularity_body = buildBasicBlock();
      use_block_granularity_body->append_statement(buildAssignStatement(
          buildVarRefExp(launch_granularity_decl),
          buildCastExp(buildVarRefExp(threads_per_block_decl),
                       buildLongLongType())));
      cap_threads_body->append_statement(buildIfStmt(
          buildLessThanOp(buildCastExp(buildVarRefExp(threads_per_block_decl),
                                       buildLongLongType()),
                          buildLongLongIntVal(32), exactLogicalResultType()),
          use_block_granularity_body, NULL));

      SgExpression *rounded_threads_expr = buildMultiplyOp(
          buildDivideOp(buildSubtractOp(
                            buildAddOp(buildVarRefExp(tripcount_decl),
                                       buildVarRefExp(launch_granularity_decl),
                                       runtime_counter_type),
                            buildLongLongIntVal(1), runtime_counter_type),
                        buildVarRefExp(launch_granularity_decl),
                        runtime_counter_type),
          buildVarRefExp(launch_granularity_decl), runtime_counter_type);
      SgVariableDeclaration *rounded_threads_decl = buildVariableDeclaration(
          "__rex_rounded_threads", runtime_counter_type,
          buildAssignInitializer(rounded_threads_expr, runtime_counter_type),
          cap_threads_body);
      cap_threads_body->append_statement(rounded_threads_decl);

      SgBasicBlock *clamp_threads_body = buildBasicBlock();
      clamp_threads_body->append_statement(buildAssignStatement(
          buildVarRefExp(rounded_threads_decl),
          buildCastExp(buildVarRefExp(threads_per_block_decl),
                       buildLongLongType())));
      cap_threads_body->append_statement(
          buildIfStmt(buildGreaterThanOp(
                          buildVarRefExp(rounded_threads_decl),
                          buildCastExp(buildVarRefExp(threads_per_block_decl),
                                       buildLongLongType()),
                          exactLogicalResultType()),
                      clamp_threads_body, NULL));

      cap_threads_body->append_statement(buildAssignStatement(
          buildVarRefExp(threads_per_block_decl),
          buildCastExp(buildVarRefExp(rounded_threads_decl), buildIntType())));
      cap_launch_body->append_statement(buildIfStmt(
          buildGreaterThanOp(
              buildCastExp(buildVarRefExp(threads_per_block_decl),
                           buildLongLongType()),
              buildVarRefExp(tripcount_decl), exactLogicalResultType()),
          cap_threads_body, NULL));

      outlined_driver_body->append_statement(buildIfStmt(
          buildGreaterThanOp(buildVarRefExp(tripcount_decl),
                             buildLongLongIntVal(0), exactLogicalResultType()),
          cap_launch_body, NULL));
    }
  }

  if (!has_explicit_num_threads && direct_launch_thread_cap > 0) {
    SgBasicBlock *cap_direct_threads_body = buildBasicBlock();
    cap_direct_threads_body->append_statement(
        buildAssignStatement(buildVarRefExp(threads_per_block_decl),
                             buildIntVal(direct_launch_thread_cap)));
    outlined_driver_body->append_statement(
        buildIfStmt(buildGreaterThanOp(buildVarRefExp(threads_per_block_decl),
                                       buildIntVal(direct_launch_thread_cap),
                                       exactLogicalResultType()),
                    cap_direct_threads_body, NULL));
  }

  // Now we have num_block declaration, we can insert the per block declaration
  // used for reduction variables
  SgExpression *shared_data = NULL; // shared data size expression for CUDA
                                    // kernel execution configuration
  SgExprListExp *map_variable_list_suffix = buildExprListExp();
  SgExprListExp *map_variable_base_list_suffix = buildExprListExp();
  SgExprListExp *map_variable_size_list_suffix = buildExprListExp();
  SgExprListExp *map_variable_type_list_suffix = buildExprListExp();
  for (std::vector<SgVariableDeclaration *>::iterator iter =
           offload_ctx.per_block_declarations.begin();
       iter != offload_ctx.per_block_declarations.end(); iter++) {
    SgVariableDeclaration *decl = *iter;
    insertStatementAfter(num_blocks_decl, decl);
    SgVariableSymbol *sym = getFirstVarSym(decl);
    SgPointerType *pointer_type = isSgPointerType(sym->get_type());
    ROSE_ASSERT(pointer_type != NULL);
    SgType *base_type = pointer_type->get_base_type();
    if (offload_ctx.per_block_declarations.size() > 1) {
      cerr << "Error. multiple reduction variables are not yet handled."
           << endl;
      ROSE_ABORT();
      // threadsPerBlock.x*sizeof(REAL)  //TODO: how to handle multiple shared
      // data blocks, each for a reduction variable??
    }
    SgType *size_type = SageInterface::requireTargetSizeType(p_scope);
    shared_data =
        buildMultiplyOp(buildVarRefExp(threads_per_block_decl),
                        buildSizeOfOp(base_type, size_type), size_type);

    // insert reduction buffer array to variable mapping list
    string reduction_buffer_name = (sym->get_name()).getString();
    SgExprListExp *reduction_map_variable_list = dynamic_map_entries.empty()
                                                     ? map_variable_list
                                                     : map_variable_list_suffix;
    SgExprListExp *reduction_map_variable_base_list =
        dynamic_map_entries.empty() ? map_variable_base_list
                                    : map_variable_base_list_suffix;
    SgExprListExp *reduction_map_variable_size_list =
        dynamic_map_entries.empty() ? map_variable_size_list
                                    : map_variable_size_list_suffix;
    SgExprListExp *reduction_map_variable_type_list =
        dynamic_map_entries.empty() ? map_variable_type_list
                                    : map_variable_type_list_suffix;
    reduction_map_variable_list->append_expression(
        buildVarRefExp(reduction_buffer_name, p_scope));
    reduction_map_variable_base_list->append_expression(
        buildVarRefExp(reduction_buffer_name, p_scope));
    SgExpression *reduction_variable_size = buildCastExp(
        buildMultiplyOp(buildVarRefExp(num_blocks_decl),
                        buildSizeOfOp(base_type, size_type), size_type),
        buildLongLongType());
    reduction_map_variable_size_list->append_expression(
        reduction_variable_size);
    SgExpression *reduction_variable_value =
        buildIntVal(OMP_TGT_MAPTYPE_TARGET_PARAM | OMP_TGT_MAPTYPE_FROM);
    reduction_map_variable_type_list->append_expression(
        reduction_variable_value);
  }

  // generate the cuda kernel launch statement
  // e.g.  axpy_ompacc_cuda <<<numBlocks, threadsPerBlock>>>(dev_x,  dev_y,
  // VEC_LEN, a);

  // func_symbol =
  // isSgFunctionSymbol(result->get_firstNondefiningDeclaration()->get_symbol_from_symbol_table
  // ());
  ROSE_ASSERT(func_symbol != NULL);
  // in the original function, we call the outlined driver and pass all the
  // required variables by reference prepare all the parameters for using LLVM
  // GPU offloading
  SgClassDeclaration *tgt_offload_entry = getOrBuildRuntimeStructDeclaration(
      getGlobalScope(target), "__tgt_offload_entry");

  kmpc_kernel_id_counter += 1;
  SgType *kernel_id_type = buildCharType();
  SgVariableDeclaration *outlined_kernel_id_decl = buildVariableDeclaration(
      func_name + "id__", kernel_id_type,
      buildAssignInitializer(buildIntVal(0), kernel_id_type), g_scope);

  // define the entry point
  SgExprListExp *offload_entry_parameters = buildExprListExp(
      buildCastExp(
          buildExactAddressOfOp(buildVarRefExp(outlined_kernel_id_decl)),
          buildPointerType(buildVoidType())),
      buildStringVal(func_name + "kernel__"), buildIntVal(0), buildIntVal(0),
      buildIntVal(0));
  SgType *offload_entry_type = tgt_offload_entry->get_type();
  SgBracedInitializer *offload_entry_initilization =
      buildBracedInitializer(offload_entry_parameters, offload_entry_type);
  SgVariableDeclaration *offload_entry_decl = buildVariableDeclaration(
      func_name + "omp_offload_entry__", offload_entry_type,
      buildAssignInitializer(offload_entry_initilization, offload_entry_type),
      g_scope);
  offload_entry_decl->get_decl_item(SgName(func_name + "omp_offload_entry__"))
      ->set_gnu_attribute_section_name("omp_offloading_entries");

  prependGlobalDeclPreservingLeadingPreproc(offload_entry_decl, g_scope);
  prependGlobalDeclPreservingLeadingPreproc(outlined_kernel_id_decl, g_scope);

  SgType *host_pointer_type = buildPointerType(buildVoidType());
  SgVariableDeclaration *host_point_decl = buildVariableDeclaration(
      "__host_ptr", host_pointer_type,
      buildAssignInitializer(buildCastExp(buildExactAddressOfOp(buildVarRefExp(
                                              outlined_kernel_id_decl)),
                                          host_pointer_type),
                             host_pointer_type),
      p_scope);
  outlined_driver_body->append_statement(host_point_decl);

  SgVariableDeclaration *args_base_decl = nullptr;
  SgVariableDeclaration *args_decl = nullptr;
  SgVariableDeclaration *arg_sizes = nullptr;
  SgVariableDeclaration *arg_types = nullptr;
  SgVariableDeclaration *arg_number_decl = nullptr;
  RuntimeMapArgumentArrayDeclarations dynamic_arrays;
  if (!dynamic_map_entries.empty()) {
    dynamic_arrays = buildDynamicRuntimeMapArgumentArrays(
        outlined_driver_body, p_scope, map_variable_list,
        map_variable_base_list, map_variable_size_list, map_variable_type_list,
        dynamic_map_entries, map_variable_list_suffix,
        map_variable_base_list_suffix, map_variable_size_list_suffix,
        map_variable_type_list_suffix);
    args_base_decl = dynamic_arrays.args_base_decl;
    args_decl = dynamic_arrays.args_decl;
    arg_sizes = dynamic_arrays.arg_sizes_decl;
    arg_types = dynamic_arrays.arg_types_decl;
    arg_number_decl = dynamic_arrays.arg_number_decl;
  } else {
    materializeLiteralTargetArgExpressions(map_variable_list,
                                           map_variable_base_list,
                                           outlined_driver_body, p_scope);

    SgType *args_base_type = buildArrayType(buildPointerType(buildVoidType()));
    SgBracedInitializer *offloading_variables_base =
        buildBracedInitializer(map_variable_base_list, args_base_type);
    args_base_decl = buildVariableDeclaration(
        "__args_base", args_base_type,
        buildAssignInitializer(offloading_variables_base, args_base_type),
        p_scope);
    outlined_driver_body->append_statement(args_base_decl);

    SgType *args_type = buildArrayType(buildPointerType(buildVoidType()));
    SgBracedInitializer *offloading_variables =
        buildBracedInitializer(map_variable_list, args_type);
    args_decl = buildVariableDeclaration(
        "__args", args_type,
        buildAssignInitializer(offloading_variables, args_type), p_scope);
    outlined_driver_body->append_statement(args_decl);

    SgType *arg_sizes_type = buildArrayType(getRuntimeInt64Type(p_scope));
    SgBracedInitializer *map_variable_sizes =
        buildBracedInitializer(map_variable_size_list, arg_sizes_type);
    arg_sizes = buildVariableDeclaration(
        "__arg_sizes", arg_sizes_type,
        buildAssignInitializer(map_variable_sizes, arg_sizes_type), p_scope);
    outlined_driver_body->append_statement(arg_sizes);

    SgType *arg_types_type = buildArrayType(getRuntimeInt64Type(p_scope));
    SgBracedInitializer *map_variable_types =
        buildBracedInitializer(map_variable_type_list, arg_types_type);
    arg_types = buildVariableDeclaration(
        "__arg_types", arg_types_type,
        buildAssignInitializer(map_variable_types, arg_types_type), p_scope);
    outlined_driver_body->append_statement(arg_types);

    int kernel_arg_num = map_variable_base_list->get_expressions().size();
    SgType *arg_number_type = buildIntType();
    arg_number_decl = buildVariableDeclaration(
        "__arg_num", arg_number_type,
        buildAssignInitializer(buildIntVal(kernel_arg_num), arg_number_type),
        p_scope);
    outlined_driver_body->append_statement(arg_number_decl);
  }

  SgVariableDeclaration *kernel_args_decl = buildTargetKernelArgsDeclaration(
      g_scope, p_scope, arg_number_decl, args_base_decl, args_decl, arg_sizes,
      arg_types, num_blocks_decl, threads_per_block_decl,
      tripcount_decl != NULL ? buildVarRefExp(tripcount_decl) : NULL);
  outlined_driver_body->append_statement(kernel_args_decl);

  // call __tgt_target_kernel to execute the CUDA kernel
  SgVariableSymbol *kernel_args_sym = getFirstVarSym(kernel_args_decl);
  ROSE_ASSERT(kernel_args_sym != NULL);
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(num_blocks_decl),
      buildVarRefExp(threads_per_block_decl), buildVarRefExp(host_point_decl),
      buildExactAddressOfOp(buildVarRefExp(kernel_args_sym)));
  string func_offloading_name = "__tgt_target_kernel";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildIntType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  outlined_driver_body->append_statement(func_offloading_stmt);

  appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays,
                                              outlined_driver_body, p_scope);

  for (ASTtools::VarSymSet_t::const_iterator iter =
           per_block_reduction_syms.begin();
       iter != per_block_reduction_syms.end(); iter++) {
    const SgVariableSymbol *current_symbol = *iter;
    SgPointerType *pointer_type = isSgPointerType(
        current_symbol->get_type()); // must be a pointer to simple type
    ROSE_ASSERT(pointer_type != NULL);
    SgType *orig_type = pointer_type->get_base_type();
    ROSE_ASSERT(orig_type != NULL);

    string per_block_var_name = (current_symbol->get_name()).getString();
    // get the original var name by stripping of the leading "_dev_per_block_"
    string leading_pattern = string("__reduction_buffer_");
    string orig_var_name = per_block_var_name.substr(
        leading_pattern.length(),
        per_block_var_name.length() - leading_pattern.length());
    //      cout<<"debug: "<<per_block_var_name <<" after "<< orig_var_name
    //      <<endl;
    SgExprListExp *parameter_list = buildExprListExp(
        buildVarRefExp(const_cast<SgVariableSymbol *>(current_symbol)),
        buildVarRefExp("_num_blocks_", target->get_scope()),
        buildIntVal(
            offload_ctx.per_block_reduction_map[const_cast<SgVariableSymbol *>(
                current_symbol)]));
    SgStatement *reduce_on_cpu_stmt = generateTargetReduceOnCPU(
        orig_var_name, const_cast<SgVariableSymbol *>(current_symbol),
        num_blocks_decl,
        offload_ctx.per_block_reduction_map[const_cast<SgVariableSymbol *>(
            current_symbol)]);
    outlined_driver_body->append_statement(reduce_on_cpu_stmt);

    // insert memory free for the _dev_per_block_variables
    // TODO: need runtime support to automatically free memory
    SgFunctionCallExp *func_call_exp2 = buildFunctionCallExp(
        "free", buildVoidType(),
        buildExprListExp(
            buildVarRefExp(const_cast<SgVariableSymbol *>(current_symbol))),
        omp_target_stmt_body_block);
    outlined_driver_body->append_statement(buildExprStatement(func_call_exp2));
  }

  // num_blocks is referenced before the declaration is inserted. So we must fix
  // it, otherwise the symbol of unkown type will be cleaned up later.
  SageInterface::rebindVariableReferencesAfterMove(
      num_blocks_decl->get_scope());

  //------------now remove omp parallel since everything within it has been
  // outlined to a function
  replaceStatement(target, outlined_driver_body, true);
  attachComment(device_id_decl, "Launch CUDA kernel ...",
                PreprocessingInfo::C_StyleComment);

  // Restore preprocessing info attached to the original directive.
  if (!save_buf_inside.empty()) {
    for (PreprocessingInfo *info : save_buf_inside) {
      if (info != nullptr) {
        info->setRelativePosition(PreprocessingInfo::after);
      }
    }
    SageInterface::pastePreprocessingInfo(
        outlined_driver_body, PreprocessingInfo::after, save_buf_inside);
  }
  SageInterface::pastePreprocessingInfo(outlined_driver_body,
                                        PreprocessingInfo::before, save_buf1);
  SageInterface::pastePreprocessingInfo(outlined_driver_body,
                                        PreprocessingInfo::after, save_buf2);

  detachTransferredOmpDirectiveBody(target, outlined_driver_body);
  recordTargetOutlinedFunction(isSgFunctionDeclaration(result),
                               enclosing_function);
  retireConsumedOmpDirective(target);
}

void transOmpLoopInTargetRegion(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  SgForStatement *associated_for_loop = requireExactAssociatedForLoop(
      target, target->get_body(), AssociatedLoopPathContract::Target,
      "target-region-loop");
  SgBasicBlock *loop_block =
      transOmpTargetLoopBlock(associated_for_loop, NULL, &offload_ctx);

  replaceStatement(target, loop_block, true);
  detachTransferredOmpDirectiveBody(target, loop_block);
  retireConsumedOmpDirective(target);
}

// FIXME: It's still work-in-progress.
void transOmpSpmdInTargetRegion(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  // Now we need to ensure that "omp target " has a basic block as its body
  // so we can insert declarations into an inner block, instead of colliding
  // declarations within the scope of "omp target" This is important since we
  // often have consecutive "omp target" regions within one big scope We cannot
  // just insert things into that big scope.
  SgBasicBlock *omp_target_stmt_body_block =
      ensureBasicBlockAsBodyOfOmpBodyStmt(target);
  ROSE_ASSERT(isSgBasicBlock(target->get_body()));

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  // Save preprocessing info as early as possible, avoiding mess up from the
  // outliner
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  // 1/15/2009, Liao, also handle the last #endif, which is attached inside of
  // the target
  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  //-----------------------------------------------------------------
  // step 1: generated an outlined function and make it a CUDA function
  SgOmpClauseBodyStatement *target_parallel_stmt =
      isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target_parallel_stmt);

  // Prepare the outliner
  Outliner::enable_classic = true;
  //    Outliner::useParameterWrapper = false; //TODO: better handling of the
  //    dependence among flags
  SgBasicBlock *body_block = Outliner::preprocess(body);
  // translator OpenMP 3.0 and earlier variables.
  transOmpVariables(target, body_block);

  ASTtools::VarSymSet_t all_syms; // all generated or remaining variables to be
                                  // passed to the outliner
  // This addressOf_syms does not apply to CUDA kernel generation: since we
  // cannot use pass-by-reference for CUDA kernel. If we want to copy back
  // value, we have to use memory copy  since they are in two different memory
  // spaces.
  ASTtools::VarSymSet_t
      addressOf_syms; // generated or remaining variables should be passed by
                      // using their addresses

  SageInterface::rebindVariableReferencesAfterMove(body_block);
  Outliner::collectVars(body_block, all_syms);
  ASTtools::VarSymSet_t::iterator iter;
  for (iter = all_syms.begin(); iter != all_syms.end(); iter++) {
    const SgVariableSymbol *var_sym = *iter;
    MLOG_DEBUG_CXX("ompLowering")
        << "candidate outlined symbol: " << var_sym->get_name();
    SgType *i_type = var_sym->get_declaration()->get_type();
    if (!isSgPointerType(i_type) && !isSgArrayType(i_type))
      addressOf_syms.insert(var_sym);
  }

  // if num_threads clause exists, we need to set up the omp number of threads
  // first. therefore, the head will be the function call of setting up
  // num_threads.
  SgExpression *omp_num_threads = NULL;
  SgExpression *kernel_launch_bounds = NULL;
  if (hasClause(target, V_SgOmpNumThreadsClause)) {
    Rose_STL_Container<SgOmpClause *> num_threads_clauses =
        getClause(target, V_SgOmpNumThreadsClause);
    ROSE_ASSERT(num_threads_clauses.size() ==
                1); // should only have one num_threads()
    SgOmpNumThreadsClause *num_threads_clause =
        isSgOmpNumThreadsClause(num_threads_clauses[0]);
    ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
    omp_num_threads = copyExpression(num_threads_clause->get_expression());
    kernel_launch_bounds = copyExpression(num_threads_clause->get_expression());
  }

  string func_name = Outliner::generateFuncName(target);
  // add a meaningful suffix to the generated unique outlined function name
  // the suffix is "<enclosing function name>__<line number of the original
  // statement>__"
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDeclaration *enclosing_function =
      getEnclosingFunctionDeclaration(target);
  std::string enclosing_function_name =
      enclosing_function->get_name().getString();
  std::stringstream statement_line_number;
  statement_line_number << info->get_line();
  func_name +=
      enclosing_function_name + "__" + statement_line_number.str() + "__";

  SgGlobal *g_scope = SageInterface::getGlobalScope(body_block);
  ROSE_ASSERT(g_scope != NULL);

  // pass all the parameters by reference
  for (std::set<const SgVariableSymbol *>::iterator iter = all_syms.begin();
       iter != all_syms.end(); iter++) {
    if (!isPointerType((*iter)->get_type()) &&
        !isSgArrayType((*iter)->get_type()) &&
        offload_ctx.literal_target_param_syms.find(
            const_cast<SgVariableSymbol *>(*iter)) ==
            offload_ctx.literal_target_param_syms.end()) {
      addressOf_syms.insert(*iter);
    };
  };

  std::set<SgInitializedName *> restoreVars;
  Outliner::OutlinedLocalTypeTemplatePlan local_type_template_plan;
  SgFunctionDeclaration *result = Outliner::generateFunction(
      body_block, func_name + "kernel__", all_syms, addressOf_syms, restoreVars,
      NULL, g_scope, local_type_template_plan);
  if (!local_type_template_plan.entries.empty()) {
    fprintf(stderr, "REX_OMP_INVARIANT[target-local-type-call]: target kernel "
                    "requires explicit local type template arguments\n");
    ROSE_ABORT();
  }
  SgFunctionDeclaration *result_decl =
      isSgFunctionDeclaration(result->get_firstNondefiningDeclaration());
  ROSE_ASSERT(result_decl != NULL);
  retireConsumedTargetDevicePlaceholders(offload_ctx);
  recordTargetKernelLaunchBounds(result, kernel_launch_bounds);
  result_decl->get_functionModifier()
      .setCudaKernel(); // add __global__ modifier

  result->get_functionModifier().setCudaKernel();

  SgGlobal *glob_scope = getGlobalScope(target);
  if (glob_scope == NULL || glob_scope != g_scope ||
      result->get_parent() != glob_scope || result->get_scope() != glob_scope ||
      !glob_scope->statementExistsInScope(result)) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[target-kernel-publication]: function=%p "
            "parent=%p scope=%p target-global=%p construction-global=%p was "
            "not published exactly once by the outliner\n",
            static_cast<void *>(result),
            static_cast<void *>(result->get_parent()),
            static_cast<void *>(result->get_scope()),
            static_cast<void *>(glob_scope), static_cast<void *>(g_scope));
    ROSE_ABORT();
  }
  SgFunctionSymbol *func_symbol =
      glob_scope->lookup_function_symbol(result->get_name());
  ROSE_ASSERT(func_symbol != NULL);

  SgScopeStatement *p_scope =
      target->get_scope(); // the scope of "omp parallel" will be destroyed
                           // later, so we use scope of "omp target"
  ROSE_ASSERT(p_scope != NULL);

  // Generate the parameter list for the call to the XOMP runtime function
  SgExprListExp *parameters = buildExprListExp();
  for (iter = all_syms.begin(); iter != all_syms.end(); iter++) {
    const SgVariableSymbol *var_sym = *iter;
    SgVarRefExp *var_ref =
        buildVarRefExp(const_cast<SgVariableSymbol *>(var_sym));
    SgType *i_type = var_sym->get_declaration()->get_type();
    if (!isSgPointerType(i_type) && !isSgArrayType(i_type))
      appendExpression(parameters, buildExactAddressOfOp(var_ref));
    else
      appendExpression(parameters, var_ref);
  }
  // create the outlined driver for GPU offloading, which is empty at this point
  SgBasicBlock *outlined_driver_body = buildBasicBlock();

  SgCudaKernelExecConfig *cuda_kernel_config =
      buildCudaKernelExecConfig_nfi(buildIntVal(1), omp_num_threads);
  SgCudaKernelCallExp *cuda_kernel_call_expression = buildCudaKernelCallExp_nfi(
      buildFunctionRefExp(result), parameters, cuda_kernel_config);
  SgStatement *outlined_function_call =
      buildExprStatement(cuda_kernel_call_expression);

  setSourcePositionForTransformation(outlined_function_call);
  outlined_driver_body->append_statement(outlined_function_call);

  SageInterface::fixStatement(outlined_driver_body, p_scope);
  //------------now remove omp parallel since everything within it has been
  // outlined to a function
  replaceStatement(target, outlined_driver_body, true);

  recordTargetOutlinedFunction(isSgFunctionDeclaration(result),
                               enclosing_function);
  retireConsumedOmpDirective(target);
}

// transformation for combined directive omp target teams
void transOmpTargetTeams(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetTeamsStatement *target = isSgOmpTargetTeamsStatement(node);
  ROSE_ASSERT(target != NULL);

  Rose_STL_Container<SgOmpClause *> num_teams_clauses =
      getClause(target, V_SgOmpNumTeamsClause);
  ROSE_ASSERT(num_teams_clauses.size() ==
              1); // should only have one num_teams()
  SgOmpNumTeamsClause *num_teams_clause =
      isSgOmpNumTeamsClause(num_teams_clauses[0]);
  ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  SgExpression *omp_num_teams =
      copyExpression(num_teams_clause->get_expression());

  SgExpression *omp_num_threads = buildIntVal(1);
  SgExpression *kernel_launch_bounds = buildIntVal(1);

  transOmpTargetSpmd(target, omp_num_teams, omp_num_threads,
                     kernel_launch_bounds);
}

// transformation for combined directive omp target parallel
void transOmpTargetParallel(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetParallelStatement *target = isSgOmpTargetParallelStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *omp_num_teams = buildIntVal(1);

  Rose_STL_Container<SgOmpClause *> num_threads_clauses =
      getClause(target, V_SgOmpNumThreadsClause);
  ROSE_ASSERT(num_threads_clauses.size() ==
              1); // should only have one num_threads()
  SgOmpNumThreadsClause *num_threads_clause =
      isSgOmpNumThreadsClause(num_threads_clauses[0]);
  ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  SgExpression *omp_num_threads =
      copyExpression(num_threads_clause->get_expression());
  SgExpression *kernel_launch_bounds =
      copyExpression(num_threads_clause->get_expression());

  transOmpTargetSpmd(target, omp_num_teams, omp_num_threads,
                     kernel_launch_bounds);
}

// transformation for omp target
void transOmpTarget(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetStatement *target = isSgOmpTargetStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *omp_num_teams = buildIntVal(1);
  SgExpression *omp_num_threads = buildIntVal(1);
  SgExpression *kernel_launch_bounds = buildIntVal(1);

  transOmpTargetSpmd(target, omp_num_teams, omp_num_threads,
                     kernel_launch_bounds);
}

// transformation for combined directive omp target teams distribute
void transOmpTargetTeamsDistribute(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetTeamsDistributeStatement *target =
      isSgOmpTargetTeamsDistributeStatement(node);
  ROSE_ASSERT(target != NULL);

  Rose_STL_Container<SgOmpClause *> num_teams_clauses =
      getClause(target, V_SgOmpNumTeamsClause);
  ROSE_ASSERT(num_teams_clauses.size() ==
              1); // should only have one num_teams()
  SgOmpNumTeamsClause *num_teams_clause =
      isSgOmpNumTeamsClause(num_teams_clauses[0]);
  ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  SgExpression *omp_num_teams =
      copyExpression(num_teams_clause->get_expression());

  SgExpression *omp_num_threads = buildIntVal(1);
  SgExpression *kernel_launch_bounds = buildIntVal(1);

  transOmpTargetSpmdWorksharing(target, omp_num_teams, omp_num_threads,
                                kernel_launch_bounds,
                                /*has_explicit_num_teams=*/true,
                                /*has_explicit_num_threads=*/false);
}

// transformation for combined directive omp target parallel for
void transOmpTargetParallelFor(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetParallelForStatement *target =
      isSgOmpTargetParallelForStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *omp_num_teams = buildIntVal(1);

  Rose_STL_Container<SgOmpClause *> num_threads_clauses =
      getClause(target, V_SgOmpNumThreadsClause);
  ROSE_ASSERT(num_threads_clauses.size() ==
              1); // should only have one num_threads()
  SgOmpNumThreadsClause *num_threads_clause =
      isSgOmpNumThreadsClause(num_threads_clauses[0]);
  ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  SgExpression *omp_num_threads =
      copyExpression(num_threads_clause->get_expression());
  SgExpression *kernel_launch_bounds =
      copyExpression(num_threads_clause->get_expression());

  transOmpTargetSpmdWorksharing(target, omp_num_teams, omp_num_threads,
                                kernel_launch_bounds,
                                /*has_explicit_num_teams=*/false,
                                /*has_explicit_num_threads=*/true);
}

// transformation for combined directive omp target teams distribute parallel
// for
void transOmpTargetTeamsDistributeParallelFor(SgNode *node) {
  // Sanity check first
  ROSE_ASSERT(node != NULL);
  SgOmpTargetTeamsDistributeParallelForStatement *target =
      isSgOmpTargetTeamsDistributeParallelForStatement(node);
  ROSE_ASSERT(target != NULL);

  Rose_STL_Container<SgOmpClause *> num_teams_clauses =
      getClause(target, V_SgOmpNumTeamsClause);
  ROSE_ASSERT(num_teams_clauses.size() ==
              1); // should only have one num_teams()
  SgOmpNumTeamsClause *num_teams_clause =
      isSgOmpNumTeamsClause(num_teams_clauses[0]);
  ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  SgExpression *omp_num_teams =
      copyExpression(num_teams_clause->get_expression());

  Rose_STL_Container<SgOmpClause *> num_threads_clauses =
      getClause(target, V_SgOmpNumThreadsClause);
  ROSE_ASSERT(num_threads_clauses.size() ==
              1); // should only have one num_threads()
  SgOmpNumThreadsClause *num_threads_clause =
      isSgOmpNumThreadsClause(num_threads_clauses[0]);
  ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  SgExpression *omp_num_threads =
      copyExpression(num_threads_clause->get_expression());
  SgExpression *kernel_launch_bounds =
      copyExpression(num_threads_clause->get_expression());

  transOmpTargetSpmdWorksharing(target, omp_num_teams, omp_num_threads,
                                kernel_launch_bounds,
                                /*has_explicit_num_teams=*/true,
                                /*has_explicit_num_threads=*/true);
}

/*
 * Expected AST layout:
 *  SgOmpSectionsStatement
 *    SgBasicBlock
 *      SgOmpSectionStatement (1 or more section statements here)
 *        SgBasicBlock
 *          SgStatement
 *
 * Lowering strategy:
 *   Translate sections as a static-scheduled iteration space [0, N-1] using
 *   __kmpc_for_static_init_4/__kmpc_for_static_fini so host lowering uses the
 *   LLVM OpenMP runtime for both C/C++ and Fortran.
 * */
void transOmpSections(SgNode *node) {
  //    cout<<"Entering transOmpSections() ..."<<endl;
  ROSE_ASSERT(node != NULL);
  // verify the AST is expected
  SgOmpSectionsStatement *target = isSgOmpSectionsStatement(node);
  ROSE_ASSERT(target != NULL);
  const KmpcGlobalTidSourceContext tid_context =
      require_kmpc_global_tid_source_context(target);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgBasicBlock *bb1 = buildBasicBlock();

  SgBasicBlock *sections_block = isSgBasicBlock(body);
  ROSE_ASSERT(sections_block != NULL);
  // verify each statement under sections is SgOmpSectionStatement
  SgStatementPtrList section_list = sections_block->get_statements();
  int section_count = static_cast<int>(section_list.size());
  for (int i = 0; i < section_count; i++) {
    SgStatement *stmt = section_list[i];
    ROSE_ASSERT(isSgOmpSectionStatement(stmt));
    SgOmpSectionStatement *section = isSgOmpSectionStatement(stmt);
    if (isSgBasicBlock(section->get_body()) == nullptr) {
      (void)ensureBasicBlockAsBodyOfOmpBodyStmt(section);
    }
  }

  // Move every source section while the directive is still attached to its
  // source file. The detached destination blocks are exact pending
  // transformation transactions; they acquire the replacement block's output
  // owner when their case statements are attached below.
  std::vector<SgBasicBlock *> moved_section_bodies;
  moved_section_bodies.reserve(section_list.size());
  for (SgStatement *section_node : section_list) {
    SgOmpSectionStatement *section_statement =
        isSgOmpSectionStatement(section_node);
    ROSE_ASSERT(section_statement != NULL);
    SgBasicBlock *src_bb = isSgBasicBlock(section_statement->get_body());
    ROSE_ASSERT(src_bb != NULL);
    SgBasicBlock *target_bb = buildBasicBlock();
    moveStatementsBetweenBlocks(src_bb, target_bb);
    moved_section_bodies.push_back(target_bb);

    SgBasicBlock *fake_src_bb = buildBasicBlock();
    section_statement->set_body(fake_src_bb);
    fake_src_bb->set_parent(section_statement);
    src_bb->set_parent(nullptr);
    SageInterface::deleteAST(src_bb,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }

  std::string sec_var_name;
  if (SageInterface::is_Fortran_language())
    sec_var_name = "_section_";
  else
    sec_var_name = "xomp_section_";

  sec_var_name += StringUtility::numberToString(++gensym_counter);
  const int sec_max_value = section_count - 1;
  std::string sec_lower_name = sec_var_name + "_lower";
  std::string sec_upper_name = sec_var_name + "_upper";
  std::string sec_stride_name = sec_var_name + "_stride";
  std::string sec_last_iter_name = sec_var_name + "_last_iter";

  replaceStatement(target, bb1, true);

  SgScopeStatement *kmpc_tid_decl_scope = scope;
  if (!SageInterface::is_Fortran_language())
    kmpc_tid_decl_scope = bb1;

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration = get_kmpc_global_tid(
      tid_context, kmpc_tid_decl_scope, &kmpc_global_tid_init);
  SgExpression *thread_global_tid =
      buildVarRefExp(getFirstVariable(*kmpc_global_tid_declaration).get_name(),
                     kmpc_tid_decl_scope);
  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    appendStatement(kmpc_global_tid_declaration, bb1);
  }
  if (kmpc_global_tid_init != NULL)
    appendStatement(kmpc_global_tid_init, bb1);

  // Declare a variable to store the current section id
  // Only used to support lastprivate
  SgScopeStatement *section_declaration_scope =
      SageInterface::is_Fortran_language()
          ? static_cast<SgScopeStatement *>(scope)
          : static_cast<SgScopeStatement *>(bb1);
  SgType *section_index_type = SageInterface::is_Fortran_language()
                                   ? static_cast<SgType *>(buildKmpcInt32Type())
                                   : static_cast<SgType *>(buildIntType());
  SgVariableDeclaration *sec_var_decl_save = NULL;
  if (hasClause(target, V_SgOmpLastprivateClause)) {
    sec_var_decl_save =
        buildVariableDeclaration(sec_var_name + "_save", section_index_type,
                                 NULL, section_declaration_scope);
    if (SageInterface::is_Fortran_language())
      insert_fortran_declaration_into_procedure(sec_var_decl_save, scope);
    else
      appendStatement(sec_var_decl_save, bb1);
  }

  SgVariableDeclaration *sec_var_decl = buildVariableDeclaration(
      sec_var_name, section_index_type, NULL, section_declaration_scope);
  SgVariableDeclaration *sec_lower_decl = buildVariableDeclaration(
      sec_lower_name, section_index_type, NULL, section_declaration_scope);
  SgVariableDeclaration *sec_upper_decl = buildVariableDeclaration(
      sec_upper_name, section_index_type, NULL, section_declaration_scope);
  SgVariableDeclaration *sec_stride_decl = buildVariableDeclaration(
      sec_stride_name, section_index_type, NULL, section_declaration_scope);
  SgVariableDeclaration *sec_last_iter_decl = buildVariableDeclaration(
      sec_last_iter_name, section_index_type, NULL, section_declaration_scope);

  if (SageInterface::is_Fortran_language())
    insert_fortran_declaration_into_procedure(sec_var_decl, scope);
  else
    appendStatement(sec_var_decl, bb1);

  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(sec_lower_decl, scope);
    insert_fortran_declaration_into_procedure(sec_upper_decl, scope);
    insert_fortran_declaration_into_procedure(sec_stride_decl, scope);
    insert_fortran_declaration_into_procedure(sec_last_iter_decl, scope);
  } else {
    appendStatement(sec_lower_decl, bb1);
    appendStatement(sec_upper_decl, bb1);
    appendStatement(sec_stride_decl, bb1);
    appendStatement(sec_last_iter_decl, bb1);
  }

  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_lower_decl), buildIntVal(0)),
      bb1);
  appendStatement(buildAssignStatement(buildVarRefExp(sec_upper_decl),
                                       buildIntVal(sec_max_value)),
                  bb1);
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_stride_decl), buildIntVal(1)),
      bb1);
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_last_iter_decl), buildIntVal(0)),
      bb1);

  SgExpression *e_last_iter =
      buildExactAddressOfOp(buildVarRefExp(sec_last_iter_decl));
  SgExpression *e_lower = buildExactAddressOfOp(buildVarRefExp(sec_lower_decl));
  SgExpression *e_upper = buildExactAddressOfOp(buildVarRefExp(sec_upper_decl));
  SgExpression *e_stride =
      buildExactAddressOfOp(buildVarRefExp(sec_stride_decl));
  if (SageInterface::is_Fortran_language()) {
    // Fortran scalar arguments are pass-by-reference.
    e_last_iter = buildVarRefExp(sec_last_iter_decl);
    e_lower = buildVarRefExp(sec_lower_decl);
    e_upper = buildVarRefExp(sec_upper_decl);
    e_stride = buildVarRefExp(sec_stride_decl);
  }
  SgExprListExp *init_parameters = buildExprListExp(
      buildIntVal(0), copyExpression(thread_global_tid),
      buildIntVal(kmp_sched_static_chunk), e_last_iter, e_lower, e_upper,
      e_stride, buildIntVal(1), buildIntVal(1));
  appendStatement(buildFunctionCallStmt(
                      getKmpcRuntimeFunctionName("__kmpc_for_static_init_4"),
                      buildVoidType(), init_parameters, scope),
                  bb1);

  SgIfStmt *clamp_upper = buildIfStmt(
      buildGreaterThanOp(buildVarRefExp(sec_upper_decl),
                         buildIntVal(sec_max_value), exactLogicalResultType()),
      buildAssignStatement(buildVarRefExp(sec_upper_decl),
                           buildIntVal(sec_max_value)),
      NULL);
  appendStatement(clamp_upper, bb1);

  SgIfStmt *init_section_id = buildIfStmt(
      buildLessOrEqualOp(buildVarRefExp(sec_lower_decl),
                         buildVarRefExp(sec_upper_decl),
                         exactLogicalResultType()),
      buildAssignStatement(buildVarRefExp(sec_var_decl),
                           buildVarRefExp(sec_lower_decl)),
      buildAssignStatement(buildVarRefExp(sec_var_decl), buildIntVal(-1)));
  appendStatement(init_section_id, bb1);

  // while (_section_1 >=0) {}
  SgWhileStmt *while_stmt = buildWhileStmt(
      buildGreaterOrEqualOp(buildVarRefExp(sec_var_decl), buildIntVal(0),
                            exactLogicalResultType()),
      buildBasicBlock());
  if (SageInterface::is_Fortran_language()) {
    while_stmt->set_has_end_statement(true);
  }
  appendStatement(while_stmt, bb1);
  // switch () {}
  SgSwitchStatement *switch_stmt = buildSwitchStatement(
      buildExprStatement(buildVarRefExp(sec_var_decl)), buildBasicBlock());
  appendStatement(switch_stmt, isSgBasicBlock(while_stmt->get_body()));
  // case 0, case 1, ...
  for (int i = 0; i < section_count; i++) {
    SgBasicBlock *target_bb = moved_section_bodies[i];
    ROSE_ASSERT(target_bb != NULL);
    SgCaseOptionStmt *option_stmt =
        buildCaseOptionStmt(buildIntVal(i), target_bb);
    appendStatement(option_stmt, isSgBasicBlock(switch_stmt->get_body()));
    ROSE_ASSERT(option_stmt->get_body() == target_bb &&
                target_bb->get_parent() == option_stmt);
    appendStatement(buildBreakStmt(), target_bb);

  } // end case 0, 1, ...
  // default option:
  SgDefaultOptionStmt *default_stmt = buildDefaultOptionStmt(buildBasicBlock(
      buildFunctionCallStmt("abort", buildVoidType(), NULL, scope)));
  appendStatement(default_stmt, isSgBasicBlock(switch_stmt->get_body()));

  // save the current section id before checking for next available one
  // This is only useful to support lastprivate clause
  if (hasClause(target, V_SgOmpLastprivateClause)) {
    SgStatement *save_stmt = buildAssignStatement(
        buildVarRefExp(sec_var_decl_save), buildVarRefExp(sec_var_decl));
    appendStatement(save_stmt, isSgBasicBlock(while_stmt->get_body()));
  }

  SgBasicBlock *while_body = isSgBasicBlock(while_stmt->get_body());
  appendStatement(
      buildAssignStatement(buildVarRefExp(sec_var_decl),
                           buildAddOp(buildVarRefExp(sec_var_decl),
                                      buildIntVal(1), section_index_type)),
      while_body);

  // Build the conditional body as one detached subtree.  Its physical output
  // identity is published with the enclosing if statement below.
  SgBasicBlock *advance_block = buildBasicBlock(
      buildAssignStatement(buildVarRefExp(sec_lower_decl),
                           buildAddOp(buildVarRefExp(sec_lower_decl),
                                      buildVarRefExp(sec_stride_decl),
                                      section_index_type)),
      buildAssignStatement(buildVarRefExp(sec_upper_decl),
                           buildAddOp(buildVarRefExp(sec_upper_decl),
                                      buildVarRefExp(sec_stride_decl),
                                      section_index_type)),
      buildIfStmt(buildGreaterThanOp(buildVarRefExp(sec_upper_decl),
                                     buildIntVal(sec_max_value),
                                     exactLogicalResultType()),
                  buildAssignStatement(buildVarRefExp(sec_upper_decl),
                                       buildIntVal(sec_max_value)),
                  NULL),
      buildIfStmt(
          buildLessOrEqualOp(buildVarRefExp(sec_lower_decl),
                             buildVarRefExp(sec_upper_decl),
                             exactLogicalResultType()),
          buildAssignStatement(buildVarRefExp(sec_var_decl),
                               buildVarRefExp(sec_lower_decl)),
          buildAssignStatement(buildVarRefExp(sec_var_decl), buildIntVal(-1))));
  appendStatement(buildIfStmt(buildGreaterThanOp(buildVarRefExp(sec_var_decl),
                                                 buildVarRefExp(sec_upper_decl),
                                                 exactLogicalResultType()),
                              advance_block, NULL),
                  while_body);

  transOmpVariables(
      target, bb1,
      buildIntVal(section_count -
                  1)); // This should happen before the barrier is inserted.

  SgExprListExp *fini_parameters =
      buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
  appendStatement(buildFunctionCallStmt(
                      getKmpcRuntimeFunctionName("__kmpc_for_static_fini"),
                      buildVoidType(), fini_parameters, scope),
                  bb1);

  if (!hasClause(target, V_SgOmpNowaitClause)) {
    SgExprListExp *barrier_parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    appendStatement(
        buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_barrier"),
                              buildVoidType(), barrier_parameters, scope),
        bb1);
  }
  retireConsumedOmpDirective(target);
}

// Two ways
// 1. builtin function TODO
//    __sync_fetch_and_add_4(&shared, (unsigned int)local);
// 2. using atomic runtime call:
//    GOMP_atomic_start (); // void GOMP_atomic_start (void);
//    shared = shared op local;
//    GOMP_atomic_end (); // void GOMP_atomic_end (void);
// We use the 2nd method only for now, for simplicity and portability
void transOmpAtomic(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpAtomicStatement *target = isSgOmpAtomicStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  replaceStatement(target, body, true);
  SgExprStatement *func_call_stmt1 =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_atomic_start"),
                            buildVoidType(), NULL, scope);
  SgExprStatement *func_call_stmt2 =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_atomic_end"),
                            buildVoidType(), NULL, scope);
  insertStatementBefore(body, func_call_stmt1);
  // this is actually sensitive to the type of preprocessing Info
  // In most cases, we want to move up them (such as #ifdef etc)
  movePreprocessingInfo(body, func_call_stmt1, PreprocessingInfo::before);
  insertStatementAfter(body, func_call_stmt2);
}

//! Translate the ordered directive, (not the ordered clause)
void transOmpOrdered(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpOrderedStatement *target = isSgOmpOrderedStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  replaceStatement(target, body, true);
  SgExprStatement *func_call_stmt1 =
      buildFunctionCallStmt("XOMP_ordered_start", buildVoidType(), NULL, scope);
  SgExprStatement *func_call_stmt2 =
      buildFunctionCallStmt("XOMP_ordered_end", buildVoidType(), NULL, scope);
  insertStatementBefore(body, func_call_stmt1);
  insertStatementAfter(body, func_call_stmt2);
}

// Two cases:
// unnamed one
//   GOMP_critical_start ();
//   work()
//   GOMP_critical_end ();
//
// named one:
//  static gomp_mutex_t  &gomp_critical_user_aaa;
//  GOMP_critical_name_start (&gomp_critical_user_aaa);
//  work()
//  GOMP_critical_name_end (&gomp_critical_user_aaa);
//
static const int kKmpCriticalNameWords = 8; // kmp_critical_name is int32_t[8]
static const int kKmpCriticalNameAlignment = alignof(void *);

void transOmpCritical(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpCriticalStatement *target = isSgOmpCriticalStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgExprStatement *func_call_stmt1 = NULL, *func_call_stmt2 = NULL;
  string c_name = target->get_name().getString();

  // Assign a default lock variable name for unnamed critical directives.
  string g_lock_name = "xomp_critical_user_" + c_name;
  SgVariableSymbol *sym = NULL;
  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    SgBasicBlock *proc_body = func_def->get_body();
    ROSE_ASSERT(proc_body != NULL);

    auto is_direct_module_scope = [](SgScopeStatement *candidate) -> bool {
      if (candidate == NULL)
        return false;
      if (isSgModuleStatement(candidate) != NULL)
        return true;
      if (SgDeclarationStatement *parent_decl =
              isSgDeclarationStatement(candidate->get_parent())) {
        if (isSgModuleStatement(parent_decl) != NULL)
          return true;
      }
      if (SgClassDefinition *class_def = isSgClassDefinition(candidate)) {
        SgDeclarationStatement *decl = class_def->get_declaration();
        return isSgModuleStatement(decl) != NULL;
      }
      return false;
    };

    auto is_symbol_from_current_procedure =
        [&](SgVariableSymbol *candidate) -> bool {
      if (candidate == NULL)
        return false;

      SgScopeStatement *decl_scope = NULL;
      if (SgInitializedName *candidate_decl = candidate->get_declaration())
        decl_scope = candidate_decl->get_scope();
      if (decl_scope == NULL)
        decl_scope = candidate->get_scope();
      if (decl_scope == NULL)
        return false;

      SgFunctionDefinition *decl_func_def =
          getEnclosingFunctionDefinition(decl_scope);
      return decl_func_def != NULL && decl_func_def == func_def;
    };

    auto ensure_local_fortran_lock_symbol = [&]() -> SgVariableSymbol * {
      SgExprListExp *lock_dims =
          buildExprListExp(buildIntVal(kKmpCriticalNameWords));
      SgType *lock_type = buildArrayType(buildKmpcInt32Type(), lock_dims);
      SgVariableDeclaration *vardecl =
          buildVariableDeclaration(g_lock_name, lock_type, NULL, proc_body);
      insert_fortran_declaration_into_procedure(vardecl, proc_body);
      return getFirstVarSym(vardecl);
    };

    sym = lookupVariableSymbolInParentScopes(SgName(g_lock_name), scope);
    bool symbol_is_module_entity = false;
    bool symbol_is_current_procedure_entity = false;
    if (sym != NULL) {
      symbol_is_module_entity = is_direct_module_scope(sym->get_scope());
      if (!symbol_is_module_entity) {
        if (SgInitializedName *sym_decl = sym->get_declaration()) {
          symbol_is_module_entity =
              is_direct_module_scope(sym_decl->get_scope());
        }
      }
      symbol_is_current_procedure_entity =
          is_symbol_from_current_procedure(sym);
    }

    // Host-associated variables from parent procedures cannot appear in COMMON
    // inside this procedure. If we found such a symbol, create a local lock
    // declaration to provide valid COMMON-backed global storage semantics.
    if (sym == NULL ||
        (!symbol_is_module_entity && !symbol_is_current_procedure_entity)) {
      sym = ensure_local_fortran_lock_symbol();
      symbol_is_module_entity = false;
      symbol_is_current_procedure_entity = true;
    }
    ROSE_ASSERT(sym != NULL);

    if (!symbol_is_module_entity) {
      // Fortran COMMON provides global storage semantics for named/unnamed
      // critical locks across procedures in a translation unit.
      const std::string common_block_name = "xomp_critical_block_" + c_name;
      bool has_common_block = false;
      const SgStatementPtrList &stmts = proc_body->get_statements();
      for (SgStatementPtrList::const_iterator it = stmts.begin();
           it != stmts.end(); ++it) {
        SgCommonBlock *common_block = isSgCommonBlock(*it);
        if (common_block == NULL)
          continue;

        const SgCommonBlockObjectPtrList &blocks =
            common_block->get_block_list();
        for (SgCommonBlockObjectPtrList::const_iterator bit = blocks.begin();
             bit != blocks.end(); ++bit) {
          if ((*bit)->get_block_name() == common_block_name) {
            has_common_block = true;
            break;
          }
        }
        if (has_common_block)
          break;
      }

      if (!has_common_block) {
        SgExprListExp *common_vars = buildExprListExp(buildVarRefExp(sym));
        SgCommonBlockObject *common_obj =
            buildCommonBlockObject(common_block_name, common_vars);
        SgCommonBlock *common_decl = buildCommonBlock(common_obj);
        insert_fortran_statement_in_specification_part(common_decl, proc_body);
      }
    }
  } else {
    SgGlobal *global = getGlobalScope(target);
    ROSE_ASSERT(global != NULL);
    sym = lookupVariableSymbolInParentScopes(SgName(g_lock_name), global);
    if (sym == NULL) {
      SgType *lock_type = buildArrayType(buildKmpcInt32Type(),
                                         buildIntVal(kKmpCriticalNameWords));
      SgVariableDeclaration *vardecl =
          buildVariableDeclaration(g_lock_name, lock_type, NULL, global);
      if (SgInitializedName *lock_decl = getFirstInitializedName(vardecl)) {
        lock_decl->set_gnu_attribute_alignment(kKmpCriticalNameAlignment);
      }
      setStatic(vardecl);
      prependStatement(vardecl, global);
      sym = getFirstVarSym(vardecl);
    }
  }
  ROSE_ASSERT(sym != NULL);

  if (SageInterface::is_Fortran_language()) {
    SgExprListExp *param1 = buildExprListExp(buildVarRefExp(sym));
    SgExprListExp *param2 = buildExprListExp(buildVarRefExp(sym));

    func_call_stmt1 = buildFunctionCallStmt("XOMP_critical_start",
                                            buildVoidType(), param1, scope);
    func_call_stmt2 = buildFunctionCallStmt("XOMP_critical_end",
                                            buildVoidType(), param2, scope);
  } else {
    SgStatement *kmpc_global_tid_init = NULL;
    SgVariableDeclaration *kmpc_global_tid_declaration =
        get_kmpc_global_tid(require_kmpc_global_tid_source_context(node), scope,
                            &kmpc_global_tid_init);
    SgName tid_name = getFirstVariable(*kmpc_global_tid_declaration).get_name();

    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
    if (kmpc_global_tid_init != NULL)
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);

    SgExpression *lock_ref1 =
        buildCastExp(buildVarRefExp(sym), buildPointerType(buildVoidType()),
                     SgCastExp::e_C_style_cast);
    SgExpression *lock_ref2 =
        buildCastExp(buildVarRefExp(sym), buildPointerType(buildVoidType()),
                     SgCastExp::e_C_style_cast);

    SgExprListExp *param1 = buildExprListExp(
        buildIntVal(0), buildVarRefExp(tid_name, scope), lock_ref1);
    SgExprListExp *param2 = buildExprListExp(
        buildIntVal(0), buildVarRefExp(tid_name, scope), lock_ref2);

    func_call_stmt1 =
        buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_critical"),
                              buildVoidType(), param1, scope);
    func_call_stmt2 =
        buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_end_critical"),
                              buildVoidType(), param2, scope);
  }

  replaceStatement(target, body, true);
  insertStatementBefore(body, func_call_stmt1);
  insertStatementAfter(body, func_call_stmt2);
}

//! Simply replace the pragma with a function call to void GOMP_taskwait(void);
void transOmpTaskwait(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTaskwaitStatement *target = isSgOmpTaskwaitStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(require_kmpc_global_tid_source_context(node), scope,
                          &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), scope);

  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }

  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }

  SgExprListExp *parameters =
      buildExprListExp(buildIntVal(0), thread_global_tid);
  SgExprStatement *func_call_stmt =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_omp_taskwait"),
                            buildVoidType(), parameters, scope);
  replaceStatement(target, func_call_stmt, true);
}

//! Lower one taskgroup to the exact LLVM OpenMP runtime scope operations.
void transOmpTaskgroup(SgNode *node) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskgroup-owner]: directive is null\n");
    ROSE_ABORT();
  }
  SgOmpTaskgroupStatement *target = isSgOmpTaskgroupStatement(node);
  SgScopeStatement *scope = target != nullptr ? target->get_scope() : nullptr;
  SgStatement *body = target != nullptr ? target->get_body() : nullptr;
  SgOmpClauseList *clause_list =
      target != nullptr ? getOmpClauseList(target) : nullptr;
  if (target == nullptr || scope == nullptr || body == nullptr ||
      body->get_parent() != target || clause_list == nullptr ||
      clause_list->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskgroup-owner]: node=%p does not "
            "own one exact scoped body and clause list\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }
  if (!clause_list->get_clauses().empty()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskgroup-clause]: task reductions "
            "require their own exact runtime reduction lowering\n");
    ROSE_ABORT();
  }

  const KmpcGlobalTidSourceContext tid_context =
      require_kmpc_global_tid_source_context(target);
  SgStatement *tid_init = nullptr;
  SgVariableDeclaration *tid_declaration =
      get_kmpc_global_tid(tid_context, scope, &tid_init);
  if (tid_declaration == nullptr) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[taskgroup-tid]: runtime thread "
                    "identity declaration was not built\n");
    ROSE_ABORT();
  }
  const SgName tid_name = getFirstVariable(*tid_declaration).get_name();
  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(tid_declaration, scope);
  } else {
    insertStatement(target, tid_declaration);
    tid_declaration->set_parent(target->get_parent());
  }
  if (tid_init != nullptr) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, tid_init);
    else
      insertStatementAfter(tid_declaration, tid_init);
  }

  SgExprStatement *begin_call = buildFunctionCallStmt(
      getKmpcRuntimeFunctionName("__kmpc_taskgroup"), buildVoidType(),
      buildExprListExp(buildIntVal(0), buildVarRefExp(tid_name, scope)), scope);
  SgExprStatement *end_call = buildFunctionCallStmt(
      getKmpcRuntimeFunctionName("__kmpc_end_taskgroup"), buildVoidType(),
      buildExprListExp(buildIntVal(0), buildVarRefExp(tid_name, scope)), scope);
  if (begin_call == nullptr || end_call == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskgroup-runtime]: exact begin/end "
            "runtime calls were not built\n");
    ROSE_ABORT();
  }

  AttachedPreprocessingInfoType before, after;
  cutPreprocessingInfo(target, PreprocessingInfo::before, before);
  cutPreprocessingInfo(target, PreprocessingInfo::after, after);
  target->set_body(nullptr);
  SgBasicBlock *block = isSgBasicBlock(body);
  if (block == nullptr) {
    block = buildBasicBlock();
    appendStatement(body, block);
  }
  replaceStatement(target, block, true);
  prependStatement(begin_call, block);
  appendStatement(end_call, block);
  pastePreprocessingInfo(block, PreprocessingInfo::before, before);
  pastePreprocessingInfo(block, PreprocessingInfo::after, after);
}

//! Simply replace the pragma with a function call to void GOMP_barrier (void);
void transOmpBarrier(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpBarrierStatement *target = isSgOmpBarrierStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(require_kmpc_global_tid_source_context(node), scope,
                          &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), scope);

  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }

  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }

  SgExprListExp *parameters =
      buildExprListExp(buildIntVal(0), thread_global_tid);
  SgExprStatement *func_call_stmt =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_barrier"),
                            buildVoidType(), parameters, scope);
  replaceStatement(target, func_call_stmt, true);
}

//! Simply replace the pragma with a function call to __sync_synchronize ();
void transOmpFlush(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpFlushStatement *target = isSgOmpFlushStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgExprStatement *func_call_stmt = NULL;
  if (SageInterface::is_Fortran_language()) {
    func_call_stmt =
        buildFunctionCallStmt("XOMP_flush", buildVoidType(), NULL, scope);
  } else
    func_call_stmt = buildFunctionCallStmt("__sync_synchronize",
                                           buildVoidType(), NULL, scope);
  replaceStatement(target, func_call_stmt, true);
}

// TODO: translate if() and device() clauses
void transOmpTargetData(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTargetDataStatement *target = isSgOmpTargetDataStatement(node);
  ROSE_ASSERT(target != NULL);
  GpuOffloadLoweringContext offload_ctx;

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  transOmpMapVariables(target, map_variable_list, map_variable_base_list,
                       map_variable_size_list, map_variable_type_list,
                       &offload_ctx, &dynamic_map_entries);

  SgBasicBlock *body = isSgBasicBlock(target->get_body());
  ROSE_ASSERT(body != NULL);

  if (!dynamic_map_entries.empty()) {
    SgBasicBlock *translated_body = buildBasicBlock();

    SgType *device_id_type = buildLongLongType();
    SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
        "__device_id", device_id_type,
        buildAssignInitializer(buildLongLongIntVal(-1), device_id_type),
        translated_body);
    translated_body->append_statement(device_id_decl);

    RuntimeMapArgumentArrayDeclarations dynamic_arrays =
        buildDynamicRuntimeMapArgumentArrays(
            translated_body, p_scope, map_variable_list, map_variable_base_list,
            map_variable_size_list, map_variable_type_list,
            dynamic_map_entries);

    SgExprListExp *parameters =
        buildExprListExp(buildVarRefExp(device_id_decl),
                         buildVarRefExp(dynamic_arrays.arg_number_decl),
                         buildVarRefExp(dynamic_arrays.args_base_decl),
                         buildVarRefExp(dynamic_arrays.args_decl),
                         buildVarRefExp(dynamic_arrays.arg_sizes_decl),
                         buildVarRefExp(dynamic_arrays.arg_types_decl));
    SgExprStatement *begin_stmt = buildFunctionCallStmt(
        "__tgt_target_data_begin", buildVoidType(), parameters, p_scope);
    setSourcePositionForTransformation(begin_stmt);
    translated_body->append_statement(begin_stmt);

    body->set_parent(NULL);
    target->set_body(NULL);
    translated_body->append_statement(body);

    SgExprStatement *end_stmt = buildFunctionCallStmt(
        "__tgt_target_data_end", buildVoidType(),
        buildExprListExp(buildVarRefExp(device_id_decl),
                         buildVarRefExp(dynamic_arrays.arg_number_decl),
                         buildVarRefExp(dynamic_arrays.args_base_decl),
                         buildVarRefExp(dynamic_arrays.args_decl),
                         buildVarRefExp(dynamic_arrays.arg_sizes_decl),
                         buildVarRefExp(dynamic_arrays.arg_types_decl)),
        p_scope);
    setSourcePositionForTransformation(end_stmt);
    translated_body->append_statement(end_stmt);

    appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays, translated_body,
                                                p_scope);

    for (SgStatement *statement : translated_body->get_statements()) {
      if (statement != body) {
        setSourcePositionForTransformation(statement);
      }
    }

    replaceStatement(target, translated_body, true);
    attachComment(translated_body,
                  "Translated from #pragma omp target data ...",
                  PreprocessingInfo::C_StyleComment);
    return;
  }

  SgBasicBlock *target_data_begin_block = body;

  // Use the OpenMP runtime's default device sentinel.
  SgType *device_id_type = buildLongLongType();
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", device_id_type,
      buildAssignInitializer(buildLongLongIntVal(-1), device_id_type),
      target_data_begin_block);
  prependStatement(device_id_decl, target_data_begin_block);

  SgType *args_base_type = buildArrayType(buildPointerType(buildVoidType()));
  SgBracedInitializer *offloading_variables_base =
      buildBracedInitializer(map_variable_base_list, args_base_type);
  SgVariableDeclaration *args_base_decl = buildVariableDeclaration(
      "__args_base", args_base_type,
      buildAssignInitializer(offloading_variables_base, args_base_type),
      target_data_begin_block);
  prependStatement(args_base_decl, target_data_begin_block);

  SgType *args_type = buildArrayType(buildPointerType(buildVoidType()));
  SgBracedInitializer *offloading_variables =
      buildBracedInitializer(map_variable_list, args_type);
  SgVariableDeclaration *args_decl = buildVariableDeclaration(
      "__args", args_type,
      buildAssignInitializer(offloading_variables, args_type),
      target_data_begin_block);
  prependStatement(args_decl, target_data_begin_block);

  SgType *arg_sizes_type = buildArrayType(getRuntimeInt64Type(p_scope));
  SgBracedInitializer *map_variable_sizes =
      buildBracedInitializer(map_variable_size_list, arg_sizes_type);
  SgVariableDeclaration *arg_sizes = buildVariableDeclaration(
      "__arg_sizes", arg_sizes_type,
      buildAssignInitializer(map_variable_sizes, arg_sizes_type),
      target_data_begin_block);
  prependStatement(arg_sizes, target_data_begin_block);

  SgType *arg_types_type = buildArrayType(getRuntimeInt64Type(p_scope));
  SgBracedInitializer *map_variable_types =
      buildBracedInitializer(map_variable_type_list, arg_types_type);
  SgVariableDeclaration *arg_types = buildVariableDeclaration(
      "__arg_types", arg_types_type,
      buildAssignInitializer(map_variable_types, arg_types_type),
      target_data_begin_block);
  prependStatement(arg_types, target_data_begin_block);

  int kernel_arg_num = map_variable_base_list->get_expressions().size();
  SgType *arg_number_type = buildIntType();
  SgVariableDeclaration *arg_number_decl = buildVariableDeclaration(
      "__arg_num", arg_number_type,
      buildAssignInitializer(buildIntVal(kernel_arg_num), arg_number_type),
      target_data_begin_block);
  prependStatement(arg_number_decl, target_data_begin_block);

  // call __tgt_target_data_begin to start the data mapping region for GPU
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(arg_number_decl),
      buildVarRefExp(args_base_decl), buildVarRefExp(args_decl),
      buildVarRefExp(arg_sizes), buildVarRefExp(arg_types));
  string func_offloading_name = "__tgt_target_data_begin";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildVoidType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  insertStatementAfter(device_id_decl, func_offloading_stmt);

  // call __tgt_target_data_end to end the data mapping region for GPU
  func_offloading_name = "__tgt_target_data_end";
  SgExprListExp *end_parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(arg_number_decl),
      buildVarRefExp(args_base_decl), buildVarRefExp(args_decl),
      buildVarRefExp(arg_sizes), buildVarRefExp(arg_types));
  func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildVoidType(), end_parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  appendStatement(func_offloading_stmt, body);
  body->set_parent(NULL);
  target->set_body(NULL);

  replaceStatement(target, body, true);
  attachComment(body, "Translated from #pragma omp target data ...",
                PreprocessingInfo::C_StyleComment);
}

void transOmpTargetUpdate(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpTargetUpdateStatement *target = isSgOmpTargetUpdateStatement(node);
  ROSE_ASSERT(target != NULL);

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);

  SgExprListExp *map_variable_list = buildExprListExp();
  SgExprListExp *map_variable_base_list = buildExprListExp();
  SgExprListExp *map_variable_size_list = buildExprListExp();
  SgExprListExp *map_variable_type_list = buildExprListExp();
  std::vector<ExpandedMapEntry> dynamic_map_entries;

  collectOmpTargetUpdateInfo(target, map_variable_list, map_variable_base_list,
                             map_variable_size_list, map_variable_type_list,
                             &dynamic_map_entries);

  if (!dynamic_map_entries.empty()) {
    SgBasicBlock *translated_block = buildBasicBlock();
    SgType *device_id_type = buildLongLongType();
    SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
        "__device_id", device_id_type,
        buildAssignInitializer(buildLongLongIntVal(-1), device_id_type),
        translated_block);
    translated_block->append_statement(device_id_decl);

    RuntimeMapArgumentArrayDeclarations dynamic_arrays =
        buildDynamicRuntimeMapArgumentArrays(
            translated_block, p_scope, map_variable_list,
            map_variable_base_list, map_variable_size_list,
            map_variable_type_list, dynamic_map_entries);

    SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
        "__tgt_target_data_update", buildVoidType(),
        buildExprListExp(buildVarRefExp(device_id_decl),
                         buildVarRefExp(dynamic_arrays.arg_number_decl),
                         buildVarRefExp(dynamic_arrays.args_base_decl),
                         buildVarRefExp(dynamic_arrays.args_decl),
                         buildVarRefExp(dynamic_arrays.arg_sizes_decl),
                         buildVarRefExp(dynamic_arrays.arg_types_decl)),
        p_scope);
    setSourcePositionForTransformation(func_offloading_stmt);
    translated_block->append_statement(func_offloading_stmt);

    appendDynamicRuntimeMapArgumentArrayCleanup(dynamic_arrays,
                                                translated_block, p_scope);

    setSourcePositionForTransformation(translated_block);

    translated_block->set_parent(target->get_parent());
    replaceStatement(target, translated_block, true);
    attachComment(func_offloading_stmt,
                  "Translated from #pragma omp target update ...",
                  PreprocessingInfo::C_StyleComment);
    return;
  }

  SgBasicBlock *target_data_begin_block = buildBasicBlock();
  // Use the OpenMP runtime's default device sentinel.
  SgType *device_id_type = buildLongLongType();
  SgVariableDeclaration *device_id_decl = buildVariableDeclaration(
      "__device_id", device_id_type,
      buildAssignInitializer(buildLongLongIntVal(-1), device_id_type),
      target_data_begin_block);
  target_data_begin_block->prepend_statement(device_id_decl);

  SgType *args_base_type = buildArrayType(buildPointerType(buildVoidType()));
  SgBracedInitializer *offloading_variables_base =
      buildBracedInitializer(map_variable_base_list, args_base_type);
  SgVariableDeclaration *args_base_decl = buildVariableDeclaration(
      "__args_base", args_base_type,
      buildAssignInitializer(offloading_variables_base, args_base_type),
      target_data_begin_block);
  target_data_begin_block->prepend_statement(args_base_decl);

  SgType *args_type = buildArrayType(buildPointerType(buildVoidType()));
  SgBracedInitializer *offloading_variables =
      buildBracedInitializer(map_variable_list, args_type);
  SgVariableDeclaration *args_decl = buildVariableDeclaration(
      "__args", args_type,
      buildAssignInitializer(offloading_variables, args_type),
      target_data_begin_block);
  target_data_begin_block->prepend_statement(args_decl);

  SgType *arg_sizes_type = buildArrayType(getRuntimeInt64Type(p_scope));
  SgBracedInitializer *map_variable_sizes =
      buildBracedInitializer(map_variable_size_list, arg_sizes_type);
  SgVariableDeclaration *arg_sizes = buildVariableDeclaration(
      "__arg_sizes", arg_sizes_type,
      buildAssignInitializer(map_variable_sizes, arg_sizes_type),
      target_data_begin_block);
  target_data_begin_block->prepend_statement(arg_sizes);

  SgType *arg_types_type = buildArrayType(getRuntimeInt64Type(p_scope));
  SgBracedInitializer *map_variable_types =
      buildBracedInitializer(map_variable_type_list, arg_types_type);
  SgVariableDeclaration *arg_types = buildVariableDeclaration(
      "__arg_types", arg_types_type,
      buildAssignInitializer(map_variable_types, arg_types_type),
      target_data_begin_block);
  target_data_begin_block->prepend_statement(arg_types);

  int kernel_arg_num = map_variable_base_list->get_expressions().size();
  SgType *arg_number_type = buildIntType();
  SgVariableDeclaration *arg_number_decl = buildVariableDeclaration(
      "__arg_num", arg_number_type,
      buildAssignInitializer(buildIntVal(kernel_arg_num), arg_number_type),
      target_data_begin_block);
  target_data_begin_block->prepend_statement(arg_number_decl);

  // call __tgt_target_data_begin to start the data mapping region for GPU
  SgExprListExp *parameters = NULL;
  parameters = buildExprListExp(
      buildVarRefExp(device_id_decl), buildVarRefExp(arg_number_decl),
      buildVarRefExp(args_base_decl), buildVarRefExp(args_decl),
      buildVarRefExp(arg_sizes), buildVarRefExp(arg_types));
  string func_offloading_name = "__tgt_target_data_update";
  SgExprStatement *func_offloading_stmt = buildFunctionCallStmt(
      func_offloading_name, buildVoidType(), parameters, p_scope);
  setSourcePositionForTransformation(func_offloading_stmt);
  target_data_begin_block->append_statement(func_offloading_stmt);

  target_data_begin_block->set_parent(target->get_parent());
  replaceStatement(target, target_data_begin_block, true);
  attachComment(func_offloading_stmt,
                "Translated from #pragma omp target update ...",
                PreprocessingInfo::C_StyleComment);
}

void transOmpAllocate(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpAllocateStatement *target = isSgOmpAllocateStatement(node);
  ROSE_ASSERT(target != NULL);

  if (!SageInterface::is_Fortran_language()) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate statement lowering is currently implemented only "
           "for Fortran allocate statements";
    ROSE_ABORT();
  }

  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  ensureFortranOmpAllocatorInterfaces(scope);

  SgStatement *next_stmt = findNextOriginalStatementInScope(target);
  SgAllocateStatement *allocate_stmt = isSgAllocateStatement(next_stmt);
  if (allocate_stmt == NULL) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate statement lowering expects the next statement to "
           "be a Fortran allocate statement";
    ROSE_ABORT();
  }

  const std::set<SgInitializedName *> target_objects =
      collectReferencedBaseObjects(target->get_variables());
  const std::set<SgInitializedName *> allocate_objects =
      collectAllocateStatementBaseObjects(allocate_stmt);
  if (target_objects.empty() || target_objects != allocate_objects) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP allocate lowering currently requires the directive variable "
           "list to match the following allocate statement exactly";
    ROSE_ABORT();
  }

  SgOmpAllocatorClause *allocator_clause = getAllocatorClauseOrAbort(target);
  SgExpression *allocator_expr =
      buildAllocatorArgumentExpression(allocator_clause, scope);
  SgType *allocator_type = allocator_expr->get_type();
  if (allocator_type == NULL) {
    allocator_type = buildIntType();
  }

  SgBasicBlock *procedure_body = getEnclosingFortranProcedureBody(scope);
  const std::string saved_name =
      generateUniqueVariableName(procedure_body, "__rex_saved_allocator_");
  SgVariableDeclaration *saved_decl = buildVariableDeclaration(
      saved_name, allocator_type, NULL, procedure_body);
  insert_fortran_declaration_into_procedure(saved_decl, scope);

  SgInitializedName &saved_var = getFirstVariable(*saved_decl);
  SgExprStatement *save_stmt = buildAssignStatement(
      buildVarRefExp(saved_var.get_name(), scope),
      buildFunctionCallExp("omp_get_default_allocator", allocator_type,
                           buildExprListExp(), scope));
  SgExprStatement *set_stmt =
      buildFunctionCallStmt("omp_set_default_allocator", buildVoidType(),
                            buildExprListExp(allocator_expr), scope);
  SgExprStatement *restore_stmt = buildFunctionCallStmt(
      "omp_set_default_allocator", buildVoidType(),
      buildExprListExp(buildVarRefExp(saved_var.get_name(), scope)), scope);

  removeStatement(target);
  insertStatementBefore(allocate_stmt, save_stmt);
  attachComment(save_stmt,
                "Translated from OpenMP allocate using explicit allocator "
                "runtime calls.",
                PreprocessingInfo::F90StyleComment);
  insertStatementAfter(save_stmt, set_stmt);
  insertStatementAfter(allocate_stmt, restore_stmt);
}

void transOmpRequires(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpRequiresStatement *target = isSgOmpRequiresStatement(node);
  ROSE_ASSERT(target != NULL);

  if (!requiresOnlyDynamicAllocators(target)) {
    MLOG_ERROR_CXX("ompLowering")
        << "OpenMP requires lowering currently supports only "
           "requires(dynamic_allocators)";
    ROSE_ABORT();
  }

  if (SgStatement *next_stmt = SageInterface::getNextStatement(target)) {
    attachComment(
        next_stmt,
        "Translated from OpenMP requires(dynamic_allocators); allocator "
        "semantics are lowered to explicit runtime calls.",
        PreprocessingInfo::F90StyleComment);
  }

  removeStatement(target);
}

namespace {
bool isAllowedLoopTransformationWrapper(SgOmpBodyStatement *wrapper) {
  return isSgOmpUnrollStatement(wrapper) != nullptr ||
         isSgOmpTileStatement(wrapper) != nullptr;
}

void requireOneExactStructuralEdge(SgNode *owner, SgNode *child,
                                   const char *contract) {
  if (owner == nullptr || child == nullptr || child->get_parent() != owner) {
    fprintf(
        stderr,
        "REX_OMP_LOWERING_INVARIANT[%s]: child=%p kind=%s parent=%p "
        "does not match exact owner=%p\n",
        contract, static_cast<void *>(child),
        child != nullptr ? child->sage_class_name() : "null",
        static_cast<void *>(child != nullptr ? child->get_parent() : nullptr),
        static_cast<void *>(owner));
    ROSE_ABORT();
  }
  const std::vector<SgNode *> successors =
      owner->get_traversalSuccessorContainer();
  const size_t matches =
      std::count(successors.begin(), successors.end(), child);
  if (matches != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: child=%p kind=%s has %zu "
            "structural edges from exact owner=%p kind=%s instead of one\n",
            contract, static_cast<void *>(child), child->sage_class_name(),
            matches, static_cast<void *>(owner), owner->sage_class_name());
    ROSE_ABORT();
  }
}
} // namespace

SgStatement *requireExactAssociatedLoop(
    SgOmpBodyStatement *directive, SgStatement *expected_root,
    AssociatedLoopPathContract path_contract, const char *contract) {
  if (directive == nullptr || expected_root == nullptr || contract == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: associated-loop contract "
            "requires one directive and expected body root\n",
            contract != nullptr ? contract : "null-associated-loop-contract");
    ROSE_ABORT();
  }
  if (directive->get_body() != expected_root) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: expected root=%p does not match "
            "directive=%p exact body=%p\n",
            contract, static_cast<void *>(expected_root),
            static_cast<void *>(directive),
            static_cast<void *>(directive->get_body()));
    ROSE_ABORT();
  }
  requireOneExactStructuralEdge(directive, expected_root, contract);

  std::unordered_set<SgStatement *> visited;
  SgStatement *cursor = expected_root;
  while (true) {
    if (!visited.insert(cursor).second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[%s]: associated-loop owner chain "
              "is cyclic at node=%p kind=%s\n",
              contract, static_cast<void *>(cursor), cursor->sage_class_name());
      ROSE_ABORT();
    }
    if (isSgForStatement(cursor) != nullptr ||
        isSgFortranDo(cursor) != nullptr) {
      return cursor;
    }

    SgStatement *next = nullptr;
    if (SgBasicBlock *block = isSgBasicBlock(cursor)) {
      const SgStatementPtrList &statements = block->get_statements();
      if (statements.size() != 1 || statements.front() == nullptr ||
          std::count(statements.begin(), statements.end(),
                     statements.front()) != 1) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[%s]: associated-loop block=%p "
                "contains %zu statements instead of one exact child\n",
                contract, static_cast<void *>(block), statements.size());
        ROSE_ABORT();
      }
      next = statements.front();
    } else if (SgAttributedStatement *attributed =
                   isSgAttributedStatement(cursor)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[%s]: attributed statement=%p is "
              "an explicit semantic/source wrapper and is not supported on "
              "an associated-loop path\n",
              contract, static_cast<void *>(attributed));
      ROSE_ABORT();
    } else if (SgOmpBodyStatement *wrapper = isSgOmpBodyStatement(cursor)) {
      if (path_contract != AssociatedLoopPathContract::LoopTransformation ||
          !isAllowedLoopTransformationWrapper(wrapper)) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[%s]: OpenMP wrapper=%p kind=%s "
                "is not admitted by associated-loop path contract=%d\n",
                contract, static_cast<void *>(wrapper),
                wrapper->sage_class_name(), static_cast<int>(path_contract));
        ROSE_ABORT();
      }
      next = wrapper->get_body();
      if (next == nullptr) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[%s]: loop-transformation "
                "wrapper=%p kind=%s has no exact body\n",
                contract, static_cast<void *>(wrapper),
                wrapper->sage_class_name());
        ROSE_ABORT();
      }
    } else {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[%s]: associated-loop path reaches "
              "unsupported node=%p kind=%s\n",
              contract, static_cast<void *>(cursor), cursor->sage_class_name());
      ROSE_ABORT();
    }
    requireOneExactStructuralEdge(cursor, next, contract);
    cursor = next;
  }
}

SgForStatement *requireExactAssociatedForLoop(
    SgOmpBodyStatement *directive, SgStatement *expected_root,
    AssociatedLoopPathContract path_contract, const char *contract) {
  SgStatement *loop = requireExactAssociatedLoop(directive, expected_root,
                                                 path_contract, contract);
  if (SgForStatement *for_loop = isSgForStatement(loop)) {
    return for_loop;
  }
  fprintf(stderr,
          "REX_OMP_LOWERING_INVARIANT[%s]: associated loop=%p kind=%s is "
          "not one exact C/C++ for loop\n",
          contract, static_cast<void *>(loop), loop->sage_class_name());
  ROSE_ABORT();
}

namespace {
void replaceLoopTransformationWithBody(SgOmpBodyStatement *target,
                                       const char *contract) {
  ROSE_ASSERT(target != nullptr);
  SgStatement *body = target->get_body();
  if (body == nullptr || body->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: replacement body has no exact "
            "directive ownership\n",
            contract);
    ROSE_ABORT();
  }
  target->set_body(nullptr);
  body->set_parent(nullptr);
  replaceStatement(target, body, true);
}

struct IntegralTypeContract {
  unsigned width;
  bool is_unsigned;
  bool is_bool;
};

size_t targetTypeSizeBytes(SgType *type, const SgNode *context) {
  SgProject *project =
      context != nullptr ? SageInterface::getProject(context) : nullptr;
  if (type == nullptr || project == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[integral-constant-abi]: type=%p "
            "context=%p has no exact target ABI\n",
            static_cast<void *>(type), static_cast<const void *>(context));
    ROSE_ABORT();
  }
  StructLayoutInfo layout;
  if (project->get_mode_32_bit()) {
    I386PrimitiveTypeLayoutGenerator primitive(nullptr);
    NonpackedTypeLayoutGenerator generator(&primitive);
    layout = generator.layoutType(type);
  } else {
    X86_64PrimitiveTypeLayoutGenerator primitive(nullptr);
    NonpackedTypeLayoutGenerator generator(&primitive);
    layout = generator.layoutType(type);
  }
  if (layout.size == 0) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[integral-constant-abi]: type=%p "
            "kind=%s has no exact target size\n",
            static_cast<void *>(type), type->sage_class_name());
    ROSE_ABORT();
  }
  return layout.size;
}

unsigned targetIntegralWidth(SgType *type, const SgNode *context) {
  const size_t size = targetTypeSizeBytes(type, context);
  if (size > 16) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[integral-constant-abi]: type=%p "
            "kind=%s has unsupported integral size=%zu\n",
            static_cast<void *>(type), type->sage_class_name(), size);
    ROSE_ABORT();
  }
  return static_cast<unsigned>(size * 8);
}

std::optional<IntegralTypeContract>
integralTypeContract(SgType *type, const SgNode *context) {
  if (type == nullptr)
    return std::nullopt;
  type =
      type->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  if (SgEnumType *enum_type = isSgEnumType(type)) {
    SgEnumDeclaration *declaration =
        isSgEnumDeclaration(enum_type->get_declaration());
    if (declaration == nullptr || declaration->get_field_type() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[integral-constant-enum]: enum "
              "type=%p has no exact underlying type\n",
              static_cast<void *>(enum_type));
      ROSE_ABORT();
    }
    SgType *field_type = declaration->get_field_type();
    return integralTypeContract(field_type, context);
  }
  if (isSgTypeBool(type))
    return IntegralTypeContract{1, true, true};
  if (isSgTypeSigned128bitInteger(type))
    return IntegralTypeContract{128, false, false};
  if (isSgTypeUnsigned128bitInteger(type))
    return IntegralTypeContract{128, true, false};
  if (isSgTypeChar(type) || isSgTypeWchar(type)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[integral-constant-abi]: type=%p "
            "kind=%s has no persisted target signedness contract\n",
            static_cast<void *>(type), type->sage_class_name());
    ROSE_ABORT();
  }
  const unsigned width = targetIntegralWidth(type, context);
  if (isSgTypeSignedChar(type))
    return IntegralTypeContract{width, false, false};
  if (isSgTypeUnsignedChar(type) || isSgTypeChar8(type))
    return IntegralTypeContract{width, true, false};
  if (isSgTypeChar16(type))
    return IntegralTypeContract{width, true, false};
  if (isSgTypeChar32(type))
    return IntegralTypeContract{width, true, false};
  if (isSgTypeShort(type) || isSgTypeSignedShort(type))
    return IntegralTypeContract{width, false, false};
  if (isSgTypeUnsignedShort(type))
    return IntegralTypeContract{width, true, false};
  if (isSgTypeInt(type) || isSgTypeSignedInt(type))
    return IntegralTypeContract{width, false, false};
  if (isSgTypeUnsignedInt(type))
    return IntegralTypeContract{width, true, false};
  if (isSgTypeLong(type) || isSgTypeSignedLong(type))
    return IntegralTypeContract{width, false, false};
  if (isSgTypeUnsignedLong(type))
    return IntegralTypeContract{width, true, false};
  if (isSgTypeLongLong(type) || isSgTypeSignedLongLong(type))
    return IntegralTypeContract{width, false, false};
  if (isSgTypeUnsignedLongLong(type))
    return IntegralTypeContract{width, true, false};
  return std::nullopt;
}

struct ExactIntegralConstant {
  unsigned __int128 bits;
  IntegralTypeContract type;
};

unsigned __int128 integralMask(unsigned width) {
  ROSE_ASSERT(width > 0 && width <= 128);
  return width == 128 ? ~static_cast<unsigned __int128>(0)
                      : (static_cast<unsigned __int128>(1) << width) - 1;
}

unsigned __int128 integralSignBit(unsigned width) {
  ROSE_ASSERT(width > 0 && width <= 128);
  return static_cast<unsigned __int128>(1) << (width - 1);
}

ExactIntegralConstant normalizedIntegralConstant(unsigned __int128 bits,
                                                 IntegralTypeContract type) {
  if (type.is_bool)
    bits = bits != 0 ? 1 : 0;
  return {bits & integralMask(type.width), type};
}

bool isNegative(const ExactIntegralConstant &value) {
  return !value.type.is_unsigned &&
         (value.bits & integralSignBit(value.type.width)) != 0;
}

__int128 signedIntegralValue(const ExactIntegralConstant &value) {
  ROSE_ASSERT(!value.type.is_unsigned);
  if (value.type.width == 128)
    return static_cast<__int128>(value.bits);
  const unsigned __int128 sign = integralSignBit(value.type.width);
  return (value.bits & sign) != 0 ? static_cast<__int128>(value.bits) -
                                        static_cast<__int128>(sign << 1)
                                  : static_cast<__int128>(value.bits);
}

__int128 minimumSignedIntegralValue(unsigned width) {
  ROSE_ASSERT(width > 0 && width <= 128);
  if (width == 128)
    return static_cast<__int128>(integralSignBit(128));
  return -static_cast<__int128>(integralSignBit(width));
}

__int128 maximumSignedIntegralValue(unsigned width) {
  ROSE_ASSERT(width > 0 && width <= 128);
  return static_cast<__int128>(integralSignBit(width) - 1);
}

ExactIntegralConstant
convertIntegralConstant(const ExactIntegralConstant &value,
                        IntegralTypeContract type) {
  if (type.is_bool)
    return normalizedIntegralConstant(value.bits != 0, type);
  unsigned __int128 bits = value.bits;
  if (type.width > value.type.width && isNegative(value))
    bits |= ~integralMask(value.type.width);
  return normalizedIntegralConstant(bits, type);
}

template <class SignedValue>
ExactIntegralConstant signedIntegralConstant(SignedValue value,
                                             IntegralTypeContract type) {
  return normalizedIntegralConstant(
      static_cast<unsigned __int128>(static_cast<__int128>(value)), type);
}

template <class UnsignedValue>
ExactIntegralConstant unsignedIntegralConstant(UnsignedValue value,
                                               IntegralTypeContract type) {
  return normalizedIntegralConstant(static_cast<unsigned __int128>(value),
                                    type);
}

std::optional<ExactIntegralConstant>
evaluateTypedIntegralConstant(SgExpression *expression,
                              std::unordered_set<SgInitializedName *> &active) {
  if (expression == nullptr)
    return std::nullopt;
  if (SgMacroExpansionExp *macro = isSgMacroExpansionExp(expression)) {
    SgExpression *expanded = macro->get_expanded_expression_checked();
    requireOneExactStructuralEdge(macro, expanded, "integral-constant");
    return evaluateTypedIntegralConstant(expanded, active);
  }

  const std::optional<IntegralTypeContract> result_type =
      integralTypeContract(expression->get_type(), expression);
  if (!result_type)
    return std::nullopt;

  if (SgBoolValExp *value = isSgBoolValExp(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgCharVal *value = isSgCharVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgSignedCharVal *value = isSgSignedCharVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgUnsignedCharVal *value = isSgUnsignedCharVal(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgWcharVal *value = isSgWcharVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgChar16Val *value = isSgChar16Val(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgChar32Val *value = isSgChar32Val(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgShortVal *value = isSgShortVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgUnsignedShortVal *value = isSgUnsignedShortVal(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgIntVal *value = isSgIntVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgUnsignedIntVal *value = isSgUnsignedIntVal(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgLongIntVal *value = isSgLongIntVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgUnsignedLongVal *value = isSgUnsignedLongVal(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgLongLongIntVal *value = isSgLongLongIntVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);
  if (SgUnsignedLongLongIntVal *value = isSgUnsignedLongLongIntVal(expression))
    return unsignedIntegralConstant(value->get_value(), *result_type);
  if (SgEnumVal *value = isSgEnumVal(expression))
    return signedIntegralConstant(value->get_value(), *result_type);

  if (SgSizeOfOp *size_of = isSgSizeOfOp(expression)) {
    SgExpression *operand_expression = size_of->get_operand_expr();
    SgType *operand_type = size_of->get_operand_type();
    if (operand_expression != nullptr) {
      if (operand_expression->get_parent() != size_of)
        return std::nullopt;
      operand_type = operand_expression->get_type();
    }
    if (operand_type == nullptr)
      return std::nullopt;
    const size_t size = targetTypeSizeBytes(operand_type, size_of);
    return unsignedIntegralConstant(size, *result_type);
  }

  if (SgVarRefExp *reference = isSgVarRefExp(expression)) {
    SgVariableSymbol *symbol = isSgVariableSymbol(reference->get_symbol());
    SgInitializedName *name =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    SgVariableDeclaration *declaration =
        name != nullptr ? isSgVariableDeclaration(name->get_declaration())
                        : nullptr;
    SgSourceFile *source_file =
        SageInterface::getEnclosingSourceFile(reference, true);
    SgProject *reference_project = SageInterface::getProject(reference);
    const bool exact_declaration =
        declaration != nullptr && name->get_type() != nullptr &&
        name->get_parent() == declaration &&
        std::count(declaration->get_variables().begin(),
                   declaration->get_variables().end(), name) == 1 &&
        name->get_scope() != nullptr &&
        declaration->get_scope() == name->get_scope() &&
        symbol->get_scope() == name->get_scope() &&
        name->get_scope()->find_symbol_from_declaration(name) == symbol &&
        reference_project != nullptr &&
        SageInterface::getProject(name) == reference_project;
    // Only C++ declaration-backed integral constants are admitted here.  A C
    // const-qualified object is not an integer constant expression.  C++
    // constexpr supplies the immutability contract even when the frontend
    // intentionally preserves the source-spelled type without its implicit
    // top-level const qualifier.
    const bool immutable_declaration =
        exact_declaration && source_file != nullptr &&
        source_file->get_Cxx_only() &&
        (SageInterface::isConstType(name->get_type()) ||
         declaration->get_is_constexpr());
    if (!immutable_declaration || name->get_type() == nullptr ||
        SageInterface::isVolatileType(name->get_type()) ||
        !SageInterface::isEquivalentType(reference->get_type(),
                                         name->get_type()) ||
        name->get_initializer() == nullptr ||
        name->get_initializer()->get_parent() != name) {
      return std::nullopt;
    }
    if (!active.insert(name).second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[integral-constant-cycle]: "
              "declaration=%p participates in a constant-expression cycle\n",
              static_cast<void *>(name));
      ROSE_ABORT();
    }
    SgInitializer *initializer = name->get_initializer();
    SgExpression *operand = nullptr;
    SgNode *operand_owner = initializer;
    if (SgAssignInitializer *assign = isSgAssignInitializer(initializer)) {
      operand = assign->get_operand_i();
    } else if (SgAggregateInitializer *aggregate =
                   isSgAggregateInitializer(initializer)) {
      SgExprListExp *values = aggregate->get_initializers();
      if (values != nullptr && values->get_parent() == aggregate &&
          values->get_expressions().size() == 1) {
        operand = values->get_expressions().front();
        operand_owner = values;
      }
    }
    if (operand == nullptr || operand->get_parent() != operand_owner) {
      active.erase(name);
      return std::nullopt;
    }
    std::optional<ExactIntegralConstant> value =
        evaluateTypedIntegralConstant(operand, active);
    active.erase(name);
    return value ? std::optional<ExactIntegralConstant>(
                       convertIntegralConstant(*value, *result_type))
                 : std::nullopt;
  }

  if (SgCastExp *cast = isSgCastExp(expression)) {
    SgExpression *operand = cast->get_operand_i();
    if (operand == nullptr || operand->get_parent() != cast)
      return std::nullopt;
    std::optional<ExactIntegralConstant> value =
        evaluateTypedIntegralConstant(operand, active);
    return value ? std::optional<ExactIntegralConstant>(
                       convertIntegralConstant(*value, *result_type))
                 : std::nullopt;
  }

  if (SgUnaryOp *unary = isSgUnaryOp(expression)) {
    SgExpression *operand = unary->get_operand_i();
    if (operand == nullptr || operand->get_parent() != unary)
      return std::nullopt;
    std::optional<ExactIntegralConstant> value =
        evaluateTypedIntegralConstant(operand, active);
    if (!value)
      return std::nullopt;
    ExactIntegralConstant converted =
        convertIntegralConstant(*value, *result_type);
    if (isSgUnaryAddOp(unary))
      return converted;
    if (isSgMinusOp(unary)) {
      if (!result_type->is_unsigned &&
          signedIntegralValue(converted) ==
              minimumSignedIntegralValue(result_type->width))
        return std::nullopt;
      return normalizedIntegralConstant(-converted.bits, *result_type);
    }
    if (isSgBitComplementOp(unary))
      return normalizedIntegralConstant(~converted.bits, *result_type);
    if (isSgNotOp(unary))
      return normalizedIntegralConstant(converted.bits == 0, *result_type);
    return std::nullopt;
  }

  if (SgConditionalExp *conditional = isSgConditionalExp(expression)) {
    conditional->validate();
    SgExpression *condition = conditional->get_conditional_exp();
    SgExpression *true_expression = conditional->get_true_exp();
    SgExpression *false_expression = conditional->get_false_exp();
    std::optional<ExactIntegralConstant> condition_value =
        evaluateTypedIntegralConstant(condition, active);
    if (!condition_value)
      return std::nullopt;
    if (condition_value->bits != 0 &&
        conditional->get_operator_kind() ==
            SgConditionalExp::e_conditional_operator_gnu_binary)
      return convertIntegralConstant(*condition_value, *result_type);
    std::optional<ExactIntegralConstant> selected =
        evaluateTypedIntegralConstant(
            condition_value->bits == 0 ? false_expression : true_expression,
            active);
    return selected ? std::optional<ExactIntegralConstant>(
                          convertIntegralConstant(*selected, *result_type))
                    : std::nullopt;
  }

  SgBinaryOp *binary = isSgBinaryOp(expression);
  if (binary == nullptr || binary->get_lhs_operand() == nullptr ||
      binary->get_rhs_operand() == nullptr ||
      binary->get_lhs_operand()->get_parent() != binary ||
      binary->get_rhs_operand()->get_parent() != binary)
    return std::nullopt;
  std::optional<ExactIntegralConstant> left =
      evaluateTypedIntegralConstant(binary->get_lhs_operand(), active);
  if (!left)
    return std::nullopt;

  if (isSgAndOp(binary) != nullptr && left->bits == 0)
    return normalizedIntegralConstant(0, *result_type);
  if (isSgOrOp(binary) != nullptr && left->bits != 0)
    return normalizedIntegralConstant(1, *result_type);

  std::optional<ExactIntegralConstant> right =
      evaluateTypedIntegralConstant(binary->get_rhs_operand(), active);
  if (!right)
    return std::nullopt;
  if (isSgAndOp(binary) != nullptr || isSgOrOp(binary) != nullptr)
    return normalizedIntegralConstant(right->bits != 0, *result_type);

  if (isSgEqualityOp(binary) != nullptr || isSgNotEqualOp(binary) != nullptr ||
      isSgLessThanOp(binary) != nullptr ||
      isSgLessOrEqualOp(binary) != nullptr ||
      isSgGreaterThanOp(binary) != nullptr ||
      isSgGreaterOrEqualOp(binary) != nullptr) {
    IntegralTypeContract common{};
    if (left->type.is_unsigned == right->type.is_unsigned) {
      common = left->type.width >= right->type.width ? left->type : right->type;
    } else {
      const IntegralTypeContract unsigned_type =
          left->type.is_unsigned ? left->type : right->type;
      const IntegralTypeContract signed_type =
          left->type.is_unsigned ? right->type : left->type;
      common = signed_type.width > unsigned_type.width
                   ? signed_type
                   : IntegralTypeContract{
                         std::max(signed_type.width, unsigned_type.width), true,
                         false};
    }
    common.is_bool = false;
    const ExactIntegralConstant comparison_lhs =
        convertIntegralConstant(*left, common);
    const ExactIntegralConstant comparison_rhs =
        convertIntegralConstant(*right, common);
    bool result = false;
    if (isSgEqualityOp(binary) != nullptr)
      result = comparison_lhs.bits == comparison_rhs.bits;
    else if (isSgNotEqualOp(binary) != nullptr)
      result = comparison_lhs.bits != comparison_rhs.bits;
    else if (common.is_unsigned) {
      if (isSgLessThanOp(binary) != nullptr)
        result = comparison_lhs.bits < comparison_rhs.bits;
      else if (isSgLessOrEqualOp(binary) != nullptr)
        result = comparison_lhs.bits <= comparison_rhs.bits;
      else if (isSgGreaterThanOp(binary) != nullptr)
        result = comparison_lhs.bits > comparison_rhs.bits;
      else
        result = comparison_lhs.bits >= comparison_rhs.bits;
    } else {
      const __int128 lhs_value = signedIntegralValue(comparison_lhs);
      const __int128 rhs_value = signedIntegralValue(comparison_rhs);
      if (isSgLessThanOp(binary) != nullptr)
        result = lhs_value < rhs_value;
      else if (isSgLessOrEqualOp(binary) != nullptr)
        result = lhs_value <= rhs_value;
      else if (isSgGreaterThanOp(binary) != nullptr)
        result = lhs_value > rhs_value;
      else
        result = lhs_value >= rhs_value;
    }
    return normalizedIntegralConstant(result, *result_type);
  }

  ExactIntegralConstant lhs = convertIntegralConstant(*left, *result_type);
  if (isSgLshiftOp(binary) || isSgRshiftOp(binary)) {
    if (isNegative(*right) || right->bits > UINT64_MAX)
      return std::nullopt;
    const uint64_t amount = static_cast<uint64_t>(right->bits);
    if (amount >= result_type->width)
      return std::nullopt;
    if (isSgLshiftOp(binary)) {
      if (!result_type->is_unsigned && isNegative(lhs))
        return std::nullopt;
      if (!result_type->is_unsigned &&
          lhs.bits > static_cast<unsigned __int128>(
                         maximumSignedIntegralValue(result_type->width)) >>
              amount)
        return std::nullopt;
      return normalizedIntegralConstant(lhs.bits << amount, *result_type);
    }
    if (isNegative(lhs)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[integral-constant-shift]: "
              "negative signed right shift has no persisted target policy\n");
      ROSE_ABORT();
    }
    unsigned __int128 shifted = lhs.bits >> amount;
    return normalizedIntegralConstant(shifted, *result_type);
  }
  ExactIntegralConstant rhs = convertIntegralConstant(*right, *result_type);
  if (isSgAddOp(binary) || isSgSubtractOp(binary) || isSgMultiplyOp(binary)) {
    if (result_type->is_unsigned) {
      if (isSgAddOp(binary))
        return normalizedIntegralConstant(lhs.bits + rhs.bits, *result_type);
      if (isSgSubtractOp(binary))
        return normalizedIntegralConstant(lhs.bits - rhs.bits, *result_type);
      return normalizedIntegralConstant(lhs.bits * rhs.bits, *result_type);
    }
    const __int128 lhs_value = signedIntegralValue(lhs);
    const __int128 rhs_value = signedIntegralValue(rhs);
    __int128 result = 0;
    bool overflow = false;
    if (isSgAddOp(binary))
      overflow = __builtin_add_overflow(lhs_value, rhs_value, &result);
    else if (isSgSubtractOp(binary))
      overflow = __builtin_sub_overflow(lhs_value, rhs_value, &result);
    else
      overflow = __builtin_mul_overflow(lhs_value, rhs_value, &result);
    if (overflow || result < minimumSignedIntegralValue(result_type->width) ||
        result > maximumSignedIntegralValue(result_type->width))
      return std::nullopt;
    return normalizedIntegralConstant(static_cast<unsigned __int128>(result),
                                      *result_type);
  }
  if (isSgDivideOp(binary) || isSgModOp(binary)) {
    if (rhs.bits == 0)
      return std::nullopt;
    if (result_type->is_unsigned)
      return normalizedIntegralConstant(
          isSgDivideOp(binary) ? lhs.bits / rhs.bits : lhs.bits % rhs.bits,
          *result_type);
    const __int128 lhs_value = signedIntegralValue(lhs);
    const __int128 rhs_value = signedIntegralValue(rhs);
    if (lhs_value == minimumSignedIntegralValue(result_type->width) &&
        rhs_value == -1)
      return std::nullopt;
    return normalizedIntegralConstant(
        static_cast<unsigned __int128>(isSgDivideOp(binary)
                                           ? lhs_value / rhs_value
                                           : lhs_value % rhs_value),
        *result_type);
  }
  if (isSgBitAndOp(binary))
    return normalizedIntegralConstant(lhs.bits & rhs.bits, *result_type);
  if (isSgBitOrOp(binary))
    return normalizedIntegralConstant(lhs.bits | rhs.bits, *result_type);
  if (isSgBitXorOp(binary))
    return normalizedIntegralConstant(lhs.bits ^ rhs.bits, *result_type);
  return std::nullopt;
}

size_t requireFullUnrollFactor(
    const SageInterface::CheckedCanonicalLoopPlan &loop_plan) {
  const std::optional<unsigned long long> trip_count =
      SageInterface::exactCanonicalLoopTripCount(loop_plan);
  if (!trip_count) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-factor]: full unroll requires "
            "one exact target-typed constant trip count\n");
    ROSE_ABORT();
  }
  if (*trip_count > static_cast<unsigned long long>(SIZE_MAX)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-factor]: exact trip count "
            "cannot be represented as a transformation factor\n");
    ROSE_ABORT();
  }
  // A zero- or one-trip loop is already fully unrolled semantically; factor
  // one is the checked identity representation and preserves the loop header.
  return *trip_count <= 1 ? 1 : static_cast<size_t>(*trip_count);
}

} // namespace

void requireExactIntegralConstantExpression(SgExpression *expression,
                                            const char *contract) {
  if (expression == nullptr || contract == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[integral-constant-request]: "
            "expression=%p contract=%s is invalid\n",
            static_cast<void *>(expression),
            contract != nullptr ? contract : "null");
    ROSE_ABORT();
  }
  std::unordered_set<SgInitializedName *> active;
  if (!evaluateTypedIntegralConstant(expression, active)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: expression=%p kind=%s is not "
            "one exact target-ABI integral constant expression\n",
            contract, static_cast<void *>(expression),
            expression->sage_class_name());
    ROSE_ABORT();
  }
}

unsigned long long
requireExactPositiveIntegralConstant(SgExpression *expression,
                                     unsigned long long maximum,
                                     const char *contract) {
  if (expression == nullptr || maximum == 0 || contract == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[integral-constant-request]: "
            "expression=%p maximum=%llu contract=%s is invalid\n",
            static_cast<void *>(expression), maximum,
            contract != nullptr ? contract : "null");
    ROSE_ABORT();
  }
  std::unordered_set<SgInitializedName *> active;
  const std::optional<ExactIntegralConstant> value =
      evaluateTypedIntegralConstant(expression, active);
  unsigned __int128 magnitude = 0;
  if (value) {
    if (value->type.is_unsigned) {
      magnitude = value->bits;
    } else {
      const __int128 signed_value = signedIntegralValue(*value);
      if (signed_value > 0)
        magnitude = static_cast<unsigned __int128>(signed_value);
    }
  }
  if (!value || magnitude == 0 ||
      magnitude > static_cast<unsigned __int128>(maximum)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[%s]: expression=%p kind=%s is not "
            "one exact positive target-ABI integral constant in [1,%llu]\n",
            contract, static_cast<void *>(expression),
            expression->sage_class_name(), maximum);
    ROSE_ABORT();
  }
  return static_cast<unsigned long long>(magnitude);
}

//! Add __thread for each threadprivate variable's declaration statement and
//! remove the #pragma omp threadprivate(...)
void transOmpThreadprivate(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpThreadprivateStatement *target = isSgOmpThreadprivateStatement(node);
  ROSE_ASSERT(target != NULL);

  const SgExpressionPtrList &nameList = target->get_variables();
  if (nameList.empty()) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: "
                    "directive has no exact variable list\n");
    ROSE_ABORT();
  }

  auto prepare_fortran_parallel_uses = [target](SgInitializedName
                                                    *threadprivate_var) {
    if (threadprivate_var == nullptr ||
        threadprivate_var->get_declaration() == nullptr) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: "
                      "Fortran variable has no exact declaration\n");
      ROSE_ABORT();
    }
    SgProject *project = SageInterface::getProject(target);
    if (project == nullptr) {
      fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: "
                      "directive has no exact project owner\n");
      ROSE_ABORT();
    }
    for (SgNode *parallel_node :
         NodeQuery::querySubTree(project, V_SgOmpParallelStatement)) {
      SgOmpParallelStatement *parallel =
          isSgOmpParallelStatement(parallel_node);
      if (parallel == nullptr ||
          isOmpContextSelectorMetadataDirective(parallel))
        continue;

      bool referenced = false;
      SgStatement *body = parallel->get_body();
      if (body == nullptr) {
        fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: "
                        "parallel directive has no exact body\n");
        ROSE_ABORT();
      }
      for (SgNode *reference_node :
           NodeQuery::querySubTree(body, V_SgVarRefExp)) {
        SgVarRefExp *reference = isSgVarRefExp(reference_node);
        SgVariableSymbol *symbol =
            reference != nullptr ? reference->get_symbol() : nullptr;
        if (symbol == nullptr || symbol->get_declaration() == nullptr) {
          fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: "
                          "parallel body contains a variable reference without "
                          "exact identity\n");
          ROSE_ABORT();
        }
        if (symbol->get_declaration() == threadprivate_var) {
          referenced = true;
          break;
        }
      }

      const bool is_copyin = isInClauseVariableList(threadprivate_var, parallel,
                                                    V_SgOmpCopyinClause);
      if (!referenced && !is_copyin)
        continue;

      for (VariantT incompatible :
           {V_SgOmpPrivateClause, V_SgOmpFirstprivateClause,
            V_SgOmpLastprivateClause, V_SgOmpSharedClause,
            V_SgOmpReductionClause, V_SgOmpCopyprivateClause}) {
        if (isInClauseVariableList(threadprivate_var, parallel, incompatible)) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[threadprivate-clause]: "
                  "Fortran threadprivate variable '%s' appears in "
                  "incompatible clause variant=%d\n",
                  threadprivate_var->get_name().getString().c_str(),
                  static_cast<int>(incompatible));
          ROSE_ABORT();
        }
      }
      if (!is_copyin)
        addClauseVariable(threadprivate_var, parallel, V_SgOmpPrivateClause);
    }
  };

  for (size_t i = 0; i < nameList.size(); i++) {
    SgExpression *item = nameList[i];
    if (item == nullptr || item->get_parent() != target) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: item=%zu "
              "has no exact directive ownership\n",
              i);
      ROSE_ABORT();
    }
    if (SgFortranCommonBlockRefExp *common =
            isSgFortranCommonBlockRefExp(item)) {
      validateFortranCommonBlockRef(common);
      SgExprListExp *members =
          common->get_common_block()->get_variable_reference_list();
      if (members == nullptr || members->get_expressions().empty())
        ROSE_ABORT();
      for (SgExpression *member : members->get_expressions()) {
        SgVarRefExp *member_ref = isSgVarRefExp(member);
        if (member_ref == nullptr || member_ref->get_symbol() == nullptr)
          ROSE_ABORT();
        SgInitializedName *member_name =
            member_ref->get_symbol()->get_declaration();
        if (member_name == nullptr ||
            isSgVariableDeclaration(member_name->get_declaration()) == nullptr)
          ROSE_ABORT();
        prepare_fortran_parallel_uses(member_name);
      }
      continue;
    }
    SgVarRefExp *vref = isSgVarRefExp(item);
    if (vref == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: item=%zu "
              "is %s instead of one exact variable reference\n",
              i, item->sage_class_name());
      ROSE_ABORT();
    }
    if (vref->get_symbol() == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: item=%zu "
              "has no exact variable symbol\n",
              i);
      ROSE_ABORT();
    }
    SgInitializedName *init_name = vref->get_symbol()->get_declaration();
    if (init_name == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: item=%zu "
              "symbol has no declaration\n",
              i);
      ROSE_ABORT();
    }
    SgVariableDeclaration *decl =
        isSgVariableDeclaration(init_name->get_declaration());
    if (decl == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-variable]: item=%zu "
              "does not resolve to an exact variable declaration\n",
              i);
      ROSE_ABORT();
    }
    if (SageInterface::is_Fortran_language()) {
      prepare_fortran_parallel_uses(init_name);
    } else {
      decl->get_declarationModifier()
          .get_storageModifier()
          .set_thread_local_storage(true);
    }
  }

  AttachedPreprocessingInfoType *attached =
      target->getAttachedPreprocessingInfo();
  if (attached != nullptr && !attached->empty()) {
    SgScopeStatement *scope = target->get_scope();
    ROSE_ASSERT(scope != nullptr);
    const SgStatementPtrList statements = scope->generateStatementList();
    SgStatementPtrList::const_iterator position =
        std::find(statements.begin(), statements.end(), target);
    if (position == statements.end() ||
        std::find(std::next(position), statements.end(), target) !=
            statements.end()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-preprocessing]: "
              "directive is not uniquely owned by its exact scope\n");
      ROSE_ABORT();
    }
    ++position;
    if (position == statements.end() || *position == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[threadprivate-preprocessing]: "
              "terminal directive has %zu preprocessing records with no "
              "exact relocation target\n",
              attached->size());
      ROSE_ABORT();
    }
    for (PreprocessingInfo *record : *attached) {
      const PreprocessingInfo::RelativePositionType relative_position =
          record != nullptr ? record->getRelativePosition()
                            : PreprocessingInfo::undef;
      if (relative_position != PreprocessingInfo::before &&
          relative_position != PreprocessingInfo::after &&
          relative_position != PreprocessingInfo::before_syntax &&
          relative_position != PreprocessingInfo::after_syntax) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[threadprivate-preprocessing]: "
                "directive owns record=%p with non-exterior position=%d\n",
                static_cast<void *>(record),
                static_cast<int>(relative_position));
        ROSE_ABORT();
      }
    }
    // Every preprocessing record attached to a standalone threadprivate
    // directive is lexically exterior to it.  Once the directive is erased,
    // both its leading records (for example #ifdef) and trailing records (for
    // example #endif) precede the next statement.  Transfer the complete,
    // source-ordered surface in one publication transaction.
    movePreprocessingInfo(target, *position, PreprocessingInfo::undef,
                          PreprocessingInfo::before, true);
  }

  removeStatement(target, false);
}

//! Lowers the OMP unroll statement
void transOmpUnroll(SgNode *node) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-owner]: directive is null\n");
    ROSE_ABORT();
  }
  SgOmpUnrollStatement *target = isSgOmpUnrollStatement(node);
  SgNode *target_parent = target != nullptr ? target->get_parent() : nullptr;
  if (target == nullptr || target->get_scope() == nullptr ||
      target_parent == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-owner]: node=%p is not one "
            "attached scoped unroll directive\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }
  requireOneExactStructuralEdge(target_parent, target, "unroll-owner");

  SgStatement *body = target->get_body();
  SgForStatement *for_loop = requireExactAssociatedForLoop(
      target, body, AssociatedLoopPathContract::LoopTransformation,
      "unroll-loop");
  const SageInterface::CheckedCanonicalLoopPlan canonical_loop =
      SageInterface::requireCheckedCanonicalLoopPlan(for_loop, "omp-unroll");

  SgOmpClauseList *clause_list = getOmpClauseList(target);
  if (clause_list == nullptr || clause_list->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-clause]: directive has no "
            "exact clause-list ownership\n");
    ROSE_ABORT();
  }
  const SgOmpClausePtrList &clauses = clause_list->get_clauses();
  if (clauses.size() != 1 || clauses.front() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-clause]: directive has %zu "
            "clauses instead of one exact full or partial clause\n",
            clauses.size());
    ROSE_ABORT();
  }
  SgOmpClause *clause = clauses.front();
  if (clause->get_parent() != clause_list ||
      std::count(clauses.begin(), clauses.end(), clause) != 1) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[unroll-clause]: clause has "
                    "no exact clause-list ownership\n");
    ROSE_ABORT();
  }
  size_t factor = 0;
  if (clause->variantT() == V_SgOmpFullClause) {
    factor = requireFullUnrollFactor(canonical_loop);
  } else if (clause->variantT() == V_SgOmpPartialClause) {
    SgOmpPartialClause *partial = static_cast<SgOmpPartialClause *>(clause);
    SgExpression *partial_expr = partial->get_expression();
    if (partial_expr == nullptr || partial_expr->get_parent() != partial) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[unroll-factor]: partial clause has "
              "no exact expression edge\n");
      ROSE_ABORT();
    }
    requireOneExactStructuralEdge(partial, partial_expr, "unroll-factor");
    factor = static_cast<size_t>(requireExactPositiveIntegralConstant(
        partial_expr, static_cast<unsigned long long>(SIZE_MAX),
        "unroll-factor"));
  } else {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[unroll-clause]: unsupported clause "
            "kind=%s\n",
            clause->sage_class_name());
    ROSE_ABORT();
  }

  const SageInterface::CheckedLoopUnrollPlan unroll_plan =
      SageInterface::requireCheckedLoopUnrollPlan(for_loop, factor,
                                                  "omp-unroll");
  SageInterface::commitLoopUnrolling(unroll_plan);
  replaceLoopTransformationWithBody(target, "unroll-owner");
}

//! Lowers the OMP tile statement
// Yes, this is basically the same as the unroll
void transOmpTile(SgNode *node) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-owner]: directive is null\n");
    ROSE_ABORT();
  }
  SgOmpTileStatement *target = isSgOmpTileStatement(node);
  if (target == nullptr || target->get_scope() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-owner]: node=%p is not one "
            "scoped tile directive\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }
  SgStatement *body = target->get_body();
  SgForStatement *for_loop = requireExactAssociatedForLoop(
      target, body, AssociatedLoopPathContract::LoopTransformation,
      "tile-loop");
  SgStatement *target_parent = isSgStatement(target->get_parent());
  if (target_parent == nullptr) {
    fprintf(stderr, "REX_OMP_LOWERING_INVARIANT[tile-owner]: directive has "
                    "no exact statement owner\n");
    ROSE_ABORT();
  }
  requireOneExactStructuralEdge(target_parent, target, "tile-owner");

  SgOmpClauseList *clause_list = getOmpClauseList(target);
  if (clause_list == nullptr || clause_list->get_parent() != target) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-sizes]: directive has no exact "
            "clause-list ownership\n");
    ROSE_ABORT();
  }
  const SgOmpClausePtrList &clauses = clause_list->get_clauses();
  if (clauses.size() != 1 || clauses.front() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-sizes]: tile directive has "
            "%zu clauses instead of one exact sizes clause\n",
            clauses.size());
    ROSE_ABORT();
  }
  SgOmpClause *clause = clauses.front();
  if (clause->get_parent() != clause_list ||
      std::count(clauses.begin(), clauses.end(), clause) != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-sizes]: sizes clause has no "
            "exact clause-list ownership\n");
    ROSE_ABORT();
  }
  SgOmpSizesClause *sizes = isSgOmpSizesClause(clause);
  if (sizes == NULL) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-sizes]: tile directive clause "
            "is not SgOmpSizesClause\n");
    ROSE_ABORT();
  }
  SgExprListExp *list = isSgExprListExp(sizes->get_expression());
  if (list == NULL || list->get_parent() != sizes ||
      list->get_expressions().empty()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[tile-sizes]: sizes clause has no "
            "nonempty exact expression list\n");
    ROSE_ABORT();
  }
  requireOneExactStructuralEdge(sizes, list, "tile-sizes");

  std::vector<size_t> tile_sizes;
  tile_sizes.reserve(list->get_expressions().size());
  for (size_t level = 0; level < list->get_expressions().size(); ++level) {
    SgExpression *size_expression = list->get_expressions()[level];
    if (size_expression == nullptr || size_expression->get_parent() != list ||
        std::count(list->get_expressions().begin(),
                   list->get_expressions().end(), size_expression) != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[tile-size]: loop level=%zu has no "
              "one exact size-expression edge\n",
              level + 1);
      ROSE_ABORT();
    }
    requireOneExactStructuralEdge(list, size_expression, "tile-size");
    tile_sizes.push_back(
        static_cast<size_t>(requireExactPositiveIntegralConstant(
            size_expression, static_cast<unsigned long long>(SIZE_MAX),
            "tile-size")));
  }

  const SageInterface::CheckedLoopTilingPlan tiling_plan =
      SageInterface::requireCheckedLoopTilingPlan(for_loop, tile_sizes,
                                                  "omp-tile");
  SageInterface::commitLoopTiling(tiling_plan);
  replaceLoopTransformationWithBody(target, "tile-owner");
}

//! Collect variables from OpenMP clauses: including private, firstprivate,
//! lastprivate, reduction, etc.
SgInitializedNamePtrList collectClauseVariables(SgStatement *clause_stmt,
                                                const VariantT &vt) {
  return collectClauseVariables(clause_stmt, VariantVector(vt));
}

// Collect variables from an OpenMP clause: including private, firstprivate,
// lastprivate, reduction, etc.
SgInitializedNamePtrList collectClauseVariables(SgStatement *clause_stmt,
                                                const VariantVector &vvt) {
  SgInitializedNamePtrList result, result2;
  ROSE_ASSERT(clause_stmt != NULL);
  Rose_STL_Container<SgOmpClause *> p_clause = getClause(clause_stmt, vvt);
  for (size_t i = 0; i < p_clause.size();
       i++) // can have multiple reduction clauses of different reduction
            // operations
  {
    SgOmpVariablesClause *vars_clause = isSgOmpVariablesClause(p_clause[i]);
    if (vars_clause == NULL) {
      std::ostringstream requested_variants;
      for (VariantT requested_variant : vvt) {
        requested_variants << ' ' << static_cast<int>(requested_variant);
      }
      MLOG_ERROR_CXX("ompLowering")
          << "Requested OpenMP variable-clause kind is not represented by "
             "SgOmpVariablesClause: "
          << p_clause[i]->class_name() << " (variant "
          << static_cast<int>(p_clause[i]->variantT())
          << "); requested variants:" << requested_variants.str();
      ROSE_ABORT();
    }
    SgExprListExp *vars = vars_clause->get_variables();
    if (vars == NULL) {
      MLOG_ERROR_CXX("ompLowering")
          << "OpenMP variable clause has no expression list";
      ROSE_ABORT();
    }
    // get initialized name from varRefExp
    SgExpressionPtrList refs = vars->get_expressions();
    result2.clear();
    for (size_t j = 0; j < refs.size(); j++) {
      SgVariableSymbol *symbol = extractClauseVariableSymbol(refs[j]);
      if (symbol == nullptr) {
        if (SgFortranCommonBlockRefExp *common =
                isSgFortranCommonBlockRefExp(refs[j])) {
          validateFortranCommonBlockRef(common);
          SgExprListExp *members =
              common->get_common_block()->get_variable_reference_list();
          if (members == nullptr || members->get_expressions().empty()) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[common-block-clause]: /%s/ "
                    "has no exact member list\n",
                    common->get_use_name().getString().c_str());
            ROSE_ABORT();
          }
          for (SgExpression *member : members->get_expressions()) {
            SgVarRefExp *member_ref = isSgVarRefExp(member);
            SgVariableSymbol *member_symbol =
                member_ref != nullptr ? member_ref->get_symbol() : nullptr;
            if (member_symbol == nullptr ||
                member_symbol->get_declaration() == nullptr) {
              fprintf(stderr,
                      "REX_OMP_LOWERING_INVARIANT[common-block-clause]: /%s/ "
                      "contains a member without exact variable identity\n",
                      common->get_use_name().getString().c_str());
              ROSE_ABORT();
            }
            result2.push_back(member_symbol->get_declaration());
          }
          continue;
        }
      }
      if (symbol == NULL) {
        MLOG_ERROR_CXX("ompLowering")
            << "OpenMP variable clause contains an unresolved semantic "
               "expression";
        ROSE_ABORT();
      }
      ROSE_ASSERT(symbol->get_declaration() != NULL);
      result2.push_back(symbol->get_declaration());
    }
    std::copy(result2.begin(), result2.end(), back_inserter(result));
  }
  return result;
}

SgExpression *getClauseExpression(SgStatement *clause_stmt,
                                  const VariantVector &vvt) {
  SgExpression *expr = NULL;
  ROSE_ASSERT(clause_stmt != NULL);
  SgOmpClausePtrList clauses;
  if (isSgOmpClauseBodyStatement(clause_stmt)) {
    clauses = (isSgOmpClauseBodyStatement(clause_stmt))->get_clauses();
  } else if (isSgOmpClauseStatement(clause_stmt)) {
    clauses = (isSgOmpClauseStatement(clause_stmt))->get_clauses();
  } else {
    ROSE_ABORT();
  }
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clauses, vvt);
  // It is possible that the requested clauses are not found. We allow returning
  // NULL expression. Liao, 6/16/2015
  if (p_clause.size() >= 1)
    expr = isSgOmpExpressionClause(p_clause[0])->get_expression();
  return expr;
}

//! Collect all variables from OpenMP clauses associated with an omp statement:
//! private, reduction, etc
SgInitializedNamePtrList collectAllClauseVariables(SgStatement *clause_stmt) {
  ROSE_ASSERT(clause_stmt != NULL);

  VariantVector vvt;
  vvt.push_back(V_SgOmpCopyinClause);
  vvt.push_back(V_SgOmpCopyprivateClause);
  vvt.push_back(V_SgOmpFirstprivateClause);
  vvt.push_back(V_SgOmpLastprivateClause);
  vvt.push_back(V_SgOmpPrivateClause);
  vvt.push_back(V_SgOmpReductionClause);

  return collectClauseVariables(clause_stmt, vvt);
}

bool isInClauseVariableList(SgInitializedName *var,
                            SgOmpClauseBodyStatement *clause_stmt,
                            const VariantVector &vvt) {
  SgInitializedNamePtrList var_list = collectClauseVariables(clause_stmt, vvt);
  if (find(var_list.begin(), var_list.end(), var) != var_list.end())
    return true;
  else
    return false;
}

//! Return a reduction variable's reduction operation type
SgOmpClause::omp_reduction_identifier_enum
getReductionOperationType(SgInitializedName *init_name,
                          SgOmpClauseBodyStatement *clause_stmt) {
  SgOmpClause::omp_reduction_identifier_enum result =
      SgOmpClause::e_omp_reduction_unknown;
  bool found = false;
  ROSE_ASSERT(init_name != NULL);
  ROSE_ASSERT(clause_stmt != NULL);
  Rose_STL_Container<SgOmpClause *> p_clause =
      NodeQuery::queryNodeList<SgOmpClause>(clause_stmt->get_clauses(),
                                            V_SgOmpReductionClause);
  ROSE_ASSERT(p_clause.size() > 0); // must be have at least reduction clause

  for (size_t i = 0; i < p_clause.size();
       i++) // can have multiple reduction clauses of different reduction
            // operations
  {
    SgOmpReductionClause *r_clause = isSgOmpReductionClause(p_clause[i]);
    ROSE_ASSERT(r_clause != NULL);
    SgExpressionPtrList refs =
        isSgOmpVariablesClause(r_clause)->get_variables()->get_expressions();
    SgInitializedNamePtrList
        var_list; //= isSgOmpVariablesClause(r_clause)->get_variables();
    for (size_t j = 0; j < refs.size(); j++) {
      SgVariableSymbol *symbol = extractClauseVariableSymbol(refs[j]);
      if (symbol == NULL)
        continue;
      var_list.push_back(symbol->get_declaration());
    }
    SgInitializedNamePtrList::const_iterator iter =
        find(var_list.begin(), var_list.end(), init_name);
    if (iter != var_list.end()) {
      result = r_clause->get_identifier();
      found = true;
      break;
    }
  }
  // Must have a hit
  ROSE_ASSERT(found == true);
  return result;
}

//! Create an initial value according to reduction operator type
SgExpression *
createInitialValueExp(SgOmpClause::omp_reduction_identifier_enum r_operator) {
  SgExpression *result = NULL;
  switch (r_operator) {
  // 0: + - ! ^ ||  ior ieor
  case SgOmpClause::e_omp_reduction_plus:
  case SgOmpClause::e_omp_reduction_minus:
  case SgOmpClause::e_omp_reduction_bitor:
  case SgOmpClause::e_omp_reduction_bitxor:
  case SgOmpClause::e_omp_reduction_or:
  case SgOmpClause::e_omp_reduction_ior:
  case SgOmpClause::e_omp_reduction_ieor:
    result = buildIntVal(0);
    break;
  // 1: * &&
  case SgOmpClause::e_omp_reduction_mul:
  case SgOmpClause::e_omp_reduction_bitand:
    result = buildIntVal(1);
    break;
    // TODO
  case SgOmpClause::e_omp_reduction_logand:
  case SgOmpClause::e_omp_reduction_logor:
  case SgOmpClause::e_omp_reduction_and:
  case SgOmpClause::e_omp_reduction_eqv:
  case SgOmpClause::e_omp_reduction_neqv:
  case SgOmpClause::e_omp_reduction_max:
  case SgOmpClause::e_omp_reduction_min:
  case SgOmpClause::e_omp_reduction_iand:

  case SgOmpClause::e_omp_reduction_unknown:
  case SgOmpClause::e_omp_reduction_last:
  default:
    cerr << "Illegal or unhandled reduction operator kind: " << r_operator
         << endl;
    ROSE_ABORT();
  }

  return result;
}

//! Check if a variable is in a variable list of a given clause type
bool isInClauseVariableList(SgInitializedName *var,
                            SgOmpClauseBodyStatement *clause_stmt,
                            const VariantT &vt) {
  return isInClauseVariableList(var, clause_stmt, VariantVector(vt));
}

// lastprivate can be used with loop constructs or sections.
/* if (i is the last iteration)
 *   *shared_i_p = local_i
 *
 * The judge of last iteration is based on the iteration space increment
 * direction and loop stop conditions Incremental loops < upper:   last
 * iteration ==> i >= upper
 *      <=     :                      i> upper
 * Decremental loops
 *      > upper:   last iteration ==> i <= upper
 *      >=     :                      i < upper
 * AST: Orphaned worksharing OmpStatement is SgOmpForStatement->get_body() is
 * SgForStatement
 *
 *  We use bottom up traversal, the inner omp for loop has already been
 * translated, so we have to get the original upper bound via parameter
 *
 *  Another tricky case is that when some threads don't get any iterations to
 * work on, the initial _p_index may still trigger the lastprivate 's if
 * (_p_index>orig_bound) statement We add a condition to test if the thread
 * really worked on at least on iteration before compare the _p_index and the
 * original boundary if (_p_index != p_lower_ && _p_index>orig_bound) statement
 *
 *  Parameters:
 *    ompStmt: the OpenMP statement node with a lastprivate clause
 *    end_stmt_list: a list of statement which will be append to the end of bb1.
 * The generated if-stmt will be added to the end of this list bb1: the basic
 * block affected by the lastprivate clause orig_var: the initialized name for
 * the original lastprivate variable. Necessary since transOmpLoop will replace
 * loop index with changed one local_decl: the variable declaration for the
 * local copy of the lastprivate variable orig_loop_upper: the worksharing
 * construct's upper limit: for-loop: the loop upper value, sections: the
 * section count - 1
 *
 * */
static void insertOmpLastprivateCopyBackStmts(
    SgStatement *ompStmt, vector<SgStatement *> &end_stmt_list,
    SgBasicBlock *bb1, SgInitializedName *orig_var,
    SgVariableDeclaration *local_decl, SgExpression *orig_loop_upper) {
  SgStatement *save_stmt = NULL;
  SgExpression *orig_var_exp = NULL;
  if (SgOmpExecStatement *target = isSgOmpExecStatement(ompStmt)) {
    std::map<SgOmpExecStatement *, std::map<SgInitializedName *, SgExpression *>
                                       *>::const_iterator map_iter =
        clause_variable_renaming_record.find(target);
    if (map_iter != clause_variable_renaming_record.end()) {
      if (map_iter->second == nullptr) {
        failOutlinedClauseRecord("has a null mapping during lastprivate use",
                                 target, orig_var);
      }
      std::map<SgInitializedName *, SgExpression *>::const_iterator var_iter =
          map_iter->second->find(orig_var);
      if (var_iter != map_iter->second->end()) {
        (void)exactDetachedBackingSymbol(target, var_iter->second);
        orig_var_exp = copyExpression(var_iter->second);
      }
    }
  }
  if (orig_var_exp == NULL)
    orig_var_exp = buildVarRefExp(orig_var, bb1);
  if (isSgOmpForStatement(ompStmt) || isSgOmpDoStatement(ompStmt)) {
    ROSE_ASSERT(orig_loop_upper != NULL);
    SgInitializedName *loop_index = NULL;
    SgExpression *loop_lower = NULL;
    SgExpression *loop_upper = NULL;
    SgExpression *loop_step = NULL;
    SgStatement *loop_body = NULL;
    bool isIncremental = true;
    SageInterface::CanonicalFortranLoopDirection fortran_direction =
        SageInterface::CanonicalFortranLoopDirection::runtime;
    bool isInclusiveBound = false;
    bool isCanonical = false;
    bool selected_fortran_loop = false;

    SgStatement *selected_loop = NULL;
    size_t selected_loop_depth = 0;
    bool has_selected_loop = false;
    auto consider_loop_candidate = [&](SgStatement *candidate) {
      if (candidate == NULL)
        return;
      size_t depth = 0;
      SgNode *cursor = candidate;
      while (cursor != NULL && cursor != bb1) {
        cursor = cursor->get_parent();
        ++depth;
      }
      if (cursor != bb1)
        return;
      if (!has_selected_loop || depth < selected_loop_depth) {
        selected_loop = candidate;
        selected_loop_depth = depth;
        has_selected_loop = true;
      }
    };

    Rose_STL_Container<SgNode *> c_loops =
        NodeQuery::querySubTree(bb1, V_SgForStatement);
    for (Rose_STL_Container<SgNode *>::const_iterator it = c_loops.begin();
         it != c_loops.end(); ++it)
      consider_loop_candidate(isSgStatement(*it));

    Rose_STL_Container<SgNode *> f_loops =
        NodeQuery::querySubTree(bb1, V_SgFortranDo);
    for (Rose_STL_Container<SgNode *>::const_iterator it = f_loops.begin();
         it != f_loops.end(); ++it)
      consider_loop_candidate(isSgStatement(*it));

    if (selected_loop == NULL) {
      MLOG_ERROR_CXX("ompLowering") << "Failed to find a lowered loop under "
                                    << ompStmt->sage_class_name()
                                    << " while inserting lastprivate copy-back";
      ROSE_ABORT();
      return;
    }

    if (SgForStatement *top_loop = isSgForStatement(selected_loop)) {
      isCanonical = SageInterface::isCanonicalForLoop(
          top_loop, &loop_index, &loop_lower, &loop_upper, &loop_step,
          &loop_body, &isIncremental, &isInclusiveBound);
    } else if (SgFortranDo *top_loop = isSgFortranDo(selected_loop)) {
      selected_fortran_loop = true;
      isCanonical = SageInterface::isCanonicalDoLoop(
          top_loop, &loop_index, &loop_lower, &loop_upper, &loop_step,
          &loop_body, &fortran_direction, &isInclusiveBound);
    } else {
      MLOG_ERROR_CXX("ompLowering")
          << "Selected non-loop node " << selected_loop->sage_class_name()
          << " while inserting lastprivate copy-back";
      ROSE_ABORT();
      return;
    }
    if (!isCanonical) {
      MLOG_ERROR_CXX("ompLowering")
          << "Non-canonical lowered loop under " << ompStmt->sage_class_name()
          << " while inserting lastprivate copy-back";
      ROSE_ABORT();
      return;
    }
    SgExpression *if_cond = NULL;
    SgStatement *if_cond_stmt = NULL;
    SgType *logical_result_type = exactLogicalResultType();
    // we need the original upper bound!!
    if (selected_fortran_loop) {
      SgExpression *increasing_terminal =
          isInclusiveBound
              ? static_cast<SgExpression *>(buildGreaterThanOp(
                    buildVarRefExp(loop_index, bb1),
                    copyExpression(orig_loop_upper), logical_result_type))
              : static_cast<SgExpression *>(buildGreaterOrEqualOp(
                    buildVarRefExp(loop_index, bb1),
                    copyExpression(orig_loop_upper), logical_result_type));
      SgExpression *decreasing_terminal =
          isInclusiveBound
              ? static_cast<SgExpression *>(buildLessThanOp(
                    buildVarRefExp(loop_index, bb1),
                    copyExpression(orig_loop_upper), logical_result_type))
              : static_cast<SgExpression *>(buildLessOrEqualOp(
                    buildVarRefExp(loop_index, bb1),
                    copyExpression(orig_loop_upper), logical_result_type));
      if_cond = buildOrOp(
          buildAndOp(buildGreaterThanOp(copyExpression(loop_step),
                                        buildIntVal(0), logical_result_type),
                     increasing_terminal, logical_result_type),
          buildAndOp(buildLessThanOp(copyExpression(loop_step), buildIntVal(0),
                                     logical_result_type),
                     decreasing_terminal, logical_result_type),
          logical_result_type);
    } else if (isIncremental) {
      if (isInclusiveBound) // <= --> >
      {
        if_cond = buildGreaterThanOp(buildVarRefExp(loop_index, bb1),
                                     copyExpression(orig_loop_upper),
                                     logical_result_type);
      } else // < --> >=
      {
        if_cond = buildGreaterOrEqualOp(buildVarRefExp(loop_index, bb1),
                                        copyExpression(orig_loop_upper),
                                        logical_result_type);
      }
    } else {                // decremental loop
      if (isInclusiveBound) // >= --> <
      {
        if_cond = buildLessThanOp(buildVarRefExp(loop_index, bb1),
                                  copyExpression(orig_loop_upper),
                                  logical_result_type);
      } else // > --> <=
      {
        if_cond = buildLessOrEqualOp(buildVarRefExp(loop_index, bb1),
                                     copyExpression(orig_loop_upper),
                                     logical_result_type);
      }
    }
    // Add (_p_index != _p_lower) as another condition, making sure the current
    // thread really worked on at least one iteration Otherwise some thread
    // which does not run any iteration may have a big initial _p_index and
    // trigger the if statement's condition
    if_cond_stmt = buildExprStatement(buildAndOp(
        buildNotEqualOp(buildVarRefExp(loop_index, bb1),
                        copyExpression(loop_lower), logical_result_type),
        if_cond, logical_result_type));
    SgStatement *true_body = buildAssignStatement(copyExpression(orig_var_exp),
                                                  buildVarRefExp(local_decl));
    save_stmt = buildIfStmt(if_cond_stmt, true_body, NULL);
  } else if (isSgOmpSectionsStatement(ompStmt)) {
    ROSE_ASSERT(orig_loop_upper != NULL);
    Rose_STL_Container<SgNode *> while_stmts =
        NodeQuery::querySubTree(bb1, V_SgWhileStmt);
    ROSE_ASSERT(while_stmts.size() != 0);
    SgWhileStmt *top_while_stmt = isSgWhileStmt(while_stmts[0]);
    ROSE_ASSERT(top_while_stmt != NULL);
    // Get the section id variable from while-stmt  while(section_id >= 0) {}
    //  SgWhileStmt -> SgExprStatement -> SgGreaterOrEqualOp-> SgVarRefExp
    SgExprStatement *exp_stmt =
        isSgExprStatement(top_while_stmt->get_condition());
    ROSE_ASSERT(exp_stmt != NULL);
    SgGreaterOrEqualOp *ge_op =
        isSgGreaterOrEqualOp(exp_stmt->get_expression());
    ROSE_ASSERT(ge_op != NULL);
    SgVarRefExp *var_ref = isSgVarRefExp(ge_op->get_lhs_operand());
    ROSE_ASSERT(var_ref != NULL);
    string switch_index_name = (var_ref->get_symbol()->get_name()).getString();
    SgExpression *if_cond = NULL;
    SgStatement *if_cond_stmt = NULL;
    if_cond = buildEqualityOp(
        buildVarRefExp((switch_index_name + "_save"), bb1), orig_loop_upper,
        exactLogicalResultType()); // no need copy here
    if_cond_stmt = buildExprStatement(if_cond);
    SgStatement *true_body = buildAssignStatement(copyExpression(orig_var_exp),
                                                  buildVarRefExp(local_decl));
    save_stmt = buildIfStmt(if_cond_stmt, true_body, NULL);
  } else {
    cerr << "Illegal SgOmpxx for lastprivate variable: \nOmpStatement is:"
         << ompStmt->class_name() << endl;
    cerr << "lastprivate variable is:" << orig_var->get_name().getString()
         << endl;
    ROSE_ABORT();
  }
  end_stmt_list.push_back(save_stmt);
}

//! Generate copy-back statements for reduction variables
// end_stmt_list: the statement lists to be appended
// bb1: the affected code block by the reduction clause
// orig_var: the reduction variable's original copy
// local_decl: the local copy of the reduction variable
// Two ways to do the reduction operation:
// 1. builtin function TODO
//    __sync_fetch_and_add_4(&shared, (unsigned int)local);
// 2. using atomic runtime call:
//    GOMP_atomic_start ();
//    shared = shared op local;
//    GOMP_atomic_end ();
// We use the 2nd method only for now for simplicity and portability
static void insertOmpReductionCopyBackStmts(
    SgOmpClause::omp_reduction_identifier_enum r_operator,
    vector<SgStatement *> &end_stmt_list, SgBasicBlock *bb1,
    SgInitializedName *orig_var, SgVariableDeclaration *local_decl,
    SgStatement *node) {
  SgExprStatement *atomic_start_stmt =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_atomic_start"),
                            buildVoidType(), NULL, bb1);
  end_stmt_list.push_back(atomic_start_stmt);
  SgExpression *r_exp = NULL;
  SgExpression *orig_var_exp_template = NULL;
  SgOmpExecStatement *target = isSgOmpExecStatement(node);
  const auto directiveMapping = clause_variable_renaming_record.find(target);
  if (directiveMapping != clause_variable_renaming_record.end()) {
    std::map<SgInitializedName *, SgExpression *> *name_mapping =
        directiveMapping->second;
    if (name_mapping == nullptr) {
      failOutlinedClauseRecord("has a null mapping during reduction use",
                               target, orig_var);
    }
    std::map<SgInitializedName *, SgExpression *>::const_iterator map_iter =
        name_mapping->find(orig_var);
    if (map_iter != name_mapping->end()) {
      (void)exactDetachedBackingSymbol(target, map_iter->second);
      orig_var_exp_template = map_iter->second;
    }
  }
  if (orig_var_exp_template == NULL)
    orig_var_exp_template = buildVarRefExp(orig_var, bb1);

  // Build distinct trees for assignment lhs and rhs to avoid reusing the same
  // expression node in two places.
  SgExpression *orig_var_lhs_exp = copyExpression(orig_var_exp_template);
  SgExpression *orig_var_rhs_exp = copyExpression(orig_var_exp_template);
  SgType *reduction_result_type = orig_var_rhs_exp->get_type();
  ROSE_ASSERT(reduction_result_type != nullptr);

  switch (r_operator) {
  case SgOmpClause::e_omp_reduction_plus:
    r_exp = buildAddOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                       reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_mul:
    r_exp = buildMultiplyOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                            reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_minus:
    r_exp = buildSubtractOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                            reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_bitand:
    r_exp = buildBitAndOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                          reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_bitor:
    r_exp = buildBitOrOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                         reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_bitxor:
    r_exp = buildBitXorOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                          reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_logand:
    r_exp = buildAndOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                       reduction_result_type);
    break;
  case SgOmpClause::e_omp_reduction_logor:
    r_exp = buildOrOp(orig_var_rhs_exp, buildVarRefExp(local_decl),
                      reduction_result_type);
    break;
    // TODO Fortran operators.
  case SgOmpClause::e_omp_reduction_and: // Fortran .and.
  case SgOmpClause::e_omp_reduction_or:  // Fortran .or.
  case SgOmpClause::e_omp_reduction_eqv:
  case SgOmpClause::e_omp_reduction_neqv:
  case SgOmpClause::e_omp_reduction_max:
  case SgOmpClause::e_omp_reduction_min:
  case SgOmpClause::e_omp_reduction_iand:
  case SgOmpClause::e_omp_reduction_ior:
  case SgOmpClause::e_omp_reduction_ieor:
  case SgOmpClause::e_omp_reduction_unknown:
  case SgOmpClause::e_omp_reduction_last:
  default:
    cerr << "Illegal or unhandled reduction operator type:" << r_operator
         << endl;
    ROSE_ABORT();
  }
  SgStatement *reduction_stmt = buildAssignStatement(orig_var_lhs_exp, r_exp);
  end_stmt_list.push_back(reduction_stmt);
  SgExprStatement *atomic_end_stmt =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_atomic_end"),
                            buildVoidType(), NULL, bb1);
  end_stmt_list.push_back(atomic_end_stmt);
}

//! Liao 2/12/2013. Insert the thread-block inner level reduction statement into
//! the end of the end_stmt_list
// e.g.  xomp_inner_block_reduction_float (local_error, per_block_error,
// XOMP_REDUCTION_PLUS);
static void insertInnerThreadBlockReduction(
    SgOmpClause::omp_reduction_identifier_enum r_operator,
    vector<SgStatement *> &end_stmt_list, SgBasicBlock *bb1,
    SgInitializedName *orig_var, SgVariableDeclaration *local_decl,
    SgVariableDeclaration *per_block_decl,
    GpuOffloadLoweringContext *offload_ctx) {
  ROSE_ASSERT(bb1 && orig_var && local_decl && per_block_decl);
  ROSE_ASSERT(offload_ctx != NULL);
  // the integer value representing different reduction operations, defined
  // within libxomp.h for accelerator model
  // TODO refactor the code to have a function converting operand types to
  // integers
  int op_value = -1;
  switch (r_operator) {
  case SgOmpClause::e_omp_reduction_plus:
    op_value = 6;
    break;
  case SgOmpClause::e_omp_reduction_minus:
    op_value = 7;
    break;
  case SgOmpClause::e_omp_reduction_mul:
    op_value = 8;
    break;
  case SgOmpClause::e_omp_reduction_bitand:
    op_value = 9;
    break;
  case SgOmpClause::e_omp_reduction_bitor:
    op_value = 10;
    break;
  case SgOmpClause::e_omp_reduction_bitxor:
    op_value = 11;
    break;
  case SgOmpClause::e_omp_reduction_logand:
    op_value = 12;
    break;
  case SgOmpClause::e_omp_reduction_logor:
    op_value = 13;
    break;
    // TODO: more operation types
  case SgOmpClause::e_omp_reduction_and: // Fortran .and.
  case SgOmpClause::e_omp_reduction_or:  // Fortran .or.
  case SgOmpClause::e_omp_reduction_eqv:
  case SgOmpClause::e_omp_reduction_neqv:
  case SgOmpClause::e_omp_reduction_max:
  case SgOmpClause::e_omp_reduction_min:
  case SgOmpClause::e_omp_reduction_iand:
  case SgOmpClause::e_omp_reduction_ior:
  case SgOmpClause::e_omp_reduction_ieor:
  case SgOmpClause::e_omp_reduction_unknown:
  case SgOmpClause::e_omp_reduction_last:
  default:
    cerr << "Error. insertThreadBlockReduction() in omp_lowering.cpp: Illegal "
            "or unhandled reduction operator type:"
         << r_operator << endl;
    ROSE_ABORT();
  }

  SgVariableSymbol *var_sym = getFirstVarSym(per_block_decl);
  ROSE_ASSERT(var_sym != NULL);
  SgPointerType *var_type = isSgPointerType(var_sym->get_type());
  ROSE_ASSERT(var_type != NULL);
  SgType *reduction_type =
      stripTypeAliasesAndReferences(var_type->get_base_type());
  ROSE_ASSERT(reduction_type != nullptr);
  const char *type_suffix = nullptr;
  if (isSgTypeInt(reduction_type) != nullptr) {
    type_suffix = "int";
  } else if (isSgTypeFloat(reduction_type) != nullptr) {
    type_suffix = "float";
  } else if (isSgTypeDouble(reduction_type) != nullptr) {
    type_suffix = "double";
  } else {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[target-reduction-type]: "
                 "inner-block reduction requires exact int, float, or double "
                 "semantic type; got "
              << reduction_type->class_name() << std::endl;
    ROSE_ABORT();
  }
  offload_ctx->per_block_reduction_map[var_sym] =
      op_value; // save the per block symbol and its corresponding reduction
                // integer value defined in the libxomp.h
  SgIntVal *reduction_op = buildIntVal(op_value);
  SgExprListExp *parameter_list = buildExprListExp(
      buildVarRefExp(local_decl), buildVarRefExp(per_block_decl), reduction_op);
  SgStatement *func_call_stmt = buildFunctionCallStmt(
      std::string("xomp_inner_block_reduction_") + type_suffix, buildVoidType(),
      parameter_list, bb1);
  end_stmt_list.push_back(func_call_stmt);
}
// TODO move to sageInterface advanced transformation ???
//! Generate element-by-element assignment from a right-hand array to left_hand
//! array variable.
//
// e.g.  for int a[M][N], b[M][N],  a=b is implemented as follows:
//
//  int element_count = ...;
//  int *a_ap = (int *)a;
//  int *b_ap = (int *)b;
//  int i;
//  for (i=0;i<element_count; i++)
//    *(b_ap+i) = *(a_ap+i);
//
static SgBasicBlock *
generateArrayAssignmentStatements(SgInitializedName *left_operand,
                                  SgInitializedName *right_operand,
                                  SgScopeStatement *scope) {
  // parameter validation
  ROSE_ASSERT(scope !=
              NULL); // enforce top-down AST construction here for simplicity
  ROSE_ASSERT(left_operand != NULL);
  ROSE_ASSERT(right_operand != NULL);

  SgType *left_type = left_operand->get_type();
  SgType *right_type = right_operand->get_type();
  SgArrayType *left_array_type = isSgArrayType(left_type);
  SgArrayType *right_array_type = isSgArrayType(right_type);

  ROSE_ASSERT(left_array_type != NULL);
  ROSE_ASSERT(right_array_type != NULL);
  // make sure two array are compatible: same dimension, bounds, and element
  // types, etc.
  ROSE_ASSERT(getElementType(left_array_type) ==
              getElementType(right_array_type));
  int dim_count = getDimensionCount(left_array_type);
  ROSE_ASSERT(dim_count == getDimensionCount(right_array_type));
  int element_count = getArrayElementCount(left_array_type);
  ROSE_ASSERT(element_count == (int)getArrayElementCount(right_array_type));

  SgBasicBlock *bb = buildBasicBlock();
  // front_stmt_list.push_back() will handle this later on.
  // Keep this will cause duplicated appendStatement()
  // appendStatement(bb, scope);

  // int *a_ap = (int*) a;
  string right_name = right_operand->get_name().getString();
  string right_name_p = right_name + "_ap"; // array pointer (ap)
  SgType *element_type = getElementType(left_array_type);
  ROSE_ASSERT(element_type != nullptr);
  SgType *elementPointerType = buildPointerType(element_type);
  SgAssignInitializer *initor = buildAssignInitializer(
      buildCastExp(buildVarRefExp(right_operand, scope), elementPointerType),
      elementPointerType);
  SgVariableDeclaration *decl_right =
      buildVariableDeclaration(right_name_p, elementPointerType, initor, bb);
  bb->append_statement(decl_right);

  // int *b_ap = (int*) b;
  string left_name = left_operand->get_name().getString();
  string left_name_p = left_name + "_ap";
  SgAssignInitializer *initor2 = buildAssignInitializer(
      buildCastExp(buildVarRefExp(left_operand, scope), elementPointerType),
      elementPointerType);
  SgVariableDeclaration *decl_left =
      buildVariableDeclaration(left_name_p, elementPointerType, initor2, bb);
  bb->append_statement(decl_left);

  // int i;
  SgVariableDeclaration *decl_i =
      buildVariableDeclaration("_p_i", buildIntType(), NULL, bb);
  bb->append_statement(decl_i);

  //  for (i=0;i<element_count; i++)
  //    *(b_ap+i) = *(a_ap+i);
  SgStatement *init_stmt =
      buildAssignStatement(buildVarRefExp(decl_i), buildIntVal(0));
  SgStatement *test_stmt = buildExprStatement(
      buildLessThanOp(buildVarRefExp(decl_i), buildIntVal(element_count),
                      exactLogicalResultType()));
  SgExpression *incr_exp = buildPlusPlusOp(buildVarRefExp(decl_i),
                                           buildIntType(), SgUnaryOp::postfix);
  SgStatement *loop_body = buildAssignStatement(
      buildPointerDerefExp(buildAddOp(buildVarRefExp(decl_left),
                                      buildVarRefExp(decl_i),
                                      elementPointerType),
                           element_type),
      buildPointerDerefExp(buildAddOp(buildVarRefExp(decl_right),
                                      buildVarRefExp(decl_i),
                                      elementPointerType),
                           element_type));
  SgForStatement *for_stmt =
      buildForStatement(init_stmt, test_stmt, incr_exp, loop_body);
  bb->append_statement(for_stmt);

  // The block is intentionally assembled off-tree so private-variable
  // rewriting cannot visit its generated references.  Publish the completed
  // transaction against the caller's attached output boundary before placing
  // it in the deferred insertion list.
  SageInterface::publishGeneratedSubtreeOutputOwner(bb, scope);

  return bb;
}

// SgBasicBlock * getEnclosingRegionOrFuncDefinition(SgBasicBlock *orig_scope)
SgBasicBlock *getEnclosingRegionOrFuncDefinition(SgNode *orig_scope) {
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  // find the right scope (target body) to insert the declaration, start from
  // the original scope
  SgBasicBlock *t_body = NULL;

  // find enclosing parallel region's body
  SgOmpParallelStatement *omp_stmt = isSgOmpParallelStatement(
      getEnclosingNode<SgOmpParallelStatement>(orig_scope));
  if (omp_stmt) {
    SgBasicBlock *omp_body = isSgBasicBlock(omp_stmt->get_body());
    ROSE_ASSERT(omp_body != NULL);
    t_body = omp_body;
  } else {
    // Find enclosing function body
    SgFunctionDefinition *func_def = getEnclosingProcedure(orig_scope);
    ROSE_ASSERT(func_def != NULL);
    SgBasicBlock *f_body = func_def->get_body();
    ROSE_ASSERT(f_body != NULL);
    t_body = f_body;
  }
  ROSE_ASSERT(t_body != NULL);
  return t_body;
}

//! This is a highly specialized operation which can find the right place to
//! insert a Fortran variable declaration
//  during OpenMP lowering.
//
//  The reasons are:
//    1)Fortran (at least F77) requires declaration statements to be consecutive
//    within an enclosing function definition. The C99-style generation of 'int
//    loop_index' within a SgBasicBlock in the middle of some executable
//    statement is illegal
//     for Fortran. We have to find the enclosing function body, located the
//     declaration sequence, and add the new declaration after it.
//
//    2) When translating OpenMP constructs within a parallel region, the
//    declaration (such as those for private variables of the construct )
//       should be inserted into the declaration part of the body of the
//       parallel region, which will become function body of the outlined
//       function when translating the region later on.
//       Insert the declaration to the current enclosing function definition is
//       not correct.
//
// Liao 1/12/2011
SgVariableDeclaration *
buildAndInsertDeclarationForOmp(const std::string &name, SgType *type,
                                SgInitializer *varInit,
                                SgBasicBlock *orig_scope) {
  ROSE_ASSERT(SageInterface::is_Fortran_language() == true);
  SgVariableDeclaration *result = NULL;

  // find the right scope (target body) to insert the declaration, start from
  // the original scope

  SgBasicBlock *t_body = NULL;

  t_body = getEnclosingRegionOrFuncDefinition(orig_scope);
  // Build the required variable declaration
  result = buildVariableDeclaration(name, type, varInit, t_body);

  // Publish and insert through the Fortran specification-part transaction.
  // The generic legacy helper can select a semantic declaration imported from
  // an INCLUDE file as its lexical anchor, which assigns the generated host
  // declaration to the included file's physical output surface.
  insert_fortran_statement_in_specification_part(result, t_body);
  ROSE_ASSERT(result != NULL);
  return result;
}

struct ExactOmpPrivateNameReservations {
  SgScopeStatement *targetScope = nullptr;
  std::map<SgVariableSymbol *, std::string> namesBySource;
  std::map<std::string, SgVariableSymbol *> sourcesByName;
};

static std::string
allocateExactOmpPrivateName(SgVariableSymbol *sourceSymbol,
                            SgScopeStatement *targetScope,
                            ExactOmpPrivateNameReservations &reservations) {
  if (sourceSymbol == NULL || sourceSymbol->get_declaration() == NULL ||
      targetScope == NULL || targetScope->get_symbol_table() == NULL) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[private-name-identity]: source=%p "
            "target=%p has no exact symbol/scope identity\n",
            static_cast<void *>(sourceSymbol),
            static_cast<void *>(targetScope));
    ROSE_ABORT();
  }
  if (reservations.targetScope == nullptr) {
    reservations.targetScope = targetScope;
  } else if (reservations.targetScope != targetScope) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[private-name-identity]: one name "
            "transaction spans distinct target scopes=%p,%p\n",
            static_cast<void *>(reservations.targetScope),
            static_cast<void *>(targetScope));
    ROSE_ABORT();
  }

  const auto existingSource = reservations.namesBySource.find(sourceSymbol);
  if (existingSource != reservations.namesBySource.end()) {
    const auto existingName =
        reservations.sourcesByName.find(existingSource->second);
    if (existingName == reservations.sourcesByName.end() ||
        existingName->second != sourceSymbol) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[private-name-identity]: source=%p "
              "has an inconsistent transaction-local reservation\n",
              static_cast<void *>(sourceSymbol));
      ROSE_ABORT();
    }
    return existingSource->second;
  }

  const std::string sourceName = sourceSymbol->get_name().getString();
  if (sourceName.empty()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[private-name-identity]: source=%p "
            "has an empty semantic name\n",
            static_cast<void *>(sourceSymbol));
    ROSE_ABORT();
  }

  const std::string baseName = "_p_" + sourceName;
  for (size_t suffix = 0;; ++suffix) {
    const std::string candidate =
        suffix == 0 ? baseName
                    : baseName + "__rex_private_" + std::to_string(suffix);
    SgSymbolTable *targetTable = targetScope->get_symbol_table();
    if (targetTable->exists(SgName(candidate))) {
      size_t exactOccupants = 0;
      const auto range =
          targetTable->get_table()->equal_range(SgName(candidate));
      for (auto current = range.first; current != range.second; ++current) {
        SgSymbol *occupied = current->second;
        if (occupied == nullptr || occupied->get_parent() != targetTable ||
            !targetTable->exists(occupied) ||
            occupied->get_scope() != targetScope) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[private-name-identity]: "
                  "candidate=%s is occupied by malformed symbol=%p in "
                  "target=%p\n",
                  candidate.c_str(), static_cast<void *>(occupied),
                  static_cast<void *>(targetScope));
          ROSE_ABORT();
        }
        ++exactOccupants;
      }
      if (exactOccupants == 0) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[private-name-identity]: "
                "candidate=%s is reported occupied without an exact table "
                "entry in target=%p\n",
                candidate.c_str(), static_cast<void *>(targetScope));
        ROSE_ABORT();
      }
      continue;
    }

    if (reservations.sourcesByName.count(candidate) != 0)
      continue;

    if (!reservations.namesBySource.emplace(sourceSymbol, candidate).second ||
        !reservations.sourcesByName.emplace(candidate, sourceSymbol).second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[private-name-identity]: failed to "
              "reserve candidate=%s for exact source=%p\n",
              candidate.c_str(), static_cast<void *>(sourceSymbol));
      ROSE_ABORT();
    }
    return candidate;
  }
}

static void requireExactOmpPrivateNamePublication(
    const ExactOmpPrivateNameReservations &reservations,
    SgVariableSymbol *source, SgVariableSymbol *generated,
    SgScopeStatement *targetScope) {
  const auto reservation = reservations.namesBySource.find(source);
  if (reservation == reservations.namesBySource.end() || generated == nullptr ||
      targetScope == nullptr || reservations.targetScope != targetScope ||
      generated->get_name().getString() != reservation->second ||
      generated->get_parent() != targetScope->get_symbol_table() ||
      !targetScope->get_symbol_table()->exists(generated) ||
      generated->get_declaration() == nullptr ||
      generated->get_declaration()->get_scope() != targetScope) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[private-name-identity]: source=%p "
            "generated=%p target=%p does not publish its exact reserved "
            "identity\n",
            static_cast<void *>(source), static_cast<void *>(generated),
            static_cast<void *>(targetScope));
    ROSE_ABORT();
  }
}

static void recordExactOmpPrivateSymbolMapping(
    VariableSymbolMap_t &mapping, SgVariableSymbol *source,
    SgVariableSymbol *target, SgScopeStatement *targetScope,
    std::set<SgVariableSymbol *> &generatedTargets, bool registerTarget) {
  if (source == NULL || target == NULL || targetScope == NULL ||
      target->get_declaration() == NULL ||
      target->get_declaration()->get_scope() != targetScope ||
      target->get_parent() != targetScope->get_symbol_table() ||
      !targetScope->get_symbol_table()->exists(target)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[private-symbol-identity]: source=%p "
            "target=%p scope=%p has no exact generated symbol identity\n",
            static_cast<void *>(source), static_cast<void *>(target),
            static_cast<void *>(targetScope));
    ROSE_ABORT();
  }

  VariableSymbolMap_t::const_iterator existing = mapping.find(source);
  if (existing != mapping.end()) {
    if (existing->second != target) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[private-symbol-identity]: source="
              "%p maps to distinct generated symbols=%p,%p\n",
              static_cast<void *>(source),
              static_cast<void *>(existing->second),
              static_cast<void *>(target));
      ROSE_ABORT();
    }
    return;
  }
  if (registerTarget) {
    if (!generatedTargets.insert(target).second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[private-symbol-identity]: generated "
              "symbol=%p was allocated more than once\n",
              static_cast<void *>(target));
      ROSE_ABORT();
    }
  } else if (generatedTargets.count(target) != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[private-symbol-identity]: alias "
            "source=%p targets unregistered generated symbol=%p\n",
            static_cast<void *>(source), static_cast<void *>(target));
    ROSE_ABORT();
  }
  mapping.insert(VariableSymbolMap_t::value_type(source, target));
}

//! Translate clauses with variable lists, such as private, firstprivate,
//! lastprivate, reduction, etc.
// bb1 is the affected code block by the clause.
// Command steps are: insert local declarations for the variables:(all)
//                    initialize the local declaration:(firstprivate, reduction)
//                    variable substitution for the variables:(all)
//                    save local copy back to its global one:(reduction,
//                    lastprivate)
//  Note that a variable could be both firstprivate and lastprivate
//  Parameters:
//      ompStmt: the OpenMP statement node with variable clauses
//      bb1: the translation-generated basic block to implement ompStmt
//      orig_loop_upper:
//        if ompStmt is loop construct, pass the original loop upper bound
//        if ompStmt is omp sections, pass the section count - 1
//  This function is later extended to support OpenMP accelerator model. In this
//  model,
//     We have no concept of firstprivate or lastprivate
//     reduction is implemented using a two-level reduction algorithm
static void transOmpVariablesWithContext(
    SgStatement *ompStmt, SgBasicBlock *bb1,
    SgExpression *orig_loop_upper /*= NULL*/,
    bool isAcceleratorModel /*= false*/,
    GpuOffloadLoweringContext *offload_ctx /*= NULL*/) {
  ROSE_ASSERT(ompStmt != NULL);
  ROSE_ASSERT(bb1 != NULL);
  SgOmpClauseBodyStatement *clause_stmt = isSgOmpClauseBodyStatement(ompStmt);
  ROSE_ASSERT(clause_stmt != NULL);

  // collect variables
  SgInitializedNamePtrList var_list = collectAllClauseVariables(clause_stmt);
  // Only keep the unique ones
  sort(var_list.begin(), var_list.end());
  ;
  SgInitializedNamePtrList::iterator new_end =
      unique(var_list.begin(), var_list.end());
  var_list.erase(new_end, var_list.end());
  VariableSymbolMap_t var_map;
  ASTtools::VarSymSet_t var_set;
  std::set<SgVariableSymbol *> scalar_locals_from_pointer_symbols;
  std::set<SgVariableSymbol *> generated_private_symbols;
  ExactOmpPrivateNameReservations private_name_reservations;

  vector<SgStatement *> front_stmt_list, end_stmt_list, front_init_list;
  vector<SgStatement *> threadprivate_end_stmt_list;
  bool has_fortran_threadprivate = false;

  std::map<std::string, SgVariableSymbol *> visible_symbols_by_name;
  if (const SgFunctionDeclaration *enclosing_decl =
          getEnclosingFunctionDeclaration(bb1)) {
    ASTtools::VarSymSet_t visible_syms;
    ASTtools::collectLocalVisibleVarSyms(enclosing_decl, bb1, visible_syms);
    for (ASTtools::VarSymSet_t::const_iterator i = visible_syms.begin();
         i != visible_syms.end(); ++i) {
      const SgVariableSymbol *sym = *i;
      if (sym == NULL)
        continue;
      const std::string name = sym->get_name().getString();
      if (visible_symbols_by_name.count(name) == 0)
        visible_symbols_by_name[name] = const_cast<SgVariableSymbol *>(sym);
    }
  }

  for (size_t i = 0; i < var_list.size(); i++) {
    SgInitializedName *orig_var = var_list[i];
    ROSE_ASSERT(orig_var != NULL);
    SgVariableSymbol *visible_symbol =
        lookupVariableSymbolInParentScopes(orig_var->get_name(), bb1);
    if (visible_symbol == NULL) {
      std::map<std::string, SgVariableSymbol *>::const_iterator visible_it =
          visible_symbols_by_name.find(orig_var->get_name().getString());
      if (visible_it != visible_symbols_by_name.end())
        visible_symbol = visible_it->second;
    }
    string orig_name = orig_var->get_name().getString();
    SgVariableSymbol *orig_symbol =
        isSgVariableSymbol(orig_var->get_symbol_from_symbol_table());
    if (orig_symbol == NULL) {
      SgScopeStatement *decl_scope = orig_var->get_scope();
      if (decl_scope != NULL)
        orig_symbol = decl_scope->lookup_var_symbol(orig_var->get_name());
    }
    if (orig_symbol == NULL && visible_symbol != NULL)
      orig_symbol = visible_symbol;
    ROSE_ASSERT(orig_symbol != NULL);
    SgVariableSymbol *active_symbol =
        visible_symbol != NULL ? visible_symbol : orig_symbol;
    ROSE_ASSERT(active_symbol != NULL);
    SgType *orig_type = orig_var->get_type();
    if (orig_type == NULL ||
        isSgTypeUnknown(stripTypeAliases(orig_type)) != NULL) {
      SgType *active_type = active_symbol->get_type();
      if (active_type != NULL &&
          isSgTypeUnknown(stripTypeAliases(active_type)) == NULL) {
        orig_type = active_type;
      }
    }
    SgExpression *orig_var_exp = NULL;
    if (SgOmpExecStatement *target = isSgOmpExecStatement(clause_stmt)) {
      std::map<SgOmpExecStatement *,
               std::map<SgInitializedName *, SgExpression *> *>::const_iterator
          map_iter = clause_variable_renaming_record.find(target);
      if (map_iter != clause_variable_renaming_record.end()) {
        if (map_iter->second == nullptr) {
          failOutlinedClauseRecord(
              "has a null mapping during clause-variable translation", target,
              orig_var);
        }
        std::map<SgInitializedName *, SgExpression *>::const_iterator var_iter =
            map_iter->second->find(orig_var);
        if (var_iter != map_iter->second->end()) {
          (void)exactDetachedBackingSymbol(target, var_iter->second);
          orig_var_exp = copyExpression(var_iter->second);
        }
      }
    }
    if (orig_var_exp == NULL)
      orig_var_exp = buildVarRefExp(active_symbol);

    VariantVector vvt(V_SgOmpPrivateClause);
    vvt.push_back(V_SgOmpReductionClause);
    vvt.push_back(V_SgOmpFirstprivateClause);
    if (SageInterface::is_Fortran_language())
      vvt.push_back(V_SgOmpCopyinClause);

    // TODO: No such concept of firstprivate and lastprivate in accelerator
    // model??
    if (!isAcceleratorModel) // we actually already has enable_accelerator, but
                             // it is too global for handling both CPU and GPU
                             // translation
    {
      vvt.push_back(V_SgOmpLastprivateClause);
    }

    // a local private copy
    SgVariableDeclaration *local_decl = NULL;
    SgOmpClause::omp_reduction_identifier_enum r_operator =
        SgOmpClause::e_omp_reduction_unknown;
    bool isReductionVar =
        isInClauseVariableList(orig_var, clause_stmt, V_SgOmpReductionClause);

    // step 1. Insert local declaration for private, firstprivate, lastprivate
    // and reduction Sara, 5/31/2013: if variable is in Function Scope ( a
    // parameter ) and array, we don't want a private copy, since the only thing
    // private is the pointer, not the pointed data We had a variable passed as
    // private that has to be used as shared We create a pointer to the variable
    // and replace all the occurrences of the variable by the pointer Example:
    // source code:
    // void outlining( int M[10][10] ) {
    //   #pragma omp task firstprivate( M )
    //   M[0][0] = 4;
    // }
    // outlined parameters struct
    // struct OUT__17__7038___data {
    //   int (*M)[10UL];
    // };
    // outlined function:
    // static void OUT__17__7038__(void *__out_argv) {
    //   int (**M)[10UL] = (int (**)[10UL])(&(((struct OUT__17__7038___data
    //   *)__out_argv) -> M));
    //   (*M)[0][0] = 4;
    // }
    if (isInClauseVariableList(orig_var, clause_stmt, vvt)) {
      SgType *effective_type = orig_var_exp->get_type();
      if (effective_type == nullptr ||
          isSgTypeUnknown(stripTypeAliases(effective_type)) != nullptr) {
        MLOG_ERROR_CXX("ompLowering")
            << "OpenMP clause variable '" << orig_name
            << "' has no exact semantic expression type";
        ROSE_ABORT();
      }
      if (SgReferenceType *ref_type = isSgReferenceType(effective_type))
        effective_type = ref_type->get_base_type();

      const bool is_firstprivate = isInClauseVariableList(
          orig_var, clause_stmt, V_SgOmpFirstprivateClause);
      const bool is_copyin =
          isInClauseVariableList(orig_var, clause_stmt, V_SgOmpCopyinClause);
      const bool is_fortran_threadprivate =
          SageInterface::is_Fortran_language() &&
          (isThreadprivate(active_symbol) ||
           (orig_symbol != active_symbol && isThreadprivate(orig_symbol)));
      const bool is_function_scope_array =
          isSgArrayType(effective_type) &&
          isSgFunctionDefinition(orig_var->get_scope()) &&
          !is_fortran_threadprivate;

      if (is_copyin && !is_fortran_threadprivate) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[copyin-variable]: variable '%s' "
                "is not one exact threadprivate object\n",
                orig_name.c_str());
        ROSE_ABORT();
      }

      if (!is_function_scope_array) {
        SgInitializer *init = NULL;
        SgExpression *fortran_firstprivate_value = NULL;
        // use copy constructor for firstprivate on C++ class object variables
        // For simplicity, we handle C and C++ scalar variables the same way
        //
        // But here is one exception: an array type firstprivate variable should
        // be initialized element-by-element
        // Liao, 4/12/2010
        if (is_firstprivate && !isSgArrayType(effective_type)) {
          SgExpression *init_value = NULL;
          // Nested task outlining can leave firstprivate clause variables bound
          // to stale declaration types while body references use the visible
          // in-scope symbol. Keep the local firstprivate declaration type and
          // initializer consistent with the active symbol in this specific
          // situation.
          if (isSgOmpTaskStatement(clause_stmt) != NULL &&
              stripTypeAliases(active_symbol->get_type()) !=
                  stripTypeAliases(effective_type)) {
            SgExpression *active_value = NULL;
            if (buildExpressionMatchingTypeFromActiveSymbol(
                    active_symbol, effective_type, active_value)) {
              init_value = active_value;
            } else {
              init_value = copyExpression(orig_var_exp);
            }
          } else {
            init_value = copyExpression(orig_var_exp);
          }

          if (SageInterface::is_Fortran_language()) {
            fortran_firstprivate_value = init_value;
          } else {
            init = buildAssignInitializer(init_value, effective_type);
          }
        }

        string private_name;
        if (SageInterface::is_Fortran_language()) {
          // leading _ is not allowed in Fortran
          private_name = "i_" + orig_name;
          nCounter++; // Fortran does not have basic block as a scope at source
                      // level
          // I have to generated all declarations at the same flat level under
          // function definitions So a name counter is needed to avoid name
          // collision
          private_name =
              private_name + "_" + StringUtility::numberToString(nCounter);

          // Special handling for variable declarations in Fortran
          local_decl = buildAndInsertDeclarationForOmp(
              private_name, effective_type, init, bb1);
        } else {
          private_name = allocateExactOmpPrivateName(active_symbol, bb1,
                                                     private_name_reservations);
          local_decl =
              buildVariableDeclaration(private_name, effective_type, init, bb1);
          front_stmt_list.push_back(local_decl);
        }
        // record the map from old to new symbol
        SgVariableSymbol *local_symbol = getFirstVarSym(local_decl);
        ROSE_ASSERT(local_symbol != NULL);
        if (!SageInterface::is_Fortran_language()) {
          requireExactOmpPrivateNamePublication(
              private_name_reservations, active_symbol, local_symbol, bb1);
        }
        SgScopeStatement *local_symbol_scope = local_symbol->get_scope();
        if (local_symbol_scope == nullptr ||
            local_symbol->get_declaration() == nullptr ||
            local_symbol->get_declaration()->get_scope() !=
                local_symbol_scope) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[private-symbol-identity]: "
                  "generated symbol=%p has no exact declaration scope\n",
                  static_cast<void *>(local_symbol));
          ROSE_ABORT();
        }

        if (fortran_firstprivate_value != NULL) {
          SgExprStatement *init_stmt = buildAssignStatement(
              buildVarRefExp(local_symbol), fortran_firstprivate_value);
          front_init_list.push_back(init_stmt);
        }

        if (is_fortran_threadprivate) {
          SgVariableDeclaration *source_decl =
              isSgVariableDeclaration(orig_var->get_declaration());
          if (source_decl == nullptr ||
              source_decl->get_declarationModifier()
                  .get_typeModifier()
                  .isAllocatable() ||
              isSgPointerType(stripTypeAliases(effective_type)) != nullptr) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[threadprivate-type]: "
                    "Fortran variable '%s' is not one fixed-storage object\n",
                    orig_name.c_str());
            ROSE_ABORT();
          }
          const size_t object_size =
              get_target_type_size_bytes(effective_type, bb1);
          if (object_size >
              static_cast<size_t>(std::numeric_limits<long long>::max())) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[threadprivate-type]: "
                    "Fortran variable '%s' exceeds the exact runtime size "
                    "domain\n",
                    orig_name.c_str());
            ROSE_ABORT();
          }
          auto build_exact_size_argument = [object_size]() -> SgExpression * {
            return buildLongLongIntVal(static_cast<long long>(object_size));
          };
          SgExprListExp *load_parameters = buildExprListExp(
              copyExpression(orig_var_exp), buildVarRefExp(local_symbol),
              build_exact_size_argument());
          front_init_list.push_back(
              buildFunctionCallStmt(is_copyin ? "rex_kmpc_threadprivate_copyin"
                                              : "rex_kmpc_threadprivate_load",
                                    buildVoidType(), load_parameters, bb1));

          SgExprListExp *store_parameters = buildExprListExp(
              copyExpression(orig_var_exp), buildVarRefExp(local_symbol),
              build_exact_size_argument());
          threadprivate_end_stmt_list.push_back(
              buildFunctionCallStmt("rex_kmpc_threadprivate_store",
                                    buildVoidType(), store_parameters, bb1));
          has_fortran_threadprivate = true;
        }

        recordExactOmpPrivateSymbolMapping(var_map, active_symbol, local_symbol,
                                           local_symbol_scope,
                                           generated_private_symbols, true);
        if (orig_symbol != NULL && orig_symbol != active_symbol)
          recordExactOmpPrivateSymbolMapping(var_map, orig_symbol, local_symbol,
                                             local_symbol_scope,
                                             generated_private_symbols, false);
        if (isPointerBackedType(active_symbol->get_type()) &&
            isSgPointerType(stripTypeAliases(local_symbol->get_type())) ==
                NULL) {
          scalar_locals_from_pointer_symbols.insert(local_symbol);
        }
      } else if (is_firstprivate && !SageInterface::is_Fortran_language()) {
        // C/C++ function parameters declared as arrays decay to pointers. For
        // firstprivate, create a local pointer copy instead of rewriting uses
        // with an extra dereference, which can create invalid forms such as
        // *(*M) or *(*v2) after outlining.
        SgArrayType *array_type = isSgArrayType(effective_type);
        ROSE_ASSERT(array_type != NULL);
        SgType *local_type = buildPointerType(array_type->get_base_type());
        SgInitializer *init =
            buildAssignInitializer(copyExpression(orig_var_exp), local_type);
        string private_name = allocateExactOmpPrivateName(
            active_symbol, bb1, private_name_reservations);
        local_decl =
            buildVariableDeclaration(private_name, local_type, init, bb1);
        front_stmt_list.push_back(local_decl);
        SgVariableSymbol *local_symbol = getFirstVarSym(local_decl);
        ROSE_ASSERT(local_symbol != NULL);
        requireExactOmpPrivateNamePublication(private_name_reservations,
                                              active_symbol, local_symbol, bb1);
        recordExactOmpPrivateSymbolMapping(var_map, active_symbol, local_symbol,
                                           bb1, generated_private_symbols,
                                           true);
        if (orig_symbol != NULL && orig_symbol != active_symbol)
          recordExactOmpPrivateSymbolMapping(var_map, orig_symbol, local_symbol,
                                             bb1, generated_private_symbols,
                                             false);
        if (isPointerBackedType(active_symbol->get_type()) &&
            isSgPointerType(stripTypeAliases(local_symbol->get_type())) ==
                NULL) {
          scalar_locals_from_pointer_symbols.insert(local_symbol);
        }
      } else {
        var_set.insert(active_symbol);
        if (orig_symbol != NULL && orig_symbol != active_symbol)
          var_set.insert(orig_symbol);
      }
    }
    // step 2. Initialize the local copy for array-type firstprivate variables
    // TODO copyin, copyprivate
    if (isInClauseVariableList(orig_var, clause_stmt,
                               V_SgOmpFirstprivateClause) &&
        isSgArrayType(orig_type) &&
        !isSgFunctionDefinition(orig_var->get_scope())) {
      SgInitializedName *leftArray = getFirstInitializedName(local_decl);
      SgBasicBlock *arrayAssign =
          generateArrayAssignmentStatements(leftArray, orig_var, bb1);
      front_stmt_list.push_back(arrayAssign);
    }
    if (isReductionVar) // create initial value assignment for the local
                        // reduction variable
    {
      r_operator = getReductionOperationType(orig_var, clause_stmt);
      SgExprStatement *init_stmt = buildAssignStatement(
          buildVarRefExp(local_decl), createInitialValueExp(r_operator));
      if (SageInterface::is_Fortran_language()) {
        // Fortran initialization statements  cannot be interleaved with
        // declaration statements. We save them here and insert them after all
        // declaration statements are inserted.
        front_init_list.push_back(init_stmt);
      } else {
        front_stmt_list.push_back(init_stmt);
      }
    }

    // Liao, 2/12/2013. For an omp for loop within "omp target". We translate
    // its reduction variable by using a two-level reduction method:
    // thread-block level (within kernel) and beyond-block level (done on CPU
    // side). So we have to insert a pointer to the array of per-block reduction
    // results right before its enclosing "omp target" directive The insertion
    // point is decided so that the outliner invoked by transOmpTargetParallel()
    // can later catch this newly introduced variable and handle it in the
    // parameter list properly.
    //
    // e.g. REAL* per_block_results = (REAL *)xomp_deviceMalloc (numBlocks.x*
    // sizeof(REAL));
    SgVariableDeclaration *per_block_decl = NULL;
    if (isReductionVar && isAcceleratorModel) {
      ROSE_ASSERT(offload_ctx != NULL);
      // SgOmpParallelStatement* enclosing_omp_parallel =
      // getEnclosingNode<SgOmpParallelStatement> (ompStmt);
      SgOmpClauseBodyStatement *enclosing_omp_parallel =
          isSgOmpClauseBodyStatement(ompStmt);
      ROSE_ASSERT(enclosing_omp_parallel != NULL);
      // SgScopeStatement* scope_for_insertion =
      // enclosing_omp_target->get_scope();
      SgScopeStatement *scope_for_insertion =
          isSgScopeStatement(enclosing_omp_parallel->get_scope());
      ROSE_ASSERT(scope_for_insertion != NULL);
      SgVarRefExp *num_block_ref =
          buildVarRefExp("_num_blocks_", scope_for_insertion);
      SgType *size_type =
          SageInterface::requireTargetSizeType(scope_for_insertion);
      SgExpression *multi_exp = buildMultiplyOp(
          num_block_ref, buildSizeOfOp(orig_type, size_type), size_type);
      SgExprListExp *parameter_list = buildExprListExp(multi_exp);
      SgExpression *init_exp = buildCastExp(
          buildFunctionCallExp(SgName("malloc"),
                               buildPointerType(buildPointerType(orig_type)),
                               parameter_list, scope_for_insertion),
          buildPointerType(orig_type));
      SgType *per_block_type = buildPointerType(orig_type);
      per_block_decl = buildVariableDeclaration(
          "__reduction_buffer_" + orig_name, per_block_type,
          buildAssignInitializer(init_exp, per_block_type),
          scope_for_insertion);
      // the prefix of "_dev_per_block_" is important for later handling when
      // calling outliner: add them into the parameter list per_block_decl =
      // buildVariableDeclaration ("_dev_per_block_"+orig_name,
      // buildPointerType(orig_type), buildAssignInitializer(init_exp),
      // scope_for_insertion); this statement refers to _num_blocks_, which will
      // be declared later on when translating "omp parallel" enclosed in "omp
      // target" so we insert it  later when the kernel launch statement is
      // inserted. insertStatementAfter(enclosing_omp_parallel, per_block_decl);
      offload_ctx->per_block_declarations.push_back(per_block_decl);
      // store all reduction variables at the loop level, they will be used
      // later when translating the enclosing "omp target" to help decide on the
      // variables being passed
    }

    // step 3. Save the value back for lastprivate and reduction
    if (isInClauseVariableList(orig_var, clause_stmt,
                               V_SgOmpLastprivateClause)) {
      insertOmpLastprivateCopyBackStmts(ompStmt, end_stmt_list, bb1, orig_var,
                                        local_decl, orig_loop_upper);
    } else if (isReductionVar) {
      // two-level reduction is used for accelerator model
      if (isAcceleratorModel)
        insertInnerThreadBlockReduction(r_operator, end_stmt_list, bb1,
                                        orig_var, local_decl, per_block_decl,
                                        offload_ctx);
      else
        insertOmpReductionCopyBackStmts(r_operator, end_stmt_list, bb1,
                                        orig_var, local_decl, ompStmt);
    }

    if (orig_var_exp != NULL) {
      SageInterface::deleteAST(orig_var_exp,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      if (SgNode::isLiveNode(orig_var_exp)) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[clause-value-template]: "
                "variable=%s left a detached value expression live\n",
                orig_name.c_str());
        ROSE_ABORT();
      }
    }

  } // end for (each variable)

  if (has_fortran_threadprivate) {
    front_init_list.push_back(buildFunctionCallStmt(
        "rex_kmpc_threadprivate_barrier", buildVoidType(), nullptr, bb1));
    end_stmt_list.push_back(buildFunctionCallStmt(
        "rex_kmpc_threadprivate_barrier", buildVoidType(), nullptr, bb1));
    end_stmt_list.insert(end_stmt_list.end(),
                         threadprivate_end_stmt_list.begin(),
                         threadprivate_end_stmt_list.end());
  } else if (!threadprivate_end_stmt_list.empty()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[threadprivate-runtime]: store list "
            "exists without an exact Fortran threadprivate transaction\n");
    ROSE_ABORT();
  }

  // step 4. Variable replacement for all original bb1
  replaceVariableReferences(bb1, var_map);
  for (SgNode *node : NodeQuery::querySubTree(bb1, V_SgVarRefExp)) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    ROSE_ASSERT(reference != NULL);
    if (shouldSkipOpenMPClauseVarRefRewrite(reference)) {
      (void)requirePreservedOpenMPClauseVarRefRole(reference);
      continue;
    }
    VariableSymbolMap_t::const_iterator stale =
        var_map.find(isSgVariableSymbol(reference->get_symbol()));
    if (stale != var_map.end()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[private-reference-identity]: "
              "reference=%p retained source symbol=%p instead of exact "
              "generated symbol=%p\n",
              static_cast<void *>(reference),
              static_cast<void *>(reference->get_symbol()),
              static_cast<void *>(stale->second));
      ROSE_ABORT();
    }
  }
  replaceVariablesWithPointerDereference(
      bb1,
      var_set); // Variables that must be replaced by a pointer to the variable
  normalizeScalarLocalDerefUses(bb1, scalar_locals_from_pointer_symbols);

  // We delay the insertion of declaration, initialization , and save-back
  // statements until variable replacement is done in order to avoid replacing
  // variables of these newly generated statements.
  prependStatementList(front_stmt_list, bb1);
  // Fortran: add initialization statements after all front statements are
  // inserted
  if (SageInterface::is_Fortran_language()) {
    SgBasicBlock *target_bb = getEnclosingRegionOrFuncDefinition(bb1);
    insertStatementAfterLastDeclaration(front_init_list, target_bb);
  } else {
    ROSE_ASSERT(front_init_list.size() == 0);
  }
  appendStatementList(end_stmt_list, bb1);
  // Liao 1/7/2010 , add assertion here, useful when generating outlined
  // functions by moving statements to a function body
  SgStatementPtrList &srcStmts = bb1->get_statements();
  for (SgStatementPtrList::iterator i = srcStmts.begin(); i != srcStmts.end();
       i++) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(*i);
    if (declaration != NULL)
      switch (declaration->variantT()) {
      case V_SgVariableDeclaration: {
        // Reset the scopes on any SgInitializedName objects.
        SgVariableDeclaration *varDecl = isSgVariableDeclaration(declaration);
        bool is_extern_decl =
            varDecl->get_declarationModifier().get_storageModifier().isExtern();
        SgInitializedNamePtrList &l = varDecl->get_variables();
        for (SgInitializedNamePtrList::iterator i = l.begin(); i != l.end();
             i++) {
          // This might be an issue for extern variable declaration that have a
          // scope in a separate namespace of a static class member defined
          // external to its class, etc. I don't want to worry about those cases
          // right now.
          if (!is_extern_decl && (*i)->get_scope() != bb1) {
            (*i)->set_scope(bb1);
          }
          ROSE_ASSERT((*i)->get_scope() == bb1);
        }
        break;
      }

      default:
        break;
      }

  } // end for
} // end void transOmpVariablesWithContext()

void transOmpVariables(SgStatement *ompStmt, SgBasicBlock *bb1,
                       SgExpression *orig_loop_upper /*= NULL*/,
                       bool isAcceleratorModel /*= false*/) {
  transOmpVariablesWithContext(ompStmt, bb1, orig_loop_upper,
                               isAcceleratorModel, NULL);
}

//  if (omp_get_thread_num () == 0)
//     { ... }
//  Or if (XOMP_master())
//     { ...  }
void transOmpMaster(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpMasterStatement *target = isSgOmpMasterStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);
  bool isLast =
      isLastStatement(target); // check this now before any transformation

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  SgIfStmt *if_stmt = NULL;
  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(require_kmpc_global_tid_source_context(node), scope,
                          &kmpc_global_tid_init);
  SgName tid_name = getFirstVariable(*kmpc_global_tid_declaration).get_name();

  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(
        func_def->get_body(),
        SgName(getKmpcRuntimeFunctionName("__kmpc_master")),
        buildKmpcInt32Type());
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }

  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }

  SgExprListExp *parameters =
      buildExprListExp(buildIntVal(0), buildVarRefExp(tid_name, scope));
  SgExpression *func_exp =
      buildFunctionCallExp(getKmpcRuntimeFunctionName("__kmpc_master"),
                           SageInterface::is_Fortran_language()
                               ? static_cast<SgType *>(buildKmpcInt32Type())
                               : static_cast<SgType *>(buildIntType()),
                           parameters, scope);
  if (SageInterface::is_Fortran_language()) {
    if_stmt = buildIfStmt(
        buildEqualityOp(func_exp, buildIntVal(1), exactLogicalResultType()),
        body, NULL);
  } else {
    if_stmt = buildIfStmt(func_exp, body, NULL);
  }

  replaceStatement(target, if_stmt, true);
  SgExprListExp *end_parameters =
      buildExprListExp(buildIntVal(0), buildVarRefExp(tid_name, scope));
  SgExprStatement *end_master_call =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_end_master"),
                            buildVoidType(), end_parameters, scope);
  SgBasicBlock *true_body = ensureBasicBlockAsTrueBodyOfIf(if_stmt);
  appendStatement(end_master_call, true_body);
  movePreprocessingInfo(target, if_stmt, PreprocessingInfo::before);
  if (isLast) // the preprocessing info after the last statement may be attached
              // to the inside of its parent scope
  {
    //    cout<<"Found a last stmt. scope is: "<<scope->class_name()<<endl;
    //    dumpPreprocInfo(scope);
    // move preprecessing info. from inside position to an after position
    movePreprocessingInfo(scope, if_stmt, PreprocessingInfo::inside,
                          PreprocessingInfo::after);
  }
}

// Two cases: without or with copyprivate clause
// without it:
//  if (GOMP_single_start ()) //bool GOMP_single_start (void)
//     { ...       }
// with it: TODO
// TODO other clauses
void transOmpSingle(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpSingleStatement *target = isSgOmpSingleStatement(node);
  ROSE_ASSERT(target != NULL);
  SgScopeStatement *scope = target->get_scope();
  ROSE_ASSERT(scope != NULL);

  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);

  // target vs. if_stmt should not share a subtree of AST (the body)
  // We need to disconnect it from old statement (target)
  // Later replaceStement has the logic to move dangling directives. repeated
  // subtree will cause troubles.
  target->set_body(NULL);

  SgIfStmt *if_stmt = NULL;

  SgStatement *kmpc_global_tid_init = NULL;
  SgVariableDeclaration *kmpc_global_tid_declaration =
      get_kmpc_global_tid(require_kmpc_global_tid_source_context(node), scope,
                          &kmpc_global_tid_init);
  SgExpression *thread_global_tid = buildVarRefExp(
      getFirstVariable(*kmpc_global_tid_declaration).get_name(), scope);
  if (SageInterface::is_Fortran_language()) {
    insert_fortran_declaration_into_procedure(kmpc_global_tid_declaration,
                                              scope);
  } else {
    insertStatement(target, kmpc_global_tid_declaration);
    kmpc_global_tid_declaration->set_parent(target->get_parent());
  }
  if (kmpc_global_tid_init != NULL) {
    if (SageInterface::is_Fortran_language())
      insertStatement(target, kmpc_global_tid_init);
    else
      insertStatementAfter(kmpc_global_tid_declaration, kmpc_global_tid_init);
  }
  SgExprListExp *single_parameters =
      buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));

  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(
        func_def->get_body(),
        SgName(getKmpcRuntimeFunctionName("__kmpc_single")),
        buildKmpcInt32Type());
    SgExpression *func_exp =
        buildFunctionCallExp(getKmpcRuntimeFunctionName("__kmpc_single"),
                             buildKmpcInt32Type(), single_parameters, scope);
    if_stmt = buildIfStmt(
        buildEqualityOp(func_exp, buildIntVal(1), exactLogicalResultType()),
        body, NULL);
  } else // C/C++
  {
    SgExpression *func_exp =
        buildFunctionCallExp(getKmpcRuntimeFunctionName("__kmpc_single"),
                             buildBoolType(), single_parameters, scope);
    if_stmt = buildIfStmt(func_exp, body, NULL);
  }

  replaceStatement(target, if_stmt, true);
  SgBasicBlock *true_body = ensureBasicBlockAsTrueBodyOfIf(if_stmt);
  transOmpVariables(target, true_body);

  SgExprListExp *end_single_parameters =
      buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
  SgExprStatement *end_single_call =
      buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_end_single"),
                            buildVoidType(), end_single_parameters, scope);
  insertStatementAfter(body, end_single_call);

  // handle nowait
  if (!hasClause(target, V_SgOmpNowaitClause)) {
    SgExprListExp *barrier_parameters =
        buildExprListExp(buildIntVal(0), copyExpression(thread_global_tid));
    SgExprStatement *barrier_call =
        buildFunctionCallStmt(getKmpcRuntimeFunctionName("__kmpc_barrier"),
                              buildVoidType(), barrier_parameters, scope);
    insertStatementAfter(if_stmt, barrier_call);
  }
}

//! Build a non-reduction variable clause for a given OpenMP directive. It
//! directly returns the clause if the clause already exists
SgOmpVariablesClause *
buildOmpVariableClause(SgOmpClauseBodyStatement *clause_stmt,
                       const VariantT &vt) {
  SgOmpVariablesClause *result = NULL;
  ROSE_ASSERT(clause_stmt != NULL);
  ROSE_ASSERT(vt != V_SgOmpReductionClause);
  Rose_STL_Container<SgOmpClause *> clauses = getClause(clause_stmt, vt);

  if (clauses.size() == 0) {
    switch (vt) {
    case V_SgOmpCopyinClause:
      result = new SgOmpCopyinClause(buildExprListExp());
      break;
    case V_SgOmpCopyprivateClause:
      result = new SgOmpCopyprivateClause(buildExprListExp());
      break;
    case V_SgOmpFirstprivateClause:
      result = new SgOmpFirstprivateClause(buildExprListExp());
      break;
    case V_SgOmpLastprivateClause:
      result = new SgOmpLastprivateClause(
          buildExprListExp(),
          SgOmpClause::e_omp_lastprivate_modifier_unspecified);
      break;
    case V_SgOmpPrivateClause:
      result = new SgOmpPrivateClause(buildExprListExp());
      break;
    case V_SgOmpSharedClause:
      result = new SgOmpSharedClause(buildExprListExp());
      break;
    case V_SgOmpReductionClause:
    default:
      cerr << "Unacceptable clause type in "
              "OmpSupport::buildOmpVariableClause(): "
           << vt << endl;
      ROSE_ABORT();
    }
  } else {
    result = isSgOmpVariablesClause(clauses[0]);
  }
  ROSE_ASSERT(result != NULL);
  ROSE_ASSERT(result->get_variables() != NULL);
  if (clauses.empty()) {
    result->get_variables()->set_parent(result);
  } else if (result->get_variables()->get_parent() != result) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-variables]: existing "
              << result->class_name() << " does not own its variable list\n";
    ROSE_ABORT();
  }
  setOneSourcePositionForTransformation(result);

  addGeneratedOmpClause(clause_stmt, result);

  return result;
}

//! Remove one or more clauses of type vt
int removeClause(SgStatement *clause_stmt, const VariantT &vt) {
  ROSE_ASSERT(clause_stmt != NULL);
  ROSE_ASSERT(isSgOmpClauseBodyStatement(clause_stmt) ||
              isSgOmpClauseStatement(clause_stmt));
  SgOmpClauseList *clause_list = getOmpClauseList(clause_stmt);
  std::vector<SgOmpClause *> matching_clauses;
  for (SgOmpClause *c_clause : clause_list->get_clauses()) {
    if (c_clause->variantT() == vt)
      matching_clauses.push_back(c_clause);
  }

  for (SgOmpClause *clause : matching_clauses)
    clause_list->remove_clause(clause);
  return matching_clauses.size();
}

//! Add a variable into a non-reduction clause of an OpenMP statement, create
//! the clause transparently if it does not exist
void addClauseVariable(SgInitializedName *var,
                       SgOmpClauseBodyStatement *clause_stmt,
                       const VariantT &vt) {
  ROSE_ASSERT(var != NULL);
  ROSE_ASSERT(clause_stmt != NULL);
  ROSE_ASSERT(vt != V_SgOmpReductionClause);
  Rose_STL_Container<SgOmpClause *> clauses = getClause(clause_stmt, vt);
  SgOmpVariablesClause *target_clause = NULL;
  // create the clause if it does not exist
  if (clauses.size() == 0) {
    target_clause = buildOmpVariableClause(clause_stmt, vt);
  } else {
    target_clause = isSgOmpVariablesClause(clauses[0]);
  }
  ROSE_ASSERT(target_clause != NULL);

  // Insert only if the variable is not in the list
  if (!isInClauseVariableList(var, clause_stmt, vt)) {
    SgExprListExp *variables = target_clause->get_variables();
    ROSE_ASSERT(variables != NULL);
    SgVarRefExp *reference = buildVarRefExp(var);
    ROSE_ASSERT(reference != NULL);
    variables->get_expressions().push_back(reference);
    reference->set_parent(variables);
  }
}

// Patch up private variables for a single OpenMP For or DO loop
// return the number of private variables added.
int patchUpPrivateVariables(SgStatement *omp_loop) {
  int result = 0;
  ROSE_ASSERT(omp_loop != NULL);

  SgOmpDoStatement *do_node = NULL;
  SgOmpClauseBodyStatement *for_node = NULL;
  switch (omp_loop->variantT()) {
  case V_SgOmpDoStatement:
    do_node = isSgOmpDoStatement(omp_loop);
    break;
  case V_SgOmpForStatement:
  case V_SgOmpTargetParallelForStatement:
  case V_SgOmpTargetTeamsDistributeParallelForStatement:
  case V_SgOmpTargetTeamsDistributeStatement:
    for_node = isSgOmpClauseBodyStatement(omp_loop);
    break;
  default:
    MLOG_ERROR_CXX("ompLowering")
        << "Unexpected statement kind in patchUpPrivateVariables(): "
        << omp_loop->sage_class_name();
    ROSE_ABORT();
  }

  if (do_node)
    omp_loop = do_node;
  else
    omp_loop = for_node;

  SgScopeStatement *directive_scope = omp_loop->get_scope();
  ROSE_ASSERT(directive_scope != NULL);
  // Collected nested loops and their indices
  // skip the top level loop?
  Rose_STL_Container<SgNode *> loops;
  if (do_node)
    loops = NodeQuery::querySubTree(do_node->get_body(), V_SgFortranDo);
  else
    loops = NodeQuery::querySubTree(for_node->get_body(), V_SgForStatement);
  // For all loops within the OpenMP loop
  Rose_STL_Container<SgNode *>::iterator loopIter = loops.begin();
  for (; loopIter != loops.end(); loopIter++) {
    SgInitializedName *index_var = getLoopIndexVariable(*loopIter);
    ROSE_ASSERT(index_var != NULL);
    SgVariableSymbol *variable_symbol =
        isSgVariableSymbol(index_var->get_symbol_from_symbol_table());
    ROSE_ASSERT(variable_symbol != NULL);
    SgScopeStatement *var_scope = index_var->get_scope();
    // Only loop index variables declared in higher or the same scopes
    // matter
    if (isAncestor(var_scope, directive_scope) ||
        var_scope == directive_scope) {
      // Grab possible enclosing parallel region
      bool isPrivateInRegion = false;
      SgOmpClauseBodyStatement *omp_stmt = NULL;
      switch (omp_loop->variantT()) {
      case V_SgOmpTargetParallelForStatement:
      case V_SgOmpTargetTeamsDistributeStatement:
      case V_SgOmpTargetTeamsDistributeParallelForStatement:
        omp_stmt = isSgOmpClauseBodyStatement(omp_loop);
        break;
      case V_SgOmpForStatement:
      case V_SgOmpDoStatement:
        omp_stmt = isSgOmpParallelStatement(
            getEnclosingNode<SgOmpParallelStatement>(omp_loop));
        break;
      default:
        ROSE_ABORT();
      }
      // Orphaned omp do/for constructs can be outside an explicit enclosing
      // parallel clause body in the local AST context.
      if (omp_stmt != NULL) {
        isPrivateInRegion = isInClauseVariableList(
            index_var, isSgOmpClauseBodyStatement(omp_stmt),
            V_SgOmpPrivateClause);
      }
      // Keep enclosing parallel region consistent with worksharing default
      // loop-index privatization so outlining does not treat loop indices as
      // shared parameters.
      if (omp_stmt != NULL && !isPrivateInRegion) {
        addClauseVariable(index_var, isSgOmpClauseBodyStatement(omp_stmt),
                          V_SgOmpPrivateClause);
        isPrivateInRegion = true;
        result++;
      }
      // add it into the private variable list only if it is not specified as
      // private in both the loop and region levels.
      if (!isPrivateInRegion &&
          !isInClauseVariableList(index_var,
                                  isSgOmpClauseBodyStatement(omp_loop),
                                  V_SgOmpPrivateClause)) {
        result++;
        addClauseVariable(index_var, isSgOmpClauseBodyStatement(omp_loop),
                          V_SgOmpPrivateClause);
      }
    }

  } // end for loops
  return result;
}

/*
 * Winnie, Handle collapse clause before openmp and openmp accelerator
 * add new variables inserted by SageInterface::loopCollasping() into mapin
 * clause
 *
 * This function passes target for loop of collpase clause and the collapse
 * factor to the function SageInterface::loopCollapse. After return from
 * SageInterface::loopCollapse, this function will insert new
 * variables(generated by loopCollapse()) into map to or map tofrom clause, if
 * the collapse clause comes with target directive.
 *
 */
namespace {
struct FortranCollapseLoop {
  SgFortranDo *loop = nullptr;
  SgInitializedName *index = nullptr;
  SgExpression *lower = nullptr;
  SgExpression *upper = nullptr;
  SgExpression *step = nullptr;
  bool positive_step = false;
  SgVariableDeclaration *captured_lower = nullptr;
  SgVariableDeclaration *captured_upper = nullptr;
  SgVariableDeclaration *captured_step = nullptr;
  SgVariableDeclaration *trip_count = nullptr;
};

bool expressionReferencesFortranCollapseIndex(
    SgExpression *expression,
    const std::unordered_set<SgInitializedName *> &indices) {
  if (expression == nullptr)
    return false;
  const Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(expression, V_SgVarRefExp);
  for (SgNode *node : references) {
    SgVarRefExp *reference = isSgVarRefExp(node);
    SgVariableSymbol *symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    if (symbol != nullptr && indices.count(symbol->get_declaration()) != 0)
      return true;
  }
  return false;
}

std::vector<FortranCollapseLoop>
requireExactFortranCollapseNest(SgFortranDo *outer, size_t factor) {
  if (outer == nullptr || factor == 0) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[fortran-collapse-preflight]: loop=%p "
            "factor=%zu does not identify a collapsible nest\n",
            static_cast<void *>(outer), factor);
    ROSE_ABORT();
  }

  std::vector<FortranCollapseLoop> loops;
  loops.reserve(factor);
  SgFortranDo *current = outer;
  for (size_t level = 0; level < factor; ++level) {
    SageInterface::doLoopNormalization(current);
    FortranCollapseLoop info;
    info.loop = current;
    SgStatement *body = nullptr;
    SageInterface::CanonicalFortranLoopDirection direction =
        SageInterface::CanonicalFortranLoopDirection::runtime;
    bool inclusive = false;
    if (!SageInterface::isCanonicalDoLoop(current, &info.index, &info.lower,
                                          &info.upper, &info.step, &body,
                                          &direction, &inclusive) ||
        info.index == nullptr || info.lower == nullptr ||
        info.upper == nullptr || info.step == nullptr || body == nullptr ||
        !inclusive) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-preflight]: "
              "level=%zu loop=%p is not one exact canonical Fortran DO\n",
              level, static_cast<void *>(current));
      ROSE_ABORT();
    }
    std::unordered_set<SgInitializedName *> active;
    const std::optional<ExactIntegralConstant> step_value =
        evaluateTypedIntegralConstant(info.step, active);
    if (!step_value || step_value->type.is_unsigned ||
        signedIntegralValue(*step_value) == 0) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-stride]: "
              "level=%zu stride=%p/%s is not one exact nonzero signed "
              "constant\n",
              level, static_cast<void *>(info.step),
              info.step->sage_class_name());
      ROSE_ABORT();
    }
    info.positive_step = signedIntegralValue(*step_value) > 0;
    const SageInterface::CanonicalFortranLoopDirection expected_direction =
        info.positive_step
            ? SageInterface::CanonicalFortranLoopDirection::increasing
            : SageInterface::CanonicalFortranLoopDirection::decreasing;
    if (direction != expected_direction) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-direction]: "
              "level=%zu canonical direction=%d does not match exact "
              "stride direction=%d\n",
              level, static_cast<int>(direction),
              static_cast<int>(expected_direction));
      ROSE_ABORT();
    }
    loops.push_back(info);

    if (level + 1 == factor)
      break;
    SgBasicBlock *block = isSgBasicBlock(body);
    const SgStatementPtrList *statements =
        block != nullptr ? &block->get_statements() : nullptr;
    if (statements == nullptr || statements->size() != 1 ||
        statements->front() == nullptr ||
        statements->front()->get_parent() != block ||
        isSgFortranDo(statements->front()) == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-perfect-nest]: "
              "level=%zu body=%p does not own exactly one nested Fortran DO\n",
              level, static_cast<void *>(body));
      ROSE_ABORT();
    }
    current = isSgFortranDo(statements->front());
  }

  std::unordered_set<SgInitializedName *> indices;
  for (const FortranCollapseLoop &loop : loops) {
    if (!indices.insert(loop.index).second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-index]: one loop "
              "index is reused by multiple associated loops\n");
      ROSE_ABORT();
    }
  }
  for (size_t level = 0; level < loops.size(); ++level) {
    const FortranCollapseLoop &loop = loops[level];
    if (expressionReferencesFortranCollapseIndex(loop.lower, indices) ||
        expressionReferencesFortranCollapseIndex(loop.upper, indices) ||
        expressionReferencesFortranCollapseIndex(loop.step, indices)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-rectangular]: "
              "level=%zu loop bounds or stride depend on a collapsed index\n",
              level);
      ROSE_ABORT();
    }
  }
  return loops;
}

SgExpression *
buildFortranCollapseProduct(const std::vector<FortranCollapseLoop> &loops,
                            size_t begin, SgType *type) {
  SgExpression *product = buildIntVal(1);
  for (size_t index = begin; index < loops.size(); ++index) {
    product =
        buildMultiplyOp(product, buildVarRefExp(loops[index].trip_count), type);
  }
  return product;
}

SgExprListExp *collapseFortranDoNest(SgOmpClauseBodyStatement *target,
                                     SgFortranDo *outer, size_t factor) {
  std::vector<FortranCollapseLoop> loops =
      requireExactFortranCollapseNest(outer, factor);
  SgScopeStatement *scope = target->get_scope();
  SgBasicBlock *procedure_body = getEnclosingFortranProcedureBody(scope);
  if (scope == nullptr || procedure_body == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[fortran-collapse-owner]: directive "
            "has no exact procedure scope\n");
    ROSE_ABORT();
  }

  SgType *flattened_type = buildKmpcInt64Type();
  auto declare_state = [&](const std::string &prefix) {
    const std::string name = generateUniqueVariableName(procedure_body, prefix);
    SgVariableDeclaration *declaration =
        buildVariableDeclaration(name, flattened_type, nullptr, procedure_body);
    insert_fortran_declaration_into_procedure(declaration, scope);
    return declaration;
  };

  SgExprListExp *new_variables = buildExprListExp();
  for (size_t level = 0; level < loops.size(); ++level) {
    FortranCollapseLoop &loop = loops[level];
    const std::string stem =
        "__rex_collapse_" + StringUtility::numberToString(level) + "_";
    loop.captured_lower = declare_state(stem + "lower_");
    loop.captured_upper = declare_state(stem + "upper_");
    loop.captured_step = declare_state(stem + "step_");
    loop.trip_count = declare_state(stem + "count_");

    insertStatementBefore(
        target, buildAssignStatement(buildVarRefExp(loop.captured_lower),
                                     copyExpression(loop.lower)));
    insertStatementBefore(
        target, buildAssignStatement(buildVarRefExp(loop.captured_upper),
                                     copyExpression(loop.upper)));
    insertStatementBefore(
        target, buildAssignStatement(buildVarRefExp(loop.captured_step),
                                     copyExpression(loop.step)));
    insertStatementBefore(
        target,
        buildAssignStatement(buildVarRefExp(loop.trip_count), buildIntVal(0)));

    SgExpression *has_iterations =
        loop.positive_step
            ? static_cast<SgExpression *>(
                  buildLessOrEqualOp(buildVarRefExp(loop.captured_lower),
                                     buildVarRefExp(loop.captured_upper),
                                     exactLogicalResultType()))
            : static_cast<SgExpression *>(
                  buildGreaterOrEqualOp(buildVarRefExp(loop.captured_lower),
                                        buildVarRefExp(loop.captured_upper),
                                        exactLogicalResultType()));
    SgExpression *distance =
        loop.positive_step
            ? static_cast<SgExpression *>(buildSubtractOp(
                  buildVarRefExp(loop.captured_upper),
                  buildVarRefExp(loop.captured_lower), flattened_type))
            : static_cast<SgExpression *>(buildSubtractOp(
                  buildVarRefExp(loop.captured_lower),
                  buildVarRefExp(loop.captured_upper), flattened_type));
    SgExpression *stride_magnitude =
        loop.positive_step
            ? static_cast<SgExpression *>(buildVarRefExp(loop.captured_step))
            : static_cast<SgExpression *>(buildMinusOp(
                  buildVarRefExp(loop.captured_step), flattened_type));
    SgExpression *count =
        buildAddOp(buildDivideOp(distance, stride_magnitude, flattened_type),
                   buildIntVal(1), flattened_type);
    insertStatementBefore(
        target, buildIfStmt(has_iterations,
                            buildAssignStatement(
                                buildVarRefExp(loop.trip_count), count),
                            nullptr));

    new_variables->append_expression(buildVarRefExp(loop.captured_lower));
    new_variables->append_expression(buildVarRefExp(loop.captured_upper));
    new_variables->append_expression(buildVarRefExp(loop.captured_step));
    new_variables->append_expression(buildVarRefExp(loop.trip_count));
  }

  SgVariableDeclaration *total_count = declare_state("__rex_collapse_total_");
  SgVariableDeclaration *flattened_index =
      declare_state("__rex_collapse_index_");
  insertStatementBefore(
      target, buildAssignStatement(
                  buildVarRefExp(total_count),
                  buildFortranCollapseProduct(loops, 0, flattened_type)));
  new_variables->append_expression(buildVarRefExp(total_count));
  new_variables->append_expression(buildVarRefExp(flattened_index));

  SgBasicBlock *collapsed_body =
      isSgBasicBlock(deepCopy(loops.back().loop->get_body()));
  if (collapsed_body == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[fortran-collapse-body]: innermost "
            "loop body did not copy as one basic block\n");
    ROSE_ABORT();
  }

  std::vector<SgStatement *> index_assignments;
  SgExpression *remainder = buildVarRefExp(flattened_index);
  for (size_t level = 0; level < loops.size(); ++level) {
    FortranCollapseLoop &loop = loops[level];
    SgExpression *position = remainder;
    if (level + 1 < loops.size()) {
      SgExpression *interval =
          buildFortranCollapseProduct(loops, level + 1, flattened_type);
      position = buildDivideOp(copyExpression(remainder),
                               copyExpression(interval), flattened_type);
      remainder = buildModOp(remainder, interval, flattened_type);
    }
    SgExpression *value =
        buildAddOp(buildVarRefExp(loop.captured_lower),
                   buildMultiplyOp(position, buildVarRefExp(loop.captured_step),
                                   flattened_type),
                   flattened_type);
    index_assignments.push_back(
        buildAssignStatement(buildVarRefExp(loop.index), value));
  }
  SgExpression *initialization = buildAssignOp(buildVarRefExp(flattened_index),
                                               buildIntVal(0), flattened_type);
  SgExpression *bound = buildSubtractOp(buildVarRefExp(total_count),
                                        buildIntVal(1), flattened_type);
  SgFortranDo *collapsed =
      buildFortranDo(initialization, bound, buildIntVal(1), collapsed_body);
  collapsed->set_has_end_statement(true);
  replaceStatement(outer, collapsed, true);
  prependStatementList(index_assignments, collapsed_body);
  return new_variables;
}
} // namespace

void transOmpCollapse(SgStatement *node) {

  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  SgStatement *associated_loop = requireExactAssociatedLoop(
      target, body, AssociatedLoopPathContract::Worksharing,
      "collapse-associated-loop");
  const size_t collapse_factor = requireExactCollapseFactor(target);
  if (getScope(associated_loop) == nullptr ||
      getScope(associated_loop)->get_parent() == nullptr ||
      getScope(associated_loop)->get_parent()->get_parent() == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-owner]: associated loop has "
            "no exact enclosing mutation scope\n");
    ROSE_ABORT();
  }
  SgExprListExp *new_var_list = nullptr;
  if (SgForStatement *for_loop = isSgForStatement(associated_loop)) {
    (void)requireExactCollapsePreflight(target, for_loop);
    new_var_list =
        SageInterface::loopCollapsing(for_loop, collapse_factor, target);
  } else if (SgFortranDo *fortran_loop = isSgFortranDo(associated_loop)) {
    new_var_list = collapseFortranDoNest(target, fortran_loop, collapse_factor);
  } else {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-associated-loop]: loop=%p "
            "kind=%s is not a supported canonical loop\n",
            static_cast<void *>(associated_loop),
            associated_loop->sage_class_name());
    ROSE_ABORT();
  }
  if (collapse_factor > 1 && new_var_list == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[collapse-commit]: checked collapse "
            "did not produce its exact generated-variable list\n");
    ROSE_ABORT();
  }
  SgStatement *collapsed_loop = requireExactAssociatedLoop(
      target, target->get_body(), AssociatedLoopPathContract::Worksharing,
      "collapse-associated-loop-postcommit");
  if (SgForStatement *collapsed_for = isSgForStatement(collapsed_loop)) {
    (void)SageInterface::requireCheckedCanonicalLoopPlan(
        collapsed_for, "omp-collapse-postcommit");
  } else if (SgFortranDo *collapsed_do = isSgFortranDo(collapsed_loop)) {
    if (!SageInterface::isCanonicalDoLoop(collapsed_do, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          nullptr)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-collapse-postcommit]: "
              "generated loop is not canonical\n");
      ROSE_ABORT();
    }
  } else {
    ROSE_ABORT();
  }

  // remove the collapse clause
  removeClause(node, V_SgOmpCollapseClause);
  // we need to insert the loop index variable of the collapsed loop into the
  // private() clause
  patchUpPrivateVariables(node);

  /*
   *Winnie, we need to add the new variables into the map in list, if there is a
   *SgOmpTargetStatement
   */
  /*For OmpTarget, we need to create SgOmpMapClause if there is no such clause
   * in the original code. target_stmt, #pragma omp target or, #pragma omp
   * parallel, when is not OmpTarget inside this if condition, ompacc=false
   * means there is no map clause, we need to create one outside this if
   * condition, ompacc=false means, no need to add new variables in the map in
   * clause
   *   TODO: adding the variables into the map() clause is not sufficient.
   *         we have to move the corresponding variable declarations to be in
   * front of the directive containing map().
   */
  SgOmpClauseBodyStatement *target_stmt = nullptr;
  for (SgNode *ancestor = node; ancestor != nullptr;
       ancestor = ancestor->get_parent()) {
    switch (ancestor->variantT()) {
    case V_SgOmpTargetStatement:
    case V_SgOmpTargetParallelStatement:
    case V_SgOmpTargetParallelForStatement:
    case V_SgOmpTargetTeamsStatement:
    case V_SgOmpTargetTeamsDistributeStatement:
    case V_SgOmpTargetTeamsDistributeParallelForStatement:
      target_stmt = isSgOmpClauseBodyStatement(ancestor);
      break;
    default:
      continue;
    }
    break;
  }
  if (target_stmt != nullptr) {
    if (new_var_list == nullptr || new_var_list->get_expressions().empty()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[collapse-target-capture]: target=%p "
              "generated=%p has no exact host-to-device capture payload\n",
              static_cast<void *>(target_stmt),
              static_cast<void *>(new_var_list));
      ROSE_ABORT();
    }

    // Collapse setup values are immutable host-computed inputs. Give them one
    // dedicated typed `map(to:)` surface instead of appending them to an
    // arbitrary source clause whose operator and source-spelling cache belong
    // to different items.
    SgExprListExp *generated_variables = buildExprListExp();
    SgOmpMapClause *map_to =
        new SgOmpMapClause(generated_variables, SgOmpClause::e_omp_map_to);
    generated_variables->set_parent(map_to);
    setOneSourcePositionForTransformation(map_to);
    addGeneratedOmpClause(target_stmt, map_to);
    if (map_to->get_parent() != target_stmt->get_clause_list() ||
        map_to->get_variables() != generated_variables ||
        generated_variables->get_parent() != map_to ||
        map_to->get_operation() != SgOmpClause::e_omp_map_to) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[collapse-target-capture]: target=%p "
              "rejected its exact generated map(to:) clause\n",
              static_cast<void *>(target_stmt));
      ROSE_ABORT();
    }

    for (SgExpression *generated : new_var_list->get_expressions()) {
      SgVarRefExp *generated_reference = isSgVarRefExp(generated);
      SgVariableSymbol *generated_symbol =
          generated_reference != nullptr ? generated_reference->get_symbol()
                                         : nullptr;
      if (generated_symbol == nullptr ||
          generated_symbol->get_declaration() == nullptr) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[collapse-target-capture]: "
                "generated expression=%p is not one exact typed variable "
                "reference\n",
                static_cast<void *>(generated));
        ROSE_ABORT();
      }
      SgExpression *locator = buildVarRefExp(generated_symbol);
      SgOmpMapItem *item = new SgOmpMapItem(locator);
      locator->set_parent(item);
      setOneSourcePositionForTransformation(item);
      generated_variables->append_expression(item);
      if (item->get_parent() != generated_variables ||
          item->get_expression() != locator || locator->get_parent() != item) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[collapse-target-capture]: "
                "generated variable '%s' was not published as one exact "
                "typed map item\n",
                generated_symbol->get_name().getString().c_str());
        ROSE_ABORT();
      }
    }
  }
} // Winnie, end of loop collapse

//! Lower a canonical Fortran taskloop into exact striped tasks.  A task owns
//! every `effective_task_count`-th logical iteration, which creates precisely
//! min(num_tasks, trip_count) nonempty tasks without evaluating the source
//! loop bounds or num_tasks expression more than once.
void transOmpTaskloop(SgNode *node) {
  SgOmpTaskloopStatement *target = isSgOmpTaskloopStatement(node);
  SgScopeStatement *scope = target != nullptr ? target->get_scope() : nullptr;
  SgOmpClauseList *clause_list =
      target != nullptr ? getOmpClauseList(target) : nullptr;
  if (target == nullptr || scope == nullptr || clause_list == nullptr ||
      clause_list->get_parent() != target ||
      !SageInterface::is_Fortran_language()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskloop-owner]: node=%p is not one "
            "attached Fortran taskloop with exact clause ownership\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }

  SgStatement *associated = requireExactAssociatedLoop(
      target, target->get_body(), AssociatedLoopPathContract::Worksharing,
      "taskloop-associated-loop");
  SgFortranDo *source_loop = isSgFortranDo(associated);
  if (source_loop == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskloop-loop]: associated statement "
            "is %s instead of one canonical Fortran DO\n",
            associated->sage_class_name());
    ROSE_ABORT();
  }
  std::vector<FortranCollapseLoop> loop_info =
      requireExactFortranCollapseNest(source_loop, 1);
  FortranCollapseLoop &loop = loop_info.front();

  SgExpression *num_tasks_expression = nullptr;
  bool nogroup = false;
  for (SgOmpClause *clause : clause_list->get_clauses()) {
    if (clause == nullptr || clause->get_parent() != clause_list) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[taskloop-clause]: clause has no "
              "exact list ownership\n");
      ROSE_ABORT();
    }
    if (SgOmpNumTasksClause *num_tasks = isSgOmpNumTasksClause(clause)) {
      if (num_tasks_expression != nullptr ||
          num_tasks->get_expression() == nullptr ||
          num_tasks->get_expression()->get_parent() != num_tasks) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[taskloop-num-tasks]: directive "
                "does not own one exact num_tasks expression\n");
        ROSE_ABORT();
      }
      num_tasks_expression = num_tasks->get_expression();
    } else if (isSgOmpNogroupClause(clause) != nullptr) {
      if (nogroup) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[taskloop-nogroup]: directive "
                "contains duplicate nogroup clauses\n");
        ROSE_ABORT();
      }
      nogroup = true;
    } else {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[taskloop-clause]: clause %s "
              "requires a distinct exact lowering path\n",
              clause->sage_class_name());
      ROSE_ABORT();
    }
  }

  SgBasicBlock *procedure_body = getEnclosingFortranProcedureBody(scope);
  if (procedure_body == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskloop-procedure]: directive has "
            "no exact enclosing Fortran procedure body\n");
    ROSE_ABORT();
  }
  SgType *counter_type = buildKmpcInt64Type();
  auto declare_counter = [&](const std::string &prefix) {
    const std::string name = generateUniqueVariableName(procedure_body, prefix);
    SgVariableDeclaration *declaration =
        buildVariableDeclaration(name, counter_type, nullptr, procedure_body);
    insert_fortran_declaration_into_procedure(declaration, scope);
    return declaration;
  };

  SgVariableDeclaration *captured_lower =
      declare_counter("__rex_taskloop_lower_");
  SgVariableDeclaration *captured_upper =
      declare_counter("__rex_taskloop_upper_");
  SgVariableDeclaration *captured_step =
      declare_counter("__rex_taskloop_step_");
  SgVariableDeclaration *trip_count =
      declare_counter("__rex_taskloop_trip_count_");
  SgVariableDeclaration *requested_tasks =
      declare_counter("__rex_taskloop_requested_tasks_");
  SgVariableDeclaration *effective_tasks =
      declare_counter("__rex_taskloop_effective_tasks_");
  SgVariableDeclaration *task_index =
      declare_counter("__rex_taskloop_task_index_");

  insertStatementBefore(target,
                        buildAssignStatement(buildVarRefExp(captured_lower),
                                             copyExpression(loop.lower)));
  insertStatementBefore(target,
                        buildAssignStatement(buildVarRefExp(captured_upper),
                                             copyExpression(loop.upper)));
  insertStatementBefore(target,
                        buildAssignStatement(buildVarRefExp(captured_step),
                                             copyExpression(loop.step)));
  insertStatementBefore(
      target, buildAssignStatement(buildVarRefExp(trip_count), buildIntVal(0)));

  SgExpression *has_iterations =
      loop.positive_step
          ? static_cast<SgExpression *>(buildLessOrEqualOp(
                buildVarRefExp(captured_lower), buildVarRefExp(captured_upper),
                exactLogicalResultType()))
          : static_cast<SgExpression *>(buildGreaterOrEqualOp(
                buildVarRefExp(captured_lower), buildVarRefExp(captured_upper),
                exactLogicalResultType()));
  SgExpression *distance =
      loop.positive_step
          ? static_cast<SgExpression *>(
                buildSubtractOp(buildVarRefExp(captured_upper),
                                buildVarRefExp(captured_lower), counter_type))
          : static_cast<SgExpression *>(
                buildSubtractOp(buildVarRefExp(captured_lower),
                                buildVarRefExp(captured_upper), counter_type));
  SgExpression *stride_magnitude =
      loop.positive_step
          ? static_cast<SgExpression *>(buildVarRefExp(captured_step))
          : static_cast<SgExpression *>(
                buildMinusOp(buildVarRefExp(captured_step), counter_type));
  SgExpression *computed_trip_count =
      buildAddOp(buildDivideOp(distance, stride_magnitude, counter_type),
                 buildIntVal(1), counter_type);
  insertStatementBefore(
      target, buildIfStmt(has_iterations,
                          buildAssignStatement(buildVarRefExp(trip_count),
                                               computed_trip_count),
                          nullptr));

  insertStatementBefore(
      target, buildAssignStatement(buildVarRefExp(requested_tasks),
                                   num_tasks_expression != nullptr
                                       ? copyExpression(num_tasks_expression)
                                       : buildVarRefExp(trip_count)));
  SgExprStatement *abort_call = buildFunctionCallStmt(
      "abort", buildVoidType(), buildExprListExp(), scope);
  insertStatementBefore(
      target,
      buildIfStmt(buildLessOrEqualOp(buildVarRefExp(requested_tasks),
                                     buildIntVal(0), exactLogicalResultType()),
                  abort_call, nullptr));
  insertStatementBefore(target,
                        buildAssignStatement(buildVarRefExp(effective_tasks),
                                             buildVarRefExp(trip_count)));
  insertStatementBefore(
      target, buildIfStmt(buildLessThanOp(buildVarRefExp(requested_tasks),
                                          buildVarRefExp(effective_tasks),
                                          exactLogicalResultType()),
                          buildAssignStatement(buildVarRefExp(effective_tasks),
                                               buildVarRefExp(requested_tasks)),
                          nullptr));

  SgBasicBlock *task_loop_body =
      isSgBasicBlock(deepCopy(loop.loop->get_body()));
  if (task_loop_body == nullptr) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[taskloop-body]: canonical loop body "
            "did not copy as one exact basic block\n");
    ROSE_ABORT();
  }
  SgExpression *task_lower =
      buildAddOp(buildVarRefExp(captured_lower),
                 buildMultiplyOp(buildVarRefExp(task_index),
                                 buildVarRefExp(captured_step), counter_type),
                 counter_type);
  SgExpression *task_stride =
      buildMultiplyOp(buildVarRefExp(captured_step),
                      buildVarRefExp(effective_tasks), counter_type);
  SgFortranDo *striped_loop = buildFortranDo(
      buildAssignOp(buildVarRefExp(loop.index), task_lower, counter_type),
      buildVarRefExp(captured_upper), task_stride, task_loop_body);
  striped_loop->set_has_end_statement(true);

  const KmpcGlobalTidSourceContext generated_source =
      require_kmpc_global_tid_source_context(target);
  SgOmpTaskStatement *task = new SgOmpTaskStatement(striped_loop);
  SageInterface::setOneSourcePositionForTransformation(task);
  striped_loop->set_parent(task);
  task->get_clause_list()->set_parent(task);
  attach_kmpc_source_context(task, generated_source,
                             KmpcSourceContextOrigin::generated_directive);
  SgBasicBlock *generation_body = buildBasicBlock();
  SageInterface::publishGeneratedSubtreeOutputOwner(generation_body, target);
  generation_body->append_statement(task);
  task->set_parent(generation_body);
  SgFortranDo *generation_loop = buildFortranDo(
      buildAssignOp(buildVarRefExp(task_index), buildIntVal(0), counter_type),
      buildSubtractOp(buildVarRefExp(effective_tasks), buildIntVal(1),
                      counter_type),
      buildIntVal(1), generation_body);
  generation_loop->set_has_end_statement(true);

  SgStatement *replacement = generation_loop;
  if (!nogroup) {
    SgBasicBlock *taskgroup_body = buildBasicBlock();
    SageInterface::publishGeneratedSubtreeOutputOwner(taskgroup_body, target);
    taskgroup_body->append_statement(generation_loop);
    generation_loop->set_parent(taskgroup_body);
    SgOmpTaskgroupStatement *taskgroup =
        new SgOmpTaskgroupStatement(taskgroup_body);
    SageInterface::setOneSourcePositionForTransformation(taskgroup);
    taskgroup_body->set_parent(taskgroup);
    taskgroup->get_clause_list()->set_parent(taskgroup);
    attach_kmpc_source_context(taskgroup, generated_source,
                               KmpcSourceContextOrigin::generated_directive);
    replacement = taskgroup;
  }

  AttachedPreprocessingInfoType before, after;
  cutPreprocessingInfo(target, PreprocessingInfo::before, before);
  cutPreprocessingInfo(target, PreprocessingInfo::after, after);
  replaceStatement(target, replacement, true);
  pastePreprocessingInfo(replacement, PreprocessingInfo::before, before);
  pastePreprocessingInfo(replacement, PreprocessingInfo::after, after);
}

bool isInOmpTargetOffloadingFunc(SgNode *node) {
  SgNode *parent = node->get_parent();
  do {
    if (isSgFunctionDeclaration(parent))
      break;
    parent = parent->get_parent();
  } while (parent);

  if (std::find(target_outlined_function_list->begin(),
                target_outlined_function_list->end(),
                parent) != target_outlined_function_list->end())
    return true;
  else
    return false;
}

//! Bottom-up processing AST tree to translate all OpenMP constructs
// the major interface of omp_lowering
// We now operation on scoped OpenMP regions and blocks
//    SgBasicBlock
//      /                   #
//     /                    #
// SgOmpParallelStatement   #
//          \               #
//           \              #
//           SgBasicBlock   #
//               \          #
//                \         #
//                SgOmpParallelStatement
void lower_omp(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  bool saved_case_insensitive =
      SageBuilder::symbol_table_case_insensitive_semantics;

  for (VariantT unsupported_declarative :
       {V_SgOmpDeclareSimdStatement, V_SgOmpDeclareVariantStatement}) {
    Rose_STL_Container<SgNode *> unsupported =
        NodeQuery::querySubTree(file, unsupported_declarative);
    if (!unsupported.empty()) {
      SgNode *first = unsupported.front();
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[unsupported-declarative]: "
              "source contains %zu visible %s directives whose compile-time "
              "semantics cannot be represented by runtime lowering\n",
              unsupported.size(), first->sage_class_name());
      ROSE_ABORT();
    }
  }

  const bool has_target_offload = hasTargetOffloadConstructs(file);
  if (file->get_Fortran_only())
    SageBuilder::symbol_table_case_insensitive_semantics = true;

  // Liao 12/2/2010, Fortran does not require function prototypes
  if (!SageInterface::is_Fortran_language())
    insertRTLHeaders(file);
  if (!enable_accelerator)
    if (has_target_offload)
      insertAcceleratorInit(file);

  if (!target_outlined_source_function.empty()) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[target-outlined-source-anchor]: a new lowering "
            "run retained %zu unconsumed source-function anchors\n",
            target_outlined_source_function.size());
    ROSE_ABORT();
  }
  target_outlined_function_list = new std::vector<SgFunctionDeclaration *>();

  Rose_STL_Container<SgNode *> omp_nodes;
  do {
    omp_nodes.clear();
    // Fix the parent-children relationship between UPIR nodes
    OmpSupport::createOmpStatementTree(file);
    if (cpu_outlined_file != NULL) {
      OmpSupport::createOmpStatementTree(cpu_outlined_file);
    }
    clearOpenMPClauseOriginalExpressionTrees(file);
    if (cpu_outlined_file != NULL) {
      clearOpenMPClauseOriginalExpressionTrees(cpu_outlined_file);
    }
    Rose_STL_Container<SgNode *>::iterator iter;
    // Collect all the OpenMP nodes
    Rose_STL_Container<SgNode *> nodeList =
        NodeQuery::querySubTree(file, V_SgOmpExecStatement);
    nodeList = mergeSgNodeList(
        nodeList, NodeQuery::querySubTree(file, V_SgOmpThreadprivateStatement));
    nodeList = mergeSgNodeList(
        nodeList, NodeQuery::querySubTree(file, V_SgOmpAllocateStatement));
    nodeList = mergeSgNodeList(
        nodeList, NodeQuery::querySubTree(file, V_SgOmpRequiresStatement));
    nodeList = mergeSgNodeList(
        nodeList, NodeQuery::querySubTree(file, V_SgOmpTaskwaitStatement));
    if (cpu_outlined_file != NULL) {
      nodeList = mergeSgNodeList(
          nodeList,
          NodeQuery::querySubTree(cpu_outlined_file, V_SgOmpExecStatement));
      nodeList = mergeSgNodeList(
          nodeList, NodeQuery::querySubTree(cpu_outlined_file,
                                            V_SgOmpThreadprivateStatement));
      nodeList = mergeSgNodeList(
          nodeList,
          NodeQuery::querySubTree(cpu_outlined_file, V_SgOmpAllocateStatement));
      nodeList = mergeSgNodeList(
          nodeList,
          NodeQuery::querySubTree(cpu_outlined_file, V_SgOmpRequiresStatement));
      nodeList = mergeSgNodeList(
          nodeList,
          NodeQuery::querySubTree(cpu_outlined_file, V_SgOmpTaskwaitStatement));
    }
    Rose_STL_Container<SgNode *> visibleNodeList;
    std::unordered_set<SgNode *> seenVisibleNodes;
    for (iter = nodeList.begin(); iter != nodeList.end(); iter++) {
      if (isOmpContextSelectorMetadataDirective(*iter)) {
        continue;
      }
      SgLocatedNode *located = isSgLocatedNode(*iter);
      if (located != NULL && !located->isOutputInCodeGeneration()) {
        continue;
      }
      if (!seenVisibleNodes.insert(*iter).second) {
        continue;
      }
      visibleNodeList.push_back(*iter);
    }

    std::unordered_set<SgOmpExecStatement *> visible_exec_nodes;
    for (SgNode *visible : visibleNodeList) {
      if (SgOmpExecStatement *exec = isSgOmpExecStatement(visible)) {
        preserve_kmpc_source_context(exec);
        visible_exec_nodes.insert(exec);
      }
    }

    // Collect all OpenMP nodes whose recorded OpenMP parent exactly matches
    // their nearest structural OpenMP ancestor.
    for (iter = visibleNodeList.begin(); iter != visibleNodeList.end();
         iter++) {
      SgOmpExecStatement *omp_node = isSgOmpExecStatement(*iter);
      if (omp_node != NULL) {
        SgNode *ancestor = omp_node->get_parent();
        while (ancestor != nullptr && isSgOmpExecStatement(ancestor) == nullptr)
          ancestor = ancestor->get_parent();
        SgOmpExecStatement *structural_parent = isSgOmpExecStatement(ancestor);
        SgStatement *recorded_parent = omp_node->get_omp_parent();
        SgOmpExecStatement *omp_parent = isSgOmpExecStatement(recorded_parent);
        if ((recorded_parent != nullptr && omp_parent == nullptr) ||
            omp_parent != structural_parent) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[root-parentage]: node=%p "
                  "kind=%s recorded-parent=%p structural-parent=%p\n",
                  static_cast<void *>(omp_node), omp_node->sage_class_name(),
                  static_cast<void *>(recorded_parent),
                  static_cast<void *>(structural_parent));
          ROSE_ABORT();
        }
        if (omp_parent != nullptr &&
            visible_exec_nodes.find(omp_parent) == visible_exec_nodes.end()) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[root-parentage]: visible "
                  "node=%p kind=%s is owned by hidden OpenMP parent=%p "
                  "kind=%s\n",
                  static_cast<void *>(omp_node), omp_node->sage_class_name(),
                  static_cast<void *>(omp_parent),
                  omp_parent->sage_class_name());
          ROSE_ABORT();
        }
        if (omp_parent == NULL) {
          omp_nodes.push_back(omp_node);
        }
      } else if (isSgOmpRequiresStatement(*iter) != NULL) {
        omp_nodes.push_back(*iter);
      } else if (isSgOmpAllocateStatement(*iter) != NULL) {
        omp_nodes.push_back(*iter);
      } else if (isSgOmpThreadprivateStatement(*iter) != NULL) {
        omp_nodes.push_back(*iter);
      } else if (isSgOmpTaskwaitStatement(*iter) != NULL) {
        omp_nodes.push_back(*iter);
      }
    }

    // Every visible OpenMP subtree must expose at least one exact root. Picking
    // an arbitrary node here used to hide a transformation that retained stale
    // OpenMP-parent edges and made lowering order depend on traversal order.
    if (omp_nodes.empty() && !visibleNodeList.empty()) {
      SgStatement *first_visible = isSgStatement(visibleNodeList.front());
      fprintf(
          stderr,
          "REX_OMP_LOWERING_INVARIANT[root-parentage]: %zu visible "
          "OpenMP nodes have no exact root; first=%p kind=%s\n",
          visibleNodeList.size(), static_cast<void *>(visibleNodeList.front()),
          first_visible != NULL ? first_visible->sage_class_name()
                                : visibleNodeList.front()->sage_class_name());
      ROSE_ABORT();
    }

    sortOpenMpRootsForLowering(omp_nodes, file, cpu_outlined_file);

    for (iter = omp_nodes.begin(); iter != omp_nodes.end(); iter++) {
      SgStatement *node = isSgStatement(*iter);
      ROSE_ASSERT(node != NULL);

      // check if it is a variant
      bool isVariant = isSgOmpWhenClause(node->get_parent()) ||
                       isSgOmpDefaultClause(node->get_parent());
      if (isVariant) {
        MLOG_ERROR_CXX("ompLowering")
            << "Unexpected variant node in lowering pipeline; expected prior "
            << "metadirective transformation";
        ROSE_ABORT();
      }

      if (!isVariant)
        switch (node->variantT()) {
        case V_SgOmpParallelStatement: {
          // check if this parallel region is under "omp target"
          SgNode *parent = node->get_parent();
          ROSE_ASSERT(parent != NULL);
          if (isSgBasicBlock(parent)) // skip the padding block in between.
            parent = parent->get_parent();
          if (isSgOmpTargetStatement(parent))
            transOmpTargetParallel(node);
          /*
          if (isInOmpTargetOffloadingFunc(node))
            transOmpSpmdInTargetRegion(node);
          */
          else
            transOmpParallel(node);
          break;
        }
        case V_SgOmpSectionsStatement: {
          transOmpSections(node);
          break;
        }

        case V_SgOmpTaskStatement: {
          transOmpTask(node);
          break;
        }
        case V_SgOmpTaskloopStatement: {
          transOmpTaskloop(node);
          break;
        }
        case V_SgOmpForStatement:
        case V_SgOmpDoStatement: {
          /*Winnie, handle Collapse clause.*/
          if (hasClause(node, V_SgOmpCollapseClause))
            transOmpCollapse(node);

          if (isInOmpTargetOffloadingFunc(node))
            transOmpLoopInTargetRegion(node);
          else
            transOmpLoop(node);

          break;
        }
        case V_SgOmpBarrierStatement: {
          transOmpBarrier(node);
          break;
        }
        case V_SgOmpFlushStatement: {
          transOmpFlush(node);
          break;
        }
        case V_SgOmpAllocateStatement: {
          transOmpAllocate(node);
          break;
        }
        case V_SgOmpRequiresStatement: {
          transOmpRequires(node);
          break;
        }

        case V_SgOmpThreadprivateStatement: {
          transOmpThreadprivate(node);
          break;
        }
        case V_SgOmpTaskwaitStatement: {
          transOmpTaskwait(node);
          break;
        }
        case V_SgOmpTaskgroupStatement: {
          transOmpTaskgroup(node);
          break;
        }
        case V_SgOmpSingleStatement: {
          transOmpSingle(node);
          break;
        }
        case V_SgOmpMasterStatement: {
          transOmpMaster(node);
          break;
        }
        case V_SgOmpAtomicStatement: {
          transOmpAtomic(node);
          break;
        }
        case V_SgOmpOrderedStatement: {
          transOmpOrdered(node);
          break;
        }
        case V_SgOmpCriticalStatement: {
          transOmpCritical(node);
          break;
        }
        case V_SgOmpTargetStatement: {
          transOmpTarget(node);
          break;
        }
        case V_SgOmpTargetTeamsStatement: {
          transOmpTargetTeams(node);
          break;
        }
        case V_SgOmpTargetParallelStatement: {
          transOmpTargetParallel(node);
          break;
        }
        case V_SgOmpTargetDataStatement: {
          transOmpTargetData(node);
          break;
        }
        case V_SgOmpTargetUpdateStatement: {
          transOmpTargetUpdate(node);
          break;
        }
        case V_SgOmpTargetTeamsDistributeStatement: {
          transOmpTargetTeamsDistribute(node);
          break;
        }
        case V_SgOmpTargetParallelForStatement: {
          transOmpTargetParallelFor(node);
          break;
        }
        case V_SgOmpTargetTeamsDistributeParallelForStatement: {
          transOmpTargetTeamsDistributeParallelFor(node);
          break;
        }
        case V_SgOmpSimdStatement:
        case V_SgOmpUnrollStatement:
        case V_SgOmpTileStatement: {
          std::vector<SgStatement *> loop_trans_nodes;
          SgStatement *frontier = node;
          while (frontier != NULL) {
            bool is_omp_loop_transformation = false;
            switch (frontier->variantT()) {
            case V_SgOmpSimdStatement:
            case V_SgOmpUnrollStatement:
            case V_SgOmpTileStatement:
              loop_trans_nodes.push_back(frontier);
              is_omp_loop_transformation = true;
              break;
            default:;
            }
            if (is_omp_loop_transformation == false)
              break;

            SgOmpBodyStatement *transformation = isSgOmpBodyStatement(frontier);
            if (transformation == nullptr ||
                transformation->get_body() == nullptr) {
              fprintf(stderr,
                      "REX_OMP_LOWERING_INVARIANT[loop-transformation-chain]: "
                      "node=%p kind=%s has no exact body\n",
                      static_cast<void *>(frontier),
                      frontier->sage_class_name());
              ROSE_ABORT();
            }
            frontier = transformation->get_body();
            // skip basic blocks if any
            SgBasicBlock *body = isSgBasicBlock(frontier);
            while (body != NULL) {
              const SgStatementPtrList &bb_statements = body->get_statements();
              if (bb_statements.size() == 1) {
                if (bb_statements[0] == nullptr ||
                    bb_statements[0]->get_parent() != body) {
                  fprintf(stderr,
                          "REX_OMP_LOWERING_INVARIANT[loop-transformation-"
                          "chain]: block=%p has no exact only child\n",
                          static_cast<void *>(body));
                  ROSE_ABORT();
                }
                frontier = bb_statements[0];
                body = isSgBasicBlock(frontier);
              } else {
                fprintf(stderr,
                        "REX_OMP_LOWERING_INVARIANT[loop-transformation-"
                        "chain]: block=%p contains %zu statements instead "
                        "of one exact associated loop path\n",
                        static_cast<void *>(body), bb_statements.size());
                ROSE_ABORT();
              }
            }
          }
          for (auto i = loop_trans_nodes.rbegin(); i != loop_trans_nodes.rend();
               ++i) {
            switch ((*i)->variantT()) {
            case V_SgOmpSimdStatement:
              if (hasClause(*i, V_SgOmpCollapseClause))
                transOmpCollapse(*i);
              transOmpSimd(*i);
              break;
            case V_SgOmpUnrollStatement:
              transOmpUnroll(*i);
              break;
            case V_SgOmpTileStatement:
              transOmpTile(*i);
              break;
            default:
              fprintf(stderr,
                      "REX_OMP_LOWERING_INVARIANT[loop-transformation-chain]: "
                      "unsupported collected node=%p kind=%s\n",
                      static_cast<void *>(*i), (*i)->sage_class_name());
              ROSE_ABORT();
            }
          }
          break;
        }
        default: {
          MLOG_ERROR_CXX("ompLowering")
              << "Unexpected OpenMP construct in lowering pass: "
              << node->sage_class_name();
          ROSE_ABORT();
        }
        } // switch
    }
  } while (omp_nodes.size() != 0);

  if (file->get_Fortran_only()) {
    normalize_fortran_if_statements(file);
    Rose_STL_Container<SgNode *> scopes =
        NodeQuery::querySubTree(file, V_SgScopeStatement);
    for (Rose_STL_Container<SgNode *>::const_iterator it = scopes.begin();
         it != scopes.end(); ++it) {
      SgScopeStatement *scope = isSgScopeStatement(*it);
      ROSE_ASSERT(scope != NULL);
      if (!scope->isCaseInsensitive() && scope->symbol_table_size() == 0)
        scope->setCaseInsensitive(true);
    }
  }

  // post processing
  post_processing(file);
  clearClauseVariableRenamingRecord();
  SageBuilder::symbol_table_case_insensitive_semantics = saved_case_insensitive;
}

} // namespace OmpSupport

// global_tid is required as a parameter in many kmpc function calls
// we always use the function "__kmpc_global_thread_num" to get the global_tid.
// each OpenMP statement has such an id with unique name
// "__global_tid_<enclosing function name>_<original statement line number>_<tid
// index>"
static std::string sanitize_identifier_component(const std::string &name) {
  std::string result;
  result.reserve(name.size());
  for (char c : name) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == '_')
      result.push_back(c);
    else
      result.push_back('_');
  }

  if (result.empty())
    return result;

  if (std::isdigit(static_cast<unsigned char>(result[0])))
    result.insert(result.begin(), '_');

  return result;
}

static KmpcGlobalTidSourceContext
require_kmpc_global_tid_source_context(SgNode *target) {
  if (target == nullptr || target->get_parent() == nullptr) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-source]: runtime "
                 "thread identity requires one attached source directive\n";
    ROSE_ABORT();
  }
  const Sg_File_Info *info = target->get_startOfConstruct();
  SgFunctionDefinition *enclosing_definition =
      getEnclosingFunctionDefinition(target);
  SgFunctionDeclaration *enclosing_function =
      enclosing_definition != nullptr ? enclosing_definition->get_declaration()
                                      : nullptr;
  if (AstAttribute *raw =
          target->getAttribute(kKmpcSourceContextAttributeName)) {
    KmpcSourceContextAttribute *preserved =
        dynamic_cast<KmpcSourceContextAttribute *>(raw);
    if (preserved == nullptr || enclosing_function == nullptr ||
        preserved->source().source_physical_file_id < 0 ||
        preserved->source().source_line < 1 ||
        preserved->source().enclosing_function_name.empty()) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-source-context]: "
                   "directive has incomplete typed runtime source context\n";
      ROSE_ABORT();
    }
    if (!preserved->copied()) {
      const bool exact_source_position =
          info != nullptr &&
          info->get_physical_file_id() ==
              preserved->source().source_physical_file_id &&
          info->get_physical_line() == preserved->source().source_line;
      const bool direct_context_matches =
          preserved->origin() == KmpcSourceContextOrigin::source_directive
              ? exact_source_position
              : preserved->source().enclosing_function_name ==
                    enclosing_function->get_name().getString();
      if (!direct_context_matches) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-source-context]: "
                     "direct runtime source context disagrees with its exact "
                     "directive owner\n";
        ROSE_ABORT();
      }
    }
    return preserved->source();
  }
  if (info == nullptr || info->get_physical_file_id() < 0 ||
      info->get_physical_line() < 1) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-source]: attached "
                 "directive has no exact source line\n";
    ROSE_ABORT();
  }
  if (enclosing_function == nullptr) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-source]: attached "
                 "directive has no exact enclosing function declaration\n";
    ROSE_ABORT();
  }
  const std::string source_function_name =
      enclosing_function->get_name().getString();
  if (source_function_name.empty()) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-source]: enclosing "
                 "function declaration has no semantic name\n";
    ROSE_ABORT();
  }
  KmpcGlobalTidSourceContext context{source_function_name,
                                     info->get_physical_file_id(),
                                     info->get_physical_line()};
  if (sanitize_identifier_component(context.enclosing_function_name).empty()) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-source]: enclosing "
                 "function name cannot form a runtime identifier\n";
    ROSE_ABORT();
  }
  return context;
}

static SgVariableDeclaration *
get_kmpc_global_tid(const KmpcGlobalTidSourceContext &context,
                    SgScopeStatement *scope, SgStatement **init_stmt) {
  if (scope == nullptr || context.source_physical_file_id < 0 ||
      context.source_line < 1 || context.enclosing_function_name.empty()) {
    std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-context]: runtime "
                 "thread identity has incomplete captured source context\n";
    ROSE_ABORT();
  }
  const std::string enclosing_function_name =
      sanitize_identifier_component(context.enclosing_function_name);
  std::stringstream statement_line_number;
  statement_line_number << context.source_line;
  std::stringstream kmpc_global_tid_number;
  kmpc_global_tid_number << kmpc_global_tid_counter;
  kmpc_global_tid_counter += 1;
  const std::string tid_prefix = SageInterface::is_Fortran_language()
                                     ? "rex_global_tid_"
                                     : "__global_tid_";
  std::string kmpc_tid_name = tid_prefix + enclosing_function_name + "_" +
                              statement_line_number.str() + "_" +
                              kmpc_global_tid_number.str();
  SgScopeStatement *declaration_scope = scope;
  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
    SgBasicBlock *func_body =
        func_def != nullptr ? func_def->get_body() : nullptr;
    if (func_body == nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[kmpc-tid-scope]: Fortran "
                   "runtime thread identity has no exact procedure body\n";
      ROSE_ABORT();
    }
    declaration_scope = func_body;
    ensure_fortran_variable_declaration(
        func_body,
        SgName(getKmpcRuntimeFunctionName("__kmpc_global_thread_num")),
        buildKmpcInt32Type());
  }
  SgType *tid_type = SageInterface::is_Fortran_language()
                         ? static_cast<SgType *>(buildKmpcInt32Type())
                         : static_cast<SgType *>(buildIntType());
  SgExpression *get_thread_global_tid = buildFunctionCallExp(
      getKmpcRuntimeFunctionName("__kmpc_global_thread_num"), tid_type,
      buildExprListExp(buildIntVal(0)), scope);
  SgVariableDeclaration *kmpc_tid_declaration = NULL;
  if (SageInterface::is_Fortran_language()) {
    if (init_stmt == NULL) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[kmpc-tid-initializer]: Fortran "
              "thread identity requires an explicit initialization owner\n");
      ROSE_ABORT();
    }
    kmpc_tid_declaration = buildVariableDeclaration(
        SgName(kmpc_tid_name), tid_type, NULL, declaration_scope);
    *init_stmt = buildAssignStatement(
        buildVarRefExp(getFirstVariable(*kmpc_tid_declaration).get_name(),
                       scope),
        get_thread_global_tid);
  } else {
    kmpc_tid_declaration = buildVariableDeclaration(
        SgName(kmpc_tid_name), tid_type,
        buildAssignInitializer(get_thread_global_tid, tid_type), scope);
    if (init_stmt != NULL)
      *init_stmt = NULL;
  }

  return kmpc_tid_declaration;
}

static bool has_fortran_variable_declaration(SgBasicBlock *body,
                                             const SgName &name) {
  ROSE_ASSERT(body != NULL);
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgVariableDeclaration *decl = isSgVariableDeclaration(*it);
    if (decl == NULL)
      continue;
    const SgInitializedNamePtrList &vars = decl->get_variables();
    for (SgInitializedNamePtrList::const_iterator vit = vars.begin();
         vit != vars.end(); ++vit) {
      if ((*vit)->get_name() == name)
        return true;
    }
  }
  return false;
}

static SgProcedureHeaderStatement *
find_fortran_procedure_declaration(SgBasicBlock *body, const SgName &name) {
  ROSE_ASSERT(body != NULL);
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgProcedureHeaderStatement *proc = isSgProcedureHeaderStatement(*it);
    if (proc != NULL && proc->get_name() == name)
      return proc;
  }
  return NULL;
}

static bool is_fortran_data_specification_statement(const SgStatement *stmt) {
  // Some ROSE trees represent DATA as dedicated nodes, others attach DATA
  // groups under SgAttributeSpecificationStatement.
  if (std::string(stmt->sage_class_name()) == "SgDataStatement")
    return true;

  const SgAttributeSpecificationStatement *attr_spec =
      isSgAttributeSpecificationStatement(stmt);
  if (attr_spec == NULL)
    return false;

  if (attr_spec->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_dataStatement)
    return true;

  return !attr_spec->get_data_statement_group_list().empty();
}

static SgStatement *
find_fortran_specification_insertion_anchor(SgBasicBlock *body) {
  ROSE_ASSERT(body != NULL);
  Sg_File_Info *body_info = body->get_file_info();
  if (body_info == NULL || body_info->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[fortran-specification-owner]: "
            "procedure body=%p has no exact physical output owner\n",
            static_cast<void *>(body));
    ROSE_ABORT();
  }
  const int body_physical_file_id = body_info->get_physical_file_id();
  SgStatement *anchor = NULL;
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgStatement *stmt = *it;
    ROSE_ASSERT(stmt != NULL);
    Sg_File_Info *stmt_info = stmt->get_file_info();
    if (stmt_info == NULL || stmt_info->get_physical_file_id() < 0) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-specification-owner]: "
              "procedure body=%p contains statement=%p/%s without exact "
              "physical ownership\n",
              static_cast<void *>(body), static_cast<void *>(stmt),
              stmt->class_name().c_str());
      ROSE_ABORT();
    }

    // Semantic declarations imported from an INCLUDE file are structurally
    // visible in the host body but own a different physical source surface.
    // They cannot be lexical insertion anchors for generated host
    // declarations: insertStatementAfter() publishes against the anchor's
    // exact physical owner.  Ignore them and classify only statements that
    // belong to the host procedure's output file.
    if (stmt_info->get_physical_file_id() != body_physical_file_id)
      continue;

    // Declarations and COMMON must remain in the specification part and before
    // DATA statements or internal subprogram definitions.
    if (is_fortran_data_specification_statement(stmt))
      break;
    if (SgProcedureHeaderStatement *proc = isSgProcedureHeaderStatement(stmt)) {
      if (proc->get_definition() != NULL)
        break;
    }
    if (!isSgDeclarationStatement(stmt))
      break;

    anchor = stmt;
  }
  return anchor;
}

static void insert_fortran_statement_in_specification_part(SgStatement *stmt,
                                                           SgBasicBlock *body) {
  ROSE_ASSERT(stmt != NULL);
  ROSE_ASSERT(body != NULL);

  SgStatement *anchor = find_fortran_specification_insertion_anchor(body);
  if (anchor != NULL)
    insertStatementAfter(anchor, stmt);
  else
    prependStatement(stmt, body);

  Sg_File_Info *stmt_info = stmt->get_file_info();
  if (stmt_info == NULL || stmt_info->get_physical_file_id() !=
                               body->get_file_info()->get_physical_file_id()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[fortran-specification-owner]: "
            "generated specification statement=%p/%s was not published to "
            "its host procedure's exact physical output owner\n",
            static_cast<void *>(stmt), stmt->class_name().c_str());
    ROSE_ABORT();
  }

  if (stmt->isTransformation()) {
    bool reached_procedure = false;
    for (SgNode *current = body; current != nullptr;
         current = current->get_parent()) {
      current->set_containsTransformation(true);
      if (SgLocatedNode *located = isSgLocatedNode(current)) {
        located->set_containsTransformationToSurroundingWhitespace(true);
      }
      if (isSgFunctionDeclaration(current) != nullptr) {
        reached_procedure = true;
        break;
      }
    }
    if (!reached_procedure) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[fortran-specification-owner]: "
              "generated specification statement=%p has no exact enclosing "
              "procedure declaration\n",
              static_cast<void *>(stmt));
      ROSE_ABORT();
    }
  }
}

static void ensure_fortran_variable_declaration(SgBasicBlock *body,
                                                const SgName &name,
                                                SgType *type) {
  ROSE_ASSERT(body != NULL);
  ROSE_ASSERT(type != NULL);
  if (has_fortran_variable_declaration(body, name))
    return;

  SgProcedureHeaderStatement *proc =
      find_fortran_procedure_declaration(body, name);
  if (proc != NULL) {
    if (proc->get_subprogram_kind() ==
        SgProcedureHeaderStatement::e_function_subprogram_kind)
      return;

    fprintf(stderr,
            "REX OpenMP lowering: conflicting Fortran declaration for '%s' "
            "(existing subroutine declaration)\n",
            name.getString().c_str());
    ROSE_ABORT();
  }

  SgVariableDeclaration *decl =
      buildVariableDeclaration(name, type, NULL, body);
  insert_fortran_statement_in_specification_part(decl, body);
}

static void
insert_fortran_declaration_into_procedure(SgVariableDeclaration *decl,
                                          SgScopeStatement *scope) {
  ROSE_ASSERT(decl != NULL);
  ROSE_ASSERT(scope != NULL);
  SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(scope);
  ROSE_ASSERT(func_def != NULL);
  SgBasicBlock *func_body = func_def->get_body();
  ROSE_ASSERT(func_body != NULL);

  insert_fortran_statement_in_specification_part(decl, func_body);
}

static void
materialize_fortran_outlined_function_result_declarations(SgBasicBlock *body) {
  ROSE_ASSERT(body != NULL);

  std::map<std::string, std::pair<SgName, SgType *>> declarations;
  Rose_STL_Container<SgNode *> references =
      NodeQuery::querySubTree(body, V_SgFunctionRefExp);
  for (SgNode *node : references) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    ROSE_ASSERT(reference != NULL);
    SgFunctionSymbol *symbol = reference->get_symbol();
    ROSE_ASSERT(symbol != NULL);
    SgFunctionDeclaration *declaration = symbol->get_declaration();
    ROSE_ASSERT(declaration != NULL);

    SgFunctionDeclaration *defining_declaration =
        isSgFunctionDeclaration(declaration->get_definingDeclaration());
    if (defining_declaration == NULL ||
        defining_declaration->get_definition() == NULL) {
      continue;
    }

    SgFunctionType *function_type = isSgFunctionType(symbol->get_type());
    ROSE_ASSERT(function_type != NULL);
    SgType *result_type = function_type->get_return_type();
    ROSE_ASSERT(result_type != NULL);
    if (isSgTypeVoid(result_type) != NULL) {
      continue;
    }

    const SgName name = symbol->get_name();
    const std::string key = StringUtility::convertToLowerCase(name.getString());
    auto inserted =
        declarations.emplace(key, std::make_pair(name, result_type));
    if (!inserted.second && inserted.first->second.second != result_type) {
      fprintf(stderr,
              "REX OpenMP lowering: conflicting result types for outlined "
              "Fortran function reference '%s'\n",
              name.getString().c_str());
      ROSE_ABORT();
    }
  }

  for (const auto &entry : declarations) {
    const SgName &name = entry.second.first;
    SgType *result_type = entry.second.second;
    if (has_fortran_variable_declaration(body, name)) {
      fprintf(stderr,
              "REX OpenMP lowering: outlined Fortran function result '%s' "
              "conflicts with a local declaration\n",
              name.getString().c_str());
      ROSE_ABORT();
    }

    SgVariableDeclaration *external_result =
        buildVariableDeclaration(name, result_type, NULL, body);
    ROSE_ASSERT(external_result != NULL);
    external_result->get_declarationModifier()
        .get_storageModifier()
        .setExtern();
    insert_fortran_statement_in_specification_part(external_result, body);
  }
}

static void rebind_fortran_outlined_function_references(SgBasicBlock *body) {
  ROSE_ASSERT(body != nullptr);
  const Rose_STL_Container<SgNode *> reference_nodes =
      NodeQuery::querySubTree(body, V_SgFunctionRefExp);
  for (SgNode *node : reference_nodes) {
    SgFunctionRefExp *reference = isSgFunctionRefExp(node);
    SgFunctionSymbol *source_symbol =
        reference != nullptr ? reference->get_symbol() : nullptr;
    SgFunctionType *function_type =
        source_symbol != nullptr ? isSgFunctionType(source_symbol->get_type())
                                 : nullptr;
    if (reference == nullptr || source_symbol == nullptr ||
        source_symbol->get_declaration() == nullptr ||
        function_type == nullptr) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[outlined-function-reference]: "
              "outlined reference=%p has no exact typed source symbol\n",
              static_cast<void *>(reference));
      ROSE_ABORT();
    }

    SgFunctionSymbol *visible = lookupFunctionSymbolInParentScopes(
        source_symbol->get_name(), function_type, body);
    if (visible == source_symbol) {
      continue;
    }

    SgFunctionRefExp *replacement =
        buildFunctionRefExp(source_symbol->get_name(), function_type, body);
    SgFunctionSymbol *replacement_symbol =
        replacement != nullptr ? replacement->get_symbol() : nullptr;
    if (replacement == nullptr || replacement_symbol == nullptr ||
        replacement_symbol == source_symbol ||
        lookupFunctionSymbolInParentScopes(source_symbol->get_name(),
                                           function_type,
                                           body) != replacement_symbol) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[outlined-function-reference]: "
              "reference=%p name=%s could not publish one exact symbol in "
              "the outlined procedure scope\n",
              static_cast<void *>(reference), source_symbol->get_name().str());
      ROSE_ABORT();
    }
    replaceExpression(reference, replacement);
  }
}

static void
normalize_fortran_external_subroutine_declarations(SgBasicBlock *body) {
  ROSE_ASSERT(body != NULL);
  std::vector<SgProcedureHeaderStatement *> declarations;
  const SgStatementPtrList &stmts = body->get_statements();
  for (SgStatementPtrList::const_iterator it = stmts.begin(); it != stmts.end();
       ++it) {
    SgProcedureHeaderStatement *proc = isSgProcedureHeaderStatement(*it);
    if (proc == NULL)
      continue;
    if (proc->get_definition() != NULL)
      continue;
    if (proc->get_subprogram_kind() !=
        SgProcedureHeaderStatement::e_subroutine_subprogram_kind)
      continue;
    declarations.push_back(proc);
  }

  for (std::vector<SgProcedureHeaderStatement *>::const_iterator it =
           declarations.begin();
       it != declarations.end(); ++it) {
    SgProcedureHeaderStatement *proc = *it;
    // These nondefining subroutine declarations are outlining artifacts.
    // Keeping them changes procedure binding semantics (e.g., forcing CALL
    // abort to resolve as abort_) and can also emit invalid declaration forms
    // in fixed-form Fortran. Drop them and preserve the original unit's
    // implicit procedure resolution.
    SageInterface::removeStatement(proc, true);
  }
}

static void normalize_fortran_if_statements(SgSourceFile *file) {
  ROSE_ASSERT(file != NULL);
  Rose_STL_Container<SgNode *> if_nodes =
      NodeQuery::querySubTree(file, V_SgIfStmt);
  for (Rose_STL_Container<SgNode *>::const_iterator it = if_nodes.begin();
       it != if_nodes.end(); ++it) {
    SgIfStmt *if_stmt = isSgIfStmt(*it);
    ROSE_ASSERT(if_stmt != NULL);

    SgStatement *true_body = if_stmt->get_true_body();
    ROSE_ASSERT(true_body != NULL);
    if (!isSgBasicBlock(true_body)) {
      if_stmt->set_true_body(nullptr);
      true_body->set_parent(nullptr);
      SgBasicBlock *wrapped_true = buildBasicBlock(true_body);
      SageInterface::publishGeneratedSubtreeOutputOwner(wrapped_true, if_stmt);
      if_stmt->set_true_body(wrapped_true);
      wrapped_true->set_parent(if_stmt);
    }

    SgStatement *false_body = if_stmt->get_false_body();
    if (false_body != NULL && !isSgBasicBlock(false_body) &&
        !isSgIfStmt(false_body)) {
      if_stmt->set_false_body(nullptr);
      false_body->set_parent(nullptr);
      SgBasicBlock *wrapped_false = buildBasicBlock(false_body);
      SageInterface::publishGeneratedSubtreeOutputOwner(wrapped_false, if_stmt);
      if_stmt->set_false_body(wrapped_false);
      wrapped_false->set_parent(if_stmt);
    }

    if_stmt->set_use_then_keyword(true);
    if (!isSgIfStmt(if_stmt->get_false_body()))
      if_stmt->set_has_end_statement(true);
  }
}

// insert a parameter to the outlined function
// it doesn't affect the forward declaration but the definition itself
// please use it before inserting the forward declaration
static void insert_function_parameter(std::string name, SgType *parameter_type,
                                      SgFunctionDeclaration *function,
                                      bool to_append) {
  if (parameter_type == NULL || function == NULL ||
      function->get_parameterList() == NULL || function->get_type() == NULL) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[runtime-parameter-insertion]: "
            "parameter type, function, semantic parameter list, and function "
            "type are required\n");
    ROSE_ABORT();
  }

  // prepare the parameter
  SgName parameter_name(name);
  SgFunctionParameterList *params = function->get_parameterList();
  SgFunctionParameterList *syntax_params = function->get_parameterList_syntax();
  const bool has_distinct_syntax_params =
      syntax_params != NULL && syntax_params != params;
  if (has_distinct_syntax_params) {
    SgFunctionType *syntax_type = function->get_type_syntax();
    SgFunctionParameterTypeList *syntax_argument_types =
        syntax_type != NULL ? syntax_type->get_argument_list() : NULL;
    if (syntax_params->get_parent() != function ||
        syntax_params->get_args().size() != params->get_args().size() ||
        syntax_type == NULL || !function->get_type_syntax_is_available() ||
        syntax_type->get_parent() != function ||
        syntax_argument_types == NULL ||
        syntax_argument_types->get_parent() != syntax_type ||
        syntax_argument_types->get_arguments().size() !=
            syntax_params->get_args().size()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[runtime-parameter-insertion]: "
              "function has inconsistent semantic and exact-syntax parameter "
              "surfaces before inserting '%s'\n",
              name.c_str());
      ROSE_ABORT();
    }
  }
  SgInitializedName *parameter =
      SageBuilder::buildInitializedName(parameter_name, parameter_type);
  setOneSourcePositionForTransformation(parameter);
  SgInitializedName *syntax_parameter = NULL;
  if (has_distinct_syntax_params) {
    syntax_parameter =
        SageBuilder::buildInitializedName(parameter_name, parameter_type);
    setOneSourcePositionForTransformation(syntax_parameter);
  }

  // insert the parameter at the end or the beginning
  if (to_append) {
    appendArg(params, parameter);
    if (syntax_parameter != NULL)
      appendArg(syntax_params, syntax_parameter);
  } else {
    prependArg(params, parameter);
    if (syntax_parameter != NULL)
      prependArg(syntax_params, syntax_parameter);
  }

  if (SageInterface::is_Fortran_language()) {
    SgFunctionDefinition *func_def = function->get_definition();
    ROSE_ASSERT(func_def != NULL);
    ensure_fortran_variable_declaration(func_def->get_body(), parameter_name,
                                        parameter_type);
  }

  // update the function metadata
  SgType *stale_func_type = function->get_type();
  function->set_type(buildFunctionType(
      function->get_type()->get_return_type(),
      buildFunctionParameterTypeList(function->get_parameterList())));
  SgFunctionDeclaration *non_def_func =
      isSgFunctionDeclaration(function->get_firstNondefiningDeclaration());
  ROSE_ASSERT(non_def_func != NULL);
  ROSE_ASSERT(stale_func_type == non_def_func->get_type());
  non_def_func->set_type(function->get_type());
  if (has_distinct_syntax_params) {
    rebuildOutlinedFunctionSyntaxType(function, syntax_params);
    if (syntax_params->get_args().size() != params->get_args().size() ||
        syntax_parameter->get_parent() != syntax_params ||
        parameter->get_parent() != params) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[runtime-parameter-insertion]: "
              "semantic and exact-syntax parameter surfaces diverged after "
              "inserting '%s'\n",
              name.c_str());
      ROSE_ABORT();
    }
  }
}

static SgFunctionDeclaration *
move_outlined_function(SgFunctionDeclaration *outlined_func,
                       SgSourceFile *new_file) {
  if (outlined_func == nullptr || new_file == nullptr) {
    failOutlinedClauseRecord("requires exact source and destination roots",
                             outlined_func, new_file);
  }

  // prepare the required information of original file
  SgGlobal *original_scope = getGlobalScope(outlined_func);

  const auto source_anchor_entry =
      target_outlined_source_function.find(outlined_func);
  SgFunctionDeclaration *source_function =
      source_anchor_entry != target_outlined_source_function.end()
          ? source_anchor_entry->second
          : NULL;

  // prepare the required information of new file
  SgGlobal *new_scope = new_file->get_globalScope();
  if (original_scope == nullptr || new_scope == nullptr) {
    failOutlinedClauseRecord("requires exact source and destination scopes",
                             original_scope, new_scope);
  }
  if (source_function == NULL) {
    failOutlinedClauseRecord("has no recorded source-function anchor",
                             outlined_func, nullptr);
  }
  if (!SgNode::isLiveNode(source_function)) {
    failOutlinedClauseRecord("retained a deleted source-function anchor",
                             outlined_func, source_function);
  }
  if (getGlobalScope(source_function) != original_scope) {
    failOutlinedClauseRecord("source-function anchor changed translation units",
                             outlined_func, source_function);
  }

  SgStatement *source_insertion_anchor = source_function;
  std::unordered_set<SgNode *> source_anchor_chain;
  while (source_insertion_anchor != NULL &&
         source_insertion_anchor->get_parent() != original_scope) {
    if (!source_anchor_chain.insert(source_insertion_anchor).second) {
      failOutlinedClauseRecord("source-function anchor has a parent cycle",
                               outlined_func, source_function);
    }
    source_insertion_anchor =
        isSgStatement(source_insertion_anchor->get_parent());
  }
  if (source_insertion_anchor == NULL ||
      source_insertion_anchor->get_parent() != original_scope ||
      !original_scope->statementExistsInScope(source_insertion_anchor)) {
    failOutlinedClauseRecord("has no exact direct source-function anchor",
                             outlined_func, source_function);
  }

  // Copy and fully validate all identities needed by clause-variable records
  // while both source and destination roots are still detached from mutation.
  SgCopyHelp::copiedNodeMapType identityMap;
  SgFunctionDeclaration *new_outlined_function = isSgFunctionDeclaration(
      SageInterface::deepCopyNodeWithIdentityMap(outlined_func, identityMap));
  if (new_outlined_function == NULL) {
    failOutlinedClauseRecord("copied to a non-function root", outlined_func,
                             new_outlined_function);
  }
  SgFunctionDeclaration *obsolete_copied_canonical = isSgFunctionDeclaration(
      new_outlined_function->get_firstNondefiningDeclaration());
  if (obsolete_copied_canonical == NULL ||
      obsolete_copied_canonical == new_outlined_function ||
      obsolete_copied_canonical->get_firstNondefiningDeclaration() !=
          obsolete_copied_canonical ||
      obsolete_copied_canonical->get_definingDeclaration() !=
          new_outlined_function) {
    failOutlinedClauseRecord(
        "deep copy has no exact replaceable detached canonical prototype",
        new_outlined_function, obsolete_copied_canonical);
  }
  std::vector<PreparedClauseVariableRenaming> preparedClauseMappings =
      prepareClauseVariableRenamingRecords(outlined_func, new_outlined_function,
                                           identityMap);

  auto translateCrossOutputType =
      [new_file](SgType *source_type, SgScopeStatement *target_scope,
                 const SgNode *source_owner, const SgNode *target_owner,
                 const char *role) -> SgType * {
    SgSourceFile *target_file =
        target_scope != NULL
            ? SageInterface::getEnclosingSourceFile(target_scope)
            : NULL;
    if (source_type == NULL || target_scope == NULL || source_owner == NULL ||
        target_owner == NULL || role == NULL || target_file != new_file) {
      fprintf(
          stderr,
          "REX_OMP_LOWERING_INVARIANT[cross-output-type]: role=%s "
          "source-owner=%p/%s source-type=%p target-owner=%p/%s "
          "target-parent=%p/%s target-scope=%p/%s scope-parent=%p/%s "
          "target-file=%p expected-file=%p has no exact translation context\n",
          role != NULL ? role : "<null>",
          static_cast<const void *>(source_owner),
          source_owner != NULL ? source_owner->class_name().c_str() : "<null>",
          static_cast<void *>(source_type),
          static_cast<const void *>(target_owner),
          target_owner != NULL ? target_owner->class_name().c_str() : "<null>",
          static_cast<const void *>(
              target_owner != NULL ? target_owner->get_parent() : NULL),
          target_owner != NULL && target_owner->get_parent() != NULL
              ? target_owner->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(target_scope),
          target_scope != NULL ? target_scope->class_name().c_str() : "<null>",
          static_cast<void *>(target_scope != NULL ? target_scope->get_parent()
                                                   : NULL),
          target_scope != NULL && target_scope->get_parent() != NULL
              ? target_scope->get_parent()->class_name().c_str()
              : "<null>",
          static_cast<void *>(target_file), static_cast<void *>(new_file));
      ROSE_ABORT();
    }
    SgType *target_type =
        SageBuilder::getTargetFileType(source_type, target_scope);
    if (target_type == NULL) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-type]: role=%s "
              "source-owner=%p/%s source-type=%p/%s target-owner=%p/%s "
              "target-scope=%p produced no exact target-file type\n",
              role, static_cast<const void *>(source_owner),
              source_owner->class_name().c_str(),
              static_cast<void *>(source_type),
              source_type->class_name().c_str(),
              static_cast<const void *>(target_owner),
              target_owner->class_name().c_str(),
              static_cast<void *>(target_scope));
      ROSE_ABORT();
    }
    return target_type;
  };

  // Build a new prototype in the outlined file and relink the copied
  // definition to it. A deep-copied defining declaration otherwise retains a
  // detached first-nondefining declaration that is not attached to any file.
  SgFunctionParameterList *new_prototype_params =
      SageBuilder::buildSemanticFunctionParameterList(
          outlined_func->get_parameterList());
  auto relocateParameterTypes =
      [new_scope, &translateCrossOutputType](
          SgFunctionParameterList *source_parameters,
          SgFunctionParameterList *destination_parameters, const char *role) {
        if (source_parameters == NULL || destination_parameters == NULL ||
            source_parameters->get_args().size() !=
                destination_parameters->get_args().size()) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[cross-output-signature]: "
                  "role=%s source=%p destination=%p has no exact parameter "
                  "pairing\n",
                  role, static_cast<void *>(source_parameters),
                  static_cast<void *>(destination_parameters));
          ROSE_ABORT();
        }
        for (size_t index = 0; index < source_parameters->get_args().size();
             ++index) {
          SgInitializedName *source = source_parameters->get_args()[index];
          SgInitializedName *destination =
              destination_parameters->get_args()[index];
          SgType *target_type =
              source != NULL && destination != NULL
                  ? translateCrossOutputType(source->get_type(), new_scope,
                                             source, destination, role)
                  : NULL;
          if (source == NULL || destination == NULL || target_type == NULL ||
              source->get_name() != destination->get_name() ||
              destination->get_parent() != destination_parameters) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[cross-output-signature]: "
                    "role=%s index=%zu source=%p destination=%p type=%p has "
                    "no exact target-file identity\n",
                    role, index, static_cast<void *>(source),
                    static_cast<void *>(destination),
                    static_cast<void *>(target_type));
            ROSE_ABORT();
          }
          destination->set_type(target_type);
        }
      };
  relocateParameterTypes(outlined_func->get_parameterList(),
                         new_outlined_function->get_parameterList(),
                         "definition");
  relocateParameterTypes(outlined_func->get_parameterList(),
                         new_prototype_params, "canonical");
  SgType *target_return_type = translateCrossOutputType(
      outlined_func->get_type()->get_return_type(), new_scope, outlined_func,
      new_outlined_function, "return-type");
  SgFunctionDeclaration *new_first_nondefining =
      SageBuilder::buildNondefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::semanticAuxiliary(),
          outlined_func->get_name(), target_return_type, new_prototype_params,
          new_scope);
  ROSE_ASSERT(new_first_nondefining != NULL);
  new_first_nondefining->set_linkage(outlined_func->get_linkage());

  // The copied function was already published against the original generated
  // output surface.  Moving it to the outlined translation unit is one exact
  // physical-owner transfer: source-spelled descendants retain their source
  // identity, while every generated descendant must prove the old owner before
  // it is assigned the new one.
  SageInterface::publishGeneratedSubtreeOutputOwner(new_outlined_function,
                                                    outlined_func);
  SageInterface::relocateGeneratedSubtreePhysicalOutputOwner(
      new_outlined_function, outlined_func, new_scope);
  new_outlined_function->get_declarationModifier()
      .get_storageModifier()
      .setUnspecified();
  new_outlined_function->set_scope(new_scope);
  new_outlined_function->set_type(buildFunctionType(
      target_return_type, buildFunctionParameterTypeList(
                              new_outlined_function->get_parameterList())));
  if (new_first_nondefining->get_type() != new_outlined_function->get_type()) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[cross-output-signature]: "
            "canonical=%p type=%p definition=%p type=%p did not publish one "
            "exact target-file function type\n",
            static_cast<void *>(new_first_nondefining),
            static_cast<void *>(new_first_nondefining->get_type()),
            static_cast<void *>(new_outlined_function),
            static_cast<void *>(new_outlined_function->get_type()));
    ROSE_ABORT();
  }
  new_outlined_function->set_firstNondefiningDeclaration(new_first_nondefining);
  new_first_nondefining->set_definingDeclaration(new_outlined_function);
  appendStatement(new_outlined_function, new_scope);

  // Deep-copying a defining declaration also copies the old canonical
  // prototype through the declaration-family edge.  The target transaction
  // replaces that prototype above; retaining its detached copy in identityMap
  // makes its parameter scope look like target-file AST even though it has no
  // physical output owner.  Retire exactly that obsolete family subtree before
  // any target-file rebinding consumes the identity map.
  std::vector<const SgNode *> obsolete_identity_sources;
  SgAuxiliaryDeclarationList *obsolete_auxiliary =
      isSgAuxiliaryDeclarationList(obsolete_copied_canonical->get_parent());
  SgScopeStatement *obsolete_semantic_scope =
      obsolete_copied_canonical->get_scope();
  std::vector<SgSymbol *> obsolete_symbols;
  if (obsolete_semantic_scope != NULL &&
      obsolete_semantic_scope->get_symbol_table() != NULL &&
      obsolete_semantic_scope->get_symbol_table()->get_table() != NULL) {
    for (const auto &entry :
         *obsolete_semantic_scope->get_symbol_table()->get_table()) {
      if (entry.second != NULL &&
          entry.second->get_symbol_basis() == obsolete_copied_canonical) {
        obsolete_symbols.push_back(entry.second);
      }
    }
  }
  if (obsolete_auxiliary == NULL || obsolete_auxiliary->get_parent() != NULL ||
      obsolete_semantic_scope == NULL || !obsolete_symbols.empty() ||
      std::count(obsolete_auxiliary->get_declarations().begin(),
                 obsolete_auxiliary->get_declarations().end(),
                 obsolete_copied_canonical) != 1 ||
      obsolete_auxiliary->get_declarations().size() != 1) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[cross-output-canonical]: obsolete "
            "canonical=%p auxiliary=%p parent=%p scope=%p symbols=%zu is not "
            "one exact detached copied semantic family\n",
            static_cast<void *>(obsolete_copied_canonical),
            static_cast<void *>(obsolete_auxiliary),
            static_cast<void *>(obsolete_auxiliary != NULL
                                    ? obsolete_auxiliary->get_parent()
                                    : NULL),
            static_cast<void *>(obsolete_semantic_scope),
            obsolete_symbols.size());
    ROSE_ABORT();
  }
  auto belongsToObsoleteCanonical = [obsolete_copied_canonical](
                                        const SgNode *candidate) {
    std::unordered_set<const SgNode *> visited;
    for (const SgNode *current = candidate; current != NULL;
         current = current->get_parent()) {
      if (!visited.insert(current).second) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-canonical]: "
                "copied node=%p/%s has a parent cycle\n",
                static_cast<const void *>(candidate),
                candidate != NULL ? candidate->class_name().c_str() : "<null>");
        ROSE_ABORT();
      }
      if (current == obsolete_copied_canonical) {
        return true;
      }
    }
    return false;
  };
  for (const auto &mapped_node : identityMap) {
    if (belongsToObsoleteCanonical(mapped_node.second) ||
        mapped_node.second == obsolete_auxiliary) {
      obsolete_identity_sources.push_back(mapped_node.first);
    }
  }
  if (obsolete_identity_sources.empty() ||
      std::find(obsolete_identity_sources.begin(),
                obsolete_identity_sources.end(),
                outlined_func->get_firstNondefiningDeclaration()) ==
          obsolete_identity_sources.end()) {
    fprintf(
        stderr,
        "REX_OMP_LOWERING_INVARIANT[cross-output-canonical]: source=%p "
        "copied canonical=%p has no exact identity-map subtree\n",
        static_cast<void *>(outlined_func->get_firstNondefiningDeclaration()),
        static_cast<void *>(obsolete_copied_canonical));
    ROSE_ABORT();
  }
  for (const SgNode *source : obsolete_identity_sources) {
    if (identityMap.erase(source) != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-canonical]: "
              "obsolete source identity=%p was not removed exactly once\n",
              static_cast<const void *>(source));
      ROSE_ABORT();
    }
  }
  obsolete_auxiliary->get_declarations().clear();
  obsolete_copied_canonical->set_parent(NULL);
  if (!obsolete_auxiliary->get_declarations().empty() ||
      obsolete_copied_canonical->get_parent() != NULL) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[cross-output-canonical]: obsolete "
            "canonical=%p did not detach from isolated auxiliary=%p\n",
            static_cast<void *>(obsolete_copied_canonical),
            static_cast<void *>(obsolete_auxiliary));
    ROSE_ABORT();
  }
  delete obsolete_auxiliary;
  obsolete_copied_canonical->set_definingDeclaration(NULL);
  SageInterface::deleteAST(obsolete_copied_canonical,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  if (SgNode::isLiveNode(obsolete_copied_canonical)) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[cross-output-canonical]: obsolete "
            "copied canonical=%p remained live after exact replacement\n",
            static_cast<void *>(obsolete_copied_canonical));
    ROSE_ABORT();
  }

  auto buildTargetFunctionType =
      [new_scope, &translateCrossOutputType](
          SgFunctionType *source_type) -> SgFunctionType * {
    SgFunctionParameterTypeList *source_arguments =
        source_type != NULL ? source_type->get_argument_list() : NULL;
    if (source_type == NULL || isSgMemberFunctionType(source_type) != NULL ||
        source_type->get_return_type() == NULL || source_arguments == NULL ||
        source_arguments->get_parent() != source_type) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-type]: "
              "source=%p/%s arguments=%p has no exact free-function "
              "signature\n",
              static_cast<void *>(source_type),
              source_type != NULL ? source_type->class_name().c_str()
                                  : "<null>",
              static_cast<void *>(source_arguments));
      ROSE_ABORT();
    }

    SgType *target_return_type = translateCrossOutputType(
        source_type->get_return_type(), new_scope, source_type, new_scope,
        "function-reference-return");
    const bool source_has_ellipses = source_type->get_has_ellipses();
    const bool source_has_ellipsis_argument =
        !source_arguments->get_arguments().empty() &&
        isSgTypeEllipse(source_arguments->get_arguments().back()) != NULL;
    if (source_has_ellipses != source_has_ellipsis_argument) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-type]: "
              "source=%p ellipses=%d final-ellipsis-argument=%d does not "
              "publish one exact variadic signature\n",
              static_cast<void *>(source_type), source_has_ellipses ? 1 : 0,
              source_has_ellipsis_argument ? 1 : 0);
      ROSE_ABORT();
    }
    SgFunctionParameterTypeList *target_arguments =
        new SgFunctionParameterTypeList;
    if (target_arguments == NULL || target_arguments->get_parent() != NULL) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-type]: "
              "source=%p arguments=%p has no exact detached "
              "construction transaction\n",
              static_cast<void *>(source_type),
              static_cast<void *>(target_arguments));
      ROSE_ABORT();
    }
    for (SgType *source_argument : source_arguments->get_arguments()) {
      target_arguments->append_argument(translateCrossOutputType(
          source_argument, new_scope, source_type, target_arguments,
          "function-reference-parameter"));
    }
    SgFunctionType *target_type =
        SageBuilder::buildFunctionType(target_return_type, target_arguments);
    SgFunctionParameterTypeList *published_arguments =
        target_type != NULL ? target_type->get_argument_list() : NULL;
    bool exact_arguments = published_arguments != NULL &&
                           published_arguments->get_parent() == target_type &&
                           published_arguments->get_arguments().size() ==
                               target_arguments->get_arguments().size();
    if (exact_arguments) {
      for (size_t index = 0; index < target_arguments->get_arguments().size();
           ++index) {
        if (published_arguments->get_arguments()[index] !=
            target_arguments->get_arguments()[index]) {
          exact_arguments = false;
          break;
        }
      }
    }
    if (target_type == NULL ||
        target_type->get_return_type() != target_return_type ||
        target_type->get_has_ellipses() != source_type->get_has_ellipses() ||
        !exact_arguments) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-type]: "
              "source=%p target=%p return=%p expected-return=%p "
              "exact-arguments=%d did not preserve one exact signature\n",
              static_cast<void *>(source_type),
              static_cast<void *>(target_type),
              static_cast<void *>(
                  target_type != NULL ? target_type->get_return_type() : NULL),
              static_cast<void *>(target_return_type), exact_arguments ? 1 : 0);
      ROSE_ABORT();
    }

    if (published_arguments != target_arguments) {
      if (target_arguments->get_parent() != NULL) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-type]: "
                "source=%p unconsumed arguments=%p acquired owner=%p\n",
                static_cast<void *>(source_type),
                static_cast<void *>(target_arguments),
                static_cast<void *>(target_arguments->get_parent()));
        ROSE_ABORT();
      }
      SageInterface::deleteAST(
          target_arguments,
          SageInterface::DeleteAstMode::kSkipExternalReferences);
    }
    return target_type;
  };

  std::map<SgFunctionDeclaration *, SgFunctionSymbol *>
      published_target_external_symbols;
  auto lookupTargetFunctionSymbol =
      [new_scope](const SgName &name, SgFunctionType *type,
                  SgScopeStatement *use_scope) -> SgFunctionSymbol * {
    if (type == NULL || use_scope == NULL) {
      return NULL;
    }
    std::unordered_set<SgScopeStatement *> visited;
    for (SgScopeStatement *scope = use_scope; scope != NULL;) {
      if (!visited.insert(scope).second) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT"
                "[cross-output-function-lookup]: name=%s scope=%p has a "
                "cyclic semantic parent chain\n",
                name.str(), static_cast<void *>(scope));
        ROSE_ABORT();
      }
      SgSymbolTable *table = scope->get_symbol_table();
      if (table == NULL || table->get_parent() != scope) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT"
                "[cross-output-function-lookup]: name=%s scope=%p has no "
                "exact local symbol table\n",
                name.str(), static_cast<void *>(scope));
        ROSE_ABORT();
      }
      if (SgFunctionSymbol *symbol = table->find_function(name, type, NULL)) {
        return symbol;
      }
      if (scope == new_scope) {
        return NULL;
      }
      if (isSgGlobal(scope) != NULL || scope->get_parent() == NULL) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT"
                "[cross-output-function-lookup]: name=%s use-scope=%p "
                "reached global=%p before destination-global=%p\n",
                name.str(), static_cast<void *>(use_scope),
                static_cast<void *>(scope), static_cast<void *>(new_scope));
        ROSE_ABORT();
      }
      scope = scope->get_scope();
    }
    return NULL;
  };
  auto requireTargetFunctionSymbol =
      [original_scope, new_scope, new_file, &translateCrossOutputType,
       &buildTargetFunctionType, &published_target_external_symbols,
       &lookupTargetFunctionSymbol](
          SgFunctionSymbol *source_symbol,
          SgScopeStatement *target_use_scope) -> SgFunctionSymbol * {
    SgFunctionDeclaration *source_declaration =
        source_symbol != NULL ? source_symbol->get_declaration() : NULL;
    SgFunctionDeclaration *source_canonical =
        source_declaration != NULL
            ? isSgFunctionDeclaration(
                  source_declaration->get_firstNondefiningDeclaration())
            : NULL;
    SgFunctionType *source_type =
        source_symbol != NULL ? isSgFunctionType(source_symbol->get_type())
                              : NULL;
    SgFunctionParameterList *source_parameters =
        source_canonical != NULL ? source_canonical->get_parameterList() : NULL;
    if (source_symbol == NULL || source_declaration == NULL ||
        source_canonical == NULL || source_type == NULL ||
        source_parameters == NULL ||
        source_parameters->get_parent() != source_canonical ||
        source_declaration->get_type() != source_type ||
        source_canonical->get_type() != source_type ||
        target_use_scope == NULL ||
        SageInterface::getEnclosingSourceFile(target_use_scope) != new_file) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
              "source-symbol=%p declaration=%p canonical=%p type=%p "
              "parameters=%p target-scope=%p has no exact source and target "
              "context\n",
              static_cast<void *>(source_symbol),
              static_cast<void *>(source_declaration),
              static_cast<void *>(source_canonical),
              static_cast<void *>(source_type),
              static_cast<void *>(source_parameters),
              static_cast<void *>(target_use_scope));
      ROSE_ABORT();
    }

    const auto prior = published_target_external_symbols.find(source_canonical);
    if (prior != published_target_external_symbols.end()) {
      SgFunctionSymbol *target_symbol = prior->second;
      SgFunctionDeclaration *target_declaration =
          target_symbol != NULL ? target_symbol->get_declaration() : NULL;
      SgFunctionType *target_type =
          target_symbol != NULL ? isSgFunctionType(target_symbol->get_type())
                                : NULL;
      if (target_symbol == NULL || target_symbol == source_symbol ||
          target_declaration == NULL || target_type == NULL ||
          target_symbol->get_name() != source_symbol->get_name() ||
          target_declaration->get_type() != target_type ||
          SageInterface::getEnclosingSourceFile(target_symbol) != new_file ||
          lookupTargetFunctionSymbol(source_symbol->get_name(), target_type,
                                     target_use_scope) != target_symbol) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
                "source=%p name=%s previously published target symbol=%p "
                "declaration=%p type=%p is not one exact visible target-file "
                "identity\n",
                static_cast<void *>(source_canonical),
                source_symbol->get_name().str(),
                static_cast<void *>(target_symbol),
                static_cast<void *>(target_declaration),
                static_cast<void *>(target_type));
        ROSE_ABORT();
      }
      return target_symbol;
    }

    SgFunctionType *target_type = buildTargetFunctionType(source_type);
    SgFunctionSymbol *target_symbol = lookupTargetFunctionSymbol(
        source_symbol->get_name(), target_type, target_use_scope);
    if (target_symbol != NULL) {
      return target_symbol;
    }

    SgScopeStatement *source_scope = source_canonical->get_scope();
    SgAuxiliaryDeclarationList *source_auxiliary =
        source_scope != NULL ? source_scope->get_auxiliary_declarations()
                             : NULL;
    const bool exact_source_lexical =
        source_scope != NULL &&
        source_canonical->get_parent() == source_scope &&
        source_scope->statementExistsInScope(source_canonical);
    const bool exact_source_auxiliary =
        source_scope != NULL && source_auxiliary != NULL &&
        source_auxiliary->get_parent() == source_scope &&
        source_canonical->get_parent() == source_auxiliary &&
        std::count(source_auxiliary->get_declarations().begin(),
                   source_auxiliary->get_declarations().end(),
                   source_canonical) == 1;
    if (source_canonical != source_declaration ||
        source_scope != original_scope ||
        SageInterface::getEnclosingSourceFile(source_scope) !=
            SageInterface::getEnclosingSourceFile(original_scope) ||
        exact_source_lexical == exact_source_auxiliary ||
        source_canonical->get_firstNondefiningDeclaration() !=
            source_canonical ||
        source_canonical->get_definition() != NULL ||
        source_canonical->get_declarationModifier()
            .get_storageModifier()
            .isStatic()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
              "source=%p name=%s parent=%p scope=%p first=%p defining=%p "
              "has no target declaration and is not one exact externally "
              "linkable source canonical\n",
              static_cast<void *>(source_canonical),
              source_symbol->get_name().str(),
              static_cast<void *>(source_canonical->get_parent()),
              static_cast<void *>(source_canonical->get_scope()),
              static_cast<void *>(
                  source_canonical->get_firstNondefiningDeclaration()),
              static_cast<void *>(source_canonical->get_definingDeclaration()));
      ROSE_ABORT();
    }

    SgFunctionParameterList *target_parameters =
        SageBuilder::buildSemanticFunctionParameterList(source_parameters);
    if (target_parameters == NULL || target_parameters->get_args().size() !=
                                         source_parameters->get_args().size()) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
              "source=%p name=%s produced no exact target parameter list\n",
              static_cast<void *>(source_canonical),
              source_symbol->get_name().str());
      ROSE_ABORT();
    }
    for (size_t index = 0; index < target_parameters->get_args().size();
         ++index) {
      SgInitializedName *source_parameter =
          source_parameters->get_args()[index];
      SgInitializedName *target_parameter =
          target_parameters->get_args()[index];
      if (source_parameter == NULL || target_parameter == NULL ||
          source_parameter->get_type() == NULL ||
          target_parameter->get_parent() != target_parameters) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
                "source=%p name=%s parameter=%zu has no exact copied "
                "identity\n",
                static_cast<void *>(source_canonical),
                source_symbol->get_name().str(), index);
        ROSE_ABORT();
      }
      target_parameter->set_type(translateCrossOutputType(
          source_parameter->get_type(), new_scope, source_parameter,
          target_parameter, "function-reference-declaration-parameter"));
    }
    SgFunctionParameterTypeList *target_type_arguments =
        target_type->get_argument_list();
    bool exact_parameter_types =
        target_type_arguments != NULL &&
        target_type_arguments->get_parent() == target_type &&
        target_type_arguments->get_arguments().size() ==
            target_parameters->get_args().size();
    if (exact_parameter_types) {
      for (size_t index = 0; index < target_parameters->get_args().size();
           ++index) {
        if (target_parameters->get_args()[index]->get_type() !=
            target_type_arguments->get_arguments()[index]) {
          exact_parameter_types = false;
          break;
        }
      }
    }
    if (!exact_parameter_types) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
              "source=%p name=%s target-type=%p arguments=%p parameters=%p "
              "does not preserve one exact translated semantic signature\n",
              static_cast<void *>(source_canonical),
              source_symbol->get_name().str(), static_cast<void *>(target_type),
              static_cast<void *>(target_type_arguments),
              static_cast<void *>(target_parameters));
      ROSE_ABORT();
    }

    SgFunctionDeclaration *target_declaration =
        SageBuilder::buildNondefiningFunctionDeclaration(
            SageBuilder::function_declaration_ownership::semanticAuxiliary(),
            source_canonical->get_name(), target_type->get_return_type(),
            target_parameters, new_scope);
    SgAuxiliaryDeclarationList *target_auxiliary =
        new_scope->get_auxiliary_declarations();
    if (target_declaration == NULL || target_auxiliary == NULL ||
        target_auxiliary->get_parent() != new_scope ||
        target_declaration->get_parent() != target_auxiliary ||
        target_declaration->get_scope() != new_scope ||
        target_declaration->get_parameterList() != target_parameters ||
        target_declaration->get_type() != target_type ||
        target_declaration->get_firstNondefiningDeclaration() !=
            target_declaration ||
        target_declaration->get_definingDeclaration() != NULL ||
        std::count(target_auxiliary->get_declarations().begin(),
                   target_auxiliary->get_declarations().end(),
                   target_declaration) != 1) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
              "source=%p name=%s target=%p parent=%p scope=%p did not "
              "publish one exact semantic prototype\n",
              static_cast<void *>(source_canonical),
              source_symbol->get_name().str(),
              static_cast<void *>(target_declaration),
              static_cast<void *>(target_declaration != NULL
                                      ? target_declaration->get_parent()
                                      : NULL),
              static_cast<void *>(target_declaration != NULL
                                      ? target_declaration->get_scope()
                                      : NULL));
      ROSE_ABORT();
    }
    target_declaration->set_linkage(source_canonical->get_linkage());
    target_declaration->get_declarationModifier()
        .get_storageModifier()
        .setExtern();
    target_symbol = lookupTargetFunctionSymbol(source_symbol->get_name(),
                                               target_type, target_use_scope);
    if (target_symbol == NULL || target_symbol == source_symbol ||
        target_symbol->get_declaration() != target_declaration ||
        SageInterface::getEnclosingSourceFile(target_symbol) != new_file ||
        !published_target_external_symbols
             .emplace(source_canonical, target_symbol)
             .second) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-function-symbol]: "
              "source=%p name=%s target=%p symbol=%p did not expose one "
              "exact target-file identity\n",
              static_cast<void *>(source_canonical),
              source_symbol->get_name().str(),
              static_cast<void *>(target_declaration),
              static_cast<void *>(target_symbol));
      ROSE_ABORT();
    }
    return target_symbol;
  };

  // The dependent declarations were copied into the outlined file before the
  // kernel itself.  Rebind every stored type edge in the copied function from
  // its exact source node, after attachment makes local declaration scopes
  // visible.  Rebinding only parameters leaves local declarations and explicit
  // expression result types pointing into the source translation unit.
  for (const auto &mapped_node : identityMap) {
    const SgNode *source_node = mapped_node.first;
    SgNode *target_node = mapped_node.second;
    if (source_node == NULL || target_node == NULL ||
        source_node->variantT() != target_node->variantT()) {
      fprintf(
          stderr,
          "REX_OMP_LOWERING_INVARIANT[cross-output-type-map]: "
          "source=%p/%s target=%p/%s is not one exact copied identity\n",
          static_cast<const void *>(source_node),
          source_node != NULL ? source_node->class_name().c_str() : "<null>",
          static_cast<void *>(target_node),
          target_node != NULL ? target_node->class_name().c_str() : "<null>");
      ROSE_ABORT();
    }

    const bool source_is_function_reference =
        isSgFunctionRefExp(const_cast<SgNode *>(source_node)) != NULL ||
        isSgMemberFunctionRefExp(const_cast<SgNode *>(source_node)) != NULL;
    if (source_is_function_reference) {
      SgExpression *target_reference = isSgExpression(target_node);
      SgStatement *target_statement =
          target_reference != NULL
              ? SageInterface::getEnclosingStatement(target_reference)
              : NULL;
      SgScopeStatement *target_reference_scope =
          target_statement != NULL ? target_statement->get_scope() : NULL;
      if (target_reference == NULL || target_reference_scope == NULL ||
          SageInterface::getEnclosingSourceFile(target_reference_scope) !=
              new_file) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-"
                "reference]: source=%p/%s target=%p/%s scope=%p has no "
                "exact target-file use context\n",
                static_cast<const void *>(source_node),
                source_node->class_name().c_str(),
                static_cast<void *>(target_node),
                target_node->class_name().c_str(),
                static_cast<void *>(target_reference_scope));
        ROSE_ABORT();
      }
      SgSymbol *target_symbol = NULL;
      SgSymbol *source_symbol = NULL;
      if (SgFunctionRefExp *source_reference =
              isSgFunctionRefExp(const_cast<SgNode *>(source_node))) {
        SgFunctionRefExp *copied_reference =
            isSgFunctionRefExp(target_reference);
        source_symbol = source_reference->get_symbol();
        SgFunctionSymbol *target_function_symbol = requireTargetFunctionSymbol(
            source_reference->get_symbol(), target_reference_scope);
        if (copied_reference != NULL) {
          copied_reference->set_symbol(target_function_symbol);
        }
        target_symbol = target_function_symbol;
      } else {
        SgMemberFunctionRefExp *source_member_reference =
            isSgMemberFunctionRefExp(const_cast<SgNode *>(source_node));
        SgMemberFunctionRefExp *copied_reference =
            isSgMemberFunctionRefExp(target_reference);
        source_symbol = source_member_reference != NULL
                            ? source_member_reference->get_symbol()
                            : NULL;
        SageBuilder::fixupCopyOfNodeFromSeparateFileInNewTargetAst(
            target_reference_scope, true, target_reference,
            const_cast<SgNode *>(source_node));
        target_symbol =
            copied_reference != NULL ? copied_reference->get_symbol() : NULL;
      }
      if (source_symbol == NULL || target_symbol == NULL ||
          source_symbol == target_symbol ||
          SageInterface::getEnclosingSourceFile(target_symbol) != new_file ||
          target_reference->get_type() != target_symbol->get_type()) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-"
                "reference]: source=%p symbol=%p target=%p symbol=%p type=%p "
                "expected-type=%p did not bind one distinct target-file "
                "function identity\n",
                static_cast<const void *>(source_node),
                static_cast<void *>(source_symbol),
                static_cast<void *>(target_node),
                static_cast<void *>(target_symbol),
                static_cast<void *>(target_reference->get_type()),
                static_cast<void *>(
                    target_symbol != NULL ? target_symbol->get_type() : NULL));
        ROSE_ABORT();
      }
      // A function reference derives its exact result and signature type from
      // the target-file symbol just published above.  Treating that function
      // type as an independently stored expression type would duplicate the
      // declaration translation and detach named parameter identities from
      // their owning target declarations.
      continue;
    }

    const SgInitializedName *source_name =
        isSgInitializedName(const_cast<SgNode *>(source_node));
    if (source_name != NULL) {
      SgInitializedName *target_name = isSgInitializedName(target_node);
      SgScopeStatement *target_type_scope =
          target_name != NULL ? target_name->get_scope() : NULL;
      if (target_type_scope == NULL && target_name != NULL) {
        SgStatement *enclosing_statement =
            SageInterface::getEnclosingStatement(target_name);
        target_type_scope = enclosing_statement != NULL
                                ? enclosing_statement->get_scope()
                                : NULL;
      }
      if (target_name == NULL || target_type_scope == NULL) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-type-map]: "
                "initialized-name source=%p target=%p scope=%p has no exact "
                "copied declaration context\n",
                static_cast<const void *>(source_name),
                static_cast<void *>(target_name),
                static_cast<void *>(target_type_scope));
        ROSE_ABORT();
      }
      SgType *target_type = translateCrossOutputType(
          source_name->get_type(), target_type_scope, source_name, target_name,
          "initialized-name");
      target_name->set_type(target_type);
      if (target_name->get_type() != target_type) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-type-map]: "
                "initialized-name=%p name=%s rejected target type=%p\n",
                static_cast<void *>(target_name), target_name->get_name().str(),
                static_cast<void *>(target_type));
        ROSE_ABORT();
      }

      SgType *source_syntax_type = source_name->get_cxx_source_type();
      if (source_syntax_type != NULL) {
        SgType *target_syntax_type = translateCrossOutputType(
            source_syntax_type, target_type_scope, source_name, target_name,
            "initialized-name-source-type");
        target_name->set_cxx_source_type(target_syntax_type);
        if (target_name->get_cxx_source_type() != target_syntax_type) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[cross-output-type-map]: "
                  "initialized-name=%p name=%s rejected target source "
                  "type=%p\n",
                  static_cast<void *>(target_name),
                  target_name->get_name().str(),
                  static_cast<void *>(target_syntax_type));
          ROSE_ABORT();
        }
      }
    }

    const SgExpression *source_expression =
        isSgExpression(const_cast<SgNode *>(source_node));
    if (source_expression == NULL ||
        !const_cast<SgExpression *>(source_expression)->hasExplicitType()) {
      continue;
    }
    SgExpression *target_expression = isSgExpression(target_node);
    SgStatement *target_statement =
        target_expression != NULL
            ? SageInterface::getEnclosingStatement(target_expression)
            : NULL;
    SgScopeStatement *target_expression_scope =
        target_statement != NULL ? target_statement->get_scope() : NULL;
    const SgThisExp *source_this =
        isSgThisExp(const_cast<SgExpression *>(source_expression));
    SgThisExp *target_this = isSgThisExp(target_expression);
    SgType *source_expression_type = source_this != NULL
                                         ? source_this->get_expression_type()
                                         : source_expression->get_type();
    if (target_expression == NULL || target_expression_scope == NULL ||
        (source_this != NULL) != (target_this != NULL)) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-type-map]: "
              "explicit expression source=%p/%s target=%p/%s scope=%p has "
              "no exact copied expression context\n",
              static_cast<const void *>(source_expression),
              source_expression->class_name().c_str(),
              static_cast<void *>(target_expression),
              target_expression != NULL
                  ? target_expression->class_name().c_str()
                  : "<null>",
              static_cast<void *>(target_expression_scope));
      ROSE_ABORT();
    }
    SgType *target_expression_type = NULL;
    SgCastExp *source_cast =
        isSgCastExp(const_cast<SgExpression *>(source_expression));
    SgCastExp *target_cast = isSgCastExp(target_expression);
    if (source_cast != NULL && target_cast != NULL &&
        source_cast->get_semantic_conversion_kind() ==
            SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
      SgFunctionRefExp *source_function_reference =
          isSgFunctionRefExp(source_cast->get_operand());
      SgFunctionRefExp *target_function_reference =
          isSgFunctionRefExp(target_cast->get_operand());
      SgPointerType *source_pointer = isSgPointerType(source_expression_type);
      if (source_function_reference == NULL ||
          target_function_reference == NULL || source_pointer == NULL ||
          source_pointer->get_base_type() !=
              source_function_reference->get_type() ||
          source_cast->get_cast_type() != target_cast->get_cast_type() ||
          source_cast->get_semantic_conversion_kind() !=
              target_cast->get_semantic_conversion_kind() ||
          source_cast->get_value_category() !=
              target_cast->get_value_category()) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-decay]: "
                "source=%p target=%p source-reference=%p target-reference=%p "
                "source-type=%p does not describe one exact copied "
                "function-to-pointer conversion\n",
                static_cast<void *>(source_cast),
                static_cast<void *>(target_cast),
                static_cast<void *>(source_function_reference),
                static_cast<void *>(target_function_reference),
                static_cast<void *>(source_expression_type));
        ROSE_ABORT();
      }
      SgFunctionSymbol *target_function_symbol = requireTargetFunctionSymbol(
          source_function_reference->get_symbol(), target_expression_scope);
      target_function_reference->set_symbol(target_function_symbol);
      if (target_function_reference->get_symbol() != target_function_symbol ||
          target_function_reference->get_type() !=
              target_function_symbol->get_type()) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-decay]: "
                "target-reference=%p symbol=%p type=%p expected-type=%p "
                "did not bind the exact target-file function identity\n",
                static_cast<void *>(target_function_reference),
                static_cast<void *>(target_function_reference->get_symbol()),
                static_cast<void *>(target_function_reference->get_type()),
                static_cast<void *>(target_function_symbol->get_type()));
        ROSE_ABORT();
      }
      target_expression_type =
          SageBuilder::buildPointerType(target_function_reference->get_type());
      SgPointerType *target_pointer = isSgPointerType(target_expression_type);
      if (target_pointer == NULL || target_pointer->get_base_type() !=
                                        target_function_reference->get_type()) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cross-output-function-decay]: "
                "target-reference=%p produced non-exact pointer type=%p\n",
                static_cast<void *>(target_function_reference),
                static_cast<void *>(target_expression_type));
        ROSE_ABORT();
      }
    } else {
      target_expression_type =
          isSgSizeOfOp(const_cast<SgExpression *>(source_expression)) != NULL ||
                  isSgAlignOfOp(
                      const_cast<SgExpression *>(source_expression)) != NULL
              ? SageInterface::requireTargetSizeType(target_expression_scope)
              : translateCrossOutputType(source_expression_type,
                                         target_expression_scope,
                                         source_expression, target_expression,
                                         "explicit-expression");
    }
    if (target_this != NULL) {
      target_this->set_expression_type(target_expression_type);
    } else {
      target_expression->set_explicitly_stored_type(target_expression_type);
    }
    SgType *published_expression_type = target_this != NULL
                                            ? target_this->get_expression_type()
                                            : target_expression->get_type();
    if (published_expression_type != target_expression_type) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cross-output-type-map]: "
              "expression=%p/%s rejected target type=%p published=%p\n",
              static_cast<void *>(target_expression),
              target_expression->class_name().c_str(),
              static_cast<void *>(target_expression_type),
              static_cast<void *>(published_expression_type));
      ROSE_ABORT();
    }
  }
  SageInterface::rebindVariableReferencesAfterMove(new_file);
  createOmpStatementTree(new_file);

  // Preserve one real lexical extern prototype in the source translation
  // unit. Generated functions can already have a source-owned canonical
  // declaration plus auxiliary family members; other producers start with an
  // auxiliary canonical. Retire only the non-output family members, and create
  // a lexical canonical exactly when none exists.
  SgFunctionDeclaration *source_canonical =
      isSgFunctionDeclaration(outlined_func->get_firstNondefiningDeclaration());
  SgAuxiliaryDeclarationList *source_auxiliary_owner =
      original_scope->get_auxiliary_declarations();
  const bool canonical_is_source_owned =
      source_canonical != NULL &&
      source_canonical->get_parent() == original_scope &&
      original_scope->statementExistsInScope(source_canonical);
  const bool canonical_is_auxiliary_owned =
      source_canonical != NULL && source_auxiliary_owner != NULL &&
      source_auxiliary_owner->get_parent() == original_scope &&
      source_canonical->get_parent() == source_auxiliary_owner &&
      std::count(source_auxiliary_owner->get_declarations().begin(),
                 source_auxiliary_owner->get_declarations().end(),
                 source_canonical) == 1;
  SgFunctionSymbol *source_symbol =
      source_canonical != NULL
          ? isSgFunctionSymbol(
                original_scope->find_symbol_from_declaration(source_canonical))
          : NULL;
  std::vector<SgFunctionDeclaration *> obsolete_source_family;
  if (source_auxiliary_owner != NULL) {
    for (SgDeclarationStatement *declaration :
         source_auxiliary_owner->get_declarations()) {
      SgFunctionDeclaration *family_member =
          isSgFunctionDeclaration(declaration);
      if (family_member != NULL &&
          (family_member == source_canonical ||
           family_member->get_firstNondefiningDeclaration() ==
               source_canonical)) {
        obsolete_source_family.push_back(family_member);
      }
    }
  }
  if (source_canonical == nullptr ||
      canonical_is_source_owned == canonical_is_auxiliary_owned ||
      source_canonical->get_scope() != original_scope ||
      source_canonical->get_firstNondefiningDeclaration() != source_canonical ||
      source_canonical->get_definingDeclaration() != outlined_func ||
      source_symbol == NULL ||
      source_symbol->get_symbol_basis() != source_canonical) {
    fprintf(
        stderr,
        "REX_OMP_LOWERING_INVARIANT[source-extern-family]: canonical=%p "
        "parent=%p expected-scope=%p source-owned=%d "
        "auxiliary-owned=%d first=%p defining=%p expected-defining=%p "
        "symbol=%p symbol-basis=%p auxiliary-members=%zu\n",
        static_cast<void *>(source_canonical),
        static_cast<void *>(
            source_canonical != NULL ? source_canonical->get_parent() : NULL),
        static_cast<void *>(original_scope), canonical_is_source_owned ? 1 : 0,
        canonical_is_auxiliary_owned ? 1 : 0,
        static_cast<void *>(
            source_canonical != NULL
                ? source_canonical->get_firstNondefiningDeclaration()
                : NULL),
        static_cast<void *>(source_canonical != NULL
                                ? source_canonical->get_definingDeclaration()
                                : NULL),
        static_cast<void *>(outlined_func), static_cast<void *>(source_symbol),
        static_cast<void *>(
            source_symbol != NULL ? source_symbol->get_symbol_basis() : NULL),
        obsolete_source_family.size());
    failOutlinedClauseRecord("has no exact original-file declaration",
                             outlined_func, source_canonical);
  }
  for (SgFunctionDeclaration *family_member : obsolete_source_family) {
    if (family_member->get_parent() != source_auxiliary_owner ||
        family_member->get_scope() != original_scope ||
        family_member->get_definingDeclaration() != outlined_func ||
        (family_member != source_canonical &&
         family_member->get_firstNondefiningDeclaration() !=
             source_canonical)) {
      failOutlinedClauseRecord("has a malformed source auxiliary family",
                               family_member, source_canonical);
    }
  }

  SgFunctionDeclaration *extern_header = source_canonical;
  if (canonical_is_auxiliary_owned) {
    SgFunctionParameterList *source_parameters =
        SageBuilder::buildSemanticFunctionParameterList(
            outlined_func->get_parameterList());
    extern_header = SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::
            sourceLexicalCanonicalReplacementBefore(
                original_scope, source_insertion_anchor, source_canonical),
        outlined_func->get_name(), outlined_func->get_type()->get_return_type(),
        source_parameters, original_scope);
  }
  if (extern_header == NULL ||
      (canonical_is_auxiliary_owned && extern_header == source_canonical) ||
      extern_header->get_parent() != original_scope ||
      extern_header->get_scope() != original_scope ||
      extern_header->get_firstNondefiningDeclaration() != extern_header ||
      extern_header->get_definingDeclaration() != outlined_func ||
      outlined_func->get_firstNondefiningDeclaration() != extern_header ||
      source_symbol->get_symbol_basis() != extern_header) {
    failOutlinedClauseRecord("failed to publish one exact source extern family",
                             extern_header, source_canonical);
  }
  extern_header->set_linkage(outlined_func->get_linkage());
  extern_header->get_declarationModifier().get_storageModifier().setExtern();

  for (SgFunctionDeclaration *family_member : obsolete_source_family) {
    if (!SageBuilder::detachAuxiliaryDeclaration(original_scope,
                                                 family_member) ||
        family_member->get_parent() != NULL) {
      failOutlinedClauseRecord("failed to detach obsolete source canonical",
                               family_member, extern_header);
    }
    family_member->set_firstNondefiningDeclaration(family_member);
    family_member->set_definingDeclaration(NULL);
  }
  for (SgFunctionDeclaration *family_member : obsolete_source_family) {
    SageInterface::deleteAST(family_member,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    if (SgNode::isLiveNode(family_member)) {
      failOutlinedClauseRecord("obsolete source canonical remained live",
                               family_member, extern_header);
    }
  }

  // remove the outlined function in the original file and perform post
  // processing later once the outlined-file transformations are complete
  commitClauseVariableRenamingRecords(preparedClauseMappings);
  removeStatement(outlined_func);
  extern_header->set_definingDeclaration(NULL);
  SgSourceFile *original_file = isSgSourceFile(original_scope->get_parent());
  if (original_file == NULL) {
    failOutlinedClauseRecord("source scope has no exact source-file owner",
                             original_scope, outlined_func);
  }
  createOmpStatementTree(original_file);
  retireOutlinedFunctionSyntaxType(outlined_func);
  SageInterface::deleteAST(outlined_func,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  if (SgNode::isLiveNode(outlined_func)) {
    failOutlinedClauseRecord("source function remained live after exact move",
                             outlined_func, new_outlined_function);
  }
  if (extern_header->get_firstNondefiningDeclaration() != extern_header ||
      extern_header->get_definingDeclaration() != NULL ||
      extern_header->get_parent() != original_scope ||
      extern_header->get_scope() != original_scope ||
      source_symbol->get_symbol_basis() != extern_header) {
    failOutlinedClauseRecord("source extern family changed after exact move",
                             extern_header, new_outlined_function);
  }
  if (target_outlined_source_function.erase(outlined_func) != 1) {
    failOutlinedClauseRecord("lost its exact source-function anchor at commit",
                             outlined_func, source_function);
  }
  return new_outlined_function;
}

static SgSourceFile *
generate_outlined_function_file(SgFunctionDeclaration *outlined_func,
                                std::string file_extension) {

  // prepare the required information of original file
  std::string original_name = outlined_func->get_name().getString();
  SgBasicBlock *function_block = outlined_func->get_definition()->get_body();
  SgSourceFile *new_file = NULL;
  SgFile *cur_file = getEnclosingNode<SgFile>(outlined_func);
  std::string original_file_name = StringUtility::stripFileSuffixFromFileName(
      StringUtility::stripPathFromFileName(
          cur_file->get_file_info()->get_filenameString()));
  if (file_extension == "") {
    file_extension = StringUtility::fileNameSuffix(
        cur_file->get_file_info()->get_filenameString());
  };

  // create a new file with all the function declaration and preprocessing
  // information of the original file
  new_file = Outliner::getLibSourceFile(function_block);
  ROSE_ASSERT(new_file != NULL);
  // reset the name of new outlined function file
  std::string new_file_name =
      "rex_lib_" + original_file_name + "." + file_extension;
  SageBuilder::rebindCopiedSourceFilePhysicalIdentity(new_file, new_file_name);
  new_file->set_unparse_output_filename(new_file_name);
  // Outlined files are synthesized/renamed after parsing, so token-stream
  // mappings from the original source are not valid for them.
  new_file->set_unparse_tokens(false);

  // insert REX runtime header to the new file (C/C++ only)
  SgGlobal *new_scope = new_file->get_globalScope();
  bool inserted_header = false;
  if (!new_file->get_Fortran_only()) {
    if (file_extension == "cu") {
      SageInterface::insertHeader(new_file, "rex_nvidia.h",
                                  /*isSystemHeader=*/false,
                                  /*asLastHeader=*/true);
      inserted_header = true;
    } else {
      SageInterface::insertHeader(new_file, "rex_kmp.h",
                                  /*isSystemHeader=*/false,
                                  /*asLastHeader=*/true);
      inserted_header = true;
    }
  }
  if (inserted_header) {
    new_file->set_processedToIncludeCppDirectivesAndComments(true);
  }

  if (file_extension == "cu") {
    rewriteCudaSiblingIncludesInOutlinedFile(
        new_file,
        std::filesystem::path(cur_file->get_file_info()->get_filenameString()));
  }

  fix_storage_modifier(new_file, getGlobalScope(outlined_func));
  if (file_extension == "cu") {
    // CUDA builtin dimensions are generated semantic declarations, so the
    // parsed output-file seed cannot contain them.  Publish the complete typed
    // runtime identity set actually produced in the source translation unit,
    // then assign every destination declaration the exact translated source
    // type before moved expressions are rebound.
    SgGlobal *source_scope = getGlobalScope(outlined_func);
    if (source_scope == NULL) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[cuda-runtime-output-owner]: "
              "outlined function=%p has no exact source global scope\n",
              static_cast<void *>(outlined_func));
      ROSE_ABORT();
    }
    (void)getCudaBuiltinDimensionType(new_scope);
    const CudaBuiltinDimensionVariable runtime_dimensions[] = {
        CudaBuiltinDimensionVariable::block_dim,
        CudaBuiltinDimensionVariable::block_index,
        CudaBuiltinDimensionVariable::grid_dim,
        CudaBuiltinDimensionVariable::thread_index};
    for (CudaBuiltinDimensionVariable dimension : runtime_dimensions) {
      const SgName name(getCudaBuiltinDimensionVariableName(dimension));
      SgVariableSymbol *source_symbol =
          source_scope->lookup_variable_symbol(name);
      if (source_symbol == NULL) {
        continue;
      }
      SgInitializedName *source_name = source_symbol->get_declaration();
      SgVariableSymbol *target_symbol =
          getCudaBuiltinDimensionVariableSymbol(dimension, new_scope);
      SgInitializedName *target_name = target_symbol->get_declaration();
      SgType *target_type = source_name != NULL
                                ? SageBuilder::getTargetFileType(
                                      source_name->get_type(), new_scope)
                                : NULL;
      if (source_name == NULL || source_name->get_scope() != source_scope ||
          target_name == NULL || target_name->get_scope() != new_scope ||
          target_type == NULL) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cuda-runtime-output-owner]: "
                "runtime dimension=%s source=%p target=%p type=%p has no "
                "exact cross-output identity\n",
                name.str(), static_cast<void *>(source_name),
                static_cast<void *>(target_name),
                static_cast<void *>(target_type));
        ROSE_ABORT();
      }
      target_name->set_type(target_type);
      if (target_name->get_type() != target_type) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[cuda-runtime-output-owner]: "
                "runtime dimension=%s rejected exact target type=%p\n",
                name.str(), static_cast<void *>(target_type));
        ROSE_ABORT();
      }
    }
  }

  return new_file;
}

static void fix_storage_modifier(SgSourceFile *new_file,
                                 SgGlobal *source_scope) {
  SgGlobal *target_scope =
      new_file != NULL ? new_file->get_globalScope() : NULL;
  if (new_file == NULL || source_scope == NULL || target_scope == NULL ||
      source_scope == target_scope) {
    fprintf(stderr,
            "REX_OMP_LOWERING_INVARIANT[outlined-global-type]: source=%p "
            "target-file=%p target=%p has no exact cross-output context\n",
            static_cast<void *>(source_scope), static_cast<void *>(new_file),
            static_cast<void *>(target_scope));
    ROSE_ABORT();
  }

  // set the regular global variables in the new file to extern and remove their
  // definition
  Rose_STL_Container<SgNode *> global_variable_list =
      NodeQuery::querySubTree(new_file, V_SgVariableDeclaration);
  Rose_STL_Container<SgNode *>::iterator global_variable_list_iterator;
  for (global_variable_list_iterator = global_variable_list.begin();
       global_variable_list_iterator != global_variable_list.end();
       global_variable_list_iterator++) {
    SgVariableDeclaration *global_variable =
        isSgVariableDeclaration(*global_variable_list_iterator);
    if (isSgGlobal(global_variable->get_scope())) {
      for (SgInitializedName *target_name : global_variable->get_variables()) {
        SgVariableSymbol *source_symbol =
            target_name != NULL
                ? source_scope->lookup_variable_symbol(target_name->get_name())
                : NULL;
        SgInitializedName *source_name =
            source_symbol != NULL ? source_symbol->get_declaration() : NULL;
        SgType *target_type = source_name != NULL
                                  ? SageBuilder::getTargetFileType(
                                        source_name->get_type(), target_scope)
                                  : NULL;
        if (target_name == NULL || source_name == NULL ||
            source_name->get_scope() != source_scope || target_type == NULL) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[outlined-global-type]: "
                  "target-name=%p source-symbol=%p source-name=%p has no "
                  "exact source global identity\n",
                  static_cast<void *>(target_name),
                  static_cast<void *>(source_symbol),
                  static_cast<void *>(source_name));
          ROSE_ABORT();
        }
        target_name->set_type(target_type);
        if (target_name->get_type() != target_type) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[outlined-global-type]: "
                  "target-name=%p did not publish exact target type=%p\n",
                  static_cast<void *>(target_name),
                  static_cast<void *>(target_type));
          ROSE_ABORT();
        }
      }
      SgStorageModifier &variable_modifier =
          global_variable->get_declarationModifier().get_storageModifier();
      if (!variable_modifier.isStatic()) {
        variable_modifier.setExtern();
        if (SgDeclarationGroupStatement *group =
                isSgDeclarationGroupStatement(global_variable->get_parent())) {
          group->validate();
          group->get_declarationModifier().get_storageModifier().setExtern();
        }
        global_variable->reset_initializer(NULL);
      };
    };
  };
};

static void post_processing(SgSourceFile *file) {
  SgSourceFile *new_file = NULL;

  // handle the outlined functions for NVIDIA GPU
  if (target_outlined_function_list->size() > 0) {
    // create a new file
    new_file = generate_outlined_function_file(
        target_outlined_function_list->at(0), "cu");
    SgGlobal *new_scope = new_file->get_globalScope();
    SgFile *cur_file =
        getEnclosingNode<SgFile>(target_outlined_function_list->at(0));
    std::string file_extension = StringUtility::fileNameSuffix(
        cur_file->get_file_info()->get_filenameString());

    bool needs_c_linkage_block =
        CommandlineProcessing::isCFileNameSuffix(file_extension) ||
        CommandlineProcessing::isCppFileNameSuffix(file_extension);
    if (needs_c_linkage_block) {
      SgClinkageStartStatement *c_linkage_start =
          new SgClinkageStartStatement();
      c_linkage_start->set_languageSpecifier("C");
      c_linkage_start->set_definingDeclaration(c_linkage_start);
      c_linkage_start->set_firstNondefiningDeclaration(c_linkage_start);
      setOneSourcePositionForTransformation(c_linkage_start);
      appendStatement(c_linkage_start, new_scope);
    }

    // A generated target function can call another generated target function
    // (for example, a target region containing a parallel loop).  Publish
    // callees first so every cross-output reference resolves to an exact
    // target-file declaration.  Reverse insertion order was only an incidental
    // approximation of this dependency and fails when nested lowering records
    // the callee before the caller.
    const size_t outlined_count = target_outlined_function_list->size();
    std::unordered_map<SgFunctionDeclaration *, size_t> outlined_family_owner;
    std::vector<std::set<size_t>> outlined_dependencies(outlined_count);
    for (size_t index = 0; index < outlined_count; ++index) {
      SgFunctionDeclaration *function =
          target_outlined_function_list->at(index);
      SgFunctionDeclaration *canonical =
          function != NULL ? isSgFunctionDeclaration(
                                 function->get_firstNondefiningDeclaration())
                           : NULL;
      if (function == NULL || function->get_definition() == NULL ||
          canonical == NULL ||
          canonical->get_definingDeclaration() != function) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[target-function-order]: "
                "index=%zu function=%p canonical=%p has no exact generated "
                "declaration family\n",
                index, static_cast<void *>(function),
                static_cast<void *>(canonical));
        ROSE_ABORT();
      }
      for (SgFunctionDeclaration *member : {function, canonical}) {
        const auto inserted = outlined_family_owner.emplace(member, index);
        if (!inserted.second && inserted.first->second != index) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[target-function-order]: "
                  "declaration=%p belongs to generated functions %zu and "
                  "%zu\n",
                  static_cast<void *>(member), inserted.first->second, index);
          ROSE_ABORT();
        }
      }
    }
    for (size_t caller_index = 0; caller_index < outlined_count;
         ++caller_index) {
      SgFunctionDeclaration *caller =
          target_outlined_function_list->at(caller_index);
      Rose_STL_Container<SgNode *> references =
          NodeQuery::querySubTree(caller, V_SgFunctionRefExp);
      for (SgNode *node : references) {
        SgFunctionRefExp *reference = isSgFunctionRefExp(node);
        SgFunctionSymbol *symbol =
            reference != NULL ? reference->get_symbol() : NULL;
        SgFunctionDeclaration *declaration =
            symbol != NULL ? symbol->get_declaration() : NULL;
        SgFunctionDeclaration *canonical =
            declaration != NULL
                ? isSgFunctionDeclaration(
                      declaration->get_firstNondefiningDeclaration())
                : NULL;
        SgFunctionDeclaration *defining =
            declaration != NULL ? isSgFunctionDeclaration(
                                      declaration->get_definingDeclaration())
                                : NULL;
        std::optional<size_t> callee_index;
        for (SgFunctionDeclaration *member :
             {declaration, canonical, defining}) {
          const auto found = outlined_family_owner.find(member);
          if (found == outlined_family_owner.end()) {
            continue;
          }
          if (callee_index && *callee_index != found->second) {
            fprintf(stderr,
                    "REX_OMP_LOWERING_INVARIANT[target-function-order]: "
                    "reference=%p resolves to competing generated functions "
                    "%zu and %zu\n",
                    static_cast<void *>(reference), *callee_index,
                    found->second);
            ROSE_ABORT();
          }
          callee_index = found->second;
        }
        if (callee_index && *callee_index != caller_index)
          outlined_dependencies[caller_index].insert(*callee_index);
      }
    }

    std::vector<unsigned char> visit_state(outlined_count, 0);
    std::vector<SgFunctionDeclaration *> ordered_outlined_functions;
    ordered_outlined_functions.reserve(outlined_count);
    std::function<void(size_t)> visit_function = [&](size_t index) {
      if (visit_state[index] == 2)
        return;
      if (visit_state[index] == 1) {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[target-function-order]: "
                "generated target call graph contains a cycle through "
                "function=%p name=%s\n",
                static_cast<void *>(target_outlined_function_list->at(index)),
                target_outlined_function_list->at(index)
                    ->get_name()
                    .getString()
                    .c_str());
        ROSE_ABORT();
      }
      visit_state[index] = 1;
      for (size_t dependency : outlined_dependencies[index])
        visit_function(dependency);
      visit_state[index] = 2;
      ordered_outlined_functions.push_back(
          target_outlined_function_list->at(index));
    };
    for (size_t index = 0; index < outlined_count; ++index)
      visit_function(index);
    if (ordered_outlined_functions.size() != outlined_count) {
      fprintf(stderr,
              "REX_OMP_LOWERING_INVARIANT[target-function-order]: ordered "
              "%zu of %zu generated functions\n",
              ordered_outlined_functions.size(), outlined_count);
      ROSE_ABORT();
    }

    // Move the outlined functions in their proved dependency order.
    for (SgFunctionDeclaration *outlined_function :
         ordered_outlined_functions) {
      // set up an omp target parameter for each generated CUDA kernel
      // the naming pattern is "<kernel name>_exec_mode"
      SgType *kernel_exec_mode_type = buildCharType();
      SgVariableDeclaration *kernel_exec_mode_decl = buildVariableDeclaration(
          outlined_function->get_name().getString() + "_exec_mode",
          kernel_exec_mode_type,
          buildAssignInitializer(buildIntVal(0), kernel_exec_mode_type),
          new_scope);
      SgStorageModifier &kernel_exec_mode_modifier =
          kernel_exec_mode_decl->get_declarationModifier()
              .get_storageModifier();
      kernel_exec_mode_modifier.setCudaGlobal();
      appendStatement(kernel_exec_mode_decl, new_scope);

      move_outlined_function(outlined_function, new_file);
    }

    if (needs_c_linkage_block) {
      SgClinkageEndStatement *c_linkage_end = new SgClinkageEndStatement();
      c_linkage_end->set_languageSpecifier("C");
      c_linkage_end->set_definingDeclaration(c_linkage_end);
      c_linkage_end->set_firstNondefiningDeclaration(c_linkage_end);
      setOneSourcePositionForTransformation(c_linkage_end);
      appendStatement(c_linkage_end, new_scope);
    }
  };

  if (new_file != NULL) {
    removeOpenMPPragmaDeclarations(new_file);
    if (new_file->get_Fortran_only())
      removeOpenMPDirectivePreprocessingInfo(new_file);
    removeUnbalancedConditionalDirectives(new_file);
    AstPostProcessing(new_file);
  };
  removeOpenMPPragmaDeclarations(file);
  if (file->get_Fortran_only())
    removeOpenMPDirectivePreprocessingInfo(file);
  removeUnbalancedConditionalDirectives(file);
  AstPostProcessing(file);
};
