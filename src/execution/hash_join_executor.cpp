//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hash_join_executor.cpp
//
// Identification: src/execution/hash_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/hash_join_executor.h"
#include "type/value_factory.h"

namespace bustub {

HashJoinExecutor::HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                                   std::unique_ptr<AbstractExecutor> &&left_child,
                                   std::unique_ptr<AbstractExecutor> &&right_child)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_child_(std::move(left_child)),
      right_child_(std::move(right_child)) {
  if (plan->GetJoinType() != JoinType::LEFT && plan->GetJoinType() != JoinType::INNER) {
    // Note for 2023 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void HashJoinExecutor::Init() {
  left_child_->Init();
  right_child_->Init();
  ht_.clear();
  it_ = ht_.end();
  end_it_ = ht_.end();
  cur_left_values_.clear();
  Tuple right_tuple;
  RID right_rid;
  while (right_child_->Next(&right_tuple, &right_rid)) {  // 用右表方便左连接
    ht_.emplace(MakeRightHashJoinKey(&right_tuple), right_tuple);
  }
}

auto HashJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (it_ != end_it_) {
    Tuple right_tuple = it_->second;
    std::vector<Value> values(cur_left_values_);
    const Schema &right_schema = right_child_->GetOutputSchema();
    for (uint32_t col_idx = 0; col_idx < right_schema.GetColumnCount(); ++col_idx) {
      values.emplace_back(right_tuple.GetValue(&right_schema, col_idx));
    }
    *tuple = Tuple(values, &GetOutputSchema());
    *rid = tuple->GetRid();
    ++it_;
    return true;
  }

  Tuple left_tuple;
  RID left_rid;
  const Schema &left_schema = left_child_->GetOutputSchema();
  const Schema &right_schema = right_child_->GetOutputSchema();
  while (left_child_->Next(&left_tuple, &left_rid)) {
    auto range = ht_.equal_range(MakeLeftHashJoinKey(&left_tuple));
    it_ = range.first;
    end_it_ = range.second;
    if (it_ == end_it_) {
      if (plan_->join_type_ == JoinType::LEFT) {
        cur_left_values_.clear();
        for (uint32_t col_idx = 0; col_idx < left_schema.GetColumnCount(); ++col_idx) {
          cur_left_values_.emplace_back(left_tuple.GetValue(&left_schema, col_idx));
        }
        std::vector<Value> values(cur_left_values_);
        for (uint32_t col_idx = 0; col_idx < right_schema.GetColumnCount(); ++col_idx) {
          values.emplace_back(ValueFactory::GetNullValueByType(right_schema.GetColumn(col_idx).GetType()));
        }
        *tuple = Tuple(values, &GetOutputSchema());
        *rid = tuple->GetRid();
        return true;
      }
    } else {
      cur_left_values_.clear();
      for (uint32_t col_idx = 0; col_idx < left_schema.GetColumnCount(); ++col_idx) {
        cur_left_values_.emplace_back(left_tuple.GetValue(&left_schema, col_idx));
      }
      Tuple right_tuple = it_->second;
      std::vector<Value> values(cur_left_values_);
      for (uint32_t col_idx = 0; col_idx < right_schema.GetColumnCount(); ++col_idx) {
        values.emplace_back(right_tuple.GetValue(&right_schema, col_idx));
      }
      *tuple = Tuple(values, &GetOutputSchema());
      *rid = tuple->GetRid();
      ++it_;
      return true;
    }
  }
  return false;
}

}  // namespace bustub
