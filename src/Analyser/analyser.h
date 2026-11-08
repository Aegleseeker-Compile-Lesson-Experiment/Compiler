#ifndef ANALYSER_H
#define ANALYSER_H

#include "init.h"
#include "tools.h"
#include "ll1.h"
#include "slr1.h"
#include "lr1.h"
#include "lalr1.h"

class Analyser {
private:
    bool is_ll1 = false, is_slr1 = false, is_lr1 = false, is_lalr1 = false;
    bool has_argumentation_grammar = false;
    LL1 ll1;
    SLR1 slr1;
    LR1 lr1;
    LALR1 lalr1;
public:
    Analyser();
    bool ll1_analyse();
    void slr1_construct();
    bool slr1_analyse();
    void lr1_construct();
    bool lr1_analyse();
    void lalr1_construct();
    bool lalr1_analyse();
};

#endif // ANALYSER_H