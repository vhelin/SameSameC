CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra-all-spill -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	$(BT) -s main.ssc
	grep -q "forced_all_spill function=allocatorRaAllSpillMode temps=1" compiler.log
	grep -q "spill slot function=allocatorRaAllSpillMode r0 offset=-9 size=1" compiler.log
	grep -q "register 0 size 1 spill yes phy NONE" main.asm
	: && ! grep -q "retained result in A" main.asm
	: && ! grep -q "no spill slot function=allocatorRaAllSpillMode" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
