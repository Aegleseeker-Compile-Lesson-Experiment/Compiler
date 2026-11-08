#include <cstdio>
#include <string>

#include "Tokenizer/tokernizer.h"

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
        puts("accept");
    }
    return 0;
}