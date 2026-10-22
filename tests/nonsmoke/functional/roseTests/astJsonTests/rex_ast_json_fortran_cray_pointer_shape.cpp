#include "astJson/sageAstJsonPrivate.h"
#include "rose.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string takeJsonDir(int &argc, char **argv) {
  constexpr char prefix[] = "--rex-ast-json-dir=";
  std::string result;
  std::vector<char *> filtered{argv[0]};
  for (int index = 1; index < argc; ++index) {
    if (std::strncmp(argv[index], prefix, sizeof(prefix) - 1) == 0) {
      ROSE_ASSERT(result.empty());
      result = argv[index] + sizeof(prefix) - 1;
    } else {
      filtered.push_back(argv[index]);
    }
  }
  argc = static_cast<int>(filtered.size());
  for (int index = 0; index < argc; ++index) {
    argv[index] = filtered[index];
  }
  argv[argc] = nullptr;
  return result;
}

SgSourceFile *firstSourceFile(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  ROSE_ASSERT(project->numberOfFiles() == 1);
  SgSourceFile *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  return file;
}

SgInitializedName *exactName(SgNode *root, const std::string &spelling) {
  SgInitializedName *result = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgInitializedName)) {
    SgInitializedName *name = isSgInitializedName(node);
    ROSE_ASSERT(name != nullptr);
    if (name->get_name().getString() != spelling) {
      continue;
    }
    ROSE_ASSERT(result == nullptr);
    result = name;
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgArrayType *ordinaryArray(SgType *type) {
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        return array;
      }
      type = array->get_base_type();
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

void verifyContract(SgNode *root) {
  SgInitializedName *pointer = exactName(root, "shape_address");
  SgInitializedName *pointee = exactName(root, "shaped_pointee");
  SgVariableDeclaration *pointerDeclaration =
      isSgVariableDeclaration(pointer->get_parent());
  SgExprListExp *shape = pointer->get_fortran_cray_pointer_pointee_shape();
  SgArrayType *semanticArray = ordinaryArray(pointee->get_type());

  ROSE_ASSERT(pointerDeclaration != nullptr);
  ROSE_ASSERT(pointerDeclaration->get_variables().size() == 1);
  ROSE_ASSERT(pointerDeclaration->get_variables().front() == pointer);
  ROSE_ASSERT(isSgTypeCrayPointer(pointer->get_fortran_source_type()) !=
              nullptr);
  ROSE_ASSERT(pointer->get_fortran_source_type()->get_fortran_source_syntax());
  ROSE_ASSERT(isSgTypeInt(pointer->get_type()) != nullptr ||
              isSgTypeSignedInt(pointer->get_type()) != nullptr);
  ROSE_ASSERT(pointer->get_cray_pointer_pointee() == pointee);
  ROSE_ASSERT(shape != nullptr);
  ROSE_ASSERT(shape->get_parent() == pointer);
  ROSE_ASSERT(shape->get_expressions().size() == 1);
  ROSE_ASSERT(semanticArray != nullptr);
  ROSE_ASSERT(semanticArray->get_rank() == 1);
  ROSE_ASSERT(pointee->get_fortran_separate_shape_declaration() ==
              pointerDeclaration);
  ROSE_ASSERT(pointee->get_shapeDeferred());
  ROSE_ASSERT(isSgSubscriptExpression(shape->get_expressions().front()) !=
              nullptr);
}

} // namespace

int main(int argc, char **argv) {
  const std::string jsonDir = takeJsonDir(argc, argv);
  ROSE_ASSERT(!jsonDir.empty());
  std::filesystem::remove_all(jsonDir);
  std::filesystem::create_directories(jsonDir);

  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  project->skipfinalCompileStep(true);
  SgSourceFile *original = firstSourceFile(project);
  verifyContract(original);

  const auto ast = Rose::AstJson::parseAstFileJson(
      Rose::AstJson::buildJson(
          original, Rose::AstJson::Checkpoint::PreOmpConstruction, original),
      Rose::AstJson::checkpointName(
          Rose::AstJson::Checkpoint::PreOmpConstruction));
  for (int malformed = 0; malformed < 7; ++malformed) {
    auto invalid = ast;
    bool changed = false;
    for (auto &record : invalid.nodes) {
      if (record.kind != "SgInitializedName")
        continue;
      const uint64_t shape = record.properties.requiredNonnegativeNodeId(
          "fortran_cray_pointer_pointee_shape");
      if (shape == 0)
        continue;
      ROSE_ASSERT(!changed);
      changed = true;
      if (malformed == 0) {
        record.properties.object["fortran_cray_pointer_pointee_shape"] =
            Rose::AstJson::JsonValue::number("0");
      } else {
        auto &shape_record = invalid.nodes.at(invalid.index_by_id.at(shape));
        if (malformed == 1) {
          for (auto &edge : shape_record.edges)
            if (edge.field == "parent")
              edge.target = 0;
        } else if (malformed == 2) {
          shape_record.kind = "SgIntVal";
        } else if (malformed == 3) {
          shape_record.edges.erase(
              std::remove_if(
                  shape_record.edges.begin(), shape_record.edges.end(),
                  [](const auto &edge) { return edge.field == "expressions"; }),
              shape_record.edges.end());
        } else {
          auto operand = std::find_if(
              shape_record.edges.begin(), shape_record.edges.end(),
              [](const auto &edge) { return edge.field == "expressions"; });
          ROSE_ASSERT(operand != shape_record.edges.end());
          if (malformed == 4) {
            auto duplicate = *operand;
            ++duplicate.index;
            shape_record.edges.push_back(duplicate);
          } else if (malformed == 5) {
            operand->target = record.id;
          } else {
            auto &child =
                invalid.nodes.at(invalid.index_by_id.at(operand->target));
            for (auto &edge : child.edges)
              if (edge.field == "parent")
                edge.target = 0;
          }
        }
      }
    }
    ROSE_ASSERT(changed);
    const auto nodes_before = numberOfNodes();
    const auto type_entries_before =
        *SgNode::get_globalTypeTable()->get_type_table()->get_table();
    const auto function_types_before =
        SgNode::get_globalFunctionTypeTable()->get_types();
    bool rejected = false;
    std::string diagnostic;
    try {
      (void)Rose::AstJson::reconstructSourceFile(invalid, original);
    } catch (const std::runtime_error &error) {
      diagnostic = error.what();
      rejected = true;
    }
    ROSE_ASSERT(rejected && numberOfNodes() == nodes_before);
    const char *expected =
        malformed == 0
            ? "AST JSON initialized-name Cray pointee shape property does not "
              "match its exact owned edge"
        : malformed < 3
            ? "AST JSON initialized-name Cray pointee shape has invalid "
              "serialized ownership"
        : malformed == 3 ? "AST JSON Cray pointee shape has no dimensions"
        : malformed == 5 ? "AST JSON expression list has a non-expression child"
                         : "AST JSON expression list has conflicting ownership";
    ROSE_ASSERT(diagnostic == expected);
    ROSE_ASSERT(*SgNode::get_globalTypeTable()->get_type_table()->get_table() ==
                type_entries_before);
    const auto &functions = SgNode::get_globalFunctionTypeTable()->get_types();
    ROSE_ASSERT(functions.size() == function_types_before.size());
    for (const auto &entry : function_types_before) {
      const auto found = functions.find(entry.first);
      ROSE_ASSERT(found != functions.end() && found->second == entry.second);
    }
    verifyContract(original);
  }

  const Rose_STL_Container<SgNode *> bodies =
      NodeQuery::querySubTree(original, V_SgBasicBlock);
  ROSE_ASSERT(bodies.size() == 1);
  SgTreeCopy copyHelp;
  SgBasicBlock *bodyCopy = isSgBasicBlock(bodies.front()->copy(copyHelp));
  ROSE_ASSERT(bodyCopy != nullptr);
  verifyContract(bodyCopy);

  std::vector<std::string> checkpointArguments =
      original->get_originalCommandLineArgumentList();
  checkpointArguments.push_back(
      "-rex:ast-json-checkpoint=pre-omp-construction");
  original->get_originalCommandLineArgumentList() = checkpointArguments;
  Rose::AstJson::Options options;
  options.outputDirectory = jsonDir;
  SgSourceFile *restored = Rose::AstJson::roundTripSourceFile(
      original, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != original);
  verifyContract(restored);

  return backend(project);
}
