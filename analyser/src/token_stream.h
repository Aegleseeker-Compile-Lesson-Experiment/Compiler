#pragma once
#include <cstddef>
#include <utility>
#include <vector>

#include "token.h"

using namespace std;

// 给语法分析准备的小工具：封装 Token 列表和游标，
// parser 里可以直接用 peek / consume / match，不用自己维护下标。
class TokenStream {
public:
    explicit TokenStream(vector<Token> tokens) : tokens_(move(tokens)) {}

    const Token& peek(size_t offset = 0) const {
        size_t index = pos_ + offset;
        if (index >= tokens_.size()) return tokens_.back();  // 末尾一定有 EndOfFile
        return tokens_[index];
    }

    bool check(TokenKind kind) const { return peek().kind == kind; }
    bool at_end() const { return peek().kind == TokenKind::EndOfFile; }

    Token consume() {
        Token token = peek();
        if (!at_end()) ++pos_;
        return token;
    }

    bool match(TokenKind kind) {
        if (!check(kind)) return false;
        consume();
        return true;
    }

    const Token& previous() const { return tokens_[pos_ - 1]; }
    size_t position() const { return pos_; }
    void reset(size_t pos) { pos_ = pos; }

private:
    vector<Token> tokens_;
    size_t pos_ = 0;
};
