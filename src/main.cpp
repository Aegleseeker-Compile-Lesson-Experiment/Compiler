#include "lexer.h"

#include <iostream>
#include <iterator>
#include <string>

namespace {
std::string normalize_newlines(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r') {
            if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
            result.push_back('\n');
        } else {
            result.push_back(raw[i]);
        }
    }
    return result;
}

void print_token(const Token& token, std::size_t index) {
    std::cout << index << ':';
    switch (token.kind) {
        case TokenKind::Identifier:
            std::cout << "Ident:\"" << token.lexeme << "\"";
            break;
        case TokenKind::IntConst:
            std::cout << "IntConst:\"" << token.lexeme << '"';
            break;
        case TokenKind::Symbol:
            std::cout << '\'' << token.lexeme << "':\""
                      << token.lexeme << '"';
            break;
    }
    std::cout << '\n';
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2 || std::string(argv[1]) != "-token") {
        std::cerr << "Usage: compiler -token < input.c\n";
        return 2;
    }

    const std::string raw(std::istreambuf_iterator<char>{std::cin},
                          std::istreambuf_iterator<char>{});
    Lexer lexer(normalize_newlines(raw));
    const auto tokens = lexer.tokenize();
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        print_token(tokens[i], i);
    }
    return lexer.ok() ? 0 : 1;
}