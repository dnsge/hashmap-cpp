#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>

namespace dnsge {

/**
 * @brief A fixed-length, dynamically allocated vector of T. Elements are not 
 * initialized nor destructed automatically -- users must take care to keep track
 * of which elements need to be destructed.
 * 
 * @tparam T 
 */
template <typename T>
class FixedUninitVec {
public:
    FixedUninitVec(size_t size)
        : size_(size) {
        auto* alloc = static_cast<T*>(::operator new(size * sizeof(T)));
        assert(alloc != nullptr);
        this->data_ = std::unique_ptr<T>(alloc);
    }

    T &operator[](size_t n) {
        assert(n < this->size_);
        return *(this->data_.get() + n);
    }

    const T &operator[](size_t n) const {
        assert(n < this->size_);
        return *(this->data_.get() + n);
    }

    size_t size() const {
        return this->size_;
    }

    T* begin() {
        return this->data_.get();
    }

    T* end() {
        return this->data_.get() + this->size_;
    }

    T* data() {
        return this->data_.get();
    }

    const T* data() const {
        return this->data_.get();
    }

private:
    size_t size_;
    std::unique_ptr<T> data_;
};

} // namespace dnsge
