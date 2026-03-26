#pragma once
#include "cortex.h"

struct Lexer;

Token next_token(Lexer& l);

struct Lexer {
    const char* current_file;
    const char* source;
    usize buflen;
    DynArray<Token>& tokens;

    u32 pos = 0;
    u32 line = 1;
    u32 col = 1;
    u32 line_start = 0;
    u32 lexing_start = 0;

    constexpr u32 advance(int offset = 1) {
        pos += offset;
        col += offset;
        return pos;
    }

    u32 nextChar() {
        pos += 1;
        col += 1;
        return pos;
    }
    static constexpr Lexer create(File file, DynArray<Token>& tokens) {
        Lexer l = Lexer{
            .current_file = file.name,
            .source = file.content,
            .buflen = strlen(file.content),
            .tokens = tokens,
            .pos = 0,
            .line = 1,
            .col = 1,
        };

        while (true) {
            auto tok = next_token(l);

            if (tok.tag == TOK_EOF)
                break;
        }

        return l;
    }
};
