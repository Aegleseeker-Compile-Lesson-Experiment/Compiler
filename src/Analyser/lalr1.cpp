#include "lalr1.h"

LALR1::LALR1(const std::map<ClosureEntry, int>& goto_lr1, std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>> closure_lr1) {
    goto_lalr1 = goto_lr1;
    closure_lalr1 = closure_lr1;
}

void LALR1::export_to_dot_lalr1(const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Failed to open " << filename << " for dot export.\n";
        return;
    }
    out << "digraph LR0_DFA {\n";
    out << "    rankdir=LR;\n";
    out << "    node [shape=box, fontname=\"Courier\", style=\"rounded,filled\", fillcolor=\"#f8f9fa\"];\n";
    out << "    edge [fontname=\"Courier\", fontsize=10];\n\n";
    for (size_t i = 0; i < closure_lalr1.size(); ++i) {
        out << "    I" << i << " [shape=none, label=<\n";
        out << "        <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\" COLOR=\"#B0BEC5\">\n";
        out << "            <TR><TD BGCOLOR=\"#37474F\" ALIGN=\"CENTER\"><FONT COLOR=\"#FFFFFF\"><B>I<SUB>" 
            << i << "</SUB></B></FONT></TD></TR>\n";
        auto format_item_text = [](const Production& prod, const FollowSymbols& follow_symbols) {
            std::string text;
            for (auto symbol : prod) {
                if (symbol == "\\to") symbol = "-&gt;";
                else if (symbol == "\\cdot") symbol = "·";
                else if (symbol == "\\epsilon") symbol = "ε";
                else if (symbol == "<") symbol = "&lt;";
                else if (symbol == "<=") symbol = "&lt;=";
                else if (symbol == ">") symbol = "&gt;";
                else if (symbol == ">=") symbol = "&gt;=";
                else if (symbol == "&") symbol = "&amp;";
                else if (symbol == "&&") symbol = "&amp;&amp;";
                text += symbol + " ";
            }
            text += ", ";
            for (auto symbol : follow_symbols) {
                if (symbol == "\\to") symbol = "-&gt;";
                else if (symbol == "\\cdot") symbol = "·";
                else if (symbol == "\\epsilon") symbol = "ε";
                else if (symbol == "<") symbol = "&lt;";
                else if (symbol == "<=") symbol = "&lt;=";
                else if (symbol == ">") symbol = "&gt;";
                else if (symbol == ">=") symbol = "&gt;=";
                else if (symbol == "&") symbol = "&amp;";
                else if (symbol == "&&") symbol = "&amp;&amp;";
                text += symbol + " ";
            }
            return text;
        };
        for (const auto& prod : closure_lalr1[i].first) {
            out << "            <TR><TD BGCOLOR=\"#E3F2FD\" ALIGN=\"LEFT\"><FONT COLOR=\"#0D47A1\"><B>"
                << format_item_text(prod.first, prod.second)
                << "</B></FONT></TD></TR>\n";
        }
        if (!closure_lalr1[i].first.empty() && !closure_lalr1[i].second.empty()) {
            out << "            <HR/>\n";
        }
        for (const auto& prod : closure_lalr1[i].second) {
            out << "            <TR><TD BGCOLOR=\"#FAFAFA\" ALIGN=\"LEFT\"><FONT COLOR=\"#616161\">"
                << format_item_text(prod.first, prod.second)
                << "</FONT></TD></TR>\n";
        }
        out << "        </TABLE>\n";
        out << "    >];\n";
    }
    out << "\n";
    for (const auto& edge : goto_lalr1) {
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

void LALR1::construct_lalr1_closure() {
    int n = closure_lalr1.size();
    if (n == 0) return;
    auto is_same_core = [](const std::vector<std::pair<Production, FollowSymbols>>& k1, const std::vector<std::pair<Production, FollowSymbols>>& k2) -> bool {
        if (k1.size() != k2.size()) return false;
        std::vector<bool> matched(k2.size(), false);
        for (const auto& item1 : k1) {
            bool found = false;
            for (size_t idx = 0; idx < k2.size(); ++idx) {
                if (!matched[idx] && item1.first == k2[idx].first) {
                    matched[idx] = true;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    };
    std::vector<int> old_to_new(n, -1);
    std::vector<std::vector<int>> groups;
    for (int i = 0; i < n; ++i) {
        if (old_to_new[i] != -1) continue;
        int new_id = groups.size();
        groups.push_back({i});
        old_to_new[i] = new_id;
        for (int j = i + 1; j < n; ++j) {
            if (old_to_new[j] == -1 && is_same_core(closure_lalr1[i].first, closure_lalr1[j].first)) {
                groups[new_id].push_back(j);
                old_to_new[j] = new_id;
            }
        }
    }
    auto merge_items = [](const std::vector<int>& group,
                          const std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>>& old_states,
                          bool is_kernel) -> std::vector<std::pair<Production, FollowSymbols>> {
        std::vector<std::pair<Production, FollowSymbols>> merged;
        for (int old_idx : group) {
            const auto& items = is_kernel ? old_states[old_idx].first : old_states[old_idx].second;
            for (const auto& item : items) {
                const auto& prod = item.first;
                const auto& lookaheads = item.second;

                auto it = std::find_if(merged.begin(), merged.end(),
                    [&](const std::pair<Production, FollowSymbols>& existing) { return existing.first == prod; });

                if (it != merged.end()) {
                    it->second.insert(lookaheads.begin(), lookaheads.end());
                } else {
                    merged.push_back({prod, lookaheads});
                }
            }
        }
        return merged;
    };
    std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>> new_closure_lalr1;
    new_closure_lalr1.reserve(groups.size());
    for (const auto& group : groups) {
        std::vector<std::pair<Production, FollowSymbols>> merged_kernel = merge_items(group, closure_lalr1, true);
        std::vector<std::pair<Production, FollowSymbols>> merged_non_kernel = merge_items(group, closure_lalr1, false);
        new_closure_lalr1.push_back({merged_kernel, merged_non_kernel});
    }
    std::map<ClosureEntry, int> new_goto_lalr1;
    for (const auto& entry : goto_lalr1) {
        int old_from = entry.first.first;
        const std::string& symbol = entry.first.second;
        int old_to = entry.second;
        int new_from = old_to_new[old_from];
        int new_to = old_to_new[old_to];
        new_goto_lalr1[{new_from, symbol}] = new_to;
    }
    closure_lalr1 = std::move(new_closure_lalr1);
    goto_lalr1 = std::move(new_goto_lalr1);
    export_to_dot_lalr1();
}

void LALR1::print_lalr1_table() {
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
    // const int W = 8;
    // int action_total_w = action_cols.size() * W;
    // int goto_total_w   = goto_cols.size() * W;
    // auto print_h_line = [&]() {
    //     std::cout << "+-------+" << std::string(action_total_w, '-') << "+" << std::string(goto_total_w, '-') << "+\n";
    // };
    // if (Global::enable_debug) {
    //     std::cout << "\nLALR(1) Parsing Table:\n";
    //     print_h_line();
    //     std::cout << "| " << std::left << std::setw(5) << " " << " |"
    //             << std::setw(action_total_w / 2 + 3) << std::right << "ACTION"
    //             << std::setw(action_total_w - (action_total_w / 2 + 3)) << " " << "|"
    //             << std::setw(goto_total_w / 2 + 2) << std::right << "GOTO"
    //             << std::setw(goto_total_w - (goto_total_w / 2 + 2)) << " " << "|\n";
    //     std::cout << "| " << std::left << std::setw(5) << "State" << " |";
    //     for (const auto& term : action_cols) {
    //         std::cout << std::right << std::setw(W - 1) << term << " ";
    //     }
    //     std::cout << "|";
    //     for (const auto& non_term : goto_cols) {
    //         std::cout << std::right << std::setw(W - 1) << non_term << " ";
    //     }
    //     std::cout << "|\n";
    //     print_h_line();
    //     for (size_t i = 0; i < closure_lalr1.size(); ++i) {
    //         std::cout << "| " << std::right << std::setw(5) << i << " |";
    //         for (const auto& term : action_cols) {
    //             std::string cell = "";
    //             if (analysis_table_lalr1.count({i, term})) {
    //                 auto act = analysis_table_lalr1[{i, term}];
    //                 if (act.first == SLRAction::S) {
    //                     cell = "s" + std::to_string(act.second);
    //                 } else if (act.first == SLRAction::R) {
    //                     cell = "r" + std::to_string(act.second);
    //                 } else if (act.first == SLRAction::Accept) {
    //                     cell = "acc";
    //                 }
    //             }
    //             std::cout << std::right << std::setw(W - 1) << cell << " ";
    //         }
    //         std::cout << "|";
    //         for (const auto& non_term : goto_cols) {
    //             std::string cell = "";
    //             if (goto_lalr1.count({i, non_term})) {
    //                 cell = std::to_string(goto_lalr1[{i, non_term}]);
    //             }
    //             std::cout << std::right << std::setw(W - 1) << cell << " ";
    //         }
    //         std::cout << "|\n";
    //     }
    //     print_h_line();
    //     std::cout << std::endl;
    // }
}

bool LALR1::construct_lalr1_analysis_table() {
    for (auto edge:goto_lalr1) {
        analysis_table_lalr1[{edge.first.first, edge.first.second}] = {SLRAction::S, edge.second};
    }
    for (size_t i = 0; i < closure_lalr1.size(); ++i) {
        for (auto production_follow_symbols : closure_lalr1[i].first) {
            auto production = production_follow_symbols.first;
            auto follow_symbols = production_follow_symbols.second;
            if (production.back() == "\\cdot") {
                if (production.front() == Global::START) {
                    analysis_table_lalr1[{i, "$"}] = {SLRAction::Accept, -1};
                } else {
                    for (auto symbol:follow_symbols) {
                        if (analysis_table_lalr1.find({i, symbol}) != analysis_table_lalr1.end()) {
                            if (Global::enable_debug) {
                                puts("This grammar does not belong to LALR(1)\n");
                            }
                            return false;
                        }
                        analysis_table_lalr1[{i, symbol}] = {SLRAction::R, getindex(production)};
                    }
                }
            }
        }
        for (auto production_follow_symbols : closure_lalr1[i].second) {
            auto production = production_follow_symbols.first;
            auto follow_symbols = production_follow_symbols.second;
            if (production.back() == "\\cdot") {
                production.push_back("\\epsilon");
                for (auto symbol:follow_symbols) {
                    if (analysis_table_lalr1.find({i, symbol}) != analysis_table_lalr1.end()) {
                        if (Global::enable_debug) {
                            puts("This grammar does not belong to LALR(1)\n");
                        }
                        return false;
                    }
                    analysis_table_lalr1[{i, symbol}] = {SLRAction::R, getindex(production)};
                }
            }
        }
    }
    print_lalr1_table();
    return true;
}

int LALR1::analyse_lalr1_sentence() {
    for (auto sentence : Global::sentence) {
        // if (Global::enable_debug) {
        //     std::cout << "Analyse ";
        //     for (const auto &symbol : sentence) {
        //         std::cout << symbol << " ";
        //     }
        //     std::cout << ":\n";
        // }
        // const int W_STEP   = 5;
        // const int W_STATE  = 22;
        // const int W_SYMBOL = 20;
        // const int W_INPUT  = 20;
        // if (Global::enable_debug) {
        //     std::cout << std::left 
        //             << std::setw(W_STEP)   << "Step"   << " | "
        //             << std::setw(W_STATE)  << "State"  << " | "
        //             << std::setw(W_SYMBOL) << "Symbol" << " | "
        //             << std::setw(W_INPUT)  << "Input"  << " | "
        //             << "Action\n";
        //     std::cout << std::string(W_STEP, '-')   << "-+-"
        //             << std::string(W_STATE, '-')  << "-+-"
        //             << std::string(W_SYMBOL, '-') << "-+-"
        //             << std::string(W_INPUT, '-')  << "-+-"
        //             << "---------------------\n";
        // }
        int step = 0;
        int index = 0;
        std::stack<ClosureEntry> stack;
        stack.push({0, "$"});
        std::stack<ASTNode*> sem_stack;
        while (true) {
            step++;
            auto closure = stack.top().first;
            auto right_symbol = sentence[index].symbol;
            auto stack_copy = stack;
            if (analysis_table_lalr1.find({closure, right_symbol}) == analysis_table_lalr1.end()) {
                return sentence[index].line + 1;
            }
            auto action = analysis_table_lalr1[{closure, right_symbol}].first;
            auto target = analysis_table_lalr1[{closure, right_symbol}].second;
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
                ss_input << sentence[i].symbol << " ";
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
            if (Global::enable_debug) {
                std::cout << "[Step " << std::setw(3) << std::setfill('0') << step << std::setfill(' ') << "] "
                        << "TopState: " << std::setw(3) << closure 
                        << " | Lookahead: '" << std::setw(8) << right_symbol << "' | ";

                if (action == SLRAction::S) {
                    std::cout << ANSI_CYAN << "Shift -> State " << target << ANSI_RESET << "\n";
                } else if (action == SLRAction::R) {
                    std::cout << ANSI_YELLOW << "Reduce: " << left << " → ";
                    for (const auto& s : production) std::cout << s << " ";
                    std::cout << ANSI_RESET << "\n";
                } else if (action == SLRAction::Accept) {
                    std::cout << ANSI_GREEN << ">>> ACCEPT <<<" << ANSI_RESET << "\n";
                }
            }
            // if (Global::enable_debug) {
            //     std::cout << std::left
            //             << std::setw(W_STEP)   << step            << " | "
            //             << std::setw(W_STATE)  << ss_state.str()  << " | "
            //             << std::setw(W_SYMBOL) << ss_symbol.str() << " | "
            //             << std::setw(W_INPUT)  << ss_input.str()  << " | "
            //             << ss_action.str() << "\n";
            // }
            if (action == SLRAction::Accept) {
                if (Global::enable_debug) {
                    std::cout << "\n";
                }
                if (Global::ASTRoot != nullptr) {
                    delete Global::ASTRoot;
                }
                Global::ASTRoot = sem_stack.top();
                break;
            }
            if (action == SLRAction::S) {
                auto token = sentence[index++];
                stack.push({target, token.symbol});
                ASTNode* node = token_to_astnode(token);
                sem_stack.push(node);
            }
            if (action == SLRAction::R) {
                for (size_t i = 0; i < production.size(); ++i) {
                    if (production[i] != "\\epsilon") {
                        stack.pop();
                    }
                }
                auto new_target = analysis_table_lalr1[{stack.top().first, left}].second;
                stack.push({new_target, left});
                reduce_to_astnode(sem_stack, left, production);
            }
        }
    }
    return 0;
}
