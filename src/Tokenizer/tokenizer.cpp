#include "tokernizer.h"
#include "../Analyser/init.h"

Tokenizer::Tokenizer() {
    for (const auto &kw : KEYWORD) keyword.insert(kw);
    for (const auto &sym : SYMBOL) symbol.insert(sym);
    std::string s;
    bool in_comment = false;
    bool may_be_eof = false;
    int eof_line = -1;
    int line = 0;
    while (std::getline(std::cin, s)) {
        if (!s.empty() && s.back() == '\r') {
            s.pop_back();
        }
        std::string t = "";
        for (size_t i = 0; i < s.length(); ++i) {
            if (in_comment) {
                if (s[i] == '*' && i + 1 < s.length() && s[i + 1] == '/') {
                    in_comment = false;
                    ++i;
                }
                continue;
            }
            if (s[i] == '/' && i + 1 < s.length() && s[i + 1] == '/') {
                catch_token(t, line, i);
                break;
            }
            if (s[i] == '/' && i + 1 < s.length() && s[i + 1] == '*') {
                catch_token(t, line, i);
                in_comment = true;
                ++i;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(s[i]))) {
                catch_token(t, line, i);
                continue;
            }
            if (i + 1 < s.length()) {
                std::string double_symbol = {s[i], s[i + 1]};
                if (is_symbol(double_symbol)) {
                    catch_token(t, line, i);
                    catch_token(double_symbol, line, i);
                    ++i;
                    continue;
                }
            }
            std::string single_symbol = {s[i]};
            if (is_symbol(single_symbol)) {
                catch_token(t, line, i);
                catch_token(single_symbol, line, i);
                continue;
            }
            t += s[i];
        }
        if (!in_comment) {
            if (t == "") {
                if (may_be_eof == false) {
                    eof_line = line;
                    may_be_eof = true;
                }
            } else {
                may_be_eof = false;
            }
            catch_token(t, line++, s.size());
        } else {
            line++;
        }
    }
    std::string eof = "$";
    catch_token(eof, eof_line, 0);
}

bool Tokenizer::is_keyword(const std::string &s) {
    if (keyword.find(s) == keyword.end()) return false;
    return true;
}

bool Tokenizer::is_symbol(const std::string &s) {
    if (symbol.find(s) == symbol.end()) return false;
    return true;
}

bool Tokenizer::is_constant(const std::string &s) {
    for (size_t i = 0; i < s.length(); ++i) {
        if (!isdigit(s[i])) return false;
    }
    return true;
}

void Tokenizer::catch_token(std::string &s, const int line, const int column) {
    if (s != "" && s != "\n" && s != "\t" && s != "\r") {
        auto token = Token { TokenType::IDENTIFIER, s, "ID", line, column, 0 };
        if (s == "$") {
            token.type = TokenType::SP_EOF;
            token.symbol = "$";
        }
        if (is_constant(s)) {
            token.type = TokenType::CONSTANT;
            token.symbol = "NUMBER";
            token.value = std::stoi(s);
        }
        if (is_keyword(s)) {
            token.type = TokenType::KEYWORD;
            token.symbol = s;
        }
        if (is_symbol(s)) {
            token.type = TokenType::SYMBOL;
            token.symbol = s;
        }
        tokens.push(token);
    }
    s = "";
}

std::string Tokenizer::getNext() {
    if (tokens.empty()) return "";
    auto token = tokens.front();
    tokens.pop();
    return token.lexeme;
}

void Tokenizer::print() {
    int cnt = 0;
    while (!tokens.empty()) {
        auto token = tokens.front();
        tokens.pop();
        if (token.type == TokenType::KEYWORD) {
            std::printf("%d:'%s':\"%s\"\n", cnt++, token.lexeme.c_str(), token.lexeme.c_str());
        } else if (token.type == TokenType::SYMBOL) {
            std::printf("%d:'%s':\"%s\"\n", cnt++, token.lexeme.c_str(), token.lexeme.c_str());
        } else if (token.type == TokenType::IDENTIFIER) {
            std::printf("%d:Ident:\"%s\"\n", cnt++, token.lexeme.c_str());
        } else if (token.type == TokenType::CONSTANT) {
            std::printf("%d:IntConst:\"%s\"\n", cnt++, token.lexeme.c_str());
        } else if (token.type == TokenType::STRING_LITERAL) {
            std::printf("%d:StringLiteral:\"%s\"\n", cnt++, token.lexeme.c_str());
        }
    }
}

void Tokenizer::print_to_sentence() {
    std::vector<Token> sentence;
    while (!tokens.empty()) {
        sentence.push_back(tokens.front());
        tokens.pop();
    }
    Global::sentence.clear();
    Global::sentence.push_back(sentence);
}