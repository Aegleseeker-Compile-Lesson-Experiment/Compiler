#include "slr1.h"

void SLR1::export_to_dot_slr1(const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Failed to open " << filename << " for dot export.\n";
        return;
    }
    out << "digraph LR0_DFA {\n";
    out << "    rankdir=LR;\n";
    out << "    node [shape=box, fontname=\"Courier\", style=\"rounded,filled\", fillcolor=\"#f8f9fa\"];\n";
    out << "    edge [fontname=\"Courier\", fontsize=10];\n\n";
    for (size_t i = 0; i < closure_slr1.size(); ++i) {
        out << "    I" << i << " [shape=none, label=<\n";
        out << "        <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\" COLOR=\"#B0BEC5\">\n";
        out << "            <TR><TD BGCOLOR=\"#37474F\" ALIGN=\"CENTER\"><FONT COLOR=\"#FFFFFF\"><B>I<SUB>" 
            << i << "</SUB></B></FONT></TD></TR>\n";
        auto format_item_text = [](const Production& prod) {
            std::string text;
            for (auto symbol : prod) {
                if (symbol == "\\to") symbol = "-&gt;";
                else if (symbol == "\\cdot") symbol = "·";
                else if (symbol == "\\epsilon") symbol = "ε";
                else if (symbol == "<") symbol = "&lt;";
                else if (symbol == ">") symbol = "&gt;";
                else if (symbol == "&") symbol = "&amp;";
                text += symbol + " ";
            }
            return text;
        };
        for (const auto& prod : closure_slr1[i].first) {
            out << "            <TR><TD BGCOLOR=\"#E3F2FD\" ALIGN=\"LEFT\"><FONT COLOR=\"#0D47A1\"><B>"
                << format_item_text(prod)
                << "</B></FONT></TD></TR>\n";
        }
        if (!closure_slr1[i].first.empty() && !closure_slr1[i].second.empty()) {
            out << "            <HR/>\n";
        }
        for (const auto& prod : closure_slr1[i].second) {
            out << "            <TR><TD BGCOLOR=\"#FAFAFA\" ALIGN=\"LEFT\"><FONT COLOR=\"#616161\">"
                << format_item_text(prod)
                << "</FONT></TD></TR>\n";
        }
        out << "        </TABLE>\n";
        out << "    >];\n";
    }
    out << "\n";
    for (const auto& edge : goto_slr1) {
        int u = edge.first.first;
        std::string symbol = edge.first.second;
        int v = edge.second;
        if (symbol == "\\epsilon") symbol = "ε";
        if (symbol == "\"") symbol = "\\\"";
        out << "    I" << u << " -> I" << v << " [label=\"" << symbol << "\"];\n";
    }
    out << "}\n";
    out.close();
}

void SLR1::construct_slr1_closure() {
    closure_slr1.push_back({{{Global::START, "\\to", "\\cdot", Global::start_symbol}}, {}});
    for (size_t i = 0;; ++i) {
        if (i >= closure_slr1.size()) break;
        std::unordered_map<std::string, std::vector<Production>> out_productions;
        for (auto production : closure_slr1[i].first) {
            auto cdot_position = std::find(production.begin(), production.end(), "\\cdot") - production.begin();
            if (size_t(cdot_position + 1) == production.size()) {
                continue;
            }
            auto symbol = production[cdot_position + 1];
            swap(production[cdot_position], production[cdot_position + 1]);
            if (is_non_terminal_symbols(symbol)) {
                for (auto production_right : Global::productions[symbol]) {
                    Production s = {symbol, "\\to", "\\cdot"};
                    s.insert(s.end(), production_right.second.begin(), production_right.second.end());
                    if (s[3] == "\\epsilon") {
                        s.erase(s.begin() + 3);
                    }
                    if (std::find(closure_slr1[i].second.begin(), closure_slr1[i].second.end(), s) == closure_slr1[i].second.end()) {
                        closure_slr1[i].second.push_back(s);
                    }
                }
            }
            out_productions[production[cdot_position]].push_back(production);
        }
        for (int t = 0;; ++t) {
            if ((size_t)t >= closure_slr1[i].second.size()) break;
            auto production = closure_slr1[i].second[t];
            auto cdot_position = std::find(production.begin(), production.end(), "\\cdot") - production.begin();
            if (size_t(cdot_position + 1) == production.size()) {
                continue;
            }
            auto symbol = production[cdot_position + 1];
            swap(production[cdot_position], production[cdot_position + 1]);
            if (is_non_terminal_symbols(symbol)) {
                for (auto production_right : Global::productions[symbol]) {
                    Production s = {symbol, "\\to", "\\cdot"};
                    s.insert(s.end(), production_right.second.begin(), production_right.second.end());
                    if (s[3] == "\\epsilon") {
                        s.erase(s.begin() + 3);
                    }
                    if (std::find(closure_slr1[i].second.begin(), closure_slr1[i].second.end(), s) == closure_slr1[i].second.end()) {
                        closure_slr1[i].second.push_back(s);
                    }
                }
            }
            out_productions[production[cdot_position]].push_back(production);
        }
        for (const auto &symbol : Global::terminal_symbols) {
            if (out_productions[symbol].empty()) {
                continue;
            }
            bool prev_exist = false;
            for (size_t closure_index = 0; closure_index < closure_slr1.size(); ++closure_index) {
                const auto &closure = closure_slr1[closure_index];
                bool found = !out_productions[symbol].empty() && closure.first.size() == out_productions[symbol].size();
                if (found) {
                    for (const auto &production:closure.first) {
                        if (std::find(out_productions[symbol].begin(), out_productions[symbol].end(), production) == out_productions[symbol].end()) {
                            found = false;
                            break;
                        }
                    }
                }
                if (found) {
                    goto_slr1[{i, symbol}] = closure_index;
                    prev_exist = true;
                    break;
                }
            }
            if (!prev_exist) {
                goto_slr1[{i, symbol}] = closure_slr1.size();
                closure_slr1.push_back({out_productions[symbol], {}});
            }
        }
        for (const auto &symbol : Global::non_terminal_symbols) {
            if (out_productions[symbol].empty()) {
                continue;
            }
            bool prev_exist = false;
            for (size_t closure_index = 0; closure_index < closure_slr1.size(); ++closure_index) {
                const auto &closure = closure_slr1[closure_index];
                bool found = !out_productions[symbol].empty() && closure.first.size() == out_productions[symbol].size();
                if (found) {
                    for (const auto &production:closure.first) {
                        if (std::find(out_productions[symbol].begin(), out_productions[symbol].end(), production) == out_productions[symbol].end()) {
                            found = false;
                            break;
                        }
                    }
                }
                if (found) {
                    goto_slr1[{i, symbol}] = closure_index;
                    prev_exist = true;
                    break;
                }
            }
            if (!prev_exist) {
                goto_slr1[{i, symbol}] = closure_slr1.size();
                closure_slr1.push_back({out_productions[symbol],{}});
            }
        }
    }
    export_to_dot_slr1();
}

void SLR1::print_slr_table() {
    std::vector<std::string> action_cols = Global::terminal_symbols;
    if (std::find(action_cols.begin(), action_cols.end(), "$") == action_cols.end()) {
        action_cols.push_back("$");
    }
    std::vector<std::string> goto_cols;
    for (const auto& nt : Global::non_terminal_symbols) {
        if (nt != Global::START) {
            goto_cols.push_back(nt);
        }
    }
    const int W = 8;
    int action_total_w = action_cols.size() * W;
    int goto_total_w   = goto_cols.size() * W;
    auto print_h_line = [&]() {
        std::cout << "+-------+" << std::string(action_total_w, '-') << "+" << std::string(goto_total_w, '-') << "+\n";
    };
    std::cout << "\nSLR(1) Parsing Table:\n";
    print_h_line();
    std::cout << "| " << std::left << std::setw(5) << " " << " |"
              << std::setw(action_total_w / 2 + 3) << std::right << "ACTION"
              << std::setw(action_total_w - (action_total_w / 2 + 3)) << " " << "|"
              << std::setw(goto_total_w / 2 + 2) << std::right << "GOTO"
              << std::setw(goto_total_w - (goto_total_w / 2 + 2)) << " " << "|\n";
    std::cout << "| " << std::left << std::setw(5) << "State" << " |";
    for (const auto& term : action_cols) {
        std::cout << std::right << std::setw(W - 1) << term << " ";
    }
    std::cout << "|";
    for (const auto& non_term : goto_cols) {
        std::cout << std::right << std::setw(W - 1) << non_term << " ";
    }
    std::cout << "|\n";
    print_h_line();
    for (size_t i = 0; i < closure_slr1.size(); ++i) {
        std::cout << "| " << std::right << std::setw(5) << i << " |";
        for (const auto& term : action_cols) {
            std::string cell = "";
            if (analysis_table_slr1.count({i, term})) {
                auto act = analysis_table_slr1[{i, term}];
                if (act.first == SLRAction::S) {
                    cell = "s" + std::to_string(act.second);
                } else if (act.first == SLRAction::R) {
                    cell = "r" + std::to_string(act.second);
                } else if (act.first == SLRAction::Accept) {
                    cell = "acc";
                }
            }
            std::cout << std::right << std::setw(W - 1) << cell << " ";
        }
        std::cout << "|";
        for (const auto& non_term : goto_cols) {
            std::string cell = "";
            if (goto_slr1.count({i, non_term})) {
                cell = std::to_string(goto_slr1[{i, non_term}]);
            }
            std::cout << std::right << std::setw(W - 1) << cell << " ";
        }
        std::cout << "|\n";
    }
    print_h_line();
    std::cout << std::endl;
}

bool SLR1::construct_slr1_analysis_table() {
    bool not_slr = false;
    for (auto edge : goto_slr1) {
        if (analysis_table_slr1.find({edge.first.first, edge.first.second}) != analysis_table_slr1.end()) {
            not_slr = true;
        }
        analysis_table_slr1[{edge.first.first, edge.first.second}] = {SLRAction::S, edge.second};
    }
    if (not_slr) {
        puts("This grammar does not belong to SLR(1)\n");
        return false;
    }
    for (size_t i = 0; i < closure_slr1.size(); ++i) {
        for (auto production:closure_slr1[i].first) {
            if (production.back() == "\\cdot") {
                if (production.front() == Global::START) {
                    analysis_table_slr1[{i, "$"}] = {SLRAction::Accept, -1};
                } else {
                    for (auto symbol : Global::follow[production.front()]) {
                        if (analysis_table_slr1.find({i, symbol}) != analysis_table_slr1.end()) {
                            not_slr = true;
                        }
                        analysis_table_slr1[{i, symbol}] = {SLRAction::R, getindex(production)};
                    }
                }
            }
        }
        for (auto production : closure_slr1[i].second) {
            if (production.back() == "\\cdot") {
                production.push_back("\\epsilon");
                for (auto symbol : Global::follow[production.front()]) {
                    if (analysis_table_slr1.find({i, symbol}) != analysis_table_slr1.end()) {
                        not_slr = true;
                    }
                    analysis_table_slr1[{i, symbol}] = {SLRAction::R, getindex(production)};
                }
            }
        }
    }
    if (not_slr) {
        puts("This grammar does not belong to SLR(1)\n");
        return false;
    }
    print_slr_table();
    return true;
}

void SLR1::analyse_slr1_sentence() {
    for (auto sentence : Global::sentence) {
        std::cout << "Analyse ";
        for (const auto &symbol : sentence) {
            std::cout << symbol << " ";
        }
        std::cout << ":\n";
        const int W_STEP   = 5;
        const int W_STATE  = 22;
        const int W_SYMBOL = 20;
        const int W_INPUT  = 20;
        std::cout << std::left 
                  << std::setw(W_STEP)   << "Step"   << " | "
                  << std::setw(W_STATE)  << "State"  << " | "
                  << std::setw(W_SYMBOL) << "Symbol" << " | "
                  << std::setw(W_INPUT)  << "Input"  << " | "
                  << "Action\n";
        std::cout << std::string(W_STEP, '-')   << "-+-"
                  << std::string(W_STATE, '-')  << "-+-"
                  << std::string(W_SYMBOL, '-') << "-+-"
                  << std::string(W_INPUT, '-')  << "-+-"
                  << "---------------------\n";
        int step = 0;
        int index = 0;
        std::stack<ClosureEntry> stack;
        stack.push({0, "$"});
        sentence.push_back("$");
        while (true) {
            step++;
            auto closure = stack.top().first;
            auto right_symbol = sentence[index];
            auto stack_copy = stack;
            auto action = analysis_table_slr1[{closure, right_symbol}].first;
            auto target = analysis_table_slr1[{closure, right_symbol}].second;
            auto _ = (action == SLRAction::R) ? getproduction(target) : std::pair<std::string, Production>{"", Production{}};
            auto left = _.first;
            auto production = _.second;
            std::vector<int> stack_items_first;
            std::vector<std::string> stack_items_second;
            while (!stack_copy.empty()) {
                stack_items_first.push_back(stack_copy.top().first);
                stack_items_second.push_back(stack_copy.top().second);
                stack_copy.pop();
            }
            std::ostringstream ss_state;
            for (auto it = stack_items_first.rbegin(); it != stack_items_first.rend(); ++it) {
                ss_state << *it << " ";
            }
            std::ostringstream ss_symbol;
            for (auto it = stack_items_second.rbegin(); it != stack_items_second.rend(); ++it) {
                ss_symbol << *it << " ";
            }
            std::ostringstream ss_input;
            for (size_t i = index; i < sentence.size(); ++i) {
                ss_input << sentence[i] << " ";
            }
            std::ostringstream ss_action;
            if (action == SLRAction::S) {
                ss_action << "Shift " << target;
            } else if (action == SLRAction::R) {
                ss_action << "Reduce: " << left << " → ";
                for (const auto& symbol : production) {
                    ss_action << symbol << " ";
                }
            } else if (action == SLRAction::Accept) {
                ss_action << "Accept";
            }
            std::cout << std::left
                      << std::setw(W_STEP)   << step            << " | "
                      << std::setw(W_STATE)  << ss_state.str()  << " | "
                      << std::setw(W_SYMBOL) << ss_symbol.str() << " | "
                      << std::setw(W_INPUT)  << ss_input.str()  << " | "
                      << ss_action.str() << "\n";
            if (action == SLRAction::Accept) {
                std::cout << "\n";
                break;
            }
            if (action == SLRAction::S) {
                stack.push({target, sentence[index++]});
            }
            if (action == SLRAction::R) {
                for (size_t i = 0; i < production.size(); ++i) {
                    if (production[i] != "\\epsilon") {
                        stack.pop();
                    }
                }
                auto new_target = analysis_table_slr1[{stack.top().first, left}].second;
                stack.push({new_target, left});
            }
        }
    }
}
