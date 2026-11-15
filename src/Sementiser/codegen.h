#ifndef CODEGEN_H
#define CODEGEN_H

#include "../Analyser/init.h"

class CodeGen {
private:
    std::ostringstream asm_out;
    std::unordered_map<std::string, int> stack_offset;
    int current_offset = -16;
    int frame_size = 0;
    std::string epilogue_label;

    int get_slot(const std::string& name);
    void emit_lw(const std::string& rd, int offset, const std::string& base = "s0");
    void emit_sw(const std::string& rs, int offset, const std::string& base = "s0");
    void load_operand(const std::string& op, const std::string& reg);
public:
    std::string generate(const std::vector<IRFunc>& funcs);
    void gen_func(const IRFunc& fn);
    void gen_inst(const IRInst& inst);
};

#endif // !CODEGEN_H