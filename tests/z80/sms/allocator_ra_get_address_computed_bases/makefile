CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "assignment function=allocatorRaGetAddressComputedBase.*result_kind=PHY_HL result_phy=HL" compiler.log)" -eq 2
	test "$$(grep -c "function=allocatorRaGetAddressComputedBaseA emitter=assignment_result_address" compiler.log)" -eq 1
	test "$$(grep -c "function=allocatorRaGetAddressComputedBaseHl emitter=assignment_result_address" compiler.log)" -eq 1
	grep -q "get_address_array_pointer_base_temp function=allocatorRaGetAddressComputedBaseA" compiler.log
	grep -q "get_address_array_pointer_base_temp function=allocatorRaGetAddressComputedBaseHl" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressComputedBaseA.*producer_tac=.*ASSIGNMENT.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressComputedBaseHl.*producer_tac=.*ASSIGNMENT.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg1" compiler.log
	grep -q "resolve function=allocatorRaGetAddressComputedBaseA operand=arg1 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaGetAddressComputedBaseHl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "get_address_base function=allocatorRaGetAddressComputedBaseA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "get_address_base function=allocatorRaGetAddressComputedBaseHl kind=PHY_HL memory_pointer_load=no" compiler.log
	! grep -q "linear_scan retain.*function=allocatorRaGetAddressComputedBaseA.*ASSIGNMENT.*XOR" compiler.log
	! grep -q "linear_scan retain.*function=allocatorRaGetAddressComputedBaseHl.*ASSIGNMENT.*XOR" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log