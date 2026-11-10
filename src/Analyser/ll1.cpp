#include "ll1.h"

bool LL1::checkLL1() {
    for (const auto &non_terminal_symbol : Global::non_terminal_symbols) {
        auto len = Global::productions[non_terminal_symbol].size();
        if (len <= 1) continue;
        std::vector<std::set<std::string>> s;
        bool check_epsilon = false;
        for (size_t i = 0; i < len; ++i) {
            std::set<std::string> sfirst;
            bool all_epsilon = true;
            for (const auto &symbol : Global::productions[non_terminal_symbol][i].second) {
                if (symbol == "\\epsilon") {
                    all_epsilon = true;
                    break;
                }
                if (is_terminal_symbols(symbol)) {
                    sfirst.insert(symbol);
                    all_epsilon = false;
                    break;
                } else {
                    sfirst.insert(Global::first[symbol].begin(), Global::first[symbol].end());
                    sfirst.erase("\\epsilon");
                    if (Global::can_be_epsilon[symbol] == Epsilon::No) {
                        all_epsilon = false;
                        break;
                    }
                }
            }
            if (all_epsilon) {
                check_epsilon = true;
                sfirst.insert("\\epsilon");
            }
            s.push_back(sfirst);
        }
        for (size_t i = 0; i < len; ++i) {
            for (size_t j = i + 1; j < len; ++j) {
                for (const auto& symbol:s[i]) {
                    if (s[j].count(symbol)) {
                        return false;
                    }
                }
            }
        }
        if (check_epsilon) {
            for (size_t i = 0; i < len; ++i) {
                if (s[i].find("\\epsilon") != s[i].end()) continue;
                for (const auto& symbol : s[i]) {
                    if (Global::follow[non_terminal_symbol].count(symbol)) return false;
                }
            }
        }
    }
    return true;
}

void LL1::construct_predict_table() {
    for (const auto &non_terminal_symbol : Global::non_terminal_symbols) {
        auto len = Global::productions[non_terminal_symbol].size();
        std::vector<std::set<std::string>> s;
        for (size_t i = 0; i < len; ++i) {
            auto symbol = Global::productions[non_terminal_symbol][i].second[0];
            if (symbol == "\\epsilon") {
                for (auto terminal_symbol:Global::follow[non_terminal_symbol]) {
                    predict_table[{non_terminal_symbol, terminal_symbol}] = Global::productions[non_terminal_symbol][i].second;
                }
                continue;
            }
            if (is_non_terminal_symbols(symbol)) {
                for (auto terminal_symbol:Global::first[symbol]) {
                    predict_table[{non_terminal_symbol, terminal_symbol}] = Global::productions[non_terminal_symbol][i].second;
                }
            } else {
                predict_table[{non_terminal_symbol, symbol}] = Global::productions[non_terminal_symbol][i].second;
            }
        }
    }
    for (const auto &prediction:predict_table) {
        std::cout << "Prediction[" << prediction.first.first << ", " << prediction.first.second << "] = " << prediction.first.first << " → ";
        for (auto symbol:prediction.second) {
            if (symbol == "\\epsilon") symbol = "ε";
            std::cout << symbol << " ";
        }
        puts("");
    }
    puts("");
}

void LL1::analyse_ll1_sentence() {
    for (auto sentence : Global::sentence) {
        std::cout << "Analyse ";
        for (const auto &symbol:sentence) {
            std::cout << symbol.lexeme << " ";
        }
        puts(":");
        std::stack<std::string> stack;
        stack.push(Global::start_symbol);
        sentence.push_back(Token{ TokenType::KEYWORD, "$", "$", 0, 0, 0 });
        for (size_t i = 0; i < sentence.size(); ++i) {
            while(!stack.empty() && stack.top() != sentence[i].lexeme) {
                auto stack_copy = stack;
                std::vector<std::string> stack_items;
                while (!stack_copy.empty()) {
                    stack_items.push_back(stack_copy.top());
                    stack_copy.pop();
                }
                for (auto it = stack_items.rbegin(); it != stack_items.rend(); ++it) {
                    if (*it == "\\epsilon") std::cout << "ε" << " ";
                    else std::cout << *it << " ";
                }
                std::cout << "| " << sentence[i].lexeme << std::endl;
                auto production = predict_table[{stack.top(), sentence[i].lexeme}];
                stack.pop();
                for (int j = production.size() - 1; j >= 0; --j) {
                    if (production[(size_t)j] != "\\epsilon") {
                        stack.push(production[(size_t)j]);
                    }
                }
            }
            auto stack_copy = stack;
            std::vector<std::string> stack_items;
            while (!stack_copy.empty()) {
                stack_items.push_back(stack_copy.top());
                stack_copy.pop();
            }
            for (auto it = stack_items.rbegin(); it != stack_items.rend(); ++it) {
                if (*it == "\\epsilon") std::cout << "ε" << " ";
                else std::cout << *it << " ";
            }
            if (!stack.empty()) {
                stack.pop();
                std::cout << "| " << sentence[i].lexeme << std::endl;
            } else {
                std::cout << "accept" << std::endl;
                puts("");
            }
        }
    }
}
