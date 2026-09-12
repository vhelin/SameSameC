CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain function=allocatorRaGetAddressResultHl.*producer_tac=.*GET_ADDRESS" compiler.log
	grep -q "linear_scan retain function=allocatorRaGetAddressArrayResultHl.*producer_tac=.*GET_ADDRESS_ARRAY" compiler.log
	grep -q "resolve function=allocatorRaGetAddressResultHl operand=result kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaGetAddressArrayResultHl operand=result kind=PHY_HL" compiler.log
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	! grep -Eq "reason=unmigrated_emitter.*GET_ADDRESS" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log