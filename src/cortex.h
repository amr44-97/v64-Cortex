#pragma once

#include <assert.h>
#include <errno.h>
#include <initializer_list>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*************************************************
               Primitives and Macros
**************************************************/
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

constexpr size_t ARENA_DEFAULT_BLOCK_SIZE = 8 * 1024;

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

/*************************************************
            Utilities and Error Handling
**************************************************/

template <typename Fn> struct DeferGuard {
    Fn func;

    ~DeferGuard() { func(); }
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
    bool operator==(std::nullptr_t nt) { return _M_value == nt; }
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

/**************************************
                        Memory Management
**************************************/

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

enum TokenTag {
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
    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of 2");

    T* ptr = nullptr;
    u32 len = 0;
    u32 capacity = 0;

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

template <typename T> struct Stack : DynArray<T> {
    Stack() {}
    explicit Stack(Allocator* allocator) : DynArray<T>(allocator) {}
    explicit Stack(Allocator* allocator, u32 _capacity) : DynArray<T>(allocator, _capacity) {}

    T& push(T item) {
        this->append(item);
        return this->last();
    }

    T& pop() {
        assert(this->len >= 1);
        T& t = this->last();
        this->len -= 1;
        return t;
    }
};

struct Arena : Allocator {
    RawArray<u8*> blocks = {};
    usize offset = 0;
    usize block_size = ARENA_DEFAULT_BLOCK_SIZE;

    ~Arena() {
        if (!blocks.empty()) deinit();
    }

    Arena() {
        blocks.ensure_capacity(256);
        // u8* _mem = (u8*)calloc(block_size, sizeof(u8));
        void* _mem = mmap(nullptr, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (_mem == MAP_FAILED) {
            fprintf(stderr, "[error]: mmap failed with %s\n", strerror(errno));
            exit(1);
        }
        blocks.append(static_cast<u8*>(_mem));
    }

    Arena(usize _block_size) {
        blocks.ensure_capacity(256);
        // u8* _mem = (u8*)calloc(block_size, sizeof(u8));
        void* _mem = mmap(nullptr, _block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (_mem == MAP_FAILED) {
            fprintf(stderr, "[error]: mmap failed with %s\n", strerror(errno));
            exit(1);
        }
        blocks.append(static_cast<u8*>(_mem));
    }

    void dealloc(void* ptr) override { (void)ptr; }
    void deinit() override {
        for (usize i = 0, res = 1; (i < blocks.len); ++i) {
            if (munmap(blocks[i], block_size) == -1) {
                fprintf(stderr, "[error]: munmap failed with %s\n", strerror(errno));
                exit(1);
            }
        }

        blocks.destroy();
    }

    void* alloc(usize size) override {
        // size = (size + alignof(u8) - 1) & ~(alignof(u8) - 1); // align to 8 bytes
        if (size > block_size - offset) { // no enough space in this block
            void* _mem =
                mmap(nullptr, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (_mem == MAP_FAILED) {
                fprintf(stderr, "[error]: mmap failed with %s\n", strerror(errno));
                exit(1);
            }
            blocks.append(static_cast<u8*>(_mem));
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

constexpr Arena create_arena(usize block_size = ARENA_DEFAULT_BLOCK_SIZE) {
    auto a = Arena{block_size};
    return a;
}

static Arena global_arena{};
static Arena buffer_allocator{};
static Allocator& string_allocator = c_allocator;

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

    ~String() {
        // we are using string_allocator , an arena allocator
        // so do not implement this destructor
        destroy();
    }

    // Block implicit copy
    String(const String& _str) : String(_str.ptr, _str.len) {}

    // String(const String& _str) = delete;
    String& operator=(const String& _str) {
        if (this == &_str) return *this;
        this->destroy();
        this->append(_str.ptr, _str.len);
        return *this;
    };

    // implement move constructor
    String(String&& _str) noexcept {
        ptr = _str.ptr;
        len = _str.len;
        capacity = _str.capacity;
        _str.ptr = nullptr;
        _str.capacity = _str.len = 0;
    }

    String(const CortexStr& __str) : String(__str.ptr, __str.len) {}
    String(const char* __str, usize n) { append(__str, n); }
    String(const char* __str) { append(__str); }
    String(char* __str) { append(__str); }

  private:
    String(char* _ptr, u32 _len, u32 _capacity) : ptr(_ptr), len(_len), capacity(_capacity) {}

  public:
    String clone() const {
        String s;
        s.append(ptr, len);
        assert(s.ptr != this->ptr);
        return s;
    }

    // explicit move
    String toOwner() {
        String res(ptr, len, capacity);
        ptr = nullptr;
        len = capacity = 0;
        return res;
    }

    void ensure_capacity(usize new_capacity = 16) {
        if (capacity >= new_capacity) return;
        usize better_capacity = capacity;

        do {
            better_capacity = better_capacity * 5 / 2 + 8;
        } while (better_capacity < new_capacity);

        const usize aligned_size = better_capacity;

        char* tmp = (char*)string_allocator.alloc(aligned_size);
        if (ptr != nullptr and len > 0) {
            memcpy(tmp, ptr, len);
            string_allocator.dealloc(ptr);
        }
        ptr = tmp;
        capacity = better_capacity;
    }

    void reset() {
        memset(ptr, 0, capacity);
        len = 0;
    }

    void destroy() {
        if (ptr) { string_allocator.dealloc(ptr); }
        ptr = nullptr;
        len = capacity = 0;
    }

    constexpr bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    u32 append(const char* items, usize n) {
        if (!enough_space_for(n + 1)) ensure_capacity(len + n + 1);

        memcpy((char*)&ptr[len], items, n);
        len += n;
        ptr[len] = '\0';
        return len;
    }

    u32 append(char* items, usize n) {
        if (!enough_space_for(n + 1)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items, n);
        len += n;
        ptr[len] = '\0';
        return len;
    }

    u32 append(const char* items) { return append(items, strlen(items)); }

    u32 append(String& items) {
        auto n = items.size();
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&ptr[len], items.data(), n);
        len += n;
        return len;
    }

    u32 append(StringRef items) { return append(items.data(), items.len); }

    void append(std::initializer_list<StringRef> items) {
        for (auto i : items)
            append(i);
    }

    // len =0 append('a'), ptr[len] = 'a' , len = 1 ptr[1] = '\0' ,return len - 1
    u32 append(char elem) {
        // at least enough space for `elem` and `null`
        if (capacity - len < 2) ensure_capacity(len + 1);
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
        String res = this->clone();
        res.append(s);
        return res;
    }

    String operator+(const char* s) {
        String res = this->clone();
        res.append(s);
        return res;
    }

    // move Assignment
    String& operator=(String&& _str) noexcept {
        if (this != &_str) {
            this->destroy();
            *this = _str.toOwner();
        }
        return *this;
    }

    operator CortexStr() { return CortexStr(ptr, len); }
};

struct StringBuilder {
    char* ptr = nullptr;
    u32 len = 0;
    u32 capacity = 0;
    StringBuilder() {}

    ~StringBuilder() {
        // we are using string_allocator , an arena allocator
        // so do not implement this destructor
    }

    // Block implicit copy
    StringBuilder(const String& _str) : StringBuilder(_str.ptr, _str.len) {}

    // String(const String& _str) = delete;
    StringBuilder& operator=(const StringBuilder& _str) {
        if (this == &_str) return *this;
        this->destroy();
        this->append(_str.ptr, _str.len);
        return *this;
    };

    // implement move constructor
    StringBuilder(StringBuilder&& _str) noexcept {
        ptr = _str.ptr;
        len = _str.len;
        capacity = _str.capacity;
        _str.ptr = nullptr;
        _str.capacity = _str.len = 0;
    }

    StringBuilder(const CortexStr& __str) : StringBuilder(__str.ptr, __str.len) {}
    StringBuilder(const char* __str, usize n) { append(__str, n); }
    StringBuilder(const char* __str) { append(__str); }
    StringBuilder(char* __str) { append(__str); }

  private:
    StringBuilder(char* _ptr, u32 _len, u32 _capacity) : ptr(_ptr), len(_len), capacity(_capacity) {}

  public:
    StringBuilder clone() const {
        String s;
        s.append(ptr, len);
        assert(s.ptr != this->ptr);
        return s;
    }

    // explicit move
    StringBuilder toOwner() {
        StringBuilder res(ptr, len, capacity);
        ptr = nullptr;
        len = capacity = 0;
        return res;
    }

    void ensure_capacity(usize new_capacity = 16) {
        if (capacity >= new_capacity) return;
        usize better_capacity = capacity;

        do {
            better_capacity = better_capacity * 5 / 2 + 8;
        } while (better_capacity < new_capacity);

        const usize aligned_size = better_capacity;

        char* tmp = (char*)c_allocator.alloc(aligned_size);
        if (ptr != nullptr and len > 0) {
            memcpy(tmp, ptr, len);
            c_allocator.dealloc(ptr);
        }
        ptr = tmp;
        capacity = better_capacity;
    }

    void reset() {
        memset(ptr, 0, capacity);
        len = 0;
    }

    void destroy() {
        if (ptr) { c_allocator.dealloc(ptr); }
        ptr = nullptr;
        len = capacity = 0;
    }

    constexpr bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    u32 append(StringBuilder& items) {
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
        ptr[len] = '\0';
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

    StringBuilder operator+(String& s) {
        StringBuilder res = this->clone();
        res.append(s);
        return res;
    }

    StringBuilder operator+(const char* s) {
        StringBuilder res = this->clone();
        res.append(s);
        return res;
    }

    // move Assignment
    StringBuilder& operator=(StringBuilder&& _str) noexcept {
        if (this != &_str) {
            this->destroy();
            *this = _str.toOwner();
        }
        return *this;
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

    bool enough_space_for(u32 n_items) const {
        if ((capacity - len) >= n_items) return true;
        return false;
    }

    u32 append(T* items, usize n) {
        if (!enough_space_for(n)) ensure_capacity(len + n + 1);

        memcpy(&items[len], items, n);
    }

    u32 append(T elem) {
        if (capacity <= len + 1) ensure_capacity(len + 1);
        items[len++] = elem;
        return len - 1;
    }

    void resize(usize new_len) {
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

static constexpr const char* to_str(TokenTag tag) {
    switch (tag) {
    case TOK_EOF:                 return "Eof";
    case TOK_IDENTIFIER:          return "identifier";
    case TOK_TYPENAME:            return "typename";
    case TOK_NUMBER_LITERAL:      return "number_literal";
    case TOK_STRING_LITERAL:      return "string_literal";
    case TOK_CHAR_LITERAL:        return "char_literal";
    case TOK_LPAREN:              return "l_paren";
    case TOK_RPAREN:              return "r_paren";
    case TOK_LBRACE:              return "l_brace";
    case TOK_RBRACE:              return "r_brace";
    case TOK_LBRACKET:            return "l_bracket";
    case TOK_RBRACKET:            return "r_bracket";
    case TOK_PERIOD:              return "period";
    case TOK_ELLIPSIS2:           return "ellipsis2";
    case TOK_ELLIPSIS3:           return "ellipsis3";
    case TOK_COLON:               return "Colon";
    case TOK_COLON_EQ:            return "colon_equal";
    case TOK_COLON_COLON:         return "colon_colon";
    case TOK_EQUAL:               return "Equal";
    case TOK_EQUAL_EQ:            return "equal_equal";
    case TOK_SEMICOLON:           return "semicolon";
    case TOK_COMMA:               return "Comma";
    case TOK_BANG:                return "BAng";
    case TOK_BANG_EQ:             return "bang_equal";
    case TOK_QUESTIONMARK:        return "questionmark";
    case TOK_DOLLARSIGN:          return "dollar_sign";
    case TOK_ATSIGN:              return "at_sign";
    case TOK_HASH:                return "HAsh";
    case TOK_PLUS:                return "PLus";
    case TOK_PLUS_PLUS:           return "plus_plus";
    case TOK_PLUS_EQ:             return "plus_equal";
    case TOK_MINUS:               return "Minus";
    case TOK_MINUS_MINUS:         return "minus_minus";
    case TOK_MINUS_EQ:            return "minus_equal";
    case TOK_ARROW:               return "Arrow";
    case TOK_ASTERISK:            return "asterisk";
    case TOK_ASTERISK_EQ:         return "asterisk_equal";
    case TOK_SLASH:               return "Slash";
    case TOK_SLASH_EQ:            return "slash_equal";
    case TOK_PERCENT:             return "percent";
    case TOK_PERCENT_EQ:          return "percent_equal";
    case TOK_PIPE:                return "PIpe";
    case TOK_PIPE_EQ:             return "pipe_equal";
    case TOK_PIPE_PIPE:           return "pipe_pipe";
    case TOK_AMPERSAND:           return "ampersand";
    case TOK_AMPERSAND_EQ:        return "ampersand_equal";
    case TOK_AMPERSAND_AMPERSAND: return "ampersand_ampersand";
    case TOK_CARET:               return "Caret";
    case TOK_CARET_EQ:            return "caret_equal";
    case TOK_TILDE:               return "Tilde";
    case TOK_TILDE_EQ:            return "tilde_equal";

    case TOK_LESS_THAN:           return "LessThan";
    case TOK_LESS_EQ:             return "LessOrEqual";
    case TOK_SHL:                 return "ShiftLeft";
    case TOK_SHL_EQ:              return "ShiftLeftEqual";

    case TOK_GREATER_THAN:        return "GreaterThan";
    case TOK_GREATER_EQ:          return "GreaterOrEqual";
    case TOK_SHR:                 return "ShiftRight";
    case TOK_SHR_EQ:              return "ShiftRightEqual";

    case TOK_RETURN:              return "keyword_return";
    case TOK_CONST:               return "keyword_const";
    case TOK_LET:                 return "keyword_let";
    case TOK_STATIC:              return "keyword_static";
    case TOK_IF:                  return "keyword_if";
    case TOK_ELSE:                return "keyword_else";
    case TOK_FOR:                 return "keyword_for";
    case TOK_WHILE:               return "keyword_while";
    case TOK_DO:                  return "keyword_do";
    case TOK_GOTO:                return "keyword_goto";
    case TOK_SWITCH:              return "keyword_switch";
    case TOK_CASE:                return "keyword_case";
    case TOK_BREAK:               return "keyword_break";
    case TOK_DEFAULT:             return "keyword_default";
    case TOK_STRUCT:              return "keyword_struct";
    case TOK_ENUM:                return "keyword_enum";
    case TOK_UNION:               return "keyword_union";
    case TOK_TYPEDEF:             return "keyword_typedef";
    case TOK_SIZEOF:              return "keyword_sizeof";
    case TOK_SIGNED:              return "Keyword_signed";
    case TOK_UNSIGNED:            return "keyword_unsigned";
    case TOK_VOID:                return "keyword_void";
    case TOK_INT:                 return "keyword_int";
    case TOK_BOOL:                return "keyword_bool";
    case TOK_CHAR:                return "keyword_char";
    case TOK_SHORT:               return "keyword_short";
    case TOK_LONG:                return "keyword_long";
    case TOK_FLOAT:               return "keyword_float";
    case TOK_DOUBLE:              return "keyword_double";
    case TOK_TRUE:                return "keyword_true";
    case TOK_FALSE:               return "keyword_false";
    case TOK_U8:                  return "keyword_u8";
    case TOK_I8:                  return "keyword_i8";
    case TOK_U16:                 return "keyword_u16";
    case TOK_I16:                 return "keyword_i16";
    case TOK_U32:                 return "keyword_u32";
    case TOK_I32:                 return "keyword_i32";
    case TOK_U64:                 return "keyword_u64";
    case TOK_I64:                 return "keyword_i64";
    case TOK_F32:                 return "keyword_f32";
    case TOK_F64:                 return "keyword_f64";
    case TOK_INCLUDE:             return "keyword_include";
    case TOK_NULL:                return "keyword_null";
    case TOK_WHITESPACE:          return "<whitespace>";
    }
};

static constexpr const char* to_str(AstTag tag) {
    switch (tag) {
    case AST_ROOT:              return "Root";
    case AST_ADD:               return "add";
    case AST_SUB:               return "sub";
    case AST_MUL:               return "mul";
    case AST_DIV:               return "div";
    case AST_MOD:               return "mod";
    case AST_LT:                return "lT";
    case AST_GT:                return "gT";
    case AST_LT_EQ:             return "LtEq";
    case AST_GT_EQ:             return "GtEq";
    case AST_EQ_EQ:             return "EqEq";
    case AST_NOT_EQ:            return "NotEq";
    case AST_BOOL_AND:          return "BoolAnd";
    case AST_BOOL_OR:           return "BoolOr";
    case AST_BIT_OR:            return "BitOr";
    case AST_BIT_AND:           return "BitAnd";
    case AST_BIT_XOR:           return "BitXor";
    case AST_SHL:               return "shl";
    case AST_SHR:               return "shr";
    case AST_BOOL_NOT:          return "BoolNot";
    case AST_NEG:               return "neg";
    case AST_BIT_NOT:           return "BitNot";
    case AST_ADDRESS_OF:        return "AddressOf";
    case AST_DEREF:             return "Dereference";
    case AST_IDENTIFIER:        return "Identifier";
    case AST_TYPENAME:          return "typename";
    case AST_STRING_LITERAL:    return "StringLiteral";
    case AST_NUMBER_LITERAL:    return "NumberLiteral";
    case AST_BOOL_LITERAL:      return "BooleanLiteral";
    case AST_NULL_LITERAL:      return "NullLiteral";
    case AST_ENUM_LITERAL:      return "EnumLiteral";
    case AST_GROUPED_EXPR:      return "GroupedExpr";
    case AST_CALL:              return "Call";
    case AST_MEMBER:            return "Member";
    case AST_CAST:              return "Cast";
    case AST_SIZE_OF:           return "SizeOf";
    case AST_ARRAY_ACCESS:      return "ArrayAccess";
    case AST_STRUCT_INIT:       return "StructInit";
    case AST_EXPR_STMT:         return "ExprStmt";
    case AST_BLOCK:             return "Block";
    case AST_RETURN:            return "Return";
    case AST_DEFER:             return "Defer";
    case AST_CALL_STMT:         return "CallStmt";
    case AST_ASSIGN:            return "Assign";
    case AST_VAR_DECL:          return "TypedVarDecl";
    case AST_CONST_DECL:        return "ConstDecl";
    case AST_INFERRED_VAR_DECL: return "InferredVarDecl";
    case AST_IF:                return "IfStmt";
    case AST_WHILE:             return "WhileLoop";
    case AST_FOR:               return "ForLoop";
    case AST_FUN_PARAM:         return "FuncParam";
    case AST_STRUCT_FIELD:      return "StructField";
    case AST_FUN_DECL:          return "FuncDef";
    case AST_FUN_PROTO:         return "FuncProto";
    // case AST_STRUCT_DEF : return  "StructDef";
    case AST_STRUCT:            return "StructDecl";
    case AST_ENUM:              return "EnumDef";
    case AST_UNION:             return "UnionDef";
    case AST_GLOBAL_VAR_DECL:   return "GlobalVarDecl";
    case AST_INCLUDE_STMT:      return "IncludeStmt";
    case AST_MACRO_DEF:         return "MacroDef";
    case AST_ALIAS_STMT:        return "AliasStmt";
    }
};
