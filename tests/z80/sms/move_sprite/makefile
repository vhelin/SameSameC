
CC = samesamecc
CFLAGS = -mz80 -rs 3 -o
LD = samesamecl
LDFLAGS = -lsms -v -o

SFILES = interrupts.ssc main.ssc
IFILES = 
OFILES = interrupts.asm main.asm


all: linked.sms


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc

interrupts.asm: interrupts.ssc
	$(CC) $(CFLAGS) interrupts.asm interrupts.ssc


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms
