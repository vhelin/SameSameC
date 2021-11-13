
CC = samesamecc
CFLAGS = -mz80 -rs 3 -o
LD = samesamecl
LDFLAGS = -lsms -ro 4 -v -o

SFILES = interrupts.ssc main.ssc bank_1.ssc bank_3.ssc
IFILES = 
OFILES = interrupts.asm main.asm bank_1.asm bank_3.asm


all: linked.sms


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc

interrupts.asm: interrupts.ssc
	$(CC) $(CFLAGS) interrupts.asm interrupts.ssc

bank_1.asm: bank_1.ssc
	$(CC) -b 1 -s 2 $(CFLAGS) bank_1.asm bank_1.ssc

bank_3.asm: bank_3.ssc
	$(CC) -b 3 -s 2 $(CFLAGS) bank_3.asm bank_3.ssc


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms
