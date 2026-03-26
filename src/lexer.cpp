#include "lexer.h"
#include "cortex.h"
#include <unordered_map>
// assumes __string_ref_v is of type StringRef
#define copy_and_nullterm(__string_ref_v)                                                                    \
    ({                                                                                                       \
        auto buflen = __string_ref_v.size();                                                                 \
        auto __buf = temp_alloc(char, buflen + 1);                                                           \
        memcpy(__buf, __string_ref_v.data(), buflen);                                                        \
        __buf[buflen] = '\0';                                                                                \
        __buf;                                                                                               \
    })

std::unordered_map<StringRef, TokenTag> map = {
    {"return", TOK_RETURN},   {"const", TOK_CONST},     {"let", TOK_LET},
    {"static", TOK_STATIC},   {"if", TOK_IF},           {"else", TOK_ELSE},
    {"for", TOK_FOR},         {"while", TOK_WHILE},     {"do", TOK_DO},
    {"goto", TOK_GOTO},       {"switch", TOK_SWITCH},   {"case", TOK_CASE},
    {"break", TOK_BREAK},     {"default", TOK_DEFAULT}, {"struct", TOK_STRUCT},
    {"enum", TOK_ENUM},       {"union", TOK_UNION},     {"typedef", TOK_TYPEDEF},
    {"sizeof", TOK_SIZEOF},   {"signed", TOK_SIGNED},   {"unsigned", TOK_UNSIGNED},
    {"void", TOK_VOID},       {"int", TOK_INT},         {"bool", TOK_BOOL},
    {"char", TOK_CHAR},       {"short", TOK_SHORT},     {"long", TOK_LONG},
    {"float", TOK_FLOAT},     {"double", TOK_DOUBLE},   {"true", TOK_TRUE},
    {"false", TOK_FALSE},     {"null", TOK_NULL},       {"u8", TOK_U8},
    {"i8", TOK_I8},           {"u16", TOK_U16},         {"i16", TOK_I16},
    {"u32", TOK_U32},         {"i32", TOK_I32},         {"u64", TOK_U64},
    {"i64", TOK_I64},         {"f32", TOK_F32},         {"f64", TOK_F64},
    {"include", TOK_INCLUDE},
};

void skip_whitespace(Lexer& self) {
    char c = self.source[self.pos];
    while (c == ' ' or c == '\t') {
        self.nextChar();
        c = self.source[self.pos];
    }
}

void advance_to_nl(Lexer& self) {
    self.pos += 1;
    self.col = 1;
    self.line_start = self.pos;
    self.line += 1;
}

slice<char> line_of(Lexer& self, Token* t = nullptr) {
    usize line_end = 0;
    usize buflen = self.buflen;

    u32 ti = self.pos;
    while (self.source[ti] != '\0') {
        if (self.source[ti] == '\n') {
            line_end = ti;
            break;
        }
        ti++;
    }

    u32 line_start = ({
        u32 res = self.line_start;
        if (t != nullptr)
            res = t->loc.line_start;
        res;
    });

    auto line_len = line_end - line_start;

    assert(line_start < self.buflen);
    assert(line_len + line_start < self.buflen);

    return slice<char>(&self.source[line_start], line_len);
}

Token new_token(Lexer& self, TokenTag kind, StringRef buf, u32 start, u32 end) {
    auto t = Token{kind, buf, Location{start, start - end, self.line, self.line_start}};
    self.tokens.append(t);
    return t;
}

Token identifier(Lexer& self) {
    auto start = self.pos - 1;

    while (true) {
        switch (self.source[self.pos]) {
        case 'a' ... 'z':
        case '0' ... '9':
        case 'A' ... 'Z':
        case '_':         self.nextChar(); break;
        default:
            auto buf = std::string_view(&self.source[start], self.pos - start);
            auto kind = TOK_IDENTIFIER;
            if (auto iter = map.find(buf); iter != map.end()) {
                kind = iter->second;
            }
            return new_token(self, kind, StringRef(buf.data(), buf.size()), start, self.pos);
        }
    }
}

Token number_literal(Lexer& self) {
    auto start = self.pos;
    while (true) {
        switch (self.source[self.pos]) {
        case '0' ... '9': self.nextChar(); break;
        default:
            StringRef buf = StringRef(&self.source[start], self.pos - start);
            auto kind = TOK_NUMBER_LITERAL;
            auto t = new_token(self, kind, buf, start, self.pos);
            t.value.integer = atoi(copy_and_nullterm(buf));
            return t;
        }
    }
}

Token char_literal(Lexer& self) {
    auto start = self.nextChar() - 1; // skip the single quote of the Char
    while (true) {
        switch (self.source[self.pos]) {
        default: self.nextChar(); break;
        case '\'':
            self.nextChar();
            auto buf = StringRef(&self.source[start], self.pos - start);
            auto kind = TOK_CHAR_LITERAL;
            return new_token(self, kind, buf, start, self.pos);
        }
    }
}

Token string_literal(Lexer& self) {
    auto start = self.nextChar() - 1; // skip the single quote of the Char
    while (true) {
        switch (self.source[self.pos]) {
        default:   self.nextChar(); break;
        case '\n': advance_to_nl(self); break;
        case '\0': {
            auto buf = StringRef(&self.source[start], self.pos - start);
            printf("[error]: Eof at the end of string -> `%*.s` \n", buf);
            printf("        | %*.s\n", line_of(self));
            exit(1);
        }
        case '"':
            self.nextChar();
            auto buf = StringRef(&self.source[start], self.pos - start);
            auto kind = TOK_STRING_LITERAL;
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

        self.nextChar();
    }
}

Token next_token(Lexer& self) {

    using enum TokenTag;
    self.lexing_start = self.pos;

    if (self.pos >= self.buflen) {
        return new_token(self, TOK_EOF, "", self.pos, self.pos);
    }
    switch (self.source[self.pos]) {

    case '\0':        return new_token(self, TOK_EOF, "", self.pos, self.pos);

    case ' ':
    case '\t':        skip_whitespace(self); return next_token(self);

    case '\n':
    case '\r':        advance_to_nl(self); return next_token(self);

    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':         self.nextChar(); return identifier(self);

    case '\'':        return char_literal(self);

    case '"':         return string_literal(self);
    case '0' ... '9': return number_literal(self);

    case '=':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_EQUAL_EQ, "==", self.pos, self.advance(2));
        default:  return new_token(self, TOK_EQUAL, "=", self.pos, self.nextChar());
        }

    case '+':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_PLUS_EQ, "+=", self.pos, self.advance(2));
        case '+': return new_token(self, TOK_PLUS_PLUS, "++", self.pos, self.advance(2));
        default:  return new_token(self, TOK_PLUS, "+", self.pos, self.nextChar());
        }
    case '-':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_MINUS_EQ, "-=", self.pos, self.advance(2));
        case '-': return new_token(self, TOK_MINUS_MINUS, "--", self.pos, self.advance(2));
        default:  return new_token(self, TOK_MINUS, "-", self.pos, self.nextChar());
        }
    case '*':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_ASTERISK_EQ, "*=", self.pos, self.advance(2));
        default:  return new_token(self, TOK_ASTERISK, "*", self.pos, self.nextChar());
        }
    case '/':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_SLASH_EQ, "/=", self.pos, self.advance(2));
        case '/': skip_comment_line(self); return next_token(self);
        default:  return new_token(self, TOK_SLASH, "/", self.pos, self.nextChar());
        }

    case '%':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_PERCENT_EQ, "%=", self.pos, self.advance(2));
        default:  return new_token(self, TOK_PERCENT, "%", self.pos, self.nextChar());
        }
    case ':':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_COLON_EQ, ":=", self.pos, self.advance(2));
        case ':': return new_token(self, TOK_COLON_COLON, "::", self.pos, self.advance(2));
        default:  return new_token(self, TOK_COLON, ":", self.pos, self.nextChar());
        }

    case '.':
        switch (self.source[self.pos + 1]) {
        case '.':
            if (self.source[self.pos + 2] == '.')
                return new_token(self, TOK_ELLIPSIS3, "...", self.pos, self.advance(3));
            return new_token(self, TOK_ELLIPSIS2, "..", self.pos, self.advance(2));
        default: return new_token(self, TOK_PERIOD, ".", self.pos, self.nextChar());
        }

    case '~':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_TILDE_EQ, "~=", self.pos, self.advance(2));
        default:  return new_token(self, TOK_TILDE, "~", self.pos, self.nextChar());
        }

    case '^':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_CARET_EQ, "^=", self.pos, self.advance(2));
        default:  return new_token(self, TOK_CARET, "^", self.pos, self.nextChar());
        }
    case '&':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_AMPERSAND_EQ, "&=", self.pos, self.advance(2));
        case '&': return new_token(self, TOK_AMPERSAND_AMPERSAND, "&&", self.pos, self.advance(2));
        default:  return new_token(self, TOK_AMPERSAND, "&", self.pos, self.nextChar());
        }

    case '<':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_LESS_EQ, "<=", self.pos, self.advance(2));
        case '<':
            if (self.source[self.pos + 2] == '=')
                return new_token(self, TOK_SHL_EQ, "<<=", self.pos, self.advance(3));
            return new_token(self, TOK_SHL, "<<", self.pos, self.advance(2));
        default: return new_token(self, TOK_LESS_THAN, "<", self.pos, self.nextChar());
        }

    case '>':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_GREATER_EQ, ">=", self.pos, self.advance(2));
        case '>':
            if (self.source[self.pos + 2] == '=')
                return new_token(self, TOK_SHR_EQ, ">>=", self.pos, self.advance(3));
            return new_token(self, TOK_SHR, ">>", self.pos, self.advance(2));
        default: return new_token(self, TOK_GREATER_THAN, ">", self.pos, self.nextChar());
        }

    case '|':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_PIPE_EQ, "|=", self.pos, self.advance(2));
        case '|': return new_token(self, TOK_PIPE_PIPE, "||", self.pos, self.advance(2));
        default:  return new_token(self, TOK_PIPE, "|", self.pos, self.nextChar());
        }

    case '!':
        switch (self.source[self.pos + 1]) {
        case '=': return new_token(self, TOK_BANG_EQ, "!=", self.pos, self.advance(2));
        default:  return new_token(self, TOK_BANG, "!", self.pos, self.nextChar());
        }

    case ';': return new_token(self, TOK_SEMICOLON, ";", self.pos, self.nextChar());

    case ',': return new_token(self, TOK_COMMA, ",", self.pos, self.nextChar());

    case '$': return new_token(self, TOK_DOLLARSIGN, "$", self.pos, self.nextChar());

    case '@': return new_token(self, TOK_ATSIGN, "@", self.pos, self.nextChar());

    case '?': return new_token(self, TOK_QUESTIONMARK, "?", self.pos, self.nextChar());

    case '{': return new_token(self, TOK_LBRACE, "{", self.pos, self.nextChar());

    case '}': return new_token(self, TOK_RBRACE, "}", self.pos, self.nextChar());

    case '[': return new_token(self, TOK_LBRACKET, "[", self.pos, self.nextChar());

    case ']': return new_token(self, TOK_RBRACKET, "]", self.pos, self.nextChar());

    case '(': return new_token(self, TOK_LPAREN, "(", self.pos, self.nextChar());

    case ')': return new_token(self, TOK_RPAREN, ")", self.pos, self.nextChar());

    default:
        printf("[error]: Unhandled char `%c` at [%s:%d:%d]\n", self.source[self.pos], self.current_file,
               self.line, self.col);
        printf("        |%d: %*.s\n", self.line, line_of(self));
        printf("%*c^\n", self.col, ' ');
        exit(1);
    }
}
