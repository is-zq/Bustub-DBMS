#include "storage/page/page_guard.h"
#include "buffer/buffer_pool_manager.h"

namespace bustub {

BasicPageGuard::BasicPageGuard(BasicPageGuard &&that) noexcept {
  bpm_ = that.bpm_;
  page_ = that.page_;
  is_dirty_ = that.is_dirty_;

  that.bpm_ = nullptr;
  that.page_ = nullptr;
  that.is_dirty_ = false;
}

void BasicPageGuard::Drop() {
  bpm_->UnpinPage(page_->GetPageId(), is_dirty_);
  bpm_ = nullptr;
  page_ = nullptr;
  is_dirty_ = false;
}

auto BasicPageGuard::operator=(BasicPageGuard &&that) noexcept -> BasicPageGuard & {
  if (bpm_ != nullptr && page_ != nullptr) {
    Drop();
  }

  bpm_ = that.bpm_;
  page_ = that.page_;
  is_dirty_ = that.is_dirty_;

  that.bpm_ = nullptr;
  that.page_ = nullptr;
  that.is_dirty_ = false;

  return *this;
}

BasicPageGuard::~BasicPageGuard() {
  if (bpm_ != nullptr && page_ != nullptr) {
    Drop();
  }
}

auto BasicPageGuard::UpgradeRead() -> ReadPageGuard {
  if (is_dirty_) {
    bpm_->FlushPage(page_->GetPageId());
  }
  ReadPageGuard rpg(bpm_, page_);

  bpm_ = nullptr;
  page_ = nullptr;
  is_dirty_ = false;

  return rpg;
}

auto BasicPageGuard::UpgradeWrite() -> WritePageGuard {
  WritePageGuard wpg(bpm_, page_);

  bpm_ = nullptr;
  page_ = nullptr;
  is_dirty_ = false;

  return wpg;
}

ReadPageGuard::ReadPageGuard(ReadPageGuard &&that) noexcept : guard_(std::move(that.guard_)) {}

auto ReadPageGuard::operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard & {
  guard_ = std::move(that.guard_);
  return *this;
}

void ReadPageGuard::Drop() {
  guard_.bpm_->UnpinPage(guard_.page_->GetPageId(), guard_.is_dirty_);
  guard_.page_->RUnlatch();
  guard_.bpm_ = nullptr;
  guard_.page_ = nullptr;
  guard_.is_dirty_ = false;
}

ReadPageGuard::~ReadPageGuard() {
  if (guard_.bpm_ != nullptr && guard_.page_ != nullptr) {
    Drop();
  }
}  // NOLINT

WritePageGuard::WritePageGuard(WritePageGuard &&that) noexcept : guard_(std::move(that.guard_)) {}

auto WritePageGuard::operator=(WritePageGuard &&that) noexcept -> WritePageGuard & {
  guard_ = std::move(that.guard_);
  return *this;
}

void WritePageGuard::Drop() {
  guard_.bpm_->UnpinPage(guard_.page_->GetPageId(), guard_.is_dirty_);
  guard_.page_->WUnlatch();
  guard_.bpm_ = nullptr;
  guard_.page_ = nullptr;
  guard_.is_dirty_ = false;
}

WritePageGuard::~WritePageGuard() {
  if (guard_.bpm_ != nullptr && guard_.page_ != nullptr) {
    Drop();
  }
}  // NOLINT

}  // namespace bustub
