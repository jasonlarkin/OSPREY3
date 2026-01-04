#ifndef OSPREY_KSTAR_MIN_HEAP_HPP
#define OSPREY_KSTAR_MIN_HEAP_HPP

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace osprey::kstar {

// A minimal heap wrapper that allows move-pop (unlike std::priority_queue::top()).
// The comparator should behave like the comparator used for std::priority_queue.
template<typename T, typename Compare>
class MinHeap {
public:
    explicit MinHeap(Compare comp = Compare{}, std::vector<T> backing = {})
        : comp_(std::move(comp))
        , data_(std::move(backing))
    {
        std::make_heap(data_.begin(), data_.end(), comp_);
    }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    [[nodiscard]] const T& top() const { return data_.front(); }

    void reserve(std::size_t n) { data_.reserve(n); }

    void push(const T& v) {
        data_.push_back(v);
        std::push_heap(data_.begin(), data_.end(), comp_);
    }

    void push(T&& v) {
        data_.push_back(std::move(v));
        std::push_heap(data_.begin(), data_.end(), comp_);
    }

    template<typename... Args>
    T& emplace(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
        std::push_heap(data_.begin(), data_.end(), comp_);
        return data_.front();
    }

    // Removes and returns the best element by move.
    T pop() {
        std::pop_heap(data_.begin(), data_.end(), comp_);
        T out = std::move(data_.back());
        data_.pop_back();
        return out;
    }

private:
    Compare comp_;
    std::vector<T> data_;
};

} // namespace osprey::kstar

#endif // OSPREY_KSTAR_MIN_HEAP_HPP

