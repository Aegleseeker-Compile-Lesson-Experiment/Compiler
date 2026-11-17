#ifndef CODEGEN_H
#define CODEGEN_H

#include "../Analyser/init.h"

class CodeGen {
private:
    std::ostringstream asm_out;
    struct AsmInst {
        std::string op;
        std::string rd;
        std::string rs1;
        std::string rs2;
        int imm = 0;
        std::string label;
    };
    std::vector<AsmInst> insts;
    std::unordered_map<std::string, int> stack_offset;
    std::unordered_map<std::string, std::string> var_reg;
    std::vector<std::string> used_s_regs;
    int current_offset = -16;
    int frame_size = 0;
    std::string epilogue_label;

    int get_slot(const std::string& name);
    void emit_lw(const std::string& rd, int offset, const std::string& base = "s0");
    void emit_sw(const std::string& rs, int offset, const std::string& base = "s0");
    void emit_branch(const std::string& op, const std::string& rs1, const std::string& rs2, const std::string& label);
    void emit_jmp(const std::string& label);
    void load_operand(const std::string& op, const std::string& reg);
public:
    std::string generate(const std::vector<IRFunc>& funcs);
    void serialize_asm();
    void gen_func(const IRFunc& fn);
    void gen_inst(const IRInst& inst);
    void gen_mul(const int& value);
    void gen_div(const int& value, const bool& divided);
    void gen_rem(const int& value, const bool& divided);
};

#endif // !CODEGEN_H