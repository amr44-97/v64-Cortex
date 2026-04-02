#pragma once

#include <assert.h>
#include <initializer_list>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;
typedef size_t usize;
struct String;
struct CortexStr;

constexpr size_t ARENA_DEFAULT_BLOCK_SIZE = 64 * 1024;

#define __pp_concat(x, y) x##y
#define __pp_defer_concat(x, y) __pp_concat(x, y)

#define defer(code)                                                                                          \
    auto __pp_defer_concat(__defer_var_, __COUNTER__) = DeferGuard{[&]() {                                   \
        static_assert(true, "ensure scope validitiy");                                                       \
        code;                                                                                                \
    }};

#define match_t(T) switch (T.tag)

#define with                                                                                                 \
    break;                                                                                                   \
    case

#define with_default                                                                                         \
    break;                                                                                                   \
    default

#define new_dyn(T, ...) create_dyn<T>(__VA_OPT__(__VA_ARGS__))
#define new_arena(...) create_arena(__VA_OPT__(__VA_ARGS__))
#define Some(VALUE) Option<decltype(VALUE)>(VALUE)
#define None nullptr

template <typename T> struct slice {
    u64 len;
    T* ptr;

    slice() = default;

    slice(T* _Data, u32 _length) : len(_length), ptr(_Data) {}
    slice(const T* _Data, u32 _length) {
        len = _length;
        ptr = const_cast<T*>(_Data);
    }

    u64 size() const { return len; }

    const T& last() const {
        assert(len >= 1);
        return ptr[len - 1];
    }

    T& last() {
        assert(len >= 1);
        return ptr[len - 1];
    }

    T* begin() { return ptr; }
    T* end() { return ptr + len; }

    T& back() { return ptr[len - 1]; }

    T* dupe() {
        // [Len][Data]
        u64* mem = (u64*)malloc(sizeof(u64) + len * sizeof(T));
        *mem = len;
        mem += 1;
        memcpy(mem, ptr, len);
        return mem;
    }

    T& operator[](usize i) const { return ptr[i]; }

    T& operator[](usize i) { return ptr[i]; }

    bool operator==(slice<T>& _buf) const {
        if (len != _buf.len) return false;
        return memcmp(_buf.ptr, ptr, _buf.len) == 0;
    }
};

struct CortexStr : slice<char> {
    CortexStr() {}

    CortexStr(slice<char>& __slice) {
        this->ptr = __slice.ptr;
        this->len = __slice.len;
    }

    template <typename T = String> CortexStr(T& _str) {
        this->ptr = _str.ptr;
        this->len = _str.len;
    }

    CortexStr(const char* __str, usize len) {
        this->ptr = const_cast<char*>(__str);
        this->len = len;
    }

    CortexStr(const char* __str) {
        this->ptr = const_cast<char*>(__str);
        this->len = strlen(__str);
    }

    // allocates the date it's pointing to
    char* dupe() const {
        char* _data = (char*)malloc(len + 1);
        memcpy(_data, ptr, len);
        _data[len] = '\0';
        return _data;
    }
    const char* data() const { return ptr; }
    CortexStr substr(u64 start_pos, u64 len) { return CortexStr(&ptr[start_pos], len); }

    usize length() const { return this->len; }

    bool operator==(const char* _buf) const {
        if (!_buf) return len == 0;
        auto buflen = strlen(_buf);
        if (len != buflen) return false;
        return memcmp(_buf, ptr, buflen) == 0;
    }

    bool operator==(CortexStr _buf) const {
        if (len != _buf.len) return false;
        return memcmp(_buf.ptr, ptr, _buf.len) == 0;
    }
};

using StringRef = CortexStr;
#define FmtStrRef(SV) (int)(SV).size(), (SV).data()

enum TokenTag : u64 {
    TOK_EOF = 0,
    TOK_IDENTIFIER,
    TOK_TYPENAME,
    TOK_NUMBER_LITERAL,
    TOK_STRING_LITERAL,
    TOK_CHAR_LITERAL,
    TOK_WHITESPACE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_PERIOD,
    TOK_ELLIPSIS2,
    TOK_ELLIPSIS3,
    TOK_COLON,
    TOK_COLON_EQ,
    TOK_COLON_COLON,
    TOK_EQUAL,
    TOK_EQUAL_EQ,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_BANG,
    TOK_BANG_EQ,
    TOK_QUESTIONMARK,
    TOK_DOLLARSIGN,
    TOK_ATSIGN,
    TOK_HASH,
    TOK_PLUS,
    TOK_PLUS_PLUS,
    TOK_PLUS_EQ,
    TOK_MINUS,
    TOK_MINUS_MINUS,
    TOK_MINUS_EQ,
    TOK_ARROW,
    TOK_ASTERISK,
    TOK_ASTERISK_EQ,
    TOK_SLASH,
    TOK_SLASH_EQ,
    TOK_PERCENT,
    TOK_PERCENT_EQ,
    TOK_PIPE,
    TOK_PIPE_EQ,
    TOK_PIPE_PIPE,
    TOK_AMPERSAND,
    TOK_AMPERSAND_EQ,
    TOK_AMPERSAND_AMPERSAND,
    TOK_CARET,
    TOK_CARET_EQ,
    TOK_TILDE,
    TOK_TILDE_EQ,
    TOK_LESS_THAN,
    TOK_SHL,
    TOK_SHL_EQ,
    TOK_LESS_EQ,
    TOK_GREATER_THAN,
    TOK_SHR,    // >>
    TOK_SHR_EQ, // >>=
    TOK_GREATER_EQ,
    TOK_RETURN,
    TOK_CONST,
    TOK_LET,
    TOK_STATIC,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,
    TOK_DO,
    TOK_GOTO,
    TOK_SWITCH,
    TOK_CASE,
    TOK_BREAK,
    TOK_DEFAULT,
    TOK_STRUCT,
    TOK_ENUM,
    TOK_UNION,
    TOK_TYPEDEF,
    TOK_SIZEOF,
    TOK_SIGNED,
    TOK_UNSIGNED,
    TOK_VOID,
    TOK_INT,
    TOK_BOOL,
    TOK_CHAR,
    TOK_SHORT,
    TOK_LONG,
    TOK_FLOAT,
    TOK_DOUBLE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_U8,
    TOK_I8,
    TOK_U16,
    TOK_I16,
    TOK_U32,
    TOK_I32,
    TOK_U64,
    TOK_I64,
    TOK_F32,
    TOK_F64,
    TOK_INCLUDE,
};

enum AstTag {
    AST_ROOT = 0,

    /* Binary Operations */

    // lhs + rhs
    AST_ADD,
    // lhs - rhs
    AST_SUB,
    // lhs * rhs
    AST_MUL,
    // lhs / rhs
    AST_DIV,
    // lhs % rhs
    AST_MOD,
    // lhs < rhs
    AST_LT,
    // lhs > rhs
    AST_GT,
    // lhs <= rhs
    AST_LT_EQ,
    // lhs >= rhs
    AST_GT_EQ,
    // lhs == rhs
    AST_EQ_EQ,
    //  lhs != rhs
    AST_NOT_EQ,
    // lhs && rhs
    AST_BOOL_AND,
    // lhs || rhs
    AST_BOOL_OR,
    // lhs | rhs
    AST_BIT_OR,
    // lhs & rhs
    AST_BIT_AND,
    // lhs ^ rhs
    AST_BIT_XOR,
    // lhs << rhs
    AST_SHL,
    // lhs >> rhs
    AST_SHR,

    /* Unary Operations */

    // !rhs
    AST_BOOL_NOT,
    // -rhs
    AST_NEG,
    // ~rhs
    AST_BIT_NOT,
    // &rhs
    AST_ADDRESS_OF,
    // *rhs
    AST_DEREF,

    /* Expressions */
    /* Expression Literals */

    AST_IDENTIFIER,
    AST_TYPENAME,
    AST_STRING_LITERAL,

    AST_NUMBER_LITERAL,

    AST_BOOL_LITERAL,

    AST_NULL_LITERAL,

    AST_ENUM_LITERAL,

    // (expr)
    AST_GROUPED_EXPR,
    // lhs(rhs) -> lhs: expr(Identifier), rhs: list(expr)
    AST_CALL,
    // lhs.rhs -> lhs: expr, rhs: Identifier
    AST_MEMBER,
    // (lhs) rhs -> lhs: Typeexpr , rhs: expr(Identifier)
    AST_CAST,
    // rhs -> rhs: TypeExpr
    AST_SIZE_OF,
    // lhs[rhs] -> lhs: expr, rhs: expr
    AST_ARRAY_ACCESS,
    // expr { expr = expr, ... }
    AST_STRUCT_INIT,

    /* statements */
    // expr
    AST_EXPR_STMT,
    // { `stmt`,`stmt`,...} list of statements
    AST_BLOCK,
    // `return` `optional expr` `;`
    AST_RETURN,
    // defer stmt;
    AST_DEFER,
    //
    AST_CALL_STMT,
    // `lhs` `AssignOp` `rhs`
    // AssignOp= ['=', '+=', '-=', '/=', '*=', '%=', '|=', '~=', '&=', '^=', '>>=', '<<=']
    AST_ASSIGN,
    //
    AST_VAR_DECL,
    // const var_name = init_expr;
    AST_CONST_DECL,
    // `keyword_let` `var_name` = `init_expr`;
    AST_INFERRED_VAR_DECL,
    // if `expr` `stmt` `optiona<stmt<else_stmt>>`
    AST_IF,
    // While `expr` `stmt<block>`
    AST_WHILE,
    // for `expr` in `expr` `stmt<block>`
    AST_FOR,
    // type Identifier =? default_value?
    AST_FUN_PARAM,
    // TypedVarDecl | Decl
    AST_STRUCT_FIELD,
    /* Top Level */
    AST_FUN_DECL,
    AST_FUN_PROTO,
    AST_STRUCT,
    AST_ENUM,
    AST_UNION,
    AST_GLOBAL_VAR_DECL,
    AST_INCLUDE_STMT,
    AST_MACRO_DEF,
    AST_ALIAS_STMT,
};

struct Allocator {
    virtual ~Allocator() = default;

    virtual void* alloc(usize size) = 0;
    virtual void dealloc(void* ptr) = 0;
    virtual void deinit() = 0;
};

struct CAllocator : Allocator {
    void* alloc(usize size) override {
        void* ptr = malloc(size);
        memset(ptr, 0, size);
        return ptr;
    }

    template <typename T> T* alloc(usize nelems) { return alloc(nelems * sizeof(T)); }

    void dealloc(void* ptr) override { std::free(ptr); }
    void deinit() override {}
    ~CAllocator() override {}
};

static CAllocator c_allocator = {};

struct Location {
    u32 offset;
    u32 len;
    u32 line;
    u32 line_start = 0;
};
union CortexValue {
    // index of the Identifier in ParsingContext.str_list;
    // get the symbol by ParsingContext.get(u32 id) -> string_view
    void* none;                 // 8
    i64 integer;                // 8
    long double floating_point; // 8
    bool boolean;
};

struct Token {
    TokenTag tag;  // 4
    StringRef buf; // 16
    Location loc;  // 16
    CortexValue value = {.none = nullptr};
};

template <typename T, usize Alignment = alignof(T)> struct RawArray {
    T* ptr = nullptr;
    u32 len = 0;
    u32 capacity = 0;

    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of 2");
    RawArray<T> move() {
        auto res = *this;
        ptr = nullptr;
        len = capacity = 0;
        return res;
    }

    RawArray<T> copy() {
        RawArray<T> cp = {};
        cp.ensure_capacity(capacity);
        cp.append(ptr, len);
        return cp;
    }

    void ensure_capacity(usize new_capacity = 16) {
        if (capacity >= new_capacity) return;
        usize better_capacity = capacity;

        do {
            better_capacity = better_capacity * 5 / 2 + 8;
        } while (better_capacity < new_capacity);

        const usize aligned_size = ((better_capacity * sizeof(T)) + Alignment - 1) & ~(Alignment - 1);

        T* tmp = (T*)calloc(aligned_size, sizeof(T));
        if (ptr != nullptr and len > 0) {
            memcpy(tmp, ptr, len * sizeof(T));
            free(ptr);
        }
        ptr = tmp;
        capacity = better_capacity;
    }

    void reset() {
        memset(ptr, 0, capacity * sizeof(T));
        len = 0;
    }

    void destroy() {
        free(ptr);
        len = capacity = 0;
    }

    constexpr bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    u32 append(T* items, usize n) {
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items, n);
        len += n;
        return len;
    }

    u32 append(const T* items, usize n) {
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy((void*)&ptr[len], items, n);
        len += n;
        return len;
    }

    u32 append(T elem) {
        if (capacity <= len + 1) ensure_capacity(len + 1);
        ptr[len++] = elem;
        return len - 1;
    }

    void append_items(const T* items, size_t count) {
        for (int i = 0; i < count; i++) {
            append(items[i]);
        }
    }

    constexpr void resize(usize new_len) {
        ensure_capacity(new_len);
        len = new_len;
    }

    bool empty() const { return ptr == nullptr or capacity == 0; }

    const T& last() const {
        assert(len >= 1);
        return ptr[len - 1];
    }

    T& last() {
        assert(len >= 1);
        return ptr[len - 1];
    }

    T* begin() { return ptr; }
    T* end() { return ptr + len; }

    T& back() { return ptr[len - 1]; }

    T& at(usize i) const {
        assert(i < len && "access beyond the lenght of the array");
        return ptr[i];
    }

    T& at(usize i) {
        assert(i < len && "access beyond the lenght of the array");
        return ptr[i];
    }

    T& operator[](usize i) const { return ptr[i]; }

    T& operator[](usize i) { return ptr[i]; }
};

template <typename T> struct Stack : RawArray<T> {
    Allocator* allocator = &c_allocator;
    Stack() {}
    explicit Stack(Allocator* _allocator) { this->allocator = _allocator; }
    explicit Stack(Allocator* _allocator, u32 _capacity) {
        this->allocator = _allocator;
        this->ensure_capacity(_capacity);
    }

    T& push(T item) { this->append(item); }
    T& pop() {
        auto& t = this->last();
        this->len -= 1;
        return t;
    }
};

template <typename T, usize Alignment = alignof(T)> struct DynArray {
    T* ptr = nullptr;
    u32 len = 0;
    u32 capacity = 0;
    Allocator* allocator = &c_allocator;

    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of 2");

    DynArray() {}

    explicit DynArray(Allocator* _allocator) { this->allocator = _allocator; }
    explicit DynArray(Allocator* _allocator, u32 _capacity) {
        this->allocator = _allocator;
        ensure_capacity(_capacity);
    }

    DynArray<T> move() {
        DynArray<T> res = *this;
        ptr = nullptr;
        len = capacity = 0;
        return res;
    }

    DynArray<T> copy() {
        DynArray<T> cp = {};
        cp.ensure_capacity(capacity);
        cp.append(ptr, len);
        return cp;
    }

    void ensure_capacity(usize new_capacity = 16) {
        if (capacity >= new_capacity) return;
        usize better_capacity = capacity;

        do {
            better_capacity = better_capacity * 5 / 2 + 8;
        } while (better_capacity < new_capacity);

        const usize aligned_size = ((better_capacity * sizeof(T)) + Alignment - 1) & ~(Alignment - 1);

        T* tmp = (T*)allocator->alloc(aligned_size);
        if (ptr != nullptr and len > 0) {
            memcpy(tmp, ptr, len * sizeof(T));
            allocator->dealloc(ptr);
        }
        ptr = tmp;
        capacity = better_capacity;
    }

    void reset() {
        memset(ptr, 0, capacity * sizeof(T));
        len = 0;
    }

    void destroy() {
        allocator->dealloc(ptr);
        len = capacity = 0;
    }

    constexpr bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    u32 append(T* items, usize n) {
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items, n);
        len += n;
        return len;
    }

    u32 append(const T* items, usize n) {
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy((void*)&ptr[len], items, n);
        len += n;
        return len;
    }

    u32 append(T elem) {
        if (capacity <= len + 1) ensure_capacity(len + 1);
        ptr[len++] = elem;
        return len - 1;
    }

    void append_items(const T* items, size_t count) {
        for (int i = 0; i < count; i++) {
            append(items[i]);
        }
    }

    constexpr void resize(usize new_len) {
        ensure_capacity(new_len);
        len = new_len;
    }
    bool empty() const { return ptr == nullptr or capacity == 0; }

    const T& last() const {
        assert(len >= 1);
        return ptr[len - 1];
    }

    T& last() {
        assert(len >= 1);
        return ptr[len - 1];
    }

    T* begin() { return ptr; }
    T* end() { return ptr + len; }

    T& back() { return ptr[len - 1]; }

    T& at(usize i) const {
        assert(i < len && "access beyond the lenght of the array");
        return ptr[i];
    }

    T& at(usize i) {
        assert(i < len && "access beyond the lenght of the array");
        return ptr[i];
    }

    T& operator[](usize i) const { return ptr[i]; }

    T& operator[](usize i) { return ptr[i]; }
};

using dynString = DynArray<char>;

struct Arena : Allocator {
    RawArray<u8*> blocks = {};
    usize offset = 0;
    usize block_size = ARENA_DEFAULT_BLOCK_SIZE;

    ~Arena() {
        if (!blocks.empty()) deinit();
    }

    Arena() {
        blocks.ensure_capacity(256);
        u8* _mem = (u8*)calloc(block_size, sizeof(u8));
        blocks.append(_mem);
    }

    Arena(usize _block_size) {
        block_size = _block_size;
        u8* _mem = (u8*)calloc(block_size, sizeof(u8));
        blocks.append(_mem);
    }

    void dealloc(void* ptr) override { (void)ptr; }
    void deinit() override {
        for (auto blk : blocks) {
            free(blk);
        }
        blocks.destroy();
    }

    void* alloc(usize size) override {
        // size = (size + alignof(u8) - 1) & ~(alignof(u8) - 1); // align to 8 bytes
        if (size > block_size - offset) { // no enough space in this block
            u8* _mem = (u8*)calloc(block_size, sizeof(u8));
            blocks.append(_mem);
            offset = 0;
        }
        void* ptr = blocks.back() + offset; // current_block + offset
        offset += size;
        return ptr;
    }

    template <typename Tp> Tp* alloc(usize nelems = 1) { return (Tp*)alloc(sizeof(Tp) * nelems); }
    template <typename Tp> Tp* alloc(Tp item) {
        auto res = (Tp*)alloc(sizeof(Tp));
        *res = item;
        return res;
    }
};

static Arena global_arena{};

template <typename T> auto create_dyn(usize _size = 8) {
    DynArray<T> l = {};
    l.ensure_capacity(_size);
    return l;
}

struct String {
    char* ptr = nullptr;
    u32 len = 0;
    u32 capacity = 0;

    String() {}
    String(CortexStr& __str) : String(__str.ptr, __str.len) {}
    String(const char* __str, usize n) { append(__str, n); }
    String(const char* __str) { append(__str); }
    String(char* __str) { append(__str); }

    String move() {
        auto res = *this;
        ptr = nullptr;
        len = capacity = 0;
        return res;
    }

    String copy() {
        String s;
        s.append(ptr, len);
        return s;
    }

    void ensure_capacity(usize new_capacity = 16) {
        if (capacity >= new_capacity) return;
        usize better_capacity = capacity;

        do {
            better_capacity = better_capacity * 5 / 2 + 8;
        } while (better_capacity < new_capacity);

        const usize aligned_size = better_capacity;

        char* tmp = (char*)calloc(aligned_size, 1);
        if (ptr != nullptr and len > 0) {
            memcpy(tmp, ptr, len);
            free(ptr);
        }
        ptr = tmp;
        capacity = better_capacity;
    }

    void reset() {
        memset(ptr, 0, capacity);
        len = 0;
    }

    void destroy() {
        if (ptr) { free(ptr); }
        ptr = nullptr;
        len = capacity = 0;
    }

    constexpr bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    u32 append(String items) {
        auto n = items.size();
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items.data(), n);
        len += n;
        return len;
    }

    u32 append(StringRef items) {
        auto n = items.size();
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items.data(), n);
        len += n;
        return len;
    }

    void append(std::initializer_list<StringRef> items) {
        for (auto i : items) {
            append(i);
        }
    }

    u32 append(char* items, usize n) {
        if (!enough_space_for(n + 1)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items, n);
        len += n;
        ptr[len] = '\0';
        return len;
    }

    u32 append(const char* items) {
        auto n = strlen(items);
        if (!enough_space_for(n + 1)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items, n);
        len += n;
        ptr[len] = '\0';
        return len;
    }

    u32 append(const char* items, usize n) {
        if (!enough_space_for(n + 1)) ensure_capacity(len + n + 1);

        memcpy((char*)&ptr[len], items, n);
        len += n;
        ptr[len] = '\0';
        return len;
    }

    // len =0 append('a'), ptr[len] = 'a' , len = 1 ptr[1] = '\0' ,return len - 1
    u32 append(char elem) {
        if (capacity <= len + 1) ensure_capacity(len + 1);
        ptr[len++] = elem;
        ptr[len] = '\0';
        return len - 1;
    }

    constexpr void resize(usize new_len) {
        ensure_capacity(new_len);
        len = new_len;
    }

    bool empty() const { return ptr == nullptr or capacity == 0; }

    const char& last() const {
        assert(len >= 1);
        return ptr[len - 1];
    }

    char& last() {
        assert(len >= 1);
        return ptr[len - 1];
    }
    usize size() const { return len; }
    char* data() { return ptr; }
    char* begin() { return ptr; }
    char* end() { return ptr + len; }
    usize length() const { return len; }
    char& at(usize i) const {
        assert(i < len && "access beyond the lenght of the array");
        return ptr[i];
    }

    char& at(usize i) {
        assert(i < len && "access beyond the lenght of the array");
        return ptr[i];
    }

    char& operator[](usize i) const { return ptr[i]; }

    char& operator[](usize i) { return ptr[i]; }

    bool operator==(const char* _buf) const {
        if (!_buf) return len == 0;
        auto buflen = strlen(_buf);
        if (len != buflen) return false;
        return memcmp(_buf, ptr, buflen) == 0;
    }

    bool operator==(String _buf) const {
        if (len != _buf.len) return false;
        return memcmp(_buf.ptr, ptr, _buf.len) == 0;
    }

    bool operator==(CortexStr& _buf) const {
        if (len != _buf.len) return false;
        return memcmp(_buf.ptr, ptr, _buf.len) == 0;
    }

    String operator+(String& s) {
        String res = this->copy();
        res.append(s);
        return res;
    }

    String operator+(const char* s) {
        String res = this->copy();
        res.append(s);
        return res;
    }
    operator CortexStr() { return CortexStr(ptr, len); }
};

template <typename T, usize FixedSize> struct StaticArray {
    T items[FixedSize];
    usize len = 0;
    usize capacity = FixedSize;

    void ensure_capacity(usize new_capacity) {
        if (capacity < new_capacity) {
            fprintf(stderr,
                    "[error]: StaticArray<T,size> cannot hold more than the Size assigned at comptime , size "
                    "= %lu\n",
                    FixedSize);
            exit(1);
        }
        return;
    }

    constexpr bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    constexpr u32 append(T* items, usize n) {
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&items[len], items, n);
    }

    constexpr u32 append(T elem) {
        if (capacity <= len + 1) ensure_capacity(len + 1);
        items[len++] = elem;
        return len - 1;
    }

    constexpr void resize(usize new_len) {
        ensure_capacity(new_len);
        len = new_len;
    }

    void append_items(const T* items, size_t count) {
        for (size_t i = 0; i < count; i++) {
            append(items[i]);
        }
    }

    const T& last() const {
        assert(len >= 1);
        return items[len - 1];
    }

    usize size() const { return len; }
    char* data() { return &items[0]; }
    bool empty() const { return capacity == 0; }
    void destroy() {}
    T& last() {
        assert(len >= 1);
        return items[len - 1];
    }

    T* begin() { return &items[0]; }
    T* end() { return &items[FixedSize - 1]; }

    T& operator[](usize i) const { return items[i]; }

    T& operator[](usize i) { return items[i]; }
};

template <typename T> struct Span {
    T start, end;

    T& operator[](u8 i) {
        assert(i < 2);
        return i == 0 ? start : end;
    }
    const T& operator[](u8 i) const {
        assert(i < 2);
        return i == 0 ? start : end;
    }
};

enum class Ok : u8 {
    NullOpt,
    ErrorResult,
    Done,
};

template <typename T> struct [[nodiscard]] Option {
  private:
    T* _M_value = nullptr;
    bool checked = false;

  public:
    ~Option() {
        if (_M_value) free(_M_value);
        _M_value = nullptr;
        checked = false;
    }
    Option() = default;

    Option(std::nullptr_t n) { (void)n; }
    Option(T _value) {
        _M_value = (T*)malloc(sizeof(T));
        *_M_value = _value;
    }

    Option<T>& set(T _value) {
        _M_value = (T*)malloc(sizeof(T));
        *_M_value = _value;
        return *this;
    }

    template <typename Lambda> T unwrap_or_else(Lambda fn) { fn; }
    T unwrap_or(T _value) { return _M_value != nullptr ? *_M_value : _value; }

    T unwrap() const {
        if (_M_value != nullptr) return *_M_value;

        if (!checked) {
            fprintf(stderr, "[panic]: Optional value unchcked\n");
            fprintf(stderr, "         %s() from  %s:%d\n", __func__, __FILE_NAME__, __LINE__);
            exit(1);
        }
        return *_M_value;
    }
    operator bool() { return _M_value != nullptr; }
    bool operator==(void* p) { return (void*)_M_value == p; }
    bool operator==(Option<T> _v) { return *_M_value == _v; }
};

struct Error {
    Error() = default;
    Error(const char* _err) { name = _err; }
    const char* name;
};

template <typename T> struct ErrorUnion {
    ErrorUnion() = default;
    ErrorUnion(Error __error) : error(__error) {}
    ErrorUnion(T _value) {
        _M_value = (T*)malloc(sizeof(T));
        *_M_value = _value;
    }

  private:
    union {
        T* _M_value;
        Error error;
    };

    template <typename Tp> friend struct Result;
};

template <typename T> struct [[nodiscard]] Result : ErrorUnion<T> {
    Result() = default;
    Result(T _value) : ErrorUnion<T>(_value) { valid = true; }
    Result(Error __error) : ErrorUnion<T>(__error) { valid = false; }

  public:
    constexpr T& unwrap() const {
        if (valid) { return *this->_M_value; }
        fprintf(stderr, "[panic: Result<T>]: %s at (%s:%d) `%s`\n", error_name(), __FILE_NAME__, __LINE__,
                __PRETTY_FUNCTION__);
        exit(1);
    }

    constexpr operator bool() const { return valid; }

    constexpr operator T() const {
        if (!valid) unwrap();

        return *this->_M_value;
    }

    // ok() and value() are used for the Some macro
    constexpr Ok ok() const {
        if (!valid) return Ok::ErrorResult;

        return Ok::Done;
    }

    constexpr const char* error_name() const {
        if (!valid) { return this->error.name; }
        return nullptr;
    }

  private:
    bool valid = false;
};

template <typename Fn> struct DeferGuard {
    Fn func;

    ~DeferGuard() { func(); }
};

// 1. Open the std namespace to specialize the formatter

struct File {
    const char* name = nullptr;
    const char* content = nullptr;

    const char& operator[](usize _index) const { return content[_index]; }
    void deinit() { free((void*)content); }
};

constexpr File create_file(const char* name, const char* content) {
    File f;
    f.name = name;
    auto contlen = strlen(content);
    f.content = (char*)malloc(sizeof(char) * contlen + 1);
    memcpy((char*)f.content, content, contlen);
    return f;
}

constexpr Arena create_arena(usize block_size = ARENA_DEFAULT_BLOCK_SIZE) {
    auto a = Arena{block_size};
    return a;
}

// NOTE: this is for temporary allocations

/**************** Functions ********************/

constexpr File read_file(const char* file_name) {
    FILE* fp = fopen(file_name, "r+");
    defer(fclose(fp));

    if (!fp) {
        fprintf(stderr, "[Lexing Error]: failed to open file `%s`, at {%s: %s : %d}\n", file_name,
                __FILE_NAME__, __func__, __LINE__);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    File f = {};
    f.name = file_name;
    f.content = (char*)calloc(fsize + 1, sizeof(char));

    size_t n = fread((char*)f.content, sizeof(char), fsize, fp);

    if (n != fsize) {
        fprintf(stderr, "[Lexing Error]: expected %lu bytes from file `%s` but got %lu bytes\n", fsize,
                file_name, n);
        exit(1);
    }
    return f;
}

static constexpr const char* TokenKindNames[] = {
    [TOK_EOF] = "Eof",
    [TOK_IDENTIFIER] = "identifier",
    [TOK_TYPENAME] = "typename",
    [TOK_NUMBER_LITERAL] = "number_literal",
    [TOK_STRING_LITERAL] = "string_literal",
    [TOK_CHAR_LITERAL] = "char_literal",
    [TOK_LPAREN] = "l_paren",
    [TOK_RPAREN] = "r_paren",
    [TOK_LBRACE] = "l_brace",
    [TOK_RBRACE] = "r_brace",
    [TOK_LBRACKET] = "l_bracket",
    [TOK_RBRACKET] = "r_bracket",
    [TOK_PERIOD] = "period",
    [TOK_ELLIPSIS2] = "ellipsis2",
    [TOK_ELLIPSIS3] = "ellipsis3",
    [TOK_COLON] = "Colon",
    [TOK_COLON_EQ] = "colon_equal",
    [TOK_COLON_COLON] = "colon_colon",
    [TOK_EQUAL] = "Equal",
    [TOK_EQUAL_EQ] = "equal_equal",
    [TOK_SEMICOLON] = "semicolon",
    [TOK_COMMA] = "Comma",
    [TOK_BANG] = "BAng",
    [TOK_BANG_EQ] = "bang_equal",
    [TOK_QUESTIONMARK] = "questionmark",
    [TOK_DOLLARSIGN] = "dollar_sign",
    [TOK_ATSIGN] = "at_sign",
    [TOK_HASH] = "HAsh",
    [TOK_PLUS] = "PLus",
    [TOK_PLUS_PLUS] = "plus_plus",
    [TOK_PLUS_EQ] = "plus_equal",
    [TOK_MINUS] = "Minus",
    [TOK_MINUS_MINUS] = "minus_minus",
    [TOK_MINUS_EQ] = "minus_equal",
    [TOK_ARROW] = "Arrow",
    [TOK_ASTERISK] = "asterisk",
    [TOK_ASTERISK_EQ] = "asterisk_equal",
    [TOK_SLASH] = "Slash",
    [TOK_SLASH_EQ] = "slash_equal",
    [TOK_PERCENT] = "percent",
    [TOK_PERCENT_EQ] = "percent_equal",
    [TOK_PIPE] = "PIpe",
    [TOK_PIPE_EQ] = "pipe_equal",
    [TOK_PIPE_PIPE] = "pipe_pipe",
    [TOK_AMPERSAND] = "ampersand",
    [TOK_AMPERSAND_EQ] = "ampersand_equal",
    [TOK_AMPERSAND_AMPERSAND] = "ampersand_ampersand",
    [TOK_CARET] = "Caret",
    [TOK_CARET_EQ] = "caret_equal",
    [TOK_TILDE] = "Tilde",
    [TOK_TILDE_EQ] = "tilde_equal",

    [TOK_LESS_THAN] = "LessThan",
    [TOK_LESS_EQ] = "LessOrEqual",
    [TOK_SHL] = "ShiftLeft",
    [TOK_SHL_EQ] = "ShiftLeftEqual",

    [TOK_GREATER_THAN] = "GreaterThan",
    [TOK_GREATER_EQ] = "GreaterOrEqual",
    [TOK_SHR] = "ShiftRight",
    [TOK_SHR_EQ] = "ShiftRightEqual",

    [TOK_RETURN] = "keyword_return",
    [TOK_CONST] = "keyword_const",
    [TOK_LET] = "keyword_let",
    [TOK_STATIC] = "keyword_static",
    [TOK_IF] = "keyword_if",
    [TOK_ELSE] = "keyword_else",
    [TOK_FOR] = "keyword_for",
    [TOK_WHILE] = "keyword_while",
    [TOK_DO] = "keyword_do",
    [TOK_GOTO] = "keyword_goto",
    [TOK_SWITCH] = "keyword_switch",
    [TOK_CASE] = "keyword_case",
    [TOK_BREAK] = "keyword_break",
    [TOK_DEFAULT] = "keyword_default",
    [TOK_STRUCT] = "keyword_struct",
    [TOK_ENUM] = "keyword_enum",
    [TOK_UNION] = "keyword_union",
    [TOK_TYPEDEF] = "keyword_typedef",
    [TOK_SIZEOF] = "keyword_sizeof",
    [TOK_SIGNED] = "Keyword_signed",
    [TOK_UNSIGNED] = "keyword_unsigned",
    [TOK_VOID] = "keyword_void",
    [TOK_INT] = "keyword_int",
    [TOK_BOOL] = "keyword_bool",
    [TOK_CHAR] = "keyword_char",
    [TOK_SHORT] = "keyword_short",
    [TOK_LONG] = "keyword_long",
    [TOK_FLOAT] = "keyword_float",
    [TOK_DOUBLE] = "keyword_double",
    [TOK_TRUE] = "keyword_true",
    [TOK_FALSE] = "keyword_false",
    [TOK_U8] = "keyword_u8",
    [TOK_I8] = "keyword_i8",
    [TOK_U16] = "keyword_u16",
    [TOK_I16] = "keyword_i16",
    [TOK_U32] = "keyword_u32",
    [TOK_I32] = "keyword_i32",
    [TOK_U64] = "keyword_u64",
    [TOK_I64] = "keyword_i64",
    [TOK_F32] = "keyword_f32",
    [TOK_F64] = "keyword_f64",
    [TOK_INCLUDE] = "keyword_include",
    [TOK_NULL] = "keyword_null",
    [TOK_WHITESPACE] = "<whitespace>",
};

static constexpr const char* AstKindNames[] = {
    [AST_ROOT] = "Root",
    [AST_ADD] = "add",
    [AST_SUB] = "sub",
    [AST_MUL] = "mul",
    [AST_DIV] = "div",
    [AST_MOD] = "mod",
    [AST_LT] = "lT",
    [AST_GT] = "gT",
    [AST_LT_EQ] = "LtEq",
    [AST_GT_EQ] = "GtEq",
    [AST_EQ_EQ] = "EqEq",
    [AST_NOT_EQ] = "NotEq",
    [AST_BOOL_AND] = "BoolAnd",
    [AST_BOOL_OR] = "BoolOr",
    [AST_BIT_OR] = "BitOr",
    [AST_BIT_AND] = "BitAnd",
    [AST_BIT_XOR] = "BitXor",
    [AST_SHL] = "shl",
    [AST_SHR] = "shr",
    [AST_BOOL_NOT] = "BoolNot",
    [AST_NEG] = "neg",
    [AST_BIT_NOT] = "BitNot",
    [AST_ADDRESS_OF] = "AddressOf",
    [AST_DEREF] = "Dereference",
    [AST_IDENTIFIER] = "Identifier",
    [AST_STRING_LITERAL] = "StringLiteral",
    [AST_NUMBER_LITERAL] = "NumberLiteral",
    [AST_BOOL_LITERAL] = "BooleanLiteral",
    [AST_NULL_LITERAL] = "NullLiteral",
    [AST_ENUM_LITERAL] = "EnumLiteral",
    [AST_GROUPED_EXPR] = "GroupedExpr",
    [AST_CALL] = "Call",
    [AST_MEMBER] = "Member",
    [AST_CAST] = "Cast",
    [AST_SIZE_OF] = "SizeOf",
    [AST_ARRAY_ACCESS] = "ArrayAccess",
    [AST_STRUCT_INIT] = "StructInit",
    [AST_EXPR_STMT] = "ExprStmt",
    [AST_BLOCK] = "Block",
    [AST_RETURN] = "Return",
    [AST_DEFER] = "Defer",
    [AST_CALL_STMT] = "CallStmt",
    [AST_ASSIGN] = "Assign",
    [AST_VAR_DECL] = "TypedVarDecl",
    [AST_CONST_DECL] = "ConstDecl",
    [AST_INFERRED_VAR_DECL] = "InferredVarDecl",
    [AST_IF] = "IfStmt",
    [AST_WHILE] = "WhileLoop",
    [AST_FOR] = "ForLoop",
    [AST_FUN_PARAM] = "FuncParam",
    [AST_STRUCT_FIELD] = "StructField",
    [AST_FUN_DECL] = "FuncDef",
    [AST_FUN_PROTO] = "FuncProto",
    // [AST_STRUCT_DEF] = "StructDef",
    [AST_STRUCT] = "StructDecl",
    [AST_ENUM] = "EnumDef",
    [AST_UNION] = "UnionDef",
    [AST_GLOBAL_VAR_DECL] = "GlobalVarDecl",
    [AST_INCLUDE_STMT] = "IncludeStmt",
    [AST_MACRO_DEF] = "MacroDef",
    [AST_ALIAS_STMT] = "AliasStmt",
};

constexpr const char* to_str(AstTag _tag) { return AstKindNames[_tag]; }
constexpr const char* to_str(TokenTag _tag) { return TokenKindNames[_tag]; }
