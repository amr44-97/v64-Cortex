#include <cstdio>
#include "c12-lib.h"
#include "lexer.h"
#include "ast.h"

Result<u32> divide(u32 a, u32 b) {
    if (b == 0) {
        return Error("division by zero error");
    }

    return a / b;
}

int main(int argc, char** argv) {
    Arena arena = new_arena();
    defer(arena.deinit());

    if (argc < 2) {
        fprintf(stderr, "usage: %s <FILE_NAME>\n", argv[0]);
        exit(1);
    }

    auto src_file = read_file(argv[1]);
    auto ints = new_dyn(usize);
    defer(ints.destroy());

    auto tokens = new_dyn(Token);
    defer(tokens.destroy());

	Ast ast = {};
	ast.source = src_file.content;
    ast.arena = new_arena(64*1024);
	ast.toki = 0;
	
	auto lex = create_lexer(src_file, ast.tokens);
	//ast.current = ast.tokens[0];
	//NodeIndex n = primary(ast);
	
	for(auto t: ast.tokens) {
		println("{} -> {}",to_str(t.kind),t.buf);
	}


}
