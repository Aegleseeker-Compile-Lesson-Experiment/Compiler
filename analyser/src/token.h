#pragma once
#include <string>

using namespace std;

// 修改：把原来 7 个大类细分成每个关键字/运算符/界符一个值，
// 这样语法分析可以直接用 kind 判断，不需要再比较字符串。
enum class TokenKind {
    // 关键字
    KwInt, KwVoid, KwConst, KwIf, KwElse, KwWhile, KwBreak, KwContinue, KwReturn,
    // 普通 Token
    Ident, IntConst,
    // 算术运算符
    Plus, Minus, Star, Slash, Percent,
    // 赋值与关系
    Assign, Eq, Ne, Lt, Gt, Le, Ge,
    // 逻辑
    AndAnd, OrOr, Not,
    // 界符
    LParen, RParen, LBrace, RBrace, Semicolon, Comma,
    // 特殊
    EndOfFile, Invalid
};

struct Token {
    TokenKind kind;
    string lexeme;        // 修改：str 改名为 lexeme，语义更清楚
    int line;             // 行号从 0 开始，和 -ast 的输出规范一致
    int column;           // 列号从 0 开始
};

// 把一个 Token 格式化成 -token 需要的一行，例如 0:'int':"int"
string format_token(const Token& token);
