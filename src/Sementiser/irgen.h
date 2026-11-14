#ifndef IRGEN_H
#define IRGEN_H

#include "../Analyser/init.h"

class IRGenerator {
public:
    std::ostringstream entry_allocas;
    std::ostringstream ir_body;
    int reg_id = 0;
    int label_id = 0;
    bool block_terminated = false;
    struct VarInfo {
        std::string ptr_name;
        bool is_const = false;
        int const_val = 0;
    };
    std::vector<std::unordered_map<std::string, VarInfo>> scopes;
    std::vector<std::pair<std::string, std::string>> loop_stack;
    std::string new_reg()   { return "%t" + std::to_string(reg_id++); }
    std::string new_label() { return "L" + std::to_string(label_id++); }

    void emit(const std::string& inst);
    void emit_label(const std::string& label);
    VarInfo* lookup_var(const std::string& name);
};

#endif // !IRGEN_H