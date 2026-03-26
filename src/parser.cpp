#include "ast.h"
#include "cortex.h"
#include <cstdio>

const NodeIndex null_node = 0;

bool is_typename(TokenTag kind) {
    using enum TokenTag;
    switch (kind) {
    case TOK_SIGNED:
    case TOK_UNSIGNED:
    case TOK_VOID:
    case TOK_INT:
    case TOK_CHAR:
    case TOK_SHORT:
    case TOK_LONG:
    case TOK_DOUBLE:
    case TOK_FLOAT:
    case TOK_I8:
    case TOK_U8:

    case TOK_I16:
    case TOK_U16:

    case TOK_I32:
    case TOK_U32:

    case TOK_I64:
    case TOK_U64:
    case TOK_IDENTIFIER: return true;
    default:             return false;
    }
}

auto next_token(Ast& ast) -> TokenIndex {
    auto i = ast.toki;
    ast.toki += 1;
    ast.set_current_token();
    return i;
}

StringRef line_of(Ast& ast, TokenIndex tok) {
    usize line_end = 0;
    usize buflen = ast.source.size();
    for (u32 i = 0; i < buflen; ++i) {
        if (ast.source[i] == '\n') {
            line_end = i;
            break;
        } else if (ast.source[i] == '\0')
            break;
    }
    auto line_start = ast.tokens[tok].loc.line_start;
    auto line_len = line_end - line_start;
    return ast.source.substr(line_start, line_len);
}

auto expect_token(Ast& ast, TokenTag _kind) -> TokenIndex {
    if (ast.current.tag == _kind)
        return next_token(ast);

    printf("[error]: expected token `%s` but found `%s : %.*s`\n", to_str(_kind), to_str(ast.current.tag),
           ast.current.buf);
    printf("        | %.*s\n", line_of(ast, ast.toki));
    exit(1);
}

auto eat_token(Ast& ast, TokenTag kind) -> Option<TokenIndex> {
    if (ast.current.tag == kind)
        return next_token(ast);
    return nullptr;
}

auto parse_include(Ast& ast) -> NodeIndex {
    using enum TokenTag;
    TokenIndex tok = eat_token(ast, TOK_INCLUDE).unwrap();
    Option<int> st = Some(123);

    auto path = expect_token(ast, TOK_STRING_LITERAL);
    bool has_wildcard = ast.match_go({TOK_PERIOD, TOK_ASTERISK});

    if (ast.get_buf() == "as") {
        next_token(ast);
        auto alias = expect_token(ast, TOK_IDENTIFIER);
        Node n = {
            .tag = AST_INCLUDE_STMT,
            .main_token = tok,
            .type = 0,
            .include = {path, alias, has_wildcard},
        };
        return ast.nodes.append(n);
    }

    Node n = {
        .tag = AST_INCLUDE_STMT,
        .main_token = tok,
        .type = 0,
        .include = {path, 0, has_wildcard},
    };
    return ast.nodes.append(n);
}

NodeIndex add_literal(Ast& ast, AstTag _tag) {
    auto res = ast.nodes.append({.tag = _tag, .main_token = ast.toki});
    next_token(ast);
    return res;
}

NodeIndex add_unary(Ast& ast, AstTag tag, TokenIndex main_token, NodeIndex expr) {
    return ast.nodes.append(Node{.tag = tag, .main_token = main_token, .unary = {.expr = expr}});
}

NodeIndex add_binary(Ast& ast, AstTag tag, TokenIndex main_token, NodeIndex lhs, NodeIndex rhs) {
    return ast.nodes.append(Node{.tag = tag, .main_token = main_token, .binary = {.lhs = lhs, .rhs = rhs}});
}

#define match_t(T) switch (T.tag)

#define with                                                                                                 \
    break;                                                                                                   \
    case

#define with_default                                                                                         \
    break;                                                                                                   \
    default

#define expect_expr_tag(AST, TAG)                                                                            \
    ({                                                                                                       \
        auto start_tok = (AST).toki;                                                                         \
        auto expr = parse_expr((AST));                                                                       \
        if ((AST).nodes[expr].tag != (TAG) || expr == null_node) {                                           \
            auto token_buf = ((AST).tokens[(AST).nodes[expr].main_token].buf);                               \
            printf("[error]: expected expr `%s` after %.*s but found `%s : %.*s`\n", to_str((TAG)),          \
                   ast.tokens[start_tok].buf, to_str((AST).nodes[expr].tag), (int)token_buf.size(),          \
                   token_buf.data());                                                                        \
            auto line_of_error = line_of(ast, (AST).nodes[expr].main_token);                                 \
            printf("        | %.*s\n", (int)line_of_error.size(), line_of_error.data());                     \
            exit(1);                                                                                         \
        }                                                                                                    \
        expr;                                                                                                \
    })

#define expect_expr(AST)                                                                                     \
    ({                                                                                                       \
        auto start_tok = (AST).toki;                                                                         \
        auto expr = parse_expr((AST));                                                                       \
        if (expr == null_node) {                                                                             \
            auto token_buf = ((AST).tokens[(AST).nodes[expr].main_token].buf);                               \
            printf("[error]: expected expr after %.*s but found `%s : %.*s`\n", ast.tokens[start_tok].buf,   \
                   to_str((AST).nodes[expr].tag), token_buf.size(), token_buf.data());                       \
            auto line_of_error = line_of(ast, (AST).nodes[expr].main_token);                                 \
            printf("        | %.*s\n", (int)line_of_error.size(), line_of_error.data());                     \
            exit(1);                                                                                         \
        }                                                                                                    \
        expr;                                                                                                \
    })

auto primary(Ast& ast) -> NodeIndex {
    match_t(ast.tokens[ast.toki]) {
        with TOK_IDENTIFIER : {
            auto lhs = add_literal(ast, AST_IDENTIFIER);

            {
                if (ast.current.tag == TOK_PERIOD) {
                    auto dot = next_token(ast); // skip the period
                    auto rhs = primary(ast);    // expected identifier
                    if (!rhs) {
                        printf("[error]: expected identifier after `%.*s.` but found `%s` `%.*s`\n",
                               ast.token_of(lhs).buf, to_str(ast.current.tag), ast.current.loc.len,
                               ast.current.buf.data());
                        auto line = line_of(ast, ast.toki);
                        printf("         | %.*s\n", (int)line.size(), line.data());
                        return null_node;
                    }
                    return ast.nodes.append({.tag = AST_MEMBER, .main_token = dot, .binary = {lhs, rhs}});
                }

                if (ast.match({TOK_LPAREN, TOK_RPAREN})) {
                    ast.advance(2);
                    return ast.nodes.append(Node{
                        .tag = AST_CALL,
                        .main_token = ast.nodes[lhs].main_token, // the identifier before the LPAREN
                        .call = Call{.callee = lhs, .args = {}},
                    });
                }

                if (ast.current.tag == TOK_LBRACKET) {
                    next_token(ast);
                    auto expr = expect_expr(ast);
                    expect_token(ast, TOK_RBRACKET);
                    return ast.nodes.append(Node{
                        .tag = AST_ARRAY_ACCESS,
                        .main_token = ast.nodes[lhs].main_token,
						// .type = ast.get_typeof(ast.get_typeof(lhs).array.element)
						// .type = scope_get_type(ast,lhs).array.base 
                        .array_access = {.ident = lhs, .expr = expr},
                    });
                }

                if (ast.current.tag == TOK_LPAREN) {
                    DynArray<NodeIndex> args = {};
                    next_token(ast); // skip `(`
                    while (true) {
                        auto expr = parse_expr(ast);
                        args.append(expr);
                        if (ast.current.tag == TOK_COMMA) {
                            next_token(ast);
                            continue;
                        }
                        if (ast.current.tag == TOK_RPAREN) {
                            next_token(ast);
                            break;
                        }
                    }

                    return ast.nodes.append(Node{
                        .tag = AST_CALL,
                        .main_token = ast.nodes[lhs].main_token, // the identifier before the LPAREN
						// .type = ast.get_typeof(ast.get_typeof(lhs).array.element)
						// .type = scope_get_type(ast,lhs).array.base 
						.call = Call{.callee = lhs, .args = args.move()},
                    });
                }
            }
            return lhs;
        }
        with TOK_STRING_LITERAL : {
            auto l = add_literal(ast, AST_STRING_LITERAL);
            return l;
        }
        with TOK_NUMBER_LITERAL : { return add_literal(ast, AST_NUMBER_LITERAL); }
        with TOK_TRUE : {
            auto l = add_literal(ast, AST_BOOL_LITERAL);
            return l;
        }
        with TOK_FALSE : {
            auto l = add_literal(ast, AST_BOOL_LITERAL);
            return l;
        }

        with TOK_NULL : {
            auto l = add_literal(ast, AST_NULL_LITERAL);
            return l;
        }

        with TOK_PERIOD : {
            if (ast.match({TOK_PERIOD, TOK_IDENTIFIER})) {
                ast.skip_token(TOK_PERIOD);
                auto l = add_literal(ast, AST_ENUM_LITERAL);
                return l;
            }
        }
        with TOK_LPAREN : {
            auto lpar = next_token(ast);
            auto expr = parse_expr(ast);
            expect_token(ast, TOK_RPAREN);
            return ast.nodes.append(Node{.tag = AST_GROUPED_EXPR, .main_token = lpar, .grouped = expr});
        }

    default:
        return null_node;
        // printf("[error]: expected primary expression but found `%s` `%.*s`\n", to_str(ast.current.tag),
        //        ast.current.loc.len, ast.current.buf.data());
        // auto line = line_of(ast, ast.toki);
        // printf("         | %.*s\n", (int)line.size(), line.data());
        // exit(1);

    } // end switch

    return null_node;
} // primary()

NodeIndex parse_expr(Ast& ast) { return parse_precedence(ast, 0); }

// Precedence constants for C-like operators
constexpr int PREC_NONE = 0;
constexpr int PREC_ASSIGN = 1;      // = += -= *= /= %= &= ^= |= <<= >>=
constexpr int PREC_LOGICAL_OR = 2;  // ||
constexpr int PREC_LOGICAL_AND = 3; // &&
constexpr int PREC_BITWISE_OR = 4;  // |
constexpr int PREC_BITWISE_XOR = 5; // ^
constexpr int PREC_BITWISE_AND = 6; // &
constexpr int PREC_EQUALITY = 7;    // == !=
constexpr int PREC_RELATIONAL = 8;  // < > <= >=
constexpr int PREC_SHIFT = 9;       // << >>
constexpr int PREC_TERM = 10;       // + -
constexpr int PREC_FACTOR = 11;     // * / %
constexpr int PREC_UNARY = 12;      // ! - ~ & *
constexpr int PREC_CALL = 13;       // . () []

// `Operator` `expr`
NodeIndex parse_unary(Ast& ast) {
    using enum TokenTag;
    AstTag op;

    switch (ast.tokens[ast.toki].tag) {
    case TOK_MINUS:     op = AST_NEG; break;
    case TOK_TILDE:     op = AST_BIT_NOT; break;
    case TOK_BANG:      op = AST_BOOL_NOT; break;
    case TOK_AMPERSAND: op = AST_ADDRESS_OF; break;
    default:            return primary(ast);
    }
    auto op_token = next_token(ast);
    NodeIndex expr = parse_precedence(ast, PREC_UNARY);
    return add_unary(ast, op, op_token, expr);
}

int get_precedence(TokenTag kind) {
    using enum TokenTag;
    switch (kind) {
    case TOK_EQUAL:
    case TOK_PLUS_EQ:
    case TOK_MINUS_EQ:
    case TOK_ASTERISK_EQ:
    case TOK_SLASH_EQ:
    case TOK_PERCENT_EQ:
    case TOK_PIPE_EQ:
    case TOK_AMPERSAND_EQ:
    case TOK_CARET_EQ:
    case TOK_SHL_EQ:
    case TOK_SHR_EQ:              return PREC_ASSIGN;

    case TOK_PIPE_PIPE:           return PREC_LOGICAL_OR;
    case TOK_AMPERSAND_AMPERSAND: return PREC_LOGICAL_AND;
    case TOK_PIPE:                return PREC_BITWISE_OR;
    case TOK_CARET:               return PREC_BITWISE_XOR;
    case TOK_AMPERSAND:           return PREC_BITWISE_AND;

    case TOK_EQUAL_EQ:
    case TOK_BANG_EQ:             return PREC_EQUALITY;

    case TOK_LESS_THAN:
    case TOK_LESS_EQ:
    case TOK_GREATER_THAN:
    case TOK_GREATER_EQ:          return PREC_RELATIONAL;

    case TOK_SHL:
    case TOK_SHR:                 return PREC_SHIFT;

    case TOK_PLUS:
    case TOK_MINUS:               return PREC_TERM;

    case TOK_ASTERISK:
    case TOK_SLASH:
    case TOK_PERCENT:             return PREC_FACTOR;

    default:                      return PREC_NONE;
    }
}

bool is_right_associative(TokenTag kind) { return get_precedence(kind) == PREC_ASSIGN; }

AstTag token_to_binary_ast_kind(Ast& ast, TokenTag kind) {
    using enum TokenTag;
    switch (kind) {
    case TOK_PLUS:                return AST_ADD;
    case TOK_MINUS:               return AST_SUB;
    case TOK_ASTERISK:            return AST_MUL;
    case TOK_SLASH:               return AST_DIV;
    case TOK_PERCENT:             return AST_MOD;
    case TOK_LESS_THAN:           return AST_LT;
    case TOK_GREATER_THAN:        return AST_GT;
    case TOK_LESS_EQ:             return AST_LT_EQ;
    case TOK_GREATER_EQ:          return AST_GT_EQ;
    case TOK_EQUAL_EQ:            return AST_EQ_EQ;
    case TOK_BANG_EQ:             return AST_NOT_EQ;
    case TOK_AMPERSAND_AMPERSAND: return AST_BOOL_AND;
    case TOK_PIPE_PIPE:           return AST_BOOL_OR;
    case TOK_PIPE:                return AST_BIT_OR;
    case TOK_AMPERSAND:           return AST_BIT_AND;
    case TOK_CARET:               return AST_BIT_XOR;
    case TOK_SHL:                 return AST_SHL;
    case TOK_SHR:                 return AST_SHR;

    case TOK_EQUAL:
    case TOK_PLUS_EQ:
    case TOK_MINUS_EQ:
    case TOK_ASTERISK_EQ:
    case TOK_SLASH_EQ:
    case TOK_PERCENT_EQ:
    case TOK_PIPE_EQ:
    case TOK_AMPERSAND_EQ:
    case TOK_CARET_EQ:
    case TOK_SHL_EQ:
    case TOK_SHR_EQ:              return AST_ASSIGN;

    default:
        fprintf(stderr, "[Parser Error]: token %s is not a binary operator\n", to_str(kind));
        fprintf(stderr, "              | %.*s\n", line_of(ast, ast.toki));
        exit(1);
    }
}

NodeIndex parse_precedence(Ast& ast, int min_prec) {
    assert(min_prec >= 0);
    NodeIndex lhs = parse_unary(ast);

    while (true) {
        TokenTag op = ast.tokens[ast.toki].tag;
        int prec = get_precedence(op);

        if (prec < min_prec || prec == PREC_NONE) {
            break;
        }

        TokenIndex op_token = next_token(ast);

        int next_prec = prec + (is_right_associative(op) ? 0 : 1);

        NodeIndex rhs = parse_precedence(ast, next_prec);

        lhs = add_binary(ast, token_to_binary_ast_kind(ast, op), op_token, lhs, rhs);
    }

    return lhs;
}

TypeIndex declspec(Ast& ast) {
    using enum TokenTag;
    u64 type_id = 0;
    String name;

    // while is typename
    while (true) {
        match_t(ast.current) {
            with TOK_SIGNED : type_id |= TYPE_SIGNED;
            with TOK_UNSIGNED : type_id |= TYPE_UNSIGNED;
            with TOK_VOID : type_id |= TYPE_UNSIGNED;
            with TOK_INT : type_id |= TYPE_INT;
            with TOK_CHAR : type_id |= TYPE_CHAR;
            with TOK_SHORT : type_id |= TYPE_SHORT;
            with TOK_LONG : type_id |= TYPE_LONG;
            with TOK_DOUBLE : type_id |= TYPE_DOUBLE;
            with TOK_FLOAT : type_id |= TYPE_FLOAT;
            with TOK_I8 : type_id |= TYPE_I8;
            with TOK_U8 : type_id |= TYPE_U8;

            with TOK_I16 : type_id |= TYPE_I16;
            with TOK_U16 : type_id |= TYPE_U16;

            with TOK_I32 : type_id |= TYPE_I32;
            with TOK_U32 : type_id |= TYPE_U32;

            with TOK_I64 : type_id |= TYPE_I64;
            with TOK_U64 : type_id |= TYPE_U64;
            with TOK_IDENTIFIER : {
                fprintf(stderr, "[error]: unexpected type_name `%.*s` within a declspec\n", ast.current.buf);
                fprintf(stderr, "         | %.*s\n", line_of(ast, ast.toki));
                goto end;
                // exit(1);
            }
        default: goto end;
        }
        name.append({ast.current.buf, " "});
        next_token(ast);
    }
end:
    auto ty = Type{type_id, .name = name, .none = nullptr};
    return ast.types.append(ty);
}

NodeIndex parseLetVarDecl(Ast& ast) {
    auto let_tok = next_token(ast);
    auto var_name = expect_token(ast, TOK_IDENTIFIER);
    expect_token(ast, TOK_EQUAL);
    auto init_expr = parse_expr(ast);
    return ast.nodes.append(Node{
        .tag = AST_INFERRED_VAR_DECL,
        .main_token = let_tok,
        .var = {.name = ast.tokens[var_name].buf, .type = 0, .init_expr = init_expr},
    });
}

NodeIndex parseConstVarDecl(Ast& ast) {
    auto let_tok = next_token(ast);
    auto var_name = expect_token(ast, TOK_IDENTIFIER);
    expect_token(ast, TOK_EQUAL);
    auto init_expr = parse_expr(ast);
    return ast.nodes.append(Node{
        .tag = AST_CONST_DECL,
        .main_token = let_tok,
        .var = {.name = ast.tokens[var_name].buf, .type = 0, .init_expr = init_expr},
    });
}

NodeIndex parse_var_decl(Ast& ast) {
    using enum TokenTag;
    if (ast.current.tag == TOK_LET)
        return parseLetVarDecl(ast);
    if (ast.match({TOK_CONST, TOK_IDENTIFIER, TOK_EQUAL}))
        return parseConstVarDecl(ast);

    bool is_const = false;
    bool is_static = false;

    if (ast.current.tag == TOK_CONST)
        is_const = true;
    if (ast.current.tag == TOK_STATIC)
        is_static = true;

    auto base_ty = declspec(ast);
    String type_name(ast.types[base_ty].name.ptr);

    while (ast.tokens[ast.toki].tag == TOK_ASTERISK) {
        type_name.append("*");
        base_ty = ast.types.append(Type{
            .id = TYPE_POINTER,
            .name = {type_name.ptr},
            .is_const = is_const,
            .is_static = is_static,
            .ptr = {.base = base_ty, .len = 0},
        });
        next_token(ast);
    }

    auto var_name = expect_token(ast, TOK_IDENTIFIER);

loop:
    while (ast.current.tag == TOK_LBRACKET) {
        next_token(ast);
        type_name.append("[");
        if (ast.current.tag == TOK_RBRACKET) {
            next_token(ast);
            type_name.append("]");
            base_ty = ast.types.append(Type{
                .id = TYPE_ARRAY,
                .name = {type_name.ptr},
                .array = {base_ty, 0},
            });

            goto loop;
        }
        auto len_expr = parse_expr(ast);
        expect_token(ast, TOK_RBRACKET);
        base_ty = ast.types.append(Type{.id = TYPE_ARRAY, .array = {.base = base_ty, .len = len_expr}});
    }

    expect_token(ast, TOK_EQUAL);
    auto init_expr = parse_expr(ast);

    return ast.nodes.append(Node{
        .tag = AST_VAR_DECL,
        .main_token = var_name,
        .type = base_ty,

        .var = {.name = ast.tokens[var_name].buf, .type = base_ty, .init_expr = init_expr},
    });
}
