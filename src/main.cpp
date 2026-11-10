#include <cstdio>
#include <string>

#include "Tokenizer/tokernizer.h"
#include "Analyser/analyser.h"
#include "Analyser/sementiser.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::printf("Usage: %s -token/-ast\n", argv[0]);
        return 1;
    }
    if (std::string(argv[1]) == "-token") {
        Tokenizer T;
        T.print();
    }
    if (std::string(argv[1]) == "-ast") {
        Tokenizer T;
        Analyser A;
        T.print_to_sentence();
        A.lr1_construct();
        A.lalr1_construct();
        int res = A.lalr1_analyse();
        if (!res) {
            Sementiser S;
            auto msg = S.check_error();
            if (msg.empty()) {
                puts("accept");
            } else {
                puts("reject");
                for (const auto &m : msg) {
                    std::cout << m.first + 1 << " " << m.second << std::endl;
                }
            }
        } else {
            puts("reject");
            printf("%d\n", res);
        }
    }
    return 0;
}