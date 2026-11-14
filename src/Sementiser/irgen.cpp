#include "irgen.h"

void IRGenerator::emit(const std::string& inst) {
    if (!block_terminated) {
        ir_body << "  " << inst << "\n";
    }
}

void IRGenerator::emit_label(const std::string& label) {
    ir_body << label << ":\n";
    block_terminated = false;
}

IRGenerator::VarInfo* IRGenerator::lookup_var(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->count(name)) return &((*it)[name]);
    }
    return nullptr;
}
