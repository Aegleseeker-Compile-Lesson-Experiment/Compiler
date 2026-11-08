#include <cstdio>
int getint() {
    int x;
    scanf("%d", &x);
    return x;
}
void putint(int x) {
    printf("%d\n", x);
}
int dead_code(int limit, int input_seed, int input_state) {
    int x = input_seed;
    int state = input_state;
    int i = 0;
    while (i < limit) {
        x = (x * 17 + 23) % 4096;
        int unused1 = x * x + x * 13;
        int unused2 = unused1 / 7 + 91;
        if (0) {
            state = state + unused2;
        }
        int j = 0;
        while (j < 8) {
            int unused3 = x * 3 + j * j;
            j = j + 1;
        }
        state = (state * 5 + x + i % 128) % 8192;
        i = i + 1;
    }
    return state * 4096 + x;
}

int main() {
    int limit = getint();
    int input_seed = getint();
    int input_state = getint();
    if (limit < 1 || limit > 150000000 || input_seed < 0 || input_seed > 4095 || input_state < 0 || input_state > 8191) {
        putint(-1);
        return 1;
    }
    int result = dead_code(limit, input_seed, input_state);
    putint(result);
    return result % 256;
}
