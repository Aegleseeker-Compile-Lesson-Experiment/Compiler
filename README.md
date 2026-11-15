## ToyC frontend

Build with `make` (C++17). `./compiler -token < input.c` prints tokens and lexical diagnostics. `./compiler -ast < input.c` checks lexical, syntactic and semantic constraints; output is `accept` or `reject` followed by sorted unique zero-based error lines. A rejected input returns exit code 0.

Semantic analysis traverses the AST, maintains lexical scopes and function signatures in declaration order (including `getint` and `putint`), and checks duplicate/undeclared names, const assignments, calls, expression types, returns, loop context, main, and constant zero divisors. A structured control-flow exit-set analysis tracks fallthrough, return, break and continue; constant branch/loop conditions refine reachability. Constant folding uses checked signed 32-bit arithmetic; unknown values remain unknown. The pass does not yet perform full definite-assignment analysis, general CFG construction, or LLVM IR generation.
