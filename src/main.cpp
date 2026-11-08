#include <cstdio>
#include <string>
#include <iostream>
#include <set>
#include <queue>
const std::string KEYWORD[] = {"void", "return", "int", "const", "while", "if", "else", "break", "continue"};
const std::string SYMBOL[] = {"+", "-", "*", "/", "%", "=", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "!", "(", ")", "{", "}", ";", ","};
enum class TokenType {
    KEYWORD,
    SYMBOL,
    IDENTIFIER,
    CONSTANT,
    STRING_LITERAL
};
struct Token {
    TokenType type;
    std::string lexeme;
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
    Tokenizer() {
        for (auto t : KEYWORD) { keyword.insert(t); }
        for (auto t : SYMBOL) { symbol.insert(t); }
        std::string s;
        bool in_comment = false;
        int line = 0;
        while (std::getline(std::cin, s)) {
            std::string t = "";
            for (int i = 0; i < s.length(); ++i) {
                if (in_comment) {
                    if (s[i] == '/' && s[i - 1] == '*') in_comment = false;
                    continue;
                }
                if (s[i] == '/' && s[i + 1] == '/') break;
                if (s[i] == '/' && s[i + 1] == '*') {
                    in_comment = true;
                    continue;
                }
                if (s[i] == ' ' || s[i] == '\t' || s[i] == '\n') {
                    catch_token(t, line, i);
                    continue;
                }
                std::string may_be_symbol = "", may_be_double_symbol = "";
                may_be_symbol += s[i];
                may_be_double_symbol = may_be_symbol + s[i + 1];
                if (is_symbol(may_be_double_symbol)) {
                    catch_token(t, line, i);
                    catch_token(may_be_double_symbol, line, i);
                    ++i;
                    continue;
                }
                if (is_symbol(may_be_symbol)) {
                    catch_token(t, line, i);
                    catch_token(may_be_symbol, line, i);
                    continue;
                }
                t += s[i];
            }
            catch_token(t, line++, s.size());
        }
    }
    bool is_keyword(const std::string &s) {
        if (keyword.find(s) == keyword.end()) return false;
        return true;
    }
    bool is_symbol(const std::string &s) {
        if (symbol.find(s) == symbol.end()) return false;
        return true;
    }
    bool is_constant(const std::string &s) {
        for (int i = 0; i < s.length(); ++i) {
            if (!isdigit(s[i])) return false;
        }
        return true;
    }
    void catch_token(std::string &s, const int line, const int column) {
        if (s != "" && s != "\n" && s != "\t" && s != "\r") {
            auto token = Token { TokenType::IDENTIFIER, s, line, column, 0 };
            if (is_constant(s)) {
                token.type = TokenType::CONSTANT;
                token.value = std::stoi(s);
            }
            if (is_keyword(s)) {
                token.type = TokenType::KEYWORD;
            }
            if (is_symbol(s)) {
                token.type = TokenType::SYMBOL;
            }
            tokens.push(token);
        }
        s = "";
    }
    std::string getNext() {
        if (tokens.empty()) return "";
        auto token = tokens.front();
        tokens.pop();
        return token.lexeme;
    }
    void print() {
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
                std::printf("%d:IntConst:\"%d\"\n", cnt++, token.value);
            } else if (token.type == TokenType::STRING_LITERAL) {
                std::printf("%d:StringLiteral:\"%s\"\n", cnt++, token.lexeme.c_str());
            }
        }
    }
};
int main(int argc, char *argv[]) {
    if (argc != 2 || std::string(argv[1]) != "-token") {
        std::fprintf(stderr, "Usage: %s -token < input.c > input.token\n", argv[0]);
        return 1;
    }
    Tokenizer Main;
    Main.print();
    return 0;
}