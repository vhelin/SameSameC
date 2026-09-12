CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	! grep -q "array_read_pointer_base_temp function=allocatorRaArrayReadPointerBaseA" compiler.log
	grep -q "array_read_pointer_base_temp function=allocatorRaArrayReadPointerBaseHl" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadPointerBaseA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadPointerBaseHl.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "linear_scan alternate function=allocatorRaArrayReadPointerBaseHl.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadPointerBaseHl.*ASSIGNMENT.*ARRAY_READ.*arg1" compiler.log
	grep -q "resolve function=allocatorRaArrayReadPointerBaseA operand=arg1 kind=STACK_LOCAL" compiler.log
	grep -q "resolve function=allocatorRaArrayReadPointerBaseHl operand=arg1 kind=PHY_BC" compiler.log
	grep -q "array_read_base function=allocatorRaArrayReadPointerBaseA kind=STACK_LOCAL memory_pointer_load=yes" compiler.log
	grep -q "array_read_base function=allocatorRaArrayReadPointerBaseHl kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "function=allocatorRaArrayReadPointerBaseA emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL offset=-6 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	! grep -q "function=allocatorRaArrayReadPointerBaseHl emitter=array_read_base_address" compiler.log
	grep -q "array_read_index function=allocatorRaArrayReadPointerBaseA kind=PHY_A" compiler.log
	grep -q "array_read_index function=allocatorRaArrayReadPointerBaseHl kind=PHY_HL" compiler.log
	! grep -q "reason=unmigrated_emitter.*ARRAY_READ" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in BC" main.asm
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
