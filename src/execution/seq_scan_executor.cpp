//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// seq_scan_executor.cpp
//
// Identification: src/execution/seq_scan_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/seq_scan_executor.h"

namespace bustub {

SeqScanExecutor::SeqScanExecutor(ExecutorContext *exec_ctx, const SeqScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan), table_info_(exec_ctx->GetCatalog()->GetTable(plan_->table_oid_)) {
  TableIterator table_it(table_info_->table_->MakeIterator());
  while (!table_it.IsEnd()) {
    rids_.emplace_back(table_it.GetRID());
    ++table_it;
  }
}

void SeqScanExecutor::Init() { rid_it_ = rids_.begin(); }

auto SeqScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  while (rid_it_ != rids_.end()) {
    RID next_rid = *rid_it_;
    auto [next_tuple_meta, next_tuple] = table_info_->table_->GetTuple(next_rid);
    ++rid_it_;
    if (!next_tuple_meta.is_deleted_ && (plan_->filter_predicate_ == nullptr || PredCmp(next_tuple))) {
      *tuple = next_tuple;
      *rid = next_rid;
      return true;
    }
  }

  return false;
}

}  // namespace bustub
