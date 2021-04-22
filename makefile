
CC = gcc
CFLAGS = -c -g -O0 -ansi -pedantic -Wall -DNDEBUG
LD = gcc
LDFLAGS = -lm
WLAFLAGS = $(CFLAGS)

CFILES = main.c parse.c include_file.c stack.c printf.c definitions.c pass_1.c pass_2.c pass_3.c pass_4.c pass_5.c tree_node.c symbol_table.c il.c tac.c
HFILES = main.h parse.h include_file.h stack.h printf.h definitions.h pass_1.h pass_2.h pass_3.h pass_4.h pass_5.h tree_node.h symbol_table.h il.h tac.h
OFILES = main.o parse.o include_file.o stack.o printf.o definitions.o pass_1.o pass_2.o pass_3.o pass_4.o pass_5.o tree_node.o symbol_table.o il.o tac.o


all: $(OFILES) makefile
	$(LD) $(LDFLAGS) $(OFILES) -o bilibali

main.o: main.c defines.h main.h makefile
	$(CC) $(CFLAGS) main.c

parse.o: parse.c defines.h parse.h makefile
	$(CC) $(CFLAGS) parse.c

include_file.o: include_file.c defines.h include_file.h makefile
	$(CC) $(CFLAGS) include_file.c

printf.o: printf.c defines.h printf.h makefile
	$(CC) $(CFLAGS) printf.c

definitions.o: definitions.c defines.h definitions.h makefile
	$(CC) $(CFLAGS) definitions.c

stack.o: stack.c defines.h stack.h makefile
	$(CC) $(CFLAGS) stack.c

tree_node.o: tree_node.c defines.h tree_node.h makefile
	$(CC) $(CFLAGS) tree_node.c

pass_1.o: pass_1.c defines.h pass_1.h makefile
	$(CC) $(CFLAGS) pass_1.c

pass_2.o: pass_2.c defines.h pass_2.h makefile
	$(CC) $(CFLAGS) pass_2.c

pass_3.o: pass_3.c defines.h pass_3.h makefile
	$(CC) $(CFLAGS) pass_3.c

pass_4.o: pass_4.c defines.h pass_4.h makefile
	$(CC) $(CFLAGS) pass_4.c

pass_5.o: pass_5.c defines.h pass_5.h makefile
	$(CC) $(CFLAGS) pass_5.c

il.o: il.c defines.h il.h makefile
	$(CC) $(CFLAGS) il.c

tac.o: tac.c defines.h tac.h makefile
	$(CC) $(CFLAGS) tac.c

symbol_table.o: symbol_table.c defines.h symbol_table.h makefile
	$(CC) $(CFLAGS) symbol_table.c

$(OFILES): $(HFILES)


clean:
	rm *.o ; rm *~ ; rm bilibali
