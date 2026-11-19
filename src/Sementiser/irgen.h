#ifndef IRGEN_H
#define IRGEN_H

#include "../Analyser/init.h"

class IRGenerator {
public:
    std::ostringstream entry_allocas;
    std::ostringstream ir_body;
    std::vector<IRInst> entry_allocas_insts;
    std::vector<IRInst> body_insts;
    std::vector<IRBlock> blocks;
    std::unordered_map<std::string, int> label_block;
    int cur_block = 0;
    int reg_id = 0;
    int label_id = 0;
    int ssa_id = 0;
    bool block_terminated = false;
    struct VarInfo {
        std::string ptr_name;
        bool is_const = false;
        int const_val = 0;
    };
    std::vector<std::pair<std::string, std::string>> loop_stack;
    std::string new_reg() { return "%t" + std::to_string(reg_id++); }
    std::string new_label() { return "L" + std::to_string(label_id++); }
    std::string new_ssa_reg() { return "%ssa_" + std::to_string(ssa_id++); }

    void emit(const std::string& inst);
    void emit_inst(const IRInst& inst);
    void emit_label(const std::string& label);
    int get_block(const std::string& label);
    void construct_blocks(const IRFunc& func);
    std::vector<LiveInterval> mem2reg(const std::vector<std::string> params);
    void dfs_block(int id);
    std::pair<std::vector<std::unordered_set<int>>, std::vector<int>> cooper();
    void create_phi(std::vector<std::unordered_set<int>> &DF, std::vector<std::string> &alloca_vars, std::unordered_map<std::string, std::unordered_set<int>> &def_blocks);
    void rename_var(std::vector<std::string> &alloca_vars, std::vector<int> &idom);
    void split_critical_edges();
    void eliminate_phis();
    std::vector<std::unordered_set<std::string>> live_inout();
    std::vector<LiveInterval> construct_intervals(const std::vector<std::string>& params, const std::vector<int>& block_start, const std::vector<int>& block_end, const std::vector<int>& call_indices, std::vector<std::unordered_set<std::string>> live_out);
};

inline bool is_vreg(const std::string& s);

std::vector<std::string> get_defs(const IRInst& inst);

std::vector<std::string> get_uses(const IRInst& inst);

#endif // !IRGEN_H