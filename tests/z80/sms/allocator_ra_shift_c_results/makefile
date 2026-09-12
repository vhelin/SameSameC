CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "shift_result function=allocatorRaShiftResultC op=SHIFT_LEFT kind=STACK_SPILL size=2 phy=NONE var_type=3 promoted_type=3" compiler.log
	grep -q "shift_result function=allocatorRaShiftResultC op=SHIFT_RIGHT kind=STACK_SPILL size=2 phy=NONE var_type=3 promoted_type=3" compiler.log
	grep -q "shift_result function=allocatorRaShiftResultB op=SHIFT_LEFT kind=STACK_SPILL size=2 phy=NONE var_type=3 promoted_type=3" compiler.log
	grep -q "shift_result function=allocatorRaShiftResultB op=SHIFT_RIGHT kind=STACK_SPILL size=2 phy=NONE var_type=3 promoted_type=3" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaShiftResultC.*16-bit in HL.*producer_tac=.*(ADD).*consumer_tac=.*(XOR arg2)" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaShiftResultC.*16-bit in HL.*producer_tac=.*(AND).*consumer_tac=.*(OR arg2)" compiler.log
	grep -q "linear_scan retain function=allocatorRaShiftResultB.*16-bit in HL.*producer_tac=.*(ADD).*consumer_tac=.*(XOR arg2)" compiler.log
	! grep -q "shift_physical_result function=allocatorRaShiftResult" compiler.log
	test "$$(grep -c "shift left HL by BC" main.asm)" -eq 2
	test "$$(grep -c "shift right HL by BC" main.asm)" -eq 2
	test "$$(grep -c "target_specialized_address_materialization.*emitter=shift_16bit_result_address.*kind=STACK_SPILL" compiler.log)" -eq 4
	test "$$(grep -c "target_specialized_address_materialization function=allocatorRaShiftResultC emitter=shift_16bit_result_address" compiler.log)" -eq 2
	test "$$(grep -c "target_specialized_address_materialization function=allocatorRaShiftResultB emitter=shift_16bit_result_address" compiler.log)" -eq 2
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
