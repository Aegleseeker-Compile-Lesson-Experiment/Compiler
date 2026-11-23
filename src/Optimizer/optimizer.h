#ifndef OPTIMIZER_H
#define OPTIMIZER_H

// Below are placeholders
#include "../Analyser/init.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

bool is_int_literal(const std::string& s);
void optimize_ir(std::vector<IRInst>& insts);
void optimize_cg(std::vector<AsmInst>& insts);
void eliminate_tail_recursion(IRFunc& fn);
void run_licm(std::vector<IRBlock>& blocks, std::vector<int>& idom);
void inline_functions(std::vector<IRFunc>& funcs);
void run_cse(std::vector<IRBlock>& blocks, const std::vector<int>& idom);

#endif //!OPTIMIZER_H