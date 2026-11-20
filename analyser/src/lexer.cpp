#include "lexer.h"

#include <utility>

using namespace std;

namespace {

// 把 Windows 的 CRLF 统一成 LF，避免行号统计出错
string normalize_newlines(string source) {
    string result;
    result.reserve(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') continue;
            result.push_back('\n');
        } else {
            result.push_back(source[i]);
        }
    }
    return result;
}

bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// 返回位置 pos 处一个 UTF-8 字符占用的字节数；非法字节序列按 1 字节处理
size_t utf8_length(const string& s, size_t pos) {
    unsigned char lead = static_cast<unsigned char>(s[pos]);
    size_t len = 1;
    if (lead >= 0xC2 && lead <= 0xDF) {
        len = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        len = 3;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        len = 4;
    }
    if (pos + len > s.size()) return 1;
    for (size_t i = 1; i < len; ++i) {
        unsigned char next = static_cast<unsigned char>(s[pos + i]);
        if (next < 0x80 || next > 0xBF) return 1;
    }
    return len;
}

TokenKind keyword_kind(const string& text) {
    if (text == "int")      return TokenKind::KwInt;
    if (text == "void")     return TokenKind::KwVoid;
    if (text == "const")    return TokenKind::KwConst;
    if (text == "if")       return TokenKind::KwIf;
    if (text == "else")     return TokenKind::KwElse;
    if (text == "while")    return TokenKind::KwWhile;
    if (text == "break")    return TokenKind::KwBreak;
    if (text == "continue") return TokenKind::KwContinue;
    if (text == "return")   return TokenKind::KwReturn;
    return TokenKind::Ident;
}

TokenKind two_char_kind(const string& text) {
    if (text == "==") return TokenKind::Eq;
    if (text == "!=") return TokenKind::Ne;
    if (text == "<=") return TokenKind::Le;
    if (text == ">=") return TokenKind::Ge;
    if (text == "&&") return TokenKind::AndAnd;
    if (text == "||") return TokenKind::OrOr;
    return TokenKind::Invalid;
}

TokenKind single_char_kind(char c) {
    switch (c) {
        case '+': return TokenKind::Plus;
        case '-': return TokenKind::Minus;
        case '*': return TokenKind::Star;
        case '/': return TokenKind::Slash;
        case '%': return TokenKind::Percent;
        case '=': return TokenKind::Assign;
        case '<': return TokenKind::Lt;
        case '>': return TokenKind::Gt;
        case '!': return TokenKind::Not;
        case '(': return TokenKind::LParen;
        case ')': return TokenKind::RParen;
        case '{': return TokenKind::LBrace;
        case '}': return TokenKind::RBrace;
        case ';': return TokenKind::Semicolon;
        case ',': return TokenKind::Comma;
        default:  return TokenKind::Invalid;
    }
}

}  // namespace

Lexer::Lexer(string source) : source_(normalize_newlines(move(source))) {}

char Lexer::peek(size_t offset) const {
    size_t index = pos_ + offset;
    return index < source_.size() ? source_[index] : '\0';
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 0;
    } else {
        ++column_;
    }
    return c;
}

void Lexer::skip_trivia() {
    while (pos_ < source_.size()) {
        char c = source_[pos_];

        if (c == ' ' || c == '\t' || c == '\n') {
            advance();
            continue;
        }

        // // 单行注释
        if (c == '/' && peek(1) == '/') {
            while (pos_ < source_.size() && source_[pos_] != '\n') advance();
            continue;
        }

        // /* */ 多行注释
        if (c == '/' && peek(1) == '*') {
            int start_line = line_;
            int start_column = column_;
            advance();  // '/'
            advance();  // '*'
            bool closed = false;
            while (pos_ < source_.size()) {
                if (source_[pos_] == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                errors_.push_back({start_line, start_column, "unterminated comment"});
                return;
            }
            continue;
        }

        break;
    }
}

Token Lexer::next_token() {
    while (true) {
        skip_trivia();

        int start_line = line_;
        int start_column = column_;

        if (pos_ >= source_.size()) {
            return Token{TokenKind::EndOfFile, "", start_line, start_column};
        }

        char c = source_[pos_];

        // 标识符 / 关键字
        if (is_letter(c) || c == '_') {
            size_t start = pos_;
            while (pos_ < source_.size() &&
                   (is_letter(source_[pos_]) || is_digit(source_[pos_]) || source_[pos_] == '_')) {
                advance();
            }
            string text = source_.substr(start, pos_ - start);
            return Token{keyword_kind(text), text, start_line, start_column};
        }

        // 整数：[0-9]+，保留原文（000000 整体一个 token）
        if (is_digit(c)) {
            size_t start = pos_;
            while (pos_ < source_.size() && is_digit(source_[pos_])) advance();
            return Token{TokenKind::IntConst, source_.substr(start, pos_ - start),
                         start_line, start_column};
        }

        // 双字符运算符：最长匹配，必须在单字符之前判断
        if (pos_ + 1 < source_.size()) {
            string two = source_.substr(pos_, 2);
            TokenKind kind = two_char_kind(two);
            if (kind != TokenKind::Invalid) {
                advance();
                advance();
                return Token{kind, two, start_line, start_column};
            }
        }

        // 单字符运算符 / 界符
        TokenKind kind = single_char_kind(c);
        if (kind != TokenKind::Invalid) {
            advance();
            return Token{kind, string(1, c), start_line, start_column};
        }

        // 非法字符：非 ASCII 字符整体按一个字符报告，其余按单字符报告
        size_t length = static_cast<unsigned char>(c) >= 0x80
                            ? utf8_length(source_, pos_)
                            : 1;
        errors_.push_back({start_line, start_column,
                           "illegal character '" + source_.substr(pos_, length) + "'"});
        for (size_t i = 0; i < length; ++i) advance();
    }
}

vector<Token> Lexer::tokenize() {
    vector<Token> tokens;
    while (true) {
        Token token = next_token();
        tokens.push_back(token);
        if (token.kind == TokenKind::EndOfFile) break;
    }
    return tokens;
}
