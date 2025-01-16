//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// sort_executor.h
//
// Identification: src/include/execution/executors/sort_executor.h
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/seq_scan_plan.h"
#include "execution/plans/sort_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

/** SortKey represents a key in an order by operation */
struct SortKey {
  std::vector<std::pair<OrderByType, Value>> keys_;

  auto operator<(const SortKey &other) const -> bool {
    for (size_t i = 0; i < keys_.size(); i++) {
      const OrderByType &order_by_type = keys_[i].first;
      const Value &value = keys_[i].second;
      const Value &o_value = other.keys_[i].second;
      if (value.CompareEquals(o_value) == CmpBool::CmpTrue) {
        continue;
      }
      if ((order_by_type == OrderByType::DEFAULT || order_by_type == OrderByType::ASC)) {
        return value.CompareLessThan(o_value) == CmpBool::CmpTrue;
      }
      return value.CompareGreaterThan(o_value) == CmpBool::CmpTrue;
    }
    return false;
  }

  auto operator==(const SortKey &other) const -> bool {
    if (other.keys_.size() != keys_.size()) {
      return false;
    }
    for (size_t i = 0; i < keys_.size(); i++) {
      const Value &value = keys_[i].second;
      const Value &o_value = other.keys_[i].second;
      if (value.CompareEquals(o_value) != CmpBool::CmpTrue) {
        return false;
      }
    }
    return true;
  }
};

/**
 * The SortExecutor executor executes a sort.
 */
class SortExecutor : public AbstractExecutor {
 public:
  /**
   * Construct a new SortExecutor instance.
   * @param exec_ctx The executor context
   * @param plan The sort plan to be executed
   */
  SortExecutor(ExecutorContext *exec_ctx, const SortPlanNode *plan, std::unique_ptr<AbstractExecutor> &&child_executor);

  /** Initialize the sort */
  void Init() override;

  /**
   * Yield the next tuple from the sort.
   * @param[out] tuple The next tuple produced by the sort
   * @param[out] rid The next tuple RID produced by the sort
   * @return `true` if a tuple was produced, `false` if there are no more tuples
   */
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the sort */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

 private:
  /** The sort plan node to be executed */
  const SortPlanNode *plan_;

  std::unique_ptr<AbstractExecutor> child_executor_;
  std::vector<std::pair<SortKey, Tuple>> tuples_;
  std::vector<std::pair<SortKey, Tuple>>::iterator it_;
};
}  // namespace bustub
