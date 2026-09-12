CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan alternate function=allocatorRaComplementResultC.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaComplementResultC.*8-bit in C.*producer_tac=.*COMPLEMENT.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "complement_8bit function=allocatorRaComplementResultC.*result_kind=PHY_C.*result_phy=C" compiler.log
	grep -q "function=allocatorRaComplementResultC emitter=complement_8bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-5 mode=frame_displacement folded_offset=-5 address_target=IX target=z80" compiler.log
	! grep -q "function=allocatorRaComplementResultC emitter=complement_8bit_result_address" compiler.log
	grep -q "linear_scan alternate function=allocatorRaComplementResultB.*primary=A alternate=B reason=clobber_between" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaComplementResultB.*8-bit in B.*producer_tac=.*COMPLEMENT.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "complement_8bit function=allocatorRaComplementResultB.*result_kind=PHY_B.*result_phy=B" compiler.log
	grep -q "function=allocatorRaComplementResultB emitter=complement_8bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-7 mode=frame_displacement folded_offset=-7 address_target=IX target=z80" compiler.log
	! grep -q "function=allocatorRaComplementResultB emitter=complement_8bit_result_address" compiler.log
	grep -q "linear_scan overlap_check function=allocatorRaComplementResultB.*candidate=C.*active_C=yes.*conflict=yes" compiler.log
	grep -q "linear_scan overlap_check function=allocatorRaComplementResultB.*candidate=B.*active_C=yes.*conflict=no" compiler.log
	grep -q "retained result in C" main.asm
	grep -q "retained result in B" main.asm
	grep -q "retained arg1 in C" main.asm
	grep -q "retained arg1 in B" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log