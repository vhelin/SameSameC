
CC = gcc
CFLAGS = -c -g -O0 -ansi -pedantic -Wall -DNDEBUG
LD = gcc
LDFLAGS = -lm
WLAFLAGS = $(CFLAGS)

CFILES = main.c file.c printf.c arch_z80.c
HFILES = main.h file.h printf.h arch_z80.h
OFILES = main.o file.o printf.o arch_z80.o


all: $(OFILES) makefile
	$(LD) $(LDFLAGS) $(OFILES) -o samesamecl

main.o: main.c defines.h main.h makefile
	$(CC) $(CFLAGS) main.c

file.o: file.c defines.h file.h makefile
	$(CC) $(CFLAGS) file.c

printf.o: printf.c defines.h printf.h makefile
	$(CC) $(CFLAGS) printf.c

arch_z80.o: arch_z80.c defines.h arch_z80.h makefile
	$(CC) $(CFLAGS) arch_z80.c


$(OFILES): $(HFILES)


clean:
	rm *.o ; rm *~ ; rm samesamecl
