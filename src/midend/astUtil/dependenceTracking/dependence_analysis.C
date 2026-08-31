#include "sage3basic.h"

#include <sstream>

#include <string>

#include "dependence_analysis.h"

#include "AstInterface.h"

#include "CommandOptions.h"

namespace AstUtilInterface {

void WholeProgramDependenceAnalysis::CollectPastResults(
    std::istream &dep_file, std::istream *annot_file) {
  Log.push("Collect past results of dependence analysis");
  main_table.CollectFromFile(dep_file);
  if (annot_file != 0) {
    Log.push("Reading existing dependence table as annotations.");
    AstUtilInterface::RegisterOperatorSideEffectAnnotation();
    AstUtilInterface::ReadAnnotations(*annot_file, &annot_table);
    Log.push("Done reading existing whole application dependence table.");
  }
  Log.push("Done collecting past results of dependence analysis");
}

WholeProgramDependenceAnalysis::WholeProgramDependenceAnalysis(
    int argc, const char **argv)
    : main_table(false), annot_table(true) {
  std::vector<std::string> argvList(argv, argv + argc);
  CmdOptions::GetInstance()->SetOptions(argvList);
  sageProject = new SgProject(argvList);
}

void WholeProgramDependenceAnalysis::ComputeDependences() {
  Log.push("Compute dependences.");
  if (sageProject == 0) {
    return;
  }
  int filenum = sageProject->numberOfFiles();
  for (int i = 0; i < filenum; ++i) {
    SgSourceFile *sageFile = isSgSourceFile(sageProject->get_fileList()[i]);
    ROSE_ASSERT(sageFile != NULL);

    std::string fname = sageFile->get_file_info()->get_raw_filename();
    Log.push("Targeting file:" + fname);

    SgGlobal *root = sageFile->get_globalScope();
    ROSE_ASSERT(root != NULL);

    SgDeclarationStatementPtrList declList = root->get_declarations();
    for (SgDeclarationStatementPtrList::iterator p = declList.begin();
         p != declList.end(); ++p) {
      SgNode *func = *p;
      if (func == 0)
        continue;
      Log.push("Analyzing declaration " + func->unparseToString() + " in " +
               fname);
      ComputeDependences(func, root);
    }
  }
}

void WholeProgramDependenceAnalysis::OutputDependences(std::ostream &output) {
  if (CmdOptions::GetInstance()->HasOption("-data")) {
    main_table.OutputDataDependences(output);
  } else {
    main_table.OutputDependences(output);
  }
}

void WholeProgramDependenceAnalysis::OutputAnnotations(std::ostream &output) {
  annot_table.OutputDependences(output);
}

void WholeProgramDependenceAnalysis::ComputeDependences(SgNode *input,
                                                        SgNode *root) {
  DebugLog DebugSaveDep("-debugdep");
  std::string function_name;
  AstInterface::AstNodeList params, children;
  AstNodePtr body;
  if (AstInterface::IsFunctionDefinition(input, &function_name, &params, 0,
                                         &body, 0, 0,
                                         /*use_global_name*/ true) &&
      body != 0) {
    Log.push("Computing dependences for " + input->unparseToString());
    std::function<bool(const AstNodePtr &, const AstNodePtr &,
                       AstUtilInterface::OperatorSideEffect)>
        save_dep = [this, input, body, &DebugSaveDep](
                       const AstNodePtr &first, const AstNodePtr &second,
                       AstUtilInterface::OperatorSideEffect relation) {
          DebugSaveDep([&relation]() {
            return "saving for:" +
                   AstUtilInterface::OperatorSideEffectName(relation);
          });
          SgNode *details = second.get_ptr();
          switch (relation) {
          case AstUtilInterface::OperatorSideEffect::Decl:
          case AstUtilInterface::OperatorSideEffect::Free: {
            SgType *t = AstInterface::GetExpressionType(first).get_ptr();
            assert(t != 0);
            details = t;
            break;
          }
          case AstUtilInterface::OperatorSideEffect::Read: {
            // If the detail is the surrounding statment, skip.
            if (AstInterface::IsStatement(second)) {
              details = 0;
            }
            break;
          }
          case AstUtilInterface::OperatorSideEffect::Kill:
            return true;
          default:
            break;
          }
          DebugSaveDep([&first, details]() {
            return "saving side effect for:" +
                   AstInterface::AstToString(first) + " = " +
                   AstInterface::AstToString(details);
          });
          if (!main_table.SaveOperatorSideEffect(input, first, relation,
                                                 details)) {
            DebugSaveDep([]() { return "Did not save dependene"; });
          }
          return true;
        };
    AstUtilInterface::ComputeAstSideEffects(input, &save_dep, &annot_table);
  }
  if (AstInterface::IsBlock(input, 0, &children)) {
    for (AstInterface::AstNodeList::const_iterator p = children.begin();
         p != children.end(); ++p) {
      AstNodePtr current = *p;
      ComputeDependences(current.get_ptr(), root);
    }
  } else if (AstInterface::IsVariableDecl(input)) {
    SgVariableDeclaration *declaration = isSgVariableDeclaration(input);
    if (declaration == nullptr) {
      std::cerr << "REX_DEPENDENCE_INVARIANT[variable-declaration]: "
                << input->class_name()
                << " was classified as a declaration but is not an exact "
                   "SgVariableDeclaration"
                << std::endl;
      ROSE_ABORT();
    }
    SgScopeStatement *declaration_scope = declaration->get_scope();
    if (declaration_scope == nullptr) {
      std::cerr << "REX_DEPENDENCE_INVARIANT[initializer-scope]: declaration="
                << declaration->unparseToString() << " has no scope"
                << std::endl;
      ROSE_ABORT();
    }
    if (isSgGlobal(declaration_scope) == nullptr &&
        isSgNamespaceDefinitionStatement(declaration_scope) == nullptr)
      return;

    SgScopeStatement *owning_scope = declaration_scope;
    while (isSgNamespaceDefinitionStatement(owning_scope) != nullptr)
      owning_scope = owning_scope->get_scope();
    if (owning_scope == nullptr || owning_scope != root ||
        isSgGlobal(owning_scope) == nullptr) {
      std::cerr << "REX_DEPENDENCE_INVARIANT[initializer-root]: declaration="
                << declaration->unparseToString()
                << " scope=" << declaration_scope->class_name()
                << " root=" << root << std::endl;
      ROSE_ABORT();
    }
    for (SgInitializedName *initialized_name : declaration->get_variables()) {
      ASSERT_not_null(initialized_name);
      SgInitializer *initializer = initialized_name->get_initializer();
      if (initializer == nullptr)
        continue;

      const AstNodePtr variable(initialized_name);
      const std::string variable_signature =
          AstInterface::GetVariableSignature(variable);
      if (variable_signature.empty() ||
          variable_signature.find("_UNKNOWN_") != std::string::npos) {
        std::cerr << "REX_DEPENDENCE_INVARIANT[initializer-target]: "
                  << initialized_name->class_name()
                  << " has no exact variable signature" << std::endl;
        ROSE_ABORT();
      }

      std::function<bool(const AstNodePtr &, const AstNodePtr &,
                         AstUtilInterface::OperatorSideEffect)>
          save_initializer_input =
              [this, &variable](const AstNodePtr &referenced_entity,
                                const AstNodePtr &details,
                                AstUtilInterface::OperatorSideEffect relation) {
                switch (relation) {
                case AstUtilInterface::OperatorSideEffect::Read:
                case AstUtilInterface::OperatorSideEffect::ReadUnknown:
                case AstUtilInterface::OperatorSideEffect::Call:
                case AstUtilInterface::OperatorSideEffect::CallUnknown:
                  if (referenced_entity == AST_NULL) {
                    std::cerr << "REX_DEPENDENCE_INVARIANT[initializer-input]: "
                                 "typed initializer traversal produced a null "
                                 "referenced entity"
                              << std::endl;
                    ROSE_ABORT();
                  }
                  main_table.SaveOperatorSideEffect(
                      variable.get_ptr(), referenced_entity,
                      AstUtilInterface::OperatorSideEffect::Init,
                      details.get_ptr());
                  break;
                default:
                  break;
                }
                return true;
              };
      AstUtilInterface::ComputeAstSideEffects(initializer,
                                              &save_initializer_input,
                                              /*add_to_dep_analysis=*/nullptr);
    }
  }
}

}; // namespace AstUtilInterface
