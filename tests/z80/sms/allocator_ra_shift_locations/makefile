CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain function=allocatorRaShiftResultA.*producer_tac=.*SHIFT_LEFT" compiler.log
	grep -q "linear_scan retain function=allocatorRaShiftArg1A.*consumer_tac=.*SHIFT_LEFT arg1" compiler.log
	grep -q "linear_scan retain function=allocatorRaShiftResultHl.*producer_tac=.*SHIFT_LEFT" compiler.log
	grep -q "linear_scan retain function=allocatorRaShiftArg1Hl.*consumer_tac=.*SHIFT_LEFT arg1" compiler.log
	grep -q "retained result in A" main.asm
	grep -q "retained arg1 in A" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	test "$$(grep -c "target_specialized_address_materialization.*emitter=shift_.*result_address" compiler.log)" -eq 2
	grep -q "function=allocatorRaShiftArg1A emitter=shift_16bit_result_address" compiler.log
	grep -q "function=allocatorRaShiftArg1Hl emitter=shift_16bit_result_address" compiler.log
	! grep -q "function=allocatorRaShiftResultA emitter=shift_.*result_address" compiler.log
	! grep -q "function=allocatorRaShiftResultHl emitter=shift_.*result_address" compiler.log
	! grep -q "reason=unmigrated_emitter.*SHIFT" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log