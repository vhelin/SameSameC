CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "migrated_emitter function=allocatorRaArrayWritePointerValueRetained.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWritePointerValueStack.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWritePointerValueRetained.*consumer_tac=.*ARRAY_WRITE.*arg1" compiler.log
	grep -q "resolve function=allocatorRaArrayWritePointerValueRetained operand=arg1 kind=PHY_HL" compiler.log
	grep -q "array_write_value function=allocatorRaArrayWritePointerValueRetained kind=PHY_HL bytes=2" compiler.log
	grep -q "array_write_value function=allocatorRaArrayWritePointerValueStack kind=STACK_LOCAL bytes=2" compiler.log
	! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
