#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "token.h"

using namespace std;

// 词法错误：记录行号、列号和原因，供 -ast 输出 reject + 行号使用
struct LexError {
    int line;
    int column;
    string message;
};

// 修改：原来 readTokens 边扫描边打印；现在拆成“扫描器”，只负责产出 Token。
class Lexer {
public:
    explicit Lexer(string source);

    Token next_token();              // 取下一个 Token，文件末尾返回 EndOfFile
    vector<Token> tokenize();        // 一次取完，末尾带一个 EndOfFile

    const vector<LexError>& errors() const { return errors_; }

private:
    char peek(size_t offset = 0) const;
    char advance();
    void skip_trivia();              // 跳过空白、// 注释、/* */ 注释

    string source_;
    size_t pos_ = 0;
    int line_ = 0;                   // 0 开始
    int column_ = 0;                 // 0 开始
    vector<LexError> errors_;
};
