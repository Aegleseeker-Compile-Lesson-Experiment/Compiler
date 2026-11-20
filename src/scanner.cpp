#include "scanner.h"

#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace toy_lex {
namespace {

bool letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool digit(char c) { return c >= '0' && c <= '9'; }

bool word_part(char c) { return letter(c) || digit(c); }

bool keyword(std::string_view word) {
    constexpr std::string_view words[] = {
        "break", "const", "continue", "else", "if", "int", "return", "void", "while"
    };
    for (auto candidate : words) {
        if (candidate == word) return true;
    }
    return false;
}

bool pair_operator(std::string_view text) {
    constexpr std::string_view pairs[] = {"&&", "||", "==", "!=", "<=", ">="};
    for (auto candidate : pairs) {
        if (candidate == text) return true;
    }
    return false;
}

} // namespace

Scanner::Scanner(std::string input) : input_(std::move(input)) {}

Result Scanner::run() {
    while (!done()) {
        if (trivia()) continue;
        const Position start = location_;
        const std::size_t first = offset_;
        const char c = current();

        if (letter(c)) {
            do { step(); } while (word_part(current()));
            auto spelling = slice(first);
            add(keyword(spelling) ? Category::keyword : Category::name,
                std::move(spelling), start);
        } else if (digit(c)) {
            unsigned long long magnitude = 0;
            bool too_large = false;
            do {
                const auto value = static_cast<unsigned>(current() - '0');
                if (!too_large) {
                    if (magnitude > (2147483648ULL - value) / 10ULL)
                        too_large = true;
                    else
                        magnitude = magnitude * 10ULL + value;
                }
                step();
            } while (digit(current()));
            if (too_large) fail(start, "integer literal exceeds 2147483648");
            else add(Category::integer, slice(first), start);
        } else if (pair_operator(std::string_view(input_).substr(offset_, 2))) {
            step(); step();
            add(Category::punctuation, slice(first), start);
        } else if (std::string_view("+-*/%=<>!(){};,").find(c) != std::string_view::npos) {
            step();
            add(Category::punctuation, slice(first), start);
        } else {
            const auto byte = static_cast<unsigned char>(c);
            std::ostringstream message;
            message << "unexpected byte 0x" << std::uppercase << std::hex
                    << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
            fail(start, message.str());
            step();
        }
    }
    return std::move(result_);
}

bool Scanner::done() const { return offset_ >= input_.size(); }

char Scanner::current(std::size_t lookahead) const {
    return lookahead < input_.size() - offset_ ? input_[offset_ + lookahead] : '\0';
}

void Scanner::step() {
    if (done()) return;
    const char c = input_[offset_++];
    if (c == '\r') {
        if (current() == '\n') ++offset_;
        ++location_.line;
        location_.column = 1;
    } else if (c == '\n') {
        ++location_.line;
        location_.column = 1;
    } else {
        ++location_.column;
    }
}

std::string Scanner::slice(std::size_t from) const { return input_.substr(from, offset_ - from); }

void Scanner::add(Category type, std::string text, Position at) {
    result_.tokens.push_back({type, std::move(text), at});
}

void Scanner::fail(Position at, std::string description) {
    result_.errors.push_back({at, std::move(description)});
}

bool Scanner::trivia() {
    const char c = current();
    if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n') {
        step();
        return true;
    }
    if (c == '/' && current(1) == '/') {
        step(); step();
        while (!done() && current() != '\r' && current() != '\n') step();
        return true;
    }
    if (c == '/' && current(1) == '*') {
        const Position start = location_;
        step(); step();
        while (!done()) {
            if (current() == '*' && current(1) == '/') {
                step(); step();
                return true;
            }
            step();
        }
        fail(start, "block comment is not closed");
        return true;
    }
    return false;
}

} // namespace toy_lex
