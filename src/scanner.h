#pragma once

#include "token.h"

#include <string>

namespace toy_lex {

// Scans a small C-like language. All positions are one-based.
class Scanner {
public:
    explicit Scanner(std::string source);
    Result run();

private:
    bool done() const;
    char current(std::size_t lookahead = 0) const;
    void step();
    std::string slice(std::size_t from) const;
    void add(Category type, std::string text, Position at);
    void fail(Position at, std::string description);
    bool trivia();

    std::string input_;
    std::size_t offset_ = 0;
    Position location_;
    Result result_;
};

} // namespace toy_lex
