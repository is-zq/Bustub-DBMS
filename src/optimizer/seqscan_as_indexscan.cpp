#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/expressions/logic_expression.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/seq_scan_plan.h"

#include "optimizer/optimizer.h"

namespace bustub {

auto Optimizer::OptimizeSeqScanAsIndexScan(const bustub::AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(student): implement seq scan with predicate -> index scan optimizer rule
  // The Filter Predicate Pushdown has been enabled for you in optimizer.cpp when forcing starter rule

  std::vector<AbstractPlanNodeRef> children;
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeSeqScanAsIndexScan(child));
  }

  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  if (optimized_plan->GetType() == PlanType::SeqScan) {
    const auto &seqscan_plan = dynamic_cast<const SeqScanPlanNode &>(*optimized_plan);
    /* 判断是否有谓词 */
    if (seqscan_plan.filter_predicate_ != nullptr) {
      // Step 1: 检查表达式是否是 ComparisonExpression
      auto comp_expr = dynamic_cast<const ComparisonExpression *>(seqscan_plan.filter_predicate_.get());
      if (comp_expr != nullptr) {
        // Step 2: 检查操作符是否为 '='
        if (comp_expr->comp_type_ == ComparisonType::Equal) {
          // Step 3: 检查左子表达式是否是 ColumnValueExpression
          auto left_expr = dynamic_cast<const ColumnValueExpression *>(comp_expr->children_[0].get());
          if (left_expr != nullptr) {
            // Step 4: 检查右子表达式是否是 ConstantValueExpression
            auto right_expr = dynamic_cast<ConstantValueExpression *>(comp_expr->children_[1].get());
            if (right_expr != nullptr) {
              /* 判断对应列是否有索引 */
              for (auto index : catalog_.GetTableIndexes(seqscan_plan.table_name_)) {
                const std::vector<uint32_t> key_attrs = index->index_->GetKeyAttrs();
                if (key_attrs.size() == 1 && left_expr->GetColIdx() == key_attrs[0]) {
                  /* 转换 */
                  return std::make_shared<IndexScanPlanNode>(seqscan_plan.output_schema_, seqscan_plan.table_oid_,
                                                             index->index_oid_, seqscan_plan.filter_predicate_,
                                                             right_expr);
                }
              }
            }
          }
        }
      }
    }
  }
  return optimized_plan;
}

}  // namespace bustub
