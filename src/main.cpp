
#include "lexer.h"
#include "parser.h"

#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <algorithm>
#include <vector>

namespace {

std::string normalize_newlines(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r') {
            if (i + 1 < raw.size() &&
                raw[i + 1] == '\n') {
                ++i;
            }
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
            std::cout << "IntConst:" << token.lexeme;
            break;

        case TokenKind::Symbol:
            std::cout << '\'' << token.lexeme << "':\""
                      << token.lexeme << '"';
            break;
    }
    std::cout << '\n';
}

SourceLocation get_eof_location(const std::string& source) {
    SourceLocation loc{1, 1};

    for (char c : source) {
        if (c == '\n') {
            ++loc.line;
            loc.column = 1;
        } else {
            ++loc.column;
        }
    }

    return loc;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: compiler -token|-ast < input.c\n";
        return 2;
    }

    std::string mode = argv[1];

    if (mode != "-token" && mode != "-ast") {
        std::cerr << "Unknown mode\n";
        return 2;
    }

    // 读取源程序
    const std::string raw(
        std::istreambuf_iterator<char>{std::cin},
        std::istreambuf_iterator<char>{}
    );

    std::string source = normalize_newlines(raw);

    // 实验一：词法分析
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (mode == "-token") {
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            print_token(tokens[i], i);
        }
        return lexer.ok() ? 0 : 1;
    }

    // 实验二：语法分析

    if (!lexer.ok()) {
    std::vector<std::size_t> errors =
        lexer.errorLines();

    // 按行号从小到大排序
    std::sort(errors.begin(), errors.end());

    // 同一行可能有多个词法错误，只报告一次
    errors.erase(
        std::unique(errors.begin(), errors.end()),
        errors.end()
    );

    std::cout << "reject\n";

    for (std::size_t line : errors) {
        std::cout << line << '\n';
    }

    return 0;
}


    Parser parser(
        std::move(tokens),
        get_eof_location(source)
    );


    try {
        auto ast = parser.parseProgram();
        (void)ast;
    } catch (const SyntaxError&) {
     // 兜底：错误已经由 Parser 记录
    }

    std::vector<std::size_t> errors = parser.errorLines();

    if (errors.empty()) {
        std::cout << "accept\n";
        return 0;
}

// 按行号排序并去重
std::sort(errors.begin(), errors.end());

errors.erase(
    std::unique(errors.begin(), errors.end()),
    errors.end()
);

std::cout << "reject\n";

for (std::size_t line : errors) {
    std::cout << line << '\n';
}

return 0;

}
