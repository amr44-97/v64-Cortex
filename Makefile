CC = clang++
CFLAGS_FAST = -std=c++26 -fno-exceptions  -Wall -Werror -Wno-c99-designator -Wno-unused-variable  -Wno-format
CFLAGS_FAST_DEBUG = -Wall -std=c++26 -ggdb -fno-omit-frame-pointer -Wno-c99-designator -Wno-unused-variable -D _GLIBCXX_DEBUG
CFLAGS_DEBUG = -Wall -std=c++26 -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -D _GLIBCXX_DEBUG

CFLAGS = $(CFLAGS_FAST)

EXE = v64
OUTPUT_DIR = bin

CPP_FILES = src/main.cpp src/lexer.cpp src/parser.cpp src/ast.cpp 


$(EXE):
	$(CC) $(CFLAGS) -o $(OUTPUT_DIR)/$(EXE) $(CPP_FILES)

clean:
	rm bin/*
