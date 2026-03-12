#include "ast.h"
#include "c12-lib.h"
#include "lexer.h"
#include <filesystem>

StringRef AstKindNames[] = {
    [static_cast<int>(AstKind::Root)] = "Root",
    [static_cast<int>(AstKind::Add)] = "Add",
    [static_cast<int>(AstKind::Sub)] = "Sub",
    [static_cast<int>(AstKind::Mul)] = "Mul",
    [static_cast<int>(AstKind::Div)] = "Div",
    [static_cast<int>(AstKind::Mod)] = "Mod",
    [static_cast<int>(AstKind::Lt)] = "Lt",
    [static_cast<int>(AstKind::Gt)] = "Gt",
    [static_cast<int>(AstKind::LtEq)] = "LtEq",
    [static_cast<int>(AstKind::GtEq)] = "GtEq",
    [static_cast<int>(AstKind::EqEq)] = "EqEq",
    [static_cast<int>(AstKind::NotEq)] = "NotEq",
    [static_cast<int>(AstKind::BoolAnd)] = "BoolAnd",
    [static_cast<int>(AstKind::BoolOr)] = "BoolOr",
    [static_cast<int>(AstKind::BitOr)] = "BitOr",
    [static_cast<int>(AstKind::BitAnd)] = "BitAnd",
    [static_cast<int>(AstKind::BitXor)] = "BitXor",
    [static_cast<int>(AstKind::Shl)] = "Shl",
    [static_cast<int>(AstKind::Shr)] = "Shr",
    [static_cast<int>(AstKind::BoolNot)] = "BoolNot",
    [static_cast<int>(AstKind::Neg)] = "Neg",
    [static_cast<int>(AstKind::BitNot)] = "BitNot",
    [static_cast<int>(AstKind::AddressOf)] = "AddressOf",
    [static_cast<int>(AstKind::Dereference)] = "Dereference",
    [static_cast<int>(AstKind::Identifier)] = "Identifier",
    [static_cast<int>(AstKind::StringLiteral)] = "StringLiteral",
    [static_cast<int>(AstKind::NumberLiteral)] = "NumberLiteral",
    [static_cast<int>(AstKind::BooleanLiteral)] = "BooleanLiteral",
    [static_cast<int>(AstKind::NullLiteral)] = "NullLiteral",
    [static_cast<int>(AstKind::EnumLiteral)] = "EnumLiteral",
    [static_cast<int>(AstKind::GroupedExpr)] = "GroupedExpr",
    [static_cast<int>(AstKind::Call)] = "Call",
    [static_cast<int>(AstKind::Member)] = "Member",
    [static_cast<int>(AstKind::Cast)] = "Cast",
    [static_cast<int>(AstKind::SizeOf)] = "SizeOf",
    [static_cast<int>(AstKind::ArrayAccess)] = "ArrayAccess",
    [static_cast<int>(AstKind::StructInit)] = "StructInit",
    [static_cast<int>(AstKind::ExprStmt)] = "ExprStmt",
    [static_cast<int>(AstKind::Block)] = "Block",
    [static_cast<int>(AstKind::Return)] = "Return",
    [static_cast<int>(AstKind::Defer)] = "Defer",
    [static_cast<int>(AstKind::CallStmt)] = "CallStmt",
    [static_cast<int>(AstKind::Assign)] = "Assign",
    [static_cast<int>(AstKind::TypedVarDecl)] = "TypedVarDecl",
    [static_cast<int>(AstKind::ConstDecl)] = "ConstDecl",
    [static_cast<int>(AstKind::InferredVarDecl)] = "InferredVarDecl",
    [static_cast<int>(AstKind::AutoVarDecl)] = "AutoVarDecl",
    [static_cast<int>(AstKind::IfStmt)] = "IfStmt",
    [static_cast<int>(AstKind::WhileLoop)] = "WhileLoop",
    [static_cast<int>(AstKind::ForLoop)] = "ForLoop",
    [static_cast<int>(AstKind::FuncParam)] = "FuncParam",
    [static_cast<int>(AstKind::StructField)] = "StructField",
    [static_cast<int>(AstKind::FuncDef)] = "FuncDef",
    [static_cast<int>(AstKind::FuncProto)] = "FuncProto",
    [static_cast<int>(AstKind::StructDef)] = "StructDef",
    [static_cast<int>(AstKind::StructDecl)] = "StructDecl",
    [static_cast<int>(AstKind::EnumDef)] = "EnumDef",
    [static_cast<int>(AstKind::UnionDef)] = "UnionDef",
    [static_cast<int>(AstKind::GlobalVarDecl)] = "GlobalVarDecl",
    [static_cast<int>(AstKind::IncludeStmt)] = "IncludeStmt",
    [static_cast<int>(AstKind::MacroDef)] = "MacroDef",
    [static_cast<int>(AstKind::AliasStmt)] = "AliasStmt",
};

StringRef to_str(AstKind kind) { return AstKindNames[static_cast<int>(kind)]; }

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


