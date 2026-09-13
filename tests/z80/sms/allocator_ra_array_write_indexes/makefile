CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteIndexA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteIndexHl.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaZ80OutWriteIndexA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWritePromotedIndexHl.*16-bit in HL.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWriteIndexA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWriteIndexHl.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaZ80OutWriteIndexA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteIndexA operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteIndexHl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaZ80OutWriteIndexA operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaArrayWritePromotedIndexHl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "retained arg2 in A" main.asm
	grep -q "retained arg2 in HL" main.asm
	grep -q "sign extend 8-bit C -> 16-bit BC preserving A" main.asm
	grep -q "array_write_index function=allocatorRaArrayWritePromotedIndexHl.*kind=PHY_HL.*phy=HL.*var_type=4" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	: && ! grep -q "loop_stack_storage_adapter" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log