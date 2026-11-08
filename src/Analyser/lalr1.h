#ifndef LALR1_H
#define LALR1_H

#include "init.h"
#include "tools.h"

class LALR1 {
public:
    LALR1() = default;
    LALR1(const std::map<ClosureEntry, int>& goto_lr1, std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>> closure_lr1);
    ~LALR1() = default;

    void export_to_dot_lalr1(const std::string& filename = "./third_party/dots/lalr_dfa.dot");
    void construct_lalr1_closure();
    void print_lalr1_table();
    bool construct_lalr1_analysis_table();
    bool analyse_lalr1_sentence();

private:
    std::vector<std::pair<std::vector<std::pair<Production, FollowSymbols>>, std::vector<std::pair<Production, FollowSymbols>>>> closure_lalr1;
    std::map<ClosureEntry, int> goto_lalr1;
    std::map<ClosureEntry, Action> analysis_table_lalr1;
};

#endif // LALR1_H