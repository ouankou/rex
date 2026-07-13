#include "sage3basic.h"

#include "accAstConstruction.h"
#include "ompAstConstruction.h"

#include <iostream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

extern SgExpression *parseExpression(SgNode *, const char *);

extern void copyStartFileInfo(SgNode *, SgNode *);
extern void copyEndFileInfo(SgNode *, SgNode *);

namespace {

[[noreturn]] void failAccAstInvariant(const char *category,
                                      const std::string &detail) {
  std::cerr << "REX_ACC_AST_INVARIANT[" << category << "]: " << detail << '\n';
  ROSE_ABORT();
}

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

bool isFortranOpenACCPragma(SgPragmaDeclaration *pragma) {
  SgSourceFile *source = pragma != nullptr
                             ? SageInterface::getEnclosingSourceFile(pragma)
                             : nullptr;
  if (source == nullptr) {
    failAccAstInvariant("directive-owner",
                        "OpenACC pragma has no exact source-file owner");
  }
  return source->get_Fortran_only() || source->get_F77_only() ||
         source->get_F90_only() || source->get_F95_only() ||
         source->get_F2003_only();
}

SgAccClausePtrList &requireAccClauseList(SgStatement *directive) {
  if (directive == nullptr) {
    failAccAstInvariant("clause-owner", "null OpenACC directive owner");
  }
  if (SgAccClauseBodyStatement *body = isSgAccClauseBodyStatement(directive)) {
    return body->get_clauses();
  }
  if (SgAccClauseStatement *clauseStatement =
          isSgAccClauseStatement(directive)) {
    return clauseStatement->get_clauses();
  }
  failAccAstInvariant("clause-owner",
                      "OpenACC clause owner has no clause list: " +
                          directive->class_name());
}

void attachAccClause(SgStatement *directive, SgAccClause *clause) {
  if (clause == nullptr || clause->get_parent() != nullptr) {
    failAccAstInvariant("clause-owner",
                        clause == nullptr
                            ? "cannot attach a null OpenACC clause"
                            : "OpenACC clause already has an AST owner");
  }
  requireAccClauseList(directive).push_back(clause);
  clause->set_parent(directive);
}

SgExpression *parseAccExpression(SgPragmaDeclaration *pragma,
                                 const std::string &spelling,
                                 const char *description) {
  if (pragma == nullptr || description == nullptr || spelling.empty()) {
    failAccAstInvariant("expression",
                        "invalid OpenACC host-expression parse request");
  }
  const std::string parserInput = "expr (" + spelling + ")\n";
  const bool useFortranExactSemantics = isFortranOpenACCPragma(pragma);
  if (useFortranExactSemantics) {
    OmpSupport::beginOpenACCFortranExactSemanticExpression(pragma, spelling);
  } else {
    OmpSupport::beginOpenACCCxxExactSemanticExpression(
        pragma, spelling, OMP_EXPR_PARSE_expression);
  }
  SgExpression *expression = parseExpression(pragma, parserInput.c_str());
  if (useFortranExactSemantics) {
    OmpSupport::endOpenACCFortranExactSemanticExpression(pragma);
  } else {
    OmpSupport::endOpenACCCxxExactSemanticExpression(pragma);
  }
  if (expression == nullptr) {
    failAccAstInvariant("expression",
                        std::string("base-language parser produced no ") +
                            description + " for '" + spelling + "'");
  }
  if (expression->get_parent() != nullptr) {
    failAccAstInvariant("expression-owner", std::string("parsed ") +
                                                description +
                                                " already has an AST owner");
  }
  return expression;
}

SgExpression *buildAccVariableExpression(SgNode *node) {
  if (SgInitializedName *name = isSgInitializedName(node)) {
    return SageBuilder::buildVarRefExp(name);
  }
  return isSgExpression(node);
}

SgExprListExp *
parseAccVariables(SgPragmaDeclaration *pragma,
                  const openacc::NonEmptyList<openacc::VariableRef> &variables,
                  const char *description) {
  if (pragma == nullptr || description == nullptr) {
    failAccAstInvariant("variable-list",
                        "invalid OpenACC variable-list parse request");
  }
  if (!OmpSupport::openMPExpressionVariables().empty()) {
    failAccAstInvariant("temporary-variable-state",
                        "OpenACC variable-list construction started with "
                        "stale expression-parser state");
  }

  for (const openacc::VariableRef &variable : variables.values()) {
    const std::size_t oldSize = OmpSupport::openMPExpressionVariables().size();
    const std::string parserInput = "varlist " + variable.spelling() + "\n";
    const bool useFortranExactSemantics = isFortranOpenACCPragma(pragma);
    if (useFortranExactSemantics) {
      OmpSupport::beginOpenACCFortranExactSemanticExpression(
          pragma, variable.spelling());
    } else {
      OmpSupport::beginOpenACCCxxExactSemanticExpression(
          pragma, variable.spelling(), OMP_EXPR_PARSE_variable_list);
    }
    parseExpression(pragma, parserInput.c_str());
    if (useFortranExactSemantics) {
      OmpSupport::endOpenACCFortranExactSemanticExpression(pragma);
    } else {
      OmpSupport::endOpenACCCxxExactSemanticExpression(pragma);
    }
    if (OmpSupport::openMPExpressionVariables().size() != oldSize + 1) {
      failAccAstInvariant(
          "variable-list-cardinality",
          std::string("base-language parser did not produce exactly one ") +
              description + " for '" + variable.spelling() + "'");
    }
  }

  if (OmpSupport::openMPExpressionVariables().size() != variables.size()) {
    failAccAstInvariant("variable-list-cardinality",
                        "typed OpenACC variables and semantic AST nodes have "
                        "different cardinalities");
  }

  // Transfer the shared expression parser's session-owned result immediately.
  // Array sections are exact typed expression trees; the removed legacy
  // symbol-keyed dimension side table must not be reconstructed here.
  std::vector<SgNode *> parsedNodes;
  parsedNodes.swap(OmpSupport::openMPExpressionVariables());
  if (!OmpSupport::openMPExpressionVariables().empty()) {
    failAccAstInvariant(
        "temporary-variable-state",
        "OpenACC variable parser scratch state was not transferred locally");
  }

  SgExprListExp *result = SageBuilder::buildExprListExp();
  if (result == nullptr || result->get_parent() != nullptr) {
    failAccAstInvariant("variable-list-owner",
                        "cannot create an unowned OpenACC variable list");
  }
  for (SgNode *node : parsedNodes) {
    SgExpression *expression = buildAccVariableExpression(node);
    if (expression == nullptr) {
      failAccAstInvariant(
          "variable-list-node",
          "unsupported base-language node in an OpenACC variable list: " +
              (node != nullptr ? node->class_name() : std::string("null")));
    }
    if (expression->get_parent() != nullptr) {
      failAccAstInvariant("variable-list-owner",
                          "parsed OpenACC variable already has an AST owner");
    }
    if (SgFortranCommonBlockRefExp *common =
            isSgFortranCommonBlockRefExp(expression)) {
      SageInterface::validateFortranCommonBlockRef(common);
    }
    result->get_expressions().push_back(expression);
    expression->set_parent(result);
  }

  if (!OmpSupport::openMPExpressionVariables().empty()) {
    failAccAstInvariant(
        "temporary-variable-state",
        "OpenACC variable conversion leaked shared expression-parser state");
  }
  return result;
}

template <typename Fragment>
SgExprListExp *
parseAccExpressionList(SgPragmaDeclaration *pragma,
                       const openacc::NonEmptyList<Fragment> &expressions,
                       const char *description) {
  SgExprListExp *result = SageBuilder::buildExprListExp();
  if (result == nullptr || result->get_parent() != nullptr) {
    failAccAstInvariant("expression-list-owner",
                        "cannot create an unowned OpenACC expression list");
  }
  for (const Fragment &fragment : expressions.values()) {
    SgExpression *expression =
        parseAccExpression(pragma, fragment.spelling(), description);
    result->get_expressions().push_back(expression);
    expression->set_parent(result);
  }
  return result;
}

void validateFlagClause(const openacc::FlagClause &clause) {
  using Kind = openacc::FlagClauseKind;
  switch (clause.kind) {
  case Kind::Capture:
  case Kind::Read:
  case Kind::Seq:
  case Kind::Update:
  case Kind::Write:
    return;
  case Kind::Auto:
  case Kind::Finalize:
  case Kind::IfPresent:
  case Kind::Independent:
  case Kind::NoHost:
    failAccAstInvariant(
        "unsupported-clause",
        "typed OpenACC flag clause has no current SgAcc representation");
  }
  failAccAstInvariant("unsupported-clause-kind",
                      "invalid typed OpenACC flag-clause kind");
}

void validateVarListClause(const openacc::VarListClause &clause) {
  using Kind = openacc::VarListClauseKind;
  switch (clause.kind) {
  case Kind::Delete:
  case Kind::DevicePtr:
  case Kind::Present:
  case Kind::Private:
    return;
  case Kind::Attach:
  case Kind::Detach:
  case Kind::Device:
  case Kind::DeviceResident:
  case Kind::FirstPrivate:
  case Kind::Host:
  case Kind::Link:
  case Kind::NoCreate:
  case Kind::Self:
  case Kind::UseDevice:
    failAccAstInvariant(
        "unsupported-clause",
        "typed OpenACC variable-list clause has no current SgAcc "
        "representation");
  }
  failAccAstInvariant("unsupported-clause-kind",
                      "invalid typed OpenACC variable-list clause kind");
}

void validateClauseForSage(const openacc::Clause &clause) {
  std::visit(
      Overloaded{
          [](const openacc::FlagClause &value) { validateFlagClause(value); },
          [](const openacc::AsyncClause &) {},
          [](const openacc::CollapseClause &value) {
            if (value.force) {
              failAccAstInvariant(
                  "unsupported-collapse-force",
                  "SgAccCollapseClause cannot represent the force modifier");
            }
          },
          [](const openacc::CopyClause &value) {
            if (!value.modifiers.empty()) {
              failAccAstInvariant(
                  "unsupported-data-modifier",
                  "SgAccCopyClause cannot represent typed copy modifiers");
            }
          },
          [](const openacc::CopyInClause &value) {
            if (!value.modifiers.empty()) {
              failAccAstInvariant(
                  "unsupported-data-modifier",
                  "SgAccCopyinClause cannot represent typed copyin "
                  "modifiers");
            }
          },
          [](const openacc::CopyOutClause &value) {
            if (!value.modifiers.empty()) {
              failAccAstInvariant(
                  "unsupported-data-modifier",
                  "SgAccCopyoutClause cannot represent typed copyout "
                  "modifiers");
            }
          },
          [](const openacc::CreateClause &value) {
            if (!value.modifiers.empty()) {
              failAccAstInvariant(
                  "unsupported-data-modifier",
                  "SgAccCreateClause cannot represent typed create "
                  "modifiers");
            }
          },
          [](const openacc::DefaultClause &) {},
          [](const openacc::GangClause &value) {
            if (!value.arguments.empty()) {
              failAccAstInvariant(
                  "unsupported-gang-argument",
                  "SgAccGangClause cannot represent typed gang arguments");
            }
          },
          [](const openacc::IfClause &) {},
          [](const openacc::NumGangsClause &value) {
            if (value.values.size() != 1) {
              failAccAstInvariant(
                  "unsupported-num-gangs-rank",
                  "SgAccNumGangsClause represents exactly one expression");
            }
          },
          [](const openacc::NumWorkersClause &) {},
          [](const openacc::ReductionClause &) {},
          [](const openacc::VarListClause &value) {
            validateVarListClause(value);
          },
          [](const openacc::VectorClause &) {},
          [](const openacc::VectorLengthClause &) {},
          [](const auto &) {
            failAccAstInvariant(
                "unsupported-clause",
                "typed OpenACC clause payload has no current SgAcc "
                "representation");
          },
      },
      clause);
}

void validateGeneralDirectiveForSage(
    const openacc::GeneralDirective &directive) {
  using Kind = openacc::DirectiveKind;
  switch (directive.kind) {
  case Kind::Atomic:
  case Kind::Data:
  case Kind::EnterData:
  case Kind::ExitData:
  case Kind::Kernels:
  case Kind::Parallel:
  case Kind::ParallelLoop:
  case Kind::Routine:
  case Kind::Wait:
    break;
  case Kind::Cache:
  case Kind::End:
    failAccAstInvariant("directive-payload",
                        "general OpenACC payload has a non-general kind");
  case Kind::Declare:
  case Kind::HostData:
  case Kind::Init:
  case Kind::KernelsLoop:
  case Kind::Loop:
  case Kind::Serial:
  case Kind::SerialLoop:
  case Kind::Set:
  case Kind::Shutdown:
  case Kind::Update:
    failAccAstInvariant(
        "unsupported-directive",
        "typed OpenACC directive has no current SgAcc representation");
  }

  if (!directive.deviceGroups.empty()) {
    failAccAstInvariant(
        "unsupported-device-group",
        "current SgAcc clauses cannot preserve device_type clause groups");
  }
  for (const openacc::Clause &clause : directive.defaultClauses) {
    validateClauseForSage(clause);
  }
}

void validateEndDirectiveForSage(const openacc::EndDirective &directive) {
  switch (directive.kind) {
  case openacc::EndDirectiveKind::Atomic:
  case openacc::EndDirectiveKind::Data:
  case openacc::EndDirectiveKind::Kernels:
  case openacc::EndDirectiveKind::Parallel:
  case openacc::EndDirectiveKind::ParallelLoop:
    return;
  case openacc::EndDirectiveKind::HostData:
  case openacc::EndDirectiveKind::KernelsLoop:
  case openacc::EndDirectiveKind::Loop:
  case openacc::EndDirectiveKind::Serial:
  case openacc::EndDirectiveKind::SerialLoop:
    failAccAstInvariant(
        "unsupported-end-directive",
        "typed OpenACC end marker has no supported SgAcc begin construct");
  }
  failAccAstInvariant("unsupported-end-directive-kind",
                      "invalid typed OpenACC end-directive kind");
}

void validateDirectiveForSageImpl(const openacc::Directive &directive) {
  std::visit(Overloaded{
                 [](const openacc::GeneralDirective &value) {
                   validateGeneralDirectiveForSage(value);
                 },
                 [](const openacc::CacheDirective &) {},
                 [](const openacc::EndDirective &value) {
                   validateEndDirectiveForSage(value);
                 },
             },
             directive.payload());
}

SgAccExpressionClause *buildExpressionClause(SgPragmaDeclaration *pragma,
                                             const openacc::Clause &clause) {
  return std::visit(
      Overloaded{
          [pragma](
              const openacc::CollapseClause &value) -> SgAccExpressionClause * {
            return new SgAccCollapseClause(parseAccExpression(
                pragma, value.count.spelling(), "collapse count"));
          },
          [pragma](
              const openacc::NumGangsClause &value) -> SgAccExpressionClause * {
            return new SgAccNumGangsClause(parseAccExpression(
                pragma, value.values.values().front().spelling(),
                "num_gangs expression"));
          },
          [pragma](const openacc::NumWorkersClause &value)
              -> SgAccExpressionClause * {
            return new SgAccNumWorkersClause(parseAccExpression(
                pragma, value.value.spelling(), "num_workers expression"));
          },
          [pragma](const openacc::VectorLengthClause &value)
              -> SgAccExpressionClause * {
            return new SgAccVectorLengthClause(parseAccExpression(
                pragma, value.value.spelling(), "vector_length expression"));
          },
          [pragma](
              const openacc::AsyncClause &value) -> SgAccExpressionClause * {
            return new SgAccAsyncClause(
                value.argument
                    ? parseAccExpression(pragma, value.argument->spelling(),
                                         "async expression")
                    : nullptr);
          },
          [pragma](const openacc::IfClause &value) -> SgAccExpressionClause * {
            return new SgAccIfClause(parseAccExpression(
                pragma, value.condition.spelling(), "if condition"));
          },
          [pragma](
              const openacc::VectorClause &value) -> SgAccExpressionClause * {
            return new SgAccVectorClause(
                value.argument ? parseAccExpression(
                                     pragma, value.argument->value.spelling(),
                                     "vector expression")
                               : nullptr);
          },
          [](const auto &) -> SgAccExpressionClause * {
            failAccAstInvariant("clause-dispatch",
                                "non-expression OpenACC clause reached the "
                                "expression-clause builder");
          },
      },
      clause);
}

SgAccVariablesClause *buildVariablesClause(SgPragmaDeclaration *pragma,
                                           const openacc::Clause &clause) {
  return std::visit(
      Overloaded{
          [pragma](const openacc::CopyClause &value) -> SgAccVariablesClause * {
            return new SgAccCopyClause(
                parseAccVariables(pragma, value.variables, "copy variable"));
          },
          [pragma](
              const openacc::CopyInClause &value) -> SgAccVariablesClause * {
            return new SgAccCopyinClause(
                parseAccVariables(pragma, value.variables, "copyin variable"));
          },
          [pragma](
              const openacc::CopyOutClause &value) -> SgAccVariablesClause * {
            return new SgAccCopyoutClause(
                parseAccVariables(pragma, value.variables, "copyout variable"));
          },
          [pragma](
              const openacc::CreateClause &value) -> SgAccVariablesClause * {
            return new SgAccCreateClause(
                parseAccVariables(pragma, value.variables, "create variable"));
          },
          [pragma](
              const openacc::ReductionClause &value) -> SgAccVariablesClause * {
            return new SgAccReductionClause(
                parseAccVariables(pragma, value.variables,
                                  "reduction variable"),
                static_cast<int>(value.op));
          },
          [pragma](
              const openacc::VarListClause &value) -> SgAccVariablesClause * {
            SgExprListExp *variables = parseAccVariables(
                pragma, value.variables, "variable-list item");
            switch (value.kind) {
            case openacc::VarListClauseKind::Delete:
              return new SgAccDeleteClause(variables);
            case openacc::VarListClauseKind::DevicePtr:
              return new SgAccDeviceptrClause(variables);
            case openacc::VarListClauseKind::Present:
              return new SgAccPresentClause(variables);
            case openacc::VarListClauseKind::Private:
              return new SgAccPrivateClause(variables);
            default:
              failAccAstInvariant(
                  "clause-dispatch",
                  "unsupported variable-list kind reached SgAcc conversion");
            }
          },
          [](const auto &) -> SgAccVariablesClause * {
            failAccAstInvariant("clause-dispatch",
                                "non-variable OpenACC clause reached the "
                                "variables-clause builder");
          },
      },
      clause);
}

SgAccClause *buildSimpleClause(const openacc::Clause &clause) {
  return std::visit(
      Overloaded{
          [](const openacc::DefaultClause &value) -> SgAccClause * {
            return new SgAccDefaultClause(static_cast<int>(value.value));
          },
          [](const openacc::GangClause &) -> SgAccClause * {
            return new SgAccGangClause();
          },
          [](const openacc::FlagClause &value) -> SgAccClause * {
            switch (value.kind) {
            case openacc::FlagClauseKind::Capture:
              return new SgAccCaptureClause();
            case openacc::FlagClauseKind::Read:
              return new SgAccReadClause();
            case openacc::FlagClauseKind::Seq:
              return new SgAccSeqClause();
            case openacc::FlagClauseKind::Update:
              return new SgAccUpdateClause();
            case openacc::FlagClauseKind::Write:
              return new SgAccWriteClause();
            default:
              failAccAstInvariant(
                  "clause-dispatch",
                  "unsupported flag kind reached SgAcc conversion");
            }
          },
          [](const auto &) -> SgAccClause * {
            failAccAstInvariant("clause-dispatch",
                                "non-simple OpenACC clause reached the "
                                "simple-clause builder");
          },
      },
      clause);
}

void finalizeAccClausePayloadOwnership(SgAccClause *clause) {
  if (clause == nullptr || clause->get_parent() != nullptr) {
    failAccAstInvariant(
        "clause-payload-owner",
        "OpenACC clause payload finalization requires an unowned clause");
  }

  if (SgAccExpressionClause *expressionClause =
          isSgAccExpressionClause(clause)) {
    SgExpression *expression = expressionClause->get_expression();
    const bool expressionIsOptional = isSgAccAsyncClause(clause) != nullptr ||
                                      isSgAccVectorClause(clause) != nullptr;
    if (expression == nullptr) {
      if (!expressionIsOptional) {
        failAccAstInvariant("required-expression",
                            clause->class_name() +
                                " has no required host expression");
      }
    } else {
      if (expression->get_parent() != nullptr) {
        failAccAstInvariant("expression-owner",
                            clause->class_name() +
                                " expression already has an AST owner");
      }
      expression->set_parent(clause);
    }
  }

  if (SgAccVariablesClause *variablesClause = isSgAccVariablesClause(clause)) {
    SgExprListExp *variables = variablesClause->get_variables();
    if (variables == nullptr || variables->get_parent() != nullptr ||
        variables->get_expressions().empty()) {
      failAccAstInvariant(
          "required-variable-list",
          clause->class_name() +
              " requires one unowned non-empty variable-list wrapper");
    }
    for (SgExpression *variable : variables->get_expressions()) {
      if (variable == nullptr || variable->get_parent() != variables) {
        failAccAstInvariant(
            "variable-list-owner",
            clause->class_name() +
                " contains a null variable or one with the wrong list owner");
      }
    }
    variables->set_parent(clause);
  }
}

void convertClauses(SgStatement *result, SgPragmaDeclaration *pragma,
                    const std::vector<openacc::Clause> &clauses) {
  for (const openacc::Clause &clause : clauses) {
    SgAccClause *converted = std::visit(
        Overloaded{
            [pragma, &clause](const openacc::AsyncClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma,
             &clause](const openacc::CollapseClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma, &clause](const openacc::IfClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma,
             &clause](const openacc::NumGangsClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma,
             &clause](const openacc::NumWorkersClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma, &clause](const openacc::VectorClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma,
             &clause](const openacc::VectorLengthClause &) -> SgAccClause * {
              return buildExpressionClause(pragma, clause);
            },
            [pragma, &clause](const openacc::CopyClause &) -> SgAccClause * {
              return buildVariablesClause(pragma, clause);
            },
            [pragma, &clause](const openacc::CopyInClause &) -> SgAccClause * {
              return buildVariablesClause(pragma, clause);
            },
            [pragma, &clause](const openacc::CopyOutClause &) -> SgAccClause * {
              return buildVariablesClause(pragma, clause);
            },
            [pragma, &clause](const openacc::CreateClause &) -> SgAccClause * {
              return buildVariablesClause(pragma, clause);
            },
            [pragma,
             &clause](const openacc::ReductionClause &) -> SgAccClause * {
              return buildVariablesClause(pragma, clause);
            },
            [pragma, &clause](const openacc::VarListClause &) -> SgAccClause * {
              return buildVariablesClause(pragma, clause);
            },
            [&clause](const openacc::DefaultClause &) -> SgAccClause * {
              return buildSimpleClause(clause);
            },
            [&clause](const openacc::FlagClause &) -> SgAccClause * {
              return buildSimpleClause(clause);
            },
            [&clause](const openacc::GangClause &) -> SgAccClause * {
              return buildSimpleClause(clause);
            },
            [](const auto &) -> SgAccClause * {
              failAccAstInvariant(
                  "clause-dispatch",
                  "unsupported typed clause reached SgAcc conversion");
            },
        },
        clause);
    if (converted == nullptr) {
      failAccAstInvariant("clause-dispatch",
                          "SgAcc clause conversion produced no node");
    }
    finalizeAccClausePayloadOwnership(converted);
    SageInterface::setOneSourcePositionForTransformation(converted);
    attachAccClause(result, converted);
  }
}

SgAccClauseBodyStatement *
buildBodyDirective(SgPragmaDeclaration *pragma,
                   const openacc::GeneralDirective &directive) {
  SgStatement *body = SageInterface::getNextStatement(pragma);
  if (body == nullptr) {
    failAccAstInvariant("directive-body",
                        "OpenACC structured directive has no following body");
  }

  SgAccClauseBodyStatement *result = nullptr;
  switch (directive.kind) {
  case openacc::DirectiveKind::Atomic:
    result = new SgAccAtomicStatement(nullptr, body);
    break;
  case openacc::DirectiveKind::Data:
    result = new SgAccDataStatement(nullptr, body);
    break;
  case openacc::DirectiveKind::Kernels:
    result = new SgAccKernelsStatement(nullptr, body);
    break;
  case openacc::DirectiveKind::Parallel:
    result = new SgAccParallelStatement(nullptr, body);
    break;
  case openacc::DirectiveKind::ParallelLoop:
    result = new SgAccParallelLoopStatement(nullptr, body);
    break;
  default:
    failAccAstInvariant("directive-dispatch",
                        "non-structured OpenACC directive reached the body "
                        "directive builder");
  }
  if (result == nullptr) {
    failAccAstInvariant("directive-dispatch",
                        "structured OpenACC conversion produced no node");
  }

  SageInterface::removeStatement(body, false);
  body->set_parent(result);
  convertClauses(result, pragma, directive.defaultClauses);
  return result;
}

SgAccClauseStatement *
buildClauseDirective(SgPragmaDeclaration *pragma,
                     const openacc::GeneralDirective &directive) {
  SgAccClauseStatement *result = nullptr;
  switch (directive.kind) {
  case openacc::DirectiveKind::EnterData:
    result = new SgAccEnterDataStatement();
    break;
  case openacc::DirectiveKind::ExitData:
    result = new SgAccExitDataStatement();
    break;
  case openacc::DirectiveKind::Routine:
    if (directive.routineName && isFortranOpenACCPragma(pragma)) {
      OmpSupport::consumeOpenACCFortranExactSemanticSyntax(
          pragma, directive.routineName->spelling());
    }
    result = new SgAccRoutineStatement(
        directive.routineName ? SgName(directive.routineName->spelling())
                              : SgName());
    break;
  case openacc::DirectiveKind::Wait: {
    SgExprListExp *queues = nullptr;
    SgExpression *deviceNumber = nullptr;
    bool hasQueuesKeyword = false;
    if (directive.waitArgument) {
      const openacc::WaitArgument &argument = *directive.waitArgument;
      if (argument.deviceNumber) {
        deviceNumber = parseAccExpression(
            pragma, argument.deviceNumber->spelling(), "wait device number");
      }
      if (argument.queues) {
        queues = parseAccExpressionList(pragma, *argument.queues,
                                        "wait queue expression");
      }
      hasQueuesKeyword = argument.hasQueuesKeyword;
    }
    result = new SgAccWaitStatement(queues, deviceNumber, hasQueuesKeyword);
    if (queues != nullptr) {
      queues->set_parent(result);
    }
    if (deviceNumber != nullptr) {
      deviceNumber->set_parent(result);
    }
    break;
  }
  default:
    failAccAstInvariant("directive-dispatch",
                        "non-clause OpenACC directive reached the clause "
                        "directive builder");
  }
  if (result == nullptr) {
    failAccAstInvariant("directive-dispatch",
                        "OpenACC clause directive conversion produced no "
                        "node");
  }
  convertClauses(result, pragma, directive.defaultClauses);
  return result;
}

SgStatement *buildGeneralDirective(SgPragmaDeclaration *pragma,
                                   const openacc::GeneralDirective &directive) {
  switch (directive.kind) {
  case openacc::DirectiveKind::Atomic:
  case openacc::DirectiveKind::Data:
  case openacc::DirectiveKind::Kernels:
  case openacc::DirectiveKind::Parallel:
  case openacc::DirectiveKind::ParallelLoop:
    return buildBodyDirective(pragma, directive);
  case openacc::DirectiveKind::EnterData:
  case openacc::DirectiveKind::ExitData:
  case openacc::DirectiveKind::Routine:
  case openacc::DirectiveKind::Wait:
    return buildClauseDirective(pragma, directive);
  default:
    failAccAstInvariant("directive-dispatch",
                        "unsupported general directive reached SgAcc "
                        "conversion");
  }
}

SgStatement *buildCacheDirective(SgPragmaDeclaration *pragma,
                                 const openacc::CacheDirective &directive) {
  SgExprListExp *variables =
      parseAccVariables(pragma, directive.variables, "cache variable");
  if (variables == nullptr || variables->get_parent() != nullptr ||
      variables->get_expressions().empty()) {
    failAccAstInvariant(
        "cache-variable-list",
        "cache requires one unowned non-empty variable-list wrapper");
  }
  SgAccCacheStatement *result =
      new SgAccCacheStatement(variables, directive.readOnly ? 1 : 0);
  if (result == nullptr) {
    failAccAstInvariant("cache", "cache conversion produced no statement");
  }
  variables->set_parent(result);
  return result;
}

void finalizeConvertedDirective(SgPragmaDeclaration *pragma,
                                SgStatement *result) {
  if (pragma == nullptr || result == nullptr) {
    failAccAstInvariant("directive-owner",
                        "cannot finalize a null OpenACC pragma or statement");
  }
  if (pragma->get_scope() == nullptr) {
    failAccAstInvariant("directive-owner",
                        "OpenACC pragma has no lexical scope");
  }

  SageInterface::setOneSourcePositionForTransformation(result);
  copyStartFileInfo(pragma, result);
  copyEndFileInfo(pragma, result);

  switch (pragma->get_directive_end_kind()) {
  case SgStatement::e_directive_end_not_applicable:
  case SgStatement::e_directive_end_implicit:
  case SgStatement::e_directive_end_explicit:
    result->set_directive_end_kind(pragma->get_directive_end_kind());
    break;
  default:
    failAccAstInvariant("directive-end-kind",
                        "pragma has invalid typed source END provenance");
  }

  // Establish the exact structural and physical-output owner before moving
  // preprocessing records. Detached converted directives are not valid
  // preprocessing destinations.
  SageInterface::replaceStatement(pragma, result);
  SageInterface::movePreprocessingInfo(pragma, result);
}

} // namespace

void validateOpenACCDirectiveForSage(const openacc::Directive &directive) {
  validateDirectiveForSageImpl(directive);
}

SgStatement *convertOpenACCDirective(SgPragmaDeclaration *pragma,
                                     const openacc::Directive &directive) {
  if (pragma == nullptr) {
    failAccAstInvariant("directive-owner",
                        "typed OpenACC directive has no pragma owner");
  }

  const bool useFortranExactSemantics = isFortranOpenACCPragma(pragma);
  if (useFortranExactSemantics) {
    OmpSupport::beginOpenACCFortranExactSemanticConsumption(pragma);
  } else {
    OmpSupport::beginOpenACCCxxExactSemanticConsumption(pragma);
  }

  // This complete semantic validation must run before body removal, clause
  // construction, or any other AST mutation.
  validateOpenACCDirectiveForSage(directive);

  SgStatement *result = std::visit(
      Overloaded{
          [pragma](const openacc::GeneralDirective &value) {
            return buildGeneralDirective(pragma, value);
          },
          [pragma](const openacc::CacheDirective &value) {
            return buildCacheDirective(pragma, value);
          },
          [](const openacc::EndDirective &) -> SgStatement * {
            failAccAstInvariant("end-conversion",
                                "OpenACC end marker reached AST conversion");
          },
      },
      directive.payload());
  if (result == nullptr) {
    failAccAstInvariant("directive-dispatch",
                        "typed OpenACC conversion produced no statement");
  }
  if (useFortranExactSemantics) {
    OmpSupport::finishOpenACCFortranExactSemanticConsumption(pragma);
  } else {
    OmpSupport::finishOpenACCCxxExactSemanticConsumption(pragma);
  }
  finalizeConvertedDirective(pragma, result);
  return result;
}
