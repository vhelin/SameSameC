CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_reload_chain_apply_test
	$(BT) -s main.ssc
	./core_reload_chain_apply_test > core_reload_chain_apply_test.log 2>&1
	test $$(grep -c "reload_chain_storage function=allocatorRaCoreReloadChainApply.*instruction_bytes=.* execution_bytes=.* status=complete" compiler.log) -eq 2
	grep -q "reload_chain_apply function=allocatorRaCoreReloadChainApplyByte.*discovered=1 capacity=.* truncated=no status=complete" compiler.log
	grep -q "reload_chain_apply function=allocatorRaCoreReloadChainApplyWord.*discovered=1 capacity=.* truncated=no status=complete" compiler.log
	test $$(grep -c "reload_graph_transaction_stage function=allocatorRaCoreReloadChainApply.*apply=yes transaction=staged status=complete" compiler.log) -eq 2
	test $$(grep -c "reload_graph_transaction function=allocatorRaCoreReloadChainApply.*reloads=1 processed=1 mutations=1 transaction=committed atomic=yes status=complete" compiler.log) -eq 2
	: && ! grep -q "reload_graph_transaction function=allocatorRaCoreReloadChainApply.*transaction=rejected" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadChainApplyByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadChainApplyWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "reload_emission function=allocatorRaCoreReloadChainApplyByte requested=yes.*bytes=2 emit=yes status=complete" compiler.log
	grep -q "reload_emission function=allocatorRaCoreReloadChainApplyWord requested=yes.*bytes=2 emit=yes status=complete" compiler.log
	grep -q "reload_insert_emit function=allocatorRaCoreReloadChainApplyByte.*bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaCoreReloadChainApplyWord.*bytes=2 phy=BC" compiler.log
	grep -q "reload_chain function=coreChainApplyThree.*count=3 capacity=4 truncated=no.*status=complete" core_reload_chain_apply_test.log
	test $$(grep -c "reload_mutation_apply function=coreChainApplyThree.*status=complete" core_reload_chain_apply_test.log) -eq 3
	test $$(grep -c "reload_execute function=coreChainApplyThree.*action=insert apply=yes order=action_prepare_apply status=complete" core_reload_chain_apply_test.log) -eq 3
	grep -q "reload_chain_execute function=coreChainApplyThree.*discovered=3 processed=3 applied=3.*order=discover_select_execute status=complete" core_reload_chain_apply_test.log
	grep -q "core_reload_chain_apply: function=coreChainApplyThree discovered=3 applied=3 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_action function=coreChainApplyRejectedMiddle.*reload_phy=-1 action=none reason=target_unsupported status=complete" core_reload_chain_apply_test.log
	grep -q "reload_execute function=coreChainApplyRejectedMiddle.*reload_phy=-1 action=none apply=no order=action_prepare_apply status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_execute function=coreChainApplyRejectedMiddle.*discovered=3 processed=3 applied=2.*order=discover_select_execute status=complete" core_reload_chain_apply_test.log
	test $$(grep -c "reload_mutation_apply function=coreChainApplyRejectedMiddle.*status=complete" core_reload_chain_apply_test.log) -eq 2
	grep -q "core_reload_chain_apply: function=coreChainApplyRejectedMiddle discovered=3 applied=2 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_mutation_apply function=coreChainApplyCallbackFail.*instruction=3.*status=callback_failed" core_reload_chain_apply_test.log
	grep -q "reload_execute function=coreChainApplyCallbackFail.*stage=application status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "reload_chain_execute function=coreChainApplyCallbackFail.*stage=execution status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "reload_execute function=coreReloadExecuteNoTarget.*action=none apply=no order=action_prepare_apply status=complete" core_reload_chain_apply_test.log
	grep -q "reload_execute function=coreReloadExecuteFailure.*stage=application status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "reload_execute function=coreReloadExecuteNoCallback.*stage=application status=dependency_failed" core_reload_chain_apply_test.log
	test $$(grep -c "reload_execute function=.*status=invalid_input" core_reload_chain_apply_test.log) -eq 6
	grep -q "reload_chain_execute function=coreChainNoSelector.*status=invalid_input" core_reload_chain_apply_test.log
	grep -q "reload_chain_execute function=coreChainNoTargetsNoCallback.*discovered=1 processed=1 applied=0.*status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_execute function=coreChainSupportedNoCallback.*stage=execution status=dependency_failed" core_reload_chain_apply_test.log
	test $$(grep -c "reload_chain_execute function=.*status=invalid_input" core_reload_chain_apply_test.log) -eq 6
	grep -q "reload_chain_storage function=coreReloadChainStorageOne capacity=1 instruction_bytes=.* execution_bytes=.* status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_storage function=coreReloadChainStorageFour capacity=4 instruction_bytes=.* execution_bytes=.* status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_storage function=coreReloadChainStorageMaximum capacity=2147483647 .*status=" core_reload_chain_apply_test.log
	test $$(grep -c "reload_chain_storage function=.*status=invalid_input" core_reload_chain_apply_test.log) -ge 4
	grep -q "reload_chain_execute function=coreReloadChainStorageLifecycle.*discovered=2 processed=2 applied=2.*order=discover_select_execute status=complete" core_reload_chain_apply_test.log
	grep -q "reload_scan_range function=coreRangeExecuteFallthrough start_block=0 end_block=2 start=0 end=7 extended=2 reason=function_end status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_execute function=coreRangeExecuteFallthrough.*start=2 end=7.*discovered=1 processed=1 applied=1.*status=complete" core_reload_chain_apply_test.log
	grep -q "reload_mutation_apply function=coreRangeExecuteFallthrough.*instruction=6.*phy=82 status=complete" core_reload_chain_apply_test.log
	grep -q "core_reload_range_apply: function=coreRangeExecuteFallthrough end_block=2 extended=2 reload=6 applied=1 status=complete" core_reload_chain_apply_test.log
	test $$(grep -c "reload_chain_select function=coreOperandChainMixed.*operand=[012].*status=complete" core_reload_chain_apply_test.log) -eq 3
	grep -q "reload_mutation_apply function=coreOperandChainMixed.*instruction=1.*operand=0 phy=90 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_mutation_apply function=coreOperandChainMixed.*instruction=3.*operand=1 phy=91 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_mutation_apply function=coreOperandChainMixed.*instruction=5.*operand=2 phy=92 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_execute function=coreOperandChainInvalid.*stage=operand status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "core_reload_operand_chain: mixed=3 invalid=1 operands=result,arg1,arg2 status=complete" core_reload_chain_apply_test.log
	test $$(grep -c "reload_chain_transaction_stage function=coreOperandChainAtomicCommit.*transaction=staged status=complete" core_reload_chain_apply_test.log) -eq 3
	grep -q "reload_chain_transaction function=coreOperandChainAtomicCommit.*mutations=3 transaction=committed atomic=yes status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain_transaction function=coreOperandChainAtomicReject.*mutations=0 transaction=rejected atomic=yes stage=transaction status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "core_reload_operand_chain_atomic: committed=3 rejected=3 partial=0 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_graph function=coreReloadGraphDiamond.*reloads=3 visited=4 stopped=0 truncated=no qualification=ready.*status=complete" core_reload_chain_apply_test.log
	grep -q "reload_graph_transaction function=coreReloadGraphDiamond.*reloads=3 processed=3 mutations=3 transaction=committed atomic=yes status=complete" core_reload_chain_apply_test.log
	grep -q "reload_graph_transaction function=coreReloadGraphAtomicReject.*mutations=0 transaction=rejected atomic=yes status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "reload_graph_transaction function=coreReloadGraphBackwardReject.*mutations=0 transaction=rejected atomic=yes status=dependency_failed" core_reload_chain_apply_test.log
	grep -q "reload_graph function=coreReloadGraphPathStop.*reloads=0 visited=4 stopped=1 truncated=no qualification=rejected.*status=complete" core_reload_chain_apply_test.log
	grep -q "reload_graph function=coreReloadGraphTruncated.*reloads=0.*truncated=yes qualification=rejected.*status=complete" core_reload_chain_apply_test.log
	grep -q "reload_graph function=coreReloadGraphInvalidBlock.*status=invalid_block" core_reload_chain_apply_test.log
	grep -q "reload_graph function=coreReloadGraphInvalidEdge.*status=invalid_edge" core_reload_chain_apply_test.log
	grep -q "core_reload_graph: diamond=3 cycle_deduplicated=yes path_stop=rejected truncated=rejected invalid=2 transaction_rejected=1 backward_rejected=1 partial=0 join=1 status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain function=coreChainApplyIneligible.*count=1.*stop=3 reason=read_ineligible status=complete" core_reload_chain_apply_test.log
	grep -q "reload_chain function=coreChainApplyReadWrite.*count=1.*stop=2 reason=redefined_after_read status=complete" core_reload_chain_apply_test.log
	grep -q "core_reload_chain_apply: function=coreChainApplyEmpty discovered=0 applied=0 status=complete" core_reload_chain_apply_test.log
	: && ! grep -q "reload_chain_apply function=allocatorRaCoreReloadChainApply.*status=out_of_memory" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_reload_chain_apply_test: core_reload_chain_apply_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_reload_chain_apply_test core_reload_chain_apply_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_reload_chain_apply_test core_reload_chain_apply_test.log
