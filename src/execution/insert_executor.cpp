//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "execution/executors/insert_executor.h"

namespace bustub {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  child_executor_->Init();
  inserted_ = false;
  }

auto InsertExecutor::Next(Tuple *tuple, [[maybe_unused]] RID *rid) -> bool {
  if (inserted_) {
    return false;
  }
  inserted_ = true;
  int32_t inserted_cnt = 0;
  TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->GetTableOid());
  TableHeap *table_heap = table_info->table_.get();
  std::vector<IndexInfo *> indexes = exec_ctx_->GetCatalog()->GetTableIndexes(table_info->name_);
  const Schema &schema = table_info->schema_;
  Tuple tuple_to_insert;
  Tuple key;
  RID rid_to_insert;
  while (child_executor_->Next(&tuple_to_insert, rid)) {
    rid_to_insert = table_heap->InsertTuple(TupleMeta{0, false}, tuple_to_insert).value();
    for (IndexInfo *ind_info : indexes) {
      key = tuple_to_insert.KeyFromTuple(schema, ind_info->key_schema_, ind_info->index_->GetKeyAttrs());
      ind_info->index_->InsertEntry(key, rid_to_insert, nullptr);
    }
    ++inserted_cnt;
  }

  std::vector<Value> values{{TypeId::INTEGER, inserted_cnt}};
  *tuple = Tuple(values, &GetOutputSchema());
  return true;
}

}  // namespace bustub
