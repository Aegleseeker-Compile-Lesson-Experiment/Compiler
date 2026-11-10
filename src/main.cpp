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
        if (!T.ill_symbol.empty()) {
            puts("reject");
            for (const auto &s : T.ill_symbol) {
                if (s.second.size() > 1) {
                    for (size_t i = 0; i < s.second.size(); ++i) {
                        if (s.second[i] == '\\') {
                            printf("%d illegal character '\\\\'\n", s.first);
                        } else if (s.second[i] == '\'') {
                            printf("%d illegal character '\''\n", s.first);
                        } else {
                            printf("%d illegal character '%c'\n", s.first, s.second[i]);
                        }
                    }
                } else {
                    if (s.second == "\\") {
                        printf("%d illegal character '\\\\'\n", s.first);
                    } else if (s.second == "\'") {
                        printf("%d illegal character '\''\n", s.first);
                    } else {
                        printf("%d illegal character '%s'\n", s.first, s.second.c_str());
                    }
                }
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
                if (s.second.size() > 1) {
                    for (size_t i = 0; i < s.second.size(); ++i) {
                        if (s.second[i] == '\\') {
                            printf("%d illegal character '\\\\'\n", s.first);
                        } else if (s.second[i] == '\'') {
                            printf("%d illegal character '\''\n", s.first);
                        } else {
                            printf("%d illegal character '%c'\n", s.first, s.second[i]);
                        }
                    }
                } else {
                    if (s.second == "\\") {
                        printf("%d illegal character '\\\\'\n", s.first);
                    } else if (s.second == "\'") {
                        printf("%d illegal character '\''\n", s.first);
                    } else {
                        printf("%d illegal character '%s'\n", s.first, s.second.c_str());
                    }
                }
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