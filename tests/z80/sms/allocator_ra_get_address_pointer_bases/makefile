CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "get_address_array_pointer_base_temp function=allocatorRaGetAddressPointerBaseA" compiler.log
	grep -q "get_address_array_pointer_base_temp function=allocatorRaGetAddressPointerBaseHl" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressPointerBaseA.*producer_tac=.*ADD.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressPointerBaseHl.*producer_tac=.*XOR.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg2" compiler.log
	grep -q "linear_scan alternate function=allocatorRaGetAddressPointerBaseA.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan alternate function=allocatorRaGetAddressPointerBaseHl.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressPointerBaseA.*ASSIGNMENT.*GET_ADDRESS_ARRAY.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressPointerBaseHl.*ASSIGNMENT.*GET_ADDRESS_ARRAY.*arg1" compiler.log
	grep -q "resolve function=allocatorRaGetAddressPointerBaseA operand=arg1 kind=PHY_BC" compiler.log
	grep -q "resolve function=allocatorRaGetAddressPointerBaseHl operand=arg1 kind=PHY_BC" compiler.log
	grep -q "get_address_base function=allocatorRaGetAddressPointerBaseA kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "get_address_base function=allocatorRaGetAddressPointerBaseHl kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "get_address_index function=allocatorRaGetAddressPointerBaseA kind=PHY_HL" compiler.log
	grep -q "get_address_index function=allocatorRaGetAddressPointerBaseHl kind=PHY_HL" compiler.log
	! grep -q "reason=unmigrated_emitter.*GET_ADDRESS" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in BC" main.asm
	grep -q "retained arg2 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
