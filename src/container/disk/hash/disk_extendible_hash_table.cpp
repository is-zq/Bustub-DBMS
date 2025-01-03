//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_extendible_hash_table.cpp
//
// Identification: src/container/disk/hash/disk_extendible_hash_table.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "common/rid.h"
#include "common/util/hash_util.h"
#include "container/disk/hash/disk_extendible_hash_table.h"
#include "storage/index/hash_comparator.h"
#include "storage/page/extendible_htable_bucket_page.h"
#include "storage/page/extendible_htable_directory_page.h"
#include "storage/page/extendible_htable_header_page.h"
#include "storage/page/page_guard.h"

namespace bustub {

template <typename K, typename V, typename KC>
DiskExtendibleHashTable<K, V, KC>::DiskExtendibleHashTable(std::string name, BufferPoolManager *bpm, const KC &cmp,
                                                           const HashFunction<K> &hash_fn, uint32_t header_max_depth,
                                                           uint32_t directory_max_depth, uint32_t bucket_max_size)
    : index_name_(std::move(name)),
      bpm_(bpm),
      cmp_(cmp),
      hash_fn_(std::move(hash_fn)),
      header_max_depth_(header_max_depth),
      directory_max_depth_(directory_max_depth),
      bucket_max_size_(bucket_max_size) {
  WritePageGuard wpg((bpm_->NewPageGuarded(&header_page_id_)).UpgradeWrite());
  auto *header = wpg.AsMut<ExtendibleHTableHeaderPage>();
  header->Init(header_max_depth_);
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::GetValue(const K &key, std::vector<V> *result, Transaction *transaction) const
    -> bool {
  /* 得到header */
  if (header_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  uint32_t hash = Hash(key);
  ReadPageGuard hrpg(bpm_->FetchPageRead(header_page_id_));
  const auto *header = hrpg.As<ExtendibleHTableHeaderPage>();
  /* 得到directory */
  page_id_t directory_page_id = header->GetDirectoryPageId(header->HashToDirectoryIndex(hash));
  if (directory_page_id == INVALID_PAGE_ID) {
    return false;
  }
  hrpg.Drop();  // 用完header就可以提前释放掉了
  ReadPageGuard drpg(bpm_->FetchPageRead(directory_page_id));
  const auto *directory = drpg.As<ExtendibleHTableDirectoryPage>();
  /* 得到bucket */
  page_id_t bucket_page_id = directory->GetBucketPageId(directory->HashToBucketIndex(hash));
  if (bucket_page_id == INVALID_PAGE_ID) {
    return false;
  }
  drpg.Drop();
  ReadPageGuard brpg(bpm_->FetchPageRead(bucket_page_id));
  const auto *bucket = brpg.As<ExtendibleHTableBucketPage<K, V, KC>>();
  /* 查找 */
  V val;
  if (!bucket->Lookup(key, val, cmp_)) {
    return false;
  }
  result->emplace_back(val);
  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Insert(const K &key, const V &value, Transaction *transaction) -> bool {
  /* 得到header */
  if (header_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  uint32_t hash = Hash(key);
  WritePageGuard hwpg(bpm_->FetchPageWrite(header_page_id_));
  auto *header = hwpg.AsMut<ExtendibleHTableHeaderPage>();
  /* 得到directory */
  uint32_t directory_idx = header->HashToDirectoryIndex(hash);
  page_id_t directory_page_id = header->GetDirectoryPageId(directory_idx);
  if (directory_page_id == INVALID_PAGE_ID) {
    return InsertToNewDirectory(header, directory_idx, hash, key, value);
  }
  hwpg.Drop();
  WritePageGuard dwpg(bpm_->FetchPageWrite(directory_page_id));
  auto *directory = dwpg.AsMut<ExtendibleHTableDirectoryPage>();
  /* 得到bucket */
  uint32_t bucket_idx = directory->HashToBucketIndex(hash);
  page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);
  if (bucket_page_id == INVALID_PAGE_ID) {
    return InsertToNewBucket(directory, bucket_idx, key, value);
  }
  WritePageGuard bwpg(bpm_->FetchPageWrite(bucket_page_id));
  auto *bucket = bwpg.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();

  while (bucket->IsFull()) {
    /* 若ld == gd, directory扩容, gd++ */
    if (directory->GetLocalDepth(bucket_idx) == directory->GetGlobalDepth()) {
      if (directory->GetGlobalDepth() == directory->GetMaxDepth()) {
        return false;
      }
      directory->IncrGlobalDepth();
      bucket_idx = directory->HashToBucketIndex(hash);
    }
    /* 收集元素并清空旧桶 */
    std::vector<std::pair<K, V>> vec;
    for (uint32_t i = 0; i < bucket->Size(); i++) {
      vec.emplace_back(bucket->EntryAt(i));
    }
    bucket->Init(bucket_max_size_);
    /* 创建新桶 */
    page_id_t new_bucket_page_id;
    WritePageGuard nbwpg((bpm_->NewPageGuarded(&new_bucket_page_id)).UpgradeWrite());
    if (new_bucket_page_id == INVALID_PAGE_ID) {
      return false;
    }
    auto *new_bucket = nbwpg.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
    new_bucket->Init(bucket_max_size_);
    /* 重新映射 */
    directory->IncrLocalDepth(bucket_idx);
    uint32_t local_depth = directory->GetLocalDepth(bucket_idx);
    uint32_t local_depth_mask = directory->GetLocalDepthMask(bucket_idx);
    UpdateDirectoryMapping(directory, bucket_idx & local_depth_mask, bucket_page_id, local_depth,
                           local_depth_mask);  // 分裂后与bucket_idx指向相同桶的, 依然指向原来的桶
    UpdateDirectoryMapping(directory, (bucket_idx ^ (1 << (local_depth - 1))) & local_depth_mask, new_bucket_page_id,
                           local_depth,
                           local_depth_mask);  // 分裂后与bucket_idx指向不同桶的, 指向新桶
    /* 重新分配 */
    for (auto &[k, v] : vec) {
      uint32_t tmp_bucket_idx = directory->HashToBucketIndex(Hash(k));
      page_id_t tmp_bucket_page_id = directory->GetBucketPageId(tmp_bucket_idx);
      BasicPageGuard bpg(bpm_->FetchPageBasic(tmp_bucket_page_id));  // 已获得锁, 用BasicPageGuard
      auto *tmp_bucket = bpg.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
      tmp_bucket->Insert(k, v, cmp_);
    }
  }

  return bucket->Insert(key, value, cmp_);
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewDirectory(ExtendibleHTableHeaderPage *header, uint32_t directory_idx,
                                                             uint32_t hash, const K &key, const V &value) -> bool {
  page_id_t directory_page_id;
  WritePageGuard dwpg((bpm_->NewPageGuarded(&directory_page_id)).UpgradeWrite());
  if (directory_page_id == INVALID_PAGE_ID) {
    return false;
  }
  header->SetDirectoryPageId(directory_idx, directory_page_id);
  auto *directory = dwpg.AsMut<ExtendibleHTableDirectoryPage>();
  directory->Init(directory_max_depth_);
  uint32_t bucket_idx = directory->HashToBucketIndex(hash);
  return InsertToNewBucket(directory, bucket_idx, key, value);
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewBucket(ExtendibleHTableDirectoryPage *directory, uint32_t bucket_idx,
                                                          const K &key, const V &value) -> bool {
  page_id_t bucket_page_id;
  WritePageGuard bwpg((bpm_->NewPageGuarded(&bucket_page_id)).UpgradeWrite());
  if (bucket_page_id == INVALID_PAGE_ID) {
    return false;
  }
  directory->SetBucketPageId(bucket_idx, bucket_page_id);
  auto *bucket = bwpg.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
  bucket->Init(bucket_max_size_);
  return bucket->Insert(key, value, cmp_);
}

template <typename K, typename V, typename KC>
void DiskExtendibleHashTable<K, V, KC>::UpdateDirectoryMapping(ExtendibleHTableDirectoryPage *directory,
                                                               uint32_t new_bucket_idx, page_id_t new_bucket_page_id,
                                                               uint32_t new_local_depth, uint32_t local_depth_mask) {
  uint32_t inc = local_depth_mask + 1;
  for (uint32_t i = new_bucket_idx; i < directory->Size(); i += inc) {
    directory->SetBucketPageId(i, new_bucket_page_id);
    directory->SetLocalDepth(i, new_local_depth);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Remove(const K &key, Transaction *transaction) -> bool {
  /* 得到header */
  if (header_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  uint32_t hash = Hash(key);
  WritePageGuard hwpg(bpm_->FetchPageWrite(header_page_id_));
  auto *header = hwpg.AsMut<ExtendibleHTableHeaderPage>();
  /* 得到directory */
  uint32_t directory_idx = header->HashToDirectoryIndex(hash);
  page_id_t directory_page_id = header->GetDirectoryPageId(directory_idx);
  if (directory_page_id == INVALID_PAGE_ID) {
    return false;
  }
  hwpg.Drop();
  WritePageGuard dwpg(bpm_->FetchPageWrite(directory_page_id));
  auto *directory = dwpg.AsMut<ExtendibleHTableDirectoryPage>();
  /* 得到bucket */
  uint32_t bucket_idx = directory->HashToBucketIndex(hash);
  page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);
  if (bucket_page_id == INVALID_PAGE_ID) {
    return false;
  }
  WritePageGuard bwpg(bpm_->FetchPageWrite(bucket_page_id));
  auto *bucket = bwpg.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();

  if (!bucket->Remove(key, cmp_)) {
    return false;
  }

  while (true) {
    /* 得到split image bucket */
    uint32_t local_depth = directory->GetLocalDepth(bucket_idx);
    if (local_depth == 0) {
      break;
    }
    uint32_t split_bucket_idx = bucket_idx ^ (1 << (local_depth - 1));
    if (directory->GetLocalDepth(split_bucket_idx) != local_depth) {  // local depth不相等，不合并
      break;
    }
    page_id_t split_bucket_page_id = directory->GetBucketPageId(split_bucket_idx);
    if (split_bucket_page_id == INVALID_PAGE_ID) {
      return true;
    }
    WritePageGuard sbwpg(bpm_->FetchPageWrite(split_bucket_page_id));
    auto *split_bucket = sbwpg.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
    if (split_bucket->IsEmpty()) {
      /* 删除split bucket的page */
      sbwpg.Drop();
      bpm_->DeletePage(split_bucket_page_id);
      /* 重新映射 */
      directory->DecrLocalDepth(bucket_idx);
      local_depth--;
      uint32_t local_depth_mask = directory->GetLocalDepthMask(bucket_idx);
      UpdateDirectoryMapping(directory, bucket_idx & local_depth_mask, bucket_page_id, local_depth, local_depth_mask);
    } else if (bucket->IsEmpty()) {
      /* 删除bucket的page */
      bwpg.Drop();
      bpm_->DeletePage(bucket_page_id);
      bwpg = std::move(sbwpg);
      bucket = split_bucket;
      /* 重新映射 */
      directory->DecrLocalDepth(split_bucket_idx);
      local_depth--;
      uint32_t local_depth_mask = directory->GetLocalDepthMask(bucket_idx);
      UpdateDirectoryMapping(directory, bucket_idx & local_depth_mask, split_bucket_page_id, local_depth,
                             local_depth_mask);
    } else {
      break;
    }
  }
  /* Shrink */
  uint32_t max_ld = 0;
  for (uint32_t i = 0; i < directory->Size(); i++) {
    max_ld = std::max(max_ld, directory->GetLocalDepth(i));
  }
  uint32_t dec = directory->GetGlobalDepth() - max_ld;
  for (uint32_t i = 0; i < dec; i++) {
    directory->DecrGlobalDepth();
  }

  return true;
}

template class DiskExtendibleHashTable<int, int, IntComparator>;
template class DiskExtendibleHashTable<GenericKey<4>, RID, GenericComparator<4>>;
template class DiskExtendibleHashTable<GenericKey<8>, RID, GenericComparator<8>>;
template class DiskExtendibleHashTable<GenericKey<16>, RID, GenericComparator<16>>;
template class DiskExtendibleHashTable<GenericKey<32>, RID, GenericComparator<32>>;
template class DiskExtendibleHashTable<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
