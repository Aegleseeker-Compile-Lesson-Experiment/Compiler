#ifndef LL1_H
#define LL1_H

#include "init.h"

class LL1 {
public:
    LL1() = default;
    ~LL1() = default;

    bool checkLL1();
    void construct_predict_table();
    void analyse_ll1_sentence();

private:
    std::map<Entry, Production> predict_table;
};

#endif // LL1_H
