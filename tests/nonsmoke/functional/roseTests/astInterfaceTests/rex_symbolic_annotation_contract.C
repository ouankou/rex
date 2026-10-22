#include "rose.h"

#include "ArrayAnnot.h"
#include "AstInterface_ROSE.h"
#include "CPPAstInterface.h"
#include "OperatorDescriptors.h"
#include "RoseAst.h"
#include "StmtInfoCollect.h"
#include "SymbolicVal.h"
#include "ValueAnnot.h"
#include "astJson/sageAstJson.h"
#include "midend/programAnalysis/pointerAnal/SteensgaardPtrAnal.h"

#include <limits>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

SgProject *parseProject(int argc, char **argv) {
  std::vector<char *> arguments;
  arguments.push_back(argv[0]);
  for (int i = 1; i < argc; ++i)
    arguments.push_back(argv[i]);
  return frontend(static_cast<int>(arguments.size()), arguments.data());
}

SgFunctionDeclaration *findFunction(SgNode *root, const std::string &name) {
  RoseAst ast(root);
  for (RoseAst::iterator current = ast.begin(); current != ast.end();
       ++current) {
    SgFunctionDeclaration *function = isSgFunctionDeclaration(*current);
    if (function != nullptr && function->get_name().getString() == name &&
        function->get_definition() != nullptr)
      return function;
  }
  ROSE_ABORT();
}

SgExpression *returnExpression(SgFunctionDeclaration *function) {
  Rose_STL_Container<SgNode *> returns =
      NodeQuery::querySubTree(function->get_definition(), V_SgReturnStmt);
  ROSE_ASSERT(returns.size() == 1);
  SgExpression *expression = isSgReturnStmt(returns.front())->get_expression();
  ROSE_ASSERT(expression != nullptr);
  return expression;
}

void requireFunction(const SymbolicVal &value,
                     AstInterface::OperatorEnum expectedOperation,
                     const std::string &expectedName, size_t expectedArity) {
  AstInterface::OperatorEnum operation = AstInterface::OP_NONE;
  std::string name;
  std::vector<SymbolicVal> arguments;
  ROSE_ASSERT(value.isFunction(&operation, &name, &arguments));
  ROSE_ASSERT(operation == expectedOperation);
  ROSE_ASSERT(name == expectedName);
  ROSE_ASSERT(arguments.size() == expectedArity);
}

void requireExactTarget(const SymbolicValDescriptor &target,
                        SgExpression *expected) {
  AstNodePtr exactTarget;
  ROSE_ASSERT(target.get_val().isAstWrap(exactTarget));
  ROSE_ASSERT(exactTarget.get_ptr() == expected);
}

SgExpression *
rebuildAnnotationArgument(AstInterface &interface, SgExpression *expression,
                          const std::string &parameterType = "int") {
  ParameterDeclaration parameters;
  parameters.add_param(parameterType, "argument");
  AstInterface::AstNodeList arguments{AstNodePtr(expression)};
  ReplaceParams replacements(interface, parameters, arguments);
  SgExpression *rebuilt = isSgExpression(
      replacements.find("argument").CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(rebuilt->get_type() != nullptr);
  return rebuilt;
}

SymbolicVal replaceAnnotationArgument(AstInterface &interface,
                                      SgExpression *expression,
                                      const std::string &parameterType) {
  ParameterDeclaration parameters;
  parameters.add_param(parameterType, "argument");
  AstInterface::AstNodeList arguments{AstNodePtr(expression)};
  ReplaceParams replacements(interface, parameters, arguments);
  return replacements.find("argument");
}

template <class SageNode, class Cast>
SageNode *findUniqueExpressionNode(SgExpression *root, Cast cast) {
  SageNode *result = nullptr;
  RoseAst traversal(root);
  for (RoseAst::iterator node = traversal.begin(); node != traversal.end();
       ++node) {
    if (SageNode *match = cast(*node)) {
      ROSE_ASSERT(result == nullptr);
      result = match;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

SgExpression *stripSemanticConversions(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  while (SgCastExp *conversion = isSgCastExp(expression)) {
    conversion->validate_semantic_conversion();
    ROSE_ASSERT(conversion->get_operand() != nullptr);
    expression = conversion->get_operand();
  }
  return expression;
}

std::set<SgNode *> readLocations(AstInterface &interface,
                                 SgExpression *expression) {
  std::set<SgNode *> reads;
  SideEffectAnalysisInterface::CollectObject collect = [&](AstNodePtr read,
                                                           AstNodePtr) {
    ROSE_ASSERT(!read.is_unknown() && read.get_ptr() != nullptr);
    reads.insert(read.get_ptr());
    return true;
  };
  StmtSideEffectCollect effects(interface);
  effects.set_read_collect(collect);
  ROSE_ASSERT(effects(expression));
  return reads;
}

void requireTypedReplacement(SgProject *project) {
  SgFunctionDeclaration *function =
      findFunction(project, "rex_symbolic_annotation_shadow");
  SgInitializedName *parameter = function->get_parameterList()->get_args()[0];
  SgVariableSymbol *parameterSymbol = nullptr;
  SgVariableSymbol *shadowSymbol = nullptr;
  for (SgNode *node :
       NodeQuery::querySubTree(function->get_definition(), V_SgVarRefExp)) {
    SgVariableSymbol *symbol = isSgVarRefExp(node)->get_symbol();
    if (symbol->get_declaration() == parameter)
      parameterSymbol = symbol;
    else if (symbol->get_name() == "value")
      shadowSymbol = symbol;
  }
  ROSE_ASSERT(parameterSymbol != nullptr && shadowSymbol != nullptr);
  ROSE_ASSERT(AstInterface::GetVarName(parameter, true) !=
              AstInterface::GetVarName(shadowSymbol->get_declaration(), true));
  auto functions =
      NodeQuery::querySubTree(function->get_definition(), V_SgFunctionRefExp);
  ROSE_ASSERT(functions.size() == 1);
  SgFunctionSymbol *calledFunction =
      isSgFunctionRefExp(functions.front())->get_symbol();
  ROSE_ASSERT(calledFunction != nullptr);
  AstInterface::SetFunctionNameMangling(
      +[](const SgFunctionDeclaration *declaration) {
        return "rex_callback_" + declaration->get_name().getString();
      });
  ROSE_ASSERT(AstInterface::GetVarName(functions.front(), true) ==
              "rex_callback_value");
  AstInterface::SetFunctionNameMangling(nullptr);
  AstInterfaceImpl implementation(function->get_definition());
  AstInterface interface(&implementation);
  AstNodePtr exactFunction;
  ROSE_ASSERT(SymbolicValGenerator::GetSymbolicVal(interface, functions.front())
                  .isAstWrap(exactFunction));
  ROSE_ASSERT(exactFunction.get_ptr() == functions.front());
  SymbolicVar parameterVariable("value", AST_NULL, parameterSymbol);
  SymbolicVar shadowVariable("value", AST_NULL, shadowSymbol);
  ROSE_ASSERT(!(parameterVariable == shadowVariable));

  AstTreeReplaceVar replace(parameterVariable, SymbolicVal(17));
  replace(interface, function->get_definition()->get_body());
  unsigned shadowReferences = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(function->get_definition(), V_SgVarRefExp)) {
    SgVariableSymbol *symbol = isSgVarRefExp(node)->get_symbol();
    ROSE_ASSERT(symbol != parameterSymbol);
    if (symbol == shadowSymbol)
      ++shadowReferences;
  }
  ROSE_ASSERT(shadowReferences == 1);
  functions =
      NodeQuery::querySubTree(function->get_definition(), V_SgFunctionRefExp);
  ROSE_ASSERT(functions.size() == 1);
  ROSE_ASSERT(isSgFunctionRefExp(functions.front())->get_symbol() ==
              calledFunction);
}

int exerciseContracts(int argc, char **argv) {
  ROSE_ASSERT(CompareVal(SymbolicVal("1/-2", "fraction"), SymbolicVal(0)) ==
              REL_LT);
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);

  for (const auto &[functionName, isStatic] :
       {std::pair<const char *, bool>{"rex_symbolic_annotation_static_call",
                                      true},
        {"rex_symbolic_annotation_template_call", false}}) {
    auto calls = NodeQuery::querySubTree(findFunction(project, functionName),
                                         V_SgFunctionCallExp);
    ROSE_ASSERT(calls.size() == 1);
    auto *call = isSgFunctionCallExp(calls.front());
    AstInterfaceImpl callImplementation(call);
    CPPAstInterface callInterface(&callImplementation);
    AstNodePtr object, access;
    AstInterface::AstNodeList arguments;
    std::string name;
    ROSE_ASSERT(callInterface.IsMemberFunctionCall(call, &object, &name,
                                                   nullptr, &arguments));
    ROSE_ASSERT(arguments.size() == 1 &&
                arguments.front().get_ptr() ==
                    call->get_args()->get_expressions().front());
    ROSE_ASSERT((object == AST_NULL) == isStatic);
    ROSE_ASSERT(name == (isStatic ? "static_member" : "template_member"));
    ROSE_ASSERT(callInterface.IsMemberFunctionCall(call, &object, &name,
                                                   &access, &arguments));
    ROSE_ASSERT(access.get_ptr() == call->get_function());
  }

  for (const auto &callCase :
       {std::pair<const char *, size_t>{"rex_symbolic_annotation_zero_min", 0},
        {"rex_symbolic_annotation_unary_max", 1}}) {
    SgExpression *call =
        returnExpression(findFunction(project, callCase.first));
    AstInterfaceImpl callImplementation(call);
    AstInterface callInterface(&callImplementation);
    AstInterface::OperatorEnum operation = AstInterface::UOP_MINUS;
    std::vector<SymbolicVal> arguments;
    ROSE_ASSERT(SymbolicValGenerator::GetSymbolicVal(callInterface, call)
                    .isFunction(&operation, nullptr, &arguments));
    ROSE_ASSERT(operation == AstInterface::OP_NONE);
    ROSE_ASSERT(arguments.size() == callCase.second);
  }

  SgExpression *memberExpression =
      returnExpression(findFunction(project, "rex_symbolic_annotation_member"));
  AstInterfaceImpl implementation(memberExpression);
  AstInterface interface(&implementation);
  for (const auto &[functionName, kind] :
       {std::pair<const char *, VariantT>{
            "rex_symbolic_annotation_member_pointer", V_SgDotStarOp},
        {"rex_symbolic_annotation_member_arrow_pointer", V_SgArrowStarOp}}) {
    const auto memberPointers = NodeQuery::querySubTree(
        findFunction(project, functionName)->get_definition(), kind);
    ROSE_ASSERT(memberPointers.size() == 3);
    ROSE_ASSERT(interface.IsSameVarRef(memberPointers[0], memberPointers[2]));
    ROSE_ASSERT(!interface.IsSameVarRef(memberPointers[0], memberPointers[1]));
    std::vector<std::string> names;
    for (SgNode *node : memberPointers) {
      AstNodeType type;
      std::string name;
      bool indirect = false;
      ROSE_ASSERT(interface.IsVarRef(node, &type, &name, nullptr, nullptr, true,
                                     &indirect));
      ROSE_ASSERT(type.get_ptr() == SageBuilder::buildIntType() && indirect);
      AstInterface::AstNodeList memory;
      ROSE_ASSERT(interface.IsMemoryAccess(node, &memory));
      ROSE_ASSERT(memory.size() == 1 && memory.front().get_ptr() == node);
      auto *access = isSgBinaryOp(node);
      auto *selector = stripSemanticConversions(access->get_rhs_operand());
      std::set<SgNode *> expectedReads{node, selector};
      if (kind == V_SgArrowStarOp)
        expectedReads.insert(
            stripSemanticConversions(access->get_lhs_operand()));
      ROSE_ASSERT(readLocations(interface, access) == expectedReads);
      names.push_back(name);
    }
    ROSE_ASSERT(names[0] == names[2] && names[0] != names[1]);
  }
  for (const auto &[functionName, kind] :
       {std::pair<const char *, VariantT>{"rex_symbolic_annotation_member_call",
                                          V_SgDotStarOp},
        {"rex_symbolic_annotation_member_arrow_call", V_SgArrowStarOp}}) {
    const auto methods = NodeQuery::querySubTree(
        findFunction(project, functionName)->get_definition(), kind);
    ROSE_ASSERT(methods.size() == 1);
    AstInterface::AstNodeList memory;
    ROSE_ASSERT(!interface.IsMemoryAccess(methods.front(), &memory));
    ROSE_ASSERT(memory.empty());
  }
  auto *templateMember = findUniqueExpressionNode<SgDotExp>(
      returnExpression(
          findFunction(project, "rex_symbolic_annotation_member_template")),
      [](SgNode *node) { return isSgDotExp(node); });
  auto *templateReference =
      isSgNonrealRefExp(templateMember->get_rhs_operand());
  ROSE_ASSERT(templateReference != nullptr);
  ROSE_ASSERT(SageInterface::requireResolvedVariableTemplateReference(
                  templateReference, "member memory contract") != nullptr);
  AstInterface::AstNodeList templateMemory;
  ROSE_ASSERT(interface.IsMemoryAccess(templateMember, &templateMemory));
  ROSE_ASSERT(templateMemory.size() == 1 &&
              templateMemory.front().get_ptr() == templateMember);
  ROSE_ASSERT(readLocations(interface, templateMember) ==
              std::set<SgNode *>{templateMember});
  auto checkStaticReferences = [&](SgNode *root) {
    auto *arraySize = isSgSizeOfOp(returnExpression(
        findFunction(root, "rex_symbolic_annotation_sizeof_array_type")));
    ROSE_ASSERT(arraySize != nullptr);
    auto *arrayType = isSgArrayType(arraySize->get_operand_type());
    ROSE_ASSERT(arrayType != nullptr);
    ROSE_ASSERT(SageInterface::getArrayElementCount(arrayType) == 4);
    auto *extent = isSgIntVal(arrayType->get_index());
    ROSE_ASSERT(extent != nullptr && extent->get_parent() == arrayType);
    auto *writtenBound = isSgSizeOfOp(extent->get_originalExpressionTree());
    ROSE_ASSERT(writtenBound != nullptr &&
                writtenBound->get_parent() == extent &&
                isSgTypeInt(writtenBound->get_operand_type()) != nullptr);
    std::vector<std::string> sizeofSignatures;
    for (const char *functionName :
         {"rex_symbolic_annotation_sizeof_fixed",
          "rex_symbolic_annotation_sizeof_vla_left",
          "rex_symbolic_annotation_sizeof_vla_right",
          "rex_symbolic_annotation_sizeof_dependent",
          "rex_symbolic_annotation_sizeof_unevaluated",
          "rex_symbolic_annotation_sizeof_vla_effect"}) {
      auto *function = findFunction(root, functionName);
      auto *operation = isSgSizeOfOp(returnExpression(function));
      ROSE_ASSERT(operation != nullptr && !interface.IsConstant(operation));
      sizeofSignatures.push_back(AstInterface::GetVariableSignature(operation));
      if (std::string(functionName) !=
          "rex_symbolic_annotation_sizeof_dependent") {
        SteensgaardPtrAnal analysis;
        analysis(interface, function->get_definition());
        ROSE_ASSERT(analysis.translate_exp(operation).name.find("v:") == 0);
        if (auto *operand = isSgCommaOpExp(operation->get_operand_expr())) {
          ROSE_ASSERT(operation->expression_operand_is_evaluated());
          ROSE_ASSERT(
              !analysis.translate_exp(operand->get_lhs_operand()).name.empty());
        }
        if (!operation->expression_operand_is_evaluated() &&
            operation->get_operand_expr() != nullptr)
          ROSE_ASSERT(analysis.translate_exp(operation->get_operand_expr())
                          .name.empty());
      }
    }
    ROSE_ASSERT(sizeofSignatures[1] != sizeofSignatures[2]);
    for (const char *functionName :
         {"rex_symbolic_annotation_integer_decimal",
          "rex_symbolic_annotation_integer_hex",
          "rex_symbolic_annotation_integer_octal",
          "rex_symbolic_annotation_integer_suffix",
          "rex_symbolic_annotation_integer_separator",
          "rex_symbolic_annotation_integer_enum"}) {
      auto *constant = returnExpression(findFunction(root, functionName));
      std::string type, value;
      ROSE_ASSERT(interface.IsConstant(constant, &type, &value));
      ROSE_ASSERT(type == "int" && value == "42");
      ROSE_ASSERT(AstInterface::GetVariableSignature(constant) == value);
      AstNodePtr rebuilt = interface.CreateConstant(type, value);
      int number = 0;
      ROSE_ASSERT(interface.IsConstInt(rebuilt, &number) && number == 42);
      SageInterface::deleteAST(rebuilt.get_ptr());
    }
    CPPAstInterface memberInterface(interface);
    std::vector<SgNode *> staticReferences;
    SgNode *differentSpecialization = nullptr;
    for (const char *functionName :
         {"rex_symbolic_annotation_static_identity",
          "rex_symbolic_annotation_static_temporary"}) {
      for (SgNode *node : NodeQuery::querySubTree(
               findFunction(root, functionName), V_SgNonrealRefExp)) {
        auto *reference = isSgNonrealRefExp(node);
        if (reference->get_resolved_variable_declaration() == nullptr)
          continue;
        auto *name = SageInterface::requireResolvedVariableTemplateReference(
            reference, "static member identity contract");
        auto *primary = SageInterface::requireCanonicalVariableTemplatePrimary(
            reference->get_resolved_variable_declaration(),
            "static member identity contract");
        ROSE_ASSERT(primary->get_declarationModifier()
                        .get_storageModifier()
                        .isStatic());
        auto &arguments = reference->get_resolved_variable_declaration()
                              ->get_semanticTemplateArguments();
        ROSE_ASSERT(arguments.size() == 1);
        SgNode *access = node;
        if (isSgDotExp(node->get_parent()) ||
            isSgArrowExp(node->get_parent())) {
          access = node->get_parent();
          AstNodePtr receiver;
          std::string selector = "unset";
          ROSE_ASSERT(
              memberInterface.IsMemberAccess(access, &receiver, &selector));
          ROSE_ASSERT(receiver.get_ptr() ==
                      isSgBinaryOp(access)->get_lhs_operand());
          ROSE_ASSERT(selector == name->get_name().getString());
        }
        if (isSgTypeInt(arguments.front()->get_type())) {
          ROSE_ASSERT(differentSpecialization == nullptr);
          differentSpecialization = access;
          continue;
        }
        staticReferences.push_back(access);
        ROSE_ASSERT(interface.IsSameVarRef(access, name));
        ROSE_ASSERT(AstInterface::GetVarName(access, true) ==
                    AstInterface::GetVarName(name, true));
        bool indirect = false;
        ROSE_ASSERT(interface.IsVarRef(access, nullptr, nullptr, nullptr,
                                       nullptr, true, &indirect));
        ROSE_ASSERT(!indirect);
      }
    }
    ROSE_ASSERT(staticReferences.size() == 5 &&
                differentSpecialization != nullptr);
    for (SgNode *reference : staticReferences) {
      ROSE_ASSERT(interface.IsSameVarRef(staticReferences.front(), reference));
      ROSE_ASSERT(!interface.IsSameVarRef(reference, differentSpecialization));
      ROSE_ASSERT(AstInterface::GetVarName(reference, true) !=
                  AstInterface::GetVarName(differentSpecialization, true));
    }
    for (const char *functionName :
         {"rex_symbolic_annotation_member",
          "rex_symbolic_annotation_dependent_member"}) {
      auto *access = findUniqueExpressionNode<SgDotExp>(
          returnExpression(findFunction(root, functionName)),
          [](SgNode *node) { return isSgDotExp(node); });
      AstNodePtr receiver;
      std::string selector = "unset";
      ROSE_ASSERT(memberInterface.IsMemberAccess(access, &receiver, &selector));
      ROSE_ASSERT(receiver.get_ptr() == access->get_lhs_operand());
      ROSE_ASSERT(selector == "member");
    }
    for (const char *functionName :
         {"rex_symbolic_annotation_template_call",
          "rex_symbolic_annotation_dependent_call"}) {
      auto *access = findUniqueExpressionNode<SgDotExp>(
          returnExpression(findFunction(root, functionName)),
          [](SgNode *node) { return isSgDotExp(node); });
      ROSE_ASSERT(!memberInterface.IsMemberAccess(access));
    }
    ROSE_ASSERT(!memberInterface.IsMemberAccess(AST_NULL));
    return std::make_tuple(
        AstInterface::GetVarName(staticReferences.front(), true),
        AstInterface::GetVarName(differentSpecialization, true),
        sizeofSignatures);
  };
  const auto staticIdentities = checkStaticReferences(project);
  SgTreeCopy treeCopy;
  auto *copied = isSgProject(project->copy(treeCopy));
  ROSE_ASSERT(copied != nullptr && copied != project);
  const auto &sourceArguments =
      findFunction(project, "rex_symbolic_annotation_member_call")
          ->get_type_syntax()
          ->get_arguments();
  const auto &copiedArguments =
      findFunction(copied, "rex_symbolic_annotation_member_call")
          ->get_type_syntax()
          ->get_arguments();
  ROSE_ASSERT(sourceArguments.size() == 2 && copiedArguments.size() == 2);
  auto *memberPointer = isSgPointerMemberType(copiedArguments[1]);
  ROSE_ASSERT(memberPointer != nullptr && memberPointer != sourceArguments[1]);
  ROSE_ASSERT(copiedArguments[0] != sourceArguments[0]);
  ROSE_ASSERT(memberPointer->get_class_type() == copiedArguments[0]);
  auto *memberFunction = isSgMemberFunctionType(memberPointer->get_base_type());
  ROSE_ASSERT(memberFunction != nullptr &&
              memberFunction->get_class_type() == copiedArguments[0]);
  ROSE_ASSERT(checkStaticReferences(copied) == staticIdentities);
  delete copied;
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_static_member_identity_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  memberExpression =
      returnExpression(findFunction(project, "rex_symbolic_annotation_member"));
  implementation.set_top(memberExpression);
  ROSE_ASSERT(checkStaticReferences(restored) == staticIdentities);
  for (const char *functionName : {"rex_symbolic_annotation_pointer_min",
                                   "rex_symbolic_annotation_pointer_max"}) {
    SgFunctionDeclaration *function = findFunction(project, functionName);
    SgFunctionCallExp *call = isSgFunctionCallExp(returnExpression(function));
    ROSE_ASSERT(call != nullptr);
    SgVarRefExp *pointerRef = nullptr;
    SgInitializedName *pointer = function->get_parameterList()->get_args()[0];
    for (SgNode *node : NodeQuery::querySubTree(call, V_SgVarRefExp)) {
      SgVarRefExp *reference = isSgVarRefExp(node);
      if (reference->get_symbol()->get_declaration() == pointer)
        pointerRef = reference;
    }
    ROSE_ASSERT(pointerRef != nullptr);
    ROSE_ASSERT(!interface.IsMin(pointerRef));
    ROSE_ASSERT(!interface.IsMax(pointerRef));
  }
  const auto &baseParameters =
      findFunction(project, "rex_symbolic_annotation_base_type")
          ->get_parameterList()
          ->get_args();
  ROSE_ASSERT(baseParameters.size() == 5);
  for (SgInitializedName *parameter : baseParameters)
    ROSE_ASSERT(AstInterface::GetBaseTypeName(parameter->get_type()) ==
                "constant_record");
  ROSE_ASSERT(AstInterface::GetTypeName(baseParameters[2]->get_type()) ==
              "constant_record_ptr_");
  SgFunctionDeclaration *scopedFunction =
      findFunction(project, "rex_symbolic_annotation_scoped_type");
  auto *scopedParameter = scopedFunction->get_parameterList()->get_args()[0];
  std::string scopedTypeName;
  AstInterface::GetTypeInfo(scopedParameter->get_type(), &scopedTypeName,
                            nullptr, true);
  ROSE_ASSERT(scopedTypeName ==
              "rex_symbolic_scope::rex_symbolic_scope::RexScopedRecord");
  AstInterface::GetTypeInfo(
      scopedFunction->get_parameterList()->get_args()[1]->get_type(),
      &scopedTypeName, nullptr, true);
  ROSE_ASSERT(scopedTypeName == "constrex_symbolic_scope::rex_symbolic_scope::"
                                "RexScopedRecord_ref_");
  const std::string scopedVariable =
      AstInterface::GetVarName(scopedParameter, true);
  auto *scopedMember = findUniqueExpressionNode<SgDotExp>(
      returnExpression(scopedFunction),
      [](SgNode *node) { return isSgDotExp(node); });
  ROSE_ASSERT(AstInterface::GetVarName(scopedMember, true) ==
              scopedVariable + ".member");
  for (VariantT kind : {V_SgDotExp, V_SgArrowExp}) {
    for (SgNode *node : NodeQuery::querySubTree(
             findFunction(project, "rex_symbolic_annotation_base_type")
                 ->get_definition(),
             kind)) {
      auto *member = isSgBinaryOp(node);
      const std::string baseName = AstInterface::GetVarName(
          stripSemanticConversions(member->get_lhs_operand()), true);
      AstNodeType memberType;
      ROSE_ASSERT(interface.IsVarRef(member, &memberType));
      ROSE_ASSERT(memberType.get_ptr() == member->get_type());
      ROSE_ASSERT(AstInterface::GetVarName(member, true) ==
                  baseName + (kind == V_SgDotExp ? ".member" : "->member"));
    }
  }
  std::set<std::string> prototypeParameters;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDeclaration)) {
    auto *declaration = isSgFunctionDeclaration(node);
    if (declaration->get_name() == "rex_symbolic_annotation_prototype")
      prototypeParameters.insert(AstInterface::GetVarName(
          declaration->get_parameterList()->get_args()[0], true));
  }
  ROSE_ASSERT(prototypeParameters.size() == 2);
  std::set<std::string> localTypes;
  for (const char *functionName :
       {"rex_symbolic_annotation_local_type_left",
        "rex_symbolic_annotation_local_type_right"}) {
    const auto declarations = NodeQuery::querySubTree(
        findFunction(project, functionName)->get_definition(),
        V_SgClassDeclaration);
    ROSE_ASSERT(!declarations.empty());
    auto *declaration = isSgClassDeclaration(declarations.front());
    ROSE_ASSERT(declaration->get_name() == "RexLocalRecord");
    std::string typeName;
    AstInterface::GetTypeInfo(declaration->get_type(), &typeName, nullptr,
                              true);
    ROSE_ASSERT(!typeName.empty());
    localTypes.insert(typeName);
  }
  ROSE_ASSERT(localTypes.size() == 2);
  const std::string bytes("a\0b", 3);
  SgStringVal *literal =
      isSgStringVal(interface.CreateConstant("string", bytes).get_ptr());
  ROSE_ASSERT(literal != nullptr && literal->get_byte_value() == bytes);
  SageInterface::deepDelete(literal);

  ParameterDeclaration parameters;
  parameters.add_param("rex_symbolic_record", "argument");
  AstInterface::AstNodeList actualArguments{AstNodePtr(memberExpression)};
  ReplaceParams replacements(interface, parameters, actualArguments);
  SymbolicVal member = replacements.find("argument");
  requireFunction(member, AstInterface::BOP_DOT_ACCESS, ".", 2);

  SgExpression *variableExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_variable"));
  ParameterDeclaration targetParameters;
  targetParameters.add_param("int", "target");
  AstInterface::AstNodeList targetArguments{AstNodePtr(variableExpression)};
  ReplaceParams targetReplacements(interface, targetParameters,
                                   targetArguments);
  SymbolicValDescriptor target(SymbolicVar("target", AST_NULL));
  targetReplacements.replace_target(target);
  requireExactTarget(target, variableExpression);

  RestrictValueOpDescriptor restriction;
  RestrictValueDescriptor restrictedTarget;
  restrictedTarget.first = SymbolicVar("target", AST_NULL);
  restriction.push_back(restrictedTarget);
  restriction.replace_val(targetReplacements);
  requireExactTarget(restriction.front().first, variableExpression);

  ArrayModifyDescriptor modification;
  modification.first.get_val() = SymbolicVar("target", AST_NULL);
  modification.replace_val(targetReplacements);
  requireExactTarget(modification.first, variableExpression);

  ArrayConstructDescriptor construction;
  construction.first.push_back(
      SymbolicValDescriptor(SymbolicVar("target", AST_NULL)));
  construction.replace_val(targetReplacements);
  requireExactTarget(construction.first.front(), variableExpression);

  SgExpression *addressExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_address"));
  auto *addressPointer = findUniqueExpressionNode<SgVarRefExp>(
      addressExpression, [](SgNode *node) { return isSgVarRefExp(node); });
  ROSE_ASSERT(readLocations(interface, addressExpression) ==
              std::set<SgNode *>{addressPointer});
  auto *dereference = findUniqueExpressionNode<SgPointerDerefExp>(
      returnExpression(
          findFunction(project, "rex_symbolic_annotation_dereference")),
      [](SgNode *node) { return isSgPointerDerefExp(node); });
  auto *dereferencePointer = findUniqueExpressionNode<SgVarRefExp>(
      dereference, [](SgNode *node) { return isSgVarRefExp(node); });
  std::set<SgNode *> dereferenceReads{dereference, dereferencePointer};
  ROSE_ASSERT(readLocations(interface, dereference) == dereferenceReads);
  for (const auto &[functionName, referenceCount] :
       {std::pair<const char *, size_t>{
            "rex_symbolic_annotation_conditional_address", 3},
        {"rex_symbolic_annotation_comma_address", 2},
        {"rex_symbolic_annotation_binary_conditional_address", 2}}) {
    auto *expression = returnExpression(findFunction(project, functionName));
    const auto references = NodeQuery::querySubTree(expression, V_SgVarRefExp);
    ROSE_ASSERT(references.size() == referenceCount);
    std::set<SgNode *> expectedReads(references.begin(), references.end());
    for (SgNode *node :
         NodeQuery::querySubTree(expression, V_SgConditionalExp)) {
      auto *conditional = isSgConditionalExp(node);
      conditional->validate();
      if (conditional->get_operator_kind() ==
          SgConditionalExp::e_conditional_operator_gnu_binary) {
        ROSE_ASSERT(isSgPointerDerefExp(conditional->get_conditional_exp()) !=
                    nullptr);
        expectedReads.insert(conditional->get_conditional_exp());
      }
    }
    ROSE_ASSERT(readLocations(interface, expression) == expectedReads);
  }
  SgExpression *arrayExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_array_access"));
  SgExpression *arrayElement = stripSemanticConversions(arrayExpression);
  ROSE_ASSERT(isSgPntrArrRefExp(arrayElement) != nullptr);
  std::set<SgNode *> arrayReads{arrayElement};
  for (SgNode *node : NodeQuery::querySubTree(arrayExpression, V_SgVarRefExp))
    arrayReads.insert(node);
  ROSE_ASSERT(arrayReads.size() == 4);
  ROSE_ASSERT(readLocations(interface, arrayExpression) == arrayReads);
  SymbolicVal address = SymbolicValGenerator::GetSymbolicVal(
      interface, AstNodePtr(addressExpression));
  std::string variableName;
  ROSE_ASSERT(address.isVar(variableName));
  ROSE_ASSERT(variableName == "pointer");

  SgExpression *conversionExpression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_conversion"));
  SymbolicVal conversion = SymbolicValGenerator::GetSymbolicVal(
      interface, AstNodePtr(conversionExpression));
  AstInterface::OperatorEnum conversionOperation = AstInterface::OP_NONE;
  std::vector<SymbolicVal> conversionArguments;
  ROSE_ASSERT(conversion.isFunction(&conversionOperation, nullptr,
                                    &conversionArguments));
  ROSE_ASSERT(conversionOperation == AstInterface::UOP_SEMANTIC_CONVERSION);
  ROSE_ASSERT(conversionArguments.size() == 1);
  SgCastExp *rebuiltConversion =
      isSgCastExp(conversion.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuiltConversion != nullptr);
  rebuiltConversion->validate_semantic_conversion();
  ROSE_ASSERT(rebuiltConversion->get_semantic_conversion_kind() ==
              SgCastExp::e_semantic_conversion_IntegralToFloating);

  SymbolicFunction named(AstInterface::OP_NONE, "rex_symbolic_helper",
                         SymbolicVal(1));
  SymbolicVal namedClone = named.cloneFunction({SymbolicVal(2)});
  requireFunction(namedClone, AstInterface::OP_NONE, "rex_symbolic_helper", 1);

  SymbolicFunction modulo(AstInterface::BOP_MOD, "%", SymbolicVal(5),
                          SymbolicVal(2));
  SymbolicVal moduloClone =
      modulo.cloneFunction({SymbolicVal(9), SymbolicVal(4)});
  requireFunction(moduloClone, AstInterface::BOP_MOD, "%", 2);
  for (const auto &[numerator, denominator, expected] :
       {std::tuple<int, int, int>{5, 2, 2},
        {-5, 2, -2},
        {5, -2, -2},
        {-5, -2, 2}}) {
    int quotient;
    ROSE_ASSERT(SymbolicValGenerator::GetSymbolicVal(
                    AstInterface::BOP_DIVIDE,
                    {SymbolicVal(numerator), SymbolicVal(denominator)})
                    .isConstInt(quotient) &&
                quotient == expected);
  }
  for (const auto &[numerator, denominator] :
       {std::pair<int, int>{1, 0}, {std::numeric_limits<int>::min(), -1}})
    requireFunction(SymbolicValGenerator::GetSymbolicVal(
                        AstInterface::BOP_DIVIDE,
                        {SymbolicVal(numerator), SymbolicVal(denominator)}),
                    AstInterface::BOP_DIVIDE, "/", 2);
  requireFunction(SymbolicValGenerator::GetSymbolicVal(
                      AstInterface::UOP_BIT_COMPLEMENT, {SymbolicVal(1)}),
                  AstInterface::UOP_BIT_COMPLEMENT, "~", 1);

  ROSE_ASSERT(isSgModOp(rebuildAnnotationArgument(
                  interface,
                  returnExpression(findFunction(
                      project, "rex_symbolic_annotation_modulo")))) != nullptr);
  ROSE_ASSERT(
      isSgBitAndOp(rebuildAnnotationArgument(
          interface, returnExpression(findFunction(
                         project, "rex_symbolic_annotation_bit_and")))) !=
      nullptr);
  ROSE_ASSERT(isSgBitOrOp(rebuildAnnotationArgument(
                  interface,
                  returnExpression(findFunction(
                      project, "rex_symbolic_annotation_bit_or")))) != nullptr);
  ROSE_ASSERT(isSgBitComplementOp(rebuildAnnotationArgument(
                  interface,
                  returnExpression(findFunction(
                      project, "rex_symbolic_annotation_bit_complement")))) !=
              nullptr);
  SgPlusPlusOp *prefixIncrement = findUniqueExpressionNode<SgPlusPlusOp>(
      rebuildAnnotationArgument(
          interface, returnExpression(findFunction(
                         project, "rex_symbolic_annotation_prefix_increment"))),
      [](SgNode *node) { return isSgPlusPlusOp(node); });
  ROSE_ASSERT(prefixIncrement->get_mode() == SgUnaryOp::Sgop_mode::prefix);
  SgMinusMinusOp *postfixDecrement = findUniqueExpressionNode<SgMinusMinusOp>(
      rebuildAnnotationArgument(
          interface,
          returnExpression(findFunction(
              project, "rex_symbolic_annotation_postfix_decrement"))),
      [](SgNode *node) { return isSgMinusMinusOp(node); });
  ROSE_ASSERT(postfixDecrement->get_mode() == SgUnaryOp::Sgop_mode::postfix);
  ROSE_ASSERT(
      isSgNewExp(rebuildAnnotationArgument(
          interface, returnExpression(findFunction(
                         project, "rex_symbolic_annotation_allocation")))) !=
      nullptr);
  SgExpression *observableZeroProduct = rebuildAnnotationArgument(
      interface,
      returnExpression(findFunction(
          project, "rex_symbolic_annotation_observable_zero_product")));
  (void)findUniqueExpressionNode<SgMultiplyOp>(
      observableZeroProduct, [](SgNode *node) { return isSgMultiplyOp(node); });
  SgPlusPlusOp *postfixIncrement = findUniqueExpressionNode<SgPlusPlusOp>(
      observableZeroProduct, [](SgNode *node) { return isSgPlusPlusOp(node); });
  ROSE_ASSERT(postfixIncrement->get_mode() == SgUnaryOp::Sgop_mode::postfix);
  SgExpression *volatileZeroProduct = rebuildAnnotationArgument(
      interface,
      returnExpression(findFunction(
          project, "rex_symbolic_annotation_volatile_zero_product")));
  (void)findUniqueExpressionNode<SgMultiplyOp>(
      volatileZeroProduct, [](SgNode *node) { return isSgMultiplyOp(node); });
  SgPointerDerefExp *volatileRead = findUniqueExpressionNode<SgPointerDerefExp>(
      volatileZeroProduct,
      [](SgNode *node) { return isSgPointerDerefExp(node); });
  ROSE_ASSERT(SageInterface::isVolatileType(volatileRead->get_type()));
  requireTypedReplacement(project);
  return 0;
}

int checkUnsignedMinus(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_unsigned_minus"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "unsigned");
  requireFunction(replacement, AstInterface::UOP_MINUS, "-", 1);
  SgMinusOp *rebuilt = isSgMinusOp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuilt->get_type(),
                                              expression->get_type()));
  return 0;
}

int checkFloatingGrouping(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_floating_group"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "double");
  AstNodePtr exactExpression;
  ROSE_ASSERT(replacement.isAstWrap(exactExpression));
  ROSE_ASSERT(exactExpression.get_ptr() == expression);
  SgAddOp *rebuilt = isSgAddOp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_rhs_operand()) != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_lhs_operand()) == nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuilt->get_type(),
                                              expression->get_type()));
  return 0;
}

int checkArrayAccess(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_array_access"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "int");
  requireFunction(replacement, AstInterface::OP_ARRAY_ACCESS, "[]", 2);
  SgPntrArrRefExp *rebuiltOuter =
      isSgPntrArrRefExp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuiltOuter != nullptr);
  SgPntrArrRefExp *rebuiltInner = isSgPntrArrRefExp(
      stripSemanticConversions(rebuiltOuter->get_lhs_operand()));
  SgVarRefExp *array = rebuiltInner == nullptr
                           ? nullptr
                           : isSgVarRefExp(stripSemanticConversions(
                                 rebuiltInner->get_lhs_operand()));
  SgVarRefExp *row = rebuiltInner == nullptr
                         ? nullptr
                         : isSgVarRefExp(stripSemanticConversions(
                               rebuiltInner->get_rhs_operand()));
  SgVarRefExp *column =
      isSgVarRefExp(stripSemanticConversions(rebuiltOuter->get_rhs_operand()));
  ROSE_ASSERT(array != nullptr && row != nullptr && column != nullptr);
  ROSE_ASSERT(array->get_symbol()->get_name() == "array");
  ROSE_ASSERT(row->get_symbol()->get_name() == "row");
  ROSE_ASSERT(column->get_symbol()->get_name() == "column");
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuiltOuter->get_type(),
                                              expression->get_type()));
  return 0;
}

int checkComplexFloatingGrouping(int argc, char **argv) {
  SgProject *project = parseProject(argc, argv);
  ROSE_ASSERT(project != nullptr);
  SgExpression *expression = returnExpression(
      findFunction(project, "rex_symbolic_annotation_complex_floating_group"));
  AstInterfaceImpl implementation(expression);
  AstInterface interface(&implementation);
  SymbolicVal replacement =
      replaceAnnotationArgument(interface, expression, "_Complex double");
  AstNodePtr exactExpression;
  ROSE_ASSERT(replacement.isAstWrap(exactExpression));
  ROSE_ASSERT(exactExpression.get_ptr() == expression);
  SgAddOp *rebuilt = isSgAddOp(replacement.CodeGen(interface).get_ptr());
  ROSE_ASSERT(rebuilt != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_rhs_operand()) != nullptr);
  ROSE_ASSERT(isSgAddOp(rebuilt->get_lhs_operand()) == nullptr);
  ROSE_ASSERT(isSgTypeComplex(
                  rebuilt->get_type()->stripTypedefsAndModifiers()) != nullptr);
  ROSE_ASSERT(SageInterface::isEquivalentType(rebuilt->get_type(),
                                              expression->get_type()));
  return 0;
}

[[noreturn]] void rejectOperatorArity() {
  (void)SymbolicValGenerator::GetSymbolicVal(AstInterface::BOP_PLUS,
                                             {SymbolicVal(1)});
  ROSE_ABORT();
}

[[noreturn]] void rejectReplacementArity() {
  AstInterface interface(nullptr);
  ParameterDeclaration parameters;
  parameters.add_param("int", "argument");
  AstInterface::AstNodeList arguments;
  (void)ReplaceParams(interface, parameters, arguments);
  ROSE_ABORT();
}

[[noreturn]] void rejectUntypedAllocation() {
  (void)SymbolicValGenerator::GetSymbolicVal(AstInterface::UOP_ALLOCATE,
                                             {SymbolicVal(1)});
  ROSE_ABORT();
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--reject-operator-arity")
    rejectOperatorArity();
  if (argc == 2 && std::string(argv[1]) == "--reject-replacement-arity")
    rejectReplacementArity();
  if (argc == 2 && std::string(argv[1]) == "--reject-untyped-allocation")
    rejectUntypedAllocation();
  if (argc >= 2 && std::string(argv[1]) == "--check-unsigned-minus")
    return checkUnsignedMinus(argc - 1, argv + 1);
  if (argc >= 2 && std::string(argv[1]) == "--check-floating-grouping")
    return checkFloatingGrouping(argc - 1, argv + 1);
  if (argc >= 2 && std::string(argv[1]) == "--check-array-access")
    return checkArrayAccess(argc - 1, argv + 1);
  if (argc >= 2 && std::string(argv[1]) == "--check-complex-floating-grouping")
    return checkComplexFloatingGrouping(argc - 1, argv + 1);
  return exerciseContracts(argc, argv);
}
