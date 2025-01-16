//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <vector>

#include "execution/executors/aggregation_executor.h"

namespace bustub {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      child_executor_(std::move(child_executor)),
      aht_(plan->aggregates_, plan->agg_types_),
      aht_iterator_(aht_.Begin()) {}

void AggregationExecutor::Init() {
  child_executor_->Init();

  Tuple tuple;
  RID rid;
  /* 表为空 */
  if (!child_executor_->Next(&tuple, &rid)) {
    if (!plan_->group_bys_.empty()) {
      return;
    }
    aht_.InsertCombine(AggregateKey{}, aht_.GenerateInitialAggregateValue());
    aht_iterator_ = aht_.Begin();
    return;
  }

  aht_.Clear();
  aht_.InsertCombine(MakeAggregateKey(&tuple), MakeAggregateValue(&tuple));
  while (child_executor_->Next(&tuple, &rid)) {
    aht_.InsertCombine(MakeAggregateKey(&tuple), MakeAggregateValue(&tuple));
  }

  aht_iterator_ = aht_.Begin();
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (aht_iterator_ == aht_.End()) {
    return false;
  }
  std::vector<Value> values(aht_iterator_.Key().group_bys_);
  for (const Value &value : aht_iterator_.Val().aggregates_) {
    values.emplace_back(value);
  }
  *tuple = Tuple(values, &GetOutputSchema());
  *rid = tuple->GetRid();
  ++aht_iterator_;
  return true;
}

auto AggregationExecutor::GetChildExecutor() const -> const AbstractExecutor * { return child_executor_.get(); }

}  // namespace bustub
