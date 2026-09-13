CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaZ80InPortA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaZ80InPortHl.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "resolve function=allocatorRaZ80InPortA operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaZ80InPortHl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "z80_in_port function=allocatorRaZ80InPortA kind=PHY_A" compiler.log
	grep -q "z80_in_port function=allocatorRaZ80InPortHl kind=PHY_HL" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=z80_in_port_address" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg2 in A" main.asm
	grep -q "retained arg2 in HL" main.asm
	grep -q "LD  C,A" main.asm
	grep -q "LD  C,L" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log