#include "token.h"

using namespace std;

// 只有 Ident 和 IntConst 是特例，关键字/符号都用原文加单引号。
string format_token(const Token& token) {
    string type;
    if (token.kind == TokenKind::Ident) {
        type = "Ident";
    } else if (token.kind == TokenKind::IntConst) {
        type = "IntConst";
    } else {
        type = "'" + token.lexeme + "'";
    }
    return type + ":\"" + token.lexeme + "\"";
}
