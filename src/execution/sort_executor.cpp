#include "execution/executors/sort_executor.h"

namespace bustub {

SortExecutor::SortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void SortExecutor::Init() {
  child_executor_->Init();
  tuples_.clear();
  const auto &order_bys = plan_->GetOrderBy();
  Tuple tuple;
  RID rid;
  while (child_executor_->Next(&tuple, &rid)) {
    SortKey sort_key;
    for (const auto &[order_by_type, expr] : order_bys) {
      sort_key.keys_.emplace_back(order_by_type, expr->Evaluate(&tuple, child_executor_->GetOutputSchema()));
    }
    tuples_.emplace_back(std::move(sort_key), std::move(tuple));
  }
  std::sort(tuples_.begin(), tuples_.end(),
            [](const std::pair<SortKey, Tuple> &a, const std::pair<SortKey, Tuple> &b) { return a.first < b.first; });
  it_ = tuples_.begin();
}

auto SortExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (it_ == tuples_.end()) {
    return false;
  }
  *tuple = it_->second;
  *rid = tuple->GetRid();
  ++it_;
  return true;
}

}  // namespace bustub
