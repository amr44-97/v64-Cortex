#include "ast.h"
#include "cortex.h"
#include "lexer.h"


Ast new_ast(File file) {
    Ast ast = {};
    auto lexer = Lexer::create(file, ast.tokens);
    ast.arena = new_arena(64 * 1024);
    ast.toki = 0;
    ast.current = ast.tokens[0];
    ast.file_name = file.name;
    ast.source = file.content;
    return ast;
}

const char* to_str(u64 type_kind) {
    switch (type_kind) {
    case TYPE_VOID:        return "void";
    case TYPE_INT:         return "int";
    case TYPE_SIGNED:      return "signed";
    case TYPE_UNSIGNED:    return "unsigned";
    case TYPE_CHAR:        return "char";
    case TYPE_SHORT:       return "short";
    case TYPE_LONG:        return "long";
    case TYPE_BOOL:        return "bool";
    case TYPE_DOUBLE:      return "double";
    case TYPE_FLOAT:       return "float";
    case TYPE_LONG_LONG:   return "long long";
    case TYPE_LONG_DOUBLE: return "long double";
    case TYPE_SINT:        return "signed int";
    case TYPE_UINT:        return "unsigned int";
    case TYPE_SCHAR:       return "signed char";
    case TYPE_UCHAR:       return "unsigned char";
    case TYPE_SSHORT:      return "signed short";
    case TYPE_USHORT:      return "unsigned short";
    case TYPE_SLONG:       return "signed long";
    case TYPE_ULONG:       return "unsigned long";
    case TYPE_F32:         return "f32";
    case TYPE_F64:         return "f64";

    case TYPE_I8:          return "i8";
    case TYPE_U8:          return "u8";
    case TYPE_I16:         return "i16";
    case TYPE_U16:         return "u16";
    case TYPE_I32:         return "i32";
    case TYPE_U32:         return "u32";
    case TYPE_I64:         return "i64";
    case TYPE_U64:         return "u64";
    case TYPE_BITS8:       return "8bit";
    case TYPE_BITS16:      return "16bit";
    case TYPE_BITS32:      return "32bit";
    case TYPE_BITS64:      return "64bit";
    }
    return "Unknown";
}
