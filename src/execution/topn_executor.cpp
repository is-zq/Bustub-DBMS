#include "execution/executors/topn_executor.h"

namespace bustub {

TopNExecutor::TopNExecutor(ExecutorContext *exec_ctx, const TopNPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void TopNExecutor::Init() {
  child_executor_->Init();
  while (!min_heap_.empty()) {
    min_heap_.pop();
  }
  Tuple tuple;
  RID rid;
  std::priority_queue<TopNElem> max_heap;
  while (child_executor_->Next(&tuple, &rid)) {
    std::vector<std::pair<OrderByType, Value>> keys;
    keys.reserve(plan_->order_bys_.size());
    for (const auto &[order_by_type, expr] : plan_->order_bys_) {
      keys.emplace_back(order_by_type, expr->Evaluate(&tuple, child_executor_->GetOutputSchema()));
    }
    TopNElem elem(std::move(keys), std::move(tuple));
    if (max_heap.size() < plan_->n_) {
      max_heap.emplace(elem);
      continue;
    }
    if (elem < max_heap.top()) {
      max_heap.pop();
      max_heap.emplace(elem);
    }
  }
  while (!max_heap.empty()) {
    min_heap_.emplace(max_heap.top());
    max_heap.pop();
  }
}

auto TopNExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (min_heap_.empty()) {
    return false;
  }
  *tuple = min_heap_.top().tuple_;
  *rid = tuple->GetRid();
  min_heap_.pop();
  return true;
}

auto TopNExecutor::GetNumInHeap() -> size_t { return min_heap_.size(); }

}  // namespace bustub
