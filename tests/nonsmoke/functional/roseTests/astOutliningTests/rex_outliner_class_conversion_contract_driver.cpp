#include "rose.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Outliner.hh"
#include "RoseAst.h"

namespace {

[[noreturn]] void fail(const std::string &reason) {
  std::cerr << "REX_TEST_ERROR[outliner-class-conversion-contract]: " << reason
            << "\n";
  std::exit(1);
}

struct CastSnapshot {
  SgCastExp *cast;
  SgCastExp::cast_type_enum sourceSurface;
  SgCastExp::semantic_conversion_kind_enum semanticConversion;
  SgCastExp::value_category_enum valueCategory;
  SgType *resultType;
  std::string resultClass;
  SgTypePtrList basePath;
  std::vector<std::string> basePathClasses;
  SgSourceFile *sourceFile;
};

SgType *removeSurfaceTypeLayers(SgType *type) {
  for (;;) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
    } else if (SgTypedefType *typedefType = isSgTypedefType(type)) {
      type = typedefType->get_base_type();
    } else if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
    } else if (SgReferenceType *reference = isSgReferenceType(type)) {
      type = reference->get_base_type();
    } else if (SgRvalueReferenceType *reference =
                   isSgRvalueReferenceType(type)) {
      type = reference->get_base_type();
    } else {
      return type;
    }
  }
}

SgClassDeclaration *classDeclaration(SgType *type) {
  SgClassType *classType = isSgClassType(removeSurfaceTypeLayers(type));
  SgClassDeclaration *declaration =
      classType != nullptr ? isSgClassDeclaration(classType->get_declaration())
                           : nullptr;
  if (declaration == nullptr) {
    fail("checked class conversion has no exact class result type");
  }
  return declaration;
}

std::string className(SgType *type) {
  SgClassDeclaration *declaration = classDeclaration(type);
  return declaration->get_name().getString();
}

bool hasExactTargetClassDeclaration(SgSourceFile *targetFile, SgType *type,
                                    const std::string &name) {
  SgClassType *canonicalType = isSgClassType(removeSurfaceTypeLayers(type));
  if (targetFile == nullptr || canonicalType == nullptr) {
    return false;
  }
  for (SgNode *node :
       NodeQuery::querySubTree(targetFile, V_SgClassDeclaration)) {
    SgClassDeclaration *declaration = isSgClassDeclaration(node);
    if (declaration != nullptr && declaration->get_name().getString() == name &&
        declaration->get_type() == canonicalType &&
        SageInterface::getEnclosingSourceFile(declaration) == targetFile) {
      return true;
    }
  }
  return false;
}

bool isCheckedClassConversion(
    SgCastExp::semantic_conversion_kind_enum conversion) {
  return conversion == SgCastExp::e_semantic_conversion_BaseToDerived ||
         conversion == SgCastExp::e_semantic_conversion_DerivedToBase ||
         conversion ==
             SgCastExp::e_semantic_conversion_UncheckedDerivedToBase ||
         conversion == SgCastExp::e_semantic_conversion_Dynamic;
}

void requireImplicitProvenance(SgCastExp *cast) {
  for (Sg_File_Info *position :
       {cast->get_file_info(), cast->get_startOfConstruct(),
        cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
    if (position == nullptr || position->isShared() ||
        !position->isCompilerGenerated() ||
        !position->isOutputInCodeGeneration() || !position->isImplicitCast() ||
        position->isTransformation()) {
      fail("implicit class conversion lost exact synthesized provenance");
    }
  }
}

void requireCastRole(SgCastExp *cast) {
  if (cast == nullptr || cast->get_operand() == nullptr ||
      cast->get_operand()->get_parent() != cast) {
    fail("checked class conversion has no exclusively owned operand");
  }
  cast->validate_semantic_conversion();

  const SgCastExp::semantic_conversion_kind_enum conversion =
      cast->get_semantic_conversion_kind();
  const bool isImplicit =
      conversion == SgCastExp::e_semantic_conversion_DerivedToBase ||
      conversion == SgCastExp::e_semantic_conversion_UncheckedDerivedToBase;
  if (isImplicit) {
    if (cast->get_cast_type() != SgCastExp::e_implicit_cast) {
      fail("implicit derived-to-base conversion gained source cast syntax");
    }
    requireImplicitProvenance(cast);
  } else {
    const SgCastExp::cast_type_enum expectedSurface =
        conversion == SgCastExp::e_semantic_conversion_BaseToDerived
            ? SgCastExp::e_static_cast
            : SgCastExp::e_dynamic_cast;
    if (cast->get_cast_type() != expectedSurface) {
      fail("explicit checked conversion lost its exact source cast syntax");
    }
    for (Sg_File_Info *position :
         {cast->get_file_info(), cast->get_startOfConstruct(),
          cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
      if (position == nullptr || position->isImplicitCast()) {
        fail("explicit checked conversion gained implicit-cast provenance");
      }
    }
  }

  const SgCastExp::value_category_enum expectedCategory =
      conversion == SgCastExp::e_semantic_conversion_DerivedToBase
          ? SgCastExp::e_value_category_prvalue
          : SgCastExp::e_value_category_lvalue;
  if (cast->get_value_category() != expectedCategory) {
    fail("checked class conversion lost its exact value category");
  }
}

SgFunctionDeclaration *findExerciseDefinition(SgProject *project) {
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(node);
    if (function != nullptr && function->get_name() == SgName("exercise") &&
        function->get_definition() != nullptr) {
      return function;
    }
  }
  fail("could not find the source exercise definition");
}

std::vector<SgAssignInitializer *> captureConstantUses(SgProject *project) {
  std::vector<SgAssignInitializer *> result;
  size_t storageUses = 0;
  size_t constantReads = 0;
  size_t declarationOnlyReads = 0;
  for (SgNode *node : NodeQuery::querySubTree(
           findExerciseDefinition(project)->get_definition()->get_body(),
           V_SgVarRefExp)) {
    auto *reference = isSgVarRefExp(node);
    if (reference->get_symbol()->get_declaration()->get_name() ==
        "declaration_only") {
      if (!reference->get_is_non_odr_use())
        fail("declaration-only constant read gained a storage requirement");
      ++declarationOnlyReads;
    }
    if (reference->get_symbol()->get_declaration()->get_name() != "constant")
      continue;
    if (reference->get_is_non_odr_use())
      ++constantReads;
    else
      ++storageUses;
    auto *copy = SageInterface::deepCopy(reference);
    if (copy->get_is_non_odr_use() != reference->get_is_non_odr_use())
      fail("native copying lost exact variable storage-use classification");
    SageInterface::deleteAST(copy);
  }
  if (storageUses != 2 || constantReads != 1 || declarationOnlyReads != 1)
    fail("frontend/reader lost address, reference-binding or constant-read "
         "roles");
  for (SgNode *node : NodeQuery::querySubTree(
           findExerciseDefinition(project)->get_definition()->get_body(),
           V_SgInitializedName)) {
    auto *name = isSgInitializedName(node);
    if (name->get_name() != "constant_address" &&
        name->get_name() != "constant_reference")
      continue;
    auto *initializer = isSgAssignInitializer(name->get_initializer());
    if (initializer == nullptr)
      fail("static member address/reference has no exact initializer");
    result.push_back(initializer);
  }
  if (result.size() != 2)
    fail("missing static member address/reference witnesses");
  return result;
}

void validateConstantUses(
    const std::vector<SgAssignInitializer *> &initializers) {
  for (SgAssignInitializer *initializer : initializers) {
    SgExpression *operand = initializer->get_operand();
    if (auto *address = isSgAddressOfOp(operand))
      operand = address->get_operand();
    if (operand == nullptr || (isSgVarRefExp(operand) == nullptr &&
                               isSgPointerDerefExp(operand) == nullptr))
      fail("static member storage use was replaced with an initializer value");
  }
}

std::vector<CastSnapshot> captureSourceCasts(SgProject *project) {
  SgFunctionDeclaration *exercise = findExerciseDefinition(project);
  std::vector<CastSnapshot> snapshots;
  std::map<SgCastExp::semantic_conversion_kind_enum, size_t> kindCounts;
  std::map<std::string, size_t> resultClassCounts;
  for (SgNode *node : NodeQuery::querySubTree(
           exercise->get_definition()->get_body(), V_SgCastExp)) {
    SgCastExp *cast = isSgCastExp(node);
    if (cast == nullptr ||
        !isCheckedClassConversion(cast->get_semantic_conversion_kind())) {
      continue;
    }
    requireCastRole(cast);
    std::vector<std::string> basePathClasses;
    for (SgType *base : cast->get_conversion_base_path())
      basePathClasses.push_back(className(base));
    snapshots.push_back(
        {cast, cast->get_cast_type(), cast->get_semantic_conversion_kind(),
         cast->get_value_category(), cast->get_type(),
         className(cast->get_type()), cast->get_conversion_base_path(),
         basePathClasses, SageInterface::getEnclosingSourceFile(cast)});
    ++kindCounts[cast->get_semantic_conversion_kind()];
    ++resultClassCounts[className(cast->get_type())];
  }

  if (snapshots.size() != 7 ||
      kindCounts[SgCastExp::e_semantic_conversion_UncheckedDerivedToBase] !=
          3 ||
      kindCounts[SgCastExp::e_semantic_conversion_DerivedToBase] != 2 ||
      kindCounts[SgCastExp::e_semantic_conversion_BaseToDerived] != 1 ||
      kindCounts[SgCastExp::e_semantic_conversion_Dynamic] != 1) {
    fail("frontend did not publish the seven exact checked class conversions");
  }
  for (const std::string &expected :
       {"rex_historical_base", "rex_adjusted_base", "rex_virtual_base",
        "rex_multiply_derived", "rex_virtual_derived"}) {
    if (resultClassCounts[expected] == 0) {
      fail("frontend omitted an expected historical/multiple/virtual class "
           "conversion");
    }
  }
  return snapshots;
}

void validateMovedCasts(SgProject *project,
                        const std::vector<CastSnapshot> &snapshots,
                        bool crossesOutputBoundary) {
  std::set<SgCastExp *> attachedCasts;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCastExp)) {
    attachedCasts.insert(isSgCastExp(node));
  }

  SgFunctionDeclaration *outlinedOwner = nullptr;
  for (const CastSnapshot &snapshot : snapshots) {
    SgCastExp *cast = snapshot.cast;
    if (attachedCasts.count(cast) != 1 ||
        cast->get_cast_type() != snapshot.sourceSurface ||
        cast->get_semantic_conversion_kind() != snapshot.semanticConversion ||
        cast->get_value_category() != snapshot.valueCategory) {
      fail("outliner deleted or changed a checked class conversion");
    }
    requireCastRole(cast);

    SgFunctionDeclaration *owner =
        SageInterface::getEnclosingFunctionDeclaration(cast);
    if (owner == nullptr || owner->get_definition() == nullptr ||
        owner->get_name().getString().find("OUT_") != 0) {
      fail("checked class conversion was not moved into the outlined function");
    }
    auto *canonical =
        isSgFunctionDeclaration(owner->get_firstNondefiningDeclaration());
    if (owner->get_definingDeclaration() != owner || canonical == nullptr ||
        canonical->get_firstNondefiningDeclaration() != canonical ||
        canonical->get_definingDeclaration() != owner ||
        canonical->get_scope() != owner->get_scope()) {
      fail("outliner broke the reciprocal canonical/defining function family");
    }
    if (outlinedOwner == nullptr) {
      outlinedOwner = owner;
    } else if (outlinedOwner != owner) {
      fail("one outlined region was split across multiple function owners");
    }

    SgSourceFile *outlinedFile = SageInterface::getEnclosingSourceFile(owner);
    SgClassDeclaration *resultDeclaration = classDeclaration(cast->get_type());
    const SgTypePtrList &movedPath = cast->get_conversion_base_path();
    if (movedPath.size() != snapshot.basePath.size()) {
      fail("checked class conversion changed its semantic base-path arity");
    }
    if (!crossesOutputBoundary) {
      if (cast->get_type() != snapshot.resultType ||
          movedPath != snapshot.basePath) {
        fail("in-file outlining changed a checked class conversion type");
      }
      continue;
    }

    // Both a newly generated file and a reparsed copied file are distinct
    // translation units.  Each must own its class declarations and canonical
    // types; project-wide pointer aliasing would leave the destination AST
    // referring to a declaration owned by the source translation unit.
    if (outlinedFile == nullptr || outlinedFile == snapshot.sourceFile ||
        cast->get_type() == snapshot.resultType ||
        className(cast->get_type()) != snapshot.resultClass ||
        SageInterface::getEnclosingSourceFile(resultDeclaration) !=
            outlinedFile ||
        !hasExactTargetClassDeclaration(outlinedFile, cast->get_type(),
                                        snapshot.resultClass)) {
      fail("cross-output outlining did not publish one distinct exact "
           "destination class result type");
    }
    for (size_t index = 0; index < movedPath.size(); ++index) {
      SgClassDeclaration *pathDeclaration = classDeclaration(movedPath[index]);
      if (movedPath[index] == snapshot.basePath[index] ||
          className(movedPath[index]) != snapshot.basePathClasses[index] ||
          SageInterface::getEnclosingSourceFile(pathDeclaration) !=
              outlinedFile ||
          !hasExactTargetClassDeclaration(outlinedFile, movedPath[index],
                                          snapshot.basePathClasses[index])) {
        fail("cross-output outlining did not publish one distinct exact "
             "destination semantic base path");
      }
    }
  }
  if (crossesOutputBoundary && !snapshots.empty()) {
    SgType *sourceClass = removeSurfaceTypeLayers(snapshots.front().resultType);
    SgType *targetClass =
        removeSurfaceTypeLayers(snapshots.front().cast->get_type());
    SgScopeStatement *targetScope = outlinedOwner->get_scope();
    auto arguments = std::make_unique<SgFunctionParameterTypeList>();
    arguments->append_argument(SageBuilder::buildPointerType(sourceClass));
    arguments->append_argument(SgTypeEllipse::createType());
    auto *member = SageBuilder::buildMemberFunctionType(
        sourceClass, arguments.get(), sourceClass,
        SgMemberFunctionType::e_const);
    auto *function =
        SageBuilder::buildFunctionType(sourceClass, arguments.get());
    if (arguments->get_parent() != nullptr)
      arguments.release();
    auto *targetPointer = isSgPointerType(SageBuilder::getTargetFileType(
        SageBuilder::buildPointerType(function), targetScope));
    auto *targetFunction =
        targetPointer != nullptr
            ? isSgFunctionType(targetPointer->get_base_type())
            : nullptr;
    auto *targetMemberPointer =
        isSgPointerMemberType(SageBuilder::getTargetFileType(
            SageBuilder::buildPointerMemberType(member, sourceClass),
            targetScope));
    auto *targetMember =
        targetMemberPointer != nullptr
            ? isSgMemberFunctionType(targetMemberPointer->get_base_type())
            : nullptr;
    if (targetFunction == nullptr || targetFunction == function ||
        targetFunction->get_return_type() != targetClass ||
        targetFunction->get_arguments().size() != 2 ||
        targetFunction->get_arguments()[0] !=
            SageBuilder::buildPointerType(targetClass) ||
        !targetFunction->get_has_ellipses() || targetMember == nullptr ||
        targetMember == member ||
        targetMember->get_class_type() != targetClass ||
        targetMember->get_return_type() != targetClass ||
        targetMember->get_arguments() != targetFunction->get_arguments() ||
        targetMember->get_mfunc_specifier() != SgMemberFunctionType::e_const ||
        !targetMember->get_has_ellipses() ||
        targetMemberPointer->get_class_type() != targetClass) {
      fail("cross-output function/member type graph retains a source identity");
    }
  }
}

} // namespace

int main(int argc, char *argv[]) {
  std::vector<std::string> arguments(argv, argv + argc);
  std::string mode;
  for (auto argument = arguments.begin(); argument != arguments.end();) {
    const std::string prefix = "--rex-class-conversion-mode=";
    if (argument->compare(0, prefix.size(), prefix) == 0) {
      mode = argument->substr(prefix.size());
      argument = arguments.erase(argument);
    } else {
      ++argument;
    }
  }
  if (mode != "direct" && mode != "wrapper" && mode != "new_file" &&
      mode != "copied") {
    fail("expected direct, wrapper, new_file, or copied mode");
  }

  Outliner::commandLineProcessing(arguments);
  SgProject *project = frontend(arguments);
  if (project == nullptr) {
    fail("frontend returned a null project");
  }
  AstTests::runAllTests(project);
  const std::vector<CastSnapshot> snapshots = captureSourceCasts(project);
  const auto constantUses = captureConstantUses(project);
  validateConstantUses(constantUses);

  Outliner::outlineAll(project);
  AstTests::runAllTests(project);
  validateMovedCasts(project, snapshots,
                     mode == "new_file" || mode == "copied");
  validateConstantUses(constantUses);

  SgTreeCopy copier;
  auto *copiedProject = isSgProject(project->copy(copier));
  if (copiedProject == nullptr || copiedProject == project)
    fail("native copying did not construct a distinct completed project");
  AstTests::runAllTests(copiedProject);
  auto copiedSnapshots = snapshots;
  for (auto &snapshot : copiedSnapshots) {
    auto *original = snapshot.cast;
    snapshot.resultType = original->get_type();
    snapshot.basePath = original->get_conversion_base_path();
    snapshot.sourceFile = SageInterface::getEnclosingSourceFile(original);
    snapshot.cast = isSgCastExp(copier.get_copiedNodeMap().at(original));
    if (snapshot.cast == nullptr || snapshot.cast == original)
      fail("native copying lost an exact outlined class conversion");
  }
  validateMovedCasts(copiedProject, copiedSnapshots, true);
  std::vector<SgAssignInitializer *> copiedConstantUses;
  for (auto *initializer : constantUses) {
    auto *copied =
        isSgAssignInitializer(copier.get_copiedNodeMap().at(initializer));
    if (copied == nullptr || copied == initializer)
      fail("native copying lost a static member storage use");
    copiedConstantUses.push_back(copied);
  }
  validateConstantUses(copiedConstantUses);
  SageInterface::deleteAST(copiedProject);
  return 0;
}
