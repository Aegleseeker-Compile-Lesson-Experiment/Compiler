
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

TARGET = compiler

SRCS = src/main.cpp src/lexer.cpp src/parser.cpp src/semantic.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)

.PHONY: all clean
