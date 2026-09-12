CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaArrayReadIndexA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadIndexHl.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "resolve function=allocatorRaArrayReadIndexA operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaArrayReadIndexHl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "array_read_index function=allocatorRaArrayReadIndexA kind=PHY_A" compiler.log
	grep -q "array_read_index function=allocatorRaArrayReadIndexHl kind=PHY_HL" compiler.log
	! grep -q "reason=unmigrated_emitter.*ARRAY_READ" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg2 in A" main.asm
	grep -q "retained arg2 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
