#ifndef LR1_H
#define LR1_H

#include "init.h"
#include "tools.h"

class LR1 {
public:
    LR1() = default;
    ~LR1() = default;

    void export_to_dot_lr1(const std::string& filename = "./third_party/dots/lr_dfa.dot");
    void construct_lr1_closure();
    void print_lr1_table();
    bool construct_lr1_analysis_table();
    void analyse_lr1_sentence();
    std::map<ClosureEntry, int> get_goto_lr1() const { return goto_lr1; }
    std::map<ClosureEntry, Action> get_analysis_table_lr1() const { return analysis_table_lr1; }
    std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>> get_closure_lr1() const { return closure_lr1; }

private:
    std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>> closure_lr1;
    std::map<ClosureEntry, int> goto_lr1;
    std::map<ClosureEntry, Action> analysis_table_lr1;
};

#endif // LR1_H