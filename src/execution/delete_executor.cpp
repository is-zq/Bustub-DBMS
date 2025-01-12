//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// delete_executor.cpp
//
// Identification: src/execution/delete_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>

#include "execution/executors/delete_executor.h"

namespace bustub {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void DeleteExecutor::Init() {
  child_executor_->Init();
  deleted_ = false;
  }

auto DeleteExecutor::Next(Tuple *tuple, [[maybe_unused]] RID *rid) -> bool {
  if (deleted_) {
    return false;
  }
  deleted_ = true;
  int32_t deleted_cnt = 0;
  TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->GetTableOid());
  TableHeap *table_heap = table_info->table_.get();
  std::vector<IndexInfo *> indexes = exec_ctx_->GetCatalog()->GetTableIndexes(table_info->name_);
  const Schema &schema = table_info->schema_;
  Tuple tuple_to_delete;
  Tuple key;
  RID rid_to_delete;
  while (child_executor_->Next(&tuple_to_delete, &rid_to_delete)) {
    TupleMeta tuple_meta = table_heap->GetTupleMeta(rid_to_delete);
    tuple_meta.is_deleted_ = true;
    table_heap->UpdateTupleMeta(tuple_meta, rid_to_delete);

    for (IndexInfo *ind_info : indexes) {
      key = tuple_to_delete.KeyFromTuple(schema, ind_info->key_schema_, ind_info->index_->GetKeyAttrs());
      ind_info->index_->DeleteEntry(key, rid_to_delete, nullptr);
    }

    ++deleted_cnt;
  }

  std::vector<Value> values{{TypeId::INTEGER, deleted_cnt}};
  *tuple = Tuple(values, &GetOutputSchema());
  return true;
}

}  // namespace bustub
