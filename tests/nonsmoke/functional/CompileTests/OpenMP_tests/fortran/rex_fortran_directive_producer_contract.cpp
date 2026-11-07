#include "ompAstConstruction.h"
#include "rose.h"
#include "sageAstJsonPrivate.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
[[noreturn]] void failReturnedOperation(const std::string &message) {
  std::cerr << message << '\n';
  std::_Exit(1);
}

using Producer = OmpFortranExactSemanticBindings::Producer;
using Rose::AstJson::AstFileRecord;
using Rose::AstJson::JsonValue;
using Rose::AstJson::NodeMap;

std::string takeMode(int &argc, char **argv) {
  constexpr const char prefix[] = "--producer-contract=";
  std::string mode;
  int retained = 1;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument.rfind(prefix, 0) == 0) {
      ROSE_ASSERT(mode.empty());
      mode = argument.substr(sizeof(prefix) - 1);
    } else {
      argv[retained++] = argv[i];
    }
  }
  argc = retained;
  argv[argc] = nullptr;
  ROSE_ASSERT(!mode.empty());
  return mode;
}

struct Fixture {
  explicit Fixture(SgProject *project, bool openmp) {
    ROSE_ASSERT(project != nullptr && project->numberOfFiles() == 1);
    file = isSgSourceFile(&project->get_file(0));
    ROSE_ASSERT(file != nullptr && file->get_Fortran_only());
    for (SgNode *node : NodeQuery::querySubTree(file, V_SgInitializedName)) {
      auto *name = isSgInitializedName(node);
      if (name->get_name() == "rex_queue") {
        ROSE_ASSERT(variable == nullptr);
        variable = name;
      }
    }
    ROSE_ASSERT(variable != nullptr);
    scope = isSgBasicBlock(variable->get_scope());
    ROSE_ASSERT(scope != nullptr);
    symbol = isSgVariableSymbol(variable->get_symbol_from_symbol_table());
    ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() == variable);
    const auto pragmas = NodeQuery::querySubTree(file, V_SgPragmaDeclaration);
    ROSE_ASSERT(pragmas.size() == (openmp ? 2 : 1));
    primary = isSgPragmaDeclaration(pragmas.front());
    if (openmp)
      ending = isSgPragmaDeclaration(pragmas.back());
    ROSE_ASSERT(primary != nullptr && primary->get_scope() == scope);
    ROSE_ASSERT(primary->get_fortran_directive_semantic_text() ==
                (openmp ? "omp parallel num_threads(rex_queue + 1)"
                        : "acc wait(rex_queue + 1)"));
    if (openmp)
      ROSE_ASSERT(ending != nullptr &&
                  ending->get_fortran_directive_semantic_text() ==
                      "omp end parallel");
    OmpSupport::discardOpenMPProducerSemanticRecords(file);
    file->set_openmp_processed(false);
    file->set_openmp_parse_only(false);
    file->set_openacc_parse_only(false);
    file->set_openmp_ast_only(openmp);
    file->set_openacc_ast_only(!openmp);
  }

  OmpFortranExactSemanticBindings record(SgPragmaDeclaration *pragma,
                                         Producer producer,
                                         bool expression = false) {
    const std::string text = pragma->get_fortran_directive_semantic_text();
    std::vector<OmpFortranExactSemanticBindings::Binding> bindings;
    std::vector<OmpFortranExactSemanticBindings::ExpressionTypes> expressions;
    if (expression) {
      const std::string spelling = "rex_queue";
      const std::string operand = spelling + " + 1";
      const auto offset = text.find(operand);
      ROSE_ASSERT(offset != std::string::npos);
      bindings.emplace_back(offset, spelling.size(), spelling, spelling,
                            OmpFortranExactSemanticBindings::BindingKind::value,
                            symbol, symbol, nullptr);
      expressions.emplace_back(
          offset, operand.size(), operand,
          std::vector<OmpExactSubexpressionType>{
              {OmpExactSubexpressionKind::add, variable->get_type()}});
    }
    return OmpFortranExactSemanticBindings(producer, text, variable->get_type(),
                                           std::move(bindings),
                                           std::move(expressions));
  }

  SgSourceFile *file = nullptr;
  SgBasicBlock *scope = nullptr;
  SgInitializedName *variable = nullptr;
  SgVariableSymbol *symbol = nullptr;
  SgPragmaDeclaration *primary = nullptr;
  SgPragmaDeclaration *ending = nullptr;
};

struct Snapshot {
  explicit Snapshot(SgSourceFile *file) {
    constexpr auto checkpoint = Rose::AstJson::Checkpoint::PreOmpConstruction;
    ast = Rose::AstJson::parseAstFileJson(
        Rose::AstJson::buildJson(file, checkpoint, file, nullptr, &nodes),
        Rose::AstJson::checkpointName(checkpoint));
    for (const auto &entry : nodes)
      ids.emplace(entry.second, entry.first);
  }

  JsonValue &fortran(SgPragmaDeclaration *pragma) {
    return ast.nodes.at(ast.index_by_id.at(ids.at(pragma)))
        .properties.object.at("openmp_producer_semantic_records")
        .object.at("fortran_exact_semantic_bindings");
  }

  AstFileRecord ast;
  NodeMap nodes;
  std::unordered_map<const SgNode *, uint64_t> ids;
};

void expectJsonFailure(const AstFileRecord &ast, SgSourceFile *file,
                       const std::string &expected) {
  try {
    static_cast<void>(Rose::AstJson::reconstructSourceFile(ast, file));
  } catch (const std::runtime_error &error) {
    if (std::string(error.what()) == expected)
      return;
    throw;
  }
  throw std::runtime_error("invalid Fortran producer JSON was accepted: " +
                           expected);
}

void requireRoundTrip(Fixture &fixture, SgPragmaDeclaration *pragma,
                      bool openmp) {
  Snapshot original(fixture.file);
  NodeMap restored;
  auto *copy = Rose::AstJson::reconstructSourceFile(original.ast, fixture.file,
                                                    &restored);
  auto *copied_pragma = Rose::AstJson::nodeByIdAs<SgPragmaDeclaration>(
      restored, original.ids.at(pragma));
  const auto record =
      OmpSupport::snapshotOpenMPProducerSemanticRecords(copied_pragma);
  ROSE_ASSERT(record.fortran_exact_semantic_bindings.has_value());
  const auto &fortran = *record.fortran_exact_semantic_bindings;
  ROSE_ASSERT(fortran.producer() == (openmp ? Producer::rex_typed_scope
                                            : Producer::flang_parse_tree));
  if (openmp) {
    ROSE_ASSERT(fortran.bindings().empty() && fortran.expressions().empty());
  } else {
    ROSE_ASSERT(fortran.bindings().size() == 1 &&
                fortran.expressions().size() == 1);
    auto *expected = Rose::AstJson::nodeByIdAs<SgInitializedName>(
        restored, original.ids.at(fixture.variable));
    ROSE_ASSERT(fortran.bindings().front().symbol()->get_symbol_basis() ==
                expected);
  }
  Snapshot serialized_again(copy);
  ROSE_ASSERT(original.fortran(pragma).exactlyEquals(
      serialized_again.fortran(copied_pragma)));

  // Exercise conversion after restoration: the OpenMP record intentionally
  // owns no Flang expressions, so the clause must resolve in the copied Sage
  // scope. OpenACC consumes its restored exact Flang binding instead.
  copy->set_openmp_processed(false);
  copy->set_openmp(openmp);
  copy->set_openmp_parse_only(false);
  copy->set_openmp_ast_only(openmp);
  copy->set_openacc(!openmp);
  copy->set_openacc_parse_only(false);
  copy->set_openacc_ast_only(!openmp);
  OmpSupport::processOpenMP(copy);
  auto *expected = Rose::AstJson::nodeByIdAs<SgInitializedName>(
      restored, original.ids.at(fixture.variable));
  const auto directives = NodeQuery::querySubTree(
      copy, openmp ? V_SgOmpParallelStatement : V_SgAccWaitStatement);
  ROSE_ASSERT(directives.size() == 1);
  const auto references =
      NodeQuery::querySubTree(directives.front(), V_SgVarRefExp);
  ROSE_ASSERT(references.size() == 1);
  auto *reference = isSgVarRefExp(references.front());
  ROSE_ASSERT(reference != nullptr &&
              reference->get_symbol()->get_declaration() == expected);
  ROSE_ASSERT(NodeQuery::querySubTree(directives.front(), V_SgAddOp).size() ==
              1);
  OmpSupport::discardOpenMPProducerSemanticRecords(fixture.file);
}
} // namespace

int main(int argc, char **argv) {
  static_assert(static_cast<int>(Producer::flang_parse_tree) == 0);
  static_assert(static_cast<int>(Producer::rex_typed_scope) == 1);
  const std::string mode = takeMode(argc, argv);
  const bool openmp = mode != "openacc-json" && mode != "consume-flang" &&
                      mode != "transfer-flang" && mode != "snapshot-flang";
  Fixture fixture(frontend(argc, argv), openmp);
  auto *pragma = fixture.primary;

  if (mode == "register-flang" || mode == "register-common-flang") {
    auto bindings = fixture.record(pragma, Producer::flang_parse_tree);
    if (mode == "register-flang") {
      OmpSupport::registerOpenMPFortranExactSemanticBindings(
          fixture.file, pragma, std::move(bindings));
    } else {
      OpenMPProducerSemanticRecords records;
      records.fortran_exact_semantic_bindings.emplace(std::move(bindings));
      OmpSupport::registerOpenMPProducerSemanticRecords(fixture.file, pragma,
                                                        std::move(records));
    }
    failReturnedOperation("prohibited Flang OpenMP producer was registered");
  }
  if (mode == "consume-flang" || mode == "transfer-flang" ||
      mode == "snapshot-flang") {
    OmpSupport::registerOpenMPFortranExactSemanticBindings(
        fixture.file, pragma,
        fixture.record(pragma, Producer::flang_parse_tree));
    std::unique_ptr<OmpSupport::OpenMPConversionSession> session;
    if (mode == "consume-flang")
      session =
          std::make_unique<OmpSupport::OpenMPConversionSession>(fixture.file);
    // Registration was valid OpenACC. Change the selected directive and typed
    // family coherently; consumption must recheck ownership of the live pragma.
    pragma->set_fortran_directive_family(
        SgPragmaDeclaration::e_fortran_directive_openmp);
    auto *selected = new SgPragma(
        "omp parallel", new Sg_File_Info(*pragma->get_startOfConstruct()),
        new Sg_File_Info(*pragma->get_endOfConstruct()));
    selected->get_startOfConstruct()->set_parent(selected);
    selected->get_endOfConstruct()->set_parent(selected);
    SageInterface::setPragma(pragma, selected);
    pragma->set_fortran_directive_raw_text("!$omp parallel");
    pragma->set_fortran_directive_logical_text("!$omp parallel");
    pragma->set_fortran_directive_semantic_text("omp parallel");
    pragma->set_fortran_selected_output_text("omp parallel");
    pragma->set_fortran_selected_output_tokens({{"omp", std::nullopt},
                                                {" ", std::nullopt},
                                                {"parallel", std::nullopt}});
    if (mode == "consume-flang") {
      OmpSupport::beginOpenACCFortranExactSemanticConsumption(pragma);
    } else if (mode == "snapshot-flang") {
      static_cast<void>(
          OmpSupport::snapshotOpenMPProducerSemanticRecords(pragma));
    } else {
      fixture.file->set_openmp_processed(false);
      fixture.file->set_openmp(true);
      fixture.file->set_openmp_ast_only(true);
      OmpSupport::processOpenMP(fixture.file);
    }
    failReturnedOperation("prohibited Flang OpenMP producer was consumed");
  }

  OmpSupport::registerOpenMPFortranExactSemanticBindings(
      fixture.file, pragma,
      fixture.record(pragma,
                     openmp ? Producer::rex_typed_scope
                            : Producer::flang_parse_tree,
                     !openmp));
  if (mode == "rex-openmp-json" || mode == "openacc-json") {
    if (openmp) {
      auto *end = fixture.ending;
      OmpSupport::registerOpenMPFortranExactSemanticBindings(
          fixture.file, end, fixture.record(end, Producer::rex_typed_scope));
    }
    requireRoundTrip(fixture, pragma, openmp);
    return 0;
  }

  Snapshot snapshot(fixture.file);
  auto &fortran = snapshot.fortran(pragma);
  if (mode == "json-flang") {
    fortran.object.at("producer") = JsonValue::number("0");
    static_cast<void>(
        Rose::AstJson::reconstructSourceFile(snapshot.ast, fixture.file));
    failReturnedOperation("JSON restored prohibited Flang OpenMP producer");
  }
  if (mode == "json-invalid-enum") {
    fortran.object.at("producer") = JsonValue::number("42");
    expectJsonFailure(snapshot.ast, fixture.file,
                      "AST JSON Fortran semantic producer has an invalid "
                      "producer enum value");
  } else if (mode == "json-malformed-bindings") {
    fortran.object.at("bindings") = JsonValue::null();
    expectJsonFailure(
        snapshot.ast, fixture.file,
        "AST JSON OpenMP Fortran semantic records are not arrays");
  } else if (mode == "json-malformed-expression") {
    fortran.object.at("expressions").array.push_back(JsonValue::null());
    expectJsonFailure(snapshot.ast, fixture.file,
                      "AST JSON OpenMP Fortran semantic expression is not an "
                      "object");
  } else {
    throw std::runtime_error("unknown producer contract mode: " + mode);
  }
  OmpSupport::discardOpenMPProducerSemanticRecords(fixture.file);
  return 0;
}
