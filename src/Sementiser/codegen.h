#ifndef CODEGEN_H
#define CODEGEN_H

#include "../Analyser/init.h"
#include "../Optimizer/optimizer.h"
#include "irgen.h"

class CodeGen {
private:
    std::ostringstream asm_out;
    std::vector<AsmInst> insts;
    std::unordered_map<std::string, int> stack_offset;
    std::unordered_map<std::string, std::string> var_reg;
    std::vector<std::string> used_s_regs;
    int current_offset = -16;
    int frame_size = 0;
    std::string epilogue_label;

    int get_slot(const std::string& name);
    void emit_ld(const std::string& rd, int offset);
    void emit_lw(const std::string& rd, int offset);
    void emit_sd(const std::string& rs, int offset);
    void emit_sw(const std::string& rs, int offset);
    std::string use_operand(const std::string& op, const std::string& scratch);
    std::string def_operand(const std::string& dst, const std::string& scratch);
    void commit_dst(const std::string& dst, const std::string& scratch);
    void emit_parallel_moves(const std::vector<std::pair<std::string, std::string>>& moves);
    void gen_mul(const std::string& dst_vreg, const std::string& var_op, int value);
    void gen_div(const std::string& dst_vreg, const std::string& var_op, int value, bool divided, bool non_neg);
    void gen_rem(const std::string& dst_vreg, const std::string& var_op, int value, bool divided, bool non_neg);
    void serialize_asm();

public:
    RegAllocResult alloc;
    std::unordered_set<std::string> non_neg_regs;

    void linear_scan_allocate(std::vector<LiveInterval>& intervals);
    void gen_func(const std::vector<IRBlock>& blocks, const IRFunc& func);
    std::string generate();
};

#endif // !CODEGEN_H