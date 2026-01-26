#include "sage3basic.h"

#include "SageTreeBuilder.h"

#include "sage-build.h"

#include "flang-sage.h"

#include "unparse-sage.h"

#include "BuildExprVisitor.h"

#include "BuildVisitor.h"

#include "FlangModuleInfo.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>

#include <optional>

#include <sstream>

#include <set>

#include <unordered_map>

#include "type-parsers.h"

// Controls debugging information output
#define PRINT_FLANG_TRAVERSAL 0

#define ABORT_NO_IMPL ROSE_ABORT()
#define ABORT_NO_TEST ROSE_ABORT()

#define REPLACE 0

#define DO_TODO 0
#define DEPRECATED 0
#define DEBUG_FLANG_UNPARSE 0

namespace Fortran::parser {

void UnparseSage(llvm::raw_ostream &out,
                 const Fortran::parser::Program &program,
                 Fortran::parser::Encoding encoding, bool capitalizeKeywords,
                 bool backslashEscapes,
                 Fortran::parser::preStatementType
                     *preStatement /*, AnalyzedObjectsAsFortran* */) {

  std::cerr << "UnparseSage:: found it \n";
  ABORT_NO_IMPL;
}

// NOTE: This is in a holding pattern as pattern for Replace
#if REPLACE
void Replace(Fortran::parser::IntLiteralConstant &x, const SgExpression *sg) {
  ASSERT_not_null(isSgLongLongIntVal(sg));

  // Testing for now
  std::string value{sage->get_valueString()};

  std::cerr
      << "Replace(Fortran::parser::IntLiteralConstant: need memory for string "
      << value << "\n";
  static const char *tt = "666xxx";

  // Need to also try replacing with a KindParam
  parser::CharBlock cb{tt, 3};
  std::tuple<parser::CharBlock, std::optional<parser::KindParam>> tup{
      std::move(cb), std::nullopt};

  x.t = std::move(tup);
}
#endif

} // namespace Fortran::parser

// Dump debug information
template <class T> static void info(const T &x) {
  std::cerr << "Rose::builder::Build() for flang type " << typeid(x).name()
            << "\n";
}
template <class T> static void info(const T &x, const std::string &pre) {
  std::cerr << pre;
  std::cerr << "Rose::builder::Build() for flang type " << typeid(x).name()
            << "\n";
}

namespace Rose::builder {

using namespace Fortran;

// Helps with finding source position information
enum class Order { begin, end };

// Why is this needed?
template <typename T> void WalkExpr(T &root, SgExpression *&expr);

namespace {
SgExprListExp *BuildArraySpecExprList(Fortran::parser::ArraySpec &x);
void DeclareFortranDummyArguments(SgScopeStatement *paramScope,
                                  const std::list<std::string> &dummyArgs);
void TransferParamScopeToFunctionBody(SgScopeStatement *paramScope,
                                      SgFunctionDeclaration *functionDecl);
SgVariableSymbol *resolveUseAssociatedVariableSymbol(SgSymbol *symbol);
SgVariableSymbol *buildUseAssociatedVariableSymbol(SgVariableSymbol *varSymbol,
                                                   const SgName &localName,
                                                   SgScopeStatement *scope);
bool NamesMatch(const std::string &left, const std::string &right,
                bool case_insensitive);
bool ScopeHasNamelistGroup(SgScopeStatement *scope,
                           const std::string &group_name);
std::optional<std::string>
ExtractBareNameFromFormat(const Fortran::parser::Format &format);
void AppendExpr(SgExprListExp *list, SgExpression *expr);
SgExpression *BuildIoUnitExpr(const Fortran::parser::IoUnit &x);
SgExpression *BuildFormatExpr(const Fortran::parser::Format &x);
std::string
FormatSpecificationToString(const Fortran::format::FormatSpecification &spec);
std::string FormatItemToString(const Fortran::format::FormatItem &item);
std::string
FormatItemsToString(const std::list<Fortran::format::FormatItem> &items);
void ApplyIoControlSpec(const Fortran::parser::IoControlSpec &x,
                        SgReadStatement *readStmt, SgWriteStatement *writeStmt);
void ApplyConnectSpec(const Fortran::parser::ConnectSpec &x,
                      SgOpenStatement *stmt);
void ApplyCloseSpec(const Fortran::parser::CloseStmt::CloseSpec &x,
                    SgCloseStatement *stmt);
void ApplyPositionOrFlushSpec(const Fortran::parser::PositionOrFlushSpec &x,
                              SgIOStatement *stmt);
SgFunctionCallExp *
BuildFunctionCallFromSymbolIfFound(const std::string &func_name,
                                   SgScopeStatement *scope,
                                   SgExprListExp *param_list);

constexpr const char *kFortranImplicitDeclAttr =
    "rose_fortran_implicit_declaration";
constexpr const char *kFortranEmitImplicitDeclAttr =
    "rose_fortran_emit_implicit_declaration";
constexpr const char *kFortranSubmoduleParentAttr =
    "rose_fortran_submodule_parent";

class FortranImplicitDeclAttribute : public AstAttribute {
public:
  AstAttribute *copy() const override {
    return new FortranImplicitDeclAttribute();
  }
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
};

class FortranEmitImplicitDeclAttribute : public AstAttribute {
public:
  AstAttribute *copy() const override {
    return new FortranEmitImplicitDeclAttribute();
  }
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
};

class FlangParamScopeTransferredAttribute : public AstAttribute {
public:
  AstAttribute *copy() const override {
    return new FlangParamScopeTransferredAttribute();
  }
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
};

void MarkFortranImplicitDeclaration(SgVariableDeclaration *decl) {
  ASSERT_not_null(decl);
  if (decl->getAttribute(kFortranImplicitDeclAttr) != nullptr) {
    return;
  }
  decl->addNewAttribute(kFortranImplicitDeclAttr,
                        new FortranImplicitDeclAttribute());
}

void MarkFortranEmitImplicitDeclaration(SgVariableDeclaration *decl) {
  ASSERT_not_null(decl);
  if (decl->getAttribute(kFortranEmitImplicitDeclAttr) != nullptr) {
    return;
  }
  decl->addNewAttribute(kFortranEmitImplicitDeclAttr,
                        new FortranEmitImplicitDeclAttribute());
}

void EnsureSymbolsForBlockDeclarations(SgBasicBlock *block) {
  if (block == nullptr) {
    return;
  }
  SgSymbolTable *symtab = block->get_symbol_table();
  const SgStatementPtrList &stmts = block->get_statements();
  for (SgStatement *stmt : stmts) {
    SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
    if (var_decl == nullptr) {
      continue;
    }
    for (SgInitializedName *init_name : var_decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      if (init_name->get_scope() != block) {
        init_name->set_scope(block);
      }
    }
    SageInterface::fixVariableDeclaration(var_decl, block);
    for (SgInitializedName *init_name : var_decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      if (symtab != nullptr && symtab->find(init_name) == nullptr) {
        SgVariableSymbol *var_sym = new SgVariableSymbol(init_name);
        block->insert_symbol(init_name->get_name(), var_sym);
      }
    }
  }
}

SgType *StripModifierType(SgType *type) {
  while (auto *modifier = isSgModifierType(type)) {
    type = modifier->get_base_type();
  }
  return type;
}

SgType *StripPointerType(SgType *type) {
  type = StripModifierType(type);
  if (auto *pointer = isSgPointerType(type)) {
    type = pointer->get_base_type();
  }
  return StripModifierType(type);
}

bool IsArrayType(SgType *type) {
  type = StripPointerType(type);
  return isSgArrayType(type) != nullptr;
}

bool IsFunctionType(SgType *type) {
  type = StripPointerType(type);
  return isSgFunctionType(type) != nullptr ||
         isSgMemberFunctionType(type) != nullptr;
}

bool KindSelectorHasStar(const std::optional<parser::KindSelector> &kind) {
  if (!kind) {
    return false;
  }
  return std::holds_alternative<parser::KindSelector::StarSize>(kind->u);
}

SgFunctionType *
BuildProcedureInterfaceType(std::optional<parser::ProcInterface> &optInterface,
                            SgScopeStatement *scope) {
  auto buildFunctionType = [](SgType *returnType) {
    SgFunctionParameterTypeList *typeList = new SgFunctionParameterTypeList();
    ASSERT_not_null(typeList);
    return SageBuilder::buildFunctionType(returnType, typeList);
  };

  SgFunctionType *procType{nullptr};
  if (optInterface) {
    common::visit(common::visitors{
                      [&](parser::Name &ifaceName) {
                        SgFunctionSymbol *sym =
                            SageInterface::lookupFunctionSymbolInParentScopes(
                                ifaceName.ToString(), scope);
                        if (sym != nullptr) {
                          procType = isSgFunctionType(sym->get_type());
                        }
                        if (procType == nullptr) {
                          procType =
                              buildFunctionType(SageBuilder::buildVoidType());
                        }
                      },
                      [&](parser::DeclarationTypeSpec &spec) {
                        SgType *returnType{nullptr};
                        Rose::builder::Build(spec, returnType);
                        if (returnType == nullptr) {
                          returnType = SageBuilder::buildUnknownType();
                        }
                        procType = buildFunctionType(returnType);
                      }},
                  optInterface->u);
  }

  if (procType == nullptr) {
    procType = buildFunctionType(SageBuilder::buildVoidType());
  }
  return procType;
}

bool IsFortranSpecificationStatement(const SgStatement *stmt) {
  return isSgDeclarationStatement(stmt) != nullptr ||
         isSgUseStatement(stmt) != nullptr || isSgImplicitStatement(stmt) ||
         isSgAttributeSpecificationStatement(stmt) != nullptr;
}

SgScopeStatement *FindFortranImplicitDeclScope(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }

  if (SgFunctionDefinition *funcDef =
          SageInterface::getEnclosingFunctionDefinition(scope, true)) {
    if (SgBasicBlock *body = funcDef->get_body()) {
      return body;
    }
  }

  if (SgBasicBlock *block = isSgBasicBlock(scope)) {
    SgNode *parent = block->get_parent();
    if (parent != nullptr && isSgScopeStatement(parent) != nullptr &&
        isSgFunctionDefinition(parent) == nullptr) {
      return block;
    }
  }

  if (SgModuleStatement *moduleStmt =
          SageInterface::getEnclosingModuleStatement(scope, true)) {
    if (SgClassDefinition *moduleDef = moduleStmt->get_definition()) {
      return moduleDef;
    }
  }

  return scope;
}

void InsertFortranSpecificationStatement(SgStatement *stmt,
                                         SgScopeStatement *scope) {
  ASSERT_not_null(stmt);
  ASSERT_not_null(scope);

  for (SgStatement *scopeStmt : scope->generateStatementList()) {
    if (!IsFortranSpecificationStatement(scopeStmt)) {
      SageInterface::insertStatementBefore(scopeStmt, stmt);
      return;
    }
  }
  SageInterface::appendStatement(stmt, scope);
}

void HoistInlineIfSpecStatements(SgIfStmt *ifNode, SgBasicBlock *trueBody) {
  if (ifNode == nullptr || trueBody == nullptr) {
    return;
  }
  if (ifNode->get_use_then_keyword() || ifNode->get_has_end_statement()) {
    return;
  }
  if (!SageInterface::is_Fortran_language()) {
    return;
  }

  const SgStatementPtrList &stmts = trueBody->get_statements();
  if (stmts.size() <= 1) {
    return;
  }

  SgScopeStatement *implicitScope = FindFortranImplicitDeclScope(trueBody);
  if (implicitScope == nullptr) {
    implicitScope = SageBuilder::topScopeStack();
  }
  ASSERT_not_null(implicitScope);

  std::vector<SgStatement *> toMove{};
  for (SgStatement *stmt : stmts) {
    if (IsFortranSpecificationStatement(stmt)) {
      toMove.push_back(stmt);
    }
  }

  for (SgStatement *stmt : toMove) {
    SageInterface::removeStatement(stmt);
    if (SgDeclarationStatement *decl = isSgDeclarationStatement(stmt)) {
      decl->set_scope(implicitScope);
    }
    if (SgVariableDeclaration *varDecl = isSgVariableDeclaration(stmt)) {
      SageInterface::fixVariableDeclaration(varDecl, implicitScope);
    }
    stmt->set_parent(implicitScope);
    InsertFortranSpecificationStatement(stmt, implicitScope);
  }
}

SgType *GetExprTypeForDisambiguation(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }
  if (expr->get_type() != nullptr) {
    return expr->get_type();
  }
  if (auto *varRef = isSgVarRefExp(expr)) {
    if (SgVariableSymbol *sym = varRef->get_symbol()) {
      return sym->get_type();
    }
  }
  if (auto *dot = isSgDotExp(expr)) {
    return GetExprTypeForDisambiguation(dot->get_rhs_operand_i());
  }
  return nullptr;
}

bool IsArrayDesignator(SgExpression *expr) {
  return IsArrayType(GetExprTypeForDisambiguation(expr));
}

SgExpression *BuildArrayRefFromCallArgs(SgExpression *base,
                                        SgExprListExp *args) {
  ASSERT_not_null(base);
  ASSERT_not_null(args);
  return SageBuilderCpp17::buildPntrArrRefExp_nfi(base, args);
}
} // namespace

void Build(parser::ComponentDecl &x, std::list<EntityDeclTuple> &componentDecls,
           SgType *baseType);
void Build(parser::ComponentDecl &x, std::string &name, SgExpression *&init,
           SgType *&type, SgType *baseType);
void Build(parser::DataComponentDefStmt &x, SgStatement *&stmt);
void Build(parser::DataStmtObject &x, SgExpression *&expr);
void Build(parser::DataIDoObject &x, SgExpression *&expr);
void Build(parser::DataImpliedDo &x, SgExpression *&expr);
void Build(parser::DataStmtValue &x, SgExpression *&expr);
void Build(parser::DataStmtConstant &x, SgExpression *&expr);
void Build(parser::DataStmtRepeat &x, SgExpression *&expr);
void getComponentAttrSpec(
    parser::ComponentAttrSpec &x,
    std::list<LanguageTranslation::ExpressionKind> &modifiers,
    SgType *&baseType);

template <typename T>
void BuildExprVisitor::BuildExpressions(T &x, SgExpression *&lhs,
                                        SgExpression *&rhs) {
  WalkExpr(std::get<0>(x.t).value(), lhs); // lhs Expr
  WalkExpr(std::get<1>(x.t).value(), rhs); // rhs Expr
}

// Name
void BuildExprVisitor::Build(Fortran::parser::Name &x) {
  std::string name{x.ToString()};
  SgExpression *expr = nullptr;
  if (SgScopeStatement *scope = SageBuilder::topScopeStack()) {
    if (SgVariableSymbol *symbol =
            SageInterface::lookupVariableSymbolInParentScopes(name, scope)) {
      expr = SageBuilder::buildVarRefExp(symbol);
    }
  }
  if (expr == nullptr) {
    expr = SageBuilderCpp17::buildVarRefExp_nfi(name);
  }
  this->set(expr);
}

void BuildExprVisitor::Build(Fortran::parser::Designator &x) {
  SgExpression *expr{nullptr};
  Rose::builder::Build(x, expr);
  ASSERT_not_null(expr);
  this->set(expr);
}

void BuildExprVisitor::Build(Fortran::parser::Substring &x) {
  SgExpression *expr{nullptr};
  Rose::builder::Build(x, expr);
  ASSERT_not_null(expr);
  this->set(expr);
}

void BuildExprVisitor::Build(Fortran::parser::CharLiteralConstantSubstring &x) {
  SgExpression *expr{nullptr};
  Rose::builder::Build(x, expr);
  ASSERT_not_null(expr);
  this->set(expr);
}

void BuildExprVisitor::Build(Fortran::parser::SubstringInquiry &x) {
  SgExpression *expr{nullptr};
  Rose::builder::Build(x, expr);
  ASSERT_not_null(expr);
  this->set(expr);
}

void BuildExprVisitor::Build(Fortran::parser::StructureConstructor &x) {
  SgExpression *expr{nullptr};
  Rose::builder::Build(x, expr);
  ASSERT_not_null(expr);
  this->set(expr);
}

void BuildExprVisitor::Build(Fortran::parser::Expr::DefinedUnary &x) {
  SgExpression *arg{nullptr};
  WalkExpr(std::get<1>(x.t).value(), arg);
  ASSERT_not_null(arg);
  std::string name{std::get<0>(x.t).v.ToString()};
  std::list<SgExpression *> args{arg};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp(
      SgName(name), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(call);
  this->set(call);
}

void BuildExprVisitor::Build(Fortran::parser::Expr::DefinedBinary &x) {
  SgExpression *lhs{nullptr};
  SgExpression *rhs{nullptr};
  WalkExpr(std::get<1>(x.t).value(), lhs);
  WalkExpr(std::get<2>(x.t).value(), rhs);
  ASSERT_not_null(lhs);
  ASSERT_not_null(rhs);
  std::string name{std::get<0>(x.t).v.ToString()};
  std::list<SgExpression *> args{lhs, rhs};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp(
      SgName(name), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(call);
  this->set(call);
}

void BuildExprVisitor::Build(Fortran::parser::Expr::ComplexConstructor &x) {
  SgExpression *lhs{nullptr};
  SgExpression *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  ASSERT_not_null(lhs);
  ASSERT_not_null(rhs);
  std::list<SgExpression *> args{lhs, rhs};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp(
      SgName("cmplx"), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(call);
  this->set(call);
}

void BuildExprVisitor::Build(Fortran::parser::Expr::Parentheses &x) {
  SgExpression *operand{nullptr};
  WalkExpr(x.v.value(), operand);
  ASSERT_not_null(operand);
  SageBuilderCpp17::set_need_paren(operand);
  this->set(operand);
}

void BuildExprVisitor::Build(Fortran::parser::Expr::PercentLoc &x) {
  SgExpression *arg{nullptr};
  WalkExpr(x.v.value(), arg);
  ASSERT_not_null(arg);
  std::list<SgExpression *> args{arg};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp(
      SgName("loc"), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(call);
  this->set(call);
}

void BuildExprVisitor::Build(Fortran::parser::FunctionReference &x) {
  std::list<SgExpression *> arg_list;
  std::string func_name;

  SgExpression *designator{nullptr};
  Rose::builder::Build(x.v, arg_list, func_name, designator); // Call

  SgExprListExp *param_list = SageBuilderCpp17::buildExprListExp_nfi(arg_list);
  ASSERT_not_null(param_list);

  SgExpression *call_expr = nullptr;
  if (designator != nullptr) {
    if (IsArrayDesignator(designator)) {
      call_expr = BuildArrayRefFromCallArgs(designator, param_list);
    } else {
      call_expr = SageBuilder::buildFunctionCallExp_nfi(designator, param_list);
    }
  } else {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    ASSERT_not_null(scope);
    if (SgVariableSymbol *var_sym =
            SageInterface::lookupVariableSymbolInParentScopes(func_name,
                                                              scope)) {
      SgType *var_type = var_sym->get_type();
      if (IsArrayType(var_type)) {
        SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
        call_expr = BuildArrayRefFromCallArgs(var_ref, param_list);
      } else if (IsFunctionType(var_type)) {
        SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
        call_expr = SageBuilder::buildFunctionCallExp_nfi(var_ref, param_list);
      }
    }

    if (call_expr == nullptr) {
      call_expr =
          BuildFunctionCallFromSymbolIfFound(func_name, scope, param_list);
    }

    if (call_expr == nullptr) {
      call_expr = SageBuilder::buildFunctionCallExp(
          SgName(func_name), SageBuilder::buildUnknownType(), param_list,
          SageBuilder::topScopeStack());
    }
  }
  ASSERT_not_null(call_expr);

  this->set(call_expr);
}

void BuildExprVisitor::Build(Fortran::parser::ArrayConstructor &x) {
  SgExpression *expr{nullptr};
  Rose::builder::Build(x, expr);
  ASSERT_not_null(expr);
  this->set(expr);
}

//-------------------------------------------

// The Build functions need to be turned into a class (global variable used for
// now)

// This constructor is temporary until ROSE supports C++17
// WARNING, requires that setSgSourceFile be called (see below) before fully
// constructed
SageTreeBuilder builder{SageTreeBuilder::LanguageEnum::Fortran};
// TEMPORARY until C++17
void setSgSourceFile(SgSourceFile *sg_file) { builder.setSourceFile(sg_file); }

// TODO: change this to a reference
Fortran::parser::AllCookedSources *cooked_{nullptr};

namespace {
class CookedSourcesGuard {
public:
  CookedSourcesGuard(Fortran::parser::AllCookedSources *&slot,
                     Fortran::parser::AllCookedSources *next)
      : slot_(slot), prev_(slot) {
    slot_ = next;
  }
  CookedSourcesGuard(const CookedSourcesGuard &) = delete;
  CookedSourcesGuard &operator=(const CookedSourcesGuard &) = delete;
  ~CookedSourcesGuard() { slot_ = prev_; }

private:
  Fortran::parser::AllCookedSources *&slot_;
  Fortran::parser::AllCookedSources *prev_;
};

bool IsFixedFormSource(const SgSourceFile *source) {
  if (source == nullptr) {
    return false;
  }
  if (source->get_inputFormat() == SgFile::e_fixed_form_output_format) {
    return true;
  }
  if (source->get_inputFormat() == SgFile::e_unknown_output_format &&
      source->get_F77_only()) {
    return true;
  }
  return false;
}

std::string NormalizeSourcePath(const std::string &path) {
  SgSourceFile *source = builder.getSourceFile();
  if (source == nullptr) {
    return path;
  }
  if (source->get_experimental_flang_frontend()) {
    std::string filename = source->get_sourceFileNameWithPath();
    if (filename.empty()) {
      filename = source->getFileName();
    }
    if (!filename.empty()) {
      return filename;
    }
  }
  if (!source->get_requires_C_preprocessor()) {
    return path;
  }
  const std::string expected = source->getFileName();
  if (expected.empty()) {
    return path;
  }
  return expected;
}

bool IsAllWhitespace(const std::string &text) {
  for (char ch : text) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

void TrimLeft(std::string &text) {
  size_t pos = 0;
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  text.erase(0, pos);
}

bool StartsWithOmp(const std::string &text) {
  if (text.size() < 3) {
    return false;
  }
  return std::tolower(static_cast<unsigned char>(text[0])) == 'o' &&
         std::tolower(static_cast<unsigned char>(text[1])) == 'm' &&
         std::tolower(static_cast<unsigned char>(text[2])) == 'p';
}

bool StartsWithAcc(const std::string &text) {
  if (text.size() < 3) {
    return false;
  }
  return std::tolower(static_cast<unsigned char>(text[0])) == 'a' &&
         std::tolower(static_cast<unsigned char>(text[1])) == 'c' &&
         std::tolower(static_cast<unsigned char>(text[2])) == 'c';
}

size_t FindFortranDirectivePayloadStart(const std::string &text) {
  if (text.empty()) {
    return std::string::npos;
  }
  const char marker =
      static_cast<char>(std::tolower(static_cast<unsigned char>(text.front())));
  if (marker == '!' || marker == 'c' || marker == 'd' || marker == '*') {
    size_t next = 1;
    while (next < text.size() &&
           std::isspace(static_cast<unsigned char>(text[next]))) {
      ++next;
    }
    if (next < text.size() && text[next] == '$') {
      ++next;
      while (next < text.size() &&
             std::isspace(static_cast<unsigned char>(text[next]))) {
        ++next;
      }
      return next;
    }
  }
  if (text.front() == '$') {
    size_t next = 1;
    while (next < text.size() &&
           std::isspace(static_cast<unsigned char>(text[next]))) {
      ++next;
    }
    return next;
  }
  return std::string::npos;
}

bool IsOpenMpOrAccDirectiveLine(const std::string &line) {
  std::string trimmed = line;
  TrimLeft(trimmed);
  if (trimmed.empty()) {
    return false;
  }
  std::string lower = trimmed;
  for (char &ch : lower) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  const size_t payload = FindFortranDirectivePayloadStart(lower);
  if (payload != std::string::npos) {
    if (lower.compare(payload, 3, "omp") == 0 ||
        lower.compare(payload, 3, "acc") == 0) {
      return true;
    }
  }
  if (StartsWithOmp(lower) || StartsWithAcc(lower)) {
    return true;
  }
  return false;
}

std::string NormalizeOmpDirectiveLine(const std::string &line) {
  std::string text = line;
  TrimLeft(text);
  if (text.empty()) {
    return text;
  }
  bool had_sentinel = false;
  const size_t payload = FindFortranDirectivePayloadStart(text);
  if (payload != std::string::npos) {
    had_sentinel = true;
    text.erase(0, payload);
    TrimLeft(text);
  }
  if (text.empty() || StartsWithOmp(text) || StartsWithAcc(text)) {
    return text;
  }
  if (had_sentinel) {
    return std::string("omp ") + text;
  }
  return text;
}

bool IsOpenMpOrAccCommentText(const std::string &text) {
  std::string trimmed = text;
  TrimLeft(trimmed);
  if (trimmed.size() < 4 || trimmed.front() != '$') {
    return false;
  }
  const char first =
      static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[1])));
  const char second =
      static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[2])));
  const char third =
      static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[3])));
  if (first == 'o' && second == 'm' && third == 'p') {
    return true;
  }
  if (first == 'a' && second == 'c' && third == 'c') {
    return true;
  }
  return false;
}

bool GetSourceRangeForCharBlock(const Fortran::parser::CharBlock &source,
                                SourcePosition &start, SourcePosition &end) {
  if (cooked_ == nullptr || source.empty()) {
    return false;
  }
  if (auto sourceInfo{cooked_->GetSourcePositionRange(source)}) {
    start = SourcePosition{NormalizeSourcePath(sourceInfo->first.path),
                           sourceInfo->first.line, sourceInfo->first.column};
    end = SourcePosition{NormalizeSourcePath(sourceInfo->second.path),
                         sourceInfo->second.line, sourceInfo->second.column};
    return true;
  }
  return false;
}

struct OmpPragmaLine {
  int line;
  int column;
  size_t length;
  std::string text;
};

std::vector<OmpPragmaLine>
CollectOmpPragmaLines(const std::string &directive,
                      const SourcePosition &startPos) {
  std::vector<OmpPragmaLine> lines;
  int line = startPos.line;
  int column = startPos.column;
  size_t line_start = 0;
  bool emitted = false;
  while (line_start < directive.size()) {
    size_t line_end = directive.find('\n', line_start);
    const bool has_newline = line_end != std::string::npos;
    const size_t line_len =
        has_newline ? (line_end - line_start) : (directive.size() - line_start);
    std::string line_text = directive.substr(line_start, line_len);
    if (!line_text.empty() && line_text.back() == '\r') {
      line_text.pop_back();
    }
    if (!line_text.empty() && !IsAllWhitespace(line_text)) {
      std::string trimmed = line_text;
      TrimLeft(trimmed);
      bool is_continuation =
          !trimmed.empty() && trimmed.front() == '&' && emitted;
      bool is_directive_line =
          IsOpenMpOrAccDirectiveLine(line_text) || is_continuation;
      if (is_directive_line || !emitted) {
        lines.push_back(OmpPragmaLine{line, column, line_text.size(),
                                      NormalizeOmpDirectiveLine(line_text)});
        emitted = true;
      }
    }
    if (!has_newline) {
      break;
    }
    line_start = line_end + 1;
    ++line;
    column = 1;
  }
  return lines;
}

void AppendPragmasFromCharBlock(const Fortran::parser::CharBlock &source) {
  if (source.empty()) {
    return;
  }
  std::string directive = source.ToString();
  if (directive.empty()) {
    return;
  }
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  SourcePosition startPos{};
  SourcePosition endPos{};
  const bool haveRange = GetSourceRangeForCharBlock(source, startPos, endPos);
  static_cast<void>(endPos);
  if (!haveRange) {
    std::string pragma_text = NormalizeOmpDirectiveLine(directive);
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration(pragma_text, scope);
    ASSERT_not_null(pragma);
    SageInterface::setSourcePosition(pragma);
    SageInterface::appendStatement(pragma, scope);
    return;
  }

  std::vector<OmpPragmaLine> lines = CollectOmpPragmaLines(directive, startPos);
  for (const auto &line_info : lines) {
    SgPragmaDeclaration *pragma =
        SageBuilder::buildPragmaDeclaration(line_info.text, scope);
    ASSERT_not_null(pragma);
    SourcePosition line_start_pos{startPos.path, line_info.line,
                                  line_info.column};
    SourcePosition line_end_pos{startPos.path, line_info.line,
                                line_info.column +
                                    static_cast<int>(line_info.length)};
    builder.setSourcePosition(pragma, line_start_pos, line_end_pos);
    SageInterface::appendStatement(pragma, scope);
  }
}

bool ScopeHasPragmaAtSource(SgScopeStatement *scope,
                            const SourcePosition &startPos,
                            const std::string &pragmaText) {
  if (scope == nullptr) {
    return false;
  }
  for (SgStatement *stmt : scope->generateStatementList()) {
    auto *pragmaStmt = isSgPragmaDeclaration(stmt);
    if (pragmaStmt == nullptr) {
      continue;
    }
    Sg_File_Info *info = pragmaStmt->get_file_info();
    if (info == nullptr || info->get_line() != startPos.line) {
      continue;
    }
    SgPragma *pragma = pragmaStmt->get_pragma();
    if (pragma != nullptr && pragma->get_pragma() == pragmaText) {
      return true;
    }
  }
  return false;
}

void AppendPragmasFromCharBlockIfMissing(
    const Fortran::parser::CharBlock &source) {
  if (source.empty()) {
    return;
  }
  std::string directive = source.ToString();
  if (directive.empty()) {
    return;
  }
  SourcePosition startPos{};
  SourcePosition endPos{};
  if (GetSourceRangeForCharBlock(source, startPos, endPos)) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    ASSERT_not_null(scope);
    std::vector<OmpPragmaLine> lines =
        CollectOmpPragmaLines(directive, startPos);
    if (lines.empty()) {
      return;
    }
    for (const auto &line_info : lines) {
      SourcePosition line_pos{startPos.path, line_info.line, line_info.column};
      if (ScopeHasPragmaAtSource(scope, line_pos, line_info.text)) {
        continue;
      }
      SgPragmaDeclaration *pragma =
          SageBuilder::buildPragmaDeclaration(line_info.text, scope);
      ASSERT_not_null(pragma);
      SourcePosition line_start_pos{startPos.path, line_info.line,
                                    line_info.column};
      SourcePosition line_end_pos{startPos.path, line_info.line,
                                  line_info.column +
                                      static_cast<int>(line_info.length)};
      builder.setSourcePosition(pragma, line_start_pos, line_end_pos);
      SageInterface::appendStatement(pragma, scope);
    }
    return;
  }
  AppendPragmasFromCharBlock(source);
}

std::vector<Rose::builder::Token>
CollectFortranCommentTokens(const SgSourceFile *source) {
  std::vector<Rose::builder::Token> tokens;
  if (source == nullptr) {
    return tokens;
  }

  std::string path = source->get_sourceFileNameWithPath();
  if (path.empty()) {
    path = source->getFileName();
  }
  if (path.empty()) {
    return tokens;
  }

  std::ifstream input(path);
  if (!input) {
    return tokens;
  }

  const bool fixed_form = IsFixedFormSource(source);
  std::string line;
  int line_number = 0;

  while (std::getline(input, line)) {
    ++line_number;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    size_t comment_start = std::string::npos;
    if (fixed_form) {
      const char first = line.front();
      if (first == 'c' || first == 'C' || first == '*' || first == '!' ||
          first == 'd' || first == 'D') {
        comment_start = 0;
      }
    }

    if (comment_start == std::string::npos) {
      bool in_string = false;
      char quote = '\0';
      const size_t scan_start =
          fixed_form ? std::min<size_t>(6, line.size()) : 0;
      for (size_t i = scan_start; i < line.size(); ++i) {
        const char ch = line[i];
        if (in_string) {
          if (ch == quote) {
            if (i + 1 < line.size() && line[i + 1] == quote) {
              ++i;
            } else {
              in_string = false;
              quote = '\0';
            }
          }
          continue;
        }
        if (ch == '\'' || ch == '"') {
          in_string = true;
          quote = ch;
          continue;
        }
        if (ch == '!') {
          comment_start = i;
          break;
        }
      }
    }

    if (comment_start == std::string::npos) {
      continue;
    }

    const size_t content_start = comment_start + 1;
    if (content_start > line.size()) {
      continue;
    }
    const std::string comment_text = line.substr(content_start);
    const int start_col = static_cast<int>(comment_start) + 1;
    const int end_col = static_cast<int>(line.size());

    std::vector<std::string> row(6);
    row[0] =
        std::to_string(static_cast<int>(Rose::builder::TokenKind::comment));
    row[1] = std::to_string(line_number);
    row[2] = std::to_string(start_col);
    row[3] = std::to_string(line_number);
    row[4] = std::to_string(end_col);
    row[5] = comment_text;
    tokens.emplace_back(row);
  }

  return tokens;
}

void InitializeCommentTokens() {
  SgSourceFile *source = builder.getSourceFile();
  if (source == nullptr) {
    return;
  }
  builder.setTokens(CollectFortranCommentTokens(source));
}

void SetFortranEndLabelReference(SgStatement *stmt, int label_value,
                                 SgScopeStatement *label_scope) {
  ROSE_ASSERT(stmt != nullptr);
  ROSE_ASSERT(label_value > 0 && label_value <= 99999);

  if (label_scope == nullptr) {
    label_scope = SageInterface::getEnclosingFunctionDefinition(stmt);
  }
  ROSE_ASSERT(label_scope != nullptr);

  SgName label_name(StringUtility::numberToString(label_value));
  SgLabelSymbol *symbol = label_scope->lookup_label_symbol(label_name);
  if (symbol == nullptr) {
    symbol = new SgLabelSymbol(static_cast<SgLabelStatement *>(nullptr));
    ROSE_ASSERT(symbol != nullptr);
    SgNullStatement *placeholder = SageBuilder::buildNullStatement();
    ROSE_ASSERT(placeholder != nullptr);
    placeholder->set_parent(label_scope);
    symbol->set_fortran_statement(placeholder);
    label_scope->insert_symbol(label_name, symbol);
  }

  SgStatement *existing_stmt = symbol->get_fortran_statement();
  if (existing_stmt == nullptr || isSgNullStatement(existing_stmt) != nullptr) {
    symbol->set_fortran_statement(stmt);
  }
  symbol->set_label_type(SgLabelSymbol::e_end_label_type);

  const int numeric_value = symbol->get_numeric_label_value();
  if (numeric_value <= 0) {
    symbol->set_numeric_label_value(label_value);
  } else if (numeric_value != label_value) {
    std::cerr << "Error. Fortran label value mismatch for " << label_name
              << " (" << numeric_value << " vs " << label_value << ")\n";
    ROSE_ABORT();
  }

  SgLabelRefExp *ref_exp = SageBuilder::buildLabelRefExp(symbol);
  ROSE_ASSERT(ref_exp != nullptr);
  ref_exp->set_parent(stmt);
  stmt->set_end_numeric_label(ref_exp);
}

SgLabelSymbol *GetOrCreateFortranLabelSymbol(int label_value,
                                             SgScopeStatement *label_scope) {
  ROSE_ASSERT(label_scope != nullptr);
  ROSE_ASSERT(label_value > 0 && label_value <= 99999);

  SgName label_name(StringUtility::numberToString(label_value));
  SgLabelSymbol *symbol = label_scope->lookup_label_symbol(label_name);
  if (symbol == nullptr) {
    symbol = new SgLabelSymbol(static_cast<SgLabelStatement *>(nullptr));
    ROSE_ASSERT(symbol != nullptr);
    SgNullStatement *placeholder = SageBuilder::buildNullStatement();
    ROSE_ASSERT(placeholder != nullptr);
    placeholder->set_parent(label_scope);
    symbol->set_fortran_statement(placeholder);
    label_scope->insert_symbol(label_name, symbol);
  }

  const int numeric_value = symbol->get_numeric_label_value();
  if (numeric_value <= 0) {
    symbol->set_numeric_label_value(label_value);
  } else if (numeric_value != label_value) {
    std::cerr << "Error. Fortran label value mismatch for " << label_name
              << " (" << numeric_value << " vs " << label_value << ")\n";
    ROSE_ABORT();
  }

  return symbol;
}
} // namespace

template <typename T>
SourcePosition BuildSourcePosition(const Fortran::parser::Statement<T> &x,
                                   Order from) {
  std::optional<SourcePosition> pos{std::nullopt};

  if (cooked_ == nullptr || x.source.empty()) {
    pos.emplace(SourcePosition{});
  } else if (auto sourceInfo{cooked_->GetSourcePositionRange(x.source)}) {
    const std::string startPath = NormalizeSourcePath(sourceInfo->first.path);
    const std::string endPath = NormalizeSourcePath(sourceInfo->second.path);
    if (from == Order::begin)
      pos.emplace(SourcePosition{startPath, sourceInfo->first.line,
                                 sourceInfo->first.column});
    else
      pos.emplace(SourcePosition{endPath, sourceInfo->second.line,
                                 sourceInfo->second.column});
  } else {
    pos.emplace(SourcePosition{});
  }

  return pos.value();
}

template <typename T>
SourcePosition
BuildSourcePosition(const Fortran::parser::UnlabeledStatement<T> &x,
                    Order from) {
  std::optional<SourcePosition> pos{std::nullopt};

  if (cooked_ == nullptr || x.source.empty()) {
    pos.emplace(SourcePosition{});
  } else if (auto sourceInfo{cooked_->GetSourcePositionRange(x.source)}) {
    const std::string startPath = NormalizeSourcePath(sourceInfo->first.path);
    const std::string endPath = NormalizeSourcePath(sourceInfo->second.path);
    if (from == Order::begin)
      pos.emplace(SourcePosition{startPath, sourceInfo->first.line,
                                 sourceInfo->first.column});
    else
      pos.emplace(SourcePosition{endPath, sourceInfo->second.line,
                                 sourceInfo->second.column});
  } else {
    pos.emplace(SourcePosition{});
  }

  return pos.value();
}

template <typename T>
std::optional<SourcePosition>
BuildSourcePosition(const std::optional<Fortran::parser::Statement<T>> &opt,
                    Order from) {
  std::optional<SourcePosition> pos{std::nullopt};

  if (opt)
    pos.emplace(BuildSourcePosition(*opt, from));

  return pos;
}

template <typename T>
std::optional<SourcePosition> BuildSourcePosition(const std::variant<T> &u,
                                                  Order from) {
  // TODO
  return std::nullopt;
}

template <typename T>
std::optional<SourcePosition>
TryBuildStatementPosition(const Fortran::parser::Statement<T> &stmt) {
  return BuildSourcePosition(stmt, Order::begin);
}

template <typename T>
std::optional<SourcePosition> TryBuildStatementPosition(const T &) {
  return std::nullopt;
}

std::optional<SourcePosition>
FirstSourcePosition(const parser::SpecificationPart &x) {
  const auto &omp_stmts{std::get<0>(x.t)};
  if (omp_stmts.size() > 0) {
  }

  const auto &use_stmts{std::get<
      std::list<parser::Statement<common::Indirection<parser::UseStmt>>>>(x.t)};
  if (use_stmts.size() > 0) {
    return std::optional<SourcePosition>{
        BuildSourcePosition(use_stmts.front(), Order::begin)};
  }

  const auto &import_stmts{std::get<
      std::list<parser::Statement<common::Indirection<parser::ImportStmt>>>>(
      x.t)};
  if (import_stmts.size() > 0) {
    return std::optional<SourcePosition>{
        BuildSourcePosition(import_stmts.front(), Order::begin)};
  }

  const auto &implicit_part_stmts{std::get<parser::ImplicitPart>(x.t).v};
  if (implicit_part_stmts.size() > 0) {
    for (const auto &implicit_part_stmt : implicit_part_stmts) {
      std::optional<SourcePosition> implicit_pos = std::nullopt;
      common::visit(
          common::visitors{
              [&](const parser::Statement<
                  common::Indirection<parser::ImplicitStmt>> &stmt) {
                implicit_pos = BuildSourcePosition(stmt, Order::begin);
              },
              [&](const parser::Statement<
                  common::Indirection<parser::ParameterStmt>> &stmt) {
                implicit_pos = BuildSourcePosition(stmt, Order::begin);
              },
              [&](const parser::Statement<
                  common::Indirection<parser::OldParameterStmt>> &stmt) {
                implicit_pos = BuildSourcePosition(stmt, Order::begin);
              },
              [&](const parser::Statement<
                  common::Indirection<parser::FormatStmt>> &stmt) {
                implicit_pos = BuildSourcePosition(stmt, Order::begin);
              },
              [&](const parser::Statement<
                  common::Indirection<parser::EntryStmt>> &stmt) {
                implicit_pos = BuildSourcePosition(stmt, Order::begin);
              },
              [&](const auto &) {}},
          implicit_part_stmt.u);
      if (implicit_pos) {
        return implicit_pos;
      }
    }
  }

  const auto &decl_stmts{
      std::get<std::list<parser::DeclarationConstruct>>(x.t)};
  for (const auto &decl_stmt : decl_stmts) {
    std::optional<SourcePosition> decl_pos = std::nullopt;
    common::visit(
        common::visitors{[&](const parser::SpecificationConstruct &spec) {
                           common::visit(
                               common::visitors{[&](const auto &spec_stmt) {
                                 if (decl_pos) {
                                   return;
                                 }
                                 if (auto pos =
                                         TryBuildStatementPosition(spec_stmt)) {
                                   decl_pos = pos;
                                 }
                               }},
                               spec.u);
                         },
                         [&](const auto &stmt) {
                           if (decl_pos) {
                             return;
                           }
                           if (auto pos = TryBuildStatementPosition(stmt)) {
                             decl_pos = pos;
                           }
                         }},
        decl_stmt.u);
    if (decl_pos) {
      return decl_pos;
    }
  }

  return std::optional<SourcePosition>{std::nullopt};
}

namespace {
bool namesMatch(const SgName &left, const SgName &right, bool caseInsensitive) {
  if (!caseInsensitive) {
    return left == right;
  }
  return StringUtility::convertToLowerCase(left.str()) ==
         StringUtility::convertToLowerCase(right.str());
}

bool isPubliclyAccessibleSymbol(SgSymbol *symbol);

using SymbolList = std::vector<SgSymbol *>;
using PublicSymbolMap = std::unordered_map<std::string, SymbolList>;

std::string symbolKey(const SgName &name, bool caseInsensitive) {
  if (!caseInsensitive) {
    return name.str();
  }
  return StringUtility::convertToLowerCase(name.str());
}

std::string formatSourcePosition(const SourcePosition &pos) {
  if (pos.path.empty() && pos.line <= 0 && pos.column <= 0) {
    return {};
  }
  std::ostringstream out;
  if (!pos.path.empty()) {
    out << pos.path;
  }
  if (pos.line > 0) {
    out << ":" << pos.line;
    if (pos.column > 0) {
      out << ":" << pos.column;
    }
  }
  return out.str();
}

std::string formatLocatedNode(const SgLocatedNode *node) {
  if (node == nullptr) {
    return {};
  }
  const Sg_File_Info *info = node->get_startOfConstruct();
  if (info == nullptr) {
    return {};
  }
  std::ostringstream out;
  const std::string &file = info->get_filenameString();
  if (!file.empty()) {
    out << file;
  }
  if (info->get_line() > 0) {
    out << ":" << info->get_line();
    if (info->get_col() > 0) {
      out << ":" << info->get_col();
    }
  }
  return out.str();
}

PublicSymbolMap collectPublicSymbols(SgClassDefinition *classDefinition,
                                     bool caseInsensitive) {
  PublicSymbolMap publicSymbols;
  SgSymbol *symbol = classDefinition->first_any_symbol();
  while (symbol != nullptr) {
    if (isPubliclyAccessibleSymbol(symbol)) {
      publicSymbols[symbolKey(symbol->get_name(), caseInsensitive)].push_back(
          symbol);
    }
    symbol = classDefinition->next_any_symbol();
  }
  return publicSymbols;
}

bool isPubliclyAccessibleSymbol(SgSymbol *symbol) {
  SgNode *symbolBasis = symbol->get_symbol_basis();
  if (auto *declaration = isSgDeclarationStatement(symbolBasis)) {
    auto &access = declaration->get_declarationModifier().get_accessModifier();
    return access.isPublic() || access.isDefault() || access.isUndefined();
  }

  if (auto *initializedName = isSgInitializedName(symbolBasis)) {
    auto *parent = initializedName->get_parent();
    if (auto *declaration = isSgDeclarationStatement(parent)) {
      auto &access =
          declaration->get_declarationModifier().get_accessModifier();
      if (access.isPublic() || access.isDefault() || access.isUndefined()) {
        return true;
      }
      if (initializedName->get_protected_declaration()) {
        return false;
      }
    }
  }

  return false;
}

std::string definedOperatorName(const parser::DefinedOpName &opName) {
  return opName.v.ToString();
}

std::string definedOperatorSpec(const parser::DefinedOpName &opName) {
  return "OPERATOR(" + definedOperatorName(opName) + ")";
}

SgRenamePair *buildRenamePair(const parser::Rename &rename) {
  SgRenamePair *renamePair{nullptr};
  common::visit(
      common::visitors{[&](const parser::Rename::Names &names) {
                         std::string localName{std::get<0>(names.t).ToString()};
                         std::string useName{std::get<1>(names.t).ToString()};
                         renamePair = new SgRenamePair(localName, useName);
                       },
                       [&](const parser::Rename::Operators &ops) {
                         std::string localName =
                             definedOperatorSpec(std::get<0>(ops.t));
                         std::string useName =
                             definedOperatorSpec(std::get<1>(ops.t));
                         renamePair = new SgRenamePair(localName, useName);
                       }},
      rename.u);
  ASSERT_not_null(renamePair);
  SageInterface::setSourcePosition(renamePair);
  return renamePair;
}

SgRenamePair *buildOnlyRenamePair(const parser::Only &only) {
  SgRenamePair *renamePair{nullptr};
  common::visit(
      common::visitors{
          [&](const parser::Name &name) {
            std::string useName{name.ToString()};
            renamePair = new SgRenamePair(useName, useName);
          },
          [&](const parser::Rename &rename) {
            renamePair = buildRenamePair(rename);
          },
          [&](const common::Indirection<parser::GenericSpec> &genericSpec) {
            std::string specText{genericSpec.value().source.ToString()};
            renamePair = new SgRenamePair(specText, specText);
          }},
      only.u);
  ASSERT_not_null(renamePair);
  SageInterface::setSourcePosition(renamePair);
  return renamePair;
}

SgModuleStatement *lookupModuleStatement(const std::string &moduleName) {
  SgClassSymbol *moduleSymbol =
      SageInterface::lookupClassSymbolInParentScopes(moduleName);
  if (moduleSymbol == nullptr) {
    return nullptr;
  }

  SgClassDeclaration *decl = moduleSymbol->get_declaration();
  if (decl == nullptr) {
    return nullptr;
  }

  SgClassDeclaration *definingDecl =
      isSgClassDeclaration(decl->get_definingDeclaration());
  if (definingDecl == nullptr) {
    definingDecl = isSgClassDeclaration(decl);
  }

  return isSgModuleStatement(definingDecl);
}

SgClassSymbol *lookupDerivedTypeSymbol(const SgName &derivedTypeName,
                                       SgScopeStatement *currentScope) {
  SgClassSymbol *derivedTypeSymbol{nullptr};
  SgScopeStatement *scope{currentScope};
  while (derivedTypeSymbol == nullptr && scope != nullptr) {
    derivedTypeSymbol = scope->lookup_class_symbol(derivedTypeName);
    scope = isSgGlobal(scope) ? nullptr : scope->get_scope();
  }

  return derivedTypeSymbol;
}

void replace_return_type(SgFunctionType *functionType,
                         SgFunctionDeclaration *functionDeclaration,
                         SgClassSymbol *derivedTypeSymbol) {
  bool hasEllipses = functionType->get_has_ellipses();
  ROSE_ASSERT(derivedTypeSymbol->get_declaration() != nullptr);
  ROSE_ASSERT(derivedTypeSymbol->get_declaration()->get_type() != nullptr);

  SgFunctionType *newFunctionType = new SgFunctionType(
      derivedTypeSymbol->get_declaration()->get_type(), hasEllipses);
  ROSE_ASSERT(functionType->get_argument_list() != nullptr);

  SgTypePtrList &functionArgumentTypeList = functionType->get_arguments();
  for (auto *argType : functionArgumentTypeList) {
    newFunctionType->append_argument(argType);
  }

  functionDeclaration->set_type(newFunctionType);

  SgFunctionSymbol *newFunctionSymbol =
      new SgFunctionSymbol(functionDeclaration);
  functionDeclaration->get_scope()->insert_symbol(
      functionDeclaration->get_name(), newFunctionSymbol);
}

void fixup_possible_incomplete_function_return_type(
    SgScopeStatement *currentScope) {
  SgFunctionDeclaration *functionDeclaration =
      SageInterface::getEnclosingFunctionDeclaration(currentScope, true);
  if (functionDeclaration == nullptr) {
    return;
  }

  SgFunctionType *functionType =
      isSgFunctionType(functionDeclaration->get_type());
  ROSE_ASSERT(functionType != nullptr);

  SgType *returnType = functionType->get_return_type();
  ROSE_ASSERT(returnType != nullptr);

  if (auto *defaultType = isSgTypeDefault(returnType)) {
    SgName derivedTypeName = defaultType->get_name();
    SgClassSymbol *derivedTypeSymbol =
        lookupDerivedTypeSymbol(derivedTypeName, currentScope);
    if (derivedTypeSymbol != nullptr) {
      replace_return_type(functionType, functionDeclaration, derivedTypeSymbol);
    }
  } else if (auto *classType = isSgClassType(returnType)) {
    SgName derivedTypeName = classType->get_name();
    SgClassSymbol *derivedTypeSymbol =
        lookupDerivedTypeSymbol(derivedTypeName, currentScope);
    replace_return_type(functionType, functionDeclaration, derivedTypeSymbol);
  }
}

void use_statement_fixup(SgScopeStatement *currentScope) {
  fixup_possible_incomplete_function_return_type(currentScope);
}
} // namespace

// Callback for the Flang parser. Converts a parsed
// Fortran::Parser::Program to ROSE Sage nodes.
void Build(parser::Program &x, parser::AllCookedSources &cooked) {
  BuildVisitor visitor{cooked};

  // TODO: make go away
  CookedSourcesGuard cooked_guard{cooked_, &cooked};
  // TODO: make go away
  common::LangOptions langOpts{};

  InitializeCommentTokens();

#if DEBUG_FLANG_UNPARSE
  parser::Encoding encoding{Fortran::parser::Encoding::LATIN_1};
  parser::Unparse(llvm::outs(), x, langOpts, encoding, true /*capitalize*/,
                  false, nullptr, cooked_);
#endif

  // Initialize SageBuilder global scope
  SgScopeStatement *scope{nullptr};
  builder.Enter(scope);

  // Start by building ProgramUnit(s)
  Walk(x.v, visitor);

  // Root of tree, finished
  visitor.Done();

  builder.Leave(scope);
}

// MainProgram
void BuildVisitor::Build(parser::MainProgram &x) {
  // std::tuple<>
  //   std::optional<Statement<ProgramStmt>>, SpecificationPart, ExecutionPart,
  //   std::optional<InternalSubprogramPart>, Statement<EndProgramStmt>>
  using namespace Fortran::parser;

  auto &stmt{std::get<std::optional<Statement<ProgramStmt>>>(x.t)};
  auto &spec{std::get<SpecificationPart>(x.t)};
  auto &end{std::get<Statement<EndProgramStmt>>(x.t)};

  std::vector<std::string> labels{};
  std::optional<SourcePosition> srcPosBody{std::nullopt};
  std::optional<SourcePosition> srcPosBegin{
      BuildSourcePosition(stmt, Order::begin)};
  SourcePosition srcPosEnd{BuildSourcePosition(end, Order::end)};
  std::vector<Rose::builder::Token> comments{};

  std::optional<std::string> name{std::nullopt};

  // ProgramStmt is optional
  if (stmt) {
    name.emplace(stmt.value().statement.v.ToString());
  }
  if (stmt && stmt->label) {
    const int labelValue = static_cast<int>(stmt->label.value());
    if (labelValue > 0) {
      labels.push_back(std::to_string(labelValue));
    }
  }

  if (auto pos{FirstSourcePosition(spec)}) {
    srcPosBody.emplace(*pos);
  }

  // Fortran only needs an end statement so check for no beginning source
  // position
  if (!srcPosBody) {
    if (srcPosBegin) {
      srcPosBody.emplace(*srcPosBegin);
    } else {
      srcPosBody.emplace(srcPosEnd);
    }
  }

  // If there is no ProgramStmt the source begins at the body of the program
  if (!srcPosBegin) {
    srcPosBegin.emplace(*srcPosBody);
  }

  // Build the SgProgramHeaderStatement node
  //
  SgProgramHeaderStatement *programDecl{nullptr};
  std::optional<std::string> opt_name{name};

  builder.Enter(programDecl, opt_name, labels,
                SourcePositions{*srcPosBegin, *srcPosBody, srcPosEnd},
                comments);

  // SpecificationPart, ExecutionPart, and optional InternalSubprogramPart
  Walk(std::get<SpecificationPart>(x.t));
  Walk(std::get<ExecutionPart>(x.t));
  Walk(std::get<std::optional<InternalSubprogramPart>>(x.t));

  // EndProgramStmt
  std::optional<std::string> endName{std::nullopt};
  std::optional<std::string> endLabel{std::nullopt};
  if (end.statement.v) {
    endName = end.statement.v.value().ToString();
  }
  if (end.label) {
    const int labelValue = static_cast<int>(end.label.value());
    if (labelValue > 0) {
      endLabel = std::to_string(labelValue);
    }
  }

  // Fortran specific functionality
  builder.setFortranEndProgramStmt(programDecl, endName, endLabel);
  builder.Leave(programDecl);
}

void DummyArg(std::list<parser::DummyArg> &x, std::list<std::string> &args) {
  // std::variant<> Name, Star
  using namespace Fortran::parser;

  for (const auto &arg : x) {
    common::visit(common::visitors{
                      [&](const Name &y) { args.push_back(y.ToString()); },
                      [&](const Star &y) { args.push_back(std::string("*")); }},
                  arg.u);
  }
}

void getSubroutineStmt(parser::SubroutineStmt &x, std::list<std::string> &args,
                       LanguageTranslation::FunctionModifierList &modifiers,
                       BuildVisitor &visitor) {
  // std::tuple<> std::list<PrefixSpec>, Name, std::list<DummyArg>,
  // std::optional<LanguageBindingSpec>
  using namespace Fortran::parser;

  SgType *prefix_type = nullptr;
  visitor.BuildPrefix(std::get<std::list<parser::PrefixSpec>>(x.t), modifiers,
                      prefix_type);

  // DummyArg list
  DummyArg(std::get<std::list<parser::DummyArg>>(x.t), args);

  if (auto &opt = std::get<std::optional<LanguageBindingSpec>>(x.t)) {
    // WARNING, likely need optional expression (or NullExpressions?)
    // BuildExpr(opt.value(), expr);
    // WalkExpr(opt.value(), expr);
    LanguageTranslation::ExpressionKind m;
    getModifiers(opt.value(), m);
  }
}

void BuildVisitor::Build(parser::InternalSubprogramPart &x) {
  // std::tuple<> Statement<ContainsStmt>, std::list<InternalSubprogram>
  using namespace Fortran::parser;
  Walk(std::get<Statement<ContainsStmt>>(x.t));
  Walk(std::get<std::list<InternalSubprogram>>(x.t));
}

void BuildVisitor::Build(parser::ContainsStmt &x) {
  // ContainsStmt is an empty class
  SgContainsStatement *stmt{nullptr};
  builder.Enter(stmt);
  builder.Leave(stmt);
}

void BuildVisitor::Build(parser::SpecificationPart &x) {
  // std::tuple<> - OmpEndForallDirective, OmpEndParallelDoDirective,
  //                std::list<Statement<UseStmt>>,
  //                std::list<Statement<ImportStmt>>, ImplicitPart,
  //                std::list<DeclarationConstruct>
  Walk(std::get<std::list<parser::OpenACCDeclarativeConstruct>>(x.t));
  Walk(std::get<std::list<parser::OpenMPDeclarativeConstruct>>(x.t));
  Walk(
      std::get<std::list<common::Indirection<parser::CompilerDirective>>>(x.t));
  Walk(std::get<
       std::list<parser::Statement<common::Indirection<parser::UseStmt>>>>(
      x.t));
  auto &importStmts = std::get<
      std::list<parser::Statement<common::Indirection<parser::ImportStmt>>>>(
      x.t);
  for (auto &importStmt : importStmts) {
    Build(importStmt);
  }
  Walk(std::get<parser::ImplicitPart>(x.t));
  Walk(std::get<std::list<parser::DeclarationConstruct>>(x.t));
}

// SubroutineSubprogram
void BuildVisitor::Build(parser::SubroutineSubprogram &x) {
  // std::tuple<> - Statement<SubroutineStmt>, SpecificationPart, ExecutionPart,
  // std::optional<InternalSubprogramPart>,
  //                Statement<EndSubroutineStmt>
  using namespace Fortran::parser;

  auto &stmt{std::get<Statement<SubroutineStmt>>(x.t)};
  auto &end{std::get<Statement<EndSubroutineStmt>>(x.t)};

  std::vector<std::string> labels{};
  std::optional<SourcePosition> srcPosBody{
      FirstSourcePosition(std::get<SpecificationPart>(x.t))};
  std::optional<SourcePosition> srcPosBegin{
      BuildSourcePosition(stmt, Order::begin)};
  SourcePosition srcPosEnd{BuildSourcePosition(end, Order::end)};
  std::vector<Rose::builder::Token> comments{};

  // There need not be any statements
  if (!srcPosBody) {
    if (srcPosBegin) {
      srcPosBody.emplace(*srcPosBegin);
    } else {
      srcPosBody.emplace(srcPosEnd);
    }
  }
  Rose::builder::SourcePositions sources(*srcPosBegin, *srcPosBody, srcPosEnd);

  SgFunctionParameterList *paramList{nullptr};
  SgScopeStatement *paramScope{nullptr};
  SgFunctionDeclaration *funcDecl{nullptr};

  std::list<std::string> dummyArgs;
  LanguageTranslation::FunctionModifierList modifiers;
  getSubroutineStmt(stmt.statement, dummyArgs, modifiers, *this);

  // Enter SageTreeBuilder for SgFunctionParameterList
  bool isDefDecl{true};
  std::string name{std::get<Name>(stmt.statement.t).ToString()};
  builder.Enter(paramList, paramScope, name, /*function_type*/ nullptr,
                isDefDecl);
  DeclareFortranDummyArguments(paramScope, dummyArgs);

  // SpecificationPart
  Walk(std::get<SpecificationPart>(x.t));

  // Leave SageTreeBuilder for SgFunctionParameterList
  builder.Leave(paramList, paramScope, dummyArgs);

  // Begin SageTreeBuilder for SgFunctionDeclaration
  builder.Enter(funcDecl, name, /*return_type*/ nullptr, paramList, modifiers,
                isDefDecl, sources, comments);
  TransferParamScopeToFunctionBody(paramScope, funcDecl);

  // ExecutionPart
  Walk(std::get<ExecutionPart>(x.t));

  // EndSubroutineStmt - std::optional<Name> v;
  bool haveEndStmt{false};
  if (end.statement.v) {
    haveEndStmt = true;
  }

  // InternalSubprogramPart is optional
  Walk(std::get<std::optional<InternalSubprogramPart>>(x.t));

  // Leave SageTreeBuilder for SgFunctionDeclaration
  builder.Leave(funcDecl, paramScope, haveEndStmt);
}

// SeparateModuleSubprogram
void BuildVisitor::Build(parser::SeparateModuleSubprogram &x) {
  // std::tuple<> Statement<MpSubprogramStmt>, SpecificationPart, ExecutionPart,
  //              std::optional<InternalSubprogramPart>,
  //              Statement<EndMpSubprogramStmt>
  using namespace Fortran::parser;

  auto &stmt{std::get<Statement<MpSubprogramStmt>>(x.t)};
  auto &end{std::get<Statement<EndMpSubprogramStmt>>(x.t)};
  auto &spec{std::get<SpecificationPart>(x.t)};

  std::optional<SourcePosition> srcPosBody{FirstSourcePosition(spec)};
  std::optional<SourcePosition> srcPosBegin{
      BuildSourcePosition(stmt, Order::begin)};
  SourcePosition srcPosEnd{BuildSourcePosition(end, Order::end)};
  std::vector<Rose::builder::Token> comments{};

  if (!srcPosBody) {
    if (srcPosBegin) {
      srcPosBody.emplace(*srcPosBegin);
    } else {
      srcPosBody.emplace(srcPosEnd);
    }
  }
  if (!srcPosBegin) {
    srcPosBegin.emplace(*srcPosBody);
  }
  Rose::builder::SourcePositions sources(*srcPosBegin, *srcPosBody, srcPosEnd);

  std::string name = stmt.statement.v.ToString();

  SgFunctionParameterList *paramList{nullptr};
  SgScopeStatement *paramScope{nullptr};
  SgFunctionDeclaration *funcDecl{nullptr};
  LanguageTranslation::FunctionModifierList modifiers;
  bool isDefDecl{true};

  std::list<std::string> dummyArgs;

  SgType *returnType{nullptr};
  std::string lookupName = name;
  BuildFunctionReturnType(spec, lookupName, returnType);
  SgType *paramResultType = returnType;

  // Enter SageTreeBuilder for SgFunctionParameterList
  builder.Enter(paramList, paramScope, name, paramResultType, isDefDecl);

  // SpecificationPart
  Walk(spec);

  // Leave SageTreeBuilder for SgFunctionParameterList
  builder.Leave(paramList, paramScope, dummyArgs);

  // Begin SageTreeBuilder for SgFunctionDeclaration
  builder.Enter(funcDecl, name, returnType, paramList, modifiers, isDefDecl,
                sources, comments);
  TransferParamScopeToFunctionBody(paramScope, funcDecl);

  // ExecutionPart
  Walk(std::get<ExecutionPart>(x.t));

  // EndMpSubprogramStmt - std::optional<Name> v;
  bool haveEndStmt{false};
  if (end.statement.v) {
    haveEndStmt = true;
  }

  // InternalSubprogramPart is optional
  Walk(std::get<std::optional<InternalSubprogramPart>>(x.t));

  // Leave SageTreeBuilder for SgFunctionDeclaration
  if (returnType != nullptr) {
    builder.Leave(funcDecl, paramScope, haveEndStmt, name);
  } else {
    builder.Leave(funcDecl, paramScope, haveEndStmt);
  }
}

// FunctionSubprogram
void BuildVisitor::Build(parser::FunctionSubprogram &x) {
  // std::tuple<> Statement<FunctionStmt>, SpecificationPart, ExecutionPart,
  //              std::optional<InternalSubprogramPart>,
  //              Statement<EndFunctionStmt>
  using namespace Fortran::parser;

  // FunctionStmt - std::tuple<> std::list<PrefixSpec>, Name, std::list<Name>,
  // std::optional<Suffix>
  auto &stmt{std::get<Statement<FunctionStmt>>(x.t)};
  auto &end{std::get<Statement<EndFunctionStmt>>(x.t)};

  std::optional<SourcePosition> srcPosBody{
      FirstSourcePosition(std::get<SpecificationPart>(x.t))};
  std::optional<SourcePosition> srcPosBegin{
      BuildSourcePosition(stmt, Order::begin)};
  SourcePosition srcPosEnd{BuildSourcePosition(end, Order::end)};

  // There need not be any statements
  if (!srcPosBody) {
    if (srcPosBegin) {
      srcPosBody.emplace(*srcPosBegin);
    } else {
      srcPosBody.emplace(srcPosEnd);
    }
  }
  Rose::builder::SourcePositions sources(*srcPosBegin, *srcPosBody, srcPosEnd);

  SgFunctionParameterList *paramList{nullptr};
  SgScopeStatement *paramScope{nullptr};
  SgFunctionDeclaration *functionDecl{nullptr};
  SgType *returnType{nullptr};
  LanguageTranslation::FunctionModifierList modifiers;
  std::vector<Rose::builder::Token> comments{};
  std::string resultName;
  bool isDefDecl{true};

  std::string name{std::get<Name>(stmt.statement.t).ToString()};

  std::list<std::string> dummyArgs;
  for (const auto &arg : std::get<std::list<Name>>(stmt.statement.t)) {
    dummyArgs.push_back(arg.ToString());
  }

  // PrefixSpec
  BuildPrefix(std::get<std::list<parser::PrefixSpec>>(stmt.statement.t),
              modifiers, returnType);

  // Suffix
  bool undeclaredResultName{false};
  auto &suffix{std::get<std::optional<Suffix>>(stmt.statement.t)};
  if (suffix && suffix->resultName) {
    // TODO: LanguageBinding also in suffix
    resultName = suffix->resultName.value().ToString();
  }
  const bool case_insensitive = SageInterface::is_language_case_insensitive();
  const bool useFunctionNameResult =
      resultName.empty() || NamesMatch(resultName, name, case_insensitive);
  if (!resultName.empty() && !useFunctionNameResult && returnType) {
    undeclaredResultName = true;
  }

  // Peek into the SpecificationPart to get the return type if don't already
  // know it
  if (!returnType) {
    std::string lookupName = resultName.empty() ? name : resultName;
    BuildFunctionReturnType(std::get<parser::SpecificationPart>(x.t),
                            lookupName, returnType);
  }

  // Enter SageTreeBuilder for SgFunctionParameterList
  SgType *param_result_type = useFunctionNameResult ? returnType : nullptr;
  builder.Enter(paramList, paramScope, name, param_result_type, isDefDecl);
  DeclareFortranDummyArguments(paramScope, dummyArgs);

  // SpecificationPart
  Walk(std::get<SpecificationPart>(x.t));

  // Need to create initialized name here for result, if result is not declared
  // in SpecificationPart
  if (undeclaredResultName &&
      paramScope->lookup_variable_symbol(resultName) == nullptr) {
    SageBuilderCpp17::fixUndeclaredResultName(resultName, paramScope,
                                              returnType);
  }
  if (resultName.empty() &&
      paramScope->lookup_variable_symbol(name) == nullptr) {
    if (!returnType) {
      returnType = SageBuilder::buildFortranImplicitType(name);
    }
    SageBuilderCpp17::fixUndeclaredResultName(name, paramScope, returnType);
  }
  if (!resultName.empty() &&
      paramScope->lookup_variable_symbol(resultName) == nullptr) {
    if (!returnType) {
      returnType = SageBuilder::buildFortranImplicitType(resultName);
    }
    SageBuilderCpp17::fixUndeclaredResultName(resultName, paramScope,
                                              returnType);
  }

  // Leave SageTreeBuilder for SgFunctionParameterList
  builder.Leave(paramList, paramScope, dummyArgs);

  // Begin SageTreeBuilder for SgFunctionDeclaration
  builder.Enter(functionDecl, name, returnType, paramList, modifiers, isDefDecl,
                sources, comments);
  TransferParamScopeToFunctionBody(paramScope, functionDecl);

  // ExecutionPart
  Walk(std::get<ExecutionPart>(x.t));

  // EndFunctionStmt - std::optional<Name>
  bool haveEndStmt{false};
  if (end.statement.v) {
    haveEndStmt = true;
  }

  // InternalSubprogramPart is optional
  Walk(std::get<std::optional<InternalSubprogramPart>>(x.t));

  // Leave SageTreeBuilder for SgFunctionDeclaration
  builder.Leave(functionDecl, paramScope, haveEndStmt, resultName);
}

// Module
void BuildVisitor::Build(parser::Module &x) {
  // std::tuple<> Statement<ModuleStmt>, SpecificationPart,
  // std::optional<ModuleSubprogramPart>,
  //              Statement<EndModuleStmt>
  using namespace Fortran::parser;

  auto &stmt{std::get<Statement<ModuleStmt>>(x.t)};
  auto &end{std::get<Statement<EndModuleStmt>>(x.t)};

  SgModuleStatement *module{nullptr};
  builder.Enter(module, stmt.statement.v.ToString());

  Walk(std::get<parser::SpecificationPart>(x.t));
  Walk(std::get<std::optional<ModuleSubprogramPart>>(x.t));

  // EndModuleStmt - std::optional<Name> v;
  std::string endName;
  if (end.statement.v) {
    endName = end.statement.v->ToString();
  }

  // Leave SageTreeBuilder for SgModuleStatement
  builder.Leave(module);
}

void BuildVisitor::Build(parser::ModuleSubprogramPart &x) {
  // std::tuple<> Statement<ContainsStmt>, std::list<ModuleSubprogram>

  // ContainsStmt
  SgContainsStatement *contains{nullptr};
  builder.Enter(contains);
  builder.Leave(contains);

  Walk(std::get<std::list<parser::ModuleSubprogram>>(x.t));
}

// Submodule
void BuildVisitor::Build(parser::Submodule &x) {
  // std::tuple<> Statement<SubmoduleStmt>, SpecificationPart,
  //              std::optional<ModuleSubprogramPart>,
  //              Statement<EndSubmoduleStmt>
  using namespace Fortran::parser;

  auto &stmt{std::get<Statement<SubmoduleStmt>>(x.t)};
  auto &parentId = std::get<ParentIdentifier>(stmt.statement.t);
  std::string parentName = std::get<Name>(parentId.t).ToString();
  if (auto &parentMod = std::get<std::optional<Name>>(parentId.t)) {
    parentName += ":" + parentMod->ToString();
  }
  const std::string submoduleName = std::get<Name>(stmt.statement.t).ToString();

  SgModuleStatement *module{nullptr};
  builder.Enter(module, submoduleName);
  module->addNewAttribute(kFortranSubmoduleParentAttr,
                          new AstValueAttribute<std::string>(parentName));

  Walk(std::get<parser::SpecificationPart>(x.t));
  Walk(std::get<std::optional<ModuleSubprogramPart>>(x.t));

  // Leave SageTreeBuilder for SgModuleStatement
  builder.Leave(module);
}

// BlockData
void BuildVisitor::Build(parser::BlockData &x) {
  // BlockData std::tuple<> Statement<BlockDataStmt>, SpecificationPart,
  // Statement<EndBlockDataStmt> BlockDataStmt std::optional<Name> v;
  // EndBlockDataStmt std::optional<Name> v;

  using namespace Fortran::parser;
  std::cout << "Rose::builder::Build(BlockData)\n";
  auto &stmt{std::get<Statement<BlockDataStmt>>(x.t)};
  auto &end{std::get<Statement<EndBlockDataStmt>>(x.t)};

  std::string name;
  if (stmt.statement.v) {
    name = stmt.statement.v->ToString();
  }
  if (name.empty()) {
    name = "BlockDataNameNotPresent__";
  }
  bool haveEndStmt{static_cast<bool>(end.statement.v)};

  std::optional<SourcePosition> srcPosBody{
      FirstSourcePosition(std::get<SpecificationPart>(x.t))};
  std::optional<SourcePosition> srcPosBegin{
      BuildSourcePosition(stmt, Order::begin)};
  SourcePosition srcPosEnd{BuildSourcePosition(end, Order::end)};
  if (!srcPosBody) {
    if (srcPosBegin) {
      srcPosBody.emplace(*srcPosBegin);
    } else {
      srcPosBody.emplace(srcPosEnd);
    }
  }
  Rose::builder::SourcePositions sources(*srcPosBegin, *srcPosBody, srcPosEnd);

  SgFunctionParameterList *paramList{nullptr};
  SgScopeStatement *paramScope{nullptr};
  SgFunctionDeclaration *functionDecl{nullptr};
  std::list<std::string> dummyArgs;
  LanguageTranslation::FunctionModifierList modifiers;
  std::vector<Rose::builder::Token> comments{};
  bool isDefDecl{true};

  builder.Enter(paramList, paramScope, name, /*function_type*/ nullptr,
                isDefDecl);

  Walk(std::get<SpecificationPart>(x.t));

  builder.Leave(paramList, paramScope, dummyArgs);

  builder.Enter(functionDecl, name, /*return_type*/ nullptr, paramList,
                modifiers, isDefDecl, sources, comments);

  if (auto *procDecl = isSgProcedureHeaderStatement(functionDecl)) {
    procDecl->set_subprogram_kind(
        SgProcedureHeaderStatement::e_block_data_subprogram_kind);
  }

  builder.Leave(functionDecl, paramScope, haveEndStmt);
}

void BuildFunctionReturnType(const parser::SpecificationPart &x,
                             std::string &result_name, SgType *&return_type) {
  using namespace Fortran::parser;

  if (return_type != nullptr || result_name.empty()) {
    return;
  }

  const bool caseInsensitive = SageInterface::is_language_case_insensitive();
  const SgName targetName(result_name);
  const auto &decls = std::get<6>(x.t);

  for (const auto &decl : decls) {
    bool found = false;
    common::visit(
        common::visitors{
            [&](const SpecificationConstruct &spec) {
              common::visit(
                  common::visitors{
                      [&](const Statement<
                          common::Indirection<TypeDeclarationStmt>> &stmt) {
                        const auto &typeDecl = stmt.statement.value();
                        SgType *baseType{nullptr};
                        auto &declType =
                            std::get<DeclarationTypeSpec>(typeDecl.t);
                        Build(const_cast<DeclarationTypeSpec &>(declType),
                              baseType);
                        if (baseType == nullptr) {
                          return;
                        }

                        const auto &entities =
                            std::get<std::list<EntityDecl>>(typeDecl.t);
                        for (const auto &entity : entities) {
                          const std::string entityName =
                              std::get<Name>(entity.t).ToString();
                          if (!namesMatch(SgName(entityName), targetName,
                                          caseInsensitive)) {
                            continue;
                          }

                          SgType *entityBase = baseType;
                          if (auto &lenOpt =
                                  std::get<std::optional<CharLength>>(
                                      entity.t)) {
                            SgExpression *lenExpr{nullptr};
                            auto &lenRef = lenOpt.value();
                            BuildImpl(const_cast<CharLength &>(lenRef),
                                      lenExpr);
                            if (lenExpr != nullptr) {
                              if (auto *stringType =
                                      isSgTypeString(entityBase)) {
                                SgTypeString *newType =
                                    SageBuilder::buildStringType(lenExpr);
                                newType->set_type_kind(
                                    stringType->get_type_kind());
                                entityBase = newType;
                              } else if (isSgTypeChar(entityBase)) {
                                entityBase =
                                    SageBuilder::buildStringType(lenExpr);
                              }
                            }
                          }

                          SgType *entityType = entityBase;
                          if (auto &arrayOpt =
                                  std::get<std::optional<ArraySpec>>(
                                      entity.t)) {
                            auto &arrayRef = arrayOpt.value();
                            Build(const_cast<ArraySpec &>(arrayRef), entityType,
                                  entityBase);
                          }

                          return_type =
                              entityType != nullptr ? entityType : entityBase;
                          found = true;
                          return;
                        }
                      },
                      [&](auto &) {}},
                  spec.u);
            },
            [&](auto &) {}},
        decl.u);
    if (found) {
      return;
    }
  }
}

namespace {
void DeclareFortranDummyArguments(SgScopeStatement *paramScope,
                                  const std::list<std::string> &dummyArgs) {
  if (paramScope == nullptr) {
    return;
  }
  for (const std::string &name : dummyArgs) {
    if (name.empty()) {
      continue;
    }
    if (name == "*") {
      continue;
    }
    SgVariableSymbol *symbol = paramScope->lookup_variable_symbol(name);
    if (symbol != nullptr) {
      continue;
    }
    SgType *implicitType = SageBuilder::buildFortranImplicitType(name);
    SgVariableDeclaration *varDecl = SageBuilder::buildVariableDeclaration_nfi(
        name, implicitType,
        /*initializer*/ nullptr, paramScope);
    ASSERT_not_null(varDecl);
    SageInterface::setSourcePosition(varDecl);
    MarkFortranImplicitDeclaration(varDecl);
    SageInterface::appendStatement(varDecl, paramScope);
    for (SgInitializedName *initName : varDecl->get_variables()) {
      if (initName == nullptr) {
        continue;
      }
      if (initName->get_scope() != paramScope) {
        initName->set_scope(paramScope);
      }
      if (initName->get_parent() == nullptr) {
        initName->set_parent(varDecl);
      }
    }
  }
}

void TransferParamScopeToFunctionBody(SgScopeStatement *paramScope,
                                      SgFunctionDeclaration *functionDecl) {
  if (paramScope == nullptr || functionDecl == nullptr) {
    return;
  }
  SgBasicBlock *paramBlock = isSgBasicBlock(paramScope);
  if (paramBlock == nullptr) {
    return;
  }
  SgFunctionDefinition *functionDef = functionDecl->get_definition();
  if (functionDef == nullptr) {
    return;
  }
  SgBasicBlock *functionBody = functionDef->get_body();
  ASSERT_not_null(functionBody);
  auto fix_initnames_from_param_scope = [&](SgScopeStatement *target_scope,
                                            SgScopeStatement *old_scope) {
    if (target_scope == nullptr || old_scope == nullptr) {
      return;
    }
    SgSymbolTable *symtab = target_scope->get_symbol_table();
    if (symtab == nullptr) {
      return;
    }
    std::set<SgNode *> symbols = symtab->get_symbols();
    for (SgNode *symNode : symbols) {
      SgVariableSymbol *varSym = isSgVariableSymbol(symNode);
      if (varSym == nullptr) {
        continue;
      }
      SgInitializedName *initName = varSym->get_declaration();
      if (initName == nullptr) {
        continue;
      }
      if (initName->get_scope() == old_scope) {
        initName->set_scope(target_scope);
      }
      if (initName->get_parent() == old_scope) {
        initName->set_parent(target_scope);
      }
      if (varSym->get_parent() == old_scope) {
        varSym->set_parent(target_scope);
      }
    }
  };
  auto rehome_param_scope_statements = [&](SgBasicBlock *old_block,
                                           SgBasicBlock *new_block) {
    if (old_block == nullptr || new_block == nullptr) {
      return;
    }
    for (SgStatement *stmt : new_block->get_statements()) {
      if (stmt == nullptr) {
        continue;
      }
      if (stmt->get_scope() == old_block) {
        stmt->set_scope(new_block);
      }
      if (stmt->get_parent() == old_block) {
        stmt->set_parent(new_block);
      }
      if (auto *decl = isSgDeclarationStatement(stmt)) {
        if (SgDeclarationStatement *nondef =
                decl->get_firstNondefiningDeclaration()) {
          if (nondef->get_scope() == old_block) {
            nondef->set_scope(new_block);
          }
          if (nondef->get_parent() == old_block) {
            nondef->set_parent(new_block);
          }
        }
        if (SgDeclarationStatement *def = decl->get_definingDeclaration()) {
          if (def->get_scope() == old_block) {
            def->set_scope(new_block);
          }
          if (def->get_parent() == old_block) {
            def->set_parent(new_block);
          }
        }
      }
      if (auto *varDecl = isSgVariableDeclaration(stmt)) {
        for (SgInitializedName *initName : varDecl->get_variables()) {
          if (initName == nullptr) {
            continue;
          }
          if (initName->get_scope() == old_block) {
            initName->set_scope(new_block);
          }
          if (initName->get_parent() == old_block) {
            initName->set_parent(varDecl);
          }
        }
      }
      if (auto *classDecl = isSgClassDeclaration(stmt)) {
        if (classDecl->get_scope() == old_block) {
          classDecl->set_scope(new_block);
        }
        if (SgClassDeclaration *defDecl =
                isSgClassDeclaration(classDecl->get_definingDeclaration())) {
          if (defDecl->get_scope() == old_block) {
            defDecl->set_scope(new_block);
          }
          if (defDecl->get_parent() == old_block) {
            defDecl->set_parent(new_block);
          }
        }
        if (SgClassDeclaration *nondefDecl = isSgClassDeclaration(
                classDecl->get_firstNondefiningDeclaration())) {
          if (nondefDecl->get_scope() == old_block) {
            nondefDecl->set_scope(new_block);
          }
          if (nondefDecl->get_parent() == old_block) {
            nondefDecl->set_parent(new_block);
          }
        }
        SageInterface::fixStructDeclaration(classDecl, new_block);
      }
    }
  };
  auto move_param_scope_statements = [&](SgBasicBlock *old_block,
                                         SgBasicBlock *new_block) {
    if (old_block == nullptr || new_block == nullptr) {
      return;
    }
    std::vector<SgStatement *> stmts(old_block->get_statements().begin(),
                                     old_block->get_statements().end());
    std::vector<SgStatement *> use_import;
    std::vector<SgStatement *> implicit;
    std::vector<SgStatement *> other_spec;
    std::vector<SgStatement *> non_spec;
    for (SgStatement *stmt : stmts) {
      if (stmt == nullptr) {
        continue;
      }
      if (isSgUseStatement(stmt) != nullptr ||
          isSgImportStatement(stmt) != nullptr) {
        use_import.push_back(stmt);
      } else if (isSgImplicitStatement(stmt) != nullptr) {
        implicit.push_back(stmt);
      } else if (IsFortranSpecificationStatement(stmt)) {
        other_spec.push_back(stmt);
      } else {
        non_spec.push_back(stmt);
      }
    }
    for (SgStatement *stmt : stmts) {
      if (stmt != nullptr) {
        SageInterface::removeStatement(stmt);
      }
    }
    auto append_spec = [&](const std::vector<SgStatement *> &spec_stmts) {
      for (SgStatement *stmt : spec_stmts) {
        if (stmt == nullptr) {
          continue;
        }
        InsertFortranSpecificationStatement(stmt, new_block);
      }
    };
    append_spec(use_import);
    append_spec(implicit);
    append_spec(other_spec);
    for (SgStatement *stmt : non_spec) {
      if (stmt == nullptr) {
        continue;
      }
      SageInterface::appendStatement(stmt, new_block);
    }
  };
  const bool force_case_insensitive =
      SageInterface::is_language_case_insensitive();
  SgName functionName = functionDecl->get_name();
  SgVariableSymbol *resultSymbol =
      paramBlock->lookup_variable_symbol(functionName);
  SageInterface::ensureCaseInsensitiveSymbolTable(paramBlock,
                                                  force_case_insensitive);
  SageInterface::ensureCaseInsensitiveSymbolTable(functionBody,
                                                  force_case_insensitive);
  EnsureSymbolsForBlockDeclarations(paramBlock);
  move_param_scope_statements(paramBlock, functionBody);
  SageInterface::transferSymbols(paramBlock, functionBody);
  rehome_param_scope_statements(paramBlock, functionBody);
  fix_initnames_from_param_scope(functionBody, paramScope);
  if (paramScope != nullptr) {
    Rose_STL_Container<SgNode *> initNodes =
        NodeQuery::querySubTree(functionDecl, V_SgInitializedName);
    for (SgNode *node : initNodes) {
      SgInitializedName *initName = isSgInitializedName(node);
      if (initName == nullptr) {
        continue;
      }
      if (initName->get_scope() == paramScope) {
        initName->set_scope(functionBody);
      }
      if (initName->get_parent() == paramScope) {
        initName->set_parent(functionBody);
      }
    }
    Rose_STL_Container<SgNode *> stmtNodes =
        NodeQuery::querySubTree(functionDecl, V_SgStatement);
    for (SgNode *node : stmtNodes) {
      SgStatement *stmt = isSgStatement(node);
      if (stmt == nullptr || stmt == paramScope) {
        continue;
      }
      if (stmt->get_scope() == paramScope) {
        stmt->set_scope(functionBody);
      }
      if (stmt->get_parent() == paramScope) {
        stmt->set_parent(functionBody);
      }
    }
  }
  {
    VariantVector variants;
    variants.push_back(V_SgFunctionTypeSymbol);
    Rose_STL_Container<SgNode *> symbols = NodeQuery::queryMemoryPool(variants);
    for (SgNode *node : symbols) {
      SgFunctionTypeSymbol *symbol = isSgFunctionTypeSymbol(node);
      if (symbol == nullptr) {
        continue;
      }

      SgSymbolTable *target_table = nullptr;
      SgType *type = symbol->get_type();
      if (isSgFunctionType(type) != nullptr ||
          isSgMemberFunctionType(type) != nullptr) {
        SgFunctionTypeTable *func_table = SgNode::get_globalFunctionTypeTable();
        if (func_table != nullptr) {
          target_table = func_table->get_function_type_table();
        }
      } else {
        SgTypeTable *type_table = SgNode::get_globalTypeTable();
        if (type_table != nullptr) {
          target_table = type_table->get_type_table();
        }
      }

      if (target_table != nullptr) {
        if (!target_table->exists(symbol)) {
          target_table->insert(symbol->get_name(), symbol);
        }
        if (symbol->get_parent() != target_table) {
          symbol->set_parent(target_table);
        }
      }
    }
  }
  auto stabilizeInterfaceBodyScopes = [&](SgScopeStatement *oldScope,
                                          SgScopeStatement *newScope) {
    if (oldScope == nullptr || newScope == nullptr) {
      return;
    }
    SgBasicBlock *newBlock = isSgBasicBlock(newScope);
    if (newBlock == nullptr) {
      return;
    }
    for (SgStatement *stmt : newBlock->get_statements()) {
      SgInterfaceStatement *interfaceStmt = isSgInterfaceStatement(stmt);
      if (interfaceStmt == nullptr) {
        continue;
      }
      for (SgInterfaceBody *body : interfaceStmt->get_interface_body_list()) {
        SgFunctionDeclaration *procDecl = body->get_functionDeclaration();
        if (procDecl == nullptr) {
          continue;
        }
        if (procDecl->get_scope() == oldScope) {
          procDecl->set_scope(newScope);
        }
        if (SgFunctionParameterScope *paramScope =
                procDecl->get_functionParameterScope()) {
          if (paramScope->get_parent() == oldScope) {
            paramScope->set_parent(newScope);
          }
          Rose_STL_Container<SgNode *> initNodes =
              NodeQuery::querySubTree(procDecl, V_SgInitializedName);
          for (SgNode *node : initNodes) {
            SgInitializedName *initName = isSgInitializedName(node);
            if (initName == nullptr) {
              continue;
            }
            if (initName->get_scope() == oldScope) {
              initName->set_scope(paramScope);
            }
          }
        }
        if (SgFunctionSymbol *symbol =
                isSgFunctionSymbol(procDecl->get_symbol_from_symbol_table())) {
          if (newScope->lookup_function_symbol(
                  procDecl->get_name(), procDecl->get_type()) == nullptr) {
            newScope->insert_symbol(procDecl->get_name(), symbol);
          }
          if (SgSymbolTable *symtab = newScope->get_symbol_table()) {
            if (symbol->get_parent() != symtab) {
              symbol->set_parent(symtab);
            }
          }
        }
      }
    }
  };
  stabilizeInterfaceBodyScopes(paramBlock, functionBody);
  if (resultSymbol != nullptr) {
    if (functionBody->lookup_variable_symbol(functionName) == nullptr) {
      functionBody->insert_symbol(functionName, resultSymbol);
    }
    if (SgInitializedName *initName = resultSymbol->get_declaration()) {
      initName->set_scope(functionBody);
    }
  }

  auto transfer_label_symbols = [&](SgScopeStatement *from_scope) {
    if (from_scope == nullptr) {
      return;
    }
    SgSymbolTable *symtab = from_scope->get_symbol_table();
    if (symtab == nullptr) {
      return;
    }
    std::set<SgNode *> symbols = symtab->get_symbols();
    for (SgNode *symNode : symbols) {
      SgLabelSymbol *labelSym = isSgLabelSymbol(symNode);
      if (labelSym == nullptr) {
        continue;
      }
      from_scope->remove_symbol(labelSym);
      if (functionDef->lookup_label_symbol(labelSym->get_name()) == nullptr) {
        functionDef->insert_symbol(labelSym->get_name(), labelSym);
      }
      if (SgLabelStatement *labelStmt = labelSym->get_declaration()) {
        labelStmt->set_scope(functionDef);
      }
    }
  };
  transfer_label_symbols(paramBlock);

  if (paramScope->getAttribute(kFlangParamScopeTransferredAttr) == nullptr) {
    paramScope->addNewAttribute(kFlangParamScopeTransferredAttr,
                                new FlangParamScopeTransferredAttribute());
  }
}
} // namespace

void BuildVisitor::Build(parser::AssignmentStmt &x) {
  // std::tuple<> Variable, Expr
  using namespace Fortran::parser;

  std::vector<std::string> labels = getLabels();
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  SgExprStatement *stmt{nullptr};

  WalkExpr(std::get<Variable>(x.t), lhs);
  WalkExpr(std::get<Expr>(x.t), rhs);

  std::vector<SgExpression *> vars;
  vars.push_back(lhs);

  // Begin SageTreeBuilder
  builder.Enter(stmt, rhs, vars);
  ApplyCurrentStatementSource(stmt);

  // Leave SageTreeBuilder
  builder.Leave(stmt, labels);
}

void BuildVisitor::Build(parser::AssociateConstruct &x) {
  using namespace Fortran::parser;

  auto &associateStmt = std::get<Statement<AssociateStmt>>(x.t);
  auto &block = std::get<Block>(x.t);
  auto &endStmt = std::get<Statement<EndAssociateStmt>>(x.t);

  SgScopeStatement *outerScope = SageBuilder::topScopeStack();
  ASSERT_not_null(outerScope);

  SgAssociateStatement *associateStatement = new SgAssociateStatement();
  ASSERT_not_null(associateStatement);

  SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
  ASSERT_not_null(body);

  associateStatement->set_body(body);
  if (outerScope->isCaseInsensitive()) {
    associateStatement->setCaseInsensitive(true);
    body->setCaseInsensitive(true);
  }

  body->set_parent(associateStatement);

  SourcePosition srcBegin{BuildSourcePosition(associateStmt, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(endStmt, Order::end)};
  builder.setSourcePosition(associateStatement, srcBegin, srcEnd);

  SageInterface::appendStatement(associateStatement, outerScope);

  auto set_label = [&](const std::optional<Label> &label,
                       SgLabelSymbol::label_type_enum label_type) {
    if (!label) {
      return;
    }
    const int labelValue = static_cast<int>(label.value());
    if (labelValue <= 0) {
      return;
    }
    SgScopeStatement *labelScope =
        SageInterface::getEnclosingFunctionDefinition(outerScope, true);
    if (labelScope == nullptr) {
      labelScope = SageInterface::getEnclosingScope(outerScope, true);
    }
    ASSERT_not_null(labelScope);
    SageInterface::setFortranNumericLabel(associateStatement, labelValue,
                                          label_type, labelScope);
  };

  set_label(associateStmt.label, SgLabelSymbol::e_start_label_type);
  if (associateStatement->has_end_numeric_label()) {
    set_label(endStmt.label, SgLabelSymbol::e_end_label_type);
  }

  const bool force_case_insensitive =
      SageInterface::is_language_case_insensitive();
  SageInterface::ensureCaseInsensitiveSymbolTable(associateStatement,
                                                  force_case_insensitive);
  SageInterface::ensureCaseInsensitiveSymbolTable(body, force_case_insensitive);

  SageInterface::setSourcePosition(body);
  SageBuilder::pushScopeStack(body);

  auto &associations =
      std::get<std::list<Association>>(associateStmt.statement.t);
  for (auto &association : associations) {
    std::string name = std::get<Name>(association.t).ToString();
    SgExpression *selector{nullptr};
    auto &selectorValue = std::get<Selector>(association.t).u;
    common::visit(
        common::visitors{[&](Expr &expr) { WalkExpr(expr, selector); },
                         [&](Variable &var) { WalkExpr(var, selector); }},
        selectorValue);
    ASSERT_not_null(selector);

    SgType *assocType = selector->get_type();
    if (assocType == nullptr) {
      assocType = SageBuilder::buildUnknownType();
    }

    SgInitializer *initializer =
        SageBuilder::buildAssignInitializer_nfi(selector, assocType);
    ASSERT_not_null(initializer);

    SgVariableDeclaration *varDecl = SageBuilder::buildVariableDeclaration_nfi(
        name, assocType, initializer, body);
    ASSERT_not_null(varDecl);
    associateStatement->append_associate(varDecl);
  }

  Walk(block);

  SageBuilder::popScopeStack();
}

namespace {
struct LoopControlInfo {
  SgExpression *initialization{nullptr};
  SgExpression *bound{nullptr};
  SgExpression *increment{nullptr};
  SgExpression *condition{nullptr};
  bool isWhile{false};
  bool isConcurrent{false};
};

void PopulateBlock(Rose::builder::BuildVisitor &visitor,
                   Fortran::parser::Block &block, SgBasicBlock *body) {
  ASSERT_not_null(body);
  SageInterface::setSourcePosition(body);

  SageBuilder::pushScopeStack(body);
  visitor.Walk(block);
  SageBuilder::popScopeStack();
}

SgExpression *BuildScalarExpr(parser::ScalarExpr &expr) {
  SgExpression *result{nullptr};
  WalkExpr(expr.thing.value(), result);
  ASSERT_not_null(result);
  return result;
}

SgExpression *BuildScalarLogicalExpr(parser::ScalarLogicalExpr &expr) {
  SgExpression *result{nullptr};
  WalkExpr(expr.thing.thing.value(), result);
  ASSERT_not_null(result);
  return result;
}

SgExpression *BuildDoInitialization(parser::LoopControl::Bounds &bounds) {
  std::string name{bounds.name.thing.ToString()};
  SgExpression *lhs = SageBuilderCpp17::buildVarRefExp_nfi(name);
  SgExpression *lower = BuildScalarExpr(bounds.lower);
  return SageBuilder::buildBinaryExpression_nfi<SgAssignOp>(lhs, lower);
}

LoopControlInfo
BuildLoopControl(std::optional<parser::LoopControl> &loopControl) {
  LoopControlInfo info;
  if (!loopControl) {
    return info;
  }

  auto &control = loopControl.value();
  common::visit(
      common::visitors{
          [&](parser::LoopControl::Bounds &bounds) {
            info.initialization = BuildDoInitialization(bounds);
            info.bound = BuildScalarExpr(bounds.upper);
            if (bounds.step) {
              info.increment = BuildScalarExpr(bounds.step.value());
            }
          },
          [&](parser::ScalarLogicalExpr &expr) {
            info.isWhile = true;
            info.condition = BuildScalarLogicalExpr(expr);
          },
          [&](parser::LoopControl::Concurrent &) { info.isConcurrent = true; }},
      control.u);
  return info;
}

SgExpression *BuildScalarIntExpr(parser::ScalarIntExpr &expr) {
  SgExpression *result{nullptr};
  WalkExpr(expr, result);
  ASSERT_not_null(result);
  return result;
}

SgExprListExp *BuildConcurrentHeader(parser::LoopControl::Concurrent &control) {
  auto &header = std::get<parser::ConcurrentHeader>(control.t);
  auto &controls = std::get<std::list<parser::ConcurrentControl>>(header.t);

  SgExprListExp *exprList = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(exprList);

  for (auto &concurrentControl : controls) {
    std::string name = std::get<parser::Name>(concurrentControl.t).ToString();
    SgExpression *varRef = SageBuilderCpp17::buildVarRefExp_nfi(name);
    ASSERT_not_null(varRef);

    SgExpression *lower = BuildScalarIntExpr(std::get<1>(concurrentControl.t));
    SgExpression *upper = BuildScalarIntExpr(std::get<2>(concurrentControl.t));
    SgExpression *step{nullptr};
    if (std::get<3>(concurrentControl.t)) {
      step = BuildScalarIntExpr(std::get<3>(concurrentControl.t).value());
    } else {
      step = SageBuilder::buildIntVal_nfi(std::string("1"));
    }

    SgExpression *subscript =
        SageBuilderCpp17::buildSubscriptExpression_nfi(lower, upper, step);
    ASSERT_not_null(subscript);
    SgExpression *assign =
        SageBuilder::buildBinaryExpression_nfi<SgAssignOp>(varRef, subscript);
    ASSERT_not_null(assign);
    SageInterface::appendExpression(exprList, assign);
  }

  if (auto &maskOpt =
          std::get<std::optional<parser::ScalarLogicalExpr>>(header.t)) {
    SgExpression *mask{nullptr};
    WalkExpr(maskOpt.value(), mask);
    ASSERT_not_null(mask);
    SageInterface::appendExpression(exprList, mask);
  }

  return exprList;
}
} // namespace

void BuildVisitor::Build(parser::DoConstruct &x) {
  //  std::tuple<Statement<NonLabelDoStmt>, Block, Statement<EndDoStmt>> t;
  //  bool IsDoNormal() const;  bool IsDoWhile() const; bool IsDoConcurrent()
  //  const;
  using namespace Fortran::parser;

  SgWhileStmt *whileStmt{nullptr};
  SgFortranDo *doStmt{nullptr};
  std::string doName;
  auto &doStmtInfo = std::get<Statement<NonLabelDoStmt>>(x.t).statement;
  if (auto &nameOpt = std::get<std::optional<Name>>(doStmtInfo.t)) {
    doName = nameOpt->ToString();
  }
  auto &endName = std::get<Statement<EndDoStmt>>(x.t).statement.v;
  if (doName.empty() && endName) {
    doName = endName->ToString();
  }
  SourcePosition srcBegin{BuildSourcePosition(
      std::get<Statement<NonLabelDoStmt>>(x.t), Order::begin)};
  SourcePosition srcEnd{
      BuildSourcePosition(std::get<Statement<EndDoStmt>>(x.t), Order::end)};
  if (SgProject::get_verbose() > 2) {
    std::cerr << "DoConstruct source range: " << srcBegin.line << "->"
              << srcEnd.line << "\n";
  }
  auto &loopControl = std::get<2>(std::get<0>(x.t).statement.t);
  if (loopControl) {
    if (auto *concurrent =
            std::get_if<parser::LoopControl::Concurrent>(&loopControl->u)) {
      SgScopeStatement *outerScope = SageBuilder::topScopeStack();
      ASSERT_not_null(outerScope);

      SgExprListExp *header = BuildConcurrentHeader(*concurrent);
      ASSERT_not_null(header);

      SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
      ASSERT_not_null(body);
      body->set_scope(outerScope);

      SgForAllStatement *doConcurrent = new SgForAllStatement(header, body);
      ASSERT_not_null(doConcurrent);

      doConcurrent->set_forall_statement_kind(
          SgForAllStatement::e_do_concurrent_statement);
      doConcurrent->set_has_end_statement(true);
      if (!doName.empty()) {
        doConcurrent->set_string_label(doName);
      }
      if (outerScope->isCaseInsensitive()) {
        doConcurrent->setCaseInsensitive(true);
        body->setCaseInsensitive(true);
      }

      doConcurrent->set_body(body);
      body->set_parent(doConcurrent);
      header->set_parent(doConcurrent);
      builder.setSourcePosition(doConcurrent, srcBegin, srcEnd);

      SageInterface::appendStatement(doConcurrent, outerScope);

      SageBuilder::pushScopeStack(body);
      Walk(std::get<1>(x.t));
      SageBuilder::popScopeStack();
      return;
    }
  }

  const LoopControlInfo control = BuildLoopControl(loopControl);

  // Enter SageTreeBuilder
  if (control.isWhile) {
    ASSERT_not_null(control.condition);
    builder.Enter(whileStmt, control.condition);
    builder.setSourcePosition(whileStmt, srcBegin, srcEnd);
    if (!doName.empty()) {
      whileStmt->set_string_label(doName);
    }
  } else {
    builder.Enter(doStmt, control.initialization, control.bound,
                  control.increment);
    builder.setSourcePosition(doStmt, srcBegin, srcEnd);
    if (!doName.empty()) {
      doStmt->set_string_label(doName);
    }
  }

  // Traverse the body
  Walk(std::get<1>(x.t));

  // Leave SageTreeBuilder
  if (control.isWhile) {
    builder.Leave(whileStmt, /*hasEndDo*/ true);
  } else {
    builder.Leave(doStmt);
  }
}

void BuildVisitor::Build(parser::IfConstruct &x) {
  using namespace Fortran::parser;

  auto &ifStmt = std::get<Statement<IfThenStmt>>(x.t);
  SgExpression *condition =
      BuildScalarLogicalExpr(std::get<ScalarLogicalExpr>(ifStmt.statement.t));
  ASSERT_not_null(condition);

  std::vector<Rose::builder::Token> comments{};
  SgBasicBlock *trueBody = SageBuilder::buildBasicBlock_nfi();
  ASSERT_not_null(trueBody);
  if (SageBuilder::topScopeStack()->isCaseInsensitive()) {
    trueBody->setCaseInsensitive(true);
  }
  trueBody->set_scope(SageBuilder::topScopeStack());
  SgIfStmt *ifNode{nullptr};
  builder.Enter(ifNode, condition, trueBody, /*false_body*/ nullptr, comments,
                /*is_ifthen*/ true, /*has_end_stmt*/ true,
                /*is_else_if*/ false);
  SourcePosition srcBegin{BuildSourcePosition(ifStmt, Order::begin)};
  SourcePosition srcEnd{
      BuildSourcePosition(std::get<Statement<EndIfStmt>>(x.t), Order::end)};
  builder.setSourcePosition(ifNode, srcBegin, srcEnd);
  builder.Leave(ifNode);
  trueBody->set_scope(SageBuilder::topScopeStack());

  PopulateBlock(*this, std::get<Block>(x.t), trueBody);

  SgIfStmt *currentIf = ifNode;
  for (auto &elseIfBlock : std::get<std::list<IfConstruct::ElseIfBlock>>(x.t)) {
    auto &elseIfStmt = std::get<Statement<ElseIfStmt>>(elseIfBlock.t);
    SgExpression *elseIfCond = BuildScalarLogicalExpr(
        std::get<ScalarLogicalExpr>(elseIfStmt.statement.t));
    ASSERT_not_null(elseIfCond);

    SgIfStmt *elseIfNode{nullptr};
    SgBasicBlock *elseIfBody = SageBuilder::buildBasicBlock_nfi();
    ASSERT_not_null(elseIfBody);
    if (SageBuilder::topScopeStack()->isCaseInsensitive()) {
      elseIfBody->setCaseInsensitive(true);
    }
    elseIfBody->set_scope(SageBuilder::topScopeStack());
    builder.Enter(elseIfNode, elseIfCond, elseIfBody, /*false_body*/ nullptr,
                  comments, /*is_ifthen*/ true, /*has_end_stmt*/ false,
                  /*is_else_if*/ true);
    SourcePosition elseIfBegin{BuildSourcePosition(elseIfStmt, Order::begin)};
    SourcePosition elseIfEnd{BuildSourcePosition(elseIfStmt, Order::end)};
    builder.setSourcePosition(elseIfNode, elseIfBegin, elseIfEnd);
    elseIfBody->set_scope(SageBuilder::topScopeStack());

    currentIf->set_false_body(elseIfNode);
    elseIfNode->set_parent(currentIf);
    PopulateBlock(*this, std::get<Block>(elseIfBlock.t), elseIfBody);
    currentIf = elseIfNode;
  }

  if (auto &optElse = std::get<std::optional<IfConstruct::ElseBlock>>(x.t)) {
    SgBasicBlock *elseBody = SageBuilder::buildBasicBlock_nfi();
    ASSERT_not_null(elseBody);
    if (SageBuilder::topScopeStack()->isCaseInsensitive()) {
      elseBody->setCaseInsensitive(true);
    }
    currentIf->set_false_body(elseBody);
    elseBody->set_parent(currentIf);
    elseBody->set_scope(SageBuilder::topScopeStack());
    PopulateBlock(*this, std::get<Block>(optElse->t), elseBody);
  }
}

void BuildVisitor::Build(parser::IfStmt &x) {
  using namespace Fortran::parser;

  SgExpression *condition =
      BuildScalarLogicalExpr(std::get<ScalarLogicalExpr>(x.t));
  ASSERT_not_null(condition);

  std::vector<std::string> labels = getLabels();
  std::optional<parser::Label> saved_label = label_;

  SgBasicBlock *trueBody = SageBuilder::buildBasicBlock_nfi();
  ASSERT_not_null(trueBody);
  if (SageBuilder::topScopeStack()->isCaseInsensitive()) {
    trueBody->setCaseInsensitive(true);
  }
  trueBody->set_scope(SageBuilder::topScopeStack());

  std::vector<Rose::builder::Token> comments{};
  SgIfStmt *ifNode{nullptr};
  builder.Enter(ifNode, condition, trueBody, /*false_body*/ nullptr, comments,
                /*is_ifthen*/ false, /*has_end_stmt*/ false,
                /*is_else_if*/ false);
  ApplyCurrentStatementSource(ifNode);
  builder.Leave(ifNode, labels);
  trueBody->set_scope(SageBuilder::topScopeStack());

  SageInterface::setSourcePosition(trueBody);
  SageBuilder::pushScopeStack(trueBody);
  label_ = std::nullopt;
  Walk(std::get<UnlabeledStatement<ActionStmt>>(x.t));
  label_ = saved_label;
  HoistInlineIfSpecStatements(ifNode, trueBody);
  SageBuilder::popScopeStack();
}

void BuildVisitor::Build(parser::Statement<parser::ActionStmt> &x) {
  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  current_stmt_source_.emplace(srcBegin, srcEnd);
  Walk(x.statement);
  current_stmt_source_.reset();
}

void BuildVisitor::Build(parser::UnlabeledStatement<parser::ActionStmt> &x) {
  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  current_stmt_source_.emplace(srcBegin, srcEnd);
  Walk(x.statement);
  current_stmt_source_.reset();
}

void BuildVisitor::Build(parser::ArithmeticIfStmt &x) {
  SgExpression *condition{nullptr};
  WalkExpr(std::get<parser::Expr>(x.t), condition);
  ASSERT_not_null(condition);

  SgExpression *lessExpr{nullptr};
  SgExpression *equalExpr{nullptr};
  SgExpression *greaterExpr{nullptr};
  Rose::builder::Build(std::get<1>(x.t), lessExpr);
  Rose::builder::Build(std::get<2>(x.t), equalExpr);
  Rose::builder::Build(std::get<3>(x.t), greaterExpr);
  ASSERT_not_null(lessExpr);
  ASSERT_not_null(equalExpr);
  ASSERT_not_null(greaterExpr);

  SgLabelRefExp *lessRef = isSgLabelRefExp(lessExpr);
  SgLabelRefExp *equalRef = isSgLabelRefExp(equalExpr);
  SgLabelRefExp *greaterRef = isSgLabelRefExp(greaterExpr);
  ROSE_ASSERT(lessRef != nullptr);
  ROSE_ASSERT(equalRef != nullptr);
  ROSE_ASSERT(greaterRef != nullptr);

  SgArithmeticIfStatement *stmt =
      new SgArithmeticIfStatement(condition, lessRef, equalRef, greaterRef);
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);
  condition->set_parent(stmt);
  lessRef->set_parent(stmt);
  equalRef->set_parent(stmt);
  greaterRef->set_parent(stmt);

  auto labels = getLabels();
  builder.Leave(stmt, labels);
}

void BuildVisitor::Build(parser::LabelDoStmt &x) {
  // LabelDoStmt std::tuple<Label, std::optional<LoopControl>> t;
  const auto endLabel = std::get<parser::Label>(x.t);
  auto &loopControl = std::get<1>(x.t);
  const LoopControlInfo control = BuildLoopControl(loopControl);

  if (control.isConcurrent) {
    auto *concurrent =
        loopControl
            ? std::get_if<parser::LoopControl::Concurrent>(&loopControl->u)
            : nullptr;
    if (concurrent == nullptr) {
      std::cerr << "[FATAL] Do concurrent control not available.\n";
      ROSE_ABORT();
    }

    SgScopeStatement *outerScope = SageBuilder::topScopeStack();
    ASSERT_not_null(outerScope);

    SgExprListExp *header = BuildConcurrentHeader(*concurrent);
    ASSERT_not_null(header);

    SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
    ASSERT_not_null(body);
    body->set_scope(outerScope);

    SgForAllStatement *doConcurrent = new SgForAllStatement(header, body);
    ASSERT_not_null(doConcurrent);

    doConcurrent->set_forall_statement_kind(
        SgForAllStatement::e_do_concurrent_statement);
    doConcurrent->set_has_end_statement(false);
    if (outerScope->isCaseInsensitive()) {
      doConcurrent->setCaseInsensitive(true);
      body->setCaseInsensitive(true);
    }

    doConcurrent->set_body(body);
    body->set_parent(doConcurrent);
    header->set_parent(doConcurrent);
    ApplyCurrentStatementSource(doConcurrent);

    if (label_) {
      SgScopeStatement *labelScope =
          SageInterface::getEnclosingFunctionDefinition(doConcurrent);
      if (labelScope == nullptr) {
        labelScope = SageInterface::getEnclosingScope(doConcurrent, true);
      }
      ASSERT_not_null(labelScope);
      SageInterface::setFortranNumericLabel(
          doConcurrent, static_cast<int>(label_.value()),
          SgLabelSymbol::e_start_label_type, labelScope);
    }

    SageInterface::appendStatement(doConcurrent, outerScope);
    SageBuilder::pushScopeStack(body);
    label_do_stack_.push_back(
        LabelDoFrame{endLabel, LabelDoFrame::Kind::DoConcurrent, doConcurrent});
    return;
  }

  if (control.isWhile) {
    ASSERT_not_null(control.condition);
    SgWhileStmt *whileStmt{nullptr};
    builder.Enter(whileStmt, control.condition);
    ApplyCurrentStatementSource(whileStmt);

    if (label_) {
      SgScopeStatement *labelScope =
          SageInterface::getEnclosingFunctionDefinition(whileStmt);
      if (labelScope == nullptr) {
        labelScope = SageInterface::getEnclosingScope(whileStmt, true);
      }
      ASSERT_not_null(labelScope);
      SageInterface::setFortranNumericLabel(
          whileStmt, static_cast<int>(label_.value()),
          SgLabelSymbol::e_start_label_type, labelScope);
    }

    label_do_stack_.push_back(
        LabelDoFrame{endLabel, LabelDoFrame::Kind::While, whileStmt});
    return;
  }

  SgFortranDo *doStmt{nullptr};
  builder.Enter(doStmt, control.initialization, control.bound,
                control.increment);
  ApplyCurrentStatementSource(doStmt);
  doStmt->set_has_end_statement(false);

  if (label_) {
    SgScopeStatement *labelScope =
        SageInterface::getEnclosingFunctionDefinition(doStmt);
    if (labelScope == nullptr) {
      labelScope = SageInterface::getEnclosingScope(doStmt, true);
    }
    ASSERT_not_null(labelScope);
    SageInterface::setFortranNumericLabel(
        doStmt, static_cast<int>(label_.value()),
        SgLabelSymbol::e_start_label_type, labelScope);
  }

  label_do_stack_.push_back(
      LabelDoFrame{endLabel, LabelDoFrame::Kind::FortranDo, doStmt});
}

void BuildVisitor::CloseLabelDoLoops(const parser::Label &label) {
  while (!label_do_stack_.empty() &&
         label_do_stack_.back().end_label == label) {
    LabelDoFrame frame = label_do_stack_.back();
    label_do_stack_.pop_back();
    SgScopeStatement *labelScope =
        SageInterface::getEnclosingFunctionDefinition(frame.stmt);
    if (labelScope == nullptr) {
      labelScope = SageInterface::getEnclosingScope(frame.stmt, true);
    }
    ASSERT_not_null(labelScope);
    SetFortranEndLabelReference(frame.stmt, static_cast<int>(label),
                                labelScope);
    if (frame.kind == LabelDoFrame::Kind::FortranDo) {
      auto *doStmt = isSgFortranDo(frame.stmt);
      ASSERT_not_null(doStmt);
      builder.Leave(doStmt);
    } else if (frame.kind == LabelDoFrame::Kind::DoConcurrent) {
      auto *doConcurrent = isSgForAllStatement(frame.stmt);
      ASSERT_not_null(doConcurrent);
      SageBuilder::popScopeStack();
    } else {
      auto *whileStmt = isSgWhileStmt(frame.stmt);
      ASSERT_not_null(whileStmt);
      builder.Leave(whileStmt, /*hasEndDo*/ false);
    }
  }
}

void BuildVisitor::ApplyStatementLabel(SgStatement *stmt,
                                       SgScopeStatement *scope) const {
  if (!label_ || stmt == nullptr) {
    return;
  }
  ASSERT_not_null(scope);
  SgScopeStatement *labelScope =
      SageInterface::getEnclosingFunctionDefinition(scope, true);
  if (labelScope == nullptr) {
    labelScope = SageInterface::getEnclosingScope(scope, true);
  }
  ASSERT_not_null(labelScope);
  SageInterface::setFortranNumericLabel(stmt, static_cast<int>(label_.value()),
                                        SgLabelSymbol::e_start_label_type,
                                        labelScope);
}

void BuildVisitor::ApplyCurrentStatementSource(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }
  if (current_stmt_source_) {
    const auto &pair = current_stmt_source_.value();
    if (SgProject::get_verbose() > 2) {
      std::cerr << "ApplyCurrentStatementSource: " << node->class_name()
                << " start " << std::get<0>(pair).line << " end "
                << std::get<1>(pair).line << "\n";
    }
    builder.setSourcePosition(node, std::get<0>(pair), std::get<1>(pair));
    return;
  }
  if (SgProject::get_verbose() > 2) {
    std::cerr << "ApplyCurrentStatementSource: " << node->class_name()
              << " no source\n";
  }
  SageInterface::setSourcePosition(node);
}

namespace {
SgType *BuildTypeSpec(parser::TypeSpec &x);

SgExpression *BuildAllocateObjectExpr(parser::AllocateObject &x) {
  SgExpression *expr{nullptr};
  common::visit(
      common::visitors{[&](parser::Name &y) {
                         std::string name = y.ToString();
                         expr = SageBuilderCpp17::buildVarRefExp_nfi(name);
                       },
                       [&](parser::StructureComponent &y) { Build(y, expr); }},
      x.u);
  ASSERT_not_null(expr);
  return expr;
}

SgExpression *BuildAllocateShapeSpecExpr(parser::AllocateShapeSpec &x) {
  SgExpression *lower{nullptr};
  SgExpression *upper{nullptr};
  if (std::get<0>(x.t)) {
    WalkExpr(std::get<0>(x.t).value(), lower);
  }
  WalkExpr(std::get<1>(x.t), upper);
  ASSERT_not_null(upper);

  if (lower == nullptr) {
    return upper;
  }

  SgExpression *stride = SageBuilder::buildIntVal_nfi(std::string("1"));
  return SageBuilderCpp17::buildSubscriptExpression_nfi(lower, upper, stride);
}

SgExprListExp *
BuildAllocateCoarraySpecExprList(parser::AllocateCoarraySpec &x) {
  SgExprListExp *coshapes = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(coshapes);

  for (auto &spec : std::get<0>(x.t)) {
    SgExpression *shapeExpr = BuildAllocateShapeSpecExpr(spec);
    ASSERT_not_null(shapeExpr);
    SageInterface::appendExpression(coshapes, shapeExpr);
  }

  if (auto &opt = std::get<1>(x.t)) {
    SgExpression *lower{nullptr};
    WalkExpr(opt.value(), lower);
    ASSERT_not_null(lower);
    SgExpression *upper{SageBuilderCpp17::buildAsteriskShapeExp_nfi()};
    SgExpression *stride{SageBuilder::buildIntVal_nfi(std::string("1"))};
    SgExpression *shapeExpr =
        SageBuilder::buildSubscriptExpression_nfi(lower, upper, stride);
    ASSERT_not_null(shapeExpr);
    SageInterface::appendExpression(coshapes, shapeExpr);
  }

  return coshapes;
}

SgExpression *BuildAllocationExpr(parser::Allocation &x) {
  auto &obj = std::get<parser::AllocateObject>(x.t);
  auto &shapeSpecs = std::get<std::list<parser::AllocateShapeSpec>>(x.t);
  auto &coarraySpec = std::get<std::optional<parser::AllocateCoarraySpec>>(x.t);

  SgExpression *base = BuildAllocateObjectExpr(obj);
  ASSERT_not_null(base);

  SgExpression *result = base;

  if (!shapeSpecs.empty()) {
    SgExprListExp *subscripts = SageBuilder::buildExprListExp_nfi();
    ASSERT_not_null(subscripts);
    for (auto &spec : shapeSpecs) {
      SgExpression *shapeExpr = BuildAllocateShapeSpecExpr(spec);
      ASSERT_not_null(shapeExpr);
      SageInterface::appendExpression(subscripts, shapeExpr);
    }
    result = SageBuilderCpp17::buildPntrArrRefExp_nfi(base, subscripts);
    ASSERT_not_null(result);
  }

  if (coarraySpec) {
    SgExprListExp *coshapes =
        BuildAllocateCoarraySpecExprList(coarraySpec.value());
    ASSERT_not_null(coshapes);
    SgCAFCoExpression *coExpr =
        new SgCAFCoExpression(/*teamId*/ nullptr, coshapes, result);
    ASSERT_not_null(coExpr);
    SageInterface::setSourcePosition(coExpr);
    result->set_parent(coExpr);
    coshapes->set_parent(coExpr);
    result = coExpr;
  }

  return result;
}

void ApplyStatOrErrmsg(parser::StatOrErrmsg &x, SgExpression *&statExpr,
                       SgExpression *&errmsgExpr) {
  common::visit(
      common::visitors{
          [&](parser::StatVariable &y) {
            if (statExpr != nullptr) {
              std::cerr << "Duplicate STAT spec in allocate/deallocate.\n";
              ROSE_ABORT();
            }
            WalkExpr(y.v, statExpr);
          },
          [&](parser::MsgVariable &y) {
            if (errmsgExpr != nullptr) {
              std::cerr << "Duplicate ERRMSG spec in allocate/deallocate.\n";
              ROSE_ABORT();
            }
            WalkExpr(y.v, errmsgExpr);
          }},
      x.u);
}
} // namespace

// ActionStmt(s)
//
void BuildVisitor::Build(parser::AllocateStmt &x) {
  SgAllocateStatement *stmt = new SgAllocateStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  SgExprListExp *exprList = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(exprList);
  if (auto &typeSpecOpt = std::get<std::optional<parser::TypeSpec>>(x.t)) {
    SgType *allocType = BuildTypeSpec(typeSpecOpt.value());
    ASSERT_not_null(allocType);
    SgTypeExpression *typeExpr = SageBuilder::buildTypeExpression(allocType);
    ASSERT_not_null(typeExpr);
    SageInterface::appendExpression(exprList, typeExpr);
  }
  for (auto &alloc : std::get<std::list<parser::Allocation>>(x.t)) {
    SgExpression *allocExpr = BuildAllocationExpr(alloc);
    ASSERT_not_null(allocExpr);
    SageInterface::appendExpression(exprList, allocExpr);
  }
  stmt->set_expr_list(exprList);
  exprList->set_parent(stmt);

  SgExpression *statExpr{nullptr};
  SgExpression *errmsgExpr{nullptr};
  SgExpression *sourceExpr{nullptr};

  for (auto &opt : std::get<std::list<parser::AllocOpt>>(x.t)) {
    common::visit(
        common::visitors{
            [&](parser::AllocOpt::Mold &) {
              std::cerr << "Allocate MOLD option not supported yet.\n";
              ROSE_ABORT();
            },
            [&](parser::AllocOpt::Source &y) {
              if (sourceExpr != nullptr) {
                std::cerr << "Duplicate SOURCE spec in allocate.\n";
                ROSE_ABORT();
              }
              WalkExpr(y.v.value(), sourceExpr);
            },
            [&](parser::StatOrErrmsg &y) {
              ApplyStatOrErrmsg(y, statExpr, errmsgExpr);
            },
            [&](parser::AllocOpt::Stream &) {
              std::cerr << "Allocate STREAM option not supported yet.\n";
              ROSE_ABORT();
            },
            [&](parser::AllocOpt::Pinned &) {
              std::cerr << "Allocate PINNED option not supported yet.\n";
              ROSE_ABORT();
            }},
        opt.u);
  }

  if (statExpr != nullptr) {
    stmt->set_stat_expression(statExpr);
    statExpr->set_parent(stmt);
  }
  if (errmsgExpr != nullptr) {
    stmt->set_errmsg_expression(errmsgExpr);
    errmsgExpr->set_parent(stmt);
  }
  if (sourceExpr != nullptr) {
    stmt->set_source_expression(sourceExpr);
    sourceExpr->set_parent(stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::ContinueStmt &) {
  SgFortranContinueStmt *stmt{nullptr};
  builder.Enter(stmt);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildVisitor::Build(parser::CycleStmt &x) {
  SgContinueStmt *stmt{nullptr};
  builder.Enter(stmt);
  ApplyCurrentStatementSource(stmt);
  if (x.v) {
    stmt->set_do_string_label(x.v->ToString());
  }
  builder.Leave(stmt, getLabels());
}

void BuildVisitor::Build(parser::DeallocateStmt &x) {
  SgDeallocateStatement *stmt = new SgDeallocateStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  SgExprListExp *exprList = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(exprList);
  for (auto &obj : std::get<std::list<parser::AllocateObject>>(x.t)) {
    SgExpression *objExpr = BuildAllocateObjectExpr(obj);
    ASSERT_not_null(objExpr);
    SageInterface::appendExpression(exprList, objExpr);
  }
  stmt->set_expr_list(exprList);
  exprList->set_parent(stmt);

  SgExpression *statExpr{nullptr};
  SgExpression *errmsgExpr{nullptr};
  for (auto &opt : std::get<std::list<parser::StatOrErrmsg>>(x.t)) {
    ApplyStatOrErrmsg(opt, statExpr, errmsgExpr);
  }

  if (statExpr != nullptr) {
    stmt->set_stat_expression(statExpr);
    statExpr->set_parent(stmt);
  }
  if (errmsgExpr != nullptr) {
    stmt->set_errmsg_expression(errmsgExpr);
    errmsgExpr->set_parent(stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::FailImageStmt &) {
  SgProcessControlStatement *stmt{nullptr};
  builder.Enter(stmt, "fail_image", std::nullopt, std::nullopt);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildVisitor::Build(parser::ExitStmt &x) {
  SgBreakStmt *stmt{nullptr};
  builder.Enter(stmt);
  ApplyCurrentStatementSource(stmt);
  if (x.v) {
    stmt->set_do_string_label(x.v->ToString());
  }
  builder.Leave(stmt, getLabels());
}

void Build(parser::FunctionStmt &x, std::list<std::string> &dummy_arg_name_list,
           std::string &name, std::string &result_name,
           LanguageTranslation::FunctionModifierList &function_modifiers,
           SgType *&type) {
  using namespace Fortran::parser;
  info(x, "Rose::builder::Build(FunctionStmt)");
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::PrintStmt &x) {
  // std::tuple<Format, std::list<OutputItem>> t;
  info(x, "Rose::builder::Build(PrintStmt)");

  // TODO: get unparse to work (may need const)
  // Fortran::parser::Unparse(llvm::errs(), x, /*encoding=*/true);

  SgPrintStatement *stmt{nullptr};
  SgExpression *format{nullptr};

  // Format
  common::visit(common::visitors{
                    [&](parser::Expr &y) { WalkExpr(y, format); },
                    [&](parser::Label &y) { Rose::builder::Build(y, format); },
                    [&](parser::Star &y) { Rose::builder::Build(y, format); }},
                std::get<0>(x.t).u);
  if (format == nullptr) {
    format = SageBuilderCpp17::buildAsteriskShapeExp_nfi();
  }

  // OutputItem
  // std::variant<Expr, common::Indirection<OutputImpliedDo>> u;
  std::list<SgExpression *> items{};

  for (auto &item : std::get<1>(x.t)) {
    SgExpression *itemExpr{nullptr};
    Rose::builder::Build(item, itemExpr);
    ASSERT_not_null(itemExpr);
    items.push_back(itemExpr);
  }

  // Enter/Leave SageTreeBuilder
  builder.Enter(stmt, format, items);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildVisitor::Build(
    parser::Statement<common::Indirection<parser::ParameterStmt>> &x) {
  using namespace Fortran::parser;

  auto &paramStmt = x.statement.value();

  SgAttributeSpecificationStatement *stmt =
      SageBuilder::buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_parameterStatement);
  ASSERT_not_null(stmt);

  SgExprListExp *paramList = stmt->get_parameter_list();
  if (paramList == nullptr) {
    paramList = SageBuilder::buildExprListExp_nfi();
    ASSERT_not_null(paramList);
    stmt->set_parameter_list(paramList);
    paramList->set_parent(stmt);
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  for (auto &def : paramStmt.v) {
    auto &named = std::get<0>(def.t);
    auto &constant = std::get<1>(def.t);
    std::string name = named.v.ToString();
    SgExpression *lhs = SageBuilderCpp17::buildVarRefExp_nfi(name);
    ASSERT_not_null(lhs);
    SgExpression *rhs{nullptr};
    Rose::builder::Build(constant, rhs);
    ASSERT_not_null(rhs);
    SgExpression *assign =
        SageBuilder::buildBinaryExpression_nfi<SgAssignOp>(lhs, rhs);
    ASSERT_not_null(assign);
    lhs->set_parent(assign);
    rhs->set_parent(assign);
    paramList->get_expressions().push_back(assign);
    assign->set_parent(paramList);
  }

  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  builder.setSourcePosition(stmt, srcBegin, srcEnd);

  ApplyStatementLabel(stmt, scope);
  SageInterface::appendStatement(stmt, scope);
}

void BuildVisitor::Build(
    parser::Statement<common::Indirection<parser::OldParameterStmt>> &x) {
  using namespace Fortran::parser;

  auto &paramStmt = x.statement.value();

  SgAttributeSpecificationStatement *stmt =
      SageBuilder::buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_parameterStatement);
  ASSERT_not_null(stmt);

  SgExprListExp *paramList = stmt->get_parameter_list();
  if (paramList == nullptr) {
    paramList = SageBuilder::buildExprListExp_nfi();
    ASSERT_not_null(paramList);
    stmt->set_parameter_list(paramList);
    paramList->set_parent(stmt);
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  for (auto &def : paramStmt.v) {
    auto &named = std::get<0>(def.t);
    auto &constant = std::get<1>(def.t);
    std::string name = named.v.ToString();
    SgExpression *lhs = SageBuilderCpp17::buildVarRefExp_nfi(name);
    ASSERT_not_null(lhs);
    SgExpression *rhs{nullptr};
    Rose::builder::Build(constant, rhs);
    ASSERT_not_null(rhs);
    SgExpression *assign =
        SageBuilder::buildBinaryExpression_nfi<SgAssignOp>(lhs, rhs);
    ASSERT_not_null(assign);
    lhs->set_parent(assign);
    rhs->set_parent(assign);
    paramList->get_expressions().push_back(assign);
    assign->set_parent(paramList);
  }

  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  builder.setSourcePosition(stmt, srcBegin, srcEnd);

  ApplyStatementLabel(stmt, scope);
  SageInterface::appendStatement(stmt, scope);
}

void BuildVisitor::Build(
    parser::Statement<common::Indirection<parser::FormatStmt>> &x) {
  using namespace Fortran::parser;

  auto &formatStmt = x.statement.value();

  SgFormatItemList *itemList = new SgFormatItemList();
  ASSERT_not_null(itemList);

  auto appendItem = [&](const std::string &text) {
    if (text.empty()) {
      return;
    }
    SgFormatItem *item = new SgFormatItem();
    ASSERT_not_null(item);
    item->set_repeat_specification(-1);

    SgStringVal *data = SageBuilder::buildStringVal_nfi(text);
    ASSERT_not_null(data);
    data->set_usesSingleQuotes(false);
    data->set_usesDoubleQuotes(false);
    data->set_parent(item);
    item->set_data(data);

    item->set_parent(itemList);
    itemList->get_format_item_list().push_back(item);
  };

  for (const auto &item : formatStmt.v.items) {
    appendItem(FormatItemToString(item));
  }
  if (!formatStmt.v.unlimitedItems.empty()) {
    appendItem("*(" + FormatItemsToString(formatStmt.v.unlimitedItems) + ")");
  }

  SgFormatStatement *stmt = new SgFormatStatement(itemList);
  ASSERT_not_null(stmt);
  itemList->set_parent(stmt);

  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  builder.setSourcePosition(stmt, srcBegin, srcEnd);

  builder.Leave(stmt, getLabels());
}

namespace {
SgStatementFunctionStatement *
BuildStatementFunctionStatement(const std::string &funcName,
                                const std::list<std::string> &dummyArgs,
                                SgExpression *rhs, SgScopeStatement *scope) {
  ASSERT_not_null(rhs);
  ASSERT_not_null(scope);

  SgType *returnType{nullptr};
  SgVariableSymbol *varSymbol =
      SageInterface::lookupVariableSymbolInParentScopes(funcName, scope);
  if (varSymbol != nullptr && varSymbol->get_declaration() != nullptr) {
    returnType = varSymbol->get_declaration()->get_type();
    if (varSymbol->get_scope() == scope) {
      scope->remove_symbol(varSymbol);
    }
  }
  if (returnType == nullptr) {
    returnType = SageBuilder::buildFortranImplicitType(funcName);
  }

  SgFunctionParameterList *paramList{nullptr};
  SgScopeStatement *paramScope{nullptr};
  bool isDefDecl{false};
  builder.Enter(paramList, paramScope, funcName, returnType, isDefDecl);
  builder.Leave(paramList, paramScope, dummyArgs);

  SgProcedureHeaderStatement *funcDecl =
      SageBuilder::buildNondefiningProcedureHeaderStatement(
          SgName(funcName), returnType, paramList,
          SgProcedureHeaderStatement::e_function_subprogram_kind, scope);
  ASSERT_not_null(funcDecl);
  funcDecl->set_functionParameterScope(isSgFunctionParameterScope(paramScope));

  SgStatementFunctionStatement *stmt =
      new SgStatementFunctionStatement(funcDecl, rhs);
  ASSERT_not_null(stmt);
  funcDecl->set_parent(stmt);
  rhs->set_parent(stmt);

  return stmt;
}
} // namespace

void BuildVisitor::Build(
    parser::Statement<common::Indirection<parser::StmtFunctionStmt>> &x) {
  using namespace Fortran::parser;

  auto &stmtNode = x.statement.value();
  const std::string funcName = std::get<Name>(stmtNode.t).ToString();

  std::list<std::string> dummyArgs;
  for (auto &arg : std::get<std::list<Name>>(stmtNode.t)) {
    dummyArgs.push_back(arg.ToString());
  }

  SgExpression *rhs{nullptr};
  WalkExpr(std::get<Scalar<Expr>>(stmtNode.t).thing, rhs);
  ASSERT_not_null(rhs);

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  SgStatementFunctionStatement *stmt =
      BuildStatementFunctionStatement(funcName, dummyArgs, rhs, scope);
  ASSERT_not_null(stmt);
  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  builder.setSourcePosition(stmt, srcBegin, srcEnd);

  ApplyStatementLabel(stmt, scope);
  SageInterface::appendStatement(stmt, scope);
}

void BuildVisitor::Build(parser::CallStmt &x) {
  std::string proc_name;
  std::list<SgExpression *> arg_list;
  SgExpression *designator{nullptr};

  if (x.chevrons) {
    ABORT_NO_IMPL;
  }

  Rose::builder::Build(x.call, arg_list, proc_name, designator);

  SgExprListExp *param_list = SageBuilderCpp17::buildExprListExp_nfi(arg_list);
  ASSERT_not_null(param_list);

  SgExprStatement *stmt{nullptr};
  auto labels = getLabels();
  if (designator != nullptr) {
    SgFunctionCallExp *call_expr =
        SageBuilder::buildFunctionCallExp_nfi(designator, param_list);
    ASSERT_not_null(call_expr);
    stmt = SageBuilder::buildExprStatement_nfi(call_expr);
    ApplyCurrentStatementSource(stmt);
    builder.Leave(stmt, labels);
  } else {
    builder.Enter(stmt, proc_name, param_list, /*abort_phrase*/ "");
    ApplyCurrentStatementSource(stmt);
    builder.Leave(stmt, labels);
  }
}

void BuildVisitor::Build(parser::WriteStmt &x) {
  SgWriteStatement *stmt = new SgWriteStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  if (x.iounit) {
    SgExpression *expr = BuildIoUnitExpr(x.iounit.value());
    stmt->set_unit(expr);
    expr->set_parent(stmt);
  }

  if (x.format) {
    std::optional<std::string> formatName =
        ExtractBareNameFromFormat(x.format.value());
    if (formatName &&
        ScopeHasNamelistGroup(SageBuilder::topScopeStack(), *formatName)) {
      SgExpression *expr = SageBuilder::buildDanglingVarRefExp(
          SgName(*formatName), SageBuilder::topScopeStack());
      ASSERT_not_null(expr);
      stmt->set_namelist(expr);
      expr->set_parent(stmt);
    } else {
      SgExpression *expr = BuildFormatExpr(x.format.value());
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }

  for (auto &spec : x.controls) {
    ApplyIoControlSpec(spec, /*readStmt*/ nullptr, /*writeStmt*/ stmt);
  }

  SgExprListExp *iolist = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(iolist);
  for (auto &item : x.items) {
    SgExpression *itemExpr{nullptr};
    Rose::builder::Build(item, itemExpr);
    ASSERT_not_null(itemExpr);
    AppendExpr(iolist, itemExpr);
  }
  stmt->set_io_stmt_list(iolist);
  iolist->set_parent(stmt);

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::ReadStmt &x) {
  SgReadStatement *stmt = new SgReadStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  if (x.iounit) {
    SgExpression *expr = BuildIoUnitExpr(x.iounit.value());
    stmt->set_unit(expr);
    expr->set_parent(stmt);
  }

  if (x.format) {
    std::optional<std::string> formatName =
        ExtractBareNameFromFormat(x.format.value());
    if (formatName &&
        ScopeHasNamelistGroup(SageBuilder::topScopeStack(), *formatName)) {
      SgExpression *expr = SageBuilder::buildDanglingVarRefExp(
          SgName(*formatName), SageBuilder::topScopeStack());
      ASSERT_not_null(expr);
      stmt->set_namelist(expr);
      expr->set_parent(stmt);
    } else {
      SgExpression *expr = BuildFormatExpr(x.format.value());
      stmt->set_format(expr);
      expr->set_parent(stmt);
    }
  }

  for (auto &spec : x.controls) {
    ApplyIoControlSpec(spec, /*readStmt*/ stmt, /*writeStmt*/ nullptr);
  }

  SgExprListExp *iolist = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(iolist);
  for (auto &item : x.items) {
    SgExpression *itemExpr{nullptr};
    Rose::builder::Build(item, itemExpr);
    ASSERT_not_null(itemExpr);
    AppendExpr(iolist, itemExpr);
  }
  stmt->set_io_stmt_list(iolist);
  iolist->set_parent(stmt);

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::OpenStmt &x) {
  SgOpenStatement *stmt = new SgOpenStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    ApplyConnectSpec(spec, stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::CloseStmt &x) {
  SgCloseStatement *stmt = new SgCloseStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    ApplyCloseSpec(spec, stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::RewindStmt &x) {
  SgRewindStatement *stmt = new SgRewindStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    ApplyPositionOrFlushSpec(spec, stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::BackspaceStmt &x) {
  SgBackspaceStatement *stmt = new SgBackspaceStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    ApplyPositionOrFlushSpec(spec, stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::EndfileStmt &x) {
  SgEndfileStatement *stmt = new SgEndfileStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    ApplyPositionOrFlushSpec(spec, stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::FlushStmt &x) {
  SgFlushStatement *stmt = new SgFlushStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    ApplyPositionOrFlushSpec(spec, stmt);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::WaitStmt &x) {
  SgWaitStatement *stmt = new SgWaitStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &spec : x.v) {
    common::visit(
        common::visitors{[&](const parser::FileUnitNumber &y) {
                           SgExpression *expr{nullptr};
                           WalkExpr(y.v, expr);
                           ASSERT_not_null(expr);
                           stmt->set_unit(expr);
                           expr->set_parent(stmt);
                         },
                         [&](const parser::ErrLabel &y) {
                           SgExpression *expr{nullptr};
                           Rose::builder::Build(y.v, expr);
                           ASSERT_not_null(expr);
                           stmt->set_err(expr);
                           expr->set_parent(stmt);
                         },
                         [&](const parser::MsgVariable &y) {
                           SgExpression *expr{nullptr};
                           WalkExpr(y.v, expr);
                           ASSERT_not_null(expr);
                           stmt->set_iomsg(expr);
                           expr->set_parent(stmt);
                         },
                         [&](const parser::StatVariable &y) {
                           SgExpression *expr{nullptr};
                           WalkExpr(y.v, expr);
                           ASSERT_not_null(expr);
                           stmt->set_iostat(expr);
                           expr->set_parent(stmt);
                         },
                         [&](const auto &) { /* ignore unsupported spec */ }},
        spec.u);
  }

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::InquireStmt &x) {
  SgInquireStatement *stmt = new SgInquireStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  common::visit(
      common::visitors{
          [&](const std::list<parser::InquireSpec> &specs) {
            for (auto &spec : specs) {
              common::visit(
                  common::visitors{
                      [&](const parser::FileUnitNumber &y) {
                        SgExpression *expr{nullptr};
                        WalkExpr(y.v, expr);
                        ASSERT_not_null(expr);
                        stmt->set_unit(expr);
                        expr->set_parent(stmt);
                      },
                      [&](const parser::FileNameExpr &y) {
                        SgExpression *expr{nullptr};
                        WalkExpr(y, expr);
                        ASSERT_not_null(expr);
                        stmt->set_file(expr);
                        expr->set_parent(stmt);
                      },
                      [&](const parser::InquireSpec::CharVar &y) {
                        SgExpression *expr{nullptr};
                        WalkExpr(std::get<1>(y.t), expr);
                        ASSERT_not_null(expr);
                        auto kind = std::get<0>(y.t);
                        switch (kind) {
                        case parser::InquireSpec::CharVar::Kind::Access:
                          stmt->set_access(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Action:
                          stmt->set_action(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Asynchronous:
                          stmt->set_asynchronous(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Blank:
                          stmt->set_blank(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Decimal:
                          stmt->set_decimal(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Delim:
                          stmt->set_delim(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Direct:
                          stmt->set_direct(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Form:
                          stmt->set_form(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Formatted:
                          stmt->set_formatted(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Iomsg:
                          stmt->set_iomsg(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Name:
                          stmt->set_name(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Pad:
                          stmt->set_pad(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Position:
                          stmt->set_position(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Read:
                          stmt->set_read(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Readwrite:
                          stmt->set_readwrite(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Sequential:
                          stmt->set_sequential(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Stream:
                          stmt->set_stream(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Unformatted:
                          stmt->set_unformatted(expr);
                          break;
                        case parser::InquireSpec::CharVar::Kind::Write:
                          stmt->set_write(expr);
                          break;
                        default:
                          break;
                        }
                        expr->set_parent(stmt);
                      },
                      [&](const parser::InquireSpec::IntVar &y) {
                        SgExpression *expr{nullptr};
                        WalkExpr(std::get<1>(y.t), expr);
                        ASSERT_not_null(expr);
                        auto kind = std::get<0>(y.t);
                        switch (kind) {
                        case parser::InquireSpec::IntVar::Kind::Iostat:
                          stmt->set_iostat(expr);
                          break;
                        case parser::InquireSpec::IntVar::Kind::Nextrec:
                          stmt->set_nextrec(expr);
                          break;
                        case parser::InquireSpec::IntVar::Kind::Number:
                          stmt->set_number(expr);
                          break;
                        case parser::InquireSpec::IntVar::Kind::Pos:
                          stmt->set_position(expr);
                          break;
                        case parser::InquireSpec::IntVar::Kind::Recl:
                          stmt->set_recl(expr);
                          break;
                        case parser::InquireSpec::IntVar::Kind::Size:
                          stmt->set_size(expr);
                          break;
                        default:
                          break;
                        }
                        expr->set_parent(stmt);
                      },
                      [&](const parser::InquireSpec::LogVar &y) {
                        SgExpression *expr{nullptr};
                        WalkExpr(std::get<1>(y.t), expr);
                        ASSERT_not_null(expr);
                        auto kind = std::get<0>(y.t);
                        switch (kind) {
                        case parser::InquireSpec::LogVar::Kind::Exist:
                          stmt->set_exist(expr);
                          break;
                        case parser::InquireSpec::LogVar::Kind::Named:
                          stmt->set_named(expr);
                          break;
                        case parser::InquireSpec::LogVar::Kind::Opened:
                          stmt->set_opened(expr);
                          break;
                        case parser::InquireSpec::LogVar::Kind::Pending:
                          stmt->set_pending(expr);
                          break;
                        }
                        expr->set_parent(stmt);
                      },
                      [&](const parser::IdExpr &y) {
                        SgExpression *expr{nullptr};
                        WalkExpr(y.v, expr);
                        ASSERT_not_null(expr);
                        stmt->set_id(expr);
                        expr->set_parent(stmt);
                      },
                      [&](const parser::ErrLabel &y) {
                        SgExpression *expr{nullptr};
                        Rose::builder::Build(y.v, expr);
                        ASSERT_not_null(expr);
                        stmt->set_err(expr);
                        expr->set_parent(stmt);
                      }},
                  spec.u);
            }
          },
          [&](parser::InquireStmt::Iolength &iolength) {
            SgExpression *lenExpr{nullptr};
            WalkExpr(std::get<0>(iolength.t), lenExpr);
            ASSERT_not_null(lenExpr);
            SgVarRefExp *lenVar = isSgVarRefExp(lenExpr);
            ROSE_ASSERT(lenVar != nullptr);
            stmt->set_iolengthExp(lenVar);
            lenVar->set_parent(stmt);

            auto &items = std::get<1>(iolength.t);
            if (!items.empty()) {
              SgExprListExp *iolist = SageBuilder::buildExprListExp_nfi();
              ASSERT_not_null(iolist);
              for (auto &item : items) {
                SgExpression *itemExpr{nullptr};
                Rose::builder::Build(item, itemExpr);
                ASSERT_not_null(itemExpr);
                AppendExpr(iolist, itemExpr);
              }
              stmt->set_io_stmt_list(iolist);
              iolist->set_parent(stmt);
            }
          }},
      x.u);

  ApplyStatementLabel(stmt, SageBuilder::topScopeStack());
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::PointerAssignmentStmt &x) {
  SgExpression *lhs{nullptr};
  Rose::builder::Build(std::get<parser::DataRef>(x.t), lhs);
  ASSERT_not_null(lhs);

  SgExpression *rhs{nullptr};
  WalkExpr(std::get<parser::Expr>(x.t), rhs);
  ASSERT_not_null(rhs);

  SgPointerAssignOp *assign = new SgPointerAssignOp(lhs, rhs, nullptr);
  ASSERT_not_null(assign);
  SageInterface::setSourcePosition(assign);
  lhs->set_parent(assign);
  rhs->set_parent(assign);

  SgExprStatement *stmt = SageBuilder::buildExprStatement_nfi(assign);
  ASSERT_not_null(stmt);
  assign->set_parent(stmt);

  auto labels = getLabels();
  builder.Leave(stmt, labels);
}

void BuildVisitor::Build(parser::NullifyStmt &x) {
  SgNullifyStatement *stmt = new SgNullifyStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  SgExprListExp *list = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(list);
  for (auto &obj : x.v) {
    SgExpression *expr{nullptr};
    common::visit(
        common::visitors{[&](const parser::Name &y) {
                           std::string name = y.ToString();
                           expr = SageBuilderCpp17::buildVarRefExp_nfi(name);
                         },
                         [&](parser::StructureComponent &y) {
                           Rose::builder::Build(y, expr);
                         }},
        obj.u);
    ASSERT_not_null(expr);
    AppendExpr(list, expr);
  }
  stmt->set_pointer_list(list);
  list->set_parent(stmt);

  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::BuildPrefix(
    std::list<parser::PrefixSpec> &x,
    LanguageTranslation::FunctionModifierList &modifiers, SgType *&type) {
  // std::variant<> - DeclarationTypeSpec, Elemental, Impure, Module,
  // Non_Recursive,
  //                  Pure, Recursive, Attributes, Launch_Bounds, Cluster_Dims
  auto add_modifier = [&](LanguageTranslation::FunctionModifier kind) {
    if (std::find(modifiers.begin(), modifiers.end(), kind) ==
        modifiers.end()) {
      modifiers.push_back(kind);
    }
  };
  auto remove_modifier = [&](LanguageTranslation::FunctionModifier kind) {
    auto it = std::remove(modifiers.begin(), modifiers.end(), kind);
    if (it != modifiers.end()) {
      modifiers.erase(it, modifiers.end());
    }
  };

  for (auto &prefix : x) {
    common::visit(
        common::visitors{
            [&](parser::DeclarationTypeSpec &y) { BuildType(y, type); },
            [&](const parser::PrefixSpec::Elemental &) {
              add_modifier(LanguageTranslation::FunctionModifier::
                               e_function_modifier_elemental);
            },
            [&](const parser::PrefixSpec::Impure &) {
              add_modifier(LanguageTranslation::FunctionModifier::
                               e_function_modifier_impure);
              remove_modifier(LanguageTranslation::FunctionModifier::
                                  e_function_modifier_pure);
            },
            [&](const parser::PrefixSpec::Module &) {
              add_modifier(LanguageTranslation::FunctionModifier::
                               e_function_modifier_module);
            },
            [&](const parser::PrefixSpec::Non_Recursive &) {
              remove_modifier(LanguageTranslation::FunctionModifier::
                                  e_function_modifier_recursive);
            },
            [&](const parser::PrefixSpec::Pure &) {
              add_modifier(LanguageTranslation::FunctionModifier::
                               e_function_modifier_pure);
            },
            [&](const parser::PrefixSpec::Recursive &) {
              add_modifier(LanguageTranslation::FunctionModifier::
                               e_function_modifier_recursive);
            },
            [&](const parser::PrefixSpec::Attributes &) {},
            [&](const parser::PrefixSpec::Launch_Bounds &) {},
            [&](const parser::PrefixSpec::Cluster_Dims &) {}},
        prefix.u);
  }
}

void BuildSuffix(parser::Suffix &x, std::string &resultName) {
  if (x.resultName) {
    resultName = x.resultName.value().ToString();
  }

  // TODO: handle LanguageBindingSpec in suffix when needed.
}

void Build(parser::Substring &x, SgExpression *&expr) {
  auto &dataRef = std::get<parser::DataRef>(x.t);
  auto &range = std::get<parser::SubstringRange>(x.t);

  SgExpression *base{nullptr};
  Build(dataRef, base);
  ASSERT_not_null(base);

  SgExpression *lower{nullptr};
  SgExpression *upper{nullptr};
  if (std::get<0>(range.t)) {
    WalkExpr(std::get<0>(range.t).value(), lower);
  }
  if (std::get<1>(range.t)) {
    WalkExpr(std::get<1>(range.t).value(), upper);
  }
  if (lower == nullptr) {
    lower = SageBuilderCpp17::buildNullExpression_nfi();
  }
  if (upper == nullptr) {
    upper = SageBuilderCpp17::buildNullExpression_nfi();
  }
  SgExpression *stride = SageBuilder::buildIntVal_nfi(std::string("1"));

  SgExpression *subscript =
      SageBuilderCpp17::buildSubscriptExpression_nfi(lower, upper, stride);
  ASSERT_not_null(subscript);

  expr = SageBuilderCpp17::buildPntrArrRefExp_nfi(base, subscript);
}

void Build(parser::Designator &x, SgExpression *&expr) {
  common::visit(common::visitors{[&](parser::DataRef &y) { Build(y, expr); },
                                 [&](parser::Substring &y) { Build(y, expr); }},
                x.u);
}

void Build(parser::DataRef &x, SgExpression *&expr) {
  common::visit(common::visitors{
                    [&](parser::Name &y) {
                      std::string name = y.ToString();
                      expr = SageBuilderCpp17::buildVarRefExp_nfi(name);
                    },
                    [&](common::Indirection<parser::StructureComponent> &y) {
                      Build(y.value(), expr);
                    },
                    [&](common::Indirection<parser::ArrayElement> &y) {
                      Build(y.value(), expr);
                    },
                    [&](common::Indirection<parser::CoindexedNamedObject> &y) {
                      Build(y.value(), expr);
                    }},
                x.u);
}

void Build(parser::FunctionReference &x, SgExpression *&expr) {
#if PRINT_FLANG_TRAVERSAL
  std::cout << "Rose::builder::Build(FunctionReference)\n";
#endif

  std::list<SgExpression *> arg_list;
  std::string func_name;

  SgExpression *designator{nullptr};
  Build(x.v, arg_list, func_name, designator); // Call

  SgExprListExp *param_list = SageBuilderCpp17::buildExprListExp_nfi(arg_list);

  SgExpression *call_expr = nullptr;
  if (designator != nullptr) {
    if (IsArrayDesignator(designator)) {
      call_expr = BuildArrayRefFromCallArgs(designator, param_list);
    } else {
      call_expr = SageBuilder::buildFunctionCallExp_nfi(designator, param_list);
    }
  } else {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    ASSERT_not_null(scope);
    if (SgVariableSymbol *var_sym =
            SageInterface::lookupVariableSymbolInParentScopes(func_name,
                                                              scope)) {
      SgType *var_type = var_sym->get_type();
      if (IsArrayType(var_type)) {
        SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
        call_expr = BuildArrayRefFromCallArgs(var_ref, param_list);
      } else if (IsFunctionType(var_type)) {
        SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
        call_expr = SageBuilder::buildFunctionCallExp_nfi(var_ref, param_list);
      }
    }
    if (call_expr == nullptr) {
      call_expr =
          BuildFunctionCallFromSymbolIfFound(func_name, scope, param_list);
    }
    if (call_expr == nullptr) {
      call_expr = SageBuilder::buildFunctionCallExp(
          SgName(func_name), SageBuilder::buildUnknownType(), param_list,
          SageBuilder::topScopeStack());
    }
  }

  expr = call_expr;
}

void Build(parser::Call &x, std::list<SgExpression *> &arg_list,
           std::string &name, SgExpression *&designator) {
  using namespace Fortran::parser;

  designator = nullptr;

  // ProcedureDesignator std::variant<Name, ProcComponentRef> u;
  common::visit(common::visitors{[&](Name &procName) {
                                   name = procName.ToString();
                                   designator = nullptr;
                                 },
                                 [&](ProcComponentRef &procRef) {
                                   name.clear();
                                   Build(procRef, designator);
                                 }},
                std::get<ProcedureDesignator>(x.t).u);

  // ActualArgSpec std::tuple<std::optional<Keyword>, ActualArg> t;
  for (auto &spec : std::get<std::list<ActualArgSpec>>(x.t)) {
    const auto &keywordOpt = std::get<std::optional<Keyword>>(spec.t);
    const bool hasKeyword = keywordOpt.has_value();
    std::string keywordName;
    if (hasKeyword) {
      keywordName = keywordOpt->v.ToString();
    }

    auto append_arg = [&](SgExpression *arg) {
      ASSERT_not_null(arg);
      if (hasKeyword) {
        arg = SageBuilder::buildActualArgumentExpression_nfi(
            SgName(keywordName), arg);
      }
      arg_list.push_back(arg);
    };

    auto &actual = std::get<ActualArg>(spec.t);
    if (auto *expr = std::get_if<common::Indirection<Expr>>(&actual.u)) {
      SgExpression *arg{nullptr};
      WalkExpr(expr->value(), arg);
      append_arg(arg);
    } else if (auto *percentVal =
                   std::get_if<ActualArg::PercentVal>(&actual.u)) {
      SgExpression *arg{nullptr};
      WalkExpr(percentVal->v, arg);
      append_arg(arg);
    } else if (auto *percentRef =
                   std::get_if<ActualArg::PercentRef>(&actual.u)) {
      SgExpression *arg{nullptr};
      WalkExpr(percentRef->v, arg);
      append_arg(arg);
    } else if (auto *altReturn = std::get_if<AltReturnSpec>(&actual.u)) {
      SgScopeStatement *currentScope = SageBuilder::topScopeStack();
      ASSERT_not_null(currentScope);
      SgScopeStatement *labelScope =
          SageInterface::getEnclosingFunctionDefinition(currentScope, true);
      if (labelScope == nullptr) {
        labelScope = SageInterface::getEnclosingScope(currentScope, true);
      }
      ASSERT_not_null(labelScope);
      const auto labelValue = static_cast<int>(altReturn->v);
      const std::string labelText = StringUtility::numberToString(labelValue);
      SgLabelSymbol *labelSymbol =
          GetOrCreateFortranLabelSymbol(labelValue, labelScope);
      SgLabelRefExp *labelRef = SageBuilder::buildLabelRefExp(labelSymbol);
      ASSERT_not_null(labelRef);
      append_arg(labelRef);
    } else {
      ABORT_NO_IMPL;
    }
  }
}

void Build(parser::ProcComponentRef &x, SgExpression *&expr) {
  Build(x.v.thing, expr);
}

void Build(parser::ActualArgSpec &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(ActualArgSpec)\n";
  ABORT_NO_IMPL;
}

void Build(parser::Keyword &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(Keyword)");
  ABORT_NO_IMPL;
}

void Build(parser::NamedConstant &x, SgExpression *&expr) {
  std::string name = x.v.ToString();
  expr = SageBuilderCpp17::buildVarRefExp_nfi(name);
}

void Build(parser::Expr::IntrinsicBinary &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(IntrinsicBinary)\n";
  ABORT_NO_IMPL;
}

// LiteralConstant(s)
void BuildImpl(parser::HollerithLiteralConstant &x, SgExpression *&expr) {
  expr = SageBuilder::buildStringVal_nfi(x.GetString());
}

// KindParam - for now create a string (seems that a Sage value expression
// should have Fortran kind
void BuildImpl(const std::optional<Fortran::parser::KindParam> &x,
               std::uint64_t &ikind, std::string &strVal) {
  // std::variant<> = std::uint64_t, Scalar<Integer<Constant<Name>>>
  using namespace Fortran::parser;

  if (x) {
    common::visit(
        common::visitors{[&](const std::int64_t &y) {
                           ikind = y;
                           strVal = std::to_string(ikind);
                         },
                         [&](const Scalar<Integer<Constant<Name>>> &y) {
                           strVal = y.thing.thing.thing.ToString();
                         }},
        x.value().u);
  }
}

void BuildImpl(Fortran::parser::KindParam &x, SgExpression *&expr) {
  // std::variant<> = std::uint64_t, Scalar<Integer<Constant<Name>>>
  using namespace Fortran::parser;

  ASSERT_require(expr == nullptr);
  common::visit(
      common::visitors{[&](const std::int64_t &y) {
                         const std::string value = std::to_string(y);
                         expr = SageBuilder::buildLongLongIntVal_nfi(y, value);
                       },
                       [&](const std::uint64_t &y) {
                         const std::string value = std::to_string(y);
                         expr = SageBuilder::buildLongLongIntVal_nfi(
                             static_cast<long long>(y), value);
                       },
                       [&](const Scalar<Integer<Constant<Name>>> &y) {
                         std::string name = y.thing.thing.thing.ToString();
                         expr = SageBuilderCpp17::buildVarRefExp_nfi(
                             name, /*scope*/ nullptr, /*allow_implicit*/ false);
                       }},
      x.u);
}

void BuildImpl(parser::IntLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<> - CharBlock, std::optional<KindParam>
  using namespace Fortran::parser;

  // Use long long as integer representation because we don't really know
  // except for default integer and kind param (compiler dependent).
  long long llVal{0};
  std::string strVal{std::get<CharBlock>(x.t).ToString()};

  try {
    llVal = std::stoll(strVal);
  } catch (const std::out_of_range &e) {
    std::cerr << "[WARN] IntLiteralConstant out of range: " << e.what()
              << std::endl;
  } catch (const std::invalid_argument &e) {
    std::cerr << "[WARN] IntLiteralConstant invalid argument: " << e.what()
              << std::endl;
  }

  if (std::get<1>(x.t)) {
    std::string kind{};
    uint64_t ikind{0};
    BuildImpl(std::get<1>(x.t), ikind, kind);
    strVal += "_" + kind;
  }

  expr = SageBuilder::buildLongLongIntVal_nfi(llVal, strVal);
}

void BuildImpl(parser::UnsignedLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<> - CharBlock, std::optional<KindParam>
  using namespace Fortran::parser;

  unsigned long long ullVal{0};
  std::string strVal{std::get<CharBlock>(x.t).ToString()};

  try {
    ullVal = std::stoull(strVal);
  } catch (const std::out_of_range &e) {
    std::cerr << "[WARN] UnsignedLiteralConstant out of range: " << e.what()
              << std::endl;
  } catch (const std::invalid_argument &e) {
    std::cerr << "[WARN] UnsignedLiteralConstant invalid argument: " << e.what()
              << std::endl;
  }

  if (std::get<1>(x.t)) {
    std::string kind{};
    uint64_t ikind{0};
    BuildImpl(std::get<1>(x.t), ikind, kind);
    strVal += "_" + kind;
  }

  expr = SageBuilder::buildUnsignedLongLongIntVal_nfi(ullVal, strVal);
}

void BuildImpl(parser::SignedIntLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<> - CharBlock, std::optional<KindParam>
  std::string strVal = std::get<0>(x.t).ToString();
  std::string parsedVal;
  parsedVal.reserve(strVal.size());
  for (char ch : strVal) {
    if (ch == '_' || std::isspace(static_cast<unsigned char>(ch))) {
      continue;
    }
    parsedVal.push_back(ch);
  }

  long long llVal{0};
  try {
    llVal = std::stoll(parsedVal);
  } catch (const std::out_of_range &e) {
    std::cerr << "[WARN] SignedIntLiteralConstant out of range: " << e.what()
              << std::endl;
  } catch (const std::invalid_argument &e) {
    std::cerr << "[WARN] SignedIntLiteralConstant invalid argument: "
              << e.what() << std::endl;
  }
  if (std::get<1>(x.t)) {
    std::string kind{};
    uint64_t ikind{0};
    BuildImpl(std::get<1>(x.t), ikind, kind);
    strVal += "_" + kind;
  }
  expr = SageBuilder::buildLongLongIntVal_nfi(llVal, strVal);
}

namespace {
std::string FormatRealLiteralConstant(const parser::RealLiteralConstant &x) {
  std::string value = x.real.source.ToString();
  if (x.kind) {
    std::string kind{};
    uint64_t ikind{0};
    BuildImpl(x.kind, ikind, kind);
    if (!kind.empty()) {
      value += "_" + kind;
    }
  }
  return value;
}
} // namespace

void BuildImpl(parser::RealLiteralConstant &x, SgExpression *&expr) {
  expr = SageBuilder::buildFloatVal_nfi(FormatRealLiteralConstant(x));
}

void BuildImpl(parser::SignedRealLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<std::optional<Sign>, RealLiteralConstant> t;
  std::string value = FormatRealLiteralConstant(std::get<1>(x.t));
  if (std::get<0>(x.t)) {
    if (std::get<0>(x.t).value() == parser::Sign::Negative) {
      value = "-" + value;
    }
  }
  expr = SageBuilder::buildFloatVal_nfi(value);
}

void BuildImpl(parser::ComplexLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<ComplexPart, ComplexPart> t;
  auto buildComplexPart = [](parser::ComplexPart &part) -> SgExpression * {
    SgExpression *partExpr{nullptr};
    common::visit(
        common::visitors{[&](parser::SignedIntLiteralConstant &y) {
                           BuildImpl(y, partExpr);
                         },
                         [&](parser::SignedRealLiteralConstant &y) {
                           BuildImpl(y, partExpr);
                         },
                         [&](parser::NamedConstant &y) { Build(y, partExpr); }},
        part.u);
    return partExpr;
  };

  SgExpression *realPart = buildComplexPart(std::get<0>(x.t));
  SgExpression *imagPart = buildComplexPart(std::get<1>(x.t));
  ASSERT_not_null(realPart);
  ASSERT_not_null(imagPart);

  if (isSgValueExp(realPart) && isSgValueExp(imagPart)) {
    expr = SageBuilderCpp17::buildComplexVal_nfi(realPart, imagPart, "");
    return;
  }

  std::list<SgExpression *> args{realPart, imagPart};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  expr = SageBuilder::buildFunctionCallExp(
      SgName("cmplx"), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
}

void BuildImpl(parser::SignedComplexLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<Sign, ComplexLiteralConstant> t;
  SgExpression *complexExpr{nullptr};
  BuildImpl(std::get<1>(x.t), complexExpr);
  ASSERT_not_null(complexExpr);
  if (std::get<0>(x.t) == parser::Sign::Negative) {
    expr = SageBuilder::buildMinusOp_nfi(complexExpr);
  } else {
    expr = complexExpr;
  }
}

void BuildImpl(parser::BOZLiteralConstant &x, SgExpression *&expr) {
  std::string value = x.v;
  int base = 10;
  if (!value.empty()) {
    char prefix = std::toupper(value.front());
    if (prefix == 'B') {
      base = 2;
    } else if (prefix == 'O') {
      base = 8;
    } else if (prefix == 'Z') {
      base = 16;
    }
  }
  auto start = value.find_first_of("'\"");
  auto end = value.find_last_of("'\"");
  std::string digits = value;
  if (start != std::string::npos && end != std::string::npos && end > start) {
    digits = value.substr(start + 1, end - start - 1);
  }
  digits.erase(
      std::remove_if(digits.begin(), digits.end(),
                     [](char c) { return c == '_' || std::isspace(c); }),
      digits.end());
  long long llVal = 0;
  if (!digits.empty()) {
    llVal = std::stoll(digits, nullptr, base);
  }
  expr = SageBuilder::buildLongLongIntVal_nfi(llVal, value);
}

void BuildImpl(parser::CharLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<std::optional<KindParam>, std::string> t;
  std::string value = x.GetString();
  std::string escaped;
  escaped.reserve(value.size());
  for (char ch : value) {
    if (ch == '\'') {
      escaped.push_back('\'');
    }
    escaped.push_back(ch);
  }

  SgStringVal *stringVal = SageBuilder::buildStringVal_nfi(escaped);
  ASSERT_not_null(stringVal);
  stringVal->set_usesSingleQuotes(true);
  stringVal->set_usesDoubleQuotes(false);
  expr = stringVal;
  if (std::get<0>(x.t)) {
    SgExpression *kindExpr{nullptr};
    BuildImpl(std::get<0>(x.t).value(), kindExpr);
  }
}

void BuildImpl(parser::LogicalLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<> bool, std::optional<KindParam>
  expr = SageBuilder::buildBoolValExp_nfi(std::get<0>(x.t));
  if (std::get<1>(x.t)) {
    SgExpression *kindExpr{nullptr};
    BuildImpl(std::get<1>(x.t).value(), kindExpr);
  }
}

void BuildImpl(parser::KindSelector::StarSize &x, SgExpression *&expr) {
  // StarSize std::uint64_t v
  uint64_t kind{x.v};
  std::string strVal{std::to_string(kind)};
  expr = SageBuilder::buildLongLongIntVal_nfi(kind, strVal);
}

void BuildImpl(parser::TypeParamValue &x, SgExpression *&expr) {
  // TypeParamValue std::variant<ScalarIntExpr, Star, Deferred> u;
  // Star is "*", Deferred is ":"
  common::visit(common::visitors{[&](parser::ScalarIntExpr &y) {
                                   // Walking inside of a visitor not fully
                                   // explored, for now make sure no previous
                                   // value for expr
                                   ASSERT_require(expr == nullptr);
                                   WalkExpr(y, expr);
                                 },
                                 [&](parser::Star &y) {
                                   expr = new SgAsteriskShapeExp();
                                   SageInterface::setSourcePosition(expr);
                                 },
                                 [&](parser::TypeParamValue::Deferred &y) {
                                   expr = new SgColonShapeExp();
                                   SageInterface::setSourcePosition(expr);
                                 }},
                x.u);
}

void BuildImpl(parser::CharLength &x, SgExpression *&expr) {
  // CharLength std::variant<TypeParamValue, std::uint64_t> u;
  using namespace Fortran;

  common::visit(
      common::visitors{[&](std::uint64_t &y) {
                         std::string strVal{std::to_string(y)};
                         expr = SageBuilder::buildLongLongIntVal_nfi(y, strVal);
                       },
                       [&](parser::TypeParamValue &y) { WalkExpr(y, expr); }},
      x.u);
}

void BuildImpl(parser::CommonBlockObject &x, SgExpression *&expr) {
  // CommonBlockObject std::tuple<Name, std::optional<ArraySpec>> t;

  std::string name{std::get<parser::Name>(x.t).ToString()};

  SgExpression *refExpr = SageBuilderCpp17::buildVarRefExp_nfi(name);
  SgVarRefExp *ref = isSgVarRefExp(refExpr);
  ASSERT_not_null(ref);
  SgVariableSymbol *symbol = ref->get_symbol();
  ASSERT_not_null(symbol);

  // ArraySpec acts like a DIMENSION specification; update the declaration type.
  if (auto &opt = std::get<std::optional<parser::ArraySpec>>(x.t)) {
    SgInitializedName *initName = symbol->get_declaration();
    ASSERT_not_null(initName);
    if (!IsArrayType(initName->get_type())) {
      SgType *baseType = initName->get_type();
      ASSERT_not_null(baseType);
      SgType *arrayType{nullptr};
      Build(opt.value(), arrayType, baseType);
      ASSERT_not_null(arrayType);
      initName->set_type(arrayType);
    }
  }

  expr = ref;
}

void BuildImpl(parser::AssumedImpliedSpec &x, SgExpression *&expr) {
  // is [ lower-bound : ] *
  // AssumedImpliedSpec std::optional<SpecificationExpr> v;
  SgExpression *ub{SageBuilderCpp17::buildAsteriskShapeExp_nfi()};
  expr = ub;

  // There may be a lower-bound
  if (x.v) {
    SgExpression *lb{nullptr};
    SgExpression *stride{
        SageBuilder::buildIntVal_nfi(std::string("1"))}; // default stride of 1
    WalkExpr(x.v, lb);
    expr = SageBuilder::buildSubscriptExpression_nfi(lb, ub, stride);
  }
}

void BuildImpl(parser::AssumedShapeSpec &x, SgExpression *&expr) {
  // is [ lower-bound ] :
  // AssumedShapeSpec std::optional<SpecificationExpr> v;
  SgExpression *lb{nullptr};                                // maybe lower-bound
  SgExpression *ub{SageBuilder::buildNullExpression_nfi()}; // no upper-bound
  SgExpression *stride{
      SageBuilder::buildIntVal_nfi(std::string("1"))}; // default stride of 1

  if (x.v) {
    WalkExpr(x.v, lb);
  }
  expr = SageBuilder::buildSubscriptExpression_nfi(lb, ub, stride);
}

void BuildImpl(parser::ExplicitShapeSpec &x, SgExpression *&expr) {
  // is [ lower-bound : ] upper-bound
  // ExplicitShapeSpec std::tuple<std::optional<SpecificationExpr>,
  // SpecificationExpr> t;

  // There shall be an upper-bound
  SgExpression *ub{nullptr};
  WalkExpr(std::get<1>(x.t), ub);
  expr = ub;

  // There may be a lower-bound
  if (std::get<0>(x.t)) {
    SgExpression *lb{nullptr};
    SgExpression *stride{
        SageBuilder::buildIntVal_nfi(std::string("1"))}; // default stride of 1
    WalkExpr(std::get<0>(x.t).value(), lb);
    expr = SageBuilder::buildSubscriptExpression_nfi(lb, ub, stride);
  }
}

void BuildImpl(parser::UseStmt &x) {
  std::cout << "BuildImpl(UseStmt)\n";
  ABORT_NO_TEST;
}

void BuildImpl(const parser::InternalSubprogramPart &x) {
  // std::tuple<> - Statement<ContainsStmt>, std::list<InternalSubprogram>
  std::cout << "Rose::builder::Build(InternalSubprogramPart)\n";
  ABORT_NO_TEST;
}

void Build(
    std::list<parser::ImplicitSpec> &x,
    std::list<
        std::tuple<SgType *, std::list<std::tuple<char, std::optional<char>>>>>
        &implicit_spec_list) {
  for (auto &spec : x) {
    SgType *type{nullptr};
    std::list<std::tuple<char, std::optional<char>>> letter_spec_list;
    Build(spec, type, letter_spec_list);
    ASSERT_not_null(type);
    implicit_spec_list.push_back(std::make_tuple(type, letter_spec_list));
  }
}

void Build(parser::ImplicitSpec &x, SgType *&type,
           std::list<std::tuple<char, std::optional<char>>> &letter_spec_list) {
  // std::tuple<DeclarationTypeSpec, std::list<LetterSpec>> t;
  BuildVisitor typeBuilder;
  typeBuilder.BuildType(std::get<parser::DeclarationTypeSpec>(x.t), type);
  Build(std::get<std::list<parser::LetterSpec>>(x.t), letter_spec_list);
}

void Build(std::list<parser::LetterSpec> &x,
           std::list<std::tuple<char, std::optional<char>>> &letter_spec_list) {
  for (auto &spec : x) {
    std::tuple<char, std::optional<char>> letter_spec;
    Build(spec, letter_spec);
    letter_spec_list.push_back(letter_spec);
  }
}

void Build(parser::LetterSpec &x,
           std::tuple<char, std::optional<char>> &letter_spec) {
  // std::tuple<Location, std::optional<Location>> t;
  // using Location = const char *;
  char first = '\0';
  if (const char *loc = std::get<0>(x.t)) {
    first = *loc;
  }
  std::optional<char> second = std::nullopt;
  if (std::get<1>(x.t)) {
    if (const char *loc = std::get<1>(x.t).value()) {
      second = *loc;
    }
  }
  letter_spec = std::make_tuple(first, second);
}

#define TEMPORARY_COOL_FIXME 0
#if TEMPORARY_COOL_FIXME
const Fortran::parser::ImplicitStmt::ImplicitNoneNameSpec &makeImplicitNone() {
  //  return
  //  std::move(Fortran::parser::ImplicitStmt::ImplicitNoneNameSpec::External);
  return Fortran::parser::ImplicitStmt::ImplicitNoneNameSpec::External;
}
#endif

void BuildVisitor::Build(parser::ImplicitStmt &x) {
  // std::variant<> std::list<ImplicitSpec>, std::list<ImplicitNoneNameSpec>
  using namespace Fortran::parser;
  bool implicitNone{false}, implicitExternal{false}, implicitType{false};

  common::visit(
      common::visitors{
          [&](std::list<ImplicitSpec> &y) {
            std::list<std::tuple<
                SgType *, std::list<std::tuple<char, std::optional<char>>>>>
                implicit_spec_list;
            Rose::builder::Build(y, implicit_spec_list);
            SgImplicitStatement *stmt{nullptr};
            builder.Enter(stmt, implicit_spec_list);
            builder.Leave(stmt);
          },
          [&](const std::list<ImplicitStmt::ImplicitNoneNameSpec> &y) {
            // ENUM_CLASS(ImplicitNoneNameSpec, External, Type) // R866
            implicitNone = true;
            for (const auto &spec : y) {
              if (spec == ImplicitStmt::ImplicitNoneNameSpec::External)
                implicitExternal = true;
              if (spec == ImplicitStmt::ImplicitNoneNameSpec::Type)
                implicitType = true;
            }
          }},
      x.u);

  if (implicitNone) {
    SgImplicitStatement *stmt{nullptr};
    builder.Enter(stmt, implicitExternal, implicitType);
    builder.Leave(stmt);
  }
}

void BuildVisitor::Build(
    parser::Statement<common::Indirection<parser::UseStmt>> &x) {
  using namespace Fortran::parser;

  SgScopeStatement *currentScope = SageBuilder::topScopeStack();
  ASSERT_not_null(currentScope);

  UseStmt &useStmtNode = x.statement.value();
  std::string moduleName{useStmtNode.moduleName.ToString()};
  bool hasOnly = std::holds_alternative<std::list<Only>>(useStmtNode.u);

  SgUseStatement *useStmt = new SgUseStatement(moduleName, hasOnly, "");
  ASSERT_not_null(useStmt);

  if (useStmtNode.nature) {
    std::string nature{UseStmt::EnumToString(*useStmtNode.nature)};
    useStmt->set_module_nature(nature);
  }

  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  builder.setSourcePosition(useStmt, srcBegin, srcEnd);

  SgModuleStatement *moduleStmt = lookupModuleStatement(moduleName);
  if (moduleStmt == nullptr) {
    moduleStmt = FlangModuleInfo::getModule(moduleName);
    if (moduleStmt == nullptr) {
      const std::string location = formatSourcePosition(srcBegin);
      std::cerr << "Error: cannot find module '" << moduleName << "'";
      if (!location.empty()) {
        std::cerr << " at " << location;
      }
      std::cerr << ". Ensure module files are in the search path (use -I) or "
                   "available as .rmod/.rcmp.\n";
    }
  }

  if (moduleStmt != nullptr) {
    useStmt->set_module(moduleStmt);
  }

  std::vector<SgRenamePair *> renamePairs;
  if (hasOnly) {
    const auto &onlyList = std::get<std::list<Only>>(useStmtNode.u);
    for (const auto &only : onlyList) {
      renamePairs.push_back(buildOnlyRenamePair(only));
    }
  } else {
    const auto &renameList = std::get<std::list<Rename>>(useStmtNode.u);
    for (const auto &rename : renameList) {
      renamePairs.push_back(buildRenamePair(rename));
    }
  }

  auto attachRenamePair = [&](SgRenamePair *renamePair) {
    useStmt->get_rename_list().push_back(renamePair);
    renamePair->set_parent(useStmt);
  };

  if (moduleStmt == nullptr) {
    for (SgRenamePair *renamePair : renamePairs) {
      attachRenamePair(renamePair);
    }
  } else {
    SgClassDefinition *classDefinition = moduleStmt->get_definition();
    ROSE_ASSERT(classDefinition != nullptr);

    const bool caseInsensitive = currentScope->isCaseInsensitive();
    const PublicSymbolMap publicSymbols =
        collectPublicSymbols(classDefinition, caseInsensitive);

    auto findPublicSymbols = [&](const SgName &name) -> const SymbolList * {
      auto it = publicSymbols.find(symbolKey(name, caseInsensitive));
      if (it == publicSymbols.end()) {
        return nullptr;
      }
      return &it->second;
    };

    if (!hasOnly) {
      if (renamePairs.empty()) {
        for (const auto &entry : publicSymbols) {
          for (SgSymbol *symbol : entry.second) {
            SgName symbolName = symbol->get_name();
            if (!currentScope->symbol_exists(symbolName)) {
              SgSymbol *useSymbol = nullptr;
              if (SgVariableSymbol *varSymbol =
                      resolveUseAssociatedVariableSymbol(symbol)) {
                useSymbol = buildUseAssociatedVariableSymbol(
                    varSymbol, symbolName, currentScope);
              } else {
                useSymbol = new SgAliasSymbol(symbol, false);
              }
              currentScope->insert_symbol(symbolName, useSymbol);
            }
          }
        }
      } else {
        std::set<SgSymbol *> renamedSymbols;
        for (SgRenamePair *renamePair : renamePairs) {
          attachRenamePair(renamePair);

          const SymbolList *symbols =
              findPublicSymbols(renamePair->get_use_name());
          if (symbols == nullptr) {
            continue;
          }
          for (SgSymbol *symbol : *symbols) {
            SgName localName = renamePair->get_local_name();
            if (!currentScope->symbol_exists(localName)) {
              SgSymbol *aliasSymbol = nullptr;
              if (SgVariableSymbol *varSymbol =
                      resolveUseAssociatedVariableSymbol(symbol)) {
                aliasSymbol = buildUseAssociatedVariableSymbol(
                    varSymbol, localName, currentScope);
              } else {
                aliasSymbol = new SgAliasSymbol(symbol, true, localName);
              }
              currentScope->insert_symbol(localName, aliasSymbol);
            }
            renamedSymbols.insert(symbol);
          }
        }

        for (const auto &entry : publicSymbols) {
          for (SgSymbol *symbol : entry.second) {
            if (renamedSymbols.find(symbol) == renamedSymbols.end()) {
              SgName symbolName = symbol->get_name();
              SgSymbol *useSymbol = nullptr;
              if (SgVariableSymbol *varSymbol =
                      resolveUseAssociatedVariableSymbol(symbol)) {
                useSymbol = buildUseAssociatedVariableSymbol(
                    varSymbol, symbolName, currentScope);
              } else {
                useSymbol = new SgAliasSymbol(symbol, false);
              }
              currentScope->insert_symbol(symbolName, useSymbol);
            }
          }
        }
      }
    } else {
      for (SgRenamePair *renamePair : renamePairs) {
        attachRenamePair(renamePair);
        const SymbolList *symbols =
            findPublicSymbols(renamePair->get_use_name());
        if (symbols == nullptr) {
          continue;
        }
        bool isRenamed =
            (renamePair->get_use_name() != renamePair->get_local_name());
        for (SgSymbol *symbol : *symbols) {
          SgName localName = renamePair->get_local_name();
          if (!currentScope->symbol_exists(localName)) {
            SgSymbol *aliasSymbol = nullptr;
            if (SgVariableSymbol *varSymbol =
                    resolveUseAssociatedVariableSymbol(symbol)) {
              aliasSymbol = buildUseAssociatedVariableSymbol(
                  varSymbol, localName, currentScope);
            } else {
              aliasSymbol = isRenamed
                                ? new SgAliasSymbol(symbol, true, localName)
                                : new SgAliasSymbol(symbol, false);
            }
            currentScope->insert_symbol(localName, aliasSymbol);
          }
        }
      }
    }
  }

  builder.Leave(useStmt);
  use_statement_fixup(currentScope);
}

void BuildVisitor::Build(
    parser::Statement<common::Indirection<parser::ImportStmt>> &x) {
  using namespace Fortran::parser;

  SgScopeStatement *currentScope = SageBuilder::topScopeStack();
  ASSERT_not_null(currentScope);

  ImportStmt &importStmtNode = x.statement.value();
  SgImportStatement *importStmt = new SgImportStatement();
  ASSERT_not_null(importStmt);
  importStmt->set_definingDeclaration(importStmt);
  importStmt->set_firstNondefiningDeclaration(importStmt);

  SourcePosition srcBegin{BuildSourcePosition(x, Order::begin)};
  SourcePosition srcEnd{BuildSourcePosition(x, Order::end)};
  if (SgSourceFile *enclosingFile =
          SageInterface::getEnclosingSourceFile(currentScope, true)) {
    std::string enclosingPath =
        NormalizeSourcePath(enclosingFile->getFileName());
    if (!enclosingPath.empty() && srcBegin.path != enclosingPath) {
      srcBegin.path = enclosingPath;
    }
    if (!enclosingPath.empty() && srcEnd.path != enclosingPath) {
      srcEnd.path = enclosingPath;
    }
  }
  builder.setSourcePosition(importStmt, srcBegin, srcEnd);
  if (SgSourceFile *enclosingFile =
          SageInterface::getEnclosingSourceFile(currentScope, true)) {
    if (Sg_File_Info *startInfo = importStmt->get_startOfConstruct()) {
      startInfo->set_file_id(enclosingFile->get_file_info()->get_file_id());
      startInfo->set_physical_file_id(
          enclosingFile->get_file_info()->get_physical_file_id());
    }
    if (Sg_File_Info *endInfo = importStmt->get_endOfConstruct()) {
      endInfo->set_file_id(enclosingFile->get_file_info()->get_file_id());
      endInfo->set_physical_file_id(
          enclosingFile->get_file_info()->get_physical_file_id());
    }
  }

  for (const auto &name : importStmtNode.names) {
    SgVarRefExp *varRef =
        SageBuilder::buildVarRefExp(name.ToString(), currentScope);
    ASSERT_not_null(varRef);
    importStmt->get_import_list().push_back(varRef);
    varRef->set_parent(importStmt);
  }

  SageInterface::appendStatement(importStmt, currentScope);
}

void BuildVisitor::Build(parser::CommonStmt &x) {
  // CommonStmt std::list<Block> blocks;
  // Block std::tuple<std::optional<Name>, std::list<CommonBlockObject>> t;

  std::list<SgCommonBlockObject *> commonBlocks;

  for (auto &block : x.blocks) {
    std::string blockName{""};
    SgExprListExp *blockObjects{SageBuilder::buildExprListExp_nfi()};
    ASSERT_not_null(blockObjects);

    if (std::get<std::optional<parser::Name>>(block.t)) {
      blockName = std::get<std::optional<parser::Name>>(block.t)->ToString();
    }

    // CommonBlockObject(s)
    for (auto &object :
         std::get<std::list<parser::CommonBlockObject>>(block.t)) {
      SgExpression *varRef{nullptr};
      WalkExpr(object, varRef);
      ASSERT_not_null(varRef);
      SageInterface::appendExpression(blockObjects, varRef);
    }

    SgCommonBlockObject *sageObject =
        SageBuilder::buildCommonBlockObject(blockName, blockObjects);
    ASSERT_not_null(sageObject);
    commonBlocks.push_back(sageObject);
  }

  SgCommonBlock *blockStmt{nullptr};
  builder.Enter(blockStmt, commonBlocks);
  builder.Leave(blockStmt);
}

void BuildVisitor::Build(parser::DataStmt &x) {
  // DataStmt std::list<DataStmtSet> v
  // DataStmtSet std::tuple<std::list<DataStmtObject>, std::list<DataStmtValue>>
  // t; DataStmtObject std::variant<common::Indirection<Variable>,
  // DataImpliedDo> u; DataStmtValue mutable std::int64_t repetitions{1};
  //   std::tuple<std::optional<DataStmtRepeat>, DataStmtConstant> t;
  // DataStmtConstant
  //   std::variant<> LiteralConstant, SignedIntLiteralConstant,
  //   SignedRealLiteralConstant, SignedComplexLiteralConstant, NullInit,
  //   common::Indirection<Designator>, StructureConstructor

  SgAttributeSpecificationStatement *dataStmt =
      SageBuilder::buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_dataStatement);
  ASSERT_not_null(dataStmt);
  SageInterface::appendStatement(dataStmt, SageBuilder::topScopeStack());

  // An SgDataStatementGroup corresponds to a DataStmtSet
  auto &groups{dataStmt->get_data_statement_group_list()};

  for (auto &set : x.v) {
    SgDataStatementGroup *dataGroup = new SgDataStatementGroup();
    ASSERT_not_null(dataGroup);

    // Add data statement groups
    for (auto &setObject : std::get<0>(set.t)) {
      SgExpression *expr{nullptr};
      Rose::builder::Build(setObject, expr);
      ASSERT_not_null(expr);

      SgDataStatementObject *dataObject = new SgDataStatementObject();
      ASSERT_not_null(dataObject);
      if (!dataObject->get_variableReference_list()) {
        dataObject->set_variableReference_list(
            SageBuilder::buildExprListExp_nfi());
      }

      SageInterface::appendExpression(dataObject->get_variableReference_list(),
                                      expr);

      dataGroup->get_object_list().push_back(dataObject);
    }

    // Add data statement values
    for (auto &setValue : std::get<1>(set.t)) {
      auto &repeatOpt =
          std::get<std::optional<parser::DataStmtRepeat>>(setValue.t);
      auto &constant = std::get<parser::DataStmtConstant>(setValue.t);

      SgDataStatementValue *dataValue = new SgDataStatementValue();
      ASSERT_not_null(dataValue);
      if (!dataValue->get_initializer_list()) {
        SgExprListExp *exprList = SageBuilder::buildExprListExp_nfi();
        ASSERT_not_null(exprList);
        dataValue->set_initializer_list(exprList);
        exprList->set_parent(dataValue);
      }

      if (repeatOpt) {
        dataValue->set_data_initialization_format(
            SgDataStatementValue::e_implicit_list);
        SgExpression *repeatExpr{nullptr};
        Rose::builder::Build(repeatOpt.value(), repeatExpr);
        ASSERT_not_null(repeatExpr);
        dataValue->set_repeat_expression(repeatExpr);
        repeatExpr->set_parent(dataValue);

        SgExpression *constExpr{nullptr};
        Rose::builder::Build(constant, constExpr);
        ASSERT_not_null(constExpr);
        dataValue->set_constant_expression(constExpr);
        constExpr->set_parent(dataValue);
      } else {
        dataValue->set_data_initialization_format(
            SgDataStatementValue::e_explict_list);
        SgExpression *constExpr{nullptr};
        Rose::builder::Build(constant, constExpr);
        ASSERT_not_null(constExpr);
        SageInterface::appendExpression(dataValue->get_initializer_list(),
                                        constExpr);
      }

      dataGroup->get_value_list().push_back(dataValue);
    }
    groups.push_back(dataGroup);
  }
}

void BuildVisitor::Build(parser::AllocatableStmt &x) {
  SgAttributeSpecificationStatement *allocStmt =
      SageBuilder::buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_allocatableStatement);
  ASSERT_not_null(allocStmt);
  SageInterface::setSourcePosition(allocStmt);

  SgExprListExp *paramList = allocStmt->get_parameter_list();
  if (paramList == nullptr) {
    paramList = SageBuilder::buildExprListExp_nfi();
    ASSERT_not_null(paramList);
    allocStmt->set_parameter_list(paramList);
    paramList->set_parent(allocStmt);
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  for (auto &decl : x.v) {
    const std::string name = std::get<parser::ObjectName>(decl.t).ToString();
    SgVariableSymbol *symbol =
        SageInterface::lookupVariableSymbolInParentScopes(name, scope);
    if (symbol == nullptr) {
      SgType *implicitType = SageBuilder::buildFortranImplicitType(name);
      SgVariableDeclaration *varDecl =
          SageBuilder::buildVariableDeclaration_nfi(name, implicitType,
                                                    /*initializer*/ nullptr,
                                                    scope);
      ASSERT_not_null(varDecl);
      SageInterface::setSourcePosition(varDecl);
      MarkFortranImplicitDeclaration(varDecl);
      SageInterface::appendStatement(varDecl, scope);
      symbol = SageInterface::lookupVariableSymbolInParentScopes(name, scope);
    }
    ASSERT_not_null(symbol);

    SgInitializedName *initName = symbol->get_declaration();
    ASSERT_not_null(initName);

    SgExpression *ref = SageBuilder::buildVarRefExp(symbol);
    ASSERT_not_null(ref);

    if (auto &arrayOpt = std::get<std::optional<parser::ArraySpec>>(decl.t)) {
      if (!IsArrayType(initName->get_type())) {
        SgType *baseType = initName->get_type();
        if (auto *modifierType = isSgModifierType(baseType)) {
          baseType = modifierType->get_base_type();
        }
        if (auto *arrayType = isSgArrayType(baseType)) {
          baseType = arrayType->get_base_type();
        }

        SgType *arrayType{nullptr};
        Rose::builder::Build(arrayOpt.value(), arrayType, baseType);
        if (arrayType != nullptr) {
          initName->set_type(arrayType);
          initName->set_shapeDeferred(true);
        }
      }
    }

    if (auto &coarrayOpt =
            std::get<std::optional<parser::CoarraySpec>>(decl.t)) {
      SgType *baseType = initName->get_type();
      if (auto *modifierType = isSgModifierType(baseType)) {
        baseType = modifierType->get_base_type();
      }
      SgType *coarrayType{nullptr};
      Rose::builder::Build(coarrayOpt.value(), coarrayType, baseType);
      if (coarrayType != nullptr) {
        initName->set_type(coarrayType);
        initName->set_shapeDeferred(true);
      }
    }

    paramList->get_expressions().push_back(ref);
    ref->set_parent(paramList);
  }

  SageInterface::appendStatement(allocStmt, scope);
}

void BuildVisitor::Build(parser::BasedPointerStmt &x) {
  using namespace Fortran::parser;

  SgSourceFile *source = builder.getSourceFile();
  if (source == nullptr || !source->get_cray_pointer_support()) {
    std::cerr << "[FATAL] [ROSE] [frontend] [Fortran] "
              << "Cray pointer construct requires -rose:cray_pointer_support "
                 "or -fcray-pointer.\n";
    ROSE_ABORT();
  }

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  for (auto &based : x.v) {
    const auto &pointerNameNode = std::get<0>(based.t);
    const auto &targetNameNode = std::get<1>(based.t);
    auto &arrayOpt = std::get<2>(based.t);
    const std::string pointerName = pointerNameNode.ToString();
    const std::string targetName = targetNameNode.ToString();

    SgVariableSymbol *targetSymbol =
        SageInterface::lookupVariableSymbolInParentScopes(targetName, scope);
    if (targetSymbol == nullptr) {
      SgType *implicitType = SageBuilder::buildFortranImplicitType(targetName);
      SgVariableDeclaration *targetDecl =
          SageBuilder::buildVariableDeclaration_nfi(
              targetName, implicitType, /*initializer*/ nullptr, scope);
      ASSERT_not_null(targetDecl);
      SageInterface::setSourcePosition(targetDecl);
      MarkFortranImplicitDeclaration(targetDecl);
      MarkFortranEmitImplicitDeclaration(targetDecl);
      SageInterface::appendStatement(targetDecl, scope);
      targetSymbol =
          SageInterface::lookupVariableSymbolInParentScopes(targetName, scope);
    }
    ASSERT_not_null(targetSymbol);

    SgInitializedName *targetInit = targetSymbol->get_declaration();
    ASSERT_not_null(targetInit);

    if (arrayOpt) {
      if (!IsArrayType(targetInit->get_type())) {
        SgType *baseType = targetInit->get_type();
        if (auto *modifier = isSgModifierType(baseType)) {
          baseType = modifier->get_base_type();
        }
        if (auto *arrayType = isSgArrayType(baseType)) {
          baseType = arrayType->get_base_type();
        }
        SgType *arrayType{nullptr};
        Rose::builder::Build(arrayOpt.value(), arrayType, baseType);
        if (arrayType != nullptr) {
          targetInit->set_type(arrayType);
        }
      }
    }

    SgVariableDeclaration *ptrDecl = SageBuilder::buildVariableDeclaration_nfi(
        pointerName, SgTypeCrayPointer::createType(),
        /*initializer*/ nullptr, scope);
    ASSERT_not_null(ptrDecl);
    SageInterface::setSourcePosition(ptrDecl);
    ptrDecl->get_declarationModifier().get_accessModifier().setUndefined();
    SgInitializedName *ptrInit = ptrDecl->get_variables().front();
    ASSERT_not_null(ptrInit);
    ptrInit->set_prev_decl_item(targetInit);
    SageInterface::appendStatement(ptrDecl, scope);
  }
}

void BuildVisitor::Build(parser::ExternalStmt &x) {
  SgAttributeSpecificationStatement *externalStmt =
      SageBuilder::buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_externalStatement);
  ASSERT_not_null(externalStmt);
  SageInterface::setSourcePosition(externalStmt);

  SgExprListExp *paramList = externalStmt->get_parameter_list();
  if (paramList == nullptr) {
    paramList = SageBuilder::buildExprListExp_nfi();
    ASSERT_not_null(paramList);
    externalStmt->set_parameter_list(paramList);
    paramList->set_parent(externalStmt);
  }

  SgFunctionParameterTypeList *types = new SgFunctionParameterTypeList();
  ASSERT_not_null(types);
  SgType *returnType = SageBuilder::buildVoidType();
  SgFunctionType *funcType = SageBuilder::buildFunctionType(returnType, types);
  ASSERT_not_null(funcType);

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);
  for (auto &name : x.v) {
    SgFunctionRefExp *ref = SageBuilder::buildFunctionRefExp(
        SgName(name.ToString()), funcType, scope);
    ASSERT_not_null(ref);
    SageInterface::setSourcePosition(ref);
    AppendExpr(paramList, ref);
  }

  SageInterface::appendStatement(externalStmt, scope);
}

void BuildVisitor::Build(parser::NamelistStmt &x) {
  SgNamelistStatement *stmt = new SgNamelistStatement();
  ASSERT_not_null(stmt);
  ApplyCurrentStatementSource(stmt);

  for (auto &group : x.v) {
    const std::string groupName = std::get<parser::Name>(group.t).ToString();
    const auto &names = std::get<std::list<parser::Name>>(group.t);

    SgNameGroup *nameGroup = new SgNameGroup();
    ASSERT_not_null(nameGroup);
    SageInterface::setSourcePosition(nameGroup);
    nameGroup->set_group_name(groupName);
    for (auto &name : names) {
      nameGroup->get_name_list().push_back(name.ToString());
    }
    stmt->get_group_list().push_back(nameGroup);
  }

  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildVisitor::Build(parser::TypeDeclarationStmt &x) {
  // std::tuple<> DeclarationTypeSpec, std::list<AttrSpec>,
  // std::list<EntityDecl>
  using namespace Fortran::parser;

  SgType *type{nullptr};
  BuildType(std::get<parser::DeclarationTypeSpec>(x.t), type);

  std::list<LanguageTranslation::ExpressionKind> modifiers{};
  parser::ArraySpec *dimensionSpec = nullptr;
  for (auto &attr : std::get<std::list<AttrSpec>>(x.t)) {
    if (auto *arraySpec = std::get_if<parser::ArraySpec>(&attr.u)) {
      if (dimensionSpec == nullptr) {
        dimensionSpec = arraySpec;
      }
      continue;
    }
    getAttrSpec(attr, modifiers, type);
  }

  std::list<EntityDeclTuple> initInfo{};
  EntityDecls(std::get<std::list<EntityDecl>>(x.t), initInfo, type,
              dimensionSpec); // std::list<EntityDecl>

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  for (const auto &entry : initInfo) {
    std::string name;
    SgType *entityType{nullptr};
    SgExpression *initExpr{nullptr};
    std::tie(name, entityType, initExpr) = entry;

    if (entityType == nullptr) {
      entityType = type;
    }

    bool reusedImplicitDecl = false;
    SgVariableSymbol *symbol =
        SageInterface::lookupVariableSymbolInParentScopes(name, scope);
    if (symbol != nullptr && symbol->get_scope() == scope) {
      SgInitializedName *initName = symbol->get_declaration();
      SgVariableDeclaration *existingDecl =
          initName != nullptr ? isSgVariableDeclaration(initName->get_parent())
                              : nullptr;
      if (existingDecl != nullptr &&
          existingDecl->getAttribute(kFortranImplicitDeclAttr) != nullptr) {
        SgType *mergedType = entityType;
        SgArrayType *existingArray =
            isSgArrayType(StripModifierType(initName->get_type()));
        if (existingArray != nullptr && !IsArrayType(entityType)) {
          SgExprListExp *dimInfo = existingArray->get_dim_info();
          if (dimInfo != nullptr) {
            mergedType = SageBuilder::buildArrayType(entityType, dimInfo);
          }
        }
        initName->set_type(mergedType);
        if (initExpr != nullptr) {
          SgInitializer *initializer =
              SageBuilder::buildAssignInitializer_nfi(initExpr, mergedType);
          ASSERT_not_null(initializer);
          initName->set_initializer(initializer);
          initializer->set_parent(initName);
        }
        builder.Leave(existingDecl, modifiers);
        existingDecl->removeAttribute(kFortranImplicitDeclAttr);
        existingDecl->removeAttribute(kFortranEmitImplicitDeclAttr);
        SageInterface::removeStatement(existingDecl);
        existingDecl->set_scope(scope);
        existingDecl->set_parent(scope);
        if (auto *varDecl = isSgVariableDeclaration(existingDecl)) {
          SageInterface::fixVariableDeclaration(varDecl, scope);
        }
        InsertFortranSpecificationStatement(existingDecl, scope);
        reusedImplicitDecl = true;
      }
    }

    if (!reusedImplicitDecl) {
      std::list<EntityDeclTuple> singleInitInfo{entry};
      SgVariableDeclaration *varDecl{nullptr};
      builder.Enter(varDecl, type, singleInitInfo);
      builder.Leave(varDecl, modifiers);
    }
  }
}

// SpecificationConstruct
//
void BuildVisitor::Build(parser::DerivedTypeDef &x) {
  // std::tuple<> Statement<DerivedTypeStmt>,
  // std::list<Statement<TypeParamDefStmt>>,
  //              std::list<Statement<PrivateOrSequence>>,
  //              std::list<Statement<ComponentDefStmt>>,
  //              std::optional<TypeBoundProcedurePart>, Statement<EndTypeStmt>
  using namespace Fortran::parser;

  auto &stmt{std::get<Statement<DerivedTypeStmt>>(x.t)};

  // DerivedTypeStmt std::tuple<std::list<TypeAttrSpec>, Name, std::list<Name>>
  // t;
  std::string name{std::get<Name>(stmt.statement.t).ToString()};

  SgDerivedTypeStatement *derived{nullptr};
  builder.Enter(derived, name);

  std::list<LanguageTranslation::ExpressionKind> modifiers;
  for (auto &attr : std::get<0>(stmt.statement.t)) {
    if (auto *extends = std::get_if<parser::TypeAttrSpec::Extends>(&attr.u)) {
      const std::string baseName = extends->v.ToString();
      SgScopeStatement *lookupScope = derived->get_scope();
      SgClassSymbol *baseSymbol =
          lookupDerivedTypeSymbol(baseName, lookupScope);
      if (baseSymbol == nullptr) {
        SgDerivedTypeStatement *forward =
            SageBuilder::buildDerivedTypeStatement(baseName, lookupScope);
        if (forward != nullptr) {
          forward->set_scope(lookupScope);
          forward->set_parent(lookupScope);
        }
        baseSymbol = lookupScope->lookup_class_symbol(baseName);
      }
      if (baseSymbol != nullptr && baseSymbol->get_declaration() != nullptr) {
        SgClassDefinition *def = derived->get_definition();
        ASSERT_not_null(def);
        SgBaseClass *base = SageBuilder::buildBaseClass(
            baseSymbol->get_declaration(), def, /*isVirtual*/ false,
            /*isDirect*/ true);
        ASSERT_not_null(base);
        def->append_inheritance(base);
      }
      continue;
    }

    LanguageTranslation::ExpressionKind m;
    getModifiers(attr, m);
    modifiers.push_back(m);
  }

  SgClassDefinition *classDef = derived->get_definition();
  ASSERT_not_null(classDef);

  for (auto &specStmt :
       std::get<std::list<Statement<PrivateOrSequence>>>(x.t)) {
    common::visit(common::visitors{
                      [&](PrivateStmt &) { classDef->set_isPrivate(true); },
                      [&](SequenceStmt &) { classDef->set_isSequence(true); }},
                  specStmt.statement.u);
  }

  for (auto &componentStmt :
       std::get<std::list<Statement<ComponentDefStmt>>>(x.t)) {
    common::visit(
        common::visitors{
            [&](DataComponentDefStmt &y) {
              SgStatement *builtStmt{nullptr};
              Rose::builder::Build(y, builtStmt);
            },
            [&](ProcComponentDefStmt &y) {
              auto &optInterface = std::get<std::optional<ProcInterface>>(y.t);
              auto &attrSpecs = std::get<std::list<ProcComponentAttrSpec>>(y.t);
              auto &decls = std::get<std::list<ProcDecl>>(y.t);

              SgScopeStatement *scope = SageBuilder::topScopeStack();
              ASSERT_not_null(scope);

              SgFunctionType *procType =
                  BuildProcedureInterfaceType(optInterface, scope);

              std::list<LanguageTranslation::ExpressionKind> modifiers{};
              for (auto &attr : attrSpecs) {
                common::visit(
                    common::visitors{[&](AccessSpec &spec) {
                                       LanguageTranslation::ExpressionKind m;
                                       getModifiers(spec, m);
                                       modifiers.push_back(m);
                                     },
                                     [&](const NoPass &) {}, [&](Pass &) {},
                                     [&](const Pointer &) {
                                       modifiers.push_back(
                                           LanguageTranslation::ExpressionKind::
                                               e_type_modifier_pointer);
                                     }},
                    attr.u);
              }

              std::list<EntityDeclTuple> initInfo{};
              for (auto &decl : decls) {
                std::string name = std::get<parser::Name>(decl.t).ToString();
                SgExpression *initExpr{nullptr};

                if (auto &optInit =
                        std::get<std::optional<ProcPointerInit>>(decl.t)) {
                  common::visit(
                      common::visitors{
                          [&](const NullInit &) {
                            initExpr =
                                SageBuilderCpp17::buildNullExpression_nfi();
                          },
                          [&](const Name &target) {
                            SgFunctionRefExp *ref =
                                SageBuilder::buildFunctionRefExp(
                                    SgName(target.ToString()), procType, scope);
                            initExpr = ref;
                          }},
                      optInit->u);
                }

                initInfo.push_back(std::make_tuple(name, procType, initExpr));
              }

              if (initInfo.empty()) {
                return;
              }

              SgVariableDeclaration *varDecl{nullptr};
              builder.Enter(varDecl, procType, initInfo);
              builder.Leave(varDecl, modifiers);
            },
            [&](common::Indirection<CompilerDirective> &y) {
              Build(y.value());
            },
            [&](ErrorRecovery &) { ABORT_NO_IMPL; }},
        componentStmt.statement.u);
  }

  // Leave SageTreeBuilder for SgDerivedTypeStmt
  builder.Leave(derived, modifiers);
}

void BuildVisitor::Build(parser::DimensionStmt &x) {
  SgAttributeSpecificationStatement *dimStmt =
      SageBuilder::buildAttributeSpecificationStatement(
          SgAttributeSpecificationStatement::e_dimensionStatement);
  ASSERT_not_null(dimStmt);
  SageInterface::appendStatement(dimStmt, SageBuilder::topScopeStack());

  SgExprListExp *paramList = dimStmt->get_parameter_list();
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  for (auto &decl : x.v) {
    const std::string name = std::get<parser::Name>(decl.t).ToString();
    SgVariableSymbol *symbol =
        SageInterface::lookupVariableSymbolInParentScopes(name, scope);
    if (symbol == nullptr) {
      SgType *implicitType = SageBuilder::buildFortranImplicitType(name);
      SgVariableDeclaration *varDecl =
          SageBuilder::buildVariableDeclaration_nfi(name, implicitType,
                                                    /*initializer*/ nullptr,
                                                    scope);
      ASSERT_not_null(varDecl);
      SageInterface::setSourcePosition(varDecl);
      MarkFortranImplicitDeclaration(varDecl);
      SageInterface::appendStatement(varDecl, scope);
      symbol = SageInterface::lookupVariableSymbolInParentScopes(name, scope);
    }
    ASSERT_not_null(symbol);

    SgInitializedName *initName = symbol->get_declaration();
    ASSERT_not_null(initName);
    SgType *baseType = initName->get_type();
    if (auto *modifierType = isSgModifierType(baseType)) {
      baseType = modifierType->get_base_type();
    }
    if (auto *arrayType = isSgArrayType(baseType)) {
      baseType = arrayType->get_base_type();
    }

    SgType *arrayType{nullptr};
    Rose::builder::Build(std::get<parser::ArraySpec>(decl.t), arrayType,
                         baseType);
    if (arrayType != nullptr) {
      initName->set_type(arrayType);
    }

    if (paramList) {
      SgVarRefExp *ref = SageBuilder::buildVarRefExp(symbol);
      ASSERT_not_null(ref);
      SgExprListExp *dimInfo =
          BuildArraySpecExprList(std::get<parser::ArraySpec>(decl.t));
      ASSERT_not_null(dimInfo);
      SgPntrArrRefExp *arrayRef = SageBuilder::buildPntrArrRefExp(ref, dimInfo);
      ASSERT_not_null(arrayRef);
      paramList->get_expressions().push_back(arrayRef);
      arrayRef->set_parent(paramList);
    }
  }
}

void BuildVisitor::Build(parser::ProcedureDeclarationStmt &x) {
  using namespace Fortran::parser;

  auto &optInterface = std::get<std::optional<ProcInterface>>(x.t);
  auto &attrSpecs = std::get<std::list<ProcAttrSpec>>(x.t);
  auto &decls = std::get<std::list<ProcDecl>>(x.t);

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  auto buildFunctionType = [](SgType *returnType) {
    SgFunctionParameterTypeList *typeList = new SgFunctionParameterTypeList();
    ASSERT_not_null(typeList);
    return SageBuilder::buildFunctionType(returnType, typeList);
  };

  SgFunctionType *procType{nullptr};
  if (optInterface) {
    common::visit(common::visitors{
                      [&](Name &ifaceName) {
                        SgFunctionSymbol *sym =
                            SageInterface::lookupFunctionSymbolInParentScopes(
                                ifaceName.ToString(), scope);
                        if (sym != nullptr) {
                          procType = isSgFunctionType(sym->get_type());
                        }
                        if (procType == nullptr) {
                          procType =
                              buildFunctionType(SageBuilder::buildVoidType());
                        }
                      },
                      [&](DeclarationTypeSpec &spec) {
                        SgType *returnType{nullptr};
                        BuildType(spec, returnType);
                        if (returnType == nullptr) {
                          returnType = SageBuilder::buildUnknownType();
                        }
                        procType = buildFunctionType(returnType);
                      }},
                  optInterface->u);
  }

  if (procType == nullptr) {
    procType = buildFunctionType(SageBuilder::buildVoidType());
  }

  std::list<LanguageTranslation::ExpressionKind> modifiers{};
  for (auto &attr : attrSpecs) {
    common::visit(
        common::visitors{
            [&](AccessSpec &y) {
              LanguageTranslation::ExpressionKind m;
              getModifiers(y, m);
              modifiers.push_back(m);
            },
            [&](LanguageBindingSpec &y) {
              LanguageTranslation::ExpressionKind m;
              getModifiers(y, m);
              modifiers.push_back(m);
            },
            [&](IntentSpec &y) {
              LanguageTranslation::ExpressionKind m;
              getModifiers(y, m);
              modifiers.push_back(m);
            },
            [&](const Optional &) {
              modifiers.push_back(LanguageTranslation::ExpressionKind::
                                      e_type_modifier_optional);
            },
            [&](const Pointer &) {
              modifiers.push_back(
                  LanguageTranslation::ExpressionKind::e_type_modifier_pointer);
            },
            [&](const Protected &) {
              modifiers.push_back(LanguageTranslation::ExpressionKind::
                                      e_type_modifier_protected);
            },
            [&](const Save &) {
              modifiers.push_back(
                  LanguageTranslation::ExpressionKind::e_type_modifier_save);
            },
            [&](const ErrorRecovery &) { ABORT_NO_IMPL; }},
        attr.u);
  }

  std::list<EntityDeclTuple> initInfo{};
  for (auto &decl : decls) {
    std::string name = std::get<Name>(decl.t).ToString();
    SgExpression *initExpr{nullptr};

    if (auto &optInit = std::get<std::optional<ProcPointerInit>>(decl.t)) {
      common::visit(common::visitors{
                        [&](const NullInit &) {
                          initExpr =
                              SageBuilderCpp17::buildNullExpression_nfi();
                        },
                        [&](const Name &target) {
                          SgFunctionRefExp *ref =
                              SageBuilder::buildFunctionRefExp(
                                  SgName(target.ToString()), procType, scope);
                          initExpr = ref;
                        }},
                    optInit->u);
    }

    initInfo.push_back(std::make_tuple(name, procType, initExpr));
  }

  if (initInfo.empty()) {
    return;
  }

  SgVariableDeclaration *varDecl{nullptr};
  builder.Enter(varDecl, procType, initInfo);
  builder.Leave(varDecl, modifiers);
}

namespace {
SgType *BuildDerivedTypeSpec(const parser::DerivedTypeSpec &derivedSpec) {
  const std::string name = std::get<parser::Name>(derivedSpec.t).ToString();
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);
  SgScopeStatement *declScope = scope;
  if (SgClassDefinition *classDef = isSgClassDefinition(declScope)) {
    SgDeclarationStatement *decl = classDef->get_declaration();
    if (isSgDerivedTypeStatement(decl)) {
      declScope = declScope->get_scope();
    }
  }
  ASSERT_not_null(declScope);
  SgClassSymbol *symbol = lookupDerivedTypeSymbol(name, declScope);
  if (symbol == nullptr) {
    SgDerivedTypeStatement *forward =
        SageBuilder::buildDerivedTypeStatement(name, declScope);
    if (forward != nullptr) {
      forward->set_scope(declScope);
      forward->set_parent(declScope);
      symbol = declScope->lookup_class_symbol(name);
    }
  }
  if (symbol != nullptr && symbol->get_declaration() != nullptr &&
      symbol->get_declaration()->get_type() != nullptr) {
    return symbol->get_declaration()->get_type();
  }
  return SgTypeDefault::createType(name);
}

SgVariableSymbol *resolveUseAssociatedVariableSymbol(SgSymbol *symbol) {
  if (symbol == nullptr) {
    return nullptr;
  }
  if (auto *varSymbol = isSgVariableSymbol(symbol)) {
    return varSymbol;
  }
  if (auto *aliasSymbol = isSgAliasSymbol(symbol)) {
    return isSgVariableSymbol(aliasSymbol->get_alias());
  }
  return nullptr;
}

SgVariableSymbol *buildUseAssociatedVariableSymbol(SgVariableSymbol *varSymbol,
                                                   const SgName &localName,
                                                   SgScopeStatement *scope) {
  ASSERT_not_null(varSymbol);
  ASSERT_not_null(scope);

  SgType *varType = varSymbol->get_type();
  if (varType == nullptr) {
    varType = SageBuilder::buildUnknownType();
  }

  SgInitializedName *initName = SageBuilder::buildInitializedName_nfi(
      localName, varType, /*initializer*/ nullptr);
  ASSERT_not_null(initName);
  SageInterface::setSourcePosition(initName);
  initName->set_scope(scope);
  initName->set_parent(scope);

  return new SgVariableSymbol(initName);
}

SgClassDefinition *findClassDefinition(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }
  SgType *stripped = type->stripType(
      SgType::STRIP_TYPEDEF_TYPE | SgType::STRIP_MODIFIER_TYPE |
      SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE |
      SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
  auto *classType = isSgClassType(stripped);
  if (classType == nullptr) {
    return nullptr;
  }
  auto *classDecl = isSgClassDeclaration(classType->get_declaration());
  if (classDecl == nullptr) {
    return nullptr;
  }
  SgClassDeclaration *defDecl =
      isSgClassDeclaration(classDecl->get_definingDeclaration());
  if (defDecl == nullptr) {
    defDecl = classDecl;
  }
  return defDecl->get_definition();
}

SgVariableSymbol *findComponentSymbol(SgClassDefinition *def,
                                      const SgName &name) {
  if (def == nullptr) {
    return nullptr;
  }
  if (SgVariableSymbol *sym = def->lookup_variable_symbol(name)) {
    return sym;
  }
  for (SgBaseClass *base : def->get_inheritances()) {
    if (base == nullptr) {
      continue;
    }
    SgClassDeclaration *baseDecl = base->get_base_class();
    if (baseDecl == nullptr) {
      continue;
    }
    if (SgVariableSymbol *sym =
            findComponentSymbol(baseDecl->get_definition(), name)) {
      return sym;
    }
  }
  return nullptr;
}
} // namespace

void BuildVisitor::Build(parser::DeclarationTypeSpec::Type &x) {
  // Type DerivedTypeSpec derived;
  //      DerivedTypeSpec std::tuple<Name, std::list<TypeParamSpec>> t;
  SgType *type = BuildDerivedTypeSpec(x.derived);
  ASSERT_not_null(type);
  this->set(type);
}

void BuildVisitor::Build(parser::DeclarationTypeSpec::Class &x) {
  SgType *type{nullptr};
  Rose::builder::Build(x, type);
  ASSERT_not_null(type);
  this->set(type);
}

void BuildVisitor::Build(parser::DeclarationTypeSpec::TypeStar &x) {
  SgType *type{nullptr};
  Rose::builder::Build(x, type);
  ASSERT_not_null(type);
  this->set(type);
}

void BuildVisitor::Build(parser::DeclarationTypeSpec::ClassStar &x) {
  SgType *type{nullptr};
  Rose::builder::Build(x, type);
  ASSERT_not_null(type);
  this->set(type);
}

void BuildVisitor::Build(parser::DeclarationTypeSpec::Record &x) {
  SgType *type{nullptr};
  Rose::builder::Build(x, type);
  ASSERT_not_null(type);
  this->set(type);
}

void Build(parser::DeclarationTypeSpec &x, SgType *&type) {
  BuildVisitor visitor;
  visitor.BuildType(x, type);
}

void Build(parser::DeclarationTypeSpec::TypeStar &x, SgType *&type) {
  type = SageBuilder::buildUnknownType();
}

void Build(parser::DeclarationTypeSpec::Class &x, SgType *&type) {
  type = BuildDerivedTypeSpec(x.derived);
}

void Build(parser::DeclarationTypeSpec::ClassStar &x, SgType *&type) {
  type = SageBuilder::buildUnknownType();
}

void Build(parser::DeclarationTypeSpec::Record &x, SgType *&type) {
  std::string name = x.v.ToString();
  type = SgTypeDefault::createType(name);
}

void Build(parser::AttrSpec &x, LanguageTranslation::ExpressionKind &modifier) {
  std::cout << "Rose::builder::Build(AttrSpec)\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::IntegerTypeSpec &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};
  const bool hasKindStar = KindSelectorHasStar(x.v);

  if (auto &kind = x.v) { // std::optional<KindSelector>
    WalkExpr(kind.value(), expr);
  }
  type = SageBuilder::buildIntType(expr);
  if (type != nullptr) {
    type->set_hasTypeKindStar(hasKindStar);
  }
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Real &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};
  const bool hasKindStar = KindSelectorHasStar(x.kind);

  if (x.kind) { // std::optional<KindSelector>
    WalkExpr(x.kind.value(), expr);
  }
  type = SageBuilder::buildFloatType(expr);
  if (type != nullptr) {
    type->set_hasTypeKindStar(hasKindStar);
  }
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::DoublePrecision &x) {
  SgType *type{SageBuilder::buildDoubleType()}; // no KindSelector
  this->set(type);                              // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Complex &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};
  SgExpression *kindExpr{nullptr};
  const bool hasKindStar = KindSelectorHasStar(x.kind);

  if (x.kind) { // std::optional<KindSelector>
    WalkExpr(x.kind.value(), expr);
  }

  if (expr != nullptr) {
    SgTreeCopy treeCopy;
    kindExpr = isSgExpression(expr->copy(treeCopy));
  }

  SgType *base = SageBuilder::buildFloatType(expr);
  type = SageBuilder::buildComplexType(base);
  if (type != nullptr) {
    if (kindExpr != nullptr) {
      SageInterface::setSourcePosition(kindExpr);
      type->set_type_kind(kindExpr);
      kindExpr->set_parent(type);
    }
    type->set_hasTypeKindStar(hasKindStar);
  }
  this->set(type); // synthesized attribute
}

#if DEPRECATED
void BuildVisitor::Build(parser::CharLength &x) {
  // CharLength std::variant<TypeParamValue, std::uint64_t> u;
  SgExpression *expr{nullptr};
  WalkExpr(x, expr);
}

void BuildVisitor::Build(parser::LengthSelector &x) {
  // CharLength std::variant<TypeParamValue, std::uint64_t> u;
  SgExpression *expr{nullptr};
  WalkExpr(x, expr);
}
#endif

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Character &x) {
  SgType *type{nullptr};

  if (x.selector) { // std::optional<CharSelector>
    SgExpression *len{nullptr}, *kind{nullptr};

    common::visit(
        common::visitors{[&](parser::LengthSelector &y) { WalkExpr(y, len); },
                         [&](parser::CharSelector::LengthAndKind &y) {
                           if (y.length) {
                             WalkExpr(y.length, len);
                           }
                           WalkExpr(y.kind, kind);
                         }},
        x.selector->u);

    if (len == nullptr) {
      // Default character length is 1 when only KIND is specified.
      len = SageBuilder::buildIntVal_nfi(1, "1");
      SageInterface::setSourcePosition(len);
    }
    type = SageBuilder::buildStringType(len);
    if (kind != nullptr) {
      type->set_type_kind(kind);
    }
  } else {
    type = SageBuilder::buildCharType();
  }

  ASSERT_not_null(type);
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Logical &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};
  const bool hasKindStar = KindSelectorHasStar(x.kind);

  if (x.kind) { // std::optional<KindSelector>
    WalkExpr(x.kind.value(), expr);
  }
  type = SageBuilder::buildBoolType(expr);
  if (type != nullptr) {
    type->set_hasTypeKindStar(hasKindStar);
  }
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::DoubleComplex &x) {
  SgType *base{SageBuilder::buildDoubleType()};
  SgType *type{SageBuilder::buildComplexType(base)}; // no KindSelector
  this->set(type);                                   // synthesized attribute
}

void Build(parser::VectorTypeSpec &x, SgType *&type) {
  std::cout << "Rose::builder::Build(VectorTypeSpec)\n";
  ABORT_NO_IMPL;
}

#if DEPRECATED
void Build(parser::LengthSelector &x, SgExpression *&expr) {
  // std::variant<TypeParamValue, CharLength> u;
  std::cout << "Rose::builder::Build(LengthSelector)\n";
  ABORT_NO_IMPL;
}
#endif

#if DEPRECATED
void Build(parser::CharSelector::LengthAndKind &x, SgExpression *&expr) {
  //    std::optional<TypeParamValue> length;
  //    ScalarIntConstantExpr kind;
  std::cout << "Rose::builder::Build(LengthAndKind)\n";
  ABORT_NO_IMPL;
}
#endif

#if DEPRECATED
void Build(parser::TypeParamValue &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(TypeParamVale)\n";
  ABORT_NO_IMPL;
}
#endif

void EntityDecls(std::list<Fortran::parser::EntityDecl> &x,
                 std::list<EntityDeclTuple> &entityDecls, SgType *baseType,
                 Fortran::parser::ArraySpec *dimensionSpec) {
  for (auto &entity : x) {
    SgType *type{nullptr};
    SgExpression *init{nullptr};
    std::string name{std::get<0>(entity.t).ToString()};
    SgType *entityBase = baseType;
    SgModifierType *modifierType = isSgModifierType(entityBase);
    SgType *charBase =
        modifierType != nullptr ? modifierType->get_base_type() : entityBase;

    if (auto &opt = std::get<3>(entity.t)) { // CharLength
      SgExpression *lenExpr{nullptr};
      BuildImpl(opt.value(), lenExpr);
      if (lenExpr != nullptr) {
        SgType *newCharType = nullptr;
        if (auto *stringType = isSgTypeString(charBase)) {
          SgTypeString *updated = SageBuilder::buildStringType(lenExpr);
          updated->set_type_kind(stringType->get_type_kind());
          newCharType = updated;
        } else if (isSgTypeChar(charBase)) {
          newCharType = SageBuilder::buildStringType(lenExpr);
        }
        if (newCharType != nullptr) {
          if (modifierType != nullptr) {
            modifierType->set_base_type(newCharType);
            entityBase = modifierType;
          } else {
            entityBase = newCharType;
          }
        }
      }
    }
    SgType *entityType = entityBase;
    if (auto &opt = std::get<1>(entity.t)) { // ArraySpec
      Build(opt.value(), entityType, entityBase);
    } else if (dimensionSpec != nullptr) {
      Build(*dimensionSpec, entityType, entityBase);
    }
    if (auto &opt = std::get<2>(entity.t)) { // CoarraySpec
      Build(opt.value(), entityType, entityType);
    }
    if (auto &opt = std::get<4>(entity.t)) { // Initialization
      Build(opt.value(), init);
    }
    type = entityType != nullptr ? entityType : entityBase;
    entityDecls.push_back(std::make_tuple(name, type, init));
  }
}

namespace {
SgExprListExp *BuildArraySpecExprList(Fortran::parser::ArraySpec &x) {
  using namespace Fortran::parser;

  SgExpression *expr{nullptr};
  SgExprListExp *dimInfo{SageBuilder::buildExprListExp_nfi()};

  common::visit(
      common::visitors{
          [&](std::list<ExplicitShapeSpec> &y) {
            for (ExplicitShapeSpec &spec :
                 y) { // is [ lower-bound : ] upper-bound
              BuildImpl(spec, expr = nullptr);
              ASSERT_not_null(expr);
              SageInterface::appendExpression(dimInfo, expr);
            }
          },
          [&](std::list<AssumedShapeSpec> &y) {
            for (AssumedShapeSpec &spec : y) { // is [ lower-bound ]:
              BuildImpl(spec, expr = nullptr);
              ASSERT_not_null(expr);
              SageInterface::appendExpression(dimInfo, expr);
            }
          },
          [&](DeferredShapeSpecList &y) {
            for (int ii{0}; ii < y.v; ii++) { // is :
              SageInterface::appendExpression(
                  dimInfo, SageBuilder::buildColonShapeExp_nfi());
            }
          },
          [&](AssumedSizeSpec &y) {
            // std::tuple<std::list<ExplicitShapeSpec>, AssumedImpliedSpec> t;
            for (ExplicitShapeSpec &spec :
                 std::get<0>(y.t)) { // is [ lower-bound : ] upper-bound
              BuildImpl(spec, expr = nullptr);
              ASSERT_not_null(expr);
              SageInterface::appendExpression(dimInfo, expr);
            }
            AssumedImpliedSpec &spec{std::get<1>(y.t)};
            BuildImpl(spec, expr = nullptr);
            ASSERT_not_null(expr);
            SageInterface::appendExpression(dimInfo, expr);
          },
          [&](ImpliedShapeSpec &y) {
            for (AssumedImpliedSpec &spec : y.v) { // is [ lower-bound : ] *
              BuildImpl(spec, expr = nullptr);
              ASSERT_not_null(expr);
              SageInterface::appendExpression(dimInfo, expr);
            }
          },
          [&](AssumedRankSpec &y) {
            // is ..
            // TODO: Need new expression type in ROSE
            ABORT_NO_IMPL;
          }},
      x.u);

  return dimInfo;
}

SgExprListExp *
BuildComponentArraySpecExprList(Fortran::parser::ComponentArraySpec &x) {
  using namespace Fortran::parser;

  SgExpression *expr{nullptr};
  SgExprListExp *dimInfo{SageBuilder::buildExprListExp_nfi()};

  common::visit(
      common::visitors{[&](std::list<ExplicitShapeSpec> &y) {
                         for (ExplicitShapeSpec &spec : y) {
                           BuildImpl(spec, expr = nullptr);
                           ASSERT_not_null(expr);
                           SageInterface::appendExpression(dimInfo, expr);
                         }
                       },
                       [&](DeferredShapeSpecList &y) {
                         for (int ii{0}; ii < y.v; ii++) {
                           SageInterface::appendExpression(
                               dimInfo, SageBuilder::buildColonShapeExp_nfi());
                         }
                       }},
      x.u);

  return dimInfo;
}

SgExprListExp *BuildCoarraySpecExprList(Fortran::parser::CoarraySpec &x) {
  using namespace Fortran::parser;

  SgExpression *expr{nullptr};
  SgExprListExp *dimInfo{SageBuilder::buildExprListExp_nfi()};

  common::visit(
      common::visitors{
          [&](DeferredCoshapeSpecList &y) {
            for (int ii{0}; ii < y.v; ii++) {
              SageInterface::appendExpression(
                  dimInfo, SageBuilder::buildColonShapeExp_nfi());
            }
          },
          [&](ExplicitCoshapeSpec &y) {
            for (ExplicitShapeSpec &spec : std::get<0>(y.t)) {
              BuildImpl(spec, expr = nullptr);
              ASSERT_not_null(expr);
              SageInterface::appendExpression(dimInfo, expr);
            }
            if (auto &opt = std::get<1>(y.t)) {
              SgExpression *lb{nullptr};
              SgExpression *ub{SageBuilderCpp17::buildAsteriskShapeExp_nfi()};
              SgExpression *stride{
                  SageBuilder::buildIntVal_nfi(std::string("1"))};
              WalkExpr(opt.value(), lb);
              ASSERT_not_null(lb);
              expr = SageBuilder::buildSubscriptExpression_nfi(lb, ub, stride);
              ASSERT_not_null(expr);
              SageInterface::appendExpression(dimInfo, expr);
            }
          }},
      x.u);

  return dimInfo;
}
} // namespace

// ArraySpec
void Build(parser::ArraySpec &x, SgType *&type, SgType *baseType) {
  // std::variant<> - std::list<ExplicitShapeSpec>, std::list<AssumedShapeSpec>,
  //                  DeferredShapeSpecList, AssumedSizeSpec, ImpliedShapeSpec,
  //                  AssumedRankSpec
  //
  // ExplicitShapeSpec - std::tuple<std::optional<SpecificationExpr>,
  // SpecificationExpr> t; AssumedShapeSpec - std::optional<SpecificationExpr>
  // v; DeferredShapeSpecList - int v; AssumedSizeSpec -
  // std::tuple<std::list<ExplicitShapeSpec>, AssumedImpliedSpec> t;
  // ImpliedShapeSpec - std::list<AssumedImpliedSpec> v;
  // AssumedRankSpec - using EmptyTrait = std::true_type;
  //
  SgExprListExp *dimInfo = BuildArraySpecExprList(x);
  type = SageBuilder::buildArrayType(baseType, dimInfo);
}

void Build(parser::ComponentArraySpec &x, SgType *&type, SgType *baseType) {
  SgExprListExp *dimInfo = BuildComponentArraySpecExprList(x);
  type = SageBuilder::buildArrayType(baseType, dimInfo);
}

// CoarraySpec
void Build(parser::CoarraySpec &x, SgType *&type, SgType *baseType) {
  ASSERT_not_null(baseType);
  SgExprListExp *dimInfo = BuildCoarraySpecExprList(x);
  ASSERT_not_null(dimInfo);

  SgArrayType *arrayType = SageBuilder::buildArrayType(baseType, dimInfo);
  ASSERT_not_null(arrayType);
  arrayType->set_isCoArray(true);
  type = arrayType;
}

void Build(parser::CharLength &x, SgExpression *&expr) { WalkExpr(x, expr); }

void Build(parser::Initialization &x, SgExpression *&expr) {
  using namespace Fortran::parser;
  common::visit(common::visitors{
                    [&](ConstantExpr &y) { WalkExpr(y, expr); },
                    [&](NullInit &y) { WalkExpr(y.v, expr); },
                    [&](InitialDataTarget &y) { Build(y.value(), expr); },
                    [&](std::list<common::Indirection<DataStmtValue>> &y) {
                      std::list<SgExpression *> values;
                      for (auto &value : y) {
                        SgExpression *valueExpr{nullptr};
                        Build(value.value(), valueExpr);
                        ASSERT_not_null(valueExpr);
                        values.push_back(valueExpr);
                      }

                      SgExprListExp *exprList =
                          SageBuilderCpp17::buildExprListExp_nfi(values);
                      for (SgExpression *item : exprList->get_expressions()) {
                        if (item) {
                          item->set_parent(exprList);
                        }
                      }
                      expr = SageBuilder::buildAggregateInitializer_nfi(
                          exprList,
                          /*explicit_type*/ nullptr);
                    }},
                x.u);
}

void Build(parser::SpecificationExpr &x, SgExpression *&expr) {
  WalkExpr(x.v, expr); // Scalar<IntExpr>
}

void Build(parser::Scalar<parser::IntExpr> &x, SgExpression *&expr) {
  WalkExpr(x.thing, expr);
}

void Build(parser::Scalar<parser::LogicalExpr> &x, SgExpression *&expr) {
  WalkExpr(x.thing, expr);
}

void Build(parser::ConstantExpr &x, SgExpression *&expr) {
  WalkExpr(x.thing, expr);
}

// DeclarationConstruct

void BuildImpl(parser::FormatStmt &x) {
  // FormatStmt format::FormatSpecification v;
  std::cout << "BuildImpl(FormatStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::EntryStmt &x) {
  // EntryStmt std::tuple<> Name, std::list<DummyArg>, std::optional<Suffix>
  using namespace Fortran::parser;

  const std::string entryName = std::get<Name>(x.t).ToString();
  std::list<std::string> dummyArgs;
  DummyArg(std::get<std::list<Fortran::parser::DummyArg>>(x.t), dummyArgs);

  std::string resultName;
  if (auto &suffix = std::get<std::optional<Suffix>>(x.t)) {
    if (suffix->resultName) {
      resultName = suffix->resultName->ToString();
    }
  }

  SgScopeStatement *currentScope = SageBuilder::topScopeStack();
  ASSERT_not_null(currentScope);

  SgFunctionDeclaration *enclosingDecl =
      SageInterface::getEnclosingFunctionDeclaration(currentScope, true);
  SgProcedureHeaderStatement *procDecl =
      isSgProcedureHeaderStatement(enclosingDecl);

  SgType *returnType = nullptr;
  if (procDecl &&
      procDecl->get_subprogram_kind() ==
          SgProcedureHeaderStatement::e_subroutine_subprogram_kind) {
    returnType = SageBuilder::buildVoidType();
  } else if (enclosingDecl && enclosingDecl->get_type()) {
    returnType = enclosingDecl->get_type()->get_return_type();
  }
  if (returnType == nullptr) {
    returnType = SageBuilder::buildFortranImplicitType(entryName);
  }

  SgFunctionType *functionType = new SgFunctionType(returnType, false);
  SgEntryStatement *entryStmt = new SgEntryStatement(
      SgName(entryName), functionType, /*definition*/ nullptr);
  entryStmt->set_scope(currentScope);

  SgFunctionParameterList *paramList = entryStmt->get_parameterList();
  ASSERT_not_null(paramList);
  SageInterface::setSourcePosition(paramList);

  for (const std::string &argName : dummyArgs) {
    SgVariableSymbol *symbol =
        SageInterface::lookupVariableSymbolInParentScopes(argName,
                                                          currentScope);
    if (symbol == nullptr) {
      SgType *implicitType = SageBuilder::buildFortranImplicitType(argName);
      SgVariableDeclaration *varDecl =
          SageBuilder::buildVariableDeclaration_nfi(argName, implicitType,
                                                    /*initializer*/ nullptr,
                                                    currentScope);
      ASSERT_not_null(varDecl);
      SageInterface::setSourcePosition(varDecl);
      MarkFortranImplicitDeclaration(varDecl);
      SageInterface::appendStatement(varDecl, currentScope);
      symbol = SageInterface::lookupVariableSymbolInParentScopes(argName,
                                                                 currentScope);
    }

    SgInitializedName *declInit =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    SgType *argType = declInit != nullptr
                          ? declInit->get_type()
                          : SageBuilder::buildFortranImplicitType(argName);
    SgInitializedName *paramInit =
        SageBuilder::buildInitializedName_nfi(argName, argType,
                                              /*initializer*/ nullptr);
    SageInterface::setSourcePosition(paramInit);
    if (declInit != nullptr) {
      paramInit->get_storageModifier() = declInit->get_storageModifier();
    }
    paramList->append_arg(paramInit);
    paramInit->set_parent(paramList);
  }

  if (!resultName.empty()) {
    SgVariableSymbol *resultSymbol =
        SageInterface::lookupVariableSymbolInParentScopes(resultName,
                                                          currentScope);
    if (resultSymbol == nullptr) {
      SageBuilderCpp17::fixUndeclaredResultName(resultName, currentScope,
                                                returnType);
      resultSymbol = SageInterface::lookupVariableSymbolInParentScopes(
          resultName, currentScope);
    }
    if (resultSymbol != nullptr && resultSymbol->get_declaration() != nullptr) {
      SgInitializedName *resultInit = resultSymbol->get_declaration();
      resultInit->set_parent(entryStmt);
      resultInit->set_scope(currentScope);
      entryStmt->set_result_name(resultInit);
    }
  }

  SageInterface::setSourcePosition(entryStmt);
  SageInterface::appendStatement(entryStmt, currentScope);
}

void BuildImpl(parser::StmtFunctionStmt &x) {
  using namespace Fortran::parser;

  const std::string funcName = std::get<Name>(x.t).ToString();
  std::list<std::string> dummyArgs;
  for (auto &arg : std::get<std::list<Name>>(x.t)) {
    dummyArgs.push_back(arg.ToString());
  }

  SgExpression *rhs{nullptr};
  WalkExpr(std::get<Scalar<Expr>>(x.t).thing, rhs);
  ASSERT_not_null(rhs);

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  SgStatementFunctionStatement *stmt =
      BuildStatementFunctionStatement(funcName, dummyArgs, rhs, scope);
  ASSERT_not_null(stmt);
  SageInterface::setSourcePosition(stmt);
  SageInterface::appendStatement(stmt, scope);
}

void BuildImpl(parser::ErrorRecovery &x) {
  std::cout << "BuildImpl(ErrorRecovery)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DataStmtObject &x, SgExpression *&expr) {
  common::visit(
      common::visitors{[&](common::Indirection<parser::Variable> &y) {
                         WalkExpr(y.value(), expr);
                       },
                       [&](parser::DataImpliedDo &y) { Build(y, expr); }},
      x.u);
}

void Build(parser::DataIDoObject &x, SgExpression *&expr) {
  common::visit(
      common::visitors{
          [&](parser::Scalar<common::Indirection<parser::Designator>> &y) {
            WalkExpr(y.thing.value(), expr);
          },
          [&](common::Indirection<parser::DataImpliedDo> &y) {
            Build(y.value(), expr);
          }},
      x.u);
}

namespace {
void BuildDataImpliedDoBounds(
    const parser::LoopBounds<parser::DoVariable, parser::ScalarIntConstantExpr>
        &bounds,
    SgExpression *&init, SgExpression *&upper, SgExpression *&step) {
  SgExpression *lower{nullptr};
  WalkExpr(const_cast<parser::ScalarIntConstantExpr &>(bounds.lower), lower);
  WalkExpr(const_cast<parser::ScalarIntConstantExpr &>(bounds.upper), upper);
  if (bounds.step) {
    WalkExpr(const_cast<parser::ScalarIntConstantExpr &>(bounds.step.value()),
             step);
  } else {
    step = SageBuilder::buildIntVal_nfi(std::string("1"));
  }
  std::string varName = bounds.name.thing.thing.ToString();
  SgExpression *varRef = SageBuilderCpp17::buildVarRefExp_nfi(varName);
  init = SageBuilder::buildAssignOp_nfi(varRef, lower);
}
} // namespace

void Build(parser::DataImpliedDo &x, SgExpression *&expr) {
  std::list<SgExpression *> object_items;
  for (auto &item : std::get<0>(x.t)) {
    SgExpression *itemExpr{nullptr};
    Build(item, itemExpr);
    ASSERT_not_null(itemExpr);
    object_items.push_back(itemExpr);
  }

  SgExprListExp *objectList =
      SageBuilderCpp17::buildExprListExp_nfi(object_items);
  for (SgExpression *item : objectList->get_expressions()) {
    if (item) {
      item->set_parent(objectList);
    }
  }

  SgExpression *init{nullptr};
  SgExpression *upper{nullptr};
  SgExpression *step{nullptr};
  BuildDataImpliedDoBounds(std::get<2>(x.t), init, upper, step);
  ASSERT_not_null(init);
  ASSERT_not_null(upper);
  ASSERT_not_null(step);

  SgImpliedDo *impliedDo =
      new SgImpliedDo(init, upper, step, objectList, nullptr);
  SageInterface::setSourcePosition(impliedDo);
  objectList->set_parent(impliedDo);
  init->set_parent(impliedDo);
  upper->set_parent(impliedDo);
  step->set_parent(impliedDo);
  expr = impliedDo;
}

// DataStmt
void Build(parser::DataStmtValue &x, SgExpression *&expr) {
  // std::tuple<std::optional<DataStmtRepeat>, DataStmtConstant> t;
  auto &constant = std::get<parser::DataStmtConstant>(x.t);
  Build(constant, expr);
}

void Build(parser::DataStmtConstant &x, SgExpression *&expr) {
  //     std::variant<LiteralConstant, SignedIntLiteralConstant,
  //      SignedRealLiteralConstant, SignedComplexLiteralConstant, NullInit,
  //      common::Indirection<Designator>, StructureConstructor>  u;
  using namespace Fortran::parser;

  common::visit(
      common::visitors{
          [&](LiteralConstant &y) {
            common::visit(
                common::visitors{
                    [&](HollerithLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](IntLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](RealLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](ComplexLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](BOZLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](CharLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](LogicalLiteralConstant &z) { BuildImpl(z, expr); },
                    [&](UnsignedLiteralConstant &z) { BuildImpl(z, expr); }},
                y.u);
          },
          [&](SignedIntLiteralConstant &y) { BuildImpl(y, expr); },
          [&](SignedRealLiteralConstant &y) { BuildImpl(y, expr); },
          [&](SignedComplexLiteralConstant &y) { BuildImpl(y, expr); },
          [&](NullInit &y) { WalkExpr(y.v, expr); },
          [&](common::Indirection<CharLiteralConstantSubstring> &y) {
            Build(y.value(), expr);
          },
          [&](common::Indirection<Designator> &y) { Build(y.value(), expr); },
          [&](StructureConstructor &y) { Build(y, expr); },
          [&](UnsignedLiteralConstant &y) { BuildImpl(y, expr); }},
      x.u);
}

void Build(parser::DataStmtRepeat &x, SgExpression *&expr) {
  // std::variant<IntLiteralConstant, Scalar<Integer<ConstantSubobject>>> u;
  using namespace Fortran::parser;
  common::visit(
      common::visitors{[&](IntLiteralConstant &y) { BuildImpl(y, expr); },
                       [&](Scalar<Integer<ConstantSubobject>> &y) {
                         auto &designator = y.thing.thing.thing.value();
                         Build(designator, expr);
                       }},
      x.u);
}

// ActionStmt

void BuildImpl(parser::AllocateStmt &x) {
  std::cout << "BuildImpl(AllocateStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::BackspaceStmt &x) {
  std::cout << "BuildImpl(BackspaceStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::CallStmt &x) {
  std::cout << "BuildImpl(CallStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::CloseStmt &x) {
  std::cout << "BuildImpl(CloseStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::CycleStmt &x, const OptLabel &label) {
  std::cout << "Rose::builder::Build(CycleStmt)\n";
  ABORT_NO_IMPL;

  // A Fortran CycleStmt is semantically similar to a C/C++ continue statement
}

void Build(parser::DeallocateStmt &x) {
  std::cout << "Rose::builder::Build(DeallocateStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::EndfileStmt &x) {
  std::cout << "Rose::builder::Build(EndfileStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::EventPostStmt &x) {
  std::cout << "Rose::builder::Build(EventPostStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::EventWaitStmt &x) {
  std::cout << "Rose::builder::Build(EventWaitStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ExitStmt &x) {
  // std::optional<Name> v;
  std::cout << "Rose::builder::Build(ExitStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::FlushStmt &x) {
  std::cout << "Rose::builder::Build(FlustStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::FormTeamStmt &x) {
  std::cout << "Rose::builder::Build(FormTeamStmt)\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::GotoStmt &x) {
  std::string target = StringUtility::numberToString(static_cast<int>(x.v));
  SgGotoStatement *stmt{nullptr};
  builder.Enter(stmt, target);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void Build(parser::IfStmt &x) {
  //  std::tuple<ScalarLogicalExpr, UnlabeledStatement<ActionStmt>> t;
  std::cout << "Rose::builder::Build(IfStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::InquireStmt &x) {
  std::cout << "Rose::builder::Build(InquireStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::LockStmt &x) {
  std::cout << "Rose::builder::Build(LockStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(const parser::NullifyStmt &x) {
  std::cout << "BuildImpl(NullifyStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(const parser::OpenStmt &x) {
  std::cout << "BuildImpl(OpenStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::PointerAssignmentStmt &x) {
  std::cout << "BuildImpl(PointerAssignmentStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DefaultCharExpr &x, SgExpression *&expr) {
  WalkExpr(x.thing, expr);
}

void Build(const parser::Label &x, SgExpression *&expr) {
  const int labelValue = static_cast<int>(x);
  const std::string labelText = StringUtility::numberToString(labelValue);
  SgScopeStatement *currentScope = SageBuilder::topScopeStack();
  ASSERT_not_null(currentScope);
  SgScopeStatement *labelScope =
      SageInterface::getEnclosingFunctionDefinition(currentScope, true);
  if (labelScope == nullptr) {
    labelScope = SageInterface::getEnclosingScope(currentScope, true);
  }
  ASSERT_not_null(labelScope);
  SgLabelSymbol *labelSymbol =
      GetOrCreateFortranLabelSymbol(labelValue, labelScope);
  expr = SageBuilder::buildLabelRefExp(labelSymbol);
}

void Build(const parser::Star &x, SgExpression *&expr) {
  expr = SageBuilderCpp17::buildAsteriskShapeExp_nfi();
}

void Build(parser::InputItem &x, SgExpression *&expr) {
  common::visit(
      common::visitors{[&](parser::Variable &y) { WalkExpr(y, expr); },
                       [&](common::Indirection<parser::InputImpliedDo> &y) {
                         Build(y.value(), expr);
                       }},
      x.u);
}

void Build(parser::OutputItem &x, SgExpression *&expr) {
  common::visit(
      common::visitors{[&](parser::Expr &y) { WalkExpr(y, expr); },
                       [&](common::Indirection<parser::OutputImpliedDo> &y) {
                         Build(y.value(), expr);
                       }},
      x.u);
}

namespace {
void BuildLoopBounds(
    const parser::LoopBounds<parser::DoVariable, parser::ScalarIntExpr> &bounds,
    SgExpression *&init, SgExpression *&upper, SgExpression *&step) {
  SgExpression *lower{nullptr};
  WalkExpr(const_cast<parser::ScalarIntExpr &>(bounds.lower), lower);
  WalkExpr(const_cast<parser::ScalarIntExpr &>(bounds.upper), upper);
  if (bounds.step) {
    WalkExpr(const_cast<parser::ScalarIntExpr &>(bounds.step.value()), step);
  } else {
    step = SageBuilder::buildIntVal_nfi(std::string("1"));
  }
  std::string varName = bounds.name.thing.thing.ToString();
  SgExpression *varRef = SageBuilderCpp17::buildVarRefExp_nfi(varName);
  init = SageBuilder::buildAssignOp_nfi(varRef, lower);
}
} // namespace

void Build(parser::OutputImpliedDo &x, SgExpression *&expr) {
  std::list<SgExpression *> object_items;
  for (auto &item : std::get<0>(x.t)) {
    SgExpression *itemExpr{nullptr};
    Build(item, itemExpr);
    ASSERT_not_null(itemExpr);
    object_items.push_back(itemExpr);
  }

  SgExprListExp *objectList =
      SageBuilderCpp17::buildExprListExp_nfi(object_items);
  for (SgExpression *item : objectList->get_expressions()) {
    if (item) {
      item->set_parent(objectList);
    }
  }

  SgExpression *init{nullptr};
  SgExpression *upper{nullptr};
  SgExpression *step{nullptr};
  BuildLoopBounds(std::get<1>(x.t), init, upper, step);
  ASSERT_not_null(init);
  ASSERT_not_null(upper);
  ASSERT_not_null(step);

  SgImpliedDo *impliedDo =
      new SgImpliedDo(init, upper, step, objectList, nullptr);
  SageInterface::setSourcePosition(impliedDo);
  objectList->set_parent(impliedDo);
  init->set_parent(impliedDo);
  upper->set_parent(impliedDo);
  step->set_parent(impliedDo);
  expr = impliedDo;
}

void Build(parser::InputImpliedDo &x, SgExpression *&expr) {
  std::list<SgExpression *> object_items;
  for (auto &item : std::get<0>(x.t)) {
    SgExpression *itemExpr{nullptr};
    Build(item, itemExpr);
    ASSERT_not_null(itemExpr);
    object_items.push_back(itemExpr);
  }

  SgExprListExp *objectList =
      SageBuilderCpp17::buildExprListExp_nfi(object_items);
  for (SgExpression *item : objectList->get_expressions()) {
    if (item) {
      item->set_parent(objectList);
    }
  }

  SgExpression *init{nullptr};
  SgExpression *upper{nullptr};
  SgExpression *step{nullptr};
  BuildLoopBounds(std::get<1>(x.t), init, upper, step);
  ASSERT_not_null(init);
  ASSERT_not_null(upper);
  ASSERT_not_null(step);

  SgImpliedDo *impliedDo =
      new SgImpliedDo(init, upper, step, objectList, nullptr);
  SageInterface::setSourcePosition(impliedDo);
  objectList->set_parent(impliedDo);
  init->set_parent(impliedDo);
  upper->set_parent(impliedDo);
  step->set_parent(impliedDo);
  expr = impliedDo;
}

namespace {
std::string QuoteFortranString(const std::string &value) {
  std::string result;
  result.reserve(value.size() + 2);
  result.push_back('\'');
  for (char ch : value) {
    if (ch == '\'') {
      result.push_back('\'');
    }
    result.push_back(ch);
  }
  result.push_back('\'');
  return result;
}

std::string FormatIntrinsicTypeToString(
    const Fortran::format::IntrinsicTypeDataEditDesc &desc) {
  using Kind = Fortran::format::IntrinsicTypeDataEditDesc::Kind;

  std::string result;
  switch (desc.kind) {
  case Kind::I:
    result = "I";
    break;
  case Kind::B:
    result = "B";
    break;
  case Kind::O:
    result = "O";
    break;
  case Kind::Z:
    result = "Z";
    break;
  case Kind::F:
    result = "F";
    break;
  case Kind::E:
    result = "E";
    break;
  case Kind::EN:
    result = "EN";
    break;
  case Kind::ES:
    result = "ES";
    break;
  case Kind::EX:
    result = "EX";
    break;
  case Kind::G:
    result = "G";
    break;
  case Kind::L:
    result = "L";
    break;
  case Kind::A:
    result = "A";
    break;
  case Kind::D:
    result = "D";
    break;
  }

  auto append_int = [&](const std::optional<int> &value) {
    if (value) {
      result += std::to_string(*value);
    }
  };

  if (desc.kind == Kind::L || desc.kind == Kind::A) {
    append_int(desc.width);
    return result;
  }

  append_int(desc.width);
  if (desc.digits) {
    result += ".";
    result += std::to_string(*desc.digits);
  }
  if (desc.exponentWidth) {
    result += "E";
    result += std::to_string(*desc.exponentWidth);
  }
  return result;
}

std::string FormatDerivedTypeToString(
    const Fortran::format::DerivedTypeDataEditDesc &desc) {
  std::string result = "DT";
  if (!desc.type.empty()) {
    result += QuoteFortranString(desc.type);
  }
  if (!desc.parameters.empty()) {
    result += "(";
    bool first = true;
    for (const auto &param : desc.parameters) {
      if (!first) {
        result += ",";
      }
      result += std::to_string(param);
      first = false;
    }
    result += ")";
  }
  return result;
}

std::string
FormatControlEditToString(const Fortran::format::ControlEditDesc &desc) {
  using Kind = Fortran::format::ControlEditDesc::Kind;
  switch (desc.kind) {
  case Kind::T:
    return "T" + std::to_string(desc.count);
  case Kind::TL:
    return "TL" + std::to_string(desc.count);
  case Kind::TR:
    return "TR" + std::to_string(desc.count);
  case Kind::X:
    return std::to_string(desc.count) + "X";
  case Kind::Slash:
    return "/";
  case Kind::Colon:
    return ":";
  case Kind::SS:
    return "SS";
  case Kind::SP:
    return "SP";
  case Kind::S:
    return "S";
  case Kind::P:
    return std::to_string(desc.count) + "P";
  case Kind::BN:
    return "BN";
  case Kind::BZ:
    return "BZ";
  case Kind::RU:
    return "RU";
  case Kind::RD:
    return "RD";
  case Kind::RZ:
    return "RZ";
  case Kind::RN:
    return "RN";
  case Kind::RC:
    return "RC";
  case Kind::RP:
    return "RP";
  case Kind::DC:
    return "DC";
  case Kind::DP:
    return "DP";
  case Kind::Dollar:
    return "$";
  case Kind::Backslash:
    return "\\";
  }
  return "";
}

std::string
FormatItemsToString(const std::list<Fortran::format::FormatItem> &items);

std::string FormatItemToString(const Fortran::format::FormatItem &item) {
  std::string body;
  common::visit(
      common::visitors{
          [&](const Fortran::format::IntrinsicTypeDataEditDesc &desc) {
            body = FormatIntrinsicTypeToString(desc);
          },
          [&](const Fortran::format::DerivedTypeDataEditDesc &desc) {
            body = FormatDerivedTypeToString(desc);
          },
          [&](const Fortran::format::ControlEditDesc &desc) {
            body = FormatControlEditToString(desc);
          },
          [&](const std::string &value) { body = QuoteFortranString(value); },
          [&](const std::list<Fortran::format::FormatItem> &nested) {
            body = "(" + FormatItemsToString(nested) + ")";
          }},
      item.u);

  if (item.repeatCount) {
    return std::to_string(*item.repeatCount) + body;
  }
  return body;
}

std::string
FormatItemsToString(const std::list<Fortran::format::FormatItem> &items) {
  std::string result;
  bool first = true;
  for (const auto &item : items) {
    if (!first) {
      result += ",";
    }
    result += FormatItemToString(item);
    first = false;
  }
  return result;
}

std::string
FormatSpecificationToString(const Fortran::format::FormatSpecification &spec) {
  std::string result = FormatItemsToString(spec.items);
  if (!spec.unlimitedItems.empty()) {
    if (!result.empty()) {
      result += ",";
    }
    result += "*(" + FormatItemsToString(spec.unlimitedItems) + ")";
  }
  return result;
}

bool NamesMatch(const std::string &left, const std::string &right,
                bool case_insensitive) {
  if (!case_insensitive) {
    return left == right;
  }
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(left[i])) !=
        std::tolower(static_cast<unsigned char>(right[i]))) {
      return false;
    }
  }
  return true;
}

bool ScopeHasNamelistGroup(SgScopeStatement *scope,
                           const std::string &group_name) {
  if (scope == nullptr) {
    return false;
  }
  if (isSgGlobal(scope)) {
    return false;
  }
  if (isSgIfStmt(scope)) {
    return ScopeHasNamelistGroup(scope->get_scope(), group_name);
  }
  const bool case_insensitive = SageInterface::is_language_case_insensitive() ||
                                scope->isCaseInsensitive();
  for (SgStatement *stmt : scope->generateStatementList()) {
    SgNamelistStatement *namelist = isSgNamelistStatement(stmt);
    if (namelist == nullptr) {
      continue;
    }
    for (SgNameGroup *group : namelist->get_group_list()) {
      if (group == nullptr) {
        continue;
      }
      if (NamesMatch(group->get_group_name(), group_name, case_insensitive)) {
        return true;
      }
    }
  }
  return ScopeHasNamelistGroup(scope->get_scope(), group_name);
}

std::optional<std::string>
ExtractBareNameFromExpr(const Fortran::parser::Expr &expr) {
  if (auto *designator =
          std::get_if<Fortran::common::Indirection<parser::Designator>>(
              &expr.u)) {
    const parser::Designator &designatorValue = designator->value();
    if (auto *dataRef = std::get_if<parser::DataRef>(&designatorValue.u)) {
      if (auto *name = std::get_if<parser::Name>(&dataRef->u)) {
        return name->ToString();
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string>
ExtractBareNameFromFormat(const Fortran::parser::Format &format) {
  if (auto *expr = std::get_if<parser::Expr>(&format.u)) {
    return ExtractBareNameFromExpr(*expr);
  }
  return std::nullopt;
}

void AppendExpr(SgExprListExp *list, SgExpression *expr) {
  ASSERT_not_null(list);
  ASSERT_not_null(expr);
  list->get_expressions().push_back(expr);
  expr->set_parent(list);
}

SgFunctionCallExp *
BuildFunctionCallFromSymbolIfFound(const std::string &func_name,
                                   SgScopeStatement *scope,
                                   SgExprListExp *param_list) {
  if (scope == nullptr || param_list == nullptr) {
    return nullptr;
  }
  SgFunctionSymbol *func_sym =
      SageInterface::lookupFunctionSymbolInParentScopes(func_name, scope);
  if (func_sym == nullptr) {
    return nullptr;
  }
  SgFunctionRefExp *func_ref = SageBuilder::buildFunctionRefExp_nfi(func_sym);
  ASSERT_not_null(func_ref);
  return SageBuilder::buildFunctionCallExp_nfi(func_ref, param_list);
}

SgExpression *BuildIoUnitExpr(const parser::IoUnit &x) {
  SgExpression *expr{nullptr};
  common::visit(
      common::visitors{
          [&](const parser::Variable &y) { WalkExpr(y, expr); },
          [&](const parser::FileUnitNumber &y) { WalkExpr(y.v, expr); },
          [&](const parser::Star &) {
            expr = SageBuilderCpp17::buildAsteriskShapeExp_nfi();
          }},
      x.u);
  ASSERT_not_null(expr);
  return expr;
}

SgExpression *BuildFormatExpr(const parser::Format &x) {
  SgExpression *expr{nullptr};
  common::visit(
      common::visitors{
          [&](const parser::Expr &y) { WalkExpr(y, expr); },
          [&](const parser::Label &y) { Rose::builder::Build(y, expr); },
          [&](const parser::Star &y) { Rose::builder::Build(y, expr); }},
      x.u);
  ASSERT_not_null(expr);
  return expr;
}

void ApplyIoStatementCommon(SgIOStatement *stmt, const parser::IoControlSpec &x,
                            SgExpression *expr) {
  ASSERT_not_null(stmt);
  ASSERT_not_null(expr);
  if (auto *ioUnit = std::get_if<parser::IoUnit>(&x.u)) {
    stmt->set_unit(expr);
  } else if (auto *msgVar = std::get_if<parser::MsgVariable>(&x.u)) {
    stmt->set_iomsg(expr);
  } else if (auto *statVar = std::get_if<parser::StatVariable>(&x.u)) {
    stmt->set_iostat(expr);
  } else if (auto *errLabel = std::get_if<parser::ErrLabel>(&x.u)) {
    stmt->set_err(expr);
  }
}

void ApplyIoControlSpec(const parser::IoControlSpec &x,
                        SgReadStatement *readStmt,
                        SgWriteStatement *writeStmt) {
  auto *ioStmt = isSgIOStatement(readStmt != nullptr
                                     ? static_cast<SgStatement *>(readStmt)
                                     : static_cast<SgStatement *>(writeStmt));
  ASSERT_not_null(ioStmt);

  common::visit(common::visitors{
                    [&](const parser::IoUnit &y) {
                      SgExpression *expr = BuildIoUnitExpr(y);
                      ioStmt->set_unit(expr);
                      expr->set_parent(ioStmt);
                    },
                    [&](const parser::Format &y) {
                      SgExpression *expr = BuildFormatExpr(y);
                      if (readStmt) {
                        readStmt->set_format(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_format(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::Name &y) {
                      std::string name = y.ToString();
                      SgExpression *expr = SageBuilder::buildDanglingVarRefExp(
                          SgName(name), SageBuilder::topScopeStack());
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_namelist(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_namelist(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::IoControlSpec::CharExpr &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(std::get<1>(y.t), expr);
                      ASSERT_not_null(expr);
                      auto kind = std::get<0>(y.t);
                      if (readStmt) {
                        switch (kind) {
                        case parser::IoControlSpec::CharExpr::Kind::Advance:
                          readStmt->set_advance(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Blank:
                          readStmt->set_blank(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Decimal:
                          readStmt->set_decimal(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Delim:
                          readStmt->set_delim(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Pad:
                          readStmt->set_pad(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Round:
                          readStmt->set_round(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Sign:
                          readStmt->set_sign(expr);
                          break;
                        }
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        switch (kind) {
                        case parser::IoControlSpec::CharExpr::Kind::Advance:
                          writeStmt->set_advance(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Blank:
                          writeStmt->set_blank(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Decimal:
                          writeStmt->set_decimal(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Delim:
                          writeStmt->set_delim(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Pad:
                          writeStmt->set_pad(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Round:
                          writeStmt->set_round(expr);
                          break;
                        case parser::IoControlSpec::CharExpr::Kind::Sign:
                          writeStmt->set_sign(expr);
                          break;
                        }
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::IoControlSpec::Asynchronous &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_asynchronous(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_asynchronous(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::EndLabel &y) {
                      SgExpression *expr{nullptr};
                      Rose::builder::Build(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_end(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_end(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::EorLabel &y) {
                      SgExpression *expr{nullptr};
                      Rose::builder::Build(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_eor(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_eor(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::ErrLabel &y) {
                      SgExpression *expr{nullptr};
                      Rose::builder::Build(y.v, expr);
                      ASSERT_not_null(expr);
                      ioStmt->set_err(expr);
                      expr->set_parent(ioStmt);
                    },
                    [&](const parser::IdVariable &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_id(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_id(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::MsgVariable &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      ioStmt->set_iomsg(expr);
                      expr->set_parent(ioStmt);
                    },
                    [&](const parser::StatVariable &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      ioStmt->set_iostat(expr);
                      expr->set_parent(ioStmt);
                    },
                    [&](const parser::IoControlSpec::Pos &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_pos(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_pos(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::IoControlSpec::Rec &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_rec(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_rec(expr);
                        expr->set_parent(writeStmt);
                      }
                    },
                    [&](const parser::IoControlSpec::Size &y) {
                      SgExpression *expr{nullptr};
                      WalkExpr(y.v, expr);
                      ASSERT_not_null(expr);
                      if (readStmt) {
                        readStmt->set_size(expr);
                        expr->set_parent(readStmt);
                      }
                      if (writeStmt) {
                        writeStmt->set_size(expr);
                        expr->set_parent(writeStmt);
                      }
                    }},
                x.u);
}

void ApplyConnectSpec(const parser::ConnectSpec &x, SgOpenStatement *stmt) {
  ASSERT_not_null(stmt);
  common::visit(
      common::visitors{[&](const parser::FileUnitNumber &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_unit(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::FileNameExpr &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y, expr);
                         ASSERT_not_null(expr);
                         stmt->set_file(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::ConnectSpec::CharExpr &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(std::get<1>(y.t), expr);
                         ASSERT_not_null(expr);
                         auto kind = std::get<0>(y.t);
                         switch (kind) {
                         case parser::ConnectSpec::CharExpr::Kind::Access:
                           stmt->set_access(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Action:
                           stmt->set_action(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Asynchronous:
                           stmt->set_asynchronous(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Blank:
                           stmt->set_blank(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Delim:
                           stmt->set_delim(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Form:
                           stmt->set_form(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Pad:
                           stmt->set_pad(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Position:
                           stmt->set_position(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Round:
                           stmt->set_round(expr);
                           break;
                         case parser::ConnectSpec::CharExpr::Kind::Sign:
                           stmt->set_sign(expr);
                           break;
                         default:
                           break;
                         }
                         expr->set_parent(stmt);
                       },
                       [&](const parser::MsgVariable &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_iomsg(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::StatVariable &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_iostat(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::ConnectSpec::Recl &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_recl(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::ConnectSpec::Newunit &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_unit(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::ErrLabel &y) {
                         SgExpression *expr{nullptr};
                         Rose::builder::Build(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_err(expr);
                         expr->set_parent(stmt);
                       },
                       [&](const parser::StatusExpr &y) {
                         SgExpression *expr{nullptr};
                         WalkExpr(y.v, expr);
                         ASSERT_not_null(expr);
                         stmt->set_status(expr);
                         expr->set_parent(stmt);
                       }},
      x.u);
}

void ApplyCloseSpec(const parser::CloseStmt::CloseSpec &x,
                    SgCloseStatement *stmt) {
  ASSERT_not_null(stmt);
  common::visit(common::visitors{[&](const parser::FileUnitNumber &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_unit(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::StatVariable &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_iostat(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::MsgVariable &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_iomsg(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::ErrLabel &y) {
                                   SgExpression *expr{nullptr};
                                   Rose::builder::Build(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_err(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::StatusExpr &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_status(expr);
                                   expr->set_parent(stmt);
                                 }},
                x.u);
}

void ApplyPositionOrFlushSpec(const parser::PositionOrFlushSpec &x,
                              SgIOStatement *stmt) {
  ASSERT_not_null(stmt);
  common::visit(common::visitors{[&](const parser::FileUnitNumber &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_unit(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::MsgVariable &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_iomsg(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::StatVariable &y) {
                                   SgExpression *expr{nullptr};
                                   WalkExpr(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_iostat(expr);
                                   expr->set_parent(stmt);
                                 },
                                 [&](const parser::ErrLabel &y) {
                                   SgExpression *expr{nullptr};
                                   Rose::builder::Build(y.v, expr);
                                   ASSERT_not_null(expr);
                                   stmt->set_err(expr);
                                   expr->set_parent(stmt);
                                 }},
                x.u);
}
} // namespace

void BuildImpl(parser::ReadStmt &x) {
  std::cout << "BuildImpl(ReadStmt)\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::ReturnStmt &x) {
  SgReturnStmt *stmt{nullptr};
  std::optional<SgExpression *> code{std::nullopt};

  if (x.v) {
    SgExpression *expr{nullptr};
    WalkExpr(x.v, expr);
    code = expr;
  }
  builder.Enter(stmt, code);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildImpl(parser::RewindStmt &x) {
  // RewindStmt std::list<PositionOrFlushSpec> v;
  std::cout << "BuildImpl(RewindStmt)\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::StopStmt &x) {
  // std::tuple<Kind, std::optional<StopCode>, std::optional<ScalarLogicalExpr>>
  // t;
  SgProcessControlStatement *stmt{nullptr};
  std::optional<SgExpression *> code{std::nullopt}, quiet{std::nullopt};
  std::string_view kind{parser::StopStmt::EnumToString(std::get<0>(x.t))};

  // change strings to match builder function
  if (kind == "Stop") {
    kind = "stop";
  } else if (kind == "ErrorStop") {
    kind = "error_stop";
  }

  // stop code
  if (std::get<1>(x.t)) {
    SgExpression *expr{nullptr};
    WalkExpr(std::get<1>(x.t), expr);
    code = expr;
  }

  // quiet
  if (std::get<2>(x.t)) {
    SgExpression *expr{nullptr};
    WalkExpr(std::get<2>(x.t), expr);
    quiet = expr;
  }

  builder.Enter(stmt, std::string{kind}, code, quiet);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildVisitor::Build(parser::PauseStmt &x) {
  SgProcessControlStatement *stmt{nullptr};
  std::optional<SgExpression *> code{std::nullopt};

  if (x.v) {
    SgExpression *expr{nullptr};
    WalkExpr(x.v.value(), expr);
    code = expr;
  }

  builder.Enter(stmt, std::string{"pause"}, code, std::nullopt);
  ApplyCurrentStatementSource(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildImpl(parser::SyncAllStmt &x) {
  std::cout << "BuildImpl(SyncAllStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::SyncImagesStmt &x) {
  std::cout << "BuildImpl(SyncImagesStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::SyncMemoryStmt &x) {
  std::cout << "BuildImpl(SyncMemoryStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::SyncTeamStmt &x) {
  std::cout << "BuildImpl(SyncTeamStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::UnlockStmt &x) {
  std::cout << "BuildImpl(UnlockStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::WaitStmt &x) {
  std::cout << "BuildImpl(WaitStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::WhereStmt &x) {
  std::cout << "BuildImpl(WhereStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::WriteStmt &x) {
  std::cout << "BuildImpl(WriteStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::ForallStmt &x) {
  std::cout << "BuildImpl(ForallStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::ArithmeticIfStmt &x) {
  SgExpression *condition{nullptr};
  WalkExpr(std::get<parser::Expr>(x.t), condition);
  ASSERT_not_null(condition);

  SgExpression *lessExpr{nullptr};
  SgExpression *equalExpr{nullptr};
  SgExpression *greaterExpr{nullptr};
  Rose::builder::Build(std::get<1>(x.t), lessExpr);
  Rose::builder::Build(std::get<2>(x.t), equalExpr);
  Rose::builder::Build(std::get<3>(x.t), greaterExpr);
  ASSERT_not_null(lessExpr);
  ASSERT_not_null(equalExpr);
  ASSERT_not_null(greaterExpr);

  SgLabelRefExp *lessRef = isSgLabelRefExp(lessExpr);
  SgLabelRefExp *equalRef = isSgLabelRefExp(equalExpr);
  SgLabelRefExp *greaterRef = isSgLabelRefExp(greaterExpr);
  ROSE_ASSERT(lessRef != nullptr);
  ROSE_ASSERT(equalRef != nullptr);
  ROSE_ASSERT(greaterRef != nullptr);

  SgArithmeticIfStatement *stmt =
      new SgArithmeticIfStatement(condition, lessRef, equalRef, greaterRef);
  ASSERT_not_null(stmt);
  SageInterface::setSourcePosition(stmt);
  condition->set_parent(stmt);
  lessRef->set_parent(stmt);
  equalRef->set_parent(stmt);
  greaterRef->set_parent(stmt);

  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void BuildImpl(parser::AssignStmt &x) {
  std::cout << "BuildImpl(AssignStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::AssignedGotoStmt &x) {
  std::cout << "BuildImpl(AssignedGotoStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::PauseStmt &x) {
  std::cout << "BuildImpl(PauseStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::NamelistStmt &x) {
  std::cout << "BuildImpl(NamelistStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::ParameterStmt &x) {
  std::cout << "BuildImpl(ParameterStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::OldParameterStmt &x) {
  std::cout << "BuildImpl(OldParameterStmt)\n";
  ABORT_NO_IMPL;
}

// Expr
//
void Build(parser::CharLiteralConstantSubstring &x, SgExpression *&expr) {
  SgExpression *base{nullptr};
  WalkExpr(std::get<parser::CharLiteralConstant>(x.t), base);
  ASSERT_not_null(base);

  auto &range = std::get<parser::SubstringRange>(x.t);
  SgExpression *lower{nullptr};
  SgExpression *upper{nullptr};
  if (std::get<0>(range.t)) {
    WalkExpr(std::get<0>(range.t).value(), lower);
  }
  if (std::get<1>(range.t)) {
    WalkExpr(std::get<1>(range.t).value(), upper);
  }
  if (lower == nullptr) {
    lower = SageBuilderCpp17::buildNullExpression_nfi();
  }
  if (upper == nullptr) {
    upper = SageBuilderCpp17::buildNullExpression_nfi();
  }
  SgExpression *stride = SageBuilder::buildIntVal_nfi(std::string("1"));

  SgExpression *subscript =
      SageBuilderCpp17::buildSubscriptExpression_nfi(lower, upper, stride);
  ASSERT_not_null(subscript);
  expr = SageBuilderCpp17::buildPntrArrRefExp_nfi(base, subscript);
}

void Build(parser::SubstringInquiry &x, SgExpression *&expr) {
  Build(x.v, expr);
}

namespace {
SgType *BuildIntrinsicTypeSpec(parser::IntrinsicTypeSpec &x) {
  if (auto *intSpec = std::get_if<parser::IntegerTypeSpec>(&x.u)) {
    SgExpression *expr{nullptr};
    if (intSpec->v) {
      WalkExpr(intSpec->v.value(), expr);
    }
    return SageBuilder::buildIntType(expr);
  }
  if (auto *unsignedSpec = std::get_if<parser::UnsignedTypeSpec>(&x.u)) {
    SgExpression *expr{nullptr};
    if (unsignedSpec->v) {
      WalkExpr(unsignedSpec->v.value(), expr);
    }
    return SageBuilder::buildUnsignedIntType(expr);
  }
  if (auto *realSpec = std::get_if<parser::IntrinsicTypeSpec::Real>(&x.u)) {
    SgExpression *expr{nullptr};
    if (realSpec->kind) {
      WalkExpr(realSpec->kind.value(), expr);
    }
    return SageBuilder::buildFloatType(expr);
  }
  if (std::get_if<parser::IntrinsicTypeSpec::DoublePrecision>(&x.u)) {
    return SageBuilder::buildDoubleType();
  }
  if (auto *complexSpec =
          std::get_if<parser::IntrinsicTypeSpec::Complex>(&x.u)) {
    SgExpression *expr{nullptr};
    if (complexSpec->kind) {
      WalkExpr(complexSpec->kind.value(), expr);
    }
    SgType *base = SageBuilder::buildFloatType(expr);
    return SageBuilder::buildComplexType(base);
  }
  if (auto *charSpec =
          std::get_if<parser::IntrinsicTypeSpec::Character>(&x.u)) {
    if (charSpec->selector) {
      SgExpression *len{nullptr};
      SgExpression *kind{nullptr};
      common::visit(
          common::visitors{[&](parser::LengthSelector &z) { WalkExpr(z, len); },
                           [&](parser::CharSelector::LengthAndKind &z) {
                             if (z.length) {
                               WalkExpr(z.length, len);
                             }
                             WalkExpr(z.kind, kind);
                           }},
          charSpec->selector->u);
      if (len == nullptr) {
        len = SageBuilder::buildIntVal_nfi(1, "1");
        SageInterface::setSourcePosition(len);
      }
      SgType *type = SageBuilder::buildStringType(len);
      if (kind != nullptr) {
        type->set_type_kind(kind);
      }
      return type;
    }
    return SageBuilder::buildCharType();
  }
  if (auto *logicalSpec =
          std::get_if<parser::IntrinsicTypeSpec::Logical>(&x.u)) {
    SgExpression *expr{nullptr};
    if (logicalSpec->kind) {
      WalkExpr(logicalSpec->kind.value(), expr);
    }
    return SageBuilder::buildBoolType(expr);
  }
  if (std::get_if<parser::IntrinsicTypeSpec::DoubleComplex>(&x.u)) {
    SgType *base = SageBuilder::buildDoubleType();
    return SageBuilder::buildComplexType(base);
  }
  return nullptr;
}

SgType *BuildTypeSpec(parser::TypeSpec &x) {
  if (auto *intrinsic = std::get_if<parser::IntrinsicTypeSpec>(&x.u)) {
    return BuildIntrinsicTypeSpec(*intrinsic);
  }
  if (auto *derived = std::get_if<parser::DerivedTypeSpec>(&x.u)) {
    return BuildDerivedTypeSpec(*derived);
  }
  return nullptr;
}
} // namespace

void Build(parser::AcValue &x, SgExpression *&expr);

void Build(parser::AcImpliedDo &x, SgExpression *&expr) {
  std::list<SgExpression *> object_items;
  for (auto &value : std::get<0>(x.t)) {
    SgExpression *itemExpr{nullptr};
    Build(value, itemExpr);
    ASSERT_not_null(itemExpr);
    object_items.push_back(itemExpr);
  }

  SgExprListExp *objectList =
      SageBuilderCpp17::buildExprListExp_nfi(object_items);
  for (SgExpression *item : objectList->get_expressions()) {
    if (item) {
      item->set_parent(objectList);
    }
  }

  auto &control = std::get<1>(x.t);
  auto &bounds = std::get<parser::AcImpliedDoControl::Bounds>(control.t);
  SgExpression *init{nullptr};
  SgExpression *upper{nullptr};
  SgExpression *step{nullptr};
  BuildLoopBounds(bounds, init, upper, step);
  ASSERT_not_null(init);
  ASSERT_not_null(upper);
  ASSERT_not_null(step);

  SgImpliedDo *impliedDo =
      new SgImpliedDo(init, upper, step, objectList, nullptr);
  SageInterface::setSourcePosition(impliedDo);
  objectList->set_parent(impliedDo);
  init->set_parent(impliedDo);
  upper->set_parent(impliedDo);
  step->set_parent(impliedDo);
  expr = impliedDo;
}

void Build(parser::AcValue &x, SgExpression *&expr) {
  common::visit(common::visitors{
                    [&](parser::AcValue::Triplet &y) {
                      SgExpression *lower{nullptr};
                      SgExpression *upper{nullptr};
                      SgExpression *step{nullptr};
                      WalkExpr(std::get<0>(y.t), lower);
                      WalkExpr(std::get<1>(y.t), upper);
                      if (std::get<2>(y.t)) {
                        WalkExpr(std::get<2>(y.t).value(), step);
                      } else {
                        step = SageBuilder::buildIntVal_nfi(std::string("1"));
                      }
                      expr = SageBuilder::buildSubscriptExpression_nfi(
                          lower, upper, step);
                    },
                    [&](common::Indirection<parser::Expr> &y) {
                      WalkExpr(y.value(), expr);
                    },
                    [&](common::Indirection<parser::AcImpliedDo> &y) {
                      Build(y.value(), expr);
                    }},
                x.u);
}

void Build(parser::ArrayConstructor &x, SgExpression *&expr) {
  Build(x.v, expr);
}

void Build(parser::AcSpec &x, SgExpression *&expr) {
  std::list<SgExpression *> values;
  for (auto &value : x.values) {
    SgExpression *valueExpr{nullptr};
    Build(value, valueExpr);
    ASSERT_not_null(valueExpr);
    values.push_back(valueExpr);
  }

  SgExprListExp *exprList = SageBuilderCpp17::buildExprListExp_nfi(values);
  for (SgExpression *item : exprList->get_expressions()) {
    if (item) {
      item->set_parent(exprList);
    }
  }
  SgType *explicitType{nullptr};
  if (x.type) {
    explicitType = BuildTypeSpec(x.type.value());
  }
  expr = SageBuilder::buildAggregateInitializer_nfi(exprList, explicitType);
}

void Build(parser::StructureConstructor &x, SgExpression *&expr) {
  auto &derivedSpec = std::get<parser::DerivedTypeSpec>(x.t);
  auto &components = std::get<std::list<parser::ComponentSpec>>(x.t);

  SgType *type = BuildDerivedTypeSpec(derivedSpec);
  std::string typeName = std::get<parser::Name>(derivedSpec.t).ToString();

  std::list<SgExpression *> args;
  for (auto &component : components) {
    SgExpression *value{nullptr};
    WalkExpr(std::get<parser::ComponentDataSource>(component.t).v.value(),
             value);
    ASSERT_not_null(value);
    args.push_back(value);
  }

  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  expr = SageBuilder::buildFunctionCallExp(
      SgName(typeName), type ? type : SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(expr);
}

void Build(parser::Expr::Parentheses &x, SgExpression *&expr) {
  WalkExpr(x.v.value(), expr);
  if (expr != nullptr) {
    SageBuilderCpp17::set_need_paren(expr);
  }
}

void Build(parser::Expr::UnaryPlus &x, SgExpression *&expr) {
  WalkExpr(x.v.value(), expr);
}

void Build(parser::Expr::Negate &x, SgExpression *&expr) {
  SgExpression *operand{nullptr};
  WalkExpr(x.v.value(), operand);
  ASSERT_not_null(operand);
  expr = SageBuilderCpp17::buildMinusOp_nfi(operand, /*is_prefix*/ true);
}

void BuildExprVisitor::Build(parser::Expr::NOT &x /*, SgExpression* &expr*/) {
  SgExpression *operand{nullptr};
  WalkExpr(x.v.value(), operand);
  ASSERT_not_null(operand);
  this->set(SageBuilder::buildNotOp_nfi(operand));
}

void BuildExprVisitor::Build(parser::Expr::UnaryPlus &x) {
  SgExpression *operand{nullptr};
  WalkExpr(x.v.value(), operand);
  ASSERT_not_null(operand);
  this->set(SageBuilder::buildUnaryAddOp_nfi(operand));
}

void BuildExprVisitor::Build(parser::Expr::Negate &x) {
  SgExpression *operand{nullptr};
  WalkExpr(x.v.value(), operand);
  ASSERT_not_null(operand);
  this->set(SageBuilder::buildMinusOp_nfi(operand));
}

void BuildExprVisitor::Build(parser::Expr::Power &x /*, SgExpression* &expr*/) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildExponentiationOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::Multiply &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildMultiplyOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::Divide &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildDivideOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::Add &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildAddOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::Subtract &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildSubtractOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::Concat &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildConcatenationOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::LT &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildLessThanOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::LE &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildLessOrEqualOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::EQ &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildEqualityOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::NE &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildNotEqualOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::GE &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildGreaterOrEqualOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::GT &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildGreaterThanOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::AND &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildAndOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::OR &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildOrOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::EQV &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildEqualityOp_nfi(lhs, rhs));
}

void BuildExprVisitor::Build(parser::Expr::NEQV &x) {
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  BuildExpressions(x, lhs, rhs);
  this->set(SageBuilder::buildNotEqualOp_nfi(lhs, rhs));
}

void Build(parser::Expr::DefinedBinary &x, SgExpression *&expr) {
  SgExpression *lhs{nullptr};
  SgExpression *rhs{nullptr};
  WalkExpr(std::get<1>(x.t).value(), lhs);
  WalkExpr(std::get<2>(x.t).value(), rhs);
  ASSERT_not_null(lhs);
  ASSERT_not_null(rhs);
  std::string name{std::get<0>(x.t).v.ToString()};
  std::list<SgExpression *> args{lhs, rhs};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  expr = SageBuilder::buildFunctionCallExp(
      SgName(name), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(expr);
}

void Build(parser::Expr::DefinedUnary &x, SgExpression *&expr) {
  SgExpression *arg{nullptr};
  WalkExpr(std::get<1>(x.t).value(), arg);
  ASSERT_not_null(arg);
  std::string name{std::get<0>(x.t).v.ToString()};
  std::list<SgExpression *> args{arg};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  expr = SageBuilder::buildFunctionCallExp(
      SgName(name), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(expr);
}

void Build(parser::Expr::PercentLoc &x, SgExpression *&expr) {
  SgExpression *arg{nullptr};
  WalkExpr(x.v.value(), arg);
  ASSERT_not_null(arg);
  std::list<SgExpression *> args{arg};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  expr = SageBuilder::buildFunctionCallExp(
      SgName("loc"), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(expr);
}

void Build(parser::Expr::NOT &x, SgExpression *&expr) {
  SgExpression *operand{nullptr};
  WalkExpr(x.v.value(), operand);
  ASSERT_not_null(operand);
  expr = SageBuilder::buildNotOp_nfi(operand);
  ASSERT_not_null(expr);
}

void Build(parser::Expr::ComplexConstructor &x, SgExpression *&expr) {
  SgExpression *lhs{nullptr};
  SgExpression *rhs{nullptr};
  WalkExpr(std::get<0>(x.t).value(), lhs);
  WalkExpr(std::get<1>(x.t).value(), rhs);
  ASSERT_not_null(lhs);
  ASSERT_not_null(rhs);
  std::list<SgExpression *> args{lhs, rhs};
  SgExprListExp *params = SageBuilderCpp17::buildExprListExp_nfi(args);
  ASSERT_not_null(params);
  expr = SageBuilder::buildFunctionCallExp(
      SgName("cmplx"), SageBuilder::buildUnknownType(), params,
      SageBuilder::topScopeStack());
  ASSERT_not_null(expr);
}

void Build(parser::StructureComponent &x, SgExpression *&expr) {
  SgExpression *base{nullptr};
  Build(x.base, base);
  ASSERT_not_null(base);
  std::string componentName = x.component.ToString();
  SgExpression *component{nullptr};
  SgType *baseType = base->get_type();
  SgVariableSymbol *componentSymbol = nullptr;
  if (baseType != nullptr) {
    if (SgClassDefinition *def = findClassDefinition(baseType)) {
      componentSymbol = findComponentSymbol(def, SgName(componentName));
      if (componentSymbol != nullptr &&
          componentSymbol->get_declaration() == nullptr) {
        componentSymbol = nullptr;
      }
    }
  }
  if (componentSymbol != nullptr) {
    component = SageBuilder::buildVarRefExp_nfi(componentSymbol);
    SageInterface::setSourcePosition(component);
  } else {
    component = SageBuilderCpp17::buildVarRefExp_nfi(componentName,
                                                     /*scope*/ nullptr,
                                                     /*allow_implicit*/ false);
  }
  ASSERT_not_null(component);
  expr = SageBuilder::buildDotExp(base, component);
  ASSERT_not_null(expr);
}

void Build(parser::ArrayElement &x, SgExpression *&expr) {
  SgExpression *base{nullptr};
  Build(x.base, base);
  ASSERT_not_null(base);

  SgExprListExp *subscripts = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(subscripts);
  for (auto &subscript : x.subscripts) {
    SgExpression *subExpr{nullptr};
    Build(subscript, subExpr);
    ASSERT_not_null(subExpr);
    subscripts->get_expressions().push_back(subExpr);
    subExpr->set_parent(subscripts);
  }

  expr = SageBuilderCpp17::buildPntrArrRefExp_nfi(base, subscripts);
  ASSERT_not_null(expr);
}

void Build(parser::CoindexedNamedObject &x, SgExpression *&expr) {
  SgExpression *base{nullptr};
  Build(x.base, base);
  ASSERT_not_null(base);

  SgExpression *selector{nullptr};
  Build(x.imageSelector, selector);
  ASSERT_not_null(selector);

  SgCAFCoExpression *coExpr =
      new SgCAFCoExpression(/*teamId*/ nullptr, selector, base);
  ASSERT_not_null(coExpr);
  SageInterface::setSourcePosition(coExpr);

  base->set_parent(coExpr);
  selector->set_parent(coExpr);

  expr = coExpr;
}

void Build(parser::ImageSelector &x, SgExpression *&expr) {
  SgExprListExp *exprList = SageBuilder::buildExprListExp_nfi();
  ASSERT_not_null(exprList);
  for (auto &cosubscript : std::get<0>(x.t)) {
    SgExpression *item{nullptr};
    WalkExpr(cosubscript.thing, item);
    ASSERT_not_null(item);
    exprList->get_expressions().push_back(item);
    item->set_parent(exprList);
  }
  if (!std::get<1>(x.t).empty()) {
    std::cerr << "[FATAL] Image selector specs (STAT/TEAM) not supported.\n";
    ROSE_ABORT();
  }
  expr = exprList;
}

void Build(parser::ImageSelectorSpec &x, SgExpression *&expr) {
  expr = SageBuilderCpp17::buildNullExpression_nfi();
}

void Build(parser::SectionSubscript &x, SgExpression *&expr) {
  common::visit(
      common::visitors{[&](parser::IntExpr &y) { WalkExpr(y, expr); },
                       [&](parser::SubscriptTriplet &y) { Build(y, expr); }},
      x.u);
}

void Build(parser::SubscriptTriplet &x, SgExpression *&expr) {
  SgExpression *lower{nullptr};
  SgExpression *upper{nullptr};
  SgExpression *stride{nullptr};

  if (std::get<0>(x.t)) {
    WalkExpr(std::get<0>(x.t).value(), lower);
  }
  if (std::get<1>(x.t)) {
    WalkExpr(std::get<1>(x.t).value(), upper);
  }
  if (std::get<2>(x.t)) {
    WalkExpr(std::get<2>(x.t).value(), stride);
  }

  if (lower == nullptr) {
    lower = SageBuilderCpp17::buildNullExpression_nfi();
  }
  if (upper == nullptr) {
    upper = SageBuilderCpp17::buildNullExpression_nfi();
  }
  if (stride == nullptr) {
    stride = SageBuilder::buildIntVal_nfi(std::string("1"));
  }

  expr = SageBuilderCpp17::buildSubscriptExpression_nfi(lower, upper, stride);
  ASSERT_not_null(expr);
}

// ExecutableConstruct
//
void Build(parser::AssociateConstruct &x) {
  info(x, "Rose::builder::Build(AssiciateConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::BlockConstruct &x) {
  // std::tuple<Statement<BlockStmt>, BlockSpecificationPart, Block,
  // Statement<EndBlockStmt>>
  info(x, "Rose::builder::Build(BlockConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::CaseConstruct &x) {
  info(x, "Rose::builder::Build(CaseConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::CaseConstruct::Case &x, SgStatement *&stmt) {
  // std::tuple<Statement<CaseStmt>, Block> t;
  info(x, "Rose::builder::Build(CaseConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::CaseStmt &x, std::list<SgExpression *> &case_list) {
  //  std::tuple<CaseSelector, std::optional<Name>> t;
  info(x, "Rose::builder::Build(CaseStmt)");
  ABORT_NO_IMPL;
}

void Build(parser::CaseValueRange::Range &x, SgExpression *&range) {
  std::cout << "Rose::builder::Build(Range)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ChangeTeamConstruct &x) {
  info(x, "Rose::builder::Build(ChangeTeamConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::CriticalConstruct &x) {
  info(x, "Rose::builder::Build(CriticalConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::LabelDoStmt &x) {
  info(x, "Rose::builder::Build(CriticalConstruct)");
  ABORT_NO_IMPL;
}

void Build(parser::EndDoStmt &x) {
  std::cout << "Rose::builder::Build(EndDoStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::IfConstruct &x) {
  // std::tuple<>
  //   Statement<IfThenStmt>, Block, std::list<ElseIfBlock>,
  //   std::optional<ElseBlock>, Statement<EndIfStmt>
  std::cout << "Rose::builder::Build(IfConstruct)\n";
  ABORT_NO_IMPL;
}

void Build(parser::IfThenStmt &x, SgExpression *&expr) {
  // std::tuple<std::optional<Name>, ScalarLogicalExpr> t;
  std::cout << "Rose::builder::Build(IfThenStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::IfConstruct::ElseBlock &x, SgBasicBlock *&false_body) {
  // std::tuple<Statement<ElseStmt>, Block> t;
  std::cout << "Rose::builder::Build(ElseBlock)\n";
  ABORT_NO_IMPL;
}

void Build(std::list<parser::IfConstruct::ElseIfBlock> &x,
           SgBasicBlock *&else_if_block, SgIfStmt *&else_if_stmt) {
  std::cout << "Rose::builder::Build(std::list<ElseIfBlock>)\n";
  ABORT_NO_IMPL;
}

void Build(parser::SelectRankConstruct &x) {
  std::cout << "Rose::builder::Build(SelectRankConstruct)\n";
  ABORT_NO_IMPL;
}

void Build(parser::SelectTypeConstruct &x) {
  std::cout << "Rose::builder::Build(SelectTypeConstruct)\n";
  ABORT_NO_IMPL;
}

void Build(parser::WhereConstruct &x) {
  std::cout << "Rose::builder::Build(WhereConstruct)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ForallConstruct &x) {
  std::cout << "Rose::builder::Build(ForallConstruct)\n";
  ABORT_NO_IMPL;
}

void Build(parser::OpenMPConstruct &x) {
  std::cerr << "[WARN] Rose::builder::Build(OpenMPConstruct) unimplemented\n";
  ABORT_NO_IMPL;
}

void Build(parser::OpenACCConstruct &x) {
  std::cerr << "[WARN] Rose::builder::Build(OpenACCConstruct) unimplemented\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::CompilerDirective &x) {
  if (x.source.empty()) {
    return;
  }
  std::string directive = x.source.ToString();
  if (directive.empty()) {
    return;
  }
  AppendPragmasFromCharBlock(x.source);
}

void BuildVisitor::Build(parser::OmpBeginBlockDirective &x) {
  if (x.source.empty()) {
    return;
  }
  AppendPragmasFromCharBlockIfMissing(x.source);
}

void BuildVisitor::Build(parser::OmpBeginLoopDirective &x) {
  if (x.source.empty()) {
    return;
  }
  AppendPragmasFromCharBlockIfMissing(x.source);
}

void Build(parser::AccEndCombinedDirective &x) {
  std::cerr
      << "[WARN] Rose::builder::Build(AccEndCombinedDirective) unimplemented\n";
  ABORT_NO_IMPL;
}

// CUFKernelDoConstruct
void Build(parser::CUFKernelDoConstruct &x) {
  std::cerr
      << "[WARN] Rose::builder::Build(CUFKernelDoConstruct) unimplemented\n";
  ABORT_NO_IMPL;
}

void Build(parser::OmpEndLoopDirective &x) {
  std::cerr << "[WARN] Rose::builder::Build(Build(OmpEndLoopDirective) "
               "unimplemented\n";
  ABORT_NO_IMPL;
}

// DoConstructf3037
void Build(parser::NonLabelDoStmt &x, SgExpression *&name,
           SgExpression *&control) {
  std::cout << "Rose::builder::Build(NonLabelDoStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::LoopControl::Concurrent &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(LoopControl::Concurrent)\n";
  ABORT_NO_IMPL;

  // x.t (tuple)
  // [0] parser::ConcurrentHeader
  // [1] [LocalitySpec (a list)]
}

// SpecificationConstruct
//
void Build(parser::DerivedTypeStmt &x, std::string &name,
           std::list<LanguageTranslation::ExpressionKind> &modifiers) {
  // std::tuple<std::list<TypeAttrSpec>, Name, std::list<Name>> t;
  std::cout << "Rose::builder::Build(DerivedTypeStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DataComponentDefStmt &x, SgStatement *&stmt) {
  // std::tuple<> DeclarationTypeSpec, std::list<ComponentAttrSpec>,
  // std::list<ComponentOrFill>
  using namespace Fortran::parser;

  SgType *type{nullptr};
  Build(std::get<DeclarationTypeSpec>(x.t), type);
  ASSERT_not_null(type);

  std::list<LanguageTranslation::ExpressionKind> modifiers{};
  for (auto &attr : std::get<std::list<ComponentAttrSpec>>(x.t)) {
    getComponentAttrSpec(attr, modifiers, type);
  }

  std::list<EntityDeclTuple> initInfo{};
  for (auto &item : std::get<std::list<ComponentOrFill>>(x.t)) {
    common::visit(common::visitors{
                      [&](ComponentDecl &decl) { Build(decl, initInfo, type); },
                      [&](FillDecl &) { /* No ROSE equivalent */ }},
                  item.u);
  }

  if (initInfo.empty()) {
    return;
  }

  SgVariableDeclaration *varDecl{nullptr};
  builder.Enter(varDecl, type, initInfo);
  builder.Leave(varDecl, modifiers);
  stmt = varDecl;
}

void Build(Fortran::parser::ComponentDecl &x,
           std::list<EntityDeclTuple> &componentDecls, SgType *baseType) {
  std::string name;
  SgExpression *init{nullptr};
  SgType *type{nullptr};
  Build(x, name, init, type, baseType);
  componentDecls.push_back(std::make_tuple(name, type, init));
}

void Build(parser::ComponentDecl &x, std::string &name, SgExpression *&init,
           SgType *&type, SgType *base_type) {
  //  std::tuple<> Name, std::optional<ComponentArraySpec>,
  //  std::optional<CoarraySpec>, std::optional<CharLength>,
  //               std::optional<Initialization>
  using namespace Fortran::parser;

  name = std::get<Name>(x.t).ToString();
  init = nullptr;
  type = nullptr;

  SgType *localBase = base_type;
  if (auto &opt = std::get<std::optional<CharLength>>(x.t)) {
    SgExpression *lenExpr{nullptr};
    Build(opt.value(), lenExpr);
    ASSERT_not_null(lenExpr);

    if (auto *stringType = isSgTypeString(localBase)) {
      SgTypeString *newType = SageBuilder::buildStringType(lenExpr);
      newType->set_type_kind(stringType->get_type_kind());
      localBase = newType;
    } else if (isSgTypeChar(localBase)) {
      localBase = SageBuilder::buildStringType(lenExpr);
    }
  }

  SgType *entityType = localBase;
  if (auto &opt = std::get<std::optional<ComponentArraySpec>>(x.t)) {
    Build(opt.value(), entityType, localBase);
  }

  if (auto &opt = std::get<std::optional<CoarraySpec>>(x.t)) {
    Build(opt.value(), entityType, entityType);
  }

  type = entityType;

  if (auto &opt = std::get<std::optional<Initialization>>(x.t)) {
    Build(opt.value(), init);
  }
}

void Build(parser::EnumDef &x) {
  std::cout << "Rose::builder::Build(EnumDef)\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::InterfaceBlock &x) {
  using namespace Fortran::parser;

  auto &stmt = std::get<Statement<InterfaceStmt>>(x.t);
  auto &specs = std::get<std::list<InterfaceSpecification>>(x.t);

  SgInterfaceStatement::generic_spec_enum kind =
      SgInterfaceStatement::e_unnamed_interface_type;
  SgName interfaceName;

  if (auto *optGeneric =
          std::get_if<std::optional<GenericSpec>>(&stmt.statement.u)) {
    if (*optGeneric) {
      GenericSpec &generic = optGeneric->value();
      common::visit(
          common::visitors{
              [&](Name &y) {
                kind = SgInterfaceStatement::e_named_interface_type;
                interfaceName = SgName(y.ToString());
              },
              [&](DefinedOperator &y) {
                kind = SgInterfaceStatement::e_operator_interface_type;
                auto operatorName = [&](const DefinedOperator &op) {
                  using IntrinsicOp = DefinedOperator::IntrinsicOperator;
                  return common::visit(
                      common::visitors{
                          [&](const DefinedOpName &name) {
                            std::string opName = name.v.ToString();
                            if (!opName.empty() && opName.front() != '.') {
                              opName.insert(opName.begin(), '.');
                            }
                            if (!opName.empty() && opName.back() != '.') {
                              opName.push_back('.');
                            }
                            return opName;
                          },
                          [&](IntrinsicOp opKind) {
                            switch (opKind) {
                            case IntrinsicOp::Power:
                              return std::string("**");
                            case IntrinsicOp::Multiply:
                              return std::string("*");
                            case IntrinsicOp::Divide:
                              return std::string("/");
                            case IntrinsicOp::Add:
                              return std::string("+");
                            case IntrinsicOp::Subtract:
                              return std::string("-");
                            case IntrinsicOp::Concat:
                              return std::string("//");
                            case IntrinsicOp::LT:
                              return std::string(".LT.");
                            case IntrinsicOp::LE:
                              return std::string(".LE.");
                            case IntrinsicOp::EQ:
                              return std::string(".EQ.");
                            case IntrinsicOp::NE:
                              return std::string(".NE.");
                            case IntrinsicOp::GE:
                              return std::string(".GE.");
                            case IntrinsicOp::GT:
                              return std::string(".GT.");
                            case IntrinsicOp::NOT:
                              return std::string(".NOT.");
                            case IntrinsicOp::AND:
                              return std::string(".AND.");
                            case IntrinsicOp::OR:
                              return std::string(".OR.");
                            case IntrinsicOp::EQV:
                              return std::string(".EQV.");
                            case IntrinsicOp::NEQV:
                              return std::string(".NEQV.");
                            }
                            ROSE_ABORT();
                          }},
                      op.u);
                };
                interfaceName = SgName(operatorName(y));
              },
              [&](GenericSpec::Assignment &) {
                kind = SgInterfaceStatement::e_assignment_interface_type;
                interfaceName = SgName("=");
              },
              [&](GenericSpec::ReadFormatted &) {
                kind = SgInterfaceStatement::e_named_interface_type;
                interfaceName = SgName(generic.source.ToString());
              },
              [&](GenericSpec::ReadUnformatted &) {
                kind = SgInterfaceStatement::e_named_interface_type;
                interfaceName = SgName(generic.source.ToString());
              },
              [&](GenericSpec::WriteFormatted &) {
                kind = SgInterfaceStatement::e_named_interface_type;
                interfaceName = SgName(generic.source.ToString());
              },
              [&](GenericSpec::WriteUnformatted &) {
                kind = SgInterfaceStatement::e_named_interface_type;
                interfaceName = SgName(generic.source.ToString());
              }},
          generic.u);
    }
  }

  SgInterfaceStatement *interfaceStmt =
      new SgInterfaceStatement(interfaceName, kind);
  ASSERT_not_null(interfaceStmt);
  SageInterface::setSourcePosition(interfaceStmt);
  SageInterface::appendStatement(interfaceStmt, SageBuilder::topScopeStack());

  auto stabilizeParamScopeParent = [](SgScopeStatement *paramScope) {
    if (paramScope == nullptr) {
      return;
    }
    SgBasicBlock *parentBlock = isSgBasicBlock(paramScope->get_parent());
    if (parentBlock == nullptr) {
      return;
    }
    SgScopeStatement *stableScope =
        isSgScopeStatement(parentBlock->get_parent());
    if (stableScope != nullptr) {
      paramScope->set_parent(stableScope);
    }
  };

  for (auto &spec : specs) {
    common::visit(
        common::visitors{
            [&](InterfaceBody &body) {
              common::visit(
                  common::visitors{
                      [&](InterfaceBody::Subroutine &subr) {
                        auto &subrStmt =
                            std::get<Statement<SubroutineStmt>>(subr.t);
                        auto &endStmt =
                            std::get<Statement<EndSubroutineStmt>>(subr.t);
                        std::list<std::string> dummyArgs;
                        LanguageTranslation::FunctionModifierList modifiers;
                        getSubroutineStmt(subrStmt.statement, dummyArgs,
                                          modifiers, *this);

                        std::string name =
                            std::get<Name>(subrStmt.statement.t).ToString();
                        SgFunctionParameterList *paramList{nullptr};
                        SgScopeStatement *paramScope{nullptr};
                        bool isDefDecl{false};

                        builder.Enter(paramList, paramScope, name,
                                      /*function_type*/ nullptr, isDefDecl);
                        stabilizeParamScopeParent(paramScope);

                        auto &specPart =
                            std::get<common::Indirection<SpecificationPart>>(
                                subr.t);
                        Walk(specPart.value());

                        builder.Leave(paramList, paramScope, dummyArgs);

                        SgProcedureHeaderStatement *procDecl = SageBuilder::
                            buildNondefiningProcedureHeaderStatement(
                                SgName(name), SageBuilder::buildVoidType(),
                                paramList,
                                SgProcedureHeaderStatement::
                                    e_subroutine_subprogram_kind,
                                SageBuilder::topScopeStack());
                        ASSERT_not_null(procDecl);

                        procDecl->set_functionParameterScope(
                            isSgFunctionParameterScope(paramScope));
                        procDecl->set_parent(interfaceStmt);

                        SgInterfaceBody *bodyNode =
                            new SgInterfaceBody(SgName(name), procDecl,
                                                /*use_function_name*/ false);
                        ASSERT_not_null(bodyNode);
                        bodyNode->set_parent(interfaceStmt);
                        SageInterface::setSourcePosition(bodyNode);
                        interfaceStmt->get_interface_body_list().push_back(
                            bodyNode);
                      },
                      [&](InterfaceBody::Function &func) {
                        auto &funcStmt =
                            std::get<Statement<FunctionStmt>>(func.t);
                        std::list<std::string> dummyArgs;
                        std::string name =
                            std::get<Name>(funcStmt.statement.t).ToString();
                        for (auto &arg :
                             std::get<std::list<Name>>(funcStmt.statement.t)) {
                          dummyArgs.push_back(arg.ToString());
                        }

                        SgFunctionParameterList *paramList{nullptr};
                        SgScopeStatement *paramScope{nullptr};
                        bool isDefDecl{false};

                        SgType *returnType{nullptr};
                        LanguageTranslation::FunctionModifierList modifiers;
                        BuildPrefix(std::get<std::list<PrefixSpec>>(
                                        funcStmt.statement.t),
                                    modifiers, returnType);

                        std::string resultName;
                        bool undeclaredResultName{false};
                        auto &suffix = std::get<std::optional<Suffix>>(
                            funcStmt.statement.t);
                        if (suffix && suffix->resultName) {
                          resultName = suffix->resultName->ToString();
                        }
                        const bool case_insensitive =
                            SageInterface::is_language_case_insensitive();
                        const bool useFunctionNameResult =
                            resultName.empty() ||
                            NamesMatch(resultName, name, case_insensitive);
                        if (!resultName.empty() && !useFunctionNameResult &&
                            returnType) {
                          undeclaredResultName = true;
                        }

                        auto &specPart =
                            std::get<common::Indirection<SpecificationPart>>(
                                func.t);
                        if (!returnType) {
                          std::string lookupName =
                              resultName.empty() ? name : resultName;
                          BuildFunctionReturnType(specPart.value(), lookupName,
                                                  returnType);
                        }

                        SgType *param_result_type =
                            useFunctionNameResult ? returnType : nullptr;
                        builder.Enter(paramList, paramScope, name,
                                      param_result_type, isDefDecl);
                        stabilizeParamScopeParent(paramScope);

                        Walk(specPart.value());

                        if (!returnType) {
                          std::string implicitName =
                              resultName.empty() ? name : resultName;
                          returnType = SageBuilder::buildFortranImplicitType(
                              implicitName);
                        }

                        if (undeclaredResultName &&
                            paramScope->lookup_variable_symbol(resultName) ==
                                nullptr) {
                          SageBuilderCpp17::fixUndeclaredResultName(
                              resultName, paramScope, returnType);
                        }
                        if (!resultName.empty() &&
                            paramScope->lookup_variable_symbol(resultName) ==
                                nullptr) {
                          SageBuilderCpp17::fixUndeclaredResultName(
                              resultName, paramScope, returnType);
                        }
                        if (useFunctionNameResult &&
                            paramScope->lookup_variable_symbol(name) ==
                                nullptr) {
                          SageBuilderCpp17::fixUndeclaredResultName(
                              name, paramScope, returnType);
                        }

                        builder.Leave(paramList, paramScope, dummyArgs);

                        if (!returnType) {
                          returnType = SageBuilder::buildUnknownType();
                        }

                        SgProcedureHeaderStatement *procDecl = SageBuilder::
                            buildNondefiningProcedureHeaderStatement(
                                SgName(name), returnType, paramList,
                                SgProcedureHeaderStatement::
                                    e_function_subprogram_kind,
                                SageBuilder::topScopeStack());
                        ASSERT_not_null(procDecl);

                        procDecl->set_functionParameterScope(
                            isSgFunctionParameterScope(paramScope));
                        procDecl->set_parent(interfaceStmt);

                        auto hasModifier =
                            [&](LanguageTranslation::FunctionModifier kind) {
                              return std::find(modifiers.begin(),
                                               modifiers.end(),
                                               kind) != modifiers.end();
                            };
                        if (hasModifier(LanguageTranslation::FunctionModifier::
                                            e_function_modifier_recursive)) {
                          procDecl->get_functionModifier().setRecursive();
                        }
                        if (hasModifier(LanguageTranslation::FunctionModifier::
                                            e_function_modifier_pure)) {
                          procDecl->get_functionModifier().setPure();
                        }
                        if (hasModifier(LanguageTranslation::FunctionModifier::
                                            e_function_modifier_elemental)) {
                          procDecl->get_functionModifier().setElemental();
                        }

                        const std::string lookupName =
                            resultName.empty() ? name : resultName;
                        if (SgVariableSymbol *result_symbol =
                                paramScope->lookup_variable_symbol(
                                    lookupName)) {
                          SgInitializedName *result_name =
                              result_symbol->get_declaration();
                          ASSERT_not_null(result_name);
                          procDecl->set_result_name(result_name);
                          result_name->set_parent(procDecl);
                        }

                        SgInterfaceBody *bodyNode =
                            new SgInterfaceBody(SgName(name), procDecl,
                                                /*use_function_name*/ false);
                        ASSERT_not_null(bodyNode);
                        bodyNode->set_parent(interfaceStmt);
                        SageInterface::setSourcePosition(bodyNode);
                        interfaceStmt->get_interface_body_list().push_back(
                            bodyNode);
                      }},
                  body.u);
            },
            [&](Statement<ProcedureStmt> &procStmt) {
              auto &names = std::get<std::list<Name>>(procStmt.statement.t);
              for (auto &procName : names) {
                SgInterfaceBody *bodyNode =
                    new SgInterfaceBody(SgName(procName.ToString()), nullptr,
                                        /*use_function_name*/ true);
                ASSERT_not_null(bodyNode);
                bodyNode->set_parent(interfaceStmt);
                SageInterface::setSourcePosition(bodyNode);
                interfaceStmt->get_interface_body_list().push_back(bodyNode);
              }
            }},
        spec.u);
  }
}

void Build(parser::StructureDef &x) {
  std::cout << "Rose::builder::Build(StructureDef)\n";
  ABORT_NO_IMPL;
}

void Build(parser::GenericStmt &x) {
  std::cout << "Rose::builder::Build(GenericStmt)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ProcedureDeclarationStmt &x) {
  BuildVisitor visitor;
  visitor.Build(x);
}

// OpenACCDeclarativeConstruct
void Build(parser::OpenMPDeclarativeConstruct &x) {
  std::cout << "Rose::builder::Build(OpenMPDeclarativeConstruct)\n";
  ABORT_NO_IMPL;
}

// OpenACCDeclarativeConstruct
void Build(parser::OpenACCDeclarativeConstruct &x) {
  std::cout << "Rose::builder::Build(OpenACCDeclarativeConstruct)\n";
  ABORT_NO_IMPL;
}

// AccessSpec
void getModifiers(parser::AccessSpec &x,
                  LanguageTranslation::ExpressionKind &m) {
  using namespace LanguageTranslation;
  switch (x.v) {
  case parser::AccessSpec::Kind::Public:
    m = ExpressionKind::e_access_modifier_public;
    break;
  case parser::AccessSpec::Kind::Private:
    m = ExpressionKind::e_access_modifier_private;
    break;
  }
}

// IntentSpec
void getModifiers(const parser::IntentSpec &x,
                  LanguageTranslation::ExpressionKind &m) {
  using namespace LanguageTranslation;
  switch (x.v) {
  case parser::IntentSpec::Intent::In:
    m = ExpressionKind::e_type_modifier_intent_in;
    break;
  case parser::IntentSpec::Intent::Out:
    m = ExpressionKind::e_type_modifier_intent_out;
    break;
  case parser::IntentSpec::Intent::InOut:
    m = ExpressionKind::e_type_modifier_intent_inout;
    break;
  }
}

// LanguageBindingSpec
void getModifiers(const parser::LanguageBindingSpec &x,
                  LanguageTranslation::ExpressionKind &m) {
  m = LanguageTranslation::ExpressionKind::e_type_modifier_bind_c;
}

// TypeAttrSpec
void getModifiers(parser::TypeAttrSpec &x,
                  LanguageTranslation::ExpressionKind &m) {
  // std::variant<> Abstract, AccessSpec, BindC, Extends
  using namespace Fortran::parser;
  using namespace LanguageTranslation;

  std::cout << "[WARN] getModifiers(TypeAttrSpec):\n";

  common::visit(
      common::visitors{
          [&](Abstract &y) { m = ExpressionKind::e_type_modifier_abstract; },
          [&](AccessSpec &y) { getModifiers(y, m); },
          [&](TypeAttrSpec::BindC &y) {
            m = ExpressionKind::e_type_modifier_bind_c;
          },
          [&](TypeAttrSpec::Extends &y) { m = ExpressionKind::e_unknown; }},
      x.u);
}

void getComponentAttrSpec(
    parser::ComponentAttrSpec &x,
    std::list<LanguageTranslation::ExpressionKind> &modifiers,
    SgType *&baseType) {
  // std::variant<> AccessSpec, Allocatable, CoarraySpec, Contiguous,
  //                ComponentArraySpec, Pointer, common::CUDADataAttr
  using namespace Fortran::parser;
  using namespace LanguageTranslation;

  common::visit(
      common::visitors{
          [&](ComponentArraySpec &y) {
            SgType *type{nullptr};
            Build(y, type, baseType);
            baseType = type;
          },
          [&](CoarraySpec &y) {
            SgType *type{nullptr};
            Build(y, type, baseType);
            baseType = type;
          },
          [&](const Allocatable &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_allocatable);
          },
          [&](const Contiguous &) {
            modifiers.push_back(ExpressionKind::e_storage_modifier_contiguous);
          },
          [&](AccessSpec &y) {
            ExpressionKind m;
            getModifiers(y, m);
            modifiers.push_back(m);
          },
          [&](const Pointer &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_pointer);
          },
          [&](const common::CUDADataAttr &) { ABORT_NO_IMPL; },
          [&](const ErrorRecovery &) { ABORT_NO_IMPL; }},
      x.u);
}

void getAttrSpec(parser::AttrSpec &x,
                 std::list<LanguageTranslation::ExpressionKind> &modifiers,
                 SgType *&baseType) {
  // std::variant<> AccessSpec, Allocatable, Asynchronous, CoarraySpec,
  // Contiguous,
  //                ArraySpec, External, IntentSpec, Intrinsic,
  //                LanguageBindingSpec, Optional, Parameter, Pointer,
  //                Protected, Save, Target, Value, Volatile,
  //                common::CUDADataAttr
  using namespace Fortran::parser;
  using namespace LanguageTranslation;

  common::visit(
      common::visitors{
          [&](ArraySpec &y) {
            /*DIMENSION*/
            SgType *type{nullptr};
            Build(y, type, baseType);
            baseType = type;
          },
          [&](CoarraySpec &y) {
            /*CODIMENSION*/
            SgType *type{nullptr};
            Build(y, type, baseType);
            baseType = type;
          },
          [&](ComponentArraySpec &y) {
            SgType *type{nullptr};
            Build(y, type, baseType);
            baseType = type;
          },
          [&](const IntentSpec &y) {
            ExpressionKind m;
            getModifiers(y, m);
            modifiers.push_back(m);
          },
          [&](const LanguageBindingSpec &y) {
            ExpressionKind m;
            getModifiers(y, m);
            modifiers.push_back(m);
          },
          [&](const common::CUDADataAttr &) {
            ABORT_NO_IMPL; /*CUDADataAttr*/
          },
          [&](const Allocatable &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_allocatable);
          },
          [&](const Asynchronous &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_asynchronous);
          },
          [&](const Contiguous &) {
            modifiers.push_back(ExpressionKind::e_storage_modifier_contiguous);
          },
          [&](AccessSpec &y) {
            ExpressionKind m;
            getModifiers(y, m);
            modifiers.push_back(m);
          },
          [&](const External &) {
            modifiers.push_back(ExpressionKind::e_storage_modifier_external);
          },
          [&](const Intrinsic &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_intrinsic);
          },
          [&](const Optional &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_optional);
          },
          [&](const Parameter &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_parameter);
          },
          [&](const Pointer &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_pointer);
          },
          [&](const Protected &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_protected);
          },
          [&](const Save &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_save);
          },
          [&](const Target &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_target);
          },
          [&](const Value &) {
            modifiers.push_back(ExpressionKind::e_param_binding_value);
          },
          [&](const Volatile &) {
            modifiers.push_back(ExpressionKind::e_type_modifier_volatile);
          },
          [&](const auto &y) { ABORT_NO_IMPL; }},
      x.u);
}

} // namespace Rose::builder
