// ===== MEMBER 2 (Samriddhi Agarwal) : binary heap + priority order queue with aging =====
// Plain priority queues can starve low-priority orders forever. Aging fixes that:
//   key = level * ticksPerLevel - arrivalTick          (level: NORMAL=1, EXPRESS=2, URGENT=3)
// For two orders the difference of keys does not depend on "now", so a static key is enough
// and the heap never needs re-keying. One priority level is worth `ticksPerLevel` ticks of waiting:
// a NORMAL order that has waited more than 2*ticksPerLevel ticks longer than a fresh URGENT order
// outranks it. Small value = priorities matter little; huge value = starvation risk returns.
#pragma once
#include "Types.h"
#include <stdexcept>
#include <vector>

// Max-heap. Less(a,b) == true means "a has LOWER priority than b" (same convention as std::priority_queue).
template <typename T, typename Less>
class BinaryHeap {
public:
    explicit BinaryHeap(Less less = Less()) : less_(less) {}
    void push(const T& v) { data_.push_back(v); siftUp(data_.size() - 1); }
    const T& top() const { if (data_.empty()) throw std::out_of_range("heap empty"); return data_.front(); }
    T pop() {
        if (data_.empty()) throw std::out_of_range("heap empty");
        T result = data_.front();
        data_.front() = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return result;
    }
    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
private:
    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t parent = (i - 1) / 2;
            if (!less_(data_[parent], data_[i])) break;
            std::swap(data_[parent], data_[i]);
            i = parent;
        }
    }
    void siftDown(std::size_t i) {
        for (;;) {
            std::size_t l = 2 * i + 1, r = l + 1, best = i;
            if (l < data_.size() && less_(data_[best], data_[l])) best = l;
            if (r < data_.size() && less_(data_[best], data_[r])) best = r;
            if (best == i) break;
            std::swap(data_[i], data_[best]);
            i = best;
        }
    }
    std::vector<T> data_;
    Less less_;
};

class OrderQueue {
public:
    static constexpr double kDefaultTicksPerLevel = 50.0;

    // fifo=true disables priorities (pure arrival order). Used as the baseline in benchmarks.
    explicit OrderQueue(bool fifo = false, double ticksPerLevel = kDefaultTicksPerLevel)
        : fifo_(fifo), ticksPerLevel_(ticksPerLevel) {}

    void push(const Order& o) {
        heap_.push(Entry{keyOf(o), nextSeq_++, o});
    }
    Order pop() { return heap_.pop().order; }
    const Order& top() const { return heap_.top().order; }
    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }

    double keyOf(const Order& o) const {
        if (fifo_) return -static_cast<double>(o.arrivalTick);
        return static_cast<int>(o.priority) * ticksPerLevel_ - static_cast<double>(o.arrivalTick);
    }
private:
    struct Entry { double key; long seq; Order order; };
    struct Less {
        bool operator()(const Entry& a, const Entry& b) const {
            if (a.key != b.key) return a.key < b.key;   // lower key = lower priority
            return a.seq > b.seq;                       // tie: earlier insertion wins (FIFO)
        }
    };
    BinaryHeap<Entry, Less> heap_;
    long nextSeq_ = 0;
    bool fifo_ = false;
    double ticksPerLevel_ = kDefaultTicksPerLevel;
};
