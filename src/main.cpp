#include "ast.h"
#include "cortex.h"
#include <cstdlib>

Result<u32> divide(u32 a, u32 b) {
    if (b == 0) {
        return Error("division by zero error");
    }

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
    auto src_file = read_file(files[0]);

    int inst[] = {1, 2, 3, 4, 5};
    slice<int> sint = {inst, 5};

    Ast ast = new_ast(src_file);
    defer(ast.deinit());

    auto res = parse_var_decl(ast); // declspec(ast)
    if (ast.nodes[res].tag == AST_VAR_DECL) {
        printf("var_name = %.*s\n", ast.token_of(res).buf);
        auto init_index = ast.nodes[res].var.init_expr;
        auto initexpr = ast.nodes[init_index];
        printf("init expression = %s\n", to_str(initexpr.tag));

        if (initexpr.tag == AST_CALL) {
            for (auto e : initexpr.call.args) {
                printf("arg = %s\n", to_str(ast.nodes[e].tag));
            }
        }
    }

    for (auto n : ast.types) {
        printf("%s -> %s -> %lu\n", to_str(n.id), n.name.ptr, n.id);
    }
}
