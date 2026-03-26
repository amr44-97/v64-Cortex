#pragma once
#ifndef CORTEX_STRING_MAP_H
#define CORTEX_STRING_MAP_H

#include "cortex.h"
#include <initializer_list>

// Initial hash bucket size
constexpr auto INIT_SIZE = 16;

// Rehash if the usage exceeds 70%.
constexpr auto HIGH_WATERMARK = 70;

// We'll keep the usage below 50% after rehashing.
constexpr auto LOW_WATERMARK = 50;

template <typename T, typename V> struct Pair {
    T key;
    V value;
};

template <typename T> struct StringMap {
    enum class State : u32 { Empty = 0, Occupied, Tombstone };

    struct Entry {
        StringRef key = {};
        T val;
        State state = State::Empty;
    };

    DynArray<Entry> buckets = {};
    int used = 0;

    StringMap() {}

  private:
    static u64 fnv_hash(StringRef s) {
        u64 hash = 0xcbf29ce484222325;
        for (u64 i = 0; i < s.len; i++) {
            hash *= 0x100000001b3;
            hash ^= (u8)s[i];
        }
        return hash;
    }

    auto rehash() {
        // Compute the size of the new hashmap.
        int nkeys = 0;
        for (const auto& entry : buckets) {
            if (entry.state == State::Occupied)
                nkeys++;
        }

        int cap = buckets.capacity;

        while ((nkeys * 100) / cap >= LOW_WATERMARK) {
            cap = cap * 2;
        }
        assert(cap > 0);

        // shooting myself in the foot
        DynArray<Entry> new_bucks = new_dyn(Entry, cap);

        for (const auto& old : buckets) {
            if (old.state == State::Occupied) {
                auto hash = fnv_hash(old.key);
                for (auto i = 0; i < new_bucks.capacity; ++i) {
                    auto pos = (hash + i) % new_bucks.capacity;
                    if (new_bucks[pos].state == State::Empty) {
                        new_bucks[pos].key = old.key;
                        new_bucks[pos].val = old.val;
                        new_bucks[pos].state = State::Occupied;
                        break;
                    }
                }
            }
        }

        // buckets = new_bucks.move();
        buckets.ptr = new_bucks.ptr;
        buckets.capacity = new_bucks.capacity;
        buckets.len = new_bucks.len;

        used = nkeys;
    }

  public:
    void destroy() { this->buckets.destroy(); }

    Entry* get_or_insert_entry(StringRef key) {
        if (buckets.empty()) {
            buckets.ensure_capacity(INIT_SIZE);
        } else if ((used * 100) / buckets.capacity >= HIGH_WATERMARK) {
            rehash();
        }

        auto hash = fnv_hash(key);
        for (auto i = 0; i < buckets.capacity; ++i) {
            Entry& ent = buckets[(hash + i) % buckets.capacity];
            if (ent.state == State::Occupied and ent.key == key)
                return &ent;

            if (ent.state == State::Tombstone) {
                ent.key = key;
                ent.state = State::Occupied;
                return &ent;
            }

            if (ent.state == State::Empty) {
                ent.key = key;
                ent.state = State::Occupied;
                used++;
                return &ent;
            }
        }

        fprintf(stderr, "HashMap is full\n");
        exit(1);
    }

    Entry* get_entry(StringRef key) {
        if (buckets.empty())
            return nullptr;
        auto hash = fnv_hash(key);
        for (auto i = 0; i < buckets.capacity; ++i) {
            Entry* ent = &buckets[(hash + i) % buckets.capacity];
            if (ent->state == State::Occupied and ent->key == key)
                return ent;
            if (ent->state == State::Empty)
                return nullptr;
        }
        return nullptr;
    }

    Option<T> get(StringRef key) {
        auto ent = get_entry(key);
        return ent ? Some(ent->val) : nullptr;
    }

    void put(StringRef key, const T& val) {
        auto ent = get_or_insert_entry(key);
        ent->val = val;
    }

    void remove(StringRef key) {
        auto ent = get_entry(key);
        if (ent)
            ent->state = State::Tombstone;
    }
};
#endif
