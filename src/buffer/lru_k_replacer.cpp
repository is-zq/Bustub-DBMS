//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"

namespace bustub {
LRUKNode::LRUKNode(size_t k, frame_id_t fid) : k_(k) {}
void LRUKNode::PushHistory(size_t cur_timestamp) {
  history_.emplace_back(cur_timestamp);
  if (history_.size() > k_) {
    history_.pop_front();
  }
}
auto LRUKNode::GetKDistance(size_t cur_timestamp) -> size_t {
  if (history_.size() < k_) {
    return LRUKNode::INF;
  }
  return cur_timestamp - history_.front();
}
auto LRUKNode::GetLRU() -> size_t { return history_.front(); }
void LRUKNode::SetEvictable(bool set_evictable) { is_evictable_ = set_evictable; }
auto LRUKNode::IsEvictable() -> bool { return is_evictable_; }

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  latch_.lock();
  if (curr_size_ == 0) {
    latch_.unlock();
    return false;
  }
  frame_id_t to_evict_id = -1;
  size_t max_dis = 0;
  size_t k_dis;
  size_t llru;
  for (auto &[fid, node] : node_store_) {
    if (!node.IsEvictable()) {
      continue;
    }
    k_dis = node.GetKDistance(current_timestamp_);
    if (k_dis > max_dis) {
      max_dis = k_dis;
      to_evict_id = fid;
      if (k_dis == LRUKNode::INF) {
        llru = node.GetLRU();
      }
    } else if (max_dis == LRUKNode::INF && k_dis == max_dis) {
      size_t lru = node.GetLRU();
      if (lru < llru) {
        to_evict_id = fid;
        llru = lru;
      }
    }
  }
  node_store_.erase(to_evict_id);
  --curr_size_;
  latch_.unlock();

  *frame_id = to_evict_id;
  return true;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  if (frame_id > static_cast<int>(replacer_size_)) {
    throw bustub::ExecutionException("invalid frame_id");
  }
  latch_.lock();
  if (node_store_.find(frame_id) == node_store_.end()) {
    node_store_.emplace(frame_id, LRUKNode(k_, frame_id));
  }
  node_store_.at(frame_id).PushHistory(current_timestamp_++);
  latch_.unlock();
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  if (frame_id > static_cast<int>(replacer_size_)) {
    throw bustub::ExecutionException("invalid frame_id");
  }
  latch_.lock();
  if (node_store_.find(frame_id) == node_store_.end()) {
    latch_.unlock();
    return;
  }
  if (node_store_.at(frame_id).IsEvictable() && !set_evictable) {
    node_store_.at(frame_id).SetEvictable(false);
    --curr_size_;
  } else if (!node_store_.at(frame_id).IsEvictable() && set_evictable) {
    node_store_.at(frame_id).SetEvictable(true);
    ++curr_size_;
  }
  latch_.unlock();
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  latch_.lock();
  if (node_store_.find(frame_id) == node_store_.end()) {
    latch_.unlock();
    return;
  }
  if (!node_store_.at(frame_id).IsEvictable()) {
    latch_.unlock();
    throw bustub::ExecutionException("remove inevictable");
  }
  node_store_.erase(frame_id);
  --curr_size_;
  latch_.unlock();
}

auto LRUKReplacer::Size() -> size_t {
  size_t ret;
  latch_.lock();
  ret = curr_size_;
  latch_.unlock();
  return ret;
}

}  // namespace bustub
