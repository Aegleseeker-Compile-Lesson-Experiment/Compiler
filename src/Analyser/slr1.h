#ifndef SLR1_H
#define SLR1_H

#include "init.h"
#include "tools.h"

class SLR1 {
public:
    SLR1() = default;
    ~SLR1() = default;

    void export_to_dot_slr1(const std::string& filename = "./third_party/dots/slr_dfa.dot");
    void construct_slr1_closure();
    void print_slr_table();
    bool construct_slr1_analysis_table();
    bool analyse_slr1_sentence();

private:
    std::vector<std::pair<std::vector<Production>, std::vector<Production>>> closure_slr1;
    std::map<ClosureEntry, int> goto_slr1;
    std::map<ClosureEntry, Action> analysis_table_slr1;
};

#endif // SLR1_H