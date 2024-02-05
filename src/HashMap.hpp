#pragma once

#include "FixedUninitVec.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dnsge {

namespace detail {

// NOLINTBEGIN(readability-magic-numbers)

using metadata_t = uint8_t;

enum Metadata : metadata_t {
    Empty = 0b10000000,
    Deleted = 0b11111111,
};

constexpr bool IsFree(metadata_t metadata) {
    // Check if the high bit of metadata is set
    return metadata >= 0b10000000;
}

constexpr size_t H1(size_t hash) {
    // Remove the lower 7 bits of hash
    return hash >> 7;
}

constexpr metadata_t H2(size_t hash) {
    // Get the lower 7 bits of hash, i.e. last byte with top bit 0
    return hash & 0x7F;
}

static_assert(IsFree(Metadata::Empty), "Empty should be considered free");
static_assert(IsFree(Metadata::Deleted), "Deleted should be considered free");
static_assert(!IsFree(H2(0xFFFF)), "H2 of 0xFFFF should be not be considered free");

// NOLINTEND(readability-magic-numbers)

}; // namespace detail

template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
class HashMap {
    static constexpr size_t DefaultInitialCapacity = 16;
    static constexpr float MaxLoadFactor = 0.875;
    static constexpr float MaxDeletedLoadFactor = 0.5;

public:
    struct Slot {
        const K key;
        V value;
    };

    static_assert(std::is_copy_assignable_v<K>, "Key must be copy assignable");
    static_assert(std::is_move_constructible_v<Slot>, "Slot must be move constructable");

    template <typename SlotType>
    class MapIterator {
    private:
        MapIterator(size_t idex, SlotType* slotPtr)
            : idex_(idex)
            , slotPtr_(slotPtr) {}

    public:
        SlotType &operator*() const {
            return *this->slotPtr_;
        }

        SlotType* operator->() const {
            return this->slotPtr_;
        }

        bool operator==(const MapIterator &other) const {
            return this->idex_ == other.idex_;
        }

        bool operator!=(const MapIterator &other) const {
            return this->idex_ != other.idex_;
        }

    private:
        friend class HashMap<K, V, Hash, Eq>;
        size_t idex_;
        SlotType* slotPtr_;
    };

    using iterator = MapIterator<Slot>;
    using const_iterator = MapIterator<const Slot>;

    HashMap()
        : HashMap(DefaultInitialCapacity) {}

    HashMap(size_t initialCapacity)
        : capacity_(initialCapacity)
        , size_(0)
        , slots_(initialCapacity)
        , deletedCount_(0) {
        this->metadata_.resize(initialCapacity, detail::Metadata::Empty);
    }

    ~HashMap() {
        if (this->empty()) {
            return;
        }
        for (size_t i = 0; i < this->capacity_; ++i) {
            if (!detail::IsFree(this->metadata_[i])) {
                this->slots_[i].~Slot(); // Destroy slot entry
            }
        }
    }

    HashMap(const HashMap &other) = default;
    HashMap &operator=(const HashMap &other) = default;

    HashMap(HashMap &&other) noexcept
        : capacity_(other.capacity_)
        , size_(other.size_)
        , metadata_(std::move(other.metadata_))
        , slots_(std::move(other.slots_))
        , deletedCount_(other.deletedCount_) {
        other.capacity_ = 0;
        other.size_ = 0;
        other.deletedCount_ = 0;
    }

    HashMap &operator=(HashMap &&other) noexcept {
        this->capacity_ = other.capacity_;
        this->size_ = other.size_;
        this->metadata_ = std::move(other.metadata_);
        this->slots_ = std::move(other.slots_);
        this->deletedCount_ = other.deletedCount_;
        other.capacity_ = 0;
        other.size_ = 0;
        other.deletedCount_ = 0;
        return *this;
    }

    iterator find(const K &key) {
        return this->iteratorAt(this->doFind(key));
    }

    const_iterator find(const K &key) const {
        return this->iteratorAt(this->doFind(key));
    }

    V &at(const K &key) {
        auto res = this->doFind(key);
        if (res) {
            return this->slots_[res.value()].value;
        }
        throw std::out_of_range("key not found");
    }

    std::optional<iterator> insert(const std::pair<K, V> &value) {
        // Check if we need to grow
        if (this->needToGrowForInsert()) {
            this->growAndRehash();
        }
        // Perform the insert
        return iteratorAt(this->insertUnchecked(value));
    }

    std::optional<iterator> insert(std::pair<K, V> &&value) {
        // Check if we need to grow
        if (this->needToGrowForInsert()) {
            this->growAndRehash();
        }
        // Perform the insert
        return iteratorAt(this->insertUnchecked(value));
    }

    template <typename = std::enable_if<std::is_default_constructible_v<V>>>
    V &operator[](const K &key) {
        auto res = this->doFind(key);
        if (res) {
            return this->slots_[res.value()].value;
        }
        // Need to insert and then return
        K keyCopy = key;
        auto it = this->insert(std::make_pair<K, V>(std::move(keyCopy), V{}));
        assert(it.has_value());
        return this->slots_[it->idex_].value;
    }

    bool erase(iterator it) {
        if (it == this->end()) {
            return false;
        }
        assert(it.idex_ < this->capacity_);
        // Delete data associated with key-value pair
        this->deleteSlot(it.idex_);
        // Decrement size
        this->size_ -= 1;

        // Check if we have an elevated number of deleted slots.
        // If so, we want to rehash everything to prevent fragmentation.
        if (this->needRehash()) {
            this->rehashEverything();
        }

        return true;
    }

    bool erase(const K &key) {
        return this->erase(this->find(key));
    }

    void clear() {
        if (this->empty()) {
            return;
        }
        for (size_t i = 0; i < this->capacity_; ++i) {
            if (!detail::IsFree(this->metadata_[i])) {
                this->slots_[i].~Slot(); // Destroy slot entry
            }
            this->metadata_[i] = detail::Metadata::Empty;
        }
        this->size_ = 0;
        this->deletedCount_ = 0;
    }

    void reserve(size_t n) {
        if (this->capacity_ >= n) {
            return;
        }
        this->growAndRehash(n);
    }

    size_t size() const {
        return this->size_;
    }

    bool empty() const {
        return this->size_ == 0;
    }

    inline iterator begin() {
        return iteratorAt(0);
    }

    inline iterator end() {
        return iteratorAt(this->slots_.size());
    }

    inline const_iterator begin() const {
        return iteratorAt(0);
    }

    inline const_iterator end() const {
        return iteratorAt(this->slots_.size());
    }

private:
    struct InsertionLoc {
        size_t idex;
        size_t h2;
    };

    std::optional<InsertionLoc> locationForInsertion(const K &key) const {
        Hash hasher;
        Eq eq;
        size_t hash = hasher(key);
        auto h1 = detail::H1(hash);
        auto h2 = detail::H2(hash);

        size_t idex = h1 % this->capacity_;
        while (true) {
            auto metadata = this->metadata_[idex];
            if (detail::IsFree(metadata)) {
                // Found free spot for insertion
                return InsertionLoc{idex, h2};
            } else if (metadata == h2 && eq(key, this->slots_[idex].key)) {
                // Key already exists
                return std::nullopt;
            }
            idex = (idex + 1) % this->capacity_;
        }
    }

    // Insertion of const lvalue
    std::optional<size_t> insertUnchecked(const std::pair<K, V> &value) {
        if (auto loc = this->locationForInsertion(value.first)) {
            // Insert value into slot. Update the metadata with the hash data.
            this->metadata_[loc->idex] = loc->h2;
            // Construct new slot entry in place
            new (&this->slots_[loc->idex]) Slot{value.first, value.second};
            // Increment size
            this->size_ += 1;
            return {loc->idex};
        }
        return std::nullopt;
    }

    // Insertion of rvalue
    std::optional<size_t> insertUnchecked(std::pair<K, V> &&value) {
        if (auto loc = this->locationForInsertion(value.first)) {
            // Insert value into slot. Update the metadata with the hash data.
            this->metadata_[loc->idex] = loc->h2;
            // Construct new slot entry in place, moving the values.
            new (&this->slots_[loc->idex]) Slot{std::move(value.first), std::move(value.second)};
            // Increment size
            this->size_ += 1;
            return {loc->idex};
        }
        return std::nullopt;
    }

    // Insertion of Slot rvalue. Used when growing the hash table.
    std::optional<size_t> insertUnchecked(Slot &&slot) {
        if (auto loc = this->locationForInsertion(slot.key)) {
            // Insert value into slot. Update the metadata with the hash data.
            this->metadata_[loc->idex] = loc->h2;
            // Move old slot into new slot.
            new (&this->slots_[loc->idex]) Slot{std::move(slot)};
            // Increment size
            this->size_ += 1;
            return {loc->idex};
        }
        return std::nullopt;
    }

    void deleteSlot(size_t idex) {
        // Mark slot as deleted
        this->metadata_[idex] = detail::Metadata::Deleted;
        this->deletedCount_ += 1;
        // Call destructor on slot entry
        this->slots_[idex].~Slot();
    }

    inline iterator iteratorAt(size_t idex) {
        return iterator(idex, this->slots_.data() + idex);
    }

    inline const_iterator iteratorAt(size_t idex) const {
        return const_iterator(idex, this->slots_.data() + idex);
    }

    inline iterator iteratorAt(std::optional<size_t> maybeIdex) {
        if (maybeIdex) {
            return iteratorAt(*maybeIdex);
        }
        return this->end();
    }

    inline const_iterator iteratorAt(std::optional<size_t> maybeIdex) const {
        if (maybeIdex) {
            return iteratorAt(*maybeIdex);
        }
        return this->end();
    }

    // Performs a find. Returns index to found slot.
    std::optional<size_t> doFind(const K &key) const {
        Hash hasher;
        Eq eq;
        size_t hash = hasher(key);
        auto h1 = detail::H1(hash);
        auto h2 = detail::H2(hash);

        size_t idex = h1 % this->capacity_;
        while (true) {
            auto metadata = this->metadata_[idex];
            if (metadata == h2 && eq(key, this->slots_[idex].key)) {
                // Found key
                return idex;
            } else if (metadata == detail::Metadata::Empty) {
                // Found empty slot, must not be in hash table
                return std::nullopt;
            }
            idex = (idex + 1) % this->capacity_;
        }
    }

    void growAndRehash() {
        size_t newCapacity = this->capacity_ * 2;
        if (this->capacity_ == 0) {
            newCapacity = DefaultInitialCapacity;
        }
        this->growAndRehash(newCapacity);
    }

    void growAndRehash(size_t newCapacity) {
        if (newCapacity <= this->capacity_) {
            return;
        }

        // Temporary new table to move existing elements into
        HashMap<K, V, Hash, Eq> newTable(newCapacity);

        if (!this->empty()) {
            for (size_t i = 0; i < this->capacity_; ++i) {
                if (!detail::IsFree(this->metadata_[i])) {
                    newTable.insertUnchecked(std::move(this->slots_[i]));
                }
            }
        }

        // Swap internals with temporary table
        *this = std::move(newTable);
    }

    void rehashEverything() {
        if (this->empty()) {
            return;
        }

        // Temporary new table to move existing elements into
        HashMap<K, V, Hash, Eq> newTable(this->capacity_);
        for (size_t i = 0; i < this->capacity_; ++i) {
            if (!detail::IsFree(this->metadata_[i])) {
                newTable.insertUnchecked(std::move(this->slots_[i]));
            }
        }

        // Swap internals with temporary table
        *this = std::move(newTable);
    }

    float loadFactor() const {
        return ((float)this->size_) / this->capacity_;
    }

    bool needToGrowForInsert() const {
        return this->loadFactor() >= MaxLoadFactor || this->size_ == this->capacity_;
    }

    bool needRehash() const {
        // We have a high number of deleted elements relative to non-deleted elements.
        return this->deletedCount_ >= (this->size_ * MaxDeletedLoadFactor);
    }

    size_t capacity_;
    size_t size_;

    std::vector<detail::metadata_t> metadata_;
    FixedUninitVec<Slot> slots_;

    size_t deletedCount_;
};

} // namespace dnsge
