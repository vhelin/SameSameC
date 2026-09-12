CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "migrated_emitter function=allocatorRaArrayWriteBaseStructA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWriteBaseStructHl.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteBaseStructA.*consumer_tac=.*ARRAY_WRITE.*result" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteBaseStructHl.*consumer_tac=.*ARRAY_WRITE.*result" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteBaseStructA operand=result kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteBaseStructHl operand=result kind=PHY_HL" compiler.log
	! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained result in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log