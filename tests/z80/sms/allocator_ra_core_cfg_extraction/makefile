CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_cfg_storage_test
	./core_cfg_storage_test > core_cfg_storage_test.log 2>&1
	$(BT) -s main.ssc
	for function in allocatorRaCoreCfgSink allocatorRaCoreCfgDiamond allocatorRaCoreCfgLoop allocatorRaCoreCfgBoundaries; do test "$$(grep -c "register_allocator_core: control_flow_storage function=$$function instructions=80 blocks=80 edges=160 instruction_bytes=[1-9][0-9]* block_bytes=[1-9][0-9]* edge_bytes=[1-9][0-9]* status=complete" compiler.log)" -eq 1 || exit 1; done
	! grep -q "register_allocator_core: control_flow_storage function=allocatorRaCoreCfg.*status=invalid_input" compiler.log
	test "$$(grep -c "register_allocator_core: block_build function=allocatorRaCoreCfgDiamond instructions=80 blocks=9 algorithm=leader_partition status=complete" compiler.log)" -eq 1
	test "$$(grep -c "register_allocator_core: block_build function=allocatorRaCoreCfgLoop instructions=80 blocks=5 algorithm=leader_partition status=complete" compiler.log)" -eq 1
	test "$$(grep -c "register_allocator_core: block_build function=allocatorRaCoreCfgBoundaries instructions=80 blocks=5 algorithm=leader_partition status=complete" compiler.log)" -eq 1
	test "$$(grep -c "register_allocator_core: cfg_build function=allocatorRaCoreCfgDiamond blocks=9 edges=8 algorithm=label_resolved status=complete" compiler.log)" -eq 1
	test "$$(grep -c "register_allocator_core: cfg_build function=allocatorRaCoreCfgLoop blocks=5 edges=3 algorithm=label_resolved status=complete" compiler.log)" -eq 1
	test "$$(grep -c "register_allocator_core: cfg_build function=allocatorRaCoreCfgBoundaries blocks=5 edges=0 algorithm=label_resolved status=complete" compiler.log)" -eq 1
	grep -q "join_path_reservation_commit function=allocatorRaCoreCfgDiamond temp=r0 .*reservations=4 .*committed=4 .*commit=committed status=complete" compiler.log
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCoreCfgDiamond temp=r0 .*phy=C status=complete" compiler.log)" -eq 3
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreCfgDiamond .*temp=r0 .*spill=exempt" compiler.log)" -eq 3
	grep -q "cfg_edge function=allocatorRaCoreCfgDiamond from_block=1 to_block=3 kind=branch_true" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreCfgDiamond from_block=1 to_block=2 kind=branch_false" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreCfgDiamond from_block=2 to_block=4 kind=jump" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreCfgDiamond from_block=3 to_block=4 kind=fallthrough" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreCfgLoop from_block=2 to_block=1 kind=jump" compiler.log
	grep -q "basic_block function=allocatorRaCoreCfgBoundaries block=0.*reason=inline_asm" compiler.log
	grep -q "basic_block function=allocatorRaCoreCfgBoundaries block=1.*reason=function_call" compiler.log
	grep -q "basic_block function=allocatorRaCoreCfgBoundaries block=2.*reason=inline_asm" compiler.log
	grep -q "basic_block function=allocatorRaCoreCfgBoundaries block=3.*reason=inline_asm" compiler.log
	grep -q "basic_block function=allocatorRaCoreCfgBoundaries block=4.*reason=return" compiler.log
	! grep -q "register_allocator_core: block_build.*status=capacity_exceeded" compiler.log
	! grep -q "register_allocator_core: cfg_build.*status=capacity_exceeded" compiler.log
	! grep -q "register_allocator_core: cfg_build.*status=missing_instruction" compiler.log
	! grep -q "register_allocator_core: cfg_build.*status=missing_target" compiler.log
	grep -q "control_flow_storage function=coreCfgStorageOne instructions=1 blocks=1 edges=2.*status=complete" core_cfg_storage_test.log
	grep -q "control_flow_storage function=coreCfgStorageFour instructions=4 blocks=4 edges=8.*status=complete" core_cfg_storage_test.log
	grep -q "control_flow_storage function=coreCfgStorageForty instructions=40 blocks=40 edges=80.*status=complete" core_cfg_storage_test.log
	grep -q "control_flow_storage function=coreCfgStorageLifecycle instructions=4 blocks=4 edges=8 instruction_bytes=.* block_bytes=.* edge_bytes=.*status=complete" core_cfg_storage_test.log
	grep -q "control_flow_storage function=coreCfgStorageMaximum instructions=1073741823 .*status=" core_cfg_storage_test.log
	test "$$(grep -c "control_flow_storage function=.*status=invalid_input" core_cfg_storage_test.log)" -ge 5
	grep -q "block_build function=coreCfgStorageLifecycle instructions=4 blocks=2.*status=complete" core_cfg_storage_test.log
	grep -q "cfg_build function=coreCfgStorageLifecycle blocks=2 edges=2.*status=complete" core_cfg_storage_test.log
	grep -q "control_flow_storage function=coreCfgPostMutation instructions=5 blocks=5 edges=10.*status=complete" core_cfg_storage_test.log
	grep -q "block_build function=coreCfgPostMutation instructions=5 blocks=2.*status=complete" core_cfg_storage_test.log
	grep -q "cfg_build function=coreCfgPostMutation blocks=2 edges=2.*status=complete" core_cfg_storage_test.log
	grep -q "post_mutation_reconstruction instructions=5 inserted=1 blocks=2 edges=2 shifted_jump=2 shifted_label=3 status=complete" core_cfg_storage_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


core_cfg_storage_test: core_cfg_storage_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_cfg_storage_test core_cfg_storage_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_cfg_storage_test core_cfg_storage_test.log
