
CC = gcc
CFLAGS = -c -g -O0 -ansi -pedantic -Wall -DNDEBUG
LD = gcc
LDFLAGS = -lm
WLAFLAGS = $(CFLAGS)

CFILES = main.c file.c
HFILES = main.h file.h
OFILES = main.o file.o


all: $(OFILES) makefile
	$(LD) $(LDFLAGS) $(OFILES) -o bilibalil

main.o: main.c defines.h main.h makefile
	$(CC) $(CFLAGS) main.c

file.o: file.c defines.h file.h makefile
	$(CC) $(CFLAGS) file.c


$(OFILES): $(HFILES)


clean:
	rm *.o ; rm *~ ; rm bilibalil
