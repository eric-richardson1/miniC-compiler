LEX_FILE := parser/tokenizer
YACC_FILE := parser/parser

EXECUTABLE := compile
SOURCE := main.cpp

LIB_SOURCES := lex.yy.c y.tab.c ../common/ast.c parser/semantic_analysis.c ir_generator/ir_generator.c optimizer/optimizer.c
LIB_OBJECTS := $(LIB_SOURCES:.c=.o)
LIB_NAME := miniC-lib

CC := g++
CPP := clang++
LLVM_CFLAGS := `llvm-config-15 --cflags` -I /usr/include/llvm-c-15
LLVM_CPPFLAGS := `llvm-config-15 --cxxflags --ldflags --libs core`

TEST = ../../test/optimizer_tests/test1

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCE) $(LIB_NAME).a
	$(CPP) $(LLVM_CPPFLAGS) -o $@ $< -L. -l:$(LIB_NAME).a

$(LIB_NAME).a: $(LIB_OBJECTS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(LLVM_CFLAGS) -c -o $@ $<

y.tab.c: $(YACC_FILE).y
	yacc -d -v $<

lex.yy.c: $(LEX_FILE).l y.tab.c
	lex $<



clean:
	rm -f $(EXECUTABLE) $(LIB_NAME).a $(LIB_OBJECTS) lex.yy.c y.tab.c y.tab.h test.ll y.output
