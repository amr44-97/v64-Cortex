#pragma once

#include "cortex.h"
#include "initializer_list.h"

typedef u32 Index; // used as TokenIndex , NodeIndex, StringIndex
typedef u32 NodeIndex;
typedef u32 TokenIndex;
typedef Index StringIndex;
typedef Index TypeIndex;
typedef Index Name;

struct AstNode;
struct Type;

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
    DynArray<TypeIndex> fields;
    // TypeName member_name = value;
    DynArray<NodeIndex> members; // in case of enums , should be a constant expression
};

// used for both arrays and pointers
struct PtrType {
    TypeIndex base;
    NodeIndex len;
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
};

// the Op is known by the AstKind
struct BinaryExpr {
    NodeIndex lhs;
    NodeIndex rhs;
};

// the Op is known by the AstKind
struct UnaryExpr {
    NodeIndex expr;
};

struct Member {
    NodeIndex parent_expr;
    NodeIndex field;

    // concatinate all the name from nodes separated by `.`
    const char* get_final_name();
};

struct FuncDeclArg {
    Index name;
    Index type;
};

struct Func {
    // index into a string list saved by the Ast
    StringIndex name;
    // DynArray<Index> arg_name;
    // // index into a type list saved by the Ast
    // DynArray<Index> arg_types;
    Span<Index> args;
    // a list of expressions, since they are optional
    DynArray<NodeIndex>* default_values = nullptr;

    // can be 0 if the tag of kind is FuncDecl
    NodeIndex body;
};

struct Block {
    DynArray<NodeIndex> statements;
};

struct Return {
    NodeIndex expr;
};

struct VarDecl {
    StringRef name;
    TypeIndex type;
    NodeIndex init_expr;
};

struct IfStmt {
    NodeIndex condition;
    NodeIndex then_body;
    NodeIndex else_body;
};

struct WhileStmt {
    NodeIndex condition;
    NodeIndex body;
    NodeIndex label;
};

// for ([init]; [condition]; [step]) [body]
struct ForStmt {
    NodeIndex init;
    NodeIndex condition;
    NodeIndex step;
    NodeIndex body;
};

struct TypeAlias {
    StringIndex alias;
    StringIndex original_type;
};

struct Call {
    // expr . ident
    NodeIndex callee;
    // list of expr
    DynArray<NodeIndex> args;
    // for named arguments
    // TODO: Impel later
    // DynArray<StringIndex> arg_names;
};

struct Identifier {
    StringIndex name;
};

// expr[index]
struct ArrayIndex {
    NodeIndex ident; // array name
    NodeIndex expr;  // index expr
};
// Arr[start..end]
struct Slice {
    NodeIndex expr;
    NodeIndex start;
    NodeIndex end;
};

// (Type) expr
struct Cast {
    TypeIndex type; // cast to
    NodeIndex expr;
};

struct StructT {
    StringIndex name;
    DynArray<NodeIndex> fields;
};

struct StructField {
    StringIndex name;
    TypeIndex type;
    NodeIndex init_expr;
};

struct Impl {
    StringIndex struct_name;
    DynArray<NodeIndex> methods;
};

// 1) #include stdio.*;
// 2) #include "./file.c12" as ffl;
// for the standard library modules use Identifiers
// for user modules use file paths
struct Include {
    TokenIndex path;
    TokenIndex alias;
    bool wildcard;
};

struct Defer {
    NodeIndex stmt;
};

struct SizeOf {
    NodeIndex expr;
};

struct Decl {
    bool is_pub = true; // for now let's use static to make a decl private else it's public
                        // so we decls are public by default.
    NodeIndex node;
};

struct Root {
    DynArray<NodeIndex> children;
};

struct Node {
    AstTag tag;
    // instead of adding it to the union
    TokenIndex main_token;
    TypeIndex type = 0;

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
        ArrayIndex array_access;
        NodeIndex grouped; // for grouped expr
        VarDecl var;
        IfStmt if_stmt;
        WhileStmt while_stmt;
        ForStmt for_stmt;
        Impl impl;

        StructT struct_decl;
        StructField struct_field;
        Include include;
        Defer defer;
    };
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
    DynArray<Node> nodes;
    DynArray<Node> extra;
    DynArray<Type> types;
    DynArray<StringRef> identifiers;
    Token current;
    Arena arena;
    u32 toki; // current token index

    void deinit() {
        tokens.destroy();
        nodes.destroy();
        arena.deinit();
    }

    void set_current_token() { current = tokens[toki]; }
    void skip_token() {
        toki++;
        current = tokens[toki];
    }

    Token token_of(NodeIndex node) { return tokens[nodes[node].main_token]; }
    // just explicit
    void skip_token(TokenTag k) {
        assert(tokens[toki].tag == k);
        toki++;
        current = tokens[toki];
    }

    template <typename T> T* alloc(usize nelems = 1) { return arena.alloc<T>(nelems); }

    void advance(u32 i = 1) {
        toki += i;
        set_current_token();
    }

    constexpr auto match(std::initializer_list<TokenTag> items) -> bool {
        const auto begin = items.begin();
        const auto size = items.size();

        if (tokens.len - toki < size)
            return false;

        for (usize i = 0; i < size; i++) {
            if (begin[i] != tokens[toki + i].tag)
                return false;
        }

        return true;
    }

    constexpr auto match_go(std::initializer_list<TokenTag> items) -> bool {
        const auto begin = items.begin();
        const auto size = items.size();

        if (tokens.len - toki < size)
            return false;

        for (usize i = 0; i < size; i++) {
            if (begin[i] != tokens[toki + i].tag)
                return false;
        }
        toki += size;
        current = tokens[toki];
        return true;
    }

    Token get_token(i32 offset = 0) { return tokens[toki + offset]; }
    Token get(u32 offset) { return tokens[offset]; }
    StringRef get_buf(i32 offset = 0) { return tokens[toki + offset].buf; }
    TokenTag get_kind(i32 offset = 0) { return tokens[toki + offset].tag; }
};

Ast new_ast(File source);
Option<TokenIndex> eat_token(Ast&, TokenTag);
TokenIndex expect_token(Ast&, TokenTag);
TokenIndex next_token(Ast&);

TypeIndex parse_base_type(Ast& ast);
NodeIndex parse(Ast&); // the entry point
NodeIndex primary(Ast&);
NodeIndex parse_unary(Ast&);
NodeIndex parse_precedence(Ast&, int);
NodeIndex parse_expr(Ast&);
NodeIndex parse_include(Ast&);
NodeIndex parse_struct_field(Ast&);
NodeIndex parse_struct(Ast&);
NodeIndex parse_enum(Ast&);
NodeIndex parse_union(Ast&);

NodeIndex parse_fn_param(Ast&);
NodeIndex parse_fn_proto(Ast&);
NodeIndex parse_fn_def(Ast&);
NodeIndex parse_var_decl(Ast&);
TypeIndex parse_type_expr(Ast&);
