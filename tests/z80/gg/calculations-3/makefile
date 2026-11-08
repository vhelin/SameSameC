ROM = linked.gg
SFILES = interrupts.ssc main.ssc bank_1.ssc bank_2.ssc

CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -ra -mz80 -rs 3 -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -ro 4 -v -o
OFILES = $(SFILES:.ssc=.asm)

all: $(ROM)

$(OFILES): makefile

$(ROM): $(OFILES) makefile
	$(LD) $(LDFLAGS) $(ROM) $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc

interrupts.asm: interrupts.ssc
	$(CC) $(CFLAGS) interrupts.asm interrupts.ssc

bank_%.asm: bank_%.ssc
	$(CC) -b $* -s 2 $(CFLAGS) $@ $<

clean:
	rm -f $(OFILES) *~ $(ROM) *.compiler.log

bank_1.asm: z80_maze_maps.ssc
