CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy_validate function=allocatorRaTargetPolicyByte target=z80 hooks=23 status=complete" compiler.log
	grep -q "target_policy_validate function=allocatorRaTargetPolicyWord target=z80 hooks=23 status=complete" compiler.log
	! grep -q "target_policy_validate function=allocatorRaTargetPolicy.*status=invalid" compiler.log
	grep -q "target_policy function=allocatorRaTargetPolicyByte target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload,argument_transport,argument_byte_access,return_slot_access,call_frame_prefix,location_materialization,address_materialization overlap_model=A|HL|BC(B,C).*stack_return_model=frame_slot.*argument_model=contiguous_stack.*split_model=preserve_and_reload.*argument_transport_model=width_and_extension.*argument_byte_model=logical_offsets.*return_slot_model=byte_offsets.*call_frame_model=return_address_and_saved_fp.*location_model=semantic_source_to_target_kind.*address_model=target_register_location_and_cache_profile" compiler.log
	grep -q "target_policy function=allocatorRaTargetPolicyWord target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload,argument_transport,argument_byte_access,return_slot_access,call_frame_prefix,location_materialization,address_materialization overlap_model=A|HL|BC(B,C).*stack_return_model=frame_slot.*argument_model=contiguous_stack.*split_model=preserve_and_reload.*argument_transport_model=width_and_extension.*argument_byte_model=logical_offsets.*return_slot_model=byte_offsets.*call_frame_model=return_address_and_saved_fp.*location_model=semantic_source_to_target_kind.*address_model=target_register_location_and_cache_profile" compiler.log
	grep -q "overlap_check function=allocatorRaTargetPolicyByte.*candidate=B.*candidate_units=B.*active_C=yes.*conflict=no" compiler.log
	grep -q "physical_overlap function=allocatorRaTargetPolicyByte target=z80 left=4 left_units=2 right=5 right_units=4 overlap=no status=complete" compiler.log
	grep -q "alternate function=allocatorRaTargetPolicyByte.*primary=A alternate=B" compiler.log
	grep -q "resolve function=allocatorRaTargetPolicyByte operand=result kind=PHY_B" compiler.log
	grep -q "overlap_check function=allocatorRaTargetPolicyWord.*candidate=BC.*candidate_units=B|C.*active_HL=yes.*conflict=no" compiler.log
	grep -q "physical_overlap function=allocatorRaTargetPolicyWord target=z80 left=3 left_units=6 right=2 right_units=24 overlap=no status=complete" compiler.log
	grep -q "alternate function=allocatorRaTargetPolicyWord.*primary=HL alternate=BC" compiler.log
	grep -q "resolve function=allocatorRaTargetPolicyWord operand=result kind=PHY_BC" compiler.log
	grep -q "retained result in B" main.asm
	grep -q "retained result in BC" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log