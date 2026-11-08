#include "analyser.h"

Analyser::Analyser() {
    if (!initial()) {
        return;
    }
    confirm_epsilon();
    // if (Global::enable_solve_left_recursion) {
    //     solve_left_recursion();
    // }
    getfirst();
    // if (Global::is_left_recursion) {
    //     puts("This grammar belongs to left recursion grammar");
    // }
    getfollow();
    if (ll1.checkLL1()) {
        is_ll1 = true;
    }
}

bool Analyser::ll1_analyse() {
    if (!is_ll1 || has_argumentation_grammar) return false;
    ll1.construct_predict_table();
    ll1.analyse_ll1_sentence();
    return true;
}

void Analyser::slr1_construct() {
    if (!has_argumentation_grammar) {
        argumentation_grammar();
    }
    slr1.construct_slr1_closure();
    is_slr1 = slr1.construct_slr1_analysis_table();
}

bool Analyser::slr1_analyse() {
    if (!is_slr1) return false;
    return slr1.analyse_slr1_sentence();
}

void Analyser::lr1_construct() {
    if (!has_argumentation_grammar) {
        argumentation_grammar();
    }
    lr1.construct_lr1_closure();
    is_lr1 = lr1.construct_lr1_analysis_table();
}

bool Analyser::lr1_analyse() {
    if (!is_lr1) return false;
    return lr1.analyse_lr1_sentence();
}

void Analyser::lalr1_construct() {
    if (!is_lr1) return;
    if (!has_argumentation_grammar) {
        argumentation_grammar();
    }
    lalr1 = LALR1(lr1.get_goto_lr1(), lr1.get_closure_lr1());
    lalr1.construct_lalr1_closure();
    is_lalr1 = lalr1.construct_lalr1_analysis_table();
}

bool Analyser::lalr1_analyse() {
    if (!is_lalr1) return false;
    return lalr1.analyse_lalr1_sentence();
}