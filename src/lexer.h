#pragma once
#include "c12-lib.h"

struct Lexer {
    const char* current_file;
    StringRef source;
    DynArray(Token) & tokens;

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
};

Token next_token(Lexer& l);
Lexer create_lexer(File& file, DynArray<Token>& tokens);
