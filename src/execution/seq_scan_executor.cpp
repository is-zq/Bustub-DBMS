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
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      table_info_(exec_ctx->GetCatalog()->GetTable(plan_->table_oid_)),
      table_it_(table_info_->table_->MakeIterator()) {}

void SeqScanExecutor::Init() {}

auto SeqScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (table_it_.IsEnd()) {
    return false;
  }

  do {
    auto [next_tuple_meta, next_tuple] = table_it_.GetTuple();
    ++table_it_;
    if (!next_tuple_meta.is_deleted_ && (plan_->filter_predicate_ == nullptr || PredCmp(next_tuple))) {
      *tuple = next_tuple;
      *rid = tuple->GetRid();
      return true;
    }
  } while (!table_it_.IsEnd());

  return false;
}

}  // namespace bustub
