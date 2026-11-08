CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteValueA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_WRITE.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteValueHl.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_WRITE.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaZ80OutWriteValueA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_WRITE.*arg1" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWriteValueA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWriteValueHl.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaZ80OutWriteValueA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteValueA operand=arg1 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteValueHl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaZ80OutWriteValueA operand=arg1 kind=PHY_A" compiler.log
	grep -q "retained arg1 in A" main.asm
	grep -q "retained arg1 in HL" main.asm
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log