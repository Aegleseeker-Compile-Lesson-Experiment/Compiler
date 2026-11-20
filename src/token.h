#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace toy_lex {

// A token retains the original spelling and its starting source position.
enum class Category {
    name,         // identifier
    integer,      // decimal integer literal
    keyword,      // reserved word
    punctuation   // operator or delimiter
};

struct Position {
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Token {
    Category category;
    std::string text;
    Position start;
};

struct Error {
    Position at;
    std::string description;
};

struct Result {
    std::vector<Token> tokens;
    std::vector<Error> errors;
};

} // namespace toy_lex
