#include <cstdio>
#include <string>

#include "Tokenizer/tokernizer.h"
#include "Analyser/analyser.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::printf("Usage: %s -token/-ast\n", argv[0]);
        return 1;
    }
    if (std::string(argv[1]) == "-token") {
        Tokenizer Main;
        Main.print();
    }
    if (std::string(argv[1]) == "-ast") {
        Analyser Main;
        // Main.slr1_construct();
        // Main.slr1_analyse();
        Main.lr1_construct();
        Main.lr1_analyse();
        Main.lalr1_construct();
        Main.lalr1_analyse();
    }
    return 0;
}