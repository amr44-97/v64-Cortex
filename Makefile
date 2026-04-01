CC = clang++
CFLAGS_FAST = -std=c++26 -O0 -fno-exceptions -Wall -Werror -Wno-c99-designator -Wno-unused-variable  -Wno-format
CFLAGS_FAST_DEBUG = -std=c++26 -O0 -ggdb -Wall -fno-omit-frame-pointer -Wno-c99-designator -Wno-unused-variable  -Wno-format -D _GLIBCXX_DEBUG
CFLAGS_DEBUG = -Wall -std=c++26 -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -D _GLIBCXX_DEBUG

CFLAGS = $(CFLAGS_FAST_DEBUG)

EXE = v64
OUTPUT_DIR = bin

CPP_FILES = src/main.cpp src/lexer.cpp src/parser.cpp  src/ast.cpp  src/pretty_print.cpp


$(EXE):
	$(CC) $(CFLAGS) -o $(OUTPUT_DIR)/$(EXE) $(CPP_FILES)

clean:
	rm bin/*
