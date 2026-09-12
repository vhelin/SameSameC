CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyCallByte target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_policy function=allocatorRaPolicyCallWord target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "call_result_convention function=allocatorRaPolicyCallByte.*size=8 target=z80 phy=A" compiler.log
	grep -q "call_result_convention function=allocatorRaPolicyCallWord.*size=16 target=z80 phy=HL" compiler.log
	grep -q "call_result_convention function=allocatorRaPolicyCallAlternate.*size=8 target=z80 phy=A" compiler.log
	grep -q "linear_scan retain function=allocatorRaPolicyCallByte.*8-bit in A.*FUNCTION_CALL_USE_RETURN_VALUE.*XOR arg1" compiler.log
	grep -q "linear_scan retain function=allocatorRaPolicyCallWord.*16-bit in HL.*FUNCTION_CALL_USE_RETURN_VALUE.*XOR arg1" compiler.log
	grep -q "linear_scan alternate function=allocatorRaPolicyCallAlternate.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain function=allocatorRaPolicyCallAlternate.*8-bit in C.*FUNCTION_CALL_USE_RETURN_VALUE.*XOR arg1" compiler.log
	grep -q "function_call_return function=allocatorRaPolicyCallByte.*kind=PHY_A.*phy=A" compiler.log
	grep -q "function_call_return function=allocatorRaPolicyCallWord.*kind=PHY_HL.*phy=HL" compiler.log
	grep -q "function_call_return function=allocatorRaPolicyCallAlternate.*kind=PHY_C.*phy=C" compiler.log
	grep -q "retained call result in A" main.asm
	grep -q "retained call result in HL" main.asm
	grep -q "retained call result in C" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log