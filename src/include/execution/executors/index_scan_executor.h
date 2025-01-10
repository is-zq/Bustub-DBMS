//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.h
//
// Identification: src/include/execution/executors/index_scan_executor.h
//
// Copyright (c) 2015-20, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include "common/rid.h"
#include "execution/executor_context.h"
#include "execution/executors/abstract_executor.h"
#include "execution/plans/index_scan_plan.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * IndexScanExecutor executes an index scan over a table.
 */

class IndexScanExecutor : public AbstractExecutor {
 public:
  /**
   * Creates a new index scan executor.
   * @param exec_ctx the executor context
   * @param plan the index scan plan to be executed
   */
  IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan);

  auto GetOutputSchema() const -> const Schema & override { return plan_->OutputSchema(); }

  void Init() override;

  auto Next(Tuple *tuple, RID *rid) -> bool override;

  /**
   * 比较是否满足谓词
   * @return 如果满足返回true，否则返回false
   */
  auto PredCmp(const Tuple &tuple) -> bool {
    return plan_->filter_predicate_->Evaluate(&tuple, table_info_->schema_).GetAs<bool>();
  }

 private:
  /** The index scan plan node to be executed. */
  const IndexScanPlanNode *plan_;

  const TableInfo *table_info_;
  const IndexInfo *index_info_;

  bool retrieved_{false};  // unique key，只取一次
};
}  // namespace bustub
