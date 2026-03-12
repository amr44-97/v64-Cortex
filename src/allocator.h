#pragma once

typedef unsigned long long size_t;
typedef unsigned long long ptrdiff_t;




template <typename _Tp> class __new_allocator {
  public:
    typedef _Tp value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;


    __attribute__((__always_inline__))  __new_allocator() {}

    __attribute__((__always_inline__))  __new_allocator(const __new_allocator&)
        _GLIBCXX_USE_NOEXCEPT {}

    template <typename _Tp1>
    __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR __new_allocator(const __new_allocator<_Tp1>&)
        _GLIBCXX_USE_NOEXCEPT {}

#if __cplusplus >= 201103L
    __new_allocator& operator=(const __new_allocator&) = default;
#endif

#if __cplusplus <= 201703L
    ~__new_allocator() _GLIBCXX_USE_NOEXCEPT {}

    pointer address(reference __x) const _GLIBCXX_NOEXCEPT { return __addressof(__x); }

    const_pointer address(const_reference __x) const _GLIBCXX_NOEXCEPT { return __addressof(__x); }
#endif

#if __has_builtin(__builtin_operator_new) >= 201802L
#define _GLIBCXX_OPERATOR_NEW __builtin_operator_new
#define _GLIBCXX_OPERATOR_DELETE __builtin_operator_delete
#else
#define _GLIBCXX_OPERATOR_NEW ::operator new
#define _GLIBCXX_OPERATOR_DELETE ::operator delete
#endif

    // NB: __n is permitted to be 0.  The C++ standard says nothing
    // about what the return value is when __n == 0.
    _GLIBCXX_NODISCARD _Tp* allocate(size_type __n, const void* = static_cast<const void*>(0)) {
#if __cplusplus >= 201103L
        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 3308. allocator<void>().allocate(n)
        static_assert(sizeof(_Tp) != 0, "cannot allocate incomplete types");
#endif

        if (__builtin_expect(__n > this->_M_max_size(), false)) {
            // _GLIBCXX_RESOLVE_LIB_DEFECTS
            // 3190. allocator::allocate sometimes returns too little storage
            if (__n > (size_t(-1) / sizeof(_Tp)))
                __throw_bad_array_new_length();
            __throw_bad_alloc();
        }

#if __cpp_aligned_new && __cplusplus >= 201103L
        if (alignof(_Tp) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            align_val_t __al = std::align_val_t(alignof(_Tp));
            return static_cast<_Tp*>(_GLIBCXX_OPERATOR_NEW(__n * sizeof(_Tp), __al));
        }
#endif
        return static_cast<_Tp*>(_GLIBCXX_OPERATOR_NEW(__n * sizeof(_Tp)));
    }

    // __p is not permitted to be a null pointer.
    void deallocate(_Tp* __p, size_type __n __attribute__((__unused__))) {
#if __cpp_sized_deallocation
#define _GLIBCXX_SIZED_DEALLOC(p, n) (p), (n) * sizeof(_Tp)
#else
#define _GLIBCXX_SIZED_DEALLOC(p, n) (p)
#endif

#if __cpp_aligned_new && __cplusplus >= 201103L
        if (alignof(_Tp) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            _GLIBCXX_OPERATOR_DELETE(_GLIBCXX_SIZED_DEALLOC(__p, __n), align_val_t(alignof(_Tp)));
            return;
        }
#endif
        _GLIBCXX_OPERATOR_DELETE(_GLIBCXX_SIZED_DEALLOC(__p, __n));
    }

#undef _GLIBCXX_SIZED_DEALLOC
#undef _GLIBCXX_OPERATOR_DELETE
#undef _GLIBCXX_OPERATOR_NEW

#if __cplusplus <= 201703L
    __attribute__((__always_inline__)) size_type max_size() const _GLIBCXX_USE_NOEXCEPT {
        return _M_max_size();
    }

#if __cplusplus >= 201103L
    template <typename _Up, typename... _Args>
    __attribute__((__always_inline__)) void
    construct(_Up* __p, _Args&&... __args) noexcept(__is_nothrow_new_constructible<_Up, _Args...>) {
        ::new ((void*)__p) _Up(forward<_Args>(__args)...);
    }

    template <typename _Up>
    __attribute__((__always_inline__)) void
    destroy(_Up* __p) noexcept(is_nothrow_destructible<_Up>::value) {
        __p->~_Up();
    }
#else
    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 402. wrong new expression in [some_] allocator::construct
    __attribute__((__always_inline__)) void construct(pointer __p, const _Tp& __val) {
        ::new ((void*)__p) _Tp(__val);
    }

    __attribute__((__always_inline__)) void destroy(pointer __p) { __p->~_Tp(); }
#endif
#endif // ! C++20

    template <typename _Up>
    friend __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR bool
    operator==(const __new_allocator&, const __new_allocator<_Up>&) _GLIBCXX_NOTHROW {
        return true;
    }

#if __cpp_impl_three_way_comparison < 201907L
    template <typename _Up>
    friend __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR bool
    operator!=(const __new_allocator&, const __new_allocator<_Up>&) _GLIBCXX_NOTHROW {
        return false;
    }
#endif

  private:
    __attribute__((__always_inline__)) _GLIBCXX_CONSTEXPR size_type
    _M_max_size() const _GLIBCXX_USE_NOEXCEPT {
#if __PTRDIFF_MAX__ < __SIZE_MAX__
        return size_t(__PTRDIFF_MAX__) / sizeof(_Tp);
#else
        return size_t(-1) / sizeof(_Tp);
#endif
    }
};

template <typename _Tp> class allocator : public __allocator_base<_Tp> {
  public:
    typedef _Tp value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

#if __cplusplus <= 201703L
    // These were removed for C++20.
    typedef _Tp* pointer;
    typedef const _Tp* const_pointer;
    typedef _Tp& reference;
    typedef const _Tp& const_reference;

    template <typename _Tp1> struct rebind {
        typedef allocator<_Tp1> other;
    };
#endif

#if __cplusplus >= 201103L
    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 2103. allocator propagate_on_container_move_assignment
    using propagate_on_container_move_assignment = true_type;

#if __cplusplus <= 202302L
    using is_always_equal _GLIBCXX20_DEPRECATED_SUGGEST("allocator_traits::is_always_equal") = true_type;
#endif
#endif

    // _GLIBCXX_RESOLVE_LIB_DEFECTS
    // 3035. allocator's constructors should be constexpr
    __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR allocator() _GLIBCXX_NOTHROW {}

    __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR allocator(const allocator& __a) _GLIBCXX_NOTHROW
        : __allocator_base<_Tp>(__a) {}

#if __cplusplus >= 201103L
    // Avoid implicit deprecation.
    allocator& operator=(const allocator&) = default;
#endif

    template <typename _Tp1>
    __attribute__((__always_inline__))
    _GLIBCXX20_CONSTEXPR allocator(const allocator<_Tp1>&) _GLIBCXX_NOTHROW {}

    __attribute__((__always_inline__))
#if __cpp_constexpr_dynamic_alloc
    constexpr
#endif
        ~allocator() _GLIBCXX_NOTHROW {
    }

#if __cplusplus > 201703L
    [[nodiscard, __gnu__::__always_inline__]]
    constexpr _Tp* allocate(size_t __n) {
        if (__is_constant_evaluated()) {
            if (__builtin_mul_overflow(__n, sizeof(_Tp), &__n))
                __throw_bad_array_new_length();
            return static_cast<_Tp*>(::operator new(__n));
        }

        return __allocator_base<_Tp>::allocate(__n, 0);
    }

    [[__gnu__::__always_inline__]]
    constexpr void deallocate(_Tp* __p, size_t __n) {
        if (__is_constant_evaluated()) {
            ::operator delete(__p);
            return;
        }
        __allocator_base<_Tp>::deallocate(__p, __n);
    }
#endif // C++20

    friend __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR bool
    operator==(const allocator&, const allocator&) _GLIBCXX_NOTHROW {
        return true;
    }

#if __cpp_impl_three_way_comparison < 201907L
    friend __attribute__((__always_inline__)) _GLIBCXX20_CONSTEXPR bool
    operator!=(const allocator&, const allocator&) _GLIBCXX_NOTHROW {
        return false;
    }
#endif

    // Inherit everything else.
};
