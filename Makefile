# Parse Bench -- pure C99, no external dependencies.
#
#   make            build the parsebench binary
#   make test       build and run the test suite
#   make run        build and launch the terminal UI
#   make clean      remove build output

# make predefines CC as "cc", which doesn't exist in a MinGW install, and a
# predefined value counts as "set" so ?= would not replace it. Only override
# when the value is make's own default -- an environment or command-line CC
# still wins.
ifeq ($(origin CC),default)
  CC = gcc
endif

CFLAGS  ?= -std=c99 -Wall -Wextra -O2
CPPFLAGS = -Isrc

BIN_DIR   = build
CORE_SRC  = src/token.c src/grammar.c src/parse_tree.c src/parser.c
APP_SRC   = $(CORE_SRC) src/term.c src/tui.c src/main.c
TEST_SRC  = $(CORE_SRC) tests/watchdog.c tests/test_main.c

# Windows executables need the .exe suffix; everything else does not.
ifeq ($(OS),Windows_NT)
  EXE = .exe
  RM_RF = cmd /c rmdir /s /q
else
  EXE =
  RM_RF = rm -rf
endif

APP  = $(BIN_DIR)/parsebench$(EXE)
TEST = $(BIN_DIR)/run_tests$(EXE)

.PHONY: all test run grammar clean

all: $(APP)

$(APP): $(APP_SRC) $(wildcard src/*.h) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(APP_SRC) -o $@

$(TEST): $(TEST_SRC) $(wildcard src/*.h) $(wildcard tests/*.h) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -Itests $(TEST_SRC) -o $@

$(BIN_DIR):
	-mkdir $(BIN_DIR)

test: $(TEST)
	@$(TEST)

run: $(APP)
	@$(APP)

grammar: $(APP)
	@$(APP) --grammar

clean:
	-$(RM_RF) $(BIN_DIR)
