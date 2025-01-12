//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.cpp
//
// Identification: src/execution/index_scan_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "execution/executors/index_scan_executor.h"

namespace bustub {
IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

void IndexScanExecutor::Init() {
  table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_oid_);
  index_info_ = exec_ctx_->GetCatalog()->GetIndex(plan_->index_oid_);
  retrieved_ = false;
}

auto IndexScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  if (retrieved_) {
    return false;
  }
  retrieved_ = true;
  if (plan_->pred_key_ == nullptr) {
    return false;
  }
  auto htable = dynamic_cast<HashTableIndexForTwoIntegerColumn *>(index_info_->index_.get());
  Schema key_schema(std::vector<Column>{Column{std::string("key"), TypeId::INTEGER}});  // 只支持单一Integer的索引
  Tuple key_tuple(std::vector<Value>{plan_->pred_key_->Evaluate(nullptr, key_schema)}, &key_schema);
  std::vector<RID> result;
  htable->ScanKey(key_tuple, &result, nullptr);
  if (result.empty()) {
    return false;
  }
  auto [ret_tuple_meta, ret_tuple] = table_info_->table_->GetTuple(result[0]);
  if (ret_tuple_meta.is_deleted_) {
    return false;
  }
  if (plan_->filter_predicate_ != nullptr && !PredCmp(ret_tuple)) {
    return false;
  }
  *tuple = ret_tuple;
  *rid = result[0];
  return true;
}

}  // namespace bustub
