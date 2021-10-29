
CC = samesamecc
CFLAGS = -mz80 -o
LD = samesamecl
LDFLAGS = -lsms -v -o

SFILES = main.ssc
IFILES = 
OFILES = main.asm


all: linked.rom


linked.rom: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.rom $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.rom
