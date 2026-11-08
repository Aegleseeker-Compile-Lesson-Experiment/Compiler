CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0
TARGET   := compiler
SRC      := src/main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)