#include "ast.h"
#include "c12-lib.h"
#include <cassert>
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

NodeIndex add_unary(Ast& ast, AstKind kind, TokenIndex main_token, NodeIndex expr) {
    return ast.nodes.append(Node{.kind = kind, .main_token = main_token, .unary = {.expr = expr}});
}

NodeIndex add_binary(Ast& ast, AstKind kind, TokenIndex main_token, NodeIndex lhs, NodeIndex rhs) {
    return ast.nodes.append(Node{.kind = kind, .main_token = main_token, .binary = {.lhs = lhs, .rhs = rhs}});
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

    switch (ast.tokens[ast.toki].kind) {
    case Identifier: {
        if (ast.match({Identifier, LBracket})) {
        }
    }
    case StringLiteral: {
        auto l = add_literal(ast, AstKind::StringLiteral, ast.toki, ast.tokens[ast.toki].buf);
        next_token(ast);
        return l;
    }
    case NumberLiteral: {
		auto t = next_token(ast);
        return add_literal(ast, AstKind::NumberLiteral, t, ast.tokens[t].value.integer);
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
        println("[Parsing Error]: expected primary expression but found `{}:{}`", to_str(ast.current.kind),ast.current.buf);
		println("               | {}",line_of(ast, ast.toki));
        exit(1);

    } // end switch

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
    using enum TokenKind;
    AstKind op;

    switch (ast.tokens[ast.toki].kind) {
    case Minus:
        op = AstKind::Neg;
        break;
    case Tilde:
        op = AstKind::BitNot;
        break;
    case Bang:
        op = AstKind::BoolNot;
        break;
    case Ampersand:
        op = AstKind::AddressOf;
        break;
    default:
        return primary(ast);
    }
    auto op_token = next_token(ast);
    NodeIndex expr = parse_precedence(ast, PREC_UNARY);
    return add_unary(ast, op, op_token, expr);
}

int get_precedence(TokenKind kind) {
    using enum TokenKind;
    switch (kind) {
    case Equal:
    case PlusEq:
    case MinusEq:
    case AsteriskEq:
    case SlashEq:
    case PercentEq:
    case PipeEq:
    case AmpersandEq:
    case CaretEq:
    case ShiftLeftEq:
    case ShiftRightEq:
        return PREC_ASSIGN;

    case PipePipe:
        return PREC_LOGICAL_OR;
    case AmpersandAmpersand:
        return PREC_LOGICAL_AND;
    case Pipe:
        return PREC_BITWISE_OR;
    case Caret:
        return PREC_BITWISE_XOR;
    case Ampersand:
        return PREC_BITWISE_AND;

    case EqualEq:
    case BangEq:
        return PREC_EQUALITY;

    case LessThan:
    case LessOrEq:
    case GreaterThan:
    case GreaterOrEq:
        return PREC_RELATIONAL;

    case ShiftLeft:
    case ShiftRight:
        return PREC_SHIFT;

    case Plus:
    case Minus:
        return PREC_TERM;

    case Asterisk:
    case Slash:
    case Percent:
        return PREC_FACTOR;

    default:
        return PREC_NONE;
    }
}

bool is_right_associative(TokenKind kind) {
    return get_precedence(kind) == PREC_ASSIGN;
}

AstKind token_to_binary_ast_kind(Ast& ast, TokenKind kind) {
    using enum TokenKind;
    switch (kind) {
    case Plus:
        return AstKind::Add;
    case Minus:
        return AstKind::Sub;
    case Asterisk:
        return AstKind::Mul;
    case Slash:
        return AstKind::Div;
    case Percent:
        return AstKind::Mod;
    case LessThan:
        return AstKind::Lt;
    case GreaterThan:
        return AstKind::Gt;
    case LessOrEq:
        return AstKind::LtEq;
    case GreaterOrEq:
        return AstKind::GtEq;
    case EqualEq:
        return AstKind::EqEq;
    case BangEq:
        return AstKind::NotEq;
    case AmpersandAmpersand:
        return AstKind::BoolAnd;
    case PipePipe:
        return AstKind::BoolOr;
    case Pipe:
        return AstKind::BitOr;
    case Ampersand:
        return AstKind::BitAnd;
    case Caret:
        return AstKind::BitXor;
    case ShiftLeft:
        return AstKind::Shl;
    case ShiftRight:
        return AstKind::Shr;

    case Equal:
    case PlusEq:
    case MinusEq:
    case AsteriskEq:
    case SlashEq:
    case PercentEq:
    case PipeEq:
    case AmpersandEq:
    case CaretEq:
    case ShiftLeftEq:
    case ShiftRightEq:
        return AstKind::Assign;

    default:
        println(stderr, "[Parser Error]: token {} is not a binary operator", to_str(kind));
        println(stderr, "              | {}", line_of(ast, ast.toki));
        exit(1);
    }
}


NodeIndex parse_precedence(Ast& ast, int min_prec) {
    assert(min_prec >= 0);
    NodeIndex lhs = parse_unary(ast);

    while (true) {
        TokenKind op = ast.tokens[ast.toki].kind;
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
