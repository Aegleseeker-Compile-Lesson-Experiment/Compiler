#include "scanner.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

std::string quoted(const std::string& text) {
    std::string output = "\"";
    for (char c : text) {
        if (c == '\\' || c == '"') output += '\\';
        output += c;
    }
    return output + '"';
}

void print(const toy_lex::Result& result) {
    if (!result.errors.empty()) {
        std::cout << "reject\n";
        for (const auto& error : result.errors)
            std::cout << error.at.line - 1 << ' ' << error.at.column << ' '
                      << error.description << '\n';
        std::cout << '\n';
    }
    for (std::size_t i = 0; i < result.tokens.size(); ++i) {
        const auto& token = result.tokens[i];
        std::string label;
        switch (token.category) {
            case toy_lex::Category::name: label = "Ident"; break;
            case toy_lex::Category::integer: label = "IntConst"; break;
            case toy_lex::Category::keyword:
            case toy_lex::Category::punctuation: label = "'" + token.text + "'"; break;
        }
        std::cout << i << ':' << label << ':' << quoted(token.text) << '\n';
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) == "--help")) {
        std::cout << "Usage: complier [-token | source.c]\n"
                     "With no argument or -token, reads standard input.\n";
        return argc > 2 ? 2 : 0;
    }
    std::ifstream file;
    std::istream* input = &std::cin;
    if (argc == 2 && std::string(argv[1]) != "-token") {
        file.open(argv[1], std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open input file: " << argv[1] << '\n';
            return 2;
        }
        input = &file;
    }
    const std::string source(std::istreambuf_iterator<char>{*input},
                             std::istreambuf_iterator<char>{});
    if (!*input && !input->eof()) {
        std::cerr << "Cannot read input\n";
        return 2;
    }
    toy_lex::Scanner scanner(source);
    print(scanner.run());
    return 0;
}
