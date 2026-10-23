CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	$(BT) -s main.ssc
	grep -q "migrated_emitter function=allocatorAllSpillMetadata.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain.*function=allocatorAllSpillMetadata.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "resolve function=allocatorAllSpillMetadata operand=arg2 kind=PHY_A" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "register 0 size 1 spill no phy A" main.asm
	grep -q "retained result in A" main.asm
	grep -q "retained arg2 in A" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log