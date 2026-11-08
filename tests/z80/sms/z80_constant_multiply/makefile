CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

OFILES = main.asm

all: linked.sms
	$(BT) -s main.ssc
	mv linked.sms default.sms
	$(CC) -mz80 -rs 3 -no-ra -o main.asm main.ssc
	$(LD) $(LDFLAGS) linked.sms $(OFILES)
	cmp default.sms linked.sms
	$(CC) -mz80 -rs 3 -ra-all-spill -o main.asm main.ssc
	$(LD) $(LDFLAGS) linked.sms $(OFILES)
	$(BT) -s main.ssc
	$(CC) -mz80 -rs 3 -ra -o main.asm main.ssc
	$(LD) $(LDFLAGS) linked.sms $(OFILES)
	$(BT) -s main.ssc

linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc

clean:
	rm -f $(OFILES) *~ linked.sms default.sms
