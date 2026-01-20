#include "sage3basic.h"

#include "SageTreeBuilder.h"

#include "sage-build.h"

#include "flang-sage.h"

#include "unparse-sage.h"

#include "BuildExprVisitor.h"

#include "BuildVisitor.h"

#include "FlangModuleInfo.h"

#include <cstdint>

#include <iostream>

#include <optional>

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

template <typename T>
void BuildExprVisitor::BuildExpressions(T &x, SgExpression *&lhs,
                                        SgExpression *&rhs) {
  WalkExpr(std::get<0>(x.t).value(), lhs); // lhs Expr
  WalkExpr(std::get<1>(x.t).value(), rhs); // rhs Expr
}

// Name
void BuildExprVisitor::Build(Fortran::parser::Name &x) {
  std::string name{x.ToString()};
  this->set(SageBuilderCpp17::buildVarRefExp_nfi(name));
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

template <typename T>
SourcePosition BuildSourcePosition(const Fortran::parser::Statement<T> &x,
                                   Order from) {
  std::optional<SourcePosition> pos{std::nullopt};

  if (auto sourceInfo{cooked_->GetSourcePositionRange(x.source)}) {
    if (from == Order::begin)
      pos.emplace(SourcePosition{sourceInfo->first.path, sourceInfo->first.line,
                                 sourceInfo->first.column});
    else
      pos.emplace(SourcePosition{sourceInfo->second.path,
                                 sourceInfo->second.line,
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
    std::cout << "... FirstSourcePosition: implicit_part_stmts list count is "
              << implicit_part_stmts.size() << "\n";
    //      const auto & implicit_part_stmt
  }

  const auto &decl_stmts{std::get<4>(x.t)};
  if (decl_stmts.size() > 0) {
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
  cooked_ = &cooked;
  // TODO: make go away
  common::LangOptions langOpts{};

  // Testing...
  parser::Encoding encoding{Fortran::parser::Encoding::LATIN_1};
  parser::Unparse(llvm::outs(), x, langOpts, encoding, true /*capitalize*/,
                  false, nullptr, cooked_);

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
    labels.push_back(std::to_string(stmt->label.value()));
  }

  if (auto pos{FirstSourcePosition(spec)}) {
    srcPosBody.emplace(*pos);
  }

  // Fortran only needs an end statement so check for no beginning source
  // position
  if (!srcPosBody) {
    srcPosBody.emplace(srcPosEnd);
  }

  // If there is no ProgramStmt the source begins at the body of the program
  if (!srcPosBegin) {
    srcPosBegin.emplace(*srcPosBody);
  }

  // Build the SgProgramHeaderStatement node
  //
  SgProgramHeaderStatement *programDecl{nullptr};
  std::optional<std::string> opt_name{*name};

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
    endLabel = std::to_string(end.label.value());
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
                       LanguageTranslation::FunctionModifierList &modifiers) {
  // std::tuple<> std::list<PrefixSpec>, Name, std::list<DummyArg>,
  // std::optional<LanguageBindingSpec>
  using namespace Fortran::parser;

  // DummyArg list
  DummyArg(std::get<std::list<parser::DummyArg>>(x.t), args);

  if (auto &opt = std::get<std::optional<LanguageBindingSpec>>(x.t)) {
    // WARNING, likely need optional expression (or NullExpressions?)
    // BuildExpr(opt.value(), expr);
    // WalkExpr(opt.value(), expr);
    LanguageTranslation::ExpressionKind m;
    getModifiers(opt.value(), m);
    ABORT_NO_IMPL;
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
    srcPosBody.emplace(srcPosEnd);
  }
  Rose::builder::SourcePositions sources(*srcPosBegin, *srcPosBody, srcPosEnd);

  SgFunctionParameterList *paramList{nullptr};
  SgScopeStatement *paramScope{nullptr};
  SgFunctionDeclaration *funcDecl{nullptr};

  std::list<std::string> dummyArgs;
  LanguageTranslation::FunctionModifierList modifiers;
  getSubroutineStmt(stmt.statement, dummyArgs, modifiers);

  // Enter SageTreeBuilder for SgFunctionParameterList
  bool isDefDecl{true};
  std::string name{std::get<Name>(stmt.statement.t).ToString()};
  builder.Enter(paramList, paramScope, name, /*function_type*/ nullptr,
                isDefDecl);

  // SpecificationPart and ExecutionPart
  Walk(std::get<SpecificationPart>(x.t));
  Walk(std::get<ExecutionPart>(x.t));

  // Leave SageTreeBuilder for SgFunctionParameterList
  builder.Leave(paramList, paramScope, dummyArgs);

  // Begin SageTreeBuilder for SgFunctionDeclaration
  builder.Enter(funcDecl, name, /*return_type*/ nullptr, paramList, modifiers,
                isDefDecl, sources, comments);

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

// FunctionSubprogram
void BuildVisitor::Build(parser::FunctionSubprogram &x) {
  // std::tuple<> Statement<FunctionStmt>, SpecificationPart, ExecutionPart,
  //              std::optional<InternalSubprogramPart>,
  //              Statement<EndFunctionStmt>
  std::cout << "BuildVisitor::Build(FunctionSubprogram)\n";
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
    srcPosBody.emplace(srcPosEnd);
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
  if (!resultName.empty() && returnType) {
    undeclaredResultName = true;
  }

  // Peek into the SpecificationPart to get the return type if don't already
  // know it
  if (!returnType) {
    BuildFunctionReturnType(std::get<parser::SpecificationPart>(x.t),
                            resultName, returnType);
  }

  // Enter SageTreeBuilder for SgFunctionParameterList
  builder.Enter(paramList, paramScope, name, returnType, isDefDecl);

  // SpecificationPart
  Walk(std::get<SpecificationPart>(x.t));

  // Need to create initialized name here for result, if result is not declared
  // in SpecificationPart
  if (undeclaredResultName) {
    SageBuilderCpp17::fixUndeclaredResultName(resultName, paramScope,
                                              returnType);
  }

  // ExecutionPart
  Walk(std::get<ExecutionPart>(x.t));

  // Leave SageTreeBuilder for SgFunctionParameterList
  builder.Leave(paramList, paramScope, dummyArgs);

  // Begin SageTreeBuilder for SgFunctionDeclaration
  builder.Enter(functionDecl, name, returnType, paramList, modifiers, isDefDecl,
                sources, comments);

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
  std::cout << "Rose::builder::Build(Submodule)\n";
  ABORT_NO_IMPL;
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

  ROSE_ASSERT(stmt.statement.v);
  std::string name{stmt.statement.v->ToString()};
  bool haveEndStmt{static_cast<bool>(end.statement.v)};

  std::optional<SourcePosition> srcPosBody{
      FirstSourcePosition(std::get<SpecificationPart>(x.t))};
  std::optional<SourcePosition> srcPosBegin{
      BuildSourcePosition(stmt, Order::begin)};
  SourcePosition srcPosEnd{BuildSourcePosition(end, Order::end)};
  if (!srcPosBody) {
    srcPosBody.emplace(srcPosEnd);
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
  std::cout << "Rose::builder::Build(SpecificationPart)\n";
}

void BuildVisitor::Build(parser::AssignmentStmt &x) {
  // std::tuple<> Variable, Expr
  using namespace Fortran::parser;

  std::vector<std::string> labels{};
  SgExpression *lhs{nullptr}, *rhs{nullptr};
  SgExprStatement *stmt{nullptr};

  WalkExpr(std::get<Variable>(x.t), lhs);
  WalkExpr(std::get<Expr>(x.t), rhs);

  std::vector<SgExpression *> vars;
  vars.push_back(lhs);

  // Begin SageTreeBuilder
  builder.Enter(stmt, rhs, vars);

  // Leave SageTreeBuilder
  builder.Leave(stmt, labels);
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
} // namespace

void BuildVisitor::Build(parser::DoConstruct &x) {
  //  std::tuple<Statement<NonLabelDoStmt>, Block, Statement<EndDoStmt>> t;
  //  bool IsDoNormal() const;  bool IsDoWhile() const; bool IsDoConcurrent()
  //  const;

  SgWhileStmt *whileStmt{nullptr};
  SgFortranDo *doStmt{nullptr};
  auto &loopControl = std::get<2>(std::get<0>(x.t).statement.t);
  const LoopControlInfo control = BuildLoopControl(loopControl);
  if (control.isConcurrent) {
    std::cerr << "[WARN] Do concurrent is not supported yet.\n";
    ROSE_ABORT();
  }

  // Enter SageTreeBuilder
  if (control.isWhile) {
    ASSERT_not_null(control.condition);
    builder.Enter(whileStmt, control.condition);
  } else {
    builder.Enter(doStmt, control.initialization, control.bound,
                  control.increment);
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

void BuildVisitor::Build(parser::LabelDoStmt &x) {
  // LabelDoStmt std::tuple<Label, std::optional<LoopControl>> t;
  const auto endLabel = std::get<parser::Label>(x.t);
  const LoopControlInfo control = BuildLoopControl(std::get<1>(x.t));

  if (control.isConcurrent) {
    std::cerr << "[WARN] Do concurrent is not supported yet.\n";
    ROSE_ABORT();
  }

  if (control.isWhile) {
    ASSERT_not_null(control.condition);
    SgWhileStmt *whileStmt{nullptr};
    builder.Enter(whileStmt, control.condition);

    if (label_) {
      SageInterface::setFortranNumericLabel(whileStmt,
                                            static_cast<int>(label_.value()),
                                            SgLabelSymbol::e_start_label_type);
    }

    label_do_stack_.push_back(
        LabelDoFrame{endLabel, LabelDoFrame::Kind::While, whileStmt});
    return;
  }

  SgFortranDo *doStmt{nullptr};
  builder.Enter(doStmt, control.initialization, control.bound,
                control.increment);
  doStmt->set_has_end_statement(false);

  if (label_) {
    SageInterface::setFortranNumericLabel(doStmt,
                                          static_cast<int>(label_.value()),
                                          SgLabelSymbol::e_start_label_type);
  }

  label_do_stack_.push_back(
      LabelDoFrame{endLabel, LabelDoFrame::Kind::FortranDo, doStmt});
}

void BuildVisitor::CloseLabelDoLoops(const parser::Label &label) {
  while (!label_do_stack_.empty() &&
         label_do_stack_.back().end_label == label) {
    LabelDoFrame frame = label_do_stack_.back();
    label_do_stack_.pop_back();
    auto *labelScope =
        SageInterface::getEnclosingFunctionDefinition(frame.stmt);
    ASSERT_not_null(labelScope);
    SgName labelName(StringUtility::numberToString(label));
    SgLabelSymbol *labelSymbol = labelScope->lookup_label_symbol(labelName);
    if (labelSymbol == nullptr) {
      std::cerr << "Missing label symbol for DO end label " << label << "\n";
      ROSE_ABORT();
    }
    SgLabelRefExp *ref = SageBuilder::buildLabelRefExp(labelSymbol);
    ref->set_parent(frame.stmt);
    frame.stmt->set_end_numeric_label(ref);
    if (frame.kind == LabelDoFrame::Kind::FortranDo) {
      auto *doStmt = isSgFortranDo(frame.stmt);
      ASSERT_not_null(doStmt);
      builder.Leave(doStmt);
    } else {
      auto *whileStmt = isSgWhileStmt(frame.stmt);
      ASSERT_not_null(whileStmt);
      builder.Leave(whileStmt, /*hasEndDo*/ false);
    }
  }
}

// ActionStmt(s)
//
void BuildVisitor::Build(parser::ContinueStmt &) {
  SgFortranContinueStmt *stmt{nullptr};
  builder.Enter(stmt);
  builder.Leave(stmt, getLabels());
}

void BuildVisitor::Build(parser::FailImageStmt &) {
  SgProcessControlStatement *stmt{nullptr};
  builder.Enter(stmt, "fail_image", std::nullopt, std::nullopt);
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

  // OutputItem
  // std::variant<Expr, common::Indirection<OutputImpliedDo>> u;
  std::list<SgExpression *> items{};

  // OutputItems
  // TODO: move to BuildVisitor:: function
  for (auto &item : std::get<1>(x.t)) {
    common::visit(
        common::visitors{
            [&](parser::Expr &y) {
              WalkExpr(y, format);
              ASSERT_not_null(format);
              items.push_back(format);
            },
            [&](auto &y) {
              std::cout
                  << "[WARN] IMPL_ME_ common::Indirection<OutputImpliedDo\n";
              ASSERT_require(false);
            }},
        item.u);
  }

  // SgAsteriskShapeExp used to represent Star
  // Fortran::parser::Star assumed, not parsed
  format = SageBuilderCpp17::buildAsteriskShapeExp_nfi();

  // Enter/Leave SageTreeBuilder
  builder.Enter(stmt, format, items);
  builder.Leave(stmt);
}

void BuildVisitor::Build(parser::WriteStmt &x) {
  SgProcessControlStatement *stmt{nullptr};
  std::cerr << "...Build(WriteStmt)...\n";
  if (x.iounit) {
    std::cerr << "...Build(WriteStmt)...   has IoUnit!\n";
  } else {
    std::cerr << "...Build(WriteStmt)...    no IoUnit!\n";
  }
  if (x.format) {
    std::cerr << "...Build(WriteStmt)...   has Format!\n";
  } else {
    std::cerr << "...Build(WriteStmt)...    no Format!\n";
  }
  for (auto &spec : x.controls) {
    std::cerr << "...Build(WriteStmt)...    IoControlSpec...list\n";
  }
}

void BuildVisitor::BuildPrefix(
    std::list<parser::PrefixSpec> &x,
    LanguageTranslation::FunctionModifierList &modifiers, SgType *&type) {
  // std::variant<> - DeclarationTypeSpec, Elemental, Impure, Module,
  // Non_Recursive,
  //                  Pure, Recursive, Attributes, Launch_Bounds, Cluster_Dims
  std::cout << "[WARN] BuildVisitor::Build(PrefixSpec): NEEDS further "
               "IMPLEMENTATION\n";

  for (auto &prefix : x) {
    common::visit(
        common::visitors{
            [&](parser::DeclarationTypeSpec &y) { BuildType(y, type); },
            [&](auto &y) {
              std::cout << "   [WARN] IMPL_ME_ SOMETHING something else\n";
            }},
        prefix.u);
  }
}

void BuildSuffix(parser::Suffix &x, std::string &resultName) {
  std::cout << "BuildVisitor::BuildSuffix(Suffix)\n";

  // std::optional<Name>
  if (x.resultName) {
    resultName = x.resultName.value().ToString();
  }

  // TODO:
  //  std::optional<LanguageBindingSpec> binding;
  ABORT_NO_IMPL;
}

void Build(parser::Substring &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(Substring)\n";
  ABORT_NO_IMPL;
}

void Build(parser::FunctionReference &x, SgExpression *&expr) {
#if PRINT_FLANG_TRAVERSAL
  std::cout << "Rose::builder::Build(FunctionReference)\n";
#endif

  std::list<SgExpression *> arg_list;
  std::string func_name;

  Build(x.v, arg_list, func_name); // Call

  SgExprListExp *param_list = SageBuilderCpp17::buildExprListExp_nfi(arg_list);

  // Begin SageTreeBuilder
  SgFunctionCallExp *func_call;
  builder.Enter(func_call, func_name, param_list);

  // Use wrapper function because can't use inheritance of pointers until Rose
  // accepts Cpp17
  expr = SageBuilder::buildFunctionCallExp(func_call);
}

void Build(parser::Call &x, std::list<SgExpression *> &arg_list,
           std::string &name) {
  std::cout << "Rose::builder::Build(Call)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ProcComponentRef &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(ProcComponentRef)\n";
  ABORT_NO_IMPL;
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
  std::cout << "Rose::builder::Build(NamedConstant)\n";
  ABORT_NO_IMPL;
}

void Build(parser::Expr::IntrinsicBinary &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(IntrinsicBinary)\n";
  ABORT_NO_IMPL;
}

// LiteralConstant(s)
void BuildImpl(parser::HollerithLiteralConstant &x, SgExpression *&expr) {
  std::cout << "BuildImpl(HollerithLiteralConstant)\n";
  ABORT_NO_IMPL;
}

// KindParam - for now create a string (seems that a Sage value expression
// should have Fortran kind
void BuildImpl(std::optional<Fortran::parser::KindParam> &x,
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

void BuildImpl(parser::SignedIntLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<> - CharBlock, std::optional<KindParam>
  std::cout << "BuildImpl(SignedIntLiteralConstant)\n";
  ABORT_NO_TEST;

  expr = SageBuilder::buildIntVal_nfi(stoi(std::get<0>(x.t).ToString()));
}

void BuildImpl(parser::RealLiteralConstant &x, SgExpression *&expr) {
  // has std::optional<KindParam> kind
  expr = SageBuilder::buildFloatVal_nfi(x.real.source.ToString());
}

void BuildImpl(parser::SignedRealLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<std::optional<Sign>, RealLiteralConstant> t;
  ABORT_NO_IMPL;
}

void BuildImpl(parser::ComplexLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<ComplexPart, ComplexPart> t;
  std::cout << "BuildImpl(ComplexLiteralConstant)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::BOZLiteralConstant &x, SgExpression *&expr) {
  std::cout << "BuildImpl(BOZLiteralConstant)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::CharLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<std::optional<KindParam>, std::string> t;

  if (std::get<0>(x.t)) {
    // KindParam
    ABORT_NO_IMPL;
  }
  expr = SageBuilder::buildStringVal_nfi(x.GetString());
}

void BuildImpl(parser::LogicalLiteralConstant &x, SgExpression *&expr) {
  // std::tuple<> bool, std::optional<KindParam>
  if (std::get<1>(x.t)) {
    // KindParam
    ABORT_NO_IMPL;
  }
  expr = SageBuilder::buildBoolValExp_nfi(std::get<0>(x.t));
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
  std::cerr << "TypeParamValue\n";
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
                                   ABORT_NO_TEST;
                                 }},
                x.u);
}

void BuildImpl(parser::CharLength &x, SgExpression *&expr) {
  // CharLength std::variant<TypeParamValue, std::uint64_t> u;
  using namespace Fortran;

  common::visit(common::visitors{[&](std::uint64_t &y) {
                                   std::string strVal{std::to_string(y)};
                                   expr = SageBuilder::buildLongLongIntVal_nfi(
                                       y, strVal);
                                 },
                                 [&](parser::TypeParamValue &y) {
                                   WalkExpr(y, expr);
                                   ABORT_NO_TEST;
                                 }},
                x.u);
}

void BuildImpl(parser::CommonBlockObject &x, SgExpression *&expr) {
  // CommonBlockObject std::tuple<Name, std::optional<ArraySpec>> t;

  // ArraySpec
  if (std::get<std::optional<parser::ArraySpec>>(x.t)) {
    std::cout << "BuildImpl(CommonBlockObject): TODO: optional ArraySpec\n";
    ABORT_NO_IMPL;
  }

  std::string name{std::get<parser::Name>(x.t).ToString()};
  expr = SageBuilder::buildVarRefExp(name);
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
  std::cout << "Rose::builder::Build(std::list<ImplicitSpec>)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ImplicitSpec &x, SgType *&type,
           std::list<std::tuple<char, std::optional<char>>> &letter_spec_list) {
  // std::tuple<DeclarationTypeSpec, std::list<LetterSpec>> t;
  std::cout << "Rose::builder::Build(ImplicitSpec)\n";
  ABORT_NO_IMPL;
}

void Build(std::list<parser::LetterSpec> &x,
           std::list<std::tuple<char, std::optional<char>>> &letter_spec_list) {
  std::cout << "Rose::builder::Build(std::list<LetterSpec>)\n";
  ABORT_NO_IMPL;
}

void Build(parser::LetterSpec &x,
           std::tuple<char, std::optional<char>> &letter_spec) {
  // std::tuple<Location, std::optional<Location>> t;
  // using Location = const char *;
  std::cout << "Rose::builder::Build(LetterSpec)\n";
  ABORT_NO_IMPL;
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
          [&](const std::list<ImplicitSpec> &y) { ABORT_NO_IMPL; },
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

#if TEMPORARY_COOL_FIXME
  // Need variant I think (no, like an enum, just assign to ONE of the
  // variants!)
  //  x.u = std::move(makeImplicitNone());
  //  const std::list<ImplicitStmt::ImplicitNoneNameSpec>
  //  &implicitList{makeImplicitNone()};
#else
  const std::list<ImplicitStmt::ImplicitNoneNameSpec> &implicitList{
      ImplicitStmt::ImplicitNoneNameSpec::External,
      ImplicitStmt::ImplicitNoneNameSpec::Type};
  const ImplicitStmt &xx{std::move(implicitList)};
  x.u = std::move(implicitList);
#endif
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
      std::cerr << "Error: cannot find module '" << moduleName << "'\n";
      ROSE_ABORT();
    }
  }

  useStmt->set_module(moduleStmt);

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

  auto attachRenamePair = [&](SgRenamePair *renamePair) {
    useStmt->get_rename_list().push_back(renamePair);
    renamePair->set_parent(useStmt);
  };

  if (!hasOnly) {
    if (renamePairs.empty()) {
      for (const auto &entry : publicSymbols) {
        for (SgSymbol *symbol : entry.second) {
          SgName symbolName = symbol->get_name();
          if (!currentScope->symbol_exists(symbolName)) {
            SgAliasSymbol *aliasSymbol = new SgAliasSymbol(symbol, false);
            currentScope->insert_symbol(symbolName, aliasSymbol);
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
          SgSymbol *aliasSymbol = nullptr;
          if (auto *varSymbol = isSgVariableSymbol(symbol)) {
            SgInitializedName *initName =
                SageInterface::deepCopy(varSymbol->get_declaration());
            initName->set_name(localName);
            initName->set_scope(currentScope);
            aliasSymbol = new SgVariableSymbol(initName);
          } else {
            aliasSymbol = new SgAliasSymbol(symbol, true, localName);
          }
          currentScope->insert_symbol(localName, aliasSymbol);
          renamedSymbols.insert(symbol);
        }
      }

      for (const auto &entry : publicSymbols) {
        for (SgSymbol *symbol : entry.second) {
          if (renamedSymbols.find(symbol) == renamedSymbols.end()) {
            SgName symbolName = symbol->get_name();
            SgAliasSymbol *aliasSymbol = new SgAliasSymbol(symbol, false);
            currentScope->insert_symbol(symbolName, aliasSymbol);
          }
        }
      }
    }
  } else {
    for (SgRenamePair *renamePair : renamePairs) {
      attachRenamePair(renamePair);
      const SymbolList *symbols = findPublicSymbols(renamePair->get_use_name());
      if (symbols == nullptr) {
        continue;
      }
      bool isRenamed =
          (renamePair->get_use_name() != renamePair->get_local_name());
      for (SgSymbol *symbol : *symbols) {
        SgName localName = renamePair->get_local_name();
        if (!currentScope->symbol_exists(localName)) {
          SgSymbol *aliasSymbol = nullptr;
          if (auto *varSymbol = isSgVariableSymbol(symbol)) {
            SgInitializedName *initName =
                SageInterface::deepCopy(varSymbol->get_declaration());
            initName->set_name(localName);
            initName->set_scope(currentScope);
            aliasSymbol = new SgVariableSymbol(initName);
          } else {
            aliasSymbol = isRenamed ? new SgAliasSymbol(symbol, true, localName)
                                    : new SgAliasSymbol(symbol, false);
          }
          currentScope->insert_symbol(localName, aliasSymbol);
        }
      }
    }
  }

  builder.Leave(useStmt);
  use_statement_fixup(currentScope);
}

void BuildVisitor::Build(parser::CommonStmt &x) {
  // CommonStmt std::list<Block> blocks;
  // Block std::tuple<std::optional<Name>, std::list<CommonBlockObject>> t;

  // Begin SageTreeBuilder for SgCommonBlock
  SgCommonBlock *blockStmt{nullptr};
  builder.Enter(blockStmt);

  for (auto &block : x.blocks) {
    std::string blockName{""};
    SgExprListExp *blockObjects{SageBuilder::buildExprListExp_nfi()};

    if (std::get<std::optional<parser::Name>>(block.t)) {
      blockName = std::get<std::optional<parser::Name>>(block.t)->ToString();
    }

    // CommonBlockObject(s)
    for (auto &object :
         std::get<std::list<parser::CommonBlockObject>>(block.t)) {
      SgExpression *varRef{nullptr};
      WalkExpr(object, varRef);
      blockObjects->get_expressions().push_back(varRef);
    }

    SgCommonBlockObject *sageObject =
        SageBuilder::buildCommonBlockObject(blockName, blockObjects);
    blockStmt->get_block_list().push_back(sageObject);
  }

  // Leave SageTreeBuilder for SgCommonBlock
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
      WalkExpr(setObject, expr);
      ASSERT_not_null(expr);

      SgDataStatementObject *dataObject = new SgDataStatementObject();
      ASSERT_not_null(dataObject);
      if (!dataObject->get_variableReference_list()) {
        dataObject->set_variableReference_list(
            SageBuilder::buildExprListExp_nfi());
      }

      dataObject->get_variableReference_list()->get_expressions().push_back(
          expr);

      dataGroup->get_object_list().push_back(dataObject);
    }

    // Add data statement values
    for (auto &setValue : std::get<1>(set.t)) {
      SgExpression *expr{nullptr};
      WalkExpr(setValue, expr);
      ASSERT_not_null(expr);

      // TODO: other kind variants
      auto valueKind{SgDataStatementValue::e_explict_list};
      SgDataStatementValue *dataValue = new SgDataStatementValue(valueKind);
      ASSERT_not_null(dataValue);
      if (!dataValue->get_initializer_list()) {
        dataValue->set_initializer_list(SageBuilder::buildExprListExp_nfi());
      }

      dataValue->get_initializer_list()->get_expressions().push_back(expr);

      dataGroup->get_value_list().push_back(dataValue);
    }
    groups.push_back(dataGroup);
  }
}

void BuildVisitor::Build(parser::TypeDeclarationStmt &x) {
  // std::tuple<> DeclarationTypeSpec, std::list<AttrSpec>,
  // std::list<EntityDecl>
  using namespace Fortran::parser;

  SgType *type{nullptr};
  BuildType(std::get<parser::DeclarationTypeSpec>(x.t), type);

  std::list<LanguageTranslation::ExpressionKind> modifiers{};
  for (auto &attr : std::get<std::list<AttrSpec>>(x.t)) {
    getAttrSpec(attr, modifiers, type);
  }

  std::list<EntityDeclTuple> initInfo{};
  EntityDecls(std::get<std::list<EntityDecl>>(x.t), initInfo,
              type); // std::list<EntityDecl>

  SgVariableDeclaration *varDecl{nullptr};
  builder.Enter(varDecl, type, initInfo);
  builder.Leave(varDecl, modifiers);
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
    LanguageTranslation::ExpressionKind m;
    getModifiers(attr, m);
    modifiers.push_back(m);
  }

  // Leave SageTreeBuilder for SgDerivedTypeStmt
  builder.Leave(derived, modifiers);
}

void BuildVisitor::Build(parser::DimensionStmt &x) {
  std::cerr << "[WARN] BuildVisitor::Build(DimensionStmt) unimplemented\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::DeclarationTypeSpec::Type &x) {
  // Type DerivedTypeSpec derived;
  //      DerivedTypeSpec std::tuple<Name, std::list<TypeParamSpec>> t;

  std::cout << "Rose::builder::Build(DeclarationTypeSpec::Type)\n";
  std::string name = std::get<parser::Name>(x.derived.t).ToString();
  std::cout << "DerivedTypeSpec name is " << name << "\n";

  ABORT_NO_IMPL;
}

void Build(parser::DeclarationTypeSpec::TypeStar &x, SgType *&type) {
  std::cout << "Rose::builder::Build(TypeStar)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DeclarationTypeSpec::Class &x, SgType *&type) {
  std::cout << "Rose::builder::Build(Class)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DeclarationTypeSpec::ClassStar &x, SgType *&type) {
  std::cout << "Rose::builder::Build(ClassStar)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DeclarationTypeSpec::Record &x, SgType *&type) {
  std::cout << "Rose::builder::Build(Record)\n";
  ABORT_NO_IMPL;
}

void Build(parser::AttrSpec &x, LanguageTranslation::ExpressionKind &modifier) {
  std::cout << "Rose::builder::Build(AttrSpec)\n";
  ABORT_NO_IMPL;
}

void BuildVisitor::Build(parser::IntegerTypeSpec &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};

  if (auto &kind = x.v) { // std::optional<KindSelector>
    WalkExpr(kind.value(), expr);
  }
  type = SageBuilder::buildIntType(expr);
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Real &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};

  if (x.kind) { // std::optional<KindSelector>
    WalkExpr(x.kind.value(), expr);
  }
  type = SageBuilder::buildFloatType(expr);
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::DoublePrecision &x) {
  SgType *type{SageBuilder::buildDoubleType()}; // no KindSelector
  this->set(type);                              // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Complex &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};

  if (x.kind) { // std::optional<KindSelector>
    WalkExpr(x.kind.value(), expr);
  }

  SgType *base = SageBuilder::buildFloatType(expr);
  type = SageBuilder::buildComplexType(base);
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

    type = SageBuilder::buildStringType(len);
    type->set_type_kind(kind);
  } else {
    type = SageBuilder::buildCharType();
  }

  ASSERT_not_null(type);
  this->set(type); // synthesized attribute
}

void BuildVisitor::Build(parser::IntrinsicTypeSpec::Logical &x) {
  SgType *type{nullptr};
  SgExpression *expr{nullptr};

  if (x.kind) { // std::optional<KindSelector>
    WalkExpr(x.kind.value(), expr);
  }
  type = SageBuilder::buildBoolType(expr);
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
                 std::list<EntityDeclTuple> &entityDecls, SgType *baseType) {
  for (auto &entity : x) {
    SgType *type{nullptr};
    SgExpression *init{nullptr};
    std::string name{std::get<0>(entity.t).ToString()};

    if (auto &opt = std::get<1>(entity.t)) { // ArraySpec
      Build(opt.value(), type, baseType);
    }
    if (auto &opt = std::get<2>(entity.t)) { // CoarraySpec
      ABORT_NO_TEST;
      Build(opt.value(), type, baseType);
    }
    if (auto &opt = std::get<3>(entity.t)) { // CharLength
      WalkExpr(opt.value(), init);
    }
    if (auto &opt = std::get<4>(entity.t)) { // Initialization
      WalkExpr(opt.value(), init);
    }
    entityDecls.push_back(std::make_tuple(name, type, init));
  }
}

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
  using namespace Fortran::parser;

  SgExpression *expr{nullptr};
  SgExprListExp *dimInfo{SageBuilder::buildExprListExp_nfi()};

  common::visit(
      common::visitors{
          [&](std::list<ExplicitShapeSpec> &y) {
            for (ExplicitShapeSpec &spec :
                 y) { // is [ lower-bound : ] upper-bound
              WalkExpr(spec, expr = nullptr);
              dimInfo->get_expressions().push_back(expr);
            }
          },
          [&](std::list<AssumedShapeSpec> &y) {
            for (AssumedShapeSpec &spec : y) { // is [ lower-bound ]:
              WalkExpr(spec, expr = nullptr);
              dimInfo->get_expressions().push_back(expr);
            }
          },
          [&](DeferredShapeSpecList &y) {
            for (int ii{0}; ii < y.v; ii++) { // is :
              dimInfo->get_expressions().push_back(
                  SageBuilder::buildColonShapeExp_nfi());
            }
          },
          [&](AssumedSizeSpec &y) {
            // std::tuple<std::list<ExplicitShapeSpec>, AssumedImpliedSpec> t;
            for (ExplicitShapeSpec &spec :
                 std::get<0>(y.t)) { // is [ lower-bound : ] upper-bound
              WalkExpr(spec, expr = nullptr);
              dimInfo->get_expressions().push_back(expr);
            }
            AssumedImpliedSpec &spec{
                std::get<1>(y.t)}; // is [ lower-bound : ] *
            WalkExpr(spec, expr = nullptr);
            dimInfo->get_expressions().push_back(expr);
          },
          [&](ImpliedShapeSpec &y) {
            for (AssumedImpliedSpec &spec : y.v) { // is [ lower-bound : ] *
              WalkExpr(spec, expr = nullptr);
              dimInfo->get_expressions().push_back(expr);
            }
          },
          [&](AssumedRankSpec &y) {
            // is ..
            // TODO: Need new espression type in ROSE
            // dimInfo->get_expressions().push_back(SageBuilder::buildRankShapeExp_nfi());
            ABORT_NO_IMPL;
          }},
      x.u);

  // build the final array type as return value
  type = SageBuilder::buildArrayType(baseType, dimInfo);
}

// CoarraySpec
void Build(parser::CoarraySpec &x, SgType *&type, SgType *baseType) {
  std::cout << "Rose::builder::Build(CoarraySpec)\n";
  ABORT_NO_IMPL;
}

void Build(parser::CharLength &x, SgExpression *&) {
  std::cout << "Rose::builder::Build(CharLength)\n";
  ABORT_NO_IMPL;
}

void Build(parser::Initialization &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(Initialization)\n";
  ABORT_NO_IMPL;
}

void Build(parser::SpecificationExpr &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(SpecificationExpr)\n";
  ABORT_NO_IMPL;

  Build(x.v, expr); // Scalar<IntExpr>
}

void Build(parser::Scalar<parser::IntExpr> &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(Scalar<IntExpr>)");
  ABORT_NO_IMPL;
}

void Build(parser::Scalar<parser::LogicalExpr> &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(Scalar<LogicalExpr>)");
  ABORT_NO_IMPL;
}

void Build(parser::ConstantExpr &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(Scalar<ConstantExpr>)");
  ABORT_NO_IMPL;
}

// DeclarationConstruct

void BuildImpl(parser::FormatStmt &x) {
  // FormatStmt format::FormatSpecification v;
  std::cout << "BuildImpl(FormatStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::EntryStmt &x) {
  // EntryStmt std::tuple<> Name, std::list<DummyArg>, std::optional<Suffix>
  std::cout << "BuildImpl(EntryStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::StmtFunctionStmt &x) {
  std::cout << "BuildImpl(StmtFunctionStmt)\n";
  ABORT_NO_IMPL;
}

void BuildImpl(parser::ErrorRecovery &x) {
  std::cout << "BuildImpl(ErrorRecovery)\n";
  ABORT_NO_IMPL;
}

// DataStmt
void Build(parser::DataStmtValue &x, SgExpression *&expr) {
  // std::tuple<std::optional<DataStmtRepeat>, DataStmtConstant> t;
  std::cout << "Rose::builder::Build(DataStmtValue)\n";
  ABORT_NO_IMPL;
}

void Build(parser::DataStmtConstant &x, SgExpression *&expr) {
  //     std::variant<LiteralConstant, SignedIntLiteralConstant,
  //      SignedRealLiteralConstant, SignedComplexLiteralConstant, NullInit,
  //      common::Indirection<Designator>, StructureConstructor>  u;
  std::cout << "Rose::builder::Build(DataStmtConstant)\n";
  ABORT_NO_IMPL;
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
  std::cout << "BuildVisitor::Build(GotoStmt)\n";

  std::string target;

  target = std::string{"13"};

  SgGotoStatement *stmt{nullptr};
  builder.Enter(stmt, target);
  builder.Leave(stmt, getLabels());

  // need target
  ABORT_NO_IMPL;
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
  info(x, "Rose::builder::Build(DefaultCharExpr)");
  ABORT_NO_IMPL;
}

void Build(parser::Label &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(Label)");
  ABORT_NO_IMPL;
}

void Build(parser::Star &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(Star)");
  ABORT_NO_TEST;

  expr = SageBuilderCpp17::buildAsteriskShapeExp_nfi();
}

void Build(parser::OutputItem &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(OutputItem)");
  ABORT_NO_IMPL;

  expr = nullptr;
}

void Build(parser::OutputImpliedDo &x) {
  std::cout << "Rose::builder::Build(OutputImpliedDo)\n";
  ABORT_NO_IMPL;
}

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
    ABORT_NO_TEST;
  }

  builder.Enter(stmt, std::string{kind}, code, quiet);
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
  std::cout << "BuildImpl(ArithmeticIfStmt)\n";
  ABORT_NO_IMPL;
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
  std::cout << "Rose::builder::Build(CharLiteralConstantSubstring)\n";
  ABORT_NO_IMPL;
}

void Build(parser::SubstringInquiry &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(SubstringInquiry)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ArrayConstructor &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(ArrayConstructor)\n";
  ABORT_NO_IMPL;
}

void Build(parser::AcSpec &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(AcSpec)\n";
  ABORT_NO_IMPL;
}

template <typename T> void Build(parser::StructureConstructor &x, T *&expr) {
  info(x, "Rose::builder::Build(StructureConstructor)");
  ABORT_NO_IMPL;
}

template <typename T> void Build(parser::Expr::Parentheses &x, T *&expr) {
  info(x, "Rose::builder::Build(Parentheses)");
  ABORT_NO_IMPL;
}

template <typename T> void Build(parser::Expr::UnaryPlus &x, T *&expr) {
  info(x, "Rose::builder::Build(UnaryPlus)");
  ABORT_NO_IMPL;
}

template <typename T> void Build(parser::Expr::Negate &x, T *&expr) {
  info(x, "Rose::builder::Build(Negate)");
  ABORT_NO_IMPL;
}

void BuildExprVisitor::Build(parser::Expr::NOT &x /*, SgExpression* &expr*/) {
  info(x, "BuildExprVisitor::::Build(NOT)");
  ABORT_NO_IMPL;
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
  std::cout << "Rose::builder::Build(Power)\n";
  ABORT_NO_IMPL;
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
  std::cout << "Rose::builder::Build(DefinedBinary)\n";
  ABORT_NO_IMPL;
}

void Build(parser::Expr::ComplexConstructor &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(ComplexConstructor)\n";
  ABORT_NO_IMPL;
}

void Build(parser::StructureComponent &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(StructureComponent)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ArrayElement &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(ArrayElement)\n";
  ABORT_NO_IMPL;
}

void Build(parser::CoindexedNamedObject &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(CoindexedNamedObject)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ImageSelector &x, SgExpression *&expr) {
  std::cout << "Rose::builder::Build(ImageSelector)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ImageSelectorSpec &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(ImageSelectorSpec)");
  ABORT_NO_IMPL;
}

void Build(parser::SectionSubscript &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(SectionSubscript)");
  ABORT_NO_IMPL;
}

void Build(parser::SubscriptTriplet &x, SgExpression *&expr) {
  info(x, "Rose::builder::Build(SubscriptTriplet)");
  ABORT_NO_IMPL;
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
  std::cout << "Rose::builder::Build(DataComponentDefStmt)\n";
  ABORT_NO_IMPL;
}

void Build(Fortran::parser::ComponentDecl &x,
           std::list<EntityDeclTuple> &componentDecls, SgType *baseType) {
  std::cout << "Rose::builder::Build(ComponentDecl&)\n";
  ABORT_NO_IMPL;
}

void Build(parser::ComponentDecl &x, std::string &name, SgExpression *&init,
           SgType *&type, SgType *base_type) {
  //  std::tuple<> Name, std::optional<ComponentArraySpec>,
  //  std::optional<CoarraySpec>, std::optional<CharLength>,
  //               std::optional<Initialization>
  std::cout << "Rose::builder::Build(ComponentDecl)\n";
  ABORT_NO_IMPL;
}

void Build(parser::EnumDef &x) {
  std::cout << "Rose::builder::Build(EnumDef)\n";
  ABORT_NO_IMPL;
}

void Build(parser::InterfaceBlock &x) {
  std::cout << "Rose::builder::Build(InterfaceBlock)\n";
  ABORT_NO_IMPL;
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
  std::cout << "Rose::builder::Build(ProcedureDeclarationStmt)\n";
  ABORT_NO_IMPL;
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
void getModifiers(parser::IntentSpec &x,
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
void getModifiers(parser::LanguageBindingSpec &x,
                  LanguageTranslation::ExpressionKind &m) {
  std::cout << "[WARN] getModifiers(LanguageBindingSpec): MAYBE need build of "
               "ScalarDefaultCharConstantExpr\n";
  ABORT_NO_IMPL;
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
          [&](TypeAttrSpec::Extends &y) { ABORT_NO_IMPL; }},
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
          [&](CoarraySpec &) {
            ABORT_NO_IMPL; /*CODIMENSION*/
          },
          [&](ComponentArraySpec &) {
            ABORT_NO_TEST; /*DIMENSION*/
          },
          [&](const IntentSpec &) {
            ABORT_NO_IMPL; /*INTENT*/
          },
          [&](const LanguageBindingSpec &) {
            ABORT_NO_IMPL; /*BINDING*/
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
