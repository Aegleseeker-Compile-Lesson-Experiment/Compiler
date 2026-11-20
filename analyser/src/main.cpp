#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "lexer.h"
#include "token.h"

using namespace std;

// -token：把 Token 流按评测格式打印出来
static int run_token_mode(Lexer& lexer) {
    vector<Token> tokens = lexer.tokenize();
    int index = 0;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) break;
        cout << index++ << ":" << format_token(token) << "\n";
    }
    for (const LexError& error : lexer.errors()) {
        cerr << error.line << ":" << error.column << ": " << error.message << "\n";
    }
    return 0;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);

    if (argc < 2) {
        cerr << "usage: compiler -token | -ast\n";
        return 1;
    }

    string mode = argv[1];
    string source((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
    Lexer lexer(move(source));

    if (mode == "-token") {
        return run_token_mode(lexer);
    }

    if (mode == "-ast") {
        lexer.tokenize();
        // 词法错误也要按 -ast 规范报 reject + 行号
        if (!lexer.errors().empty()) {
            cout << "reject\n";
            for (const LexError& error : lexer.errors()) {
                cout << error.line << "\n";
            }
            return 1;
        }
        // 语法分析还没实现，先明确提示，避免假装 accept
        cerr << "parser not implemented yet\n";
        return 1;
    }

    cerr << "unknown option: " << mode << "\n";
    return 1;
}
