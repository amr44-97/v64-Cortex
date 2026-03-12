#include "ast.h"
#include "c12-lib.h"
#include <cstdio>
#include <cstdlib>
#include <print>

using std::println;

auto next_token(Ast& ast) -> TokenIndex {
    auto i = ast.toki;
    ast.toki += 1;
    ast.set_current_token();
    return i;
}

StringRef line_of(Ast& ast, TokenIndex tok) {
    usize line_end = 0;
    usize buflen = ast.source.length();
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

auto expect_token(Ast& ast, TokenKind _kind) -> TokenIndex {
    if (ast.current.kind == _kind)
        return next_token(ast);

    println("[error]: expected token `{}` but found `{} : {}`", to_str(_kind), to_str(ast.current.kind),
            ast.current.buf);
    println("         {}", line_of(ast, ast.toki));
    exit(1);
}

auto eat_token(Ast& ast, TokenKind kind) -> Optional<TokenIndex> {
    if (ast.current.kind == kind)
        return next_token(ast);
    return {};
}

auto parse_include(Ast& ast) -> NodeIndex {
    using enum TokenKind;
    Optional<TokenIndex> tok = eat_token(ast, Keyword_include);
    if (!tok) {
        println(stderr, "[Parsing error]: expected #include stmt");
        exit(1);
    }

    auto path = expect_token(ast, StringLiteral);
    bool has_wildcard = ast.match_go({Period, Asterisk});

    if (ast.get_buf() == "as") {
        next_token(ast);
        auto alias = expect_token(ast, Identifier);
        Node n = {
            .kind = AstKind::IncludeStmt,
            .main_token = tok,
            .type = 0,
            .include = {path, alias, has_wildcard},
        };
        return ast.nodes.append(n);
    }

    Node n = {
        .kind = AstKind::IncludeStmt,
        .main_token = tok,
        .type = 0,
        .include = {path, 0, has_wildcard},
    };
    return ast.nodes.append(n);
}

NodeIndex add_literal(Ast& ast, AstKind _kind, TokenIndex token, i64 value) {
    return ast.nodes.append({.kind = _kind, .main_token = token, .literal = Literal{.int_value = value}});
}

NodeIndex add_literal(Ast& ast, AstKind _kind, TokenIndex token, double value) {
    return ast.nodes.append({.kind = _kind, .main_token = token, .literal = Literal{.float_value = value}});
}

NodeIndex add_literal(Ast& ast, AstKind _kind, TokenIndex token, StringRef value) {
    return ast.nodes.append({.kind = _kind, .main_token = token, .literal = Literal{.str = value}});
}

NodeIndex add_enum_literal(Ast& ast, AstKind _kind, TokenIndex token, StringRef value) {
    return ast.nodes.append({.kind = _kind, .main_token = token, .literal = Literal{.enum_literal = value}});
}

NodeIndex add_literal(Ast& ast, AstKind _kind, TokenIndex token, bool value) {
    return ast.nodes.append({.kind = _kind, .main_token = token, .literal = Literal{.bool_value = value}});
}

// #define expect_expr(_AST_, _KIND_) \
//     ({ \
//         if (_AST_.current.kind != _KIND_) { \
//             println("error: expected expr {} aftet {}", to_str(_KIND_, _AST_.tokens[_AST_.toki - 1].buf));
//             \
//             println("      {}", line_of(_AST_, _AST_.toki)); \
//             exit(1); \
//         } \
//         next_token(_AST_); \
//     })

auto primary(Ast& ast) -> NodeIndex {
    using enum TokenKind;

    switch (ast.current.kind) {
    case Identifier: {
        if (ast.match({Identifier, LBracket})) {
        }
    }
    case StringLiteral: {
        auto l = add_literal(ast, AstKind::StringLiteral, ast.toki, ast.current.buf);
        next_token(ast);
        return l;
    }
    case NumberLiteral: {
        TokenIndex lit_token = next_token(ast);
        const char* buf = copy_and_nullterm(ast.get_buf());
        i64 parsed_int = atoi(buf);
        return add_literal(ast, AstKind::NumberLiteral, lit_token, parsed_int);
    } break;
    case Keyword_true: {
        auto l = add_literal(ast, AstKind::BooleanLiteral, ast.toki, true);
        next_token(ast);
        return l;
    }
    case Keyword_false: {
        auto l = add_literal(ast, AstKind::BooleanLiteral, ast.toki, false);
        next_token(ast);
        return l;
    }

    case Keyword_null: {
        auto l = add_literal(ast, AstKind::NullLiteral, ast.toki, false);
        next_token(ast);
        return l;
    }

    case Period: {
        if (ast.match({Period, Identifier})) {
            ast.skip_token(Period);
            auto l = add_enum_literal(ast, AstKind::EnumLiteral, ast.toki, ast.current.buf);
            next_token(ast);
            return l;
        }
    }
    case LParen: {
        auto lpar = next_token(ast);
        auto expr = parse_expr(ast);
        expect_token(ast, RParen);
        return ast.nodes.append(Node{.kind = AstKind::GroupedExpr, .main_token = lpar, .grouped = expr});
    }

    default:
        printf("[Parsing Error]: expected primary expression!\n");
        exit(1);

    } // end switch

} // primary()

NodeIndex parse_expr(Ast& ast) { return parse_precedence(ast, 0); }

NodeIndex add_unary(Ast& ast, AstKind kind, TokenIndex main_token, NodeIndex expr) {
    return ast.nodes.append(Node{.kind = kind, .main_token = main_token, .unary = {.expr = expr}});
}

// `Operator` `expr`
NodeIndex parse_unary(Ast& ast) {
    using enum TokenKind;
    AstKind op;

    switch (ast.current.kind) {
		case Minus: op = AstKind::Neg; break;
		case Tilde: op = AstKind::BitNot; break;
		case Bang:  op = AstKind::BoolNot; break;
		case Ampersand: op = AstKind::AddressOf; break;
		default: return primary(ast);
    }
    auto op_token = next_token(ast);
    NodeIndex expr = parse_expr(ast);
    return add_unary(ast, op, op_token, expr);
}

NodeIndex parse_precedence(Ast& ast, int prec) {
    assert(prec >= 0);
    auto lhs = parse_unary(ast);
}
