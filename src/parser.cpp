#include "ast.h"
#include "cortex.h"
#include <cstdio>
#include <cstring>

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

auto next_token(Ast& ast) -> Token& {
    auto i = ast.toki;
    ast.toki += 1;
    ast.set_current_token();
    return ast.tokens[i];
}

const char* line_of(Ast& ast, Token& tok) {
    usize buflen = ast.source.size();
    auto line_len = 0;
    auto line_start = tok.loc.line_start;
    for (u32 i = line_start; i < buflen; ++i) {
        if (ast.source[i] == '\n' or ast.source[i] == '\0') {
            line_len = i - line_start;
            break;
        }
    }
    auto cstr = (char*)calloc(line_len + 1, 1);
    memcpy(cstr, &ast.source[line_start], line_len);
    cstr[line_len] = '\0';
    return cstr;
}

#define expect_token(ast, _kind)                                                                             \
    ({                                                                                                       \
        if (ast.current.tag != _kind) {                                                                      \
            printf("[error:]: expected token `%s` but found `%s : %.*s`\n", to_str(_kind),                   \
                   to_str(ast.current.tag), FmtStrRef(ast.current.buf));                                     \
            printf("        | %s:%d `%s`\n", __FILE_NAME__, __LINE__, __PRETTY_FUNCTION__);                  \
            printf("        |%d: %s\n", ast.current.loc.line, line_of(ast, ast.current));                    \
            exit(1);                                                                                         \
        }                                                                                                    \
        next_token(ast);                                                                                     \
    })

auto eat_token(Ast& ast, TokenTag kind) -> Option<Token> {
    if (ast.current.tag == kind) return next_token(ast);
    return nullptr;
}

auto parse_include(Ast& ast) -> Node* {
    using enum TokenTag;
    Token tok = eat_token(ast, TOK_INCLUDE).unwrap();
    auto n = ast.new_node();
    n->tag = AST_INCLUDE_STMT;
    n->main_token = tok;

    auto path = expect_token(ast, TOK_STRING_LITERAL);
    bool has_wildcard = ast.match_go({TOK_PERIOD, TOK_ASTERISK});

    if (ast.get_buf() == "as") {
        next_token(ast);
        auto alias = expect_token(ast, TOK_IDENTIFIER);

        n->include = {&path, &alias, has_wildcard};

        return n;
    }

    n->include = {&path, nullptr, has_wildcard};

    return n;
}

Node* new_literal(Ast& ast, AstTag _tag) {
    auto res = ast.new_node(Node{.tag = _tag, .main_token = ast.tokens[ast.toki]});
    next_token(ast);
    return res;
}

Node* new_unary(Ast& ast, AstTag tag, Token main_token, Node* expr) {
    return ast.new_node(Node{.tag = tag, .main_token = main_token, .unary = {.expr = expr}});
}

Node* new_binary(Ast& ast, AstTag tag, Token main_token, Node* lhs, Node* rhs) {
    return ast.new_node(Node{.tag = tag, .main_token = main_token, .binary = {.lhs = lhs, .rhs = rhs}});
}

#define expect_expr_tag(AST, TAG)                                                                            \
    ({                                                                                                       \
        auto start_tok = (AST).toki;                                                                         \
        auto expr = parse_expr((AST));                                                                       \
        if ((AST).nodes[expr].tag != (TAG) || expr == nullptr) {                                             \
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
        if (expr == nullptr) {                                                                               \
            auto token_buf = expr->main_token.buf;                                                           \
            printf("[error]: expected expr after %.*s but found `%s : %.*s`\n",                              \
                   FmtStrRef(ast.tokens[start_tok].buf), to_str(expr->tag), FmtStrRef(token_buf));           \
            auto line_of_error = line_of(ast, expr->main_token);                                             \
            printf("        | %s\n", line_of_error);                                                         \
            exit(1);                                                                                         \
        }                                                                                                    \
        expr;                                                                                                \
    })

#define expect_primary(ast)                                                                                  \
    ({                                                                                                       \
        Node* __primary_expr = primary(ast);                                                                 \
        if (!__primary_expr) {                                                                               \
            printf("[error]: expected identifier after `%.*s.` but found `%s` `%.*s`\n",                     \
                   FmtStrRef(lhs->main_token.buf), to_str(ast.current.tag), FmtStrRef(ast.current.buf));     \
            auto line = line_of(ast, ast.current);                                                           \
            printf("         | %s\n", line);                                                                 \
            nullptr;                                                                                         \
        }                                                                                                    \
        __primary_expr;                                                                                      \
    })

auto primary(Ast& ast) -> Node* {
    match_t(ast.tokens[ast.toki]) {
        with TOK_IDENTIFIER : {
            auto lhs = new_literal(ast, AST_IDENTIFIER);
            for (;;) {
                if (ast.current.tag == TOK_PERIOD) {
                    auto dot = next_token(ast);     // skip the period
                    auto rhs = expect_primary(ast); // expected identifier
                    if (!rhs) { return nullptr; }
                    lhs = ast.new_node({.tag = AST_MEMBER, .main_token = dot, .binary = {lhs, rhs}});
                    continue;
                }

                if (ast.match({TOK_LPAREN, TOK_RPAREN})) {
                    ast.advance(2);
                    lhs = ast.new_node(Node{
                        .tag = AST_CALL,
                        .main_token = lhs->main_token, // the identifier before the LPAREN
                        .call = Call{.callee = lhs, .args = {}},
                    });

                    continue;
                }

                if (ast.current.tag == TOK_LPAREN) {
                    DynArray<Node*> args{&ast.arena};
                    next_token(ast); // skip `(`
                    while (true) {
                        auto expr = parse_expr(ast);
                        args.append(expr);
                        if (ast.current.tag == TOK_COMMA) {
                            next_token(ast);
                            continue;
                        }
                        if (ast.current.tag == TOK_RPAREN) {
                            // next_token(ast);
                            break;
                        }
                    }
                    expect_token(ast, TOK_RPAREN);
                    lhs = ast.new_node(Node{
                        .tag = AST_CALL,
                        .main_token =
                            lhs->main_token, // the identifier before the LPAREN
                                             // .type = ast.get_typeof(ast.get_typeof(lhs).array.element)
                                             // .type = scope_get_type(ast,lhs).array.base
                        .call = Call{.callee = lhs, .args = args.move()},
                    });
                    continue;
                }

                if (ast.current.tag == TOK_LBRACKET) {
                    next_token(ast);
                    auto expr = expect_expr(ast);
                    expect_token(ast, TOK_RBRACKET);
                    lhs = ast.new_node(Node{
                        .tag = AST_ARRAY_ACCESS,
                        .main_token = lhs->main_token,
                        // .type = ast.get_typeof(ast.get_typeof(lhs).array.element)
                        // .type = scope_get_type(ast,lhs).array.base
                        .array_access = {.ident = lhs, .expr = expr},
                    });
                    continue;
                }
                return lhs;
            }
        }
        with TOK_STRING_LITERAL : {
            auto l = new_literal(ast, AST_STRING_LITERAL);
            return l;
        }
        with TOK_NUMBER_LITERAL : { return new_literal(ast, AST_NUMBER_LITERAL); }
        with TOK_TRUE : {
            auto l = new_literal(ast, AST_BOOL_LITERAL);
            return l;
        }
        with TOK_FALSE : {
            auto l = new_literal(ast, AST_BOOL_LITERAL);
            return l;
        }

        with TOK_NULL : {
            auto l = new_literal(ast, AST_NULL_LITERAL);
            return l;
        }

        with TOK_PERIOD : {
            if (ast.match({TOK_PERIOD, TOK_IDENTIFIER})) {
                ast.skip_token(TOK_PERIOD);
                auto l = new_literal(ast, AST_ENUM_LITERAL);
                return l;
            }
        }
        with TOK_LPAREN : {
            auto lpar = next_token(ast);
            auto expr = parse_expr(ast);
            expect_token(ast, TOK_RPAREN);
            return ast.new_node(Node{.tag = AST_GROUPED_EXPR, .main_token = lpar, .grouped = expr});
        }

    default:
        return nullptr;
        // printf("[error]: expected primary expression but found `%s` `%.*s`\n", to_str(ast.current.tag),
        //        ast.current.loc.len, ast.current.buf.data());
        // auto line = line_of(ast, ast.toki);
        // printf("         | %.*s\n", (int)line.size(), line.data());
        // exit(1);

    } // end switch

    return nullptr;
} // primary()

Node* parse_expr(Ast& ast) { return parse_precedence(ast, 0); }

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
constexpr int PREC_CALL = 13;       // . () [] ++ --

// `Operator` `expr`
Node* parse_unary(Ast& ast) {
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
    Node* expr = parse_precedence(ast, PREC_UNARY);
    return new_unary(ast, op, op_token, expr);
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

    // case TOK_PERIOD:
    // case TOK_LBRACKET:
    // case TOK_LPAREN:
    // case TOK_PLUS_PLUS:
    // case TOK_MINUS_MINUS:         return PREC_CALL;
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
        fprintf(stderr, "              | %s\n", line_of(ast, ast.current));
        exit(1);
    }
}

Node* parse_precedence(Ast& ast, int min_prec) {
    assert(min_prec >= 0);
    Node* lhs = parse_unary(ast);

    while (true) {
        TokenTag op = ast.tokens[ast.toki].tag;
        int prec = get_precedence(op);

        if (prec < min_prec || prec == PREC_NONE) { break; }

        Token& op_token = next_token(ast);

        int next_prec = prec + (is_right_associative(op) ? 0 : 1);

        Node* rhs = parse_precedence(ast, next_prec);

        lhs = new_binary(ast, token_to_binary_ast_kind(ast, op), op_token, lhs, rhs);
    }

    return lhs;
}

Type* declspec(Ast& ast) {
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
                fprintf(stderr, "[error]: unexpected type_name `%.*s` within a declspec\n",
                        FmtStrRef(ast.current.buf));
                fprintf(stderr, "         | %s\n", line_of(ast, ast.tokens[ast.toki]));
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
    return ast.new_type(ty);
}

Node* parse_inferred_var_decl(Ast& ast) {
    auto let_tok = next_token(ast);
    auto var_name = expect_token(ast, TOK_IDENTIFIER);
    expect_token(ast, TOK_EQUAL);
    auto init_expr = parse_expr(ast);
    return ast.new_node({
        .tag = AST_INFERRED_VAR_DECL,
        .main_token = let_tok,
        .var = {.name = var_name.buf, .type = 0, .init_expr = init_expr},
    });
}

Node* parse_const_var_decl(Ast& ast) {
    auto let_tok = next_token(ast);
    auto var_name = expect_token(ast, TOK_IDENTIFIER);
    expect_token(ast, TOK_EQUAL);
    auto init_expr = parse_expr(ast);
    return ast.new_node({
        .tag = AST_CONST_DECL,
        .main_token = let_tok,
        .var = {.name = var_name.buf, .type = 0, .init_expr = init_expr},
    });
}

Node* parse_var_decl(Ast& ast) {
    using enum TokenTag;
    if (ast.current.tag == TOK_LET) return parse_inferred_var_decl(ast);
    if (ast.match({TOK_CONST, TOK_IDENTIFIER, TOK_EQUAL})) return parse_const_var_decl(ast);

    bool is_const = false;
    bool is_static = false;

    if (ast.current.tag == TOK_CONST) {
        is_const = true;
        next_token(ast);
    }
    if (ast.current.tag == TOK_STATIC) {
        is_static = true;
        next_token(ast);
    }

    auto base_ty = declspec(ast);
    String type_name = base_ty->name.copy();

    while (ast.tokens[ast.toki].tag == TOK_ASTERISK) {
        type_name.append("*");
        base_ty = ast.new_type(Type{
            .id = TYPE_POINTER,
            .name = {type_name.ptr},
            .is_const = is_const,
            .is_static = is_static,
            .ptr = {.base = base_ty, .len = 0},
        });
        next_token(ast);
    }

    auto var_name = expect_token(ast, TOK_IDENTIFIER);

    while (ast.current.tag == TOK_LBRACKET) {
        next_token(ast);
        type_name.append("[");
        if (ast.current.tag == TOK_RBRACKET) {
            next_token(ast);
            type_name.append("]");
            base_ty = ast.new_type(Type{
                .id = TYPE_ARRAY,
                .name = {type_name.ptr},
                .array = {base_ty, 0},
            });

            continue;
        }
        auto len_expr = parse_expr(ast);
        expect_token(ast, TOK_RBRACKET);
        base_ty = ast.new_type(Type{.id = TYPE_ARRAY, .array = {.base = base_ty, .len = len_expr}});
    }

    expect_token(ast, TOK_EQUAL);
    auto init_expr = parse_expr(ast);

    return ast.new_node(Node{
        .tag = AST_VAR_DECL,
        .main_token = var_name,
        .type = base_ty,

        .var = {.name = var_name.buf, .type = base_ty, .init_expr = init_expr},
    });
}

Node* parse_if(Ast& ast) {
    auto kw = eat_token(ast, TOK_IF).unwrap();

    auto node = ast.new_node(Node{
        .tag = AST_IF,
        .main_token = kw,
        .if_stmt = {},
    });

    expect_token(ast, TOK_LPAREN);
    node->if_stmt.condition = parse_expr(ast);
    expect_token(ast, TOK_RPAREN);
    node->if_stmt.then_body = parse_stmt(ast);
    node->if_stmt.else_body = nullptr;
    if (ast.current.tag == TOK_ELSE) {
        next_token(ast);
        node->if_stmt.else_body = parse_stmt(ast);
    }

    return node;
}

Node* parse_for(Ast& ast) { return nullptr; }
Node* parse_while(Ast& ast) {
    auto kw = eat_token(ast, TOK_WHILE).unwrap();

    auto node = ast.new_node(Node{
        .tag = AST_WHILE,
        .main_token = kw,
        .while_stmt = {},
    });

    expect_token(ast, TOK_LPAREN);
    node->while_stmt.condition = parse_expr(ast);
    expect_token(ast, TOK_RPAREN);
    node->while_stmt.body = parse_stmt(ast);
    node->while_stmt.label = nullptr;

    return node;
}

Node* parse_switch(Ast& ast) { return nullptr; }

Node* parse_block(Ast& ast) {
    auto begin_block = expect_token(ast, TOK_LBRACE);
    DynArray<Node*> stmts{&ast.arena};
    while (ast.tokens[ast.toki].tag != TOK_RBRACE) {
        auto statement = parse_stmt(ast);
        stmts.append(statement);
    }

    auto end_block = next_token(ast);
    return ast.new_node({
        .tag = AST_BLOCK,
        .main_token = begin_block,
        .block = Block{.stmts = stmts.move()},
    });
}

Node* parse_stmt(Ast& ast) {
    switch (ast.current.tag) {
    case TOK_IF:       return parse_if(ast);
    case TOK_FOR:      return parse_for(ast);
    case TOK_WHILE:    return parse_while(ast);
    case TOK_SWITCH:   return parse_switch(ast);
    case TOK_LBRACE:   return parse_block(ast);
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
    case TOK_STATIC:
    case TOK_LET:
    case TOK_CONST:    {
        auto res = parse_var_decl(ast);
        // printf("`%.*s` + `%.*s`", ast.tokens[ast.toki].buf, ast.tokens[ast.toki + 1].buf);
        expect_token(ast, TOK_SEMICOLON);
        return res;
    }
    case TOK_RETURN: {
        auto e = next_token(ast);
        Node* res = ast.new_node(Node{
            .tag = AST_RETURN,
            .main_token = e,
            .ret = {.expr = parse_expr(ast)},
        });
        expect_token(ast, TOK_SEMICOLON);
        return res;
    }
    case TOK_ELSE: {
        printf("[error]: expected expr but found keyword_else \n");
        auto line_of_error = line_of(ast, ast.current);
        printf("        | %s\n", line_of_error);
        exit(1);
    };
    default: {
        auto res = parse_expr(ast);
        expect_token(ast, TOK_SEMICOLON);
        return res;
    }
    }
}
