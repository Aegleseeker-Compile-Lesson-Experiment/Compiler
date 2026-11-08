#include "lexer.h"

#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>

namespace {
bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || c == '_';
}
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

const std::unordered_set<std::string> kKeywords = {
    "int", "void", "const", "if", "else", "while",
    "break", "continue", "return"
};
const std::unordered_set<std::string> kTwoCharOperators = {
    "==", "!=", "<=", ">=", "&&", "||"
};
const std::string kSingleCharSymbols = "+-*/%=<>!(){};,";
} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek(std::size_t offset) const {
    if (offset >= source_.size() - pos_) return '\0';
    return source_[pos_ + offset];
}

char Lexer::advance() {
    const char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

void Lexer::report_error(std::size_t line, std::size_t column,
                         const std::string& message) {
    ok_ = false;
    std::cerr << "lexical error at " << line << ':' << column
              << ": " << message << '\n';
}

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < source_.size()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            advance(); advance(); // consume '//'
            while (pos_ < source_.size() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            const std::size_t comment_line = line_;
            const std::size_t comment_col = column_;
            advance(); advance(); // consume '/*'
            bool closed = false;
            while (pos_ < source_.size()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                report_error(comment_line, comment_col, "unterminated block comment");
                return; // pos_ is now at EOF
            }
        } else {
            return;
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skip_whitespace_and_comments();
        if (pos_ == source_.size()) break;

        const std::size_t start = pos_;
        const std::size_t start_line = line_;
        const std::size_t start_col = column_;
        const char c = peek();

        // Identifier or keyword: [_A-Za-z][_A-Za-z0-9]*
        if (is_alpha(c)) {
            do { advance(); } while (is_alnum(peek()));
            std::string word = source_.substr(start, pos_ - start);
            const TokenKind kind = kKeywords.count(word)
                ? TokenKind::Symbol : TokenKind::Identifier;
            tokens.push_back({kind, std::move(word), start_line, start_col});
            continue;
        }

        // Unsigned integer: (0|[1-9][0-9]*)
        if (is_digit(c)) {
            do {
                advance();
            } while (is_digit(peek()));
            std::string digits = source_.substr(start, pos_ - start);

            // Keep lexical conversion safe. 2147483648 is allowed here
            // so that a later unary-minus expression can represent INT_MIN.
            // A positive value of that magnitude needs a semantic check.
            constexpr unsigned long long kMaxMagnitude = 2147483648ULL;
            unsigned long long value = 0;
            bool overflow = false;
            for (const char digit : digits) {
                const unsigned d = static_cast<unsigned>(digit - '0');
                if (value > (kMaxMagnitude - d) / 10) {
                    overflow = true;
                    break;
                }
                value = value * 10 + d;
            }
            if (overflow) {
                report_error(start_line, start_col, "integer literal is out of range");
            } else {
                tokens.push_back({TokenKind::IntConst, std::move(digits),
                                  start_line, start_col});
            }
            continue;
        }

        // Maximal munch: check two-character operators before one-character ones.
        const std::string two = source_.substr(pos_, 2);
        if (kTwoCharOperators.count(two)) {
            advance(); advance();
            tokens.push_back({TokenKind::Symbol, two, start_line, start_col});
            continue;
        }
        if (kSingleCharSymbols.find(c) != std::string::npos) {
            advance();
            tokens.push_back({TokenKind::Symbol, std::string(1, c),
                              start_line, start_col});
            continue;
        }

        // Error recovery: report and skip one character to avoid infinite loops.
        report_error(start_line, start_col, "unexpected character");
        advance();
    }
    return tokens;
}