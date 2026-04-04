#include "ast.h"
#include "cortex.h"

const char* pp_node(Node* n, String prefix = "");
Result<u32> divide(u32 a, u32 b) {
    if (b == 0) { return Error("division by zero error"); }

    return a / b;
}

void check_args(int minimum, int argc, char** argv) {
    if (argc < minimum) {
        fprintf(stderr, "usage: %s <FILE_NAME>\n", argv[0]);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    // check_args(2, argc, argv);
    const char* example_1 = "test/syntax.c12";
    const char* files[2] = {argv[1], example_1};
    auto file = read_file(files[0]);
    defer(file.deinit());

    Ast ast = new_ast(file);
    defer(ast.deinit());
    // ast.dump_tokens();

    auto res = parse_struct_decl(ast, nullptr); // declspec(ast)
    String prefix = "";
    defer(prefix.destroy());
    pp_node(res, prefix);
    for (auto& s : ast.known_types) {
        printf("type = %s\n", s.ptr);
    }
}
