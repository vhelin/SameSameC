CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	$(BT) -s main.ssc
	test `grep -c "control_flow_storage function=allocatorRaBlockExit.*status=complete" compiler.log` -eq 4
	test `grep -c "control_flow_storage function=allocatorRaBlockExit.*instruction_bytes=[1-9][0-9]*.*status=complete" compiler.log` -eq 4
	! grep -q "control_flow_storage function=allocatorRaBlockExit.*status=invalid_input" compiler.log
	grep -q "linear_scan retain function=allocatorRaBlockExitRetainA" compiler.log
	grep -q "linear_scan retain function=allocatorRaBlockExitRetainHl" compiler.log
	grep -q "join_path_reservation_commit function=allocatorRaBlockExitSpillA temp=r0 .*reservations=4 .*committed=4 .*commit=committed status=complete" compiler.log
	test `grep -c "join_path_reservation_commit_entry function=allocatorRaBlockExitSpillA temp=r0" compiler.log` -eq 4
	test `grep -c "join_assignment_apply_entry function=allocatorRaBlockExitSpillA temp=r0 .*phy=C status=complete" compiler.log` -eq 3
	grep -q "join_assignment_apply function=allocatorRaBlockExitSpillA temp=r0 .*assignments=3 .*applied=3 application=applied status=complete" compiler.log
	grep -q "join_retention_preflight function=allocatorRaBlockExitSpillA block=4 temp=r0 .*candidate=C selected=C eligible=yes" compiler.log
	test `grep -c "block_exit_spill_decision function=allocatorRaBlockExitSpillA .*spill=exempt" compiler.log` -eq 3
	test `grep -c "block_exit_spill_decision function=allocatorRaBlockExitSpillHl .*spill=required" compiler.log` -eq 3
	! grep -q "block_exit_spill_decision function=allocatorRaBlockExitRetain" compiler.log
	! grep -q "block_exit spill function=allocatorRaBlockExitSpillA" compiler.log
	grep -q "block_exit spill function=allocatorRaBlockExitSpillHl block=1 r0 end_tac=66(JUMP_EQ) reason=block_exit replaced=label live=63..74" compiler.log
	test `grep -c "block_exit spill function=allocatorRaBlockExitSpill.*replaced=label" compiler.log` -eq 1
	test `grep -c "join_path_state_scan function=allocatorRaBlockExit.*consumption=enabled status=complete" compiler.log` -eq 4
	grep -q "join_path_state_release function=allocatorRaBlockExitSpillA capacity=8 count=4 bytes=160 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "LD  C,0" main.asm
	grep -q "LD  C,1" main.asm
	grep -q "LD  A,C" main.asm
	grep -q "; spill: block_exit" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log