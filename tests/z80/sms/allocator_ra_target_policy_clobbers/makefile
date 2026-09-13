CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyClobberArray target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_policy function=allocatorRaPolicyClobberCall target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_policy function=allocatorRaPolicyClobberComplement target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "z80_clobber function=allocatorRaPolicyClobberArray.*(ARRAY_READ).*may_clobber_bc=yes.*clobbers=A|BC|HL|IX|IY|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaPolicyClobberCall.*(FUNCTION_CALL_USE_RETURN_VALUE).*may_clobber_bc=yes.*clobbers=A|BC|DE|HL|IX|IY|SP|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaPolicyClobberComplement.*(COMPLEMENT).*may_clobber_bc=no.*clobbers=A|HL|IX|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaPolicyClobberComplement.*(RETURN_VALUE).*may_clobber_bc=no.*clobbers=A|HL|IX|SP|FLAGS" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaPolicyClobberArray.*8-bit in C.*ARRAY_READ.*XOR arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaPolicyClobberCall.*8-bit in C.*FUNCTION_CALL_USE_RETURN_VALUE.*XOR arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaPolicyClobberComplement.*8-bit in C.*COMPLEMENT.*XOR arg1" compiler.log
	grep -q "retained result in C" main.asm
	grep -q "retained arg1 in C" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log