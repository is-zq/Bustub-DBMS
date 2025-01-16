//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// window_function_executor.h
//
// Identification: src/include/execution/executors/window_function_executor.h
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/executors/sort_executor.h"
#include "execution/plans/window_plan.h"
#include "storage/table/tuple.h"
#include "type/value_factory.h"

namespace bustub {

/** WinFuncKey represents a key in an window function operation */
struct WinFuncKey {
  /** The window_func index */
  uint32_t win_func_idx_;
  /** The window_func type */
  WindowFunctionType win_func_type_;
  /** The partition-by values */
  std::vector<Value> partition_bys_;

  /**
   * Compares two window function keys for equality.
   * @param other the other window function key to be compared with
   * @return `true` if both window function keys have equivalent partition-by expressions, `false` otherwise
   */
  auto operator==(const WinFuncKey &other) const -> bool {
    if (other.win_func_idx_ != win_func_idx_) {
      return false;
    }
    if (other.partition_bys_.size() != partition_bys_.size()) {
      return false;
    }
    for (uint32_t i = 0; i < other.partition_bys_.size(); i++) {
      if (partition_bys_[i].CompareEquals(other.partition_bys_[i]) != CmpBool::CmpTrue) {
        return false;
      }
    }
    return true;
  }
  auto operator!=(const WinFuncKey &other) const -> bool { return !(*this == other); }
};

/** WinFuncKey represents a value in an window function operation */
struct WinFuncVal {
  Value val_;
  SortKey sort_key_;
  int cur_rank_;
  int prev_rank_;
};

}  // namespace bustub

namespace std {

/** Implements std::hash on WinFuncKey */
template <>
struct hash<bustub::WinFuncKey> {
  auto operator()(const bustub::WinFuncKey &win_func_key) const -> std::size_t {
    size_t curr_hash = std::hash<uint32_t>()(win_func_key.win_func_idx_);
    for (const auto &key : win_func_key.partition_bys_) {
      if (!key.IsNull()) {
        curr_hash = bustub::HashUtil::CombineHashes(curr_hash, bustub::HashUtil::HashValue(&key));
      }
    }
    return curr_hash;
  }
};

}  // namespace std

namespace bustub {

/**
 * A simplified hash table that has all the necessary functionality for window function.
 */
class SimpleWindowFunctionHashTable {
 public:
  /** @return The initial window function value for this window function executor */
  auto GenerateInitialWinFuncValue(const WindowFunctionType &win_func_type) -> WinFuncVal {
    switch (win_func_type) {
      case WindowFunctionType::Rank:
      case WindowFunctionType::CountStarAggregate:
        // Count start starts at zero.
        return {ValueFactory::GetIntegerValue(0), SortKey{}, 1, 1};
      case WindowFunctionType::CountAggregate:
      case WindowFunctionType::SumAggregate:
      case WindowFunctionType::MinAggregate:
      case WindowFunctionType::MaxAggregate:
        // Others starts at null.
        return {ValueFactory::GetNullValueByType(TypeId::INTEGER), SortKey{}, 1, 1};
    }
    return {};
  }

  /**
   * TODO(Student)
   *
   * Combines the input into the window function result.
   * @param[out] result The output window function value
   * @param input The input value
   */
  void CombineWinFuncValues(WinFuncVal *result, const WinFuncVal &input, const WindowFunctionType &win_func_type) {
    switch (win_func_type) {
      case WindowFunctionType::Rank:
        if (result->sort_key_ == input.sort_key_) {
          result->val_ = ValueFactory::GetIntegerValue(result->prev_rank_);
        } else {
          result->val_ = ValueFactory::GetIntegerValue(result->cur_rank_);
          result->prev_rank_ = result->cur_rank_;
          result->sort_key_ = input.sort_key_;
        }
        result->cur_rank_++;
        break;
      case WindowFunctionType::CountStarAggregate:
        result->val_ = result->val_.Add(input.val_);
        break;
      case WindowFunctionType::CountAggregate:
        if (!input.val_.IsNull()) {
          if (result->val_.IsNull()) {
            result->val_ = ValueFactory::GetIntegerValue(1);
          } else {
            result->val_ = result->val_.Add(ValueFactory::GetIntegerValue(1));
          }
        }
        break;
      case WindowFunctionType::SumAggregate:
        if (result->val_.IsNull()) {
          result->val_ = input.val_;
        } else if (!input.val_.IsNull()) {
          result->val_ = result->val_.Add(input.val_);
        }
        break;
      case WindowFunctionType::MinAggregate:
        if (result->val_.IsNull()) {
          result->val_ = input.val_;
        } else if (!input.val_.IsNull()) {
          result->val_ = result->val_.Min(input.val_);
        }
        break;
      case WindowFunctionType::MaxAggregate:
        if (result->val_.IsNull()) {
          result->val_ = input.val_;
        } else if (!input.val_.IsNull()) {
          result->val_ = result->val_.Max(input.val_);
        }
        break;
    }
  }

  /**
   * Inserts a value into the hash table and then combines it with the current window function.
   * @param win_func_key the key to be inserted
   * @param win_func_val the value to be inserted
   */
  void InsertCombine(const WinFuncKey &win_func_key, const WinFuncVal &win_func_val) {
    if (ht_.count(win_func_key) == 0) {
      ht_.insert({win_func_key, GenerateInitialWinFuncValue(win_func_key.win_func_type_)});
    }
    CombineWinFuncValues(&ht_[win_func_key], win_func_val, win_func_key.win_func_type_);
  }

  /**
   * Clear the hash table
   */
  void Clear() { ht_.clear(); }

  /** The hash table is just a map from window function keys to window function values */
  std::unordered_map<WinFuncKey, WinFuncVal> ht_{};
};

/**
 * The WindowFunctionExecutor executor executes a window function for columns using window function.
 *
 * Window function is different from normal aggregation as it outputs one row for each inputing rows,
 * and can be combined with normal selected columns. The columns in WindowFunctionPlanNode contains both
 * normal selected columns and placeholder columns for window functions.
 *
 * For example, if we have a query like:
 *    SELECT 0.1, 0.2, SUM(0.3) OVER (PARTITION BY 0.2 ORDER BY 0.3), SUM(0.4) OVER (PARTITION BY 0.1 ORDER BY 0.2,0.3)
 *      FROM table;
 *
 * The WindowFunctionPlanNode contains following structure:
 *    columns: std::vector<AbstractExpressionRef>{0.1, 0.2, 0.-1(placeholder), 0.-1(placeholder)}
 *    window_functions_: {
 *      3: {
 *        partition_by: std::vector<AbstractExpressionRef>{0.2}
 *        order_by: std::vector<AbstractExpressionRef>{0.3}
 *        functions: std::vector<AbstractExpressionRef>{0.3}
 *        window_func_type: WindowFunctionType::SumAggregate
 *      }
 *      4: {
 *        partition_by: std::vector<AbstractExpressionRef>{0.1}
 *        order_by: std::vector<AbstractExpressionRef>{0.2,0.3}
 *        functions: std::vector<AbstractExpressionRef>{0.4}
 *        window_func_type: WindowFunctionType::SumAggregate
 *      }
 *    }
 *
 * Your executor should use child executor and exprs in columns to produce selected columns except for window
 * function columns, and use window_agg_indexes, partition_bys, order_bys, functionss and window_agg_types to
 * generate window function columns results. Directly use placeholders for window function columns in columns is
 * not allowed, as it contains invalid column id.
 *
 * Your WindowFunctionExecutor does not need to support specified window frames (eg: 1 preceding and 1 following).
 * You can assume that all window frames are UNBOUNDED FOLLOWING AND CURRENT ROW when there is ORDER BY clause, and
 * UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING when there is no ORDER BY clause.
 *
 */
class WindowFunctionExecutor : public AbstractExecutor {
 public:
  /**
   * Construct a new WindowFunctionExecutor instance.
   * @param exec_ctx The executor context
   * @param plan The window aggregation plan to be executed
   */
  WindowFunctionExecutor(ExecutorContext *exec_ctx, const WindowFunctionPlanNode *plan,
                         std::unique_ptr<AbstractExecutor> &&child_executor);

  /** Initialize the window aggregation */
  void Init() override;

  /**
   * Yield the next tuple from the window aggregation.
   * @param[out] tuple The next tuple produced by the window aggregation
   * @param[out] rid The next tuple RID produced by the window aggregation
   * @return `true` if a tuple was produced, `false` if there are no more tuples
   */
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the window aggregation plan */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

 private:
  /** @return The tuple as an WinFuncKey */
  auto MakeWinFuncKey(const Tuple *tuple, uint32_t win_func_idx, WindowFunctionType win_func_type) -> WinFuncKey {
    std::vector<Value> keys;
    const auto &partition_bys = plan_->window_functions_.at(win_func_idx).partition_by_;
    keys.reserve(partition_bys.size());
    for (const auto &expr : partition_bys) {
      keys.emplace_back(expr->Evaluate(tuple, child_executor_->GetOutputSchema()));
    }
    return {win_func_idx, win_func_type, std::move(keys)};
  }
  /** @return The tuple as an WinFuncVal */
  auto MakeWinFuncValue(const Tuple *tuple, SortKey sort_key, const AbstractExpressionRef &function) -> WinFuncVal {
    return {function->Evaluate(tuple, child_executor_->GetOutputSchema()), std::move(sort_key), 1, 1};
  }

  /** The window aggregation plan node to be executed */
  const WindowFunctionPlanNode *plan_;

  /** The child executor from which tuples are obtained */
  std::unique_ptr<AbstractExecutor> child_executor_;

  std::vector<Tuple> tuples_;
  std::vector<Tuple>::iterator it_;
};

}  // namespace bustub
