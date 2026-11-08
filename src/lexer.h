#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class TokenKind {
    Identifier, // e.g. main, integer
    IntConst,   // e.g. 0, 123
    Symbol      // keywords, operators, delimiters
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();
    bool ok() const { return ok_; }
    const std::vector<std::size_t>& errorLines() const {
    return error_lines_;
}

private:
    std::string source_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    bool ok_ = true;

    char peek(std::size_t offset = 0) const;
    char advance();
    void skip_whitespace_and_comments();
    void report_error(std::size_t line, std::size_t column,
                      const std::string& message);
    std::vector<std::size_t> error_lines_;
};