CC = $(SAMESAMECVALGRIND) samesamecc
# Byte definitions cover empty and non-empty copies even without string support.
CFLAGS = -mz80 -D ALIAS=tokenDefinitionAlias -D 'TEXT="nop"' -D 'EMPTY=""'

.PHONY: all clean

all: main.asm
	: && ! $(CC) $(CFLAGS) -D OVERFLOW=1 -o overflow.asm main.ssc > overflow.log 2>&1
	grep -q "Out of stack space." overflow.log
	: && ! $(CC) $(CFLAGS) -D OVERFLOW=2 -o overflow.asm main.ssc > overflow.log 2>&1
	grep -q "Out of stack space." overflow.log

main.asm: main.ssc makefile
	$(CC) $(CFLAGS) -D OVERFLOW=0 -o main.asm main.ssc
	test -s main.asm
	grep -q "tokenDefinitionAlias:" main.asm

clean:
	rm -f main.asm overflow.asm overflow.log
