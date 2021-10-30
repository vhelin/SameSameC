
CC = samesamecc
CFLAGS = -mz80 -rs 3 -o
LD = samesamecl
LDFLAGS = -lsms -v -o

SFILES = main.ssc
IFILES = 
OFILES = main.asm


all: linked.sms


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms
