CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_liveness_storage_test tac_insert_test z80_register_spill_test
	./core_liveness_storage_test > core_liveness_storage_test.log 2>&1
	./tac_insert_test > tac_insert_test.log 2>&1
	$(BT) -s main.ssc
	grep -q "tac_storage: insert index=0 old_count=3 count=4 capacity=1024 moved=3 status=complete" tac_insert_test.log
	grep -q "tac_storage: insert index=2 old_count=4 count=5 capacity=1024 moved=2 status=complete" tac_insert_test.log
	grep -q "tac_storage: insert index=5 old_count=5 count=6 capacity=1024 moved=0 status=complete" tac_insert_test.log
	test "$$(grep -c "tac_storage: insert .*status=invalid_input" tac_insert_test.log)" -eq 2
	test "$$(grep -c "tac_storage: register_spill .*status=invalid_input" tac_insert_test.log)" -eq 7
	grep -q "tac_storage: register_spill temp=r2 phy=2 destination_offset=-6 bytes=2 status=complete" tac_insert_test.log
	grep -q "tac_storage: register_spill temp=r3 phy=1 destination_offset=-8 bytes=1 status=complete" tac_insert_test.log
	grep -q "; TAC: spill r2 phy 2 -> fp-6 bytes 2" tac_insert_test.log
	grep -q "; TAC: spill r3 phy 1 -> fp-8 bytes 1" tac_insert_test.log
	grep -q "tac_storage: insert index=1 old_count=1024 count=1025 capacity=2048 moved=1023 status=complete" tac_insert_test.log
	grep -q "tac_storage_test: ownership first=first second=second third=third count=1025 capacity=2048 status=complete" tac_insert_test.log
	./z80_register_spill_test > z80_register_spill_test.log 2>&1
	test "$$(grep -c "register_allocator_z80: register_spill_emit .*status=complete" z80_register_spill_test.log)" -eq 5
	test "$$(grep -c "register_allocator_z80: register_spill_emit .*status=invalid_input" z80_register_spill_test.log)" -eq 21
	grep -q "register_spill_emit function=z80SpillEmitter temp=r3 phy=2 destination_offset=-32768 bytes=2 stores=2 status=complete" z80_register_spill_test.log
	grep -q "register_spill_emit function=z80SpillEmitter temp=r4 phy=3 destination_offset=-9 bytes=2 stores=2 status=complete" z80_register_spill_test.log
	grep -q "z80_register_spill_test: assembly_end status=complete" z80_register_spill_test.log
	grep -q "z80_register_spill_test: invalid_cases=21 output_bytes=0 status=complete" z80_register_spill_test.log
	grep -q "reload_operand function=z80ReloadOperand op=ARRAY_WRITE operand_id=0 status=supported" z80_register_spill_test.log
	test "$$(grep -c "reload_operand function=z80ReloadOperand op=ARRAY_WRITE operand_id=[12] status=unsupported_operand" z80_register_spill_test.log)" -eq 2
	test "$$(grep -c "reload_operand .*status=invalid_input" z80_register_spill_test.log)" -eq 3
	grep -q "z80_register_spill_test: reload_operands supported=1 unsupported=2 invalid=3 status=complete" z80_register_spill_test.log
	grep -q "split_reload_endpoint op=.*operand=0 size=16 phy=3 status=supported" z80_register_spill_test.log
	test "$$(grep -c "split_reload_endpoint .*status=unsupported" z80_register_spill_test.log)" -eq 8
	test "$$(grep -c "split_reload_endpoint .*status=invalid_input" z80_register_spill_test.log)" -eq 4
	grep -q "z80_register_spill_test: split_reload_endpoints supported=1 unsupported=8 invalid=4 status=complete" z80_register_spill_test.log
	for function in allocatorRaCoreLivenessDiamond allocatorRaCoreLivenessLoop; do test "$$(grep -c "register_allocator_core: liveness_storage function=$$function .*status=complete" compiler.log)" -eq 2 || exit 1; done
	! grep -q "register_allocator_core: liveness_storage function=allocatorRaCoreLiveness.*status=invalid_input" compiler.log
	test "$$(grep -c "register_allocator_core: liveness_solve function=allocatorRaCoreLivenessDiamond blocks=9 edges=8 temps=1 iterations=2 algorithm=backward_fixed_point status=converged" compiler.log)" -eq 2
	test "$$(grep -c "register_allocator_core: liveness_solve function=allocatorRaCoreLivenessLoop blocks=5 edges=3 temps=1 iterations=1 algorithm=backward_fixed_point status=converged" compiler.log)" -eq 2
	grep -q "liveness_iterations function=allocatorRaCoreLivenessDiamond iterations=2" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreLivenessDiamond from_block=1 to_block=3 kind=branch_true" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreLivenessDiamond from_block=1 to_block=2 kind=branch_false" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreLivenessDiamond from_block=2 to_block=4 kind=jump" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreLivenessDiamond from_block=3 to_block=4 kind=fallthrough" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessDiamond block=1 use={} def={r0} live_in={} live_out={r0}" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessDiamond block=2 use={} def={} live_in={r0} live_out={r0}" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessDiamond block=3 use={} def={r0} live_in={} live_out={r0}" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessDiamond block=4 use={r0} def={} live_in={r0} live_out={}" compiler.log
	test "$$(grep -c "liveness_use_def function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 18
	test "$$(grep -c "liveness_use_def function=allocatorRaCoreLivenessLoop .*status=complete" compiler.log)" -eq 10
	grep -q "liveness_use_def function=allocatorRaCoreLivenessDiamond block=1 start=5 end=8 instructions=441 temps=1 uses=0 defs=1 order=read_before_write status=complete" compiler.log
	grep -q "liveness_use_def function=allocatorRaCoreLivenessDiamond block=4 start=13 end=16 instructions=441 temps=1 uses=1 defs=0 order=read_before_write status=complete" compiler.log
	! grep -q "liveness_use_def function=allocatorRaCoreLiveness.*status=invalid_input" compiler.log
	grep -q "join_reconcile function=allocatorRaCoreLivenessDiamond block=4 predecessors=2 policy=stack_only" compiler.log
	grep -q "join_reconcile function=allocatorRaCoreLivenessDiamond block=4 r0 live_in=yes state=stack action=reload_on_demand" compiler.log
	grep -q "join_temp_state function=allocatorRaCoreLivenessDiamond block=4 predecessors=2 temp=r0 spill=yes .*stack=yes state=stack status=complete" compiler.log
	grep -q "join_action_plan function=allocatorRaCoreLivenessDiamond block=4 predecessors=2 temp=r0 stack=yes action=reload_on_demand status=complete" compiler.log
	test "$$(grep -c "join_temp_state function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 2
	test "$$(grep -c "join_action_plan function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 2
	! grep -q "join_temp_state function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_action_plan function=allocatorRaCoreLivenessLoop " compiler.log
	grep -q "join_predecessors function=allocatorRaCoreLivenessDiamond block=4 blocks=9 edges=8 capacity=0 predecessors=2 mode=count status=complete" compiler.log
	test "$$(grep -c "join_predecessors function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 9
	test "$$(grep -c "join_predecessors function=allocatorRaCoreLivenessLoop .*status=complete" compiler.log)" -eq 5
	! grep -q "join_predecessors function=allocatorRaCoreLiveness.*status=invalid" compiler.log
	grep -q "join_spill_site function=allocatorRaCoreLivenessDiamond block=4 predecessor=2 edge=jump .*placement=before status=complete" compiler.log
	grep -q "join_spill_site function=allocatorRaCoreLivenessDiamond block=4 predecessor=3 edge=fallthrough .*placement=after status=complete" compiler.log
	grep -q "join_spill_sites function=allocatorRaCoreLivenessDiamond block=4 .*sites=2 mode=count status=complete" compiler.log
	test "$$(grep -c "join_spill_sites function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 11
	test "$$(grep -c "join_spill_sites function=allocatorRaCoreLivenessLoop .*status=complete" compiler.log)" -eq 5
	! grep -q "join_spill_site.*status=invalid" compiler.log
	! grep -q "join_spill_sites.*status=invalid" compiler.log
	grep -q "join_spill_order function=allocatorRaCoreLivenessDiamond block=4 site=0 predecessor=3 edge=fallthrough anchor=12 placement=after order=descending_anchor status=complete" compiler.log
	grep -q "join_spill_order function=allocatorRaCoreLivenessDiamond block=4 site=1 predecessor=2 edge=jump anchor=10 placement=before order=descending_anchor status=complete" compiler.log
	grep -q "join_spill_order function=allocatorRaCoreLivenessDiamond block=7 site=0 predecessor=6 edge=fallthrough anchor=22 placement=after order=descending_anchor status=complete" compiler.log
	grep -q "join_spill_order function=allocatorRaCoreLivenessDiamond block=7 site=1 predecessor=5 edge=jump anchor=20 placement=before order=descending_anchor status=complete" compiler.log
	test "$$(grep -c "join_spill_order function=allocatorRaCoreLivenessDiamond .*order=descending_anchor status=complete" compiler.log)" -eq 6
	grep -q "join_spill_emission_order function=allocatorRaCoreLivenessDiamond blocks=9 instructions=441 items=0 order=descending_anchor status=complete" compiler.log
	grep -q "join_spill_emission_order function=allocatorRaCoreLivenessLoop blocks=5 instructions=441 items=0 order=descending_anchor status=complete" compiler.log
	grep -q "post_mutation_rebuild function=allocatorRaCoreLivenessDiamond original_instructions=441 current_instructions=441 inserted=0 rewritten_operands=0 added_temps=0 rebuild=no status=complete" compiler.log
	grep -q "post_mutation_rebuild function=allocatorRaCoreLivenessLoop original_instructions=441 current_instructions=441 inserted=0 rewritten_operands=0 added_temps=0 rebuild=no status=complete" compiler.log
	grep -q "post_mutation_rebuild function=main original_instructions=441 current_instructions=441 inserted=0 rewritten_operands=0 added_temps=0 rebuild=no status=complete" compiler.log
	test "$$(grep -c "register_allocator: post_mutation_control_flow" compiler.log)" -eq 16
	grep -q "post_mutation_control_flow function=allocatorRaCoreLivenessLoopJoin instructions=441 blocks=4 edges=4 status=complete" compiler.log
	test "$$(grep -c "register_allocator: post_mutation_validation" compiler.log)" -eq 16
	! grep -q "post_mutation_control_flow function=allocatorRaCoreLivenessDiamond" compiler.log
	! grep -q "post_mutation_control_flow function=allocatorRaCoreLivenessLoop " compiler.log
	grep -q "post_mutation_rebuild function=corePostMutationUnchanged original_instructions=12 current_instructions=12 inserted=0 rewritten_operands=0 added_temps=0 rebuild=no status=complete" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationInserted original_instructions=12 current_instructions=15 inserted=3 rewritten_operands=0 added_temps=0 rebuild=yes status=complete" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationRewritten original_instructions=12 current_instructions=12 inserted=0 rewritten_operands=5 added_temps=0 rebuild=yes status=complete" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationAddedTemp original_instructions=12 current_instructions=12 inserted=0 rewritten_operands=0 added_temps=1 rebuild=yes status=complete" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationPromotion original_instructions=12 current_instructions=12 inserted=0 rewritten_operands=5 added_temps=1 rebuild=yes status=complete" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationNegativeRewrite .*rewritten_operands=-1 added_temps=0 rebuild=no status=invalid_input" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationNegativeTemp .*rewritten_operands=0 added_temps=-1 rebuild=no status=invalid_input" core_liveness_storage_test.log
	grep -q "post_mutation_rebuild function=corePostMutationTotalOverflow .*rewritten_operands=2147483647 added_temps=1 rebuild=no status=invalid_input" core_liveness_storage_test.log
	test "$$(grep -c "post_mutation_rebuild function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 11
	grep -q "fixed_point_limit function=coreFixedPointLimit instruction_capacity=12 limit=13 basis=initial_instructions status=complete" core_liveness_storage_test.log
	test "$$(grep -c "fixed_point_limit .*status=invalid_input" core_liveness_storage_test.log)" -eq 4
	grep -q "fixed_point_step function=coreFixedPointRebuild pass=1 limit=13 rebuild=yes action=rebuild status=complete" core_liveness_storage_test.log
	grep -q "fixed_point_step function=coreFixedPointStable pass=7 limit=13 rebuild=no action=stable status=complete" core_liveness_storage_test.log
	grep -q "fixed_point_step function=coreFixedPointStableAtLimit pass=13 limit=13 rebuild=no action=stable status=complete" core_liveness_storage_test.log
	grep -q "fixed_point_step function=coreFixedPointLimitReached pass=13 limit=13 rebuild=yes action=iteration_limit status=complete" core_liveness_storage_test.log
	test "$$(grep -c "fixed_point_step .*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	test "$$(grep -c "loop_stack_storage_rewrite function=coreLoopStackStorageCommit .*transaction=staged status=complete" core_liveness_storage_test.log)" -eq 5
	grep -q "loop_stack_storage_transaction function=coreLoopStackStorageCommit temp=r3 rewrites=5 temps=2 capacity=3 transaction=committed atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_storage_rewrite function=coreLoopStackStorageStale temp=r3 rewrite=4 .*expected_identity=5 actual_identity=6 transaction=rejected status=stale_operand" core_liveness_storage_test.log
	grep -q "loop_stack_storage_rewrite function=coreLoopStackStorageDuplicate temp=r3 rewrite=4 transaction=rejected status=duplicate_target" core_liveness_storage_test.log
	grep -q "loop_stack_storage_transaction function=coreLoopStackStorageConflict .*transaction=rejected atomic=yes status=temp_conflict" core_liveness_storage_test.log
	grep -q "loop_stack_storage_transaction function=coreLoopStackStorageOperands .*transaction=rejected atomic=yes status=invalid_storage" core_liveness_storage_test.log
	grep -q "loop_stack_storage_rewrite function=coreLoopStackStorageRewrite temp=r3 rewrite=4 transaction=rejected status=invalid_rewrite" core_liveness_storage_test.log
	test "$$(grep -c "loop_stack_storage_transaction function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 9
	grep -q "join_retention_path function=allocatorRaCoreLivenessDiamond block=4 temp=r0 path=0 predecessor=3 definition=12 anchor=12 phy=5 definition_found=yes definition_supported=yes path_transparent=yes status=complete" compiler.log
	grep -q "reaching_definition function=allocatorRaCoreLivenessDiamond predecessor=2 temp=r0 blocks=9 instructions=441 edges=8 definition=5 transparent=yes iterations=2 status=unique" compiler.log
	test "$$(grep -c "reaching_definition function=allocatorRaCoreLiveness" compiler.log)" -eq 10
	test "$$(grep -c "target_transparency function=allocatorRaCoreLivenessDiamond .*phy=C" compiler.log)" -eq 15
	grep -q "target_transparency function=allocatorRaCoreLivenessDiamond tac=8 op=JUMP_EQ phy=C transparent=yes" compiler.log
	grep -q "join_retention_path function=allocatorRaCoreLivenessDiamond block=4 temp=r0 path=1 predecessor=2 definition=5 anchor=10 phy=5 definition_found=yes definition_supported=yes path_transparent=yes status=complete" compiler.log
	grep -q "join_retention function=allocatorRaCoreLivenessDiamond block=4 temp=r0 blocks=9 instructions=441 paths=2 consumer_supported=yes selected=5 eligible=yes reason=all_paths_proven status=complete" compiler.log
	grep -q "join_retention_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 consumer=16 operand=1 candidate=C selected=C eligible=yes mode=observe_only status=complete" compiler.log
	grep -q "join_schedule function=allocatorRaCoreLivenessDiamond temp=r0 instructions=441 paths=2 consumer=16 operand=1 selected=5 assignments=3 capacity=3 conflict=-1 schedule=ready status=complete" compiler.log
	test "$$(grep -c "join_schedule_assignment function=allocatorRaCoreLivenessDiamond" compiler.log)" -eq 3
	grep -q "join_reservation function=allocatorRaCoreLivenessDiamond temp=r0 instructions=441 selected=5 slot=4 slots=5 assignments=3 occupied=0 start=5 end=16 conflict=none reservation=ready status=complete" compiler.log
	grep -q "join_reservation_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 selected=C slot=4 start=5 end=16 conflict=r-1 reservation=ready mode=observe_only status=complete" compiler.log
	test "$$(grep -c "join_reservation function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	test "$$(grep -c "join_reservation_preflight function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	grep -q "join_schedule_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 selected=C assignments=3 schedule=ready mode=observe_only status=complete" compiler.log
	test "$$(grep -c "join_schedule function=allocatorRaCoreLiveness" compiler.log)" -eq 36
	test "$$(grep -c "register_allocator_core: join_retention function=allocatorRaCoreLiveness.*eligible=yes" compiler.log)" -eq 1
	! grep -q "join_retention.*function=allocatorRaCoreLiveness.*status=invalid" compiler.log
	grep -q "join_retention function=coreJoinRetentionA .*selected=4 eligible=yes reason=all_paths_proven status=complete" core_liveness_storage_test.log
	grep -q "join_retention function=coreJoinRetentionHL .*selected=2 eligible=yes reason=all_paths_proven status=complete" core_liveness_storage_test.log
	grep -q "join_retention function=coreJoinRetentionMissing .*eligible=no reason=missing_definition status=complete" core_liveness_storage_test.log
	grep -q "join_retention function=coreJoinRetentionConsumer .*eligible=no reason=consumer_unsupported status=complete" core_liveness_storage_test.log
	grep -q "join_retention function=coreJoinRetentionDefinition .*eligible=no reason=definition_unsupported status=complete" core_liveness_storage_test.log
	grep -q "join_retention function=coreJoinRetentionClobber .*eligible=no reason=path_clobber status=complete" core_liveness_storage_test.log
	grep -q "join_retention function=coreJoinRetentionMismatch .*eligible=no reason=register_mismatch status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_retention function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 9
	test "$$(grep -Ec "join_retention_path .*status=(invalid_path|duplicate_predecessor)" core_liveness_storage_test.log)" -eq 10
	grep -q "reaching_definition function=coreReachingDefinitionUnique .*definition=2 transparent=yes iterations=2 status=unique" core_liveness_storage_test.log
	grep -q "reaching_definition function=coreReachingDefinitionClobber .*definition=2 transparent=no iterations=2 status=unique" core_liveness_storage_test.log
	grep -q "reaching_definition function=coreReachingDefinitionAmbiguous .*definition=-1 transparent=no iterations=2 status=ambiguous" core_liveness_storage_test.log
	grep -q "reaching_definition function=coreReachingDefinitionMissing .*definition=-1 transparent=no iterations=1 status=missing" core_liveness_storage_test.log
	grep -q "reaching_definition function=coreReachingDefinitionCycle .*definition=-1 transparent=no iterations=1 status=missing" core_liveness_storage_test.log
	test "$$(grep -c "reaching_definition function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 17
	! grep -q "reaching_definition function=coreReachingDefinition.*status=out_of_memory" core_liveness_storage_test.log
	grep -q "join_schedule function=coreJoinScheduleDistinct .*assignments=3 .*schedule=ready status=complete" core_liveness_storage_test.log
	grep -q "join_schedule function=coreJoinScheduleCommon .*assignments=2 .*schedule=ready status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_schedule_assignment function=coreJoinScheduleDistinct" core_liveness_storage_test.log)" -eq 3
	test "$$(grep -c "join_schedule_assignment function=coreJoinScheduleCommon" core_liveness_storage_test.log)" -eq 2
	grep -q "join_schedule function=coreJoinScheduleProducerConflict .*assignments=0 .*conflict=3 schedule=conflict status=complete" core_liveness_storage_test.log
	grep -q "join_schedule function=coreJoinScheduleConsumerConflict .*assignments=0 .*conflict=10 schedule=conflict status=complete" core_liveness_storage_test.log
	grep -q "join_schedule function=coreJoinScheduleIneligible .*assignments=0 .*schedule=ineligible status=complete" core_liveness_storage_test.log
	grep -q "join_schedule function=coreJoinScheduleIneligibleNoConsumer .*consumer=-1 operand=0 selected=-1 assignments=0 .*schedule=ineligible status=complete" core_liveness_storage_test.log
	grep -q "join_schedule function=coreJoinScheduleCapacity .*assignments=3 capacity=2 .*status=insufficient_capacity" core_liveness_storage_test.log
	test "$$(grep -c "join_schedule function=coreJoinSchedule.*status=invalid_input" core_liveness_storage_test.log)" -eq 10
	test "$$(grep -c "join_schedule function=.*status=invalid_path" core_liveness_storage_test.log)" -eq 2
	grep -q "loop_join function=allocatorRaCoreLivenessDiamond block=4 blocks=9 edges=8 entries=2 latches=0 entry=2 latch=-1 entry_edge=2 back_edge=-1 topology=ambiguous status=complete" compiler.log
	grep -q "loop_join_preflight function=allocatorRaCoreLivenessDiamond block=4 predecessors=2 entry=2 latch=-1 entry_edge=2 back_edge=-1 topology=ambiguous mode=observe_only status=complete" compiler.log
	test "$$(grep -c "loop_join function=allocatorRaCoreLivenessDiamond .*topology=ambiguous status=complete" compiler.log)" -eq 2
	test "$$(grep -c "loop_join_preflight function=allocatorRaCoreLivenessDiamond .*topology=ambiguous mode=observe_only status=complete" compiler.log)" -eq 2
	grep -q "loop_join function=coreLoopJoinReady block=1 blocks=4 edges=2 entries=1 latches=1 entry=0 latch=3 entry_edge=0 back_edge=1 topology=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_join function=coreLoopJoinForward .*topology=not_loop status=complete" core_liveness_storage_test.log
	grep -q "loop_join function=coreLoopJoinSelf .*topology=not_loop status=complete" core_liveness_storage_test.log
	grep -q "loop_join function=coreLoopJoinEntries .*entries=2 latches=1 .*topology=ambiguous status=complete" core_liveness_storage_test.log
	grep -q "loop_join function=coreLoopJoinLatches .*entries=1 latches=2 .*topology=ambiguous status=complete" core_liveness_storage_test.log
	grep -q "loop_flow_profile function=coreLoopFlowBreak .*entries=1 back_edges=1 exits=2 terminal_exits=1 topology=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_flow_profile function=coreLoopFlowEarlyReturn .*entries=1 back_edges=1 exits=2 terminal_exits=2 topology=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_flow_profile function=coreLoopFlowContinue .*entries=1 back_edges=2 exits=1 terminal_exits=1 topology=ambiguous status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_flow_profile function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	grep -q "loop_flow_profile_edge function=coreLoopFlowInvalidEdge .*status=invalid_edge" core_liveness_storage_test.log
	grep -q "loop_flow_profile_block function=coreLoopFlowInvalidBlock .*status=invalid_block" core_liveness_storage_test.log
	grep -q "loop_latch_selection function=coreLoopLatchUnique block=1 temp=r2 entries=1 back_edges=2 defining_latches=1 latch=3 definition=7 selection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_latch_selection function=coreLoopLatchMissing .*defining_latches=0 .*selection=ineligible status=complete" core_liveness_storage_test.log
	grep -q "loop_latch_selection function=coreLoopLatchAmbiguous .*defining_latches=2 .*selection=ambiguous status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_latch_selection function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	test "$$(grep -c "loop_latch_selection_callback function=.*status=invalid_callback" core_liveness_storage_test.log)" -eq 2
	grep -q "loop_latch_selection_edge function=coreLoopLatchEdge .*status=invalid_edge" core_liveness_storage_test.log
	grep -q "loop_latch_selection_block function=coreLoopLatchBlock .*status=invalid_block" core_liveness_storage_test.log
	test "$$(grep -c "loop_join function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	test "$$(grep -c "loop_join_edge function=.*status=invalid_edge" core_liveness_storage_test.log)" -eq 1
	grep -q "loop_join_candidate function=coreLoopCandidateReady block=1 blocks=2 temps=4 candidates=1 temp=r2 candidate=ready reason=unique_live_in status=complete" core_liveness_storage_test.log
	grep -q "loop_join_candidate function=coreLoopCandidateEmpty .*candidates=0 temp=r-1 candidate=ineligible reason=no_live_in status=complete" core_liveness_storage_test.log
	grep -q "loop_join_candidate function=coreLoopCandidateMultiple .*candidates=2 temp=r1 candidate=ready reason=first_live_in status=complete" core_liveness_storage_test.log
	grep -q "loop_join_candidate function=coreLoopCandidateTopology .*candidates=1 temp=r-1 candidate=ambiguous reason=topology status=complete" core_liveness_storage_test.log
	grep -q "loop_join_candidate function=coreLoopCandidateIneligible .*candidate=ineligible reason=topology status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_candidate function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 7
	test "$$(grep -c "loop_join_candidate function=.*status=invalid_topology" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "loop_join_candidate_value function=.*status=invalid_liveness" core_liveness_storage_test.log)" -eq 1
	grep -q "loop_representation function=coreLoopRepresentationTemp .*stack_values=2 representation=temp_ready status=complete" core_liveness_storage_test.log
	grep -q "loop_representation function=coreLoopRepresentationEmpty .*stack_values=0 representation=no_value status=complete" core_liveness_storage_test.log
	grep -q "loop_representation function=coreLoopRepresentationUnique .*stack_values=1 representation=unique_stack_value status=complete" core_liveness_storage_test.log
	grep -q "loop_representation function=coreLoopRepresentationMultiple .*stack_values=3 representation=ambiguous_stack_values status=complete" core_liveness_storage_test.log
	grep -q "loop_representation function=coreLoopRepresentationTopology .*representation=not_applicable status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_representation function=.*status=invalid_candidate" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "loop_representation function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	grep -q "loop_stack_value function=coreLoopStackValues block=1 value=0 identity=5 order=first_observation status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_value function=coreLoopStackValues block=1 value=1 identity=7 order=first_observation status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_value function=coreLoopStackValues block=1 value=2 identity=9 order=first_observation status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_values function=coreLoopStackValues block=1 observations=7 values=3 capacity=4 collection=complete order=first_observation status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_values function=coreLoopStackValuesCapacity .*values=3 capacity=2 collection=rejected status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_stack_value function=coreLoopStackValuesObservation .*observation=3 instruction=12 identity=99 status=invalid_observation" core_liveness_storage_test.log
	grep -q "loop_stack_values function=coreLoopStackValuesTopology .*values=0 .*collection=skipped reason=topology status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_values function=coreLoopStackValuesTopologyInvalid .*collection=rejected status=invalid_topology" core_liveness_storage_test.log
	grep -q "loop_stack_values_block function=coreLoopStackValuesBlock block=2 start=5 end=8 status=invalid_block" core_liveness_storage_test.log
	test "$$(grep -c "loop_stack_values function=.*collection=rejected status=invalid_input" core_liveness_storage_test.log)" -eq 4
	grep -q "loop_stack_selection function=coreLoopStackSelectReady .*candidates=1 identity=5 selection=ready required=31 status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_selection function=coreLoopStackSelectNone .*candidates=0 identity=-1 selection=ineligible required=31 status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_selection function=coreLoopStackSelectAmbiguous .*candidates=2 identity=5 selection=ready required=31 status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionFirst .*identity=5 size=8 proposed_temp=r3 existing_temps=3 plan=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_selection function=coreLoopStackSelectEmpty .*values=0 candidates=0 identity=-1 selection=ineligible required=31 status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_selection_value function=coreLoopStackSelectDuplicate .*status=duplicate_identity" core_liveness_storage_test.log
	test "$$(grep -c "loop_stack_selection_value function=.*status=invalid_value" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "loop_stack_selection function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	grep -q "loop_stack_promotion function=coreLoopStackPromotionReady8 .*identity=5 size=8 proposed_temp=r3 existing_temps=3 plan=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionReady16 .*identity=5 size=16 proposed_temp=r3 existing_temps=3 plan=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionIneligible .*plan=ineligible reason=selection status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionAmbiguous .*plan=ineligible reason=selection status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionSize .*size=32 .*plan=ineligible reason=unsupported_size status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionConflict .*proposed_temp=r2 .*plan=ineligible reason=temp_conflict status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion function=coreLoopStackPromotionSelection .*status=invalid_selection" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_temp function=coreLoopStackPromotionTemp .*status=invalid_temp" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_temp function=coreLoopStackPromotionDuplicate .*status=duplicate_temp" core_liveness_storage_test.log
	test "$$(grep -c "loop_stack_promotion function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 7
	test "$$(grep -c "loop_stack_rewrite function=coreLoopStackRewriteReady" core_liveness_storage_test.log)" -eq 5
	grep -q "loop_stack_rewrite function=coreLoopStackRewriteReady rewrite=0 instruction=2 operand=0 identity=5 temp=r3 role=1 order=observation status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite function=coreLoopStackRewriteReady rewrite=4 instruction=10 operand=1 identity=5 temp=r3 role=16 order=observation status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_schedule function=coreLoopStackRewriteReady .*rewrites=5 capacity=5 roles=31 schedule=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_schedule function=coreLoopStackRewriteCapacity .*rewrites=5 capacity=4 roles=31 .*status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_schedule function=coreLoopStackRewriteRoles .*rewrites=0 .*roles=15 schedule=ineligible reason=incomplete_roles status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_schedule function=coreLoopStackRewritePromotion .*schedule=ineligible reason=promotion status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_observation function=coreLoopStackRewriteDuplicate .*status=duplicate_target" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_observation function=coreLoopStackRewriteObservation .*status=invalid_observation" core_liveness_storage_test.log
	grep -q "loop_stack_rewrite_schedule function=coreLoopStackRewritePlan .*status=invalid_promotion" core_liveness_storage_test.log
	test "$$(grep -c "loop_stack_rewrite_schedule function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	grep -q "loop_stack_promotion_apply function=coreLoopStackApplyReady temp=r3 rewrites=5 application=applied atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply function=coreLoopStackApplyReject temp=r3 rewrites=0 application=failed atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply function=coreLoopStackApplyCallback .*application=ineligible status=invalid_callback" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply function=coreLoopStackApplyIneligible temp=r-1 rewrites=0 application=ineligible status=complete" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply_rewrite function=coreLoopStackApplyRewrite .*rewrite=4 status=invalid_rewrite" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply_rewrite function=coreLoopStackApplyDuplicate .*rewrite=4 status=duplicate_target" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply function=coreLoopStackApplyRoles .*application=ineligible roles=15 status=invalid_plan" core_liveness_storage_test.log
	grep -q "loop_stack_promotion_apply function=coreLoopStackApplyCapacity .*rewrites=6 application=ineligible status=invalid_plan" core_liveness_storage_test.log
	test "$$(grep -c "loop_stack_promotion_apply function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	grep -q "loop_join_fact_discovery function=coreLoopFactsReady block=1 temp=r2 entry=2 latch=9 consumer=4 operand=2 facts=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_join_fact_discovery function=coreLoopFactsInactive block=1 temp=r2 entry=1 latch=8 consumer=4 operand=2 facts=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_join_fact_discovery function=coreLoopFactsEntry .*facts=ineligible reason=missing_entry status=complete" core_liveness_storage_test.log
	grep -q "loop_join_fact_discovery function=coreLoopFactsLatch .*facts=ineligible reason=missing_latch status=complete" core_liveness_storage_test.log
	grep -q "loop_join_fact_discovery function=coreLoopFactsConsumer .*facts=ineligible reason=no_consumer status=complete" core_liveness_storage_test.log
	grep -q "loop_join_fact_discovery function=coreLoopFactsTopology .*facts=ineligible reason=topology status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_fact_discovery_callback function=.*status=invalid_callback" core_liveness_storage_test.log)" -eq 4
	test "$$(grep -c "loop_join_fact_discovery function=.*status=invalid_topology" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "loop_join_fact_discovery_block function=.*status=invalid_block" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "loop_join_fact_discovery function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 11
	grep -q "loop_join_definition function=coreLoopDefinitionReady block=1 temp=r2 live_in=yes entry=2 latch=9 consumer=4 operand=1 definition=ready reason=none status=complete" core_liveness_storage_test.log
	grep -q "loop_join_definition function=coreLoopDefinitionNotLive .*definition=ineligible reason=not_live_in status=complete" core_liveness_storage_test.log
	grep -q "loop_join_definition function=coreLoopDefinitionEntry .*definition=ineligible reason=missing_entry status=complete" core_liveness_storage_test.log
	grep -q "loop_join_definition function=coreLoopDefinitionLatch .*definition=ineligible reason=missing_latch status=complete" core_liveness_storage_test.log
	grep -q "loop_join_definition function=coreLoopDefinitionConsumer .*definition=ineligible reason=no_consumer status=complete" core_liveness_storage_test.log
	grep -q "loop_join_definition function=coreLoopDefinitionTopology .*definition=ineligible reason=topology status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_definition function=.*status=invalid_definition" core_liveness_storage_test.log)" -eq 4
	test "$$(grep -c "loop_join_definition function=.*status=invalid_topology" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "loop_join_definition_block function=.*status=invalid_block" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "loop_join_definition function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 7
	grep -q "loop_join_schedule function=coreLoopScheduleReady temp=r2 instructions=12 entry_definition=2 latch_definition=9 consumer=4 operand=1 selected=5 assignments=3 capacity=3 conflict=-1 schedule=ready order=entry_latch_consumer status=complete" core_liveness_storage_test.log
	grep -q "loop_join_schedule function=coreLoopScheduleUnassigned temp=r2 instructions=12 entry_definition=2 latch_definition=9 consumer=4 operand=1 selected=5 assignments=3 capacity=3 conflict=-1 schedule=ready order=entry_latch_consumer status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_schedule_assignment function=coreLoopScheduleReady" core_liveness_storage_test.log)" -eq 3
	grep -q "loop_join_schedule function=coreLoopScheduleEntryConflict .*conflict=2 schedule=conflict status=complete" core_liveness_storage_test.log
	grep -q "loop_join_schedule function=coreLoopScheduleLatchConflict .*conflict=9 schedule=conflict status=complete" core_liveness_storage_test.log
	grep -q "loop_join_schedule function=coreLoopScheduleConsumerConflict .*conflict=4 schedule=conflict status=complete" core_liveness_storage_test.log
	grep -q "loop_join_schedule function=coreLoopScheduleCapacity .*capacity=2 .*status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_join_schedule function=coreLoopScheduleIneligible .*schedule=ineligible status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_schedule function=.*status=invalid_loop" core_liveness_storage_test.log)" -eq 4
	test "$$(grep -c "loop_join_schedule function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	grep -q "loop_join function=allocatorRaCoreLivenessLoopJoin block=1 blocks=4 edges=4 entries=1 latches=1 entry=0 latch=2 entry_edge=0 back_edge=3 topology=ready status=complete" compiler.log
	grep -q "loop_join_preflight function=allocatorRaCoreLivenessLoopJoin block=1 predecessors=2 entry=0 latch=2 entry_edge=0 back_edge=3 topology=ready mode=observe_only status=complete" compiler.log
	grep -q "loop_join_candidate function=allocatorRaCoreLivenessLoopJoin block=1 blocks=4 temps=1 candidates=0 temp=r-1 candidate=ineligible reason=no_live_in status=complete" compiler.log
	grep -q "loop_join_candidate_preflight function=allocatorRaCoreLivenessLoopJoin block=1 candidates=0 temp=r-1 candidate=ineligible reason=no_live_in_temp mode=observe_only status=complete" compiler.log
	test "$$(grep -c "loop_stack_value_observation function=allocatorRaCoreLivenessLoopJoin " compiler.log)" -eq 21
	test "$$(grep -c "loop_stack_value_observation function=allocatorRaCoreLivenessLoopJoin .*operand=result" compiler.log)" -eq 10
	test "$$(grep -c "loop_stack_value_observation function=allocatorRaCoreLivenessLoopJoin .*operand=arg1" compiler.log)" -eq 10
	grep -q "loop_stack_value_observation function=allocatorRaCoreLivenessLoopJoin instruction=59 operand=arg2 identity=1 variable=value status=complete" compiler.log
	grep -q "loop_stack_value function=allocatorRaCoreLivenessLoopJoin block=1 value=0 identity=1 order=first_observation status=complete" compiler.log
	grep -q "loop_stack_value function=allocatorRaCoreLivenessLoopJoin block=1 value=1 identity=0 order=first_observation status=complete" compiler.log
	grep -q "loop_stack_values function=allocatorRaCoreLivenessLoopJoin block=1 observations=13 values=2 capacity=2 collection=complete order=first_observation status=complete" compiler.log
	grep -q "loop_representation function=allocatorRaCoreLivenessLoopJoin block=1 candidate=ineligible reason=2 stack_values=2 representation=ambiguous_stack_values status=complete" compiler.log
	grep -q "loop_stack_value_identity_preflight function=allocatorRaCoreLivenessLoopJoin block=1 value=0 identity=1 variable=value mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_value_identity_preflight function=allocatorRaCoreLivenessLoopJoin block=1 value=1 identity=0 variable=count mode=observe_only status=complete" compiler.log
	grep -q "loop_representation_preflight function=allocatorRaCoreLivenessLoopJoin block=1 observations=13 values=2 representation=ambiguous_stack_values mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_selection_value function=allocatorRaCoreLivenessLoopJoin block=1 value=0 identity=1 roles=31 required=31 candidate=yes status=complete" compiler.log
	grep -q "loop_stack_selection_value function=allocatorRaCoreLivenessLoopJoin block=1 value=1 identity=0 roles=14 required=31 candidate=no status=complete" compiler.log
	grep -q "loop_stack_selection function=allocatorRaCoreLivenessLoopJoin block=1 values=2 candidates=1 identity=1 selection=ready required=31 status=complete" compiler.log
	grep -q "loop_stack_value_roles_preflight function=allocatorRaCoreLivenessLoopJoin block=1 value=0 identity=1 variable=value roles=31 required=31 mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_value_roles_preflight function=allocatorRaCoreLivenessLoopJoin block=1 value=1 identity=0 variable=count roles=14 required=31 mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_selection_preflight function=allocatorRaCoreLivenessLoopJoin block=1 values=2 candidates=1 identity=1 variable=value selection=ready mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_promotion function=allocatorRaCoreLivenessLoopJoin block=1 identity=1 size=8 proposed_temp=r1 existing_temps=1 plan=ready reason=none status=complete" compiler.log
	grep -q "loop_stack_promotion_preflight function=allocatorRaCoreLivenessLoopJoin block=1 identity=1 variable=value size=8 proposed_temp=r1 existing_temps=1 plan=ready reason=none mode=observe_only status=complete" compiler.log
	test "$$(grep -c "loop_stack_rewrite function=allocatorRaCoreLivenessLoopJoin " compiler.log)" -eq 5
	grep -q "loop_stack_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=0 instruction=55 operand=0 identity=1 temp=r1 role=1 order=observation status=complete" compiler.log
	grep -q "loop_stack_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=1 instruction=59 operand=2 identity=1 temp=r1 role=2 order=observation status=complete" compiler.log
	grep -q "loop_stack_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=2 instruction=62 operand=1 identity=1 temp=r1 role=4 order=observation status=complete" compiler.log
	grep -q "loop_stack_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=3 instruction=62 operand=0 identity=1 temp=r1 role=8 order=observation status=complete" compiler.log
	grep -q "loop_stack_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=4 instruction=71 operand=1 identity=1 temp=r1 role=16 order=observation status=complete" compiler.log
	grep -q "loop_stack_rewrite_schedule function=allocatorRaCoreLivenessLoopJoin temp=r1 observations=8 rewrites=5 capacity=8 roles=31 schedule=ready reason=none status=complete" compiler.log
	test "$$(grep -c "loop_stack_rewrite_preflight function=allocatorRaCoreLivenessLoopJoin " compiler.log)" -eq 5
	grep -q "loop_stack_rewrite_schedule_preflight function=allocatorRaCoreLivenessLoopJoin block=1 observations=8 rewrites=5 roles=31 schedule=ready mode=observe_only status=complete" compiler.log
	test "$$(grep -c "loop_stack_storage_rewrite function=allocatorRaCoreLivenessLoopJoin .*transaction=staged status=complete" compiler.log)" -eq 5
	grep -q "loop_stack_storage_transaction function=allocatorRaCoreLivenessLoopJoin temp=r1 rewrites=5 temps=2 capacity=2 transaction=committed atomic=yes status=complete" compiler.log
	test "$$(grep -c "loop_stack_storage_adapter_rewrite function=allocatorRaCoreLivenessLoopJoin " compiler.log)" -eq 5
	grep -q "loop_stack_storage_adapter_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=0 instruction=55 operand=0 source_identity=1 staged_temp=r1 publication=real mode=mutation status=complete" compiler.log
	grep -q "loop_stack_storage_adapter_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=1 instruction=59 operand=2 source_identity=1 staged_temp=r1 publication=real mode=mutation status=complete" compiler.log
	grep -q "loop_stack_storage_adapter_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=2 instruction=62 operand=1 source_identity=1 staged_temp=r1 publication=real mode=mutation status=complete" compiler.log
	grep -q "loop_stack_storage_adapter_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=3 instruction=62 operand=0 source_identity=1 staged_temp=r1 publication=real mode=mutation status=complete" compiler.log
	grep -q "loop_stack_storage_adapter_rewrite function=allocatorRaCoreLivenessLoopJoin rewrite=4 instruction=71 operand=1 source_identity=1 staged_temp=r1 publication=real mode=mutation status=complete" compiler.log
	grep -q "loop_stack_storage_adapter function=allocatorRaCoreLivenessLoopJoin instructions=441 rewrites=5 real_temps=1 staged_temps=2 publication=real mode=mutation status=complete" compiler.log
	grep -q "loop_stack_promotion_handoff function=allocatorRaCoreLivenessLoopJoin block=1 rewritten=5 added_temps=1 analysis=stale next=rebuild status=complete" compiler.log
	grep -q "post_mutation_rebuild function=allocatorRaCoreLivenessLoopJoin original_instructions=441 current_instructions=441 inserted=0 rewritten_operands=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_validation function=allocatorRaCoreLivenessLoopJoin inserted=0 rewritten=5 added_temps=1 blocks=4 edges=4 liveness=recomputed status=complete" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessLoopJoin block=0 use={} def={r1} live_in={} live_out={r1}" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessLoopJoin block=1 use={r1} def={r0} live_in={r1} live_out={r1}" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessLoopJoin block=2 use={r1} def={r1} live_in={r1} live_out={r1}" compiler.log
	grep -q "liveness function=allocatorRaCoreLivenessLoopJoin block=3 use={r1} def={} live_in={r1} live_out={}" compiler.log
	! grep -q "block_exit_spill_decision function=allocatorRaCoreLivenessLoopJoin .*temp=r1 .*spill=required" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessLoopJoin .*temp=r1 .*spill=exempt" compiler.log)" -eq 3
	grep -q "spill slot function=allocatorRaCoreLivenessLoopJoin r1 offset=-7 size=1" compiler.log
	sed -n '/allocatorRaCoreLivenessLoopJoin:/,/\.ENDS/p' main.asm | grep -q "; register 1 size 1 spill yes phy NONE"
	! sed -n '/allocatorRaCoreLivenessLoopJoin:/,/\.ENDS/p' main.asm | grep -q "; spill: block_exit r1"
	grep -q "; TAC: r1.u8 (u8) := value.u8 (u8)" main.asm
	grep -q "; TAC: r1.u8 (u8) := r1.u8 (u8) + 1.u8 (u8)" main.asm
	grep -q "; TAC: return r1.u8 (u8)" main.asm
	sed -n '/allocatorRaCoreLivenessLoopJoin:/,/\.ENDS/p' main.asm | grep -q "LD  A,C"
	sed -n '/allocatorRaCoreLivenessLoopJoin:/,/\.ENDS/p' main.asm | grep -q "LD  L,C"
	! sed -n '/allocatorRaCoreLivenessLoopJoin:/,/\.ENDS/p' main.asm | grep -q "IX-7"
	! grep -q "loop_stack_storage_adapter .*publication=snapshot" compiler.log
	! grep -q "loop_stack_promotion_apply function=allocatorRaCoreLivenessLoopJoin" compiler.log
	! grep -q "loop_stack_rewrite_apply function=allocatorRaCoreLivenessLoopJoin" compiler.log
	test "$$(grep -c "loop_join_candidate function=allocatorRaCoreLiveness" compiler.log)" -eq 41
	test "$$(grep -c "loop_join_candidate_preflight function=allocatorRaCoreLiveness" compiler.log)" -eq 41
	test "$$(grep -c "loop_join_candidate_preflight function=allocatorRaCoreLiveness.*reason=multiple_entries" compiler.log)" -eq 3
	test "$$(grep -c "loop_join_fact_discovery_preflight function=allocatorRaCoreLiveness.*facts=skipped reason=multiple_entries" compiler.log)" -eq 3
	grep -q "loop_join_fact_discovery_preflight function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 entry=55 latch=62 consumer=59 operand=2 facts=ready mode=observe_only status=complete" compiler.log
	grep -q "loop_join_definition_preflight function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 entry=55 latch=62 consumer=59 operand=2 definition=ready reason=0 mode=observe_only status=complete" compiler.log
	test "$$(grep -c "loop_retention_plan_preflight function=allocatorRaCoreLiveness.*plan=skipped reason=multiple_entries" compiler.log)" -eq 3
	test "$$(grep -c "loop_retention_apply_preflight function=allocatorRaCoreLiveness.*application=skipped reason=multiple_entries atomic=yes" compiler.log)" -eq 3
	grep -q "loop_join_schedule function=allocatorRaCoreLivenessLoopJoin temp=r1 instructions=441 entry_definition=55 latch_definition=62 consumer=59 operand=2 selected=5 assignments=3 capacity=3 conflict=-1 schedule=ready order=entry_latch_consumer status=complete" compiler.log
	test "$$(grep -c "loop_join_schedule_assignment function=allocatorRaCoreLivenessLoopJoin " compiler.log)" -eq 3
	grep -q "loop_retention_supplemental_consumers function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 consumers=2 assignments=5 status=complete" compiler.log
	grep -q "loop_retention_plan_assignment function=allocatorRaCoreLivenessLoopJoin temp=r1 assignment=3 role=supplemental_consumer instruction=62 operand=1 phy=5 status=complete" compiler.log
	grep -q "loop_retention_plan_assignment function=allocatorRaCoreLivenessLoopJoin temp=r1 assignment=4 role=supplemental_consumer instruction=71 operand=1 phy=5 status=complete" compiler.log
	grep -q "loop_join_path_reservation_assignment function=allocatorRaCoreLivenessLoopJoin temp=r1 assignment=3 block=2 path_blocks=2 added_blocks=0 status=complete" compiler.log
	grep -q "loop_join_path_reservation_assignment function=allocatorRaCoreLivenessLoopJoin temp=r1 assignment=4 block=3 path_blocks=3 added_blocks=1 status=complete" compiler.log
	grep -q "loop_join_path_reservation function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 slot=4 blocks=4 instructions=441 edges=4 assignments=5 capacity=4 reservations=4 plan=ready order=stable_block status=complete" compiler.log
	test "$$(grep -c "loop_join_path_reservation_block function=allocatorRaCoreLivenessLoopJoin " compiler.log)" -eq 8
	grep -q "loop_join_path_reservation_block function=allocatorRaCoreLivenessLoopJoin temp=r1 reservation=3 block=3 start=69 end=71 slot=4 status=complete" compiler.log
	grep -q "loop_retention_plan function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 slot=4 assignments=5 reservations=4 plan=ready status=complete" compiler.log
	grep -q "loop_retention_coverage function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 live_start=55 live_end=71 assignments=5 uncovered=0 first_instruction=-1 first_operand=0 status=complete" compiler.log
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCoreLivenessLoopJoin temp=r1" compiler.log)" -eq 5
	grep -q "loop_retention_transaction function=allocatorRaCoreLivenessLoopJoin temp=r1 slot=4 assignments=5 reservations=4 state_count=4 transaction=applied atomic=yes ownership=function_scan status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 selected=C slot=4 assignments=5 reservations=4 application=applied ownership=function_scan status=complete" compiler.log
	grep -q "loop_latch_collection function=coreLoopLatchCollectionReady block=1 temp=r2 entries=1 back_edges=3 definitions=2 capacity=2 collection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_latch_collection function=coreLoopLatchCollectionCapacity block=1 temp=r2 entries=1 back_edges=2 definitions=2 capacity=1 status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_latch_collection_callback function=coreLoopLatchCollectionActive .*status=invalid_callback" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreMultiLatchRetentionReady block=1 temp=r2 slot=7 assignments=4 reservations=4 plan=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_apply function=coreMultiLatchRetentionApply temp=r2 slot=7 assignments=4 reservations=4 application=applied atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_assignment function=coreMultiLatchRetentionBackEdge temp=r2 assignment=2 block=3 status=missing_back_edge" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterPreferred block=1 temp=r2 preferred=5 candidates=3 assignments=3 bound=0 selected=1 phy=5 selection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_register_path_candidate function=coreLoopRegisterPaths block=1 temp=r2 candidate=1 phy=5 path_safe=no status=complete" core_liveness_storage_test.log
	grep -q "loop_register_path_evaluation function=coreLoopRegisterPaths block=1 temp=r2 candidates=3 capacity=3 safe=2 status=complete" core_liveness_storage_test.log
	grep -q "loop_register_path_candidate function=coreLoopRegisterPathsCallback block=1 temp=r2 candidate=2 phy=6 path_safe=no status=invalid_callback" core_liveness_storage_test.log
	test "$$(grep -c "loop_register_path_evaluation function=coreLoopRegisterPaths.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	grep -q "loop_register_exclusions function=coreLoopRegisterExclusions block=1 temp=r2 candidates=3 capacity=3 excluded=0 status=complete" core_liveness_storage_test.log
	grep -q "loop_register_exclusion function=coreLoopRegisterExclusions block=1 temp=r2 candidate=1 phy=5 reason=reservation blocking_block=3 blocking_instruction=11 status=complete" core_liveness_storage_test.log
	grep -q "loop_register_candidate function=coreLoopRegisterExclusionSelection block=1 temp=r2 candidate=1 phy=5 preferred=yes excluded=yes reason=reservation blocking_block=3 blocking_instruction=11 status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterExclusionSelection block=1 temp=r2 .*selected=0 phy=4 selection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterExclusionChain block=1 temp=r2 .*selected=2 phy=6 selection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterExclusionAll block=1 temp=r2 .*selection=ineligible reason=all_candidates_excluded status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_register_exclusion function=coreLoopRegisterExclusions.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	test "$$(grep -c "loop_register_exclusion_candidate function=coreLoopRegisterExclusion.*status=invalid_exclusion" core_liveness_storage_test.log)" -eq 2
	grep -q "loop_register_candidate function=coreLoopRegisterPinnedAlternate block=1 temp=r2 candidate=2 phy=6 preferred=no bound=3 compatible=yes path_safe=yes conflict_assignment=-1 status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterPinnedAlternate block=1 temp=r2 preferred=5 candidates=3 assignments=3 bound=3 selected=2 phy=6 selection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterConflict block=1 temp=r2 preferred=5 candidates=3 assignments=3 bound=3 selected=-1 phy=-1 conflict_assignment=0 selection=ineligible reason=no_common_register status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterUnsupported .*selection=ineligible reason=no_common_register status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterPathAlternate block=1 temp=r2 preferred=5 candidates=3 assignments=3 bound=0 selected=0 phy=4 selection=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_register_selection function=coreLoopRegisterPathBlocked .*selection=ineligible reason=no_path_safe_register status=complete" core_liveness_storage_test.log
	grep -q "loop_register_candidate function=coreLoopRegisterDuplicate .*duplicate=1 status=invalid_candidate" core_liveness_storage_test.log
	test "$$(grep -c "loop_register_assignment function=coreLoopRegister.*status=invalid_assignment" core_liveness_storage_test.log)" -eq 4
	test "$$(grep -c "loop_register_selection function=coreLoopRegister.*status=invalid_input" core_liveness_storage_test.log)" -eq 7
	grep -q "loop_latch_collection function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 entries=1 back_edges=2 definitions=2 capacity=6 collection=ready status=complete" compiler.log
	grep -q "loop_register_path_legality_preflight function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 candidate=A producer=402 consumer=406 operand=2 legal=no status=complete" compiler.log
	grep -q "loop_register_path_candidate function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 candidate=0 phy=1 path_safe=no status=complete" compiler.log
	grep -q "loop_register_path_candidate function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 candidate=1 phy=5 path_safe=yes status=complete" compiler.log
	grep -q "loop_register_path_legality_preflight function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 candidate=B producer=402 consumer=430 operand=1 legal=no status=complete" compiler.log
	grep -q "loop_register_path_evaluation function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 candidates=3 capacity=3 safe=1 status=complete" compiler.log
	grep -q "loop_register_candidate function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 candidate=1 phy=5 preferred=yes bound=0 compatible=yes path_safe=yes conflict_assignment=-1 status=complete" compiler.log
	grep -q "loop_register_selection function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 preferred=5 candidates=3 assignments=7 bound=0 selected=1 phy=5 selection=ready status=complete" compiler.log
	grep -q "loop_register_selection_preflight function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 preferred=C candidates=3 bound=0 selected=C candidate=1 attempt=1 action=retain mode=mutation status=complete" compiler.log
	test "$$(grep -c "loop_register_exclusions function=allocatorRaCoreLiveness" compiler.log)" -eq 17
	test "$$(grep -c "loop_register_selection_preflight function=allocatorRaCoreLiveness.*attempt=1" compiler.log)" -eq 17
	! grep -q "loop_register_selection_preflight function=allocatorRaCoreLiveness.*attempt=2" compiler.log
	! grep -q "loop_register_retry function=allocatorRaCoreLiveness" compiler.log
	grep -q "loop_register_path_legality_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r2 candidate=B producers_consumers=complete legal=yes status=complete" compiler.log
	grep -q "loop_register_path_legality_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 candidate=B producers_consumers=complete legal=yes status=complete" compiler.log
	grep -q "loop_register_selection_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 preferred=C candidates=3 bound=0 selected=B candidate=2 attempt=1 action=retain mode=mutation status=complete" compiler.log
	grep -q "loop_retention_plan_assignment function=allocatorRaCoreLivenessMultiLatchWriters temp=r1 assignment=2 role=supplemental_producer instruction=421 operand=0 phy=5 status=complete" compiler.log
	grep -q "loop_retention_plan function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 slot=4 assignments=7 reservations=6 plan=ready status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessMultiLatchWriters block=1 temp=r1 selected=C slot=4 assignments=7 reservations=6 application=applied ownership=function_scan status=complete" compiler.log
	grep -q "fixed_point_step_preflight function=allocatorRaCoreLivenessMultiLatchWriters pass=2 limit=442 rebuild=no action=stable status=complete" compiler.log
	test "$$(sed -n '/allocatorRaCoreLivenessMultiLatchWriters:/,/\.ENDS/p' main.asm | grep -c "LD  C,A")" -eq 3
	sed -n '/allocatorRaCoreLivenessMultiLatchWriters:/,/\.ENDS/p' main.asm | grep -q "LD  L,C"
	! sed -n '/allocatorRaCoreLivenessMultiLatchWriters:/,/\.ENDS/p' main.asm | grep -q "IX-6"
	grep -q "post_promotion_reconciliation function=allocatorRaCoreLivenessLoopJoin inserted=0 rewritten=0 added_temps=0 state_count=4 status=complete" compiler.log
	grep -q "loop_stack_promotion function=allocatorRaCoreLivenessLoopJoin16 block=1 identity=1 size=16 proposed_temp=r1 existing_temps=1 plan=ready reason=none status=complete" compiler.log
	grep -q "loop_join_candidate function=allocatorRaCoreLivenessLoopJoin16 block=1 blocks=4 temps=2 candidates=1 temp=r1 candidate=ready reason=unique_live_in status=complete" compiler.log
	grep -q "loop_register_path_reservation_preflight function=allocatorRaCoreLivenessLoopJoin16 block=1 temp=r1 candidate=HL reservation=2 path_block=2 start=82 end=88 transparent=no blocking=85 status=complete" compiler.log
	grep -q "loop_register_path_candidate function=allocatorRaCoreLivenessLoopJoin16 block=1 temp=r1 candidate=1 phy=3 path_safe=no status=complete" compiler.log
	grep -q "loop_register_path_evaluation function=allocatorRaCoreLivenessLoopJoin16 block=1 temp=r1 candidates=2 capacity=3 safe=0 status=complete" compiler.log
	grep -q "loop_register_selection function=allocatorRaCoreLivenessLoopJoin16 .*selection=ineligible reason=no_path_safe_register status=complete" compiler.log
	grep -q "loop_register_selection_preflight function=allocatorRaCoreLivenessLoopJoin16 block=1 temp=r1 preferred=HL candidates=2 bound=0 selected=NONE candidate=-1 attempt=1 action=preserve_stack mode=mutation status=complete" compiler.log
	! grep -q "loop_retention_transaction function=allocatorRaCoreLivenessLoopJoin16" compiler.log
	grep -q "loop_retention_supplemental_consumers function=allocatorRaCoreLivenessLoopJoin16 block=1 temp=r1 consumers=2 assignments=5 status=complete" compiler.log
	grep -q "loop_join_path_reservation function=allocatorRaCoreLivenessLoopJoin16 block=1 temp=r1 slot=1 blocks=4 instructions=441 edges=4 assignments=5 capacity=4 reservations=4 plan=ready order=stable_block status=complete" compiler.log
	! grep -q "loop_retention_coverage function=allocatorRaCoreLivenessLoopJoin16" compiler.log
	! grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessLoopJoin16" compiler.log
	! grep -q "block_exit_spill_decision function=allocatorRaCoreLivenessLoopJoin16 .*temp=r1 .*spill=exempt" compiler.log
	sed -n '/allocatorRaCoreLivenessLoopJoin16:/,/\.ENDS/p' main.asm | grep -q "ADD HL,BC"
	sed -n '/allocatorRaCoreLivenessLoopJoin16:/,/\.ENDS/p' main.asm | grep -q "INC HL"
	sed -n '/allocatorRaCoreLivenessLoopJoin16:/,/\.ENDS/p' main.asm | grep -q "LD  L,(IX-10)"
	sed -n '/allocatorRaCoreLivenessLoopJoin16:/,/\.ENDS/p' main.asm | grep -q "LD  (IX-10),L"
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessTwoLoops pass=1 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessTwoLoops pass=2 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessTwoLoops pass=3 inserted=0 rewritten=0 added_temps=0 rebuild=no status=complete" compiler.log
	test "$$(grep -c "post_promotion_reconciliation function=allocatorRaCoreLivenessTwoLoops .*status=additional_mutation" compiler.log)" -eq 2
	grep -q "post_promotion_reconciliation function=allocatorRaCoreLivenessTwoLoops inserted=0 rewritten=0 added_temps=0 state_count=8 status=complete" compiler.log
	grep -q "join_assignment_apply_transaction function=allocatorRaCoreLivenessTwoLoops temp=r1 assignments=5 replayed=5 status=complete" compiler.log
	grep -q "loop_join_path_reservation_block function=allocatorRaCoreLivenessTwoLoops temp=r1 reservation=3 block=3 start=111 end=113 slot=4 status=complete" compiler.log
	grep -q "loop_join_path_reservation_block function=allocatorRaCoreLivenessTwoLoops temp=r2 reservation=0 block=3 start=114 end=114 slot=4 status=complete" compiler.log
	grep -q "loop_retention_transaction function=allocatorRaCoreLivenessTwoLoops temp=r2 slot=4 assignments=5 reservations=4 state_count=8 transaction=applied atomic=yes ownership=function_scan status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessTwoLoops .*temp=r1 .*spill=exempt" compiler.log)" -eq 3
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessTwoLoops .*temp=r2 .*spill=exempt" compiler.log)" -eq 3
	! grep -q "block_exit_spill_decision function=allocatorRaCoreLivenessTwoLoops .*temp=r[12] .*spill=required" compiler.log
	test "$$(sed -n '/allocatorRaCoreLivenessTwoLoops:/,/\.ENDS/p' main.asm | grep -c "retained arg2 in C")" -eq 2
	! sed -n '/allocatorRaCoreLivenessTwoLoops:/,/\.ENDS/p' main.asm | grep -Eq "IX-10|IX-11"
	test "$$(grep -c "loop_stack_promotion_fallback function=allocatorRaCoreLivenessLoopConflict block=1 values=2 candidates=0 identity=-1 selection=ineligible reason=selection action=preserve_stack status=complete" compiler.log)" -eq 2
	! grep -q "loop_stack_promotion_handoff function=allocatorRaCoreLivenessLoopConflict block=1" compiler.log
	grep -q "loop_stack_promotion_handoff function=allocatorRaCoreLivenessLoopConflict block=4 rewritten=5 added_temps=1 analysis=stale next=rebuild status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessLoopConflict block=4 temp=r1 selected=C slot=4 assignments=5 reservations=4 application=applied ownership=function_scan status=complete" compiler.log
	grep -q "post_promotion_reconciliation function=allocatorRaCoreLivenessLoopConflict inserted=0 rewritten=0 added_temps=0 state_count=4 status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessLoopConflict .*temp=r1 .*spill=exempt" compiler.log)" -eq 3
	sed -n '/allocatorRaCoreLivenessLoopConflict:/,/\.ENDS/p' main.asm | grep -q "ADD A,(IX-6)"
	sed -n '/allocatorRaCoreLivenessLoopConflict:/,/\.ENDS/p' main.asm | grep -q "retained arg2 in C"
	! sed -n '/allocatorRaCoreLivenessLoopConflict:/,/\.ENDS/p' main.asm | grep -q "IX-10"
	grep -q "loop_stack_selection_deferred function=allocatorRaCoreLivenessMultiValue block=1 candidates=2 selected_identity=1 deferred=1 action=preserve_stack order=first_observation status=complete" compiler.log
	grep -q "loop_stack_promotion function=allocatorRaCoreLivenessMultiValue block=1 identity=1 size=8 proposed_temp=r2 existing_temps=2 plan=ready reason=none status=complete" compiler.log
	grep -q "fixed_point_budget function=allocatorRaCoreLivenessMultiValue instruction_capacity=441 limit=442 basis=initial_instructions status=complete" compiler.log
	grep -q "fixed_point_step_preflight function=allocatorRaCoreLivenessMultiValue pass=1 limit=442 rebuild=yes action=rebuild status=complete" compiler.log
	grep -q "fixed_point_step_preflight function=allocatorRaCoreLivenessMultiValue pass=2 limit=442 rebuild=yes action=rebuild status=complete" compiler.log
	grep -q "fixed_point_step_preflight function=allocatorRaCoreLivenessMultiValue pass=3 limit=442 rebuild=no action=stable status=complete" compiler.log
	test "$$(grep -c "register_allocator: fixed_point_budget function=" compiler.log)" -eq 15
	test "$$(grep -c "register_allocator: fixed_point_step_preflight function=" compiler.log)" -eq 31
	! grep -q "register_allocator: post_mutation_fixed_point .*status=iteration_limit" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessMultiValue pass=1 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessMultiValue pass=2 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessMultiValue pass=3 inserted=0 rewritten=0 added_temps=0 rebuild=no status=complete" compiler.log
	grep -q "loop_retention_conflict_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r2 selected=C slot=4 reservations=4 conflict=no conflict_block=-1 conflict_start=-1 conflict_end=-1 owner=r-1 action=retain status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r2 selected=C slot=4 assignments=5 reservations=4 application=applied ownership=function_scan status=complete" compiler.log
	sed -n '/allocatorRaCoreLivenessMultiValue:/,/\.ENDS/p' main.asm | grep -q "retained arg2 in C"
	grep -q "loop_register_path_reservation_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 candidate=C reservation=1 path_block=1 start=179 end=183 transparent=no blocking=180 status=complete" compiler.log
	grep -q "loop_register_path_candidate function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 candidate=2 phy=4 path_safe=yes status=complete" compiler.log
	grep -q "loop_register_selection function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 preferred=5 candidates=3 assignments=5 bound=0 selected=2 phy=4 selection=ready status=complete" compiler.log
	grep -q "loop_retention_conflict_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 selected=B slot=3 reservations=4 conflict=no conflict_block=-1 conflict_start=-1 conflict_end=-1 owner=r-1 action=retain status=complete" compiler.log
	grep -q "loop_retention_plan function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 slot=3 assignments=5 reservations=4 plan=ready status=complete" compiler.log
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCoreLivenessMultiValue temp=r3" compiler.log)" -eq 5
	grep -q "loop_retention_transaction function=allocatorRaCoreLivenessMultiValue temp=r3 slot=3 assignments=5 reservations=4 state_count=8 transaction=applied atomic=yes ownership=function_scan status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessMultiValue block=1 temp=r3 selected=B slot=3 assignments=5 reservations=4 application=applied ownership=function_scan status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessMultiValue .*temp=r3 .*spill=exempt" compiler.log)" -eq 3
	grep -q "assignment function=allocatorRaCoreLivenessMultiValue arg1_kind=STACK_LOCAL arg1_phy=NONE arg1_size=1 result_kind=PHY_B result_phy=B result_size=1" compiler.log
	grep -q "target_address_materialization target=IX kind=PHY_B offset=0 mode=none folded_offset=0 target_policy=z80" compiler.log
	grep -q "assignment_physical_result function=allocatorRaCoreLivenessMultiValue source_kind=STACK_LOCAL source_size=1 result_phy=B result_size=1 via=A store=B status=complete" compiler.log
	test "$$(sed -n '/allocatorRaCoreLivenessMultiValue:/,/\.ENDS/p' main.asm | grep -c "LD  B,A")" -eq 2
	test "$$(sed -n '/allocatorRaCoreLivenessMultiValue:/,/\.ENDS/p' main.asm | grep -c "retained arg2 in B")" -eq 2
	sed -n '/allocatorRaCoreLivenessMultiValue:/,/\.ENDS/p' main.asm | grep -q "retained arg1 in B"
	! sed -n '/allocatorRaCoreLivenessMultiValue:/,/\.ENDS/p' main.asm | grep -q "IX-10"
	grep -q "loop_stack_promotion function=allocatorRaCoreLivenessNestedLoops block=1 identity=1 size=8 proposed_temp=r1 existing_temps=1 plan=ready reason=none status=complete" compiler.log
	grep -q "loop_join_path_reservation function=allocatorRaCoreLivenessNestedLoops block=1 temp=r1 slot=4 blocks=7 instructions=441 edges=8 assignments=5 capacity=7 reservations=7 plan=ready order=stable_block status=complete" compiler.log
	grep -q "loop_retention_conflict_preflight function=allocatorRaCoreLivenessNestedLoops block=1 temp=r1 selected=C slot=4 reservations=7 conflict=no conflict_block=-1 conflict_start=-1 conflict_end=-1 owner=r-1 action=retain status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessNestedLoops block=1 temp=r1 selected=C slot=4 assignments=5 reservations=7 application=applied ownership=function_scan status=complete" compiler.log
	grep -q "join_retention_fallback function=allocatorRaCoreLivenessNestedLoops block=3 temp=r1 consumer=-1 operand=0 reason=consumer_unsupported action=skip_join_rewrite status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessNestedLoops pass=2 inserted=0 rewritten=0 added_temps=0 rebuild=no status=complete" compiler.log
	sed -n '/allocatorRaCoreLivenessNestedLoops:/,/\.ENDS/p' main.asm | grep -q "retained arg2 in C"
	sed -n '/allocatorRaCoreLivenessNestedLoops:/,/\.ENDS/p' main.asm | grep -q "ADD A,(IX-8)"
	grep -q "loop_stack_search function=allocatorRaCoreLivenessMultiValue block=1 live_in_candidates=1 action=inspect_stack_values reason=retained_live_in_present status=complete" compiler.log
	grep -q "loop_stack_promotion function=allocatorRaCoreLivenessMultiValue block=1 identity=2 size=8 proposed_temp=r3 existing_temps=3 plan=ready reason=none status=complete" compiler.log
	grep -q "loop_candidate_iteration function=allocatorRaCoreLivenessMultiValue block=1 processed_temp=r2 remaining=1 action=select_next status=complete" compiler.log
	grep -q "loop_candidate_owner_filter function=allocatorRaCoreLivenessMultiValue block=1 path_entries=4 filtered=1 action=select_unreserved status=complete" compiler.log
	grep -q "loop_stack_promotion function=allocatorRaCoreLivenessNestedDisjoint block=3 identity=3 size=16 proposed_temp=r2 existing_temps=2 plan=ready reason=none status=complete" compiler.log
	grep -q "loop_candidate_owner_filter function=allocatorRaCoreLivenessNestedDisjoint block=3 path_entries=0 filtered=0 action=select_unreserved status=complete" compiler.log
	! grep -q "loop_retention_conflict_preflight function=allocatorRaCoreLivenessNestedDisjoint block=3 temp=r2" compiler.log
	! grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessNestedDisjoint block=3 temp=r2" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessNestedDisjoint .*temp=r2 .*spill=required" compiler.log)" -eq 3
	! sed -n '/allocatorRaCoreLivenessNestedDisjoint:/,/\.ENDS/p' main.asm | grep -q "retained arg2 in HL"
	sed -n '/allocatorRaCoreLivenessNestedDisjoint:/,/\.ENDS/p' main.asm | grep -q "INC HL"
	! grep -q "loop_retention_conflict_preflight function=allocatorRaCoreLivenessNestedConflict block=3 temp=r2" compiler.log
	! grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessNestedConflict block=3 temp=r2" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessNestedConflict .*temp=r2 .*spill=required" compiler.log)" -eq 3
	sed -n '/allocatorRaCoreLivenessNestedConflict:/,/\.ENDS/p' main.asm | grep -q "ADD A,(IX-11)"
	! sed -n '/allocatorRaCoreLivenessNestedConflict:/,/\.ENDS/p' main.asm | grep -q "retained arg1 in HL"
	grep -q "loop_exit_profile_preflight function=allocatorRaCoreLivenessBreakExit block=1 entries=1 back_edges=1 exits=2 terminal_exits=1 topology=ready action=inspect_candidate mode=observe_only status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessBreakExit block=1 temp=r1 selected=C slot=4 assignments=5 reservations=6 application=applied ownership=function_scan status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessBreakExit .*temp=r1 .*spill=exempt" compiler.log)" -eq 5
	sed -n '/allocatorRaCoreLivenessBreakExit:/,/\.ENDS/p' main.asm | grep -q "retained arg2 in C"
	sed -n '/allocatorRaCoreLivenessBreakExit:/,/\.ENDS/p' main.asm | grep -q "retained arg1 in C"
	! sed -n '/allocatorRaCoreLivenessBreakExit:/,/\.ENDS/p' main.asm | grep -q "IX-6"
	grep -q "loop_exit_profile_preflight function=allocatorRaCoreLivenessEarlyReturn block=1 entries=1 back_edges=1 exits=2 terminal_exits=2 topology=ready action=inspect_candidate mode=observe_only status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessEarlyReturn block=1 temp=r1 selected=C slot=4 assignments=6 reservations=6 application=applied ownership=function_scan status=complete" compiler.log
	test "$$(sed -n '/allocatorRaCoreLivenessEarlyReturn:/,/\.ENDS/p' main.asm | grep -c "retained arg1 in C")" -eq 3
	! sed -n '/allocatorRaCoreLivenessEarlyReturn:/,/\.ENDS/p' main.asm | grep -q "IX-6"
	grep -q "loop_exit_profile_preflight function=allocatorRaCoreLivenessContinueExit block=1 entries=1 back_edges=2 exits=1 terminal_exits=1 topology=ambiguous action=inspect_multi_latch mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_value_roles_preflight function=allocatorRaCoreLivenessContinueExit block=1 value=0 identity=1 variable=value roles=31 required=31 mode=observe_only status=complete" compiler.log
	grep -q "loop_stack_promotion_handoff function=allocatorRaCoreLivenessContinueExit block=1 rewritten=5 added_temps=1 analysis=stale next=rebuild status=complete" compiler.log
	grep -q "loop_latch_selection_preflight function=allocatorRaCoreLivenessContinueExit block=1 temp=r1 entries=1 back_edges=2 defining_latches=1 latch=4 definition=360 selection=ready action=retain status=complete" compiler.log
	grep -q "loop_join_fact_discovery_preflight function=allocatorRaCoreLivenessContinueExit block=1 temp=r1 entry=347 latch=360 consumer=351 operand=2 facts=ready mode=observe_only status=complete" compiler.log
	grep -q "loop_retention_apply_preflight function=allocatorRaCoreLivenessContinueExit block=1 temp=r1 selected=C slot=4 assignments=5 reservations=6 application=applied ownership=function_scan status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessContinueExit .*temp=r1 .*spill=exempt" compiler.log)" -eq 5
	grep -q "return_value_source function=allocatorRaCoreLivenessContinueExit kind=PHY_C size=1 phy=C" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessContinueExit pass=1 inserted=0 rewritten=5 added_temps=1 rebuild=yes status=complete" compiler.log
	grep -q "post_mutation_fixed_point function=allocatorRaCoreLivenessContinueExit pass=2 inserted=0 rewritten=0 added_temps=0 rebuild=no status=complete" compiler.log
	sed -n '/allocatorRaCoreLivenessContinueExit:/,/\.ENDS/p' main.asm | grep -q "retained arg2 in C"
	sed -n '/allocatorRaCoreLivenessContinueExit:/,/\.ENDS/p' main.asm | grep -q "retained arg1 in C"
	! sed -n '/allocatorRaCoreLivenessContinueExit:/,/\.ENDS/p' main.asm | grep -q "IX-6"
	test "$$(grep -c "liveness function=allocatorRaCoreLivenessLoopJoin block=1 use={} def={r0} live_in={} live_out={}" compiler.log)" -eq 1
	grep -q "join_retention function=allocatorRaCoreLivenessLoopJoin block=1 temp=r1 .*eligible=no reason=definition_unsupported status=complete" compiler.log
	grep -q "loop_join_path_reservation function=coreLoopPathReady .*reservations=4 plan=ready order=stable_block status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_path_reservation_block function=coreLoopPathReady" core_liveness_storage_test.log)" -eq 4
	grep -q "loop_join_path_reservation_block function=coreLoopPathReady temp=r2 reservation=0 block=0 start=2 end=2 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopPathReady temp=r2 reservation=1 block=1 start=3 end=5 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopPathReady temp=r2 reservation=2 block=2 start=6 end=7 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopPathReady temp=r2 reservation=3 block=3 start=8 end=10 slot=7 status=complete" core_liveness_storage_test.log
	! grep -q "loop_join_path_reservation_block function=coreLoopPathReady .*block=4 " core_liveness_storage_test.log
	grep -q "loop_join_path_reservation function=coreLoopPathCapacity .*capacity=3 reservations=4 status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation function=coreLoopPathUnreachable .*latch=3 reachable=no status=unreachable_latch" core_liveness_storage_test.log
	test "$$(grep -Ec "loop_join_path_reservation function=coreLoopPath(Assignment(Block|Register|Role)|TopologyEdge) .*status=invalid_assignment" core_liveness_storage_test.log)" -eq 4
	grep -q "loop_join_path_reservation_edge function=coreLoopPathInvalidEdge .*status=invalid_edge" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopPathOverlappingBlock .*status=overlapping_block" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopPathInvalidBlock .*status=invalid_block" core_liveness_storage_test.log
	test "$$(grep -Ec "loop_join_path_reservation function=coreLoopPath(Ambiguous|Schedule) .*status=invalid_plan" core_liveness_storage_test.log)" -eq 2
	grep -q "loop_join_path_reservation function=coreLoopPathIneligible .*reservations=0 plan=ineligible status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_join_path_reservation function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	test "$$(grep -c "register_allocator_core: loop_retention_plan " core_liveness_storage_test.log)" -eq 15
	grep -q "loop_retention_plan function=coreLoopRetentionReady block=1 temp=r2 slot=7 assignments=3 reservations=4 plan=ready status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreLoopRetentionIneligible .*plan=ineligible reason=definition status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreLoopRetentionConflict .*plan=ineligible reason=schedule status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation function=coreLoopRetentionReservation .*capacity=3 reservations=4 status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreLoopRetentionCapacity .*status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreLoopRetentionDefinition .*status=invalid_definition" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreLoopSupplementalReady block=1 temp=r2 slot=7 assignments=5 reservations=5 plan=ready status=complete" core_liveness_storage_test.log
	test "$$(grep -c "loop_retention_plan_assignment function=coreLoopSupplementalReady" core_liveness_storage_test.log)" -eq 2
	grep -q "loop_join_path_reservation function=coreLoopSupplementalReady .*assignments=5 capacity=5 reservations=5 plan=ready order=stable_block status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_plan_assignment function=coreLoopSupplementalDuplicate .*assignment=4 duplicate=3 status=invalid_assignment" core_liveness_storage_test.log
	grep -q "loop_retention_plan_assignment function=coreLoopSupplementalConflict .*assignment=4 status=invalid_assignment" core_liveness_storage_test.log
	grep -q "loop_retention_plan_assignment function=coreLoopSupplementalRole .*assignment=4 status=invalid_assignment" core_liveness_storage_test.log
	grep -q "loop_retention_plan function=coreLoopSupplementalCapacity .*status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_assignment function=coreLoopSupplementalUnreachable .*assignment=4 block=0 reachable=no status=invalid_assignment" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_assignment function=coreLoopSupplementalExitArms temp=r2 assignment=3 block=5 path_blocks=5 added_blocks=2 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_assignment function=coreLoopSupplementalExitArms temp=r2 assignment=4 block=6 path_blocks=5 added_blocks=1 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation function=coreLoopSupplementalExitArms .*assignments=5 capacity=8 reservations=7 plan=ready order=stable_block status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopSupplementalExitArms temp=r2 reservation=5 block=5 start=10 end=10 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopSupplementalExitArms temp=r2 reservation=6 block=6 start=12 end=12 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_assignment function=coreLoopSupplementalMerge temp=r2 assignment=3 block=7 path_blocks=7 added_blocks=4 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation function=coreLoopSupplementalMerge .*assignments=4 capacity=8 reservations=8 plan=ready order=stable_block status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopSupplementalMerge temp=r2 reservation=5 block=5 start=10 end=11 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopSupplementalMerge temp=r2 reservation=6 block=6 start=12 end=13 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation_block function=coreLoopSupplementalMerge temp=r2 reservation=7 block=7 start=14 end=14 slot=7 status=complete" core_liveness_storage_test.log
	grep -q "loop_join_path_reservation function=coreLoopSupplementalMergeCapacity .*assignments=4 capacity=7 reservations=8 status=insufficient_capacity" core_liveness_storage_test.log
	test "$$(grep -c "register_allocator_core: loop_retention_apply function=" core_liveness_storage_test.log)" -eq 9
	grep -q "loop_retention_apply function=coreLoopSupplementalApply temp=r2 slot=7 assignments=5 reservations=5 application=applied atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_apply_assignment function=coreLoopSupplementalApplyDuplicate temp=r2 assignment=4 duplicate=3 status=invalid_assignment" core_liveness_storage_test.log
	grep -q "loop_retention_apply function=coreLoopApplyReady .*application=applied atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_apply function=coreLoopApplyFailed .*application=failed atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_apply function=coreLoopApplyCallback .*status=invalid_callback" core_liveness_storage_test.log
	grep -q "loop_retention_apply_assignment function=coreLoopApplyAssignment temp=r2 assignment=2 status=invalid_assignment" core_liveness_storage_test.log
	grep -q "loop_retention_apply_reservation function=coreLoopApplyReservation temp=r2 reservation=1 status=invalid_reservation" core_liveness_storage_test.log
	grep -q "loop_retention_apply function=coreLoopApplyIneligible .*application=ineligible status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_apply function=coreLoopApplyPlanInvalid .*status=invalid_plan" core_liveness_storage_test.log
	test "$$(grep -Ec "loop_retention_apply function=coreLoopApply(Context|Function) .*status=invalid_input" core_liveness_storage_test.log)" -eq 2
	grep -q "loop_retention_transaction function=coreLoopTransactionReady .*state_count=4 state_capacity=8 transaction=applied atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_transaction function=coreLoopSupplementalTransaction temp=r2 slot=7 assignments=5 reservations=5 state_count=5 state_capacity=8 transaction=applied atomic=yes status=complete" core_liveness_storage_test.log
	grep -q "loop_retention_transaction_assignment function=coreLoopSupplementalTransactionDuplicate temp=r2 assignment=4 duplicate=3 transaction=rejected status=invalid_assignment" core_liveness_storage_test.log
	test "$$(grep -c "loop_retention_transaction function=coreLoopTransaction.*transaction=rejected atomic=yes status=complete" core_liveness_storage_test.log)" -eq 4
	grep -q "join_path_reservation_commit function=coreLoopTransactionConflict .*conflict=0 commit=conflict status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_commit function=coreLoopTransactionCapacity .*state_capacity=3 .*status=insufficient_capacity" core_liveness_storage_test.log
	grep -q "join_path_reservation_commit_block function=coreLoopTransactionReservation .*status=invalid_reservation" core_liveness_storage_test.log
	grep -q "join_reservation function=coreJoinReservationReady .*selected=4 slot=1 slots=5 assignments=3 occupied=3 start=3 end=10 conflict=none reservation=ready status=complete" core_liveness_storage_test.log
	grep -q "join_reservation function=coreJoinReservationConflict .*start=3 end=10 conflict=r8 reservation=conflict status=complete" core_liveness_storage_test.log
	grep -q "join_reservation function=coreJoinReservationEndConflict .*start=3 end=10 conflict=r9 reservation=conflict status=complete" core_liveness_storage_test.log
	grep -q "join_reservation function=coreJoinReservationIneligible .*selected=-1 slot=-1 slots=5 assignments=0 capacity=0 occupied=3 reservation=ineligible status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_reservation function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	test "$$(grep -c "join_reservation function=.*status=invalid_schedule" core_liveness_storage_test.log)" -eq 4
	test "$$(grep -c "join_reservation_assignment function=.*status=invalid_assignment" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "join_reservation function=.*status=invalid_span" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "join_reservation_interval function=.*status=invalid_interval" core_liveness_storage_test.log)" -eq 3
	grep -q "join_path_reservation function=allocatorRaCoreLivenessDiamond temp=r0 blocks=9 instructions=441 edges=8 assignments=3 capacity=9 reservations=4 plan=ready status=complete" compiler.log
	grep -q "join_path_reservation_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 selected=C slot=4 blocks=4 plan=ready mode=observe_only status=complete" compiler.log
	test "$$(grep -c "join_path_reservation function=allocatorRaCoreLiveness" compiler.log)" -eq 53
	test "$$(grep -c "join_path_reservation_preflight function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	test "$$(grep -c "join_path_reservation_block function=allocatorRaCoreLivenessDiamond" compiler.log)" -eq 4
	grep -q "join_path_reservation_block function=allocatorRaCoreLivenessDiamond temp=r0 reservation=0 block=1 start=5 end=8 slot=4 status=complete" compiler.log
	grep -q "join_path_reservation_block function=allocatorRaCoreLivenessDiamond temp=r0 reservation=3 block=4 start=13 end=16 slot=4 status=complete" compiler.log
	grep -q "join_path_reservation_route function=coreJoinPathReservationDiamond temp=r2 producer=3 producer_block=1 consumer=10 consumer_block=5 reachable=yes status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_route function=coreJoinPathReservationDiamond temp=r2 producer=7 producer_block=3 consumer=10 consumer_block=5 reachable=yes status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_block function=coreJoinPathReservationDiamond temp=r2 reservation=0 block=1 start=3 end=3 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_block function=coreJoinPathReservationDiamond temp=r2 reservation=1 block=3 start=7 end=7 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_block function=coreJoinPathReservationDiamond temp=r2 reservation=2 block=5 start=10 end=10 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation function=coreJoinPathReservationDiamond .*reservations=3 plan=ready status=complete" core_liveness_storage_test.log
	! grep -q "join_path_reservation_block function=coreJoinPathReservationDiamond .*block=4 " core_liveness_storage_test.log
	grep -q "join_path_reservation function=coreJoinPathReservationCommon .*assignments=2 capacity=6 reservations=5 plan=ready status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_path_reservation_block function=coreJoinPathReservationCommon" core_liveness_storage_test.log)" -eq 5
	grep -q "join_path_reservation_block function=coreJoinPathReservationCommon .*block=1 start=2 end=3 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_block function=coreJoinPathReservationCommon .*block=3 start=6 end=7 slot=1 status=complete" core_liveness_storage_test.log
	! grep -q "join_path_reservation_block function=coreJoinPathReservationCommon .*block=4 " core_liveness_storage_test.log
	grep -q "join_path_reservation function=coreJoinPathReservationIneligible .*reservations=0 plan=ineligible status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_route function=coreJoinPathReservationUnreachable .*producer=9 producer_block=4 .*reachable=no status=unreachable_consumer" core_liveness_storage_test.log
	! grep -q "join_path_reservation_block function=coreJoinPathReservationUnreachable" core_liveness_storage_test.log
	grep -q "join_path_reservation function=coreJoinPathReservationCapacity .*capacity=2 reservations=3 status=insufficient_capacity" core_liveness_storage_test.log
	test "$$(grep -c "register_allocator_core: join_path_reservation function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	test "$$(grep -c "register_allocator_core: join_path_reservation_block function=.*status=overlapping_block" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "register_allocator_core: join_path_reservation_edge function=.*status=invalid_edge" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "register_allocator_core: join_path_reservation_assignment function=.*status=invalid_assignment" core_liveness_storage_test.log)" -eq 1
	grep -q "join_path_reservation_apply function=allocatorRaCoreLivenessDiamond temp=r0 blocks=9 instructions=441 reservations=4 capacity=9 applied=4 failed_block=-1 application=applied status=complete" compiler.log
	grep -q "join_path_reservation_apply_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 selected=C slot=4 planned=4 observed=4 application=applied mode=observe_only status=complete" compiler.log
	test "$$(grep -c "join_path_reservation_apply function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	test "$$(grep -c "join_path_reservation_apply_preflight function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	test "$$(grep -c "join_path_reservation_observe function=allocatorRaCoreLivenessDiamond" compiler.log)" -eq 4
	grep -q "join_path_reservation_apply_block function=coreJoinPathApply temp=r2 reservation=0 block=1 start=3 end=3 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_apply_block function=coreJoinPathApply temp=r2 reservation=1 block=3 start=7 end=7 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_apply_block function=coreJoinPathApply temp=r2 reservation=2 block=5 start=10 end=10 slot=1 status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_apply function=coreJoinPathApply temp=r2 blocks=6 instructions=12 reservations=3 capacity=6 applied=3 failed_block=-1 application=applied status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_apply function=coreJoinPathApplyPartial temp=r2 blocks=6 instructions=12 reservations=3 capacity=6 applied=1 failed_block=3 application=partial status=callback_failed" core_liveness_storage_test.log
	grep -q "join_path_reservation_apply function=coreJoinPathApplyIneligible temp=r2 blocks=6 instructions=12 reservations=0 capacity=0 applied=0 failed_block=-1 application=ineligible status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_path_reservation_apply function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	test "$$(grep -c "join_path_reservation_apply function=.*status=invalid_plan" core_liveness_storage_test.log)" -eq 3
	test "$$(grep -c "join_path_reservation_apply_block function=.*status=invalid_reservation" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "join_path_reservation_apply function=.*status=callback_failed" core_liveness_storage_test.log)" -eq 1
	grep -q "join_path_reservation_commit function=allocatorRaCoreLivenessDiamond temp=r0 blocks=9 instructions=441 reservations=4 reservation_capacity=9 state_count=4 state_capacity=9 committed=4 conflict=-1 commit=committed status=complete" compiler.log
	grep -q "join_path_reservation_commit_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 selected=C slot=4 planned=4 committed=4 state_count=4 commit=committed ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_reservation_commit function=allocatorRaCoreLivenessLoopJoin temp=r1 blocks=4 instructions=441 reservations=4 reservation_capacity=4 state_count=4 state_capacity=8 committed=4 conflict=-1 commit=committed status=complete" compiler.log
	test "$$(grep -c "join_path_reservation_commit function=allocatorRaCoreLiveness" compiler.log)" -eq 36
	test "$$(grep -c "join_path_reservation_commit_preflight function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	test "$$(grep -c "join_path_reservation_commit_entry function=allocatorRaCoreLivenessDiamond" compiler.log)" -eq 4
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCoreLivenessDiamond" compiler.log)" -eq 3
	grep -q "join_assignment_apply function=allocatorRaCoreLivenessDiamond temp=r0 instructions=441 assignments=3 capacity=3 applied=3 application=applied status=complete" compiler.log
	grep -q "join_assignment_apply_preflight function=allocatorRaCoreLivenessDiamond block=4 temp=r0 assignments=3 applied=3 application=applied ownership=function_scan status=complete" compiler.log
	grep -q "join_path_state_storage function=allocatorRaCoreLivenessDiamond blocks=9 temps=1 capacity=9 bytes=180 status=complete" compiler.log
	grep -q "join_path_state_acquire function=allocatorRaCoreLivenessDiamond capacity=9 bytes=180 count=0 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_state_scan function=allocatorRaCoreLivenessDiamond capacity=9 count=4 ownership=function_scan consumption=enabled status=complete" compiler.log
	grep -q "join_path_state_release function=allocatorRaCoreLivenessDiamond capacity=9 count=4 bytes=180 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_state_storage function=allocatorRaCoreLivenessLoop blocks=5 temps=1 capacity=5 bytes=100 status=complete" compiler.log
	grep -q "join_path_state_scan function=allocatorRaCoreLivenessLoop capacity=5 count=0 ownership=function_scan consumption=enabled status=complete" compiler.log
	grep -q "join_path_state_release function=allocatorRaCoreLivenessLoop capacity=5 count=0 bytes=100 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_state_storage function=allocatorRaCoreLivenessLoopJoin blocks=4 temps=1 capacity=4 bytes=80 status=complete" compiler.log
	grep -q "join_path_state_acquire function=allocatorRaCoreLivenessLoopJoin capacity=4 bytes=80 count=0 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_state_resize function=allocatorRaCoreLivenessLoopJoin old_capacity=4 new_capacity=8 state_count=0 old_bytes=80 new_bytes=160 status=complete" compiler.log
	grep -q "join_path_state_scan function=allocatorRaCoreLivenessLoopJoin capacity=8 count=4 ownership=function_scan consumption=enabled status=complete" compiler.log
	grep -q "join_path_state_release function=allocatorRaCoreLivenessLoopJoin capacity=8 count=4 bytes=160 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_state_acquire function=main capacity=0 bytes=0 count=0 ownership=function_scan mode=observe_only status=complete" compiler.log
	grep -q "join_path_state_scan function=main capacity=0 count=0 ownership=function_scan consumption=enabled status=complete" compiler.log
	grep -q "join_path_state_release function=main capacity=0 count=0 bytes=0 ownership=function_scan mode=observe_only status=complete" compiler.log
	test "$$(grep -c "join_path_state_acquire function=" compiler.log)" -eq 15
	test "$$(grep -c "join_path_state_scan function=" compiler.log)" -eq 15
	test "$$(grep -c "join_path_state_release function=" compiler.log)" -eq 15
	grep -q "join_path_state_candidate function=allocatorRaCoreLivenessLoop block=1 temp=r0 slot=0 start=36 end=38 reservation=available owner=r-1 status=complete" compiler.log
	grep -q "join_path_state_candidate function=allocatorRaCoreLivenessLoopJoin block=1 temp=r0 slot=0 start=59 end=61 reservation=available owner=r-1 status=complete" compiler.log
	test "$$(grep -c "join_path_state_candidate function=allocatorRaCoreLiveness" compiler.log)" -eq 24
	grep -q "block_exit_spill_decision function=allocatorRaCoreLivenessDiamond block=1 temp=r0 end=8 entry=0 slot=4 spill=exempt ownership=function_scan status=complete" compiler.log
	grep -q "block_exit_spill_decision function=allocatorRaCoreLivenessDiamond block=2 temp=r0 end=10 entry=1 slot=4 spill=exempt ownership=function_scan status=complete" compiler.log
	grep -q "block_exit_spill_decision function=allocatorRaCoreLivenessDiamond block=3 temp=r0 end=12 entry=2 slot=4 spill=exempt ownership=function_scan status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCoreLivenessDiamond .*spill=exempt" compiler.log)" -eq 3
	! grep -q "block_exit spill function=allocatorRaCoreLivenessDiamond .*r0" compiler.log
	grep -q "; retained arg1 in C" main.asm
	test "$$(grep -c "LD  C,[01]" main.asm)" -eq 2
	grep -q "join_path_state_storage function=coreJoinPathStateStorage blocks=6 temps=4 capacity=24 bytes=480 status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_path_state_storage function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	grep -q "join_path_state_query function=coreJoinPathStateOwned block=1 slot=1 temp=r2 start=3 end=5 entries=3 entry=0 owner=r2 reservation=owned status=complete" core_liveness_storage_test.log
	grep -q "join_path_state_query function=coreJoinPathStateConflict block=1 slot=1 temp=r7 start=4 end=4 entries=3 entry=0 owner=r2 reservation=conflict status=complete" core_liveness_storage_test.log
	grep -q "join_path_state_query function=coreJoinPathStateDisjoint block=1 slot=3 temp=r7 start=4 end=4 entries=3 entry=-1 owner=r-1 reservation=available status=complete" core_liveness_storage_test.log
	grep -q "join_path_state_query function=coreJoinPathStateBefore block=1 slot=1 temp=r7 start=0 end=2 entries=3 entry=-1 owner=r-1 reservation=available status=complete" core_liveness_storage_test.log
	grep -q "join_path_state_query function=coreJoinPathStateAfter block=1 slot=1 temp=r7 start=6 end=7 entries=3 entry=-1 owner=r-1 reservation=available status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_path_state_query function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 11
	test "$$(grep -c "join_path_state_query_entry function=.*status=invalid_entry" core_liveness_storage_test.log)" -eq 1
	grep -q "block_exit_spill_plan function=coreBlockExitSpillExempt block=1 end=5 temp=r2 entries=3 entry=0 slot=1 spill=exempt status=complete" core_liveness_storage_test.log
	grep -q "block_exit_spill_plan function=coreBlockExitSpillBefore block=1 end=2 temp=r2 entries=3 entry=-1 slot=-1 spill=required status=complete" core_liveness_storage_test.log
	grep -q "block_exit_spill_plan function=coreBlockExitSpillOtherTemp block=1 end=5 temp=r7 entries=3 entry=-1 slot=-1 spill=required status=complete" core_liveness_storage_test.log
	grep -q "block_exit_spill_plan function=coreBlockExitSpillAmbiguous block=1 end=5 temp=r2 entries=3 status=ambiguous_reservation" core_liveness_storage_test.log
	test "$$(grep -c "block_exit_spill_plan function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 10
	test "$$(grep -c "block_exit_spill_plan_entry function=.*status=invalid_entry" core_liveness_storage_test.log)" -eq 1
	grep -q "join_assignment_apply function=coreJoinAssignmentApply temp=r2 instructions=12 assignments=3 capacity=3 applied=3 application=applied status=complete" core_liveness_storage_test.log
	grep -q "join_assignment_apply function=coreJoinAssignmentFailure temp=r2 instructions=12 assignments=3 capacity=3 applied=0 application=failed status=callback_failed" core_liveness_storage_test.log
	grep -q "join_assignment_apply function=coreJoinAssignmentIneligible temp=r2 instructions=12 assignments=0 capacity=0 applied=0 application=ineligible status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_assignment_apply function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	test "$$(grep -c "join_assignment_apply function=.*status=invalid_schedule" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "join_assignment_apply function=.*status=invalid_assignment" core_liveness_storage_test.log)" -eq 5
	test "$$(grep -c "join_path_reservation_commit_entry function=coreJoinPathCommit temp=r2" core_liveness_storage_test.log)" -eq 3
	grep -q "join_path_reservation_commit function=coreJoinPathCommit temp=r2 .*state_count=3 state_capacity=8 committed=3 conflict=-1 commit=committed status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_path_reservation_commit_entry function=coreJoinPathCommitAppend temp=r2" core_liveness_storage_test.log)" -eq 3
	grep -q "join_path_reservation_commit function=coreJoinPathCommitAppend temp=r2 .*state_count=4 state_capacity=8 committed=3 conflict=-1 commit=committed status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_commit function=coreJoinPathCommitConflict temp=r2 .*state_count=1 state_capacity=8 committed=0 conflict=0 commit=conflict status=complete" core_liveness_storage_test.log
	! grep -q "join_path_reservation_commit_entry function=coreJoinPathCommitConflict" core_liveness_storage_test.log
	grep -q "join_path_reservation_commit function=coreJoinPathCommitNonoverlap temp=r2 .*state_count=4 state_capacity=8 committed=3 conflict=-1 commit=committed status=complete" core_liveness_storage_test.log
	grep -q "join_path_reservation_commit function=coreJoinPathCommitIneligible temp=r2 .*committed=0 conflict=-1 commit=ineligible status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_path_reservation_commit function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 10
	test "$$(grep -c "join_path_reservation_commit function=.*status=invalid_plan" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "join_path_reservation_commit function=.*status=insufficient_capacity" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "join_path_reservation_commit_state function=.*status=invalid_entry" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "join_path_reservation_commit_block function=.*status=invalid_reservation" core_liveness_storage_test.log)" -eq 2
	grep -q "join_spill_emission_order function=allocatorRaCoreLivenessLoopJoin blocks=4 instructions=441 items=0 order=descending_anchor status=complete" compiler.log
	test "$$(grep -c "join_spill_emission_order function=allocatorRaCoreLiveness.*status=complete" compiler.log)" -eq 14
	! grep -q "join_spill_emission_order function=allocatorRaCoreLivenessDiamond .*status=invalid" compiler.log
	! grep -q "join_spill_order function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_spill_order function=allocatorRaCoreLiveness.*status=invalid" compiler.log
	test "$$(grep -c "register_allocator: target_join_spill_site function=allocatorRaCoreLivenessDiamond .*approved=yes" compiler.log)" -eq 4
	test "$$(grep -c "join_spill_approval function=allocatorRaCoreLivenessDiamond .*approved=yes status=complete" compiler.log)" -eq 4
	test "$$(grep -c "join_spill_approval function=allocatorRaCoreLivenessDiamond .*policy=all_or_nothing status=complete" compiler.log)" -eq 2
	! grep -q "target_join_spill_site function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_spill_approval function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_spill_approval function=allocatorRaCoreLiveness.*status=invalid" compiler.log
	! grep -q "join_spill_approval function=allocatorRaCoreLiveness.*status=rejected" compiler.log
	grep -q "join_spill_mutation function=allocatorRaCoreLivenessDiamond block=4 temp=r0 predecessor=2 edge=jump anchor=10 placement=before action=reload_on_demand .*apply=no reason=stack_resident status=complete" compiler.log
	grep -q "join_spill_mutation function=allocatorRaCoreLivenessDiamond block=4 temp=r0 predecessor=3 edge=fallthrough anchor=12 placement=after action=reload_on_demand .*apply=no reason=stack_resident status=complete" compiler.log
	test "$$(grep -c "join_spill_mutation function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 2
	! grep -q "join_spill_mutation function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_spill_mutation function=allocatorRaCoreLiveness.*status=invalid_input" compiler.log
	test "$$(grep -c "spill_emission function=allocatorRaCoreLivenessDiamond requested=no emit=no status=complete" compiler.log)" -eq 2
	test "$$(grep -c "join_spill_emission function=allocatorRaCoreLivenessDiamond block=4 apply=no emit=no status=complete reason=empty_plan" compiler.log)" -eq 2
	! grep -q "join_spill_emission function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_spill_emission function=allocatorRaCoreLiveness.*status=invalid" compiler.log
	test "$$(grep -c "join_spill_emission_apply function=allocatorRaCoreLivenessDiamond block=4 emit=no status=skipped reason=empty_plan" compiler.log)" -eq 2
	! grep -q "join_spill_emission_apply function=allocatorRaCoreLivenessLoop " compiler.log
	! grep -q "join_spill_emission_apply function=allocatorRaCoreLiveness.*status=invalid_input" compiler.log
	! grep -q "join_spill_emission_apply function=allocatorRaCoreLiveness.*status=callback_failed" compiler.log
	grep -q "register_allocator_core: join_reconcile function=allocatorRaCoreLivenessDiamond block=4 predecessors=2 temp=0 live_in=yes state=stack action=reload_on_demand policy=stack_only status=complete" compiler.log
	grep -q "register_allocator_core: join_reconcile function=allocatorRaCoreLivenessDiamond blocks=9 edges=8 temps=1 joins=2 live_temps=1 policy=stack_only status=complete" compiler.log
	grep -q "register_allocator_core: join_reconcile function=allocatorRaCoreLivenessLoop blocks=5 edges=3 temps=1 joins=0 live_temps=0 policy=stack_only status=complete" compiler.log
	grep -q "liveness_iterations function=allocatorRaCoreLivenessLoop iterations=1" compiler.log
	grep -q "cfg_edge function=allocatorRaCoreLivenessLoop from_block=2 to_block=1 kind=jump" compiler.log
	test "$$(grep -c "liveness_index function=allocatorRaCoreLivenessDiamond .*status=complete" compiler.log)" -eq 29
	test "$$(grep -c "liveness_index function=allocatorRaCoreLivenessLoop .*status=complete" compiler.log)" -eq 15
	grep -q "liveness_index function=allocatorRaCoreLivenessDiamond blocks=9 temps=1 block=8 temp=0 cells=9 index=8 status=complete" compiler.log
	! grep -q "liveness_index function=allocatorRaCoreLiveness.*status=invalid_input" compiler.log
	! grep -q "register_allocator_core: liveness_solve.*status=failed_to_converge" compiler.log
	! grep -q "register_allocator_core: liveness_solve.*status=invalid_edge" compiler.log
	grep -q "liveness_storage function=coreLivenessStorageOne blocks=1 temps=1 cells=1 buffer_bytes=1 total_bytes=4 status=complete" core_liveness_storage_test.log
	grep -q "liveness_storage function=coreLivenessStorageWide blocks=4 temps=40 cells=160 buffer_bytes=160 total_bytes=640 status=complete" core_liveness_storage_test.log
	grep -q "liveness_storage function=coreLivenessStorageBlocks blocks=9 temps=3 cells=27 buffer_bytes=27 total_bytes=108 status=complete" core_liveness_storage_test.log
	grep -q "liveness_storage function=coreLivenessStorageMaximum blocks=2147483647 temps=1 .*status=" core_liveness_storage_test.log
	test "$$(grep -c "liveness_storage function=.*status=invalid_input" core_liveness_storage_test.log)" -ge 7
	grep -q "liveness_storage function=coreLivenessStorageLifecycle blocks=2 temps=2 cells=4 buffer_bytes=4 total_bytes=16 status=complete" core_liveness_storage_test.log
	grep -q "liveness_solve function=coreLivenessStorageLifecycle blocks=2 edges=1 temps=2 iterations=2 algorithm=backward_fixed_point status=converged" core_liveness_storage_test.log
	grep -q "liveness_index function=coreLivenessIndexFirst blocks=4 temps=3 block=0 temp=0 cells=12 index=0 status=complete" core_liveness_storage_test.log
	grep -q "liveness_index function=coreLivenessIndexMiddle blocks=4 temps=3 block=2 temp=1 cells=12 index=7 status=complete" core_liveness_storage_test.log
	grep -q "liveness_index function=coreLivenessIndexLast blocks=4 temps=3 block=3 temp=2 cells=12 index=11 status=complete" core_liveness_storage_test.log
	test "$$(grep -c "liveness_index function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 12
	test "$$(grep -c "liveness_solve function=.*status=invalid_storage" core_liveness_storage_test.log)" -eq 13
	grep -q "liveness_solve function=coreLivenessSolveCells blocks=2 edges=1 temps=2 cells=3 buffer_bytes=4 total_bytes=16 iterations=0 algorithm=backward_fixed_point status=invalid_storage" core_liveness_storage_test.log
	grep -q "liveness_solve function=coreLivenessSolveBuffer blocks=2 edges=1 temps=2 cells=4 buffer_bytes=3 total_bytes=16 iterations=0 algorithm=backward_fixed_point status=invalid_storage" core_liveness_storage_test.log
	grep -q "liveness_solve function=coreLivenessSolveTotal blocks=2 edges=1 temps=2 cells=4 buffer_bytes=4 total_bytes=15 iterations=0 algorithm=backward_fixed_point status=invalid_storage" core_liveness_storage_test.log
	grep -q "liveness_solve function=coreLivenessSolveIterationOverflow blocks=2147483647 edges=1 temps=1 cells=2147483647 .*iterations=0 algorithm=backward_fixed_point status=invalid_storage" core_liveness_storage_test.log
	grep -q "liveness_use_def function=coreLivenessUseDef block=4 start=0 end=4 instructions=5 temps=3 uses=2 defs=2 order=read_before_write status=complete" core_liveness_storage_test.log
	test "$$(grep -c "liveness_use_def function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 14
	test "$$(grep -c "liveness_use_def function=.*status=invalid_callback" core_liveness_storage_test.log)" -eq 3
	grep -q "liveness_use_def function=coreLivenessUseDefInvalidActive .*instruction=0 temp=-1 invalid_predicate=is_active status=invalid_callback" core_liveness_storage_test.log
	grep -q "liveness_use_def function=coreLivenessUseDefInvalidRead .*instruction=0 temp=0 invalid_predicate=reads_temp status=invalid_callback" core_liveness_storage_test.log
	grep -q "liveness_use_def function=coreLivenessUseDefInvalidWrite .*instruction=0 temp=0 invalid_predicate=writes_temp status=invalid_callback" core_liveness_storage_test.log
	grep -q "join_temp_state function=coreJoinStateStack block=3 predecessors=2 temp=r0 spill=yes phy=-1 no_phy=-1 stack=yes state=stack status=complete" core_liveness_storage_test.log
	grep -q "join_temp_state function=coreJoinStateRetained block=3 predecessors=2 temp=r1 spill=no phy=4 no_phy=-1 stack=no state=retained status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_temp_state function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 9
	grep -q "join_action_plan function=coreJoinActionStack block=3 predecessors=2 temp=r0 stack=yes action=reload_on_demand status=complete" core_liveness_storage_test.log
	grep -q "join_action_plan function=coreJoinActionRetained block=3 predecessors=2 temp=r1 stack=no action=spill_predecessors status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_action_plan function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 6
	grep -q "join_predecessors function=coreJoinPredCount block=3 blocks=4 edges=4 capacity=0 predecessors=2 mode=count status=complete" core_liveness_storage_test.log
	grep -q "join_predecessors function=coreJoinPredCollect block=3 blocks=4 edges=4 capacity=2 predecessors=2 mode=collect status=complete" core_liveness_storage_test.log
	grep -q "join_predecessors function=coreJoinPredSingle block=1 blocks=4 edges=4 capacity=2 predecessors=1 mode=collect status=complete" core_liveness_storage_test.log
	grep -q "join_predecessors function=coreJoinPredCapacity block=3 blocks=4 edges=4 capacity=1 predecessors=2 mode=collect status=insufficient_capacity" core_liveness_storage_test.log
	test "$$(grep -c "join_predecessors function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 9
	test "$$(grep -c "join_predecessors function=.*status=invalid_edge" core_liveness_storage_test.log)" -eq 2
	grep -q "join_spill_site function=coreJoinSpillMixed block=3 predecessor=0 edge=jump anchor=2 end_reason=2 placement=before status=complete" core_liveness_storage_test.log
	grep -q "join_spill_site function=coreJoinSpillMixed block=3 predecessor=1 edge=branch_true anchor=5 end_reason=2 placement=before status=complete" core_liveness_storage_test.log
	grep -q "join_spill_site function=coreJoinSpillMixed block=3 predecessor=2 edge=fallthrough anchor=8 end_reason=1 placement=after status=complete" core_liveness_storage_test.log
	grep -q "join_spill_site function=coreJoinSpillBranchFalse block=2 predecessor=1 edge=branch_false anchor=5 end_reason=2 placement=before status=complete" core_liveness_storage_test.log
	grep -q "join_spill_sites function=coreJoinSpillCapacity .*sites=1 mode=collect status=insufficient_capacity" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_sites function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 11
	test "$$(grep -c "join_spill_sites function=.*status=invalid_edge" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "join_spill_sites function=.*status=invalid_site" core_liveness_storage_test.log)" -eq 7
	grep -q "join_spill_order function=coreJoinOrderMixed block=3 site=0 predecessor=2 edge=fallthrough anchor=8 placement=after order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderMixed block=3 site=1 predecessor=1 edge=jump anchor=5 placement=before order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderMixed block=3 site=2 predecessor=0 edge=branch_true anchor=2 placement=before order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderStable block=3 site=0 predecessor=3 edge=jump anchor=8 placement=before order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderStable block=3 site=1 predecessor=1 edge=jump anchor=5 placement=before order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderStable block=3 site=2 predecessor=2 edge=branch_true anchor=5 placement=before order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderStable block=3 site=3 predecessor=2 edge=branch_true anchor=2 placement=before order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_order function=coreJoinOrderEmpty block=0 instructions=12 sites=0 order=descending_anchor status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_order function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	test "$$(grep -c "join_spill_order function=.*status=invalid_site" core_liveness_storage_test.log)" -eq 7
	grep -q "join_spill_emission_order function=coreJoinEmissionOrderMixed item=0 block=5 predecessor=3 edge=fallthrough anchor=8 placement=after temp=r2 phy=3 destination_offset=-6 bytes=2 order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_order function=coreJoinEmissionOrderMixed item=1 block=4 predecessor=4 edge=fallthrough anchor=5 placement=after temp=r3 phy=4 destination_offset=-8 bytes=2 order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_order function=coreJoinEmissionOrderMixed item=2 block=4 predecessor=1 edge=jump anchor=5 placement=before temp=r0 phy=1 destination_offset=-2 bytes=2 order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_order function=coreJoinEmissionOrderMixed item=3 block=5 predecessor=1 edge=jump anchor=5 placement=before temp=r1 phy=2 destination_offset=-4 bytes=2 order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_order function=coreJoinEmissionOrderMixed item=4 block=3 predecessor=2 edge=branch_true anchor=2 placement=before temp=r0 phy=1 destination_offset=-2 bytes=2 order=descending_anchor status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_order function=coreJoinEmissionOrderEmpty blocks=6 instructions=12 items=0 order=descending_anchor status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_emission_order function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 5
	test "$$(grep -c "join_spill_emission_order function=.*status=invalid_item" core_liveness_storage_test.log)" -eq 9
	grep -q "join_spill_approval function=coreJoinApprovalAll block=3 site=0 predecessor=1 edge=jump anchor=5 placement=before approved=yes status=complete" core_liveness_storage_test.log
	grep -q "join_spill_approval function=coreJoinApprovalAll block=3 instructions=12 sites=2 approved=2 order=site_order policy=all_or_nothing status=complete" core_liveness_storage_test.log
	grep -q "join_spill_approval function=coreJoinApprovalReject block=3 site=1 predecessor=2 edge=fallthrough anchor=8 placement=after approved=no status=rejected" core_liveness_storage_test.log
	grep -q "join_spill_approval function=coreJoinApprovalCallback block=3 site=1 predecessor=2 edge=fallthrough anchor=8 placement=after approved=invalid status=invalid_callback" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_approval function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	test "$$(grep -c "join_spill_approval function=.*status=invalid_site" core_liveness_storage_test.log)" -eq 7
	grep -q "join_spill_mutation function=coreJoinMutationRetained block=3 temp=r0 predecessor=1 edge=jump anchor=5 placement=before action=spill_predecessors phy=4 apply=yes reason=retained_live_in status=complete" core_liveness_storage_test.log
	grep -q "join_spill_mutation function=coreJoinMutationStack block=3 temp=r1 predecessor=2 edge=fallthrough anchor=8 placement=after action=reload_on_demand phy=-1 apply=no reason=stack_resident status=complete" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_mutation function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 14
	grep -q "join_spill_emission function=coreJoinEmitJump block=3 apply=yes predecessor=1 edge=jump anchor=5 placement=before temp=r2 phy=4 spill=yes destination_offset=-6 bytes=2 emit=yes status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission function=coreJoinEmitFall block=3 apply=yes predecessor=2 edge=fallthrough anchor=8 placement=after temp=r2 phy=4 spill=yes destination_offset=-7 bytes=1 emit=yes status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission function=coreJoinEmitEmpty block=3 apply=no emit=no status=complete reason=empty_plan" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_emission function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 9
	test "$$(grep -c "join_spill_emission function=.*status=invalid_spill_emission" core_liveness_storage_test.log)" -eq 6
	grep -q "join_spill_emission_apply function=coreJoinEmitApply block=3 emit=yes predecessor=1 edge=jump anchor=5 placement=before temp=r2 phy=4 destination_offset=-6 bytes=2 status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_apply function=coreJoinEmitApplyFall block=3 emit=yes predecessor=2 edge=fallthrough anchor=8 placement=after temp=r2 phy=4 destination_offset=-7 bytes=1 status=complete" core_liveness_storage_test.log
	grep -q "join_spill_emission_apply function=coreJoinEmitReject block=3 emit=yes predecessor=2 edge=fallthrough anchor=8 placement=after temp=r2 phy=4 destination_offset=-7 bytes=1 status=callback_failed" core_liveness_storage_test.log
	grep -q "join_spill_emission_apply function=coreJoinEmitSkip block=3 emit=no status=skipped reason=empty_plan" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_emission_apply function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 11
	grep -q "join_spill_mutation_apply function=coreJoinApply block=3 apply=yes predecessor=1 edge=jump anchor=5 placement=before temp=r2 phy=4 status=complete" core_liveness_storage_test.log
	grep -q "join_spill_mutation_apply function=coreJoinApplyReject block=3 apply=yes predecessor=1 edge=jump anchor=5 placement=before temp=r2 phy=4 status=callback_failed" core_liveness_storage_test.log
	grep -q "join_spill_mutation_apply function=coreJoinApplyEmpty block=3 apply=no status=skipped reason=empty_plan" core_liveness_storage_test.log
	test "$$(grep -c "join_spill_mutation_apply function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 14
	grep -q "join_reconcile function=coreJoinDiamond block=3 predecessors=2 temp=0 live_in=yes state=stack action=reload_on_demand policy=stack_only status=complete" core_liveness_storage_test.log
	grep -q "join_reconcile function=coreJoinDiamond blocks=4 edges=4 temps=3 joins=1 live_temps=2 policy=stack_only status=complete" core_liveness_storage_test.log
	grep -q "join_reconcile function=coreJoinRetained block=3 predecessors=2 temp=2 live_in=yes state=retained action=spill_predecessors policy=stack_only status=retained_live_in" core_liveness_storage_test.log
	test "$$(grep -c "join_reconcile function=.*status=invalid_callback" core_liveness_storage_test.log)" -eq 2
	test "$$(grep -c "join_reconcile function=.*status=invalid_input" core_liveness_storage_test.log)" -eq 8
	test "$$(grep -c "join_reconcile function=.*status=invalid_edge" core_liveness_storage_test.log)" -eq 1
	test "$$(grep -c "join_reconcile function=.*status=invalid_liveness" core_liveness_storage_test.log)" -eq 1
	grep -q "join_reconcile function=coreJoinNoEdges blocks=1 edges=0 temps=1 joins=0 live_temps=0 policy=stack_only status=complete" core_liveness_storage_test.log
	grep -q "join_reconcile function=coreJoinLoop blocks=3 edges=2 temps=1 joins=0 live_temps=0 policy=stack_only status=complete" core_liveness_storage_test.log
	grep -q "join_reconcile function=coreJoinEmpty blocks=0 edges=0 temps=1 joins=0 live_temps=0 policy=stack_only status=empty" core_liveness_storage_test.log
	grep -q "liveness_solve function=coreLivenessSolveEmpty blocks=0 edges=0 temps=2 iterations=0 algorithm=backward_fixed_point status=empty" core_liveness_storage_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


core_liveness_storage_test: core_liveness_storage_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_liveness_storage_test core_liveness_storage_test.c ../../../../compiler/register_allocator.c

tac_insert_test: tac_insert_test.c ../../../../compiler/tac.c ../../../../compiler/tac.h
	gcc -ansi -pedantic -Wall -Werror -o tac_insert_test tac_insert_test.c ../../../../compiler/tac.c

z80_register_spill_test: z80_register_spill_test.c ../../../../compiler/z80_register_spill.c ../../../../compiler/z80_register_spill.h
	gcc -ansi -pedantic -Wall -Werror -o z80_register_spill_test z80_register_spill_test.c ../../../../compiler/z80_register_spill.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_liveness_storage_test core_liveness_storage_test.log tac_insert_test tac_insert_test.log z80_register_spill_test z80_register_spill_test.log