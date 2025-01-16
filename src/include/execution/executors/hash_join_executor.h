//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hash_join_executor.h
//
// Identification: src/include/execution/executors/hash_join_executor.h
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/util/hash_util.h"
#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/hash_join_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

/** HashJoinKey represents a key in an hash join operation */
struct HashJoinKey {
  std::vector<Value> keys_;
  /**
   * Compares two hash join keys for equality.
   * @param other the other hash join key to be compared with
   * @return `true` if both they are same, `false` otherwise
   */
  auto operator==(const HashJoinKey &other) const -> bool {
    for (uint32_t i = 0; i < other.keys_.size(); i++) {
      if (keys_[i].CompareEquals(other.keys_[i]) != CmpBool::CmpTrue) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace bustub

namespace std {

/** Implements std::hash on AggregateKey */
template <>
struct hash<bustub::HashJoinKey> {
  auto operator()(const bustub::HashJoinKey &hash_join_key) const -> std::size_t {
    size_t curr_hash = 0;
    for (const auto &key : hash_join_key.keys_) {
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
 * HashJoinExecutor executes a nested-loop JOIN on two tables.
 */
class HashJoinExecutor : public AbstractExecutor {
 public:
  /**
   * Construct a new HashJoinExecutor instance.
   * @param exec_ctx The executor context
   * @param plan The HashJoin join plan to be executed
   * @param left_child The child executor that produces tuples for the left side of join
   * @param right_child The child executor that produces tuples for the right side of join
   */
  HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                   std::unique_ptr<AbstractExecutor> &&left_child, std::unique_ptr<AbstractExecutor> &&right_child);

  /** Initialize the join */
  void Init() override;

  /**
   * Yield the next tuple from the join.
   * @param[out] tuple The next tuple produced by the join.
   * @param[out] rid The next tuple RID, not used by hash join.
   * @return `true` if a tuple was produced, `false` if there are no more tuples.
   */
  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /** @return The output schema for the join */
  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); };

 private:
  /** @return The left tuple as an HashJoinKey */
  auto MakeLeftHashJoinKey(const Tuple *tuple) -> HashJoinKey {
    std::vector<Value> keys;
    keys.reserve(plan_->left_key_expressions_.size());
    for (const auto &expr : plan_->left_key_expressions_) {
      keys.emplace_back(expr->Evaluate(tuple, left_child_->GetOutputSchema()));
    }
    return {keys};
  }
  /** @return The right tuple as an HashJoinKey */
  auto MakeRightHashJoinKey(const Tuple *tuple) -> HashJoinKey {
    std::vector<Value> keys;
    keys.reserve(plan_->right_key_expressions_.size());
    for (const auto &expr : plan_->right_key_expressions_) {
      keys.emplace_back(expr->Evaluate(tuple, right_child_->GetOutputSchema()));
    }
    return {keys};
  }
  /** The HashJoin plan node to be executed. */
  const HashJoinPlanNode *plan_;
  std::unique_ptr<AbstractExecutor> left_child_;
  std::unique_ptr<AbstractExecutor> right_child_;
  /** 用multimap解决哈希冲突的情况 */
  std::unordered_multimap<HashJoinKey, Tuple> ht_;
  std::unordered_multimap<HashJoinKey, Tuple>::iterator it_;
  std::unordered_multimap<HashJoinKey, Tuple>::iterator end_it_;
  std::vector<Value> cur_left_values_;
};

}  // namespace bustub
