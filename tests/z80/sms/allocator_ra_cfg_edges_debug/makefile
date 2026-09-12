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
	grep -q "cfg_edge function=allocatorRaCfgEdgesDebug.*kind=branch_true" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgEdgesDebug.*kind=branch_false" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgEdgesDebug.*kind=jump" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgEdgesDebug.*kind=fallthrough" compiler.log
	grep -q "liveness_iterations function=allocatorRaCfgEdgesDebug" compiler.log
	grep -q "liveness function=allocatorRaCfgEdgesDebug block=1 use={} def={r0} live_in={} live_out={r0}" compiler.log
	grep -q "liveness function=allocatorRaCfgEdgesDebug block=4 use={r0} def={} live_in={r0} live_out={}" compiler.log
	grep -q "join_reconcile function=allocatorRaCfgEdgesDebug block=4 predecessors=2 policy=stack_only" compiler.log
	grep -q "join_reconcile function=allocatorRaCfgEdgesDebug block=4 r0 live_in=yes state=stack action=reload_on_demand" compiler.log
	grep -q "join_retention_preflight function=allocatorRaCfgEdgesDebug block=4 temp=r0 .*candidate=C selected=C eligible=yes" compiler.log
	test `grep -c "join_path_reservation_commit_entry function=allocatorRaCfgEdgesDebug temp=r0" compiler.log` -eq 4
	grep -q "join_path_reservation_commit function=allocatorRaCfgEdgesDebug temp=r0 .*reservations=4 .*committed=4 .*commit=committed status=complete" compiler.log
	test `grep -c "join_assignment_apply_entry function=allocatorRaCfgEdgesDebug temp=r0 .*phy=C status=complete" compiler.log` -eq 3
	grep -q "join_assignment_apply function=allocatorRaCfgEdgesDebug temp=r0 .*assignments=3 .*applied=3 application=applied status=complete" compiler.log
	test `grep -c "block_exit_spill_decision function=allocatorRaCfgEdgesDebug .*temp=r0 .*spill=exempt" compiler.log` -eq 3
	grep -q "join_path_state_release function=allocatorRaCfgEdgesDebug capacity=9 count=4 bytes=180 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "LD  C,0" main.asm
	grep -q "LD  C,1" main.asm
	grep -q "LD  A,C" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
