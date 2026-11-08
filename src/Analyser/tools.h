#ifndef TOOLS_H
#define TOOLS_H

#include "init.h"
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

bool isblank(const std::string &s);
std::string trim(const std::string &s);
bool is_terminal_symbols(const std::string &s);
bool is_non_terminal_symbols(const std::string &s);
int getindex(Production production);
std::pair<std::string, Production> getproduction(int index);
void confirm_epsilon();
void getfirst();
void getfollow();
void argumentation_grammar();
void solve_left_recursion();
FollowSymbols get_follow_symbols(ptrdiff_t cdot_position, const Production &production, const std::unordered_set<std::string> &follow_symbols);

#endif // TOOLS_H