CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyBoundaryVoidCall target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_policy function=allocatorRaPolicyBoundaryValueCall target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_call_boundary function=allocatorRaPolicyBoundaryVoidCall.*op=FUNCTION_CALL target=z80 spill_reason=function_call" compiler.log
	grep -q "target_call_boundary function=allocatorRaPolicyBoundaryValueCall.*op=FUNCTION_CALL_USE_RETURN_VALUE target=z80 spill_reason=function_call" compiler.log
	grep -q "target_call_boundary function=allocatorRaPolicyBoundaryPlainReturn.*op=RETURN target=z80 spill_reason=return" compiler.log
	grep -q "target_call_boundary function=allocatorRaPolicyBoundaryReturnValue.*op=RETURN_VALUE target=z80 spill_reason=return" compiler.log
	grep -q "spill function=allocatorRaPolicyBoundaryVoidCall.*reason=function_call.*(FUNCTION_CALL)" compiler.log
	grep -q "return_boundary_consumer function=allocatorRaPolicyBoundaryReturnValue.*(RETURN_VALUE) action=defer_to_candidate_policy" compiler.log
	grep -q "linear_scan retain function=allocatorRaPolicyBoundaryReturnValue.*16-bit in HL.*(ADD).*\(RETURN_VALUE arg1\)" compiler.log
	: && ! grep -q "spill function=allocatorRaPolicyBoundaryReturnValue.*reason=return.*(RETURN_VALUE)" compiler.log
	grep -q "call_result_convention function=allocatorRaPolicyBoundaryValueCall.*size=16 target=z80 phy=HL" compiler.log
	grep -q "linear_scan retain function=allocatorRaPolicyBoundaryValueCall.*16-bit in HL.*FUNCTION_CALL_USE_RETURN_VALUE.*XOR arg1" compiler.log
	grep -q "; spill: function_call" main.asm
	grep -q "; retained result in HL" main.asm
	grep -q "retained call result in HL" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log