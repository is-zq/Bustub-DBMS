#include "execution/executors/window_function_executor.h"
#include "execution/plans/window_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

WindowFunctionExecutor::WindowFunctionExecutor(ExecutorContext *exec_ctx, const WindowFunctionPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void WindowFunctionExecutor::Init() {
  child_executor_->Init();
  std::vector<std::pair<SortKey, Tuple>> tuples;
  Tuple tuple;
  RID rid;
  SimpleWindowFunctionHashTable ht;
  while (child_executor_->Next(&tuple, &rid)) {
    SortKey sort_key;
    if (!plan_->window_functions_.begin()->second.order_by_.empty()) {
      for (const auto &[order_by_type, expr] : plan_->window_functions_.begin()->second.order_by_) {
        sort_key.keys_.emplace_back(order_by_type, expr->Evaluate(&tuple, child_executor_->GetOutputSchema()));
      }
    }
    tuples.emplace_back(std::move(sort_key), std::move(tuple));
  }
  if (!plan_->window_functions_.begin()->second.order_by_.empty()) {
    // 测试保证每个窗口函数order by字段一样，只需排序一次
    std::sort(tuples.begin(), tuples.end(),
              [](const std::pair<SortKey, Tuple> &a, const std::pair<SortKey, Tuple> &b) { return a.first < b.first; });
  }
  // 有order，算从分区开始行到当前行的
  if (!plan_->window_functions_.begin()->second.order_by_.empty()) {
    // 扫描tuples，以win_func_idx和partition_by共同为key，单独计算每个分区的每个窗口函数，每计算一个放入结果tuples_中
    for (const auto &[sort_key, cur_tuple] : tuples) {
      std::vector<Value> values;
      values.resize(plan_->columns_.size());
      for (const auto &[window_func_idx, window_function] : plan_->window_functions_) {
        auto win_func_key = MakeWinFuncKey(&cur_tuple, window_func_idx, window_function.type_);
        ht.InsertCombine(win_func_key, MakeWinFuncValue(&cur_tuple, sort_key, window_function.function_));
        values[window_func_idx] = ht.ht_[win_func_key].val_;
      }
      for (size_t col_idx = 0; col_idx < plan_->columns_.size(); col_idx++) {
        // 不是占位符
        if (plan_->window_functions_.find(col_idx) == plan_->window_functions_.end()) {
          values[col_idx] = plan_->columns_[col_idx]->Evaluate(&cur_tuple, child_executor_->GetOutputSchema());
        }
      }
      tuples_.emplace_back(values, &GetOutputSchema());
    }
  } else {  // 否则算整个分区的
    // 先扫描一次tuples计算最终结果
    for (const auto &[sort_key, cur_tuple] : tuples) {
      for (const auto &[window_func_idx, window_function] : plan_->window_functions_) {
        ht.InsertCombine(MakeWinFuncKey(&cur_tuple, window_func_idx, window_function.type_),
                         MakeWinFuncValue(&cur_tuple, sort_key, window_function.function_));
      }
    }
    // 再次扫描tuples，由key找到value，与columns列拼成values构造tuple.
    for (const auto &[sort_key, cur_tuple] : tuples) {
      std::vector<Value> values;
      values.resize(plan_->columns_.size());
      for (size_t col_idx = 0; col_idx < plan_->columns_.size(); col_idx++) {
        // 不是占位符
        if (plan_->window_functions_.find(col_idx) == plan_->window_functions_.end()) {
          values[col_idx] = plan_->columns_[col_idx]->Evaluate(&cur_tuple, child_executor_->GetOutputSchema());
        }
      }
      for (const auto &[window_func_idx, window_function] : plan_->window_functions_) {
        values[window_func_idx] = ht.ht_[MakeWinFuncKey(&cur_tuple, window_func_idx, window_function.type_)].val_;
      }
      tuples_.emplace_back(values, &GetOutputSchema());
    }
  }
  it_ = tuples_.begin();
}

auto WindowFunctionExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (it_ == tuples_.end()) {
    return false;
  }
  *tuple = *it_;
  *rid = tuple->GetRid();
  ++it_;
  return true;
}
}  // namespace bustub
