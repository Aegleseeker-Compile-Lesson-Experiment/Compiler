#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <iostream>
#include <string>
#include <queue>
#include <set>

const std::string KEYWORD[] = {"void", "return", "int", "const", "while", "if", "else", "break", "continue"};
const std::string SYMBOL[] = {"+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "!", "(", ")", "{", "}", ";", ","};
enum class TokenType {
    KEYWORD,
    SYMBOL,
    IDENTIFIER,
    CONSTANT,
    STRING_LITERAL,
    SP_EOF
};
struct Token {
    TokenType type;
    std::string lexeme;
    std::string symbol;
    int line;
    int column;
    int value; // Value if constant else 0.
};

class Tokenizer {
private:
    std::queue<Token> tokens;
    std::set<std::string> keyword;
    std::set<std::string> symbol;
public:
    Tokenizer();
    bool is_keyword(const std::string &s);
    bool is_symbol(const std::string &s);
    bool is_constant(const std::string &s);
    void catch_token(std::string &s, const int line, const int column);
    std::string getNext();
    void print();
    void print_to_sentence();
};

#endif // TOKENIZER_H