CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicySplitByte target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_policy function=allocatorRaPolicySplitWord target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack.*split_model=preserve_and_reload" compiler.log
	grep -q "target_split_spill function=allocatorRaPolicySplitByte.*op=ARRAY_READ operand=arg1 size=16 target=z80 phy=HL preserve=yes" compiler.log
	grep -q "target_split_reload function=allocatorRaPolicySplitByte.*op=ARRAY_WRITE operand=result size=16 target=z80 phy=BC" compiler.log
	grep -q "target_split_spill function=allocatorRaPolicySplitWord.*op=ARRAY_READ operand=arg1 size=16 target=z80 phy=HL preserve=yes" compiler.log
	grep -q "target_split_reload function=allocatorRaPolicySplitWord.*op=ARRAY_WRITE operand=result size=16 target=z80 phy=BC" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaPolicySplitByte.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaPolicySplitByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaPolicySplitWord.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaPolicySplitWord.*source=existing_spill_slot phy=BC" compiler.log
	test `grep -c "reload_mutation function=allocatorRaPolicySplit.*operand=0 action=insert.*apply=yes status=complete" compiler.log` -eq 2
	grep -q "spill_insert_emit function=allocatorRaPolicySplitByte.*phy=HL.*bytes=2" compiler.log
	grep -q "reload_insert_emit function=allocatorRaPolicySplitByte.*bytes=2 phy=BC" compiler.log
	grep -q "spill_insert_emit function=allocatorRaPolicySplitWord.*phy=HL.*bytes=2" compiler.log
	grep -q "reload_emission function=allocatorRaPolicySplitByte requested=yes.*operand=0.*emit=yes status=complete" compiler.log
	grep -q "reload_emission function=allocatorRaPolicySplitWord requested=yes.*operand=0.*emit=yes status=complete" compiler.log
	grep -q "reload_insert_emit function=allocatorRaPolicySplitWord.*bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaPolicySplitByte.*operand=result.*phy=BC operand_id=0" compiler.log
	grep -q "reload_insert_emit function=allocatorRaPolicySplitWord.*operand=result.*phy=BC operand_id=0" compiler.log
	grep -q "reload_graph_preflight function=allocatorRaPolicySplitByte.*reloads=1 truncated=no mode=cfg_graph status=complete" compiler.log
	grep -q "reload_graph_preflight function=allocatorRaPolicySplitWord.*reloads=1 truncated=no mode=cfg_graph status=complete" compiler.log
	test `grep -c "reload_graph_transaction_stage function=allocatorRaPolicySplit.*operand=0 phy=3 apply=yes transaction=staged status=complete" compiler.log` -eq 2
	test `grep -c "reload_graph_transaction function=allocatorRaPolicySplit.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log` -eq 2
	test `grep -c "reload_transaction_apply function=allocatorRaPolicySplit.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log` -eq 2
	! grep -q "reload_graph_preflight function=allocatorRaPolicySplitNonzero" compiler.log
	! grep -q "target_split_.*function=allocatorRaPolicySplitNonzero" compiler.log
	grep -q "preserve retained arg1 in spill slot" main.asm
	grep -q "reload result from spill slot into BC" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log