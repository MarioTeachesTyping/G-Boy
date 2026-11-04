# Fallback build for machines without CMake (e.g. plain MinGW). CMakeLists.txt is the primary build.
#   make          build gboy + gboy_tests into build/
#   make test     build and run the unit tests
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc -MMD -MP

BUILD    := build
EXE      := $(if $(filter Windows_NT,$(OS)),.exe,)

CORE_SRC  := $(wildcard src/core/*.cpp src/core/*/*.cpp)
CLI_SRC   := src/frontend/cli/main.cpp
TEST_SRC  := $(wildcard tests/*.cpp)

CORE_OBJ  := $(CORE_SRC:%.cpp=$(BUILD)/obj/%.o)
CLI_OBJ   := $(CLI_SRC:%.cpp=$(BUILD)/obj/%.o)
TEST_OBJ  := $(TEST_SRC:%.cpp=$(BUILD)/obj/%.o)

.PHONY: all test clean

all: $(BUILD)/gboy$(EXE) $(BUILD)/gboy_tests$(EXE)

$(BUILD)/gboy$(EXE): $(CLI_OBJ) $(CORE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/gboy_tests$(EXE): $(TEST_OBJ) $(CORE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

test: $(BUILD)/gboy_tests$(EXE)
	./$(BUILD)/gboy_tests$(EXE)

clean:
	rm -rf $(BUILD)

-include $(CORE_OBJ:.o=.d) $(CLI_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
