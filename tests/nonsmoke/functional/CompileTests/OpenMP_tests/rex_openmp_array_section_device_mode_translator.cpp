#include "nodeQuery.h"
#include "rose.h"

#include <string>

namespace {

template <class Clause>
Clause *requireClause(SgOmpClauseBodyStatement *statement) {
  Clause *result = nullptr;
  for (SgOmpClause *clause : statement->get_clauses()) {
    if (Clause *candidate = dynamic_cast<Clause *>(clause)) {
      ROSE_ASSERT(result == nullptr);
      result = candidate;
    }
  }
  ROSE_ASSERT(result != nullptr);
  return result;
}

void requireExactType(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr && expression->get_type() != nullptr);
  ROSE_ASSERT(isSgTypeUnknown(expression->get_type()) == nullptr);
  ROSE_ASSERT(isSgTypeDefault(expression->get_type()) == nullptr);
}

void requireExactGeneratedSource(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  Sg_File_Info *primary = expression->get_file_info();
  Sg_File_Info *start = expression->get_startOfConstruct();
  Sg_File_Info *end = expression->get_endOfConstruct();
  Sg_File_Info *operator_position = expression->get_operatorPosition();
  auto exact = [expression](Sg_File_Info *file_info) {
    return file_info != nullptr && file_info->get_parent() == expression &&
           file_info->isTransformation() &&
           file_info->isOutputInCodeGeneration();
  };
  ROSE_ASSERT(exact(primary) && exact(start) && exact(end) &&
              exact(operator_position));
  ROSE_ASSERT(primary == operator_position);
  ROSE_ASSERT(start != end && start != operator_position &&
              end != operator_position);
}

void requireExactGeneratedSourceCopy(SgExpression *source, SgExpression *copy) {
  requireExactGeneratedSource(source);
  requireExactGeneratedSource(copy);
  auto exact_copy = [source, copy](Sg_File_Info *source_info,
                                   Sg_File_Info *copy_info) {
    ROSE_ASSERT(source_info != nullptr && copy_info != nullptr);
    ROSE_ASSERT(source_info != copy_info);
    ROSE_ASSERT(source_info->get_parent() == source);
    ROSE_ASSERT(copy_info->get_parent() == copy);
    ROSE_ASSERT(source_info->get_filenameString() ==
                copy_info->get_filenameString());
    ROSE_ASSERT(source_info->get_line() == copy_info->get_line());
    ROSE_ASSERT(source_info->get_col() == copy_info->get_col());
    ROSE_ASSERT(source_info->get_physical_file_id() ==
                copy_info->get_physical_file_id());
    ROSE_ASSERT(source_info->isTransformation() ==
                copy_info->isTransformation());
    ROSE_ASSERT(source_info->isOutputInCodeGeneration() ==
                copy_info->isOutputInCodeGeneration());
  };
  exact_copy(source->get_startOfConstruct(), copy->get_startOfConstruct());
  exact_copy(source->get_endOfConstruct(), copy->get_endOfConstruct());
  exact_copy(source->get_operatorPosition(), copy->get_operatorPosition());
}

size_t requireExactGeneratedArraySectionTree(SgExpression *expression) {
  const Rose_STL_Container<SgNode *> subscripts =
      NodeQuery::querySubTree(expression, V_SgSubscriptExpression);
  size_t omitted_operands = 0;
  for (SgNode *node : subscripts) {
    SgSubscriptExpression *subscript = isSgSubscriptExpression(node);
    requireExactGeneratedSource(subscript);
    SgExpression *operands[] = {subscript->get_lowerBound(),
                                subscript->get_upperBound(),
                                subscript->get_stride()};
    for (SgExpression *operand : operands) {
      ROSE_ASSERT(operand != nullptr && operand->get_parent() == subscript);
      if (SgNullExpression *absence = isSgNullExpression(operand)) {
        ROSE_ASSERT(absence->get_role() ==
                    SgNullExpression::e_null_expression_syntactic_absence);
        requireExactGeneratedSource(absence);
        ++omitted_operands;
      }
    }
  }
  return omitted_operands;
}

size_t requireExactGeneratedArraySectionCopy(SgExpression *expression) {
  SgTreeCopy copy_help;
  SgExpression *copy = isSgExpression(expression->copy(copy_help));
  ROSE_ASSERT(copy != nullptr && copy->get_parent() == nullptr);
  const Rose_STL_Container<SgNode *> source_subscripts =
      NodeQuery::querySubTree(expression, V_SgSubscriptExpression);
  const Rose_STL_Container<SgNode *> copied_subscripts =
      NodeQuery::querySubTree(copy, V_SgSubscriptExpression);
  ROSE_ASSERT(source_subscripts.size() == copied_subscripts.size());
  for (size_t index = 0; index < source_subscripts.size(); ++index) {
    SgSubscriptExpression *source_subscript =
        isSgSubscriptExpression(source_subscripts[index]);
    SgSubscriptExpression *copied_subscript =
        isSgSubscriptExpression(copied_subscripts[index]);
    requireExactGeneratedSourceCopy(source_subscript, copied_subscript);
    SgExpression *source_operands[] = {source_subscript->get_lowerBound(),
                                       source_subscript->get_upperBound(),
                                       source_subscript->get_stride()};
    SgExpression *copied_operands[] = {copied_subscript->get_lowerBound(),
                                       copied_subscript->get_upperBound(),
                                       copied_subscript->get_stride()};
    for (size_t operand = 0; operand < 3; ++operand) {
      if (isSgNullExpression(source_operands[operand]) != nullptr) {
        ROSE_ASSERT(isSgNullExpression(copied_operands[operand]) != nullptr);
        requireExactGeneratedSourceCopy(source_operands[operand],
                                        copied_operands[operand]);
      }
    }
  }

  const size_t source_omitted =
      requireExactGeneratedArraySectionTree(expression);
  const size_t copied_omitted = requireExactGeneratedArraySectionTree(copy);
  ROSE_ASSERT(source_omitted == copied_omitted);
  return source_omitted;
}

SgVarRefExp *requireSectionRoot(SgExpression *expression) {
  SgExpression *current = expression;
  for (;;) {
    if (SgPntrArrRefExp *section = isSgPntrArrRefExp(current)) {
      current = section->get_lhs_operand();
      continue;
    }
    if (SgCastExp *cast = isSgCastExp(current)) {
      ROSE_ASSERT(cast->cast_type() == SgCastExp::e_implicit_cast);
      requireExactType(cast);
      ROSE_ASSERT(cast->get_operand() != nullptr &&
                  cast->get_operand()->get_parent() == cast);
      current = cast->get_operand();
      continue;
    }
    break;
  }
  SgVarRefExp *root = isSgVarRefExp(current);
  ROSE_ASSERT(root != nullptr && root->get_symbol() != nullptr);
  return root;
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  const Rose_STL_Container<SgNode *> target_nodes =
      NodeQuery::querySubTree(project, V_SgOmpTargetStatement);
  ROSE_ASSERT(target_nodes.size() == 1);
  auto *target = isSgOmpTargetStatement(target_nodes.front());
  ROSE_ASSERT(target != nullptr);

  SgOmpDeviceClause *device = requireClause<SgOmpDeviceClause>(target);
  SgOmpSourceExpression *wildcard =
      isSgOmpSourceExpression(device->get_expression());
  ROSE_ASSERT(wildcard != nullptr && wildcard->get_parent() == device);
  ROSE_ASSERT(wildcard->get_spelling() == "*");

  SgOmpMapClause *map = requireClause<SgOmpMapClause>(target);
  ROSE_ASSERT(map->get_variables() != nullptr &&
              map->get_variables()->get_parent() == map);
  const SgExpressionPtrList &items = map->get_variables()->get_expressions();
  ROSE_ASSERT(items.size() == 2);

  bool found_matrix = false;
  bool found_values = false;
  size_t omitted_operands = 0;
  for (SgExpression *item_expression : items) {
    SgOmpMapItem *item = isSgOmpMapItem(item_expression);
    ROSE_ASSERT(item != nullptr && item->get_parent() == map->get_variables());
    SgPntrArrRefExp *outer = isSgPntrArrRefExp(item->get_expression());
    ROSE_ASSERT(outer != nullptr && outer->get_parent() == item);
    requireExactType(outer);
    ROSE_ASSERT(isSgTypeInt(outer->get_type()->stripType()) != nullptr);

    SgVarRefExp *root = requireSectionRoot(outer);
    const std::string name = root->get_symbol()->get_name().getString();
    if (name == "matrix") {
      ROSE_ASSERT(!found_matrix);
      found_matrix = true;
      SgPntrArrRefExp *inner = isSgPntrArrRefExp(outer->get_lhs_operand());
      ROSE_ASSERT(inner != nullptr && inner->get_parent() == outer);
      requireExactType(inner);
      SgArrayType *row_type = isSgArrayType(inner->get_type());
      ROSE_ASSERT(row_type != nullptr);
      ROSE_ASSERT(isSgTypeInt(row_type->get_base_type()->stripType()) !=
                  nullptr);
    } else {
      ROSE_ASSERT(name == "values" && !found_values);
      found_values = true;
      ROSE_ASSERT(outer->get_lhs_operand() == root);
    }
    omitted_operands += requireExactGeneratedArraySectionCopy(outer);
  }
  ROSE_ASSERT(found_matrix && found_values);
  // matrix[1:2][2:3] omits both strides; values[:length] omits its
  // lower bound and stride.  Every source-level omission must remain a
  // distinct typed syntactic-absence node.
  ROSE_ASSERT(omitted_operands == 4);

  const Rose_STL_Container<SgNode *> subscripts =
      NodeQuery::querySubTree(target, V_SgSubscriptExpression);
  ROSE_ASSERT(subscripts.size() == 3);
  for (SgNode *node : subscripts) {
    requireExactGeneratedSource(isSgSubscriptExpression(node));
  }

  AstTests::runAllTests(project);
  return backend(project);
}
