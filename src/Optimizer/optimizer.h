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
void analyze_interprocedural_non_negative(std::vector<IRUnit>& units);
void reassociate_loop_adds(std::vector<IRBlock>& blocks, const std::vector<int>& idom);
void run_ivsr(std::vector<IRBlock>& blocks, const std::vector<int>& idom, int& ssa_reg_counter);
void run_linear_combine(std::vector<IRBlock>& blocks, const std::vector<int>& idom, int& ssa_reg_counter);

#endif //!OPTIMIZER_H