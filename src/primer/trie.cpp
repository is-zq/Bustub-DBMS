#include "primer/trie.h"
#include <string_view>
#include "common/exception.h"

namespace bustub {

template <class T>
auto Trie::Get(std::string_view key) const -> const T * {
  std::shared_ptr<const TrieNode> node = GetRoot();
  size_t len = key.size();
  if (node == nullptr) {
    return nullptr;
  }
  size_t ind = 0;
  while (ind < len) {
    if (node->children_.find(key[ind]) == node->children_.end()) {
      return nullptr;
    }
    node = node->children_.at(key[ind++]);
  }
  if (!node->is_value_node_) {
    return nullptr;
  }

  auto val_node = std::dynamic_pointer_cast<const TrieNodeWithValue<T>>(node);
  if (val_node == nullptr) {
    return nullptr;
  }
  return val_node->value_.get();

  // You should walk through the trie to find the node corresponding to the key. If the node doesn't exist, return
  // nullptr. After you find the node, you should use `dynamic_cast` to cast it to `const TrieNodeWithValue<T> *`. If
  // dynamic_cast returns `nullptr`, it means the type of the value is mismatched, and you should return nullptr.
  // Otherwise, return the value.
}

template <class T>
auto Trie::Put(std::string_view key, T value) const -> Trie {
  auto root = this->GetRoot();
  size_t len = key.size();
  std::shared_ptr<TrieNode> new_root;
  if (root == nullptr) {
    if (len == 0) {
      new_root = std::make_shared<TrieNodeWithValue<T>>(std::map<char, std::shared_ptr<const TrieNode>>(),
                                                        std::make_shared<T>(std::move(value)));
      return Trie(new_root);
    }
    new_root = std::make_shared<TrieNode>();
  } else {
    if (len == 0) {
      new_root = std::make_shared<TrieNodeWithValue<T>>(root->children_, std::make_shared<T>(std::move(value)));
      return Trie(new_root);
    }
    new_root = root->Clone();
  }

  auto cur_node = new_root;
  for (size_t ind = 0; ind < len; ind++) {
    std::shared_ptr<TrieNode> new_node;
    if (cur_node->children_.find(key[ind]) == cur_node->children_.end()) {
      if (ind == len - 1) {
        new_node = std::make_shared<TrieNodeWithValue<T>>(std::map<char, std::shared_ptr<const TrieNode>>(),
                                                          std::make_shared<T>(std::move(value)));
        cur_node->children_[key[ind]] = new_node;
        return Trie(new_root);
      }
      new_node = std::make_shared<TrieNode>();
      cur_node->children_.emplace(key[ind], new_node);
    } else {
      if (ind == len - 1) {
        new_node = std::make_shared<TrieNodeWithValue<T>>(cur_node->children_[key[ind]]->children_,
                                                          std::make_shared<T>(std::move(value)));
        cur_node->children_[key[ind]] = new_node;
        return Trie(new_root);
      }
      new_node = cur_node->children_[key[ind]]->Clone();
      cur_node->children_[key[ind]] = new_node;
    }
    cur_node = new_node;
  }

  return Trie(new_root);

  // Note that `T` might be a non-copyable type. Always use `std::move` when creating `shared_ptr` on that value.

  // You should walk through the trie and create new nodes if necessary. If the node corresponding to the key already
  // exists, you should create a new `TrieNodeWithValue`.
}

auto Trie::RemoveHelper(std::string_view key, size_t ind, const std::shared_ptr<TrieNode> &node) const -> bool {
  if (node == nullptr) {
    return true;
  }
  if (ind == key.size()) {
    return node->children_.empty();
  }
  if (node->children_.find(key[ind]) == node->children_.end()) {
    return false;
  }

  std::shared_ptr<TrieNode> next_child = node->children_[key[ind]]->Clone();
  bool purge = RemoveHelper(key, ind + 1, next_child);
  if (purge) {
    node->children_.erase(key[ind]);
  } else {
    if (ind == key.size() - 1) {
      node->children_[key[ind]] = std::make_shared<TrieNode>(next_child->children_);
    } else {
      node->children_[key[ind]] = std::move(next_child);
    }
  }

  return !node->is_value_node_ && node->children_.empty();
}

auto Trie::Remove(std::string_view key) const -> Trie {
  auto root = this->GetRoot();
  if (root == nullptr) {
    return Trie(nullptr);
  }

  if (key.empty()) {
    return Trie(std::make_shared<TrieNode>(root->children_));
  }

  std::shared_ptr<TrieNode> new_root = root->Clone();
  bool purge = RemoveHelper(key, 0, new_root);
  if (purge) {
    return Trie(nullptr);
  }

  return Trie(new_root);

  // You should walk through the trie and remove nodes if necessary. If the node doesn't contain a value any more,
  // you should convert it to `TrieNode`. If a node doesn't have children any more, you should remove it.
}

// Below are explicit instantiation of template functions.
//
// Generally people would write the implementation of template classes and functions in the header file. However, we
// separate the implementation into a .cpp file to make things clearer. In order to make the compiler know the
// implementation of the template functions, we need to explicitly instantiate them here, so that they can be picked up
// by the linker.

template auto Trie::Put(std::string_view key, uint32_t value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const uint32_t *;

template auto Trie::Put(std::string_view key, uint64_t value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const uint64_t *;

template auto Trie::Put(std::string_view key, std::string value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const std::string *;

// If your solution cannot compile for non-copy tests, you can remove the below lines to get partial score.

using Integer = std::unique_ptr<uint32_t>;

template auto Trie::Put(std::string_view key, Integer value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const Integer *;

template auto Trie::Put(std::string_view key, MoveBlocked value) const -> Trie;
template auto Trie::Get(std::string_view key) const -> const MoveBlocked *;

}  // namespace bustub
