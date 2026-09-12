CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaTargetTransparencyByte target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_policy function=allocatorRaTargetTransparencyWord target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_transparency function=allocatorRaTargetTransparencyByte.*op=ADD phy=A transparent=no" compiler.log
	grep -q "target_transparency function=allocatorRaTargetTransparencyByte.*op=ADD phy=C transparent=yes" compiler.log
	grep -q "target_transparency function=allocatorRaTargetTransparencyByte.*op=ADD phy=B transparent=yes" compiler.log
	grep -q "target_transparency function=allocatorRaTargetTransparencyWord.*op=DEAD phy=HL transparent=yes" compiler.log
	grep -q "target_transparency function=allocatorRaTargetTransparencyWord.*op=COMPLEMENT phy=HL transparent=no" compiler.log
	grep -q "target_transparency function=allocatorRaTargetTransparencyWord.*op=ADD phy=HL transparent=no" compiler.log
	grep -q "linear_scan alternate function=allocatorRaTargetTransparencyByte.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan alternate function=allocatorRaTargetTransparencyByte.*primary=A alternate=B reason=clobber_between" compiler.log
	grep -q "linear_scan skip function=allocatorRaTargetTransparencyWord.*reason=clobber_between" compiler.log
	grep -q "retained result in C" main.asm
	grep -q "retained result in B" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log