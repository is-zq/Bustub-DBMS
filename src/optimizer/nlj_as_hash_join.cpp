#include <algorithm>
#include <memory>
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/expressions/logic_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/hash_join_plan.h"
#include "execution/plans/nested_loop_join_plan.h"
#include "execution/plans/projection_plan.h"
#include "optimizer/optimizer.h"
#include "type/type_id.h"

namespace bustub {

auto Optimizer::IsMultiEqui(const AbstractExpressionRef &expr, std::vector<AbstractExpressionRef> &left_key_expressions,
                            std::vector<AbstractExpressionRef> &right_key_expressions) -> bool {
  if (const auto *comp_expr = dynamic_cast<const ComparisonExpression *>(expr.get()); comp_expr != nullptr) {
    if (comp_expr->comp_type_ == ComparisonType::Equal) {
      if (const auto *left_expr = dynamic_cast<const ColumnValueExpression *>(comp_expr->children_[0].get());
          left_expr != nullptr) {
        if (const auto *right_expr = dynamic_cast<const ColumnValueExpression *>(comp_expr->children_[1].get());
            right_expr != nullptr) {
          // 等式左边是连接的左边
          if (left_expr->GetTupleIdx() == 0 && right_expr->GetTupleIdx() == 1) {
            left_key_expressions.emplace_back(
                std::make_shared<ColumnValueExpression>(0, left_expr->GetColIdx(), left_expr->GetReturnType()));
            right_key_expressions.emplace_back(
                std::make_shared<ColumnValueExpression>(1, right_expr->GetColIdx(), right_expr->GetReturnType()));
          }
          if (left_expr->GetTupleIdx() == 1 && right_expr->GetTupleIdx() == 0) {
            left_key_expressions.emplace_back(
                std::make_shared<ColumnValueExpression>(0, right_expr->GetColIdx(), right_expr->GetReturnType()));
            right_key_expressions.emplace_back(
                std::make_shared<ColumnValueExpression>(1, left_expr->GetColIdx(), left_expr->GetReturnType()));
          }
          return true;
        }
      }
    }
  } else if (const auto *logic_expr = dynamic_cast<const LogicExpression *>(expr.get()); logic_expr != nullptr) {
    if (logic_expr->logic_type_ == LogicType::And) {
      return IsMultiEqui(logic_expr->children_[0], left_key_expressions, right_key_expressions) &&
             IsMultiEqui(logic_expr->children_[1], left_key_expressions, right_key_expressions);
    }
  }
  return false;
}

auto Optimizer::OptimizeNLJAsHashJoin(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(student): implement NestedLoopJoin -> HashJoin optimizer rule
  // Note for 2023 Fall: You should support join keys of any number of conjunction of equi-condistions:
  // E.g. <column expr> = <column expr> AND <column expr> = <column expr> AND ...
  std::vector<AbstractPlanNodeRef> children;
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeNLJAsHashJoin(child));
  }
  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  if (optimized_plan->GetType() == PlanType::NestedLoopJoin) {
    const auto &nlj_plan = dynamic_cast<const NestedLoopJoinPlanNode &>(*optimized_plan);
    // Has exactly two children
    BUSTUB_ENSURE(nlj_plan.children_.size() == 2, "NLJ should have exactly 2 children.");
    std::vector<AbstractExpressionRef> left_key_expressions;
    std::vector<AbstractExpressionRef> right_key_expressions;
    if (IsMultiEqui(nlj_plan.Predicate(), left_key_expressions, right_key_expressions)) {
      return std::make_shared<HashJoinPlanNode>(nlj_plan.output_schema_, nlj_plan.GetLeftPlan(),
                                                nlj_plan.GetRightPlan(), left_key_expressions, right_key_expressions,
                                                nlj_plan.GetJoinType());
    }
  }

  return optimized_plan;
}

}  // namespace bustub
