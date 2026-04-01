#include "ast.h"
#include "cortex.h"

void pp_node(Node* n, String prefix_str = "") {
    if (n == nullptr) return;
    usize bufsize = 512;
    String prefix = prefix_str.ptr;
    defer(prefix.destroy());

    switch (n->tag) {
    case AST_IDENTIFIER:
        printf("%s%s(%.*s)\n", prefix.ptr, to_str(n->tag), FmtStrRef(n->main_token.buf));
        break;
    case AST_NUMBER_LITERAL:
        printf("%s%s(%.*s)\n", prefix.ptr, to_str(n->tag), FmtStrRef(n->main_token.buf));
        break;
    case AST_STRING_LITERAL:
        printf("%s%s(%.*s)\n", prefix.ptr, to_str(n->tag), FmtStrRef(n->main_token.buf));
        break;
    case AST_BOOL_LITERAL:
        printf("%s%s(%.*s)\n", prefix.ptr, to_str(n->tag), FmtStrRef(n->main_token.buf));
        break;
    case AST_ENUM_LITERAL:
        printf("%s%s(.%.*s)\n", prefix.ptr, to_str(n->tag), FmtStrRef(n->main_token.buf));
        break;
    case AST_BLOCK: {
        printf("%s%s\n", prefix.ptr, to_str(n->tag));
        prefix.append("    ");
        for (auto s : n->block.stmts) {
            pp_node(s, prefix);
        }
        break;
    }
    case AST_CALL: {
        printf("%s%s(%.*s)\n", prefix.ptr, to_str(n->tag), FmtStrRef(n->call.callee->main_token.buf));
        prefix.append("    ");
        for (auto s : n->call.args) {
            pp_node(s, prefix);
        }
        break;
    }
    case AST_IF: {
        printf("%s%s\n", prefix.ptr, to_str(n->tag));
        prefix.append("    ");
        pp_node(n->if_stmt.condition, prefix);
        pp_node(n->if_stmt.then_body, prefix);
        pp_node(n->if_stmt.else_body, prefix);
        break;
    }

    case AST_WHILE: {
        printf("%s%s\n", prefix.ptr, to_str(n->tag));
        prefix.append("    ");
        pp_node(n->while_stmt.condition, prefix);
        pp_node(n->while_stmt.body, prefix);
        break;
    }

    case AST_CONST_DECL: {
        printf("%s%s -> %s\n", prefix.ptr, to_str(n->tag), "const");
        prefix.append("    ");
        printf("%sName = %.*s\n", prefix.ptr, FmtStrRef(n->var.name));
        pp_node(n->var.init_expr, prefix);
        break;
    }
    case AST_INFERRED_VAR_DECL: {
        printf("%s%s -> %s\n", prefix.ptr, to_str(n->tag), "let");
        prefix.append("    ");
        printf("%sName = %.*s\n", prefix.ptr, FmtStrRef(n->var.name));
        pp_node(n->var.init_expr, prefix);
        break;
    }
    case AST_VAR_DECL: {
        printf("%s%s -> %s\n", prefix.ptr, to_str(n->tag), n->type->to_str());
        prefix.append("    ");
        printf("%sName = %.*s\n", prefix.ptr, FmtStrRef(n->var.name));
        pp_node(n->var.init_expr, prefix);
        break;
    }

    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_LT:
    case AST_GT:
    case AST_LT_EQ:
    case AST_GT_EQ:
    case AST_EQ_EQ:
    case AST_NOT_EQ:
    case AST_BOOL_AND:
    case AST_BOOL_OR:
    case AST_BIT_OR:
    case AST_BIT_AND:
    case AST_BIT_XOR:
    case AST_SHL:
    case AST_SHR:      {
        printf("%s%s\n", prefix.ptr, to_str(n->tag));
        prefix.append("    ");
        pp_node(n->binary.lhs, prefix);
        pp_node(n->binary.rhs, prefix);
        break;
    }
    default: break;
    }
}
