#ifndef C12_INIT_LIST
#define C12_INIT_LIST
#pragma once
namespace std
{
template <class _E> class init_list {
  public:
    typedef _E value_type;
    typedef const _E& reference;
    typedef const _E& const_reference;
    typedef unsigned long size_type;
    typedef const _E* iterator;
    typedef const _E* const_iterator;

  private:
    iterator _M_array;
    size_type _M_len;

    // The compiler can call a private constructor.
    constexpr init_list(const_iterator __a, size_type __l) : _M_array(__a), _M_len(__l) {}

  public:
    constexpr init_list() noexcept : _M_array(0), _M_len(0) {}

    // Number of elements.
    constexpr size_type size() const noexcept { return _M_len; }

    // First element.
    constexpr const_iterator begin() const noexcept { return _M_array; }

    // One past the last element.
    constexpr const_iterator end() const noexcept { return begin() + size(); }
};

/**
 *  @brief  Return an iterator pointing to the first element of
 *          the initializer_list.
 *  @param  __ils  Initializer list.
 *  @relates initializer_list
 */
template <class _Tp> constexpr const _Tp* begin(init_list<_Tp> __ils) noexcept {
    return __ils.begin();
}

/**
 *  @brief  Return an iterator pointing to one past the last element
 *          of the initializer_list.
 *  @param  __ils  Initializer list.
 *  @relates initializer_list
 */
template <class _Tp> constexpr const _Tp* end(init_list<_Tp> __ils) noexcept { return __ils.end(); }
}
#endif
