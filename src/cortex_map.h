#pragma once
#include <cstdlib>
#ifndef CORTEX_STRING_MAP_H
#define CORTEX_STRING_MAP_H

#include "cortex.h"
#include <initializer_list>
#include <type_traits>

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

template <typename Key> static u64 fnv_hash(Key s) {
    static_assert(std::is_same_v<Key, String> || std::is_same_v<Key, CortexStr>);
    u64 hash = 0xcbf29ce484222325;
    for (u64 i = 0; i < s.length(); i++) {
        hash *= 0x100000001b3;
        hash ^= (u8)s[i];
    }
    return hash;
}

template <typename Key, typename T, u64 HashFn(Key s)> struct HashMap {

    enum class State : u32 { Empty = 0, Occupied, Tombstone };

    struct Entry {
        Key key{};
        T val{};
        State state = State::Empty;
    };

    DynArray<Entry> buckets = {};
    int used = 0;

    HashMap() {}
    HashMap(Allocator* allocator) { buckets.allocator = allocator; }
    HashMap(Allocator* allocator, std::initializer_list<Pair<Key, T>> list) {
        buckets.allocator = allocator;
        for (auto i : list) {
            put(i.key, i.value);
        }
    }

  private:
    auto rehash() {
        // Compute the size of the new hashmap.
        int nkeys = 0;
        for (usize i = 0; i < buckets.capacity; ++i) {
            if (buckets[i].state == State::Occupied) nkeys++;
            // nkeys += (buckets[i].state == State::Occupied);
        }

        u32 cap = buckets.capacity == 0 ? INIT_SIZE : buckets.capacity;
        // u32 cap = (buckets.capacity == 0) * INIT_SIZE + buckets.capacity;

        while ((nkeys * 100) / cap >= LOW_WATERMARK) {
            cap = cap * 2;
        }

        assert(cap > 0);

        DynArray<Entry> new_bucks{buckets.allocator, cap};

        for (usize i = 0; i < buckets.capacity; ++i) {
            auto& old = buckets[i];
            if (old.state == State::Occupied) {
                auto hash = HashFn(old.key);
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
    void deinit() { this->buckets.destroy(); }

    Entry* get_or_insert_entry(Key key) {
        if (buckets.empty() || (used * 100) / buckets.capacity >= HIGH_WATERMARK) rehash();

        auto hash = HashFn(key);
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

    Entry* get_entry(Key key) {
        if (buckets.empty()) return nullptr;
        auto hash = HashFn(key);
        for (auto i = 0; i < buckets.capacity; ++i) {
            Entry* ent = &buckets[(hash + i) % buckets.capacity];
            if (ent->state == State::Occupied and ent->key == key) return ent;
            if (ent->state == State::Empty) return nullptr;
        }
        return nullptr;
    }

    Option<T> get(Key key) {
        auto ent = get_entry(key);
        return ent ? Some(ent->val) : nullptr;
    }

    void put(Key key, const T& val) {
        auto ent = get_or_insert_entry(key);
        ent->val = val;
    }

    void remove(Key key) {
        auto ent = get_entry(key);
        if (ent) ent->state = State::Tombstone;
    }
};

template <typename Key, typename T, u64 HashFn(Key s), u64 capacity> struct StaticHashMap {

    enum class State : u32 { Empty = 0, Occupied, Tombstone };

    struct Entry {
        Key key{};
        T val{};
        State state = State::Empty;
    };

    StaticArray<Entry, capacity> buckets = {};
    int used = 0;

    StaticHashMap() {}
    StaticHashMap(std::initializer_list<Pair<Key, T>> list) {
        for (auto i : list) {
            put(i.key, i.value);
        }
    }

  private:
    auto rehash() {
        // Compute the size of the new hashmap.
        int nkeys = 0;
        for (usize i = 0; i < buckets.capacity; ++i) {
            // if (buckets[i].state == State::Occupied) nkeys++;
            nkeys += (buckets[i].state == State::Occupied);
        }

        // u64 cap = buckets.capacity == 0 ? INIT_SIZE : buckets.capacity;
        u32 cap = (buckets.capacity == 0) * INIT_SIZE + buckets.capacity;

        while ((nkeys * 100) / cap >= LOW_WATERMARK) {
            cap = cap * 2;
        }

        assert(cap > 0);

        StaticArray<Entry, capacity*(HIGH_WATERMARK / 100) + capacity * 2> new_bucks = {};

        for (usize i = 0; i < buckets.capacity; ++i) {
            auto& old = buckets[i];
            if (old.state == State::Occupied) {
                auto hash = HashFn(old.key);
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
    void deinit() { this->buckets.destroy(); }

    Entry* get_or_insert_entry(Key key) {
        if (buckets.empty() || (used * 100) / buckets.capacity >= HIGH_WATERMARK) {
            fprintf(stderr, "[cortex_map:%d]: increase the fixed length of the StaticHashMap, used = %d\n",
                    __LINE__, used);
            exit(1);
        }
        // rehash();

        auto hash = HashFn(key);
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

        fprintf(stderr, "StaticHashMap is full\n");
        exit(1);
    }

    Entry* get_entry(Key key) {
        if (buckets.empty()) return nullptr;
        auto hash = HashFn(key);
        for (auto i = 0; i < buckets.capacity; ++i) {
            Entry* ent = &buckets[(hash + i) % buckets.capacity];
            if (ent->state == State::Occupied and ent->key == key) return ent;
            if (ent->state == State::Empty) return nullptr;
        }
        return nullptr;
    }

    Option<T> get(Key key) {
        auto ent = get_entry(key);
        return ent ? Some(ent->val) : nullptr;
    }

    void put(Key key, const T& val) {
        auto ent = get_or_insert_entry(key);
        ent->val = val;
    }

    void remove(Key key) {
        auto ent = get_entry(key);
        if (ent) ent->state = State::Tombstone;
    }
};

template <typename T> using StringMap = HashMap<CortexStr, T, fnv_hash>;

#endif
