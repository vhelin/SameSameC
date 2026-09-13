CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "loop_register_selection function=allocatorRaShiftLeftLoop.*assignments=5.*selected=1 phy=5 selection=ready status=complete" compiler.log
	grep -q "loop_register_selection function=allocatorRaShiftRightLoop.*assignments=5.*selected=1 phy=5 selection=ready status=complete" compiler.log
	grep -q "loop_retention_transaction function=allocatorRaShiftLeftLoop.*assignments=5 reservations=4.*transaction=applied atomic=yes ownership=function_scan status=complete" compiler.log
	grep -q "loop_retention_transaction function=allocatorRaShiftRightLoop.*assignments=5 reservations=4.*transaction=applied atomic=yes ownership=function_scan status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaShiftLeftLoop.*selected=C slot=4 assignments=5 reservations=4 application=applied ownership=function_scan status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaShiftRightLoop.*selected=C slot=4 assignments=5 reservations=4 application=applied ownership=function_scan status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaShiftLeftLoop.*temp=r1.*spill=exempt ownership=function_scan status=complete" compiler.log)" -eq 3
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaShiftRightLoop.*temp=r1.*spill=exempt ownership=function_scan status=complete" compiler.log)" -eq 3
	grep -q "post_mutation_fixed_point function=allocatorRaShiftLeftLoop pass=1 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaShiftLeftLoop pass=2 inserted=0 rewritten=0 added_temps=0 rebuild=no status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaShiftRightLoop pass=1 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaShiftRightLoop pass=2 inserted=0 rewritten=0 added_temps=0 rebuild=no status=complete" compiler.log
	grep -q "shift_result function=allocatorRaShiftLeftLoop op=SHIFT_LEFT kind=PHY_C size=1 phy=C var_type=4 promoted_type=4" compiler.log
	grep -q "shift_result function=allocatorRaShiftRightLoop op=SHIFT_RIGHT kind=PHY_C size=1 phy=C var_type=4 promoted_type=4" compiler.log
	grep -q "shift_physical_result function=allocatorRaShiftLeftLoop op=SHIFT_LEFT result_phy=C value_register=L transfer=LD_C_L store=none status=complete" compiler.log
	grep -q "shift_physical_result function=allocatorRaShiftRightLoop op=SHIFT_RIGHT result_phy=C value_register=L transfer=LD_C_L store=none status=complete" compiler.log
	grep -q "loop_register_path_reservation_preflight function=allocatorRaShiftLoopConflict.*candidate=C.*transparent=no.*blocking=.*status=complete" compiler.log
	grep -q "loop_register_path_reservation_preflight function=allocatorRaShiftLoopConflict.*candidate=B.*transparent=no.*blocking=.*status=complete" compiler.log
	grep -q "loop_register_selection function=allocatorRaShiftLoopConflict.*selected=-1 phy=0.*selection=ineligible reason=no_path_safe_register status=complete" compiler.log
	grep -q "shift_result function=allocatorRaShiftLoopConflict op=SHIFT_LEFT kind=STACK_SPILL size=1 phy=NONE var_type=4 promoted_type=4" compiler.log
	sed -n '/allocatorRaShiftLeftLoop:/,/\.ENDS/p' main.asm | grep -q "ADD HL,HL"
	: && ! sed -n '/allocatorRaShiftLeftLoop:/,/\.ENDS/p' main.asm | grep -q "SRL H"
	sed -n '/allocatorRaShiftRightLoop:/,/\.ENDS/p' main.asm | grep -q "SRL H"
	: && ! sed -n '/allocatorRaShiftRightLoop:/,/\.ENDS/p' main.asm | grep -q "ADD HL,HL"
	test "$$(grep -c "LD  C,L" main.asm)" -eq 2
	test "$$(grep -c "retained result in C" main.asm)" -eq 2
	test "$$(grep -c "retained arg2 in C" main.asm)" -eq 2
	: && ! sed -n '/allocatorRaShiftLeftLoop:/,/\.ENDS/p' main.asm | grep -q "(IX-7)\|(IY-7)"
	: && ! sed -n '/allocatorRaShiftRightLoop:/,/\.ENDS/p' main.asm | grep -q "(IX-7)\|(IY-7)"
	sed -n '/allocatorRaShiftLoopConflict:/,/\.ENDS/p' main.asm | grep -q "LD  (IX-10),L"
	: && ! sed -n '/allocatorRaShiftLoopConflict:/,/\.ENDS/p' main.asm | grep -q "retained result in C"
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log