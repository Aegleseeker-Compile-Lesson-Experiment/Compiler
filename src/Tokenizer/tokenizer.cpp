#include "tokernizer.h"
#include "../Analyser/init.h"

Tokenizer::Tokenizer() {
    for (const auto &kw : KEYWORD) keyword.insert(kw);
    for (const auto &sym : SYMBOL) symbol.insert(sym);
    std::string s;
    bool in_comment = false;
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
            catch_token(t, line++, s.size());
        } else {
            line++;
        }
    }
    auto token = Token { TokenType::SP_EOF, s, "$", line, 0, 0 };
    tokens.push(token);
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

bool Tokenizer::is_identifier(const std::string &s) {
    unsigned char first = static_cast<unsigned char>(s[0]);
    if (!std::isalpha(first) && first != '_') {
        return false;
    }
    for (size_t i = 1; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

void Tokenizer::catch_token(std::string &s, const int line, const int column) {
    if (s != "" && s != "\n" && s != "\t" && s != "\r") {
        auto token = Token { TokenType::IDENTIFIER, s, "ID", line, column, 0 };
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
        if (token.type == TokenType::IDENTIFIER) {
            if (is_identifier(s)) {
                tokens.push(token);
            } else {
                ill_symbol.push_back({line, s});
            }
        } else {
            tokens.push(token);
        }
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

inline int Tokenizer::get_utf8_char_len(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x06) return 2;
    if ((lead >> 4) == 0x0E) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;
}

void Tokenizer::report_illegal_sequence(int line, const std::string& invalid_chunk) {
    size_t pos = 0;
    while (pos < invalid_chunk.size()) {
        unsigned char lead = static_cast<unsigned char>(invalid_chunk[pos]);
        int len = get_utf8_char_len(lead);
        if (pos + len > invalid_chunk.size()) {
            len = invalid_chunk.size() - pos;
        }
        std::string single_char = invalid_chunk.substr(pos, len);
        if (single_char == "\\") {
            std::cout << line << " illegal character '\\\\'" << std::endl;
        } else if (single_char == "\'") {
            std::cout << line << " illegal character \"\'\"" << std::endl;
        } else {
            std::cout << line << " illegal character '" << single_char << "'" << std::endl;
        }
        pos += len;
    }
}