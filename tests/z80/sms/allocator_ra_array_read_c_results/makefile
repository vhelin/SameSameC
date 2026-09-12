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
	grep -q "candidate_evaluation function=allocatorRaArrayReadResultB block=1 reason=clobber_between candidate=0 phy=5 allowed=yes path_safe=yes conflict_free=no status=complete" compiler.log
	grep -q "candidate_evaluation function=allocatorRaArrayReadResultB block=1 reason=clobber_between candidate=1 phy=4 allowed=yes path_safe=yes conflict_free=yes status=complete" compiler.log
	grep -q "candidate_arbitration function=allocatorRaArrayReadResultB block=1 reason=clobber_between candidates=2 selected=1 phy=4 status=complete" compiler.log
	grep -q "linear_scan alternate function=allocatorRaArrayReadResultB block=1 r1 primary=A alternate=B reason=clobber_between producer_tac=.* consumer_tac=.*" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaArrayReadResultB block=1 r1 8-bit in B producer_tac=.*(ARRAY_READ) consumer_tac=.*(XOR arg1) spill_slot=kept reason=none boundary_tac=-1" compiler.log
	test "$$(grep -c "array_read_result function=allocatorRaArrayReadResultB" compiler.log)" -eq 2
	grep -q "array_read_result function=allocatorRaArrayReadResultB kind=PHY_C size=1 phy=C value_bytes=1" compiler.log
	grep -q "array_read_result function=allocatorRaArrayReadResultB kind=PHY_B size=1 phy=B value_bytes=1" compiler.log
	grep -q "array_read_physical_result function=allocatorRaArrayReadResultB result_phy=B value_bytes=1 via=L store=B status=complete" compiler.log
	! grep -q "array_read_result function=allocatorRaArrayReadResultB kind=STACK_SPILL" compiler.log
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "LD  C,L"
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "LD  B,L"
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "retained result in B"
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "retained arg1 in B"
	sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "XOR A,B"
	! sed -n '/allocatorRaArrayReadResultB:/,/\.ENDS/p' main.asm | grep -q "LD  (IX+0),L"
	grep -q "retained result in C" main.asm
	grep -q "retained arg1 in C" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log