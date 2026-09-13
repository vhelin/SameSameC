CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "struct_access_address function=allocatorRaSplitSpillArrayA root=g_allocator_split_spill_values access=array" compiler.log
	grep -q "struct_access_update function=allocatorRaSplitSpillArrayA op=increment.*address_uses=2" compiler.log
	grep -q "struct_access_update function=allocatorRaSplitSpillArrayHl op=decrement.*address_uses=2" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaSplitSpillArrayA.*phy=HL.*reason=multi_read_interval" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaSplitSpillArrayHl.*phy=HL.*reason=multi_read_interval" compiler.log
	grep -q "candidate_selection_prepare function=allocatorRaSplitSpillArrayA.*single_read=no phy=2 proceed=yes blocked=no preservation_queried=yes preservation_supported=yes preservation_phy=2 reason=multi_read status=complete" compiler.log
	grep -q "candidate_selection_prepare function=allocatorRaSplitSpillArrayHl.*single_read=no phy=2 proceed=yes blocked=no preservation_queried=yes preservation_supported=yes preservation_phy=2 reason=multi_read status=complete" compiler.log
	test `grep -c "candidate_selection_prepare function=allocatorRaSplitSpill.*preservation_queried=yes" compiler.log` -eq 2
	: && ! grep -q "candidate_selection_prepare function=allocatorRaSplitSpill.*status=invalid\|candidate_selection_prepare function=allocatorRaSplitSpill.*status=dependency_failed" compiler.log
	test `grep -c "candidate_selection_orchestrate function=allocatorRaSplitSpill.*proceed=yes.*order=prepare_finalize status=complete" compiler.log` -eq 14
	: && ! grep -q "candidate_selection_orchestrate function=allocatorRaSplitSpill.*status=invalid\|candidate_selection_orchestrate function=allocatorRaSplitSpill.*status=dependency_failed" compiler.log
	test `grep -c "candidate_transition_prepare function=allocatorRaSplitSpill.*order=slot_resolve_transition_plan status=complete" compiler.log` -eq 14
	: && ! grep -q "candidate_transition_prepare function=allocatorRaSplitSpill.*status=invalid\|candidate_transition_prepare function=allocatorRaSplitSpill.*status=dependency_failed" compiler.log
	test `grep -c "candidate_transition_execute function=allocatorRaSplitSpill.*order=prepare_apply status=complete" compiler.log` -eq 14
	: && ! grep -q "candidate_transition_execute function=allocatorRaSplitSpill.*status=invalid\|candidate_transition_execute function=allocatorRaSplitSpill.*status=dependency_failed" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaSplitSpillArrayA.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaSplitSpillArrayHl.*destination=existing_spill_slot" compiler.log
	grep -q "spill_insert_emit function=allocatorRaSplitSpillArrayA op=ARRAY_READ operand=arg1 phy=HL spill_offset=-8 bytes=2" compiler.log
	grep -q "spill_insert_emit function=allocatorRaSplitSpillArrayHl op=ARRAY_READ operand=arg1 phy=HL spill_offset=-9 bytes=2" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaSplitSpillArrayA.*operand=result.*phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaSplitSpillArrayHl.*operand=result.*phy=BC" compiler.log
	grep -q "reload_graph_preflight function=allocatorRaSplitSpillArrayA.*reloads=1 truncated=no mode=cfg_graph status=complete" compiler.log
	grep -q "reload_graph_preflight function=allocatorRaSplitSpillArrayHl.*reloads=1 truncated=no mode=cfg_graph status=complete" compiler.log
	test `grep -c "reload_chain_storage function=allocatorRaSplitSpill.*instruction_bytes=.* execution_bytes=.* status=complete" compiler.log` -eq 2
	test `grep -c "reload_mutation function=allocatorRaSplitSpill.*operand=0 action=insert.*apply=yes status=complete" compiler.log` -eq 2
	test `grep -c "reload_graph_transaction_stage function=allocatorRaSplitSpill.*apply=yes transaction=staged status=complete" compiler.log` -eq 2
	test `grep -c "reload_graph_transaction function=allocatorRaSplitSpill.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log` -eq 2
	test `grep -c "reload_transaction_apply function=allocatorRaSplitSpill.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log` -eq 2
	: && ! grep -q "reload_graph_transaction function=allocatorRaSplitSpill.*transaction=rejected" compiler.log
	grep -q "reload_emission function=allocatorRaSplitSpillArrayA requested=yes.*operand=0.*emit=yes status=complete" compiler.log
	grep -q "reload_emission function=allocatorRaSplitSpillArrayHl requested=yes.*operand=0.*emit=yes status=complete" compiler.log
	grep -q "reload_insert_emit function=allocatorRaSplitSpillArrayA op=ARRAY_WRITE operand=result source_offset=-8 bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaSplitSpillArrayHl op=ARRAY_WRITE operand=result source_offset=-9 bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaSplitSpillArrayA.*operand=result.*phy=BC operand_id=0" compiler.log
	grep -q "reload_insert_emit function=allocatorRaSplitSpillArrayHl.*operand=result.*phy=BC operand_id=0" compiler.log
	grep -q "array_read_base function=allocatorRaSplitSpillArrayA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_write_base function=allocatorRaSplitSpillArrayA kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "array_read_base function=allocatorRaSplitSpillArrayHl kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_write_base function=allocatorRaSplitSpillArrayHl kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "preserve retained arg1 in spill slot" main.asm
	grep -q "reload result from spill slot into BC" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log