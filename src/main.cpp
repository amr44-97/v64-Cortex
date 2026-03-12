#include "ast.h"
#include "c12-lib.h"

Result<u32> divide(u32 a, u32 b) {
    if (b == 0) {
        return Error("division by zero error");
    }

    return a / b;
}

int main(int argc, char** argv) {

    if (argc < 2) {
        fprintf(stderr, "usage: %s <FILE_NAME>\n", argv[0]);
        exit(1);
    }

    auto src_file = read_file(argv[1]); // create_file("unnamed", "1 + 2");

    Ast ast = new_ast(src_file);
    defer(ast.deinit());

    println(stdout, "Parsing {}", ast.file_name);
    auto res = parse_precedence(ast, 0);

    for (auto n : ast.nodes) {
        println(stdout, "{}: {}", to_str(n.kind), ast.tokens[n.main_token].buf);
    }
}
