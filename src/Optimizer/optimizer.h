#ifndef OPTIMIZER_H
#define OPTIMIZER_H

// Below are placeholders
#include "../Analyser/init.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

bool is_int_literal(const std::string& s);

void optimize_ir(IRFunc& fn);

#endif //!OPTIMIZER_H