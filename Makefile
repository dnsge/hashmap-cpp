.PHONY: autograder release debug run clean 

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE := $(BIN_DIR)/test
SRC := $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/**/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

CXX      := clang++
CXXFLAGS := -std=c++17 -Werror -Wextra -pedantic -Wall -I./include
LDFLAGS  :=
LDLIBS   := -lbenchmark -lgtest

debug: CXXFLAGS += -g -DDEBUG
debug: $(EXE)

profile: CXXFLAGS += -O2 -g -DNDEBUG
profile: $(EXE)

release: CXXFLAGS += -O2 -DNDEBUG
release: $(EXE)

run: debug
	$(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)
	rm -rf *.dSYM
