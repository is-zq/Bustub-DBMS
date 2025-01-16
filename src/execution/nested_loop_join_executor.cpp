//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"
#include "binder/table_ref/bound_join_ref.h"
#include "common/exception.h"
#include "type/value_factory.h"

namespace bustub {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_executor_(std::move(left_executor)),
      right_executor_(std::move(right_executor)) {
  if (plan->GetJoinType() != JoinType::LEFT && plan->GetJoinType() != JoinType::INNER) {
    // Note for 2023 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestedLoopJoinExecutor::Init() {
  left_executor_->Init();
  right_executor_->Init();
  if (!left_executor_->Next(&left_tuple_, &left_rid_)) {
    left_end_ = true;
  }
  left_end_ = false;
  left_join_null_ = true;
}

auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (left_end_) {
    return false;
  }

  const Schema &left_schema = left_executor_->GetOutputSchema();
  const Schema &right_schema = right_executor_->GetOutputSchema();
  Tuple right_tuple;
  RID right_rid;
  while (true) {
    if (!right_executor_->Next(&right_tuple, &right_rid)) {
      bool ret = false;
      if (left_join_null_ && plan_->join_type_ == JoinType::LEFT) {
        std::vector<Value> values;
        for (uint32_t col_idx = 0; col_idx < left_schema.GetColumnCount(); ++col_idx) {
          values.emplace_back(left_tuple_.GetValue(&left_schema, col_idx));
        }
        for (uint32_t col_idx = 0; col_idx < right_schema.GetColumnCount(); ++col_idx) {
          values.emplace_back(ValueFactory::GetNullValueByType(right_schema.GetColumn(col_idx).GetType()));
        }
        *tuple = Tuple(values, &GetOutputSchema());
        *rid = tuple->GetRid();
        ret = true;
      }
      if (!left_executor_->Next(&left_tuple_, &left_rid_)) {
        left_end_ = true;
        return ret;
      }
      right_executor_->Init();
      left_join_null_ = true;
      if (ret) {
        return true;
      }
    } else {
      do {
        auto pred_res = plan_->predicate_->EvaluateJoin(&left_tuple_, left_schema, &right_tuple, right_schema);
        if (!pred_res.IsNull() && pred_res.GetAs<bool>()) {
          std::vector<Value> values;
          for (uint32_t col_idx = 0; col_idx < left_schema.GetColumnCount(); ++col_idx) {
            values.emplace_back(left_tuple_.GetValue(&left_schema, col_idx));
          }
          for (uint32_t col_idx = 0; col_idx < right_schema.GetColumnCount(); ++col_idx) {
            values.emplace_back(right_tuple.GetValue(&right_schema, col_idx));
          }
          *tuple = Tuple(values, &GetOutputSchema());
          *rid = tuple->GetRid();
          left_join_null_ = false;
          return true;
        }
      } while (right_executor_->Next(&right_tuple, &right_rid));
    }
  }
}

}  // namespace bustub
