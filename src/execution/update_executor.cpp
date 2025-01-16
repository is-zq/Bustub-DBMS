//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// update_executor.cpp
//
// Identification: src/execution/update_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>

#include "execution/executors/update_executor.h"

namespace bustub {

UpdateExecutor::UpdateExecutor(ExecutorContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void UpdateExecutor::Init() {
  child_executor_->Init();
  updated_ = false;
}

auto UpdateExecutor::Next(Tuple *tuple, [[maybe_unused]] RID *rid) -> bool {
  if (updated_) {
    return false;
  }
  updated_ = true;
  int32_t updated_cnt = 0;
  TableInfo *table_info = exec_ctx_->GetCatalog()->GetTable(plan_->GetTableOid());
  TableHeap *table_heap = table_info->table_.get();
  std::vector<IndexInfo *> indexes = exec_ctx_->GetCatalog()->GetTableIndexes(table_info->name_);
  const Schema &schema = table_info->schema_;
  Tuple old_tuple;
  Tuple new_tuple;
  Tuple old_key;
  Tuple new_key;
  RID old_rid;
  RID new_rid;
  while (child_executor_->Next(&old_tuple, &old_rid)) {
    /* Delete old tuple */
    TupleMeta tuple_meta = table_heap->GetTupleMeta(old_rid);
    tuple_meta.is_deleted_ = true;
    table_heap->UpdateTupleMeta(tuple_meta, old_rid);

    /* Insert new tuple */
    std::vector<Value> values;
    values.reserve(plan_->target_expressions_.size());
    for (const auto &expression : plan_->target_expressions_) {
      values.emplace_back(expression->Evaluate(&old_tuple, schema));
    }
    new_tuple = Tuple{values, &schema};
    new_rid = table_heap->InsertTuple(TupleMeta{0, false}, new_tuple).value();

    /* Update indexes */
    for (IndexInfo *ind_info : indexes) {
      /* Delete old index entry */
      old_key = old_tuple.KeyFromTuple(schema, ind_info->key_schema_, ind_info->index_->GetKeyAttrs());
      ind_info->index_->DeleteEntry(old_key, old_rid, nullptr);
      /* Insert new index entry */
      new_key = new_tuple.KeyFromTuple(schema, ind_info->key_schema_, ind_info->index_->GetKeyAttrs());
      ind_info->index_->InsertEntry(new_key, new_rid, nullptr);
    }

    ++updated_cnt;
  }

  std::vector<Value> values{{TypeId::INTEGER, updated_cnt}};
  *tuple = Tuple(values, &GetOutputSchema());
  return true;
}

}  // namespace bustub
