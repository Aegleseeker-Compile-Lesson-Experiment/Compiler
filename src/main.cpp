#include <cstdio>
#include <string>

#include "Tokenizer/tokernizer.h"
#include "Analyser/analyser.h"
#include "Sementiser/sementiser.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::printf("Usage: %s -token/-ast\n", argv[0]);
        return 1;
    }
    if (std::string(argv[1]) == "-token") {
        Tokenizer T;
        if (!T.ill_symbol.empty()) {
            puts("reject");
            for (const auto &s : T.ill_symbol) {
                T.report_illegal_sequence(s.first, s.second);
            }
            puts("");
        }
        T.print();
    }
    if (std::string(argv[1]) == "-ast") {
        Tokenizer T;
        if (!T.ill_symbol.empty()) {
            puts("reject");
            for (const auto &s : T.ill_symbol) {
                T.report_illegal_sequence(s.first, s.second);
            }
            puts("");
        }
        Analyser A;
        T.print_to_sentence();
        A.lr1_construct();
        A.lalr1_construct();
        int res = A.lalr1_analyse();
        if (res == -1) {
            Sementiser S;
            auto msg = S.check_error();
            if (msg.empty()) {
                puts("accept");
            } else {
                puts("reject");
                for (const auto &m : msg) {
                    std::cout << m.first << " " << m.second << std::endl;
                }
            }
        } else {
            puts("reject");
            printf("%d\n", res);
        }
    }
    return 0;
}