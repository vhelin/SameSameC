CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaArrayReadResultC.*8-bit in C.*producer_tac=.*ARRAY_READ.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "array_read_result function=allocatorRaArrayReadResultC.*kind=PHY_C.*phy=C.*value_bytes=1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaZ80InReadResultC.*8-bit in C.*producer_tac=.*ARRAY_READ.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "z80_in_result function=allocatorRaZ80InReadResultC.*kind=PHY_C.*phy=C" compiler.log
	grep -q "allocation_verify function=allocatorRaArrayReadResultB .*status=complete" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaArrayReadResultB block=1 r1 8-bit in C producer_tac=.*(ARRAY_READ) consumer_tac=.*(XOR arg1) spill_slot=kept reason=none boundary_tac=-1" compiler.log
	test "$$(grep -c "array_read_result function=allocatorRaArrayReadResultB" compiler.log)" -eq 2
	grep -q "array_read_result function=allocatorRaArrayReadResultB kind=STACK_SPILL size=1 phy=NONE value_bytes=1" compiler.log
	grep -q "array_read_result function=allocatorRaArrayReadResultB kind=PHY_C size=1 phy=C value_bytes=1" compiler.log
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "LD  C,L"
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "LD  (IX-10),L"
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "XOR A,(IX-10)"
	: && ! sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "retained result in B"
	grep -q "retained result in C" main.asm
	grep -q "retained arg1 in C" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log