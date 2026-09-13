CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "migrated_emitter function=allocatorRaIntervalReuseAfterSpillA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaIntervalReuseAfterSpillHl.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaIntervalReuseAfterSpillA.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaIntervalReuseAfterSpillHl.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained result in A" main.asm
	grep -q "retained arg1 in A" main.asm
	grep -q "retained arg2 in A" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	grep -q "retained arg2 in HL" main.asm
	grep -q "register 0 size 1 spill yes phy NONE" main.asm
	grep -q "register 0 size 2 spill yes phy NONE" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
