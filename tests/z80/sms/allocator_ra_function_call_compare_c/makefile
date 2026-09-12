CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "z80_clobber function=allocatorRaFunctionCallCompareC.*tac=.*(ASSIGNMENT).*may_clobber_bc=yes.*clobbers=A|BC|HL|IX|IY|FLAGS" compiler.log
	grep -q "function_call_return function=allocatorRaFunctionCallCompareC.*kind=STACK_SPILL.*phy=NONE.*var_type=4.*promoted_type=4" compiler.log
	grep -q "compare_8bit function=allocatorRaFunctionCallCompareC.*op_name=JUMP_EQ.*arg1_kind=STACK_SPILL.*arg2_kind=PHY_A" compiler.log
	grep -q "join_path_reservation_commit function=allocatorRaFunctionCallCompareC temp=r2 .*reservations=4 .*committed=4 .*commit=committed status=complete" compiler.log
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaFunctionCallCompareC temp=r2 .*phy=C status=complete" compiler.log)" -eq 3
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaFunctionCallCompareC .*temp=r2 .*spill=exempt" compiler.log)" -eq 3
	grep -q "compare_8bit function=allocatorRaFunctionCallCompareC.*op_name=JUMP_EQ.*arg1_kind=PHY_C.*arg2_kind=CONST" compiler.log
	! grep -q "retained call result in C" main.asm
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