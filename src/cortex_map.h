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
        String key = {};
        T val{};
        State state = State::Empty;
    };

    DynArray<Entry> buckets = {};
    int used = 0;

    StringMap() {}
    StringMap(std::initializer_list<Pair<String, T>> list) {
        for (auto i : list) {
            put(i.key, i.value);
        }
    }

  private:
    static u64 fnv_hash(String s) {
        u64 hash = 0xcbf29ce484222325;
        for (u64 i = 0; i < s.length(); i++) {
            hash *= 0x100000001b3;
            hash ^= (u8)s[i];
        }
        return hash;
    }

    auto rehash() {
        // Compute the size of the new hashmap.
        int nkeys = 0;
        for (usize i = 0; i < buckets.capacity; ++i) {
            if (buckets[i].state == State::Occupied) nkeys++;
        }

        u32 cap = buckets.capacity == 0 ? INIT_SIZE : buckets.capacity;

        while ((nkeys * 100) / cap >= LOW_WATERMARK) {
            cap = cap * 2;
        }

        assert(cap > 0);

        // shooting myself in the foot
        DynArray<Entry> new_bucks(buckets.allocator);
        new_bucks.ensure_capacity(cap);
        // initialize all entries to Empty
        for (u32 i = 0; i < cap; ++i)
            new_bucks.append(Entry{});

        for (usize i = 0; i < buckets.capacity; ++i) {
            auto& old = buckets[i];
            if (old.state == State::Occupied) {
                auto hash = fnv_hash(old.key);
                for (auto i = 0; i < new_bucks.capacity; ++i) {
                    auto pos = (hash + i) % new_bucks.capacity;
                    if (new_bucks[pos].state == State::Empty) {
                        new_bucks[pos] = old;
                        break;
                    }
                }
            }
        }

        // buckets = new_bucks.move();
        buckets.destroy();
        buckets = new_bucks.move();
        used = nkeys;
    }

  public:
    void destroy() { this->buckets.destroy(); }

    Entry* get_or_insert_entry(String key) {
        if (buckets.empty() || (used * 100) / buckets.capacity >= HIGH_WATERMARK) rehash();

        auto hash = fnv_hash(key);
        Entry* first_tombstone = nullptr;

        for (auto i = 0; i < buckets.capacity; ++i) {
            Entry& ent = buckets[(hash + i) % buckets.capacity];
            if (ent.state == State::Occupied) {
                if (ent.key == key) return &ent;
                continue;
            }

            if (ent.state == State::Tombstone) {
                if (!first_tombstone) first_tombstone = &ent;
                continue;
                // ent.key = key;
                // ent.state = State::Occupied;
                // return &ent;
            }

            if (ent.state == State::Empty) {
                // ent.key = key;
                // ent.state = State::Occupied;
                // used++;
                // return &ent;
                Entry* target = first_tombstone ? first_tombstone : &ent;
                target->key = key;
                target->state = State::Occupied;
                used++;
                return target;
            }
        }

        fprintf(stderr, "HashMap is full\n");
        exit(1);
    }

    Entry* get_entry(String key) {
        if (buckets.empty()) return nullptr;
        auto hash = fnv_hash(key);
        for (auto i = 0; i < buckets.capacity; ++i) {
            Entry* ent = &buckets[(hash + i) % buckets.capacity];
            if (ent->state == State::Occupied and ent->key == key) return ent;
            if (ent->state == State::Empty) return nullptr;
        }
        return nullptr;
    }

    Option<T> get(String key) {
        auto ent = get_entry(key);
        return ent ? Some(ent->val) : nullptr;
    }

    void put(String key, const T& val) {
        auto ent = get_or_insert_entry(key);
        ent->val = val;
    }

    void remove(String key) {
        auto ent = get_entry(key);
        if (ent) ent->state = State::Tombstone;
    }
};
#endif
