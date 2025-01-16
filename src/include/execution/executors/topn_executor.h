//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// topn_executor.h
//
// Identification: src/include/execution/executors/topn_executor.h
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/seq_scan_plan.h"
#include "execution/plans/topn_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

/** TopNKey represents a key in an order by operation */
struct TopNElem {
  std::vector<std::pair<OrderByType, Value>> keys_;
  Tuple tuple_;

  TopNElem() = default;
  TopNElem(std::vector<std::pair<OrderByType, Value>> &&keys, Tuple &&tuple)
      : keys_(std::move(keys)), tuple_(std::move(tuple)) {}

  auto operator<(const TopNElem &other) const -> bool {
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
  auto operator>(const TopNElem &other) const -> bool {
    for (size_t i = 0; i < keys_.size(); i++) {
      const OrderByType &order_by_type = keys_[i].first;
      const Value &value = keys_[i].second;
      const Value &o_value = other.keys_[i].second;
      if (value.CompareEquals(o_value) == CmpBool::CmpTrue) {
        continue;
      }
      if ((order_by_type == OrderByType::DEFAULT || order_by_type == OrderByType::ASC)) {
        return value.CompareGreaterThan(o_value) == CmpBool::CmpTrue;
      }
      return value.CompareLessThan(o_value) == CmpBool::CmpTrue;
    }
    return false;
  }
};

/**
 * The TopNExecutor executor executes a topn.
 */
class TopNExecutor : public AbstractExecutor {
 public:
  /**
   * Construct a new TopNExecutor instance.
   * @param exec_ctx The executor context
   * @param plan The TopN plan to be executed
   */
  TopNExecutor(ExecutorContext *exec_ctx, const TopNPlanNode *plan, std::unique_ptr<AbstractExecutor> &&child_executor);

  /** Initialize the TopN */
  void Init() override;

  /**
   * Yield the next tuple from the TopN.
   * @param[out] tuple The next tuple produced by the TopN
   * @param[out] rid The next tuple RID produced by the TopN
   * @return `true` if a tuple was produced, `false` if there are no more tuples
   */
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the TopN */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

  /** Sets new child executor (for testing only) */
  void SetChildExecutor(std::unique_ptr<AbstractExecutor> &&child_executor) {
    child_executor_ = std::move(child_executor);
  }

  /** @return The size of top_entries_ container, which will be called on each child_executor->Next(). */
  auto GetNumInHeap() -> size_t;

 private:
  /** The TopN plan node to be executed */
  const TopNPlanNode *plan_;
  /** The child executor from which tuples are obtained */
  std::unique_ptr<AbstractExecutor> child_executor_;

  std::priority_queue<TopNElem, std::vector<TopNElem>, std::greater<>> min_heap_;
};
}  // namespace bustub
