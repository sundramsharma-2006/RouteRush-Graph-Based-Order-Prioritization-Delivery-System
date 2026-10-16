// ===== MEMBER 2 (Samriddhi Agarwal) : hash map with separate chaining =====
// Hand-written (not std::unordered_map) so the data structure itself is demonstrated.
// Average O(1) insert/find/erase; table doubles when load factor exceeds 0.75.
#pragma once
#include <cstddef>
#include <string>
#include <vector>

template <typename V>
class HashMap {
    struct Node { std::string key; V value; Node* next; };
public:
    explicit HashMap(std::size_t initialBuckets = 16) : buckets_(initialBuckets ? initialBuckets : 16, nullptr) {}
    ~HashMap() { clear(); }
    HashMap(const HashMap&) = delete;             // owning raw pointers: forbid accidental shallow copies
    HashMap& operator=(const HashMap&) = delete;

    // Returns true if a new key was inserted, false if an existing key was overwritten.
    bool insert(const std::string& key, const V& value) {
        if (V* existing = find(key)) { *existing = value; return false; }
        if (static_cast<double>(count_ + 1) / buckets_.size() > kMaxLoad) rehash(buckets_.size() * 2);
        std::size_t b = hash(key) % buckets_.size();
        buckets_[b] = new Node{key, value, buckets_[b]};
        ++count_;
        return true;
    }
    V* find(const std::string& key) {
        for (Node* n = buckets_[hash(key) % buckets_.size()]; n; n = n->next)
            if (n->key == key) return &n->value;
        return nullptr;
    }
    const V* find(const std::string& key) const {
        for (const Node* n = buckets_[hash(key) % buckets_.size()]; n; n = n->next)
            if (n->key == key) return &n->value;
        return nullptr;
    }
    bool erase(const std::string& key) {
        Node** link = &buckets_[hash(key) % buckets_.size()];
        while (*link) {
            if ((*link)->key == key) { Node* dead = *link; *link = dead->next; delete dead; --count_; return true; }
            link = &(*link)->next;
        }
        return false;
    }
    template <typename Fn> void forEach(Fn fn) const {
        for (Node* head : buckets_) for (Node* n = head; n; n = n->next) fn(n->key, n->value);
    }
    void clear() {
        for (Node*& head : buckets_) {
            while (head) { Node* nx = head->next; delete head; head = nx; }
        }
        count_ = 0;
    }
    std::size_t size() const { return count_; }
    std::size_t bucketCount() const { return buckets_.size(); }

private:
    static constexpr double kMaxLoad = 0.75;
    static std::size_t hash(const std::string& s) {      // djb2
        std::size_t h = 5381;
        for (unsigned char c : s) h = h * 33 + c;
        return h;
    }
    void rehash(std::size_t newSize) {
        std::vector<Node*> fresh(newSize, nullptr);
        for (Node* head : buckets_) {
            while (head) {
                Node* nx = head->next;
                std::size_t b = hash(head->key) % newSize;
                head->next = fresh[b];
                fresh[b] = head;
                head = nx;
            }
        }
        buckets_.swap(fresh);
    }
    std::vector<Node*> buckets_;
    std::size_t count_ = 0;
};
