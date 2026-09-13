CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "get_address_array_pointer_base_temp function=allocatorRaGetAddressBcBaseHlIndexA" compiler.log
	grep -q "get_address_array_pointer_base_temp function=allocatorRaGetAddressBcBaseHlIndexHl" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressBcBaseHlIndexA.*producer_tac=.*ADD.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressBcBaseHlIndexHl.*producer_tac=.*XOR.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg2" compiler.log
	grep -q "linear_scan alternate function=allocatorRaGetAddressBcBaseHlIndexA.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan alternate function=allocatorRaGetAddressBcBaseHlIndexHl.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressBcBaseHlIndexA.*ASSIGNMENT.*GET_ADDRESS_ARRAY.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressBcBaseHlIndexHl.*ASSIGNMENT.*GET_ADDRESS_ARRAY.*arg1" compiler.log
	grep -q "resolve function=allocatorRaGetAddressBcBaseHlIndexA operand=arg1 kind=PHY_BC" compiler.log
	grep -q "resolve function=allocatorRaGetAddressBcBaseHlIndexHl operand=arg1 kind=PHY_BC" compiler.log
	grep -q "get_address_base function=allocatorRaGetAddressBcBaseHlIndexA kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "get_address_base function=allocatorRaGetAddressBcBaseHlIndexHl kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "get_address_index function=allocatorRaGetAddressBcBaseHlIndexA kind=PHY_HL" compiler.log
	grep -q "get_address_index function=allocatorRaGetAddressBcBaseHlIndexHl kind=PHY_HL" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
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