#include "AstInterface.h"

#include "AstInterface_ROSE.h"

#include "AstUtilInterface.h"

#include "CommandOptions.h"

#include "StmtInfoCollect.h"

#include "SymbolicVal.h"

#include "annotation/OperatorAnnotation.h"

#include "dependenceTracking/dependence_analysis.h"

#include "sage3basic.h"

#include <map>

#include <sstream>

DebugLog DebugAstUtil("-debugastutil");

namespace AstUtilInterface {
class VariableSignatureDictionary {
private:
  static DependenceTable *dict;
  static bool do_save;

public:
  static void set_doit(bool doit) {
    if (doit && dict == 0) {
      dict = new DependenceTable(/*update annotations=*/false);
    }
    do_save = doit;
  }

  static bool do_it() { return do_save; }
  static DependenceTable *get_dictionary() { return dict; }
};

DependenceTable *VariableSignatureDictionary::dict = 0;
bool VariableSignatureDictionary::do_save = false;
} // namespace AstUtilInterface

void AstUtilInterface::SetSaveVariableDictionary(bool doit) {
  VariableSignatureDictionary::set_doit(doit);
}

void AstUtilInterface::ComputeAstSideEffects(
    SgNode *ast,
    std::function<bool(const AstNodePtr &, const AstNodePtr &,
                       AstUtilInterface::OperatorSideEffect)> *collect,
    SaveOperatorSideEffectInterface *add_to_dep_analysis) {
  AstInterfaceImpl astImpl(ast);
  AstInterface fa(&astImpl);

  OperatorSideEffectAnnotation *funcAnnot =
      OperatorSideEffectAnnotation::get_inst();
  assert(funcAnnot != 0);
  DebugAstUtil([&ast]() {
    return "ComputeAstSideEffect: " + AstInterface::AstToString(ast);
  });
  bool do_annot = false;
  bool done_annot_mod = false;
  bool done_annot_read = false;
  bool done_annot_call = false;
  AstInterface::AstNodePtr body;
  AstInterface::AstNodeList ast_params;
  AstInterface::AstTypeList ast_param_types;
  if (AstInterface::IsFunctionDefinition(ast, 0, &ast_params, 0, &body,
                                         &ast_param_types, 0,
                                         /*use_global_unique_name=*/false,
                                         /*skip_function_declaration=*/true) &&
      body != AST_NULL) {
    DebugAstUtil([&ast]() {
      return "Saving side effects for :" + AstInterface::AstToString(ast) +
             "\n";
    });
    if (add_to_dep_analysis != 0) {
      add_to_dep_analysis->ClearOperatorSideEffect(ast);
      assert(ast_params.size() == ast_param_types.size());
      auto pt = ast_param_types.begin();
      for (const auto &p : ast_params) {
        add_to_dep_analysis->SaveOperatorSideEffect(
            ast, p, OperatorSideEffect::Parameter, (*pt).get_ptr());
        pt++;
      }
    }
    do_annot = true;
  }

  StmtSideEffectCollect collect_operator(fa, funcAnnot);
  std::map<std::string, AstNodePtr> alias_map;
  std::function<bool(AstNodePtr, AstNodePtr)> save_alias =
      [&collect, &alias_map](AstNodePtr first, AstNodePtr second) {
        DebugAstUtil([&first, &second]() {
          return "save alias:" + AstInterface::GetVariableSignature(first) +
                 "->" + AstInterface::GetVariableSignature(second);
        });
        alias_map[AstInterface::GetVariableSignature(first)] = second;
        if (collect != 0) {
          (*collect)(first, second, OperatorSideEffect::Alias);
        }
        return true;
      };
  auto save_memory_ref = [&alias_map, &collect, &ast, do_annot, &body,
                          add_to_dep_analysis](AstNodePtr ref,
                                               AstNodePtr details,
                                               OperatorSideEffect what) {
    bool done_annot = false;
    AstNodeList subrefs;
    if (!ref.is_unknown() && !AstInterface::IsMemoryAccess(ref, &subrefs)) {
      DebugAstUtil([&ref]() {
        return "Do not save non-memory-access ref:" +
               AstInterface::AstToString(ref);
      });
      return false;
    }
    if (subrefs.empty()) {
      subrefs.push_back(ref);
    }
    for (AstNodePtr &subref : subrefs) {
      AstNodePtr memory_ref = subref;
      AstNodePtr array;
      bool is_unknown_ref = false;
      bool is_local_ref =
          (do_annot)
              ? AstInterface::IsLocalRef(memory_ref, body, &is_unknown_ref)
              : true;
      if (AstInterface::IsArrayAccess(memory_ref, &array)) {
        memory_ref = array;
        is_local_ref = false;
        DebugAstUtil([&memory_ref]() {
          return "Finding array reference:" +
                 AstInterface::AstToString(memory_ref);
        });
      }
      auto ref_aliased =
          alias_map.find(AstInterface::GetVariableSignature(memory_ref));
      if (ref_aliased != alias_map.end()) {
        memory_ref = AstNodePtr((*ref_aliased).second);
        is_local_ref = false;
        DebugAstUtil([&memory_ref]() {
          return "Finding aliased reference:" +
                 AstInterface::AstToString(memory_ref);
        });
      } else {
        if (is_unknown_ref) {
          memory_ref.set_is_unknown_reference();
        }
      }
      if (collect != 0) {
        (*collect)(memory_ref, details, what);
      }
      if (do_annot &&
          (memory_ref.is_unknown() || is_unknown_ref || !is_local_ref)) {
        DebugAstUtil([&memory_ref]() {
          return "save annotation:" + AstInterface::AstToString(memory_ref);
        });
        if (add_to_dep_analysis != 0) {
          add_to_dep_analysis->SaveOperatorSideEffect(ast, memory_ref, what,
                                                      details.get_ptr());
        }
        AddOperatorSideEffectAnnotation(ast, memory_ref, what);
        done_annot = true;
      }
    }
    return done_annot;
  };
  std::function<bool(AstNodePtr, AstNodePtr)> save_mod =
      [&done_annot_mod, &save_memory_ref](AstNodePtr first, AstNodePtr second) {
        if (save_memory_ref(first, second, OperatorSideEffect::Modify)) {
          DebugAstUtil([]() { return "Done mod annotation."; });
          done_annot_mod = true;
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> save_read =
      [&save_memory_ref, &done_annot_read](AstNodePtr first,
                                           AstNodePtr second) {
        if (save_memory_ref(first, second, OperatorSideEffect::Read)) {
          DebugAstUtil([]() { return "Done read annotation."; });
          done_annot_read = true;
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> save_kill =
      [&collect](AstNodePtr first, AstNodePtr second) {
        if (collect != 0) {
          return (*collect)(first, second, OperatorSideEffect::Kill);
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> save_call =
      [&collect, &ast, do_annot, &done_annot_call, &body,
       add_to_dep_analysis](AstNodePtr first, AstNodePtr second) {
        if (do_annot &&
            (first.is_unknown() || !AstInterface::IsLocalRef(first, body))) {
          done_annot_call = true;
          if (add_to_dep_analysis != 0) {
            add_to_dep_analysis->SaveOperatorSideEffect(
                ast, GetVariableSignature(first), OperatorSideEffect::Call,
                second.get_ptr());
          }
          AddOperatorSideEffectAnnotation(ast, first, OperatorSideEffect::Call);
        }
        DebugAstUtil([&first]() {
          return "save call:" + AstInterface::AstToString(first);
        });
        if (collect != 0) {
          (*collect)(first, second, OperatorSideEffect::Call);
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> save_decl =
      [&collect](AstNodePtr var, AstNodePtr init) {
        DebugAstUtil([&var]() {
          return "save new decl:" + AstInterface::AstToString(var);
        });
        if (collect != 0) {
          (*collect)(var, init, OperatorSideEffect::Decl);
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> save_allocate =
      [&collect](AstNodePtr op, AstNodePtr init) {
        DebugAstUtil([&op, &init]() {
          return "save allocate:" + AstInterface::AstToString(op) + ":" +
                 AstInterface::AstToString(init);
        });
        if (collect != 0) {
          return (*collect)(op, init, OperatorSideEffect::Allocate);
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> save_free =
      [&collect](AstNodePtr var, AstNodePtr init) {
        DebugAstUtil(
            [&var]() { return "save free:" + AstInterface::AstToString(var); });
        if (collect != 0) {
          (*collect)(var, init, OperatorSideEffect::Free);
        }
        return true;
      };
  collect_operator.set_modify_collect(save_mod);
  collect_operator.set_alias_collect(save_alias);
  collect_operator.set_read_collect(save_read);
  collect_operator.set_kill_collect(save_kill);
  collect_operator.set_call_collect(save_call);
  collect_operator.set_new_var_collect(save_decl);
  collect_operator.set_allocate_collect(save_allocate);
  collect_operator.set_free_collect(save_free);
  collect_operator(ast);
  if (do_annot) {
    if (!done_annot_mod) {
      if (add_to_dep_analysis != 0) {
        add_to_dep_analysis->SaveOperatorSideEffect(
            ast, AST_NULL, OperatorSideEffect::Modify, 0);
      }
      AddOperatorSideEffectAnnotation(ast, AST_NULL,
                                      OperatorSideEffect::Modify);
    }
    if (!done_annot_read) {
      if (add_to_dep_analysis != 0) {
        add_to_dep_analysis->SaveOperatorSideEffect(
            ast, AST_NULL, OperatorSideEffect::Read, 0);
      }
      AddOperatorSideEffectAnnotation(ast, AST_NULL, OperatorSideEffect::Read);
    }
    if (!done_annot_call) {
      if (add_to_dep_analysis != 0) {
        add_to_dep_analysis->SaveOperatorSideEffect(
            ast, AST_NULL, OperatorSideEffect::Call, 0);
      }
      AddOperatorSideEffectAnnotation(ast, AST_NULL, OperatorSideEffect::Call);
    }
  }
}

void AstUtilInterface::ReadAnnotations(std::istream &input,
                                       DependenceTable *use_dep_analysis) {
  if (use_dep_analysis != 0) {
    use_dep_analysis->CollectFromFile(input);
  } else {
    ReadAnnotation::get_inst()->read(input);
  }
}

void AstUtilInterface::OutputOperatorSideEffectAnnotations(
    std::ostream &output, DependenceTable *use_dep_analysis) {
  if (use_dep_analysis != 0) {
    use_dep_analysis->OutputDependences(output);
  } else {
    OperatorSideEffectAnnotation *funcAnnot =
        OperatorSideEffectAnnotation::get_inst();
    funcAnnot->write(output);
  }
}

void AstUtilInterface::RegisterOperatorSideEffectAnnotation() {
  OperatorSideEffectAnnotation *funcAnnot =
      OperatorSideEffectAnnotation::get_inst();
  funcAnnot->register_annot();
};

void AstUtilInterface::AddOperatorSideEffectAnnotation(
    SgNode *op_ast, const AstNodePtr &var,
    AstUtilInterface::OperatorSideEffect relation) {
  DebugAstUtil([&]() {
    return "Adding operator annotation: " + OperatorSideEffectName(relation) +
           ":" + "var is : " + AstInterface::AstToString(var);
  });
  if (!AstInterface::IsFunctionDefinition(op_ast)) {
    DebugAstUtil([&]() {
      return "Expecting an operator but getting " +
             AstInterface::AstToString(op_ast);
    });
    return;
  }
  AstInterfaceImpl astImpl(op_ast);
  AstInterface fa(&astImpl);

  OperatorSideEffectAnnotation *funcAnnot =
      OperatorSideEffectAnnotation::get_inst();
  OperatorSideEffectDescriptor *desc = 0;
  switch (relation) {
  case OperatorSideEffect::Modify:
    desc = funcAnnot->get_modify_descriptor(fa, op_ast, true);
    break;
  case OperatorSideEffect::Read:
    desc = funcAnnot->get_read_descriptor(fa, op_ast, true);
    break;
  case OperatorSideEffect::Call:
    desc = funcAnnot->get_call_descriptor(fa, op_ast, true);
    break;
  case OperatorSideEffect::Kill:
  case OperatorSideEffect::Decl:
  case OperatorSideEffect::Allocate:
  case OperatorSideEffect::Free:
    break;
  default:
    std::cerr << "Unexpected relation: " << relation << "\n";
    assert(0);
  }
  if (desc != 0 && !var.is_null()) {
    if (var.is_unknown() || var.is_unknown_reference() ||
        var.is_unknown_function_call()) {
      desc->set_has_unknown(true);
    }
    std::string varname = GetVariableSignature(var);
    SymbolicValDescriptor val_desc(
        SymbolicValGenerator::GetSymbolicVal(fa, var), varname);
    desc->push_back(val_desc);
    DebugAstUtil([&relation, &val_desc]() {
      return "Done adding operator annotation: " +
             OperatorSideEffectName(relation) + ":" +
             "annotation is : " + val_desc.toString();
    });
  }
}

bool AstUtilInterface::IsLocalRef(SgNode *ref, SgNode *scope,
                                  bool *has_ptr_deref) {
  if (ref == 0 || scope == 0)
    return false;
  return AstInterface::IsLocalRef(AstNodePtr(ref), AstNodePtr(scope),
                                  has_ptr_deref);
}

std::string AstUtilInterface::GetVariableSignature(const AstNodePtr &variable) {
  auto sig = AstInterface::GetVariableSignature(variable);
  auto *dict_table = VariableSignatureDictionary::get_dictionary();
  if (VariableSignatureDictionary::do_it() && dict_table != 0 &&
      variable.get_ptr() != 0) {
    std::string filename;
    int lineno = -1;
    AstInterfaceImpl astImpl(variable.get_ptr());
    AstInterface fa(&astImpl);
    if (fa.get_fileInfo(variable, &filename, &lineno)) {
      std::stringstream loc;
      loc << "Line:" << lineno;
      DependenceEntry e(sig, filename, "", loc.str());
      dict_table->SaveDependence(e);
    }
  }
  return sig;
}

void AstUtilInterface::OutputSignatureDictionary(std::ostream &output) {
  auto *dict_table = VariableSignatureDictionary::get_dictionary();
  if (dict_table != 0) {
    dict_table->OutputDependences(output);
  }
}

void AstUtilInterface::SetFunctionNameMangling(
    std::string (*f)(const SgFunctionDeclaration *)) {
  AstInterface::SetFunctionNameMangling(f);
}
