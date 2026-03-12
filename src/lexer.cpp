#include "lexer.h"
#include "ast.h"
#include "c12-lib.h"
#include <boost/unordered/unordered_flat_map.hpp>
#include <string>

template <typename K, typename V> using Map = boost::unordered::unordered_flat_map<K, V>;

Lexer create_lexer(File& file, DynArray<Token>& tokens) {
    Lexer l = Lexer{
        .current_file = file.name,
        .source = file.content,
        .tokens = tokens,
        .pos = 0,
        .line = 1,
        .col = 1,
    };

    while (true) {
        auto tok = next_token(l);

        if (tok.kind == TokenKind::Eof)
            break;
    }

    return l;
}

void skip_whitespace(Lexer& self) {
    char c = self.source[self.pos];
    while (c == ' ' or c == '\t') {
        self.pos++;
        c = self.source[self.pos];
    }
}

void advance_to_nl(Lexer& self) {
    self.pos += 1;
    self.line_start = self.pos;
    self.line += 1;
}

static Map<StringRef, TokenKind> Keywords = {
    {"include", TokenKind::Keyword_include}, {"return", TokenKind::Keyword_return},
    {"const", TokenKind::Keyword_const},     {"let", TokenKind::Keyword_let},
    {"static", TokenKind::Keyword_static},   {"if", TokenKind::Keyword_if},
    {"else", TokenKind::Keyword_else},       {"for", TokenKind::Keyword_for},
    {"while", TokenKind::Keyword_while},     {"do", TokenKind::Keyword_do},
    {"goto", TokenKind::Keyword_goto},       {"switch", TokenKind::Keyword_switch},
    {"case", TokenKind::Keyword_case},       {"break", TokenKind::Keyword_break},
    {"default", TokenKind::Keyword_default}, {"struct", TokenKind::Keyword_struct},
    {"enum", TokenKind::Keyword_enum},       {"union", TokenKind::Keyword_union},
    {"typedef", TokenKind::Keyword_typedef}, {"sizeof", TokenKind::Keyword_sizeof},
    {"signed", TokenKind::keyword_signed},   {"unsigned", TokenKind::Keyword_unsigned},
    {"int", TokenKind::Keyword_int},         {"bool", TokenKind::Keyword_bool},
    {"char", TokenKind::Keyword_char},       {"short", TokenKind::Keyword_short},
    {"long", TokenKind::Keyword_long},       {"float", TokenKind::Keyword_float},
    {"double", TokenKind::Keyword_double},   {"true", TokenKind::Keyword_true},
    {"false", TokenKind::Keyword_false},     {"u8", TokenKind::Keyword_u8},
    {"i8", TokenKind::Keyword_i8},           {"u16", TokenKind::Keyword_u16},
    {"i16", TokenKind::Keyword_i16},         {"u32", TokenKind::Keyword_u32},
    {"i32", TokenKind::Keyword_i32},         {"u64", TokenKind::Keyword_u64},
    {"i64", TokenKind::Keyword_i64},         {"f32", TokenKind::Keyword_f32},
    {"f64", TokenKind::Keyword_f64},
};


StringRef line_of(Lexer& self, Token* t = nullptr) {
    usize line_end = 0;
    usize buflen = self.source.length();
    for (u32 i = 0; i < buflen; ++i) {
        if (self.source[i] == '\n') {
            line_end = i;
            break;
        } else if (self.source[i] == '\0')
            break;
    }
    u32 line_start = ({
        u32 res = self.line_start;
        if (t != nullptr)
            res = t->loc.line_start;
        res;
    });
    auto line_len = line_end - line_start;
    return self.source.substr(line_start, line_len);
}

Token new_token(Lexer& self, TokenKind kind, StringRef buf, u32 start, u32 end) {
    auto t = Token{kind, buf, Location{start, start - end, self.line, self.line_start}};
    self.tokens.append(t);
    return t;
}

Token identifier(Lexer& self) {
    auto start = self.pos;
    while (true) {
        switch (self.source[self.pos]) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '_':
            self.pos++;
            break;
        default:
            auto buf = self.source.substr(start, self.pos - start);
            auto kind = TokenKind::Identifier;
            if (auto iter = Keywords.find(buf); iter != Keywords.end()) {
                kind = iter->second;
            }
            return new_token(self, kind, buf, start, self.pos);
        }
    }
}

Token number_literal(Lexer& self) {
    auto start = self.pos;
    while (true) {
        switch (self.source[self.pos]) {
        case '0' ... '9':
            self.pos++;
            break;
        default:
            auto buf = self.source.substr(start, self.pos - start);
            auto kind = TokenKind::NumberLiteral;
            if (auto iter = Keywords.find(buf); iter != Keywords.end()) {
                kind = iter->second;
            }
            auto t = new_token(self, kind, buf, start, self.pos);
            t.value.integer = atoi(copy_and_nullterm(buf));
            return t;
        }
    }
}

Token char_literal(Lexer& self) {
    auto start = self.pos++; // skip the single quote of the Char
    while (true) {
        switch (self.source[self.pos]) {
        default:
            self.pos++;
            break;
        case '\'':
            self.pos++;
            auto buf = self.source.substr(start, self.pos - start);
            auto kind = TokenKind::CharLiteral;
            return new_token(self, kind, buf, start, self.pos);
        }
    }
}

Token string_literal(Lexer& self) {
    auto start = self.pos++; // skip the single quote of the Char
    while (true) {
        switch (self.source[self.pos]) {
        default:
            self.pos++;
            break;
        case '\n':
            advance_to_nl(self);
            break;
        case '\0': {
            auto buf = self.source.substr(start, self.pos - start);
            println("[error]: Eof at the end of string -> `{}` ", buf);
            println("        | {}", line_of(self));
            exit(1);
        }
        case '"':
            self.pos++;
            auto buf = self.source.substr(start, self.pos - start);
            auto kind = TokenKind::StringLiteral;
            return new_token(self, kind, buf, start, self.pos);
        }
    }
}
void skip_comment_line(Lexer& self) {
    self.advance(2); // skip the start of the comment `//`
    while (true) {
        [[likely]]
        if (self.source[self.pos] == '\n') {
            advance_to_nl(self);
            return;
        }

        [[unlikely]]
        if (self.source[self.pos] == '\0')
            break;

        self.pos++;
    }
}

Token next_token(Lexer& self) {

    using enum TokenKind;
    self.lexing_start = self.pos;

    switch (self.source[self.pos]) {

    case '\0':
        return new_token(self, Eof, "", self.pos, self.pos);

    case ' ':
    case '\t':
        skip_whitespace(self);
        return next_token(self);

    case '\n':
    case '\r':
        advance_to_nl(self);
        return next_token(self);

    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return identifier(self);

    case '\'':
        return char_literal(self);

    case '"':
        return string_literal(self);
    case '0' ... '9':
        return number_literal(self);

    case '=':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, EqualEq, "==", self.pos, self.advance(2));
        default:
            return new_token(self, Equal, "=", self.pos, self.nextChar());
        }

    case '+':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, PlusEq, "+=", self.pos, self.advance(2));
        case '+':
            return new_token(self, PlusPlus, "++", self.pos, self.advance(2));
        default:
            return new_token(self, Plus, "+", self.pos, self.nextChar());
        }
    case '-':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, MinusEq, "-=", self.pos, self.advance(2));
        case '-':
            return new_token(self, MinusMinus, "--", self.pos, self.advance(2));
        default:
            return new_token(self, Minus, "-", self.pos, self.nextChar());
        }
    case '*':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, AsteriskEq, "*=", self.pos, self.advance(2));
        default:
            return new_token(self, Asterisk, "*", self.pos, self.nextChar());
        }
    case '/':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, SlashEq, "/=", self.pos, self.advance(2));
        case '/':
            skip_comment_line(self);
            return next_token(self);
        default:
            return new_token(self, Slash, "/", self.pos, self.nextChar());
        }

    case '%':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, PercentEq, "%=", self.pos, self.advance(2));
        default:
            return new_token(self, Percent, "%", self.pos, self.nextChar());
        }
    case ':':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, ColonEq, ":=", self.pos, self.advance(2));
        case ':':
            return new_token(self, ColonColon, "::", self.pos, self.advance(2));
        default:
            return new_token(self, Colon, ":", self.pos, self.nextChar());
        }

    case '.':
        switch (self.source[self.pos + 1]) {
        case '.':
            if (self.source[self.pos + 2] == '.')
                return new_token(self, Ellipsis3, "...", self.pos, self.advance(3));
            return new_token(self, Ellipsis2, "..", self.pos, self.advance(2));
        default:
            return new_token(self, Period, ".", self.pos, self.nextChar());
        }

    case '~':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, TildeEq, "~=", self.pos, self.advance(2));
        default:
            return new_token(self, Tilde, "~", self.pos, self.nextChar());
        }

    case '^':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, CaretEq, "^=", self.pos, self.advance(2));
        default:
            return new_token(self, Caret, "^", self.pos, self.nextChar());
        }
    case '&':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, AmpersandEq, "&=", self.pos, self.advance(2));
        case '&':
            return new_token(self, AmpersandAmpersand, "&&", self.pos, self.advance(2));
        default:
            return new_token(self, Ampersand, "&", self.pos, self.nextChar());
        }

    case '<':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, LessOrEq, "<=", self.pos, self.advance(2));
        case '<':
            if (self.source[self.pos + 2] == '=')
                return new_token(self, ShiftLeftEq, "<<=", self.pos, self.advance(3));
            return new_token(self, ShiftLeft, "<<", self.pos, self.advance(2));
        default:
            return new_token(self, LessThan, "<", self.pos, self.nextChar());
        }

    case '>':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, GreaterOrEq, ">=", self.pos, self.advance(2));
        case '>':
            if (self.source[self.pos + 2] == '=')
                return new_token(self, ShiftRightEq, ">>=", self.pos, self.advance(3));
            return new_token(self, ShiftRight, ">>", self.pos, self.advance(2));
        default:
            return new_token(self, GreaterThan, ">", self.pos, self.nextChar());
        }

    case '|':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, PipeEq, "|=", self.pos, self.advance(2));
        case '|':
            return new_token(self, PipePipe, "||", self.pos, self.advance(2));
        default:
            return new_token(self, Pipe, "|", self.pos, self.nextChar());
        }

    case '!':
        switch (self.source[self.pos + 1]) {
        case '=':
            return new_token(self, BangEq, "!=", self.pos, self.advance(2));
        default:
            return new_token(self, Bang, "!", self.pos, self.nextChar());
        }

    case ';':
        return new_token(self, Semicolon, ";", self.pos, self.nextChar());

    case ',':
        return new_token(self, Comma, ",", self.pos, self.nextChar());

    case '$':
        return new_token(self, DollarSign, "$", self.pos, self.nextChar());

    case '@':
        return new_token(self, AtSign, "@", self.pos, self.nextChar());

    case '?':
        return new_token(self, Questionmark, "?", self.pos, self.nextChar());

    case '{':
        return new_token(self, LBrace, "{", self.pos, self.nextChar());

    case '}':
        return new_token(self, RBrace, "}", self.pos, self.nextChar());

    case '[':
        return new_token(self, LBracket, "[", self.pos, self.nextChar());

    case ']':
        return new_token(self, RBracket, "]", self.pos, self.nextChar());

    case '(':
        return new_token(self, LParen, "(", self.pos, self.nextChar());

    case ')':
        return new_token(self, RParen, ")", self.pos, self.nextChar());

    default:
        println("[error]: Unhandled char `{}` at [{}:{}:{}]",self.source[self.pos], self.current_file ,self.line, self.col);
        println("        |{}: {}", self.line, line_of(self));
        exit(1);
    }
}

StringRef TokenKindNames[] = {
    [static_cast<int>(TokenKind::Eof)] = "eof",
    [static_cast<int>(TokenKind::Identifier)] = "identifier",
    [static_cast<int>(TokenKind::TypeNameIdent)] = "type_name",
    [static_cast<int>(TokenKind::NumberLiteral)] = "number_literal",
    [static_cast<int>(TokenKind::StringLiteral)] = "string_literal",
    [static_cast<int>(TokenKind::CharLiteral)] = "char_literal",
    [static_cast<int>(TokenKind::LParen)] = "l_paren",
    [static_cast<int>(TokenKind::RParen)] = "r_paren",
    [static_cast<int>(TokenKind::LBrace)] = "l_brace",
    [static_cast<int>(TokenKind::RBrace)] = "r_brace",
    [static_cast<int>(TokenKind::LBracket)] = "l_bracket",
    [static_cast<int>(TokenKind::RBracket)] = "r_bracket",
    [static_cast<int>(TokenKind::Period)] = "period",
    [static_cast<int>(TokenKind::Ellipsis2)] = "ellipsis2",
    [static_cast<int>(TokenKind::Ellipsis3)] = "ellipsis3",
    [static_cast<int>(TokenKind::Colon)] = "colon",
    [static_cast<int>(TokenKind::ColonEq)] = "colon_equal",
    [static_cast<int>(TokenKind::ColonColon)] = "colon_colon",
    [static_cast<int>(TokenKind::Equal)] = "equal",
    [static_cast<int>(TokenKind::EqualEq)] = "equal_equal",
    [static_cast<int>(TokenKind::Semicolon)] = "semicolon",
    [static_cast<int>(TokenKind::Comma)] = "comma",
    [static_cast<int>(TokenKind::Bang)] = "bang",
    [static_cast<int>(TokenKind::BangEq)] = "bang_equal",
    [static_cast<int>(TokenKind::Questionmark)] = "questionmark",
    [static_cast<int>(TokenKind::DollarSign)] = "dollar_sign",
    [static_cast<int>(TokenKind::AtSign)] = "at_sign",
    [static_cast<int>(TokenKind::Hash)] = "hash",
    [static_cast<int>(TokenKind::Plus)] = "plus",
    [static_cast<int>(TokenKind::PlusPlus)] = "plus_plus",
    [static_cast<int>(TokenKind::PlusEq)] = "plus_equal",
    [static_cast<int>(TokenKind::Minus)] = "minus",
    [static_cast<int>(TokenKind::MinusMinus)] = "minus_minus",
    [static_cast<int>(TokenKind::MinusEq)] = "minus_equal",
    [static_cast<int>(TokenKind::Arrow)] = "arrow",
    [static_cast<int>(TokenKind::Asterisk)] = "asterisk",
    [static_cast<int>(TokenKind::AsteriskEq)] = "asterisk_equal",
    [static_cast<int>(TokenKind::Slash)] = "slash",
    [static_cast<int>(TokenKind::SlashEq)] = "slash_equal",
    [static_cast<int>(TokenKind::Percent)] = "percent",
    [static_cast<int>(TokenKind::PercentEq)] = "percent_equal",
    [static_cast<int>(TokenKind::Pipe)] = "pipe",
    [static_cast<int>(TokenKind::PipeEq)] = "pipe_equal",
    [static_cast<int>(TokenKind::PipePipe)] = "pipe_pipe",
    [static_cast<int>(TokenKind::Ampersand)] = "ampersand",
    [static_cast<int>(TokenKind::AmpersandEq)] = "ampersand_equal",
    [static_cast<int>(TokenKind::AmpersandAmpersand)] = "ampersand_ampersand",
    [static_cast<int>(TokenKind::Caret)] = "caret",
    [static_cast<int>(TokenKind::CaretEq)] = "caret_equal",
    [static_cast<int>(TokenKind::Tilde)] = "tilde",
    [static_cast<int>(TokenKind::TildeEq)] = "tilde_equal",

    [static_cast<int>(TokenKind::LessThan)] = "LessThan",
    [static_cast<int>(TokenKind::LessOrEq)] = "LessOrEqual",
    [static_cast<int>(TokenKind::ShiftLeft)] = "ShiftLeft",
    [static_cast<int>(TokenKind::ShiftLeftEq)] = "ShiftLeftEqual",

    [static_cast<int>(TokenKind::GreaterThan)] = "GreaterThan",
    [static_cast<int>(TokenKind::GreaterOrEq)] = "GreaterOrEqual",
    [static_cast<int>(TokenKind::ShiftRight)] = "ShiftRight",
    [static_cast<int>(TokenKind::ShiftRightEq)] = "ShiftRightEqual",

    [static_cast<int>(TokenKind::Keyword_return)] = "keyword_return",
    [static_cast<int>(TokenKind::Keyword_const)] = "keyword_const",
    [static_cast<int>(TokenKind::Keyword_let)] = "keyword_let",
    [static_cast<int>(TokenKind::Keyword_static)] = "keyword_static",
    [static_cast<int>(TokenKind::Keyword_if)] = "keyword_if",
    [static_cast<int>(TokenKind::Keyword_else)] = "keyword_else",
    [static_cast<int>(TokenKind::Keyword_for)] = "keyword_for",
    [static_cast<int>(TokenKind::Keyword_while)] = "keyword_while",
    [static_cast<int>(TokenKind::Keyword_do)] = "keyword_do",
    [static_cast<int>(TokenKind::Keyword_goto)] = "keyword_goto",
    [static_cast<int>(TokenKind::Keyword_switch)] = "keyword_switch",
    [static_cast<int>(TokenKind::Keyword_case)] = "keyword_case",
    [static_cast<int>(TokenKind::Keyword_break)] = "keyword_break",
    [static_cast<int>(TokenKind::Keyword_default)] = "keyword_default",
    [static_cast<int>(TokenKind::Keyword_struct)] = "keyword_struct",
    [static_cast<int>(TokenKind::Keyword_enum)] = "keyword_enum",
    [static_cast<int>(TokenKind::Keyword_union)] = "keyword_union",
    [static_cast<int>(TokenKind::Keyword_typedef)] = "keyword_typedef",
    [static_cast<int>(TokenKind::Keyword_sizeof)] = "keyword_sizeof",
    [static_cast<int>(TokenKind::keyword_signed)] = "Keyword_signed",
    [static_cast<int>(TokenKind::Keyword_unsigned)] = "keyword_unsigned",
    [static_cast<int>(TokenKind::Keyword_int)] = "keyword_int",
    [static_cast<int>(TokenKind::Keyword_bool)] = "keyword_bool",
    [static_cast<int>(TokenKind::Keyword_char)] = "keyword_char",
    [static_cast<int>(TokenKind::Keyword_short)] = "keyword_short",
    [static_cast<int>(TokenKind::Keyword_long)] = "keyword_long",
    [static_cast<int>(TokenKind::Keyword_float)] = "keyword_float",
    [static_cast<int>(TokenKind::Keyword_double)] = "keyword_double",
    [static_cast<int>(TokenKind::Keyword_true)] = "keyword_true",
    [static_cast<int>(TokenKind::Keyword_false)] = "keyword_false",
    [static_cast<int>(TokenKind::Keyword_u8)] = "keyword_u8",
    [static_cast<int>(TokenKind::Keyword_i8)] = "keyword_i8",
    [static_cast<int>(TokenKind::Keyword_u16)] = "keyword_u16",
    [static_cast<int>(TokenKind::Keyword_i16)] = "keyword_i16",
    [static_cast<int>(TokenKind::Keyword_u32)] = "keyword_u32",
    [static_cast<int>(TokenKind::Keyword_i32)] = "keyword_i32",
    [static_cast<int>(TokenKind::Keyword_u64)] = "keyword_u64",
    [static_cast<int>(TokenKind::Keyword_i64)] = "keyword_i64",
    [static_cast<int>(TokenKind::Keyword_f32)] = "keyword_f32",
    [static_cast<int>(TokenKind::Keyword_f64)] = "keyword_f64",
    [static_cast<int>(TokenKind::Keyword_include)] = "include",
    [static_cast<int>(TokenKind::Keyword_null)] = "null",
    [static_cast<int>(TokenKind::WhiteSpace)] = "<whitespace>",
};

StringRef to_str(TokenKind kind) { return TokenKindNames[static_cast<int>(kind)]; }
