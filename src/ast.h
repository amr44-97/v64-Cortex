#pragma once

#include "c12-lib.h"
#include <cassert>

#define copy_and_nullterm(__string_t)                                                                        \
    ({                                                                                                       \
        auto buflen = __string_t.size();                                                                     \
        auto __buf = temp_alloc(char, buflen + 1);                                                           \
        memcpy(__buf, __string_t.data(), buflen);                                                            \
        __buf[buflen] = '\0';                                                                                \
        __buf;                                                                                               \
    })

typedef u32 Index; // used as TokenIndex , NodeIndex, StringIndex
typedef u32 NodeIndex;
typedef u32 TokenIndex;
typedef Index StringIndex;
typedef Index TypeIndex;
typedef Index Name;

struct AstNode;
struct Type;

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
    String get_final_name();
};

union Literal {
    void* none;
    i64 int_value;
    double float_value;
    StringRef str;
    StringRef enum_literal;
    bool bool_value;
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
    StringIndex name;
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
    NodeIndex expr; // array name
    NodeIndex index;
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
    AstKind kind;
    // instead of adding it to the union
    TokenIndex main_token;
    Type* type = nullptr;
    // we can traverse the children nodes to get the exact range of locs

    union { /* data */
        Root root;
        Func func;
        Return ret;
        Block block;
        Literal literal;
        UnaryExpr unary;
        BinaryExpr binary;
        ArrayIndex array_index;
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
    StringRef file_name;
    StringRef source;       // the file from which we are producing this Ast
    DynArray<Token> tokens; // uses realloc not the Ast arena
    DynArray<Node> nodes;
    DynArray<Node> extra;
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

    // just explicit
    void skip_token(TokenKind k) {
        assert(tokens[toki].kind == k);
        toki++;
        current = tokens[toki];
    }

    template <typename T> T* alloc(usize nelems = 1) { return arena.alloc<T>(nelems); }

    void advance(u32 i = 1) { toki += i; }

    constexpr auto match(std::initializer_list<TokenKind> items) -> bool {
        const auto begin = items.begin();
        const auto size = items.size();

        if (tokens.len - toki < size)
            return false;

        for (usize i = 0; i < size; i++) {
            if (begin[i] != tokens[toki + i].kind)
                return false;
        }

        return true;
    }

    constexpr auto match_go(std::initializer_list<TokenKind> items) -> bool {
        const auto begin = items.begin();
        const auto size = items.size();

        if (tokens.len - toki < size)
            return false;

        for (usize i = 0; i < size; i++) {
            if (begin[i] != tokens[toki + i].kind)
                return false;
        }
        toki += size;
        current = tokens[toki];
        return true;
    }

    Token get_token(i32 offset = 0) { return tokens[toki + offset]; }
    Token get(u32 offset) { return tokens[offset]; }
    StringRef get_buf(i32 offset = 0) { return tokens[toki + offset].buf; }
    TokenKind get_kind(i32 offset = 0) { return tokens[toki + offset].kind; }
};

Ast new_ast(File source);
Optional<TokenIndex> eat_token(Ast&, TokenKind);
TokenIndex expect_token(Ast&, TokenKind);
TokenIndex next_token(Ast&);

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
