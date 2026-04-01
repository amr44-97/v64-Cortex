#pragma once

#include "cortex.h"
#include "cortex_map.h"

typedef u32 TokenIndex;

struct Node;
struct Type;
struct Symbol;
struct Scope;
struct Ast;

enum SymbolTag {
    SYMB_VAR = 1 << 0,
    SYM_FUNC = 1 << 2,
    SYM_STRUCT = 1 << 3,
    SYM_ENUM = 1 << 4,
    SYM_UNION = 1 << 5,
    SYM_TYPE = SYM_STRUCT | SYM_ENUM | SYM_UNION,
};

enum TypeKind : u64 {
    TYPE_VOID = 1 << 0, // 1
    TYPE_INT = 1 << 1,
    TYPE_SIGNED = 1 << 2,
    TYPE_UNSIGNED = 1 << 3,
    TYPE_CHAR = 1 << 4,
    TYPE_SHORT = 1 << 5,
    TYPE_LONG = 1 << 6,
    TYPE_BOOL = 1 << 7,
    TYPE_DOUBLE = 1 << 8,
    TYPE_FLOAT = 1 << 9,
    TYPE_LONG_LONG = 1 << 10,
    TYPE_BITS8 = 1 << 11,
    TYPE_BITS16 = 1 << 12,
    TYPE_BITS32 = 1 << 13,
    TYPE_BITS64 = 1 << 14,
    TYPE_F32 = 1 << 15,
    TYPE_F64 = 1 << 16,
    TYPE_POINTER = 1 << 17,
    TYPE_ARRAY = 1 << 18,
    TYPE_NAME = 1 << 19, // Unresolved type
    TYPE_I8 = TYPE_SIGNED | TYPE_BITS8,
    TYPE_U8 = TYPE_UNSIGNED | TYPE_BITS8,

    TYPE_I16 = TYPE_SIGNED | TYPE_BITS16,
    TYPE_U16 = TYPE_UNSIGNED | TYPE_BITS16,
    TYPE_I32 = TYPE_SIGNED | TYPE_BITS32,
    TYPE_U32 = TYPE_UNSIGNED | TYPE_BITS32,
    TYPE_I64 = TYPE_SIGNED | TYPE_BITS64,
    TYPE_U64 = TYPE_UNSIGNED | TYPE_BITS64,

    TYPE_LONG_DOUBLE = TYPE_LONG | TYPE_DOUBLE,
    TYPE_SINT = TYPE_SIGNED | TYPE_INT,
    TYPE_UINT = TYPE_UNSIGNED | TYPE_INT,
    TYPE_SCHAR = TYPE_SIGNED | TYPE_CHAR,
    TYPE_UCHAR = TYPE_UNSIGNED | TYPE_CHAR,
    TYPE_SSHORT = TYPE_SIGNED | TYPE_SHORT,
    TYPE_USHORT = TYPE_UNSIGNED | TYPE_SHORT,
    TYPE_SLONG = TYPE_SIGNED | TYPE_LONG,
    TYPE_ULONG = TYPE_UNSIGNED | TYPE_LONG,
};

const char* to_str(u64 type_kind);

// AU64st contain an dynArray<Type> similar to the Node list
// index 0 means null

// TODO: add string interner in the Ast to replace StringRef

struct Container {
    // TypeName member_name = value;
    DynArray<Type*> fields;

    // in case of enums , should be a constant expression
    DynArray<Node*> members;
};

// used for both arrays and pointers
struct PtrType {
    Type* base;
    Node* len;
};

struct Type {
    u64 id;
    String name = {};
    bool resolved = false;
    bool is_static = false;
    bool is_const = false;

    union {
        Container Struct;
        Container Enum;
        PtrType ptr;
        PtrType array;
        void* none;
    };

    const char* to_str() {
        String s = "";
        if (id == TYPE_ARRAY) {
            s.append("Array<");
            s.append(array.base->to_str());
            s.append(">");
        } else if (id == TYPE_POINTER) {
            s.append("Pointer<");
            s.append(ptr.base->to_str());
            s.append(">");
        } else if (id == TYPE_NAME) {
            s.append(name);
        } else {
            return ::to_str(id);
        }
        return s.ptr;
    }
};

struct Symbol {
    SymbolTag tag;
    String name;
    Type* type;
    Node* decl_node;
};

struct ISymbolTable {
    virtual ~ISymbolTable() = default;

    // Scope Management
    virtual void enter_scope() = 0;
    virtual void exit_scope() = 0;

    // Symbol Management
    virtual void declare(StringRef name, Symbol* sym) = 0;

    // Looks up a symbol starting from the current scope, moving upwards
    virtual Symbol* lookup(StringRef name) = 0;

    // Optional: Only check the current immediate scope (useful to catch duplicate declarations like `int x;
    // int x;`)
    virtual Symbol* lookup_current_scope(StringRef name) = 0;
};

struct StackSymbolTable : ISymbolTable {
    Stack<StringMap<Symbol*>> scopes;
    
	StackSymbolTable(Arena* arena) : scopes(arena) {
        // Always push a global scope on initialization
        enter_scope();
    }

    void enter_scope() override { scopes.append(StringMap<Symbol*>{}); }

    void exit_scope() override {
        // Destroy the deepest scope, dropping all its variables
        if (scopes.len > 1) { // Prevent popping the global scope
            scopes.pop();
        }
    }
    void declare(StringRef name, Symbol* sym) override {
        // Always declare in the most deeply nested scope
        scopes.last().put(name, sym);
    }

    Symbol* lookup(StringRef name) override {
        // Iterate backwards from innermost scope to global scope
        for (int i = scopes.len - 1; i >= 0; --i) {
            auto result = scopes[i].get(name);
            if (result) { return result.unwrap(); }
        }
        return nullptr; // Not found in any active scope
    }

    Symbol* lookup_current_scope(StringRef name) override {
        auto result = scopes.last().get(name);
        if (result) return result.unwrap();
        return nullptr;
    }
};

// the Op is known by the AstKind
struct BinaryExpr {
    Node* lhs;
    Node* rhs;
};

// the Op is known by the AstKind
struct UnaryExpr {
    Node* expr;
};

struct Member {
    Node* parent_expr;
    Node* field;

    // concatinate all the name from nodes separated by `.`
    const char* get_final_name();
};

struct FuncDeclArg {
    Node* name;
    Type* type;
};

struct Func {
    // index into a string list saved by the Ast
    String name;
    // DynArray<Index> arg_name;
    // // index into a type list saved by the Ast
    // DynArray<Index> arg_types;
    DynArray<Node*> args;
    // a list of expressions, since they are optional
    DynArray<Node*>* default_values = nullptr;

    // can be 0 if the tag of kind is FuncDecl
    Node* body;
};

struct Block {
    DynArray<Node*> stmts;
};

struct Return {
    Node* expr;
};

struct VarDecl {
    StringRef name;
    Type* type;
    Node* init_expr;
};

struct IfStmt {
    Node* condition;
    Node* then_body;
    Node* else_body;
};

// for ([init]; [condition]; [step]) [body]
struct LoopStmt {
    Node* label;
    Node* init;
    Node* condition;
    Node* step; // payload
    Node* body;
};

struct TypeAlias {
    String alias;
    String original_type;
};

struct Call {
    // expr . ident
    Node* callee;
    // list of expr
    DynArray<Node*> args;
    // for named arguments
    // TODO: Impel later
    // DynArray<String> arg_names;
};

// expr[index]
struct ArrayAccess {
    Node* ident; // array name
    Node* expr;  // index expr
};

// `Expr`[`start`..`end`]
struct Slice {
    Node* expr;
    Node* start;
    Node* end;
};

// (Type) expr
struct Cast {
    Type* type; // cast to
    Node* expr;
};

struct StructT {
    String name;
    DynArray<Node*> fields;
};

struct StructField {
    String name;
    Type* type;
    Node* init_expr;
};

struct Impl {
    String struct_name;
    DynArray<Node*> methods;
};

// 1) #include stdio.*;
// 2) #include "./file.c12" as ffl;
// for the standard library modules use Identifiers
// for user modules use file paths
struct Include {
    Token* path;
    Token* alias;
    bool wildcard;
};

struct Defer {
    Node* stmt;
};

struct SizeOf {
    Node* expr;
};

struct Decl {
    bool is_pub = true; // for now let's use static to make a decl private else it's public
                        // so we decls are public by default.
    Node* node;
};

struct Root {
    DynArray<Node*> children;
};

struct Node {
    AstTag tag;
    // instead of adding it to the union
    Token main_token;
    Type* type = nullptr;

    // we can traverse the children nodes to get the exact range of locs
    // literals values are contained within the Token
    union { /* data */
        Root root;
        Func func;
        Call call;
        Return ret;
        Block block;
        UnaryExpr unary;
        BinaryExpr binary;
        ArrayAccess array_access;
        Slice slice;
        Node* grouped; // for grouped expr
        VarDecl var;
        IfStmt if_stmt;
        LoopStmt while_stmt;
        LoopStmt for_stmt;
        Impl impl;

        StructT struct_decl;
        StructField struct_field;
        Include include;
        Defer defer;
    };

    void deinit();
};

// location of an AstNode like Token's Location
struct LocationSpan {
    u32 start;
    u32 end;
};

struct Ast {
    const char* file_name;
    StringRef source;       // the file from which we are producing this Ast
    DynArray<Token> tokens; // uses realloc not the Ast arena
    Token current;
    Arena arena{256 * 1024};
    DynArray<Node> node_pool{&arena};
    DynArray<Type> type_pool{&arena};
    Scope* scope;
    u32 toki; // current token index

    Ast() {
        // node_pool.allocator = &arena;
        // type_pool.allocator = &arena;
    }
    void deinit() {
        tokens.destroy();
        arena.deinit();
        // for (auto n : node_pool) {
        //     n.deinit();
        // }
        // node_pool.destroy();
        // type_pool.destroy();
    }

    void dump_tokens() {
        for (auto tok : tokens) {
            printf("%s -> `%.*s`\n", to_str(tok.tag), FmtStrRef(tok.buf));
        }
    }
    void set_current_token() { current = tokens[toki]; }
    void skip_token() {
        toki++;
        current = tokens[toki];
    }

    // just explicit
    void skip_token(TokenTag k) {
        assert(tokens[toki].tag == k);
        toki++;
        current = tokens[toki];
    }

    void advance(u32 i = 1) {
        toki += i;
        set_current_token();
    }

    constexpr auto match(std::initializer_list<TokenTag> items) -> bool {
        const auto begin = items.begin();
        const auto size = items.size();

        if (tokens.len - toki < size) return false;

        for (usize i = 0; i < size; i++) {
            if (begin[i] != tokens[toki + i].tag) return false;
        }

        return true;
    }

    constexpr auto match_go(std::initializer_list<TokenTag> items) -> bool {
        const auto begin = items.begin();
        const auto size = items.size();

        if (tokens.len - toki < size) return false;

        for (usize i = 0; i < size; i++) {
            if (begin[i] != tokens[toki + i].tag) return false;
        }
        toki += size;
        current = tokens[toki];
        return true;
    }
    Node* new_node() {
        auto idx = node_pool.append(Node{});
        return &node_pool.ptr[idx];
    }

    Type* new_type() {
        auto idx = type_pool.append(Type{});
        return &type_pool.ptr[idx];
    }

    Type* new_type(Type t) {
        auto idx = type_pool.append(t);
        return &type_pool.ptr[idx];
    }

    Node* new_node(Node n) {
        auto idx = node_pool.append(n);
        return &node_pool.ptr[idx];
    }

    Token get_token(i32 offset = 0) { return tokens[toki + offset]; }
    Token get(u32 offset) { return tokens[offset]; }
    StringRef get_buf(i32 offset = 0) { return tokens[toki + offset].buf; }
    TokenTag get_kind(i32 offset = 0) { return tokens[toki + offset].tag; }
};

Ast new_ast(File source);
void record_types(Ast&);
Option<Token> eat_token(Ast&, TokenTag);
Token& expect_token(Ast&, TokenTag);
Token& next_token(Ast&);
bool is_typename(TokenTag kind);

Type* parse_base_type(Ast& ast);
Node* parse(Ast&); // the entry point
Node* primary(Ast&);
Node* parse_unary(Ast&);
Node* parse_precedence(Ast&, int);
Node* parse_expr(Ast&);
Node* parse_stmt(Ast&);
Node* parse_block(Ast&);
Node* parse_include(Ast&);
Node* parse_struct_field(Ast&);
Node* parse_struct(Ast&);
Node* parse_enum(Ast&);
Node* parse_union(Ast&);
Node* parse_for(Ast&);
Node* parse_if(Ast&);
Node* parse_while(Ast&);
Node* parse_switch(Ast&);
Node* parse_fn_param(Ast&);
Node* parse_fn_proto(Ast&);
Node* parse_fn_def(Ast&);
Node* parse_var_decl(Ast&);
Type* parse_type_expr(Ast&);
