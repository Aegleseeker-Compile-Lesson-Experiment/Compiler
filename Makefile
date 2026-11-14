CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0
TARGET   := compiler
SRC      := src/main.cpp src/Tokenizer/tokenizer.cpp src/Analyser/analyser.cpp src/Analyser/init.cpp src/Analyser/lalr1.cpp src/Analyser/ll1.cpp src/Analyser/lr1.cpp src/Analyser/slr1.cpp src/Analyser/tools.cpp src/Sementiser/sementiser.cpp src/Sementiser/irgen.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)