CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaGetAddressArrayIndexA.*producer_tac=.*ADD.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaGetAddressArrayIndexHl.*producer_tac=.*XOR.*consumer_tac=.*GET_ADDRESS_ARRAY.*arg2" compiler.log
	grep -q "resolve function=allocatorRaGetAddressArrayIndexA operand=arg2 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaGetAddressArrayIndexHl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "get_address_index function=allocatorRaGetAddressArrayIndexA kind=PHY_HL" compiler.log
	grep -q "get_address_index function=allocatorRaGetAddressArrayIndexHl kind=PHY_HL" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*GET_ADDRESS" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg2 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
