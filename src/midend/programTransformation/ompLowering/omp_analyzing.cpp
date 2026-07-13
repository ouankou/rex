#include "omp_lowering.h"

#include "sage3basic.h"

using namespace std;
using namespace SageBuilder;
using namespace SageInterface;

namespace OmpSupport {

bool isOmpContextSelectorMetadataDirective(const SgNode *node) {
  for (const SgNode *owner = node; owner != nullptr;
       owner = owner->get_parent()) {
    if (isSgOmpContextSelector(owner) != nullptr) {
      return true;
    }
  }
  return false;
}

namespace {
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

bool shouldSkipImplicitDataSharingVar(const SgInitializedName *init_var) {
  if (init_var == nullptr) {
    return true;
  }
  if (const Sg_File_Info *fi = init_var->get_file_info()) {
    if (fi->isCompilerGenerated() || fi->isTransformation()) {
      return true;
    }
  }

  // Clang may materialize predefined function-name expressions as internal
  // declarations. They are not user data-sharing candidates.
  const std::string name = init_var->get_name().getString();
  return name == "__PRETTY_FUNCTION__" || name == "__func__" ||
         name == "__FUNCTION__";
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

bool scheduleKindUsesImplicitChunkOne(
    SgOmpClause::omp_schedule_kind_enum schedule_kind) {
  return schedule_kind == SgOmpClause::e_omp_schedule_kind_dynamic ||
         schedule_kind == SgOmpClause::e_omp_schedule_kind_guided;
}
} // namespace

Rose_STL_Container<SgNode *>
mergeSgNodeList(Rose_STL_Container<SgNode *> node_list1,
                Rose_STL_Container<SgNode *> node_list2) {

  std::sort(node_list1.begin(), node_list1.end());
  std::sort(node_list2.begin(), node_list2.end());
  Rose_STL_Container<SgNode *> node_list;
  std::merge(node_list1.begin(), node_list1.end(), node_list2.begin(),
             node_list2.end(),
             std::insert_iterator<Rose_STL_Container<SgNode *>>(
                 node_list, node_list.end()));
  return node_list;
}

void analyzeOmpMetadirective(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpMetadirectiveStatement *target = isSgOmpMetadirectiveStatement(node);

  ROSE_ASSERT(target != NULL);

  SgFunctionDefinition *func_def = NULL;
  if (SageInterface::is_Fortran_language()) {
    func_def = getEnclosingFunctionDefinition(target);
    ROSE_ASSERT(func_def != NULL);
  }
  SgStatement *body = target->get_body();
  ROSE_ASSERT(body != NULL);
  AttachedPreprocessingInfoType save_buf1, save_buf2, save_buf_inside;
  cutPreprocessingInfo(target, PreprocessingInfo::before, save_buf1);
  cutPreprocessingInfo(target, PreprocessingInfo::after, save_buf2);

  cutPreprocessingInfo(target, PreprocessingInfo::inside, save_buf_inside);

  SgIfStmt *root_if_statement = NULL;
  SgStatement *variant_directive;
  SgStatement *variant_body = copyStatement(body);
  ROSE_ASSERT(variant_body != NULL);
  SgIfStmt *if_stmt = NULL;
  SgIfStmt *previous_if_stmt = NULL;
  auto requireRuntimeCondition = [](SgOmpWhenClause *when_clause) {
    if (when_clause == nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[metadirective]: null when "
                   "clause\n";
      ROSE_ABORT();
    }
    SgExpression *condition = nullptr;
    const SgOmpContextSelectorSetPtrList &sets =
        when_clause->get_context_selector_sets();
    if (sets.size() != 1) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[metadirective]: when clause "
                   "must own exactly one selector set\n";
      ROSE_ABORT();
    }
    for (SgOmpContextSelectorSet *set : sets) {
      if (set == nullptr ||
          set->get_set_kind() != SgOmpClause::e_omp_context_selector_set_user) {
        std::cerr << "REX_OMP_LOWERING_UNSUPPORTED[metadirective]: lowering "
                     "requires a sole user condition selector\n";
        ROSE_ABORT();
      }
      const SgOmpContextSelectorPtrList &selectors = set->get_selectors();
      SgOmpContextSelector *selector =
          selectors.size() == 1 ? selectors.front() : nullptr;
      const SgOmpContextSelectorPropertyPtrList *properties =
          selector != nullptr ? &selector->get_properties() : nullptr;
      SgOmpContextSelectorProperty *property =
          properties != nullptr && properties->size() == 1 ? properties->front()
                                                           : nullptr;
      if (selector == nullptr || selector->get_parent() != set ||
          selector->get_selector_kind() !=
              SgOmpClause::e_omp_context_trait_condition ||
          selector->get_score() != nullptr ||
          selector->get_construct_directive() != nullptr ||
          !selector->get_implementation_defined_name().is_null() ||
          property == nullptr || property->get_parent() != selector ||
          property->get_expression() == nullptr ||
          property->get_expression()->get_parent() != property ||
          property->get_context_kind() !=
              SgOmpClause::e_omp_when_context_kind_unknown ||
          property->get_context_vendor() !=
              SgOmpClause::e_omp_when_context_vendor_unspecified ||
          property->get_atomic_default_mem_order() !=
              SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified ||
          condition != nullptr) {
        std::cerr << "REX_OMP_LOWERING_UNSUPPORTED[metadirective]: user set "
                     "must contain one unscored condition\n";
        ROSE_ABORT();
      }
      condition = property->get_expression();
    }
    if (condition == nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[metadirective]: user condition "
                   "is absent\n";
      ROSE_ABORT();
    }
    SgExpression *copied_condition = SageInterface::copyExpression(condition);
    if (copied_condition == nullptr || copied_condition == condition ||
        copied_condition->get_parent() != nullptr) {
      std::cerr << "REX_OMP_LOWERING_INVARIANT[metadirective]: condition "
                   "copy is not a detached expression\n";
      ROSE_ABORT();
    }
    return copied_condition;
  };
  if (hasClause(target, V_SgOmpWhenClause)) {
    Rose_STL_Container<SgOmpClause *> clauses =
        getClause(target, V_SgOmpWhenClause);
    SgOmpWhenClause *when_clause = isSgOmpWhenClause(clauses[0]);
    SgExpression *condition_expression = requireRuntimeCondition(when_clause);
    SgExprStatement *condition_statement =
        buildExprStatement(condition_expression);
    variant_directive = when_clause->get_variant_directive();
    if (variant_directive) {
      SgOmpBodyStatement *variant_body_statement =
          isSgOmpBodyStatement(variant_directive);
      if (variant_body_statement == nullptr) {
        std::cerr << "REX_OMP_LOWERING_INVARIANT[metadirective]: variant "
                     "directive cannot own a body\n";
        ROSE_ABORT();
      }
      variant_body_statement->set_body(variant_body);
      setOneSourcePositionForTransformation(variant_directive);
      variant_body->set_parent(variant_directive);
      if_stmt = buildIfStmt(condition_statement, variant_directive, body);
    } else {
      if_stmt = buildIfStmt(condition_statement, variant_body, body);
    }
    root_if_statement = if_stmt;
    for (unsigned int i = 1; i < clauses.size(); i++) {
      previous_if_stmt = if_stmt;
      when_clause = isSgOmpWhenClause(clauses[i]);
      condition_expression = requireRuntimeCondition(when_clause);
      condition_statement = buildExprStatement(condition_expression);
      variant_directive = when_clause->get_variant_directive();
      variant_body = copyStatement(body);
      ROSE_ASSERT(variant_body != NULL);
      if (variant_directive) {
        SgOmpBodyStatement *variant_body_statement =
            isSgOmpBodyStatement(variant_directive);
        if (variant_body_statement == nullptr) {
          std::cerr << "REX_OMP_LOWERING_INVARIANT[metadirective]: variant "
                       "directive cannot own a body\n";
          ROSE_ABORT();
        }
        variant_directive->set_parent(target->get_parent());
        variant_body_statement->set_body(variant_body);
        setOneSourcePositionForTransformation(variant_directive);
        variant_body->set_parent(variant_directive);
        if_stmt = buildIfStmt(condition_statement, variant_directive, NULL);
      } else {
        if_stmt = buildIfStmt(condition_statement, body, NULL);
      }
      previous_if_stmt->set_false_body(if_stmt);
    }
  }

  SageInterface::replaceStatement(target, root_if_statement, true);
  pastePreprocessingInfo(root_if_statement, PreprocessingInfo::after,
                         save_buf2);
  pastePreprocessingInfo(root_if_statement, PreprocessingInfo::before,
                         save_buf1);
  // std::cout << root_if_statement->unparseToString() << "\n";
} // end analyze omp metadirective

void normalizeOmpLoop(SgStatement *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);
  switch (node->variantT()) {
  case V_SgOmpForStatement:
  case V_SgOmpDoStatement:
  case V_SgOmpTargetParallelForStatement:
  case V_SgOmpTargetTeamsDistributeParallelForStatement:
    break;
  default:
    return;
  }

  SgScopeStatement *p_scope = target->get_scope();
  ROSE_ASSERT(p_scope != NULL);
  SgStatement *loop = target->get_body();
  ROSE_ASSERT(loop != NULL);
  ROSE_ASSERT(isSgForStatement(loop) != NULL || isSgFortranDo(loop) != NULL);

  Rose_STL_Container<SgOmpClause *> clauses =
      getClause(target, V_SgOmpScheduleClause);
  if (clauses.size() != 0) {
    ROSE_ASSERT(clauses.size() == 1);
    SgOmpScheduleClause *s_clause = isSgOmpScheduleClause(clauses[0]);
    ROSE_ASSERT(s_clause);
    SgOmpClause::omp_schedule_kind_enum sg_kind = s_clause->get_kind();
    if (!s_clause->get_chunk_size() &&
        scheduleKindUsesImplicitChunkOne(sg_kind)) {
      SgExpression *chunk_size = buildIntVal(1);
      s_clause->set_chunk_size(chunk_size);
      chunk_size->set_parent(s_clause);
      SageInterface::publishGeneratedSubtreeOutputOwner(chunk_size, s_clause);
    }
  } else {
    SgOmpClause::omp_schedule_modifier_enum sg_modifier1 =
        SgOmpClause::e_omp_schedule_modifier_unspecified;
    SgOmpClause::omp_schedule_modifier_enum sg_modifier2 =
        SgOmpClause::e_omp_schedule_modifier_unspecified;
    SgOmpClause::omp_schedule_kind_enum sg_kind =
        SgOmpClause::e_omp_schedule_kind_static;
    SgExpression *chunk_size = NULL;
    SgOmpScheduleClause *sg_clause = new SgOmpScheduleClause(
        sg_modifier1, sg_modifier2, sg_kind, chunk_size);

    ROSE_ASSERT(sg_clause);
    setOneSourcePositionForTransformation(sg_clause);
    addGeneratedOmpClause(target, sg_clause);
    SageInterface::publishGeneratedSubtreeOutputOwner(sg_clause, target);
  }
}

//! Patch up private variables for omp for. The reason is that loop indices
//! should be private by default and this function will make this explicit.
//! This should happen before the actual translation is done.
int patchUpPrivateVariables(SgFile *file) {
  int result = 0;
  ROSE_ASSERT(file != NULL);

  VariantVector directive_vv = VariantVector(V_SgOmpForStatement);
  directive_vv.push_back(V_SgOmpDoStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsDistributeStatement);
  directive_vv.push_back(V_SgOmpTargetParallelForStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsDistributeParallelForStatement);
  Rose_STL_Container<SgNode *> node_list =
      NodeQuery::querySubTree(file, directive_vv);

  // For each omp for/do statement
  for (Rose_STL_Container<SgNode *>::iterator nodeListIterator =
           node_list.begin();
       nodeListIterator != node_list.end(); nodeListIterator++) {
    if (isOmpContextSelectorMetadataDirective(*nodeListIterator)) {
      continue;
    }
    SgStatement *omp_loop = isSgStatement(*nodeListIterator);
    ROSE_ASSERT(omp_loop != NULL);
    result += patchUpPrivateVariables(omp_loop);
  } // end for omp for statments
  return result;
} // end patchUpPrivateVariables()

//! Collect threadprivate variables within the current project, return a set
//! to avoid duplicated elements
std::set<SgInitializedName *> collectThreadprivateVariables() {
  set<SgInitializedName *> result;
  std::vector<SgOmpThreadprivateStatement *> tp_stmts =
      getSgNodeListFromMemoryPool<SgOmpThreadprivateStatement>();
  std::vector<SgOmpThreadprivateStatement *>::const_iterator c_iter;
  for (c_iter = tp_stmts.begin(); c_iter != tp_stmts.end(); c_iter++) {
    SgExpressionPtrList refs = (*c_iter)->get_variables();
    SgInitializedNamePtrList var_list; // = (*c_iter)->get_variables();
    for (size_t j = 0; j < refs.size(); j++) {
      if (SgVarRefExp *vref = extractVarRefFromExpression(refs[j])) {
        if (vref->get_symbol() == nullptr ||
            vref->get_symbol()->get_declaration() == nullptr) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[threadprivate-collection]: "
                  "variable item=%zu has no exact declaration identity\n",
                  j);
          ROSE_ABORT();
        }
        var_list.push_back(vref->get_symbol()->get_declaration());
      } else if (SgFortranCommonBlockRefExp *common =
                     isSgFortranCommonBlockRefExp(refs[j])) {
        validateFortranCommonBlockRef(common);
        SgExprListExp *members =
            common->get_common_block()->get_variable_reference_list();
        if (members == nullptr || members->get_expressions().empty()) {
          fprintf(stderr,
                  "REX_OMP_LOWERING_INVARIANT[threadprivate-collection]: "
                  "COMMON /%s/ has no exact member list\n",
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
                    "REX_OMP_LOWERING_INVARIANT[threadprivate-collection]: "
                    "COMMON /%s/ contains a member without exact variable "
                    "identity\n",
                    common->get_use_name().getString().c_str());
            ROSE_ABORT();
          }
          var_list.push_back(member_symbol->get_declaration());
        }
      } else {
        fprintf(stderr,
                "REX_OMP_LOWERING_INVARIANT[threadprivate-collection]: "
                "item=%zu is %s instead of one exact variable or COMMON "
                "reference\n",
                j, refs[j] != nullptr ? refs[j]->sage_class_name() : "null");
        ROSE_ABORT();
      }
    }
    std::copy(var_list.begin(), var_list.end(),
              std::inserter(result, result.end()));
  }
  return result;
}

// Check if a variable that is determined to be shared in all enclosing
// constructs, up to and including the innermost enclosing parallel construct,
// is shared start_stmt is the start point to find enclosing OpenMP
// constructs. It is excluded as an enclosing construct for itself.
// TODO: we only check if it is shared to the innermost enclosing parallel
// construct for now
static bool isSharedInEnclosingConstructs(SgInitializedName *init_var,
                                          SgStatement *start_stmt) {
  bool result = false;
  ROSE_ASSERT(init_var != NULL);
  ROSE_ASSERT(start_stmt != NULL);
  if (shouldSkipImplicitDataSharingVar(init_var))
    return true;
  SgScopeStatement *var_scope = init_var->get_scope();
  //    SgScopeStatement* directive_scope = start_stmt->get_scope();
  // locally declared variables are private to the start_stmt
  // We should not do this here. It is irrelevant to this function.
  // if (isAncestor(start_stmt, init_var))
  //   return false;

  //   cout<<"Debug omp_lowering.cpp isSharedInEnclosingConstructs()
  //   SgInitializedName name = "<<init_var->get_name().getString()<<endl;
  SgOmpParallelStatement *enclosing_par_stmt =
      getEnclosingNode<SgOmpParallelStatement>(start_stmt, false);
  // Lexically nested within a parallel region
  if (enclosing_par_stmt) {
    // locally declared variables are private to enclosing_par_stmt
    SgScopeStatement *enclosing_construct_scope =
        enclosing_par_stmt->get_scope();
    ROSE_ASSERT(enclosing_construct_scope != NULL);
    if (isAncestor(enclosing_construct_scope, var_scope))
      return false;

    // Explicitly declared as a shared variable
    if (isInClauseVariableList(init_var, enclosing_par_stmt,
                               V_SgOmpSharedClause))
      result = true;
    else { // shared by default
      VariantVector vv(V_SgOmpPrivateClause);
      vv.push_back(V_SgOmpFirstprivateClause);
      vv.push_back(V_SgOmpCopyinClause);
      vv.push_back(V_SgOmpReductionClause);
      if (isInClauseVariableList(init_var, enclosing_par_stmt, vv))
        result = false;
      else
        result = true;
    }
  } else
  // the variable is in an orphaned construct
  // The variable could be
  // 1. a function parameter: it is private to its enclosing parallel region
  // 2. a global variable: either a threadprivate variable or shared by
  // default
  // 3. is a variable declared within an orphaned function: it is private to
  // its enclosing parallel region
  // ?? any other cases?? TODO
  {
    SgFunctionDefinition *func_def = getEnclosingFunctionDefinition(start_stmt);
    ROSE_ASSERT(func_def != NULL);
    if (isSharedByDefaultInOrphanedConstruct(init_var)) {
      set<SgInitializedName *> tp_vars = collectThreadprivateVariables();
      if (tp_vars.find(init_var) != tp_vars.end())
        result = false; // is threadprivate
      else
        result = true; // otherwise
    } else if (isSgFunctionParameterList(init_var->get_parent())) {
      // function parameters are private to its dynamically (non-lexically)
      // nested parallel regions.
      result = false;
    } else if (isAncestor(func_def, var_scope)) {
      // declared within an orphaned function, should be private
      result = false;
    } else {
      cerr << "Error: OmpSupport::isSharedInEnclosingConstructs() \n "
              "Unhandled "
              "variables within an orphaned construct:"
           << endl;
      cerr << "SgInitializedName name = " << init_var->get_name().getString()
           << endl;
      dumpInfo(init_var);
      init_var->get_file_info()->display("tttt");
      ROSE_ABORT();
    }
  }
  return result;
} // end isSharedInEnclosingConstructs()

//! Patch up firstprivate variables for omp task. The reason is that the
//! specification 3.0 defines rules for implicitly determined data-sharing
//! attributes and this function will make the implicit firstprivate variable
//! of omp task explicit.
/*
variables used in task block:

2.9.1.1 Data-sharing Attribute Rules for Variables Referenced in a Construct
Ref. OMP 3.0 page 79
A variable is firstprivate to the task (default) , if
** not explicitly specified by default(), shared(),private(), firstprivate()
clauses
** not shared in enclosing constructs

It should also satisfy the restriction defined in specification 3.0 page 93
TODO
* cannot be a variable which is part of another variable (as an array or
structure element)
* cannot be private, reduction
* must have an accessible, unambiguous copy constructor for the class type
* must not have a const-qualified type unless it is of class type with a
mutable member
* must not have an incomplete C/C++ type or a reference type
*
I decided to exclude variables which are used by addresses when recognizing
firstprivate variables The reason is that in real code, it is often to have
private variables first then use their address later.   Skipping the
replacement will result in wrong semantics. e.g. from Allan Porterfield void
create_seq( double seed, double a )
      {
             double x, s;
             int    i, k;

      #pragma omp parallel private(x,s,i,k)
         {
              // .....
             // here s is private
             s = find_my_seed( myid, num_procs,
                               (long)4*NUM_KEYS, seed, a );

             for (i=k1; i<k2; i++)
             {
                 x = randlc(&s, &a); // here s is used by its address

             }
         }
      }
If not, wrong code will be generated later on. The reason follows:
   * Considering nested omp tasks:
         #pragma omp task untied
            {
              int j =100;
              // i is firstprivate, item is shared
              {
                for (i = 0; i < LARGE_NUMBER; i++)
                {
      #pragma omp task if(1)
                  process (item[i],&j);
                }
              }
            }
   * the variable j will be firstprivate by default
   * however, it is used by its address within a nested task (&j)
   * replacing it with its local copy will not get the right, original
address.
   *
   * Even worse: the replacement will cause some later translation (outlining)
to
   * access the address of a parent task's local variable.
   * It seems (not 100% certain!!!) that GOMP implements tasks as independent
entities.
   * As a result a parent task's local stack will not be always accessible to
its nested tasks.
   * A segmentation fault will occur when the lexically nested task tries to
obtain the address of
   * its parent task's local variable.
   * An example mistaken translation is shown below
       int main()
      {
        GOMP_parallel_start(OUT__3__1527__,0,0);
        OUT__3__1527__();
        GOMP_parallel_end();
        return 0;
      }

      void OUT__3__1527__()
      {
        if (GOMP_single_start()) {
          int i;
          printf(("Using %d threads.\n"),omp_get_num_threads());
          void *__out_argv2__1527__[1];
          __out_argv2__1527__[0] = ((void *)(&i));
          GOMP_task(OUT__2__1527__,&__out_argv2__1527__,0,4,4,1,1);
          //GOMP_task(OUT__2__1527__,&__out_argv2__1527__,0,4,4,1,0); //untied
or not, no difference
        }
      }

      void OUT__2__1527__(void **__out_argv)
{
  int *i = (int *)(__out_argv[0]);
  //  int _p_i;
  //  _p_i =  *i;
  //  for (_p_i = 0; _p_i < 1000; _p_i++) {
  for (*i = 0; *i < 1000; (*i)++) {
    void *__out_argv1__1527__[1];
    // cannot access auto variable from the stack of another task instance!!
    //__out_argv1__1527__[0] = ((void *)(&_p_i));
    __out_argv1__1527__[0] = ((void *)(&(*i)));// this is the right
translation GOMP_task(OUT__1__1527__,&__out_argv1__1527__,0,4,4,1,0);
  }
}
void OUT__1__1527__(void **__out_argv)
{
  int *i = (int *)(__out_argv[0]);
  int _p_i;
  _p_i =  *i;
  assert(_p_i>=0);
  assert(_p_i<10000);

  process((item[_p_i]));
}
*
  */
int patchUpFirstprivateVariables(SgFile *file) {
  int result = 0;
  ROSE_ASSERT(file != NULL);
  Rose_STL_Container<SgNode *> nodeList =
      NodeQuery::querySubTree(file, V_SgOmpTaskStatement);
  Rose_STL_Container<SgNode *>::iterator iter = nodeList.begin();
  for (; iter != nodeList.end(); iter++) {
    if (isOmpContextSelectorMetadataDirective(*iter)) {
      continue;
    }
    SgOmpTaskStatement *target = isSgOmpTaskStatement(*iter);
    SgScopeStatement *directive_scope = target->get_scope();
    SgStatement *body = target->get_body();
    ROSE_ASSERT(body != NULL);

    // Find all variable references from the task's body
    Rose_STL_Container<SgNode *> refList =
        NodeQuery::querySubTree(body, V_SgVarRefExp);
    Rose_STL_Container<SgNode *>::iterator var_iter = refList.begin();
    for (; var_iter != refList.end(); var_iter++) {
      SgVarRefExp *var_ref = isSgVarRefExp(*var_iter);
      ROSE_ASSERT(var_ref->get_symbol() != NULL);
      SgInitializedName *init_var = var_ref->get_symbol()->get_declaration();
      ROSE_ASSERT(init_var != NULL);
      if (shouldSkipImplicitDataSharingVar(init_var))
        continue;
      SgScopeStatement *var_scope = init_var->get_scope();
      ROSE_ASSERT(var_scope != NULL);

      // Variables with automatic storage duration that are declared in
      // a scope inside the construct are private. Skip them
      if (isAncestor(directive_scope, var_scope))
        continue;

      if (SageInterface::isUseByAddressVariableRef(var_ref))
        continue;
      // Skip variables already with explicit data-sharing attributes
      VariantVector vv;
      vv.push_back(V_SgOmpPrivateClause);
      vv.push_back(V_SgOmpSharedClause);
      vv.push_back(V_SgOmpFirstprivateClause);
      vv.push_back(V_SgOmpCopyinClause);
      if (isInClauseVariableList(init_var, target, vv))
        continue;
      if (isThreadprivate(var_ref->get_symbol()))
        continue;
      // Skip variables which are class/structure members: part of another
      // variable
      if (isSgClassDefinition(init_var->get_scope()))
        continue;
      // Skip variables which are shared in enclosing constructs
      if (isSharedInEnclosingConstructs(init_var, target))
        continue;
      // Now it should be a firstprivate variable
      addClauseVariable(init_var, target, V_SgOmpFirstprivateClause);
      result++;
    } // end for each variable reference
  } // end for each SgOmpTaskStatement
  return result;
} // end patchUpFirstprivateVariables()

int patchUpImplicitMappingVariables(SgFile *file) {
  int result = 0;
  ROSE_ASSERT(file != NULL);

  VariantVector directive_vv = VariantVector(V_SgOmpTargetStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsStatement);
  directive_vv.push_back(V_SgOmpTargetParallelStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsDistributeParallelForStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsDistributeStatement);
  directive_vv.push_back(V_SgOmpTargetParallelForStatement);
  Rose_STL_Container<SgNode *> node_list =
      NodeQuery::querySubTree(file, directive_vv);

  Rose_STL_Container<SgNode *>::iterator iter = node_list.begin();
  for (iter = node_list.begin(); iter != node_list.end(); iter++) {
    if (isOmpContextSelectorMetadataDirective(*iter)) {
      continue;
    }
    SgOmpClauseBodyStatement *target = NULL;
    target = isSgOmpClauseBodyStatement(*iter);
    SgScopeStatement *directive_scope = target->get_scope();
    SgStatement *body = target->get_body();
    ROSE_ASSERT(body != NULL);

    // Find all variable references from the task's body
    Rose_STL_Container<SgNode *> ref_list =
        NodeQuery::querySubTree(body, V_SgVarRefExp);
    Rose_STL_Container<SgNode *>::iterator var_iter = ref_list.begin();
    for (var_iter = ref_list.begin(); var_iter != ref_list.end(); var_iter++) {
      SgVarRefExp *var_ref = isSgVarRefExp(*var_iter);
      ROSE_ASSERT(var_ref->get_symbol() != NULL);
      SgInitializedName *init_var = var_ref->get_symbol()->get_declaration();
      ROSE_ASSERT(init_var != NULL);
      if (shouldSkipImplicitDataSharingVar(init_var))
        continue;
      SgScopeStatement *var_scope = init_var->get_scope();
      ROSE_ASSERT(var_scope != NULL);

      // Variables with automatic storage duration that are declared in
      // a scope inside the construct are private. Skip them
      if (isAncestor(directive_scope, var_scope))
        continue;

      // Skip variables already with explicit data-sharing attributes
      VariantVector vv;
      vv.push_back(V_SgOmpPrivateClause);
      vv.push_back(V_SgOmpSharedClause);
      vv.push_back(V_SgOmpFirstprivateClause);
      vv.push_back(V_SgOmpMapClause);
      if (isInClauseVariableList(init_var, target, vv))
        continue;
      // Skip variables which are class/structure members: part of another
      // variable
      if (isSgClassDefinition(init_var->get_scope()))
        continue;
      // Skip variables which are shared in enclosing constructs
      if (!isSgGlobal(var_scope) &&
          isSharedInEnclosingConstructs(init_var, target))
        continue;
      // Now it should be mapped explicitly.
      SgVariableSymbol *sym = var_ref->get_symbol();
      ROSE_ASSERT(sym != NULL);

      SgOmpMapClause *map_clause = NULL;
      SgExprListExp *explist = NULL;

      if (hasClause(target, V_SgOmpMapClause)) {
        Rose_STL_Container<SgOmpClause *> map_clauses =
            getClause(target, V_SgOmpMapClause);
        Rose_STL_Container<SgOmpClause *>::const_iterator iter;
        for (iter = map_clauses.begin(); iter != map_clauses.end(); iter++) {
          SgOmpMapClause *temp_map_clause = isSgOmpMapClause(*iter);
          if (temp_map_clause->get_operation() == SgOmpClause::e_omp_map_to ||
              temp_map_clause->get_operation() ==
                  SgOmpClause::e_omp_map_present ||
              temp_map_clause->get_operation() == SgOmpClause::e_omp_map_self ||
              temp_map_clause->get_operation() ==
                  SgOmpClause::e_omp_map_tofrom ||
              temp_map_clause->get_operation() ==
                  SgOmpClause::e_omp_map_unknown) {
            map_clause = temp_map_clause;
            explist = map_clause->get_variables();
            break;
          }
        }
      }

      if (map_clause == NULL) {
        explist = buildExprListExp();
        SgOmpClause::omp_map_operator_enum sg_type = SgOmpClause::e_omp_map_to;
        map_clause = new SgOmpMapClause(explist, sg_type);
        explist->set_parent(map_clause);
        setOneSourcePositionForTransformation(map_clause);
        addGeneratedOmpClause(target, map_clause);
      }

      bool has_mapped = false;
      Rose_STL_Container<SgExpression *>::iterator iter;
      SgExpressionPtrList expression_list = explist->get_expressions();
      for (iter = expression_list.begin(); iter != expression_list.end();
           iter++) {
        SgVarRefExp *mapped_ref = extractVarRefFromExpression(*iter);
        if (mapped_ref != nullptr && mapped_ref->get_symbol() == sym) {
          has_mapped = true;
          break;
        }
      }

      if (has_mapped == false) {
        SgExpression *locator = buildVarRefExp(var_ref->get_symbol());
        SgType *orig_type = sym->get_type();
        SgArrayType *a_type = isSgArrayType(orig_type);
        if (a_type != NULL) {
          SgExpression *array_length = a_type->get_index();
          if (array_length == nullptr ||
              isSgNullExpression(array_length) != nullptr) {
            fprintf(stderr,
                    "REX_OMP_INVARIANT[implicit-map-array-bound]: array=%s "
                    "has no exact outer bound\n",
                    sym->get_name().getString().c_str());
            ROSE_ABORT();
          }
          locator = buildPntrArrRefExp(
              locator,
              buildSubscriptExpression_nfi(
                  buildIntVal(0), copyExpression(array_length), buildIntVal(1)),
              a_type->get_base_type());
        }
        setSourcePositionForTransformation(locator);
        SgOmpMapItem *map_item = new SgOmpMapItem(locator);
        locator->set_parent(map_item);
        setOneSourcePositionForTransformation(map_item);
        explist->append_expression(map_item);
        markImplicitTargetMapVariable(target, init_var);
      }
      result++;
    } // end for each variable reference
  }
  return result;
} // end patchUpImplicitMappingVariables()

int patchUpImplicitSharedVariables(SgFile *file) {
  int result = 0;
  ROSE_ASSERT(file != NULL);

  VariantVector directive_vv = VariantVector(V_SgOmpParallelStatement);
  directive_vv.push_back(V_SgOmpTeamsStatement);
  directive_vv.push_back(V_SgOmpTeamsDistributeParallelForStatement);
  directive_vv.push_back(V_SgOmpTeamsDistributeStatement);
  Rose_STL_Container<SgNode *> node_list =
      NodeQuery::querySubTree(file, directive_vv);

  Rose_STL_Container<SgNode *>::iterator iter = node_list.begin();
  for (iter = node_list.begin(); iter != node_list.end(); iter++) {
    if (isOmpContextSelectorMetadataDirective(*iter)) {
      continue;
    }
    SgOmpClauseBodyStatement *target = NULL;
    target = isSgOmpClauseBodyStatement(*iter);
    SgScopeStatement *directive_scope = target->get_scope();
    SgStatement *body = target->get_body();
    ROSE_ASSERT(body != NULL);

    // Find all variable references from the task's body
    Rose_STL_Container<SgNode *> ref_list =
        NodeQuery::querySubTree(body, V_SgVarRefExp);
    Rose_STL_Container<SgNode *>::iterator var_iter = ref_list.begin();
    for (var_iter = ref_list.begin(); var_iter != ref_list.end(); var_iter++) {
      SgVarRefExp *var_ref = isSgVarRefExp(*var_iter);
      ROSE_ASSERT(var_ref->get_symbol() != NULL);
      SgInitializedName *init_var = var_ref->get_symbol()->get_declaration();
      ROSE_ASSERT(init_var != NULL);
      if (shouldSkipImplicitDataSharingVar(init_var))
        continue;
      SgScopeStatement *var_scope = init_var->get_scope();
      ROSE_ASSERT(var_scope != NULL);

      // Variables with automatic storage duration that are declared in
      // a scope inside the construct are private. Skip them
      if (isAncestor(directive_scope, var_scope))
        continue;

      // Skip variables already with explicit data-sharing attributes
      VariantVector vv;
      vv.push_back(V_SgOmpPrivateClause);
      vv.push_back(V_SgOmpSharedClause);
      vv.push_back(V_SgOmpFirstprivateClause);
      vv.push_back(V_SgOmpCopyinClause);
      if (isInClauseVariableList(init_var, target, vv))
        continue;
      if (isThreadprivate(var_ref->get_symbol()))
        continue;
      // Skip variables which are class/structure members: part of another
      // variable
      if (isSgClassDefinition(init_var->get_scope()))
        continue;
      // Skip variables which are shared in enclosing constructs
      if (!isSgGlobal(var_scope) &&
          isSharedInEnclosingConstructs(init_var, target))
        continue;

      // Now it should be in a shared variable
      addClauseVariable(init_var, target, V_SgOmpSharedClause);
      result++;
    } // end for each variable reference
  }
  return result;
} // end patchUpImplicitMappingVariables()

// map variables in omp target firstprivate clause
int normalizeOmpMapVariables(SgFile *file, VariantVector clause_vv,
                             SgOmpClause::omp_map_operator_enum map_type) {
  int result = 0;
  ROSE_ASSERT(file != NULL);

  VariantVector directive_vv = VariantVector(V_SgOmpTargetStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsDistributeParallelForStatement);
  directive_vv.push_back(V_SgOmpTargetTeamsDistributeStatement);
  directive_vv.push_back(V_SgOmpTargetParallelStatement);
  directive_vv.push_back(V_SgOmpTargetParallelForStatement);
  Rose_STL_Container<SgNode *> node_list =
      NodeQuery::querySubTree(file, directive_vv);

  Rose_STL_Container<SgNode *>::iterator iter;
  for (iter = node_list.begin(); iter != node_list.end(); iter++) {
    if (isOmpContextSelectorMetadataDirective(*iter)) {
      continue;
    }
    SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(*iter);
    SgStatement *body = target->get_body();
    ROSE_ASSERT(body != NULL);

    SgInitializedNamePtrList all_vars =
        collectClauseVariables(target, clause_vv);

    SgOmpMapClause *map_clause = NULL;
    SgExprListExp *explist = NULL;
    bool has_map_to_clause = false;
    // use the existing MAP TO clause if any.
    if (hasClause(target, V_SgOmpMapClause)) {
      Rose_STL_Container<SgOmpClause *> map_clauses =
          getClause(target, V_SgOmpMapClause);
      Rose_STL_Container<SgOmpClause *>::const_iterator iter;
      for (iter = map_clauses.begin(); iter != map_clauses.end(); iter++) {
        SgOmpMapClause *temp_map_clause = isSgOmpMapClause(*iter);
        if (temp_map_clause->get_operation() == SgOmpClause::e_omp_map_to ||
            temp_map_clause->get_operation() ==
                SgOmpClause::e_omp_map_present ||
            temp_map_clause->get_operation() == SgOmpClause::e_omp_map_self ||
            temp_map_clause->get_operation() == SgOmpClause::e_omp_map_tofrom ||
            temp_map_clause->get_operation() ==
                SgOmpClause::e_omp_map_unknown) {
          map_clause = temp_map_clause;
          explist = map_clause->get_variables();
          has_map_to_clause = true;
          break;
        }
      }
    }

    // create a new MAP TO clause if there isn't one.
    if (has_map_to_clause == false) {
      explist = buildExprListExp();
      SgOmpClause::omp_map_operator_enum sg_type = map_type;
      map_clause = new SgOmpMapClause(explist, sg_type);
    };
    bool has_mapped = false;

    for (size_t i = 0; i < all_vars.size(); i++) {
      if (isInClauseVariableList(all_vars[i], target, V_SgOmpMapClause))
        continue;
      SgVarRefExp *locator = buildVarRefExp(all_vars[i]);
      setSourcePositionForTransformation(locator);
      SgOmpMapItem *map_item = new SgOmpMapItem(locator);
      locator->set_parent(map_item);
      setOneSourcePositionForTransformation(map_item);
      explist->append_expression(map_item);
      markImplicitTargetMapVariable(target, all_vars[i]);
      has_mapped = true;
    }

    if (has_map_to_clause == false && has_mapped == true) {
      setOneSourcePositionForTransformation(map_clause);
      explist->set_parent(map_clause);
      addGeneratedOmpClause(target, map_clause);
    }
  }
  return result;
} // end normalizeOmpMapVariables()

bool isInOmpTargetRegion(SgStatement *node) {
  SgOmpExecStatement *target = isSgOmpExecStatement(node);
  ROSE_ASSERT(target);
  SgOmpExecStatement *parent = NULL;
  do {
    parent = isSgOmpExecStatement(target->get_omp_parent());
    if (parent != NULL) {
      switch (parent->variantT()) {
      case V_SgOmpTargetStatement:
      case V_SgOmpTargetTeamsStatement:
      case V_SgOmpTargetTeamsDistributeStatement:
        return true;
      default:
        target = parent;
      }
    }
  } while (parent != NULL);
  return false;
}

// set the parent and children of a given OpenMP executable directive node
void setOmpRelationship(SgStatement *parent, SgStatement *child) {
  SgOmpExecStatement *omp_parent = isSgOmpExecStatement(parent);
  SgOmpExecStatement *omp_child = isSgOmpExecStatement(child);
  if (omp_parent == NULL || omp_child == NULL || omp_parent == omp_child ||
      getOmpParent(child) != parent || omp_child->get_omp_parent() != NULL) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[statement-tree-edge]: parent=%p/%s "
            "child=%p/%s structural-parent=%p semantic-parent=%p does not "
            "identify one fresh exact OpenMP relationship\n",
            static_cast<void *>(parent),
            parent != NULL ? parent->class_name().c_str() : "<null>",
            static_cast<void *>(child),
            child != NULL ? child->class_name().c_str() : "<null>",
            static_cast<void *>(child != NULL ? getOmpParent(child) : NULL),
            static_cast<void *>(omp_child != NULL ? omp_child->get_omp_parent()
                                                  : NULL));
    ROSE_ABORT();
  }
  SgStatementPtrList &children = omp_parent->get_omp_children();
  if (std::find(children.begin(), children.end(), child) != children.end()) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[statement-tree-edge]: parent=%p child=%p "
            "already has a semantic child edge\n",
            static_cast<void *>(parent), static_cast<void *>(child));
    ROSE_ABORT();
  }
  children.push_back(child);
  omp_child->set_omp_parent(parent);
}

// search the OpenMP parent of a given OpenMP executable directive node, not
// its SgNode parent.
SgStatement *getOmpParent(SgStatement *node) {
  SgStatement *parent = isSgStatement(node->get_parent());
  while (parent != NULL) {
    if (isSgOmpExecStatement(parent))
      return parent;
    parent = isSgStatement(parent->get_parent());
  }
  return NULL;
}

// traverse the SgNode AST and fill the information of OpenMP executable
// directive parent and children.
void createOmpStatementTree(SgSourceFile *file) {
  if (file == NULL) {
    fprintf(stderr,
            "REX_OMP_INVARIANT[statement-tree-root]: null source file\n");
    ROSE_ABORT();
  }
  Rose_STL_Container<SgNode *> node_list =
      NodeQuery::querySubTree(file, V_SgOmpExecStatement);
  node_list.erase(std::remove_if(node_list.begin(), node_list.end(),
                                 [](const SgNode *node) {
                                   return isOmpContextSelectorMetadataDirective(
                                       node);
                                 }),
                  node_list.end());

  // This routine is the sole producer of the semantic OpenMP tree. Detach both
  // sides of every prior relationship before rebuilding from structural
  // ownership. Some former semantic parents can already be detached from the
  // project after lowering moved their bodies, so clearing only the attached
  // children's back-pointers would leave stale reverse edges in those parents.
  for (SgNode *raw_node : node_list) {
    SgOmpExecStatement *node = isSgOmpExecStatement(raw_node);
    if (node == NULL) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[statement-tree-node]: query returned a "
              "non-OpenMP executable node=%p\n",
              static_cast<void *>(raw_node));
      ROSE_ABORT();
    }
    SgStatement *old_parent = node->get_omp_parent();
    if (old_parent != NULL) {
      SgOmpExecStatement *old_parent_exec = isSgOmpExecStatement(old_parent);
      if (old_parent_exec == NULL || !SgNode::isLiveNode(old_parent_exec) ||
          std::count(old_parent_exec->get_omp_children().begin(),
                     old_parent_exec->get_omp_children().end(), node) != 1) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[statement-tree-detach]: node=%p has "
                "malformed prior semantic parent=%p\n",
                static_cast<void *>(node), static_cast<void *>(old_parent));
        ROSE_ABORT();
      }
      SgStatementPtrList &old_siblings = old_parent_exec->get_omp_children();
      old_siblings.erase(
          std::find(old_siblings.begin(), old_siblings.end(), node));
      node->set_omp_parent(NULL);
    }
  }

  for (SgNode *raw_node : node_list) {
    SgOmpExecStatement *node = isSgOmpExecStatement(raw_node);
    ROSE_ASSERT(node != NULL);
    for (SgStatement *old_child : node->get_omp_children()) {
      SgOmpExecStatement *old_child_exec = isSgOmpExecStatement(old_child);
      if (old_child_exec == NULL || !SgNode::isLiveNode(old_child_exec) ||
          old_child_exec->get_omp_parent() != node) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[statement-tree-detach]: node=%p has "
                "malformed prior semantic child=%p\n",
                static_cast<void *>(node), static_cast<void *>(old_child));
        ROSE_ABORT();
      }
      old_child_exec->set_omp_parent(NULL);
    }
    node->get_omp_children().clear();
  }

  Rose_STL_Container<SgNode *>::reverse_iterator node_list_iterator;
  for (node_list_iterator = node_list.rbegin();
       node_list_iterator != node_list.rend(); node_list_iterator++) {
    SgOmpExecStatement *node = isSgOmpExecStatement(*node_list_iterator);
    SgStatement *parent = getOmpParent(node);
    if (parent != NULL) {
      setOmpRelationship(parent, node);
    } else {
      node->set_omp_parent(parent);
    }
  }

  for (SgNode *raw_node : node_list) {
    SgOmpExecStatement *node = isSgOmpExecStatement(raw_node);
    ROSE_ASSERT(node != NULL);
    SgStatement *expected_parent = getOmpParent(node);
    if (node->get_omp_parent() != expected_parent) {
      fprintf(stderr,
              "REX_OMP_INVARIANT[statement-tree-validation]: node=%p "
              "expected-parent=%p semantic-parent=%p\n",
              static_cast<void *>(node), static_cast<void *>(expected_parent),
              static_cast<void *>(node->get_omp_parent()));
      ROSE_ABORT();
    }
    if (expected_parent != NULL) {
      SgOmpExecStatement *omp_parent = isSgOmpExecStatement(expected_parent);
      ROSE_ASSERT(omp_parent != NULL);
      if (std::count(omp_parent->get_omp_children().begin(),
                     omp_parent->get_omp_children().end(), node) != 1) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[statement-tree-validation]: node=%p "
                "does not have one exact reverse child edge in parent=%p\n",
                static_cast<void *>(node),
                static_cast<void *>(expected_parent));
        ROSE_ABORT();
      }
    }
    for (SgStatement *child : node->get_omp_children()) {
      SgOmpExecStatement *omp_child = isSgOmpExecStatement(child);
      if (omp_child == NULL || omp_child->get_omp_parent() != node ||
          getOmpParent(child) != node) {
        fprintf(stderr,
                "REX_OMP_INVARIANT[statement-tree-validation]: parent=%p "
                "contains malformed child=%p\n",
                static_cast<void *>(node), static_cast<void *>(child));
        ROSE_ABORT();
      }
    }
  }
}

void setOmpNumTeams(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *num_teams_expression = NULL;
  SgOmpNumTeamsClause *num_teams_clause = NULL;
  if (hasClause(target, V_SgOmpNumTeamsClause)) {
    Rose_STL_Container<SgOmpClause *> num_teams_clauses =
        getClause(target, V_SgOmpNumTeamsClause);
    ROSE_ASSERT(num_teams_clauses.size() ==
                1); // should only have one num_teams()
    num_teams_clause = isSgOmpNumTeamsClause(num_teams_clauses[0]);
    ROSE_ASSERT(num_teams_clause->get_expression() != NULL);
  } else {
    num_teams_expression = buildIntVal(256);
    num_teams_clause = new SgOmpNumTeamsClause(num_teams_expression);
    num_teams_expression->set_parent(num_teams_clause);
    setOneSourcePositionForTransformation(num_teams_clause);
    addGeneratedOmpClause(target, num_teams_clause);
    SageInterface::publishGeneratedSubtreeOutputOwner(num_teams_clause, target);
  }
}

void setOmpNumThreads(SgNode *node) {
  ROSE_ASSERT(node != NULL);
  SgOmpClauseBodyStatement *target = isSgOmpClauseBodyStatement(node);
  ROSE_ASSERT(target != NULL);

  SgExpression *num_threads_expression = NULL;
  SgOmpNumThreadsClause *num_threads_clause = NULL;
  if (hasClause(target, V_SgOmpNumThreadsClause)) {
    Rose_STL_Container<SgOmpClause *> num_threads_clauses =
        getClause(target, V_SgOmpNumThreadsClause);
    ROSE_ASSERT(num_threads_clauses.size() ==
                1); // should only have one num_threads()
    num_threads_clause = isSgOmpNumThreadsClause(num_threads_clauses[0]);
    ROSE_ASSERT(num_threads_clause->get_expression() != NULL);
  } else {
    num_threads_expression = buildIntVal(128);
    num_threads_clause = new SgOmpNumThreadsClause(num_threads_expression);
    num_threads_expression->set_parent(num_threads_clause);
    setOneSourcePositionForTransformation(num_threads_clause);
    addGeneratedOmpClause(target, num_threads_clause);
    SageInterface::publishGeneratedSubtreeOutputOwner(num_threads_clause,
                                                      target);
  }
}

void normalizeOmpTargetOffloadingUnits(SgFile *file) {
  ROSE_ASSERT(file != NULL);
  Rose_STL_Container<SgNode *> omp_nodes =
      NodeQuery::querySubTree(file, V_SgOmpExecStatement);
  Rose_STL_Container<SgNode *>::iterator iter;
  SgOmpExecStatement *parent = NULL;
  for (iter = omp_nodes.begin(); iter != omp_nodes.end(); iter++) {
    if (isOmpContextSelectorMetadataDirective(*iter)) {
      continue;
    }
    SgOmpExecStatement *node = isSgOmpExecStatement(*iter);
    ROSE_ASSERT(node != NULL);
    // It doesn't need to check whether the directive is a variant because
    // metadirective has been lowered at this point.
    switch (node->variantT()) {
    case V_SgOmpTargetTeamsStatement:
    case V_SgOmpTargetTeamsDistributeStatement:
      setOmpNumTeams(node);
      break;
    case V_SgOmpTargetParallelForStatement:
    case V_SgOmpTargetParallelStatement:
      setOmpNumThreads(node);
      break;
    case V_SgOmpTargetTeamsDistributeParallelForStatement:
      setOmpNumTeams(node);
      setOmpNumThreads(node);
      break;
    // Check whether parallel/parallel for is in a target region.
    // case V_SgOmpParallelForStatement:
    case V_SgOmpParallelStatement:
      if (isInOmpTargetRegion(node))
        setOmpNumThreads(node);
      break;
    // Check whether teams/teams distribute is in a target region.
    case V_SgOmpTeamsStatement:
    case V_SgOmpTeamsDistributeStatement:
      if (isInOmpTargetRegion(node))
        setOmpNumTeams(node);
      break;
    // Check whether teams distribute parallel for is in a target region.
    case V_SgOmpTeamsDistributeParallelForStatement:
      if (isInOmpTargetRegion(node)) {
        setOmpNumTeams(node);
        setOmpNumThreads(node);
      }
      break;
    default:;
    }
  }
}

void analyze_omp(SgSourceFile *file) {
  clearImplicitTargetMapVariables();

  // Transform omp metadirective to multiple variants.
  Rose_STL_Container<SgNode *> variant_directives =
      NodeQuery::querySubTree(file, V_SgOmpMetadirectiveStatement);
  Rose_STL_Container<SgNode *>::iterator node_list_iterator;
  for (node_list_iterator = variant_directives.begin();
       node_list_iterator != variant_directives.end(); node_list_iterator++) {
    SgStatement *node = isSgStatement(*node_list_iterator);
    ROSE_ASSERT(node != NULL);
    analyzeOmpMetadirective(node);
  }

  patchUpPrivateVariables(file); // the order of these two functions matter! We
                                 // want to patch up private variable first!
  patchUpFirstprivateVariables(file);

  patchUpImplicitSharedVariables(file);

  patchUpImplicitMappingVariables(file);

  // Convert firstprivate/private/shared clause in target directive to map
  // clause because later only map clause will be lowered for data transferring.
  VariantVector clause_vv = VariantVector(V_SgOmpFirstprivateClause);
  clause_vv.push_back(V_SgOmpPrivateClause);
  clause_vv.push_back(V_SgOmpSharedClause);
  normalizeOmpMapVariables(file, clause_vv, SgOmpClause::e_omp_map_to);

  VariantVector loop_directive_vv = VariantVector(V_SgOmpForStatement);
  loop_directive_vv.push_back(V_SgOmpDoStatement);
  loop_directive_vv.push_back(V_SgOmpTargetParallelForStatement);
  loop_directive_vv.push_back(V_SgOmpTargetTeamsDistributeParallelForStatement);
  Rose_STL_Container<SgNode *> node_list =
      NodeQuery::querySubTree(file, loop_directive_vv);
  for (node_list_iterator = node_list.begin();
       node_list_iterator != node_list.end(); node_list_iterator++) {
    if (isOmpContextSelectorMetadataDirective(*node_list_iterator)) {
      continue;
    }
    SgStatement *node = isSgStatement(*node_list_iterator);
    ROSE_ASSERT(node != NULL);
    normalizeOmpLoop(node);
  }

  // Add the information of OpenMP directive parent and children.
  OmpSupport::createOmpStatementTree(file);
  // Normalize num_teams and num_threads in the target region.
  OmpSupport::normalizeOmpTargetOffloadingUnits(file);
}

} // namespace OmpSupport
