CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaArrayReadComputedBaseA.*producer_tac=.*GET_ADDRESS.*consumer_tac=.*ARRAY_READ arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadComputedBaseHl.*producer_tac=.*GET_ADDRESS.*consumer_tac=.*ARRAY_READ arg1" compiler.log
	grep -q "resolve function=allocatorRaArrayReadComputedBaseA operand=arg1 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaArrayReadComputedBaseHl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "array_read_base function=allocatorRaArrayReadComputedBaseA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_read_base function=allocatorRaArrayReadComputedBaseHl kind=PHY_HL memory_pointer_load=no" compiler.log
	! grep -q "reason=unmigrated_emitter.*ARRAY_READ" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in HL" main.asm
	grep -q "XOR A,L" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
